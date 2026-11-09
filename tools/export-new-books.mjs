// 3DS pages for the Penguin Style 2013-2014 issues and the Monster Maker, served externally like the
// other past issues (books/<id*1024+page>.gpk on the asset host). Ids continue after the pirate
// catalogue (39) so published pages never change address.
import {readFile,writeFile,mkdir,rename} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {exportScene} from './export-scene.mjs';
import {packMovie} from './pack-movie.mjs';
const root=path.resolve(import.meta.dirname,'..'),web=path.resolve(process.env.GOPENGUIN_WEB);
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const issues=JSON.parse(await readFile(web+'/public/data/catalogue-archive.json','utf8')).filter(i=>i.id>='2013-01'||i.id==='2011-10-monster-maker');
const ordered=[...issues.filter(i=>/^\d{4}-\d{2}$/.test(i.id)).sort((a,b)=>b.id.localeCompare(a.id)),...issues.filter(i=>!/^\d{4}-\d{2}$/.test(i.id))];
export const books=[];
for(const [n,issue] of ordered.entries()){
 const data=JSON.parse(await readFile(`${web}/public/swf/catalogue-archive/${issue.id}/timelines.json`,'utf8'));
 const frames=data.main.frames.length,first=frames>1&&data.main.frames[0].length===0?2:1;
 books.push({id:40+n,key:'archive/'+issue.id,pack:'catalogue-archive/'+issue.id,frames,first,title:/^\d{4}-\d{2}$/.test(issue.id)?'PENGUIN STYLE '+issue.title.toUpperCase():issue.title.toUpperCase()+' CATALOGUE'});
}
if(process.argv[1]===import.meta.filename){
 const only=process.argv.includes('--list');
 const folder=root+'/build/catalogue-new';await mkdir(folder,{recursive:true});await writeFile(folder+'/books.json',JSON.stringify(books,null,1)+'\n');
 if(only){console.log(JSON.stringify(books.map(b=>[b.id,b.key,b.frames,b.first])));process.exit(0);}
 const check=async file=>{const bytes=await readFile(file).catch(()=>null);if(!bytes||bytes.subarray(0,4).toString()!=='GPK1'||bytes.readUInt32LE(8)!==bytes.length||sha(bytes.subarray(64))!==bytes.subarray(32,64).toString('hex'))return null;const o=bytes.readUInt32LE(72),l=bytes.readUInt32LE(76);return {bytes:bytes.length,sha256:sha(bytes),metadata:JSON.parse(bytes.subarray(o,o+l))};};
 const workers=Number(process.env.CATALOGUE_WORKERS??2),jobs=books.flatMap(book=>Array.from({length:book.frames},(_,i)=>({book,page:i+1}))).filter(j=>j.page>=j.book.first);
 const report={expectedPages:jobs.length,pages:[]};let next=0,writing=Promise.resolve();
 await mkdir(root+'/build/resources/books',{recursive:true});
 await Promise.all(Array.from({length:workers},async()=>{const shared={};try{
  while(next<jobs.length){
   const {book,page}=jobs[next++];const code=book.id*1024+page,file=root+`/build/resources/books/${code}.gpk`;let entry=await check(file);
   if(!entry||entry.metadata.bookId!==book.id||entry.metadata.page!==page){
    if(shared.book!==undefined&&shared.book!==book.id){await shared.page?.close();shared.page=undefined;}shared.book=book.id;
    const name=`book-${book.id}-${page}`;
    await exportScene(['node','export-scene',name,book.pack,'book',String(page),'--page','--external','--single-reference','--catalogue='+book.key],shared);
    await packMovie(name,file,{clip:'book',frames:book.frames,bookId:book.id,page});entry=await check(file);
   }
   if(!entry||entry.metadata.bookId!==book.id||entry.metadata.page!==page)throw Error('Invalid page '+code);
   report.pages.push({code,key:book.key,page,bytes:entry.bytes,sha256:entry.sha256});console.log('READY',book.key,'page',page,`(${report.pages.length}/${jobs.length})`);
   const snapshot=JSON.stringify(report,null,1);writing=writing.then(async()=>{await writeFile(folder+'/report.json.tmp',snapshot);await rename(folder+'/report.json.tmp',folder+'/report.json');});await writing;
  }}finally{await shared.browser?.close();}}));
 report.status='PASS';report.bytes=report.pages.reduce((s,p)=>s+p.bytes,0);await writeFile(folder+'/report.json',JSON.stringify(report,null,1));console.log('PASS',report.pages.length,'pages',report.bytes,'bytes');
}
