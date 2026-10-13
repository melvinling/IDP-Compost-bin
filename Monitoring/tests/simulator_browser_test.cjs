// Operator journeys: visible UI and downloaded CSV; no application-state assertions.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..'),temp=path.join(root,'.build/tmp'),out=path.join(root,'.build/user-acceptance');
fs.mkdirSync(temp,{recursive:true});fs.mkdirSync(out,{recursive:true});process.env.TEMP=temp;process.env.TMP=temp;
let playwright;try{playwright=require('playwright');}catch(e){playwright=require(process.env.PLAYWRIGHT_MODULE);}
let assertions;try{assertions=require('playwright/test');}catch(e){assertions=require(path.join(process.env.PLAYWRIGHT_MODULE,'test'));}
const expect=assertions.expect.configure({timeout:7000});
const port=18788,base=`http://127.0.0.1:${port}`,results=[];
let server,browser,page;
async function startServer(){
 server=spawn(process.execPath,[path.join(root,'simulator/server.cjs'),'--port',String(port)],{windowsHide:true,stdio:['ignore','pipe','inherit']});
 await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Server readiness timeout')),15000);server.once('error',reject);server.stdout.on('data',chunk=>{if(chunk.toString().includes('ready:')){clearTimeout(timer);resolve();}});server.once('exit',code=>{clearTimeout(timer);reject(Error('Server exited '+code));});});
}
async function stopServer(){if(!server||server.exitCode!==null)return;await new Promise(resolve=>{server.once('exit',resolve);server.kill();});}
async function journey(name,steps,expected,work){
 try{await work();results.push({name,steps,expected,actual:'Passed the visible UI assertions',result:'PASS'});console.log('PASS user journey: '+name);}
 catch(error){results.push({name,steps,expected,actual:error.message,result:'FAIL'});await page?.screenshot({path:path.join(out,'failure.png'),fullPage:true}).catch(()=>{});throw error;}
 finally{fs.writeFileSync(path.join(out,'browser-results.json'),JSON.stringify({date:new Date().toISOString(),browser:'Installed Microsoft Edge',results},null,2));}
}
const button=name=>page.getByRole('button',{name,exact:true});
const message=()=>page.locator('#simMessage');
async function simClick(name,text){await Promise.all([page.waitForResponse(r=>new URL(r.url()).pathname==='/api/simulation'),button(name).click()]);await expect(message()).toHaveText(text);await expect(page.locator('.simulation')).toHaveAttribute('aria-busy','false');}
async function scenario(name){await simClick(name,'Scenario loaded · press Play or Step');}
async function step(){await simClick('Step 4 seconds','Advanced 4 seconds');}
async function control(name){await Promise.all([page.waitForResponse(r=>new URL(r.url()).pathname==='/api/control'),button(name).click()]);}
async function off(){for(const name of ['motor','pump','fan'])await expect(page.locator('#'+name+'State')).toContainText('Commanded relay OFF');}
const value=i=>page.locator('#value'+i);
const readingStatus=i=>page.locator('#status'+i);
const clock=()=>page.locator('#simClock');
async function speed(n){await Promise.all([page.waitForResponse(r=>new URL(r.url()).pathname==='/api/simulation'),page.getByLabel('Speed',{exact:true}).selectOption(String(n))]);await expect(message()).toHaveText('Speed updated');await expect(clock()).toContainText(n+'×');}
async function inputs(){await simClick('Apply sensor inputs','Sensor edits queued for acquisition');}
async function details(){const summary=page.getByText('Adjust sensors and failures',{exact:true});if(!await summary.locator('..').getAttribute('open').then(v=>v!==null))await summary.click();}
(async()=>{
 const errors=[];
 try{
  await startServer();browser=await playwright.chromium.launch({headless:true,executablePath:process.env.BROWSER_EXECUTABLE||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'});
  const context=await browser.newContext({viewport:{width:1200,height:1000},acceptDownloads:true,locale:'en-US'});page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
  await page.goto(base);
  await journey('First screen','Open the simulator without selecting a scenario.','Connected, manual/fermentation, six measurements and all outputs OFF.',async()=>{
   await expect(page.getByRole('heading',{name:'Bokashi simulator',exact:true})).toBeVisible();await expect(page.locator('#connection')).toHaveText('Local simulator connected');
   await expect(page.locator('.card')).toHaveCount(6);await expect(page.locator('#modeStatus')).toContainText('manual');await expect(page.locator('#stageStatus')).toContainText('fermentation');await off();
   await expect(page.getByText('No hardware connected',{exact:true})).toBeVisible();await expect(page.locator('.simulation .notice')).toContainText('Synthetic Random Forest');await expect(button('Start · 5 seconds').first()).toBeDisabled();
   await page.screenshot({path:path.join(out,'first-screen.png'),fullPage:true});
  });
  await journey('Too dry','Select Too dry; step the paused clock three times.','Pump turns OFF at 2s, mixing starts, finishes at 9s, and cooldown remains.',async()=>{
   await scenario('Too dry');await expect(page.locator('#pumpState')).toContainText('Commanded relay ON');await expect(page.locator('#model')).toContainText('water');
   await step();await expect(page.locator('#pumpState')).toContainText('Commanded relay OFF');await expect(page.locator('#motorState')).toContainText('Commanded relay ON');await expect(page.locator('#simEvents')).toContainText('2.00s · Water pump OFF');
   await step();await step();await off();await expect(page.locator('#simEvents')).toContainText('9.00s · Mixer OFF');await expect(page.locator('#pumpState')).toContainText('Cooldown 50s');
  });
  await journey('Too wet','Select Too wet; step twice.','Fan runs eight seconds; pump stays OFF and fan cooldown is 30s.',async()=>{
   await scenario('Too wet');await expect(page.locator('#fanState')).toContainText('Commanded relay ON');await expect(page.locator('#pumpState')).toContainText('Commanded relay OFF');await step();await step();await off();
   await expect(page.locator('#fanState')).toContainText('Cooldown 30s');await expect(page.locator('#simEvents')).toContainText('8.00s · Drying fan OFF');
  });
  await journey('Balanced, mix due and fermentation','Select each scenario and advance the mix scenario.','Balanced stays OFF; due mixing completes; fermentation stays OFF.',async()=>{
   await scenario('Balanced');await off();await expect(page.locator('#model')).toContainText('idle');
   await scenario('Mix due');await expect(page.locator('#motorState')).toContainText('Commanded relay ON');await expect(page.locator('#stageStatus')).toContainText('Periodic mix due');await step();await step();await off();
   await expect(page.locator('#stageStatus')).toContainText('Mix timer running');await scenario('Fermentation');await off();await expect(page.locator('#stageStatus')).toContainText('fermentation');
  });
  await journey('Manual operation','Reset; select finishing/manual; start pump, stop it, try fan during cooldown; test all manual starts.','Five-second expiry, cooldown and fan/pump interlock are acknowledged.',async()=>{
   await simClick('Reset bin','Bin reset · manual / fermentation');await control('Finishing');
   await page.locator('#pumpStart').click();await expect(page.locator('#pumpState')).toContainText('Commanded relay ON');
   await page.locator('#fanStart').click();await expect(page.locator('#commandMessage')).toContainText('Fan and pump are mutually exclusive');await expect(page.locator('#fanState')).toContainText('Commanded relay OFF');
   await step();await step();await off();await expect(page.locator('#simEvents')).toContainText('5.00s · Water pump OFF');
   await page.locator('#pumpStart').click();await expect(page.locator('#commandMessage')).toHaveText('Actuator cooldown active');
   await page.locator('#fanStart').click();await expect(page.locator('#fanState')).toContainText('Commanded relay ON');await step();await step();await off();
   await page.locator('#motorStart').click();await expect(page.locator('#motorState')).toContainText('Commanded relay ON');await control('Stop motor');await off();
  });
  await journey('Stop All and reset','Start a wet scenario, Stop All, select Automatic, step, then Reset stop latch.','Latch prevents restarts; reset leaves manual/OFF.',async()=>{
   await scenario('Too wet');await control('Stop All');await expect(page.locator('#latchStatus')).toHaveText('STOP LATCHED');await off();await control('Automatic');await step();await off();
   await control('Reset stop latch');await expect(page.locator('#latchStatus')).toHaveText('Stop latch clear');await expect(page.locator('#modeStatus')).toContainText('manual');await off();
  });
  await journey('Sensor editing and invalid entries','Edit six sensor inputs; apply then step; try a blank and moisture 101.','Values update only on acquisition; invalid forms do not reach the server.',async()=>{
   await scenario('Balanced');await details();
   const labels=['Distance (cm)','EC (µS/cm)','pH (pH)','CO₂ (ppm)','Moisture (%)','Temperature (°C)'],chosen=['80','1234','6.2','1200','50','31.5'];
   for(let i=0;i<labels.length;i++)await page.getByLabel(labels[i],{exact:true}).fill(chosen[i]);
   await inputs();await expect(value(0)).toHaveText('100.0');await step();
   for(const [i,text] of ['80.0','1234','6.2','1200','50.0','31.5'].entries())await expect(value(i)).toHaveText(text);
   let requests=0;const listener=req=>{if(new URL(req.url()).pathname==='/api/simulation')requests++;};page.on('request',listener);
   const ec=page.getByLabel('EC (µS/cm)',{exact:true});await ec.fill('');await button('Apply sensor inputs').click();await expect(ec).toBeFocused();assert(await ec.evaluate(el=>Boolean(el.validationMessage)));
   await ec.fill('1234');const moisture=page.getByLabel('Moisture (%)',{exact:true});await moisture.fill('101');await button('Apply sensor inputs').click();await expect(moisture).toBeFocused();assert(await moisture.evaluate(el=>Boolean(el.validationMessage)));assert.equal(requests,0);page.off('request',listener);await moisture.fill('50');
  });
  await journey('Missing and frozen readings','Test each of six missing checkboxes; then select Frozen acquisition and step 12s.','Failures are clearly labelled, values hidden, automatic outputs OFF.',async()=>{
   const labels=['Distance','EC','pH','CO₂','Moisture','Temperature'];
   for(let i=0;i<6;i++){await scenario('Balanced');await page.getByRole('checkbox',{name:labels[i]+' missing',exact:true}).check();await inputs();await step();await expect(readingStatus(i)).toHaveText('No RS485 reply');await expect(value(i)).toHaveText('—');await off();}
   await scenario('Frozen acquisition');await step();await step();await step();for(let i=0;i<6;i++){await expect(readingStatus(i)).toHaveText('Stale reading');await expect(value(i)).toHaveText('—');}await off();
  });
  await journey('Clock and display controls','Use Play/Pause at 1x, 10x, 60x; pause display while clock advances; resume; step while paused.','Clock pause freezes time; display pause freezes readings while time continues.',async()=>{
   await scenario('Too wet');for(const n of [1,10,60]){await speed(n);await simClick('Play','Clock running');await expect(clock()).toContainText('Running');await expect(button('Step 4 seconds')).toBeDisabled();await page.waitForTimeout(n===1?1100:250);await simClick('Pause clock','Clock paused');await expect(clock()).toContainText('Paused');const text=await clock().textContent();await page.waitForTimeout(200);await expect(clock()).toHaveText(text);}
   await simClick('Play','Clock running');await button('Pause display').click();const frozen=await value(4).textContent(),before=await clock().textContent();await page.waitForTimeout(1000);await expect(value(4)).toHaveText(frozen);assert.notEqual(await clock().textContent(),before);await button('Resume display').click();await expect(value(4)).not.toHaveText(frozen);await simClick('Pause clock','Clock paused');await step();
  });
  await journey('Reset and scenario while display paused','Pause a wet display and Reset bin; pause again and select Too dry.','Previous readings disappear; live display resumes with the new bin.',async()=>{
   await scenario('Too wet');await expect(value(4)).toHaveText('80.0');await button('Pause display').click();await simClick('Reset bin','Bin reset · manual / fermentation');await expect(value(4)).toHaveText('50.0');await expect(button('Pause display')).toBeVisible();
   await button('Pause display').click();await scenario('Too dry');await expect(value(4)).toHaveText('25.0');await expect(button('Pause display')).toBeVisible();
  });
  await journey('Pending requests, errors and timeout','Hold a Step request; use Stop All; wait for 5s timeout; inject a rejected request; retry normally.','Progress is visible, duplicates disabled, Stop All works, and controls recover.',async()=>{
   await scenario('Balanced');let release,requests=0;const held=new Promise(resolve=>release=resolve);
   await page.route('**/api/simulation',async route=>{requests++;await held;await route.abort().catch(()=>{});});
   await button('Step 4 seconds').click();await expect(message()).toHaveText('Applying simulation change…');await expect(button('Reset bin')).toBeDisabled();await expect(button('Too dry')).toBeDisabled();await expect(button('Stop All')).toBeEnabled();
   await control('Stop All');await expect(page.locator('#latchStatus')).toHaveText('STOP LATCHED');await expect(message()).toContainText('No response after 5 seconds');assert.equal(requests,1);release();await page.unroute('**/api/simulation');await expect(button('Step 4 seconds')).toBeEnabled();
   await page.route('**/api/simulation',route=>route.fulfill({status:409,contentType:'application/json',body:JSON.stringify({error:'Simulation change rejected'})}));
   await button('Step 4 seconds').click();await expect(message()).toHaveText('Simulation change rejected');await expect(button('Step 4 seconds')).toBeEnabled();await page.unroute('**/api/simulation');await step();
  });
  await journey('Disconnect and restart','Stop the actual server, observe disabled controls, restart it without reloading the page.','Recovery message, disabled actions, automatic reconnection and fresh defaults.',async()=>{
   await stopServer();await expect(page.locator('#connection')).toHaveText('Connection lost');await expect(page.locator('#sensorNotice')).toContainText('terminal is running');
   for(const name of ['Play','Step 4 seconds','Reset bin','Too dry','Stop All','Apply sensor inputs'])await expect(button(name)).toBeDisabled();await expect(page.getByLabel('Speed',{exact:true})).toBeDisabled();await expect(value(4)).toHaveText('—');
   await startServer();await expect(page.locator('#connection')).toHaveText('Local simulator connected');await expect(value(4)).toHaveText('50.0');await expect(page.locator('#modeStatus')).toContainText('manual');await expect(button('Reset bin')).toBeEnabled();await off();
  });
  await journey('CSV export','Acquire normal readings, mark moisture missing, acquire again, download CSV.','Eight columns: time/uptime and six measurements; missing moisture blank.',async()=>{
   await scenario('Balanced');await step();await page.getByRole('checkbox',{name:'Moisture missing',exact:true}).check();await inputs();await step();
   const [download]=await Promise.all([page.waitForEvent('download'),button('Download CSV').click()]);assert.equal(download.suggestedFilename(),'compost-readings.csv');await download.saveAs(path.join(out,'export.csv'));
   const rows=fs.readFileSync(path.join(out,'export.csv'),'utf8').trim().split(/\r?\n/).map(line=>line.split(','));assert.equal(rows[0].join(','),'time_utc,uptime_seconds,distance_cm,ec_uS_cm,pH,co2_ppm,moisture_pct,temperature_c');assert(rows.length>=4);for(const row of rows.slice(1)){assert.equal(row.length,8);assert(Number.isFinite(Date.parse(row[0])));}assert.equal(rows.at(-1)[6],'');assert.equal(rows.at(-1)[1],'8.000');
  });
  await journey('Refresh and shared tabs','Reload; open a second tab; choose Too wet there and observe the first tab.','History reloads; both tabs show the same virtual bin.',async()=>{
   await page.reload();await expect(page.locator('#connection')).toHaveText('Local simulator connected');await expect(readingStatus(4)).toHaveText('No RS485 reply');
   const second=await context.newPage();await second.goto(base);await expect(second.locator('#connection')).toHaveText('Local simulator connected');await second.getByRole('button',{name:'Too wet',exact:true}).click();await expect(page.locator('#fanState')).toContainText('Commanded relay ON');await expect(value(4)).toHaveText('80.0');await second.close();
  });
  await journey('Keyboard, mobile and zoom','Tab and Enter on clock controls; inspect 390px mobile and 200% zoom layout.','Visible focus, labelled fields, operable keyboard controls, no horizontal clipping.',async()=>{
   await page.reload();await expect(page.locator('#connection')).toHaveText('Local simulator connected');await page.keyboard.press('Tab');await expect(button('Play')).toBeFocused();assert(await button('Play').evaluate(el=>getComputedStyle(el).outlineStyle!=='none'));
   await page.keyboard.press('Enter');await expect(clock()).toContainText('Running');await page.keyboard.press('Tab');await expect(button('Pause clock')).toBeFocused();await page.keyboard.press('Enter');await expect(clock()).toContainText('Paused');
   await page.screenshot({path:path.join(out,'desktop.png'),fullPage:true});
   await page.setViewportSize({width:390,height:844});await page.waitForTimeout(200);assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
   for(const control of await page.locator('.simulation button, .control [data-command]').all()){const box=await control.boundingBox();if(box)assert(box.x>=0&&box.x+box.width<=390.5);}
   await page.screenshot({path:path.join(out,'mobile.png'),fullPage:true});
   await page.setViewportSize({width:1200,height:1000});await page.evaluate(()=>document.body.style.zoom='2');await page.waitForTimeout(200);assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
   await page.screenshot({path:path.join(out,'zoom-200.png'),fullPage:true});await page.evaluate(()=>document.body.style.zoom='');assert.deepEqual(errors,[]);
  });
  console.log('All '+results.length+' operator journeys passed. Evidence: '+out);
 }finally{if(browser)await browser.close();await stopServer();}
})().catch(error=>{console.error(error);process.exitCode=1;});
