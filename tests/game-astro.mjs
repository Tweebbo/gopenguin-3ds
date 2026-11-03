// TS/C parity for Astro Barrier. Build build/game-astro-test first:
// cc -O2 -std=c11 -Isource tests/game-astro.c source/game_astro_model.c -lm -o build/game-astro-test
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
const phases=['ready','playing','done','lost','over','won'];

const snapshot=model=>{
  const level=model.levels[model.level];
  const values=[model.level,model.score,model.lives,model.shots,model.hits,phases.indexOf(model.phase),model.gunX,model.bulletY,model.bullet?1:0,level.frame,model.totalShots,model.oneUps,model.restarted?1:0,model.stamps.length,...model.stamps];
  for(const object of model.objects)values.push(object.frame,object.hit?1:0);
  return values;
};

const target=model=>{
  const candidates=model.objects.filter(object=>
    ((['barrier','slide','bonus'].includes(object.kind)&&!object.hit)||(object.kind==='yellow'&&!object.hit&&object.frame<3)));
  const blockers=model.objects.filter(object=>object.kind==='block'||object.kind==='train'||(object.kind==='orange'&&object.frame<18));
  const clear=candidates.filter(object=>{
    const x=(object.box[0]+object.box[2])/2;
    if(x<48||x>352)return false;
    return !blockers.some(block=>x>block.box[0]&&x<block.box[2]&&block.box[1]>object.box[1]);
  });
  const pool=clear.length?clear:candidates.filter(object=>{const x=(object.box[0]+object.box[2])/2;return x>=48&&x<=352;});
  pool.sort((a,b)=>b.box[3]-a.box[3]);
  return pool[0];
};

const model=new AstroModel(data);
model.start();
const commands=[],expected=[];
for(let tick=0;tick<40000;tick++){
  if(model.phase==='won'||model.phase==='over')break;
  let action=0;
  if(model.phase==='done')action=1;
  else if(model.phase==='lost')action=2;
  let direction=0,fire=0;
  if(!action&&!model.bullet){
    const next=target(model);
    if(next){
      const x=(next.box[0]+next.box[2])/2;
      if(Math.abs(model.gunX-x)<=2.5)fire=model.shots>0?1:0;
      else direction=model.gunX<x?1:-1;
    }else fire=model.shots>0?1:0;
  }
  commands.push(`${direction} ${fire} ${action}`);
  if(action===1)model.next();
  else if(action===2)model.restart();
  if(direction<0)model.press(-1);else if(direction>0)model.press(1);else model.release();
  if(fire)model.fire();
  model.step();
  model.sounds.length=0;
  expected.push(snapshot(model));
  if(expected.length>40000)break;
}

const run=spawnSync(path.join(root,'build/game-astro-test'),[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:64*1024*1024});
assert.equal(run.status,0,run.stderr);
const actual=run.stdout.trim().split('\n').filter(Boolean).map(line=>line.trim().split(/\s+/).map(Number));
assert.equal(actual.length,expected.length,`ticks ${actual.length} vs ${expected.length}`);
actual.forEach((row,index)=>{
  assert.equal(row.length,expected[index].length,`tick ${index} fields`);
  row.forEach((value,field)=>{
    assert(Math.abs(value-expected[index][field])<1e-6,`tick ${index}, field ${field}: ${value} vs ${expected[index][field]}`);
  });
});

const levels=new Set(actual.map(row=>row[0]));
console.log(`PASS Astro Barrier TS/C parity: ${actual.length} ticks, levels ${Math.min(...levels)+1}..${Math.max(...levels)+1}, final phase ${phases[actual.at(-1)[5]]}, score ${actual.at(-1)[1]}`);
