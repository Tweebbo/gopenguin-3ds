import assert from 'node:assert/strict';
import {execFileSync,spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile,rm} from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),build=root+'/build/resource-update-tests';await mkdir(build,{recursive:true});
execFileSync('cc',['-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-DGP_LAN_ONLY','-DGOPENGUIN_VERSION="1.0.0"','-DGP_UPDATE_URL="http://127.0.0.1:18894/latest.json"','-Wall','-Wextra','-Werror','-O2','-Isource','-Ibuild/host-include','tests/resource-update-worker.c','source/gp_update.c','source/update_manifest.c','source/incremental.c','source/resource_store.c','source/hash.c','source/common.c','source/transport/conn.c','source/transport/ws.c','source/transport/download.c','-lcrypto','-l:libjansson.so.4','-lz','-pthread','-o',build+'/worker'],{cwd:root});
const sha=b=>createHash('sha256').update(b).digest(),contents={'audio/song.gpz':Buffer.alloc(30000,5),'games/cart.gpk':Buffer.from('new game artwork')};
const index=Buffer.alloc(16+Object.keys(contents).length*164);index.write('GPRIDX1');index.writeUInt32LE(Object.keys(contents).length,8);index.writeUInt32LE(Object.values(contents).reduce((n,b)=>n+b.length,0),12);
Object.entries(contents).sort().forEach(([name,b],n)=>{const at=16+n*164;index.write(name,at);sha(b).copy(index,at+128);index.writeUInt32LE(b.length,at+160);});
const pack=entries=>{const head=Buffer.alloc(12);head.write('GPRBND1');head.writeUInt32LE(Object.keys(entries).length,8);return Buffer.concat([head,...Object.values(entries).map(b=>{const h=Buffer.alloc(36);sha(b).copy(h);h.writeUInt32LE(b.length,32);return Buffer.concat([h,b]);})]);};
const full=pack(contents),delta=pack({'games/cart.gpk':contents['games/cart.gpk']}),app=Buffer.concat([Buffer.from('3DSX'),Buffer.alloc(65532,0x5a)]);
const artifact=(name,b)=>({url:'http://127.0.0.1:18894/'+name,bytes:b.length,sha256:sha(b).toString('hex')});
const offer={schema:1,index:artifact('resources.idx',index),bundle:artifact('resources.gpb',full),deltas:[{from:'1.0.0',bundle:artifact('delta.gpb',delta)}]};
let scenario,requests,manifest;
const server=http.createServer((req,res)=>{
 requests[req.url]=(requests[req.url]??0)+1;
 if(req.url==='/latest.json')return res.end(JSON.stringify(manifest));
 const original=({'/resources.idx':index,'/resources.gpb':full,'/delta.gpb':delta,'/app.3dsx':app})[req.url];assert(original,req.url);
 const data=Buffer.from(original),offset=Number(/bytes=(\d+)-/.exec(req.headers.range??'')?.[1]??0);
 if((scenario==='corrupt-bundle'&&req.url==='/delta.gpb')||(scenario==='corrupt-index'&&req.url==='/resources.idx'))data[data.length-1]^=1;
 const interrupted=scenario==='interrupted'&&req.url==='/delta.gpb'&&requests[req.url]===1;
 const body=data.subarray(offset);res.writeHead(offset?206:200,{'Content-Length':body.length,...(offset?{'Content-Range':`bytes ${offset}-${data.length-1}/${data.length}`}:{})});res.end(interrupted?body.subarray(0,Math.floor(body.length/2)):body);
});
await new Promise(r=>server.listen(18894,'127.0.0.1',r));const results=[];
try{
 for(scenario of ['bridge','interrupted','missing-base','corrupt-bundle','corrupt-index','repair','repair-corrupt']){
  requests={};manifest={schema:1,titleId:'0004000007E90200',version:'1.0.1',cia:artifact('app.3dsx',app),'3dsx':artifact('app.3dsx',app),resources:offer};
  const folder=build+'/'+scenario;await rm(folder,{recursive:true,force:true});await mkdir(folder+'/sdmc:/3ds/gopenguin',{recursive:true});await mkdir(folder+'/romfs:/audio',{recursive:true});await mkdir(folder+'/romfs:/data',{recursive:true});
  if(scenario!=='missing-base'&&!scenario.startsWith('repair'))await writeFile(folder+'/romfs:/audio/song.gpz',contents['audio/song.gpz']);
  if(scenario==='repair-corrupt')for(const b of Object.values(contents)){const digest=sha(b).toString('hex'),directory=folder+'/sdmc:/3ds/gopenguin/resources/'+digest[0];await mkdir(directory,{recursive:true});await writeFile(directory+'/'+digest+'.bin',Buffer.alloc(b.length,0x58));}
  await writeFile(folder+'/romfs:/data/resource-offer.json',JSON.stringify(offer));
  const saved=Buffer.from('{"session":"local identity fixture"}');await writeFile(folder+'/sdmc:/3ds/gopenguin/settings.json',saved);
  const launch=folder+'/app.3dsx',old=Buffer.from('3DSX current app');await writeFile(launch,old);
  const child=spawn(build+'/worker',[launch,scenario.startsWith('repair')?'repair':'update'],{cwd:folder});let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);const code=await new Promise(r=>child.once('exit',r));
  const failed=scenario.startsWith('corrupt-');assert.equal(code,failed?6:0,scenario+': '+output);
  assert.deepEqual(await readFile(launch),failed||scenario.startsWith('repair')?old:app);
  assert.deepEqual(await readFile(folder+'/sdmc:/3ds/gopenguin/settings.json'),saved);
  if(!failed)for(const b of Object.values(contents)){const digest=sha(b).toString('hex');assert.deepEqual(await readFile(folder+'/sdmc:/3ds/gopenguin/resources/'+digest[0]+'/'+digest+'.bin'),b);}
  if(failed)assert(!requests['/app.3dsx']);
  if(scenario==='bridge')assert(!requests['/resources.gpb']);
  if(scenario==='interrupted')assert.equal(requests['/delta.gpb'],2);
  if(scenario==='missing-base')assert.equal(requests['/resources.gpb'],1);
  if(scenario.startsWith('repair'))assert(!requests['/app.3dsx']&&!requests['/latest.json']);
  results.push({scenario,status:'PASS',requests,accountFileRetained:true});
 }
}finally{await new Promise(r=>server.close(r));}
await writeFile(build+'/report.json',JSON.stringify({status:'PASS',physicalConsoleTested:false,results},null,2)+'\n');console.log('PASS resource updater: bridge migration, changed-only download, HTTP resume, explicit full repair, damaged downloads and bootstrap repair.');
