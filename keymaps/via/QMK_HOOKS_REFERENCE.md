# QMK hooks reference (via keymap)

Documentation links and short descriptions for each QMK callback used in this keymap.

---

## Initialization

### `keyboard_pre_init_user(void)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Keyboard Pre Initialization](https://docs.qmk.fm/custom_quantum_functions#keyboard-pre-initialization-code)
- **When:** Very early during startup, before USB and before most other init.
- **Use for:** Hardware-oriented setup (e.g. LED pins). Most keymaps use `keyboard_post_init_user` instead.

### `keyboard_post_init_user(void)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Keyboard Post Initialization](https://docs.qmk.fm/custom_quantum_functions#keyboard-post-initialization-code)
- **When:** Very last task in `keyboard_init`; features are initialized.
- **Use for:** RGB, EEPROM, or other setup that needs the system fully up.

---

## Input / key handling

### `process_record_user(uint16_t keycode, keyrecord_t *record)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Programming the Behavior of Any Keycode](https://docs.qmk.fm/custom_quantum_functions#programming-the-behavior-of-any-keycode)
- **When:** Every key press and release, before normal key handling.
- **Return:** `true` = let QMK handle the key; `false` = skip normal handling (you handle it).
- **Use for:** Custom keycodes, overriding key behavior, or extending it (e.g. play sound and still send key).

### `layer_state_set_user(layer_state_t state)`
- **Docs:** [Layers → Layer Change Code](https://docs.qmk.fm/feature_layers#layer-change-code) and [Layer Change Code](https://docs.qmk.fm/custom_quantum_functions#layer-change-code)
- **When:** Whenever the layer state changes (e.g. `MO()`, `LT()`, `TG()`).
- **Return:** The (possibly modified) layer state to use.
- **Use for:** Layer indication (e.g. RGB by layer), tri-layer, or other layer-dependent logic.

---

## Power / suspend

### `suspend_power_down_user(void)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Keyboard Idling/Wake Code](https://docs.qmk.fm/custom_quantum_functions#keyboard-idlingwake-code)
- **When:** When the keyboard enters suspend/sleep.
- **Use for:** Turning off LEDs, stopping features, or other cleanup before sleep.

### `suspend_wakeup_init_user(void)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Keyboard Idling/Wake Code](https://docs.qmk.fm/custom_quantum_functions#keyboard-idlingwake-code)
- **When:** When the keyboard wakes from suspend.
- **Use for:** Restoring LEDs, state, or features after wake. Avoid relying on `IS_LAYER_ON()` for default layer here; use `layer_state_cmp(default_layer_state, LAYER)` instead.

---

## Housekeeping (main loop)

### `housekeeping_task_user(void)`
- **Docs:** [How to Customize Your Keyboard's Behavior → Keyboard housekeeping](https://docs.qmk.fm/custom_quantum_functions#keyboard-housekeeping)
- **When:** End of each main loop iteration (after matrix scan, USB, LEDs, etc.).
- **Use for:** Periodic tasks (e.g. RGB timeout, low-power checks). Keep it light; throttle if needed.

---

## DIP switch

### `dip_switch_update_user(uint8_t index, bool active)`
- **Docs:** [DIP Switches → Callbacks](https://docs.qmk.fm/features/dip_switch#callbacks)
- **When:** When a DIP switch changes state (only if `DIP_SWITCH_ENABLE = yes` and pins/matrix defined).
- **Args:** `index` = which switch; `active` = current on/off state.
- **Return:** `true` to allow further processing.
- **Use for:** Mode switching (e.g. Win/Mac, cable/BT) or other hardware toggles.

---

## LED indicators (host lock keys)

### `led_update_kb(led_t led_state)`
- **Docs:** [LED Indicators → LED update function](https://docs.qmk.fm/features/led_indicators#led-update-function)
- **When:** When one of the 5 HID LEDs changes (Num Lock, Caps Lock, Scroll Lock, Compose, Kana).
- **Args:** `led_state` holds `.num_lock`, `.caps_lock`, `.scroll_lock`, `.compose`, `.kana`.
- **Return:** Typically call `led_update_user(led_state)` and return its result; `true` allows default LED handling to run.
- **Use for:** Driving physical LEDs or mapping lock state to RGB (e.g. battery LED as Caps/Num/Scroll).

---

## RGB Matrix

### `rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max)`
- **Docs:** [RGB Matrix](https://docs.qmk.fm/features/rgb_matrix) (search for “indicators_advanced” or “Indicator callbacks”).
- **When:** After the current animation frame is rendered, before it is sent to the LEDs (per batch of LEDs).
- **Args:** `led_min`, `led_max` = range of LED indices for this batch (use to only touch relevant LEDs).
- **Use for:** Per-LED or per-key indicators (e.g. layer color on one key, Caps on a specific LED) without scanning the whole matrix every time. Can use `RGB_MATRIX_INDICATOR_SET_COLOR(i, r, g, b)` for range-safe updates.

---

## Central doc entry point

- **Customizing behavior (all hooks in one place):**  
  [How to Customize Your Keyboard's Behavior](https://docs.qmk.fm/custom_quantum_functions)
