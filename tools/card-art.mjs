import {readFile,readdir,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB??root+'/../web-server/web');
const matrix=(x=0,y=0,scale=1)=>[scale,0,0,scale,x,y];
const still=timeline=>{if(timeline.frames.length===1){timeline.frames.push(structuredClone(timeline.frames[0]));timeline.actions={1:[['stop',null]]};}};
export async function cardAssets(){
 const folder=root+'/build/debug-card-icons',assets=new Map(),icons=[];
 for(const name of (await readdir(folder)).sort((a,b)=>parseInt(a)-parseInt(b))){
  if(!/^\d+\.swf$/.test(name))continue;
  const id=parseInt(name),file=folder+'/'+name+'/1.png',png=await readFile(file);
  if(png.toString('hex',0,8)!=='89504e470d0a1a0a')throw Error('Invalid original card frame '+id);
  const virtual='debug-icon-'+id+'.png';assets.set(path.resolve(web+'/public/swf/games/card/card/chars/'+virtual),file);
  icons.push({id,file:virtual,width:png.readUInt32BE(16),height:png.readUInt32BE(20)});
 }
 if(icons.length!==509)throw Error('Expected all 509 original card illustrations; got '+icons.length);
 const walk=JSON.parse(await readFile(root+'/build/debug-prepared/public/swf/card-walk/timelines.json'));
 const ambient=JSON.parse(await readFile(root+'/build/debug-prepared/public/swf/card-ambient/timelines.json'));
 for(const art of Object.values(ambient.characters))for(const item of [art,...Object.values(art.states??{})])for(const field of ['file','maskFile'])if(item[field])assets.set(path.resolve(web+'/public/swf/games/card/battles/ambient/chars/'+item[field]),root+'/build/debug-prepared/public/swf/card-ambient/chars/'+item[field]);
 for(const art of Object.values(walk.characters))for(const item of [art,...Object.values(art.states??{})])for(const field of ['file','maskFile'])if(item[field])assets.set(path.resolve(web+'/public/swf/games/card/battles/ambient/chars/debug-walk-'+item[field]),root+'/build/debug-prepared/public/swf/card-walk/chars/'+item[field]);
 // The dojo bank (Sensei menu pack) reuses the board background of the card pack: unpack its vectors once
 // and serve them to the Sensei pack under a card- prefix.
 const cardDir=web+'/public/swf/games/card/card',board=JSON.parse(await readFile(cardDir+'/timelines.json')),unpacked=root+'/build/card-chars';
 await mkdir(unpacked,{recursive:true});const vectors=await readFile(cardDir+'/chars/'+board.pack.file);
 for(const [file,[offset,length]] of Object.entries(board.pack.entries)){await writeFile(unpacked+'/'+file,vectors.subarray(offset,offset+length));assets.set(path.resolve(web+'/public/swf/games/sensei/SenseiWidget/chars/card-'+file),unpacked+'/'+file);}
 if(process.argv[2]==='debug-fire'||process.env.GAME_EXPORT_PRELOAD?.startsWith('debug-fire:')){
  const fire=JSON.parse(await readFile(root+'/build/debug-prepared/public/swf/card-fire/timelines.json'));
  for(const art of Object.values(fire.characters))for(const item of [art,...Object.values(art.states??{})])for(const field of ['file','maskFile'])if(item[field])assets.set(path.resolve(web+'/public/swf/games/fire/fire/chars/'+item[field]),root+'/build/debug-prepared/public/swf/card-fire/chars/'+item[field]);
  for(const i of icons)assets.set(path.resolve(web+'/public/swf/games/fire/fire/chars/debug-icon-'+i.id+'.png'),folder+'/'+i.id+'.swf/1.png');
 }
 return {assets,icons,walk,board};
}
const scaled=(x,y,sx,sy=sx)=>[sx,0,0,sy,x,y];
const nextId=data=>1+Math.max(...Object.keys(data.sprites).map(Number),...Object.keys(data.characters).map(Number),...Object.keys(data.editTexts??{}).map(Number));
/** Card-Jitsu board bank. Frame 1 (upper screen, drawn over the dojo and fighters): board panel, names, both hands,
 *  played cards and won-card thumbnails at the original card.ts poses. Frame 2 (touch screen, 1 px = 2.375 stage
 *  units): the help scroll paper, the original clock and the hand as large card faces. Frame 3: the help scroll. */
export function cardStage(data,icons){
 const lookup=5000;data.sprites[lookup]={frames:icons.map((icon,i)=>{
  const id=6000+i;data.characters[id]={kind:'shape',file:icon.file,size:[icon.width/.23,icon.height/.23],origin:[0,0],nativeRasterScale:1};return [{id,depth:1,matrix:matrix()}];
 }),labels:{},actions:{1:[['stop',null]]},sounds:[]};
 // Keep each tint/number/icon as its own instance, including single-frame clips.
 for(const id of [53,56,66])still(data.sprites[id]);
 for(const frame of data.sprites[67].frames)for(const e of frame){if(e.name==='mc_art')e.id=lookup;if(e.name==='mc_glow')e.name='card_glow';}
 const original=Object.fromEntries(data.main.frames[0].map(e=>[e.depth,e]));
 const upper=[{...original[7],name:'board'},{...original[9]},{...original[10]}];
 for(let seat=0;seat<2;seat++)for(let slot=0;slot<5;slot++)upper.push({id:67,name:`seat${seat}_${slot}`,depth:20+seat*5+slot,matrix:matrix(seat?710-(slot+1)*35:50+slot*35,385,.15)});
 for(let seat=0;seat<2;seat++)for(let e=0;e<3;e++)for(let i=0;i<8;i++)upper.push({id:67,name:`score${seat}_${e}_${i}`,depth:100+seat*30+e*10+(7-i),matrix:matrix((seat?550:50)+e*50,25+i*15,.4)});
 upper.push({id:67,name:'pick0',depth:200,matrix:matrix(225,150,.4)},{id:67,name:'pick1',depth:201,matrix:matrix(425,150,.4)});
 const lower=[{id:102,name:'paper',depth:1,matrix:scaled(-12,40,1.69,2.0)},{id:101,name:'mc_clock',depth:12,matrix:scaled(330,92,1.25)}];
 for(let i=0;i<5;i++)lower.push({id:67,name:'hand'+i,depth:20+i,matrix:matrix(12+i*149.6,380,.6)});
 data.sprites[112].frames=data.sprites[112].frames.map(frame=>frame.filter(e=>e.name!=='tf_help'));
 const help=[{id:102,name:'paper',depth:1,matrix:scaled(-12,40,1.69,2.0)},{id:112,name:'help',depth:2,matrix:scaled(78,40,1.3)}];
 data.main={frames:[upper,lower,help],labels:{battle:1,hand:2,help:3},actions:{1:[['stop',null]],2:[['stop',null]],3:[['stop',null]]},sounds:[]};
 return data;
}
/** Dojo bank from the Sensei menu pack: frame 1 = the board's dojo background (card pack art), frame 2 = the
 *  Sensei menu scene for the upper screen, frame 3 = the options scroll with its belt/Sensei/instructions icons. */
export function dojoStage(data,board){
 const offset=30000,copied=new Set();
 const visit=id=>{
  if(copied.has(id))return;copied.add(id);
  const sprite=board.sprites[id];
  if(sprite){const copy=structuredClone(sprite);for(const frame of copy.frames)for(const e of frame){visit(e.id);e.id+=offset;}data.sprites[id+offset]=copy;return;}
  const art=structuredClone(board.characters[id]);if(!art)throw Error('Missing board symbol '+id);
  for(const item of [art,...Object.values(art.states??{})])for(const field of ['file','maskFile'])if(item[field])item[field]='card-'+item[field];
  data.characters[id+offset]=art;
 };
 const background=board.main.frames[0].find(e=>e.depth===1);visit(background.id);
 const texts=new Set(Object.keys(data.editTexts??{}).map(Number)),menus=631,options=625;
 const strip=frame=>frame.filter(e=>!texts.has(e.id)&&data.characters[e.id]?.kind!=='button');
 for(const sprite of Object.values(data.sprites))sprite.frames=sprite.frames.map(strip);
 // Only the Sensei's idle pose: the other poses belong to the first-time tutorial.
 const pose=data.sprites[617];pose.frames=[pose.frames[1],structuredClone(pose.frames[1])];pose.labels={};pose.actions={1:[['stop',null]]};
 const menu=data.sprites[menus];menu.frames=menu.frames.map(frame=>frame.filter(e=>!['howtoplay','options','background'].includes(e.name)));
 const backdrop=nextId(data);data.sprites[backdrop]={frames:[menu.frames[0].filter(e=>[1,2,9].includes(e.depth))],labels:{},actions:{1:[['stop',null]]},sounds:[]};still(data.sprites[backdrop]);
 const panel=data.sprites[options].frames[0].find(e=>e.name==='background');
 const top=[{id:menus,name:'menu',depth:1,matrix:scaled(57,344.9,1)}];
 const scroll=[{id:backdrop,name:'backdrop',depth:1,matrix:scaled(57*1.25-95,344.9*1.25-130,1.25)},{id:panel.id,name:'panel',depth:2,matrix:scaled(36,74,2.05,1.85)}];
 [35,31,33].forEach((icon,i)=>scroll.push({id:icon,name:'icon'+i,depth:10+i,matrix:scaled(100,108+i*100,.62)}));
 data.main={frames:[[{...background,id:background.id+offset,name:'dojo'}],top,scroll],labels:{dojo:1,menu:2,options:3},actions:{1:[['stop',null]],2:[['stop',null]],3:[['stop',null]]},sounds:[]};
 data.editTexts={};data.fonts={};
 return data;
}
export function fireStage(data,icons){
 const lookup=5000;data.sprites[lookup]={frames:icons.map((icon,i)=>{const id=6000+i;data.characters[id]={kind:'shape',file:'debug-icon-'+icon.id+'.png',size:[icon.width/.23,icon.height/.23],origin:[0,0],nativeRasterScale:1};return [{id,depth:1,matrix:matrix()}];}),labels:{},actions:{1:[['stop',null]]},sounds:[]};
 for(const frame of data.sprites[51].frames)for(const e of frame)if(e.name==='mc_art')e.id=lookup;
 data.sprites[51].frames=data.sprites[51].frames.map(frame=>frame.filter(e=>e.name!=='hitarea_mc'));
 for(const id of [32,35])still(data.sprites[id]);
 const fire=Number(data.main.frames[0].find(e=>e.name==='fire_mc').id),root=data.sprites[fire];
 root.frames=root.frames.map(frame=>frame.filter(e=>['bkg_topArt','board_mc','bkg_frontArt'].includes(e.name)));still(root);
 const upper=[{id:fire,name:'fire_mc',depth:1,matrix:matrix()}];
 for(let seat=0;seat<4;seat++){
  upper.push({id:319,name:'piece'+seat,depth:10+seat,matrix:matrix(380,240,.5)});
  upper.push({id:51,name:'battle'+seat,depth:30+seat,matrix:matrix(95+seat*150,175,.55)});
 }
 const lower=[];for(let i=0;i<5;i++)lower.push({id:51,name:'hand'+i,depth:10+i,matrix:matrix(15+i*150,135,.55)});
 data.main={frames:[upper,lower],labels:{board:1,hand:2},actions:{1:[['stop',null]],2:[['stop',null]]},sounds:[]};
 return data;
}
export function cardActors(data,walk){
 const offset=10000;
 for(const [id,art] of Object.entries(walk.characters)){const copy=structuredClone(art);for(const item of [copy,...Object.values(copy.states??{})])for(const field of ['file','maskFile'])if(item[field])item[field]='debug-walk-'+item[field];data.characters[Number(id)+offset]=copy;}
 for(const [id,sprite] of Object.entries(walk.sprites)){
  const copy=structuredClone(sprite);for(const frame of copy.frames)for(const e of frame)e.id+=offset;data.sprites[Number(id)+offset]=copy;
 }
 const ambient=Number(Object.entries(data.exports).find(([,name])=>name==='ambient')[0]),walkId=Number(Object.entries(walk.exports).find(([,name])=>name==='walk')[0])+offset;
 for(const t of Object.values(data.sprites))for(const frame of t.frames)for(const e of frame)if(['body_mc','frontArm_mc','backArm_mc','belt_mc'].includes(e.name)&&data.sprites[e.id])still(data.sprites[e.id]);
 const entries=id=>[0,1].map(seat=>({id,depth:seat+1,name:'fighter'+seat,matrix:[seat? .7:-.7,0,0,.7,380,270]}));
 data.main={frames:[entries(ambient),entries(walkId)],labels:{ambient:1,walk:2},actions:{1:[['stop',null]],2:[['stop',null]]},sounds:[]};
 data.editTexts={};data.fonts={};return data;
}
export async function writeCardManifest(icons){
 await mkdir(root+'/build/debug-checks',{recursive:true});
 const source=web+'/public/swf/games/card/card/timelines.json';
 const result={source:JSON.parse(await readFile(source)).source,icons:icons.map((icon,i)=>({id:icon.id,frame:i+1})),originalArtworkOnly:true,debugOnly:true};
 await writeFile(root+'/build/debug-checks/card-art.json',JSON.stringify(result,null,2)+'\n');
 const lines=['#pragma once','#include <stdint.h>',`#define CARD_ICON_COUNT ${icons.length}`,`static const unsigned CARD_ICON_IDS[CARD_ICON_COUNT]={${icons.map(i=>i.id).join(',')}};`];
 await writeFile(root+'/source/game_card_art.h',lines.join('\n')+'\n');
 return createHash('sha256').update(await readFile(source)).digest('hex');
}
