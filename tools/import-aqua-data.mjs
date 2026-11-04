// Regenerates source/game_aqua_data.h from the PC Aqua Grabber maps.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/aqua.json','utf8'));
const levels=data.levels,maxW=Math.max(...levels.map(l=>l.width)),maxH=Math.max(...levels.map(l=>l.height));
const lines=['#pragma once','#include <stdint.h>','',`#define AQUA_LEVELS ${levels.length}`,`#define AQUA_MAX_W ${maxW}`,`#define AQUA_MAX_H ${maxH}`,'#define AQUA_TILE 80','','typedef struct { unsigned level,width,height,startX,startY,netX,netY,time; const uint16_t *map; } AquaLevelData;',''];
levels.forEach((level,index)=>{
 lines.push(`static const uint16_t aqua_level${index+1}_map[] = {`);
 const values=level.map.flat();for(let at=0;at<values.length;at+=32)lines.push(values.slice(at,at+32).join(',')+',');
 lines.push('};','');
});
lines.push('static const AquaLevelData AQUA_LEVEL_DATA[] = {');
levels.forEach((level,index)=>lines.push(`  {${level.level},${level.width},${level.height},${level.start[0]},${level.start[1]},${level.net[0]},${level.net[1]},${level.level===1?1440:5760},aqua_level${index+1}_map},`));
lines.push('};','');
await writeFile(root+'/source/game_aqua_data.h',lines.join('\n'));
console.log(`Wrote ${levels.length} Aqua levels (${maxW}x${maxH} max) to source/game_aqua_data.h`);
