#ifndef BINNACLE_CSS_H
#define BINNACLE_CSS_H
// Binnacle house style (ejknotts.com), trimmed for the KNOMI web pages.
// Served at /binnacle.css. Fonts are optional: offline (AP mode) falls back to system fonts.
const char binnacle_css[] PROGMEM = R"rawliteral(
:root{
  --ink:#0E1419;--panel:#151E27;--panel-2:#1B2731;--well:#101922;
  --line:#26333E;--line-2:#334353;
  --amber:#E8A33D;--amber-soft:#F0C079;--cyan:#4FD1C5;--cyan-dim:#2E7E77;
  --neutral:#8FA0AE;--neutral-dim:#4A5A68;--danger:#E06C5A;
  --text:#E7EEF4;--muted:#93A4B2;--muted-2:#7E8F9F;
  --font-disp:"Space Grotesk",system-ui,sans-serif;
  --font-ui:"Inter",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --font-mono:"IBM Plex Mono",ui-monospace,Consolas,monospace;
  --t-body:14px;--t-small:12.5px;--t-label:11px;
  --r-card:10px;--r-ctrl:8px;--r-pill:20px;
  --ease:cubic-bezier(.2,.7,.3,1);--dur:.15s;
}
[data-theme="medium"]{--ink:#232F3A;--panel:#2C3A47;--panel-2:#354553;--well:#1C2833;--line:#3E4E5C;--line-2:#4E6070;
  --text:#EDF3F8;--muted:#B6C6D2;--muted-2:#9EB0BF;--amber-soft:#F3C68A;--cyan:#5AD8CC;--cyan-dim:#3E9A92;
  --neutral:#A6B6C3;--neutral-dim:#5E7080;--danger:#F2907E}
[data-theme="light"]{--ink:#EEF2F6;--panel:#FFFFFF;--panel-2:#F3F6F9;--well:#E9EEF3;--line:#D4DDE5;--line-2:#B9C6D1;
  --text:#0E1419;--muted:#4A5A68;--muted-2:#65788A;--amber:#8A5D10;--amber-soft:#6E4A0C;--cyan:#186059;--cyan-dim:#8FBFB9;
  --neutral:#5A6B79;--neutral-dim:#A8B6C1;--danger:#A32C1A}
[data-theme="contrast"]{--ink:#000;--panel:#0A0F14;--panel-2:#141C24;--well:#05080B;--line:#5A6E7E;--line-2:#8098AC;
  --text:#FFF;--muted:#DCE7EF;--muted-2:#BCCCD8;--amber:#FFC061;--amber-soft:#FFD79A;--cyan:#7BEFE3;--cyan-dim:#4FB3A8;
  --neutral:#B8C7D3;--neutral-dim:#7A8B99;--danger:#FF8E7A}

*{box-sizing:border-box}
html,body{margin:0}
body{background:var(--ink);color:var(--text);font-family:var(--font-ui);font-size:var(--t-body);
  line-height:1.6;-webkit-font-smoothing:antialiased}
a{color:var(--cyan)}
:focus-visible{outline:2px solid var(--amber);outline-offset:2px}
.wrap{max-width:860px;margin:0 auto;padding:0 20px}

/* top rail */
.rail{position:sticky;top:0;z-index:20;background:var(--ink);background:color-mix(in srgb,var(--ink) 88%,transparent);
  backdrop-filter:blur(8px);border-bottom:1px solid var(--line)}
.rail-in{display:flex;align-items:center;gap:16px;padding-top:12px;padding-bottom:12px;min-height:56px}
.brand{display:flex;align-items:baseline;gap:10px;flex:none}
.brand .n{font-family:var(--font-disp);font-weight:700;font-size:19px;letter-spacing:-.01em;color:var(--text);
  white-space:nowrap;text-decoration:none}
.brand .dot{color:var(--amber)}
.brand .n{display:inline-flex;align-items:center;gap:8px}
.brand .mark{width:24px;height:24px;flex:none}
.brand .f{font-size:var(--t-small);color:var(--muted-2);white-space:nowrap;border-left:1px solid var(--line-2);padding-left:10px}
.rail-sp{flex:1}
.rail nav{display:flex;gap:4px;min-width:0;overflow-x:auto;scrollbar-width:none}
.rail nav a{white-space:nowrap;font-size:var(--t-small);color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:var(--r-ctrl)}
.rail nav a:hover{color:var(--text);background:var(--panel-2)}
.rail nav a.on{color:var(--amber)}
.modes select{width:auto;padding:5px 8px;font-size:12px}

/* masthead */
.mast{padding:26px 0 20px}
.mast h1{font-family:var(--font-disp);font-weight:700;font-size:clamp(30px,6vw,46px);line-height:1.05;
  letter-spacing:-.022em;margin:10px 0 8px}
.mast h1 .dot{color:var(--amber)}
.mast .lede{color:var(--muted);font-size:15px;max-width:60ch;margin:0}
.rule{height:2px;width:132px;margin:18px 0 0;background:linear-gradient(90deg,var(--amber),transparent);border-radius:2px}
.strip{display:flex;flex-wrap:wrap;gap:8px;margin-top:14px}

/* cards */
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r-card);margin:0 0 16px}
.card-h{display:flex;align-items:center;gap:10px;padding:12px 16px;border-bottom:1px solid var(--line)}
.card-h .idx{font-family:var(--font-mono);font-size:11px;color:var(--amber);opacity:.85}
.card-h .k{font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--muted-2);font-weight:700}
.card-h .sp{flex:1}
.card-b{padding:16px}
.card-f{display:flex;flex-wrap:wrap;gap:10px;align-items:center;padding:12px 16px;border-top:1px solid var(--line)}

.label,.field-label{display:block;font-size:var(--t-label);letter-spacing:.16em;text-transform:uppercase;
  color:var(--muted-2);font-weight:600;margin-bottom:7px}
.hint{font-size:11.5px;color:var(--muted-2);margin-top:6px}
.row{margin-bottom:14px}
.cols{display:grid;grid-template-columns:1fr 1fr;gap:14px}

input[type=text],input[type=password],select,.field{width:100%;background:var(--panel-2);color:var(--text);
  border:1px solid var(--line-2);border-radius:var(--r-ctrl);padding:10px 13px;font-family:var(--font-ui);font-size:var(--t-body)}
input::placeholder{color:var(--muted-2)}
select:disabled,input:disabled{opacity:.5;cursor:not-allowed;background:var(--well)}
input[type=text]:hover,input[type=password]:hover,select:hover{border-color:var(--muted-2)}
.mono,input.mono{font-family:var(--font-mono);font-size:var(--t-small)}
input[type=number]{width:100%;background:var(--panel-2);color:var(--text);border:1px solid var(--line-2);
  border-radius:var(--r-ctrl);padding:10px 13px}
input[type=range]{width:100%;accent-color:var(--amber);margin:8px 0 4px}
.field-label .mono{color:var(--amber-soft);letter-spacing:0}
input[type=file]{color:var(--muted);font-size:var(--t-small);max-width:100%}
input[type=file]::file-selector-button{background:transparent;border:1px solid var(--line-2);color:var(--muted);
  padding:7px 12px;border-radius:var(--r-ctrl);margin-right:10px;cursor:pointer;font-family:var(--font-ui)}

.btn-ghost,.btn-primary{display:inline-flex;align-items:center;gap:8px;font-family:var(--font-ui);font-size:var(--t-small);
  border-radius:var(--r-ctrl);cursor:pointer;text-decoration:none;transition:var(--dur) var(--ease);line-height:1.2}
.btn-ghost{background:transparent;border:1px solid var(--line-2);color:var(--muted);padding:10px 17px}
.btn-ghost:hover{color:var(--text);border-color:var(--muted-2)}
.btn-primary{background:linear-gradient(180deg,var(--amber),#D48F2A);color:#1A1206;border:0;padding:10px 22px;
  font-weight:700;box-shadow:0 8px 22px -8px rgba(232,163,61,.55)}
[data-theme="light"] .btn-primary{background:linear-gradient(180deg,#8A5D10,#734B0B);color:#fff}
.btn-danger{color:var(--danger)}
.btn-danger:hover{color:var(--danger);border-color:var(--danger)}

.pill{display:inline-flex;align-items:center;gap:6px;font-family:var(--font-mono);font-size:11.5px;color:var(--muted);
  border:1px solid var(--line-2);border-radius:var(--r-pill);padding:3px 11px;background:var(--well);white-space:nowrap}
.pill.ok{color:var(--cyan);border-color:rgba(79,209,197,.28);background:rgba(79,209,197,.08)}
.pill.now{color:var(--amber);border-color:rgba(232,163,61,.28);background:rgba(232,163,61,.08)}
.pill.held{color:var(--neutral);border-color:var(--neutral-dim);background:rgba(143,160,174,.07)}
.pill.bad{color:var(--danger);border-color:rgba(224,108,90,.28);background:rgba(224,108,90,.08)}
.pill.ok::before{content:"\2713"}
.pill.now::before{content:"\25CF";font-size:8px}
.pill.held::before{content:"\2013"}
.pill.bad::before{content:"\2715"}
.well{background:var(--well);border:1px solid var(--line);border-radius:var(--r-ctrl);font-family:var(--font-mono);
  font-size:11.5px;color:var(--muted);padding:12px 14px}

/* tables */
.table-wrap{max-height:260px;overflow-y:auto;border:1px solid var(--line);border-radius:var(--r-ctrl)}
table{width:100%;border-collapse:collapse}
th{position:sticky;top:0;background:var(--panel-2);text-align:left;font-size:var(--t-label);letter-spacing:.16em;
  text-transform:uppercase;color:var(--muted-2);font-weight:600;padding:9px 12px;border-bottom:1px solid var(--line)}
td{padding:9px 12px;border-bottom:1px solid var(--line)}
tr:last-child td{border-bottom:0}
tbody tr{cursor:pointer;transition:background var(--dur) var(--ease)}
tbody tr:hover{background:var(--panel-2)}
td.num{font-family:var(--font-mono);font-size:var(--t-small);color:var(--muted)}

/* dialogs */
.modal{display:none;position:fixed;inset:0;z-index:50;background:rgba(5,8,11,.66);padding:20px;overflow:auto}
.dialog{max-width:420px;margin:12vh auto 0;background:var(--panel);border:1px solid var(--line-2);border-radius:var(--r-card);
  box-shadow:0 24px 60px -20px rgba(0,0,0,.6)}
.dialog .card-h{justify-content:space-between}
.dialog .x{background:none;border:0;color:var(--muted-2);font-size:20px;cursor:pointer;line-height:1}
.kv{display:grid;grid-template-columns:auto 1fr;gap:6px 14px;font-size:var(--t-small)}
.kv dt{color:var(--muted-2)}
.kv dd{margin:0;font-family:var(--font-mono);color:var(--amber-soft);word-break:break-all}

/* discovery results */
.found{display:flex;flex-direction:column;gap:6px;margin-top:10px}
.found button{justify-content:space-between;width:100%}
.found .ip{font-family:var(--font-mono);color:var(--cyan)}

/* animation slots */
.slots{display:grid;gap:16px;grid-template-columns:repeat(auto-fill,minmax(250px,1fr))}
.slots .card{margin:0;display:flex;flex-direction:column}
.slots .pill{white-space:normal;text-align:center}
.screen{width:150px;height:150px;margin:4px auto 12px;border-radius:50%;background:#000;border:6px solid var(--panel-2);
  box-shadow:0 0 0 1px var(--line-2);display:flex;align-items:center;justify-content:center;overflow:hidden}
.screen img{max-width:100%;max-height:100%}
.screen.empty{background:var(--well);color:var(--muted-2);font-size:var(--t-small)}
.screen .mark{width:72px;height:72px;display:block}
.slot-actions{display:flex;flex-direction:column;gap:10px;margin-top:auto}
.slot-actions form{display:flex;flex-direction:column;gap:8px;margin:0}
.meter{height:7px;background:var(--well);border-radius:5px;overflow:hidden;margin-top:8px}
.meter>i{display:block;height:100%;border-radius:5px;background:linear-gradient(90deg,var(--amber),var(--amber-soft))}

.checks{display:flex;flex-wrap:wrap;gap:8px 18px;margin-top:4px}
.checks label{display:inline-flex;align-items:center;gap:7px;font-size:var(--t-small);color:var(--text);cursor:pointer}
input[type=checkbox],input[type=radio]{accent-color:var(--amber);width:16px;height:16px;margin:0}
.color-row{display:flex;gap:10px;align-items:center}
input[type=color]{width:52px;height:36px;padding:2px;background:var(--panel-2);border:1px solid var(--line-2);border-radius:6px;cursor:pointer}
.preset{display:grid;grid-template-columns:minmax(0,1.4fr) minmax(0,1fr) minmax(0,1fr);gap:8px;margin-bottom:8px}
.preset.ex{grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto auto;align-items:center}
.preset-h{display:grid;grid-template-columns:minmax(0,1.4fr) minmax(0,1fr) minmax(0,1fr);gap:8px;margin-bottom:6px}
.preset-h.ex{grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto auto}
.preset-h span{font-family:var(--font-mono);font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted-2)}
.def{display:inline-flex;align-items:center;gap:5px;font-size:12px;color:var(--muted);white-space:nowrap;cursor:pointer}
.sub{font-family:var(--font-mono);font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted-2);margin:4px 0 10px}
.foot{color:var(--muted-2);font-size:var(--t-small);padding:10px 0 40px}
@media (max-width:640px){
  .cols{grid-template-columns:1fr}
  .brand .f,.modes{display:none}
  .rail-in{gap:8px}
  .rail nav{gap:0}
  .rail nav a{padding:6px 7px;font-size:12px}
  input[type=text],input[type=password],select{font-size:16px}
  .btn-primary,.btn-ghost{padding:12px 18px}
}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
)rawliteral";

// Shared <head> bits: fonts load without blocking (skipped silently offline) + saved color mode
#define BINNACLE_HEAD \
  "<meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>" \
  "<link rel='stylesheet' href='/binnacle.css'>" \
  "<link rel='stylesheet' media='print' onload=\"this.media='all'\" " \
  "href='https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@600;700&family=Inter:wght@400;500;600;700&family=IBM+Plex+Mono:wght@400;500&display=swap'>" \
  "<script>try{var m=localStorage.getItem('knomi-mode');if(m&&m!='dark')document.documentElement.setAttribute('data-theme',m)}catch(e){}" \
  "function setMode(m){if(m=='dark')document.documentElement.removeAttribute('data-theme');else document.documentElement.setAttribute('data-theme',m);" \
  "try{localStorage.setItem('knomi-mode',m)}catch(e){}}" \
  "addEventListener('DOMContentLoaded',function(){var s=document.getElementById('mode-sel');if(s)s.value=document.documentElement.getAttribute('data-theme')||'dark'})</script>"

// Coaster, the mascot, next to the KNOMI wordmark
#define KNOMI_MARK "<svg class='mark' viewBox='0 0 256 256' aria-hidden='true'><circle cx='128' cy='128' r='126' fill='#000' stroke='#334353' stroke-width='6'/><g stroke='#C02F30' stroke-width='16' stroke-linecap='round' fill='none'><path d='M32 112h80M144 112h80'/><path stroke-width='14' d='M100 176a14 14 0 0 0 28 0a14 14 0 0 0 28 0'/></g><g fill='#C02F30'><path d='M42 112a30 30 0 0 0 60 0z'/><path d='M154 112a30 30 0 0 0 60 0z'/></g></svg>"

#define BINNACLE_MODES \
  "<div class='modes'><select id='mode-sel' aria-label='Color mode' onchange='setMode(this.value)'>" \
  "<option value='dark'>Dark</option><option value='medium'>Medium</option>" \
  "<option value='light'>Light</option><option value='contrast'>Contrast</option></select></div>"

#endif
