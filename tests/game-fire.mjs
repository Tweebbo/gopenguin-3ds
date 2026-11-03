// TS/C parity for Card-Jitsu Fire. Build build/game-fire-test first.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

const root=path.resolve(import.meta.dirname,'..');
const reference=path.resolve(process.env.FIRE_REFERENCE??path.join(root,'../../gopenguin-dev/web'));
const require=createRequire(reference+'/package.json');
const {build}=require('esbuild');
const bundle=(await build({entryPoints:[reference+'/src/games/fire-model.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
const module={exports:{}};new Function('module','exports',bundle)(module,module.exports);
const {FireModel}=module.exports;
const table=JSON.parse(await readFile(reference+'/public/data/fire-cards.json','utf8'));
const cards=Object.fromEntries(table.cards.map(card=>[card.id,card]));
const phases=['menu','queue','handshake','board','spin','move','trump','opponent','select','wait','reveal','over','error'];
const hand='1,2,3,11,21';
let model=new FireModel(cards);
const lines=[],expected=[];

const list=values=>values.map(value=>`${value},`).join('');
const snapshot=(model,ok)=>[
  ok?1:0,phases.indexOf(model.phase),model.seat,model.players,model.active,
  list(model.colour),list(model.energy),list(model.position),list(model.spin),list([...model.hand,...Array(Math.max(0,5-model.hand.length)).fill(0)]),
  list(model.played),list(model.result),list(model.standing),model.trump,
  list(model.participants.map(value=>value?1:0)),list(model.opponents.map(value=>value?1:0)),
  `${+model.menuReady}${+model.started}${+model.requestGet}${+model.requestReady}${+model.readySent}${+model.inputPending}${+model.rankAwarded}`,
  model.rank,model.percentage,model.names.map(name=>`${name},`).join('')
].join('|');

const begin=sensei=>{lines.push(`begin|${sensei?1:0}`);model.begin(sensei);expected.push(snapshot(model,true));};
const reset=()=>{lines.push('reset');model.reset();expected.push(snapshot(model,true));};
const packet=(command,...args)=>{lines.push([command,...args].join('|'));const ok=model.packet(command,args);expected.push(snapshot(model,ok));};

reset();
begin(true);
packet('scard','997','1001','1','0','A|1');
packet('jz','0');
packet('sz','0','A,Sensei','1,-1','6,6','12,4',hand,'3,5,9','0','');
packet('zm','is','','1');
packet('zm','ub','0','9,4','0');
packet('zm','sb','bt','0,1','f');
packet('zm','rb','0,1','1,11','6,5','4,1','bt,f',hand,'1,2');
packet('zm','nt','1','2,6,10',hand);
packet('zm','zo','2,1');

reset();
begin(false);
packet('jg','953');
packet('jz','1');
packet('sz','1','A,B,C,D','1,2,3,4','6,6,6,6','4,8,0,12',hand,'3,5,9','0,0,0,0','');
packet('zm','co','','0,2,3');
packet('zm','sb','bt','0,2','f');
packet('zm','rb','0,2','1,11','6,5','4,1','bt,f',hand,'1,2,3,4');
packet('zm','cz','2');
packet('zm','tb');
packet('zm','nr','0','4');
packet('zm','zo','4,3,2,1');

reset();
begin(true);
packet('scard','997','1001','1','0','A|1');
packet('jz','0');
packet('zm','nt','1','2,6,10',hand);
packet('sz','0','A,Sensei','1,-1','6,6','12,4',hand,'3,5,9','0','');
packet('cz');
packet('sz','0','A,Sensei','1,-1','6,6','12,4','1,2,3', '3,5,9','0','');

import('node:fs/promises').then(m=>m.writeFile('/tmp/opencode/fire-lines.txt',lines.join('\n')+'\n'));
const run=spawnSync(path.join(root,'build/game-fire-test'),[],{input:lines.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});
assert.equal(run.status,0,run.stderr);
const actual=run.stdout.trim().split('\n').filter(Boolean);
assert.equal(actual.length,expected.length,`rows ${actual.length} vs ${expected.length}`);
actual.forEach((line,index)=>assert.equal(line,expected[index],`step ${index}: ${lines[index]}`));
console.log(`PASS Card-Jitsu Fire TS/C parity: ${actual.length} packets, Sensei and four-seat matches, battles, standings, quit and malformed packets`);
