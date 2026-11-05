// Regenerates source/game_jetpack_data.h from the PC level data.
// Usage: node tools/import-jetpack-data.mjs [web-root]
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/jetpack.json','utf8'));
const levels=[];const seen=new Set();
for(const level of data.levels){if(seen.has(level.level))continue;seen.add(level.level);levels.push(level);}
const maxW=Math.max(...levels.map(level=>level.width)),maxH=Math.max(...levels.map(level=>level.height));
const lines=['#pragma once','#include <stdint.h>','',`#define JETPACK_LEVELS ${levels.length}`,`#define JETPACK_MAX_W ${maxW}`,`#define JETPACK_MAX_H ${maxH}`,'#define JETPACK_TILE 80','','#define JETPACK_HAZARD 480','','/* hazards: the level\'s 480x480 terrain pieces (hazard frame per cell, 1 = empty), hazardW x hazardH. */','typedef struct { const char *name; unsigned width,height,startX,startY; const unsigned char *tiles; unsigned hazardW,hazardH; const unsigned char *hazards; } JetpackLevelData;',''];
levels.forEach((level,index)=>{
 lines.push(`static const unsigned char jetpack_level${index+1}_tiles[] = {`);
 const values=level.map.flat();
 for(let at=0;at<values.length;at+=32)lines.push(values.slice(at,at+32).join(',')+',');
 lines.push('};','');
 lines.push(`static const unsigned char jetpack_level${index+1}_hazards[] = {`);
 for(const row of level.hazards)lines.push(row.join(',')+',');
 lines.push('};','');
});
lines.push('static const JetpackLevelData JETPACK_LEVEL_DATA[] = {');
levels.forEach((level,index)=>lines.push(`  {"${level.name}",${level.width},${level.height},${level.start[0]},${level.start[1]},jetpack_level${index+1}_tiles,${level.hazards[0].length},${level.hazards.length},jetpack_level${index+1}_hazards},`));
lines.push('};','');
await writeFile(root+'/source/game_jetpack_data.h',lines.join('\n'));
console.log(`Wrote ${levels.length} Jetpack levels (${maxW}x${maxH} max) to source/game_jetpack_data.h`);
