#ifndef COASTER_HTML_H
#define COASTER_HTML_H
// Coaster face tuning page (/coaster). Served as-is (no template processor).
const char coaster_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>KNOMI · Coaster face</title>
<link rel="stylesheet" media="print" onload="this.media='all'" href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@600;700&family=Inter:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
/* Binnacle instrument-panel palette, single dark look (it's a screen on a printer) */
:root{
  color-scheme:dark;
  --ink:#0E1419;--panel:#151E27;--panel-2:#1B2731;--well:#0A1016;
  --line:#26333E;--line-2:#334353;
  --amber:#E8A33D;--amber-soft:#F0C079;--cyan:#4FD1C5;--violet:#9AA7F0;
  --text:#E7EEF4;--muted:#93A4B2;--muted-2:#7E8F9F;
  --font-disp:"Space Grotesk",system-ui,sans-serif;
  --font-ui:"Inter",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --font-mono:"IBM Plex Mono",ui-monospace,Consolas,monospace;
  --r:10px;--r-ctrl:8px;
}
*{box-sizing:border-box}
body{background:var(--ink);color:var(--text);font-family:var(--font-ui);font-size:14px;line-height:1.55;
  padding-inline:20px;padding-block:22px 40px;-webkit-font-smoothing:antialiased}
.wrap{max-width:1120px;margin:0 auto;display:flex;flex-direction:column;gap:22px}
.pagehead{display:flex;flex-direction:column;gap:6px;border-bottom:1px solid var(--line);padding-bottom:16px}
.eyebrow{font-family:var(--font-mono);font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted-2)}
h1{font-family:var(--font-disp);font-weight:700;font-size:clamp(26px,4vw,34px);letter-spacing:-.01em;margin:0;line-height:1.1;text-wrap:balance}
h1 .dot{color:var(--amber)}
.lede{color:var(--muted);max-width:68ch;margin:0}
.grid{display:grid;grid-template-columns:minmax(0,420px) minmax(0,1fr);gap:24px;align-items:start}
.stage{position:sticky;top:calc(env(safe-area-inset-top,0px) + 16px);display:flex;flex-direction:column;align-items:center;gap:14px}
.mount{position:relative;width:100%;max-width:400px;aspect-ratio:1;display:grid;place-items:center;
  background:repeating-linear-gradient(90deg,transparent 0 23px,rgba(255,255,255,.025) 23px 24px),
             repeating-linear-gradient(0deg,transparent 0 23px,rgba(255,255,255,.025) 23px 24px);border-radius:14px;
  border:1px solid var(--line);touch-action:none;user-select:none;overflow:hidden}
.mount .hint{position:absolute;left:12px;bottom:10px;font-family:var(--font-mono);font-size:10.5px;color:var(--muted-2);letter-spacing:.04em}
.mount .axes{position:absolute;right:12px;top:10px;font-family:var(--font-mono);font-size:10.5px;color:var(--muted-2);text-align:right;line-height:1.5}
.axes b{font-weight:500}.ax-x{color:var(--amber)}.ax-y{color:var(--cyan)}.ax-z{color:var(--violet)}
.device{width:78%;aspect-ratio:1;border-radius:50%;cursor:grab;will-change:transform;
  background:radial-gradient(circle at 32% 26%,#39444f,#11171d 62%);padding:5.5%;
  box-shadow:0 18px 40px rgba(0,0,0,.55),inset 0 0 0 1px #46535f,inset 0 -6px 14px rgba(0,0,0,.6)}
.device:active{cursor:grabbing}
.device canvas{width:100%;height:100%;border-radius:50%;display:block;background:#000;box-shadow:0 0 0 2px #05080b}
.mood{width:100%;max-width:400px;display:flex;flex-direction:column;gap:6px;background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:12px 14px}
.mood-row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.chip{font-family:var(--font-mono);font-size:12px;font-weight:500;letter-spacing:.06em;text-transform:uppercase;
  padding:3px 10px;border-radius:20px;border:1px solid currentColor;color:var(--amber)}
.why{color:var(--muted);font-size:12.5px;min-height:1.5em}
.meters{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin-top:4px}
.meter{display:flex;flex-direction:column;gap:3px}
.meter span{font-family:var(--font-mono);font-size:10.5px;color:var(--muted-2);text-transform:uppercase;letter-spacing:.08em}
.meter .bar{height:6px;background:var(--well);border-radius:4px;overflow:hidden}
.meter .bar i{display:block;height:100%;width:0;background:var(--amber);border-radius:4px}
.meter b{font-family:var(--font-mono);font-weight:500;font-size:12px;font-variant-numeric:tabular-nums}

.cards{display:flex;flex-direction:column;gap:16px;min-width:0}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r)}
.card-h{display:flex;align-items:center;gap:10px;padding:11px 16px;border-bottom:1px solid var(--line)}
.card-h h2{font-family:var(--font-mono);font-weight:500;font-size:11.5px;letter-spacing:.12em;text-transform:uppercase;margin:0;color:var(--text)}
.card-h .sp{flex:1}
.card-b{padding:14px 16px;display:flex;flex-direction:column;gap:12px}
.scen{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
button{font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:var(--r-ctrl);
  padding:8px 12px;cursor:pointer;text-align:left}
button:hover{border-color:var(--muted-2)}
button:focus-visible,input:focus-visible,label.file:focus-within{outline:2px solid var(--amber);outline-offset:2px}
.scen button{display:flex;flex-direction:column;gap:2px}
.scen button b{font-weight:600;font-size:13px}
.scen button small{color:var(--muted-2);font-size:11.5px;line-height:1.35}
.scen button[aria-pressed="true"]{border-color:var(--amber);background:color-mix(in srgb,var(--amber) 10%,var(--panel-2))}
.scen button[aria-pressed="true"] b{color:var(--amber)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.btn-primary{background:var(--amber);color:#1a1206;border-color:var(--amber);font-weight:600}
.btn-primary:hover{background:var(--amber-soft);border-color:var(--amber-soft)}
label.file{display:inline-flex;align-items:center;gap:8px;background:var(--panel-2);border:1px solid var(--line-2);border-radius:var(--r-ctrl);padding:8px 12px;cursor:pointer}
label.file:hover{border-color:var(--muted-2)}
.note{color:var(--muted-2);font-size:12px;margin:0}
.note code{font-family:var(--font-mono);font-size:11.5px;color:var(--muted)}
#scope{width:100%;height:150px;display:block;background:var(--well);border-radius:var(--r-ctrl);border:1px solid var(--line)}
.legend{display:flex;gap:14px;flex-wrap:wrap;font-family:var(--font-mono);font-size:11px;color:var(--muted)}
.legend i{display:inline-block;width:14px;height:2px;vertical-align:middle;margin-right:6px}
.toggles{display:flex;flex-wrap:wrap;gap:8px}
.toggle{display:inline-flex;align-items:center;gap:8px;background:var(--panel-2);border:1px solid var(--line-2);border-radius:var(--r-ctrl);padding:7px 12px;cursor:pointer;font-size:13px}
.toggle input{accent-color:var(--amber);width:16px;height:16px;margin:0}
.sliders{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px 20px}
.sl{display:flex;flex-direction:column;gap:2px}
.sl-top{display:flex;justify-content:space-between;gap:8px}
.sl label{font-size:12.5px;font-weight:500}
.sl output{font-family:var(--font-mono);font-size:12px;color:var(--amber);font-variant-numeric:tabular-nums}
.sl small{color:var(--muted-2);font-size:11.5px;line-height:1.35}
.sl input[type=range]{width:100%;accent-color:var(--amber);margin:4px 0}
.swatches{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.sw{width:30px;height:30px;border-radius:50%;padding:0;border:2px solid var(--line-2)}
.sw[aria-pressed="true"]{border-color:var(--text)}
input[type=color]{width:42px;height:32px;padding:2px;background:var(--panel-2);border:1px solid var(--line-2);border-radius:6px;cursor:pointer}
.copied{font-family:var(--font-mono);font-size:11.5px;color:var(--cyan)}
textarea{width:100%;min-height:90px;background:var(--well);color:var(--text);border:1px solid var(--line-2);border-radius:var(--r-ctrl);font-family:var(--font-mono);font-size:11.5px;padding:8px}
@media (max-width:860px){
  .grid{grid-template-columns:1fr}
  .stage{position:static}
  .sliders{grid-template-columns:1fr}
}
/* top rail, same as the other KNOMI pages */
body{padding-top:0}
.rail{position:sticky;top:0;z-index:20;background:var(--ink);border-bottom:1px solid var(--line);margin:0 -20px 18px;padding:0 20px}
.rail-in{max-width:1120px;margin:0 auto;display:flex;align-items:center;gap:16px;min-height:56px}
.brand{display:flex;align-items:baseline;gap:10px;flex:none}
.brand a{font-family:var(--font-disp);font-weight:700;font-size:19px;color:var(--text);text-decoration:none}
.brand .dot{color:var(--amber)}
.rail-sp{flex:1}
.rail nav{display:flex;gap:4px;min-width:0;overflow-x:auto;scrollbar-width:none}
.rail nav a{white-space:nowrap;font-size:12.5px;color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:8px}
.rail nav a:hover{color:var(--text);background:var(--panel-2)}
.rail nav a.on{color:var(--amber)}
.kstate{display:flex;flex-direction:column;gap:10px}
.saved{font-family:var(--font-mono);font-size:11.5px;color:var(--cyan)}
.saved.bad{color:#E06C5A}
@media (max-width:640px){.rail nav a{padding:6px 7px;font-size:12px}}
.album .kv{display:grid;grid-template-columns:auto 1fr;gap:6px 14px;margin:0}.album dt{color:var(--muted-2)}.album dd{margin:0;color:var(--text);font-variant-numeric:tabular-nums}
</style></head><body>
<header class="rail"><div class="rail-in">
  <div class="brand"><a href="/" style="display:inline-flex;align-items:center;gap:8px"><svg class="mark" style="width:24px;height:24px" viewBox="0 0 256 256" aria-hidden="true"><circle cx="128" cy="128" r="126" fill="#000" stroke="#334353" stroke-width="6"/><g stroke="#C02F30" stroke-width="16" stroke-linecap="round" fill="none"><path d="M32 112h80M144 112h80"/><path stroke-width="14" d="M100 176a14 14 0 0 0 28 0a14 14 0 0 0 28 0"/></g><g fill="#C02F30"><path d="M42 112a30 30 0 0 0 60 0z"/><path d="M154 112a30 30 0 0 0 60 0z"/></g></svg><span>KNOMI<span class="dot">.</span></span></a></div><span class="rail-sp"></span>
  <nav><a href="/">Settings</a><a href="/gifs">Animations</a><a href="/layout">Print screen</a><a class="on" href="/coaster">Coaster face</a><a href="/update">Firmware</a><a href="/log">Log</a></nav>
</div></header>

<div class="wrap">
  <header class="pagehead">
    <span class="eyebrow">KNOMI · reactive face</span>
    <h1>Coaster Face<span class="dot">.</span></h1>
    <p class="lede">A face drawn live from the KNOMI's accelerometer instead of a GIF. Its eyes and head hang on springs, so every move of the toolhead sloshes them around, and it picks a mood from how hard and how long it's being thrown. Try settings here on the simulated KNOMI (play a print, or grab it and fling it), then save them to your KNOMI. It's every face on your KNOMI: idle, getting ready, paused and after the print, and you can place it on the <a href="/layout" style="color:var(--cyan)">print screen</a> too.</p>
  </header>

  <div class="grid">
    <div class="stage">
      <div class="mount" id="mount">
        <div class="axes"><b class="ax-x">X</b> side to side<br><b class="ax-y">Y</b> toward / away<br><b class="ax-z">Z</b> up / down</div>
        <div class="device" id="device" title="Drag to move, flick to throw, tap the screen to poke it"><canvas id="face" width="480" height="480" aria-label="Animated KNOMI face"></canvas></div>
        <span class="hint">drag · flick · shift-drag = toward/away · tap = poke</span>
      </div>
      <div class="mood" aria-live="polite">
        <div class="mood-row"><span class="chip" id="moodChip">calm</span><span class="why" id="why">Nothing moving.</span></div>
        <div class="meters">
          <div class="meter"><span>Thrill</span><div class="bar"><i id="mThrill"></i></div><b id="vThrill">0%</b></div>
          <div class="meter"><span>Buzz</span><div class="bar"><i id="mBuzz" style="background:var(--cyan)"></i></div><b id="vBuzz">0%</b></div>
          <div class="meter"><span>Dizzy</span><div class="bar"><i id="mDizzy" style="background:var(--violet)"></i></div><b id="vDizzy">0%</b></div>
        </div>
      </div>
    </div>

    <div class="cards">
      <section class="card">
        <div class="card-h"><h2>On your KNOMI</h2><span class="sp"></span><span class="chip" id="kMood">…</span></div>
        <div class="card-b kstate">
          <div class="meters">
            <div class="meter"><span>Thrill</span><div class="bar"><i id="kThrill"></i></div><b id="kvThrill">0%</b></div>
            <div class="meter"><span>Buzz</span><div class="bar"><i id="kBuzz" style="background:var(--cyan)"></i></div><b id="kvBuzz">0%</b></div>
            <div class="meter"><span>Dizzy</span><div class="bar"><i id="kDizzy" style="background:var(--violet)"></i></div><b id="kvDizzy">0%</b></div>
          </div>
          <p class="note" id="kNote">Live mood from the KNOMI's own accelerometer.</p>
          <div class="feel">
            <div class="meter"><span>Feeling</span><div class="bar"><i id="kFeelBar" style="background:var(--cyan)"></i></div><b id="kFeel">…</b></div>
            <p class="note" id="kWhy"></p>
            <p class="note" id="kLikes"></p>
            <p class="note" id="kHabits"></p>
            <p class="note">Every Coaster is born with its own likes and dislikes, and picks up new ones from what you print. You can't change them, only get to know them.</p>
          </div>
          <div id="kReport" class="note"></div>
        </div>
      </section>
      <section class="card">
        <div class="card-h"><h2>Decorations</h2><span class="sp"></span><span class="chip" id="decoNow">…</span></div>
        <div class="card-b deco">
          <div class="sl"><div class="sl-top"><label for="dMode">Decorations</label></div>
            <select id="dMode" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px">
              <option value="auto">By season (automatic)</option><option value="off">Off</option>
              <option value="holidays">Holidays: lights, snow, Santa hat</option><option value="newyear">New Year: party hat, fireworks</option>
              <option value="winter">Winter: snow</option><option value="valentine">Valentine's: hearts</option>
              <option value="spring">Spring: petals, a flower</option><option value="summer">Summer: sunglasses</option>
              <option value="july4">4th of July: fireworks</option><option value="autumn">Autumn: falling leaves</option>
              <option value="halloween">Halloween: witch hat, bats</option><option value="birthday">Coaster's birthday</option>
            </select><small id="dWhy">Changes with the seasons. Holidays get a few days, not just the one date.</small></div>
          <div class="sl"><div class="sl-top"><label for="dLights">Holiday lights</label></div>
            <select id="dLights" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px">
              <option value="classic">Classic colors</option><option value="warm">Warm white</option><option value="theme">UI color</option>
              <option value="candy">Candy cane</option><option value="rainbow">Rainbow fade</option>
            </select></div>
          <div class="sl"><div class="sl-top"><label for="dAnim">Lights effect</label></div>
            <select id="dAnim" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px">
              <option value="twinkle">Twinkle</option><option value="chase">Chase</option><option value="breathe">Slow fade</option><option value="steady">Steady</option>
            </select></div>
          <div class="sl"><div class="sl-top"><label for="dHemi">Seasons</label></div>
            <select id="dHemi" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px"><option value="n">Northern hemisphere</option><option value="s">Southern hemisphere</option></select></div>
          <div class="sl"><div class="sl-top"><label for="dBday">Coaster's birthday</label></div>
            <input id="dBday" type="date" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px"><small>Party hat and confetti all day. The year doesn't matter.</small></div>
          <div class="sl"><div class="sl-top"><label for="dDate">Try a date</label></div>
            <input id="dDate" type="date" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px"><small>Only for this page: see what Coaster wears on that day.</small></div>
          <div class="row"><button type="button" id="dShades">Sunglasses on/off</button></div>
        </div>
      </section>
      <section class="card">
        <div class="card-h"><h2>Talking and clock</h2></div>
        <div class="card-b deco">
          <div class="sl"><div class="sl-top"><label for="dTalk">Coaster talks</label></div>
            <select id="dTalk" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px"><option value="2">Often</option><option value="1" selected>Sometimes</option><option value="0">Never</option></select>
            <small>Speech bubbles of its own: print milestones, the filament, what it thinks. Printer messages (M117) always show.</small></div>
          <div class="sl"><div class="sl-top"><label for="dClock">Clock when idle</label></div>
            <select id="dClock" style="font:inherit;color:var(--text);background:var(--panel-2);border:1px solid var(--line-2);border-radius:8px;padding:8px 10px"><option value="1">12 hour</option><option value="2">24 hour</option><option value="0">Off</option></select>
            <small>Under Coaster when nothing is printing.</small></div>
        </div>
      </section>
      <section class="card">
        <div class="card-h"><h2>Coaster's album</h2></div>
        <div class="card-b"><div id="album" class="album note">Loading…</div></div>
      </section>
      <section class="card">
        <div class="card-h"><h2>Motion</h2><span class="sp"></span><span class="note" id="scenTime"></span></div>
        <div class="card-b">
          <div class="scen" id="scen"></div>
          <div class="row">
            <label class="file"><input type="file" id="csv" accept=".csv,text/csv" hidden>Play a Klipper accelerometer CSV</label>
            <button type="button" id="shake">Shake it</button>
          </div>
          <p class="note">The CSV is the file from <code>ACCELEROMETER_MEASURE</code> or <code>TEST_RESONANCES</code> (<code>/tmp/*.csv</code> on the Pi): <code>time, accel_x, accel_y, accel_z</code> in mm/s². It plays at real speed and loops.</p>
        </div>
      </section>

      <section class="card">
        <div class="card-h"><h2>Accelerometer</h2><span class="sp"></span>
          <div class="legend"><span><i style="background:var(--amber)"></i>X</span><span><i style="background:var(--cyan)"></i>Y</span><span><i style="background:var(--violet)"></i>Z</span><span><i style="background:#E7EEF4;opacity:.5"></i>used-to level</span></div>
        </div>
        <div class="card-b"><canvas id="scope" height="150" aria-label="Last 4 seconds of acceleration"></canvas>
          <p class="note">Last 4 seconds, ±2 g, gravity removed. The white line is the shaking it has gotten used to; only motion above it excites the face.</p></div>
      </section>

      <section class="card">
        <div class="card-h"><h2>Printer</h2></div>
        <div class="card-b">
          <div class="toggles">
            <label class="toggle"><input type="checkbox" id="hot"> Nozzle hot (sweats)</label>
            <label class="toggle"><input type="checkbox" id="paused"> Paused (bored)</label>
            <button type="button" id="done">Print finished</button>
            <button type="button" id="homeBtn">Endstop hit</button>
          </div>
        </div>
      </section>
      <section class="card">
        <div class="card-h"><h2>Quirks</h2></div>
        <div class="card-b">
          <p class="note">Every few seconds Coaster does little things on its own, printing or not. Tap one to see it now.</p>
          <div class="toggles" id="quirkBtns"></div>
        </div>
      </section>

      <section class="card">
        <div class="card-h"><h2>Tuning</h2><span class="sp"></span><span class="saved" id="saved"></span></div>
        <div class="card-b">
          <div class="sliders" id="sliders"></div>
          <div class="row">
            <button type="button" class="btn-primary" id="saveK">Save to KNOMI</button>
            <button type="button" id="loadK">Undo changes</button>
            <button type="button" id="reset">Default tuning</button>
          </div>
        </div>
      </section>

    </div>
  </div>
</div>

<script>
(function(){
"use strict";
var $=function(id){return document.getElementById(id)};
var HZ=240, DT=1/HZ, G=9806.65;

/* ---------------- tuning ---------------- */
var TUNE_DEF={wobble:2.0,settle:0.2,sense:1.0,habit:20,scare:0.6,dizzy:5,sleep:20};
var SENSE_K=3, BOUNCE=6;   // same as the firmware
var TUNE_META=[
  ["wobble","Wobble","Hz",0.8,6,0.1,"How fast the head bounces back. Low is floppy, high is stiff."],
  ["settle","Settle","",0.05,1,0.01,"Damping. Low keeps it wobbling after a move; 1 stops dead."],
  ["sense","Sensitivity","×",0.2,3,0.05,"How far a 1 g shove throws the head and eyes."],
  ["habit","Gets used to it","s",2,60,1,"How long before steady shaking stops being exciting."],
  ["scare","Scream at","g",0.3,2.5,0.05,"Motion above the used-to level that makes it scream."],
  ["dizzy","Dizzy after","s",1,20,0.5,"Seconds of screaming before it gets dizzy."],
  ["sleep","Sleepy after","s",5,120,1,"Seconds of stillness before it dozes off."]
];
var T={};
var saved=null;
Object.keys(TUNE_DEF).forEach(function(k){T[k]=saved&&typeof saved[k]=="number"?saved[k]:TUNE_DEF[k]});
function saveTune(){}
function buildSliders(){
  $("sliders").innerHTML=TUNE_META.map(function(m){
    return '<div class="sl"><div class="sl-top"><label for="t_'+m[0]+'">'+m[1]+'</label><output id="o_'+m[0]+'"></output></div>'+
      '<input type="range" id="t_'+m[0]+'" min="'+m[3]+'" max="'+m[4]+'" step="'+m[5]+'" value="'+T[m[0]]+'"><small>'+m[6]+'</small></div>';
  }).join("");
  TUNE_META.forEach(function(m){
    var inp=$("t_"+m[0]),out=$("o_"+m[0]);
    var show=function(){out.textContent=(+T[m[0]]).toFixed(m[5]<0.1?2:(m[5]<1?1:0))+(m[2]?" "+m[2]:"")};
    show();
    inp.addEventListener("input",function(){T[m[0]]=+inp.value;show();saveTune()});
  });
}
buildSliders();
$("reset").addEventListener("click",function(){T=Object.assign({},TUNE_DEF);saveTune();buildSliders()});
/* ---------------- your KNOMI ---------------- */
var faceColor="#C02F30";
fetch("/status.json").then(function(r){return r.json()}).then(function(j){if(j.theme)faceColor=j.theme}).catch(function(){});
function say(msg,bad){var e=$("saved");e.textContent=msg;e.className="saved"+(bad?" bad":"");clearTimeout(say.t);say.t=setTimeout(function(){e.textContent=""},3000)}
function knomiLoad(){
  return fetch("/coaster.json").then(function(r){return r.json()}).then(function(j){
    Object.keys(TUNE_DEF).forEach(function(k){if(typeof j[k]=="number")T[k]=j[k]});decoLoad(j);buildSliders();
  }).catch(function(){say("Couldn't read the KNOMI's tuning",true)});
}
function knomiSave(msg){
  var body=Object.assign({},T,decoSettings());
  fetch("/coaster.json",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)})
    .then(function(r){if(!r.ok)throw 0;say(msg)}).catch(function(){say("Not saved, try again",true)});
}
["dMode","dLights","dAnim","dHemi","dBday","dTalk","dClock"].forEach(function(id){$(id).addEventListener("change",function(){knomiSave("Saved")})});
function album(){fetch("/coaster/album").then(function(r){return r.json()}).then(function(a){
  function esc(x){return String(x).replace(/[&<>]/g,"")}
  var born=a.born?new Date(a.born*1000).toLocaleDateString(undefined,{year:"numeric",month:"long",day:"numeric"}):"not yet (no clock)";
  var bd=(a.bday||"9-28").split("-"),bdt=new Date(2000,+bd[0]-1,+bd[1]).toLocaleDateString(undefined,{month:"long",day:"numeric"});
  function hm(s){var h=Math.floor(s/3600),m=Math.round(s%3600/60);return(h?h+" h ":"")+m+" min"}
  var tr=a.traits||{},top=Object.keys(tr).sort(function(x,y){return tr[y]-tr[x]});
  var fil=(a.filaments||[]).slice().sort(function(x,y){return y.like-x.like});
  var rows=[["First switched on",born],["Birthday",bdt],["Prints together",a.prints+(a.failed?" ("+a.failed+" didn't make it)":"")],
    ["Best streak",a.best+" in a row"],["Time printing",a.hours.toFixed(1)+" h"],["Longest print",a.longest?hm(a.longest):"none yet"],
    ["Wildest ride",a.wildest?a.wildest.toFixed(2)+" g":"none yet"],["Screams, dizzy spells",a.screams+", "+a.dizzies],
    ["Favorite time of year",a.season],["Personality","most "+top[0]+", least "+top[top.length-1]+(a.open>0.15?", loves a change of pace":a.open<-0.15?", a creature of habit":"")],
    ["Favorite filament",fil.length?fil[0].name+(fil[0].prints?" ("+fil[0].prints+" prints)":""):"?"],["Least favorite",fil.length?fil[fil.length-1].name:"?"],
    ["Feeling",a.feeling]];
  $("album").innerHTML="<dl class='kv'>"+rows.map(function(r){return"<dt>"+esc(r[0])+"</dt><dd>"+esc(r[1])+"</dd>"}).join("")+"</dl>";
}).catch(function(){$("album").textContent="The album is on the KNOMI (firmware OP28 or newer)."})}
album();setInterval(album,60000);
function knomiCard(){fetch("/coaster/card").then(function(r){return r.json()}).then(function(j){var r=j.report;if(!r){$("kReport").textContent="";return}
  var h=Math.floor(r.secs/3600),m=Math.floor(r.secs%3600/60);
  $("kReport").textContent="Last print: "+(r.done?"done":"stopped at "+r.progress+"%")+" after "+(h?h+"h ":"")+m+"m · "+r.screams+" screams · peak "+r.peak.toFixed(1)+" g · dizzy "+r.dizzies+"x · "+r.jolts+" jolts"}).catch(function(){})}
knomiCard();setInterval(knomiCard,10000);
$("saveK").addEventListener("click",function(){knomiSave("Saved. The KNOMI uses it now.")});
$("loadK").addEventListener("click",function(){knomiLoad().then(function(){say("Back to the KNOMI's tuning")})});
var KCOL={sad:"#9AA7F0",shocked:"#E06C5A",lonely:"#7E8F9F",confused:"#F0C079",impatient:"#E8A33D","cooling off":"#4FD1C5",focused:"#4FD1C5","almost there":"#E8A33D",hungry:"#F0C079",windy:"#4FD1C5","hanging on":"#E06C5A",ready:"#E8A33D",calm:"#93A4B2",riding:"#4FD1C5",excited:"#E8A33D",screaming:"#E06C5A",startled:"#F0C079",dizzy:"#9AA7F0",shivering:"#4FD1C5",elevator:"#9AA7F0",sleepy:"#7E8F9F",bored:"#7E8F9F",giggle:"#F0C079",celebrate:"#E8A33D",whee:"#E8A33D",mad:"#E06C5A"};
function knomiPoll(){
  fetch("/coaster/state").then(function(r){return r.json()}).then(function(j){
    var c=$("kMood");c.textContent=j.mood;c.style.color=KCOL[j.mood]||"#E8A33D";
    var th=Math.min(1,j.thrill),bz=Math.min(1,j.buzz/0.4),dz=Math.min(1,j.dizzy);
    $("kThrill").style.width=th*100+"%";$("kvThrill").textContent=Math.round(j.thrill*100)+"%";
    $("kBuzz").style.width=bz*100+"%";$("kvBuzz").textContent=Math.round(bz*100)+"%";
    $("kDizzy").style.width=dz*100+"%";$("kvDizzy").textContent=Math.round(dz*100)+"%";
    if(typeof j.feel=="number"){
      var fv=j.feel;$("kFeelBar").style.width=Math.round((fv+1)*50)+"%";$("kFeelBar").style.background=fv>0.25?"var(--cyan)":fv>-0.15?"var(--amber)":"var(--danger)";
      $("kFeel").textContent=j.feeling;
      $("kWhy").textContent=j.why&&j.why.length?"Lately: "+j.why.map(function(w){return w.t+" ("+(w.d>0?"+":"")+w.d+")"}).join(" · "):"";
      $("kLikes").textContent=(j.likes&&j.likes.length?"Likes "+j.likes.join(", ")+". ":"")+(j.dislikes&&j.dislikes.length?"Doesn't like "+j.dislikes.join(", ")+".":"")+
        (j.prints?" "+j.prints+" prints together"+(j.streak>1?", "+j.streak+" in a row.":"."):"");
      var hb=j.habit||{},lines=[];
      lines.push(j.open>0.15?"Loves a change of pace.":j.open<-0.15?"A creature of habit.":"Takes things as they come.");
      if(hb.n>=5){var hr=hb.hour>=0?Math.round(hb.hour):-1;
        lines.push("Used to "+(hb.min>=90?(hb.min/60).toFixed(1)+" h":Math.round(hb.min)+" min")+" prints"+(hr>=0?", usually started around "+((hr+11)%12+1)+(hr<12?" am":" pm"):"")+".")}
      else lines.push("Still learning your habits ("+(hb.n||0)+" of 5 prints).");
      if(j.learned&&j.learned.length)lines.push("Learned: "+j.learned.map(function(l){return(l.d>0?"warming up to ":"going off ")+l.t}).join(", ")+".");
      $("kHabits").textContent=lines.join(" ");
    }
    $("kNote").textContent=j.sensor?"Live mood from the KNOMI's own accelerometer. Used to "+j.used_to.toFixed(2)+" g of motion right now.":
      "This board has no accelerometer, so the face reacts to printer data only (Z moves, heat, pause, print done).";
  }).catch(function(){$("kMood").textContent="offline"}).then(function(){setTimeout(knomiPoll,500)});
}
knomiLoad();knomiPoll();

/* ---------------- motion sources ---------------- */
// Each scenario is a generator of planned moves; a move is accel / cruise / decel along a unit vector.
function rnd(a,b){return a+Math.random()*(b-a)}
function norm(v){var l=Math.hypot(v[0],v[1],v[2])||1;return[v[0]/l,v[1]/l,v[2]/l]}
function move(dir,dist,speed,accelG){ // returns list of [duration, ax,ay,az] segments (g)
  var a=accelG*G, d=norm(dir), ta=speed/a, da=0.5*a*ta*ta;
  if(2*da>dist){ta=Math.sqrt(dist/a);da=dist/2}
  var vmax=a*ta, tc=Math.max(0,(dist-2*da)/vmax);
  var s=[[ta,d[0]*accelG,d[1]*accelG,d[2]*accelG]];
  if(tc>0)s.push([tc,0,0,0]);
  s.push([ta,-d[0]*accelG,-d[1]*accelG,-d[2]*accelG]);
  return s;
}
function pause(t){return[[t,0,0,0]]}
function bump(dir,g,t){var d=norm(dir);return[[t,d[0]*g,d[1]*g,d[2]*g]]}
var SCEN={
  full:{name:"Whole print",desc:"Homing, QGL, then perimeters, infill and travel",gen:function(){
    return SCEN.homing.gen().concat(SCEN.qgl.gen(),SCEN.perim.gen(),SCEN.infill.gen(),SCEN.travel.gen(),SCEN.infill.gen(),pause(2))}},
  perim:{name:"Perimeters",desc:"Careful loops, 5k mm/s²",gen:function(){
    var s=[],ang=0;for(var i=0;i<28;i++){ang+=Math.PI/2+rnd(-0.3,0.3);s=s.concat(move([Math.cos(ang),Math.sin(ang)*0.35,0],rnd(10,35),120,0.5))}return s}},
  infill:{name:"Infill",desc:"Fast zig-zag, 10k mm/s², constant flipping",gen:function(){
    var s=[];for(var i=0;i<34;i++){var sg=i%2?1:-1;s=s.concat(move([sg,sg*0.3,0],rnd(30,55),300,1.0))}return s}},
  travel:{name:"Travel moves",desc:"Long hops, 15k mm/s², then still",gen:function(){
    var s=[];for(var i=0;i<8;i++){var a=rnd(0,Math.PI*2);s=s.concat(move([Math.cos(a),Math.sin(a)*0.5,0],rnd(80,220),500,1.5),pause(rnd(0.2,0.9)))}return s}},
  shaper:{name:"Input shaper test",desc:"Buzzing sweep 5 → 90 Hz",gen:function(){return[["sweep",14]]}},
  homing:{name:"Homing",desc:"Slow crawl, then a hard stop at each endstop",gen:function(){
    var s=[];[[1,0,0],[0,0.4,0],[0,0,-1]].forEach(function(d){
      s=s.concat(move(d,120,60,0.3).slice(0,2),bump([-d[0],-d[1],-d[2]],2.6,0.03),pause(0.3),move([-d[0],-d[1],-d[2]],5,20,0.2),pause(0.4),move(d,6,8,0.1).slice(0,2),bump([-d[0],-d[1],-d[2]],1.4,0.02),pause(0.6))});
    return s}},
  qgl:{name:"Gantry leveling (QGL)",desc:"Up and down, like an elevator",gen:function(){
    var s=[];for(var i=0;i<5;i++){s=s.concat(move([0,0,1],22,15,0.25),pause(0.5),move([0,0,-1],22,15,0.25),pause(0.4),move([rnd(-1,1),rnd(-.4,.4),0],150,300,0.8))}return s}},
  idle:{name:"Idle",desc:"Nothing happening, it gets sleepy",gen:function(){return pause(10)}}
};
var scenKey="full", plan=[], planIdx=0, segT=0, sweepT=-1, sweepDur=0, csv=null, csvT=0;
function buildScen(){
  $("scen").innerHTML=Object.keys(SCEN).map(function(k){return'<button type="button" data-k="'+k+'" aria-pressed="'+(k==scenKey&&!csv)+'"><b>'+SCEN[k].name+'</b><small>'+SCEN[k].desc+'</small></button>'}).join("")+
    '<button type="button" data-k="hand" aria-pressed="'+(scenKey=="hand"&&!csv)+'"><b>Just my hand</b><small>No printer motion, drag the KNOMI yourself</small></button>'+
    (csv?'<button type="button" data-k="csv" aria-pressed="true"><b>'+escapeHtml(csv.name)+'</b><small>'+csv.dur.toFixed(1)+' s recording, looping</small></button>':"");
}
function escapeHtml(s){return String(s).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
$("scen").addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;var k=b.dataset.k;
  if(k=="csv")return;csv=null;scenKey=k;plan=[];planIdx=0;segT=0;sweepT=-1;buildScen()});
buildScen();
function scenarioAccel(dt){ // returns [ax,ay,az] in g
  if(csv){
    csvT+=dt;if(csvT>=csv.dur)csvT=0;
    var i=Math.min(csv.t.length-1,Math.max(0,bsearch(csv.t,csvT)));
    return[csv.x[i],csv.y[i],csv.z[i]];
  }
  if(scenKey=="hand")return[0,0,0];
  if(sweepT>=0){
    sweepT+=dt;
    if(sweepT>=sweepDur){sweepT=-1;}
    else{
      var half=sweepDur/2, local=sweepT%half, f0=5,f1=90, k=(f1-f0)/half;
      var ph=2*Math.PI*(f0*local+0.5*k*local*local), amp=0.9*Math.min(1,local*2);
      var v=amp*Math.sin(ph);
      return sweepT<half?[v,0,0]:[0,v,0];
    }
  }
  for(;;){
    if(planIdx>=plan.length){plan=SCEN[scenKey].gen();planIdx=0;segT=0}
    var s=seg();
    if(s[0]==="sweep"){sweepT=0;sweepDur=s[1];planIdx++;segT=0;return[0,0,0]}
    if(segT<s[0])break;
    segT-=s[0];planIdx++;
  }
  segT+=dt;
  var c=seg();return[c[1],c[2],c[3]];
}
function seg(){return plan[planIdx]}
function bsearch(a,v){var lo=0,hi=a.length-1;while(lo<hi){var m=(lo+hi)>>1;if(a[m]<v)lo=m+1;else hi=m}return lo}
$("csv").addEventListener("change",function(){
  var f=this.files[0];if(!f)return;var rd=new FileReader();
  rd.onload=function(){
    var t=[],x=[],y=[],z=[];
    rd.result.split(/\r?\n/).forEach(function(l){
      if(!l||l[0]=="#")return;var p=l.split(",");if(p.length<4)return;
      var v=p.slice(0,4).map(Number);if(v.some(isNaN))return;t.push(v[0]);x.push(v[1]);y.push(v[2]);z.push(v[3]);
    });
    if(t.length<20){$("scenTime").textContent="That file has no accelerometer rows";return}
    // remove gravity / offsets per axis, convert to g, start at 0 s
    var mean=function(a){return a.reduce(function(s,v){return s+v},0)/a.length};
    var mx=mean(x),my=mean(y),mz=mean(z),t0=t[0];
    csv={name:f.name,t:t.map(function(v){return v-t0}),x:x.map(function(v){return(v-mx)/G}),y:y.map(function(v){return(v-my)/G}),z:z.map(function(v){return(v-mz)/G})};
    csv.dur=csv.t[csv.t.length-1]||1;csvT=0;buildScen();$("scenTime").textContent="";
  };
  rd.readAsText(f);this.value="";
});

/* ---------------- hand input (drag / flick) ---------------- */
var dev={x:0,y:0,d:0,vx:0,vy:0,vd:0,drag:false,px:0,py:0,shift:false}; // px on screen, d = toward/away
var devAcc=[0,0,0], prevV=[0,0,0];
var mount=$("mount"), device=$("device");
var dragStart=null, moved=0;
device.addEventListener("pointerdown",function(e){
  dev.drag=true;dev.shift=e.shiftKey;device.setPointerCapture(e.pointerId);
  dragStart={x:e.clientX,y:e.clientY,ox:dev.x,oy:dev.y,od:dev.d,t:performance.now()};moved=0;e.preventDefault();
});
device.addEventListener("pointermove",function(e){
  if(!dev.drag)return;var dx=e.clientX-dragStart.x,dy=e.clientY-dragStart.y;moved=Math.max(moved,Math.hypot(dx,dy));
  if(dev.shift){dev.d=clamp(dragStart.od-dy,-90,90)}else{dev.px=clamp(dragStart.ox+dx,-110,110);dev.py=clamp(dragStart.oy+dy,-110,110)}
});
function endDrag(){if(!dev.drag)return;dev.drag=false;
  if(moved<6&&performance.now()-dragStart.t<350)poke();}
device.addEventListener("pointerup",endDrag);device.addEventListener("pointercancel",endDrag);
function clamp(v,a,b){return Math.max(a,Math.min(b,v))}
$("shake").addEventListener("click",function(){shakeT=1.2});
var shakeT=0;
function stepDevice(dt){
  // while dragging the device follows the pointer; released, it springs home on its mount
  var ox=dev.x,oy=dev.y,od=dev.d;
  if(dev.drag&&!dev.shift){dev.vx=(dev.px-dev.x)/dt*0.35;dev.vy=(dev.py-dev.y)/dt*0.35;dev.x+=dev.vx*dt;dev.y+=dev.vy*dt;}
  else{var k=(2*Math.PI*3.2)*(2*Math.PI*3.2),c=2*0.18*Math.sqrt(k);
    dev.vx+=(-k*dev.x-c*dev.vx)*dt;dev.vy+=(-k*dev.y-c*dev.vy)*dt;dev.x+=dev.vx*dt;dev.y+=dev.vy*dt;}
  if(!(dev.drag&&dev.shift)){var k2=(2*Math.PI*3.2)*(2*Math.PI*3.2),c2=2*0.18*Math.sqrt(k2);dev.vd+=(-k2*dev.d-c2*dev.vd)*dt;dev.d+=dev.vd*dt}
  else{dev.vd=(dev.d-od)/dt}
  if(shakeT>0){shakeT-=dt;var f=7,a=45*Math.min(1,shakeT);dev.x=a*Math.sin(2*Math.PI*f*shakeT);dev.vx=0}
  var v=[(dev.x-ox)/dt,(dev.d-od)/dt,-(dev.y-oy)/dt];
  // screen px/s² → g (one screen px ≈ 0.25 mm of toolhead travel)
  for(var i=0;i<3;i++){var a=(v[i]-prevV[i])/dt*0.25/G;devAcc[i]+= (clamp(a,-3,3)-devAcc[i])*0.25;prevV[i]=v[i]}
}

/* ---------------- sensing: what the face "feels" ---------------- */
var S={act2:0,act:0,quietT:0,whee:0,bx:[0,0,0],lp:[0,0,0],m:[0,0,0],ring:[],prev:[0,0,0],rmsShort:0,base:0,vib:0,step:0,thrill:0,screamT:0,dizzyMeter:0,stillT:0,vz:0,last:[0,0,0]};
function sense(a,dt){
  // low-pass ~15 Hz = the "felt" motion, the rest is vibration
  var aLP=1-Math.exp(-2*Math.PI*15*dt);
  var hpE=0;
  for(var i=0;i<3;i++){S.lp[i]+=(a[i]-S.lp[i])*aLP;var h=a[i]-S.lp[i];hpE+=h*h}
  var mag=Math.hypot(S.lp[0],S.lp[1],S.lp[2]);
  // peak envelope: toolhead moves are short hard pulses (a 15k mm/s² travel accelerates for ~30 ms)
  S.rmsShort=Math.max(mag,S.rmsShort*Math.exp(-dt/0.3));
  S.act2+=(mag*mag-S.act2)*(1-Math.exp(-dt/0.12));S.act=Math.sqrt(S.act2);
  S.vib+=(Math.sqrt(hpE)-S.vib)*(1-Math.exp(-dt/0.6));
  // sudden step: change of the 40 Hz-filtered signal over the last 25 ms (endstop hits, not normal accel ramps)
  var aM=1-Math.exp(-2*Math.PI*40*dt);
  for(var q=0;q<3;q++)S.m[q]+=(a[q]-S.m[q])*aM;
  S.ring.push(S.m.slice());if(S.ring.length>6)S.ring.shift();
  var o=S.ring[0];S.step=Math.hypot(S.m[0]-o[0],S.m[1]-o[1],S.m[2]-o[2]);
  S.fromRest=Math.hypot(o[0],o[1],o[2])<0.3;
  S.base+=(S.act-S.base)*(1-Math.exp(-dt/Math.max(1,T.habit)));  // habituation
  // a burst of motion after a pause (a travel, a new perimeter): whee!
  S.whee=Math.max(0,S.whee-dt);
  if(S.act<0.1)S.quietT+=dt;else if(S.act>0.15){if(S.quietT>0.3&&S.whee<=0){S.whee=0.5;headKick(0,-60*T.sense)}S.quietT=0}
  S.thrill=Math.max(0,S.rmsShort-0.7*S.base)/T.scare;
  // Z speed in mm/s (integrated, leaking back to 0). On the KNOMI this would come from OctoPrint's Z position.
  S.vz=(S.vz+a[2]*G*dt)*Math.exp(-dt/3);
  S.stillT=(S.rmsShort<0.03&&S.vib<0.03)?S.stillT+dt:0;
  S.last=a;
}

/* ---------------- mood ---------------- */
var mood="calm", moodT=0, why="", timers={startle:0,dizzy:0,giggle:0,celebrate:0};
var printer={hot:false,paused:false};
$("hot").addEventListener("change",function(){printer.hot=this.checked});
$("paused").addEventListener("change",function(){printer.paused=this.checked});
$("done").addEventListener("click",function(){timers.celebrate=4.5;spawnConfetti()});
$("homeBtn").addEventListener("click",function(){impulse([0,0,-2.8],0.03)});
var impulses=[];function impulse(v,t){impulses.push({v:v,t:t})}
function poke(){timers.giggle=1.6;headKick(0,-60)}
function pickMood(dt){
  for(var k in timers)timers[k]=Math.max(0,timers[k]-dt);
  if(S.step>1.8&&S.fromRest&&S.vib<0.35&&timers.startle<=0&&timers.celebrate<=0){timers.startle=0.7;blinkNow();headKick(0,-40)}
  if(S.thrill>1){S.screamT+=dt;S.dizzyMeter+=dt*Math.min(2,S.thrill)}else{S.screamT=Math.max(0,S.screamT-dt*2);S.dizzyMeter=Math.max(0,S.dizzyMeter-dt*0.6)}
  if(S.dizzyMeter>T.dizzy&&timers.dizzy<=0){timers.dizzy=3.5;S.dizzyMeter=0}
  var m,w;
  if(timers.celebrate>0){m="celebrate";w="Print finished."}
  else if(timers.giggle>0){m="giggle";w="You poked it."}
  else if(timers.dizzy>0){m="dizzy";w="Too much screaming. Needs a second."}
  else if(timers.startle>0){m="startled";w="Sudden jolt ("+S.step.toFixed(1)+" g in 25 ms)."}
  else if(S.vib>0.28){m="shivering";w=S.vib.toFixed(2)+" g of buzz above 15 Hz."}
  else if(S.thrill>1&&S.screamT>0.4){m="screaming";w=S.rmsShort.toFixed(2)+" g peaks, used to "+S.base.toFixed(2)+" g."}
  else if(S.whee>0){m="whee";w="Moving again after a pause."}
  else if(S.thrill>0.35){m="excited";w=S.rmsShort.toFixed(2)+" g peaks, used to "+S.base.toFixed(2)+" g."}
  else if(Math.abs(S.vz)>4){m="elevator";w=(S.vz>0?"Going up":"Going down")+" at "+Math.abs(S.vz).toFixed(0)+" mm/s."}
  else if(printer.paused){m="bored";w="Printer paused."}
  else if(S.stillT>T.sleep){m="sleepy";w="Still for "+Math.round(S.stillT)+" s."}
  else if(S.rmsShort>0.05||S.vib>0.05){m="riding";w=S.base>0.2?"Used to "+S.base.toFixed(2)+" g of motion by now.":"Some motion."}
  else{m="calm";w="Nothing moving."}
  if(m!==mood){mood=m;moodT=0}else moodT+=dt;
  why=w;
}

/* ---------------- body: springs ---------------- */
var head={x:0,y:0,vx:0,vy:0,s:0,vs:0}, pup={x:0,y:0,vx:0,vy:0};
function headKick(vx,vy){head.vx+=vx;head.vy+=vy}
function squash(v){return (v<0?-1:1)*Math.pow(Math.abs(v),0.6)}
function stepBody(lp,dt){
  var a=[squash(lp[0]),squash(lp[1]),squash(lp[2])], sn=T.sense*SENSE_K;
  var w=2*Math.PI*T.wobble,k=w*w,c=2*T.settle*w, gain=sn*k*18; // 18 px per (squashed) g at rest
  // the spring can't follow 3-10 Hz toolhead motion, so the head also jiggles with it directly
  var k8=1-Math.exp(-2*Math.PI*8*dt);for(var i=0;i<3;i++)S.bx[i]+=(a[i]-S.bx[i])*k8;
  // inertia: the head lags opposite the acceleration. screen x ← X, screen y ← Z (up = -y), depth ← Y
  head.vx+=(-k*head.x-c*head.vx-gain*a[0])*dt;
  head.vy+=(-k*head.y-c*head.vy+gain*a[2])*dt;
  head.vs+=(-k*head.s-c*head.vs-sn*k*0.16*a[1])*dt;
  head.x+=head.vx*dt;head.y+=head.vy*dt;head.s+=head.vs*dt;
  head.x=clamp(head.x,-34,34);head.y=clamp(head.y,-30,30);head.s=clamp(head.s,-0.3,0.3);
  // pupils: looser spring inside the eye, they slosh after the head
  var w2=2*Math.PI*T.wobble*1.35,k2=w2*w2,c2=2*T.settle*0.6*w2,g2=sn*k2*11;
  pup.vx+=(-k2*pup.x-c2*pup.vx-g2*a[0]-head.vx*0.0)*dt;
  pup.vy+=(-k2*pup.y-c2*pup.vy+g2*a[2])*dt;
  pup.x+=pup.vx*dt;pup.y+=pup.vy*dt;
  var r=Math.hypot(pup.x,pup.y),lim=13;if(r>lim){pup.x*=lim/r;pup.y*=lim/r;pup.vx*=0.4;pup.vy*=0.4}
}

/* ---------------- expression ---------------- */
// One face that morphs: every mood is a set of numbers for the same eye and mouth,
// and the face eases between them, so it never jumps to a different drawing.
// Eye = a pupil disc under a lid line (stock KNOMI look). open: 0 lid shut, .5 half disc, 1 round.
// cheek: pushes up from below for happy "^" eyes. orbit: pupils circle (dizzy).
// Mouth = one curve: w half-width, curve smile, omega the cat "w", gape how far it opens, zig/wave wobble.
var E={open:0.5,size:1,cheek:0,orbit:0,look:0,w:14,curve:0,omega:1,gape:0,zig:0,wave:0};
var MOODS={
  calm:     {open:0.5, size:1,   cheek:0,orbit:0,w:14,curve:0,  omega:1,gape:0,  zig:0,wave:0},
  riding:   {open:0.56,size:1,   cheek:0,orbit:0,w:14,curve:0.3,omega:0.7,gape:0,zig:0,wave:0},
  excited:  {open:0.85,size:1.05,cheek:0,orbit:0,w:12,curve:0.7,omega:0,gape:0.9,zig:0,wave:0},
  screaming:{open:1,   size:1.3, cheek:0,orbit:0,w:8, curve:0,  omega:0,gape:1.7,zig:0,wave:0},
  startled: {open:1,   size:1.2, cheek:0,orbit:0,w:6, curve:0,  omega:0,gape:1.1,zig:0,wave:0},
  elevator: {open:0.75,size:1.05,cheek:0,orbit:0,w:7, curve:0,  omega:0,gape:0.6,zig:0,wave:0},
  sleepy:   {open:0.02,size:1,   cheek:0,orbit:0,w:11,curve:0,  omega:1,gape:0,  zig:0,wave:0},
  bored:    {open:0.32,size:1,   cheek:0,orbit:0,w:9, curve:0,  omega:0,gape:0,  zig:0,wave:0},
  shivering:{open:0.42,size:1,   cheek:0,orbit:0,w:14,curve:0,  omega:0,gape:0,  zig:1,wave:0},
  dizzy:    {open:0.5, size:1,   cheek:0,orbit:1,w:14,curve:0,  omega:0,gape:0,  zig:0,wave:1},
  giggle:   {open:1,   size:1.05,cheek:1,orbit:0,w:12,curve:0.8,omega:0,gape:0.8,zig:0,wave:0},
  celebrate:{open:1,   size:1.1, cheek:1,orbit:0,w:15,curve:0.9,omega:0,gape:1,  zig:0,wave:0},
  whee:     {open:1,   size:1.15,cheek:0,orbit:0,w:13,curve:0.8,omega:0,gape:1.3,zig:0,wave:0}
};
var blink={t:rnd(2,5),closing:0};
function blinkNow(){blink.closing=0.16}
var wander={x:0,tx:0,t:0};
function stepExpr(dt){
  wander.t-=dt;if(wander.t<=0){wander.tx=rnd(-12,12);wander.t=rnd(1.2,3.5)}
  wander.x+=(wander.tx-wander.x)*(1-Math.exp(-dt*5));
  var t=MOODS[mood]||MOODS.calm, kk=1-Math.exp(-dt*9);
  for(var k in t)E[k]+=(t[k]-E[k])*kk;
  var look=(mood=="bored")?Math.sin(now*1.3)*12:(mood=="calm"||mood=="riding"||mood=="sleepy")?wander.x:0;
  E.look+=(look-E.look)*kk;
  if(blink.closing>0)blink.closing-=dt;
  else{blink.t-=dt;if(blink.t<=0&&mood!="sleepy"&&mood!="dizzy"){blinkNow();blink.t=rnd(2.2,6)}}
}

/* ---------------- quirks (same as the firmware) ---------------- */
var QUIRKS={glance:1.6,"double blink":0.5,"slow blink":1.3,wink:0.8,yawn:2.4,hum:3.6,sneeze:1.7,"look up":1.9,stretch:1.8,"eye roll":1.3,nod:0.7,cheer:1.3,sigh:1.9,huff:0.9,cough:1.1};
var Q={name:"",t:0,side:1,fired:{},next:5}, QF={}, notes=[], sacc={x:0,y:0,tx:0,ty:0,t:1};
function qReset(){QF={look:0,lookY:0,openL:1,openR:1,gape:0,curve:0,w:0,cheek:0,dx:0,dy:0,sq:0}}qReset();
function bump(t,d,e){return clamp(Math.min(t/e,(d-t)/e),0,1)}
function qOnce(n){if(Q.fired[n])return false;Q.fired[n]=1;return true}
function qStart(n){Q.name=n;Q.t=0;Q.fired={};Q.side=Math.random()<0.5?-1:1}
function qPick(){
  var w={};
  if(mood=="calm"){w.glance=3;w["look up"]=1.5;w["double blink"]=1.5;w["slow blink"]=1.5;w.yawn=S.stillT>60?2.5:0.8;w.hum=1.5;w.wink=0.8;w.sneeze=0.35;w.stretch=0.8}
  else if(mood=="riding"){w.glance=2;w["double blink"]=1.5;w.wink=0.8;w.hum=1;w["look up"]=0.8;w["slow blink"]=0.6}
  else if(mood=="bored"){w["eye roll"]=2;w.yawn=2;w.glance=1;w.hum=1}
  else if(mood=="sleepy"){w.yawn=1}
  else if(mood=="screaming"||mood=="startled"||mood=="dizzy"||mood=="whee"||mood=="shivering"){w["double blink"]=1;w.glance=0.5}
  else{w.glance=2;w["double blink"]=1.5;w.wink=0.8;w.hum=1.2;w["look up"]=0.8;w["slow blink"]=0.6}
  if(printer.hot)w.sneeze=(w.sneeze||0)+0.3;
  var sum=0,k;for(k in w)sum+=w[k];var r=Math.random()*sum;
  for(k in w){r-=w[k];if(r<=0)return k}return "";
}
function stepQuirks(dt){
  qReset();
  var free=true;   // quirks happen any time, printing or not
  sacc.t-=dt;if(sacc.t<=0){sacc.t=rnd(0.4,2.2);sacc.tx=rnd(-4,4);sacc.ty=rnd(-2,2)}
  var ks=1-Math.exp(-dt*30);sacc.x+=(sacc.tx-sacc.x)*ks;sacc.y+=(sacc.ty-sacc.y)*ks;
  if(mood!="sleepy"&&mood!="dizzy"){QF.look=sacc.x;QF.lookY=sacc.y}
  if(Q.name&&!free&&Q.name!="cheer"&&Q.name!="nod"&&!Q.forced)Q.name="";
  if(!Q.name){
    if(!free){Q.next=Math.max(Q.next,2);return}
    Q.next-=dt;if(Q.next>0)return;
    Q.next=rnd(4,13);
    var n=qPick();if(!n)return;qStart(n);Q.forced=false;
  }
  Q.t+=dt;var t=Q.t,D=QUIRKS[Q.name],e;
  if(t>=D){if(Q.name=="yawn"||Q.name=="sneeze")blinkNow();Q.name="";return}
  switch(Q.name){
    case "glance":e=bump(t,D,0.15);QF.look+=Q.side*18*e;QF.lookY-=2*e;if(t>0.9&&qOnce(0)&&Math.random()<0.5)blinkNow();break;
    case "double blink":if(qOnce(0))blinkNow();if(t>0.26&&qOnce(1))blinkNow();break;
    case "slow blink":e=bump(t,D,0.45);QF.openL=QF.openR=1-0.97*e;QF.curve=0.5*e;break;
    case "wink":e=bump(t,D,0.12);if(Q.side>0)QF.openR=1-e;else QF.openL=1-e;QF.curve=0.7*e;QF.dx=Q.side*2*e;break;
    case "yawn":e=bump(t,D,0.8);QF.gape=1.6*e;QF.w=-5*e;QF.openL=QF.openR=1-0.85*e;QF.dy=-6*e;QF.sq=0.08*e;break;
    case "hum":e=bump(t,D,0.3);QF.w=-8*e;QF.gape=0.45*e;QF.dx=Math.sin(t*4.2)*4*e;QF.dy=-Math.abs(Math.sin(t*4.2))*2*e;QF.openL=QF.openR=1-0.4*e;
      for(var n2=0;n2<3;n2++)if(t>0.3+n2&&qOnce(n2))notes.push({x:150+rnd(-6,10),y:128,life:1.6});break;
    case "sneeze":
      if(t<1.1){e=clamp(t/1.1,0,1);QF.openL=QF.openR=1-0.75*e;QF.dy=-7*e;QF.gape=0.7*e;QF.w=-4*e;QF.dx=Math.sin(t*30)*e}
      else{if(qOnce(0)){headKick(rnd(-30,30),170);blinkNow()}e=1-clamp((t-1.1)/0.6,0,1);QF.openL=QF.openR=1-e;QF.gape=0.2*e}break;
    case "look up":e=bump(t,D,0.3);QF.lookY-=7*e;QF.look+=Q.side*7*e;QF.openL=QF.openR=1+0.2*e;break;
    case "stretch":e=bump(t,D,0.6);QF.sq=0.16*e;QF.cheek=0.9*e;QF.curve=0.5*e;QF.dy=-4*e;QF.openL=QF.openR=1+e;break;
    case "eye roll":var a=clamp(t/D,0,1)*2*Math.PI;e=bump(t,D,0.15);QF.look+=Math.sin(a)*16*e;QF.lookY-=(1-Math.cos(a))*4*e;QF.openL=QF.openR=1-0.25*e;break;
    case "nod":QF.dy=Math.sin(Math.PI*clamp(t/D,0,1))*6;break;
    case "sigh":e=bump(t,D,0.5);QF.openL=QF.openR=1-0.6*e;QF.dy=5*e;QF.curve=-0.3*e;QF.gape=0.25*e;QF.w=-6*e;break;
    case "huff":e=bump(t,D,0.12);QF.dy=4*e;QF.gape=0.4*e;QF.w=-7*e;QF.dx=Math.sin(t*40)*1.5*e;if(qOnce(0))headKick(0,60);break;
    case "cough":e=bump(t,D,0.15);QF.openL=QF.openR=1-0.6*e;QF.gape=0.5*e*Math.abs(Math.sin(t*9));QF.w=-6*e;if(t>0.1&&qOnce(0))headKick(0,70);if(t>0.5&&qOnce(1))headKick(0,55);break;
    case "cheer":e=bump(t,D,0.2);QF.cheek=e;QF.curve=0.8*e;QF.gape=0.6*e;QF.dy=-Math.abs(Math.sin(t*10))*6*e;QF.openL=QF.openR=1+e;break;
  }
}
function stepNotes(dt){notes.forEach(function(n){n.life-=dt;n.y-=22*dt;n.x+=Math.sin(n.life*5)*12*dt});notes=notes.filter(function(n){return n.life>0})}
$("quirkBtns").innerHTML=Object.keys(QUIRKS).map(function(k){return'<button type="button" data-q="'+k+'">'+k+'</button>'}).join("");
$("quirkBtns").addEventListener("click",function(ev){var b=ev.target.closest("[data-q]");if(b){qStart(b.getAttribute("data-q"));Q.forced=true}});

/* ---------------- particles ---------------- */
var confetti=[];
function spawnConfetti(){for(var i=0;i<40;i++)confetti.push({x:120+rnd(-20,20),y:120,vx:rnd(-160,160),vy:rnd(-260,-80),r:rnd(0,6.28),vr:rnd(-8,8),life:rnd(1.8,3.2)})}
function stepParticles(dt){confetti.forEach(function(p){p.vy+=420*dt;p.x+=p.vx*dt;p.y+=p.vy*dt;p.r+=p.vr*dt;p.life-=dt});confetti=confetti.filter(function(p){return p.life>0})}

/* ---------------- drawing ---------------- */
var cv=$("face"),ctx=cv.getContext("2d"),now=0;
var LINE=26, R=11, SW=5; // half lid length, pupil radius, stroke
function lerp(a,b,t){return a+(b-a)*t}
function drawEye(ex,ey,side,sx,open,lookY){
  var r=R*E.size, L=LINE*sx;
  var slide=clamp(pup.x*1.6+E.look+QF.look,-(L-r),L-r);
  var squash=clamp(1+pup.y/22,0.55,1.5);
  var ox=Math.cos(now*6*side)*9*E.orbit, oy=Math.sin(now*6*side)*4*E.orbit;
  var px=ex+slide+ox, py=ey+oy+(lookY||0), ry=r*squash;
  // lid height: open 0 -> below the pupil, .5 -> through its middle, 1 -> above it
  var lidY=py+ry-open*2*ry;
  ctx.save();
  ctx.beginPath();ctx.rect(ex-L-20,lidY,2*L+40,ry*3+40);
  if(E.cheek>0.01){ctx.moveTo(px+r*1.5,py+r*2.4-E.cheek*1.55*r);ctx.arc(px,py+r*2.4-E.cheek*1.55*r,r*1.5,0,Math.PI*2,true)}
  ctx.clip("evenodd");
  ctx.beginPath();ctx.ellipse(px,py,r,ry,0,0,Math.PI*2);ctx.fill();
  ctx.restore();
  // the lid line shrinks into the pupil as the eye opens past halfway, and is gone when round
  var k=clamp((open-0.5)/0.3,0,1);
  if(k<0.98){
    var x0=lerp(ex-L,px,k), x1=lerp(ex+L,px,k);
    ctx.globalAlpha=1-k*0.6;ctx.beginPath();ctx.moveTo(x0,lidY);ctx.lineTo(x1,lidY);ctx.stroke();ctx.globalAlpha=1;
  }
}
function tri(u){return 2*Math.abs(2*(u-Math.floor(u+0.5)))-1}
function drawMouth(mx,my){
  var N=28,top=[],bot=[],w=E.w;
  for(var i=0;i<=N;i++){
    var t=-1+2*i/N, x=mx+t*w, env=1-t*t;
    var y=my+E.curve*7*env+E.omega*5*Math.abs(Math.sin(Math.PI*t))
      +E.zig*3*tri(t*3+(Math.sin(now*38)>0?0.5:0))
      +E.wave*3*Math.sin(t*5+now*9);
    top.push([x,y]);bot.push([x,y+E.gape*9*Math.pow(env,0.7)]);
  }
  ctx.lineWidth=4;ctx.beginPath();
  top.forEach(function(p,i){i?ctx.lineTo(p[0],p[1]):ctx.moveTo(p[0],p[1])});
  if(E.gape>0.06){for(var j=bot.length-1;j>=0;j--)ctx.lineTo(bot[j][0],bot[j][1]);ctx.closePath();ctx.fill()}
  ctx.stroke();
}
function draw(){
  var fc=faceColor;
  ctx.setTransform(2,0,0,2,0,0);
  ctx.fillStyle="#000";ctx.fillRect(0,0,240,240);
  drawDecoBack();
  ctx.strokeStyle=fc;ctx.fillStyle=fc;ctx.lineWidth=SW;ctx.lineCap="round";ctx.lineJoin="round";
  var jit=Math.min(3,S.vib*6)*E.zig;
  var bs=BOUNCE*T.sense*SENSE_K, cx=120+clamp(head.x-bs*S.bx[0],-34,34)+rnd(-jit,jit), cy=118+clamp(head.y+bs*S.bx[2],-30,30)+rnd(-jit,jit);
  var hsq=head.s+QF.sq, sx=1-hsq*0.5, sy=1+hsq;
  cx+=QF.dx;cy+=QF.dy;
  if(timers.giggle>0)cy-=Math.abs(Math.sin(now*14))*6;
  if(timers.celebrate>0)cy-=Math.abs(Math.sin(now*9))*8;
  cy+=Math.sin(now*1.6)*1.5*(1-clamp(E.open*4,0,1)); // breathing while asleep
  var blinkK=blink.closing>0?Math.sin(Math.PI*(1-blink.closing/0.16)):0;
  var open=E.open*(1-blinkK);
  ctx.lineWidth=SW;
  var keep={curve:E.curve,w:E.w,cheek:E.cheek,gape:E.gape};
  E.curve+=QF.curve;E.w=Math.max(4,E.w+QF.w);E.cheek=Math.max(E.cheek,QF.cheek);E.gape=Math.max(E.gape,QF.gape);
  var keepO=E.omega;E.omega*=1-clamp(QF.gape/0.4,0,1);
  drawEye(cx-54*sx,cy-14*sy,-1,sx,clamp(open*QF.openL,0,1.1),QF.lookY);
  drawEye(cx+54*sx,cy-14*sy,1,sx,clamp(open*QF.openR,0,1.1),QF.lookY);
  drawMouth(cx,cy+22*sy);
  for(var k in keep)E[k]=keep[k];E.omega=keepO;
  drawDecoFront(cx,cy,sx,sy);ctx.strokeStyle=fc;ctx.fillStyle=fc;ctx.lineWidth=SW;
  notes.forEach(function(n){ctx.globalAlpha=clamp(n.life/0.6,0,1);ctx.beginPath();ctx.ellipse(n.x,n.y,4,3,0,0,7);ctx.fill();
    ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(n.x+3,n.y);ctx.lineTo(n.x+3,n.y-13);ctx.lineTo(n.x+8,n.y-9);ctx.stroke();ctx.lineWidth=SW});ctx.globalAlpha=1;

  // extras, same flat color
  if(printer.hot&&mood!="sleepy"){var q=(now*0.5)%1,dx=cx+86*sx,dy=cy-40+q*26;ctx.globalAlpha=1-q*0.8;
    ctx.beginPath();ctx.moveTo(dx,dy-8);ctx.quadraticCurveTo(dx+6,dy+1,dx,dy+4);ctx.quadraticCurveTo(dx-6,dy+1,dx,dy-8);ctx.fill();ctx.globalAlpha=1}
  if(mood=="sleepy"){for(var i=0;i<3;i++){var z=(now*0.45+i/3)%1;ctx.globalAlpha=Math.sin(z*Math.PI)*clamp(moodT/1.5,0,1);ctx.font="700 "+(9+z*9)+"px Space Grotesk, sans-serif";ctx.fillText("z",160+z*26,92-z*36)}ctx.globalAlpha=1}
  confetti.forEach(function(p){ctx.save();ctx.translate(p.x,p.y);ctx.rotate(p.r);ctx.globalAlpha=Math.min(1,p.life);ctx.fillRect(-3,-1.5,6,3);ctx.restore()});
  ctx.globalAlpha=1;
}

/* ---------------- decorations (same rules as the firmware) ---------------- */
// Seasons are windows, not single days. Holidays go by date anywhere; the weather
// ones (snow, petals, sunglasses, leaves) flip with the hemisphere.
var DECO={mode:"auto",lights:"classic",anim:"twinkle",hemi:"n",bday:"09-28"}, decoTry=null;
function decoLoad(j){
  if(typeof j.talk=="number")$("dTalk").value=j.talk;if(typeof j.clock=="number")$("dClock").value=j.clock;
  if(j.deco)DECO.mode=j.deco;if(j.lights)DECO.lights=j.lights;if(j.anim)DECO.anim=j.anim;if(j.hemi)DECO.hemi=j.hemi;if(j.bday)DECO.bday=j.bday;
  decoUi();
}
function decoSettings(){return{deco:$("dMode").value,lights:$("dLights").value,anim:$("dAnim").value,hemi:$("dHemi").value,bday:($("dBday").value||"2026-09-28").slice(5),tz:-new Date().getTimezoneOffset(),talk:+$("dTalk").value,clock:+$("dClock").value}}
function decoUi(){$("dMode").value=DECO.mode;$("dLights").value=DECO.lights;$("dAnim").value=DECO.anim;$("dHemi").value=DECO.hemi;$("dBday").value="2026-"+DECO.bday}
function inWin(m,d,m0,d0,m1,d1){var v=m*100+d,a=m0*100+d0,b=m1*100+d1;return a<=b?(v>=a&&v<=b):(v>=a||v<=b)}
function weather(m,south){var s=["winter","winter","spring","spring","spring","summer","summer","summer","autumn","autumn","autumn","winter"][m-1];
  if(south)s={winter:"summer",summer:"winter",spring:"autumn",autumn:"spring"}[s];return s}
function decoFor(date){
  var m=date.getMonth()+1,d=date.getDate(),south=$("dHemi").value=="s",b=($("dBday").value||"2026-09-28").slice(5).split("-");
  if(m==+b[0]&&d==+b[1])return"birthday";
  if(inWin(m,d,12,31,1,1))return"newyear";
  if(inWin(m,d,12,1,12,30))return"holidays";
  if(inWin(m,d,10,20,10,31))return"halloween";
  if(inWin(m,d,2,10,2,14))return"valentine";
  if(inWin(m,d,7,1,7,5))return"july4";
  return weather(m,south);
}
var deco={kind:"",parts:[],spawn:0,fw:[],fwT:1,shades:0,shadesOn:false,shadesT:40,confT:6};
function decoKind(){
  var mode=$("dMode").value;
  if(mode=="off")return"";
  if(mode!="auto")return mode;
  var dt=$("dDate").value?new Date($("dDate").value+"T12:00"):new Date();
  return decoFor(dt);
}
var DNAME={holidays:"Holiday lights",newyear:"New Year",winter:"Snow",valentine:"Valentine's",spring:"Spring",summer:"Summer",july4:"4th of July",autumn:"Autumn leaves",halloween:"Halloween",birthday:"Birthday!","":"None"};
// southern summer holidays: lights but no snow
function snowy(k){return k=="winter"||(k=="holidays"&&!($("dMode").value=="auto"&&$("dHemi").value=="s"))}
function stepDeco(dt){
  var k=decoKind();
  if(k!==deco.kind){deco.kind=k;deco.parts=[];deco.fw=[];$("decoNow").textContent=DNAME[k]}
  // falling / floating things
  var want=snowy(k)?(k=="winter"?34:26):k=="spring"?14:k=="autumn"?12:k=="valentine"?10:0;
  deco.spawn-=dt;
  if(deco.parts.length<want&&deco.spawn<=0){
    deco.spawn=k=="valentine"?0.5:0.25;
    var p={x:rnd(10,230),y:-8,ph:rnd(0,6.28),rot:rnd(0,6.28),vr:rnd(-2,2),life:99};
    if(snowy(k)){p.kind="snow";p.r=rnd(1,2.4);p.vy=rnd(14,30)}
    else if(k=="spring"){p.kind="petal";p.vy=rnd(10,18);p.col=Math.random()<0.5?"#F8BBD0":"#F48FB1"}
    else if(k=="autumn"){p.kind="leaf";p.vy=rnd(16,26);p.col=["#E65100","#F9A825","#BF360C","#A1887F"][Math.floor(rnd(0,4))]}
    else if(k=="valentine"){p.kind="heart";p.y=250;p.vy=-rnd(10,18);p.col=Math.random()<0.5?"#E53935":"#F48FB1";p.r=rnd(3.5,6)}
    deco.parts.push(p);
  }
  deco.parts.forEach(function(p){p.ph+=dt;p.y+=p.vy*dt;p.x+=Math.sin(p.ph*(p.kind=="leaf"?2.2:1.3))*(p.kind=="snow"?8:16)*dt;p.rot+=p.vr*dt});
  deco.parts=deco.parts.filter(function(p){return p.y<250&&p.y>-20&&p.x>-20&&p.x<260});
  if(!want)deco.parts=[];
  // fireworks
  if(k=="newyear"||k=="july4"){
    deco.fwT-=dt;
    if(deco.fwT<=0){deco.fwT=rnd(0.9,2.2);
      var cols=k=="july4"?["#E53935","#F5F5F5","#42A5F5"]:["#FFD54F","#FFB300","#F5F5F5","#E53935","#4FC3F7"];
      deco.fw.push({x:rnd(50,190),y:250,ty:rnd(30,95),col:cols[Math.floor(rnd(0,cols.length))],sparks:null})}
    deco.fw.forEach(function(f){
      if(!f.sparks){f.y-=180*dt;if(f.y<=f.ty){f.sparks=[];var n=28;for(var i=0;i<n;i++){var a=i/n*6.283+rnd(-0.05,0.05),v=rnd(62,70);f.sparks.push({x:f.x,y:f.y,vx:Math.cos(a)*v,vy:Math.sin(a)*v})}f.life=1.1}}
      else{f.life-=dt;var dr=Math.exp(-dt*2.2);f.sparks.forEach(function(s){s.vx*=dr;s.vy=s.vy*dr+28*dt;s.x+=s.vx*dt;s.y+=s.vy*dt})}
    });
    deco.fw=deco.fw.filter(function(f){return !f.sparks||f.life>0});
  } else deco.fw=[];
  // summer: puts on sunglasses now and then, keeps them on a while
  if(k=="summer"||k=="july4"){
    deco.shadesT-=dt;
    if(deco.shadesT<=0&&(mood=="calm"||mood=="riding"||deco.shadesOn)){deco.shadesOn=!deco.shadesOn;deco.shadesT=deco.shadesOn?rnd(15,30):rnd(40,120)}
  } else if(!deco.forced) deco.shadesOn=false;
  deco.shades+=((deco.shadesOn?1:0)-deco.shades)*(1-Math.exp(-dt*5));
  // birthday: confetti every so often
  if(k=="birthday"){deco.confT-=dt;if(deco.confT<=0){deco.confT=rnd(10,20);spawnConfetti()}}
}
$("dShades").addEventListener("click",function(){deco.shadesOn=!deco.shadesOn;deco.forced=deco.shadesOn;deco.shadesT=deco.shadesOn?30:60});
["dDate","dHemi","dBday","dMode"].forEach(function(id){$(id).addEventListener("change",function(){deco.kind="?"})});
decoUi();

function heart(x,y,r){ctx.beginPath();ctx.arc(x-r*0.5,y,r*0.55,Math.PI,0);ctx.arc(x+r*0.5,y,r*0.55,Math.PI,0);ctx.lineTo(x,y+r*1.1);ctx.closePath();ctx.fill()}
// behind the face
function drawDecoBack(){
  deco.parts.forEach(function(p){
    ctx.save();ctx.translate(p.x,p.y);
    if(p.kind=="snow"){ctx.fillStyle="#E7EEF4";ctx.globalAlpha=0.85;ctx.beginPath();ctx.arc(0,0,p.r,0,7);ctx.fill()}
    else if(p.kind=="petal"){ctx.rotate(p.rot);ctx.fillStyle=p.col;ctx.beginPath();ctx.ellipse(0,0,3.6,2,0,0,7);ctx.fill()}
    else if(p.kind=="leaf"){ctx.rotate(p.rot);ctx.fillStyle=p.col;ctx.strokeStyle=p.col;ctx.beginPath();ctx.ellipse(0,0,5.5,2.8,0,0,7);ctx.fill();ctx.lineWidth=1.2;ctx.beginPath();ctx.moveTo(5,0);ctx.lineTo(8,0);ctx.stroke()}
    else if(p.kind=="heart"){ctx.fillStyle=p.col;ctx.globalAlpha=clamp((p.y-10)/60,0,0.9);heart(0,0,p.r)}
    ctx.restore();
  });
  deco.fw.forEach(function(f){
    ctx.fillStyle=f.col;ctx.strokeStyle=f.col;
    if(!f.sparks){ctx.beginPath();ctx.arc(f.x,f.y,1.8,0,7);ctx.fill();ctx.globalAlpha=0.4;ctx.fillRect(f.x-0.8,f.y+2,1.6,8);ctx.globalAlpha=1;return}
    var al=clamp(f.life/0.7,0,1);ctx.lineWidth=1.4;
    f.sparks.forEach(function(s){ctx.globalAlpha=al*0.45;ctx.beginPath();ctx.moveTo(s.x,s.y);ctx.lineTo(s.x-s.vx*0.12,s.y-s.vy*0.12);ctx.stroke();
      ctx.globalAlpha=al;ctx.beginPath();ctx.arc(s.x,s.y,1.9,0,7);ctx.fill()});
    ctx.globalAlpha=1;
  });
}
// the string of lights along the top of the screen
function bulbColor(i,n){
  var pal={classic:["#E53935","#43A047","#1E88E5","#FDD835","#FB8C00"],warm:["#FFD27A"],theme:[faceColor],candy:["#E53935","#F5F5F5"]}[DECO.lights];
  if(DECO.lights=="rainbow")return"hsl("+Math.round((i*40+now*40)%360)+",85%,60%)";
  return pal[i%pal.length];
}
function bulbLevel(i,n){
  if(DECO.anim=="steady")return 1;
  if(DECO.anim=="chase")return 0.25+0.75*Math.pow(Math.max(0,Math.cos((i/n)*6.283*2-now*4)),2);
  if(DECO.anim=="breathe")return 0.35+0.65*(0.5+0.5*Math.sin(now*1.6+(i%2)*Math.PI));
  var h=Math.sin(i*91.7+Math.floor(now*3+i*0.37)*13.1)*43758.5;h-=Math.floor(h);return h<0.22?0.25:1;   // twinkle
}
function drawLights(){
  var hooks=[],N=6;
  for(var i=0;i<=N;i++){var a=Math.PI*(1.16+0.68*i/N);hooks.push([120+Math.cos(a)*116,122+Math.sin(a)*116])}
  var bulbs=[];
  ctx.strokeStyle="#2E3B2F";ctx.lineWidth=1.6;ctx.beginPath();
  for(var j=0;j<N;j++){
    var p0=hooks[j],p1=hooks[j+1],mx=(p0[0]+p1[0])/2,my=(p0[1]+p1[1])/2,dx=120-mx,dy=122-my,dl=Math.hypot(dx,dy);
    var c=[mx+dx/dl*11,my+dy/dl*11];
    if(j==0)ctx.moveTo(p0[0],p0[1]);ctx.quadraticCurveTo(c[0],c[1],p1[0],p1[1]);
    [0.3,0.7].forEach(function(t){var u=1-t;bulbs.push([u*u*p0[0]+2*u*t*c[0]+t*t*p1[0],u*u*p0[1]+2*u*t*c[1]+t*t*p1[1],Math.atan2(dy,dx)])});
  }
  ctx.stroke();
  bulbs.forEach(function(b,i){
    var lv=bulbLevel(i,bulbs.length),col=bulbColor(i,bulbs.length);
    ctx.save();ctx.translate(b[0],b[1]);ctx.rotate(b[2]-Math.PI/2);
    ctx.fillStyle="#2E3B2F";ctx.fillRect(-1.8,-1,3.6,3);
    ctx.fillStyle=col;
    ctx.globalAlpha=0.22*lv;ctx.beginPath();ctx.arc(0,7,8.5,0,7);ctx.fill();
    ctx.globalAlpha=0.35+0.65*lv;ctx.beginPath();ctx.ellipse(0,6.5,3.4,4.8,0,0,7);ctx.fill();
    ctx.restore();
  });
  ctx.globalAlpha=1;
}
function drawBats(){
  for(var i=0;i<3;i++){
    var t=(now*0.09+i/3)%1,x=-20+t*280,y=46+i*16+Math.sin(now*1.7+i*2)*10,f=Math.sin(now*14+i*3);
    ctx.fillStyle=faceColor;ctx.strokeStyle=faceColor;ctx.lineWidth=1.8;
    ctx.beginPath();ctx.arc(x,y,2.4,0,7);ctx.fill();
    ctx.beginPath();ctx.moveTo(x-2,y);ctx.lineTo(x-6,y-3-f*4);ctx.lineTo(x-11,y-f*2);ctx.moveTo(x+2,y);ctx.lineTo(x+6,y-3-f*4);ctx.lineTo(x+11,y-f*2);ctx.stroke();
  }
}
function drawHat(kind,cx,top){
  ctx.fillStyle=faceColor;
  if(kind=="party"){ctx.beginPath();ctx.moveTo(cx-22,top);ctx.lineTo(cx+22,top);ctx.lineTo(cx+6,top-46);ctx.fill();ctx.strokeStyle="#000";ctx.lineWidth=3;
    ctx.beginPath();ctx.moveTo(cx-14,top-14);ctx.lineTo(cx+15,top-14);ctx.moveTo(cx-6,top-30);ctx.lineTo(cx+11,top-30);ctx.stroke();ctx.beginPath();ctx.arc(cx+6,top-48,5,0,7);ctx.fill()}
  else if(kind=="santa"){ctx.beginPath();ctx.moveTo(cx-30,top);ctx.lineTo(cx+30,top);ctx.lineTo(cx+44,top-34);ctx.fill();ctx.fillStyle="#E7EEF4";ctx.fillRect(cx-34,top-2,68,10);ctx.beginPath();ctx.arc(cx+46,top-34,7,0,7);ctx.fill()}
  else if(kind=="witch"){ctx.beginPath();ctx.moveTo(cx-16,top-2);ctx.lineTo(cx+16,top-2);ctx.lineTo(cx+12,top-58);ctx.fill();ctx.beginPath();ctx.ellipse(cx,top,42,6,0,0,7);ctx.fill();
    ctx.strokeStyle="#000";ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(cx-15,top-10);ctx.lineTo(cx+15,top-10);ctx.stroke()}
}
function drawFlower(x,y){
  ctx.fillStyle="#F48FB1";for(var i=0;i<5;i++){var a=i/5*6.283+0.3;ctx.beginPath();ctx.arc(x+Math.cos(a)*4.6,y+Math.sin(a)*4.6,3.4,0,7);ctx.fill()}
  ctx.fillStyle="#FDD835";ctx.beginPath();ctx.arc(x,y,2.8,0,7);ctx.fill();
}
function drawShades(cx,ey,sx){
  var k=deco.shades;if(k<0.02)return;
  var drop=(1-k)*-40;ctx.globalAlpha=Math.min(1,k*1.5);
  ctx.fillStyle=faceColor;ctx.strokeStyle=faceColor;ctx.lineWidth=3;
  [-1,1].forEach(function(s){var x=cx+s*54*sx;ctx.beginPath();ctx.moveTo(x-31,ey-11+drop);ctx.lineTo(x+31,ey-11+drop);ctx.quadraticCurveTo(x+30,ey+15+drop,x,ey+15+drop);ctx.quadraticCurveTo(x-30,ey+15+drop,x-31,ey-11+drop);ctx.fill()});
  ctx.beginPath();ctx.moveTo(cx-24*sx,ey-8+drop);ctx.quadraticCurveTo(cx,ey-14+drop,cx+24*sx,ey-8+drop);ctx.stroke();
  ctx.strokeStyle="#000";ctx.lineWidth=2;ctx.globalAlpha=Math.min(1,k*1.5)*0.7;
  [-1,1].forEach(function(s){var x=cx+s*54*sx;ctx.beginPath();ctx.moveTo(x-14,ey-5+drop);ctx.lineTo(x-6,ey-5+drop);ctx.stroke()});
  ctx.globalAlpha=1;
}
// in front of the face
function drawDecoFront(cx,cy,sx,sy){
  var k=deco.kind;
  if(k=="holidays")drawHat("santa",cx,cy-50*sy);
  if(k=="newyear"||k=="birthday")drawHat("party",cx,cy-50*sy);
  if(k=="halloween"){drawHat("witch",cx,cy-50*sy);drawBats()}
  if(k=="spring")drawFlower(cx-38*sx,cy-50*sy);
  drawShades(cx,cy-14*sy,sx);
  if(k=="holidays")drawLights();
}

/* ---------------- scope ---------------- */
var sc=$("scope"),sctx=sc.getContext("2d"),hist=[],HIST=240; // 4 s at 60 fps
function drawScope(){
  var w=sc.clientWidth,h=150,dpr=Math.min(2,window.devicePixelRatio||1);
  if(sc.width!==Math.round(w*dpr)){sc.width=Math.round(w*dpr);sc.height=Math.round(h*dpr)}
  sctx.setTransform(dpr,0,0,dpr,0,0);sctx.clearRect(0,0,w,h);
  var mid=h/2,sy=(h/2-10)/2; // ±2 g
  sctx.strokeStyle="#26333E";sctx.lineWidth=1;sctx.font="10px IBM Plex Mono, monospace";sctx.fillStyle="#7E8F9F";
  [-2,-1,0,1,2].forEach(function(g){var y=mid-g*sy;sctx.beginPath();sctx.moveTo(34,y+0.5);sctx.lineTo(w,y+0.5);sctx.stroke();sctx.fillText((g>0?"+":"")+g+" g",0,y+3)});
  var n=hist.length,x0=34,dx=(w-x0)/(HIST-1);
  [["#E8A33D",0],["#4FD1C5",1],["#9AA7F0",2]].forEach(function(c){sctx.strokeStyle=c[0];sctx.lineWidth=1.5;sctx.beginPath();
    for(var i=0;i<n;i++){var y=mid-clamp(hist[i][c[1]],-2.2,2.2)*sy;var x=x0+(HIST-n+i)*dx;if(i==0)sctx.moveTo(x,y);else sctx.lineTo(x,y)}sctx.stroke()});
  sctx.strokeStyle="rgba(231,238,244,.5)";sctx.setLineDash([4,4]);sctx.beginPath();
  for(var i=0;i<n;i++){var y=mid-hist[i][3]*sy,x=x0+(HIST-n+i)*dx;if(i==0)sctx.moveTo(x,y);else sctx.lineTo(x,y)}sctx.stroke();sctx.setLineDash([]);
}

/* ---------------- loop ---------------- */
var last=performance.now(),acc=0,uiT=0;
function frame(t){
  var el=Math.min(0.1,(t-last)/1000);last=t;acc+=el;
  var a=[0,0,0];
  while(acc>=DT){
    acc-=DT;now+=DT;
    stepDevice(DT);
    var s=scenarioAccel(DT);
    a=[s[0]+devAcc[0],s[1]+devAcc[1],s[2]+devAcc[2]];
    impulses.forEach(function(p){if(p.t>0){a[0]+=p.v[0];a[1]+=p.v[1];a[2]+=p.v[2];p.t-=DT}});impulses=impulses.filter(function(p){return p.t>0});
    // a little printer noise so it never looks dead-still while printing
    if(scenKey!="idle"&&scenKey!="hand"&&!csv){a[0]+=rnd(-0.015,0.015);a[1]+=rnd(-0.015,0.015)}
    sense(a,DT);pickMood(DT);stepBody(S.lp,DT);stepExpr(DT);stepQuirks(DT);stepNotes(DT);stepDeco(DT);stepParticles(DT);
  }
  device.style.transform="translate("+dev.x.toFixed(1)+"px,"+dev.y.toFixed(1)+"px) scale("+(1+dev.d/400).toFixed(3)+")";
  draw();
  hist.push([S.lp[0],S.lp[1],S.lp[2],S.base]);if(hist.length>HIST)hist.shift();drawScope();
  uiT-=el;if(uiT<=0){uiT=0.1;updateUi()}
  requestAnimationFrame(frame);
}
var MOODCOL={calm:"#93A4B2",riding:"#4FD1C5",excited:"#E8A33D",screaming:"#E06C5A",startled:"#F0C079",dizzy:"#9AA7F0",shivering:"#4FD1C5",elevator:"#9AA7F0",sleepy:"#7E8F9F",bored:"#7E8F9F",giggle:"#F0C079",celebrate:"#E8A33D",whee:"#E8A33D",mad:"#E06C5A"};
function updateUi(){
  var chip=$("moodChip");if(chip.textContent!==mood){chip.textContent=mood;chip.style.color=MOODCOL[mood]||"#E8A33D"}
  $("why").textContent=why;
  var th=Math.min(1,S.thrill),bz=Math.min(1,S.vib/0.4),dz=Math.min(1,timers.dizzy>0?1:S.dizzyMeter/T.dizzy);
  $("mThrill").style.width=(th*100)+"%";$("vThrill").textContent=Math.round(S.thrill*100)+"%";
  $("mBuzz").style.width=(bz*100)+"%";$("vBuzz").textContent=Math.round(bz*100)+"%";
  $("mDizzy").style.width=(dz*100)+"%";$("vDizzy").textContent=Math.round(dz*100)+"%";
  $("scenTime").textContent=csv?"":(scenKey=="hand"?"":(sweepT>=0?"sweeping "+(sweepT<sweepDur/2?"X":"Y"):""));
}
requestAnimationFrame(frame);
})();
</script>
</body></html>
)rawliteral";
#endif
