import {createRequire} from 'node:module';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB??root+'/../web-server/web');
const [name,pack,kind]=process.argv.slice(2);if(!/^[a-z0-9-]+$/.test(name??'')||!/^[A-Za-z0-9_/-]+$/.test(pack??name)||String(pack).includes('..'))throw Error('Use a local room or game package.');
const require=createRequire(web+'/package.json'),{chromium}=require('playwright'),{build}=require('esbuild');
const bundle=(await build({stdin:{contents:"export {Movie} from './src/swf/movie';export {Room} from './src/engine/room';export {roomScript} from './src/rooms';",resolveDir:web},bundle:true,format:'iife',globalName:'Original',write:false})).outputFiles[0].text;
const browser=await chromium.launch({headless:true,executablePath:'/usr/bin/google-chrome',args:['--mute-audio']});
try{
 const page=await browser.newPage();
 await page.route('http://art.test/**',async route=>{
  const url=new URL(route.request().url());if(url.pathname==='/')return route.fulfill({body:'<!doctype html><body></body>',contentType:'text/html'});
  const file=path.resolve(web+'/public','.'+decodeURIComponent(url.pathname));if(!file.startsWith(web+'/public/'))return route.abort();
  try{await route.fulfill({body:await readFile(file),contentType:file.endsWith('.json')?'application/json':file.endsWith('.svg')?'image/svg+xml':file.endsWith('.ttf')?'font/ttf':'application/octet-stream'});}catch{await route.fulfill({status:404,body:'Not found'});}
 });
 await page.goto('http://art.test/');await page.addScriptTag({content:bundle});
 const images=await page.evaluate(async({name,pack,kind,frame})=>{
  const {Movie,Room,roomScript}=Original,base=await Movie.load('/swf/'+pack+'/'),clip=name==='beans'?'board':'root',movie=kind==='game'?base.asClip(clip):base;
  movie.muted=true;movie.rasterize=false;const script=kind==='game'?{start:[380,340]}:roomScript(name,movie);script.setup?.(movie,'english');
  if(kind==='game')movie.goto(clip,frame);
  const room=kind==='game'?{draw:ctx=>movie.draw(ctx)}:new Room(name,movie,script);
  await movie.preloadAnimation(1);
  const warm=document.createElement('canvas');warm.width=400;warm.height=240;room.draw(warm.getContext('2d'));await movie.ready();
  const result=[];
  for(const [label,zoom] of [['overview',1],['zoom',1.2]]){
   const canvas=document.createElement('canvas');canvas.width=400;canvas.height=240;const ctx=canvas.getContext('2d'),scale=400/760*zoom;
   ctx.setTransform(scale,0,0,scale,200-380*scale,120-240*scale);room.draw(ctx);await movie.ready();ctx.clearRect(-1000,-1000,3000,3000);room.draw(ctx);
   result.push({label,png:canvas.toDataURL()});
  }
  return result;
 },{name,pack:pack??name,kind,frame:Number(process.env.SWL_RENDER_FRAME??1)});
 const out=root+'/build/sharpness/'+name;await mkdir(out,{recursive:true});
 for(const img of images)await writeFile(out+'/source-'+img.label+'.png',Buffer.from(img.png.split(',')[1],'base64'));
 console.log('Rendered original vectors at 400x240: '+name);
}finally{await browser.close();}
