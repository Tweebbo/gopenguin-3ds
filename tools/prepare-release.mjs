import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=path.resolve(process.argv[2]??root+'/build/package-reviewed-20261028-final');
if(!folder.startsWith(root+'/build/'))throw Error('Use a local build directory.');
const manifest=JSON.parse(await readFile(folder+'/manifest.json')),validation=JSON.parse(await readFile(folder+'/validation.json'));
const version=(await readFile(root+'/VERSION','utf8')).trim(),base=`https://github.com/Tweebbo/gopenguin-3ds/releases/download/v${version}`;
if(!/^\d+\.\d+\.\d+$/.test(version))throw Error('Invalid release version.');
if(manifest.version!==version)throw Error('The packaged version differs from VERSION. Rebuild and repackage before publishing.');
if(manifest.titleId!=='0004000007E90200'||validation.status!=='PASS'||manifest.status!=='release-candidate')throw Error('The release package has not passed its identity checks.');
const latest={schema:1,titleId:manifest.titleId,version,notes:'Town and more room music, complete puffle care animations, steadier dance-floor colours, crowded-room improvements and update download speed. Your saved penguin is retained. Install, then close and reopen GoPenguin.'};
for(const kind of ['cia','3dsx']){
 const artifact=manifest.artifacts[kind],bytes=await readFile(folder+'/'+artifact.file);
 if(bytes.length!==artifact.bytes||createHash('sha256').update(bytes).digest('hex')!==artifact.sha256)throw Error('Artifact changed after validation: '+kind);
 latest[kind]={url:base+'/'+artifact.file,sha256:artifact.sha256,bytes:artifact.bytes};
}
await writeFile(folder+'/latest.json',JSON.stringify(latest,null,2)+'\n');
await writeFile(folder+'/publication-plan.json',JSON.stringify({status:'AWAITING_OWNER_REVIEW',version,titleId:manifest.titleId,repository:'Tweebbo/gopenguin-3ds',manifest:'https://gopenguin.lol/3ds/latest.json',actionsAfterApproval:['Publish the exact validated artifacts and latest.json on GitHub.','Serve the same latest.json at the GoPenguin manifest URL.','Verify public sizes, SHA-256, TLS and redirects.'],tweebboHostChanges:false},null,2)+'\n');
console.log('Prepared local latest.json; nothing published.');
