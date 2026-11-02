import {readFile,writeFile,mkdir,rename} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {spawn} from 'node:child_process';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
export async function run(command,args){const child=spawn(command,args,{cwd:root,stdio:'inherit'});const status=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',resolve);});if(status!==0)throw Error(command+' failed');}
export async function packMovie(name,destination,fields={}){
 const folder=root+'/build/scenes/'+name,manifest=JSON.parse(await readFile(folder+'/manifest.json')),metadata={...JSON.parse(await readFile(folder+'/movie.json')),...fields};
 const parts=[await readFile(folder+'/scene.swl'),Buffer.from(JSON.stringify(metadata))];
 const container=process.env.GOPENGUIN_TEXTURE_CONTAINER;
 if(container&&!/^[a-zA-Z0-9][a-zA-Z0-9_.-]*$/.test(container))throw Error('Invalid texture worker container name.');
 const texture=async i=>{
  const args=['tex3ds','-f',manifest.textureFormat,'-z','auto','-o',`build/scenes/${name}/atlas-${i}.t3x`,`build/scenes/${name}/atlas-${i}.png`];
  await run('podman',container?['exec',container,...args]:['run','--rm','--network','none','--cpus','2','--memory','1g','-v',root+':/work:z','-w','/work',process.env.DEVKITARM_IMAGE??'localhost/native-3ds-libctru-fixed:latest',...args]);
 };
 if(container)await Promise.all(Array.from({length:manifest.pages},(_,i)=>texture(i)));
 else for(let i=0;i<manifest.pages;i++)await texture(i);
 for(let i=0;i<manifest.pages;i++)parts.push(await readFile(folder+`/atlas-${i}.t3x`));
 const table=Buffer.alloc(parts.length*8);let offset=64+table.length;parts.forEach((part,i)=>{table.writeUInt32LE(offset,i*8);table.writeUInt32LE(part.length,i*8+4);offset+=part.length;});if(offset>24*1024*1024)throw Error('Movie package exceeds download budget');
 const body=Buffer.concat([table,...parts]),header=Buffer.alloc(64);header.write('GPK1');header.writeUInt32LE(1,4);header.writeUInt32LE(offset,8);header.writeUInt32LE(parts.length,12);createHash('sha256').update(body).digest().copy(header,32);
 await mkdir(path.dirname(destination),{recursive:true});await writeFile(destination+'.tmp',Buffer.concat([header,body]));await rename(destination+'.tmp',destination);console.log(`PASS ${name}: ${offset} download bytes, ${manifest.textureBytes} resident texture bytes`);
}
