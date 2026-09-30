#ifndef UPDATE_HTML_H
#define UPDATE_HTML_H
// Firmware page (/update). Replaces AsyncElegantOTA's own page so it looks like the rest; uploads still go to
// its POST /update (multipart: MD5 field, then the file). $name$ placeholders are filled by knomi_html_processor(),
// so no literal dollar signs in here.
#include "binnacle_css.h"
const char update_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><title>KNOMI · Firmware</title>
)rawliteral" BINNACLE_HEAD R"rawliteral(
<style>
.act{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.status{font-family:var(--font-mono);font-size:12px;color:var(--muted);min-height:1.5em;margin-top:10px}
.status.ok{color:var(--cyan)}.status.bad{color:var(--danger)}
.card-b p{margin:0 0 12px;color:var(--muted)}
.two{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:16px;align-items:start}.two .card{margin:0}
.meter{display:none}.meter.on{display:block}
@media (max-width:640px){.two{grid-template-columns:1fr}}
</style></head><body>
)rawliteral" BINNACLE_RAIL R"rawliteral(
<main class="wrap wide">
  <section class="mast">
    <span class="label">Firmware</span>
    <h1>Firmware<span class="dot">.</span></h1>
    <p class="lede">Install a new version of the KNOMI's firmware over WiFi. Settings, Coaster's memories and custom animations stay.
    If a new version keeps crashing, the KNOMI goes back to the one before by itself.</p>
    <div class="strip"><span class="pill">$fw$</span><span class="pill">$board$</span><span class="pill" id="slot">slot …</span></div>
    <div class="rule"></div>
  </section>
  <div class="two">
  <section class="card">
    <div class="card-h"><span class="idx">01</span><span class="k">From GitHub</span></div>
    <div class="card-b">
      <p>Checks <a href="https://github.com/$repo$/releases" target="_blank" rel="noopener">$repo$</a> for a newer release and installs it.</p>
      <div class="act"><button type="button" class="btn-primary" id="gh-go" onclick="installUpdate()">Check and install</button><span class="pill" id="gh-latest" style="display:none"></span></div>
      <div class="meter"><i id="gh-bar" style="width:0"></i></div>
      <div class="status" id="gh-status"></div>
    </div>
  </section>
  <section class="card">
    <div class="card-h"><span class="idx">02</span><span class="k">Upload a file</span></div>
    <div class="card-b">
      <p>Pick the <code>$board$-octoprint-firmware.bin</code> from a release, or your own build.</p>
      <div class="act"><input type="file" id="bin" accept=".bin"><button type="button" class="btn-primary" id="up-go" onclick="upload()">Install</button></div>
      <div class="meter"><i id="up-bar" style="width:0"></i></div>
      <div class="status" id="up-status"></div>
    </div>
  </section>
  </div>
  <div class="foot">Don't unplug the KNOMI while it installs. It restarts by itself when it's done.</div>
</main>
<script>
function el(id){ return document.getElementById(id); }
function bar(id, pct){ var b = el(id); b.style.width = pct + "%"; b.parentNode.className = "meter on"; }
function say(id, text, cls){ var s = el(id); s.textContent = text; s.className = "status" + (cls ? " " + cls : ""); }
function fwParse(v){ var m = /v?(\d+)\.(\d+)\.(\d+)(?:-op(\d+))?/i.exec(v || ""); return m ? [+m[1], +m[2], +m[3], +(m[4] || 0)] : null; }
function fwNewer(a, b){ for (var i = 0; i < 4; i++){ if (a[i] !== b[i]) return a[i] > b[i]; } return false; }
function md5(buf){ // RFC 1321; the upload needs it and browsers don't have MD5 built in
  var K = [], S = [7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21], i;
  for (i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) | 0;
  var n = buf.length, len = ((n + 8) >>> 6 << 4) + 16, W = new Int32Array(len);
  for (i = 0; i < n; i++) W[i >> 2] |= buf[i] << (i % 4 * 8);
  W[n >> 2] |= 0x80 << (n % 4 * 8); W[len - 2] = n * 8; W[len - 1] = Math.floor(n / 536870912);
  var a0 = 1732584193, b0 = -271733879, c0 = -1732584194, d0 = 271733878;
  for (var o = 0; o < len; o += 16){
    var a = a0, b = b0, c = c0, d = d0, f, g, t, s;
    for (i = 0; i < 64; i++){
      if (i < 16){ f = (b & c) | (~b & d); g = i; } else if (i < 32){ f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
      else if (i < 48){ f = b ^ c ^ d; g = (3 * i + 5) % 16; } else { f = c ^ (b | ~d); g = 7 * i % 16; }
      t = d; d = c; c = b; f = (a + f + K[i] + W[o + g]) | 0; s = S[(i >> 4) * 4 + i % 4];
      b = (b + ((f << s) | (f >>> (32 - s)))) | 0; a = t;
    }
    a0 = (a0 + a) | 0; b0 = (b0 + b) | 0; c0 = (c0 + c) | 0; d0 = (d0 + d) | 0;
  }
  var h = ""; [a0, b0, c0, d0].forEach(function(v){ for (var j = 0; j < 4; j++) h += ((v >>> (j * 8)) & 255).toString(16).padStart(2, "0"); });
  return h;
}
// after an install: wait for the KNOMI to come back and say what it runs now
function waitBack(sid){
  var tries = 0;
  setTimeout(function poll(){
    fetch("/log/info", {cache:"no-store"}).then(function(r){ return r.json(); }).then(function(i){
      if (i.uptime < 120){ say(sid, "Done. Now running " + i.fw + ".", "ok"); setTimeout(function(){ location.reload(); }, 3000); }
      else if (++tries < 60) setTimeout(poll, 2000);
    }).catch(function(){ if (++tries < 60) setTimeout(poll, 2000); else say(sid, "It hasn't come back after 2 minutes. Check the screen.", "bad"); });
  }, 6000);
}
function installUpdate(){
  var b = el("gh-go"); b.disabled = true;
  say("gh-status", "Checking…");
  fetch("/update/github", {method:"POST"}).then(poll).catch(function(){ say("gh-status", "Couldn't start the update.", "bad"); b.disabled = false; });
  function poll(){
    fetch("/update/progress", {cache:"no-store"}).then(function(r){ return r.json(); }).then(function(s){
      if (s.state == "downloading" || s.state == "done") bar("gh-bar", s.state == "done" ? 100 : s.pct);
      if (s.state == "checking" || s.state == "downloading"){ say("gh-status", s.msg + (s.state == "downloading" ? " · " + s.pct + "%" : "")); setTimeout(poll, 800); return; }
      if (s.state == "done"){ say("gh-status", s.msg + " Restarting…", "ok"); waitBack("gh-status"); return; }
      say("gh-status", s.msg, s.state == "error" ? "bad" : ""); b.disabled = false;
    }).catch(function(){ say("gh-status", "Restarting…", "ok"); waitBack("gh-status"); });
  }
}
function upload(){
  var f = el("bin").files[0], b = el("up-go");
  if (!f){ say("up-status", "Pick a .bin file first.", "bad"); return; }
  var board = "$board$".toLowerCase(), name = f.name.toLowerCase();
  if (/knomiv\d/.test(name) && name.indexOf(board) < 0 &&
      !confirm("This file looks like it's for a different KNOMI (this one is " + board + "). Install anyway?")) return;
  b.disabled = true; say("up-status", "Reading the file…");
  f.arrayBuffer().then(function(ab){
    var bytes = new Uint8Array(ab);
    if (bytes[0] != 0xE9){ say("up-status", "That isn't ESP32 firmware (a .bin from a release or .pio/build).", "bad"); b.disabled = false; return; }
    var fd = new FormData();
    fd.append("MD5", md5(bytes));                   // must come before the file
    fd.append("firmware", new Blob([ab]), "firmware");
    var x = new XMLHttpRequest();
    x.upload.onprogress = function(e){ if (e.lengthComputable){ var p = Math.round(e.loaded * 100 / e.total); bar("up-bar", p); say("up-status", "Installing · " + p + "%"); } };
    x.onload = function(){
      if (x.status == 200 && /OK/i.test(x.responseText)){ bar("up-bar", 100); say("up-status", "Installed. Restarting…", "ok"); waitBack("up-status"); }
      else { say("up-status", "The KNOMI didn't take it: " + (x.responseText || x.status), "bad"); b.disabled = false; }
    };
    x.onerror = function(){ say("up-status", "The upload broke off. Try again.", "bad"); b.disabled = false; };
    x.open("POST", "/update"); x.send(fd);
  });
}
fetch("/log/info").then(function(r){ return r.json(); }).then(function(i){ if (i.slot) el("slot").textContent = "running from " + i.slot; }).catch(function(){ el("slot").style.display = "none"; });
// newest release, if this browser can reach GitHub (not over the KNOMI's own access point)
fetch("https://api.github.com/repos/$repo$/releases/latest").then(function(r){ return r.ok ? r.json() : null; }).then(function(rel){
  if (!rel) return;
  var l = fwParse(rel.tag_name), c = fwParse("$fw$"), p = el("gh-latest");
  if (!l || !c) return;
  p.style.display = ""; p.textContent = fwNewer(l, c) ? rel.tag_name + " available" : "up to date";
  p.className = "pill " + (fwNewer(l, c) ? "now" : "ok");
}).catch(function(){});
</script>
</body></html>)rawliteral";
#endif
