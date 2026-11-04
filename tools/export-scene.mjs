import {createRequire} from 'node:module';
import {readFile,writeFile,mkdir,readdir,rm} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {encodeScene,packRectangles,limits} from './swl-format.mjs';
import {deduplicateTimelines} from './dedup-scene.mjs';
import {encodeNavigation} from './navigation-format.mjs';

export async function exportScene(args=process.argv,shared){
const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB??root+'/../../GoPenguin-Gameplay-PC-20261101/web');
const positional=args.slice(2).filter(arg=>!arg.startsWith('--'));
const name=positional[0]??'town';if(!/^[a-z][a-z0-9-]*$/.test(name))throw Error('Invalid room name');
const igloo=args.includes('--igloo'),library=!igloo&&positional[1]!==undefined,pack=positional[1]??name,clip=positional[2]??'root',initial=Number(positional[3]??1);
const pageOnly=args.includes('--page'),external=args.includes('--external');
const referenceTicks=args.includes('--single-reference')?[0]:[0,1,12,24,48,96];
const newspaper=args.includes('--newspaper');
const roomBakeScale=Number(process.env.SWL_ROOM_BAKE_SCALE??.5);
if(!Number.isFinite(roomBakeScale)||roomBakeScale<.5||roomBakeScale>1)throw Error('Room bake scale must be between 0.5 and 1.');
const catalogue=args.find(value=>value.startsWith('--catalogue='))?.slice(12)??'';
if(!/^[A-Za-z0-9_/-]+$/.test(pack)||pack.includes('..')||!/^\w+$/.test(clip)||!Number.isInteger(initial)||initial<1)throw Error('Invalid movie export arguments');
const require=createRequire(web+'/package.json'),{chromium}=require('playwright'),{build}=require('esbuild');
const bundle=shared?.bundle??(await build({stdin:{contents:"export {Movie} from './src/swf/movie'; export {Room} from './src/engine/room'; export {StageMask} from './src/engine/mask'; export {roomScript,ROOM_IDS,ROOM_NAMES} from './src/rooms'; export {Catalogue} from './src/ui/catalogue'; export {Newspaper} from './src/ui/newspaper'; export {inspectMovie} from '"+root+"/tools/movie-controls.mjs'; export {exportMovieFonts} from '"+root+"/tools/movie-fonts.mjs';",resolveDir:web},bundle:true,format:'iife',globalName:'Native',write:false})).outputFiles[0].text;
if(shared)shared.bundle=bundle;
await mkdir(root+'/build/scenes/'+name,{recursive:true});await mkdir(root+'/build/gfx',{recursive:true});await mkdir(root+'/romfs/scenes',{recursive:true});
const browser=shared?.browser??await chromium.launch({headless:true,executablePath:'/usr/bin/google-chrome',args:['--mute-audio']});
if(shared)shared.browser=browser;
try{
 const page=shared?.page??await browser.newPage();
 if(!shared?.page){
 await page.route('http://assets.test/**',async route=>{
  const url=new URL(route.request().url());if(url.pathname==='/')return route.fulfill({body:'<!doctype html><body></body>',contentType:'text/html'});
  const file=path.resolve(web+'/public','.'+decodeURIComponent(url.pathname));
  if(!file.startsWith(web+'/public/'))return route.abort();
  try{const body=await readFile(file);await route.fulfill({body,contentType:file.endsWith('.svg')?'image/svg+xml':file.endsWith('.json')?'application/json':file.endsWith('.ttf')?'font/ttf':file.endsWith('.woff')?'font/woff':'application/octet-stream'});}catch{await route.fulfill({status:404,body:'Not found'});}
 });
 await page.goto('http://assets.test/');await page.addScriptTag({content:bundle});
 if(shared)shared.page=page;
 }
 const data=await page.evaluate(async({name,library,pack,clip,initial,pageOnly,catalogue,igloo,newspaper,reuse,referenceTicks,roomBakeScale})=>{
  const {Movie,Room,roomScript}=Native;let movie,news;const newsTextIds=new Set();
  if(newspaper){
   const paper=await Native.Newspaper.open({show:()=>{},close:()=>{},notice:()=>{}});movie=paper.movie;movie.muted=true;
   const empty=new Set(movie.data.emptyTexts??[]);for(const timeline of [movie.data.main,...Object.values(movie.data.sprites)])timeline.frames=timeline.frames.map(frame=>frame.filter(entry=>!empty.has(entry.id)));
   const catalogue=paper.catalogue,timeline=movie.data.sprites[catalogue.root],index=catalogue.pages.findIndex(label=>timeline.labels[label]===initial);
   for(const id of Object.keys(catalogue.texts))newsTextIds.add(Number(id));
   if(index<0)throw Error('Newspaper frame must start a page');
   const last=index+1<catalogue.pages.length?timeline.labels[catalogue.pages[index+1]]-1:timeline.frames.length;
   news={issue:catalogue.issue,page:index+1,first:initial,last,pages:catalogue.pages.length};
   movie.goto('news#1',initial);
   timeline.frames=timeline.frames.map((frame,i)=>i+1>=initial&&i+1<=last?frame:[]);
   timeline.actions=Object.fromEntries(Object.entries(timeline.actions??{}).filter(([frame])=>Number(frame)>=initial&&Number(frame)<=last));
   for(const action of Object.values(movie.data.buttonActions??{}))if(action.release)action.release=action.release.replace(/gotoAndStop\("([^"]+)"\)/g,(source,label)=>{const canonical=Object.keys(timeline.labels).find(name=>name.toLowerCase()===label.toLowerCase());return canonical?`gotoAndStop("${canonical}")`:source;});
  }
  else if(catalogue){const book=await Native.Catalogue.load(catalogue,{coins:()=>0,show:()=>{},close:()=>{},buy:()=>{},adopt:()=>{},archive:()=>{},error:error=>{throw error;}});movie=book.movie;if(reuse)movie.data=structuredClone(movie.data);movie.muted=true;movie.goto(clip,initial);await book.render();book.cancel();}
  else{const base=await Movie.load(`/swf/${pack}/`);movie=library?base.asClip(clip):base;movie.muted=true;}
  if(pack.startsWith('assets/missions/')){
   movie.data=structuredClone(movie.data);
   if(name==='mission1-note'){movie.data.main.frames=[[{id:119,depth:1,name:'root',matrix:[1,0,0,1,80,310]}]];movie.goto('root',2);}
   for(const timeline of [movie.data.main,...Object.values(movie.data.sprites)])timeline.frames=timeline.frames.map(frame=>frame.filter(entry=>!['screenArea','anchor_mc'].includes(entry.name)));
   if(pack==='assets/missions/1/minigame')for(const timeline of Object.values(movie.data.sprites))timeline.frames=timeline.frames.map(frame=>frame.filter(entry=>!['bergHit','crosshair','gameHint','endSpeech','windText_mc','saved_mc'].includes(entry.name)));
   for(const timeline of Object.values(movie.data.sprites))for(const action of Object.values(timeline.actions??{}))if(!Array.isArray(action)&&action.targets)action.targets=action.targets.map(([target,command,arg])=>[target,command,arg==='_root.myGame.myLangCode'?'en':arg]);
   for(const timeline of Object.values(movie.data.sprites))for(const [frame,action] of Object.entries(timeline.actions??{}))if(Array.isArray(action))timeline.actions[frame]=action.map(([command,arg])=>[command,arg==='_root.myGame.myLangCode'?(timeline.labels.en??1):arg]);
  }
  movie.rasterize=false;
  const script=library?{start:[380,340]}:roomScript(igloo?'igloo':name,movie);script.setup?.(movie,'english');if(library&&!newspaper)movie.goto(clip,initial);
  // Catalogue language comes from the Flash shell; these native resources use English.
  if(catalogue)for(const timeline of Object.values(movie.data.sprites))for(const action of Object.values(timeline.actions??{})){
   if(!Array.isArray(action)&&action.source?.includes('getLocalizedFrame'))action.targets=action.targets?.map(([target,command,arg])=>[target,command,arg==='language'?'english':arg]);
  }
  if(pageOnly){const root=movie.data.sprites[movie.data.main.frames[0][0].id];root.frames=root.frames.map((frame,index)=>index+1===initial?frame:[]);root.actions=Object.fromEntries(Object.entries(root.actions??{}).filter(([frame])=>Number(frame)===initial));}
  await movie.preloadAnimation(1);if(igloo)await movie.preloadFrame(1,['room_area','wall_area','trash_area']);
  const room=library?{depthOf:entry=>entry.depth,draw:ctx=>movie.draw(ctx),advance:()=>movie.advance()}:new Room(igloo?'igloo':name,movie,script);
  const canvas=(w,h)=>{const c=document.createElement('canvas');c.width=w;c.height=h;return c;};
  // Resolve language labels after Room applies its original setup script.
  const states=[...movie.state].filter(([path])=>movie.find(path)).map(([path,s])=>{s=Object.fromEntries(Object.entries(s).filter(([key,v])=>v!==undefined&&!['color','matrix','text','textWidth','textHeight','textLeading'].includes(key)));if(s.button==='up')delete s.button;if(typeof s.frame==='string'){const found=movie.find(path),t=movie.data.sprites[found.entry.id];s.frame=t?.labels[s.frame]??1;}return [path,s];});
  const timelines={0:structuredClone(movie.data.main)},art=[],seen=new Set(),rasterScales=new Map();
  // Root setup is already captured as instance states; it is not an ActionScript VM.
  timelines[0].actions={};
  const visit=async id=>{
   if(seen.has(id))return;seen.add(id);
   const sprite=movie.data.sprites[id],c=movie.data.characters[id];
   if(sprite){timelines[id]=structuredClone(sprite);for(const f of sprite.frames)for(const e of f)await visit(e.id);return;}
   if(c?.kind==='button'&&c.records){timelines[id]={frames:[c.records.up],labels:{}};for(const e of c.records.up)await visit(e.id);return;}
   if(!c&&!movie.data.editTexts?.[id])throw Error(`SWL2: missing character ${id}`);
  };
  const raster=async id=>{
   const c=movie.data.characters[id],text=movie.data.editTexts?.[id];
   if(text){
    const pad=text.nativeOutline?.width??0,left=Math.floor(text.bounds[0]-pad)-1,top=Math.floor(text.bounds[1]-pad)-1,image=canvas(Math.ceil(text.bounds[2]+pad)-left+1,Math.ceil(text.bounds[3]+pad)-top+1),ctx=image.getContext('2d');ctx.translate(-left,-top);movie.drawText(ctx,text,text.text,undefined,text.nativeOutline);art.push({id,w:image.width,h:image.height,left,top,width:image.width,height:image.height,png:image.toDataURL()});return;
   }
   const a=c.kind==='button'?c.states.up:c;
   if(!a||a.size[0]<0||a.size[1]<0)throw Error(`SWL1: invalid art ${id}`);
   await movie.art.load(a.file);const source=movie.art.get(a.file);
   if(!source)throw Error(`SWL1: missing decoded art ${id}`);
   // Keep the SVG image's native pixel phase; drawing its fractional origin into
   // another surface first would resample every outline twice.
   const limit=newspaper&&!newsTextIds.has(id)&&source.naturalWidth>512?.85:1;
   const density=library?Math.max(.01,Math.min(limit,c.nativeRasterScale??(rasterScales.get(id)??1)*(c.nativeRasterFactor??1))):1,iw=Math.max(1,source.naturalWidth),ih=Math.max(1,source.naturalHeight);
   const w=iw+2,h=ih+2,image=canvas(w,h),ctx=image.getContext('2d');
   ctx.drawImage(source,1,1,iw,ih);
   const ux=a.size[0]/iw,uy=a.size[1]/ih;
   if(!Number.isFinite(ux)||!Number.isFinite(uy)||!ux||!uy){
    const left=Math.floor(-a.origin[0])-1,top=Math.floor(-a.origin[1])-1;
    image.width=Math.max(3,Math.ceil(a.size[0])+2);image.height=Math.max(3,Math.ceil(a.size[1])+2);
    ctx.setTransform(1,0,0,1,-left,-top);movie.drawCharacter(ctx,id,'export-art');
    art.push({id,w:image.width,h:image.height,left,top,width:image.width,height:image.height,png:image.toDataURL()});
   }else art.push({id,w,h,left:-a.origin[0]-ux,top:-a.origin[1]-uy,width:w*ux,height:h*uy,png:image.toDataURL(),density});
  };
  // Hidden hit masks stay in the collision data, not in the visual atlas.
  timelines[0].frames=timelines[0].frames.map(f=>f.filter(e=>!['block_mc','triggers_mc'].includes(e.name)).map(e=>({...e,renderDepth:room.depthOf(e)})).sort((a,b)=>a.renderDepth-b.renderDepth));
  for(const f of timelines[0].frames)for(const e of f)await visit(e.id);
  const metadata=library||igloo?Native.inspectMovie(movie,seen,catalogue):null;
  const warm=canvas(380,240),warmctx=warm.getContext('2d');warmctx.scale(.5,.5);room.draw(warmctx);await movie.ready();room.draw(warmctx);
  const protectedPaths=['foreground_mc',...Object.entries(script.buttons??{}).flatMap(([path,button])=>[path,...(button.hover??[]).map(h=>h.path)]),...Object.values(script.triggers??{}).flatMap(t=>t.frames?[t.frames[0]]:[])];
  if(pack==='assets/missions/2/miniGame')protectedPaths.push('root#1.landscape','root#1.sled',...Array.from({length:21},(_,i)=>'root#1.seg'+(i+1)));
  const targetPath=(from,target)=>{let parts=from.match(/[^.#]+|#\d+/g)??[];for(const p of target.split('.')){if(p==='this')continue;if(p==='_parent')parts.pop();else parts.push(p);}return parts.reduce((s,p)=>s+(s&&!p.startsWith('#')?'.':'')+p,'');};
  const scanned=new Set();
  const scan=(id,parent)=>{const key=id+':'+parent;if(scanned.has(key)||parent.length>255)return;scanned.add(key);const t=timelines[id];if(!t)return;for(const a of Object.values(t.actions??{}))for(const [target] of a.targets??[])protectedPaths.push(targetPath(parent,target));for(const f of t.frames)for(const e of f)if(timelines[e.id])scan(e.id,e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth);};
  scan(0,'');if(igloo)protectedPaths.push('background_mc.igloo_floor_mc');
  if(pack==='assets/missions/1/telescope')protectedPaths.push('root.telescope');
  if(library){
   const visited=new Set();const protect=(id,parent,depth=0)=>{const key=id+':'+parent;if(depth>24||visited.has(key))return;visited.add(key);for(const frame of timelines[id]?.frames??[])for(const e of frame){const path=e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth;if(metadata.hitIds.has(e.id)||(!newspaper&&movie.data.editTexts?.[e.id])||(timelines[e.id]?.frames.length??0)>1)protectedPaths.push(path);if(timelines[e.id])protect(e.id,path,depth+1);}};protect(0,'');
   if(catalogue==='sport')protectedPaths.push(...['penguin','star','shell','silver_mc'].map(p=>'book.'+p));
  }
  const roomActions=movie.data.main.actions?.[1],roomSource=roomActions&&!Array.isArray(roomActions)?roomActions.source??'':'';
  if(roomSource.includes('isItemInMyInventory'))for(const m of roomSource.matchAll(/([\w.]+)\.goto(?:AndStop|AndPlay)\(/g))if(movie.find(m[1]))protectedPaths.push(m[1]);
  const protectedEntry=path=>protectedPaths.some(p=>p===path||p.startsWith(path+'.')||p.startsWith(path+'#'));
  const original=structuredClone(timelines),optimized={},compiled=new Map(),bakeMemo=new Map(),bakeImages=new Map();let nextId=100000,baked=0;
  const textVariants=new Map();
  const localRoots=original[0].frames.some(frame=>frame.length>50);
  const multiply=(p,q)=>[p[0]*q[0]+p[2]*q[1],p[1]*q[0]+p[3]*q[1],p[0]*q[2]+p[2]*q[3],p[1]*q[2]+p[3]*q[3],p[0]*q[4]+p[2]*q[5]+p[4],p[1]*q[4]+p[3]*q[5]+p[5]];
  const inverse=m=>{const d=m[0]*m[3]-m[1]*m[2];if(Math.abs(d)<1e-8)throw Error('SWL1: singular static parent');return [m[3]/d,-m[1]/d,-m[2]/d,m[0]/d,(m[2]*m[5]-m[3]*m[4])/d,(m[1]*m[4]-m[0]*m[5])/d];};
  const staticLocal=(e,parent,depth=0)=>{
   if(depth>24)return false;const path=e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth;
   if(protectedEntry(path))return false;const state=movie.state.get(path),t=original[e.id];if(!t)return true;
   if(state?.playing||Object.values(t.actions??{}).some(a=>a.targets?.length))return false;
   let frame=1;if(state?.frame!==undefined)frame=typeof state.frame==='string'?t.labels?.[state.frame]??1:state.frame;
   else if(t.frames.length>1)return false;
   return (t.frames[frame-1]??[]).every(child=>child.clipDepth===undefined&&staticLocal(child,path,depth+1));
  };
  const leaves=(e,parent,depth=0)=>{if(depth>24)return 0;const t=original[e.id];if(!t)return 1;const path=e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth,s=movie.state.get(path);const frame=typeof s?.frame==='string'?t.labels[s.frame]??1:s?.frame??1;return (t.frames[frame-1]??[]).reduce((n,child)=>Math.min(3,n+leaves(child,path,depth+1)),0);};
  const bake=(list,parent,world,stable)=>{
   const memoKey=JSON.stringify([stable,stable?world:null,parent,list]);if(bakeMemo.has(memoKey))return structuredClone(bakeMemo.get(memoKey));
   if(!stable&&(localRoots||list.reduce((n,e)=>Math.min(3,n+leaves(e,parent)),0)<3))return null;
   const rasterScale=stable?1:.5;let originX=0,originY=0,width=760*rasterScale,height=480*rasterScale;
   if(!stable||library){let boxes=list.map(e=>movie.entryBounds(e,parent)).filter(Boolean);if(!boxes.length)return [];if(stable)boxes=boxes.map(b=>{const points=[[b[0],b[1]],[b[2],b[1]],[b[0],b[3]],[b[2],b[3]]].map(([x,y])=>[world[0]*x+world[2]*y+world[4],world[1]*x+world[3]*y+world[5]]);return [Math.min(...points.map(p=>p[0])),Math.min(...points.map(p=>p[1])),Math.max(...points.map(p=>p[0])),Math.max(...points.map(p=>p[1]))];});originX=Math.floor(Math.min(...boxes.map(b=>b[0]))*rasterScale)-1;originY=Math.floor(Math.min(...boxes.map(b=>b[1]))*rasterScale)-1;width=Math.ceil(Math.max(...boxes.map(b=>b[2]))*rasterScale)-originX+1;height=Math.ceil(Math.max(...boxes.map(b=>b[3]))*rasterScale)-originY+1;if(width<=0||height<=0)return [];if(width>(stable?1024:128)||height>(stable?1024:128))return null;}
   const c=canvas(width,height),ctx=c.getContext('2d');ctx.setTransform(rasterScale,0,0,rasterScale,-originX,-originY);if(stable)ctx.transform(...world);for(const e of list)movie.entry(ctx,e,parent);
   const rgba=ctx.getImageData(0,0,width,height).data;let x0=width,y0=height,x1=0,y1=0;
   for(let y=0;y<height;y++)for(let x=0;x<width;x++)if(rgba[(y*width+x)*4+3]){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
   if(x1<=x0||y1<=y0)return [];
   const image=canvas(x1-x0+2,y1-y0+2);image.getContext('2d').drawImage(c,x0,y0,x1-x0,y1-y0,1,1,x1-x0,y1-y0);
   const worldWidth=image.width/rasterScale,worldHeight=image.height/rasterScale;let packed=image;
   if(stable&&!library&&roomBakeScale<1){packed=canvas(Math.max(1,Math.ceil(image.width*roomBakeScale)),Math.max(1,Math.ceil(image.height*roomBakeScale)));const reduced=packed.getContext('2d');reduced.imageSmoothingQuality='high';reduced.drawImage(image,0,0,packed.width,packed.height);}
   const png=packed.toDataURL(),left=(originX+x0-1)/rasterScale,top=(originY+y0-1)/rasterScale,key=JSON.stringify([png,packed.width,packed.height,worldWidth,worldHeight]);let id=bakeImages.get(key);
   if(!id){id=nextId++;bakeImages.set(key,id);art.push({id,w:packed.width,h:packed.height,left:0,top:0,width:worldWidth,height:worldHeight,png,baked:true});baked++;}
   const output=[{id,depth:list[0].depth,renderDepth:list[0].renderDepth,name:list.length===1?list[0].name:undefined,matrix:multiply(stable?inverse(world):[1,0,0,1,0,0],[1,0,0,1,left,top])}];bakeMemo.set(memoKey,output);return structuredClone(output);
  };
  const optimize=(id,parent,world,stable)=>{
   const key=`${id}:${parent}:${stable?world.join(','):'dynamic'}`;
   if(compiled.has(key))return compiled.get(key);
   const source=original[id],out=structuredClone(source),newId=id===0?0:nextId++;
   out.originalSymbol=id;
   compiled.set(key,newId);
   optimized[newId]=out;
   const fixed=source.frames.length===1||(pageOnly&&parent===clip)||(newspaper&&parent==='news#1');
   out.frames=source.frames.map((list,frameIndex)=>{
    metadata?.collect(id,parent,frameIndex+1);
    const entries=[];let batch=[],clipUntil=0;
    const fixedChild=e=>stable&&fixed&&!(id===0&&localRoots&&e.renderDepth>=900);
    const convert=e=>{const path=e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth,entry=structuredClone(e);
     const state=movie.state.get(path);if(state?.color)entry.color=state.color;if(state?.matrix)entry.matrix=state.matrix;
     const text=movie.data.editTexts?.[e.id];if(text&&state&&['text','textWidth','textHeight','textLeading'].some(key=>state[key]!==undefined)){
      const value={...text,bounds:[...text.bounds],text:state.text??text.text};if(state.textWidth!==undefined)value.bounds[2]=value.bounds[0]+state.textWidth;if(state.textHeight!==undefined)value.bounds[3]=value.bounds[1]+state.textHeight;if(state.textLeading!==undefined)value.leading=state.textLeading;
      const key=JSON.stringify(value);let id=textVariants.get(key);if(id===undefined){id=nextId++;textVariants.set(key,id);movie.data.editTexts[id]=value;}entry.id=id;
     }
     if(metadata?.hitIds.has(e.id))entry.hitId=e.id;
     if(original[e.id])entry.id=optimize(e.id,path,multiply(world,e.matrix??[1,0,0,1,0,0]),fixedChild(e));
     else{const matrix=multiply(world,entry.matrix??[1,0,0,1,0,0]),scale=Math.max(Math.hypot(matrix[0],matrix[1]),Math.hypot(matrix[2],matrix[3]));rasterScales.set(entry.id,Math.max(rasterScales.get(entry.id)??0,scale));}
     return entry;
    };
    const flush=()=>{if(batch.length){const baked=bake(batch,parent,world,stable&&fixed&&!(id===0&&localRoots&&batch[0].renderDepth>=900));if(baked)entries.push(...baked);else for(const e of batch)entries.push(convert(e));batch=[];}};
    for(const e of list){
     if(id===0)flush();
     const path=e.name?(parent?parent+'.':'')+e.name:parent+'#'+e.depth;
     if(clipUntil&&e.depth>clipUntil){flush();clipUntil=0;}
     if(e.clipDepth===undefined&&!protectedEntry(path)&&((stable&&fixed&&movie.staticEntry(e,parent))||staticLocal(e,parent))){batch.push(e);continue;}
     flush();if(e.clipDepth!==undefined)clipUntil=e.clipDepth;entries.push(convert(e));
    }
    flush();return entries;
   });
   return newId;
  };
  optimize(0,'',[1,0,0,1,0,0],true);
  const outlineIDs=new Map(),ignoredClipOutlines=[];
  for(const t of Object.values(optimized))for(const f of t.frames)for(const e of f)if(e.outline||e.outlineArt){
   if(library&&movie.data.editTexts?.[e.id]){movie.data.editTexts[e.id].nativeOutline=e.outline;delete e.outlineArt;delete e.outline;continue;}
   // Movie.entry applies outlines to text/art; clip outlines have no rendered counterpart there.
   if(!e.outlineArt&&optimized[e.id]){ignoredClipOutlines.push({symbol:e.id,outline:e.outline});delete e.outline;continue;}
   if(!e.outlineArt)throw Error('Unbaked primitive outline '+e.id);
   const key=JSON.stringify(e.outlineArt);let id=outlineIDs.get(key);if(!id){id=nextId++;outlineIDs.set(key,id);movie.data.characters[id]={kind:'shape',...e.outlineArt};}e.id=id;delete e.outlineArt;delete e.outline;
  }
  // Flash masks ignore fill alpha; clip entries use the opaque mask art the web player uses.
  const maskIDs=new Map();
  for(const t of Object.values(optimized))for(const f of t.frames)for(const e of f){
   const c=e.clipDepth!==undefined&&movie.data.characters[e.id];if(!c?.maskFile)continue;
   let id=maskIDs.get(e.id);if(!id){id=nextId++;maskIDs.set(e.id,id);movie.data.characters[id]={kind:'shape',file:c.maskFile,size:c.size,origin:c.origin};}e.id=id;
  }
  const usedArt=new Set();for(const t of Object.values(optimized))for(const f of t.frames)for(const e of f)if(!optimized[e.id])usedArt.add(e.id);
  for(const id of usedArt)if(!art.some(a=>a.id===id))await raster(id);
  art.splice(0,art.length,...art.filter(a=>usedArt.has(a.id)));
  // Newspaper replacements are fixed at export time; runtime glyph alphabets would waste the page budget.
  const nativeFonts=metadata?(newspaper?{styles:[],fields:{}}:Native.exportMovieFonts(movie,usedArt,art,nextId)):null;
  const dynamicRefs=[];
  if(metadata){const snapshot=movie.snapshot(),paths=[];for(const [path] of movie.state)if(path.endsWith('.coinTotal.coins_txt')){snapshot.set(path,{text:'YOUR COINS: 12345'});if(snapshot.find(path))paths.push(path);}if(paths.length){const image=canvas(380,240),ctx=image.getContext('2d');ctx.scale(.5,.5);snapshot.draw(ctx);await snapshot.ready();ctx.clearRect(0,0,760,480);snapshot.draw(ctx);dynamicRefs.push({name:'coins-12345',png:image.toDataURL()});ctx.clearRect(0,0,760,480);for(const path of paths){const found=snapshot.find(path),field=snapshot.data.editTexts[found.entry.id];if(!field)continue;ctx.save();ctx.transform(...found.matrix);ctx.transform(...(found.entry.matrix??[1,0,0,1,0,0]));snapshot.drawText(ctx,field,'YOUR COINS: 12345',undefined,found.entry.outline);ctx.restore();}dynamicRefs.push({name:'coins-only',png:image.toDataURL()});}}
  const refs=[];
  const image=canvas(380,240),ctx=image.getContext('2d');ctx.scale(.5,.5);
  for(let tick=0;tick<=referenceTicks[referenceTicks.length-1];tick++){
   if(referenceTicks.includes(tick)){
    ctx.clearRect(0,0,760,480);room.draw(ctx);await movie.ready();ctx.clearRect(0,0,760,480);room.draw(ctx);
    refs.push({tick,png:image.toDataURL()});
   }else room.draw(ctx);
   room.advance();
  }
  const bits=mask=>{const out=new Uint8Array(760*480/8);for(let i=0;i<mask.alpha.length;i++)if(mask.alpha[i])out[i>>3]|=1<<(i&7);return Array.from(out);};
  const navigation=library?null:{room:name,name:Native.ROOM_NAMES[name],id:Native.ROOM_IDS[name],start:script.start,block:bits(room.block),triggers:room.triggers.map(t=>({...script.triggers[t.name],name:t.name,mask:bits(t.mask)})),buttons:room.buttons.map(b=>({path:b.path,...script.buttons[b.path],mask:bits(b.mask)})).map(({action,...b})=>b)};
  const details=metadata?.result();if(details){const root=igloo?movie.data.main:newspaper?movie.data.sprites[movie.find('news#1').entry.id]:movie.data.sprites[movie.data.main.frames[0][0].id];details.clip=newspaper?'news#1':clip;details.frames=root.frames.length;details.labels=root.labels??{};details.nativeFonts=nativeFonts;if(newspaper){details.newspaper=news;for(const control of details.controls){const source=movie.data.buttonActions?.[control.id]?.release??'';if(/showWindow\("News Form"\)|getURL\(/.test(source))control.commands=[{op:24}];}}if(igloo){const main=movie.data.main.actions?.[1],source=main&&!Array.isArray(main)?main.source??'':'';details.igloo={floorFrame:Number(/\bfloor_frame\s*=\s*(\d+)/.exec(source)?.[1]??1),start:script.start,block:bits(room.block)};for(const key of ['room_area','wall_area','trash_area'])details.igloo[key]=bits(new Native.StageMask(movie,key,760,480));}}
  return {width:760,height:480,fps:movie.data.stage.frameRate,timelines:optimized,art,states,refs,dynamicRefs,baked,navigation,metadata:details,ignoredClipOutlines};
 },{name,library,pack,clip,initial,pageOnly,catalogue,igloo,newspaper,reuse:!!shared,referenceTicks,roomBakeScale});
 if(data.metadata?.igloo){const source=await readFile(web+'/src/rooms/index.ts','utf8'),ids=Object.fromEntries([...source.split('ROOM_IDS')[1].split('};')[0].matchAll(/(\w+)\s*:\s*(\d+)/g)].map(m=>[m[1],Number(m[2])]));data.metadata.igloo.navigation=encodeNavigation(data.navigation,ids).toString('base64');}
 if(data.metadata?.igloo)for(const key of ['block','room_area','wall_area','trash_area'])data.metadata.igloo[key]=Buffer.from(data.metadata.igloo[key]).toString('base64');
 await writeFile(root+'/build/scenes/'+name+'/art-profile.json',JSON.stringify(data.art.map(({id,w,h,baked})=>({id,w,h,baked:!!baked})).sort((a,b)=>b.w*b.h-a.w*a.h),null,2)+'\n');
 // Texture budget: keep the highest raster scale that fits (a middle ground between sharpness and
 // memory) instead of dropping straight to half size. 32-bit pages are preferred down to 0.75;
 // below that, 16-bit pages with ordered dithering keep more resolution in the same budget.
 // Each candidate is resampled once from the full-size raster with high-quality filtering.
 let pages,dynamicScale=1,textureFormat='rgba8888';const fullArt=data.art,sourceTimelines=data.timelines;
 const scaled=s=>page.evaluate(async({art,s,scaleBaked})=>{
  const out=[];
  for(const a of art){const factor=s*(a.density??1);if((a.baked&&!scaleBaked)||factor===1){out.push(a);continue;}const image=new Image();image.src=a.png;await image.decode();const c=document.createElement('canvas');c.width=Math.max(1,Math.ceil(a.w*factor));c.height=Math.max(1,Math.ceil(a.h*factor));const g=c.getContext('2d');g.imageSmoothingEnabled=true;g.imageSmoothingQuality='high';g.drawImage(image,0,0,c.width,c.height);out.push({...a,w:c.width,h:c.height,png:c.toDataURL()});}
  return out;
 },{art:fullArt,s,scaleBaked:pack.startsWith('assets/missions/')});
 const tile=async(art,timelines)=>{
 // Oversized scrolling art is split without overlapping geometry. Its sampling
 // border contains neighbouring source texels, so tile boundaries stay seamless.
 return page.evaluate(async({art,timelines})=>{
  let next=1+Math.max(...art.map(a=>a.id),...Object.keys(timelines).map(Number));const result=[];
  for(const a of art){
   if(a.w+2<=1024&&a.h+2<=512){result.push(a);continue;}
   const image=new Image();image.src=a.png;await image.decode();const entries=[];
   for(let y=0;y<a.h;y+=504)for(let x=0;x<a.w;x+=1016){
    const w=Math.min(1016,a.w-x),h=Math.min(504,a.h-y),c=document.createElement('canvas');c.width=w+2;c.height=h+2;c.getContext('2d').drawImage(image,x-1,y-1,w+2,h+2,0,0,w+2,h+2);const id=next++;
    result.push({id,w:w+2,h:h+2,pad:1,left:a.left+x*a.width/a.w,top:a.top+y*a.height/a.h,width:w*a.width/a.w,height:h*a.height/a.h,png:c.toDataURL(),baked:a.baked});entries.push({id,depth:entries.length+1});
   }
   timelines[a.id]={frames:[entries],labels:{},actions:{1:[['stop',null]]}};
  }
  return {art:result,timelines};
 },{art,timelines});
 };
 const forced=Number(process.env.SWL_FORCE_SCALE);
 const attempts=Number.isFinite(forced)&&forced>0&&forced<=1?[[forced,'rgba4444']]:newspaper?[[1,'rgba8888'],[1,'rgba4444']]:[...[1,.9,.8,.75].map(s=>[s,'rgba8888']),...[.9,.8,.75,.7,.65,.6,.55,.5,...(process.env.SWL_RECOVERY_SCALE==='1'?[.45,.4,.35,.3,.25]:[])].map(s=>[s,'rgba4444'])];
 for(const [index,[s,format]] of attempts.entries()){
  const tiled=await tile(await scaled(s),structuredClone(sourceTimelines));
  try{pages=format==='rgba8888'?packRectangles(tiled.art):packRectangles(tiled.art,1024,512,2);}
  catch(error){if(!/texture budget|exceeds.*atlas/.test(error.message)||index===attempts.length-1)throw error;continue;}
  data.art=tiled.art;data.timelines=deduplicateTimelines(tiled.timelines);dynamicScale=s;textureFormat=format;break;
 }
 if(data.metadata)data.metadata.symbols=Object.fromEntries(Object.entries(data.timelines).map(([id,timeline])=>[id,timeline.originalSymbol??Number(id)]));
 const atlas=await page.evaluate(async({art,pages,textureFormat})=>{
  const out=[];
  for(let p=0;p<pages;p++){
   const c=document.createElement('canvas');c.width=1024;c.height=512;const ctx=c.getContext('2d');
   for(const a of art.filter(a=>a.page===p)){const im=new Image();im.src=a.png;await im.decode();ctx.drawImage(im,a.x,a.y);}
   // Ordered (Bayer 4x4) dithering on colour hides 16-bit banding; alpha is rounded so edges stay clean.
   if(textureFormat==='rgba4444'){const pixels=ctx.getImageData(0,0,1024,512),d=pixels.data,bayer=[0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5];for(let i=0;i<d.length;i+=4){const p=i>>2,t=(bayer[(p>>10&3)*4+(p&3)]+.5)/16-.5;for(let c=0;c<3;c++)d[i+c]=Math.max(0,Math.min(255,Math.round(d[i+c]/17+t)*17));d[i+3]=Math.round(d[i+3]/17)*17;}ctx.putImageData(pixels,0,0);}
   const pixels=ctx.getImageData(0,0,1024,512).data;let raw='';
   for(let offset=0;offset<pixels.length;offset+=32768)raw+=String.fromCharCode(...pixels.subarray(offset,offset+32768));
   out.push({png:c.toDataURL(),rgba:btoa(raw)});
  }return out;
 },{art:data.art,pages,textureFormat});
 const binary=encodeScene({...data,pages,textureFormat});
 await mkdir(`${root}/build/scene-formats`,{recursive:true});
 const atlasHashes=[];
 for(const [i,a] of atlas.entries()){
  atlasHashes.push(createHash('sha256').update(Buffer.from(a.png.split(',')[1],'base64')).digest('hex'));
  await writeFile(`${root}/${external?'build/scenes/'+name+'/atlas-'+i:'build/gfx/swl-'+name+'-'+i}.png`,Buffer.from(a.png.split(',')[1],'base64'));
  await writeFile(`${root}/build/scenes/${name}/atlas-${i}.rgba`,Buffer.from(a.rgba,'base64'));
  await writeFile(`${root}/build/scene-formats/swl-${name}-${i}`,textureFormat+'\n');
 }
 for(const r of data.refs)await writeFile(`${root}/build/scenes/${name}/web-${r.tick}.png`,Buffer.from(r.png.split(',')[1],'base64'));
 for(const r of data.dynamicRefs)await writeFile(`${root}/build/scenes/${name}/web-${r.name}.png`,Buffer.from(r.png.split(',')[1],'base64'));
 await writeFile(`${root}/${external?'build/scenes/'+name+'/scene.swl':'romfs/scenes/'+name+'.swl'}`,binary);
 if(data.metadata){await writeFile(`${root}/build/scenes/${name}/movie.json`,JSON.stringify(data.metadata)+'\n');if(!external)await writeFile(`${root}/romfs/scenes/${name}.json`,JSON.stringify(data.metadata)+'\n');}
 if(data.navigation)await writeFile(`${root}/build/scenes/${name}/navigation.json`,JSON.stringify(data.navigation)+'\n');
 for(const [folder,suffix] of [['build/gfx','.png'],['romfs/gfx','.t3x']])for(const file of await readdir(`${root}/${folder}`)){
  const prefix=`swl-${name}-`;if(!file.startsWith(prefix)||!file.endsWith(suffix))continue;
  const index=file.slice(prefix.length,-suffix.length);if(/^\d+$/.test(index)&&Number(index)>=pages)await rm(`${root}/${folder}/${file}`);
 }
 const sha=file=>createHash('sha256').update(file).digest('hex');
 const manifest={format:'SWL2',room:name,source:sha(await readFile(`${web}/public/swf/${pack}/timelines.json`)),runtimeSource:sha(await readFile(`${web}/src/swf/movie.ts`)),binary:sha(binary),atlasHashes,art:data.art.length,bakedGroups:data.baked,roomBakeScale:library?null:roomBakeScale,dynamicScale,textureFormat,timelines:Object.keys(data.timelines).length,pages,atlasWidth:1024,atlasHeight:512,textureBytes:pages*1024*512*(textureFormat==='rgba4444'?2:4),textureBudget:limits.texture,binaryBytes:binary.length,referenceTicks:data.refs.map(r=>r.tick),ignoredClipOutlines:data.ignoredClipOutlines,unsupported:['dynamic text updates','native audio playback']};
 await writeFile(`${root}/build/scenes/${name}/manifest.json`,JSON.stringify(manifest,null,2)+'\n');
 console.log(JSON.stringify(manifest,null,2));
}finally{if(!shared)await browser.close();}
}
if(process.argv[1]&&path.resolve(process.argv[1])===path.resolve(import.meta.filename))await exportScene();
