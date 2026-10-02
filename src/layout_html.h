#ifndef LAYOUT_HTML_H
#define LAYOUT_HTML_H
// Print screen designer (/layout). Served as-is (no template processor), so '$' is fine here.
#include "binnacle_css.h"

const char layout_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><title>KNOMI · Print screen</title>
)rawliteral" BINNACLE_HEAD R"rawliteral(
<style>
.designer{display:grid;grid-template-columns:auto minmax(0,1fr);gap:22px;align-items:start}
.stage{position:sticky;top:76px;display:flex;flex-direction:column;align-items:center;gap:12px}
.bezel{padding:14px;border-radius:50%;background:radial-gradient(circle at 30% 25%,#2a3642,#0b1015 70%);box-shadow:0 10px 30px rgba(0,0,0,.45),inset 0 0 0 1px #3a4855}
.scr-wrap{position:relative;border-radius:50%;overflow:hidden;background:#000;touch-action:none}
#scr{position:absolute;left:0;top:0;width:240px;height:240px;transform-origin:0 0;font-family:"Inter",system-ui,sans-serif;font-weight:600;overflow:hidden}
#scr .el{position:absolute;white-space:pre;line-height:1.2;cursor:grab;user-select:none}
#scr .el.txt{transform:translate(-50%,-50%)}
#scr .el.gifel{transform:translate(-50%,-50%);display:block}
#scr .el.gifel img{display:block;pointer-events:none}
#scr .el.barel{transform:translate(-50%,-50%);overflow:hidden}
#scr .el.barel i{display:block;height:100%}
#scr svg.arcs{position:absolute;left:0;top:0;width:240px;height:240px;overflow:visible}
#scr svg.arcs path{cursor:grab}
#scr .sel{outline:1px dashed var(--amber);outline-offset:2px}
#scr svg path.sel{outline:none;filter:drop-shadow(0 0 2px #E8A33D)}
.guide{position:absolute;background:rgba(79,209,197,.55);pointer-events:none;display:none;z-index:5}
.guide.v{left:50%;top:0;bottom:0;width:1px}.guide.h{top:50%;left:0;right:0;height:1px}
.stage-bar{display:flex;gap:8px;flex-wrap:wrap;justify-content:center}
.pages{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.pages button{min-width:38px}
.pages button.on{border-color:var(--amber);color:var(--amber)}
.pages button.evt{border-style:dashed}
.pages button small{font-family:var(--font-mono);font-size:10px;color:var(--muted-2);margin-left:5px}
.trig[hidden]{display:none}
.trig{display:flex;flex-direction:column;gap:8px;border:1px solid var(--line);border-radius:var(--r-ctrl);padding:10px 12px;background:var(--well)}
.trig .t{display:flex;align-items:center;gap:8px;flex-wrap:wrap;font-size:13px}
.trig .t input[type=checkbox]{accent-color:var(--amber);width:16px;height:16px;margin:0}
.trig .t input[type=number]{width:74px;padding:5px 8px}
.trig .t input[type=text]{width:130px;padding:5px 8px}
.trig .grp{font-family:var(--font-mono);font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:var(--muted-2);margin-top:2px}
.playcap{font-family:var(--font-mono);font-size:11.5px;color:var(--cyan);text-align:center;min-height:18px}
.playbar{width:220px;height:5px;background:var(--well);border-radius:3px;overflow:hidden}
.playbar i{display:block;height:100%;width:0;background:var(--cyan)}
.adds{display:flex;flex-wrap:wrap;gap:6px}
.adds button{font-size:12px;padding:6px 10px}
.layers{list-style:none;margin:0;padding:0;border:1px solid var(--line);border-radius:var(--r-ctrl);max-height:230px;overflow:auto}
.layers li{display:flex;align-items:center;gap:8px;padding:6px 10px;border-bottom:1px solid var(--line);cursor:pointer;font-size:12.5px}
.layers li:last-child{border-bottom:0}
.layers li.on{background:var(--panel-2);color:var(--amber)}
.layers li .ty{font-family:var(--font-mono);font-size:10.5px;color:var(--muted-2);text-transform:uppercase;min-width:34px}
.layers li .nm{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.layers li button{padding:2px 7px;font-size:11px}
.props{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px 12px}
.props .full{grid-column:1/-1}.props .half{grid-column:span 2}
.props label.field-label{margin-bottom:4px}
.tokens{display:flex;flex-wrap:wrap;gap:5px;margin-top:6px}
.tokens button{font-family:var(--font-mono);font-size:11px;padding:3px 7px}
.inline{display:flex;gap:8px;align-items:center}
.inline input[type=color]{width:44px;height:34px;padding:2px;background:var(--panel-2);border:1px solid var(--line-2);border-radius:6px;flex:none}
.chk{display:inline-flex;align-items:center;gap:7px;font-size:12.5px;cursor:pointer;margin-top:26px}
.chk input{accent-color:var(--amber);width:16px;height:16px;margin:0}
.status{font-size:12px;color:var(--muted-2);min-height:18px;text-align:center}
.status.ok{color:var(--cyan)}.status.bad{color:var(--danger)}
.empty-props{color:var(--muted-2);font-size:12.5px}
@media (max-width:860px){
  .designer{grid-template-columns:1fr}
  .stage{position:static}
  .props{grid-template-columns:repeat(2,minmax(0,1fr))}
  .props .half{grid-column:1/-1}
}
</style></head><body>
)rawliteral" BINNACLE_RAIL R"rawliteral(
<main class="wrap wide">
<section class="mast">
  <span class="label">Print screen</span>
  <h1>Print screen<span class="dot">.</span></h1>
  <p class="lede">Design what the KNOMI shows while printing. Drag things around on the screen, and use pages to rotate between
  views (for example a face for 30 seconds, then your stats for 8). Tapping the KNOMI skips to the next page.</p>
  <div class="rule"></div>
</section>

<div class="designer">
  <div class="stage">
    <div class="bezel"><div class="scr-wrap" id="scrwrap"><div id="scr"></div><div class="guide v" id="gv"></div><div class="guide h" id="gh"></div></div></div>
    <div class="stage-bar">
      <button type="button" class="btn-primary" id="save">Save to KNOMI</button>
      <button type="button" class="btn-ghost" id="preview">Save &amp; preview on KNOMI</button>
    </div>
    <div class="status" id="status"></div>
    <div class="stage-bar">
      <label class="chk" style="margin:0"><input type="checkbox" id="live"> Live printer data</label>
      <button type="button" class="btn-ghost" id="play">Play a print</button>
    </div>
    <div class="playbar" id="playbar" hidden><i id="playfill"></i></div>
    <div class="playcap" id="playcap"></div>
  </div>

  <div>
    <section class="card">
      <div class="card-h"><span class="idx">01</span><span class="k">Pages</span></div>
      <div class="card-b">
        <div class="pages" id="pages"></div>
        <div class="cols" style="margin-top:14px">
          <div class="row"><label class="field-label" for="pmode">Show this page</label>
            <select id="pmode"><option value="r">In the rotation</option><option value="e">When something happens</option></select></div>
          <div class="row"><label class="field-label" for="pdur" id="pdurlbl">For (seconds)</label>
            <input type="number" class="mono" id="pdur" min="1" max="3600"></div>
        </div>
        <div class="trig" id="trig" hidden>
          <span class="grp">Pops up for the time above</span>
          <label class="t"><input type="checkbox" id="tr_pe"> Every <input type="number" class="mono" id="tr_pe_n" min="1" max="50" value="10"> %</label>
          <label class="t"><input type="checkbox" id="tr_pa"> At <input type="text" class="mono" id="tr_pa_n" value="25, 50, 75" aria-label="Percentages"> %</label>
          <label class="t"><input type="checkbox" id="tr_le"> Every <input type="number" class="mono" id="tr_le_n" min="1" max="250" value="1"> layer change(s)</label>
          <label class="t"><input type="checkbox" id="tr_st"> When the print starts</label>
          <span class="grp">Stays up while true</span>
          <label class="t"><input type="checkbox" id="tr_lm"> Less than <input type="number" class="mono" id="tr_lm_n" min="1" max="1440" value="10"> minutes left</label>
          <label class="t"><input type="checkbox" id="tr_fl"> During the first layer</label>
        </div>
        <div class="cols" style="margin-top:14px">
          <div class="row" style="display:flex;gap:8px;align-items:flex-end;flex-wrap:wrap">
            <button type="button" class="btn-ghost" id="pdup">Duplicate page</button>
            <button type="button" class="btn-ghost" id="pdel">Delete page</button>
          </div>
        </div>
        <div class="hint">Rotation pages take turns. “When something happens” pages stay out of the rotation and cut in when a trigger fires. Layer changes use the printer's layer number when it reports one, otherwise each new Z height. Up to 4 pages. The paused animation still takes over when the print pauses.</div>
      </div>
    </section>

    <section class="card">
      <div class="card-h"><span class="idx">02</span><span class="k">Add to this page</span></div>
      <div class="card-b">
        <div class="adds" id="adds"></div>
        <div class="row" style="margin:14px 0 0">
          <label class="field-label" for="starter">Start from a layout</label>
          <select id="starter">
            <option value="">Choose…</option>
            <option value="default">Info (the default)</option>
            <option value="face">Coaster, stats every 30 s</option>
            <option value="facepct">Coaster, stats every 10 % and at the end</option>
            <option value="big">Big percent</option>
            <option value="gauge">Gauge</option>
            <option value="blank">Blank</option>
          </select>
          <div class="hint">Replaces all pages. Nothing changes on the KNOMI until you save.</div>
        </div>
      </div>
    </section>

    <section class="card">
      <div class="card-h"><span class="idx">03</span><span class="k">Elements</span><span class="sp"></span><span class="pill" id="elcount"></span></div>
      <div class="card-b">
        <ul class="layers" id="layers"></ul>
        <div class="hint">Later in the list draws on top. Select one to edit it, or click it on the screen. Arrow keys nudge (Shift = 5 px), Delete removes.</div>
      </div>
    </section>

    <section class="card">
      <div class="card-h"><span class="idx">04</span><span class="k" id="ptitle">Properties</span></div>
      <div class="card-b"><div id="props"><div class="empty-props">Select an element.</div></div></div>
    </section>

    <section class="card">
      <div class="card-h"><span class="idx">05</span><span class="k">Layout file</span></div>
      <div class="card-b" style="display:flex;gap:8px;flex-wrap:wrap">
        <button type="button" class="btn-ghost" id="export">Download layout</button>
        <label class="btn-ghost" style="cursor:pointer">Load layout file<input type="file" id="import" accept=".json,application/json" hidden></label>
        <button type="button" class="btn-ghost" id="revert">Undo unsaved changes</button>
        <button type="button" class="btn-ghost" id="reset">Reset KNOMI to default</button>
      </div>
    </section>
  </div>
</div>
<p class="foot">The printing screen uses this layout when Settings › Display › Printing screen is set to “Your layout”.</p>
</main>
<script>
(function(){
var TOKENS=[["pct","percent done"],["time","“58m left, 9:40pm”"],["left","time left"],["elapsed","elapsed"],["total","total time"],
  ["file","file name"],["noz","nozzle °"],["noz_t","nozzle target"],["bed","bed °"],["bed_t","bed target"],["deg","℃"],
  ["z","Z height"],["layer","layer"],["layers","layer count"],["pos","Layer x/y or Z"],["state","Printing / Paused"],["msg","last M117 message"],["eta","finish time (9:40 pm)"]];
var SAMPLE={pct:"42",time:"58m left, 9:40pm",left:"58m",elapsed:"42m",total:"1h 40m",file:"benchy_0.2mm_PLA.gcode",noz:"215",noz_t:"215",
  bed:"60",bed_t:"60",deg:"℃",z:"8.40",layer:"42",layers:"240",pos:"Layer 42/240",state:"Printing",msg:"Heat soaking 5 min",eta:"9:40 pm"};
var FONTS=[14,16,18,20,24,32,48];
var NAMED={t:"UI color",x:"Text",m:"Muted",a:"Amber"};
var NAMEDHEX={x:"#E7EEF4",m:"#93A4B2",a:"#FFD164"};
var state={theme:"#C02F30",tokens:SAMPLE,gifs:[]};
var L=null,saved="",page=0,sel=-1,S=1.5,live=false,liveTimer=null,V=Date.now();
var $=function(id){return document.getElementById(id)};
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
function clone(o){return JSON.parse(JSON.stringify(o))}
function col(c){if(!c)return "transparent";if(c[0]=="#")return c;if(c=="t")return state.theme;return NAMEDHEX[c]||"#E7EEF4"}
function fontPx(f){for(var i=FONTS.length-1;i>=0;i--){if(f>=FONTS[i])return FONTS[i]}return 14}
function src(){return sim?sim.tokens:(live?state.tokens:SAMPLE)}
function expand(t){return String(t||"").replace(/\{([a-z_]{1,10})\}/g,function(m,k){var v=src()[k];return v===undefined?m:v})}
function status(msg,cls){var s=$("status");s.textContent=msg||"";s.className="status "+(cls||"")}
function dirty(){var d=JSON.stringify(L)!==saved;$("save").textContent=d?"Save to KNOMI •":"Save to KNOMI";return d}

/* ---------- starter layouts ---------- */
var DEF=null;
function ringDecor(){return[{t:"arc",x:120,y:120,d:240,w:2,s:0,e:360,c:"t",b:"",p:0}]}
function starters(k){
  var info=DEF?clone(DEF.pages[0]):{s:8,el:[]};
  if(k=="default")return clone(DEF);
  if(k=="blank")return{v:1,pages:[{s:10,el:[]}]};
  if(k=="face"){info.s=8;return{v:1,pages:[{s:30,el:[{t:"face",x:120,y:120,d:240},
    {t:"arc",x:120,y:120,d:236,w:4,s:0,e:360,c:"t",b:"#1B2731",p:1,rd:1},
    {t:"text",x:120,y:212,f:14,c:"m",w:0,a:"c",txt:"{pct}%  -  {left}"}]},info]}}
  if(k=="facepct"){var f=starters("face");f.pages[0].s=60;f.pages[1].m="e";f.pages[1].s=10;f.pages[1].tr={pe:10,lm:10,fl:1};return f}
  if(k=="big")return{v:1,pages:[{s:10,el:[{t:"arc",x:120,y:120,d:232,w:14,s:0,e:360,c:"t",b:"#1B2731",p:1,rd:1},
    {t:"text",x:120,y:76,f:14,c:"m",w:140,a:"c",sc:1,txt:"{file}"},
    {t:"text",x:120,y:118,f:48,c:"x",w:0,a:"c",txt:"{pct}%"},
    {t:"text",x:120,y:160,f:20,c:"t",w:0,a:"c",txt:"{left}"},
    {t:"text",x:120,y:184,f:14,c:"m",w:0,a:"c",txt:"left"}]}]};
  if(k=="gauge")return{v:1,pages:[{s:10,el:[{t:"arc",x:120,y:124,d:208,w:12,s:225,e:135,c:"t",b:"#1B2731",p:1,rd:1},
    {t:"text",x:120,y:112,f:32,c:"x",w:0,a:"c",txt:"{pct}%"},
    {t:"text",x:120,y:146,f:16,c:"m",w:0,a:"c",txt:"{time}"},
    {t:"text",x:78,y:204,f:14,c:"m",w:0,a:"c",txt:"{noz}{deg}"},
    {t:"text",x:162,y:204,f:14,c:"m",w:0,a:"c",txt:"{bed}{deg}"},
    {t:"text",x:120,y:62,f:14,c:"m",w:100,a:"c",sc:1,txt:"{file}"}]}]};
}
var ADDS=[
  ["Text",{t:"text",x:120,y:120,f:18,c:"x",w:0,a:"c",txt:"Text"}],
  ["Percent",{t:"text",x:120,y:110,f:48,c:"a",w:0,a:"c",txt:"{pct}%"}],
  ["Time left",{t:"text",x:120,y:150,f:18,c:"x",w:0,a:"c",txt:"{time}"}],
  ["Temps",{t:"text",x:120,y:176,f:14,c:"m",w:0,a:"c",txt:"{noz}/{noz_t}{deg}   {bed}/{bed_t}{deg}"}],
  ["File name",{t:"text",x:120,y:62,f:14,c:"m",w:150,a:"c",sc:1,txt:"{file}"}],
  ["Z / layer",{t:"text",x:120,y:196,f:14,c:"m",w:0,a:"c",txt:"{pos}"}],
  ["Progress ring",{t:"arc",x:120,y:120,d:212,w:12,s:0,e:360,c:"t",b:"#1B2731",p:1,rd:1}],
  ["Progress gauge",{t:"arc",x:120,y:120,d:200,w:10,s:225,e:135,c:"t",b:"#1B2731",p:1,rd:1}],
  ["Progress bar",{t:"bar",x:120,y:170,w:130,h:8,c:"t",b:"#1B2731",rd:1}],
  ["Ring",{t:"arc",x:120,y:120,d:236,w:2,s:0,e:360,c:"t",b:"",p:0}],
  ["Animation",{t:"gif",x:120,y:120,g:"print"}],
  ["Coaster face",{t:"face",x:120,y:120,d:240}]
];

/* ---------- render ---------- */
function pt(cx,cy,r,a){a=(a-90)*Math.PI/180;return[cx+r*Math.cos(a),cy+r*Math.sin(a)]}
function arcPath(cx,cy,r,s,sweep){
  if(sweep<=0)return"";
  if(sweep>=360)sweep=359.99;
  var a=pt(cx,cy,r,s),b=pt(cx,cy,r,s+sweep);
  return"M"+a[0].toFixed(2)+" "+a[1].toFixed(2)+" A"+r+" "+r+" 0 "+(sweep>180?1:0)+" 1 "+b[0].toFixed(2)+" "+b[1].toFixed(2);
}
function span(e){var sw=(e.e|0)-(e.s|0);if(sw<0)sw+=360;return sw}
function pct(){var v=parseInt(src().pct,10);return isNaN(v)?0:Math.max(0,Math.min(100,v))}
function render(){
  if(sim)return;
  var pg=L.pages[page];
  $("scr").innerHTML=layered(pg);
  renderList();
  $("elcount").textContent=pg.el.length+" / 24";
  dirty();
}
// one SVG per arc keeps the stacking order identical to the KNOMI's
function layered(pg){
  var h="";
  pg.el.forEach(function(e,i){
    var s=i==sel?" sel":"";
    if(e.t=="arc"){
      var rr=e.d/2-e.w/2,sw=span(e),v=e.p?sw*pct()/100:sw,cap=e.rd?"round":"butt";
      h+='<svg class="arcs" viewBox="0 0 240 240" style="pointer-events:none">';
      if(e.b)h+='<path class="'+s+'" d="'+arcPath(e.x,e.y,rr,e.s|0,sw)+'" stroke="'+col(e.b)+'" stroke-width="'+e.w+'" fill="none" stroke-linecap="'+cap+'"/>';
      h+='<path class="'+s+'" d="'+arcPath(e.x,e.y,rr,e.s|0,v)+'" stroke="'+col(e.c)+'" stroke-width="'+e.w+'" fill="none" stroke-linecap="'+cap+'"/>';
      h+='<path style="pointer-events:stroke" d="'+arcPath(e.x,e.y,rr,e.s|0,sw)+'" stroke="transparent" stroke-width="'+Math.max(e.w,10)+'" fill="none" data-i="'+i+'"/></svg>';
    }else if(e.t=="text"){
      var st="left:"+e.x+"px;top:"+e.y+"px;font-size:"+fontPx(e.f)+"px;color:"+col(e.c)+";text-align:"+({l:"left",r:"right"}[e.a]||"center")+";";
      if(e.w>0)st+="width:"+e.w+"px;overflow:hidden;text-overflow:"+(e.sc?"clip":"ellipsis")+";";
      h+='<div class="el txt'+s+'" data-i="'+i+'" style="'+st+'">'+(esc(expand(e.txt))||"&nbsp;")+'</div>';
    }else if(e.t=="bar"){
      var r=e.rd?e.h/2:0;
      h+='<div class="el barel'+s+'" data-i="'+i+'" style="left:'+e.x+'px;top:'+e.y+'px;width:'+e.w+'px;height:'+e.h+'px;border-radius:'+r+'px;background:'+col(e.b)+'"><i style="width:'+pct()+'%;background:'+col(e.c)+';border-radius:'+r+'px"></i></div>';
    }else if(e.t=="gif"){
      h+='<div class="el gifel'+s+'" data-i="'+i+'" style="left:'+e.x+'px;top:'+e.y+'px"><img alt="" src="gif/file?slot='+encodeURIComponent(e.g)+'&v='+V+'"></div>';
    }else if(e.t=="face"){
      // resting coaster face (it moves on the KNOMI)
      var c=col("t"),d=e.d||240;
      h+='<div class="el gifel'+s+'" data-i="'+i+'" style="left:'+e.x+'px;top:'+e.y+'px;width:'+d+'px;height:'+d+'px">'+
        '<svg viewBox="0 0 240 240" width="'+d+'" height="'+d+'" style="display:block;pointer-events:none">'+
        '<g stroke="'+c+'" stroke-width="5" stroke-linecap="round" fill="none"><line x1="40" y1="104" x2="92" y2="104"/><line x1="148" y1="104" x2="200" y2="104"/>'+
        '<path stroke-width="4" d="M106 140 a7 7 0 0 0 14 0 a7 7 0 0 0 14 0"/></g>'+
        '<g fill="'+c+'"><path d="M55 104 a11 11 0 0 0 22 0z"/><path d="M163 104 a11 11 0 0 0 22 0z"/></g></svg></div>';
    }
  });
  return h;
}
function label(e){
  if(e.t=="text")return e.txt||"(empty text)";
  if(e.t=="arc")return(e.p?"Progress ":"")+(span(e)>=360?"ring":"arc")+" ⌀"+e.d;
  if(e.t=="bar")return"Progress bar "+e.w+"×"+e.h;
  if(e.t=="face")return"Coaster face ⌀"+(e.d||240);
  if(e.t=="gif"){var g=state.gifs.filter(function(x){return x.name==e.g})[0];return"Animation: "+(g?g.label:e.g)}
  return e.t;
}
function renderList(){
  var pg=L.pages[page],h="";
  pg.el.forEach(function(e,i){
    h+='<li data-i="'+i+'" class="'+(i==sel?"on":"")+'"><span class="ty">'+e.t+'</span><span class="nm">'+esc(label(e))+'</span>'+
      '<button type="button" class="btn-ghost" data-act="up" data-i="'+i+'" title="Draw earlier (below)">↑</button>'+
      '<button type="button" class="btn-ghost" data-act="down" data-i="'+i+'" title="Draw later (on top)">↓</button>'+
      '<button type="button" class="btn-ghost" data-act="del" data-i="'+i+'" title="Delete">✕</button></li>';
  });
  $("layers").innerHTML=h||'<li style="cursor:default;color:var(--muted-2)">Nothing on this page yet.</li>';
  var ph="";
  L.pages.forEach(function(p,i){var ev=p.m=="e";ph+='<button type="button" class="btn-ghost'+(i==page?" on":"")+(ev?" evt":"")+'" data-p="'+i+'" title="'+(ev?"Shown when something happens":"In the rotation")+'">'+(i+1)+(ev?'<small>'+esc(trigSummary(p))+'</small>':'')+'</button>'});
  if(L.pages.length<4)ph+='<button type="button" class="btn-ghost" data-p="new" title="Add a page">+ page</button>';
  $("pages").innerHTML=ph;
  $("pdur").value=L.pages[page].s;
  fillTrig();
  $("pdel").disabled=L.pages.length<2;
  $("pdup").disabled=L.pages.length>=4;
}

/* ---------- properties ---------- */
function num(k,lbl,min,max,cls){return'<div class="'+(cls||"")+'"><label class="field-label">'+lbl+'</label><input type="number" class="mono" data-k="'+k+'" min="'+min+'" max="'+max+'"></div>'}
function colorField(k,lbl,allowNone){
  var o=allowNone?'<option value="">None</option>':"";
  for(var n in NAMED)o+='<option value="'+n+'">'+NAMED[n]+'</option>';
  o+='<option value="#">Custom</option>';
  return'<div class="half"><label class="field-label">'+lbl+'</label><div class="inline"><select data-ck="'+k+'">'+o+'</select><input type="color" data-cc="'+k+'"></div></div>';
}
function renderProps(){
  var box=$("props"),e=sel>=0?L.pages[page].el[sel]:null;
  if(!e){box.innerHTML='<div class="empty-props">Select an element.</div>';$("ptitle").textContent="Properties";return}
  $("ptitle").textContent={text:"Text",arc:"Ring / arc",bar:"Progress bar",gif:"Animation",face:"Coaster face"}[e.t];
  var h='<div class="props">';
  if(e.t=="text"){
    h+='<div class="full"><label class="field-label">Text</label><input type="text" data-k="txt" maxlength="120"><div class="tokens">';
    TOKENS.forEach(function(t){h+='<button type="button" class="btn-ghost" data-tok="'+t[0]+'" title="'+t[1]+'">{'+t[0]+'}</button>'});
    h+='</div><div class="hint">Words in braces become live values.</div></div>';
    h+='<div><label class="field-label">Size</label><select data-k="f">'+FONTS.map(function(f){return'<option value="'+f+'">'+f+'</option>'}).join("")+'</select></div>';
    h+='<div><label class="field-label">Align</label><select data-k="a"><option value="l">Left</option><option value="c">Center</option><option value="r">Right</option></select></div>';
    h+=colorField("c","Color");
    h+=num("x","X",0,240)+num("y","Y",0,240)+num("w","Width (0 = fit)",0,240,"half");
    h+='<div class="full"><label class="chk" style="margin-top:0"><input type="checkbox" data-b="sc"> Scroll long text (needs a width)</label></div>';
  }else if(e.t=="arc"){
    h+=num("x","X",0,240)+num("y","Y",0,240)+num("d","Diameter",10,240)+num("w","Thickness",1,120);
    h+=num("s","Start angle",0,360)+num("e","End angle",0,360);
    h+='<div class="half"><label class="field-label">Quick shape</label><select data-shape><option value="">Choose…</option><option value="0,360">Full ring</option><option value="225,135">Gauge (open bottom)</option><option value="270,90">Top half</option><option value="90,270">Bottom half</option></select></div>';
    h+=colorField("c","Color")+colorField("b","Track",true);
    h+='<div class="half"><label class="chk"><input type="checkbox" data-b="p"> Shows progress</label></div>';
    h+='<div class="half"><label class="chk"><input type="checkbox" data-b="rd"> Rounded ends</label></div>';
    h+='<div class="full hint">Angles: 0 is the top, going clockwise.</div>';
  }else if(e.t=="bar"){
    h+=num("x","X",0,240)+num("y","Y",0,240)+num("w","Width",4,240)+num("h","Height",2,120);
    h+=colorField("c","Color")+colorField("b","Track",true);
    h+='<div class="half"><label class="chk"><input type="checkbox" data-b="rd"> Rounded</label></div>';
  }else if(e.t=="face"){
    h+=num("x","X",0,240)+num("y","Y",0,240)+num("d","Size",60,240,"half");
    h+='<div class="full hint">The face reacts to the toolhead live on the KNOMI; this preview shows it at rest. <a href="coaster">Tune it</a></div>';
  }else if(e.t=="gif"){
    h+='<div class="half"><label class="field-label">Animation</label><select data-k="g">'+
      state.gifs.map(function(g){return'<option value="'+g.name+'">'+esc(g.label)+'</option>'}).join("")+'</select></div>';
    h+=num("x","X",0,240)+num("y","Y",0,240);
    h+='<div class="full hint">Plays the same animation as that slot, including one you uploaded. <a href="gifs">Animations</a></div>';
  }
  h+='<div class="full" style="display:flex;gap:8px;flex-wrap:wrap;margin-top:4px">'+
     '<button type="button" class="btn-ghost" data-center="x">Center horizontally</button>'+
     '<button type="button" class="btn-ghost" data-center="y">Center vertically</button>'+
     '<button type="button" class="btn-ghost" data-dup>Duplicate</button></div></div>';
  box.innerHTML=h;
  fillProps();
}
function fillProps(){
  var e=sel>=0?L.pages[page].el[sel]:null;if(!e)return;
  var box=$("props");
  box.querySelectorAll("[data-k]").forEach(function(inp){var v=e[inp.dataset.k];inp.value=v===undefined?"":v});
  box.querySelectorAll("[data-b]").forEach(function(inp){inp.checked=!!e[inp.dataset.b]});
  box.querySelectorAll("[data-ck]").forEach(function(s){
    var k=s.dataset.ck,v=e[k]||"",cc=box.querySelector('[data-cc="'+k+'"]');
    s.value=v[0]=="#"?"#":v;cc.value=v[0]=="#"?v:(v?col(v):"#000000");cc.style.display=v[0]=="#"?"":"none";
  });
}
function setVal(k,v){var e=L.pages[page].el[sel];e[k]=v;render();}

/* ---------- events ---------- */
$("props").addEventListener("input",function(ev){
  var t=ev.target,e=sel>=0?L.pages[page].el[sel]:null;if(!e)return;
  if(t.dataset.k){var k=t.dataset.k,v=t.value;
    if(t.type=="number"||k=="f"){v=parseInt(v,10);if(isNaN(v))return;}
    e[k]=v;render();
    if(k=="g")renderList();
  }else if(t.dataset.cc){e[t.dataset.cc]=t.value;render();}
});
$("props").addEventListener("change",function(ev){
  var t=ev.target,e=sel>=0?L.pages[page].el[sel]:null;if(!e)return;
  if(t.dataset.b){e[t.dataset.b]=t.checked?1:0;render();}
  else if(t.dataset.ck){var k=t.dataset.ck;if(t.value=="#"){e[k]=col(e[k]||"x");}else e[k]=t.value;render();fillProps();}
  else if(t.hasAttribute("data-shape")&&t.value){var p=t.value.split(",");e.s=+p[0];e.e=+p[1];t.value="";render();fillProps();}
});
$("props").addEventListener("click",function(ev){
  var t=ev.target,e=sel>=0?L.pages[page].el[sel]:null;if(!e)return;
  if(t.dataset.tok){var inp=$("props").querySelector('[data-k="txt"]'),a=inp.selectionStart||inp.value.length,b=inp.selectionEnd||a;
    inp.value=inp.value.slice(0,a)+"{"+t.dataset.tok+"}"+inp.value.slice(b);e.txt=inp.value;render();inp.focus();}
  else if(t.dataset.center){e[t.dataset.center]=120;render();fillProps();}
  else if(t.hasAttribute("data-dup")){addEl(clone(e),true);}
});
function addEl(e,offset){
  var pg=L.pages[page];if(pg.el.length>=24){status("24 elements per page at most","bad");return}
  if(offset){e.x=Math.min(236,e.x+8);e.y=Math.min(236,e.y+8)}
  pg.el.push(e);sel=pg.el.length-1;render();renderProps();
}
$("adds").innerHTML=ADDS.map(function(a,i){return'<button type="button" class="btn-ghost" data-add="'+i+'">+ '+a[0]+'</button>'}).join("");
$("adds").addEventListener("click",function(ev){var i=ev.target.dataset.add;if(i===undefined)return;var e=clone(ADDS[i][1]);
  if(e.t=="gif"){if(!state.gifs.length){status("No animations left: Coaster acts everything out. Upload a GIF on the Animations page first.","bad");return}e.g=state.gifs[0].name}
  addEl(e)});
$("layers").addEventListener("click",function(ev){
  var t=ev.target,i=parseInt(t.dataset.i,10),pg=L.pages[page];
  if(t.dataset.act=="del"){pg.el.splice(i,1);sel=-1;render();renderProps();return}
  if(t.dataset.act=="up"&&i>0){var x=pg.el.splice(i,1)[0];pg.el.splice(i-1,0,x);sel=i-1;render();renderProps();return}
  if(t.dataset.act=="down"&&i<pg.el.length-1){var y=pg.el.splice(i,1)[0];pg.el.splice(i+1,0,y);sel=i+1;render();renderProps();return}
  var li=t.closest("li");if(li&&li.dataset.i!==undefined){sel=parseInt(li.dataset.i,10);render();renderProps()}
});
$("pages").addEventListener("click",function(ev){
  var b=ev.target.closest("[data-p]");if(!b)return;var p=b.dataset.p;
  if(p=="new"){L.pages.push({s:10,el:[]});page=L.pages.length-1}else page=parseInt(p,10);
  sel=-1;render();renderProps();
});
$("pdur").addEventListener("input",function(){var v=parseInt(this.value,10);if(v>0){L.pages[page].s=Math.min(3600,v);dirty()}});
/* page triggers */
var TR=[["pe","tr_pe","tr_pe_n",10],["pa","tr_pa","tr_pa_n",[25,50,75]],["le","tr_le","tr_le_n",1],["st","tr_st",null,1],["lm","tr_lm","tr_lm_n",10],["fl","tr_fl",null,1]];
function trigSummary(p){var t=p.tr||{},o=[];
  if(t.pe)o.push(t.pe+"%");if(t.pa&&t.pa.length)o.push("@"+t.pa.join("/")+"%");if(t.le)o.push(t.le>1?t.le+" layers":"layer");
  if(t.st)o.push("start");if(t.lm)o.push("<"+t.lm+"m");if(t.fl)o.push("1st layer");return o.join(" · ")||"no trigger"}
function fillTrig(){
  var p=L.pages[page],ev=p.m=="e",t=p.tr||{};
  $("pmode").value=ev?"e":"r";$("trig").hidden=!ev;
  $("pdurlbl").textContent=ev?"Pop up for (seconds)":"For (seconds)";
  TR.forEach(function(d){var on=!!(d[0]=="pa"?(t.pa&&t.pa.length):t[d[0]]);$(d[1]).checked=on;
    if(d[2]&&on)$(d[2]).value=d[0]=="pa"?t.pa.join(", "):t[d[0]]});
}
function readTrig(){
  var p=L.pages[page],t={};
  TR.forEach(function(d){if(!$(d[1]).checked)return;
    if(!d[2]){t[d[0]]=1;return}
    if(d[0]=="pa"){var v=$(d[2]).value.split(/[^0-9]+/).map(Number).filter(function(x){return x>=1&&x<=100}).slice(0,8);if(v.length)t.pa=v;return}
    var n=parseInt($(d[2]).value,10);if(n>0)t[d[0]]=n});
  p.tr=t;dirty();renderList();
}
$("pmode").addEventListener("change",function(){var p=L.pages[page];
  if(this.value=="e"){p.m="e";if(!p.tr||!Object.keys(p.tr).length)p.tr={pe:10};if(p.s>20)p.s=8}else{delete p.m;delete p.tr}
  fillTrig();renderList();dirty()});
$("trig").addEventListener("input",readTrig);$("trig").addEventListener("change",readTrig);

/* play a simulated print: 90 s for the whole thing, pages picked the way the KNOMI picks them */
var sim=null;
function simTokens(s){var left=Math.max(0,Math.round(s.leftMin*60));
  var fmt=function(x){return x<60?"<1m":x<3600?Math.floor(x/60)+"m":Math.floor(x/3600)+"h "+String(Math.floor(x%3600/60)).padStart(2,"0")+"m"};
  return{pct:String(Math.floor(s.pct)),left:fmt(left),time:fmt(left)+" left, "+new Date(Date.now()+left*1000).toLocaleTimeString([],{hour:"numeric",minute:"2-digit"}).toLowerCase().replace(" ",""),elapsed:fmt(s.el),total:fmt(s.el+left),file:SAMPLE.file,
    noz:"215",noz_t:"215",bed:"60",bed_t:"60",deg:"℃",z:(s.layer*0.2).toFixed(2),layer:String(s.layer),layers:"200",pos:"Layer "+s.layer+"/200",state:"Printing",msg:SAMPLE.msg,eta:new Date(Date.now()+left*1000).toLocaleTimeString([],{hour:"numeric",minute:"2-digit"}).toLowerCase()}}
function simCond(p,s){var t=p.tr||{};return(t.lm&&s.leftMin<t.lm)||(t.fl&&s.layer<=1)}
function simStep(){
  var s=sim,now=performance.now(),dt=(now-s.last)/1000;s.last=now;
  var prevPct=s.pct,prevLayer=s.layer;
  s.pct=Math.min(100,s.pct+dt*100/90);s.layer=Math.max(1,Math.ceil(s.pct*2));s.leftMin=(100-s.pct)*0.9;s.el+=dt*54;s.t+=dt;
  var P=L.pages;
  function fire(i,why){s.ev=i;s.evUntil=s.t+P[i].s;s.why=why}
  if(!s.started){s.started=true;P.forEach(function(p,i){if(p.m=="e"&&(p.tr||{}).st)fire(i,"print started")})}
  P.forEach(function(p,i){if(p.m!="e")return;var t=p.tr||{};
    if(t.pe&&Math.floor(s.pct/t.pe)>Math.floor(prevPct/t.pe))fire(i,Math.floor(s.pct/t.pe)*t.pe+"% reached");
    (t.pa||[]).forEach(function(v){if(prevPct<v&&s.pct>=v)fire(i,v+"% reached")});
    if(t.le&&s.layer!=prevLayer&&(s.layer-1)%t.le==0)fire(i,"layer "+s.layer)});
  var show=-1,why="";
  P.forEach(function(p,i){if(show<0&&p.m=="e"&&simCond(p,s)){show=i;why=(p.tr.lm&&s.leftMin<p.tr.lm)?"under "+p.tr.lm+" min left":"first layer"}});
  if(show<0&&s.ev>=0&&s.t<s.evUntil){show=s.ev;why=s.why}
  if(show<0){s.ev=-1;
    if(s.rot<0||P[s.rot].m=="e"||s.t>=s.rotNext){var n=-1;for(var k=1;k<=P.length;k++){var c=((s.rot<0?P.length-1:s.rot)+k)%P.length;if(P[c].m!="e"){n=c;break}}
      if(n<0)n=0;s.rot=n;s.rotNext=s.t+P[n].s}
    show=s.rot;why="rotation"}
  s.tokens=simTokens(s);
  $("scr").innerHTML=layered(P[show]);
  $("playcap").textContent="Page "+(show+1)+" · "+why;
  $("playfill").style.width=s.pct+"%";
  if(s.pct>=100){stopSim();$("playcap").textContent="Print finished";return}
  s.raf=requestAnimationFrame(simStep);
}
function stopSim(){if(!sim)return;cancelAnimationFrame(sim.raf);sim=null;$("play").textContent="Play a print";$("playbar").hidden=true;$("scrwrap").style.pointerEvents="";render()}
$("play").addEventListener("click",function(){
  if(sim){stopSim();$("playcap").textContent="";return}
  sel=-1;renderProps();
  sim={pct:0,layer:1,leftMin:90,el:0,t:0,last:performance.now(),ev:-1,evUntil:0,rot:-1,rotNext:0,started:false,tokens:SAMPLE};
  $("play").textContent="Stop";$("playbar").hidden=false;$("scrwrap").style.pointerEvents="none";simStep();
});
$("pdup").addEventListener("click",function(){if(L.pages.length>=4)return;L.pages.splice(page+1,0,clone(L.pages[page]));page++;sel=-1;render();renderProps()});
$("pdel").addEventListener("click",function(){if(L.pages.length<2)return;if(!confirm("Delete page "+(page+1)+"?"))return;L.pages.splice(page,1);page=Math.max(0,page-1);sel=-1;render();renderProps()});
$("starter").addEventListener("change",function(){var k=this.value;this.value="";if(!k)return;
  if(!confirm("Replace all pages with this layout?"))return;L=starters(k);page=0;sel=-1;render();renderProps()});

/* drag on the screen */
var drag=null;
function scrPoint(ev){var r=$("scrwrap").getBoundingClientRect();return[(ev.clientX-r.left)/S,(ev.clientY-r.top)/S]}
$("scrwrap").addEventListener("pointerdown",function(ev){
  var t=ev.target.closest("[data-i]");
  if(!t){sel=-1;render();renderProps();return}
  sel=parseInt(t.dataset.i,10);var e=L.pages[page].el[sel],p=scrPoint(ev);
  drag={dx:p[0]-e.x,dy:p[1]-e.y,id:ev.pointerId};$("scrwrap").setPointerCapture(ev.pointerId);
  render();renderProps();ev.preventDefault();
});
$("scrwrap").addEventListener("pointermove",function(ev){
  if(!drag||ev.pointerId!==drag.id)return;
  var e=L.pages[page].el[sel],p=scrPoint(ev),x=Math.round(p[0]-drag.dx),y=Math.round(p[1]-drag.dy);
  var sx=Math.abs(x-120)<=3,sy=Math.abs(y-120)<=3;if(sx)x=120;if(sy)y=120;
  $("gv").style.display=sx?"block":"none";$("gh").style.display=sy?"block":"none";
  e.x=Math.max(0,Math.min(240,x));e.y=Math.max(0,Math.min(240,y));render();fillProps();
});
function endDrag(){drag=null;$("gv").style.display="none";$("gh").style.display="none"}
$("scrwrap").addEventListener("pointerup",endDrag);$("scrwrap").addEventListener("pointercancel",endDrag);
document.addEventListener("keydown",function(ev){
  if(sel<0||/INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName))return;
  var e=L.pages[page].el[sel],st=ev.shiftKey?5:1,k=ev.key;
  if(k=="ArrowLeft")e.x-=st;else if(k=="ArrowRight")e.x+=st;else if(k=="ArrowUp")e.y-=st;else if(k=="ArrowDown")e.y+=st;
  else if(k=="Delete"||k=="Backspace"){L.pages[page].el.splice(sel,1);sel=-1;render();renderProps();ev.preventDefault();return}
  else return;
  ev.preventDefault();render();fillProps();
});

/* save / preview / file */
function save(then){
  status("Saving…");
  fetch("layout.json",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(L)})
  .then(function(r){return r.text().then(function(t){if(!r.ok)throw new Error(t||r.status);return t})})
  .then(function(){saved=JSON.stringify(L);dirty();status("Saved. The KNOMI switched to it.","ok");if(then)then()})
  .catch(function(e){status("Not saved: "+e.message,"bad")});
}
$("save").addEventListener("click",function(){save()});
$("preview").addEventListener("click",function(){save(function(){
  fetch("layout/preview",{method:"POST"}).then(function(r){status(r.ok?"Showing on the KNOMI for 20 seconds (sample data unless printing).":"Preview failed",r.ok?"ok":"bad")})})});
$("export").addEventListener("click",function(){
  var a=document.createElement("a");a.href=URL.createObjectURL(new Blob([JSON.stringify(L,null,1)],{type:"application/json"}));
  a.download="knomi-print-screen.json";a.click();setTimeout(function(){URL.revokeObjectURL(a.href)},2000);
});
$("import").addEventListener("change",function(){
  var f=this.files[0];if(!f)return;var rd=new FileReader();
  rd.onload=function(){try{var j=JSON.parse(rd.result);if(!j.pages||!j.pages.length)throw 0;L=j;page=0;sel=-1;render();renderProps();status("Loaded. Save to put it on the KNOMI.","ok")}catch(e){status("That file isn't a KNOMI layout","bad")}};
  rd.readAsText(f);this.value="";
});
$("revert").addEventListener("click",function(){L=JSON.parse(saved);page=Math.min(page,L.pages.length-1);sel=-1;render();renderProps();status("")});
$("reset").addEventListener("click",function(){if(!confirm("Put the default printing screen back on the KNOMI?"))return;
  fetch("layout/reset",{method:"POST"}).then(function(){return load()}).then(function(){status("Default layout restored.","ok")})});
window.addEventListener("beforeunload",function(ev){if(dirty()){ev.preventDefault();ev.returnValue=""}});

/* live data */
function poll(){fetch("status.json").then(function(r){return r.json()}).then(function(j){
  state.tokens=j.tokens||SAMPLE;if(j.theme)state.theme=j.theme;if(j.gifs)state.gifs=j.gifs;render();}).catch(function(){})}
$("live").addEventListener("change",function(){live=this.checked;clearInterval(liveTimer);if(live){poll();liveTimer=setInterval(poll,2000)}render()});

/* sizing */
function fit(){
  var w=Math.min(360,Math.max(220,window.innerWidth-80));S=w/240;
  var sw=$("scrwrap");sw.style.width=w+"px";sw.style.height=w+"px";$("scr").style.transform="scale("+S+")";
}
window.addEventListener("resize",fit);fit();

function load(){
  return Promise.all([
    fetch("layout.json").then(function(r){return r.json()}),
    fetch("layout.json?default=1").then(function(r){return r.json()}),
    fetch("status.json").then(function(r){return r.json()}).catch(function(){return{}})
  ]).then(function(res){
    L=res[0];DEF=res[1];var st=res[2];
    if(st.theme)state.theme=st.theme;if(st.gifs)state.gifs=st.gifs;if(st.tokens)state.tokens=st.tokens;
    if(st.printing){live=true;$("live").checked=true;liveTimer=setInterval(poll,2000)}
    saved=JSON.stringify(L);page=0;sel=-1;render();renderProps();
  }).catch(function(){status("Couldn't load the layout from the KNOMI","bad")});
}
load();
})();
</script>
</body></html>)rawliteral";

#endif
