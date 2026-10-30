import assert from 'node:assert/strict';
import {execFileSync,spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import {deflateSync} from 'node:zlib';
import http from 'node:http';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=root+'/build/update-incremental-checks';await mkdir(folder,{recursive:true});
execFileSync('cc',['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include','-DGP_LAN_ONLY','-DGOPENGUIN_VERSION="1.0.0"','-DGP_UPDATE_URL="http://127.0.0.1:18892/latest.json"','tests/update-worker.c','source/gp_update.c','source/update_manifest.c','source/incremental.c','source/hash.c','source/common.c','source/transport/conn.c','source/transport/ws.c','source/transport/download.c','-lz','-lcrypto','-l:libjansson.so.4','-pthread','-o',folder+'/worker'],{cwd:root});
const sha=b=>createHash('sha256').update(b).digest('hex'),bank=Buffer.alloc(65536,0x74),prefix=Buffer.from('3DSX update code'),payload=Buffer.concat([prefix,bank]);
const header=Buffer.alloc(116);header.write('TUPD001');header.writeUInt32LE(2,8);header.writeUInt32LE(payload.length,12);header.writeUInt32LE(2,16);header.write('0004000007E90200',20);header.write('1.0.0',36);header.write('1.0.1',60);Buffer.from(sha(payload),'hex').copy(header,84);
const literal=Buffer.alloc(5);literal[0]=1;literal.writeUInt32LE(prefix.length,1);const name=Buffer.from('games/bank.gpk'),copy=Buffer.alloc(7);copy[0]=2;copy.writeUInt32LE(bank.length,1);copy.writeUInt16LE(name.length,5);
const patch=deflateSync(Buffer.concat([header,literal,prefix,copy,name,Buffer.from(sha(bank),'hex')]));
let scenario,requests,manifest;
const server=http.createServer((req,res)=>{
 if(req.url==='/latest.json'){const body=JSON.stringify(manifest);res.writeHead(200,{'Content-Length':Buffer.byteLength(body)});return res.end(body);}
 const small=req.url==='/patch.tupd';assert(small||req.url==='/app.3dsx');requests[small?'patch':'full']++;
 const data=small?patch:payload,offset=Number(/bytes=(\d+)-/.exec(req.headers.range??'')?.[1]??0);
 if(small&&scenario==='interrupted'&&requests.patch===1){res.writeHead(200,{'Content-Length':data.length});return res.end(data.subarray(0,Math.floor(data.length/2)));}
 const body=Buffer.from(data.subarray(offset));if(small&&scenario==='checksum'&&body.length)body[body.length-1]^=1;
 res.writeHead(offset?206:200,{'Content-Length':body.length,...(offset?{'Content-Range':`bytes ${offset}-${data.length-1}/${data.length}`}:{})});res.end(body);
});
await new Promise(resolve=>server.listen(18892,'127.0.0.1',resolve));
const results=[];
try{
 for(scenario of ['success','interrupted','missing','damaged','checksum']){
  const dir=folder+'/'+scenario;await rm(dir,{recursive:true,force:true});await mkdir(dir+'/sdmc:/3ds/gopenguin',{recursive:true});await mkdir(dir+'/romfs:/games',{recursive:true});
  if(scenario!=='missing')await writeFile(dir+'/romfs:/games/bank.gpk',scenario==='damaged'?Buffer.alloc(bank.length,0):bank);
  const app=dir+'/app.3dsx';await writeFile(app,Buffer.from('3DSX current app'));
  const artifact={url:'http://127.0.0.1:18892/app.3dsx',bytes:payload.length,sha256:sha(payload)};
  manifest={schema:1,titleId:'0004000007E90200',version:'1.0.1',cia:artifact,'3dsx':artifact,incremental:[{format:'tupd1',from:'1.0.0','3dsx':{url:'http://127.0.0.1:18892/patch.tupd',bytes:patch.length,sha256:sha(patch)}}]};requests={patch:0,full:0};
  const child=spawn(folder+'/worker',[app],{cwd:dir});let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);const code=await new Promise(resolve=>child.once('exit',resolve));assert.equal(code,0,output);assert.deepEqual(await readFile(app),payload);
  assert.equal(requests.patch,scenario==='interrupted'?2:1);assert.equal(requests.full,['missing','damaged','checksum'].includes(scenario)?1:0);
  results.push({scenario,status:'PASS',requests});
 }
}finally{await new Promise(resolve=>server.close(resolve));}
await writeFile(folder+'/report.json',JSON.stringify({status:'PASS',scope:'Production worker with real local HTTP, incremental Range resume, reconstruction, verification and atomic3DSX replacement',results},null,2)+'\n');console.log('PASS incremental3DSX:',results.length,'real HTTP resume/fallback/atomic replacement cases');
