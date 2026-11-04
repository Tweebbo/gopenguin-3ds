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
import {cardAssets,cardStage,cardActors,fireStage,writeCardManifest} from './card-art.mjs';

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
 for(let i=0;i<96;i++)frame.push({depth:1+i,id:tileId,name:'tile'+i,matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<40;i++)frame.push({depth:200+i,id:partId,name:'part'+i,matrix:[1,0,0,1,0,-10000]});
 const snowball=symbol('snowball'),bat=symbol('bat');
 for(let i=0;i<4;i++)frame.push({depth:300+i,id:snowball,name:'snowball'+i,matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<4;i++)frame.push({depth:320+i,id:bat,name:'bat'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:340,id:symbol('kraken'),name:'kraken',matrix:[1,0,0,1,0,-10000]});
 for(let i=0;i<2;i++)frame.push({depth:342+i,id:symbol('secretcoinballoon'),name:'secret'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:350,id:symbol('bg'),name:'bg',matrix:[1,0,0,1,0,0]});
 frame.push({depth:360,id:symbol('player'),name:'player',matrix:[1,0,0,1,0,-10000]});
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
function bookBank(data,pages){
 const frame=[];
 pages.forEach((page,index)=>{
  const art=(data.main.frames[page-1]??[]).find(entry=>entry.name==='art');
  if(!art)throw Error('Missing book page art '+page);
  const id=nextId(data);
  data.sprites[id]={frames:[data.sprites[art.id].frames[0]],labels:{},scriptFrames:[],sounds:[],actions:{1:[['stop',null]]}};
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
function jetpackBank(data){
 const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
 const tile=symbol('tile'),player=symbol('player');
 const frame=[];
 for(let i=0;i<96;i++)frame.push({depth:1+i,id:tile,name:'tile'+i,matrix:[1,0,0,1,0,-10000]});
 frame.push({depth:120,id:player,name:'player',matrix:[1,0,0,1,0,-10000]});
 data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
 return data;
}
const games={
 'debug-fire':{code:703002,banks:{main:{pack:'games/fire/fire',artScale:.5,textureBudget:8*1024*1024,fonts:{fields:[34],chars:'0123456789?'} ,virtualAssets:cardArt?.assets,transform:data=>fireStage(data,cardArt.icons)}}},
 'debug-card':{code:703001,banks:{
  main:{pack:'games/card/card',artScale:.5,textureBudget:4*1024*1024,fonts:{fields:[55,75,76],chars:'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz -!?'} ,virtualAssets:cardArt?.assets,transform:data=>cardStage(data,cardArt.icons)},
  actors:{pack:'games/card/battles/ambient',artScale:.5,textureBudget:4*1024*1024,virtualAssets:cardArt?.assets,transform:data=>cardActors(data,cardArt.walk)},
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
   snow:{pack:'rescue',artScale:.4,textureBudget:3*1024*1024,transform:data=>rescueBank(data,0,'snow','tilesnow','partssnow')},
   temple:{pack:'rescue',artScale:.4,textureBudget:3*1024*1024,transform:data=>rescueBank(data,1,'temple','tiletemple','partstemple')},
   volcanoe:{pack:'rescue',artScale:.4,textureBudget:3*1024*1024,transform:data=>rescueBank(data,2,'volcanoe','tilevolcanoe','partsvolcanoe')},
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
  banks:{main:{pack:'games/waves/CatchinWaves',artScale:.5,textureBudget:4*1024*1024,
   transform(data){
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    const frame=[
     {id:1596,depth:1,name:'bg',matrix:[1,0,0,1,380,240]},
     {id:symbol('watersurface4'),depth:2,name:'water',matrix:[1,0,0,1,380,300]},
     {id:symbol('player'),depth:3,name:'player',matrix:[1,0,0,1,380,300]},
     {id:symbol('shark'),depth:4,name:'shark',matrix:[1,0,0,1,380,300]},
    ];
    data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
 dance:{
  code:700019,
  banks:{main:{pack:'dance',artScale:.35,textureBudget:6*1024*1024,
   transform(data){
    const symbol=name=>{const id=Object.entries(data.exports).find(([,v])=>v===name)?.[0];if(!id)throw Error('Missing export '+name);return Number(id);};
    const gameMC=data.main.frames[2].find(entry=>entry.name==='gameMC');
    if(!gameMC)throw Error('Missing Dance gameMC');
    const parts=data.sprites[gameMC.id].frames[0];
    const background=parts.find(entry=>entry.name==='background'),arrows=parts.find(entry=>entry.name==='arrows');
    if(!background||!arrows)throw Error('Missing Dance stage');
    const frame=[{...background,depth:1},{...arrows,depth:2}];
    const names={up:'noteBaseUp',down:'noteBaseDown',left:'noteBaseLeft',right:'noteBaseRight'};
    for(const [dir,exportName] of Object.entries(names)){
      const source=symbol(exportName),id=nextId(data);
      data.sprites[id]={frames:[data.sprites[source].frames[0]],labels:{},scriptFrames:[],sounds:[],actions:{1:[['stop',null]]}};
      for(let i=0;i<8;i++)frame.push({depth:900+i,id,name:dir+'Note'+i,matrix:[1,0,0,1,0,-10000]});
    }
    for(const timeline of [data.main,...Object.values(data.sprites)]){
      timeline.labels={...timeline.labels,en:1,english:1};
      for(const action of Object.values(timeline.actions??{}))if(Array.isArray(action))for(const command of action)if(command.length>1&&typeof command[1]==='string'&&command[1].startsWith('_root.myGame'))command[1]=1;
    }
    data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
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
    data.main={frames:[frame],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
 jetpack:{
  code:700016,
  banks:{main:{pack:'jetpack-1-1',artScale:.5,textureBudget:6*1024*1024,transform:data=>jetpackBank(data)}},
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
    data.main={frames:[[...frame]],labels:{},actions:{1:[['stop',null]]},scriptFrames:[],sounds:[]};
    return data;
   }}},
 },
};

games['debug-beans']={...games.beans,code:703003,banks:{main:games.beans.banks.main}};
for(let mission=4;mission<=11;mission++){
 const data=JSON.parse(await readFile(`${web}/public/data/mission${mission}.json`,'utf8'));
 let index=0;
 for(const [scene,info] of Object.entries(data.scenes)){
  const pack=String(info.pack).replace(/^\/swf\//,'').replace(/\/$/,'');
  games[`psa${mission}-${scene}`]={code:704000+mission*100+index++,banks:{main:{pack,artScale:.5,textureBudget:12*1024*1024,transform:data=>missionBank(data)}}};
 }
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
   if(!cached){const data=game.transform(JSON.parse(await original.call(fsp,file,'utf8')));
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
