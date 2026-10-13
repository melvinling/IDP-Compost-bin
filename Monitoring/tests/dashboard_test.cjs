const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const taskTemp=path.resolve(__dirname,'../.build/tmp');fs.mkdirSync(taskTemp,{recursive:true});
process.env.TEMP=taskTemp;process.env.TMP=taskTemp;
let playwright;
try{playwright=require('playwright');}
catch(error){if(!process.env.PLAYWRIGHT_MODULE)throw Error('Install dev dependencies with npm install, or set PLAYWRIGHT_MODULE to an installed playwright module');playwright=require(process.env.PLAYWRIGHT_MODULE);}
const root=path.resolve(__dirname,'..'),out=path.join(root,'.build','dashboard');
fs.mkdirSync(out,{recursive:true});
const executable=process.env.BROWSER_EXECUTABLE||[
 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
 'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
 'C:/Program Files/Google/Chrome/Application/chrome.exe'
].find(fs.existsSync);
const off={motor:false,pump:false,fan:false};
const initialStatus=()=>({mode:'manual',stage:'fermentation',stageElapsedMs:0,periodicMixDue:false,postWaterMixDue:false,cooldownMs:{motor:0,pump:0,fan:0},stopLatched:false,relayConfigured:false,requested:{...off},commanded:{...off},reason:'Physical relay control disabled',prediction:{modelReady:false,command:{...off},reason:'No trained model installed'}});
const fixtureData=(boot='first',seq=5)=>({
 schemaVersion:2,sampleMs:4000,freshMs:12000,boot,controlToken:'test-token',uptime:10000,reset:false,sequence:seq,more:false,next:seq,status:initialStatus(),
 latest:{sequence:seq,uptime:8000,readings:[100,0,7,0,50,28].map(value=>({value,valid:true,status:'Live',lastSuccessMs:8000,ageMs:2000,exceptionCode:0}))},
 samples:Array.from({length:5},(_,i)=>[i+1,i*4000,100-i,i===2?null:0,7,0,50,28])
});
(async()=>{
 const browser=await playwright.chromium.launch({headless:true,...(executable?{executablePath:executable}:{})});
 try{
  const context=await browser.newContext({viewport:{width:1200,height:1000},acceptDownloads:true});
  const page=await context.newPage(),errors=[],requests=[],commands=[];let fixture=fixtureData(),offline=false;
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('http://esp32.test/**',async route=>{
   const request=route.request(),url=new URL(request.url());
   if(url.pathname==='/api/data'){
    if(offline){await route.abort();return;}
    requests.push(url.searchParams.get('after'));
    const reset=url.searchParams.get('boot')!==fixture.boot,after=reset?0:Number(url.searchParams.get('after'));
    await route.fulfill({contentType:'application/json',body:JSON.stringify({...fixture,reset,samples:fixture.samples.filter(s=>s[0]>after)})});
   }else if(url.pathname==='/api/control'){
    assert.equal(request.method(),'POST');assert.equal(request.headers()['x-control-token'],'test-token');
    const command=new URLSearchParams(request.postData()).get('command');commands.push(command);
    let accepted=true,message='Accepted';
    if(command.endsWith('_start')){accepted=false;message='Physical relay control disabled';}
    else if(command==='stop_all'){fixture.status.stopLatched=true;fixture.status.mode='manual';fixture.status.reason='Software Stop All latched';}
    else if(command==='reset_stop'){fixture.status=initialStatus();}
    else if(command==='mode_auto')fixture.status.mode='auto';
    else if(command==='mode_manual')fixture.status.mode='manual';
    await route.fulfill({status:accepted?200:409,contentType:'application/json',body:JSON.stringify({accepted,message,status:fixture.status})});
   }else{
    const file=url.pathname==='/'?'index.html':url.pathname.slice(1);
    assert(['index.html','style.css','app.js'].includes(file),'Unexpected external asset: '+file);
    await route.fulfill({contentType:file.endsWith('.css')?'text/css':file.endsWith('.js')?'text/javascript':'text/html',body:fs.readFileSync(path.join(root,'web',file),'utf8')});
   }
  });
  await page.goto('http://esp32.test/');await page.waitForFunction(()=>document.getElementById('connection').textContent==='Connected to ESP32');
  assert.equal(await page.locator('.card').count(),6);assert.equal(await page.locator('#value1').textContent(),'0');assert.equal(await page.locator('#value3').textContent(),'0');
  assert.equal(await page.locator('.unit').nth(1).textContent(),'\u00b5S/cm');
  assert(await page.locator('#motorStart').isDisabled());assert(await page.locator('#model').textContent().then(s=>s.includes('No trained model installed')));
  const segments=await page.evaluate(()=>{const ctx=document.getElementById('plot1').getContext('2d'),old=ctx.lineTo;let lines=0;ctx.lineTo=function(...args){lines++;return old.apply(this,args);};drawChart(1);ctx.lineTo=old;return lines;});
  assert.equal(segments,6,'4 grid lines plus 2 trend segments; null creates a gap');
  await page.screenshot({path:path.join(out,'desktop.png'),fullPage:true});
  await page.evaluate(()=>sendCommand('motor_start'));assert((await page.locator('#commandMessage').textContent()).includes('Physical relay control disabled'));
  assert((await page.locator('#motorState').textContent()).includes('Commanded relay OFF'));
  await page.locator('#autoMode').click();await page.waitForFunction(()=>document.getElementById('autoMode').getAttribute('aria-pressed')==='true');
  await page.locator('#stopAll').click();await page.waitForFunction(()=>document.getElementById('latchStatus').textContent==='STOP LATCHED');
  await page.locator('#resetStop').click();await page.waitForFunction(()=>document.getElementById('latchStatus').textContent==='Stop latch clear');
  assert((await page.locator('#modeStatus').textContent()).includes('manual'));
  await page.locator('#pause').click();
  fixture.samples.push([6,10000,42,null,7,0,50,28]);fixture.sequence=fixture.latest.sequence=6;fixture.latest.uptime=10000;
  fixture.latest.readings[0].value=42;fixture.latest.readings[1]={value:null,valid:false,status:'No RS485 reply',lastSuccessMs:8000,ageMs:0};
  await page.evaluate(data=>acceptData(data),fixture);
  assert.equal(await page.locator('#value0').textContent(),'100.0','pause freezes readings only');
  assert.equal(await page.evaluate(()=>state.samples.length),6);
  await page.locator('#pause').click();assert.equal(await page.locator('#value0').textContent(),'42.0');assert.equal(await page.locator('#value1').textContent(),'—');
  const [download]=await Promise.all([page.waitForEvent('download'),page.locator('#export').click()]);
  await download.saveAs(path.join(out,'export.csv'));const csv=fs.readFileSync(path.join(out,'export.csv'),'utf8');
  assert(csv.startsWith('time_utc,'));assert(csv.includes(',10.000,42,,7,0'));assert(csv.includes(',0.000,100,0,7,0'));
  await page.evaluate(data=>acceptData(data),fixture);assert.equal(await page.evaluate(()=>state.samples.length),6,'duplicate samples rejected');
  await page.evaluate(data=>acceptData({...data,latest:{...data.latest,readings:data.latest.readings.map(r=>({...r,ageMs:12000}))}}),fixture);assert.equal(await page.locator('#value0').textContent(),'—');
  offline=true;await page.waitForFunction(()=>document.getElementById('connection').textContent==='Connection lost',{},{timeout:8000});
  assert(await page.locator('#stopAll').isDisabled());assert.equal(await page.locator('#value0').textContent(),'—');
  offline=false;await page.waitForFunction(()=>document.getElementById('connection').textContent==='Connected to ESP32',{},{timeout:8000});
  assert.equal(await page.evaluate(()=>state.samples.length),6);assert(requests.includes('6'));
  fixture=fixtureData('restart',1);fixture.samples=[[1,0,null,0,7,0,50,28]];fixture.latest.sequence=1;
  await page.evaluate(data=>acceptData(data),fixture);assert.equal(await page.evaluate(()=>state.samples.length),1);assert.equal(await page.evaluate(()=>state.after),1);
  const many={...fixture,reset:false,samples:Array.from({length:950},(_,i)=>[i+2,i*4000,1,0,7,0,50,28])};
  await page.evaluate(data=>acceptData(data),many);assert.equal(await page.evaluate(()=>state.samples.length),900);
  fixture=fixtureData('phone',5);await page.evaluate(data=>acceptData(data),fixture);
  await page.setViewportSize({width:390,height:844});await page.waitForTimeout(200);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'No phone horizontal overflow');
  assert.equal(await page.locator('.grid').evaluate(el=>getComputedStyle(el).gridTemplateColumns.split(' ').length),1);
  await page.screenshot({path:path.join(out,'mobile.png'),fullPage:true});
  assert.deepEqual(errors,[]);assert(commands.includes('motor_start')&&commands.includes('stop_all')&&commands.includes('reset_stop'));
  console.log('PASS browser: 6 charts, null gaps, zeros, CSV download, incremental dedup, pause-only presentation, server acknowledgements, controls, stale display, reconnect/restart, bounded history and phone layout; no page errors');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
