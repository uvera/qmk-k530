# K530 stock radio protocol

Decompiled from `/tmp/k530_sn32.bin` (65536 bytes, SN32F248BF) with radare2 6.2.2 and r2ghidra, Thumb, base address 0. Binwalk found no embedded files, filesystems, or compressed blobs. The only readable product text is the UTF-16 USB strings at about `0xCC01`: `SONiX`, `Draconic`, `DEMO`. The Bluetooth names live in the PixArt PAR2801, selected by a profile byte in the SPI frame.

Entry: reset vector `0xC7A5` is a trampoline. The literal at `0xC7C0` is `0xC1`, so the Thumb entry is `0xC0`. The helper at `0xC9D0` is a byte jump table and does not return. Disassemble the builders below as their own functions. Treating the table bytes as instructions produces the coprocessor junk Ghidra emitted on the first pass.

## SPI link

SPI0 slave, pins P0.2 MISO, P0.3 MOSI, P0.4 SCK, P0.5 SEL. Ready is P0.1. The keyboard is the slave. A transfer starts when the host pulses P0.1 high, forces GPIOA mode bit 1 to output, pulses again, waits, then drives P0.1 low. The module then clocks 40 bytes. The first clocked byte is the module opcode and is not stored. The next 39 bytes are the frame. Opcode `0xA2` shifts the TX buffer out. Opcode `0x20` is an echo. Opcode `0xA7` is a 3-byte status reply: bytes 3, 4, and 5 land in `0x200000C0`, `0x200000BF`, and `0x200000C1`.

TX buffer `0x20000B38`, 39 bytes. Byte 38 is the sum of bytes 0..37. Byte 0 is the command, copied from `0x200000BC` before the jump table. Byte 1 is the link mode from GPIO1 (`0x40046000`) bit 14: high is 1, low is 2. Byte 2 is the status byte `0x200000B6`. The scatter-load table at `0xE4A0` copies `0xE4C0` to `0x20000000..0x2000010F`, and that image is 0 at `0xB6`. Only the unbound `0xA8` action sets it to 2, so every normal stock frame carries 0.

Delays: `0x244(n)` spins `n * (80 >> AHB prescaler)` loops, and `0x25A(n)` calls `0x244(25)` n times while feeding the watchdog. At 48 MHz that is roughly 0.2 ms per `0x25A` unit, so the zero pulse's `delay(1)` and `delay(100)` are about 0.2 ms and 20 ms.

## Link byte `0x200000BB`

2 is wireless, 1 is wired. `0x42BE` only clears the USB pending bits and queues `0xAA` for the radio when it is 2 (`0x42E2`). `0x6868` only reads the module state `0x200000BF` when it is 2.

The sender at `0x2AC` runs only when bit 5 of `0x20000008` is set and `0x200000C6` (busy) is clear. It clears that bit, calls SPI init `0x9B70`, fills the frame, and pulses P0.1. `0x43C` is a separate pulse: it zeroes 42 bytes at the TX buffer, pulses, waits `delay(1)`, drops the line, then waits `delay(100)`. The real command is queued after that and sent by a later `0x2AC`.

## Frame builders

These are the r2ghidra functions, with literal-pool addresses replaced by the RAM they point at.

`0xA5` at `0x30E`:

```
tx[3] = byte at 0x200000C2
tx[4] = 10
tx[5] = 25
tx[6] = byte at 0x200000C3
tx[7] = byte at 0x200000BB
```

then checksum and pulse. `0x200000C2` and `0x200000C3` are cleared.

`0xA6` at `0x336`:

```
tx[3] = byte at 0x200000BE
tx[4] = state[21]          /* state block is 0x20000110, 42 bytes */
```

`0xA8` at `0x346`:

```
tx[3] = state[23]
```

`0xA9` at `0x350`:

```
tx[3] = state[20]
```

`0xAA` keyboard report at `0x360`:

```
tx[3..10]  = boot report at 0x2000003A          /* mods, reserved, 6 keys */
tx[11..25] = NKRO bitmap 0x20000262, bytes 1..15
tx[26..32] = bytes 1..7 of the 8-byte buffer at 0x20000032
tx[33..34] = bytes 1..2 of the 3-byte buffer at 0x20000042
tx[35] = tx[36] = 0x2C
```

`0xA5`, `0xA6`, `0xA8`, and `0xA9` skip the `0x2C` fill. They also leave every byte they do not assign as whatever was already in the TX buffer. A zeroed buffer is a different frame from a stock frame.

`0x42BE(0xA1)` stores command `0xAA` and sets the sender-pending bit when `0x200000BB == 2`. The wire command for key traffic is `0xAA`.

## Boot

`0x4180` runs before the main loop. It sets bits 17, 18, and 19 of `0x20000000`. If bit 20 of that word is clear, it clears bit 3 of `0x20000008` (USB IRQ1 at `0x124A` sets it) and loops at most 32 times with `delay(2000)`, about 13 s.

- GPIO1 bit 13 (P1.13) low: `0x200000BB = 2` (wireless) at once. No frame.
- P1.13 high and USB activity: zero pulse, `0x200000BB = 1` (wired), command `0xA5`, `0x200000C2 = 3`, sent immediately by `0x2AC`.
- P1.13 high and no USB within the loop: `0x200000BB = 2`. No frame.

So stock sends nothing to the radio when it boots wireless. The module reconnects to its saved host on its own.

## Runtime link switch

`0x6868` returns early while bit 19 of `0x20000000` is set; `0x86C4` clears it at the end of the boot LED animation. After that it debounces P1.13 with the halfword counter `0x2000007A` and acts once it passes 60:

- P1.13 low and `0x200000BB != 2`: zero pulse, `0x200000BB = 2`, `0xA5` with `0x200000C2 = 0`. Byte 7 is the link byte, so 2.
- P1.13 high and `0x200000BB != 1`: release all keys over the radio (`0x4520`), zero pulse, `0x200000BB = 1`, `0xA5` with `0x200000C2` = 3 when P1.14 is high and 2 when low. Byte 7 is 1.

The module's `0xA7` reply updates `0x200000BF` (byte 4, connection state 0, 1, 2), `0x200000C0`, and `0x200000C1`. While wireless, `0x6868` maps state 1 and 2 to the panel LED flags at `0x2000007E` and `0x2000007F`.

## Key actions

Special keys are 3-byte keymap entries with type 5; `0x45B8` dispatches the code. Codes from `0x2A` go through the switch table after `0x48A0` (`0xC9D0` is Keil's `__ARM_common_switch8`). The Fn tables at `0xCCD0` and `0xCD30` are `[position, type, code, arg]`. Positions are on a 21-wide template at `0xE09E`: row 1 starts at `0x15` with Esc, then `1`, `2`, and so on.

| Code | Entry | Bound to | What it does |
|------|-------|----------|--------------|
| `0x2C` | `0x4C62` | position `0x15` (Esc) in the `0xCD30` table | pair / reconnect, wireless only |
| `0x30` | `0x4BD2` | nothing | `0xA5` wired/wireless toggle |
| `0x33` | `0x4CE2` | nothing | `0xA8` cycle of `state[23]` |

`0xA6` (`0x2C`): key down sets the hold flag (bit 6 of `0x20000008`) and clears the counter `0x200000B7`. The slow timer at `0x0AEE` increments that counter while the flag is set.

- Hold to 55 (`0x0B60`): zero pulse, `0x200000BE = 1`, `0xA6`, save the state block, clear `0x200000BF`, send. This pairs a new host.
- Release before 55: zero pulse, `0x200000BE = 2`, `0xA6`, save, clear `0x200000BF`, send. This reconnects the saved host.

The builder puts `0x200000BE` in byte 3 and the channel `state[21]` in byte 4. The channel comes from the BT1/BT2/BT3 switch described under "Profile switch" below. Every `0xA6` path stores the channel it sent in `0x200000BD` (0 at boot).

`0xA8` (`0x33`) runs only when `0x200000BB != 1`. It cycles `state[23]` through 1, 3, 7, saves, sets the status byte `0x200000B6 = 2`, and sends `0xA8` with `state[23]` in byte 3. Flash holds 1 (`0xA097`). No stock table binds this code, so stock never sends `0xA8` in normal use.

`0xA5` (`0x30`) toggles the link byte. Going wireless sends byte 3 = 0 and byte 7 = 2. Going wired sends byte 3 = 2 or 3 from P1.14 and byte 7 = 1.

## Esc action

Action `0x29` at `0x4EC8` calls `0x4520`, which zeroes the 8-byte boot report at `0x2000003A` and 16 bytes at `0x20000262`, sets bits 2 and 3 of `0x20000000`, calls `0x42BE(0xA1)`, calls `0x2AC`, and waits `delay(40)`. It then sets the pending bit and calls `0x6116`.

`0x6116` copies 64 bytes into `0x20000A38` from `0xB940 + (byte 0x20000013 << 10) + (byte 0x200000AE * 64)`. The base block at `0xB940` is zeros in this image. This is a keymap copy. It does not write the TX buffer.

## Pairing (long hold)

`0xA6` key down sets bit 6 of `0x20000008`. While it stays set, `0xAE2` counts `0x200000B7`. At 55 (`0xB62`) it runs `0xB72`:

```
0x43C                      zero 42 bytes of TX, pulse ready, delay(1), low, delay(100)
cmd = 0xA6, 0x200000BE = 1, pending bit
0x911C(0xA080, 42, state)  save the state block
0x200000BF = 0, delay(20), 0x2AC
0x200000BD = state[21]
```

Wire frame: `A6, mode, status, 01, state[21], 00 ... 00, sum`. The zero pulse cleared the buffer, so everything past byte 4 is zero. A key-up before 55 sends the same frame with byte 3 = 2.

The status byte `0x200000B6` is written in one place only, the `0xA8` key path (`0x4D40`, value 2). On a normal boot it is 0, so the stock pairing frame carries status 0.

## Profile switch

`state[21]` is the BT1/BT2/BT3 position. Boot loads it from flash record byte `0x16` (`0x4220`). `0x5512` reads the switch: A6 has a pull-up, and A7 is driven low and then high, with A6 sampled each time and debounced 16 times.

| A6 with A7 low | A6 with A7 high | profile |
|---|---|---|
| high | high | 0 |
| low | high | 1 |
| low | low | 2 |

When the debounced profile differs from `0x200000BD` for 50 ticks, it sends the short `0xA6` (byte 3 = 2, byte 4 = profile).

## SPI timing on QMK

Answering each byte quickly matters more than any other detail. The slave reply for byte n has to be in the TX FIFO before the radio clocks byte n+1. If the IRQ is late by one byte, every later byte goes out one slot late. The radio then sees a shifted frame, so it gets the wrong modifiers and keys, and the checksum fails.

On QMK, the RGB matrix PWM timer (CT16B1) runs at priority 2. Its callback also scans the shared matrix, so it runs long. With SPI0 at priority 3, frames reached the radio shifted by two bytes:

- byte 1 (link byte 2) landed in the modifier slot as Left Shift, so letters came out uppercase;
- key slots three to six landed in the overflow bitmap and showed up as extra keys;
- the checksum matched only when the key codes happened to sum right, so frames got through only while mashing, and key-ups never did.

SPI0 at priority 1 fixes all of it.

## What the port sends

`k530_bt.c` follows the stock rules above:

- **Boot:** no control frame when booting wireless.
- **Status byte:** 0 in every frame.
- **Key reports (`0xAA`):**
  - Layout is stock's: mods, reserved and six keys in bytes 3..10, then `0x2C` in bytes 35 and 36.
  - The overflow bitmap (bytes 11..25) stays zero. A seventh key is dropped. Stock copies its NKRO bitmap there once six slots are full, and that made the radio emit an extra report.
  - Bytes 26..32 stay zero. Stock copies `0x20000032` bytes 1..7 there. This image never fills those bytes with a mouse report: `0x260A` sets byte 0 to report id 5, sends eight bytes on USB, and on failure zeros bytes 2..7.
  - After a report with keys, the all-keys-up report is always sent. It waits at most 50 ms for the radio to read the key-down first.
  - Each report is offered to the radio 5 times, 10 ms apart.
- **Wired or wireless:**
  - Wired only when P1.13 is high and USB is active. Stock goes wired on P1.13 high alone at runtime; requiring USB keeps a battery-only board wireless whichever level its switch gives.
  - Transitions send stock's `0xA5` after a zero pulse.
- **`K530_BT_PAIR` (FN2+Esc):** a tap sends the reconnect `0xA6` (byte 3 = 2), and a 3 s hold sends the pair `0xA6` (byte 3 = 1). Both follow a zero pulse and carry the switch channel in byte 4. The 55-tick hold time has not been measured, so 3 s is an estimate.
- **Channel switch:** a channel that differs from the last one sent for 500 ms sends the reconnect `0xA6`, as `0x56B6` does.
- **Bluetooth LED (62):** stock's blink pattern in the channel colour, then dim instead of off while wireless.
- **Host LED byte:** `0xA7` byte 3, stored by stock at `0x200000C0` and returned from `bluetooth_keyboard_leds()`. `0x6572` paints bit 1 (Caps Lock) and forces the byte to 0 while wired and `0x2000006E` has reached 10. Byte 5 (`0x200000C1`) is read at `0x65A0`: `0xF0` with P1.14 high, or 1 or 2 with P1.14 low, drives a different lamp. It is stored and not displayed.

## Host side

- Pairing asks the host to confirm a passkey. BlueZ needs a registered agent for that, or it rejects the request at once (`No agent available for request type 2`).
- Resetting the keyboard MCU does not reset the radio. After flashing, a reconnect (FN2+Esc tap) may be needed before the host links again.

Earlier builds sent `0xA8` (byte 3 = 1, later 3) and a pair `0xA6` on every boot, and some sent the wired `0xA5` (byte 7 = 1), all with status byte 2 and with P1.13 high treated as wireless. Stock does none of that. Their "status byte 2 is needed" result came from the shifted frames described under "SPI timing on QMK".
