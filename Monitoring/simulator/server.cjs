'use strict';
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process'),{createInterface}=require('node:readline'),{randomBytes}=require('node:crypto');
const root=path.resolve(__dirname,'..'),portIndex=process.argv.indexOf('--port'),port=portIndex<0?8787:Number(process.argv[portIndex+1]);
if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Port must be 1024–65535');
const engine=path.join(root,'.build','simulator',process.platform==='win32'?'engine.exe':'engine');
if(!fs.existsSync(engine))throw Error('Build first: powershell -File simulator/build.ps1');
const child=spawn(engine,[],{stdio:['pipe','pipe','inherit'],windowsHide:true});
let snapshot=null,boot=randomBytes(8).toString('hex'),token=randomBytes(16).toString('hex'),playing=false,speed=1,closed=false;
const pending=[];let serial=Promise.resolve();
createInterface({input:child.stdout}).on('line',line=>{const job=pending.shift();if(!job)return;try{snapshot=JSON.parse(line);job.resolve(snapshot);}catch(e){job.reject(e);}});
child.on('error',fatal);child.on('exit',code=>{if(!closed)fatal(Error('Simulator engine exited: '+code));});
function fatal(error){console.error(error.code==='EADDRINUSE'?`Port ${port} is already in use. Close the other application or run start.ps1 -Port 8788.`:error.message);for(const job of pending.splice(0))job.reject(error);shutdown();process.exitCode=1;}
function call(line){const job=serial.then(()=>new Promise((resolve,reject)=>{pending.push({resolve,reject});child.stdin.write(line+'\n');}));serial=job.catch(()=>{});return job;}
const ranges=[[3,450],[0,10000],[0,14],[0,10000],[0,100],[-40,80]];
const scenarios=new Set(['normal','dry','wet','missing','stale','mix_due','fermentation']);
function metadata(data){return {...data,boot,controlToken:token,playback:{playing,speed}};}
function body(req){return new Promise((resolve,reject)=>{let text='';req.on('data',chunk=>{text+=chunk;if(text.length>2048){reject(Error('Request too large'));req.destroy();}});req.on('end',()=>resolve(text));req.on('error',reject);});}
function reply(res,code,data){res.writeHead(code,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(data));}
const server=http.createServer(async(req,res)=>{
 try{
  const origin=`http://127.0.0.1:${port}`;
  if(req.headers.host!==`127.0.0.1:${port}`){reply(res,403,{error:'Use the loopback URL'});return;}
  const url=new URL(req.url,origin);
  if(req.method==='GET'&&url.pathname==='/api/data'){
   const data=metadata(await call('data'));const afterText=url.searchParams.get('after')||'0';
   if(!/^\d+$/.test(afterText)||Number(afterText)>4294967295){reply(res,400,{error:'Invalid sequence'});return;}
   const after=Number(afterText);data.reset=url.searchParams.get('boot')!==boot||after>data.sequence;
   const rows=data.samples.filter(s=>data.reset||s[0]>after);data.samples=rows.slice(0,60);data.more=rows.length>60;data.next=data.samples.at(-1)?.[0]||(data.reset?0:after);
   reply(res,200,data);return;
  }
  if(req.method==='POST'&&['/api/control','/api/simulation'].includes(url.pathname)){
   if(req.headers.origin&&req.headers.origin!==origin){reply(res,403,{error:'Origin rejected'});return;}
   if(req.headers['x-control-token']!==token){reply(res,403,{error:'Invalid control token'});return;}
   if((req.headers['content-type']||'').split(';')[0]!=='application/x-www-form-urlencoded'){reply(res,415,{error:'Use a form request'});return;}
   const form=new URLSearchParams(await body(req));let data;
   if(url.pathname==='/api/control'){
    if([...form.keys()].length!==1||!form.has('command')||!/^([a-z_]{1,30})$/.test(form.get('command'))){reply(res,400,{error:'Invalid command form'});return;}
    data=await call('command '+form.get('command'));
   }else{
    const action=form.get('action');
    if(action==='play'){playing=true;data=await call('data');}
    else if(action==='pause'){playing=false;data=await call('data');}
    else if(action==='speed'&&[1,10,60].includes(Number(form.get('value')))){speed=Number(form.get('value'));data=await call('data');}
    else if(action==='reset'){playing=false;data=await call('reset');boot=randomBytes(8).toString('hex');}
    else if(action==='scenario'&&scenarios.has(form.get('value'))){playing=false;data=await call('scenario '+form.get('value'));boot=randomBytes(8).toString('hex');}
    else if(action==='step'){if(playing){reply(res,409,{error:'Pause before stepping'});return;}data=await call('advance 4000');}
    else if(action==='inputs'){
     const values=form.getAll('value').map(Number),missing=Number(form.get('missing')),frozen=form.get('frozen');
     if(values.length!==6||values.some((v,i)=>!Number.isFinite(v)||v<ranges[i][0]||v>ranges[i][1])||!Number.isInteger(missing)||missing<0||missing>63||!['0','1'].includes(frozen)){reply(res,400,{error:'Invalid sensor input'});return;}
     data=await call(`set ${values.join(' ')} ${missing} ${frozen}`);
    }else{reply(res,400,{error:'Unknown simulation action'});return;}
   }
   reply(res,data.accepted?200:409,metadata(data));return;
  }
  if(['/api/control','/api/simulation'].includes(url.pathname)){reply(res,405,{error:'Use POST'});return;}
  if(req.method!=='GET'){reply(res,405,{error:'Method not allowed'});return;}
  const assets={'/':['web/index.html','text/html; charset=utf-8'],'/style.css':['web/style.css','text/css; charset=utf-8'],'/app.js':['web/app.js','text/javascript; charset=utf-8'],'/sim.js':['simulator/sim.js','text/javascript; charset=utf-8'],'/sim.css':['simulator/sim.css','text/css; charset=utf-8']};
  if(!assets[url.pathname]){reply(res,404,{error:'Not found'});return;}
  const [file,type]=assets[url.pathname];let content=fs.readFileSync(path.join(root,file));
  if(url.pathname==='/')content=content.toString().replace('ONE BIN · LOCAL ESP32','BOKASHI · VIRTUAL BIN').replace('<h1>Compost monitor</h1>','<h1>Bokashi simulator</h1>').replace('Read the bin. Stay in control.','Explore moisture control, mixing and safety.').replace('</head>','<link rel="stylesheet" href="/sim.css"><script src="/sim.js" defer></script></head>');
  res.writeHead(200,{'Content-Type':type,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});res.end(content);
 }catch(error){if(!res.headersSent)reply(res,500,{error:error.message});}
});
server.on('error',fatal);
let advancing=false;
const timer=setInterval(async()=>{if(!playing||advancing)return;advancing=true;try{await call('advance '+100*speed);}catch(e){fatal(e);}finally{advancing=false;}},100);
function shutdown(){if(closed)return;closed=true;clearInterval(timer);server.close();child.stdin.end();}
process.on('SIGINT',shutdown);process.on('SIGTERM',shutdown);
call('data').then(()=>server.listen(port,'127.0.0.1',()=>{
 const url=`http://127.0.0.1:${port}/`;console.log(`Bokashi simulator ready: ${url}`);
 if(process.argv.includes('--open')){
  const opener=process.platform==='win32'?spawn('powershell.exe',['-NoProfile','-Command',`Start-Process -FilePath '${url}'`],{windowsHide:true,stdio:'ignore'}):spawn(process.platform==='darwin'?'open':'xdg-open',[url],{stdio:'ignore'});
  opener.on('error',()=>console.warn(`Could not open your browser. Open ${url} manually.`));
  opener.on('exit',code=>console.log(code===0?'Browser opened. Choose a scenario and press Play.':`Could not open your browser. Open ${url} manually.`));
 }
})).catch(fatal);
