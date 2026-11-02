import {readFile,mkdir,copyFile,writeFile} from 'node:fs/promises';
import {spawn} from 'node:child_process';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..');
const header=await readFile(root+'/source/island_rooms.h','utf8');
const rooms=new Map([...header.matchAll(/\{(\d+),(\d+),"([a-z0-9-]+)",/g)].map(m=>[m[3],{id:Number(m[1]),pages:Number(m[2])}]));
const names=process.argv.slice(2);if(!names.length)throw Error('Choose rooms from source/island_rooms.h.');
const run=(cmd,args,env={})=>new Promise((resolve,reject)=>{
 const child=spawn(cmd,args,{cwd:root,env:{...process.env,...env},stdio:'inherit'});child.once('error',reject);child.once('exit',code=>code===0?resolve():reject(Error(cmd+' failed: '+code)));
});
await mkdir(root+'/build/sharpness',{recursive:true});const report=[];
for(const name of names){
 const room=rooms.get(name);if(!room)throw Error('Unknown room '+name);
 let resident=0;
 for(let i=0;i<room.pages;i++){const tex=await readFile(root+`/build/sharpness/baseline-romfs/gfx/swl-${name}-${i}.t3x`);resident+=1024*512*(tex[3]===0?4:tex[3]===1?3:tex[3]<=5?2:1);}
 await run(process.execPath,['tools/export-scene.mjs',name,'--external'],{SWL_TEXTURE_BUDGET:String(resident),SWL_TEXTURE_PAGES:String(room.pages),SWL_ROOM_BAKE_SCALE:String(2/3)});
 const folder=root+'/build/scenes/'+name,manifest=JSON.parse(await readFile(folder+'/manifest.json'));
 if(manifest.pages!==room.pages||manifest.textureBytes>resident)throw Error(name+': resource layout or memory grew');
 const out=root+'/build/sharpness/romfs';await mkdir(out+'/scenes',{recursive:true});await mkdir(out+'/gfx',{recursive:true});
 for(let i=0;i<manifest.pages;i++)await run('podman',['run','--rm','--network','none','--cpus','2','--memory','1g','-v',root+':/work:z','-w','/work',process.env.DEVKITARM_IMAGE??'localhost/native-3ds-libctru-fixed:latest','tex3ds','-f',manifest.textureFormat,'-z','auto','-o',`build/sharpness/romfs/gfx/swl-${name}-${i}.t3x`,`build/scenes/${name}/atlas-${i}.png`]);
 await copyFile(folder+'/scene.swl',out+'/scenes/'+name+'.swl');
 report.push({room:name,pages:manifest.pages,beforeTextureBytes:resident,afterTextureBytes:manifest.textureBytes,bakeScale:manifest.roomBakeScale,format:manifest.textureFormat});
 await writeFile(root+'/build/sharpness/rooms.json',JSON.stringify(report,null,2)+'\n');
 console.log('SHARP ROOM '+name+': '+manifest.textureBytes+' texture bytes, unchanged page count.');
}
