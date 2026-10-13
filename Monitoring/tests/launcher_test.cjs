'use strict';
// Executes the shipped Windows CMD/PowerShell launcher from a different folder.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),http=require('node:http'),{spawn,spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),out=path.join(root,'.build/user-acceptance'),fixtures=path.join(out,'launcher fixtures'),clone=path.join(fixtures,'workspace with spaces','Monitoring'),external=path.join(fixtures,'outside working directory');
fs.mkdirSync(clone,{recursive:true});fs.mkdirSync(external,{recursive:true});
for(const folder of ['src','simulator','web'])fs.cpSync(path.join(root,folder),path.join(clone,folder),{recursive:true});
fs.copyFileSync(path.join(root,'Start Simulator.cmd'),path.join(clone,'Start Simulator.cmd'));
const powershell=path.join(process.env.SystemRoot,'System32/WindowsPowerShell/v1.0/powershell.exe'),port=18790,url=`http://127.0.0.1:${port}/`,results=[],owned=new Set();
const quote=s=>"'"+s.replace(/'/g,"''")+"'";
function launch({cmd=false,noBrowser=true,env=process.env}={}){
 const target=path.join(clone,cmd?'Start Simulator.cmd':'simulator/start.ps1');
 const args=cmd?['-NoProfile','-ExecutionPolicy','Bypass','-Command',`& ${quote(target)} ${noBrowser?'-NoBrowser ':''}-Port ${port}; exit $LASTEXITCODE`]:['-NoProfile','-ExecutionPolicy','Bypass','-File',target,'-Port',String(port),...(noBrowser?['-NoBrowser']:[])];
 const child=spawn(powershell,args,{cwd:external,env,windowsHide:true,stdio:['pipe','pipe','pipe']});owned.add(child);child.log='';
 for(const stream of [child.stdout,child.stderr])stream.on('data',chunk=>{child.log+=chunk.toString();});child.once('exit',()=>owned.delete(child));return child;
}
async function until(child,text,timeout=30000){const end=Date.now()+timeout;while(Date.now()<end){if(child.log.includes(text))return;if(child.exitCode!==null)throw Error(child.log);await new Promise(resolve=>setTimeout(resolve,50));}throw Error('Timed out: '+text+'\n'+child.log);}
async function exit(child){if(child.exitCode===null)await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Launcher did not exit\n'+child.log)),15000);child.once('exit',()=>{clearTimeout(timer);resolve();});});return child.exitCode;}
async function stop(child){if(!child||child.exitCode!==null)return;await new Promise(resolve=>{const killer=spawn('taskkill.exe',['/PID',String(child.pid),'/T','/F'],{windowsHide:true,stdio:'ignore'});killer.once('exit',resolve);});await exit(child);}
async function check(name,steps,expected,work){try{const actual=await work();results.push({name,steps,expected,actual:actual||'Passed',result:'PASS'});console.log('PASS launcher: '+name);}catch(error){results.push({name,steps,expected,actual:error.message,result:'FAIL'});throw error;}finally{fs.writeFileSync(path.join(out,'launcher-results.json'),JSON.stringify({date:new Date().toISOString(),results},null,2));}}
(async()=>{
 let running,occupied;
 try{
  await check('CMD startup from another folder with spaces','Execute the shipped CMD launcher from an unrelated directory, using -NoBrowser.','Native build succeeds, HTTP page is usable, no browser opener is run.',async()=>{
   running=launch({cmd:true});await until(running,'ready:');const response=await fetch(url);assert.equal(response.status,200);assert((await response.text()).includes('<h1>Bokashi simulator</h1>'));assert(!running.log.includes('Browser opened'));return 'Actual CMD launch built the engine and served HTTP 200; working-directory and space handling passed.';
  });
  await check('Relaunch reuses the existing bin','Run the launcher again while its engine executable is in use.','Exit successfully without rebuilding, resetting or starting a conflicting server.',async()=>{
   const before=await (await fetch(url+'api/data')).json();const reuse=launch();assert.equal(await exit(reuse),0);assert(reuse.log.includes('already running'));assert(!reuse.log.includes('Preparing your virtual bin'));const after=await (await fetch(url+'api/data')).json();assert.equal(before.boot,after.boot);return 'Existing instance reused; boot ID unchanged; no rebuild attempted.';
  });
  fs.writeFileSync(path.join(out,'launcher-startup.log'),running.log);await stop(running);
  await check('Default browser opens after readiness','Execute the CMD launcher without -NoBrowser.','HTTP is ready before Windows accepts the default-browser open command.',async()=>{
   running=launch({cmd:true,noBrowser:false});await until(running,'Browser opened.');assert(running.log.indexOf('ready:')<running.log.indexOf('Browser opened.'));assert.equal((await fetch(url)).status,200);fs.writeFileSync(path.join(out,'launcher-browser-open.log'),running.log);return 'Windows browser opener exited successfully after server readiness; HTTP 200 confirmed.';
  });
  await stop(running);
  const systemPaths=[path.dirname(powershell),path.join(process.env.SystemRoot,'System32'),process.env.SystemRoot].join(';');
  await check('Missing Node.js','Launch with a controlled PATH that excludes Node.js.','Clear Node.js/PATH guidance and nonzero exit.',async()=>{
   const child=launch({env:{...process.env,PATH:systemPaths}});assert.equal(await exit(child),1);assert(child.log.includes('Node.js was not found'));fs.writeFileSync(path.join(out,'launcher-missing-node.log'),child.log);return 'Node.js/PATH instructions shown; exit 1.';
  });
  await check('Missing C++ compiler','Launch with Node.js available but g++ absent from PATH.','Clear compiler/PATH guidance and nonzero exit.',async()=>{
   const child=launch({env:{...process.env,PATH:systemPaths+';'+path.dirname(process.execPath)}});assert.equal(await exit(child),1);assert(child.log.includes('g++ compiler was not found'));fs.writeFileSync(path.join(out,'launcher-missing-compiler.log'),child.log);return 'Compiler bin-folder/PATH instructions shown; exit 1.';
  });
  await check('Build failure','Use an isolated failing compiler fixture without altering the real compiler.','Compiler output and actionable build-failure message; no server starts.',async()=>{
   const fake=path.join(fixtures,'failing compiler');fs.mkdirSync(fake,{recursive:true});const source=path.join(fake,'fixture.cpp');fs.writeFileSync(source,'#include <stdio.h>\nint main(){puts("Intentional compiler failure for launcher test");return 9;}\n');
   const compiled=spawnSync('g++',[source,'-o',path.join(fake,'g++.exe')],{windowsHide:true,encoding:'utf8'});assert.equal(compiled.status,0,compiled.stderr);
   const child=launch({env:{...process.env,PATH:fake+';'+process.env.PATH}});assert.equal(await exit(child),1);assert(child.log.includes('Intentional compiler failure'));assert(child.log.includes('C++ engine build failed'));assert(child.log.includes('reopen the launcher'));fs.writeFileSync(path.join(out,'launcher-build-failure.log'),child.log);return 'Intentional compiler failure reported with repair/retry instructions; exit 1.';
  });
  await check('Occupied port','Start an unrelated HTTP fixture on the requested port, then launch.','Clear port conflict and alternative-port instruction; fixture stays running.',async()=>{
   occupied=http.createServer((req,res)=>res.end('Unrelated application'));await new Promise(resolve=>occupied.listen(port,'127.0.0.1',resolve));const child=launch();assert.equal(await exit(child),1);assert(child.log.includes('used by another application'));assert(child.log.includes('-Port 8788'));assert.equal(await (await fetch(url)).text(),'Unrelated application');fs.writeFileSync(path.join(out,'launcher-port-conflict.log'),child.log);return 'Port conflict explained with -Port 8788; unrelated application untouched.';
  });
  console.log('All '+results.length+' Windows launcher checks passed.');
 }finally{for(const child of [...owned])await stop(child);if(occupied)await new Promise(resolve=>occupied.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
