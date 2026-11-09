// Adds the covers of the books in export-new-books.mjs to the existing 512x512 atlas (BOOK_COVERS_BASE).
import {createRequire} from 'node:module';
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {books as all} from './export-new-books.mjs';
const books=all;

const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB),require=createRequire(web+'/package.json');
const {chromium}=require('playwright'),{build}=require('esbuild');
const code=(await build({entryPoints:[web+'/src/swf/movie.ts'],bundle:true,format:'iife',globalName:'Native',write:false})).outputFiles[0].text;
const browser=await chromium.launch({headless:true,executablePath:'/usr/bin/google-chrome',args:['--mute-audio']});
try{
 const page=await browser.newPage();
 await page.route('http://asset.test/**',async route=>{
  const url=new URL(route.request().url());if(url.pathname==='/')return route.fulfill({body:'<body></body>',contentType:'text/html'});
  const file=web+'/public'+decodeURIComponent(url.pathname);return route.fulfill({body:await readFile(file),contentType:file.endsWith('.png')?'image/png':file.endsWith('.svg')?'image/svg+xml':file.endsWith('.woff')?'font/woff':'application/json'});
 });
 await page.goto('http://asset.test');await page.addScriptTag({content:code});
 const base=(await readFile(process.env.BOOK_COVERS_BASE)).toString('base64');
 await page.evaluate(async png=>{window.covers=document.createElement('canvas');covers.width=512;covers.height=512;const image=new Image();image.src='data:image/png;base64,'+png;await image.decode();covers.getContext('2d').drawImage(image,0,0);},base);
 const report=[];
 for(const book of books){
  report.push(await page.evaluate(async book=>{
   const source=await Native.Movie.load('/swf/'+book.pack+'/'),movie=source.asClip('book');movie.goto('book',book.first??1);
   const full=document.createElement('canvas');full.width=380;full.height=240;const f=full.getContext('2d');
   for(let pass=0;pass<3;pass++){f.setTransform(1,0,0,1,0,0);f.clearRect(0,0,380,240);f.scale(.5,.5);movie.draw(f);await movie.ready();}
   const rgba=f.getImageData(0,0,380,240).data,grey=i=>{const r=rgba[i*4],g=rgba[i*4+1],b=rgba[i*4+2];return Math.abs(r-g)<6&&Math.abs(g-b)<6&&r>=180&&r<=215;},seen=new Uint8Array(380*240),queue=new Uint32Array(seen.length);let largest=0,b;
   for(let start=0;start<seen.length;start++){
    if(seen[start]||rgba[start*4+3]<200||grey(start))continue;let head=0,tail=1,x1=380,y1=240,x2=0,y2=0;queue[0]=start;seen[start]=1;
    while(head<tail){const at=queue[head++],x=at%380,y=Math.floor(at/380);x1=Math.min(x1,x);y1=Math.min(y1,y);x2=Math.max(x2,x+1);y2=Math.max(y2,y+1);
     for(const n of [x?at-1:-1,x<379?at+1:-1,y?at-380:-1,y<239?at+380:-1])if(n>=0&&!seen[n]&&rgba[n*4+3]>=200&&!grey(n)){seen[n]=1;queue[tail++]=n;}
    }
    if(tail>largest){largest=tail;b=[x1,y1,x2,y2];}
   }
   if(!b||largest<20)throw Error('Empty original cover '+book.key);
   const tile=document.createElement('canvas');tile.width=tile.height=64;const c=tile.getContext('2d'),scale=Math.min(60/(b[2]-b[0]),60/(b[3]-b[1])),w=(b[2]-b[0])*scale,h=(b[3]-b[1])*scale;
   c.drawImage(full,b[0],b[1],b[2]-b[0],b[3]-b[1],(64-w)/2,(64-h)/2,w,h);
   const index=book.id-1;covers.getContext('2d').drawImage(tile,index%8*64,Math.floor(index/8)*64);movie.dispose();source.dispose();return {id:book.id,key:book.key,bounds:b};
  },book));
 }
 const png=await page.evaluate(()=>covers.toDataURL());await writeFile(root+'/build/gfx/book-covers.png',Buffer.from(png.split(',')[1],'base64'));
 await writeFile(root+'/build/catalogue-new/book-covers.json',JSON.stringify({source:'original catalogue first frames; proportional fit, no replacement artwork',covers:report},null,2)+'\n');
 console.log('PASS '+books.length+' new original catalogue covers added to the existing atlas, stable book IDs, 64px cells');
}finally{await browser.close();}
