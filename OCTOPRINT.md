# KNOMI + OctoPrint setup

> Only tested on a KNOMI 2 with OctoPrint on a Raspberry Pi 5.

Each release has an OctoPrint build and a Moonraker build of the same source; this guide is about the OctoPrint one.

## How it connects (OctoPrint build)
- **Bluetooth** to the OctoPrint-KNOMI plugin is the control link: the plugin pushes printer status the moment it
  changes, sends the file list, and runs the KNOMI's buttons inside OctoPrint. No API key, no polling.
- **The KNOMI network** (OP58+): the KNOMI runs its own small WPA2 network (`KNOMI-XXXXXX`, the KNOMI at
  `192.168.77.1`) and the Pi joins it on a spare WiFi interface. The KNOMI's pages, GIF uploads and firmware go over
  it. Without it everything still works over Bluetooth, just slower for those.
- The KNOMI never joins your WiFi. Its network's password is made up on the KNOMI and handed to the Pi only over the
  paired, encrypted Bluetooth link; it's never shown in logs, settings or backups.

The Moonraker build is the stock arrangement instead: the KNOMI joins your WiFi and polls Moonraker.

## Upgrading to OP58
OP58 changes how the OctoPrint build connects: **the KNOMI leaves your WiFi.** Before installing it:
1. Update the plugin to 0.14.0 or newer.
2. Pair over Bluetooth if you haven't, and check that the plugin's **Link** says *connected*. Bluetooth is how the
   Pi reaches the KNOMI from now on.
3. Optional, for the fast link: move the Pi to Ethernet (or plug in a USB WiFi adapter), see step 4 below.
4. Install OP58 from Settings › KNOMI. The KNOMI restarts on its own network, the plugin reads its details over
   Bluetooth and the Pi joins.

Settings, animations, layouts and Coaster's feelings carry over. The old WiFi and OctoPrint address/API key settings
are no longer used. If something goes wrong, the KNOMI shows a QR code after 2 minutes without Bluetooth: join its
network from a phone and open `http://192.168.77.1/update` to upload any firmware, including an older one.

## 1. Install the plugin
OctoPrint → Settings → Plugin Manager → **Get More…** → **…from URL**:
`https://github.com/Binnacle-Tech/OctoPrint-KNOMI/archive/refs/heads/main.zip`, then restart OctoPrint.
Use 0.14.0 or newer with OP58 and later.

## 2. Flash
From stock firmware (or a Moonraker build): open `http://<knomi-ip>/update` and upload
`knomiv2-octoprint-firmware.bin` from the [latest release](https://github.com/Binnacle-Tech/KNOMI/releases/latest)
(`knomiv1-…` for a KNOMI 1). The first boot after flashing formats the GIF storage, which adds a few seconds once.
After that, updates come from OctoPrint (see [Updates](#updates)).

Building it yourself: `pio pkg install -e knomiv2`, copy `lv_disp_(bugfix_backup).c` over
`.pio/libdeps/knomiv2/lvgl/src/core/lv_disp.c`, then `pio run -e knomiv2`.

## 3. Pair over Bluetooth
OctoPrint, Settings, **KNOMI**, **Bluetooth**: tick **Connect to the KNOMI over Bluetooth**, click **Find KNOMI**,
then **Pair** next to it, and type the 6-digit code the KNOMI shows. Details in [Bluetooth](#bluetooth).

## 4. Let the Pi join the KNOMI network
The Pi needs a WiFi interface that isn't busy with your own network:
- **Pi on Ethernet:** its built-in WiFi is free, once it stops joining your WiFi. If it was on WiFi before, run this
  over SSH **while connected by Ethernet**, with `YOUR-WIFI` replaced by your network's name (`nmcli connection show`
  lists it; the plugin also shows the command with the name filled in):
  ```
  sudo nmcli connection modify "YOUR-WIFI" connection.autoconnect no && sudo nmcli connection down "YOUR-WIFI"
  ```
- **Pi on WiFi:** add a USB WiFi adapter for the KNOMI network, or skip the fast link and stay on Bluetooth.

The plugin picks a free interface on its own (or set one under **WiFi interface**). It never takes an interface that
is on, or could rejoin, another network, or that carries the Pi's internet. This needs NetworkManager (Raspberry Pi OS
Bookworm and newer).

**Permission for OctoPrint (once).** NetworkManager won't let OctoPrint's user set up a WiFi connection on its own.
Run this over SSH, with `pi` replaced by the user OctoPrint runs as (`systemctl show -p User octoprint` shows it on OctoPi):

```
echo 'polkit.addRule(function(a,s){if(s.user=="pi"&&["org.freedesktop.NetworkManager.settings.modify.system","org.freedesktop.NetworkManager.network-control"].indexOf(a.id)>=0)return polkit.Result.YES;});' | sudo tee /etc/polkit-1/rules.d/00-octoprint-knomi.rules
```

It allows that user to set up and switch NetworkManager connections, nothing else, and takes effect right away (no
restart). Check with `ls /etc/polkit-1/rules.d/`; undo with `sudo rm /etc/polkit-1/rules.d/00-octoprint-knomi.rules`.
The plugin also shows this command, with your user filled in, when it needs it.

After that, **KNOMI network** in the plugin settings says *joined*.

## 5. Klipper macros (optional, for steps run inside PRINT_START)
The plugin can only see commands that OctoPrint sends itself (G28, QUAD_GANTRY_LEVEL, BED_MESH_CALIBRATE, M109/M190...).
To make the steps inside your macros show up, include `knomi_octoprint.cfg` and replace each
`SET_GCODE_VARIABLE MACRO=_KNOMI_STATUS VARIABLE=x VALUE=True|False`
with `_KNOMI_SET VAR=x VALUE=1|0`. That macro still sets the variable (so Mainsail keeps working),
and it also prints `// KNOMI x=1`, which the plugin reads.

## Live updates
The plugin pushes status over Bluetooth the moment something changes (and a heartbeat every few seconds), and wakes
the KNOMI's printer task as it arrives, so the screen follows within a fraction of a second. Nothing is polled.

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
In OctoPrint, Settings › KNOMI › **KNOMI's web pages** › Animations (`http://<knomi-ip>/gifs` on the Moonraker
build). Every busy state has a slot: WiFi setup, homing, probing, QGL, the four new
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
Open the **Print screen** tab of the KNOMI's web pages (in OctoPrint, or `http://<knomi-ip>/layout` on the Moonraker build). Drag elements around a preview of the round screen:
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

**Decorations** change with the seasons: holiday lights, snow and a Santa hat in December, fireworks at New Year and on the 4th of July, hearts around Valentine's, petals in spring, sunglasses in summer, falling leaves in autumn, a witch hat and bats for Halloween, and a party hat on Coaster's birthday (September 28, changeable). Holidays last a few days, not just the date. On the /coaster page you can pick one by hand, turn them off, choose the lights' colors and effect, switch to the southern hemisphere, and try any date. The date comes from the Pi's clock over Bluetooth (from the internet on the Moonraker build).

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

**OctoPrint sidebar (plugin 0.6+):** a live copy of Coaster in OctoPrint's sidebar, with its mood, how it's feeling (plugin 0.7.1+) and the last report card. The KNOMI sends its mood to the plugin over Bluetooth.

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
In the OctoPrint build Bluetooth is always on: it's how the plugin reaches the KNOMI.

1. OctoPrint, Settings, **KNOMI**, **Bluetooth**: tick **Connect to the KNOMI over Bluetooth**, click **Find KNOMI**,
   then **Pair** next to it, and type the 6-digit code the KNOMI shows. The plugin pairs, trusts it, remembers the
   address and connects. The **Link** line shows the state. (Pairing by hand with `bluetoothctl` works too.)
   If the Pi's Bluetooth is disabled (`dtoverlay=disable-bt` in `/boot/firmware/config.txt`, common on Klipper setups
   that use the GPIO UART), remove that line and reboot first.
2. If it doesn't connect, **Reconnect** retries, and **Forget** unpairs it on the Pi.
3. Once connected, the plugin reads the KNOMI network's details and the Pi joins it (see step 4 above).

**The KNOMI's pages in OctoPrint:** Settings, **KNOMI**: *KNOMI's own settings* shows every setting, and
**KNOMI's web pages** opens all of its pages (animations, print screen designer, Coaster, firmware, log) inside
OctoPrint. The plugin fetches them over the KNOMI network when the Pi is on it, and over Bluetooth when it isn't
(a few seconds per page, a GIF upload about a minute, a firmware file a few minutes). Only OctoPrint users allowed to
change settings can open them.

To pair again from scratch: **Forget** in the plugin settings, and **Forget paired devices** on the KNOMI's settings
page. Forgetting on the KNOMI also gives its network a new password and drops whoever is connected, so a Pi that's no
longer paired can't stay on it.

## Backup and restore
Settings page → System → **Download backup** saves one `.knomi` file with every setting and custom animation.
**Restore** loads it back and restarts the KNOMI. A restore replaces all custom animations, and only after the whole
file has arrived; an incomplete or wrong file changes nothing. It can contain a WiFi password and API key saved by
older firmware (not the KNOMI network's password), so keep it private.

## Updates
**From OctoPrint:** Settings › KNOMI › **Update the KNOMI**. **Install latest from GitHub** has the Pi download the
release for the KNOMI's board and build, and send it over (the KNOMI network if the Pi is on it, Bluetooth if not);
**Install this .bin** sends a file you pick. Coaster mentions it when a new release is out. Settings, animations and
layouts stay. If it fails, the old firmware keeps running and the reason is shown.

The Moonraker build updates itself from its own settings page (**Update from GitHub**), since it's on the internet.

The KNOMI checks which build an upload is: the OctoPrint build won't install a Moonraker `.bin` and the other way round.
Firmware from before OP58 has no marker and is accepted, so going back to an older release works.

**Self-rescue:** if new firmware crashes three times in a row, each within a minute of starting, the KNOMI switches back
to the firmware in its other slot (the one it had before the update) and says so in the log. It only does this once, so
two broken versions can't bounce back and forth; unplugging it resets the count. If both slots are broken, flash over USB:
hold BOOT while plugging it in, then write `boot_app0.bin` at `0xe000` and the firmware at `0x10000` with
[esptool-js](https://espressif.github.io/esptool-js/).

## Log
The **Log** tab of the KNOMI's web pages (`http://<knomi-ip>/log` on the Moonraker build) shows what the KNOMI prints to its serial port (the last 32 KB, with uptime stamps), plus
firmware, build, memory, network, printer connection, why it last restarted, which firmware slot it's running from and how much stack each task has left. **Download** saves it as a text file to send
along with a bug report. Passwords and API keys are never logged.

## The KNOMI network
- Name `KNOMI-XXXXXX` (from the KNOMI's address), WPA2, channel 6, the KNOMI at `192.168.77.1` (the Pi gets an
  address from it). Up to 3 devices: the Pi and a phone or two.
- The password is made up on the KNOMI's first start and kept. **Forget paired devices** makes a new one.
- The Pi's side is a NetworkManager connection called `knomi-link`. It's never the Pi's way to the internet and doesn't
  connect by itself: the plugin brings it up on the interface it checked is free, and checks on it every 30 s.
- If your own network uses `192.168.77.x`, the plugin says so instead of joining (routing would break).
- No Bluetooth for 2 minutes (the plugin stopped, the Pi is off, pairing broke): the KNOMI shows a QR code to join its
  network from a phone. Then open `http://192.168.77.1/`.

On the Moonraker build the KNOMI joins your WiFi instead:
- First-time setup: the KNOMI shows a QR code. Scan it with your phone camera to join the KNOMI's setup network
  (`BTT-KNOMI` by default). Once your phone is connected, the QR switches to the setup page (`http://192.168.20.1/`).
- If the saved network can't be reached (router rebooting, out of range), the KNOMI keeps its settings, opens its
  setup access point alongside, and retries every 30 s. Retries pause while someone is connected to the setup AP.
- Hidden networks: on the settings page, click **join a hidden network** under the network list.

## Accelerometer bars
The printing screen's accelerometer view subtracts gravity and detects the mounting, so the bars show real
toolhead movement: X side-to-side, Y front-to-back, Z up-down. That works upright on a Stealthburner,
upside-down, rotated, or lying flat.

## What maps to what
The KNOMI sends its buttons to the plugin over Bluetooth as Moonraker-style paths, and the plugin runs them in OctoPrint:

| KNOMI action | In OctoPrint |
|---|---|
| gcode (home, QGL, ABL, preheat, extrude, temps) | sent to the printer as G-code |
| print file | selects and prints the file from OctoPrint's local storage |
| pause / resume / cancel | OctoPrint's job controls |
| Klipper Restart / Firmware Restart | gcode RESTART / FIRMWARE_RESTART (through OctoKlipper) |
| Host Reboot / Shutdown, Restart OctoPrint | the commands set in OctoPrint › Settings › Server |
| resume while paused inside Klipper (M600, MMU) | gcode RESUME |

Status (temps, progress, file, layer, flags) goes the other way, pushed by the plugin on change.
