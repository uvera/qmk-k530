#pragma once

#include "quantum.h"

/*
 * QK_KB_0..2 are taken by the VIA keymap's USER00 range.
 */
enum k530_keycodes {
    K530_BT_PAIR = QK_KB_8, /* tap: reconnect, hold 3 s: pair a new host */
    K530_BT_STATUS,         /* toggle the radio status display */
    K530_BT_RESTORE,        /* send 0xA8 with the stock default value 1 */
    K530_BT_WIRELESS,       /* resend the stock switch-to-wireless 0xA5 */
    K530_BT_PROBE,          /* 0xAA frame with byte j = keycode 4 + j - 3, to map the radio's layout */
};
