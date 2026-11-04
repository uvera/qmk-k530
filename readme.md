# Redragon K530 Devarajas

K530

**Keymap features (DIP switches, panel LEDs, layer mask, reboot, bootloader, Bluetooth keys):** see [FEATURES.md](FEATURES.md).

## Bluetooth

Bluetooth works with this firmware: pairing, reconnecting, and typing go through the board's PixArt PAR2801 radio, the same way the stock firmware does it. The driver is `k530_bt.c`; the stock radio protocol it follows is written up in [stock_radio.md](stock_radio.md).

- **Use Bluetooth:** put the mode switch in the battery (BT) position. The board is wired only when that switch is in the cable position *and* USB is connected.
- **Pick a host slot:** the BT1/BT2/BT3 switch. The radio advertises as `Draconic -1`, `-2` or `-3` for the slot.
- **Pair a new host:** hold **Down** (FN2) and hold **Esc** for 3 seconds. Then pair `Draconic -N` from the host.
- **Reconnect:** hold **Down** (FN2) and tap **Esc**. This is useful when the host doesn't reconnect by itself after you flash.

Pairing asks the host to confirm a passkey. On Linux that needs a running Bluetooth agent (your desktop's Bluetooth applet, or `bluetoothctl` with `agent on` and `default-agent`). Without one, BlueZ logs `No agent available for request type 2` and pairing fails at once.

Flashing resets only the keyboard chip, not the radio. If the host doesn't reconnect after flashing, tap FN2+Esc.

Config files for K530 for [SonixQMK](https://github.com/SonixQMK/qmk_firmware) for development branch.
To install, follow instructions on [https://sonixqmk.github.io//SonixDocs/install/](https://sonixqmk.github.io//SonixDocs/install/)
to point 2.1.

Next, use those instructions.

```
qmk setup SonixQMK/qmk_firmware -b sn32_develop
cd ~/qmk_firmware/keyboards/redragon

git clone https://github.com/uvera/qmk-k530.git k530

qmk compile -kb redragon/k530 -km via
```
And continue instructions [https://sonixqmk.github.io//SonixDocs/install/](https://sonixqmk.github.io//SonixDocs/install/#4-flashing-the-firmware)
from point 4.0.


![](./assets/preview.jpg)

Config was based on files for [K530 on master branch](https://github.com/SonixQMK/qmk_firmware/tree/sn32_master/keyboards/redragon/k530)
and [k630 on develepment branch](https://github.com/SonixQMK/qmk_firmware/tree/sn32_develop/keyboards/redragon/k630)

