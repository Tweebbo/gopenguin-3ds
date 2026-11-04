// Regenerates source/game_mission_flow_data.h from the PC mission 4-11 data.
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..');
const web=path.resolve(process.argv[2]??process.env.GOPENGUIN_WEB??root+'/../gopenguin-dev/web');
const clean=value=>String(value).replaceAll('\u2019',"'").replaceAll('\u2018',"'").replaceAll('\u201c','"').replaceAll('\u201d','"');
const escape=value=>clean(value).replaceAll('\\','\\\\').replaceAll('"','\\"').replaceAll('\n','\\n');
const missions=[];
for(let mission=4;mission<=11;mission++)missions.push(JSON.parse(await readFile(`${web}/public/data/mission${mission}.json`,'utf8')));
const maxScenes=Math.max(...missions.map(mission=>Object.keys(mission.scenes).length));
const maxSteps=Math.max(...missions.map(mission=>mission.steps.length));
const lines=['#pragma once','#include <stdint.h>','',`#define MISSION_FLOW_COUNT ${missions.length}`,`#define MISSION_FLOW_MAX_SCENES ${maxScenes}`,`#define MISSION_FLOW_MAX_STEPS ${maxSteps}`,'','typedef struct { uint8_t type,neighbours,count,target,order[8]; const char *code,*labels[8]; } MissionPuzzle;','typedef struct { uint8_t scene; const char *title,*text,*objective,*action; MissionPuzzle puzzle; } MissionStep;','typedef struct { uint8_t mission,sceneCount,stepCount; uint16_t room,reward; const char *title; const char *sceneKeys[MISSION_FLOW_MAX_SCENES]; const char *scenePacks[MISSION_FLOW_MAX_SCENES]; MissionStep steps[MISSION_FLOW_MAX_STEPS]; } MissionData;',''];
for(const mission of missions){
 const sceneKeys=Object.keys(mission.scenes),packs=sceneKeys.map(key=>`missions/${mission.mission}/${key}.gpk`);
 const name=`MISSION${mission.mission}`;
 lines.push(`static const MissionData ${name} = {`);
 lines.push(` ${mission.mission},${sceneKeys.length},${mission.steps.length},${mission.room},${mission.reward},"${escape(mission.title)}",`);
 lines.push(` {${sceneKeys.map(key=>`"${escape(key)}"`).join(',')}},`);
 lines.push(` {${packs.map(pack=>`"${escape(pack)}"`).join(',')}},`);
 lines.push(' {');
 for(const step of mission.steps){
  const scene=sceneKeys.indexOf(step.scene);if(scene<0)throw Error(`Unknown scene ${step.scene}`);
  let puzzle='{0,0,0,0,{0,0,0,0,0,0,0,0},0,{0}}';
  if(step.puzzle?.type==='pattern'){
   const target=step.puzzle.target.reduce((mask,value,index)=>mask|(value?1<<index:0),0);
   puzzle=`{1,${step.puzzle.neighbours?1:0},${step.puzzle.target.length},${target},{0,0,0,0,0,0,0,0},0,{0}}`;
  }else if(step.puzzle?.type==='code'){
   puzzle=`{2,0,${step.puzzle.code.length},0,{0,0,0,0,0,0,0,0},"${escape(step.puzzle.code.toUpperCase())}",{0}}`;
  }else if(step.puzzle?.type==='order'){
   const order=[...step.puzzle.order,...Array(8-step.puzzle.order.length).fill(0)];
   puzzle=`{3,0,${step.puzzle.labels.length},0,{${order.join(',')}},0,{${step.puzzle.labels.map(label=>`"${escape(label)}"`).join(',')}}}`;
  }
  lines.push(`  {${scene},"${escape(step.title)}","${escape(step.text)}","${escape(step.objective)}","${escape(step.action)}",${puzzle}},`);
 }
 lines.push(' },','};','');
}
lines.push(`static const MissionData *const MISSION_FLOW_DATA[]={${missions.map(mission=>`&MISSION${mission.mission}`).join(',')}};`,'');
await writeFile(root+'/source/game_mission_flow_data.h',lines.join('\n'));
console.log(`Wrote ${missions.length} missions and ${missions.reduce((n,m)=>n+m.steps.length,0)} steps to source/game_mission_flow_data.h`);
