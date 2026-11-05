// Exports a native game's banks (GPK1) with the shared scene exporter.
// Usage: node tools/export-game.mjs hydro
//
// Games create instances from code (attachMovie, per-player colours, wake
// pieces). The scene exporter bakes static subtrees, so this tool preloads a
// hook into tools/export-scene.mjs that adapts only the game's timelines.json in
// memory: it adds hidden instance slots for code-created clips and keeps
// code-addressed single-frame clips as separate instances. No original art is
// changed and the web tree is not written.
import {readFile,writeFile,mkdir,rm,readdir} from 'node:fs/promises';
import {createRequire,syncBuiltinESMExports} from 'node:module';
import {spawn} from 'node:child_process';
import path from 'node:path';
import {readFishArt,prepareFishArt} from './fish-art.mjs';
import {cardAssets,cardStage,cardActors,dojoStage,fireStage,writeCardManifest} from './card-art.mjs';

const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB??root+'/../web-server/web');
const debugCard=['debug-card','debug-fire'].includes(process.argv[2])||/^debug-(card|fire):/.test(process.env.GAME_EXPORT_PRELOAD??'');
const cardArt=debugCard?await cardAssets():null;
// Measured concurrency is at most 21 obstacles (18 in one depth group) through level 20.
export const HYDRO_SLOTS=24;

/** Turn a static clip into a stopped two-frame clip so the exporter keeps it addressable. */
function addressable(data,id){
 const t=data.sprites[id];if(!t)throw Error(`Missing sprite ${id}`);
 if(t.frames.length===1){t.frames=[t.frames[0],structuredClone(t.frames[0])];t.actions={...t.actions,1:[['stop',null]]};}
}
function nextId(data){return 1+Math.max(...Object.keys(data.sprites).map(Number),...Object.keys(data.characters).map(Number),...Object.keys(data.editTexts??{}).map(Number));}

/** Hydro: the game clip with obstacle slots; water, wake and the stage background move to an "under" bank. */
function hydroParts(data){
 const main=data.main,game=main.frames[main.labels.Game-1].find(e=>e.name==='game'),sprite=data.sprites[game.id];
 if(sprite.frames.length!==1)throw Error('Unexpected Hydro game clip');
 const by=name=>{const e=sprite.frames[0].find(x=>x.name===name);if(!e)throw Error('Missing Hydro '+name);return e;};
 const background=main.frames[main.labels.Game-1].find(e=>e.depth===1);
 if(!background||data.sprites[background.id])throw Error('Unexpected Hydro background');
 return {main,game,sprite,background,hazard:by('hazard'),water:by('water'),wake:by('wake'),boat:by('boat'),player:by('player'),crash:by('crash_mc')};
}
const booksData=JSON.parse(await readFile(web+'/public/data/books.json','utf8'));
const beansConfig=JSON.parse(await readFile(web+'/public/data/beans.json','utf8'));
const cartConfig=JSON.parse(await readFile(web+'/public/data/cart.json','utf8'));
const sledData=JSON.parse(await readFile(web+'/public/data/sled.json','utf8'));
const pizzatronConfig=JSON.parse(await readFile(web+'/public/data/pizzatron.json','utf8'));
const pizzatronOriginal=JSON.parse(await readFile(web+'/public/swf/games/pizzatron/Pizzatron/timelines.json','utf8'));
const fishArt=await readFishArt(web),fishOriginal=JSON.parse(await readFile(web+'/public/swf/games/fish/IceFishing/timelines.json','utf8'));
const pizzatronModule={exports:{}};
const beansModule={exports:{}};
const beansCode=(await createRequire(web+'/package.json')('esbuild').build({entryPoints:[web+'/src/games/beans-art.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
new Function('module','exports',beansCode)(beansModule,beansModule.exports);
const pizzatronCode=(await createRequire(web+'/package.json')('esbuild').build({entryPoints:[web+'/src/games/pizzatron-art.ts'],bundle:true,platform:'node',format:'cjs',write:false})).outputFiles[0].text;
new Function('module','exports',pizzatronCode)(pizzatronModule,pizzatronModule.exports);
/**
 * Cart Surfer tints entries by symbol id (cart.json tints) and one #depth path can hold a
 * tinted symbol in one frame and an untinted one in another. Unnamed tinted entries are
 * named `tint<depth>` and kept addressable; returns every resulting path under the cart.
 */
export function cartTint(data,tints=cartConfig.tints){
 const set=new Set(tints),paths=new Set();
 for(const t of Object.values(data.sprites))for(const f of t.frames)for(const e of f)if(set.has(e.id)&&!e.name)e.name='tint'+e.depth;
 for(const id of set)if(data.sprites[id])addressable(data,id);
 const cart=data.sprites[data.main.frames[0].find(e=>e.name==='game_mc').id].frames[0].find(e=>e.name==='cart_mc');
 const walk=(id,parent,depth)=>{const t=data.sprites[id];if(!t||depth>16)return;for(const f of t.frames)for(const e of f){const p=parent+(e.name?'.'+e.name:'#'+e.depth);if(set.has(e.id))paths.add(p);walk(e.id,p,depth+1);}};
 walk(cart.id,'root.game_mc.cart_mc',0);
 return [...paths].sort();
}
function rescueBank(data,theme,key,tile,parts){
 const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
 const frame=[];
 const tileId=symbol(tile),partId=symbol(parts);
 // Rescue.as draws bg first, then tiles and parts: the backdrop sits below every slot.
 frame.push({depth:1,id:symbol('bg'),name:'bg',matrix:[1,0,0,1,0,0]});
 // 3DS dual screen: the upper screen shows a wider window (about 12x7 tiles), so more slots.
 for(let i=0;i<144;i++)frame.push({depth:2+i,id:tileId,name:'tile'+i,matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<64;i++)frame.push({depth:200+i,id:partId,name:'part'+i,matrix:[1,0,0,1,0,-10000]});
 const snowball=symbol('snowball'),bat=symbol('bat');
 for(let i=0;i<4;i++)frame.push({depth:300+i,id:snowball,name:'snowball'+i,matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<4;i++)frame.push({depth:320+i,id:bat,name:'bat'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:340,id:symbol('kraken'),name:'kraken',matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<2;i++)frame.push({depth:342+i,id:symbol('secretcoinballoon'),name:'secret'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:360,id:symbol('player'),name:'player',matrix:[1,0,0,1,0,-10000]});
 // Original in-game HUD (hud_display): level and score fields get names so the game can write them.
 const hud=symbol('hud_display'),hudSprite=data.sprites[hud];
 for(const entry of hudSprite.frames[0]){if(entry.depth===7)entry.name='stage_txt';if(entry.depth===118)entry.name='score_txt';}
 frame.push({depth:400,id:hud,name:'hud',matrix:[1,0,0,1,0,0]});
 // Root frame 2 is the touch-screen composition: the same slots without the HUD.
 data.main={frames:[frame,frame.filter(entry=>entry.name!=='hud')],labels:{},actions:{1:[['stop',null]],2:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
function bookBank(data,pages){
 const frame=[];
 pages.forEach((page,index)=>{
  const art=(data.main.frames[page-1]??[]).find(entry=>entry.name==='art');
  if(!art)throw Error('Missing book page art '+page);
  const id=nextId(data);
  /* Paint By Letters paints the picture while the player types (books.json "paint": [letter index, art frame]).
     Keep frame 1 and every frame a paint step reaches; other frames stay empty so frame numbers are unchanged. */
  const source=data.sprites[art.id],book=booksData.books.find(b=>b.source?.sha256===data.source),info=book?.pages.find(p=>p.frame===page);
  const keep=new Set([1,...(info?.paint??[]).map(([,frame])=>frame)]),last=Math.max(...keep);
  const frames=Array.from({length:last},(_,i)=>keep.has(i+1)?source.frames[i]??[]:[]);
  data.sprites[id]={frames,labels:{},scriptFrames:[],sounds:[],actions:Object.fromEntries(frames.map((_,i)=>[i+1,[['stop',null]]]))};
  frame.push({...art,id,depth:1+index,name:'page'+index,matrix:[art.matrix[0],art.matrix[1],art.matrix[2],art.matrix[3],380,240]});
 });
 const sprites=new Set(),characters=new Set();
 const addCharacter=id=>{
  if(characters.has(id)||!data.characters[id])return;
  characters.add(id);
  const character=data.characters[id];
  if(character.records)for(const list of Object.values(character.records))walk(list);
 };
 const walk=entries=>{for(const entry of entries){const id=String(entry.id);if(data.sprites[id]){if(!sprites.has(id)){sprites.add(id);for(const child of data.sprites[id].frames)walk(child);}}else addCharacter(id);}};
 walk(frame);
 for(const id of Object.keys(data.sprites))if(!sprites.has(id))delete data.sprites[id];
 for(const id of Object.keys(data.characters))if(!characters.has(id))delete data.characters[id];
 for(const timeline of Object.values(data.sprites))for(const action of Object.values(timeline.actions??{})){
  const lists=Array.isArray(action)?[action]:[action.commands??[]];
  for(const list of lists)for(const command of list)if(typeof command[1]==='string'&&command[0]?.startsWith('goto')&&!(command[1] in (timeline.labels??{})))command[1]=1;
 }
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
/** Keeps only the timelines and characters reachable from `frame`, which becomes the bank's single root frame. */
function bookRoot(data,frame){
 const sprites=new Set(),characters=new Set();
 const addCharacter=id=>{
  if(characters.has(id)||!data.characters[id])return;
  characters.add(id);
  const character=data.characters[id];
  if(character.records)for(const list of Object.values(character.records))walk(list);
 };
 const walk=entries=>{for(const entry of entries){const id=String(entry.id);if(data.sprites[id]){if(!sprites.has(id)){sprites.add(id);for(const child of data.sprites[id].frames)walk(child);}}else addCharacter(id);}};
 walk(frame);
 for(const id of Object.keys(data.sprites))if(!sprites.has(id))delete data.sprites[id];
 for(const id of Object.keys(data.characters))if(!characters.has(id))delete data.characters[id];
 for(const timeline of Object.values(data.sprites))for(const action of Object.values(timeline.actions??{})){
  const lists=Array.isArray(action)?[action]:[action.commands??[]];
  for(const list of lists)for(const command of list)if(typeof command[1]==='string'&&command[0]?.startsWith('goto')&&!(command[1] in (timeline.labels??{})))command[1]=1;
 }
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return unbakedTints(data);
}
/**
 * Player-coloured placements (sentinel setRGB, see applyPlayerTint) must reach the runtime: the scene exporter bakes
 * static groups with their colour transform, which froze the sentinel green into the page atlas. Each tinted
 * placement goes through a two-frame playing wrapper, which the scene exporter never bakes; the runtime then paints
 * it with the player's colour, as the original setRGB does.
 */
function unbakedTints(data){
 const sentinel=c=>c?.mult?.[0]===0&&c.mult[1]===0&&c.mult[2]===0&&c.add?.[0]===1&&c.add[1]===254&&c.add[2]===3;
 const wrappers=new Map();let sequence=nextId(data);
 for(const timeline of [data.main,...Object.values(data.sprites)])for(const list of timeline.frames)for(const entry of list){
  if(!sentinel(entry.color))continue;
  if(data.sprites[entry.id]?.wrapped)continue;
  let id=wrappers.get(entry.id);
  if(id===undefined){id=sequence++;wrappers.set(entry.id,id);}
  entry.id=id;
 }
 for(const [child,id] of wrappers)data.sprites[id]={frames:[[{id:child,depth:1}],[{id:child,depth:1}]],labels:{},actions:{},scriptFrames:[],sounds:[],wrapped:true};
 return data;
}
const stopped=frames=>({frames,labels:{},scriptFrames:[],sounds:[],actions:Object.fromEntries(frames.map((_,i)=>[i+1,[['stop',null]]]))});
/**
 * Paint By Letters, one bank per page (2026-11-05 dual-screen pass). The page's original `art` clip at its original
 * stage position, with frame 1 (the blank page) and every frame a paint step reaches (books.json paintClips: root and
 * named child clips). Child clips that no step drives keep all their frames (their own animations). The puffle colour
 * clip of My Puffle stops on "blue", the colour the text names. Nothing else of the spread: the book chrome is drawn
 * over it by the cover bank (bookCoverBank "spread").
 */
function bookPageBank(data,page){
 const art=(data.main.frames[page-1]??[]).find(entry=>entry.name==='art');
 if(!art)throw Error('Missing book page art '+page);
 const book=booksData.books.find(b=>b.source?.sha256===data.source),info=book?.pages.find(p=>p.frame===page);
 if(!info)throw Error('Missing books.json page '+page);
 const commands=info.paintClips??(info.paint??[]).map(([at,frame])=>[at,'',frame]);
 const copied=new Map();let sequence=nextId(data);
 const clone=(symbol,path,depth=0)=>{
  const timeline=data.sprites[symbol];if(!timeline||depth>24)return symbol;
  const key=symbol+':'+path;if(copied.has(key))return copied.get(key);
  const id=sequence++;copied.set(key,id);data.sprites[id]={frames:[]};
  const selected=commands.filter(([,target])=>target===path).map(([, ,frame])=>frame);
  const colour=timeline.labels?.blue&&JSON.stringify(timeline.actions?.[1]??[]).includes('pufflecolor');
  const keep=selected.length||path===''||colour?new Set([1,...selected,...(colour?[timeline.labels.blue]:[])]):null;
  const last=keep?Math.max(...keep):timeline.frames.length;
  const frames=Array.from({length:last},(_,i)=>keep&&!keep.has(i+1)?[]:(timeline.frames[i]??[]).map(entry=>{
   const child=entry.name?(path?path+'.':'')+entry.name:(path+'#'+entry.depth);
   return {...entry,id:clone(entry.id,child,depth+1)};
  }));
  let actions=timeline.actions;
  if(path==='')actions=stopped(frames).actions;
  else if(colour)actions={...timeline.actions,1:[['gotoAndStop','blue']]};
  else if(selected.length)actions=Object.fromEntries(frames.map((_,i)=>[i+1,[['stop',null]]]));
  const labels=Object.fromEntries(Object.entries(timeline.labels??{}).filter(([,frame])=>frame<=frames.length));
  actions=Object.fromEntries(Object.entries(actions??{}).filter(([frame])=>Number(frame)<=frames.length));
  const scriptFrames=(timeline.scriptFrames??[]).filter(frame=>frame<=frames.length),sounds=(timeline.sounds??[]).filter(sound=>!(sound?.frame>frames.length));
  data.sprites[id]={...timeline,frames,labels,actions,scriptFrames,sounds};
  return id;
 };
 return bookRoot(data,[{...art,id:clone(art.id,''),depth:1,name:'page'}]);
}
/**
 * Paint By Letters cover bank: the original cover (main frame 1), the closing page (last frame) and the open-book
 * "spread" overlay that frames every page: the cover board with the right-page window, the left page and the spine
 * (the original bookmask layer, drawn above the page art so its bleed stays hidden, as in the original).
 */
function bookCoverBank(data,ending=false){
 const frames=data.main.frames,pageFrame=frames.find(f=>f.some(e=>e.name==='art')),artIndex=pageFrame.findIndex(e=>e.name==='art');
 const pageId=pageFrame.slice(0,artIndex).find(e=>Math.abs(e.matrix[4]-238.5)<2)?.id;
 const spine=id=>{const c=data.characters[id];return c?.size&&Math.abs(c.size[0]-40.6)<1&&Math.abs(c.size[1]-393.8)<1;};
 const spread=[pageFrame[artIndex+1],...pageFrame.slice(artIndex+2).filter(e=>e.id===pageId||spine(e.id))].map((e,i)=>({id:e.id,depth:1+i,matrix:e.matrix,...(e.color?{color:e.color}:{})}));
 if(!pageId||spread.length<2)throw Error('Unexpected book spread');
 const full=e=>{const c=data.characters[e.id];return c?.size&&c.size[0]>=760&&c.size[1]>=480;};
 const keep=e=>e.name!=='closeBtn'&&!(data.characters[e.id]?.kind==='text')&&e.matrix[5]<480&&!full(e);
 const holder=list=>{const id=nextId(data);data.sprites[id]=stopped([list.filter(keep).map(e=>({...e}))]);return id;};
 if(ending)return bookRoot(data,[{id:holder(frames[frames.length-1]),depth:1,name:'ending',matrix:[1,0,0,1,0,0]}]);
 const cover=holder(frames[0]),id=nextId(data);data.sprites[id]=stopped([spread]);
 return bookRoot(data,[
  {id:cover,depth:1,name:'cover',matrix:[1,0,0,1,0,0]},
  {id,depth:2,name:'spread',matrix:[1,0,0,1,0,0]},
 ]);
}
function missionBank(data){
 const frame=(data.main.frames[0]??[]).filter(entry=>!['screenArea','blocker_mc','dark_mc','instruc'].includes(entry.name));
 const sprites=new Set(),characters=new Set();
 const addCharacter=id=>{
  if(characters.has(id)||!data.characters[id])return;
  characters.add(id);
  const character=data.characters[id];
  if(character.records)for(const list of Object.values(character.records))walk(list);
 };
 const walk=entries=>{for(const entry of entries){const id=String(entry.id);if(data.sprites[id]){if(!sprites.has(id)){sprites.add(id);for(const child of data.sprites[id].frames)walk(child);}}else addCharacter(id);}};
 walk(frame);
 for(const id of Object.keys(data.sprites))if(!sprites.has(id))delete data.sprites[id];
 for(const id of Object.keys(data.characters))if(!characters.has(id))delete data.characters[id];
 for(const timeline of Object.values(data.sprites))for(const action of Object.values(timeline.actions??{})){
  const lists=Array.isArray(action)?[action]:[action.commands??[]];
  for(const list of lists)for(const command of list)if(typeof command[1]==='string'&&command[0]?.startsWith('goto')&&!(command[1] in (timeline.labels??{})))command[1]=1;
 }
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
export const JETPACK_HAZARD_SLOTS=12;
function jetpackBank(data){
 const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
 const tile=symbol('tile'),player=symbol('player'),hazard=symbol('hazard');
 // Hit-area clips hide themselves at runtime (this._visible = false); drop them from the art.
 const hidden=new Set(Object.entries(data.sprites).filter(([,s])=>Object.values(s.actions??{}).some(a=>/_visible\s*=\s*false/.test(a?.custom??''))).map(([id])=>Number(id)));
 for(const id of [tile,hazard])for(const f of data.sprites[id].frames)for(let i=f.length-1;i>=0;i--)if(hidden.has(f[i].id))f.splice(i,1);
 /* The player's `hit` clip is an authored collision shape (a flat oval); drop it so it does not cover the
    penguin. The game moves and collides through the level map, not through this clip. */
 for(const timeline of [data.main,...Object.values(data.sprites)])for(const list of timeline.frames)for(let i=list.length-1;i>=0;i--)if(list[i].name==='hit')list.splice(i,1);
 const frame=[];
 // Terrain: the level's 480x480 hazard pieces (Jetpack.drawStage draws them over the 80x80 tiles).
 for(let i=0;i<96;i++)frame.push({depth:1+i,id:tile,name:'tile'+i,matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<JETPACK_HAZARD_SLOTS;i++)frame.push({depth:100+i,id:hazard,name:'haz'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:120,id:player,name:'player',matrix:[1,0,0,1,0,-10000]});
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
const games={
 'debug-fire':{code:703002,banks:{main:{pack:'games/fire/fire',artScale:.5,textureBudget:8*1024*1024,fonts:{fields:[34],chars:'0123456789?'} ,virtualAssets:cardArt?.assets,transform:data=>fireStage(data,cardArt.icons)}}},
 'debug-card':{code:703001,banks:{
  main:{pack:'games/card/card',artScale:.5,textureBudget:4*1024*1024,fonts:{fields:[55,75,76,80],chars:'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz -!?'} ,virtualAssets:cardArt?.assets,transform:data=>cardStage(data,cardArt.icons)},
  actors:{pack:'games/card/battles/ambient',artScale:.5,textureBudget:4*1024*1024,virtualAssets:cardArt?.assets,transform:data=>cardActors(data,cardArt.walk)},
  dojo:{pack:'games/sensei/SenseiWidget',artScale:.5,textureBudget:6*1024*1024,virtualAssets:cardArt?.assets,transform:data=>dojoStage(data,cardArt.board)},
 }},
 fish:{code:700006,banks:{main:{pack:'games/fish/IceFishing',artScale:.5,
  fonts:{fields:Object.keys(fishOriginal.editTexts).map(Number),chars:Array.from({length:95},(_,i)=>String.fromCharCode(32+i)).join('')},
  transform(data){return prepareFishArt(data,fishArt);}}}},
 pizzatron:{
  code:700010,
  banks:{
   main:{pack:'games/pizzatron/Pizzatron',artScale:.5,
    fonts:{fields:Object.keys(pizzatronOriginal.editTexts).map(Number),chars:Array.from({length:95},(_,i)=>String.fromCharCode(32+i)).join('')},
    transform(data){return pizzatronModule.exports.preparePizzatronArt(data,pizzatronConfig);}},
   title:{pack:'games/pizzatron/lang/en/title'},
  },
 },
 sled:{
  code:700009,
  banks:{main:{pack:'games/sled/SledRacer',
   // Racer names on the sleds; everything else keeps its raster.
   fonts:{fields:[96],chars:Array.from({length:95},(_,i)=>String.fromCharCode(32+i)).join('')},
   texts:{54:'SLED RACING',235:'Steer with Left and Right'},
   transform(data){
    // Sled.drawRace(): hill tiles t<N> (attachMovie/addTile) along the diagonal and the finish
    // t100 under player0-3; authored as 7 visible tile slots and a finish slot in `hill`.
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    const tiles=Object.keys(sledData.tiles).sort((a,b)=>Number(a)-Number(b)).map(k=>symbol('t'+k));
    const game=data.sprites[data.main.frames[0].find(e=>e.name==='game').id],hill=data.sprites[game.frames[0].find(e=>e.name==='hill').id];
    const slot=nextId(data);
    data.sprites[slot]={frames:[...tiles.map(id=>[{depth:1,id,name:'t',matrix:[1,0,0,1,0,0]}]),[]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    const players=hill.frames[0];
    hill.frames[0]=[...Array.from({length:7},(_,i)=>({depth:1+i,id:slot,name:'tl'+i,matrix:[1,0,0,1,0,-10000]})),{depth:10,id:symbol('t100'),name:'fin',matrix:[1,0,0,1,0,-10000]},
     ...players.map((e,i)=>({...e,depth:20+i}))];
    // art.body takes each racer's colour.
    for(const t of Object.values(data.sprites))for(const f of t.frames)for(const e of f)if(e.name==='body'&&data.sprites[e.id])addressable(data,e.id);
    return data;
   }}},
 },
 mancala:{
  code:700008,
  banks:{main:{pack:'games/mancala/Mancala',
   // Runtime text: player names and scores, the cup hint count and the Game Over scores.
   fonts:{fields:[15,19,16,20,85,87,105],chars:Array.from({length:95},(_,i)=>String.fromCharCode(32+i)).join('')},
   transform(data){
   // Mancala.swf duplicates stone1-5 for every stone: 48 slots (frame k = stone k, 6 empty) replace them.
   const gameEntry=data.main.frames[0].find(e=>e.name==='game'),game=data.sprites[gameEntry.id];
   const stones=[1,2,3,4,5].map(i=>{const e=game.frames[0].find(x=>x.name==='stone'+i);if(!e)throw Error('Missing stone'+i);return e.id;});
   const slot=nextId(data);
   data.sprites[slot]={frames:[...stones.map(id=>[{depth:1,id,name:'p',matrix:[1,0,0,1,0,0]}]),[]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
   game.frames[0]=Array.from({length:48},(_,i)=>({depth:i+1,id:slot,name:'s'+i,matrix:[1,0,0,1,0,0]}));
   return data;
  }}},
 },
 four:{
  code:700007,
  banks:{main:{pack:'games/four/FindFour',transform(data){
   // FindFour.swf attaches coindrop1/2 inside game_mc under front_mc/turn_mc (swapDepths 1000/1001)
   // and the coinwin1-4 line at 2000: authored here as 42 cell slots and one win slot.
   const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
   const gameEntry=data.main.frames[0].find(e=>e.name==='game_mc'),game=data.sprites[gameEntry.id];
   const coin=nextId(data),win=coin+1;
   data.sprites[coin]={frames:[[],[{depth:1,id:symbol('coindrop1'),name:'c',matrix:[1,0,0,1,0,0]}],[{depth:1,id:symbol('coindrop2'),name:'c',matrix:[1,0,0,1,0,0]}]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
   data.sprites[win]={frames:[[],...[1,2,3,4].map(d=>[{depth:1,id:symbol('coinwin'+d),name:'w',matrix:[1,0,0,1,0,0]}])],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
   const by=name=>game.frames[0].find(e=>e.name===name);
   game.frames[0]=[...game.frames[0].filter(e=>!e.name),...Array.from({length:42},(_,i)=>({depth:10+i,id:coin,name:'k'+i,matrix:[1,0,0,1,0,0]})),
    {...by('front_mc'),depth:100},{...by('turn_mc'),depth:101},{depth:200,id:win,name:'win',matrix:[1,0,0,1,0,0]}];
   return data;
  }}},
 },
 thinice:{
  code:700005,clip:'game',
  banks:{
   main:{pack:'game-thinice',
    fonts:{fields:[165,166,176,177,180,191,192,193,194,195,196,197],chars:'0123456789'},
    transform(data){
     // ThinIce.render() draws the 19x15 board of the exported `tile` symbol, then the warps,
     // the movable block and the player over the game frame: authored here as instances.
     const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
     const board=nextId(data),tile=symbol('tile');
     data.sprites[board]={frames:[Array.from({length:285},(_,i)=>{const x=i%19,y=Math.floor(i/19);return {depth:i+1,id:tile,name:`t${y}_${x}`,matrix:[1,0,0,1,x*24,y*24]};})],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
     const frame=data.main.frames[data.main.labels.startgame-1];
     if(frame.some(e=>e.depth>=300&&e.depth<=304))throw Error('Thin Ice depth collision');
     frame.push({depth:300,id:board,name:'board',matrix:[1,0,0,1,156,60]},{depth:301,id:symbol('warp'),name:'warp0',matrix:[1,0,0,1,-100,-100]},{depth:302,id:symbol('warp'),name:'warp1',matrix:[1,0,0,1,-100,-100]},
      {depth:303,id:symbol('movable'),name:'iceBlock',matrix:[1,0,0,1,-100,-100]},{depth:304,id:symbol('player'),name:'icePlayer',matrix:[1,0,0,1,-100,-100]});
     frame.sort((a,b)=>a.depth-b.depth);
     return data;
    }},
   title:{pack:'game-thinice-title'},
  },
 },
 cart:{
  code:700004,
  banks:{
   main:{pack:'game-cart',artScale:.5,
    fonts:{fields:[338,339,343],chars:'0123456789'},
    texts:{349:'PRESS A TO CONTINUE',414:'Use Left and Right on the Circle Pad or D-Pad to lean the cart.',418:'Up and Down change how you ride the cart.',416:'Press A to jump. Combine moves to earn more points.'},
    transform(data){
     cartTint(data);
     // Cart.localize(): config text by instance name (play_txt, *_shadow -> *_title, ...).
     for(const t of [data.main,...Object.values(data.sprites)])for(const f of t.frames)for(const e of f){
      const field=data.editTexts?.[e.id];if(!field||!e.name)continue;
      const name=e.name,key=name==='play_txt'?'ui_play':name==='instructions_txt'?'ui_instruction':name==='score_label'?'score_score':name.endsWith('_shadow')?name.replace('_shadow','_title'):name;
      if(cartConfig.text[key])field.text=cartConfig.text[key];
     }
     return data;
    }},
   title:{pack:'game-cart-title'},
  },
 },
 beans:{
  code:700003,clip:'board',
  banks:{
   main:{pack:'game-beans',
    fonts:{fields:[568,570,572],chars:'0123456789'},
    transform(data){
     const board=data.main,game=board.frames[1],by=name=>{const e=game.find(x=>x.name===name);if(!e)throw Error('Missing beans '+name);return e;};
     beansModule.exports.prepareBeansArt(data,true);
     const multiply=(p,q)=>[p[0]*q[0]+p[2]*q[1],p[1]*q[0]+p[3]*q[1],p[0]*q[2]+p[2]*q[3],p[1]*q[2]+p[3]*q[3],p[0]*q[4]+p[2]*q[5]+p[4],p[1]*q[4]+p[3]*q[5]+p[5]];
     const layer=by('flyingBagsLayer'),bags=data.sprites[layer.id].frames[0];
     const holders=[...[0,1,2,3].map(i=>{const e=bags.find(x=>x.name==='bag'+i);return {id:e.id,matrix:multiply(layer.matrix??[1,0,0,1,0,0],e.matrix??[1,0,0,1,0,0])};}),
      ...['oneup','hazard0','hazard1','hazard2'].map(n=>{const e=by(n);return {id:e.id,matrix:e.matrix??[1,0,0,1,0,0]};})];
     // Sixteen flight/spill slots: frame k holds the original thrower clip of kind k-1, frame 9 is empty.
     const slot=nextId(data);
     data.sprites[slot]={frames:[...holders.map(h=>[{depth:1,id:h.id,name:'h',matrix:h.matrix}]),[]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
     // Clips the web hides in classic mode are not exported; the throwers are replaced by the slots.
     const hidden=new Set(['candyPeng','candyTruck','sign','matchColors','bagDropsLeft_label','bagDropsLeftBoard','stack','back_into_truck','hazardPoof','invincibleShieldPowerUp','candyHazard0','candyHazard1','candyHazard2','flyingBagsLayer','oneup','hazard0','hazard1','hazard2']);
     board.frames[1]=[...game.filter(e=>!hidden.has(e.name)),...Array.from({length:16},(_,i)=>({depth:466+i,id:slot,name:'s'+i,matrix:[1,0,0,1,0,0]}))].sort((a,b)=>a.depth-b.depth);
     if(new Set(board.frames[1].map(e=>e.depth)).size!==board.frames[1].length)throw Error('Beans slot depth collision');
     // config.xml text by instance name (Beans.localize) and the HUD labels.
     const text={...beansConfig.text,msg_getpaid:'PRESS A to GET PAID!!!'},labels={lives_label:'score_lives',truck_label:'score_truck',score_label:'score_score'};
     for(const t of [data.main,...Object.values(data.sprites)])for(const f of t.frames)for(const e of f){
      const field=data.editTexts?.[e.id];if(!field||!e.name)continue;
      const value=labels[e.name]?beansConfig.text[labels[e.name]]:text[e.name];if(value!==undefined)field.text=value;
     }
     return data;
    }},
   title:{pack:'game-beans-logo'},
  },
 },
 roundup:{
  code:700002,clip:'board',
  banks:{main:{pack:'game-roundup',
   // Runtime text: counters, timer and the round results. Other fields keep their English raster.
   fonts:{fields:[252,253,257,273,277,280,281,282],chars:'0123456789 :=ABCDEFGHIJKLMNOPQRSTUVWXYZ?'},
   texts:{36:'Move the stylus or the Circle Pad around, "herding" as many puffles as you can into the brown pen.'},
   transform(data){
   // Ten interchangeable puffle slots (a herd repeats types): slot frame k shows pet k-1,
   // frame 11 is empty for escaped puffles. They replace pet0..pet9 above the pen art and
   // are reassigned by y every tick, like the web's sorted drawPets().
   const board=data.main,gameEntry=board.frames[1].find(e=>e.name==='game'),game=data.sprites[gameEntry.id];
   if(game.frames.length!==1)throw Error('Unexpected Roundup game clip');
   const pets=Array.from({length:10},(_,k)=>{const e=game.frames[0].find(x=>x.name==='pet'+k);if(!e)throw Error('Missing pet'+k);return e;});
   const slot=nextId(data);
   data.sprites[slot]={frames:[...pets.map(e=>[{depth:1,id:e.id,name:'pet',matrix:[1,0,0,1,0,0]}]),[]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
   const kept=game.frames[0].filter(e=>!/^pet\d$/.test(e.name??'')),top=Math.max(...kept.map(e=>e.depth));
   game.frames[0]=[...kept,...Array.from({length:10},(_,i)=>({depth:top+1+i,id:slot,name:'h'+i,matrix:[1,0,0,1,0,0]}))];
   return data;
  }}},
 },
 hydro:{
  code:700001,
  banks:{
   // The native draw order is under, the updateRope() line, main, title.
   // Main and the lazily loaded water bank (4 MiB) share Hydro's 8 MiB, so main keeps 4 MiB.
   main:{pack:'games/hydro/HydroHopper',textureBudget:4*1024*1024,transform(data){
    const {main,sprite,background,hazard,boat,player,crash}=hydroParts(data);
    const slots=(group,depth)=>Array.from({length:HYDRO_SLOTS},(_,i)=>({depth:depth+i,id:hazard.id,name:`${group}${i}`,matrix:[1,0,0,1,0,0]}));
    // Obstacles are drawn sorted by y around the boat (y 140) and the player.
    sprite.frames[0]=[...slots('a',10),{...boat,depth:100},...slots('b',110),{...player,depth:200},...slots('c',210),{...crash,depth:300}];
    // While the game clip is on stage, its background and water are drawn by the under bank.
    for(const frame of main.frames)if(frame.some(e=>e.name==='game')){const i=frame.findIndex(e=>e.depth===1&&e.id===background.id);if(i>=0)frame.splice(i,1);}
    // Every `body` under a holder whose script runs `new Color(body).setRGB(...)` takes the player colour.
    const bodies=new Set();
    for(const t of Object.values(data.sprites)){
     const source=Object.values(t.actions??{}).map(a=>Array.isArray(a)?'':a.source??'').join('\n');
     if(!/new Color\(body\)/.test(source))continue;
     for(const f of t.frames)for(const e of f)if(e.name==='body')bodies.add(e.id);
    }
    if(!bodies.size)throw Error('Hydro body holders not found');
    for(const id of bodies)addressable(data,id);
    return data;
   }},
   title:{pack:'games/hydro/lang/en/title'},
   under:{pack:'games/hydro/HydroHopper',transform(data){
    const {main,game,background,water,wake}=hydroParts(data);
    const holder=nextId(data);data.sprites[holder]={frames:[[{...water,depth:1},{...wake,depth:2}]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    main.frames=[[{...background},{depth:2,id:holder,name:'water_mc',matrix:game.matrix}]];main.labels={};main.actions={1:[['stop',null]]};
    // No text is drawn from this bank: skip the dynamic font alphabets.
    data.editTexts={};data.fonts={};
    // Wake pieces move individually (updateWake).
    for(const id of new Set(data.sprites[wake.id].frames[0].map(e=>e.id)))addressable(data,id);
    return data;
   }},
  },
 },
 'rescue-intro':{
  code:700044,
  banks:{main:{pack:'rescue-intro-anim',artScale:1,textureBudget:12*1024*1024}},
 },
 rescue:{
  code:700018,
  banks:{
   snow:{pack:'rescue',artScale:.4,fonts:{fields:[2199,2237],chars:'0123456789'},textureBudget:3*1024*1024,transform:data=>rescueBank(data,0,'snow','tilesnow','partssnow')},
   temple:{pack:'rescue',artScale:.4,fonts:{fields:[2199,2237],chars:'0123456789'},textureBudget:3*1024*1024,transform:data=>rescueBank(data,1,'temple','tiletemple','partstemple')},
   volcanoe:{pack:'rescue',artScale:.4,fonts:{fields:[2199,2237],chars:'0123456789'},textureBudget:3*1024*1024,transform:data=>rescueBank(data,2,'volcanoe','tilevolcanoe','partsvolcanoe')},
  },
 },
 'psa3-vault':{
  code:700031,
  banks:{main:{pack:'q3-vault',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-office':{
  code:700032,
  banks:{main:{pack:'q3-office',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-hq':{
  code:700033,
  banks:{main:{pack:'q3-hq',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-roof':{
  code:700034,
  banks:{main:{pack:'q3-roof',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-boiler':{
  code:700035,
  banks:{main:{pack:'q3-boiler',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-fusebox':{
  code:700036,
  banks:{main:{pack:'q3-fusebox',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 'psa3-nightclub':{
  code:700037,
  banks:{main:{pack:'q3-nightclub',artScale:.5,textureBudget:6*1024*1024,transform:data=>missionBank(data)}},
 },
 book1a:{
  code:700027,
  banks:{main:{pack:'book1',artScale:.3,textureBudget:3*1024*1024,transform:data=>bookBank(data,[4,5])}},
 },
 book1b:{
  code:700027,
  banks:{main:{pack:'book1',artScale:.3,textureBudget:3*1024*1024,transform:data=>bookBank(data,[6,7])}},
 },
 book1c:{
  code:700027,
  banks:{main:{pack:'book1',artScale:.3,textureBudget:3*1024*1024,transform:data=>bookBank(data,[8,9])}},
 },
 book2a:{
  code:700021,
  banks:{main:{pack:'book2',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[4,5,6,7,8])}},
 },
 book2b:{
  code:700021,
  banks:{main:{pack:'book2',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[9,10,11])}},
 },
 book2c:{
  code:700021,
  banks:{main:{pack:'book2',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[12,13])}},
 },
 book3a:{
  code:700022,
  banks:{main:{pack:'book3',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[3,4,5])}},
 },
 book3b:{
  code:700022,
  banks:{main:{pack:'book3',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[6,7,8])}},
 },
 book3c:{
  code:700022,
  banks:{main:{pack:'book3',artScale:.5,textureBudget:6*1024*1024,transform:data=>bookBank(data,[9,10,11,12])}},
 },
 waves:{
  code:700020,
  // Catchin' Waves, composed like the original gameplay frame (frame 3 of CatchinWaves.swf):
  // bg, map.water (the wave face at 58.4x), the four parallax foam layers, the crest curl,
  // shark, player, the breaking wave and the screeninterface HUD; plus the original title sign
  // and end-screen art. Root frame 2 holds lower-screen-only art (menu panels, red arrows, face texture).
  banks:{main:{pack:'games/waves/CatchinWaves',artScale:.5,textureBudget:6*1024*1024,
   fonts:{fields:[698,1610,1612,1613,1621,1623,1649,1650,1655,1658,1659,1660,1692,1694,1695,1696,1711,1713,1714,1715,1765,1767,1768,1769],chars:'0123456789'},
   transform(data){
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    let next=nextId(data);
    // The breaking wave's 5130x655 body (sprite 1262) has 8 drawings; the first is held (budget).
    {const body=data.sprites[1262];if(!body||body.frames.length!==8)throw Error('Unexpected wave body');const a=body.frames[0];body.frames=body.frames.map(()=>structuredClone(a));}
    // Its lip spray (sprite 1287) and foam overlay (sprite 1331) redraw every one of their 24 frames;
    // every sixth drawing is held for six frames (same art, 4 drawings per second) to fit the budget.
    for(const id of [1287,1331]){const t=data.sprites[id];if(!t||t.frames.length!==24)throw Error('Unexpected wave foam '+id);t.frames=t.frames.map((_,i)=>structuredClone(t.frames[i-i%6]));}
    const sprite=frames=>{const id=next++;data.sprites[id]={frames,labels:{},actions:Object.fromEntries(frames.map((_,i)=>[i+1,[['stop',null]]])),scriptFrames:[],sounds:[]};return id;};
    const mul=(a,b)=>[a[0]*b[0]+a[2]*b[1],a[1]*b[0]+a[3]*b[1],a[0]*b[2]+a[2]*b[3],a[1]*b[2]+a[3]*b[3],a[0]*b[4]+a[2]*b[5]+a[4],a[1]*b[4]+a[3]*b[5]+a[5]];
    const titleMain=data.main.frames[0].find(e=>e.name==='titlescreen');if(!titleMain)throw Error('Missing titlescreen');
    const menuFrame=data.sprites[titleMain.id].frames[data.sprites[titleMain.id].labels.main-1];
    // Title sign (CATCHIN' WAVES board + SELECT GAME MODE plank) for the upper menu.
    const signIds=new Set([1457,1458,1459,1460,726,1447]),signText=new Set(['titlet','select']);
    const sign=sprite([menuFrame.filter(e=>signIds.has(e.id)||signText.has(e.name)).map(e=>structuredClone(e))]);
    // Mode panels for the lower menu: the title frame without its text fields, lock badges and
    // the instructions button; the four panel buttons become their authored up-state art.
    const panels=[];
    const menu=sprite([menuFrame.flatMap(e=>{
     const c=data.characters[e.id];
     if(data.editTexts?.[e.id]||e.name==='memberLocks'||e.id===1453||signIds.has(e.id))return [];
     if(c?.kind==='button'){const up=c.records.up;return up.map((r,i)=>({depth:e.depth*4+i,id:r.id,matrix:mul(e.matrix,r.matrix),...(i===0&&e.name?{name:e.name}:{}),...(r.color?{color:r.color}:{})}));}
     return [{...structuredClone(e),depth:e.depth*4}];
    })]);
    // End screens: main frames endnormal..endcompetitionfirst without the Done button.
    const endFrames=['endnormal','endsurvivalgood','endsurvivalbad','endcompetitionthird','endcompetitionsecond','endcompetitionfirst']
     // 1615 is the plate under the Done button (the lower screen has the buttons).
     .map(label=>data.main.frames[data.main.labels[label]-1].filter(e=>data.characters[e.id]?.kind!=='button'&&e.id!==1615).map(e=>structuredClone(e)));
    const ends=sprite(endFrames);
    const arrow=data.sprites[symbol('screeninterface')].frames[0].find(e=>e.name==='gouparrow');
    const arrowArt=data.sprites[arrow.id].frames[1][0];
    const frame=[
     {id:1596,depth:1,name:'bg',matrix:[1,0,0,1,380,240]},
     {id:symbol('water'),depth:2,name:'water',matrix:[58.4,0,0,58.4,380,240]},
     {id:symbol('wavecurl'),depth:4,name:'curl',matrix:[1,0,0,1,110,240]},
     {id:symbol('watersurface2'),depth:5,name:'surface2',matrix:[1,0,0,1,380,742]},
     {id:symbol('watersurface3'),depth:6,name:'surface3',matrix:[1,0,0,1,380,742]},
     {id:symbol('watersurface4'),depth:7,name:'surface4',matrix:[1,0,0,1,380,742]},
     {id:symbol('shark'),depth:8,name:'shark',matrix:[1,0,0,1,-300,300]},
     {id:symbol('player'),depth:9,name:'player',matrix:[1,0,0,1,480,300]},
     {id:symbol('wave'),depth:10,name:'wave',matrix:[1,0,0,1,-1400,240]},
     {id:symbol('screeninterface'),depth:11,name:'hud',matrix:[1,0,0,1,0,0]},
     {id:sign,depth:12,name:'sign',matrix:[1,0,0,1,380,240]},
     {id:ends,depth:13,name:'ends',matrix:[1,0,0,1,0,0]},
    ];
    // Lower-screen art (root frame 2): menu panels around (380,240); the steering pad around x 1400.
    const lower=[
     {id:menu,depth:1,name:'menu',matrix:[1,0,0,1,380,240]},
     {id:symbol('water'),depth:2,name:'padwater',matrix:[40,0,0,40,1400,140]},
     ...['up','right','down','left'].map((n,i)=>{const a=i*Math.PI/2,c=Math.cos(a),sn=Math.sin(a),off=[[0,-1],[1,0],[0,1],[-1,0]][i];
      const base=[c,sn,-sn,c,1300+off[0]*74,240+off[1]*74];return {id:arrowArt.id,depth:3+i,name:'arrow'+n,matrix:mul(base,[1.5,0,0,1.5,arrowArt.matrix[4]*1.5,arrowArt.matrix[5]*1.5])};}),
    ];
    data.main={frames:[frame,lower],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
 dance:{
  code:700019,
  banks:{main:{pack:'dance',artScale:.35,textureBudget:6*1024*1024,fonts:{fields:[3046,3050],chars:'0123456789'},
   transform(data){
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    const gameMC=data.main.frames[2].find(entry=>entry.name==='gameMC');
    if(!gameMC)throw Error('Missing Dance gameMC');
    const parts=data.sprites[gameMC.id].frames[0];
    const background=parts.find(entry=>entry.name==='background'),arrows=parts.find(entry=>entry.name==='arrows');
    if(!background||!arrows)throw Error('Missing Dance stage');
    const frame=[{...background,depth:1},{...arrows,depth:2}];
    for(const dir of ['left','down','up','right'])for(const [part,prefix] of [['Stem','noteStem'],['Tail','noteBase'],['Note','notePress']]){
      const source=symbol(prefix+dir[0].toUpperCase()+dir.slice(1)),id=nextId(data);
      let entries=structuredClone(data.sprites[source].frames[0]);
      if(part==='Stem'){
        const height=data.characters[entries[0].id].size[1];
        entries=entries.map(entry=>{const matrix=[...(entry.matrix??[1,0,0,1,0,0])];for(const index of [1,3,5])matrix[index]/=height;return {...entry,matrix};});
      }
      data.sprites[id]={frames:[entries],labels:{},scriptFrames:[],sounds:[],actions:{1:[['stop',null]]}};
      for(let i=0;i<8;i++)frame.push({depth:900+frame.length,id,name:dir+part+i,matrix:[1,0,0,1,0,-10000]});
    }
    for(const timeline of [data.main,...Object.values(data.sprites)]){
      timeline.labels={...timeline.labels,en:1,english:1};
      for(const action of Object.values(timeline.actions??{}))if(Array.isArray(action))for(const command of action)if(command.length>1&&typeof command[1]==='string'&&command[1].startsWith('_root.myGame'))command[1]=1;
    }
    // Root frame 2: the 3DS touch-screen lane. The club backdrop without the crowd (background's first
    // child), the original receptors and notes, and the original score/combo clip (game.score).
    const backdrop=data.sprites[background.id].frames[0].find(entry=>entry.depth===1),score=parts.find(entry=>entry.name==='score');
    if(!backdrop||!score)throw Error('Missing Dance backdrop or score');
    const lower=[{...backdrop,depth:1,name:'wall'},...frame.slice(1),{...score,depth:3000,name:'score'}];
    data.main={frames:[frame,lower],labels:{},actions:{1:[['stop',null]],2:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
 mixmaster:{
  code:700017,
  banks:{main:{pack:'mixmaster',artScale:.5,textureBudget:6*1024*1024}},
 },
 aqua1:{
  code:700041,
  banks:{main:{pack:'aqua-baked1',artScale:1,textureBudget:12*1024*1024}},
 },
 aqua2:{
  code:700042,
  banks:{main:{pack:'aqua-baked2',artScale:1,textureBudget:12*1024*1024}},
 },
 'aqua-sub':{
  code:700043,
  banks:{main:{pack:'aqua-grabber',artScale:.5,textureBudget:2*1024*1024,
   transform(data){
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    const frame=[{id:symbol('sub'),depth:1,name:'sub',matrix:[1,0,0,1,0,0]},{id:symbol('puffle'),depth:2,name:'puffle',matrix:[1,0,0,1,0,-10000]},{id:symbol('claw'),depth:3,name:'claw',matrix:[1,0,0,1,0,-10000]}];
    // Frame 2 (touch screen): the same sub, puffle and claw plus the hudbar's sub (lives) and coin icons.
    const bar=data.sprites[symbol('hudbar')],show=bar.frames[bar.labels.show-1].find(e=>e.name==='art'),art=data.sprites[show.id].frames[0][0],icons=data.sprites[art.id].frames[0];
    // Each icon gets its own two-frame holder clip so the exporter keeps it a movable instance.
    const icon=x=>{const e=icons.find(e=>Math.abs(e.matrix[4]-x)<4&&data.sprites[e.id]);if(!e)throw Error('Missing Aqua hudbar icon');
     const id=nextId(data),inner=[{depth:1,id:e.id,name:'art',matrix:[1,0,0,1,0,0]}];data.sprites[id]={frames:[inner,structuredClone(inner)],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};return id;};
    const off=[1,0,0,1,0,-10000];
    data.main={frames:[frame,[...frame,{id:icon(241),depth:4,name:'hudsub',matrix:off},{id:icon(20),depth:5,name:'hudcoin',matrix:off}]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
 jetpack:{
  code:700016,
  banks:{main:{pack:'jetpack-1-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
 },
 // Original level backdrop: JetpackAdventures' background_mc (one frame per level), drawn behind the level bank.
 jetpackbg:{
  code:703106,
  banks:{main:{pack:'games/jetpack/JetpackAdventures',artScale:.5,textureBudget:4*1024*1024,transform(data){
   const holder=data.main.frames.flat().find(e=>e.name==='background_mc');if(!holder)throw Error('Missing Jet Pack background_mc');
   // Frame 2 is the touch-screen composition: the original interFace pieces (bar, fuelGuage with
   // its mask, lives_mc, coin) as separately placed instances; the controller lays them out.
   const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
   const hud=data.sprites[symbol('interFace')].frames[0],by=test=>{const e=hud.find(test);if(!e)throw Error('Missing interFace piece');return e;};
   const bar=by(e=>!e.name&&data.characters[e.id]?.kind==='shape'),fuel=by(e=>e.name==='fuelGuage'),lives=by(e=>e.name==='lives_mc'),coin=by(e=>!e.name&&data.sprites[e.id]&&e.id!==fuel.id&&e.id!==lives.id);
   addressable(data,fuel.id);const mask=data.sprites[fuel.id].frames[0].find(e=>e.name==='mask');if(!mask)throw Error('Missing fuel mask');addressable(data,mask.id);addressable(data,coin.id);
   const crisp=id=>{const walk=i=>{if(data.sprites[i]){for(const f of data.sprites[i].frames)for(const e of f)walk(e.id);}else if(data.characters[i]?.kind==='shape')data.characters[i].nativeRasterScale=1;};walk(id);};
   for(const e of [bar,fuel,lives,coin])crisp(e.id);
   const off=[1,0,0,1,0,-10000],title={...data.main.frames[0].find(e=>e.name==='titleScreen'),depth:6,name:'title'};if(!title.id)throw Error('Missing Jet Pack titleScreen');
   data.main={frames:[[{depth:1,id:holder.id,name:'bg',matrix:[1,0,0,1,0,0]}],
    [{depth:2,id:bar.id,name:'hudbar',matrix:off},{depth:3,id:fuel.id,name:'fuel',matrix:off},{depth:4,id:lives.id,name:'lives',matrix:off},{depth:5,id:coin.id,name:'coin',matrix:off}],
    // Frame 3: the original title art (titleScreen without its Flash buttons) for the touch-screen menu.
    [title]],
    labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
   return data;
  }}},
 },
 jetpack2:{
  code:700023,
  banks:{main:{pack:'jetpack-2-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
 },
 jetpack3:{
  code:700024,
  banks:{main:{pack:'jetpack-3-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
 },
 jetpack4:{
  code:700025,
  banks:{main:{pack:'jetpack-4-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
 },
 jetpack5:{
  code:700026,
  banks:{main:{pack:'jetpack-5-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
 },
 astro:{
  code:700015,
  banks:{main:{pack:'games/astro/AstroBarrier',artScale:.5,
   fonts:{fields:[68,69,74,75,76,77],chars:'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ '},
   transform(data){
    // The cabinet and the game live on main frame 3 of the original timeline.
    const frame=data.main.frames[2];
    // Root frame 2: the 3DS touch-screen control panel in screen pixels (320x240): the cabinet's
    // backdrop and its orange joystick knob (the original stage has no fire button art).
    const backdrop=frame.find(entry=>entry.depth===1),knob=frame.find(entry=>entry.depth===131);
    if(!backdrop||!knob)throw Error('Missing Astro backdrop or knob');
    const panel=[{...backdrop,name:'panelBg',matrix:[.5,0,0,.5,-35,-4]},{...knob,depth:3,name:'knob',matrix:[.8,0,0,.8,80,120]}];
    data.main={frames:[[...frame],panel],labels:{},actions:{1:[['stop',null]],2:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
};

games['debug-beans']={...games.beans,code:703003,banks:{main:games.beans.banks.main}};
/* Paint By Letters page banks (games/bookN-pK.gpk) and cover banks (games/bookN-cover.gpk), loaded one page at a time. */
for(const [id,code] of [['book1',700027],['book2',700021],['book3',700022]]){
 const book=booksData.books.find(b=>b.id===id);if(!book)continue;
 games[id+'-cover']={code,banks:{main:{pack:id,artScale:Number(process.env.BOOK_ART_SCALE??.65),textureBudget:4*1024*1024,transform:data=>bookCoverBank(data)}}};
 games[id+'-end']={code,banks:{main:{pack:id,artScale:Number(process.env.BOOK_ART_SCALE??.65),textureBudget:6*1024*1024,transform:data=>bookCoverBank(data,true)}}};
 book.pages.forEach((page,index)=>{
  games[`${id}-p${index+1}`]={code,banks:{main:{pack:id,artScale:Number(process.env.BOOK_ART_SCALE??.65),textureBudget:6*1024*1024,transform:data=>bookPageBank(data,page.frame)}}};
 });
}
for(let mission=4;mission<=11;mission++){
 const data=JSON.parse(await readFile(`${web}/public/data/mission${mission}.json`,'utf8'));
 let index=0;
 for(const [scene,info] of Object.entries(data.scenes)){
  const pack=String(info.pack).replace(/^\/swf\//,'').replace(/\/$/,'');
  games[`psa${mission}-${scene}`]={code:704000+mission*100+index++,banks:{main:{pack,artScale:Number(process.env.PSA_ART_SCALE??.5),textureBudget:12*1024*1024,transform:data=>missionBank(data)}}};
 }
}

/**
 * Player colour. The original games paint the penguin from clip scripts (`new Color(this).setRGB(<player colour>)`)
 * that the exported timelines do not run. web/tools/import-player-tints.py recovers those placements from the
 * decompiled scripts into web/public/data/player-tints.json; here they get the sentinel setRGB(0x01FE03), which
 * swl.c draws with the local player's colour (swl_player_color, set by game_port when a bank becomes ready).
 * Applied before each bank's own transform, because transforms rebuild the main timeline. Only the games ported
 * after v1.3.9 use it: the published games keep their hand-made tint lists (consistency with the public build).
 */
export const PLAYER_TINT_GAMES=new Set(['waves','dance','mixmaster','aqua-sub','jetpack','jetpack2','jetpack3','jetpack4','jetpack5','rescue','rescue-intro','book1a','book1b','book1c','book2a','book2b','book2c','book3a','book3b','book3c',...['book1','book2','book3'].flatMap(id=>[id+'-cover',id+'-end',...Array.from({length:10},(_,i)=>id+'-p'+(i+1))])]);
const playerTints=JSON.parse(await readFile(web+'/public/data/player-tints.json','utf8').catch(()=>'{}'));
const PLAYER_SENTINEL={mult:[0,0,0,1],add:[1,254,3,0]};
/** Resolves a tint entry to placements [sprite, depth, character]; sprite 0 is the main timeline. */
export function tintPlacements(data,entry){
 const timeline=id=>id===0?data.main:data.sprites[id],out=new Map(),add=p=>out.set(p.join(':'),p);
 for(const p of entry.placements)add(p);
 for(const [sprite,path] of entry.paths){
  let parents=[sprite];const names=path.split('.');
  names.forEach((name,i)=>{const next=[];for(const parent of parents)for(const frame of timeline(parent)?.frames??[])for(const e of frame){if(e.name!==name)continue;if(i===names.length-1)add([parent,e.depth,e.id]);else next.push(e.id);}parents=[...new Set(next)];});
 }
 return [...out.values()];
}
export function applyPlayerTint(data,pack){
 const entry=playerTints[pack];if(!entry)return 0;let count=0;
 for(const [sprite,depth,id] of tintPlacements(data,entry))for(const frame of (sprite===0?data.main:data.sprites[sprite])?.frames??[])for(const e of frame)if(e.depth===depth&&e.id===id){e.color=structuredClone(PLAYER_SENTINEL);count++;}
 return count;
}
for(const key of PLAYER_TINT_GAMES)for(const bank of Object.values(games[key]?.banks??{})){
 if(!playerTints[bank.pack])continue;
 /* Tint before the transform (the transform may rebuild the main timeline) and wrap what survives afterwards:
    the scene exporter bakes static groups with their colour, which paints the sentinel green into the atlas. */
 const own=bank.transform;bank.transform=data=>{applyPlayerTint(data,bank.pack);return unbakedTints(own?own(data):data);};
}

// Preload mode: patch fs/promises.readFile for the single timelines.json of the game.
if(process.env.GAME_EXPORT_PRELOAD){
 const [key,part]=process.env.GAME_EXPORT_PRELOAD.split(':'),game=games[key]?.banks[part];if(!game?.transform)throw Error('Unknown game preload');
 const require=createRequire(import.meta.url),fsp=require('node:fs/promises'),original=fsp.readFile;
 const target=path.resolve(web+'/public/swf/'+game.pack+'/timelines.json');let cached;
 fsp.readFile=async function(file,...rest){
  const virtual=typeof file==='string'&&game.virtualAssets?.get(path.resolve(file));
  if(virtual)return original.call(fsp,virtual,...rest);
  if(typeof file==='string'&&path.resolve(file)===target){
   if(!cached){const data=game.transform(JSON.parse(await original.call(fsp,file,'utf8')));if(process.env.BOOK_DUMP)require('node:fs').writeFileSync(process.env.BOOK_DUMP,JSON.stringify(data));
    // Script variables become SWL counters (8 per clip); keep only those a timeline loop reads.
    for(const t of [data.main,...Object.values(data.sprites)]){
     const used=new Set(Object.values(t.actions??{}).map(a=>!Array.isArray(a)&&a.loop?.counter).filter(Boolean));
     for(const a of Object.values(t.actions??{}))if(!Array.isArray(a)&&a.initial){for(const k of Object.keys(a.initial))if(!used.has(k))delete a.initial[k];if(!Object.keys(a.initial).length)delete a.initial;}
    }
    // SHELL.getLocalizedFrame() is the English frame 1 for these banks.
    for(const t of Object.values(data.sprites))for(const a of Object.values(t.actions??{})){const commands=Array.isArray(a)?a:a.commands??[];for(const c of commands)if(c[1]==='localized_frame')c[1]=1;}for(const [id,text] of Object.entries(game.texts??{})){if(!data.editTexts?.[id])throw Error('Missing text field '+id);data.editTexts[id].text=text;}cached=Buffer.from(JSON.stringify(data));}
   return rest[0]==='utf8'||rest[0]?.encoding==='utf8'?cached.toString('utf8'):Buffer.from(cached);
  }
  return original.call(fsp,file,...rest);
 };
 syncBuiltinESMExports();
 // Native font alphabets: the scene exporter gives every text style a full Latin-1
 // alphabet. Keep alphabets only for the fields the game changes at runtime and
 // only for the characters it writes; '?' stays as the missing-glyph fallback.
 if(game.fonts||game.artScale){
  const webRequire=createRequire(web+'/package.json'),resolved=webRequire.resolve('esbuild'),esbuild=webRequire('esbuild'),build=esbuild.build;
  // esbuild's exports are getters: replace the cached module object for the exporter's require().
  const wrapped={...esbuild};webRequire.cache[resolved].exports=wrapped;
 const line=`export {exportMovieFonts} from '${root}/tools/movie-fonts.mjs';`,movieLine="export {Movie} from './src/swf/movie';";
  wrapped.build=async function(options){
   const contents=options?.stdin?.contents;
   if(typeof contents==='string'&&game.artScale&&contents.includes(movieLine)){
    // Keep source vectors intact while baking. Apply the density hint only when
    // the exporter rasterizes a remaining primitive into its final atlas image.
    options={...options,stdin:{...options.stdin,contents:options.stdin.contents.replace(movieLine,`import {Movie} from './src/swf/movie';
const artLoad=Movie.load,artScale=${Number(game.artScale)};
const scaleArt=movie=>{for(const c of Object.values(movie.data.characters))if(c.nativeRasterScale===undefined)c.nativeRasterFactor=artScale;};
Movie.load=async function(...args){const movie=await artLoad.apply(this,args);scaleArt(movie);return movie;};
export {Movie};`)}};
   }
   if(typeof contents==='string'&&game.fonts&&contents.includes(line)){
    const contents=options.stdin.contents,keep=JSON.stringify(game.fonts.fields),chars=JSON.stringify([...new Set(game.fonts.chars+'?')].map(c=>c.charCodeAt(0)));
    options={...options,stdin:{...options.stdin,contents:contents.replace(line,`import {exportMovieFonts as allFonts} from '${root}/tools/movie-fonts.mjs';
export function exportMovieFonts(movie,usedArt,art,nextId){
 const keep=new Set(${keep}),chars=new Set(${chars}),used=new Set([...usedArt].filter(id=>!movie.data.editTexts?.[id]||keep.has(id)));
 const start=art.length,result=allFonts(movie,used,art,nextId),drop=new Set();
 for(const style of result.styles)for(const code of Object.keys(style.glyphs))if(!chars.has(Number(code))){const g=style.glyphs[code];drop.add(g.art);if(g.stroke)drop.add(g.stroke);delete style.glyphs[code];}
 for(const style of result.styles)for(const pair of Object.keys(style.kern)){const [a,b]=pair.split(':').map(Number);if(!chars.has(a)||!chars.has(b))delete style.kern[pair];}
 const kept=art.slice(start).filter(a=>!drop.has(a.id));art.splice(start,art.length-start,...kept);
 return result;
}`)}};
   }
   return build.call(esbuild,options);
  };
 }
}else if(import.meta.filename===path.resolve(process.argv[1]??'')){
 const key=process.argv[2],game=games[key];
 if(!game)throw Error('Usage: export-game.mjs '+Object.keys(games).join('|'));
 const {packMovie}=await import('./pack-movie.mjs');
 const debug=key.startsWith('debug-'),output=root+(debug?'/build/debug-resources/games':'/build/resources/games');
 await mkdir(output,{recursive:true});await mkdir(root+'/build/games',{recursive:true});
 if(key==='debug-card')await writeCardManifest(cardArt.icons);
 if(key==='beans'||key==='debug-beans'){
  const data=JSON.parse(await readFile(web+'/public/swf/game-beans/timelines.json'));
  const paths=beansModule.exports.prepareBeansArt(data,true).map(p=>'board.'+p);
  await writeFile(root+'/source/game_beans_colours.h','#pragma once\n#define BEANS_COLOUR_PATH_COUNT '+paths.length+'\nstatic const char *const BEANS_COLOUR_PATHS[]={'+paths.map(p=>JSON.stringify(p)).join(',')+'};\n');
 }
 const node=(args,preload,budget)=>new Promise((resolve,reject)=>{
  const child=spawn(process.execPath,preload?['--import',import.meta.filename,...args]:args,{cwd:root,stdio:'inherit',env:{...process.env,GAME_EXPORT_PRELOAD:preload??'',SWL_TEXTURE_BUDGET:budget?String(budget):''}});
  child.once('error',reject);child.once('exit',code=>code===0?resolve():reject(Error(args.join(' ')+' failed')));
 });
 const banks={};let offset=0;
 for(const [part,bank] of Object.entries(game.banks)){
  const suffix=part==='main'?'':'-'+part,name='game-'+key+suffix,destination=output+`/${key}${suffix}.gpk`;
  await node(['tools/export-scene.mjs',name,bank.pack,game.clip&&part==='main'?game.clip:'root','1','--external',...(process.env.SWL_BAKE_SCENE==='1'?['--bake']:[])],bank.transform?key+':'+part:undefined,bank.textureBudget);
  await packMovie(name,destination,{resource:game.code+offset,game:key+suffix});offset+=1000;
  banks[part]=path.relative(root,destination);
  for(const file of await readdir(root+'/build/scene-formats').catch(()=>[]))if(file.startsWith(`swl-${name}-`))await rm(root+'/build/scene-formats/'+file);
 }
 await writeFile(root+`/build/games/${key}.json`,JSON.stringify({key,code:game.code,banks},null,1)+'\n');
}
