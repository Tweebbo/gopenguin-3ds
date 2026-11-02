import path from 'node:path';
import {exportScene} from './export-scene.mjs';
import {packMovie} from './pack-movie.mjs';
const root=path.resolve(import.meta.dirname,'..'),id=Number(process.argv[2]);
if(!Number.isInteger(id)||id<1||id>1000000)throw Error('Expected a clothing ID');
process.env.SWL_TEXTURE_BUDGET=String(2*1024*1024);
const name='outfit-'+id;
await exportScene(['node','export-scene.mjs',name,'assets/clothing/sprites/'+id,'root','1','--external','--single-reference']);
await packMovie(name,root+'/romfs/outfits/'+id+'.gpa',{outfit:id});
