# KNOMI + OctoPrint setup

> Only tested on a KNOMI 2 with OctoPrint on a Raspberry Pi 5.

This fork adds an OctoPrint backend next to the stock Moonraker one. You pick the backend on the KNOMI web page.

## 1. Flash
Build with PlatformIO: `pio pkg install -e knomiv2`, copy `lv_disp_(bugfix_backup).c` over
`.pio/libdeps/knomiv2/lvgl/src/core/lv_disp.c`, then `pio run -e knomiv2`, or use the prebuilt `release/knomiv2-octoprint-firmware.bin`.
The first boot after flashing formats the GIF storage, which adds a few seconds once.
If the KNOMI is already on WiFi, open `http://<knomi-ip>/update` and upload the .bin (OTA).
Saved WiFi and printer settings carry over. The backend defaults to Moonraker until you change it.

## 2. Configure (KNOMI web page)
- Backend: OctoPrint
- Printer IP / Port: press **Find OctoPrint on network** to pick it from a list (uses OctoPrint's
  mDNS announcement, on by default). Or type it in: port 80 on OctoPi, 5000 on a bare install.
- Tool ID: tool0
- OctoPrint API Key: make one in OctoPrint → User Settings → Application Keys.
  Use an admin user if you want Host Reboot/Shutdown/Restart OctoPrint from the KNOMI.

## 3. OctoPrint plugin (optional, recommended)
`OctoPrint-KNOMI` provides `/api/plugin/knomi`, which drives the homing / probing / QGL animations.
Install it from OctoPrint → Plugin Manager → "... from an uploaded file" (upload `OctoPrint-KNOMI.zip`),
or run `pip install ./OctoPrint-KNOMI` in OctoPrint's venv, then restart OctoPrint.
Without the plugin, everything works except those three animations. Heating is still detected from temperatures.

## 4. Klipper macros (optional, for steps run inside PRINT_START)
The plugin can only see commands that OctoPrint sends itself (G28, QUAD_GANTRY_LEVEL, BED_MESH_CALIBRATE, M109/M190...).
To make the steps inside your macros show up, include `knomi_octoprint.cfg` and replace each
`SET_GCODE_VARIABLE MACRO=_KNOMI_STATUS VARIABLE=x VALUE=True|False`
with `_KNOMI_SET VAR=x VALUE=1|0`. That macro still sets the variable (so Mainsail keeps working),
and it also prints `// KNOMI x=1`, which the plugin reads.

## Live updates
The KNOMI opens OctoPrint's websocket (`/sockjs/websocket`) and gets state pushed about twice a second.
The plugin pushes its flags the moment they change. If the socket drops or goes quiet for 5s,
the KNOMI falls back to HTTP polling until the socket is back. It also resyncs over HTTP every 10s.

## Extra animation states
The KNOMI has four more states: **input shaping, PID tuning, nozzle cleaning, filament load/unload**.
The plugin raises them on these commands:
- shaping: SHAPER_CALIBRATE, TEST_RESONANCES
- pid_tuning: PID_CALIBRATE, M303
- cleaning: CLEAN_NOZZLE, NOZZLE_CLEAN, WIPE_NOZZLE, NOZZLE_WIPE
- filament: LOAD_FILAMENT, UNLOAD_FILAMENT, M701, M702

If your macros have other names, call `_KNOMI_SET VAR=cleaning VALUE=1` / `VALUE=0` inside them.
Each state has its own built-in animation, as does printing. You can replace any of them (next section). Faces (idle, pause, print starting, after the print) are Coaster.

## Custom animations
Open `http://<knomi-ip>/gifs` (or use the link under the printer settings). Every animation has a slot:
WiFi setup, homing, probing, QGL, the four new states,
paused, heated, printing, finished. Upload a GIF to any slot and it applies right away. Use Restore to
go back to the built-in. Files live in the 7MB flash partition, so they survive firmware updates.
Limits: 1.5MB per GIF and 5MB loaded in total. The screen is 240x240 and round.

## Display settings (settings page, section 02)
- **Brightness** is saved now (the slider on the KNOMI saves too), so it survives restarts.
- **Dim after / screen off after (minutes):** 0 means never. Any touch, a screen change, or the printer
  starting something wakes it. The tap that wakes a dark screen doesn't press anything.
- **While the printer is busy:** stay awake (default), or dim and sleep as usual.
- **Printing screen:** *Info* shows the file name, %, time left (or elapsed), nozzle/bed temps, and Z height
  (Layer x/y on Moonraker when the slicer reports layers). *Accelerometer* is the stock bars view.
- **Animations follow the UI color:** recolors the built-in animations to the UI color picked on the KNOMI.
  on or off. The green "print finished" check stays green. Uploaded GIFs are never recolored.


## Screen, animations and presets (settings page, sections 03 and 04)
Everything the touchscreen menus set, plus a few things they can't. Changes apply right away, even mid-print.
- **UI color** (same as Settings › UI color on the KNOMI).
- **Back to Coaster after:** seconds of no touch before a menu closes (0 = never).
- **Heating screens:** turn the nozzle/bed heating screens off to go straight to the printing screen.
- **Animation lengths:** heated, print finished, after print finished (0 = skip).
- **Preheat presets:** names and nozzle/bed temperatures for Temperature › Preheat.
- **Extrude choices:** the five lengths and speeds on the extruder screen, and which are selected at boot.
## Print screen designer
Open `http://<knomi-ip>/layout` (the **Print screen** tab). Drag elements around a preview of the round screen:
- **Text** with live values: `{pct}` `{time}` `{left}` `{elapsed}` `{total}` `{file}` `{noz}` `{noz_t}` `{bed}` `{bed_t}` `{deg}` `{z}` `{layer}` `{layers}` `{pos}` `{state}`. Pick the size, color, alignment and width. Long text can scroll.
- **Rings and arcs:** full rings, gauges or any angle range. They can show progress or just be decoration.
- **Progress bar.**
- **Animation:** any animation slot, including GIFs you uploaded.

Up to 4 **pages**. A page either takes turns in the rotation for its number of seconds, or is shown **when something happens**:
- pops up for its seconds every N %, at chosen percentages (e.g. 25, 50, 75), every N layer changes, or when the print starts
- stays up while less than N minutes are left, or during the first layer

Layer changes use the printer's layer number when it reports one, otherwise each new Z height that holds for 1.5 s. Tapping the KNOMI skips to the next rotation page. **Play a print** runs a 90-second fake print in the designer so you can see which page shows when.

**Save & preview on KNOMI** shows the layout on the real screen for 20 s, using sample values if nothing is printing. You can also start from a few ready-made layouts, and download or load layout files to share them. The layout is included in backups. Settings › Printing screen switches between your layout and the stock accelerometer bars.

## Coaster face
Coaster is the mascot and every face on the KNOMI: the idle screen, getting ready when a print starts, bored while paused and celebrating after the print. It's a face drawn live instead of a GIF, in the same flat style as the stock faces. Its head and pupils hang on springs driven by the KNOMI's accelerometer (now read at 200 Hz), so toolhead moves slosh them around. It picks a mood from how hard and how long it's being thrown, and gets used to steady shaking:
calm, riding, excited, screaming, startled (endstop hits), dizzy, shivering (input shaper test), elevator (Z moves, from OctoPrint's Z), sleepy, bored (paused), plus a sweat drop that grows with the nozzle temperature and confetti when a print finishes. It also reacts to the printer:

| What happens | Coaster |
|---|---|
| Print cancelled or failed | sad |
| Printer not operational / Klipper shutdown | shocked and trembling (on the error popup) |
| WiFi or OctoPrint lost | lonely, looking around (WiFi-lost screen and connection popup) |
| API key rejected | confused |
| Heating up | impatient, glancing at the heater |
| After a print, while the nozzle cools | cooling off, content |
| First layer | focused squint |
| Last 10 % | almost there |
| Hours into a long print | heavier eyelids |
| Screen dims or turns off | dozes off |
| Filament runout / M600 (plugin 0.5+) | hungry, chomping |
| Part fan 80 %+ (plugin 0.5+) | squinting into the wind |
| Speed factor 130 %+ (plugin 0.5+) | hanging on |

**Touch:** tap Coaster on the idle screen to poke it, hold to keep tickling. On the print screen a tap still changes pages; hold to tickle.

- **Where:** every face screen. The busy animations without a face (homing, QGL, probing, input shaping, PID, cleaning, filament, printing) still play as GIFs you can replace.
- **Print screen:** add "Coaster face" in the designer, or start from "Coaster face, stats every 10 %".
- **Tuning:** the **Coaster face** page (`/coaster`) has a simulated KNOMI to try settings on (play print moves, fling it, or play a Klipper accelerometer CSV), the KNOMI's live mood, and **Save to KNOMI**. Saved in `/coaster.json` and included in backups.

The KNOMI 1 has no accelerometer, so there the face only reacts to printer data.

## Paused animation
While a print is paused, the printing screen plays the **Paused** animation (its own slot on /gifs).
Swipe down to resume, swipe up to cancel, same as before. The plugin treats these as paused:
OctoPrint's own pause, PAUSE / M600 / M601, `// action:paused` lines, and `_KNOMI_SET VAR=paused VALUE=1`.
It clears on RESUME, cancel, or print events. If Klipper paused on its own (M600, an MMU) without OctoPrint
knowing, Resume on the KNOMI sends `RESUME` to Klipper instead of OctoPrint's resume.

## Bluetooth
The KNOMI can get printer status straight from the plugin over Bluetooth LE, with WiFi as the fallback.
Over Bluetooth there's no API key and no polling: the plugin pushes status on change (and every 2 s),
sends the file list, and runs the KNOMI's buttons inside OctoPrint.

1. KNOMI settings page, **Bluetooth**: set *On*, save, restart the KNOMI (System, then Restart).
2. Pair once from the Pi (the KNOMI shows a 6-digit code):
   ```
   bluetoothctl
   scan on                    # wait for KNOMI-<hostname>, note its address (also shown on the KNOMI page)
   pair  XX:XX:XX:XX:XX:XX    # type the code shown on the KNOMI
   trust XX:XX:XX:XX:XX:XX
   exit
   ```
   If the Pi's Bluetooth is disabled (`dtoverlay=disable-bt` in `/boot/config.txt`, common on Klipper setups
   that use the GPIO UART), remove that line and reboot first.
3. OctoPrint, Settings, **KNOMI**: tick *Connect to the KNOMI over Bluetooth* and save. The address fills in
   after the first connection. The Status line shows the link state.
4. Optional, once it says *connected*: on the KNOMI page set **WiFi while Bluetooth is connected** to
   *Turn WiFi off*. This option only unlocks while a Bluetooth link is live, so you can't strand the KNOMI.
   - WiFi turns off 10 s after Bluetooth connects.
   - If Bluetooth is down for the **fallback** time (default 60 s), including after boot, WiFi comes back.
   - **Turn KNOMI WiFi on** in the plugin settings brings WiFi back for 10 minutes (to reach the web page).
   - GIF uploads, OTA and this settings page need WiFi.

"Forget paired devices" on the KNOMI clears its bonds. Also run `remove <address>` in bluetoothctl before pairing again.

## Backup and restore
Settings page → System → **Download backup** saves one `.knomi` file with every setting and custom animation.
**Restore** loads it back and restarts the KNOMI. A restore replaces all custom animations, and only after the whole
file has arrived; an incomplete or wrong file changes nothing. The file contains your WiFi password and OctoPrint
API key, so keep it private.

## Updates
When the settings page is opened from a network with internet access, it checks GitHub for a newer release and shows
a banner with a download link for your board. Install it from `/update` as usual. The check runs in your browser;
the KNOMI itself never contacts GitHub.

## WiFi behaviour
- First-time setup: the KNOMI shows a QR code. Scan it with your phone camera to join the KNOMI's setup network
  (`BTT-KNOMI` by default). Once your phone is connected, the QR switches to the setup page (`http://192.168.20.1/`).
- If the saved network can't be reached (router rebooting, out of range), the KNOMI keeps its settings, opens its
  setup access point alongside, and retries every 30 s. Retries pause while someone is connected to the setup AP.
  When the network comes back, the setup AP closes again.
- Hidden networks: on the settings page, click **join a hidden network** under the network list.

## Accelerometer bars
The printing screen's accelerometer view subtracts gravity and detects the mounting, so the bars show real
toolhead movement: X side-to-side, Y front-to-back, Z up-down. That works upright on a Stealthburner,
upside-down, rotated, or lying flat.

## What maps to what
Over WiFi:

| KNOMI action | OctoPrint call |
|---|---|
| status / temps | GET /api/printer (409 = printer not operational) |
| progress, file | GET /api/job |
| file list | GET /api/files/local?recursive=true (streamed + filtered) |
| gcode (home, QGL, ABL, preheat, extrude, temps) | POST /api/printer/command |
| print file | POST /api/files/local/<path> {select, print} |
| pause / resume / cancel | POST /api/job |
| Klipper Restart / Firmware Restart | gcode RESTART / FIRMWARE_RESTART (through OctoKlipper) |
| Host Reboot / Shutdown | POST /api/system/commands/core/reboot or shutdown |
| Service Control → OctoPrint → Restart | POST /api/system/commands/core/restart (Start/Stop not supported) |
