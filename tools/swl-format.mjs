import {internRun} from './dedup-scene.mjs';
// Scene tables are little endian; native C struct layout never crosses the boundary.
export const VERSION=2, HEADER=64;
export const strides=[40,12,16,80,32,12,16,1];
export const ops={stop:1,play:2,gotoAndStop:3,gotoAndPlay:4,nextFrame:5,prevFrame:6};
// SWL_TEXTURE_BUDGET (bytes) lowers the texture budget for a bank that shares a game's 8 MiB with others.
const texturePages=Number(process.env.SWL_TEXTURE_PAGES??8);
if(!Number.isInteger(texturePages)||texturePages<1||texturePages>8)throw Error('Texture page limit must be an integer from 1 to 8.');
export const limits={file:16*1024*1024,texture:Math.min(8*1024*1024,Number(process.env.SWL_TEXTURE_BUDGET)||6*1024*1024),pages:texturePages,instances:1024,depth:24};

export function encodeScene({width,height,fps,root=0,pages,art,timelines,states=[],textureFormat='rgba8888'}){
 const strings=[Buffer.from([0])],offsets=new Map([['',0]]);let stringBytes=1;
 const str=value=>{value??='';if(!offsets.has(value)){const b=Buffer.from(value+'\0');offsets.set(value,stringBytes);strings.push(b);stringBytes+=b.length;}return offsets.get(value);};
 const arts=[],tls=[],frames=[],entries=[],commands=[],labels=[],settings=[];
 const entryRuns=new Map(),commandRuns=new Map();
 const put=(types,values)=>{const b=Buffer.alloc(types.length*4);types.forEach((t,i)=>b[t==='f'?'writeFloatLE':'writeUInt32LE'](values[i],i*4));return b;};
 for(const a of [...art].sort((a,b)=>a.id-b.id)){if(![a.left,a.top,a.width,a.height].every(Number.isFinite))throw Error(`Non-finite art bounds ${a.id}`);const p=a.pad??0;arts.push(put('uuuuuuffff'.split(''),[a.id,a.page,a.x+p,a.y+p,a.w-p*2,a.h-p*2,a.left,a.top,a.width,a.height]));}
 for(const [id,t] of Object.entries(timelines).sort((a,b)=>Number(a[0])-Number(b[0]))){
  tls.push(put(['u','u','u'],[Number(id),frames.length,t.frames.length]));
  const resolve=arg=>typeof arg==='string'?t.labels?.[arg]:arg;
  for(const [name,frame] of Object.entries(t.labels??{}))labels.push(put(['u','u','u'],[Number(id),str(name),frame]));
  for(const [index,list] of t.frames.entries()){
   const action=t.actions?.[index+1],cmds=[];
   const command=(op,arg=0,arg2=0,arg3=0,target='',counter='',flags=0)=>cmds.push(put(Array(8).fill('u'),[op,arg,arg2,arg3,str(target),str(counter),flags,0]));
   for(const sound of t.sounds??[])if(sound.frame===index+1)for(const start of sound.starts??sound.ids.map(id=>[id,0,0,1]))command(10,start[0],(start[1]?1:0)|(start[2]?2:0),start[3]);
   if(!Array.isArray(action)){
    for(const [name,value] of Object.entries(action?.initial??{}))command(7,value,0,0,'',name);
    if(action?.loop){const l=action.loop;command(8,l.limit,l.frame,l.rest,'',l.counter);}
   }
   const addCommand=(name,arg,target='')=>{
    if(!ops[name])throw Error(`SWL2: unsupported command ${name}`);
    let frame=name.startsWith('goto')?arg:0,flags=0,randomOffset=0;
    if(typeof frame==='string'){
     const random=/^random\((\d+)\)\s*\+\s*(\d+)$/.exec(frame);
     const assignment=Array.isArray(action)?null:new RegExp(`\\bvar\\s+${frame.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')}\\s*=\\s*random\\((\\d+)\\)\\s*\\+\\s*(\\d+)`).exec(action?.source??'');
     if(random||assignment){const match=random??assignment;frame=Number(match[1]);randomOffset=Number(match[2]);if(!frame||frame>65535||!randomOffset||randomOffset>65535)throw Error(`SWL2: invalid random frame range in ${id}`);flags=2;}
     else if(target){frame=str(frame);flags=1;}
     else if(t.labels?.[frame]!==undefined)frame=resolve(frame);
     else{
      const variable=new RegExp(`\\bvar\\s+${frame.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')}\\s*=\\s*([\\w.]+)\\._currentframe\\s*;`).exec(Array.isArray(action)?'':action?.source??'');
      if(variable){frame=str(variable[1]);flags=4;}else frame=resolve(frame);
     }
    }
    if(name.startsWith('goto')&&(!Number.isInteger(frame)||frame<1))throw Error(`SWL2: unresolved frame ${arg} in ${id}`);
    command(ops[name],frame??0,randomOffset,0,target,'',flags);
   };
   for(const [name,arg] of Array.isArray(action)?action:action?.commands??[])addCommand(name,arg);
   for(const [target,name,arg] of action?.targets??[])addCommand(name,arg,target);
   const frameEntries=[];
   for(const e of list){
    if(e.outline||e.outlineArt)throw Error(`SWL1: outlines need baking (symbol ${id})`);
    frameEntries.push(put([...Array(5).fill('u'),...Array(15).fill('f')],[e.id,e.depth,str(e.name),e.clipDepth!==undefined?1:e.hitId?2:0,e.clipDepth??e.hitId??0,e.renderDepth??e.depth,...(e.matrix??[1,0,0,1,0,0]),...(e.color?.mult??[1,1,1,1]),...(e.color?.add??[0,0,0,0])]));
   }
   const firstEntry=internRun(entryRuns,frameEntries,entries),firstCommand=internRun(commandRuns,cmds,commands);
   frames.push(put(['u','u','u','u'],[firstEntry,list.length,firstCommand,cmds.length]));
  }
 }
 for(const [path,state] of states){
  const extra=Object.keys(state).filter(k=>!['visible','frame','playing'].includes(k));
  if(extra.length)throw Error(`SWL1: unsupported instance state ${path}: ${extra}`);
  if(state.frame!==undefined&&!Number.isInteger(state.frame))throw Error(`SWL1: unresolved initial frame ${path}`);
  settings.push(put(['u','u','u','u'],[str(path),state.frame??1,(state.visible===false?1:0)|(state.frame!==undefined?2:0)|(state.playing===true?4:0),0]));
 }
 const tables=[arts,tls,frames,entries,commands,labels,settings,strings],counts=tables.map((x,i)=>i===7?stringBytes:x.length);
 const bytes=HEADER+counts.reduce((n,c,i)=>n+c*strides[i],0);
 if(bytes>limits.file)throw Error('SWL1: binary exceeds file budget');
 const header=Buffer.alloc(HEADER);header.write('SWL2');header.writeUInt32LE(VERSION,4);header.writeUInt32LE(bytes,8);
 [width,height,fps].forEach((n,i)=>header.writeFloatLE(n,12+i*4));header.writeUInt32LE(root,24);header.writeUInt32LE((pages+(textureFormat==='rgba4444'?0x80000000:0))>>>0,28);
 counts.forEach((n,i)=>header.writeUInt32LE(n,32+i*4));
 return Buffer.concat([header,...tables.flat()]);
}

export function packRectangles(rectangles,size=1024,height=512,bytesPerPixel=4){
 const pages=[];
 for(const r of [...rectangles].sort((a,b)=>Math.max(b.h,b.w)-Math.max(a.h,a.w)||b.h*b.w-a.h*a.w||a.id-b.id)){
  const w=r.w+2,h=r.h+2;
  if(w>size||h>height)throw Error(`SWL1: art ${r.id} exceeds ${size}x${height} atlas`);
  let best;
  for(const [p,page] of pages.entries())for(const f of page){
   if(w>f.w||h>f.h)continue;
   const score=Math.min(f.w-w,f.h-h)*size+Math.max(f.w-w,f.h-h);
   if(!best||score<best.score)best={p,x:f.x,y:f.y,w,h,score};
  }
  if(!best){
   if((pages.length+1)*size*height*bytesPerPixel>limits.texture||pages.length>=limits.pages)throw Error(`SWL1: room texture budget exceeded (${rectangles.reduce((n,a)=>n+(a.w+2)*(a.h+2),0)} source pixels)`);
   best={p:pages.length,x:0,y:0,w,h};pages.push([{x:0,y:0,w:size,h:height}]);
  }
  Object.assign(r,{page:best.p,x:best.x+1,y:best.y+1});
  const free=[];
  for(const f of pages[best.p]){
   if(best.x>=f.x+f.w||best.x+w<=f.x||best.y>=f.y+f.h||best.y+h<=f.y){free.push(f);continue;}
   if(best.x>f.x)free.push({...f,w:best.x-f.x});
   if(best.x+w<f.x+f.w)free.push({...f,x:best.x+w,w:f.x+f.w-best.x-w});
   if(best.y>f.y)free.push({...f,h:best.y-f.y});
   if(best.y+h<f.y+f.h)free.push({...f,y:best.y+h,h:f.y+f.h-best.y-h});
  }
  pages[best.p]=free.filter((a,i)=>!free.some((b,j)=>i!==j&&a.x>=b.x&&a.y>=b.y&&a.x+a.w<=b.x+b.w&&a.y+a.h<=b.y+b.h&&(i>j||a.x!==b.x||a.y!==b.y||a.w!==b.w||a.h!==b.h)));
 }
 return pages.length;
}
