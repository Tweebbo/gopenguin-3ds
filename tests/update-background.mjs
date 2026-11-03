import assert from 'node:assert/strict';
import {execFileSync,spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile,rm} from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=root+'/build/background-tests';await mkdir(folder,{recursive:true});
execFileSync('cc',['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include','-DGP_LAN_ONLY','-DGOPENGUIN_VERSION="1.0.0"','-DGP_UPDATE_URL="http://127.0.0.1:18895/latest.json"','tests/update-background-worker.c','source/gp_update.c','source/update_manifest.c','source/incremental.c','source/hash.c','source/resource_store.c','source/common.c','source/transport/conn.c','source/transport/ws.c','source/transport/download.c','-lz','-lcrypto','-l:libjansson.so.4','-pthread','-o',folder+'/worker'],{cwd:root});
const payload=Buffer.alloc(1024*1024,0x5a);payload.write('3DSX');const old=Buffer.from('3DSX previous application');
const artifact={url:'http://127.0.0.1:18895/app.3dsx',bytes:payload.length,sha256:createHash('sha256').update(payload).digest('hex')};
const manifest={schema:1,titleId:'0004000007E90200',version:'1.0.1',cia:artifact,'3dsx':artifact};
const resource=Buffer.alloc(128,0x74),digest=createHash('sha256').update(resource).digest(),index=Buffer.alloc(180);index.write('GPRIDX1');index.writeUInt32LE(1,8);index.writeUInt32LE(resource.length,12);index.write('test/art.bin',16);digest.copy(index,144);index.writeUInt32LE(resource.length,176);
const bundleHeader=Buffer.alloc(48);bundleHeader.write('GPRBND1');bundleHeader.writeUInt32LE(1,8);digest.copy(bundleHeader,12);bundleHeader.writeUInt32LE(resource.length,44);const bundle=Buffer.concat([bundleHeader,resource]);
const resourceArtifact=(name,content)=>({url:'http://127.0.0.1:18895/'+name,bytes:content.length,sha256:createHash('sha256').update(content).digest('hex')});
let ranges=[],currentMode='',dropped=false;const server=http.createServer((req,res)=>{
 if(req.url==='/latest.json')return res.end(JSON.stringify(manifest));
 if(req.url==='/resources.idx'||req.url==='/resources.gpb'){const bytes=req.url==='/resources.idx'?index:bundle;res.writeHead(200,{'Content-Length':bytes.length});return res.end(bytes);}
 assert.equal(req.url,'/app.3dsx');const match=/bytes=(\d+)-(\d*)/.exec(req.headers.range??'');
 const first=Number(match?.[1]??0),last=match?.[2]?Number(match[2]):payload.length-1;ranges.push({first,last});
 res.writeHead(match?206:200,{'Content-Length':last-first+1,'Accept-Ranges':'bytes','ETag':'"background-fixture"',...(match?{'Content-Range':`bytes ${first}-${last}/${payload.length}`}:{})});
 if(currentMode==='pause-drop'&&!dropped&&last-first>65536){dropped=true;res.write(payload.subarray(first,first+65536));setTimeout(()=>res.destroy(),200);return;}
 res.end(payload.subarray(first,last+1));
});
await new Promise(r=>server.listen(18895,'127.0.0.1',r));const results=[];
const run=async(dir,mode)=>{const start=Date.now(),child=spawn(folder+'/worker',[dir+'/app.3dsx',mode],{cwd:dir});let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);const code=await new Promise(r=>child.once('exit',r));assert.equal(code,0,mode+': '+output);return {mode,status:'PASS',hostMilliseconds:Date.now()-start,output:output.trim()};};
try{
 for(const mode of ['old','new','saver','pause','pause-drop','pressure','sleep','corrupt','cancel','resources','missing-resource','damaged-index']){
  const dir=folder+'/'+mode;await rm(dir,{recursive:true,force:true});await mkdir(dir+'/sdmc:/3ds/gopenguin',{recursive:true});await writeFile(dir+'/app.3dsx',old);
  const identity=Buffer.from('{"session":"background fixture account"}');await writeFile(dir+'/sdmc:/3ds/gopenguin/settings.json',identity);ranges=[];currentMode=mode;dropped=false;
  if(['resources','missing-resource','damaged-index'].includes(mode))manifest.resources={schema:1,index:resourceArtifact('resources.idx',index),bundle:resourceArtifact('resources.gpb',bundle),deltas:[]};else delete manifest.resources;
  const result=await run(dir,mode);
  if(mode==='pause-drop')assert(dropped,'The connection must actually drop while preparation yields');
  assert.deepEqual(await readFile(dir+'/app.3dsx'),['corrupt','cancel','missing-resource','damaged-index'].includes(mode)?old:payload);
  if(mode==='cancel'){const cancelledSize=(await import('node:fs/promises')).stat;const partial=(await cancelledSize(dir+'/app.3dsx.part')).size;assert(partial>0&&partial<payload.length);ranges=[];result.resume=await run(dir,'new');assert(ranges.some(r=>r.first===partial),'Resume must request the retained offset');assert.deepEqual(await readFile(dir+'/app.3dsx'),payload);}
  assert.deepEqual(await readFile(dir+'/sdmc:/3ds/gopenguin/settings.json'),identity);result.ranges=ranges;results.push(result);
 }
}finally{await new Promise(r=>server.close(r));}
await writeFile(folder+'/report.json',JSON.stringify({status:'PASS',liveNetwork:false,physicalConsole:false,results},null,2)+'\n');
console.log('PASS: Old/New/saver pacing, manual pause, loading pressure, suspend, corrupt staged update rejected, cancellation and cross-process HTTP Range resume; identity retained.');
