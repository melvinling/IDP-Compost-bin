'use strict';
const panel=document.createElement('section');panel.className='simulation control';
panel.innerHTML=`<div class="control-heading"><div><p class="eyebrow">SIMULATION LAB</p><h2>Your virtual compost bin</h2></div><span class="tag">No hardware connected</span></div>
<p class="notice">Synthetic Random Forest · illustrative physics. Scores and simulated behavior do not establish real compost performance.</p>
<div class="mode-row"><button id="simPlay">Play</button><button id="simPause">Pause clock</button><button id="simStep">Step 4 seconds</button><button id="simReset">Reset bin</button><label for="simSpeed">Speed</label><select id="simSpeed"><option value="1">1×</option><option value="10">10×</option><option value="60">60×</option></select><span id="simClock" class="small">Clock paused</span></div>
<p class="small">Scenarios reset the bin and select automatic mode. Finishing scenarios can immediately command a virtual relay. Press Play or Step to advance the clock. Reset returns to manual / fermentation.</p>
<div class="scenarios">${[['normal','Balanced'],['dry','Too dry'],['wet','Too wet'],['mix_due','Mix due'],['missing','Missing moisture'],['stale','Frozen acquisition'],['fermentation','Fermentation']].map(([value,label])=>`<button data-scenario="${value}">${label}</button>`).join('')}</div>
<details><summary>Adjust sensors and failures</summary><form id="simInputs"><div class="sensor-inputs"></div><label><input id="simFrozen" type="checkbox"> Freeze acquisition to test stale inputs</label><p class="small">Edits apply at the next 4-second acquisition. Missing sensor flags inhibit automatic outputs at that acquisition. Freezing preserves the last snapshot until it becomes stale after 12 seconds.</p><button type="submit">Apply sensor inputs</button></form></details>
<p id="simMessage" role="status" aria-live="polite" class="command-message"></p><details open><summary>Event log</summary><ol id="simEvents" class="events"></ol></details>`;
document.querySelector('header').after(panel);
const inputSpecs=[['Distance','cm',3,450,1],['EC','µS/cm',0,10000,1],['pH','pH',0,14,.1],['CO₂','ppm',0,10000,1],['Moisture','%',0,100,.1],['Temperature','°C',-40,80,.1]];
inputSpecs.forEach(([label,unit,min,max,step],i)=>{const row=document.createElement('div');row.innerHTML=`<label for="simValue${i}">${label} (${unit})</label><input id="simValue${i}" type="number" min="${min}" max="${max}" step="${step}" required><label class="small"><input id="simMissing${i}" type="checkbox" aria-label="${label} missing"> Missing</label>`;panel.querySelector('.sensor-inputs').append(row);});
let simBusy=false,simEdited=false;
$('simInputs').addEventListener('input',()=>simEdited=true);
async function simulate(action,extra={}){
 if(simBusy||!state.online||!state.meta)return;simBusy=true;
 const form=new URLSearchParams({action,...extra});
 if(action==='inputs'){inputSpecs.forEach((_,i)=>form.append('value',$('simValue'+i).value));form.set('missing',inputSpecs.reduce((n,_,i)=>n+($('simMissing'+i).checked?1<<i:0),0));form.set('frozen',$('simFrozen').checked?'1':'0');}
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);
 $('simMessage').className='command-message';$('simMessage').textContent='Applying simulation change…';renderSimulation();
 try{
  const response=await fetch('/api/simulation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Control-Token':state.meta.controlToken},body:form,signal:controller.signal});
  const data=await response.json();if(!response.ok)throw Error(data.error||data.message);acceptData({...data,reset:state.boot!==data.boot,more:false});
  if(['reset','scenario','inputs'].includes(action))simEdited=false;
  if(['reset','scenario'].includes(action))$('commandMessage').textContent='';
  const messages={play:'Clock running',pause:'Clock paused',step:'Advanced 4 seconds',reset:'Bin reset · manual / fermentation',scenario:'Scenario loaded · press Play or Step',speed:'Speed updated',inputs:'Sensor edits queued for acquisition'};
  $('simMessage').textContent=messages[action]||data.message;
  renderSimulation();
 }catch(error){$('simMessage').className='command-message error';$('simMessage').textContent=error.name==='AbortError'?'No response after 5 seconds. Check the current status before retrying; the change may have been applied.':error instanceof TypeError?'Cannot reach the simulator. Check that its terminal is running, then retry.':error.message;
 }finally{clearTimeout(timer);simBusy=false;renderSimulation();}
}
for(const [id,action] of [['simPlay','play'],['simPause','pause'],['simReset','reset'],['simStep','step']])$(id).addEventListener('click',()=>simulate(action));
$('simSpeed').addEventListener('change',()=>simulate('speed',{value:$('simSpeed').value}));
panel.querySelectorAll('[data-scenario]').forEach(button=>button.addEventListener('click',()=>simulate('scenario',{value:button.dataset.scenario})));
$('simInputs').addEventListener('submit',event=>{event.preventDefault();simulate('inputs');});
function renderSimulation(){
 const data=state.meta,unavailable=!state.online||simBusy||!data?.simulation;
 panel.querySelectorAll('button,select,input').forEach(control=>control.disabled=unavailable);
 panel.setAttribute('aria-busy',String(simBusy));
 if(!data?.simulation)return;
 const seconds=Math.floor(data.uptime/1000);$('simClock').textContent=`${data.playback.playing?'Running':'Paused'} · ${Math.floor(seconds/60)}m ${seconds%60}s · ${data.playback.speed}×`;
 $('simPlay').disabled=unavailable||data.playback.playing;$('simPause').disabled=unavailable||!data.playback.playing;$('simStep').disabled=unavailable||data.playback.playing;$('simSpeed').value=data.playback.speed;
 if(!simEdited){data.inputs.forEach((v,i)=>{$('simValue'+i).value=Number(v.toFixed(inputSpecs[i][4]===.1?1:0));$('simMissing'+i).checked=Boolean(data.missingMask&(1<<i));});$('simFrozen').checked=data.frozen;}
 const log=$('simEvents');log.replaceChildren();for(const [time,text] of [...data.events].reverse()){const item=document.createElement('li');item.textContent=`${(time/1000).toFixed(2)}s · ${text}`;log.append(item);}
}
setInterval(renderSimulation,300);
document.addEventListener('monitor-state',renderSimulation);
renderSimulation();
