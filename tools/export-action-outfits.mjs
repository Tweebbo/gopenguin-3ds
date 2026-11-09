// Secret item actions beyond the 2010 penguin's 76 frames (77-161): the body comes from the original
// per-action penguins (assets/penguin-actions/<frame>, frame 2) into avatars/2.gpa, and each worn item's
// own frames into outfits/<item+500000>.gpa. Same capture and encoding as export-outfits.mjs.
import {createRequire} from 'node:module';
import {readFile,writeFile,mkdir,access} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {spawn} from 'node:child_process';
import path from 'node:path';
import {packRectangles} from './pack-outfit-rectangles.mjs';
import {encodeSprite} from './sprite-format.mjs';
const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB??root+'/../../GoPenguin-Gameplay-PC-20261101/web');
const require=createRequire(web+'/package.json'),{chromium}=require('playwright'),{build}=require('esbuild');
const rules=JSON.parse(await readFile(web+'/src/engine/frame-hacks.json','utf8')),slots=['head','face','neck','body','hand','feet'];
const BASE=76,EXTRA=500000,POSE=2,extra=new Set(JSON.parse(await readFile(web+'/public/swf/assets/penguin-actions/index.json','utf8'))),max=Math.max(...extra);
const actions=[...new Set(Object.values(rules).flat().map(r=>r.secret_frame).filter(a=>a>BASE&&extra.has(a)))].sort((a,b)=>a-b);
const items=new Map();for(const combinations of Object.values(rules))for(const rule of combinations)if(rule.secret_frame>BASE&&extra.has(rule.secret_frame))for(const slot of slots)if(rule[slot]){const list=items.get(rule[slot])??new Set();list.add(rule.secret_frame);items.set(rule[slot],list);}
const SPLIT=128,jobs=[{id:1,file:2,kind:'avatars',actions:actions.filter(a=>a<=SPLIT),avatar:true},{id:1,file:3,kind:'avatars',actions:actions.filter(a=>a>SPLIT),avatar:true}],missing=[];
for(const [id,frames] of [...items].sort((a,b)=>a[0]-b[0]))try{await access(web+`/public/swf/assets/clothing/sprites/${id}/timelines.json`);jobs.push({id,file:id+EXTRA,kind:'outfits',actions:[...frames].sort((a,b)=>a-b),avatar:false});}catch{missing.push(id);}
const folder=root+'/build/outfit-extra-export';await mkdir(folder,{recursive:true});
const bundle=(await build({stdin:{contents:"export {Movie,setRGB} from './src/swf/movie';",resolveDir:web},bundle:true,format:'iife',globalName:'Native',write:false})).outputFiles[0].text;
const browser=await chromium.launch({headless:true,executablePath:'/usr/bin/google-chrome',args:['--mute-audio']});const results=[];
try{
 const page=await browser.newPage();page.setDefaultTimeout(0);
 await page.route('http://outfits.test/**',async route=>{const name=new URL(route.request().url()).pathname;if(name==='/')return route.fulfill({body:'<!doctype html>'});const file=path.resolve(web+'/public','.'+decodeURIComponent(name));if(!file.startsWith(web+'/public/'))return route.abort();try{await route.fulfill({body:await readFile(file),contentType:file.endsWith('.json')?'application/json':file.endsWith('.svg')?'image/svg+xml':'application/octet-stream'});}catch{return route.fulfill({status:404,body:'Not found'});}});
 await page.goto('http://outfits.test/');await page.addScriptTag({content:bundle});
 for(const job of jobs){
  const work=folder+`/${job.kind}-${job.file}`;await mkdir(work,{recursive:true});const destination=root+`/romfs/${job.kind}/${job.file}.gpa`;await mkdir(path.dirname(destination),{recursive:true});
  const source=web+(job.avatar?`/public/swf/assets/penguin-actions/${job.actions[0]}`:`/public/swf/assets/clothing/sprites/${job.id}`);
  const recipe=createHash('sha256').update(await readFile(import.meta.filename)).update(await readFile(source+'/timelines.json')).update(bundle).digest('hex');
  try{const prior=JSON.parse(await readFile(work+'/manifest.json','utf8'));if(createHash('sha256').update(await readFile(destination)).digest('hex')===prior.sha256){results.push(prior);console.log('CACHED',job.kind,job.id);continue;}}catch{}
  const capture=job=>page.evaluate(async(job)=>{
   const load=async action=>{const movie=await Native.Movie.load(job.avatar?`/swf/assets/penguin-actions/${action}/`:`/swf/assets/clothing/sprites/${job.id}/`);movie.muted=true;
    if(job.avatar){movie.set('ring',{visible:false});movie.set('hotspot',{visible:false});movie.set('body',{color:Native.setRGB(0xffffff)});}return movie;};
   let movie=await load(job.actions[0]);
   const size=job.big?384:192,originX=size/2,originY=size*.75,scale=.5,layers=job.avatar?2:1;
   const canvases=Array.from({length:layers},()=>{const c=document.createElement('canvas');c.width=c.height=size;return c;}),contexts=canvases.map(c=>c.getContext('2d',{willReadFrequently:true}));
   let symbols,captures=[];
   for(const action of job.actions){
    if(job.avatar)movie=await load(action);symbols=new Map([[movie.data.main,0],...Object.entries(movie.data.sprites).map(([id,s])=>[s,Number(id)])]);
    const shown=job.avatar?2:action;
    await movie.preloadAnimation(shown);movie.resetPlayback();const seen=new Map(),frames=Array.from({length:layers},()=>[]),art=[],images=new Map();let loop=0,complete=false;
    for(let tick=0;tick<2048;tick++){
     for(const ctx of contexts){ctx.setTransform(1,0,0,1,0,0);ctx.clearRect(0,0,size,size);ctx.setTransform(scale,0,0,scale,originX,originY);}
     movie.beginDraw();if(job.avatar){for(const e of movie.rootEntries(shown))movie.drawRootEntry(contexts[e.name==='body'?0:1],e,true);}else movie.draw(contexts[0],shown);
     await movie.ready();
     const signature=JSON.stringify([...movie.playheads].filter(([,h])=>h.seen).map(([p,h])=>[p,symbols.get(h.timeline),h.frame,h.playing,h.counters]).sort((a,b)=>a[0].localeCompare(b[0])));
     if(seen.has(signature)){loop=seen.get(signature);complete=true;break;}seen.set(signature,tick);
     for(let layer=0;layer<layers;layer++){
      const ctx=contexts[layer],pixels=ctx.getImageData(0,0,size,size).data;let x0=size,y0=size,x1=0,y1=0;
      for(let y=0;y<size;y++)for(let x=0;x<size;x++)if(pixels[(y*size+x)*4+3]){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
      if(x1<=x0||y1<=y0)frames[layer].push({});else{
       if(!x0||!y0||x1===size||y1===size)throw Error(`Capture clipped: ${job.kind}/${job.id}, action ${action}`);
       const image=document.createElement('canvas');image.width=x1-x0;image.height=y1-y0;image.getContext('2d').drawImage(canvases[layer],x0,y0,image.width,image.height,0,0,image.width,image.height);
       const png=image.toDataURL();let id=images.get(png);if(id===undefined){id=art.length;images.set(png,id);art.push({id,w:image.width,h:image.height,png});}
       frames[layer].push({art:id,left:(x0-originX)/scale,top:(y0-originY)/scale});
      }
     }
     movie.advance();
    }
    if(!complete)return {dynamic:true};captures.push({action,frames,loop,art});
   }
   return {captures,scale};
  },job);
  let data;try{data=await capture(job);}catch(error){if(!/Capture clipped/.test(error.message))throw error;console.log('RETRY larger capture',job.kind,job.id);try{data=await capture({...job,big:true});}catch(again){if(job.avatar||!/Capture clipped/.test(again.message))throw again;console.log('SKIP full-screen secret action art',job.id);missing.push(job.id);continue;}}
  if(data.dynamic){throw Error(`Dynamic secret action art is not supported for ${job.kind}/${job.id}`);
   const child=spawn(process.execPath,['tools/export-outfit-movie.mjs',String(job.id)],{cwd:root,stdio:'inherit'});if(await new Promise(resolve=>child.on('exit',resolve)))throw Error('Dynamic outfit export failed');
   const binary=await readFile(destination),result={...job,dynamic:true,recipe,sha256:createHash('sha256').update(binary).digest('hex'),bytes:binary.length,maxResidentPageBytes:2*1024*1024};
   await writeFile(work+'/manifest.json',JSON.stringify(result,null,2)+'\n');results.push(result);continue;
  }
  const frames=[{}],sequences=[],textures=[],poses=new Map();
  for(const capture of data.captures){
   let dimensions;for(const [width,height] of [[64,64],[128,128],[256,256],[512,512],[1024,512]])try{packRectangles(capture.art,width,height);dimensions={width,height};break;}catch{}
   if(!dimensions)throw Error(`Outfit ${job.id}/${capture.action} exceeds one page`);
   const png=await page.evaluate(async({art,width,height})=>{const c=document.createElement('canvas');c.width=width;c.height=height;const ctx=c.getContext('2d');for(const e of art){const image=new Image();image.src=e.png;await image.decode();ctx.drawImage(image,e.x,e.y);}return c.toDataURL();},{art:capture.art,...dimensions});
   const base=work+`/${capture.action}`;await writeFile(base+'.png',Buffer.from(png.split(',')[1],'base64'));
   const child=spawn('podman',['run','--rm','--network','none','--cpus','2','--memory','1g','-v',root+':/work:z','-w','/work',process.env.DEVKITARM_IMAGE??'localhost/native-3ds-libctru-fixed:latest','tex3ds','-f','rgba8888','-z','auto','-o',path.relative(root,base+'.t3x'),path.relative(root,base+'.png')],{stdio:'inherit'});
   if(await new Promise(resolve=>child.on('exit',resolve)))throw Error('tex3ds failed');const index=textures.length;textures.push({...dimensions,data:await readFile(base+'.t3x')});
   capture.frames.forEach((list,layer)=>{const start=frames.length;for(const f of list)frames.push(f.art===undefined?{}:{...capture.art[f.art],left:f.left,top:f.top});poses.set(capture.action+':'+(layer+1),{start,count:list.length,loop:capture.loop,page:index});});
  }
  for(let action=1;action<=max;action++)for(let variation=1;variation<=(job.avatar?2:1);variation++)sequences.push({...poses.get(action+':'+variation)??{start:0,count:1,loop:0,page:0},rotation:action,variation});
  const binary=encodeSprite({frames,sequences,textures,scale:data.scale});await writeFile(destination,binary);
  const result={...job,recipe,sha256:createHash('sha256').update(binary).digest('hex'),bytes:binary.length,pages:textures.length,maxResidentPageBytes:Math.max(...textures.map(t=>t.width*t.height*4)),poses:data.captures.map(c=>({action:c.action,count:c.frames[0].length,loop:c.loop,empty:c.art.length===0}))};
  await writeFile(work+'/manifest.json',JSON.stringify(result,null,2)+'\n');results.push(result);console.log('EXPORTED',job.kind,job.id,job.actions.length,'actions',binary.length,'bytes');
 }
}finally{await browser.close();}
await mkdir(root+'/assets/outfits',{recursive:true});await writeFile(root+'/assets/outfits/extra-manifest.json',JSON.stringify({source:'Original per-action penguins and clothing timelines',firstAction:BASE+1,maximumAction:max,avatarActions:actions,missingClothing:missing,results},null,2)+'\n');
const available=results.filter(r=>!r.avatar).map(r=>r.id).sort((a,b)=>a-b);
await writeFile(root+'/source/outfit_extra_assets.h','#pragma once\n/* Generated by tools/export-action-outfits.mjs: items with original secret actions 77-'+max+'. */\nstatic const unsigned outfit_extra_items[]={'+available.join(',')+'};\nstatic const unsigned char outfit_extra_actions[]={'+actions.join(',')+'};\n#define OUTFIT_EXTRA_BASE '+EXTRA+'\n#define OUTFIT_EXTRA_AVATAR 2\n#define OUTFIT_EXTRA_AVATAR_SPLIT '+SPLIT+' /* avatars/3.gpa holds the actions above it */\n#define OUTFIT_EXTRA_MAX_ACTION '+max+'\n');
console.log('PASS',results.length,'outfit resources;',missing.length,'unavailable original clothing IDs:',missing.join(','));
