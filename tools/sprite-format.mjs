import {createHash} from 'node:crypto';
export function encodeSprite({frames,sequences,textures,scale=.5,fps=24,nativeTimeline=false}){
 if(!frames.length||frames.length>16384||!sequences.length||sequences.length>512||!textures.length||textures.length>64)throw Error('Sprite metadata exceeds budget');
 const actions=Buffer.alloc(sequences.length*12),rects=Buffer.alloc(frames.length*12),table=Buffer.alloc(textures.length*16);
 sequences.forEach((s,i)=>[s.start,s.count,s.loop,s.page,s.rotation,s.variation].forEach((v,n)=>actions.writeUInt16LE(v,i*12+n*2)));
 frames.forEach((f,i)=>{[f.x??0,f.y??0,f.w??0,f.h??0].forEach((v,n)=>rects.writeUInt16LE(v,i*12+n*2));rects.writeInt16LE(f.left??0,i*12+8);rects.writeInt16LE(f.top??0,i*12+10);});
 let offset=64+actions.length+rects.length+table.length;
 textures.forEach((t,i)=>{if(t.width*t.height*4>2*1024*1024)throw Error('Sprite texture exceeds 2 MiB');table.writeUInt32LE(offset,i*16);table.writeUInt32LE(t.data.length,i*16+4);table.writeUInt16LE(t.width,i*16+8);table.writeUInt16LE(t.height,i*16+10);offset+=t.data.length;});
 if(offset>24*1024*1024)throw Error('Sprite download exceeds 24 MiB');
 const body=Buffer.concat([actions,rects,table,...textures.map(t=>t.data)]),header=Buffer.alloc(64);header.write('GPA2');header.writeUInt32LE(2,4);header.writeUInt32LE(offset,8);header.writeUInt32LE(frames.length,12);header.writeUInt32LE(textures.length,16);header.writeUInt32LE(sequences.length,20);header.writeFloatLE(scale,24);header.writeUInt16LE(fps,28);header.writeUInt16LE(nativeTimeline?1:0,30);createHash('sha256').update(body).digest().copy(header,32);return Buffer.concat([header,body]);
}
