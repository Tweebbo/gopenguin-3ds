export function encodeNavigation(nav,ids){
 const strings=[Buffer.from([0])],offsets=new Map([['',0]]);let size=1;
 const string=value=>{value??='';if(!offsets.has(value)){const b=Buffer.from(value+'\0');offsets.set(value,size);strings.push(b);size+=b.length;}return offsets.get(value);};
 const masks=[Buffer.from(nav.block)],triggers=[],buttons=[],hovers=[],events=[];
 const put=(values,types)=>{const b=Buffer.alloc(values.length*4);values.forEach((v,i)=>b[types?.[i]==='f'?'writeFloatLE':'writeUInt32LE'](v,i*4));return b;};
 for(const t of nav.triggers){const index=masks.length;masks.push(Buffer.from(t.mask));triggers.push(put([ids[t.room]??0,t.x,t.y,t.item??0,string(t.otherwise??(t.room?.startsWith('@')?t.room.slice(1):'')),string(t.frames?.[0]),t.frames?.[1]??0,t.frames?.[2]??0,index],['u','f','f']));}
 for(const b of nav.buttons){const index=masks.length;masks.push(Buffer.from(b.mask));const first=hovers.length;for(const h of b.hover??[])hovers.push(put([string(h.path),h.over,h.out,h.playOver?1:0]));buttons.push(put([string(b.path),string(b.content),b.walk?.[0]??0,b.walk?.[1]??0,first,hovers.length-first,index,0],['u','u','f','f']));}
 for(const event of nav.events??[]){const index=event.type===1?event.item:nav.buttons.findIndex(b=>b.path===event.button);if(index<0)throw Error(`Missing button ${event.button}`);events.push(put([event.type,index,string(event.guard),event.min??0,event.max??0,string(event.path),event.frame,event.playing?1:0]));}
 const bytes=48+triggers.length*36+buttons.length*32+hovers.length*16+events.length*32+size+masks.length*45600;
 const header=Buffer.alloc(48);header.write('GPN2');header.writeUInt32LE(2,4);header.writeUInt32LE(bytes,8);header.writeUInt32LE(nav.id??0,12);header.writeFloatLE(nav.start[0],16);header.writeFloatLE(nav.start[1],20);header.writeUInt32LE(triggers.length,24);header.writeUInt32LE(buttons.length,28);header.writeUInt32LE(hovers.length,32);header.writeUInt32LE(size,36);header.writeUInt32LE(masks.length,40);header.writeUInt32LE(events.length,44);
 for(const mask of masks)if(mask.length!==45600)throw Error('Invalid navigation mask');
 return Buffer.concat([header,...triggers,...buttons,...hovers,...events,...strings,...masks]);
}
