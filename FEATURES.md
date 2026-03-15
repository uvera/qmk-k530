# Redragon K530 – Keymap features

This document explains the **default and VIA keymaps** for the K530 Devarajas: layers, DIP switches, the three extra LEDs, and how to use them.

---

## Overview

The K530 has hardware that many other Redragon boards don’t:

- **Two DIP switches** – set connection type (Win/Android vs Mac/iOS) and power mode (cable vs battery).
- **Three extra LEDs** – one on a key (layer indicator) and two on the case (Bluetooth and Battery/status).

The keymaps use **layers**: you hold a key to get a different set of actions (F-keys, RGB, reboot, etc.) and release to go back to normal typing. No need to remember everything; the **Quick reference** table at the end covers the main actions.

---

## 1. Layers – what they are and how to use them

A **layer** is an extra keymap that is active only while you hold a modifier key. On the K530:

| Layer | How you get there | What it’s for |
|-------|-------------------|----------------|
| **BASE** | (default) | Normal typing – letters, numbers, modifiers. |
| **FN1** | Hold **Caps Lock** (tap = Caps, hold = FN1) | F-keys (F1–F12), media, navigation, **Bootloader**. |
| **FN2** | Hold **Down Arrow** (bottom row, second from right) | RGB controls, **layer mask**, **reboot**, **Bootloader**. |
| **FN3** | (internal) | Same as FN2 but without the layer-mask key; used for LED logic. |

**Entering the bootloader (for flashing):** Hold **Caps** or **Down**, then press **Right Ctrl** (the rightmost key).

---

## 2. DIP switches (on the board)

Two physical switches on the keyboard are read by the firmware. You don’t press any key – just flip the switch.

| Switch | ON | OFF |
|--------|-----|-----|
| **DIP 0** | **Win/Android** – Bluetooth LED shows a simple “connected” style. | **Mac/iOS** – Bluetooth LED shows the **detected OS** color (see section 4). |
| **DIP 1** | **Cable** – Battery LED shows a fixed status; RGB can stay on when the PC is asleep. | **Battery (BT)** – Battery LED shows Caps/Num/Scroll lock; RGB turns off when the PC sleeps. |

So: DIP 0 = “how the Bluetooth LED behaves,” DIP 1 = “plugged in vs battery” and “RGB in sleep.”

---

## 3. The three extra LEDs

The K530 has **63 RGB LEDs**: 60 under the keys and **3 extra** used for status.

| LED | Where it is | What it shows |
|-----|-------------|----------------|
| **Indicator (key LED 28)** | One of the key LEDs (e.g. above a modifier) | **Current layer**: Red = FN1, Green = FN2, Blue = FN3. On BASE it follows the normal RGB effect. With layer mask on, it still shows the layer colour. |
| **Battery LED (61)** | On the case (battery/status) | **DIP 1 ON (cable):** fixed colour. **DIP 1 OFF (BT):** Caps lock (red), Num lock (green), Scroll lock (blue). |
| **Bluetooth LED (62)** | On the case (Bluetooth) | **DIP 0 ON (Win/Android):** “connected” style. **DIP 0 OFF (Mac/iOS):** colour by **detected OS** (see next section). |

---

## 4. OS detection (Bluetooth LED when DIP 0 = Mac/iOS)

When **DIP 0 is OFF** (Mac/iOS mode), the firmware tries to detect which OS the keyboard is connected to and sets the **Bluetooth LED** colour:

| Detected OS | Bluetooth LED |
|-------------|----------------|
| Unsure | Cyan |
| Linux | Yellow |
| Windows | Blue |
| macOS | Magenta |

So you can see at a glance which OS the keyboard thinks it’s talking to (in Mac/iOS mode).

---

## 5. Custom keycodes – what the special keys do

These keycodes are handled in the keymap (e.g. in `process_record_user()`).

| Keycode | Default position | What it does |
|---------|------------------|----------------|
| **MY_REBOOT** (default) / **QK_REBOOT** (VIA) | **FN2**, key **above Tab** (same column as Q) | **Software reboot** – restarts the MCU, does **not** enter bootloader. |
| **QK_LAYER_MASK** (default) / **CS_LAYER_MASK_F** (VIA) | **FN2**, key at **Q** | **Layer mask on.** RGB then lights only keys that do something on the current layer. **Shift + same key** = layer mask off. |
| **QK_LAYER_NO_MASK** / **CS_LAYER_MASK_B** (VIA) | Not on default keymap | Same as “layer mask off” (you can bind it in VIA if you like). |
| **QK_TOGGLE_PANEL_LED** / **RD_PANEL_TG** (VIA) | Not bound by default | **Toggle panel LEDs** – turns the Bluetooth and Battery LEDs on or off; key LEDs unchanged. |

**In practice (default keymap):**

- **Reboot:** FN2 → key above Q.
- **Layer mask on:** FN2 → **Q**. **Layer mask off:** FN2 → **Shift + Q**.

---

## 6. Layer mask – “light only the keys that do something”

When **layer mask is on**:

- For the **current** layer, each key LED is checked: if that key is “empty” on this layer (`KC_NO` or `KC_TRNS`), its LED is **off**; otherwise it uses the normal RGB effect.
- The **indicator LED** still shows the layer colour (red/green/blue).
- **Bluetooth** and **Battery** LEDs are unchanged.

So you get a clear “preview” of which keys have an action on the layer you’re on.

- **Turn on:** FN2 → **Q** (QK_LAYER_MASK / CS_LAYER_MASK_F).
- **Turn off:** FN2 → **Shift + Q** (or use CS_LAYER_MASK_B in VIA).

---

## 7. Panel LED toggle

- A variable `panelLedEnable` (default **true**) controls whether the **Bluetooth** and **Battery** LEDs are on.
- The keycode **QK_TOGGLE_PANEL_LED** (default) or **RD_PANEL_TG** (VIA) toggles this. It’s not bound by default; you can assign it to any key in your keymap or in VIA.
- When **off**, only those two LEDs are forced off; the rest of RGB (including the indicator key LED) is unchanged.

---

## 8. Suspend and wake

- **When the PC wakes:** The firmware refreshes OS detection, may **auto-reboot once** if the OS was “unsure” at boot and becomes known after a short delay (~7 s), and updates the panel LEDs.
- **RGB after wake:** After a short delay (e.g. 300 ms), RGB is turned back on unless the current state keeps it off.
- **When the PC sleeps:** If **DIP 1 is OFF** (BT mode), RGB is turned off. If **DIP 1 is ON** (cable), RGB can stay on and the panel LEDs are updated.

---

## 9. VIA keymap – same behaviour, different names

The **VIA** keymap does the **same things** as the default keymap but uses keycode names that work well with VIA/Remapping:

| Default keymap | VIA keymap | Function |
|----------------|------------|----------|
| MY_REBOOT | QK_REBOOT | Reboot |
| QK_LAYER_MASK | CS_LAYER_MASK_F | Layer mask on/off (Shift = off) |
| (none) | CS_LAYER_MASK_B | Layer mask off only |
| QK_TOGGLE_PANEL_LED | RD_PANEL_TG | Panel LED toggle |

On **FN2** in VIA: **CS_LAYER_MASK_F** at “Q”, **QK_REBOOT** above Tab. **RD_PANEL_TG** is not bound by default; you can assign it in VIA. Bootloader is still **hold FN (Caps or Down) + Right Ctrl**.

---

## 10. Quick reference

| Goal | How |
|------|-----|
| **Enter bootloader** | Hold **Caps** or **Down**, then press **Right Ctrl**. |
| **Reboot (no bootloader)** | FN2 → key **above Q** (MY_REBOOT / QK_REBOOT). |
| **Layer mask on** | FN2 → **Q** (QK_LAYER_MASK / CS_LAYER_MASK_F). |
| **Layer mask off** | FN2 → **Shift + Q** (or CS_LAYER_MASK_B in VIA). |
| **Panel LEDs on/off** | Bind QK_TOGGLE_PANEL_LED or RD_PANEL_TG to a key and press it. |
| **Win vs Mac / Cable vs BT** | Use the **DIP switches** on the board. |
| **Indicator LED** | Shows current layer (red/green/blue); automatic. |
| **Bluetooth LED** | DIP 0 OFF = OS colour; DIP 0 ON = “connected” style. |
| **Battery LED** | DIP 1 ON = cable; DIP 1 OFF = Caps/Num/Scroll lock. |

Behaviour is implemented in `keymaps/default/keymap.c` and mirrored in `keymaps/via/keymap.c`. You can rebind these keycodes in your own keymap or in VIA; the behaviour stays the same.
