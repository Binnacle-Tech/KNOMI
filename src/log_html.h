#ifndef LOG_HTML_H
#define LOG_HTML_H
// Log page (/log). Served as-is (no template processor).
#include "binnacle_css.h"
const char log_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><title>KNOMI · Log</title>
)rawliteral" BINNACLE_HEAD R"rawliteral(
<style>
.info{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px 18px}
.info div{display:flex;flex-direction:column;gap:2px;min-width:0}
.info span{font-family:var(--font-mono);font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:var(--muted-2)}
.info b{font-family:var(--font-mono);font-weight:500;font-size:13px;overflow-wrap:anywhere}
#log{margin:0;background:var(--well);border:1px solid var(--line);border-radius:var(--r-ctrl);padding:12px;height:60vh;min-height:320px;
  overflow:auto;font-family:var(--font-mono);font-size:12px;line-height:1.5;white-space:pre-wrap;overflow-wrap:anywhere;color:var(--text)}
.bar{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.bar .sp{flex:1}
.bar input[type=search]{width:220px;background:var(--panel-2);color:var(--text);border:1px solid var(--line-2);border-radius:var(--r-ctrl);padding:8px 10px;font:inherit}
#copied{font-family:var(--font-mono);font-size:11.5px;color:var(--cyan)}
</style></head><body>
)rawliteral" BINNACLE_RAIL R"rawliteral(
<main class="wrap wide">
<section class="mast">
  <span class="label">Log</span>
  <h1>Log<span class="dot">.</span></h1>
  <p class="lede">What the KNOMI has been printing to its serial port since it started (the last 32 KB). If something acts up, download this and send it along with the problem. Passwords and API keys are never logged.</p>
  <div class="rule"></div>
</section>
<section class="card">
  <div class="card-h"><span class="k">This KNOMI</span></div>
  <div class="card-b"><div class="info" id="info"><div><span>Loading</span><b>…</b></div></div></div>
</section>
<section class="card">
  <div class="card-h"><span class="k">Output</span><span class="sp"></span><span id="copied"></span></div>
  <div class="card-b">
    <div class="bar" style="margin-bottom:12px">
      <button type="button" class="btn-ghost" id="pause">Pause</button>
      <input type="search" id="filter" placeholder="Filter lines" aria-label="Filter lines">
      <span class="sp"></span>
      <button type="button" class="btn-ghost" id="copy">Copy</button>
      <a class="btn-ghost" href="log.txt?dl=1">Download</a>
      <button type="button" class="btn-ghost" id="clear">Clear</button>
    </div>
    <pre id="log">Loading…</pre>
  </div>
</section>
</main>
<script>
(function(){
var $=function(i){return document.getElementById(i)},paused=false,text="";
function fmtUp(s){var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+"d ":"")+h+"h "+m+"m"}
function kb(b){return Math.round(b/1024)+" KB"}
function info(){fetch("log/info").then(function(r){return r.json()}).then(function(j){
  var rows=[["Firmware",j.fw],["Board",j.board],["Uptime",fmtUp(j.uptime)],["Last restart",j.reset],["Free RAM",kb(j.heap)+" (low "+kb(j.heap_min)+")"],
    ["Free PSRAM",kb(j.psram)],["WiFi",j.wifi],["Printer",j.backend+" "+j.host],["Coaster",j.mood],["Flash used",kb(j.fs_used)+" of "+kb(j.fs_total)]];
  $("info").innerHTML=rows.map(function(r){return'<div><span>'+r[0]+'</span><b>'+String(r[1]).replace(/[&<>]/g,"")+'</b></div>'}).join("");
}).catch(function(){})}
function show(){var f=$("filter").value.toLowerCase(),t=f?text.split("\n").filter(function(l){return l.toLowerCase().indexOf(f)>=0}).join("\n"):text;
  var el=$("log"),atEnd=el.scrollTop+el.clientHeight>=el.scrollHeight-20;el.textContent=t||"(nothing yet)";if(atEnd)el.scrollTop=el.scrollHeight}
function poll(){if(!paused)fetch("log.txt").then(function(r){return r.text()}).then(function(t){text=t;show()}).catch(function(){});setTimeout(poll,2000)}
$("pause").addEventListener("click",function(){paused=!paused;this.textContent=paused?"Resume":"Pause"});
$("filter").addEventListener("input",show);
$("clear").addEventListener("click",function(){fetch("log/clear",{method:"POST"}).then(function(){text="";show()})});
$("copy").addEventListener("click",function(){var t=$("log").textContent;
  try{navigator.clipboard.writeText(t).then(function(){$("copied").textContent="copied"},function(){sel()})}catch(e){sel()}
  function sel(){var r=document.createRange();r.selectNodeContents($("log"));var s=getSelection();s.removeAllRanges();s.addRange(r);$("copied").textContent="selected, press Ctrl+C"}
  setTimeout(function(){$("copied").textContent=""},2500)});
info();setInterval(info,5000);poll();
})();
</script></body></html>)rawliteral";
#endif
