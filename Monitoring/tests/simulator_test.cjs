'use strict';
const assert=require('node:assert/strict'),path=require('node:path'),{spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..'),port=18787,base=`http://127.0.0.1:${port}`;
const child=spawn(process.execPath,[path.join(root,'simulator/server.cjs'),'--port',String(port)],{windowsHide:true,stdio:['ignore','pipe','inherit']});
const ready=new Promise((resolve,reject)=>{const timeout=setTimeout(()=>reject(Error('Server start timeout')),15000);child.once('error',reject);child.stdout.on('data',chunk=>{if(chunk.toString().includes('ready:')){clearTimeout(timeout);resolve();}});child.once('exit',code=>{clearTimeout(timeout);reject(Error('Server exited '+code));});});
let token;
async function get(){const response=await fetch(base+'/api/data');assert.equal(response.status,200);const data=await response.json();token=data.controlToken;return data;}
async function post(route,fields,status=200){const response=await fetch(base+route,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Control-Token':token},body:new URLSearchParams(fields)});assert.equal(response.status,status);return response.json();}
const sim=(action,value)=>post('/api/simulation',{action,...(value===undefined?{}:{value})});
const command=(command,status)=>post('/api/control',{command},status);
const off=data=>assert.deepEqual(data.status.commanded,{motor:false,pump:false,fan:false});
(async()=>{
 try{
  await ready;let data=await get();assert.equal(data.schemaVersion,2);assert.equal(data.latest.readings.length,6);assert.equal(data.status.mode,'manual');assert.equal(data.status.stage,'fermentation');off(data);
  await command('fan_start',409);await command('stage_finishing');data=await command('fan_start');assert(data.status.commanded.fan);
  await command('pump_start',409);data=await sim('step');assert(data.status.commanded.fan);data=await sim('step');off(data);assert(data.status.cooldownMs.fan>0);
  data=await sim('scenario','dry');assert.equal(data.status.prediction.action,'water');assert(data.status.commanded.pump);
  data=await sim('step');assert(!data.status.commanded.pump&&data.status.commanded.motor&&data.status.postWaterMixDue);
  await sim('step');data=await sim('step');assert(!data.status.commanded.motor&&!data.status.postWaterMixDue);assert(data.status.cooldownMs.pump>0);
  assert(data.events.some(e=>e[1]==='Water pump OFF')&&data.events.some(e=>e[1]==='Mixer ON'));
  data=await sim('scenario','wet');assert(data.status.commanded.fan);await sim('step');data=await sim('step');off(data);assert.equal(data.status.cooldownMs.fan,30000);
  data=await sim('scenario','mix_due');assert(data.status.periodicMixDue&&data.status.commanded.motor);await sim('step');data=await sim('step');assert(!data.status.periodicMixDue&&!data.status.commanded.motor);
  data=await sim('scenario','missing');off(data);assert.equal(data.latest.readings[4].value,null);assert(!data.status.prediction.modelReady);
  data=await sim('scenario','stale');const seq=data.sequence;await sim('step');await sim('step');data=await sim('step');off(data);assert.equal(data.sequence,seq);assert.equal(data.latest.readings[0].status,'Stale reading');
  data=await sim('scenario','fermentation');off(data);assert.equal(data.status.stage,'fermentation');
  await sim('scenario','wet');data=await command('stop_all');off(data);assert(data.status.stopLatched);await command('mode_auto');data=await sim('step');off(data);data=await command('reset_stop');assert.equal(data.status.mode,'manual');off(data);
  const invalid=await fetch(base+'/api/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'command=stop_all'});assert.equal(invalid.status,403);
  await command('unknown',409);const getControl=await fetch(base+'/api/control');assert.equal(getControl.status,405);
  const invalidCursor=await fetch(base+'/api/data?after=-1');assert.equal(invalidCursor.status,400);
  const speed=await sim('speed','60');assert.equal(speed.playback.speed,60);const now=speed.uptime;await sim('play');await new Promise(resolve=>setTimeout(resolve,250));data=await sim('pause');assert(data.uptime>now&&!data.playback.playing);
  data=await sim('reset');assert.equal(data.uptime,0);assert.equal(data.status.mode,'manual');off(data);
  console.log('PASS live simulator HTTP/native engine: six readings, stage defaults, manual timers, interlock, automatic wet/dry/mix, cooldown, missing/stale, stop/reset, token/cursor validation and playback');
 }finally{child.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
