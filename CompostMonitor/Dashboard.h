#pragma once
#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"COMPOSTHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f3f5ef">
<title>Compost Bin · Live Monitor</title>
<style>
:root{--ink:#20352b;--muted:#68756d;--paper:#f3f5ef;--border:#dfe6dc;--green:#357354}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:1200px;margin:auto;padding:32px 24px 48px}header{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:26px}
.eyebrow{font-size:11px;font-weight:750;letter-spacing:.16em;text-transform:uppercase;color:var(--green)}h1{font-size:clamp(27px,4vw,40px);letter-spacing:-.05em;margin:3px 0}p{margin:0;color:var(--muted)}
.connection{display:flex;align-items:center;gap:8px;font-size:13px;background:white;border:1px solid var(--border);padding:9px 13px;border-radius:24px;white-space:nowrap}
.dot{width:8px;height:8px;background:#b59043;border-radius:50%}.connected .dot{background:#409369}.offline .dot{background:#bf5c50}
.toolbar{display:flex;flex-wrap:wrap;align-items:center;gap:10px;margin-bottom:22px}.toolbar label{font-size:13px;color:var(--muted)}
button,select{font:inherit;font-size:13px;border:1px solid var(--border);background:white;color:var(--ink);border-radius:8px;padding:9px 12px;cursor:pointer}button:hover{border-color:#8aab93}button:focus-visible,select:focus-visible{outline:3px solid #9bbf9b;outline-offset:2px}button:disabled{cursor:default;opacity:.5}
.sample-meta{margin-left:auto;font-size:12px;color:var(--muted)}.notice{padding:13px 16px;background:#faf4e4;border:1px solid #ecdfb8;border-radius:9px;margin-bottom:22px;font-size:13px;color:#786232}.notice[hidden]{display:none}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}.card{background:white;border:1px solid var(--border);border-radius:14px;padding:22px;overflow:hidden}
.card-top{display:flex;align-items:center;justify-content:space-between;gap:10px}.card h2{font-size:14px;font-weight:600;margin:0;color:var(--muted)}.tag{font-size:10px;letter-spacing:.07em;text-transform:uppercase;color:var(--muted)}
.reading{display:flex;align-items:baseline;gap:8px;margin:8px 0 0}.value{font-size:38px;font-weight:650;letter-spacing:-.05em;font-variant-numeric:tabular-nums}.unit{color:var(--muted);font-size:13px}
.status{font-size:12px;color:var(--muted);min-height:36px}.plot{height:188px;position:relative;margin-top:6px}canvas{display:block;width:100%;height:100%}.range{font-size:11px;color:var(--muted);margin-top:4px;min-height:17px}
footer{display:flex;flex-wrap:wrap;justify-content:space-between;gap:10px;margin-top:22px;font-size:12px;color:var(--muted)}details{margin-top:22px;border:1px solid var(--border);border-radius:10px;padding:13px 16px;background:#f9faf7;font-size:13px}summary{cursor:pointer;font-weight:600}details p{margin-top:10px}code{background:#e9eee5;padding:2px 5px;border-radius:4px;font-size:12px}.steps{margin:10px 0 0;padding-left:22px}.steps li{margin:6px 0}
@media(max-width:650px){main{padding:22px 14px 35px}header{align-items:flex-start;flex-direction:column}.grid{grid-template-columns:1fr}.card{padding:18px}.sample-meta{width:100%;margin-left:0}.plot{height:180px}}
</style>
</head>
<body>
<main>
<header><div><div class="eyebrow">Integrated Design Project</div><h1>Compost bin monitor</h1><p>Your sensors, changing over time.</p></div><div class="connection" id="connection" role="status"><span class="dot"></span><span id="connectionText">Connecting…</span></div></header>
<div class="toolbar"><label for="window">Show last</label><select id="window"><option value="60">1 minute</option><option value="300" selected>5 minutes</option><option value="900">15 minutes</option><option value="1800">30 minutes</option></select><button id="pause" type="button">Pause display</button><button id="export" type="button" disabled>Download CSV</button><span class="sample-meta" id="meta">Waiting for measurements</span></div>
<div class="notice" id="notice" role="status">Connect to the CompostBin Wi-Fi network to view live readings.</div>
<div class="grid" id="cards"></div>
<details><summary>Sensor settings and connection</summary><p>All sensor reads start automatically after boot. Keep both RS485 sensors connected. The ESP32 reads soil EC/pH at ID 1, 4800 baud, then switches its UART to read CO2 at ID 1, 9600 baud. Both use 8N1. These are factory defaults; the sensor settings are never changed.</p><p>EC, pH and CO2 appear when valid replies arrive. Missing replies remain blank with a status on each card. No setup command or scan is needed. If actual sensor settings differ, edit SOIL_ADDRESS, SOIL_BAUD, CO2_ADDRESS and CO2_BAUD in the sketch. Mixed baud rates on the shared bus are a diagnostic arrangement; verify real replies before relying on readings.</p><p>Serial Monitor: 115200 baud. Optional commands: <code>STOP MONITORING</code>, <code>START MONITORING</code>, <code>TRACE ON</code>, <code>TRACE OFF</code>. Optional scans remain available as <code>SCAN BUS</code> and <code>SCAN BUS FULL</code>; use <code>STOP SCAN</code> to cancel. The soil ECPH model supplies EC and pH. Sampling is every 2 seconds. No relay or motor control is included.</p></details>
<footer><span>Local monitoring · no internet required</span><span>History stays on the ESP32 for about 30 minutes and clears when it restarts.</span></footer>
</main>
<script>
'use strict';
const definitions = [
 {name:'Distance to surface',unit:'cm',source:'Ultrasonic',color:'#3e815b',decimals:1,column:2},
 {name:'Electrical conductivity',unit:'µS/cm',source:'Soil · EC',color:'#bd8240',decimals:0,column:3},
 {name:'Soil pH',unit:'pH',source:'Soil · pH',color:'#688abb',decimals:1,column:4},
 {name:'Carbon dioxide',unit:'ppm',source:'CO₂',color:'#9678ab',decimals:0,column:5}
];
const $ = id => document.getElementById(id);
const state = {samples:[], boot:'', after:0, paused:false, online:false, meta:null, anchor:0};
definitions.forEach((d,i)=>{
 const article=document.createElement('article'); article.className='card';
 article.innerHTML=`<div class="card-top"><h2>${d.name}</h2><span class="tag">${d.source}</span></div><div class="reading"><span class="value" id="value${i}">—</span><span class="unit">${d.unit}</span></div><div class="status" id="status${i}">Waiting for measurements</div><div class="plot"><canvas id="plot${i}" role="img" aria-label="${d.name} over time"></canvas></div><div class="range" id="range${i}"></div>`;
 $('cards').appendChild(article);
});
function connection(text,mode){$('connection').className='connection '+mode;$('connectionText').textContent=text;}
function clock(time){return new Date(state.anchor+time).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'});}
function format(value,d){return Number.isFinite(value)?value.toFixed(d.decimals):'—';}
function drawChart(index){
 const d=definitions[index],canvas=$('plot'+index),ctx=canvas.getContext('2d');
 const width=canvas.clientWidth,height=canvas.clientHeight,scale=window.devicePixelRatio||1;
 canvas.width=Math.round(width*scale);canvas.height=Math.round(height*scale);ctx.setTransform(scale,0,0,scale,0,0);ctx.clearRect(0,0,width,height);
 if(width<100)return;
 const left=49,right=12,top=12,bottom=30,w=width-left-right,h=height-top-bottom;
 const end=state.samples.length?state.samples[state.samples.length-1][1]:0;
 const span=Number($('window').value)*1000,start=Math.max(0,end-span),extent=Math.max(1000,end-start);
 const visible=state.samples.filter(s=>s[1]>=start&&s[1]<=end);
 const values=visible.map(s=>s[d.column]).filter(Number.isFinite);
 let minimum=0,maximum=1;
 if(index===2)maximum=14;
 else if(values.length) maximum=Math.max(1,Math.max(...values)*1.12);
 const y=value=>top+h-(value-minimum)/(maximum-minimum)*h;
 const x=time=>left+(time-start)/extent*w;
 ctx.font='10px system-ui';ctx.fillStyle='#7c877e';ctx.lineWidth=1;
 for(let tick=0;tick<4;tick++){
  const value=maximum*tick/3,py=y(value);ctx.strokeStyle='#e9ede6';ctx.beginPath();ctx.moveTo(left,py);ctx.lineTo(width-right,py);ctx.stroke();
  ctx.textAlign='right';ctx.fillText(value>=1000?Math.round(value).toLocaleString():value.toFixed(value<10?1:0),left-7,py+3);
 }
 ctx.textAlign='left';ctx.fillText(state.samples.length?clock(start):'Time',left,height-7);
 ctx.textAlign='right';if(state.samples.length)ctx.fillText(clock(end),width-right,height-7);
 if(!values.length){ctx.fillStyle='#8d978e';ctx.textAlign='center';ctx.fillText('Waiting for valid readings',left+w/2,top+h/2);$('range'+index).textContent='Missing readings appear as gaps.';return;}
 ctx.strokeStyle=d.color;ctx.lineWidth=2;ctx.lineJoin='round';ctx.beginPath();
 let continuing=false,previousTime=null;
 for(const sample of visible){
  const value=sample[d.column];
  if(!Number.isFinite(value)){continuing=false;previousTime=null;continue;}
  if(!continuing||previousTime===null||sample[1]-previousTime>3500)ctx.moveTo(x(sample[1]),y(value));else ctx.lineTo(x(sample[1]),y(value));
  continuing=true;previousTime=sample[1];
 }
 ctx.stroke();
 const last=visible[visible.length-1];
 if(last&&Number.isFinite(last[d.column])){ctx.fillStyle=d.color;ctx.beginPath();ctx.arc(x(last[1]),y(last[d.column]),3,0,Math.PI*2);ctx.fill();}
 $('range'+index).textContent=`Min ${format(Math.min(...values),d)} · Max ${format(Math.max(...values),d)} ${d.unit} · ${values.length} readings`;
}
function render(){
 if(!state.meta)return;
 const latest=state.samples[state.samples.length-1];
 definitions.forEach((d,i)=>{
  $('value'+i).textContent=latest?format(latest[d.column],d):'—';
  $('status'+i).textContent=state.meta.status[i];drawChart(i);
 });
 $('meta').textContent=latest?`Last sample ${clock(latest[1])} · every 2 seconds`:'Waiting for first sample';
 $('export').disabled=!state.samples.length;
}
function acceptData(data){
 state.online=true;
 if(state.boot!==data.boot){state.samples=[];state.after=0;state.boot=data.boot;}
 state.anchor=Date.now()-data.uptime;
 for(const sample of data.samples){
  if(sample[0]<=state.after)continue;
  state.samples.push(sample);state.after=sample[0];
 }
 if(state.samples.length>900)state.samples.splice(0,state.samples.length-900);
 state.meta=data;
 if(state.paused)connection('Display paused','');else connection('Connected to ESP32','connected');
 const stale=!state.samples.length||data.uptime-state.samples[state.samples.length-1][1]>6000;
 if(stale){
  $('notice').hidden=false;$('notice').textContent='Waiting for fresh measurements from the ESP32.';
  definitions.forEach((d,i)=>{$('value'+i).textContent='—';$('status'+i).textContent='Waiting for fresh sample';});
 }else{
  $('notice').hidden=data.monitoring;
  if(!data.monitoring)$('notice').textContent='RS485 monitoring is paused or scanning. Check Serial Monitor, or send START MONITORING after stopping a scan. Ultrasonic sampling continues.';
  if(!state.paused)render();
 }
}
function markOffline(){
 state.online=false;
 connection('Connection lost','offline');$('notice').hidden=false;$('notice').textContent='Reconnect to CompostBin Wi-Fi. The dashboard will retry automatically.';
 definitions.forEach((d,i)=>{$('value'+i).textContent='—';$('status'+i).textContent='Connection lost · history retained';});
}
async function poll(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);
 try{
  const response=await fetch(`/api/data?after=${state.after}&boot=${encodeURIComponent(state.boot)}`,{cache:'no-store',signal:controller.signal});
  if(!response.ok)throw Error('HTTP '+response.status);
  acceptData(await response.json());
 }catch(error){markOffline();}
 finally{clearTimeout(timer);setTimeout(poll,2000);}
}
$('window').addEventListener('change',()=>definitions.forEach((d,i)=>drawChart(i)));
$('pause').addEventListener('click',()=>{
 state.paused=!state.paused;$('pause').textContent=state.paused?'Resume display':'Pause display';
 if(!state.online){markOffline();return;}
 if(state.paused)connection('Display paused','');else{render();connection('Connected to ESP32','connected');}
});
$('export').addEventListener('click',()=>{
 const rows=['time_local,uptime_seconds,distance_cm,ec_uS_cm,pH,co2_ppm'];
 for(const s of state.samples)rows.push([new Date(state.anchor+s[1]).toISOString(),(s[1]/1000).toFixed(3),...s.slice(2).map(v=>Number.isFinite(v)?v:'')].join(','));
 // ISO timestamps include UTC "Z"; spreadsheet users can convert to their local timezone.
 rows[0]='time_utc,uptime_seconds,distance_cm,ec_uS_cm,pH,co2_ppm';
 const url=URL.createObjectURL(new Blob([rows.join('\r\n')],{type:'text/csv;charset=utf-8'}));
 const link=document.createElement('a');link.href=url;link.download='compost-readings.csv';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
});
let resizeTimer;window.addEventListener('resize',()=>{clearTimeout(resizeTimer);resizeTimer=setTimeout(()=>definitions.forEach((d,i)=>drawChart(i)),100);});
definitions.forEach((d,i)=>drawChart(i));poll();
</script>
</body>
</html>
)COMPOSTHTML";
