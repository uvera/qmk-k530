// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Host side of the Redragon K530 Draconic radio link.
 *
 * The PixArt PAR2801 keeps its own firmware. The Sonix is an SPI slave
 * (P0.2 MISO, P0.3 MOSI, P0.4 SCK, P0.5 SEL) and pulses P0.1 when a new
 * 39-byte frame is ready. Byte 0 is the command, byte 38 is the sum of
 * bytes 0..37. Reconstructed from the stock SN32 image, see stock_radio.md.
 *
 * The first clocked byte is the module's command and is answered with
 * tx[0]. Later bytes are stored and, when that command is 0xA2, answered
 * with the rest of the frame. 0x20 is echoed. Anything else is answered
 * with zeros after the first byte.
 *
 * Stock never sends a control frame at boot in wireless. The module
 * reconnects to its saved host by itself. Pairing a new host is a held
 * key (0xA6, byte 3 = 1), and leaving or entering wired mode is 0xA5.
 */

#include "quantum.h"
#include "bluetooth.h"
#include "connection.h"
#include "usb_main.h"
#include "k530.h"

#define K530_BT_FRAME_LEN 39
#define K530_BT_READY_PIN A1
#define K530_BT_MODE_PIN B13

/* Stock waits up to 32 x delay(2000) for USB, about 13 s at 48 MHz. */
#define K530_BT_USB_WAIT_MS 3000
#define K530_BT_LINK_DEBOUNCE_MS 1000
#define K530_BT_PAIR_HOLD_MS 3000
/* Frame byte 2 of 0xAA key reports: stock's status byte 0x200000B6, 0 in normal use. */
#define K530_BT_REPORT_STATUS 0
#define K530_BT_REPULSE_MS 10
#define K530_BT_REPULSE_MAX 5
#define K530_BT_LED 62
/* Stock blink periods for module states 1 and 2: 32 and 96 ticks of 20 ms. */
#define K530_BT_BLINK_FAST_MS 640
#define K530_BT_BLINK_SLOW_MS 1920
#define K530_BT_PAIR_TIMEOUT_MS 60000
#define K530_BT_RECONNECT_TIMEOUT_MS 10000
#define K530_BT_CONNECTED_SHOW_MS 2000

enum k530_link {
    LINK_UNKNOWN,
    LINK_WIRED,    /* stock 0x200000BB = 1 */
    LINK_WIRELESS, /* stock 0x200000BB = 2 */
};

static uint8_t          frame[K530_BT_FRAME_LEN];
static volatile uint8_t tx[K530_BT_FRAME_LEN + 1];
/* Only the 0xA7 bytes, the running sum, and the previous byte for the 0x20 echo are kept. */
static volatile uint8_t rx[6];
static volatile uint8_t rx_sum;
static volatile uint8_t rx_prev;
static volatile uint8_t txn_open;
static volatile uint8_t rx_count;
static volatile uint8_t tx_count;
static volatile uint8_t first_byte;
static volatile uint8_t pulse_pending;
static volatile uint8_t burst;
static volatile uint8_t publish_seq;
static volatile uint8_t txn_seq;
static uint16_t         txn_idle_at;

static volatile uint8_t module_stat[3];
static volatile uint8_t module_seen;
static volatile uint8_t  a6_clocked;
static volatile uint8_t  last_opcode;
static volatile uint8_t  last_rx0; /* frame byte 0 of the last transfer */
static volatile uint8_t  last_len; /* bytes after the opcode in the last transfer */
static volatile uint16_t a7_seen_at;   /* last 0xA7 reply */
static volatile uint16_t xfer_seen_at; /* last completed transfer */
static volatile uint16_t aa_seen_at;   /* last key report read with 0xA2 */
static uint16_t          aa_sent_at;
static volatile uint8_t  clocked_seq; /* publish_seq of the last frame read with 0xA2 */

#ifdef CONSOLE_ENABLE
#    include "print.h"
/* Event ring filled from the SPI IRQ and the main loop, printed by bluetooth_task. */
#    define K530_LOG_LEN 16
static volatile uint8_t log_ring[K530_LOG_LEN][4];
static volatile uint8_t log_head;
static uint8_t          log_tail;

static void log_event(uint8_t type, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t slot = log_head & (K530_LOG_LEN - 1);
    log_ring[slot][0] = type;
    log_ring[slot][1] = a;
    log_ring[slot][2] = b;
    log_ring[slot][3] = c;
    log_head++;
}

static void log_flush(void) {
    while (log_tail != log_head) {
        uint8_t slot = log_tail & (K530_LOG_LEN - 1);
        uprintf("%5u %c %02X %02X %02X\n", timer_read() % 100000U, log_ring[slot][0], log_ring[slot][1], log_ring[slot][2], log_ring[slot][3]);
        log_tail++;
    }
}
#    define LOG_EVENT(t, a, b, c) log_event(t, a, b, c)
#    define LOG_THREAD(t, a, b, c) \
        do {                        \
            chSysLock();            \
            log_event(t, a, b, c);  \
            chSysUnlock();          \
        } while (0)
#else
#    define LOG_EVENT(t, a, b, c)
#    define LOG_THREAD(t, a, b, c)
#endif
static uint16_t          pulsed_at;
static uint8_t           repulses;

static volatile uint8_t hold_key;
static volatile uint8_t need_release;
static bool             keys_down; /* the last report published carried a key */

static uint8_t  link = LINK_UNKNOWN;
static uint16_t boot_at;
static bool     link_want_wired;
static uint16_t link_changed_at;

static uint8_t  bt_channel;
static uint8_t  sent_channel; /* stock 0x200000BD, 0 at boot */
static uint8_t  channel_seen;
static uint8_t  channel_count;
static uint16_t channel_sampled_at;
static uint16_t channel_changed_at;
static bool     pair_held;
static bool     pair_long_sent;
static uint16_t pair_pressed_at;
static uint8_t  pair_last; /* 0 none, 1 reconnect, 2 pair */

static bool status_on;

/*
 * Module state for the Bluetooth LED, as stock keeps it in 0x200000BF:
 * 0 quiet, 1 and 2 blink. A pair or reconnect command sets it at once;
 * the 0xA7 reply clocked with that command predates it, so replies are
 * ignored for the first second.
 */
static uint8_t  radio_state;
static uint16_t state_hold_at;
static uint16_t state_hold_ms;
static uint16_t a7_used_at;
static uint16_t connected_at;

static bool usb_up(void) {
    return usbGetDriverStateI(&USB_DRIVER) == USB_ACTIVE;
}

static void pull_link_pin(void) {
    /* Stock writes 0 to GPIO1 CFG, which pull-ups every pin on the port.
     * P1.14 is the mode pin copied into the radio frame. */
    SN_GPIO1->MODE &= ~(1U << 14);
    SN_GPIO1->CFG &= ~(3U << 28);
}

static bool p1_14_high(void) {
    pull_link_pin();
    return (SN_GPIO1->DATA & (1U << 14)) != 0;
}

static uint8_t link_mode(void) {
    /* Stock firmware: GPIOB.14 high -> 1, low -> 2. */
    return p1_14_high() ? 1 : 2;
}

static void select_host(void) {
    connection_host_t host;
    if (link == LINK_WIRED || (link == LINK_UNKNOWN && usb_up())) {
        host = CONNECTION_HOST_USB;
    } else {
        host = CONNECTION_HOST_BLUETOOTH;
    }
    if (connection_get_host_raw() != host) {
        connection_set_host_noeeprom(host);
    }
}

static void spi_slave_init(void) {
    SN_SYS1->AHBCLKEN |= (1U << 12); /* SPI0 */

    SN_SPI0->CTRL0 = (SN_SPI0->CTRL0 & ~0xF00U) | 0x700U; /* 8-bit */
    SN_SPI0->CTRL0 = (SN_SPI0->CTRL0 & ~0x8U) | 0x8U;     /* slave */
    SN_SPI0->CTRL0 &= ~(1U << 1);                         /* loopback off */
    SN_SPI0->CTRL0 &= ~(1U << 2);                         /* SDO on */
    SN_SPI0->CLKDIV = (SN_SPI0->CLKDIV & ~0xFFU) | 2U;
    SN_SPI0->CTRL1  = 0;
    SN_SPI0->CTRL0 &= ~(1U << 18); /* auto CS */

    SN_SPI0->IC = 0x0F;
    SN_SPI0->IE = 0x04; /* RX FIFO threshold */

    nvicDisableVector(SPI0_IRQn);
    /*
     * Above the RGB matrix PWM timer (CT16B1, priority 2), whose callback
     * also scans the shared matrix. A late reply here sends every following
     * byte one slot late, and the radio then sees a shifted frame.
     */
    nvicEnableVector(SPI0_IRQn, 1);

    txn_open = 0;
    rx_count = 0;
    tx_count = 0;

    SN_SPI0->CTRL0 |= 1U;                               /* enable */
    SN_SPI0->CTRL0 = (SN_SPI0->CTRL0 & ~0xC0U) | 0xC0U; /* FIFO reset */
}

static void ready_pulse(void) {
    burst = 0;
    gpio_write_pin_high(K530_BT_READY_PIN);
    gpio_set_pin_output(K530_BT_READY_PIN);
    gpio_write_pin_high(K530_BT_READY_PIN);
    wait_us(100);
    gpio_write_pin_low(K530_BT_READY_PIN);
    pulsed_at = timer_read();
}

static void copy_tx(void) {
    for (uint8_t i = 0; i < K530_BT_FRAME_LEN; i++) {
        tx[i] = frame[i];
    }
    tx[K530_BT_FRAME_LEN] = 0;
}

static void publish_frame(void) {
    frame[1] = link_mode();

    uint8_t sum = 0;
    for (uint8_t i = 0; i < K530_BT_FRAME_LEN - 1; i++) {
        sum += frame[i];
    }
    frame[K530_BT_FRAME_LEN - 1] = sum;
    publish_seq++;
    LOG_THREAD('P', frame[0], frame[5], publish_seq);

    /*
     * A transfer already on the wire keeps the bytes it started with.
     * Copying here would replace a pressed key with the release report.
     */
    chSysLock();
    if (txn_open) {
        pulse_pending = 1;
        chSysUnlock();
        return;
    }
    copy_tx();
    pulse_pending = 0;
    chSysUnlock();

    repulses = 0;
    spi_slave_init();
    ready_pulse();
}

static void clear_frame(void) {
    for (uint8_t i = 0; i < K530_BT_FRAME_LEN; i++) {
        frame[i] = 0;
    }
}

/*
 * Stock 0x43C: zero the TX buffer, pulse ready, hold it about 0.2 ms,
 * drop it, then wait about 20 ms (delay(1) and delay(100) of a helper
 * that spins roughly 2000 cycles per unit). Every 0xA5 and 0xA6 follows it.
 */
static void zero_pulse(void) {
    chSysLock();
    for (uint8_t i = 0; i < K530_BT_FRAME_LEN + 1; i++) {
        tx[i] = 0;
    }
    pulse_pending = 0;
    chSysUnlock();

    spi_slave_init();
    gpio_write_pin_high(K530_BT_READY_PIN);
    gpio_set_pin_output(K530_BT_READY_PIN);
    gpio_write_pin_high(K530_BT_READY_PIN);
    wait_us(200);
    gpio_write_pin_low(K530_BT_READY_PIN);
    wait_ms(20);
}

static void send_release(void) {
    hold_key     = 0;
    need_release = 0;
    clear_frame();
    frame[0]  = 0xAA;
    frame[2]  = K530_BT_REPORT_STATUS;
    frame[35] = 0x2C;
    frame[36] = 0x2C;
    publish_frame();
}

/* Stock 0x30E: bytes 4 and 5 are always 10 and 25, byte 7 is the link byte. */
static void send_a5(uint8_t mode, uint8_t link_byte) {
    zero_pulse();
    clear_frame();
    frame[0] = 0xA5;
    frame[3] = mode;
    frame[4] = 10;
    frame[5] = 25;
    frame[7] = link_byte;
    publish_frame();
}

/* Stock 0x336: byte 3 is 1 to pair a new host, 2 to reconnect; byte 4 is the channel. */
static void send_a6(uint8_t action) {
    zero_pulse();
    clear_frame();
    frame[0]       = 0xA6;
    frame[3]       = action;
    frame[4]       = bt_channel;
    module_stat[1] = 0;
    a6_clocked     = 0;
    sent_channel   = bt_channel;
    radio_state    = action;
    state_hold_at  = timer_read();
    state_hold_ms  = action == 1 ? K530_BT_PAIR_TIMEOUT_MS : K530_BT_RECONNECT_TIMEOUT_MS;
    connected_at   = 0;
    publish_frame();
}

static void set_radio_state(uint8_t state) {
    if (radio_state != 0 && state == 0) {
        connected_at = timer_read() | 1;
    }
    radio_state = state;
}

static void radio_state_task(void) {
    uint16_t seen = a7_seen_at;
    if (module_seen && seen != a7_used_at) {
        a7_used_at = seen;
        if (!state_hold_ms || (uint16_t)(seen - state_hold_at) >= 1000) {
            state_hold_ms = 0;
            set_radio_state(module_stat[1]);
        }
    }
    if (state_hold_ms && timer_elapsed(state_hold_at) > state_hold_ms) {
        state_hold_ms = 0;
        radio_state   = 0;
    }
}

/*
 * Stock 0x5512/0x5724: the BT1/BT2/BT3 switch is A6 (pull-up) sampled
 * with A7 driven low and then high. Both high is 0, low then high is 1,
 * both low is 2. Each phase is debounced 16 times.
 */
static uint8_t read_channel(void) {
    gpio_set_pin_output(A7);
    gpio_write_pin_low(A7);
    wait_us(10);
    bool with_low = gpio_read_pin(A6);
    gpio_write_pin_high(A7);
    wait_us(10);
    bool with_high = gpio_read_pin(A6);

    if (with_low && with_high) {
        return 0;
    }
    if (!with_low && with_high) {
        return 1;
    }
    if (!with_low && !with_high) {
        return 2;
    }
    return 0xFF;
}

/*
 * When the debounced channel differs from the last one sent for 50 slow
 * ticks, stock 0x56B6 sends the reconnect 0xA6 for it.
 */
static void channel_task(void) {
    if (timer_elapsed(channel_sampled_at) < 10) {
        return;
    }
    channel_sampled_at = timer_read();

    uint8_t sample = read_channel();
    if (sample == 0xFF) {
        return;
    }
    if (sample != channel_seen) {
        channel_seen  = sample;
        channel_count = 0;
        return;
    }
    if (channel_count < 16) {
        channel_count++;
        return;
    }
    if (bt_channel != sample) {
        bt_channel         = sample;
        channel_changed_at = timer_read();
    }
    if (link == LINK_WIRELESS && bt_channel != sent_channel && !pair_held && timer_elapsed(channel_changed_at) > 500 && !txn_open && !pulse_pending) {
        pair_last = 1;
        send_a6(2);
    }
}

/*
 * Stock 0xA8 action: byte 3 is state[23], which flash holds as 1. The
 * action also stores status byte 2. Nothing in stock binds it to a key.
 */
static void send_a8_default(void) {
    clear_frame();
    frame[0] = 0xA8;
    frame[2] = 2;
    frame[3] = 1;
    publish_frame();
}

static void enter_wired(bool from_boot) {
    if (link == LINK_WIRELESS) {
        send_release();
        wait_ms(40);
    }
    link = LINK_WIRED;
    /* Boot always sends 3; the runtime switch picks 3 or 2 from P1.14. */
    send_a5((from_boot || p1_14_high()) ? 3 : 2, 1);
}

static void enter_wireless(bool from_boot) {
    bool was_wired = link == LINK_WIRED;
    link           = LINK_WIRELESS;
    if (!from_boot && was_wired) {
        send_a5(0, 2);
    }
}

/*
 * Stock 0x4180 and 0x6868: P1.13 low is wireless with no frame. P1.13 high
 * goes wired once USB enumerates and falls back to wireless otherwise.
 * Wired also requires USB here, so a battery-only board never tells the
 * radio to stand down whichever level its switch gives.
 */
static void link_task(void) {
    bool want_wired = gpio_read_pin(K530_BT_MODE_PIN) && usb_up();

    if (link == LINK_UNKNOWN) {
        if (txn_open || pulse_pending) {
            return;
        }
        if (want_wired) {
            enter_wired(true);
        } else if (!gpio_read_pin(K530_BT_MODE_PIN) || timer_elapsed(boot_at) > K530_BT_USB_WAIT_MS) {
            enter_wireless(true);
        }
        link_want_wired = want_wired;
        link_changed_at = timer_read();
        return;
    }

    if (want_wired != link_want_wired) {
        link_want_wired = want_wired;
        link_changed_at = timer_read();
        return;
    }
    if (timer_elapsed(link_changed_at) < K530_BT_LINK_DEBOUNCE_MS || txn_open || pulse_pending) {
        return;
    }
    if (want_wired && link != LINK_WIRED) {
        enter_wired(false);
    } else if (!want_wired && link != LINK_WIRELESS) {
        enter_wireless(false);
    }
}

static void load_keyboard(const report_keyboard_t *report) {
    clear_frame();
    frame[0] = 0xAA;
    frame[2] = K530_BT_REPORT_STATUS;
    frame[3] = report->mods;
    frame[4] = report->reserved;

    uint8_t nkeys = KEYBOARD_REPORT_KEYS;
    if (nkeys > 6) {
        nkeys = 6;
    }
    for (uint8_t i = 0; i < 6; i++) {
        frame[5 + i] = (i < nkeys) ? report->keys[i] : 0;
    }

    /*
     * Bytes 11..25 stay zero. Stock (0x442A) fills its overflow bitmap
     * there only once all six key slots are taken, and the radio forwards
     * those bytes as an extra report on top of the six keys.
     */
    frame[35] = 0x2C;
    frame[36] = 0x2C;

    publish_frame();
}

OSAL_IRQ_HANDLER(SN32_SPI0_HANDLER) {
    OSAL_IRQ_PROLOGUE();

    if ((SN_SPI0->RIS & 0x04U) == 0) {
        OSAL_IRQ_EPILOGUE();
        return;
    }

    /* Threshold flag with an empty FIFO reads as 0x00 and would be latched
     * as the command. The radio's read request is 0xA2, which then follows.
     */
    if ((SN_SPI0->STAT & (1U << 2)) != 0) {
        SN_SPI0->IC = 0x04;
        OSAL_IRQ_EPILOGUE();
        return;
    }

    uint8_t data = (uint8_t)SN_SPI0->DATA;
    SN_SPI0->IC  = 0x04;

    if (!txn_open) {
        /*
         * Keep the first byte of a clock burst. A later 0xA2 or 0x20
         * inside that burst is payload, not a new command. Stock answers
         * the first clock with tx[0] for every opcode.
         */
        if (!burst) {
            burst         = 1;
            rx_count      = 0;
            rx_sum        = 0;
            tx_count      = 0;
            txn_open      = 1;
            first_byte    = data;
            txn_seq       = publish_seq;
            SN_SPI0->DATA = tx[0];
            tx_count      = 1;
            OSAL_IRQ_EPILOGUE();
            return;
        }
        SN_SPI0->DATA = 0;
        OSAL_IRQ_EPILOGUE();
        return;
    }

    uint8_t idx = rx_count++;
    if (idx < sizeof(rx)) {
        rx[idx] = data;
    }
    if (idx < 0x26) {
        rx_sum += data;
    }

    if (first_byte == 0x20) {
        if (rx_count < 2) {
            SN_SPI0->DATA = 0xBB;
        } else {
            SN_SPI0->DATA = (idx <= K530_BT_FRAME_LEN) ? rx_prev : 0;
        }
    } else if (first_byte == 0xA2) {
        SN_SPI0->DATA = (tx_count < K530_BT_FRAME_LEN) ? tx[tx_count] : 0;
    } else {
        SN_SPI0->DATA = 0;
    }
    tx_count++;
    rx_prev = data;

    if (rx_count > 0x26 && idx < K530_BT_FRAME_LEN) {
        txn_open = 0;
        burst    = 0;
        SN_SPI0->CTRL0 = (SN_SPI0->CTRL0 & ~0xC0U) | 0xC0U;

        xfer_seen_at = timer_read();
        last_opcode  = first_byte;
        last_rx0     = rx[0];
        last_len     = rx_count;
        LOG_EVENT(txn_seq == publish_seq ? 'X' : 'x', first_byte, tx[0], tx[5]);
        if (first_byte == 0xA2 && txn_seq == publish_seq) {
            clocked_seq = txn_seq;
            bool keyed = (tx[3] | tx[5] | tx[6] | tx[7] | tx[8] | tx[9] | tx[10]) != 0;
            if (tx[0] == 0xAA && keyed) {
                hold_key = 0;
                aa_seen_at = timer_read();
            }
            if (tx[0] == 0xA6) {
                a6_clocked = 1;
            }
        }
        /* Stock 0x9DAA: a received frame whose byte 0 is 0xA7 is the status reply. */
        if (data == rx_sum && rx[0] == 0xA7) {
            module_stat[0] = rx[3];
            module_stat[1] = rx[4];
            module_stat[2] = rx[5];
            module_seen    = 1;
            a7_seen_at     = timer_read();
        }
    }

    OSAL_IRQ_EPILOGUE();
}

void bluetooth_init(void) {
    gpio_set_pin_output(K530_BT_READY_PIN);
    gpio_write_pin_low(K530_BT_READY_PIN);

    spi_slave_init();
    clear_frame();
    boot_at = timer_read();
    link    = LINK_UNKNOWN;
    select_host();
}

static void pair_task(void) {
    if (!pair_held || pair_long_sent || timer_elapsed(pair_pressed_at) < K530_BT_PAIR_HOLD_MS) {
        return;
    }
    if (txn_open || pulse_pending) {
        return;
    }
    pair_long_sent = true;
    pair_last      = 2;
    send_a6(1);
}

void bluetooth_task(void) {
#ifdef CONSOLE_ENABLE
    log_flush();
#endif
    /*
     * A short clock burst leaves the transfer open, and the ready line is
     * then held until 39 bytes arrive. The radio never gets another pulse.
     * Drop it once SCK has been idle.
     */
    bool idle = ((SN_SPI0->STAT & 0x14U) == 0x04U);
    if (idle) {
        if (timer_elapsed(txn_idle_at) > 5) {
            if (txn_open && rx_count < 39) {
                last_opcode = first_byte;
                last_rx0    = rx[0];
                last_len    = rx_count;
                LOG_THREAD('I', rx_count, first_byte, 0);
                txn_open = 0;
            }
            burst = 0;
        }
    } else {
        txn_idle_at = timer_read();
    }

    link_task();
    select_host();
    channel_task();
    pair_task();
    radio_state_task();

    /* The key-up waits for the radio to read the key-down, but not forever. */
    if (link == LINK_WIRELESS && need_release && (!hold_key || timer_elapsed(aa_sent_at) > 50) && !txn_open && !pulse_pending) {
        send_release();
        return;
    }
    if (pulse_pending && !txn_open) {
        pulse_pending = 0;
        chSysLock();
        copy_tx();
        chSysUnlock();
        repulses = 0;
        spi_slave_init();
        ready_pulse();
        return;
    }
    /*
     * The radio reads every key report but forwards only some of them.
     * Offer each one again for a while, read or not, so a press or key-up
     * is not lost.
     */
    if (link == LINK_WIRELESS && tx[0] == 0xAA && !txn_open && repulses < K530_BT_REPULSE_MAX && timer_elapsed(pulsed_at) >= K530_BT_REPULSE_MS) {
        repulses++;
        LOG_THREAD('R', repulses, tx[5], publish_seq);
        spi_slave_init();
        ready_pulse();
    }
}

bool bluetooth_is_connected(void) {
    /*
     * Auto host selection only routes reports here once this is true.
     * The module is quiet until it sees a frame, so this has to be true
     * before the first exchange. select_host() keeps USB while wired.
     */
    return true;
}

bool bluetooth_can_send_nkro(void) {
    return false;
}

uint8_t bluetooth_keyboard_leds(void) {
    return 0;
}

static bool report_is_empty(const report_keyboard_t *report) {
    if (report->mods != 0) {
        return false;
    }
    uint8_t nkeys = KEYBOARD_REPORT_KEYS;
    if (nkeys > 6) {
        nkeys = 6;
    }
    for (uint8_t i = 0; i < nkeys; i++) {
        if (report->keys[i] != 0) {
            return false;
        }
    }
    return true;
}

void bluetooth_send_keyboard(report_keyboard_t *report) {
    if (link != LINK_WIRELESS) {
        return;
    }
    /*
     * The matrix reports "no keys" on every scan. Stock sends a frame only
     * when a key changes. A release is published later, once, from
     * bluetooth_task.
     */
    if (report_is_empty(report)) {
        if (keys_down) {
            keys_down    = false;
            need_release = 1;
        }
        return;
    }
    keys_down    = true;
    hold_key     = 1;
    need_release = 0;
    aa_sent_at   = timer_read();
    load_keyboard(report);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }
    switch (keycode) {
        case K530_BT_PAIR:
            /* Stock action 0x2C (Esc on its Fn layer) is ignored while wired. */
            if (record->event.pressed) {
                pair_held       = link == LINK_WIRELESS;
                pair_long_sent  = false;
                pair_pressed_at = timer_read();
            } else if (pair_held) {
                pair_held = false;
                if (!pair_long_sent) {
                    pair_last = 1;
                    send_a6(2);
                }
            }
            return false;
        case K530_BT_WIRELESS:
            /* Stock runtime switch to wireless: 0xA5 with byte 3 = 0, byte 7 = 2. */
            if (record->event.pressed && link == LINK_WIRELESS) {
                send_a5(0, 2);
            }
            return false;
        case K530_BT_PROBE:
            if (link != LINK_WIRELESS) {
                return false;
            }
            if (record->event.pressed) {
                clear_frame();
                frame[0] = 0xAA;
                frame[2] = K530_BT_REPORT_STATUS;
                for (uint8_t j = 3; j <= 36; j++) {
                    frame[j] = 4 + j - 3;
                }
                hold_key     = 1;
                need_release = 0;
                publish_frame();
            } else {
                need_release = 1;
            }
            return false;
        case K530_BT_STATUS:
            if (record->event.pressed) {
                status_on = !status_on;
            }
            return false;
        case K530_BT_RESTORE:
            if (record->event.pressed && link == LINK_WIRELESS) {
                send_a8_default();
            }
            return false;
    }
    return true;
}

static void set_led(uint8_t led_min, uint8_t led_max, uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
    if (led >= led_min && led < led_max) {
        rgb_matrix_set_color(led, r, g, b);
    }
}

static void paint_bits(uint8_t led_min, uint8_t led_max, uint8_t base, uint8_t value) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t v = (value & (1U << bit)) ? 255 : 0;
        set_led(led_min, led_max, base + bit, v, v, v);
    }
}

static void paint_level(uint8_t led_min, uint8_t led_max, uint8_t led, bool high) {
    if (high) {
        set_led(led_min, led_max, led, 255, 255, 255);
    } else {
        set_led(led_min, led_max, led, 60, 0, 0);
    }
}

/*
 * Status display, toggled with K530_BT_STATUS:
 *   Esc  P1.13      1  P1.14      2  A6       (white high, dim red low)
 *   3    USB up     4  link: blue wireless, red wired, yellow undecided
 *   5    module state from 0xA7 byte 4: red 0, yellow 1, green 2,
 *        purple other, off before the first 0xA7. Stock blinks the
 *        channel LED for 1 and 2 and treats 0 as quiet.
 *   6    last 0xA6: cyan reconnect, purple pair
 *   7    BT switch channel: red 0, green 1, blue 2
 *   8    last 0xA6 read by the radio with 0xA2: green yes, red not yet
 *   9    white while 0xA7 replies arrived in the last 2 s
 *   0    white while any transfer completed in the last 2 s
 *   -    key report in the last 2 s: green read by the radio, red not read
 *   =    opcode of the last transfer: green 0xA2, yellow 0x20, purple other
 *   Tab..U  last opcode, A..K its frame byte 0, Z..comma its length,
 *           bit 0 leftmost
 *   Right Shift  white once any 0xA7 arrived
 */
/*
 * Bluetooth case LED while wireless, after stock 0x6694/0x674A: the
 * channel colour (red BT1, green BT2, blue BT3) blinks fast in state 1
 * and slowly in state 2, and shows solid for a moment once the radio
 * goes quiet. Otherwise the keymap's own panel colour stays.
 */
static void bt_led(uint8_t led_min, uint8_t led_max) {
    if (link != LINK_WIRELESS) {
        return;
    }
    uint16_t period;
    if (radio_state == 1) {
        period = K530_BT_BLINK_FAST_MS;
    } else if (radio_state == 2) {
        period = K530_BT_BLINK_SLOW_MS;
    } else if (connected_at && timer_elapsed(connected_at) < K530_BT_CONNECTED_SHOW_MS) {
        period = 0;
    } else {
        connected_at = 0;
        return;
    }
    bool on = period == 0 || (timer_read() % period) < period / 2;
    uint8_t v = on ? 255 : 0;
    set_led(led_min, led_max, K530_BT_LED, bt_channel == 0 ? v : 0, bt_channel == 1 ? v : 0, bt_channel == 2 ? v : 0);
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (!rgb_matrix_indicators_advanced_user(led_min, led_max)) {
        return false;
    }
    if (!status_on) {
        bt_led(led_min, led_max);
        return true;
    }
    for (uint8_t i = led_min; i < led_max; i++) {
        rgb_matrix_set_color(i, 0, 0, 0);
    }

    paint_level(led_min, led_max, 0, gpio_read_pin(K530_BT_MODE_PIN));
    paint_level(led_min, led_max, 1, p1_14_high());
    paint_level(led_min, led_max, 2, gpio_read_pin(A6));
    paint_level(led_min, led_max, 3, usb_up());

    if (link == LINK_WIRELESS) {
        set_led(led_min, led_max, 4, 0, 0, 255);
    } else if (link == LINK_WIRED) {
        set_led(led_min, led_max, 4, 255, 0, 0);
    } else {
        set_led(led_min, led_max, 4, 255, 180, 0);
    }

    if (module_seen) {
        switch (module_stat[1]) {
            case 0:
                set_led(led_min, led_max, 5, 255, 0, 0);
                break;
            case 1:
                set_led(led_min, led_max, 5, 255, 180, 0);
                break;
            case 2:
                set_led(led_min, led_max, 5, 0, 255, 0);
                break;
            default:
                set_led(led_min, led_max, 5, 180, 0, 255);
                break;
        }
        set_led(led_min, led_max, 52, 255, 255, 255);
    }

    if (pair_last == 1) {
        set_led(led_min, led_max, 6, 0, 255, 255);
    } else if (pair_last == 2) {
        set_led(led_min, led_max, 6, 180, 0, 255);
    }

    if (bt_channel == 0) {
        set_led(led_min, led_max, 7, 255, 0, 0);
    } else if (bt_channel == 1) {
        set_led(led_min, led_max, 7, 0, 255, 0);
    } else {
        set_led(led_min, led_max, 7, 0, 0, 255);
    }

    if (pair_last != 0) {
        if (a6_clocked) {
            set_led(led_min, led_max, 8, 0, 255, 0);
        } else {
            set_led(led_min, led_max, 8, 255, 0, 0);
        }
    }

    if (module_seen && timer_elapsed(a7_seen_at) < 2000) {
        set_led(led_min, led_max, 9, 255, 255, 255);
    }

    if (xfer_seen_at && timer_elapsed(xfer_seen_at) < 2000) {
        set_led(led_min, led_max, 10, 255, 255, 255);
    }

    if (aa_seen_at && timer_elapsed(aa_seen_at) < 2000) {
        set_led(led_min, led_max, 11, 0, 255, 0);
    } else if (aa_sent_at && timer_elapsed(aa_sent_at) < 2000) {
        set_led(led_min, led_max, 11, 255, 0, 0);
    }

    if (xfer_seen_at) {
        switch (last_opcode) {
            case 0xA2:
                set_led(led_min, led_max, 12, 0, 255, 0);
                break;
            case 0x20:
                set_led(led_min, led_max, 12, 255, 180, 0);
                break;
            default:
                set_led(led_min, led_max, 12, 180, 0, 255);
                break;
        }
    }

    paint_bits(led_min, led_max, 14, last_opcode);
    paint_bits(led_min, led_max, 29, last_rx0);
    paint_bits(led_min, led_max, 42, last_len);
    return true;
}

void bluetooth_send_nkro(report_nkro_t *report) {
    (void)report;
}

void bluetooth_send_mouse(report_mouse_t *report) {
    (void)report;
}

void bluetooth_send_consumer(uint16_t usage) {
    frame[0]  = 0xA3;
    frame[33] = (uint8_t)usage;
    frame[34] = (uint8_t)(usage >> 8);
    publish_frame();
}

void bluetooth_send_system(uint16_t usage) {
    frame[33] = (uint8_t)usage;
    frame[34] = (uint8_t)(usage >> 8);
    publish_frame();
}

void bluetooth_send_raw_hid(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
}
