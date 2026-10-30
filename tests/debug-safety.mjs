import assert from 'node:assert/strict';
import {spawnSync,spawn} from 'node:child_process';
import {createServer} from 'node:http';
import {readFile,writeFile} from 'node:fs/promises';
const flags=['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-DNO_TLS','-DGP_DEBUG_GAMES','-DGP_LAN_ONLY','-DGP_UPDATE_URL="http://127.0.0.1:25519/latest.json"','-Wall','-Wextra','-Werror','-Isource','-Ibuild/host-include'];
const sources=['tests/debug-safety.c','source/gp_update.c','source/update_manifest.c','source/incremental.c','source/penguin_net.c','source/hash.c','source/common.c','source/transport/conn.c','source/transport/ws.c','source/transport/download.c'];
const compile=spawnSync('cc',[...flags,...sources,'-lz','-lcrypto','-l:libjansson.so.4','-lm','-pthread','-o','build/debug-safety-test'],{stdio:'inherit'});assert.equal(compile.status,0);
const file='build/debug-checks/blocked-updater.3dsx',previous=Buffer.from('3DSX isolated sentinel');await writeFile(file,previous);let requests=0;
const server=createServer((req,res)=>{requests++;res.writeHead(500);res.end();});await new Promise(r=>server.listen(25519,'127.0.0.1',r));
try{const child=spawn('build/debug-safety-test',[file],{stdio:'inherit'});assert.equal(await new Promise(r=>child.once('exit',r)),0);assert.equal(requests,0);assert.deepEqual(await readFile(file),previous);await writeFile('build/debug-checks/safety.json',JSON.stringify({status:'PASS',checks:['public API rejected before network startup','public world rejected before network startup','update check and install disabled before worker startup','zero HTTP requests; local launch sentinel unchanged'],httpRequests:requests,production:false},null,2)+'\n');}finally{await new Promise(r=>server.close(r));}
