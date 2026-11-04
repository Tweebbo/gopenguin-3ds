// Regenerates source/game_book_data.h from the PC Paint By Letters data.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/books.json','utf8'));
const books=data.books.filter(book=>book.id==='book1'||book.id==='book2'||book.id==='book3');
const escape=value=>value.replaceAll('\\','\\\\').replaceAll('"','\\"').replaceAll('\n','\\n');
const lines=['#pragma once','','#define BOOK_COUNT 3','','typedef struct { const char *id,*title; unsigned room,pages; const char *const *texts; } BookData;',''];
for(const book of books){
 lines.push(`static const char *const ${book.id.toUpperCase()}_TEXTS[] = {`);
 for(const page of book.pages)lines.push(`  "${escape(page.text)}",`);
 lines.push('};','');
}
lines.push('static const BookData BOOK_DATA[] = {');
for(const book of books)lines.push(`  {"${book.id}","${escape(book.title)}",${book.room},${book.pages.length},${book.id.toUpperCase()}_TEXTS},`);
lines.push('};','');
await writeFile(root+'/source/game_book_data.h',lines.join('\n'));
console.log(`Wrote ${books.length} books and ${books.reduce((n,b)=>n+b.pages.length,0)} pages to source/game_book_data.h`);
