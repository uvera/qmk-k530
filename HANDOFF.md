# K530 Bluetooth port – handoff notes

State of the work as of 2026-11-03, for picking it up in a new session.
Protocol details live in `stock_radio.md`; user-facing behaviour in
`FEATURES.md` section 10.

## Setup

- This directory is its own git repo (`git@github.com:uvera/qmk-k530.git`,
  branch `main`). It sits inside SonixQMK `qmk_firmware`, branch
  `sn32_develop`. Don't push the outer repo.
- Build: `qmk compile -kb redragon/k530 -km default` and `-km via`. The
  binaries land in the `qmk_firmware` root (`redragon_k530_*.bin`).
- The VIA build is RAM-tight. Adding statics to `k530_bt.c` overflowed it
  by 8 bytes once; keep new buffers small.
- `CONSOLE_ENABLE` builds break USB enumeration (only interface 0 comes
  up). The debug keys, the status display (FN2 + 1) and the console log
  ring were removed once BT worked; `80919fe` still has them.

## Reverse-engineering material (not in git)

Kept in `qmk_firmware/k530_porting/` (untracked, vendor code):

- `K530_Draconic_1222.exe`: stock updater.
- `work/k530_sn32.bin`: stock SN32F248BF image, base 0. Disassemble with
  `r2 -a arm -b 16 -e asm.cpu=cortex`.
- `work/k530_full.asm`: full disassembly; `work/k530_decomp.txt` and
  `work/k530_decompile.py`: Ghidra output and script.
- `work/k530_cap.py`: evdev capture of the `Draconic` input device, to
  see what the host actually receives over BT.
- `work/k530_pair.sh`: pair, trust and connect a slot with
  `bluetoothctl --agent NoInputNoOutput`. Edit the address inside.

Key stock addresses: LED and radio state `0x6868`, `0x674A`, `0x6694`;
0xAA key report builder `0x442A`; 0xA7 status parse `0x9DAA`; zero pulse
`0x43C`; 0xA5 `0x30E`; 0xA6 `0x336`; channel switch `0x5512`/`0x5724`/`0x56B6`.

## What works

- Pairing (FN2 + Esc held 3 s) and reconnect (tap) per BT slot.
- Typing over BT, including key-up.
- BT case LED 62 shows the radio state and, dimly, the active slot.

## Lessons that cost the most time

- **SPI IRQ priority.** It must stay at 1. At 3 it ran below the RGB PWM
  timer CT16B1 (priority 2, which also scans the matrix). Late replies
  shifted every frame by 2 bytes, giving uppercase letters, phantom keys
  and typing only when mashing keys.
- **0xAA frame layout.** Byte 2 = 0, mods/reserved/6 keys in bytes 3..10,
  bitmap bytes 11..25 zero, 0x2C in bytes 35 and 36, checksum in byte 38.
  Filling the bitmap makes the radio send an extra report.
- **Key-up.** Always publish the empty report after a keyed one, waiting
  at most 50 ms for the key-down to be read. Each 0xAA is re-pulsed 5
  times, 10 ms apart; more made things worse.
- **No polling.** The radio pushes 0xA7 by itself; stock never polls.
- **Host side.** BlueZ pairing fails with "No agent available" unless an
  agent runs (`bluetoothctl` with `agent on`, or `k530_pair.sh`).
  Flashing doesn't reset the radio; tap FN2 + Esc to reconnect.

## Open items

- **BT2 connect/drop loop.** `FB:E9:91:84:A9:77` (`Draconic -2`) connects and
  drops every ~2 s ("ACL packet for unknown connection handle" in
  `journalctl -k`). Slot 1 (`FB:E9:91:84:89:77`) is fine. The slots have
  different addresses; the likely cause is a bad bond from a pairing
  attempt without an agent. The BlueZ entry was removed; next step is to
  re-pair slot 2 with `k530_pair.sh` and check whether the loop stops.
  If it persists after a clean pair, compare with stock firmware on the
  same slot.
- **Git history.** An amend squashed `548965e` ("set debounce to 10ms")
  and a WIP commit into `aa167de`. The content is all there; the history
  could be rebuilt if wanted.
- Not done: consumer/system keys over BT are sent but untested; battery
  level and host LED state (`bluetooth_keyboard_leds`) aren't implemented.
