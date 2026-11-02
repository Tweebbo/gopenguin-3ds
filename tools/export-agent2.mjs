import path from 'node:path';
import {exportScene} from './export-scene.mjs';
import {packMovie} from './pack-movie.mjs';

const root=path.resolve(import.meta.dirname,'..');
const rooms=['sportShop','mountain','hillSigns','miniGame','crashSite','clearing1','clearing2','puffles','river','outsideCave','cave','guide'];
const requested=process.argv.slice(2);
for(const room of requested.length?requested:rooms){
  if(!rooms.includes(room))throw Error('Unknown mission 2 scene: '+room);
  const name='mission2-'+room.toLowerCase();
  const recovery=process.env.SWL_RECOVERY_SCALE;
  if(room==='miniGame')process.env.SWL_RECOVERY_SCALE='1';
  await exportScene(['node','export-scene',name,'assets/missions/2/'+room,'root','1','--external','--single-reference']);
  if(recovery===undefined)delete process.env.SWL_RECOVERY_SCALE;else process.env.SWL_RECOVERY_SCALE=recovery;
  await packMovie(name,root+'/romfs/missions/2/'+room+'.gpk',{mission:2,room});
}
