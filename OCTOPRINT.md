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
Coaster acts each state out (see Coaster below). You can upload your own GIF for any of them instead (next section).

## Custom animations
Open `http://<knomi-ip>/gifs`. Every busy state has a slot: WiFi setup, homing, probing, QGL, the four new
states, printing, finished. Coaster acts them out unless you upload a GIF to a slot, which then plays instead
right away. "Back to Coaster" removes it. Files live in the 7MB flash partition, so they survive firmware updates.
Limits: 1.5MB per GIF and 5MB loaded in total. The screen is 240x240 and round.

## Display settings (settings page, section 02)
- **Brightness** is saved now (the slider on the KNOMI saves too), so it survives restarts.
- **Dim after / screen off after (minutes):** 0 means never. Any touch, a screen change, or the printer
  starting something wakes it. The tap that wakes a dark screen doesn't press anything.
- **While the printer is busy:** stay awake (default), or dim and sleep as usual.
- **Printing screen:** *Info* shows the file name, %, time left (or elapsed), nozzle/bed temps, and Z height
  (Layer x/y on Moonraker when the slicer reports layers, and on OctoPrint with plugin 0.7+, which works the layer out from the file position the way OctoPrint's G-code viewer does; files printed from OctoPrint's own storage only). *Accelerometer* is the stock bars view.
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

Layer changes use the layer number from Moonraker or the plugin (0.7+) when there is one, otherwise each new Z height that holds for 1.5 s. Without either (no plugin, or printing from Klipper's virtual SD card) the layer triggers and the first-layer condition do nothing. Tapping the KNOMI skips to the next rotation page. **Play a print** runs a 90-second fake print in the designer so you can see which page shows when.

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
| Heating up | straining: angry squint, gritted teeth, steam puffs, trembling harder as the temperature closes in, with the temperature underneath. "Phew" when it gets there. This replaces the old nozzle/bed heating screens |
| After a print, while the nozzle cools | cooling off, content |
| First layer | focused squint |
| Last 10 % | almost there |
| Hours into a long print | heavier eyelids |
| Screen dims or turns off | dozes off |
| Filament runout / M600 (plugin 0.5+) | hungry, chomping |
| Part fan 80 %+ (plugin 0.5+) | squinting into the wind |
| Speed factor 130 %+ (plugin 0.5+) | hanging on |

**Busy states, acted out** (with a label underneath): homing (bracing, flinches at the endstop), probing (tap, tap, tap), leveling the gantry (eyes see-saw like the corners), input shaping (shivering), PID tuning (straining), cleaning the nozzle (scrubbing side to side), filament (chomping), starting a print (focused) and done (celebrating). Upload a GIF to a slot on /gifs to use your own animation instead.

**Messages:** `M117 …`, `SET_DISPLAY_TEXT MSG=…` and `// action:notification …` (plugin 0.6+, or Moonraker's display status) show as a speech bubble from Coaster, who talks for the first couple of seconds. The text is also the `{msg}` token in the print screen designer.

**Report card:** after a print, the "after the print" screen shows how the ride went: done or where it stopped, how many times Coaster screamed, peak g, dizzy spells and jolts. The last one is also on the /coaster page and in the OctoPrint sidebar.

**Decorations** change with the seasons: holiday lights, snow and a Santa hat in December, fireworks at New Year and on the 4th of July, hearts around Valentine's, petals in spring, sunglasses in summer, falling leaves in autumn, a witch hat and bats for Halloween, and a party hat on Coaster's birthday (September 28, changeable). Holidays last a few days, not just the date. On the /coaster page you can pick one by hand, turn them off, choose the lights' colors and effect, switch to the southern hemisphere, and try any date. The date comes from the internet once the KNOMI is on WiFi.

**Feelings:** under the quick moods, Coaster has a slower feeling that lasts hours and survives restarts. Printing makes it happier, finishing prints (especially a streak) more so. Getting dizzy, failed prints, being left off for days, losing OctoPrint and heating up with nothing to print make it unhappier; heating for nothing gets it mad. Crashes, power blips and waking up at night leave it confused for a moment. Happy, it smiles more and hums; down, it droops, sighs and sulks. Each Coaster is also born with its own likes and dislikes (a favorite season, one it could do without, long or quick prints, fast moves, being poked, late nights, fans, heat, quiet time). They're rolled once, kept on the KNOMI, and can't be changed. It also learns from you: after five prints it knows your usual print length and time of day, and a print that breaks the habit (much longer, much shorter, an odd hour, a much wilder ride) gets a reaction. Some Coasters love a change of pace, others are creatures of habit and get uneasy. What you print a lot it slowly warms up to (lots of quick prints and it grows fond of them, late nights make a night owl), on top of what it was born with. The /coaster page shows how it feels, why, and what it likes.

**Filaments (plugin 0.9+):** the plugin reads the filament type from the slicer's settings in the file (or its name), and Coaster reacts: coughs through ABS and ASA, keeps an eye on the nozzle with PETG, goes wobbly on TPU. Every Coaster is born with a favorite filament and one it doesn't like, grows fond of what you print a lot, and goes off a filament that fails.

**Talking:** Coaster says things of its own in speech bubbles: good morning, the filament, a new file or one it has printed a lot, halfway, almost done, done, a streak, a season it likes or hates, and the odd thought when it's idle. Set it to often, sometimes or never on the /coaster page. Printer messages (M117) always win.

**Idle clock:** when nothing is printing, the time shows under Coaster (12 or 24 hour, or off, on the /coaster page).

**Album:** the /coaster page keeps Coaster's life so far: when it was first switched on, its birthday, prints together, best streak, time printing, longest print, wildest ride, screams, favorite season and filament, and its personality.

**Its own ways:** each Coaster is born with a pre-print ritual (a deep breath, cracking its knuckles, an eye roll and a nod, or a stretch), a hobby (counting layers out loud, singing, stargazing or tidying up after prints) and a favorite spot to rest its eyes. It rates every finished print out of 5 stars, gives files you print a lot a nickname, and remembers the ones that failed or made it dizzy. It notices which day you usually print, has a morning routine the first time it wakes each day, and celebrates milestones (first print, 10, 25, 50, 100... prints, hours printed, a year together). It starts out a bit shy and warms up as you print and poke it. It dreams when it sleeps (a thought bubble with its favorite season, a spool, a boat or a star), daydreams on long prints, sometimes nods off on very long ones and jolts awake at the next layer, gets the odd bout of hiccups (a poke cures them), and after a great or a bad day has a little sun or rain cloud over its head.

**Little reactions:** its likes and dislikes show on the spot, not only in its mood. A fast move, the part fan kicking in, heating up, a poke, a long or quick print, its favorite or least favorite filament, waking up at night, half an hour of quiet, its favorite or least favorite season: things it likes get a delighted grin, a wink or a cheer; things it doesn't get a wince, an eye roll, a sigh or a huff.

**Signature move:** each Coaster has its own flourish (spinning its eyes, a double wink, a shimmy, a jump for joy, or a look left and right) that it does after every good print.

**Tally:** it counts every layer it has ever ridden through and cheers at the round numbers (100, 1,000, 5,000... up to a million). The album shows the count.

**Talking to itself:** on long, steady prints it mutters under its breath, in small text under its face: "hmm", "steady...", "nice layer", "almost...", "no strings..." with PETG, "la la la" when it's happy.

**Quirks:** every few seconds Coaster does something small on its own, printing or not: glances around, blinks, winks, yawns, hums, sneezes, stretches, rolls its eyes, sighs when it's down, huffs when it's mad. It nods at each layer change and cheers at 25, 50 and 75 %. Each KNOMI gets its own personality from its chip ID.

**OctoPrint sidebar (plugin 0.6+):** a live copy of Coaster in OctoPrint's sidebar, with its mood, how it's feeling (plugin 0.7.1+) and the last report card. The KNOMI sends its mood to the plugin (over WiFi or Bluetooth).

**Touch:** tap Coaster on the idle screen to poke it, hold to keep tickling. On the print screen a tap still changes pages; hold to tickle.

- **Where:** everywhere a face or busy animation used to be. Only the WiFi setup screens still use GIFs.
- **Print screen:** add "Coaster face" in the designer, or start from "Coaster, stats every 10 % and at the end".
- **Tuning:** the **Coaster face** page (`/coaster`) has a simulated KNOMI to try settings on (play print moves, fling it, or play a Klipper accelerometer CSV), the KNOMI's live mood, and **Save to KNOMI**. Saved in `/coaster.json` and included in backups.

The KNOMI 1 has no accelerometer, so there the face only reacts to printer data.

## Paused animation
While a print is paused, Coaster takes over the printing screen, bored (or hungry on a filament runout).
Swipe down to resume, swipe up to cancel, same as before. The plugin treats these as paused:
OctoPrint's own pause, PAUSE / M600 / M601, `// action:paused` lines, and `_KNOMI_SET VAR=paused VALUE=1`.
It clears on RESUME, cancel, or print events. If Klipper paused on its own (M600, an MMU) without OctoPrint
knowing, Resume on the KNOMI sends `RESUME` to Klipper instead of OctoPrint's resume.

## Bluetooth
The KNOMI can get printer status straight from the plugin over Bluetooth LE, with WiFi as the fallback.
Over Bluetooth there's no API key and no polling: the plugin pushes status on change (and every 2 s),
sends the file list, and runs the KNOMI's buttons inside OctoPrint.

1. KNOMI settings page, **Bluetooth**: set *On*, save, restart the KNOMI (System, then Restart).
2. OctoPrint, Settings, **KNOMI**, **Bluetooth**: click **Find KNOMI**, then **Pair** next to it, and type the
   6-digit code the KNOMI shows. That's it: the plugin pairs, trusts it, remembers the address and connects.
   The **Link** line shows the state. (You can still pair by hand with `bluetoothctl` if you prefer.)
   If the Pi's Bluetooth is disabled (`dtoverlay=disable-bt` in `/boot/config.txt`, common on Klipper setups
   that use the GPIO UART), remove that line and reboot first.
3. If it doesn't connect, **Reconnect** retries, and **Forget** unpairs it on the Pi.
4. Optional, once it says *connected*: on the KNOMI page set **WiFi while Bluetooth is connected** to
   *Turn WiFi off*. This option only unlocks while a Bluetooth link is live, so you can't strand the KNOMI.
   - WiFi turns off 10 s after Bluetooth connects.
   - If Bluetooth is down for the **fallback** time (default 60 s), including after boot, WiFi comes back.
   - **Turn KNOMI WiFi on** in the plugin settings brings WiFi back for 10 minutes (to reach the web page).
   - GIF uploads, OTA and this settings page need WiFi.

**The KNOMI's pages without WiFi (OP41+, plugin 0.12.3+):** OctoPrint, Settings, **KNOMI**, *KNOMI's own settings*:
**Open here** (or **New tab**) shows all of the KNOMI's web pages (settings, animations, print screen, Coaster,
firmware, log) inside OctoPrint. The plugin fetches them over WiFi when the KNOMI has it on, and over Bluetooth
when it doesn't. Over Bluetooth pages take a few seconds, a GIF upload about a minute, a firmware file a few
minutes. Only OctoPrint users allowed to change settings can open them.

To pair again from scratch: **Forget** in the plugin settings, and **Forget paired devices** on the KNOMI's settings page.

## Backup and restore
Settings page → System → **Download backup** saves one `.knomi` file with every setting and custom animation.
**Restore** loads it back and restarts the KNOMI. A restore replaces all custom animations, and only after the whole
file has arrived; an incomplete or wrong file changes nothing. The file contains your WiFi password and OctoPrint
API key, so keep it private.

## Updates
**One click:** Settings › System › **Update from GitHub** (or **Install now** on the banner that appears when a newer
release exists). The KNOMI asks GitHub for the latest release, downloads the .bin for its board over HTTPS (with the
certificates checked) straight into its spare firmware slot, and restarts into it. Settings, animations and layouts stay.
Printer polling pauses while it downloads. If it fails, the old firmware keeps running; the reason is on the page and in the log.

Manual updates still work: upload a .bin at `/update`.

**Self-rescue:** if new firmware crashes three times in a row, each within a minute of starting, the KNOMI switches back
to the firmware in its other slot (the one it had before the update) and says so in the log. It only does this once, so
two broken versions can't bounce back and forth; unplugging it resets the count. If both slots are broken, flash over USB:
hold BOOT while plugging it in, then write `boot_app0.bin` at `0xe000` and the firmware at `0x10000` with
[esptool-js](https://espressif.github.io/esptool-js/).

## Log
`http://<knomi-ip>/log` shows what the KNOMI prints to its serial port (the last 32 KB, with uptime stamps), plus
firmware, memory, WiFi, printer connection, why it last restarted, which firmware slot it's running from and how much stack each task has left. **Download** saves it as a text file to send
along with a bug report. Passwords and API keys are never logged.

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
