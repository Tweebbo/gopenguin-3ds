import assert from 'node:assert/strict';
import {execFileSync,spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=root+'/build/update-checks';await mkdir(folder,{recursive:true});
const common=['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include'];
execFileSync('cc',[...common,'tests/update-manifest.c','source/update_manifest.c','source/hash.c','-lcrypto','-l:libjansson.so.4','-o',folder+'/manifest'],{cwd:root});
const base={schema:1,titleId:'0004000007E90200',version:'1.0.1',cia:{url:'https://github.com/Tweebbo/gopenguin-3ds/releases/download/v1.0.1/gopenguin.cia',sha256:'a'.repeat(64),bytes:224000000},'3dsx':{url:'https://gopenguin.lol/3ds/gopenguin.3dsx',sha256:'b'.repeat(64),bytes:222000000}};
const parse=async value=>{await writeFile(folder+'/manifest.json',typeof value==='string'?value:JSON.stringify(value));return execFileSync(folder+'/manifest',['manifest',folder+'/manifest.json'],{encoding:'utf8'}).trim();};
assert.equal(await parse(base),'VALID');let checks=1;
for(const mutate of [j=>j.titleId='0004000007EBB000',j=>j.schema=2,j=>j.version='dev',j=>j.version='1.0.1-extra',j=>j.version='1.0',j=>j.cia.sha256='bad',j=>j.cia.bytes=0,j=>j.cia.bytes=600000000,j=>j.cia.url='http://gopenguin.lol/app.cia',j=>j.cia.url='https://gopenguin.lol.evil.example/app.cia',j=>j.cia.url='https://github.com/Tweebbo/tweebbo-3ds/releases/download/v1/a.cia',j=>j.cia.url='https://gopenguin.lol/a\r\nBad: true',j=>delete j['3dsx']]){const value=structuredClone(base);mutate(value);assert.equal(await parse(value),'INVALID');checks++;}
assert.equal(await parse(JSON.stringify(base).replace('"schema":1','"schema":1,"schema":1')),'INVALID');checks++;
for(const [a,b,want] of [['1.10.0','1.9.9',1],['1.0.0','1.0.0',0],['1.0.0','2.0.0',-1]]){assert.equal(Number(execFileSync(folder+'/manifest',['compare',a,b],{encoding:'utf8'})),want);checks++;}
const candidate=process.env.GOPENGUIN_PACKAGE??root+'/build/package-release-'+(await readFile(root+'/VERSION','utf8')).trim();
const old=JSON.parse(await readFile(candidate+'/manifest.json'));
for(const kind of ['cia','3dsx']){const a=old.artifacts[kind];assert.equal(execFileSync(folder+'/manifest',['file',candidate+'/'+a.file,a.sha256,String(a.bytes),kind,...(old.version?[old.version]:[])],{encoding:'utf8'}).trim(),'VALID');checks++;}
const cia=old.artifacts.cia;
const incompatible=old.version.replace(/\d+$/,patch=>String(Number(patch)+1));
assert.equal(execFileSync(folder+'/manifest',['file',candidate+'/'+cia.file,cia.sha256,String(cia.bytes),'cia',incompatible],{encoding:'utf8'}).trim(),'INVALID');checks++;
assert.equal(execFileSync(folder+'/manifest',['file',candidate+'/'+cia.file,'0'.repeat(64),String(cia.bytes),'cia'],{encoding:'utf8'}).trim(),'INVALID');checks++;
const foreignCia=folder+'/foreign-title.cia',foreignBytes=await readFile(candidate+'/'+cia.file);
const align=n=>Math.ceil(n/64)*64,tmd=align(foreignBytes.readUInt32LE(0))+align(foreignBytes.readUInt32LE(8))+align(foreignBytes.readUInt32LE(12));
foreignBytes.writeBigUInt64BE(0x0004000007EBB000n,tmd+0x140+0x4c);await writeFile(foreignCia,foreignBytes);
assert.equal(execFileSync(folder+'/manifest',['file',foreignCia,createHash('sha256').update(foreignBytes).digest('hex'),String(foreignBytes.length),'cia'],{encoding:'utf8'}).trim(),'INVALID');checks++;
const payload=Buffer.alloc(256,0x5a);payload.write('3DSX');const sha=createHash('sha256').update(payload).digest('hex');
const launch=folder+'/test.3dsx';await writeFile(launch,Buffer.from('3DSX previous app'));
const manifest=structuredClone(base);manifest['3dsx']={url:'http://127.0.0.1:18891/app.3dsx',sha256:sha,bytes:payload.length};
let requests=0,resumed=false;
const server=http.createServer((req,res)=>{
 if(req.url==='/latest.json'){res.writeHead(200,{'Content-Length':Buffer.byteLength(JSON.stringify(manifest))});return res.end(JSON.stringify(manifest));}
 if(req.url!=='/app.3dsx'){res.writeHead(404);return res.end();}
 requests++;const offset=Number(/bytes=(\d+)-/.exec(req.headers.range??'')?.[1]??0);
 if(requests===1){res.writeHead(200,{'Content-Length':payload.length});res.end(payload.subarray(0,100));return;}
 resumed=offset===100;res.writeHead(offset?206:200,{'Content-Length':payload.length-offset,...(offset?{'Content-Range':`bytes ${offset}-${payload.length-1}/${payload.length}`}:{})});res.end(payload.subarray(offset));
});
await new Promise(resolve=>server.listen(18891,'127.0.0.1',resolve));
try{
 execFileSync('cc',[...common,'-DGP_LAN_ONLY','-DGOPENGUIN_VERSION="1.0.0"','-DGP_UPDATE_URL="http://127.0.0.1:18891/latest.json"','tests/update-worker.c','source/gp_update.c','source/update_manifest.c','source/hash.c','source/common.c','source/transport/conn.c','source/transport/ws.c','source/transport/download.c','-lcrypto','-l:libjansson.so.4','-pthread','-o',folder+'/worker'],{cwd:root});
 const child=spawn(folder+'/worker',[launch],{stdio:['ignore','pipe','pipe']});let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);const code=await new Promise(resolve=>child.once('exit',resolve));assert.equal(code,0,output);
 assert.equal(requests,2);assert.ok(resumed);assert.deepEqual(await readFile(launch),payload);checks+=4;
}finally{await new Promise(resolve=>server.close(resolve));}
const report={status:'PASS',checks,manifest:'strict semantic version, title, size, SHA-256, HTTPS origin, duplicate keys',worker:'real HTTP interruption, Range resume, checksum and atomic 3DSX replacement; bounded shutdown',cia:'actual prepared CIA identity/hash accepted; AM installation requires physical validation'};
await writeFile(folder+'/report.json',JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
