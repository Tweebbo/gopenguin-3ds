import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import path from 'node:path';
import {createRequire} from 'node:module';

export async function readFishArt(web){
 const folder=path.join(web,'public/swf/games/fish');
 const extras={};
 const module={exports:{}},code=(await createRequire(web+'/package.json')('esbuild').build({entryPoints:[web+'/src/games/fish-data.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
 new Function('module','exports',code)(module,module.exports);extras.callbacks=module.exports.prepareFishCallbacks;
 for(const name of ['title','canoworms']){
  const data=JSON.parse(await readFile(folder+'/lang/en/'+name+'/timelines.json','utf8'));
  assert.equal(Object.keys(data.fonts??{}).length,0);assert.equal(Object.keys(data.editTexts??{}).length,0);assert.equal(Object.keys(data.soundFiles??{}).length,0);
  const bytes=await readFile(folder+'/lang/en/'+name+'/chars/'+data.pack.file);
  for(const character of Object.values(data.characters)){
   assert.equal(character.kind,'shape');const [offset,length]=data.pack.entries[character.file];
   character.file='data:image/svg+xml;base64,'+bytes.subarray(offset,offset+length).toString('base64');
  }
  extras[name]=data;
 }
 return extras;
}

/** Keep the original English title/worm labels in the same atlas as the game. */
export function prepareFishArt(data,extras){
 extras.callbacks(data);
 let next=1+Math.max(...Object.keys(data.sprites).map(Number),...Object.keys(data.characters).map(Number),...Object.keys(data.editTexts??{}).map(Number));
 const add=extra=>{
  const ids=new Map([...Object.keys(extra.sprites),...Object.keys(extra.characters)].map(id=>[Number(id),next++]));
  const entries=frames=>frames.map(frame=>frame.map(entry=>({...entry,id:ids.get(entry.id)})));
  for(const [id,character]of Object.entries(extra.characters))data.characters[ids.get(Number(id))]=structuredClone(character);
  for(const [id,sprite]of Object.entries(extra.sprites))data.sprites[ids.get(Number(id))]={...structuredClone(sprite),frames:entries(sprite.frames)};
  const root=next++;data.sprites[root]={...structuredClone(extra.main),frames:entries(extra.main.frames)};return root;
 };
 const title=add(extras.title),worms=add(extras.canoworms),protect=new Set([55]),lures=new Map();
 const lower=next++;
 data.sprites[lower]={...structuredClone(data.sprites[145]),frames:[structuredClone(data.sprites[145].frames[0].filter(entry=>![28,32,36,37,162].includes(entry.depth)))],actions:{1:[['stop',null]]}};
 data.main.frames.push([{id:lower,depth:1,name:'lower_title',matrix:[.45,0,0,.45,160,130]}]);
 for(const timeline of [data.main,...Object.values(data.sprites)])for(const frame of timeline.frames)for(const entry of frame){
  if(entry.name==='logo_title')entry.id=title;
  if(entry.name==='canOWorms')entry.id=worms;
  if(['body','hookFall','mask','gameOver_mc','puffle'].includes(entry.name)&&data.sprites[entry.id])protect.add(entry.id);
  if([155,159,162].includes(entry.id)){
   const id=entry.id;entry.name='lure'+id;
   if(!lures.has(id)){const wrapper=next++;data.sprites[wrapper]={frames:[[{id,depth:1}],[{id,depth:1}]],labels:{},scriptFrames:[],sounds:[],actions:{1:[['stop',null]]}};lures.set(id,wrapper);}
   entry.id=lures.get(id);
  }
 }
 for(const id of protect){const timeline=data.sprites[id];if(timeline?.frames.length===1){timeline.frames.push(structuredClone(timeline.frames[0]));timeline.actions={...timeline.actions,1:[['stop',null]]};}}
 // Hit geometry remains addressable; only its debug rectangle is transparent.
 for(const frame of data.sprites[55].frames)for(const entry of frame)entry.color={mult:[1,1,1,0],add:[0,0,0,0]};
 return data;
}
