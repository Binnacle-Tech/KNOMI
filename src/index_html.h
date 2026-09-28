#include "binnacle_css.h"

// Settings page. $name$ placeholders are filled by knomi_html_processor() in webserver.cpp,
// so no literal dollar signs anywhere in here.
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head>
<title>KNOMI · Settings</title>
<link rel='shortcut icon' type='image/x-icon' href='/favicon.ico'>
)rawliteral" BINNACLE_HEAD R"rawliteral(
<script>
var popup_clicked = false, popup_btn = false;
function esc(s){ return String(s).replace(/[&<>"']/g, function(c){ return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]; }); }
async function waitPopupBtn(){
  await new Promise(function(resolve){
    var timer = setInterval(function(){ if (popup_clicked) { clearInterval(timer); resolve(true); } }, 100);
  });
  document.getElementById("popup_id").style.display = "none";
  return true;
}
async function confirmSubmit(title, rows, formId, note){
  popup_clicked = false; popup_btn = false;
  document.getElementById("popup_title_id").textContent = title;
  var html = "";
  if (rows.length) {
    html = "<dl class='kv'>";
    rows.forEach(function(r){ html += "<dt>" + esc(r[0]) + "</dt><dd>" + esc(r[1] === "" ? "(empty)" : r[1]) + "</dd>"; });
    html += "</dl>";
  }
  if (note) html += "<p class='hint' style='margin-top:12px'>" + note + "</p>";
  document.getElementById("popup_content_id").innerHTML = html;
  document.getElementById("popup_id").style.display = "block";
  await waitPopupBtn();
  if (popup_btn) document.getElementById(formId).submit();
  return popup_btn;
}
function showPopupKlipper(){
  var be = document.getElementById("backend");
  var key = document.getElementById("api_key").value;
  return confirmSubmit("Save printer connection", [
    ["Backend", be.options[be.selectedIndex].text],
    ["Host", document.getElementById("ip").value],
    ["Port", document.getElementById("port").value],
    ["Tool", document.getElementById("tool").value],
    ["API key", key ? key.substring(0, 4) + "…" : "(none)"]
  ], "klipper-form");
}
function showPopupKnomi(){
  var m = document.getElementById("mode");
  return confirmSubmit("Save KNOMI network", [
    ["WiFi mode", m.options[m.selectedIndex].text],
    ["AP SSID", document.getElementById("ap-ssid").value],
    ["AP password", document.getElementById("ap-pwd").value],
    ["Hostname", document.getElementById("hostname").value]
  ], "knomi-form");
}
function showPopupRestart(){
  return confirmSubmit("Restart KNOMI", [], "restart-form",
    "KNOMI will drop its network connection and restart. Reconnect once it's back.");
}
function popupConfirm(){ popup_clicked = true; popup_btn = true; }
function popupCancel(){ popup_clicked = true; popup_btn = false; }
function syncBackend(){
  var octo = document.getElementById("backend").value == "octoprint";
  document.getElementById("octo-only").style.display = octo ? "" : "none";
}
function pickOcto(ip, port){
  document.getElementById("ip").value = ip;
  document.getElementById("port").value = port;
  document.getElementById("backend").value = "octoprint";
  syncBackend();
  document.getElementById("discover-status").innerHTML = "<span class='pill ok'>selected " + esc(ip) + ":" + esc(port) + "</span> Add your API key, then Save.";
  document.getElementById("api_key").focus();
}
async function discoverOcto(){
  var st = document.getElementById("discover-status");
  var list = document.getElementById("discover-list");
  var btn = document.getElementById("discover-btn");
  st.innerHTML = "<span class='pill now'>searching</span>";
  list.innerHTML = ""; btn.disabled = true;
  try {
    var r = await (await fetch("/discover?start=1")).json();
    for (var i = 0; i < 15 && r.scanning; i++) {
      await new Promise(function(res){ setTimeout(res, 1000); });
      r = await (await fetch("/discover")).json();
    }
    if (!r.results.length) {
      st.innerHTML = "<span class='pill held'>none found</span> Is OctoPrint's discovery plugin on?";
    } else {
      st.innerHTML = "<span class='pill ok'>" + r.results.length + " found</span>";
      r.results.forEach(function(o){
        var b = document.createElement("button");
        b.type = "button"; b.className = "btn-ghost";
        b.innerHTML = "<span>" + esc(o.name) + "</span><span class='ip'>" + esc(o.ip) + ":" + esc(o.port) + "</span>";
        b.onclick = function(){ pickOcto(o.ip, o.port); };
        list.appendChild(b);
      });
    }
  } catch (e) { st.innerHTML = "<span class='pill bad'>search failed</span>"; }
  btn.disabled = false;
}
</script>
</head>
<body>
<header class="rail"><div class="wrap rail-in">
  <div class="brand"><a class="n" href="/">KNOMI<span class="dot">.</span></a><span class="f">Printer display</span></div>
  <span class="rail-sp"></span>
  <nav><a class="on" href="/">Settings</a><a href="/gifs">Animations</a><a href="/update">Firmware</a></nav>
  )rawliteral" BINNACLE_MODES R"rawliteral(
</div></header>

<main class="wrap">
  <section class="mast">
    <span class="label">Settings</span>
    <h1>KNOMI<span class="dot">.</span></h1>
    <p class="lede">Where the display gets its printer status, how the screen behaves, how it joins your network, and system controls.</p>
    <div class="strip">$backend_pill$ <span class="pill">$fw$</span> <span class="pill">$sta_ip$</span></div>
    <div class="rule"></div>
  </section>

  <section class="card">
    <div class="card-h"><span class="idx">01</span><span class="k">Printer connection</span></div>
    <div class="card-b">
      <form id="klipper-form" name="klipper-form" action="/" method="POST">
        <div class="row">
          <label class="field-label" for="backend">Backend</label>
          <select id="backend" name="backend" onchange="syncBackend()">
            <option value="moonraker" $be_moonraker$>Moonraker (Klipper)</option>
            <option value="octoprint" $be_octoprint$>OctoPrint</option>
          </select>
        </div>
        <div class="cols">
          <div class="row">
            <label class="field-label" for="ip">Host</label>
            <input type="text" class="mono" id="ip" name="ip" $ip$ maxlength="64" placeholder="192.168.1.20 or octopi.local">
          </div>
          <div class="row">
            <label class="field-label" for="port">Port</label>
            <input type="text" class="mono" id="port" name="port" $port$ maxlength="5" placeholder="80">
          </div>
        </div>
        <div class="row">
          <button type="button" class="btn-ghost" id="discover-btn" onclick="discoverOcto()">Find OctoPrint on network</button>
          <span id="discover-status" class="hint" style="margin-left:8px"></span>
          <div id="discover-list" class="found"></div>
        </div>
        <div class="cols">
          <div class="row">
            <label class="field-label" for="tool">Tool</label>
            <input type="text" class="mono" id="tool" name="tool" $tool$ maxlength="6" placeholder="tool0">
          </div>
          <div class="row" id="octo-only">
            <label class="field-label" for="api_key">OctoPrint API key</label>
            <input type="password" class="mono" id="api_key" name="api_key" $api_key$ maxlength="64" placeholder="Application key" autocomplete="off">
            <div class="hint">OctoPrint &rsaquo; User Settings &rsaquo; Application Keys</div>
          </div>
        </div>
      </form>
    </div>
    <div class="card-f"><button type="button" class="btn-primary" onclick="showPopupKlipper()">Save connection</button></div>
  </section>

  <section class="card" id="display">
    <div class="card-h"><span class="idx">02</span><span class="k">Display</span></div>
    <form id="display-form" action="/display" method="POST">
    <div class="card-b">
      <div class="cols">
        <div class="row">
          <label class="field-label" for="bl">Brightness <span class="mono" id="bl-v">$bl$</span>/16</label>
          <input type="range" id="bl" name="bl" min="1" max="16" value="$bl$" oninput="document.getElementById('bl-v').textContent=this.value">
        </div>
        <div class="row">
          <label class="field-label" for="dim_lvl">Dimmed brightness <span class="mono" id="dl-v">$dim_lvl$</span>/16</label>
          <input type="range" id="dim_lvl" name="dim_lvl" min="1" max="16" value="$dim_lvl$" oninput="document.getElementById('dl-v').textContent=this.value">
        </div>
        <div class="row">
          <label class="field-label" for="dim_min">Dim after (minutes)</label>
          <input type="number" class="mono" id="dim_min" name="dim_min" min="0" max="1440" value="$dim_min$">
          <div class="hint">0 = never. Any touch or printer activity wakes it.</div>
        </div>
        <div class="row">
          <label class="field-label" for="sleep_min">Screen off after (minutes)</label>
          <input type="number" class="mono" id="sleep_min" name="sleep_min" min="0" max="1440" value="$sleep_min$">
          <div class="hint">0 = never. The tap that wakes it doesn't press anything.</div>
        </div>
        <div class="row">
          <label class="field-label" for="awake_print">While the printer is busy</label>
          <select id="awake_print" name="awake_print">
            <option value="1" $aw_1$>Stay awake</option>
            <option value="0" $aw_0$>Dim and sleep as usual</option>
          </select>
        </div>
        <div class="row">
          <label class="field-label" for="print_view">Printing screen</label>
          <select id="print_view" name="print_view">
            <option value="0" $pv_0$>Info: time left, temps, Z</option>
            <option value="1" $pv_1$>Accelerometer bars (stock)</option>
          </select>
        </div>
        <div class="row" style="grid-column:1/-1">
          <label class="field-label" for="gif_tint">Animations follow the UI color</label>
          <select id="gif_tint" name="gif_tint">
            <option value="0" $tint_0$>Off: stock colors</option>
            <option value="1" $tint_1$>Idle faces only</option>
            <option value="2" $tint_2$>All built-in animations</option>
          </select>
          <div class="hint">Uses the UI color picked on the KNOMI (Settings &rsaquo; UI color). Has no effect on the default red or on your uploaded GIFs.</div>
        </div>
      </div>
    </div>
    <div class="card-f"><button type="submit" class="btn-primary">Save display</button></div>
    </form>
  </section>

  <section class="card" id="bluetooth">
    <div class="card-h"><span class="idx">03</span><span class="k">Bluetooth</span><span class="sp"></span>$bt_state$</div>
    <form action="/bluetooth" method="POST">
    <div class="card-b">
      <div class="cols">
        <div class="row">
          <label class="field-label" for="bt_enabled">Bluetooth link to OctoPrint</label>
          <select id="bt_enabled" name="bt_enabled">
            <option value="0" $bt_off$>Off</option>
            <option value="1" $bt_on$>On: prefer Bluetooth, WiFi as fallback</option>
          </select>
          <div class="hint">Needs the OctoPrint-KNOMI plugin with Bluetooth enabled. Takes effect after a restart.</div>
        </div>
        <div class="row">
          <label class="field-label" for="bt_address">KNOMI Bluetooth address</label>
          <input type="text" class="mono" id="bt_address" value="$bt_addr$" readonly placeholder="Bluetooth is off">
          <div class="hint">Pair from the Pi: <span class="mono">bluetoothctl</span>, then <span class="mono">pair</span> this address and type the code the KNOMI shows.</div>
        </div>
        <div class="row">
          <label class="field-label" for="bt_wifi_off">WiFi while Bluetooth is connected</label>
          <select id="bt_wifi_off" name="bt_wifi_off" $wo_lock$>
            <option value="0" $wo_0$>Keep WiFi on</option>
            <option value="1" $wo_1$>Turn WiFi off</option>
          </select>
          <div class="hint">$wo_hint$</div>
        </div>
        <div class="row">
          <label class="field-label" for="bt_fallback">WiFi fallback (seconds)</label>
          <input type="number" class="mono" id="bt_fallback" name="bt_fallback" min="15" max="3600" value="$bt_fb$">
          <div class="hint">If Bluetooth isn't connected for this long, including after boot, WiFi turns back on.</div>
        </div>
      </div>
    </div>
    <div class="card-f">
      <button type="submit" class="btn-primary">Save Bluetooth</button>
      <button type="submit" class="btn-ghost btn-danger" formaction="/bluetooth/forget" onclick="return confirm('Forget all paired devices? The Pi will need to pair again.')">Forget paired devices</button>
    </div>
    </form>
  </section>

  <section class="card">
    <div class="card-h"><span class="idx">04</span><span class="k">WiFi networks</span><span class="sp"></span>
      <form name="refresh" action="/" method="POST" style="margin:0"><button type="submit" class="btn-ghost" name="refresh" value="1">Rescan</button></form>
    </div>
    <div class="card-b">
      <div class="table-wrap"><table>
        <thead><tr><th>Network</th><th>Signal</th><th>Status</th></tr></thead>
        <tbody>$wifi_list$</tbody>
      </table></div>
      <div class="hint">Pick a network to connect the KNOMI to it.</div>
    </div>
  </section>

  <section class="card">
    <div class="card-h"><span class="idx">05</span><span class="k">KNOMI network</span></div>
    <div class="card-b">
      <form id="knomi-form" name="knomi-form" action="/" method="POST">
        <div class="cols">
          <div class="row">
            <label class="field-label" for="mode">WiFi mode</label>
            <select id="mode" name="mode">
              <option value="ap" $ap$>Access point</option>
              <option value="sta" $sta$>Station (join network)</option>
              <option value="apsta" $apsta$>Access point + station</option>
            </select>
          </div>
          <div class="row">
            <label class="field-label" for="hostname">Hostname</label>
            <input type="text" class="mono" id="hostname" name="hostname" $hostname$ maxlength="15">
          </div>
          <div class="row">
            <label class="field-label" for="ap-ssid">AP name</label>
            <input type="text" id="ap-ssid" name="ap_ssid" $ap_ssid$ minlength="1" maxlength="32" required>
          </div>
          <div class="row">
            <label class="field-label" for="ap-pwd">AP password</label>
            <input type="text" id="ap-pwd" name="ap_password" $ap_password$ minlength="6" maxlength="64" placeholder="Open network if empty">
          </div>
        </div>
      </form>
    </div>
    <div class="card-f"><button type="button" class="btn-primary" onclick="showPopupKnomi()">Save network</button></div>
  </section>

  <section class="card">
    <div class="card-h"><span class="idx">06</span><span class="k">System</span></div>
    <div class="card-f" style="border-top:0">
      <a class="btn-ghost" href="/gifs">Custom animations</a>
      <a class="btn-ghost" href="/update">Update firmware</a>
      <form id="restart-form" name="restart-form" action="/" method="POST" style="margin:0"><input type="hidden" name="restart"></form>
      <button type="button" class="btn-ghost btn-danger" onclick="showPopupRestart()">Restart</button>
    </div>
  </section>
  <div class="foot">KNOMI firmware $fw$ · OctoPrint edition</div>
</main>

<div id="modalOne" class="modal">
  <div class="dialog">
    <div class="card-h"><span class="k">Join network</span><button type="button" class="x close" aria-label="Close">&times;</button></div>
    <form action="/" method="POST">
      <div class="card-b">
        <div class="row"><label class="field-label" for="ssid">Network</label><input readonly id="ssid" type="text" name="ssid" class="mono"></div>
        <div class="row" style="margin:0"><label class="field-label" for="wifi-pwd">Password</label><input id="wifi-pwd" type="password" name="password" autocomplete="off"></div>
      </div>
      <div class="card-f"><button type="submit" class="btn-primary">Connect</button></div>
    </form>
  </div>
</div>

<div id="popup_id" class="modal">
  <div class="dialog">
    <div class="card-h"><span id="popup_title_id" class="k"></span></div>
    <div id="popup_content_id" class="card-b"></div>
    <div class="card-f"><button class="btn-primary" onclick="popupConfirm()">Confirm</button><button class="btn-ghost" onclick="popupCancel()">Cancel</button></div>
  </div>
</div>

<script>
syncBackend();
document.querySelectorAll(".showpop").forEach(function(row){
  row.onclick = function(){
    document.getElementById("ssid").value = row.getElementsByClassName("ssid")[0].textContent;
    document.getElementById(row.getAttribute("data-modal")).style.display = "block";
    document.getElementById("wifi-pwd").focus();
  };
});
document.querySelectorAll(".close").forEach(function(btn){
  btn.onclick = function(){ btn.closest(".modal").style.display = "none"; };
});
window.onclick = function(e){ if (e.target.id === "modalOne") e.target.style.display = "none"; };
</script>
</body>
</html>)rawliteral";
