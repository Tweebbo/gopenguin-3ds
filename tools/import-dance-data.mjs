// Regenerates source/game_dance_data.h from the PC Dance Contest data.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const data=JSON.parse(await readFile(web+'/public/data/dance.json','utf8'));
const maxNotes=Math.max(...data.charts.map(chart=>chart.notes.length),...data.songs.map(song=>2*song.lengthBeats+1));
const dirs={left:0,down:1,up:2,right:3};
const f=value=>Number.isInteger(value)?`${value}.0f`:`${value}f`;
const lines=['#pragma once','#include <stdint.h>','',`#define DANCE_SONGS ${data.songs.length}`,`#define DANCE_CHARTS ${data.charts.length}`,`#define DANCE_MAX_NOTES ${maxNotes}`,'','typedef struct { float time,hold; uint8_t dir; } DanceNote;','typedef struct { const char *name; float millisPerBeat; unsigned lengthBeats; } DanceSongData;','typedef struct { uint8_t song,difficulty; uint16_t count; uint16_t approach; const DanceNote *notes; } DanceChartData;',''];
data.songs.forEach((song,index)=>lines.push(`static const char dance_song${index+1}_name[] = "${song.name.replaceAll('"','\\"')}";`));
lines.push(`static const DanceSongData DANCE_SONGS_DATA[] = {`);
data.songs.forEach((song,index)=>lines.push(`  {dance_song${index+1}_name,${f(song.millisPerBeat)},${song.lengthBeats}},`));
lines.push('};','');
data.charts.forEach((chart,index)=>{
 lines.push(`static const DanceNote dance_chart${index+1}_notes[] = {`);
 for(const note of [...chart.notes].sort((a,b)=>a[0]-b[0]))lines.push(`  {${f(note[0]*data.songs[chart.song-1].millisPerBeat)},${f(note[2]*data.songs[chart.song-1].millisPerBeat)},${dirs[note[1]]}},`);
 lines.push('};');
});
lines.push('static const DanceChartData DANCE_CHART_DATA[] = {');
data.charts.forEach((chart,index)=>lines.push(`  {${chart.song-1},${['easy','medium','hard'].indexOf(chart.difficulty)},${chart.notes.length},${chart.approach},dance_chart${index+1}_notes},`));
lines.push('};','');
await writeFile(root+'/source/game_dance_data.h',lines.join('\n'));
console.log(`Wrote ${data.charts.length} Dance charts (${maxNotes} notes max) to source/game_dance_data.h`);
