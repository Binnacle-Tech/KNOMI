# KNOMI · OctoPrint edition

A fork of [BigTreeTech's KNOMI firmware](https://github.com/bigtreetech/KNOMI) (the `firmware` branch) that works with **OctoPrint** as well as Moonraker/Klipper. It also adds a handful of features people have been asking for.

Companion plugin: **[OctoPrint-KNOMI](https://github.com/Binnacle-Tech/OctoPrint-KNOMI)**

## What's different from stock

- **OctoPrint backend.** Pick Moonraker or OctoPrint on the web page. OctoPrint mode uses an application key.
- **Live updates** over OctoPrint's websocket, with HTTP polling as the fallback.
- **Find OctoPrint on network** (mDNS) on the settings page.
- **Bluetooth LE link** to the plugin (optional). WiFi can turn off while Bluetooth is connected and comes back on a timeout.
- **Custom animations.** Upload a GIF to any slot from the web page, with no reflash. They're stored in the 7 MB flash partition.
- **More animation states:** input shaping, PID tuning, nozzle cleaning, filament load/unload, and paused.
- **Printing screen** with time left, temperatures, Z/layer and file name.
- **Brightness is saved**, plus auto-dim and screen-off, wake on touch or printer activity.
- **Animations can follow the UI color.**
- **Web UI restyled**, with dark, medium, light and high-contrast modes.

Your existing settings carry over when you flash this on top of stock firmware.

## Build

```
pio pkg install -e knomiv2
# LVGL display fix (see platformio.ini):
cp "lv_disp_(bugfix_backup).c" .pio/libdeps/knomiv2/lvgl/src/core/lv_disp.c
pio run -e knomiv2
```

Flash `.pio/build/knomiv2/firmware.bin` from `http://<knomi-ip>/update`, or with `pio run -e knomiv2 -t upload` over USB. Use `-e knomiv1` for the original KNOMI.

## Setup

See **[OCTOPRINT.md](OCTOPRINT.md)** for backend setup, the plugin, Klipper macros (`knomi_octoprint.cfg`), Bluetooth pairing, display settings and the command mapping.

## Credits

The original firmware, UI and animations are by [BIGTREETECH](https://github.com/bigtreetech/KNOMI). The upstream repository has no license file ([bigtreetech/KNOMI#54](https://github.com/bigtreetech/KNOMI/issues/54)). The changes in this fork are provided on the same terms as the upstream code.
