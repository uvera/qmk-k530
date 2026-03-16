/*
Copyright 2021 Adam Honse

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include QMK_KEYBOARD_H
#define INDICATOR_KEY 28
#define BLUETOOTH_LED 62
#define BATTERY_LED 61
#define USER00 SAFE_RANGE -64
#ifdef OS_DETECTION_ENABLE
#    include "os_detection.h"
#endif
enum my_keycodes {

    CS_LAYER_MASK_F = USER00,

    CS_LAYER_MASK_B,
    RD_PANEL_TG,
};

bool BATTERYENABLED = true;
bool BLUETOOTHSTATE = true;

bool panelLedEnable = true;

short layer_MASK = 0;
enum layer_names { BASE, FN1, FN2, FN3 };

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
     * Layer BASE
     * ,-----------------------------------------------------------------------------------------.
     * | Esc |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  0  |  -  |  =  |   Bksp   |
     * |-----------------------------------------------------------------------------------------+
     * | Tab    |  Q  |  W  |  E  |  R  |  T  |  Y  |  U  |  I  |  O  |  P  |  [  |  ]  |   \    |
     * |-----------------------------------------------------------------------------------------+
     * | Caps    |  A  |  S  |  D  |  F  |  G  |  H  |  J  |  K  |  L  |  ;  |  '  |    Enter    |
     * |-----------------------------------------------------------------------------------------+
     * | Shift      |  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |    Up/Shift   |
     * |-----------------------------------------------------------------------------------------+
     * | Ctrl  | Win  |  Alt  |               space             | Alt  | Left/FN1 | Down/FN2 | Ctrl  |
     * \-----------------------------------------------------------------------------------------/
     */
    [BASE] = LAYOUT_60_ansi(
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,
        LT(FN1, KC_CAPS), KC_A,   KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,
        KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT, KC_SLSH, RSFT_T(KC_UP),
        KC_LCTL, KC_LGUI, KC_LALT, KC_SPC,  KC_RALT, LT(FN1, KC_LEFT), LT(FN2, KC_DOWN), RCTL_T(KC_RGHT)
    ),
    /*
     * Layer FN1 (hold Caps or hold Down)
     * ,-----------------------------------------------------------------------------------------.
     * |  `  |  F1 |  F2 |  F3 |  F4 |  F5 |  F6 |  F7 |  F8 |  F9 | F10 | F11 | F12 |  DELETE   |
     * |-----------------------------------------------------------------------------------------+
     * | Tab    |  q  | UP  |  e  |  r  |  t  |  y  |  u  |  i  |  o  | PS  | HOME | END |   \    |
     * |-----------------------------------------------------------------------------------------+
     * | Caps    |LEFT |DOWN |RIGHT|  f  |  g  |  h  |  j  |  k  |  l  | PGUP| PGDN |    Enter    |
     * |-----------------------------------------------------------------------------------------+
     * | Shift      |V-UP |V-DWN|MUTE |  v  |  b  |  n  |  m  |  ,  | INS | DEL  |    Shift       |
     * |-----------------------------------------------------------------------------------------+
     * | Ctrl  | Lock | TO FN2 |  Alt  |             space             | Alt  |  FN2  | Boot   |
     * \-----------------------------------------------------------------------------------------/
     */
    [FN1] = LAYOUT_60_ansi(
        KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL,
        _______, _______, KC_UP,   _______, _______, _______, _______, _______, _______, _______, KC_PSCR, KC_HOME, KC_END,  _______,
        _______, KC_LEFT, KC_DOWN, KC_RGHT, _______, _______, _______, _______, _______, _______, KC_PGUP, KC_PGDN, _______,
        _______, KC_VOLU, KC_VOLD, KC_MUTE, _______, _______, _______, _______, _______, KC_INS,  KC_DEL,  _______,
        _______, QK_LLCK, TO(FN2), _______, _______, _______, MO(FN2), QK_BOOTLOADER
    ),
    /*
     * Layer FN2 (VIA: CS_LAYER_MASK_F, QK_REBOOT, mouse keys; RGB controls)
     * ,-----------------------------------------------------------------------------------------.
     * |    |    |    |    |    |NEXT |PREV |HUE+ |HUE- |VAL+ |VAL- |SPD+ |SPD- |   Bksp   |
     * |-----------------------------------------------------------------------------------------+
     * | FN2  |MASK |MS_UP|    |REBOOT|    |    |    |    |    | PS  |HOME | END |                |
     * |-----------------------------------------------------------------------------------------+
     * |        |MS_L |MS_DN|MS_R |    |    |    |    |    | PGUP| PGDN |    Enter          |
     * |-----------------------------------------------------------------------------------------+
     * | ACL1  | WhL  | WhU  | WhD  | WhR  |    |    |    |    | INS | DEL  |    Shift         |
     * |-----------------------------------------------------------------------------------------+
     * | Ctrl  | Lock | ACL2 | Btn1 |               space             | FN1  | FN2  | Boot   |
     * \-----------------------------------------------------------------------------------------/
     */
    [FN2] = LAYOUT_60_ansi(
        _______, _______, _______, _______, _______, RM_NEXT, RM_PREV, RM_HUEU, RM_HUED, RM_VALU, RM_VALD, RM_SPDU, RM_SPDD, MS_BTN2,
        MO(FN2), CS_LAYER_MASK_F, MS_UP, _______, QK_REBOOT, _______, _______, _______, _______, _______, KC_PSCR, KC_HOME, KC_END,  _______,
        _______, MS_LEFT, MS_DOWN, MS_RGHT, _______, _______, _______, _______, _______, _______, KC_PGUP, KC_PGDN, _______,
        MS_ACL1, MS_WHLL, MS_WHLU, MS_WHLD, MS_WHLR, _______, _______, _______, _______, KC_INS,  KC_DEL,  _______,
        MS_ACL0, QK_LLCK, MS_ACL2, MS_BTN1, _______, MO(FN1), MO(FN2), QK_BOOTLOADER
    ),
    /*
     * Layer FN3 (same as FN2 but no layer-mask key; used for indicator logic)
     * ,-----------------------------------------------------------------------------------------.
     * |    |    |    |    |    |NEXT |PREV |HUE+ |HUE- |VAL+ |VAL- |SPD+ |SPD- |   Bksp   |
     * |-----------------------------------------------------------------------------------------+
     * | FN2  |    | UP  |    |REBOOT|    |    |    |    |    | PS  |HOME | END |                |
     * |-----------------------------------------------------------------------------------------+
     * |        |LEFT |DOWN |RIGHT|    |    |    |    |    |    | PGUP| PGDN |    Enter          |
     * |-----------------------------------------------------------------------------------------+
     * | Shift      |    |    |    |    |    |    |    |    |    | INS | DEL  |    Shift         |
     * |-----------------------------------------------------------------------------------------+
     * | Ctrl  | Win  |  Alt  |               space             | Alt  |  FN1  |  FN2  | Boot   |
     * \-----------------------------------------------------------------------------------------/
     */
    [FN3] = LAYOUT_60_ansi(
        _______, _______, _______, _______, _______, RM_NEXT, RM_PREV, RM_HUEU, RM_HUED, RM_VALU, RM_VALD, RM_SPDU, RM_SPDD, _______,
        MO(FN2), _______, KC_UP,   _______, QK_REBOOT, _______, _______, _______, _______, _______, KC_PSCR, KC_HOME, KC_END,  _______,
        _______, KC_LEFT, KC_DOWN, KC_RGHT, _______, _______, _______, _______, _______, _______, KC_PGUP, KC_PGDN, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_INS,  KC_DEL,  _______,
        _______, _______, _______, _______, _______, MO(FN1), MO(FN2), QK_BOOTLOADER
    ),
};

#ifdef OS_DETECTION_ENABLE

os_variant_t previous_os = OS_UNSURE;
#endif
enum{
KANA_LED = (1 << 0),
CAPS_LED = (1 <<1),
NUM_LED = (1<< 2),
SCROLL_LED = (1<<3),
COMPOSE_LED = (1<< 4)
};

unsigned char ledState = 0;
#define LEDSTRENG 200
#define ENABLEDELAY 300  // ms to wait until rgblight time out, 900K ms is 15min.

static bool     delayedEnable = false;
static uint32_t key_timer;

/* --- Helpers --- */

static uint8_t resolve_layer(layer_state_t state) {
    uint8_t layer = get_highest_layer(state);
    if (layer == 0) {
        layer = biton32(default_layer_state);
    }
    return layer;
}

uint8_t getLayer(void) {
    return resolve_layer(layer_state);
}

void maskLayer(uint8_t layer) {
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            uint8_t index = g_led_config.matrix_co[row][col];
            if (index == INDICATOR_KEY || index == BLUETOOTH_LED || index == BATTERY_LED) {
                continue;
            }
            keypos_t keypos  = {col, row};
            uint16_t keycode = keymap_key_to_keycode(layer, keypos);
            if (keycode == KC_NO || keycode == KC_TRNS) {
                g_led_config.flags[index] = 0;
                rgb_matrix_set_color(index, RGB_OFF);
            } else {
                g_led_config.flags[index] = 4;
            }
        }
    }
}

static void setIndicatorLed(uint8_t layer) {
    switch (layer) {
        case FN1:
            g_led_config.flags[INDICATOR_KEY] = 0;
            rgb_matrix_set_color(INDICATOR_KEY, 255, 0, 0);
            break;
        case FN2:
            g_led_config.flags[INDICATOR_KEY] = 0;
            rgb_matrix_set_color(INDICATOR_KEY, 0, 255, 0);
            break;
        case FN3:
            g_led_config.flags[INDICATOR_KEY] = 0;
            rgb_matrix_set_color(INDICATOR_KEY, 0, 0, 255);
            break;
        case BASE:
            g_led_config.flags[INDICATOR_KEY] = 4;
            break;
        default:
            break;
    }
}

static void check_rgb_timeout(void) {
    if (delayedEnable && timer_elapsed32(key_timer) > ENABLEDELAY) {
        rgb_matrix_enable();
        delayedEnable = false;
    }
}

/* --- Custom --- */

void panelIndicators(void) {
    if (panelLedEnable == false) {
        rgb_matrix_set_color(BATTERY_LED, 0, 0, 0);
        rgb_matrix_set_color(BLUETOOTH_LED, 0, 0, 0);
        return;
    }
#ifdef OS_DETECTION_ENABLE
    if (BLUETOOTHSTATE == false) {
        switch (previous_os) {
            case OS_UNSURE:
                rgb_matrix_set_color(BLUETOOTH_LED, 0, 100, 100);
                break;
            case OS_LINUX:
                rgb_matrix_set_color(BLUETOOTH_LED, 255, 255, 30);
                break;
            case OS_WINDOWS:
                rgb_matrix_set_color(BLUETOOTH_LED, 0, 0, 255);
                break;
            case OS_MACOS:
                rgb_matrix_set_color(BLUETOOTH_LED, 255, 0, 255);
                break;
            case OS_IOS:
                break;
        }
    }
    rgb_matrix_set_color(BLUETOOTH_LED, 255, 0, 255);
#endif
    short w = BATTERYENABLED ? 50 : 0;
    rgb_matrix_set_color(BATTERY_LED, (ledState & CAPS_LED) * LEDSTRENG + w, (ledState & NUM_LED) * LEDSTRENG + w, (ledState & SCROLL_LED) * LEDSTRENG + w);
}

void changeLayerMask(bool reverse) {
    if (reverse) {
        if (layer_MASK == 1) {
            layer_MASK = 0;
        }
    } else {
        if (layer_MASK == 0) {
            layer_MASK = 1;
        }
    }
    if (layer_MASK > 0) {
        maskLayer(getLayer());
    } else {
        maskLayer(biton32(default_layer_state));
    }
}

int setIndicator(uint8_t layer) {
    if (layer_MASK > 0) {
        maskLayer(layer);
    }
    setIndicatorLed(layer);
    return 0;
}

static void updateLedStateFromHost(led_t led_state) {
    ledState = 0;
    if (led_state.num_lock) {
        ledState = NUM_LED;
    }
    if (led_state.scroll_lock) {
        ledState |= SCROLL_LED;
    }
    if (led_state.caps_lock) {
        ledState |= CAPS_LED;
    }
    if (led_state.compose) {
        ledState |= COMPOSE_LED;
    }
    if (led_state.kana) {
        ledState |= KANA_LED;
    }
    panelIndicators();
}

/* --- QMK hooks --- */

void keyboard_pre_init_user(void) {
    g_led_config.flags[BATTERY_LED]   = 0;
    g_led_config.flags[BLUETOOTH_LED] = 0;
    rgb_matrix_set_color(BATTERY_LED, 0, 0, 0);
    rgb_matrix_set_color(BLUETOOTH_LED, 255, 0, 0);
}

void keyboard_post_init_user(void) {
#ifdef OS_DETECTION_ENABLE
    previous_os = detected_host_os();
#endif
    panelIndicators();
}

void suspend_power_down_user(void) {
#ifdef OS_DETECTION_ENABLE
    previous_os = OS_UNSURE;
#endif
    if (!BATTERYENABLED) {
        rgb_matrix_disable();
    } else {
        rgb_matrix_enable();
    }
    panelIndicators();
}

void suspend_wakeup_init_user(void) {
    delayedEnable = true;
    key_timer     = timer_read32();
#ifdef OS_DETECTION_ENABLE
    os_variant_t os = detected_host_os();
    previous_os     = os;
#endif
    panelIndicators();
}

void housekeeping_task_user(void) {
    check_rgb_timeout();
}

#if defined(DIP_SWITCH_PINS) || defined(DIP_SWITCH_MATRIX_GRID)
bool dip_switch_update_user(uint8_t index, bool active) {
    switch (index) {
        case 0:
            BLUETOOTHSTATE = active;
            break;
        case 1:
            BATTERYENABLED = active;
            break;
    }
    panelIndicators();
    return true;
}
#endif

bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);
    if (res) {
        updateLedStateFromHost(led_state);
    }
    return res;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case CS_LAYER_MASK_F:
            if (record->event.pressed) {
                changeLayerMask(layer_MASK == 1);
            }
            return false;
        case CS_LAYER_MASK_B:
            if (record->event.pressed) {
                changeLayerMask(true);
            }
            return false;
        case RD_PANEL_TG:
            if (record->event.pressed) {
                panelLedEnable = !panelLedEnable;
                panelIndicators();
            }
            return false;
        default:
            return true;
    }
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (INDICATOR_KEY >= led_min && INDICATOR_KEY <= led_max) {
        setIndicatorLed(resolve_layer(layer_state));
    }
    if ((BATTERY_LED >= led_min && BATTERY_LED <= led_max) || (BLUETOOTH_LED >= led_min && BLUETOOTH_LED <= led_max)) {
        panelIndicators();
    }
    return true;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    setIndicator(resolve_layer(state));
    return state;
}
