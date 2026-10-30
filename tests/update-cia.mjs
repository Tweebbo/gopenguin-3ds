import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import path from 'node:path';
import {deflateSync} from 'node:zlib';
const root=path.resolve(import.meta.dirname,'..'),folder=root+'/build/update-cia-checks';await mkdir(folder,{recursive:true});
const flags=['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include'];
const armFlags=['-D__3DS__','-Itests/stubs/updater-3ds'];
execFileSync('cc',[...flags,...armFlags,'-DGOPENGUIN_VERSION="1.1.4"','-c','source/gp_update.c','-o',folder+'/gp_update.o'],{cwd:root});
execFileSync('cc',[...flags,...armFlags,'-c','tests/update-cia-worker.c','-o',folder+'/worker.o'],{cwd:root});
execFileSync('cc',[...flags,folder+'/gp_update.o',folder+'/worker.o','source/update_manifest.c','source/incremental.c','source/hash.c','source/common.c','-lz','-lcrypto','-l:libjansson.so.4','-pthread','-o',folder+'/worker'],{cwd:root});
const bytes=Buffer.alloc(512*1024,0x5a);bytes.fill(0,0,32);bytes.writeUInt32LE(32,0);const tmd=64;bytes.writeUInt32BE(0x10004,tmd);bytes.writeBigUInt64BE(0x0004000007E90200n,tmd+0x140+0x4c);bytes.writeUInt16BE(1045,tmd+0x140+0x9c);
const artifact={url:'https://gopenguin.lol/3ds/test.cia',bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')};
const manifest={schema:1,titleId:'0004000007E90200',version:'1.1.5',cia:artifact,'3dsx':artifact};
const scenarios=['success','am-fail','start-fail','write-fail','short-write','finish-fail','query-fail','wrong-version','wrong-title','cancel','truncated','checksum','retry','slow-finish','small-success','small-base-missing','small-base-damaged','small-damaged-header','small-checksum','small-interrupted','small-install-retry'];
const results=[];
for(const scenario of scenarios){
 const dir=folder+'/'+scenario;await rm(dir,{recursive:true,force:true});await mkdir(dir+'/sdmc:/3ds/gopenguin',{recursive:true});await writeFile(dir+'/payload.cia',bytes);await writeFile(dir+'/manifest.json',JSON.stringify(manifest));
 if(scenario.startsWith('small-')){
  const source=bytes.subarray(4096),name=Buffer.from('games/bank.gpk'),header=Buffer.alloc(116);header.write('TUPD001');header.writeUInt32LE(1,8);header.writeUInt32LE(bytes.length,12);header.writeUInt32LE(2,16);header.write(scenario==='small-damaged-header'?'0004000007EBB000':'0004000007E90200',20);header.write('1.1.4',36);header.write('1.1.5',60);createHash('sha256').update(bytes).digest().copy(header,84);
  const literal=Buffer.alloc(5);literal[0]=1;literal.writeUInt32LE(4096,1);const copy=Buffer.alloc(7);copy[0]=2;copy.writeUInt32LE(source.length,1);copy.writeUInt16LE(name.length,5);
  const patch=deflateSync(Buffer.concat([header,literal,bytes.subarray(0,4096),copy,name,createHash('sha256').update(source).digest()]));
  await writeFile(dir+'/payload.tupd',patch);const latest=structuredClone(manifest);latest.incremental=[{format:'tupd1',from:'1.1.4',cia:{url:'https://gopenguin.lol/3ds/test.tupd',bytes:patch.length,sha256:createHash('sha256').update(patch).digest('hex')}}];await writeFile(dir+'/manifest.json',JSON.stringify(latest));await mkdir(dir+'/romfs:/games',{recursive:true});
  if(scenario!=='small-base-missing')await writeFile(dir+'/romfs:/games/bank.gpk',scenario==='small-base-damaged'?Buffer.alloc(source.length,0):source);
 }
 const output=execFileSync(folder+'/worker',[scenario],{cwd:dir,encoding:'utf8',timeout:15000}).trim();const log=await readFile(dir+'/sdmc:/3ds/gopenguin/last-update.log','utf8');assert(log.includes('Verifying the update on SD'));
 if(scenario==='success'||scenario==='retry'||scenario==='slow-finish')assert(log.includes('Confirming the installed')&&log.includes('Update installed.'));
 results.push({scenario,status:'PASS',output});
}
const report={status:'PASS',cases:results.length,implementation:'Production gp_update.c compiled through its __3DS__ CIA branch; mocked libctru services and network, real file I/O, SHA-256 and CIA identity validation',physicalConsoleTested:false,results};
await writeFile(folder+'/report.json',JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
