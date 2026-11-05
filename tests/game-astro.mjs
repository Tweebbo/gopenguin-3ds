// TS/C parity for Astro Barrier: the same seeded inputs drive src/games/astro-model.ts and
// source/game_astro_model.c; every frame's state must match. Build build/game-astro-test first.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
import path from 'node:path';
import {spawnSync} from 'node:child_process';

const root=path.resolve(import.meta.dirname,'..');
const reference=path.resolve(process.env.ASTRO_REFERENCE??path.join(root,'../../gopenguin-dev/web'));
const require=createRequire(reference+'/package.json');
const {build}=require('esbuild');
const bundle=(await build({entryPoints:[reference+'/src/games/astro-model.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
const module={exports:{}};new Function('module','exports',bundle)(module,module.exports);
const {AstroModel}=module.exports;
const data=JSON.parse(await readFile(reference+'/public/data/astro.json','utf8'));
const phases=['title','intro','playing','done','restart','secret','over','won'];
const f=v=>v.toFixed(6);

let seed=0x5eed1234;const random=()=>{seed=(seed*1103515245+12345)>>>0;return seed/4294967296;};
const model=new AstroModel(data);
const commands=[],expected=[];
// Sessions: each starts a level (some in a secret set), then plays random held inputs.
const sessions=[[1,0],[7,0],[10,0],[11,0],[16,0],[20,0],[30,0],[31,0],[38,0],[2,2],[3,2],[4,2],[5,2],[6,2],[7,2],[8,2],[10,2],[3,1],[10,1],[40,0]];
let fireHeld=false,dir=0;
for(const [startLevel,set] of sessions){
  for(let t=0;t<2400;t++){
    let start=0,confirm=0;
    if(t===0)start=startLevel;
    if(model.phase==='intro'&&model.introTicks===12&&t<1800)confirm=1;
    if(random()<0.05)dir=Math.floor(random()*3)-1;
    if(random()<0.08)fireHeld=!fireHeld;
    const left=dir<0?1:0,right=dir>0?1:0,fire=fireHeld?1:0;
    commands.push(`${left} ${right} ${fire} ${confirm} ${start} ${start?set:0}`);
    if(start){model.start(start);if(set){model.secretSet=set;model.level=start;model.load();}}
    if(confirm)model.confirm();
    model.release();if(left)model.press(-1);if(right)model.press(1);
    model.trigger(!!fire);model.step();
    const m=model,values=[phases.indexOf(m.phase),m.level,m.secretSet,m.score,m.lives,m.shotsFrame,m.hitCounter,m.totalShots>>>0,m.levelsStarted,
      f(m.gunX),m.gunFrame,m.bullet.active?1:0,f(m.bullet.x),f(m.bullet.y),m.turretBullet.active?1:0,f(m.turretBullet.x),f(m.turretBullet.y),m.overlay,m.introTicks,m.planeFrame,m.livesCollected,m.levelRestarted?1:0,m.stamps.length,...m.stamps];
    for(const o of m.objects){values.push(o.clipFrame,o.wallFrame,o.frame,o.hit?1:0,f(o.matrix[4]),f(o.matrix[5]));for(const p of o.parts??[])values.push(p.clipFrame,p.hit?1:0);}
    expected.push(values.join(' '));
    m.sounds.length=0;
  }
}
const run=spawnSync(path.join(root,'build/game-astro-test'),{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:256*1024*1024});
assert.equal(run.status,0,run.stderr);
const actual=run.stdout.trimEnd().split('\n');
assert.equal(actual.length,expected.length);
for(let i=0;i<expected.length;i++)assert.equal(actual[i],expected[i],`frame ${i}: ${commands[i]}`);
const phasesSeen=new Set(expected.map(line=>phases[Number(line.split(' ')[0])]));
console.log(`PASS Astro Barrier TS/C parity: ${expected.length} frames over ${sessions.length} sessions; phases ${[...phasesSeen].join(', ')}`);
