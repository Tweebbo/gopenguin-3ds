import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=root+'/build/update-cia-checks';await mkdir(folder,{recursive:true});
const flags=['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include'];
const armFlags=['-D__3DS__','-Itests/stubs/updater-3ds'];
execFileSync('cc',[...flags,...armFlags,'-DGOPENGUIN_VERSION="1.1.4"','-c','source/gp_update.c','-o',folder+'/gp_update.o'],{cwd:root});
execFileSync('cc',[...flags,...armFlags,'-c','tests/update-cia-worker.c','-o',folder+'/worker.o'],{cwd:root});
execFileSync('cc',[...flags,folder+'/gp_update.o',folder+'/worker.o','source/update_manifest.c','source/hash.c','source/common.c','-lcrypto','-l:libjansson.so.4','-pthread','-o',folder+'/worker'],{cwd:root});
const bytes=Buffer.alloc(512*1024,0x5a);bytes.fill(0,0,32);bytes.writeUInt32LE(32,0);const tmd=64;bytes.writeUInt32BE(0x10004,tmd);bytes.writeBigUInt64BE(0x0004000007E90200n,tmd+0x140+0x4c);bytes.writeUInt16BE(1045,tmd+0x140+0x9c);
const artifact={url:'https://gopenguin.lol/3ds/test.cia',bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')};
const manifest={schema:1,titleId:'0004000007E90200',version:'1.1.5',cia:artifact,'3dsx':artifact};
const scenarios=['success','am-fail','start-fail','write-fail','short-write','finish-fail','query-fail','wrong-version','wrong-title','cancel','truncated','checksum','retry','slow-finish'];
const results=[];
for(const scenario of scenarios){
 const dir=folder+'/'+scenario;await rm(dir,{recursive:true,force:true});await mkdir(dir+'/sdmc:/3ds/gopenguin',{recursive:true});await writeFile(dir+'/payload.cia',bytes);await writeFile(dir+'/manifest.json',JSON.stringify(manifest));
 const output=execFileSync(folder+'/worker',[scenario],{cwd:dir,encoding:'utf8',timeout:15000}).trim();const log=await readFile(dir+'/sdmc:/3ds/gopenguin/last-update.log','utf8');assert(log.includes('Verifying the update on SD'));
 if(scenario==='success'||scenario==='retry'||scenario==='slow-finish')assert(log.includes('Confirming the installed')&&log.includes('Update installed.'));
 results.push({scenario,status:'PASS',output});
}
const report={status:'PASS',cases:results.length,implementation:'Production gp_update.c compiled through its __3DS__ CIA branch; mocked libctru services and network, real file I/O, SHA-256 and CIA identity validation',physicalConsoleTested:false,results};
await writeFile(folder+'/report.json',JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
