'use strict';
const definitions = [
 {name:'Distance to surface',unit:'cm',decimals:1,color:'#397d54'},
 {name:'Electrical conductivity',unit:'µS/cm',decimals:0,color:'#b5843d'},
 {name:'Soil pH',unit:'pH',decimals:1,color:'#6489ae'},
 {name:'Carbon dioxide',unit:'ppm',decimals:0,color:'#9576a6'},
 {name:'Moisture',unit:'%',decimals:1,color:'#378994'},
 {name:'Temperature',unit:'°C',decimals:1,color:'#bc6c45'}
];
const $ = id => document.getElementById(id);
const state = {boot:'',after:0,samples:[],meta:null,anchor:0,online:false,paused:false,pending:0,lastResponse:0};
definitions.forEach((d,i)=>{
 const card=document.createElement('article');card.className='card';
 card.innerHTML=`<h2>${d.name}</h2><div class="reading"><span class="value" id="value${i}">—</span><span class="unit">${d.unit}</span></div><p class="sensor-status" id="status${i}">Waiting</p><div class="plot"><canvas id="plot${i}" role="img" aria-label="${d.name} trend"></canvas></div><div class="range" id="range${i}"></div>`;
 $('cards').appendChild(card);
});
function format(v,i){return Number.isFinite(v)?v.toFixed(definitions[i].decimals):'—';}
function clock(t){return new Date(state.anchor+t).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'});}
function drawChart(i){
 const c=$('plot'+i),ctx=c.getContext('2d'),w=c.clientWidth,h=c.clientHeight,ratio=devicePixelRatio||1;
 c.width=Math.round(w*ratio);c.height=Math.round(h*ratio);ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,w,h);
 if(w<100)return;
 const end=state.samples.length?state.samples.at(-1)[1]:0,start=Math.max(0,end-Number($('window').value)*1000);
 const rows=state.samples.filter(s=>s[1]>=start),values=rows.map(s=>s[i+2]).filter(Number.isFinite);
 const max=i===2?14:i===4?100:values.length?Math.max(1,...values)*1.12:1;
 const min=i===5&&values.length?Math.min(0,...values)*1.12:0;
 const left=48,top=12,pw=w-60,ph=h-42,x=t=>left+(t-start)/Math.max(1000,end-start)*pw,y=v=>top+ph-(v-min)/(max-min)*ph;
 ctx.font='10px system-ui';ctx.lineWidth=1;
 for(let k=0;k<4;k++){const v=min+(max-min)*k/3,py=y(v);ctx.strokeStyle='#e6ede1';ctx.beginPath();ctx.moveTo(left,py);ctx.lineTo(w-12,py);ctx.stroke();ctx.fillStyle='#748477';ctx.textAlign='right';ctx.fillText(v.toFixed(v<10?1:0),left-7,py+3);}
 ctx.textAlign='left';ctx.fillText(state.samples.length?clock(start):'Time',left,h-7);ctx.textAlign='right';if(state.samples.length)ctx.fillText(clock(end),w-12,h-7);
 if(!values.length){ctx.textAlign='center';ctx.fillText('Waiting for valid readings',left+pw/2,top+ph/2);}
 ctx.strokeStyle=definitions[i].color;ctx.lineWidth=2;ctx.lineJoin='round';ctx.beginPath();
 let previous=null;
 for(const row of rows){const value=row[i+2];if(!Number.isFinite(value)){previous=null;continue;}if(previous===null||row[1]-previous>(state.meta?.sampleMs||4000)*1.75)ctx.moveTo(x(row[1]),y(value));else ctx.lineTo(x(row[1]),y(value));previous=row[1];}
 ctx.stroke();
 const last=rows.at(-1);if(last&&Number.isFinite(last[i+2])){ctx.fillStyle=definitions[i].color;ctx.beginPath();ctx.arc(x(last[1]),y(last[i+2]),3,0,Math.PI*2);ctx.fill();}
 $('range'+i).textContent=values.length?`Min ${format(Math.min(...values),i)} · Max ${format(Math.max(...values),i)} ${definitions[i].unit} · ${values.length} readings`:'Missing readings appear as gaps.';
}
function renderControls(s){
 const on=v=>v?'ON':'OFF';
 $('modeStatus').textContent=`Mode: ${s.mode} · ${state.meta?.simulation?'Virtual relay outputs':`Physical relay configuration ${s.relayConfigured?'enabled':'disabled / unconfigured'}`}`;
 $('stageStatus').textContent=`${s.stage} · ${(s.stageElapsedMs/60000).toFixed(1)} min · ${s.postWaterMixDue?'Mix after watering due':s.periodicMixDue?'Periodic mix due':'Mix timer running'}`;
 $('fermentationStage').setAttribute('aria-pressed',String(s.stage==='fermentation'));$('finishingStage').setAttribute('aria-pressed',String(s.stage==='finishing'));
 $('manualMode').setAttribute('aria-pressed',String(s.mode==='manual'));$('autoMode').setAttribute('aria-pressed',String(s.mode==='auto'));
 $('latchStatus').textContent=s.stopLatched?'STOP LATCHED':'Stop latch clear';
 for(const name of ['motor','pump','fan'])$(''+name+'State').textContent=`Requested ${on(s.requested[name])} · Commanded relay ${on(s.commanded[name])} · Cooldown ${(s.cooldownMs[name]/1000).toFixed(0)}s`;
 $('inhibition').textContent=s.reason;
 $('model').textContent=`Synthetic Random Forest · ${s.prediction.modelReady?`${s.prediction.action} (${(s.prediction.confidence*100).toFixed(1)}%)`:'inputs unavailable'} · ${s.prediction.reason}`;
 document.querySelectorAll('[data-command]').forEach(b=>{
  const command=b.dataset.command,start=command.endsWith('_start'),stop=command==='stop_all'||command.endsWith('_stop')&&command!=='reset_stop';
  b.disabled=!state.online||(!stop&&state.pending>0)||(start&&(!s.relayConfigured||s.stopLatched||s.mode!=='manual'||s.stage!=='finishing'))||(command==='reset_stop'&&!s.stopLatched);
 });
}
function renderReadings(){
 if(!state.meta)return;
 const elapsed=state.meta.simulation?0:Date.now()-state.lastResponse,latest=state.meta.latest;
 definitions.forEach((d,i)=>{
  const r=latest.readings[i],fresh=state.online&&r.valid&&r.ageMs+elapsed<(state.meta.freshMs||12000);
  $('value'+i).textContent=fresh?format(r.value,i):'—';
  $('status'+i).textContent=!state.online?'Connection lost · history retained':r.valid&&!fresh?'Stale reading':r.status;
  drawChart(i);
 });
 $('freshness').textContent=latest.sequence?`Sample #${latest.sequence} · ${clock(latest.uptime)} · every ${(state.meta.sampleMs||4000)/1000} seconds`:'Waiting for first sample';
 $('export').disabled=!state.samples.length;
 $('historyMeta').textContent=`${state.samples.length} / 900 samples · RAM clears on restart`;
}
function acceptData(data){
 if(state.boot!==data.boot||data.reset){state.samples=[];state.after=0;state.boot=data.boot;state.paused=false;$('pause').textContent='Pause display';}
 state.online=true;state.lastResponse=Date.now();if(!data.simulation||!state.meta||state.meta.boot!==data.boot)state.anchor=Date.now()-data.uptime;
 for(const row of data.samples){if(row[0]>state.after){state.samples.push(row);state.after=row[0];}}
 if(state.samples.length>900)state.samples.splice(0,state.samples.length-900);
 state.meta=data;
 $('connection').className='connection online';$('connection').textContent=state.paused?'Connected · display paused':data.simulation?'Local simulator connected':'Connected to ESP32';
 const stale=!data.latest.sequence||data.latest.readings.some(r=>!r.valid||r.ageMs>=(data.freshMs||12000));
 $('sensorNotice').hidden=!stale;$('sensorNotice').textContent=stale?'One or more sensors are unavailable or stale. See each sensor status.':'';
 renderControls(data.status);if(!state.paused)renderReadings();
 document.dispatchEvent(new Event('monitor-state'));
}
function markOffline(){
 state.online=false;$('connection').className='connection offline';$('connection').textContent='Connection lost';
 $('sensorNotice').hidden=false;$('sensorNotice').textContent=state.meta?.simulation?'Simulator connection lost. Check that its terminal is running; the page will retry.':'Reconnect to CompostControl Wi-Fi. Readings and control status will retry automatically.';
 definitions.forEach((d,i)=>{$('value'+i).textContent='—';$('status'+i).textContent='Connection lost · history retained';});
 document.querySelectorAll('[data-command]').forEach(b=>b.disabled=true);
 document.dispatchEvent(new Event('monitor-state'));
}
async function poll(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);let more=false;
 try{
  const response=await fetch(`/api/data?after=${state.after}&boot=${encodeURIComponent(state.boot)}`,{cache:'no-store',signal:controller.signal});
  if(!response.ok)throw Error('HTTP '+response.status);
  const data=await response.json();acceptData(data);more=data.more;
 }catch(error){markOffline();}
 finally{clearTimeout(timer);setTimeout(poll,more?20:state.meta?.simulation?250:2000);}
}
async function sendCommand(command){
 if(!state.online||!state.meta)return;
 state.pending++;renderControls(state.meta.status);$('commandMessage').className='command-message';$('commandMessage').textContent='Waiting for device acknowledgement…';
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);
 try{
  const response=await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Control-Token':state.meta.controlToken},body:new URLSearchParams({command}),signal:controller.signal});
  const data=await response.json();
  state.meta.status=data.status;renderControls(data.status);
  $('commandMessage').textContent=data.message;$('commandMessage').className='command-message'+(data.accepted?'':' error');
 }catch(error){$('commandMessage').textContent='No acknowledgement. Command outcome unknown; reconnect to read authoritative device status.';$('commandMessage').className='command-message error';markOffline();}
 finally{clearTimeout(timer);state.pending--;if(state.online)renderControls(state.meta.status);}
}
function csvText(){
 const rows=['time_utc,uptime_seconds,distance_cm,ec_uS_cm,pH,co2_ppm,moisture_pct,temperature_c'];
 for(const s of state.samples)rows.push([new Date(state.anchor+s[1]).toISOString(),(s[1]/1000).toFixed(3),...s.slice(2).map(v=>Number.isFinite(v)?v:'')].join(','));
 return rows.join('\r\n');
}
document.querySelectorAll('[data-command]').forEach(b=>b.addEventListener('click',()=>sendCommand(b.dataset.command)));
$('pause').addEventListener('click',()=>{state.paused=!state.paused;$('pause').textContent=state.paused?'Resume display':'Pause display';if(state.online){$('connection').textContent=state.paused?'Connected · display paused':state.meta.simulation?'Local simulator connected':'Connected to ESP32';if(!state.paused)renderReadings();}});
$('window').addEventListener('change',()=>{if(!state.paused)definitions.forEach((d,i)=>drawChart(i));});
$('export').addEventListener('click',()=>{const url=URL.createObjectURL(new Blob([csvText()],{type:'text/csv;charset=utf-8'}));const link=document.createElement('a');link.href=url;link.download='compost-readings.csv';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
let resizeTimer;addEventListener('resize',()=>{clearTimeout(resizeTimer);resizeTimer=setTimeout(()=>{if(!state.paused)definitions.forEach((d,i)=>drawChart(i));},100);});
setInterval(()=>{if(state.online&&!state.paused)renderReadings();},1000);
definitions.forEach((d,i)=>drawChart(i));poll();
