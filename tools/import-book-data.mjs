// Regenerates source/game_book_data.h from the PC Paint By Letters data.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/books.json','utf8'));
const books=data.books.filter(book=>book.id==='book1'||book.id==='book2'||book.id==='book3');
const escape=value=>value.replaceAll('\\','\\\\').replaceAll('"','\\"').replaceAll('\n','\\n');
const maxPaint=Math.max(...books.flatMap(book=>book.pages.map(page=>(page.paintClips??page.paint??[]).length)))+1;
const lines=['#pragma once','','#define BOOK_COUNT 3',`#define BOOK_PAINT_MAX ${maxPaint}`,'',
 '/** Original paint progression: when the page text reaches `at` characters the page art goes to `frame` (0 = end). */',
 'typedef struct { unsigned short at,frame; const char *path; } BookPaint;',
 'typedef struct { const char *id,*title; unsigned room,pages; const char *const *texts; const BookPaint (*paint)[BOOK_PAINT_MAX]; } BookData;',''];
for(const book of books){
 lines.push(`static const char *const ${book.id.toUpperCase()}_TEXTS[] = {`);
 for(const page of book.pages)lines.push(`  "${escape(page.text)}",`);
 lines.push('};','');
 lines.push(`static const BookPaint ${book.id.toUpperCase()}_PAINT[][BOOK_PAINT_MAX] = {`);
 for(const page of book.pages){const steps=page.paintClips??(page.paint??[]).map(([at,frame])=>[at,'',frame]);lines.push(`  {${steps.map(([at,path,frame])=>`{${at},${frame},"${escape(path)}"}`).join(',')}${steps.length?',':''}{0,0,NULL}},`);}
 lines.push('};','');
}
lines.push('static const BookData BOOK_DATA[] = {');
for(const book of books)lines.push(`  {"${book.id}","${escape(book.title)}",${book.room},${book.pages.length},${book.id.toUpperCase()}_TEXTS,${book.id.toUpperCase()}_PAINT},`);
lines.push('};','');
await writeFile(root+'/source/game_book_data.h',lines.join('\n'));
console.log(`Wrote ${books.length} books and ${books.reduce((n,b)=>n+b.pages.length,0)} pages to source/game_book_data.h`);
