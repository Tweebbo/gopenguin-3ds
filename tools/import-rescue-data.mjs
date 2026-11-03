// Regenerates source/game_rescue_data.h from the PC Puffle Rescue data.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/rescue.json','utf8'));
const levels=data.levels;
const maxW=Math.max(...levels.map(level=>level.width)),maxH=Math.max(...levels.map(level=>level.height));
const lines=['#pragma once','#include <stdint.h>','',`#define RESCUE_LEVELS ${levels.length}`,`#define RESCUE_MAX_W ${maxW}`,`#define RESCUE_MAX_H ${maxH}`,'#define RESCUE_TILE 80','#define RESCUE_PUFFLES 3','', 'typedef struct { const char *name; unsigned theme,width,height,homeX,homeY; const uint16_t *map; const uint8_t *parts; const uint8_t *parts2; const uint8_t *puffleX; const uint8_t *puffleY; } RescueLevelData;',''];
levels.forEach((level,index)=>{
 lines.push(`static const uint16_t rescue_level${index+1}_map[] = {`);
 const map=level.map.flat();for(let at=0;at<map.length;at+=32)lines.push(map.slice(at,at+32).join(',')+',');
 lines.push('};');
 for(const [key,arr] of [['parts',level.parts],['parts2',level.parts2]]){
  lines.push(`static const uint8_t rescue_level${index+1}_${key}[] = {`);
  const flat=arr.flat();for(let at=0;at<flat.length;at+=32)lines.push(flat.slice(at,at+32).join(',')+',');
  lines.push('};');
 }
 lines.push(`static const uint8_t rescue_level${index+1}_puffleX[] = {${level.puffles.map(p=>p[0]).join(',')}};`);
 lines.push(`static const uint8_t rescue_level${index+1}_puffleY[] = {${level.puffles.map(p=>p[1]).join(',')}};`);
 lines.push('');
});
lines.push('static const RescueLevelData RESCUE_LEVEL_DATA[] = {');
const themeIndex={snow:0,temple:1,volcanoe:2};
levels.forEach((level,index)=>lines.push(`  {"${level.name}",${themeIndex[level.theme]},${level.width},${level.height},${level.home[0]},${level.home[1]},rescue_level${index+1}_map,rescue_level${index+1}_parts,rescue_level${index+1}_parts2,rescue_level${index+1}_puffleX,rescue_level${index+1}_puffleY},`));
lines.push('};','');
await writeFile(root+'/source/game_rescue_data.h',lines.join('\n'));
console.log(`Wrote ${levels.length} Puffle Rescue levels (${maxW}x${maxH} max) to source/game_rescue_data.h`);
