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
 if(process.argv[2]==='debug-fire'||process.env.GAME_EXPORT_PRELOAD?.startsWith('debug-fire:')){
  const fire=JSON.parse(await readFile(root+'/build/debug-prepared/public/swf/card-fire/timelines.json'));
  for(const art of Object.values(fire.characters))for(const item of [art,...Object.values(art.states??{})])for(const field of ['file','maskFile'])if(item[field])assets.set(path.resolve(web+'/public/swf/games/fire/fire/chars/'+item[field]),root+'/build/debug-prepared/public/swf/card-fire/chars/'+item[field]);
  for(const i of icons)assets.set(path.resolve(web+'/public/swf/games/fire/fire/chars/debug-icon-'+i.id+'.png'),folder+'/'+i.id+'.swf/1.png');
 }
 return {assets,icons,walk};
}
export function cardStage(data,icons){
 const lookup=5000;data.sprites[lookup]={frames:icons.map((icon,i)=>{
  const id=6000+i;data.characters[id]={kind:'shape',file:icon.file,size:[icon.width/.23,icon.height/.23],origin:[0,0],nativeRasterScale:1};return [{id,depth:1,matrix:matrix()}];
 }),labels:{},actions:{1:[['stop',null]]},sounds:[]};
 // Keep each tint/number/icon as its own instance, including single-frame clips.
 for(const id of [53,56])still(data.sprites[id]);
 for(const frame of data.sprites[67].frames)for(const e of frame){if(e.name==='mc_art')e.id=lookup;if(e.name==='mc_glow')e.name='card_glow';}
 const rootEntries=data.main.frames[0].filter(e=>[1,4,5,6].includes(e.depth));
 rootEntries.push({id:67,name:'pick0',depth:100,matrix:matrix(225,50,.4)},{id:67,name:'pick1',depth:101,matrix:matrix(425,50,.4)});
 for(let seat=0;seat<2;seat++)for(let slot=0;slot<9;slot++)rootEntries.push({id:67,name:`score${seat}_${slot}`,depth:110+seat*10+slot,matrix:matrix((seat?540:15)+slot%3*63,15+Math.floor(slot/3)*19,.14)});
 const lower=[];for(let i=0;i<5;i++)lower.push({id:67,name:'hand'+i,depth:10+i,matrix:matrix(15+i*150,135,.55)});
 data.main={frames:[rootEntries,lower],labels:{battle:1,hand:2},actions:{1:[['stop',null]],2:[['stop',null]]},sounds:[]};
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
