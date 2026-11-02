import assert from 'node:assert/strict';
import {createRequire} from 'node:module';
import {readFile,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..'),reference=path.resolve(process.env.PIZZATRON_REFERENCE||path.join(root,'../../GoPenguin-Gameplay-PC-20261101/web')),require=createRequire(reference+'/package.json'),{build}=require('esbuild');
const bundle=(await build({entryPoints:[reference+'/src/games/pizzatron-model.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
const module={exports:{}};new Function('module','exports',bundle)(module,module.exports);const {PizzatronModel}=module.exports;
const config=JSON.parse(await readFile(reference+'/public/data/pizzatron.json','utf8'));
const results=[];
const phases=['order','play','remove','place'];
function snapshot(m){
 let art=0,drop=0;
 for(const s of m.splats)art+=s.x+3*s.y+7*s.kind+11*s.variant+13*s.age+17*s.slot;
 for(const t of m.toppings)art+=t.x+3*t.y+7*t.kind+11*t.variant+17*t.slot;
 for(const d of m.drops)drop+=d.x+3*d.y+7*d.vx+11*d.vy+13*d.rotation;
 return [m.x,m.speed,m.boost,phases.indexOf(m.phase),m.order,m.made,m.ordersMade,m.failures,m.score,m.handicap,m.tips,m.consecutive,+m.normal,+m.hot,+m.cheese,+m.perfect,m.ingredient,m.variant,m.result,m.splatCount,m.toppingCount,m.serial,...m.counts,m.splats.length+m.toppings.length,art,drop];
}
for(const candy of [false,true])for(const strategy of ['perfect','none','four-mistakes','chaos']){
 let seed=0x12345678;const random=()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;seed>>>=0;return seed/4294967296;};
 const model=new PizzatronModel(config,candy,random),commands=[],expected=[];let serial=0,step=0,ingredient=0;
 for(let tick=0;tick<20000&&!model.result;tick++){
  if(serial!==model.serial){serial=model.serial;step=0;ingredient=0;}
  let x=380,y=275,held=0,pickup=0;
  const work=strategy!=='none'&&(strategy!=='four-mistakes'||model.ordersMade>=4);
  if(strategy==='chaos'){
   x=((tick*47)%1000)-100;y=150+(tick*31)%320;held=tick%9<6?1:0;if(tick%11===0)pickup=1+Math.floor(tick/11)%7;
  }else if(work&&model.phase==='play'&&model.x>=100){
   const points=[[0,-50],[0,50],[-90,0],[90,0],[20,0],[-20,0]],order=model.recipe;
   if(step<28){
    if(!step)pickup=order.sauce;
    const point=points[Math.min(5,Math.floor(step/3))];x=model.x+model.speed+model.boost+point[0];y=375+point[1]-100;held=1;
   }else if(step===28){pickup=3;x=model.x+model.speed+model.boost;y=375;}
   else{
    ingredient=order.topping.findIndex((n,i)=>model.counts[i]<n);
    if(ingredient>=0){pickup=ingredient+4;x=model.x+model.speed+model.boost;y=375;}
   }
   step++;
  }
  model.mouseX=x;model.mouseY=y;model.held=!!held;if(pickup)model.pickup(pickup);model.step();model.sounds.length=0;
  commands.push(`${x} ${y} ${held} ${pickup}`);expected.push(snapshot(model));
 }
 assert(model.result,`${candy}/${strategy}: game did not end`);
 if(strategy==='none'){assert.equal(model.result,4);assert.equal(model.failures,5);assert.equal(model.coins,0);}
 if(strategy==='perfect'){assert.equal(model.made,40);assert.equal(model.result,7);assert.equal(model.failures,0);assert.equal(model.tips,885);assert.equal(model.coins,candy?1285:1085);}
 if(strategy==='four-mistakes'){assert.equal(model.made,36);assert.equal(model.result,6);assert.equal(model.failures,4);}
 const run=spawnSync(root+'/build/game-pizzatron-test',[candy?'1':'0'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
 const actual=run.stdout.trim().split('\n').map(line=>line.split(' ').map(Number));assert.equal(actual.length,expected.length);
 actual.forEach((row,tick)=>{assert.equal(row.length,expected[tick].length);row.forEach((value,i)=>assert(Math.abs(value-expected[tick][i])<.00001,`${candy}/${strategy} tick ${tick}, field ${i}: ${value} vs ${expected[tick][i]}`));});
 results.push({candy,strategy,ticks:actual.length,made:model.made,failures:model.failures,score:model.score,tips:model.tips,coins:model.coins,result:model.result});
}
await writeFile(root+'/build/pizzatron-model.json',JSON.stringify({status:'PASS',scope:'independently ported TS/C game rules, real original silhouettes/recipes and growing-sauce masks',results},null,2)+'\n');
console.log('PASS Pizzatron TS/C rules, both modes, 40-order perfect/mixed runs, five mistakes and wrong-ingredient/drag chaos',results);
