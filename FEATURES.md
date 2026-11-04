# Redragon K530 – Keymap features

This document explains the **default and VIA keymaps** for the K530 Devarajas: layers, DIP switches, the three extra LEDs, and how to use them.

---

## Overview

The K530 has hardware that many other Redragon boards don’t:

- **Two switch pins** – the Bluetooth slot sense line, and the cable/wireless switch.
- **Three extra LEDs** – one on a key (layer indicator) and two on the case (Bluetooth and Battery/status).

The keymaps use **layers**: you hold a key to get a different set of actions (F-keys, RGB, reboot, etc.) and release to go back to normal typing. No need to remember everything; the **Quick reference** table at the end covers the main actions.

---

## 1. Layers – what they are and how to use them

A **layer** is an extra keymap that is active only while you hold a modifier key. On the K530:

| Layer | How you get there | What it’s for |
|-------|-------------------|----------------|
| **BASE** | (default) | Normal typing – letters, numbers, modifiers. |
| **FN1** | Hold **Caps Lock** (tap = Caps, hold = FN1) | F-keys (F1–F12), media, navigation, **Bootloader**. |
| **FN2** | Hold **Down Arrow** (bottom row, second from right) | **Bluetooth** keys, RGB controls, **layer mask**, **reboot**, **Bootloader**. |
| **FN3** | (internal) | Same as FN2 but without the layer-mask key; used for LED logic. |

**Entering the bootloader (for flashing):** Hold **Caps** or **Down**, then press **Right Ctrl** (the rightmost key).

---

## 2. Switch pins

QMK lists two pins as DIP switches. They are not a Win/Mac switch and a power switch.

| Pin | What it is | What the keymap does with it |
|-----|------------|------------------------------|
| **A6** (called DIP 0) | Bluetooth slot sense line. `A7` is driven low, then high, and `A6` is sampled both times to read BT1, BT2, or BT3. | Nothing in the keymap. The slot colour is painted from the radio's own channel while wireless. |
| **B13** (called DIP 1) | Cable/wireless switch (P1.13). High is cable. | Cable: the battery LED is green, and RGB can stay on while the PC sleeps. Wireless: the battery LED is white while Caps Lock is on, and RGB turns off when the PC sleeps. |

---

## 3. The three extra LEDs

The K530 has **63 RGB LEDs**: 60 under the keys and **3 extra** used for status.

| LED | Where it is | What it shows |
|-----|-------------|----------------|
| **Indicator (key LED 28)** | One of the key LEDs (e.g. above a modifier) | **Current layer**: Red = FN1, Green = FN2, Blue = FN3. On BASE it follows the normal RGB effect. With layer mask on, it still shows the layer colour. |
| **Battery LED (61)** | On the case (battery/status) | **Cable:** solid green. That is the plugged-in light. **Wireless:** white while Caps Lock is on, off otherwise. |
| **Bluetooth LED (62)** | On the case (Bluetooth) | **Wireless:** slot colour (see section 10). **Wired:** the OS colour in the next section, on every slot. |

---

## 4. OS detection (Bluetooth LED while wired)

The OS colour is only visible while the keyboard is wired. While wireless, LED 62 shows the Bluetooth slot instead. The slot switch does not change the wired colour.

| Detected OS | Bluetooth LED |
|-------------|----------------|
| Unsure | Cyan |
| Linux | Yellow |
| Windows | Blue |
| macOS | Magenta |

So while wired, the Bluetooth LED shows which OS the keyboard thinks it is talking to.

---

## 5. Custom keycodes – what the special keys do

These keycodes are handled in the keymap (e.g. in `process_record_user()`).

| Keycode | Default position | What it does |
|---------|------------------|----------------|
| **MY_REBOOT** (default) / **QK_REBOOT** (VIA) | **FN2**, key **above Tab** (same column as Q) | **Software reboot** – restarts the MCU, does **not** enter bootloader. |
| **QK_LAYER_MASK** (default) / **CS_LAYER_MASK_F** (VIA) | **FN2**, key at **Q** | **Toggle layer mask.** Press once = mask on (RGB lights only keys that do something on the current layer). Press again = mask off. |
| **QK_LAYER_NO_MASK** / **CS_LAYER_MASK_B** (VIA) | Not on default keymap | Turn layer mask off (you can bind it in VIA if you like). |
| **QK_TOGGLE_PANEL_LED** / **RD_PANEL_TG** (VIA) | Not bound by default | **Toggle panel LEDs** – turns the Bluetooth and Battery LEDs on or off; key LEDs unchanged. |

**In practice (default keymap):**

- **Reboot:** FN2 → key above Q.
- **Layer mask:** FN2 → **Q** toggles mask on/off (no Shift needed).

---

## 6. Layer mask – “light only the keys that do something”

When **layer mask is on**:

- For the **current** layer, each key LED is checked: if that key is “empty” on this layer (`KC_NO` or `KC_TRNS`), its LED is **off**; otherwise it uses the normal RGB effect.
- The **indicator LED** still shows the layer colour (red/green/blue).
- **Bluetooth** and **Battery** LEDs are unchanged.

So you get a clear “preview” of which keys have an action on the layer you’re on.

- **Toggle:** FN2 → **Q** (QK_LAYER_MASK / CS_LAYER_MASK_F). Same key turns the mask on and off. Optionally bind CS_LAYER_MASK_B in VIA for “mask off only”.

---

## 7. Panel LED toggle

- A variable `panelLedEnable` (default **true**) controls whether the **Bluetooth** and **Battery** LEDs are on.
- The keycode **QK_TOGGLE_PANEL_LED** (default) or **RD_PANEL_TG** (VIA) toggles this. It’s not bound by default; you can assign it to any key in your keymap or in VIA.
- When **off**, only those two LEDs are forced off; the rest of RGB (including the indicator key LED) is unchanged.

---

## 8. Suspend and wake

- **When the PC wakes:** The firmware refreshes OS detection, may **auto-reboot once** if the OS was “unsure” at boot and becomes known after a short delay (~7 s), and updates the panel LEDs.
- **RGB after wake:** After a short delay (e.g. 300 ms), RGB is turned back on unless the current state keeps it off.
- **When the PC sleeps:** In the battery position, RGB is turned off. In the cable position, RGB can stay on and the panel LEDs are updated.

---

## 9. VIA keymap – same behaviour, different names

The **VIA** keymap does the **same things** as the default keymap but uses keycode names that work well with VIA/Remapping:

| Default keymap | VIA keymap | Function |
|----------------|------------|----------|
| MY_REBOOT | QK_REBOOT | Reboot |
| QK_LAYER_MASK | CS_LAYER_MASK_F | Layer mask toggle (FN2+Q) |
| (none) | CS_LAYER_MASK_B | Layer mask off only |
| QK_TOGGLE_PANEL_LED | RD_PANEL_TG | Panel LED toggle |

On **FN2** in VIA: **CS_LAYER_MASK_F** at “Q”, **QK_REBOOT** above Tab. **RD_PANEL_TG** is not bound by default; you can assign it in VIA. Bootloader is still **hold FN (Caps or Down) + Right Ctrl**.

---

## 10. Bluetooth

The board talks to its PixArt PAR2801 radio over SPI (`k530_bt.c`). The radio does the Bluetooth part itself and keeps the pairings for three host slots.

**Wired or wireless.** The mode switch (P1.13) decides. In the battery (BT) position the board is always wireless, even with a USB cable plugged in. In the cable position it is wired once USB connects, and wireless if USB never comes up within 3 s of boot.

**Host slot.** The BT1/BT2/BT3 switch picks the slot. The radio advertises as `Draconic -1`, `-2` or `-3`. Moving the switch reconnects to the host saved in that slot.

**FN2 + Esc** (both keymaps):

| Key | Keycode | What it does |
|-----|---------|--------------|
| FN2 + **Esc** | `K530_BT_PAIR` | **Tap:** reconnect to the saved host. **Hold 3 s:** pair a new host on the current slot. Wireless only. |

**Pairing.** Hold FN2+Esc for 3 s, then pair `Draconic -N` from the host. The host has to confirm a passkey. On Linux that needs a running Bluetooth agent (desktop applet, or `bluetoothctl` with `agent on` and `default-agent`).

**After flashing.** Flashing resets only the keyboard chip. If the host doesn't reconnect by itself, tap FN2+Esc.

**Bluetooth LED (case LED 62) while wireless.** The colour is the slot: red BT1, green BT2, blue BT3. Blinking matches the stock firmware; unlike stock, the LED stays dimly lit afterwards so the active slot is always visible.

| Radio state | LED |
|-------------|-----|
| Pairing (state 1) | Fast blink, about 0.6 s per cycle. Starts as soon as you hold FN2+Esc for 3 s. Stops after 60 s if nothing else is reported. |
| Reconnecting (state 2) | Slow blink, about 1.9 s per cycle. Starts on an FN2+Esc tap or a slot change. Stops after 10 s if nothing else is reported. |
| Connected or idle (state 0) | Bright for 2 s after a blink ends, then dim. |

When wired, the LED always shows the keymap's panel colour.

**Six keys.** A Bluetooth report carries at most six keys. A seventh is dropped. The overflow area of the radio frame (bytes 11..25) stays zero; filling it made the radio emit a second report.

**Media keys.** Volume and mute go out in the same key frame, bytes 33 and 34. A media key-up clears those bytes and leaves any held letters in place. System keys (power, sleep) are not sent: the stock image queues a key frame for them but never copies the system usage into it.

**Host lock lights.** While wireless, the battery LED is white when Caps Lock is on. The radio's status reply puts the host LED byte in its third payload byte, and bit 1 of that byte is Caps Lock. Num Lock used to light the same LED green, which read as a cable indicator. The fifth payload byte is saved and not shown: stock compares it with 1, 2, and `0xF0` for a different lamp, which is not a battery level.

**Mouse.** The same key frame carries the pointer, in the layout of the radio's HID report 5. Byte 26 is the buttons. Bytes 27–28 and 29–30 are 16-bit X and Y, least significant byte first. Byte 31 is the wheel and byte 32 is horizontal scroll. Movement is sent once so a later key does not nudge the cursor again. A held button stays until it is released. A held direction repeats the same delta, and the radio drops a frame that matches the last one, so each step flips a padding bit in the button byte.

On the **VIA** keymap, hold **Down**: **W A S D** move the pointer, **Space** is left click, **Backspace** is right click, and **X C V B** are the wheel. On the **default** keymap only the clicks are bound: **FN2+Space** left, **FN2+Backspace** right.

---

## 11. Quick reference

| Goal | How |
|------|-----|
| **Enter bootloader** | Hold **Caps** or **Down**, then press **Right Ctrl**. |
| **Reboot (no bootloader)** | FN2 → key **above Q** (MY_REBOOT / QK_REBOOT). |
| **Layer mask on/off** | FN2 → **Q** toggles (QK_LAYER_MASK / CS_LAYER_MASK_F). Or bind CS_LAYER_MASK_B for “off only”. |
| **Panel LEDs on/off** | Bind QK_TOGGLE_PANEL_LED or RD_PANEL_TG to a key and press it. |
| **Cable vs wireless** | **B13** switch. Pin `A6` is the Bluetooth slot sense line, not a Win/Mac switch. |
| **Pair a Bluetooth host** | Hold **Down**, hold **Esc** 3 s, then pair `Draconic -N` on the host. |
| **Reconnect Bluetooth** | Hold **Down**, tap **Esc**. |
| **Bluetooth slot** | BT1/BT2/BT3 switch. |
| **Indicator LED** | Shows current layer (red/green/blue); automatic. |
| **Bluetooth LED** | Wireless: slot colour, blinking while pairing/reconnecting. Wired: OS colour (cyan, yellow, blue, or magenta) on every slot. |
| **Battery LED** | Cable: solid green. Wireless: white while Caps Lock is on. |
| **More than six keys** | Not sent over Bluetooth. The seventh key is dropped. |
| **Mouse, VIA keymap** | Hold **Down**. **W A S D** move, **Space** left click, **Backspace** right click. |

Behaviour is implemented in `keymaps/default/keymap.c` and mirrored in `keymaps/via/keymap.c`. You can rebind these keycodes in your own keymap or in VIA; the behaviour stays the same.
