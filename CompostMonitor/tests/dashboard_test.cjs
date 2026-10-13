const fs=require('fs');
const path=require('path');
const assert=require('node:assert/strict');
const {chromium}=require('C:/Users/Acer/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const header=fs.readFileSync(path.join(__dirname,'../Dashboard.h'),'utf8');
const html=header.split('R"COMPOSTHTML(')[1].split(')COMPOSTHTML";')[0];
const out=path.resolve(__dirname,'../../.build/dashboard-check');
fs.mkdirSync(out,{recursive:true});
fs.writeFileSync(path.join(out,'index.html'),html);
const executable=[
 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
 'C:/Program Files/Google/Chrome/Application/chrome.exe',
 'C:/Program Files/Microsoft/Edge/Application/msedge.exe'
].find(fs.existsSync);

(async()=>{
 const browser=await chromium.launch({headless:true,...(executable?{executablePath:executable}:{})});
 try{
  const context=await browser.newContext({viewport:{width:1200,height:950},acceptDownloads:true});
  const page=await context.newPage();
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const samples=Array.from({length:20},(_,i)=>[i+1,i*2000,67.5-i*.2,1000+i*5,5.6+i*.01,536+i*2]);
  let fixture={boot:'a',sequence:20,uptime:39000,monitoring:true,soilReady:true,co2Ready:true,status:['Live','Live','Live','Live'],samples};
  let requests=[];
  await page.route('http://esp32.test/**',async route=>{
   const url=new URL(route.request().url());
   if(url.pathname==='/api/data'){
    requests.push(url.searchParams.get('after'));
    const after=url.searchParams.get('boot')===fixture.boot?Number(url.searchParams.get('after')):0;
    await route.fulfill({contentType:'application/json',body:JSON.stringify({...fixture,samples:fixture.samples.filter(s=>s[0]>after)})});
   }else await route.fulfill({contentType:'text/html',body:html});
  });
  await page.goto('http://esp32.test/');
  await page.waitForFunction(()=>document.getElementById('connectionText').textContent==='Connected to ESP32');
  assert.equal(await page.locator('.card').count(),4);
  assert.equal(await page.locator('#value0').textContent(),'63.7');
  assert.equal(await page.locator('#value3').textContent(),'574');
  assert.equal(await page.locator('#notice').isVisible(),false);
  await page.screenshot({path:path.join(out,'desktop.png'),fullPage:true});

  const missing={...fixture,sequence:21,uptime:42000,status:['Live','No reply','No reply','No reply'],samples:[[21,42000,30,null,null,null]]};
  await page.evaluate(data=>acceptData(data),missing);
  assert.equal(await page.locator('#value1').textContent(),'—');
  assert.equal(await page.evaluate(()=>state.samples.at(-1)[3]),null);
  assert.equal(await page.locator('#value0').textContent(),'30.0');

  await page.locator('#pause').click();
  await page.evaluate(data=>acceptData(data),{...missing,sequence:22,uptime:44000,samples:[[22,44000,60,900,6,600]]});
  assert.equal(await page.locator('#value0').textContent(),'30.0');
  await page.locator('#pause').click();
  assert.equal(await page.locator('#value0').textContent(),'60.0');

  const downloadPromise=page.waitForEvent('download');await page.locator('#export').click();
  const download=await downloadPromise;await download.saveAs(path.join(out,'export.csv'));
  const csv=fs.readFileSync(path.join(out,'export.csv'),'utf8');
  assert(csv.startsWith('time_utc,uptime_seconds,distance_cm,ec_uS_cm,pH,co2_ppm'));
  assert(csv.split('\r\n').some(row=>row.endsWith(',42.000,30,,,')));

  await page.evaluate(()=>markOffline());
  assert.equal(await page.locator('#connectionText').textContent(),'Connection lost');
  assert.equal(await page.locator('#value0').textContent(),'—');
  assert.equal(await page.evaluate(()=>state.samples.length),22);
  await page.locator('#pause').click();await page.locator('#pause').click();
  assert.equal(await page.locator('#connectionText').textContent(),'Connection lost');
  assert.equal(await page.locator('#value0').textContent(),'—');
  await page.evaluate(data=>acceptData(data),{...missing,sequence:23,uptime:46000,samples:[[23,46000,50,900,6,600]]});
  assert.equal(await page.locator('#value0').textContent(),'50.0');

  fixture={boot:'b',sequence:1,uptime:1000,monitoring:false,soilReady:false,co2Ready:false,status:['Live','Monitoring paused','Monitoring paused','Monitoring paused'],samples:[[1,1000,25,null,null,null]]};
  await page.evaluate(data=>acceptData(data),fixture);
  assert.equal(await page.evaluate(()=>state.samples.length),1);
  assert.equal(await page.locator('#value0').textContent(),'25.0');
  assert(await page.locator('#notice').isVisible());
  // A real zero remains a number; a missing sensor remains null.
  await page.evaluate(data=>acceptData(data),{...fixture,sequence:2,uptime:3000,samples:[[2,3000,30,0,null,0]]});
  assert.equal(await page.locator('#value1').textContent(),'0');
  assert.equal(await page.locator('#value2').textContent(),'—');
  await page.evaluate(data=>acceptData(data),{...fixture,sequence:2,uptime:3000,samples:[[2,3000,30,0,null,0]]});
  assert.equal(await page.evaluate(()=>state.samples.length),2);
  await page.setViewportSize({width:390,height:844});
  await page.waitForTimeout(150);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  await page.screenshot({path:path.join(out,'mobile.png'),fullPage:true});
  await page.waitForTimeout(2200);
  assert(requests.length>=2);
  // A pH timeout must not hide valid EC/CO2 or label their cards as failed.
  await page.evaluate(data=>acceptData(data),{...fixture,monitoring:true,sequence:3,uptime:5000,co2Ready:true,status:['Live','Live','No reply','Live'],samples:[[3,5000,30,1100,null,500]]});
  assert.equal(await page.locator('#value1').textContent(),'1100');
  assert.equal(await page.locator('#value2').textContent(),'—');
  assert.equal(await page.locator('#value3').textContent(),'500');
  assert.equal(await page.locator('#status1').textContent(),'Live');
  assert.equal(await page.locator('#status2').textContent(),'No reply');
  assert.equal(await page.locator('#notice').isVisible(),false);
  assert.deepEqual(errors,[]);
  console.log('Dashboard tests passed with simulated API readings: live values, missing versus zero readings, duplicate prevention, pause, CSV, reconnect, reboot, mobile layout, incremental polling; no browser errors.');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
