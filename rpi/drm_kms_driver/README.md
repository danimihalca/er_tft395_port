# lt7680-drm for Raspberry Pi OS

DRM/KMS driver for the Levetop LT7680 on the EastRising ER-TFT3.95-1 (720x720), as a DKMS
module plus a device-tree overlay.

## Install

    sudo ./install.sh
    # then add to /boot/firmware/config.txt (or /boot/config.txt):
    dtoverlay=er-tft395
    sudo poweroff        # wire the display with the Pi off, then boot

## Wiring (Pi 3/4/5, SPI0 CE0)

| Display pin | Signal | Pi GPIO | Header pin |
|---|---|---|---|
| 1, 2 | GND | GND | 6 |
| 3, 4 | VCC | 3.3V or 5V (per your module) | 1 or 2 |
| 5 | CS | GPIO8 (CE0) | 24 |
| 6 | MISO | GPIO9 | 21 |
| 7 | MOSI | GPIO10 | 19 |
| 8 | SCK | GPIO11 | 23 |
| 9 | BL | 3.3V (tutorial: HIGH = on) | 17 |
| 11 | RES | GPIO25 | 22 |
| 16 | panel CS | GPIO5 | 29 |
| 18 | panel SDI | GPIO13 | 33 |
| 19 | panel SCK | GPIO6 | 31 |

