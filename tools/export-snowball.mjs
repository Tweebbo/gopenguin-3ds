#!/usr/bin/env node
// Builds romfs:/gfx/snowball.t3x from the original town snowball shapes (3/4/6).
// The same shapes are 1/2/4 in the seasonal Halloween town.
import {createRequire} from 'node:module';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {spawn} from 'node:child_process';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.env.TWEEBBO_WEB??path.join(root,'../Tweebbo-Penguin'));
const require=createRequire(web+'/package.json');
const {chromium}=require('playwright');
const timeline=JSON.parse(await readFile(web+'/public/swf/town/timelines.json','utf8'));
const pack=await readFile(web+'/public/swf/town/chars/chars.pack');
const svg=id=>{
  const entry=timeline.pack.entries[`${id}.svg`];
  if(!entry)throw Error(`Missing snowball shape ${id}.svg`);
  return pack.subarray(entry[0],entry[0]+entry[1]).toString('utf8');
};
const shapes={
  shadow:{id:3,svg:svg(3),x:0,y:0,w:timeline.characters[3].size[0],h:timeline.characters[3].size[1]},
  ball:{id:4,svg:svg(4),x:12,y:0,w:timeline.characters[4].size[0],h:timeline.characters[4].size[1]},
  impact:{id:6,svg:svg(6),x:24,y:0,w:timeline.characters[6].size[0],h:timeline.characters[6].size[1]},
};

await mkdir(root+'/build/gfx',{recursive:true});
await mkdir(root+'/romfs/gfx',{recursive:true});
const browser=await chromium.launch({executablePath:'/usr/bin/google-chrome',headless:true});
try{
  const page=await browser.newPage();
  const png=await page.evaluate(async shapes=>{
    const canvas=document.createElement('canvas');
    canvas.width=64;canvas.height=16;
    const ctx=canvas.getContext('2d');
    for(const shape of Object.values(shapes)){
      const image=new Image();
      image.src='data:image/svg+xml;base64,'+btoa(unescape(encodeURIComponent(shape.svg)));
      await image.decode();
      ctx.drawImage(image,shape.x,shape.y,shape.w,shape.h);
    }
    return canvas.toDataURL('image/png');
  },shapes);
  await writeFile(root+'/build/gfx/snowball.png',Buffer.from(png.split(',')[1],'base64'));
}finally{await browser.close();}

const child=spawn('podman',['run','--rm','--network','none','--cpus','2','--memory','1g','-v',root+':/work:z','-w','/work',
  'docker.io/devkitpro/devkitarm:latest','tex3ds','-f','rgba8888','-z','auto','-o','romfs/gfx/snowball.t3x','build/gfx/snowball.png'],{stdio:'inherit'});
if(await new Promise(resolve=>child.on('exit',resolve)))throw Error('tex3ds failed');
await writeFile(root+'/build/gfx/snowball.json',JSON.stringify({
  source:'Tweebbo-Penguin/public/swf/town/chars/chars.pack',
  shapes:Object.fromEntries(Object.entries(shapes).map(([name,s])=>[name,{id:s.id,x:s.x,y:s.y,w:s.w,h:s.h}])),
},null,2)+'\n');
console.log('snowball atlas: build/gfx/snowball.png -> romfs/gfx/snowball.t3x');
