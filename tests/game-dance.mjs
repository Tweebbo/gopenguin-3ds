import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {createRequire} from 'node:module';
const web=process.env.GOPENGUIN_WEB??'/home/miguelv/gopenguin-dev/web';
const {build}=createRequire(web+'/package.json')('esbuild');
const source=(await build({entryPoints:[web+'/src/games/dance-model.ts'],bundle:true,format:'esm',write:false})).outputFiles[0].text;
const {DanceModel,createExpertChart}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
const data=JSON.parse(await readFile(web+'/public/data/dance.json'));
const lines=execFileSync('./build/game-dance-test',{encoding:'utf8',maxBuffer:2*1024*1024}).trim().split('\n');let cursor=0,notes=0;
const dirs=['left','down','up','right'];
const charts=[...data.charts,...data.songs.map(song=>{let state=1;return createExpertChart(song,()=>{state=(Math.imul(state,1664525)+1013904223)>>>0;return state/4294967296;});})];
for(const [index,chart] of charts.entries()){
 const song=data.songs[chart.song-1],model=new DanceModel(data,song,chart);
 assert.equal(lines[cursor++],`C ${index} ${model.notes.length}`);
 for(const note of model.notes){
  const [,time,direction,hold]=lines[cursor++].split(' ').map(Number);
  assert.equal(dirs[direction],note.direction,`chart ${index}, note ${notes}`);
  assert(Math.abs(time-note.time)<.02);assert(Math.abs(hold-note.hold)<.02);notes++;
 }
 for(let hold=0;hold<2;hold++){
  const m=new DanceModel(data,song,chart);
  for(const note of m.notes){m.advance(note.time);m.hit(note.direction);if(!hold)m.release(note.direction);}
  m.advance(m.duration);
  assert.equal(lines[cursor++],`R ${hold} ${m.score} ${m.coins} ${m.maxCombo}`,`chart ${index}, hold ${hold}`);
 }
}
const offsets=[[4000,[58,59,116,117,290,291,581,582,1162,1163]],[2500,[36,37,72,73,181,182,363,364,726,727]],[1750,[25,26,50,51,127,128,254,255,508,509]],[1500,[21,22,43,44,109,110,218,219,436,437]]];
const grades=['perfect','great','good','almost','miss','boo'];let timingCases=0;
for(const [approach,cases] of offsets)for(const offset of cases)for(const sign of [-1,1]){
 const m=new DanceModel(data,data.songs[0],{song:1,difficulty:'expert',approach,notes:[[10,'left',0]]});m.advance(5000+sign*offset);
 const grade=m.hit('left'),state=m.notes[0].judgement===null?0:grades.indexOf(m.notes[0].judgement)+1;
 assert.equal(lines[cursor++],`W ${approach} ${sign*offset} ${grades.indexOf(grade)} ${m.rating} ${m.score} ${state}`);timingCases++;
}
for(const grade of ['AAA','AA','A','B','C','D','B','B'])assert.equal(lines[cursor++],`G ${grade}`);
for(const song of data.songs){const m=new DanceModel(data,song,data.charts[0]);assert(Math.abs(Number(lines[cursor++].slice(2))-m.audioOffset)<1e-6);}
assert.equal(cursor,lines.length);
console.log(`PASS Dance C/TypeScript parity: ${notes} notes across 18 authored and 6 seeded Expert charts; ${timingCases} early/late timing cases, Boo/distant misses, crowd rating, eight result grades, six song offsets, hold scores, coin brackets and full combos.`);
