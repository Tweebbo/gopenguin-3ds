import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
const root=path.resolve(import.meta.dirname,'..'),folder=path.resolve(process.argv[2]??root+'/build/package-reviewed-20261028-final');
if(!folder.startsWith(root+'/build/'))throw Error('Use a local build directory.');
const manifest=JSON.parse(await readFile(folder+'/manifest.json')),validation=JSON.parse(await readFile(folder+'/validation.json'));
const version=(process.env.GOPENGUIN_VERSION??await readFile(root+'/VERSION','utf8')).trim(),base=`https://github.com/Tweebbo/gopenguin-3ds/releases/download/v${version}`;
if(!/^\d+\.\d+\.\d+$/.test(version))throw Error('Invalid release version.');
if(manifest.version!==version)throw Error('The packaged version differs from VERSION. Rebuild and repackage before publishing.');
if(manifest.titleId!=='0004000007E90200'||validation.status!=='PASS'||manifest.status!=='release-candidate')throw Error('The release package has not passed its identity checks.');
const defaultNotes=manifest.resources?'Optional update: a smaller application package with resources stored separately on SD. The full bridge is required before this release. Your saved penguin is retained. See gopenguin.lol/changelog.':'Optional update: install whenever convenient; no immediate update is required. Improves update preparation and fixes an igloo loading hang when leaving a game. Existing clients need this full bridge before smaller application updates. Your saved penguin is retained. See gopenguin.lol/changelog.';
const latest={schema:1,titleId:manifest.titleId,version,notes:process.env.RELEASE_NOTES??defaultNotes};
if(manifest.resources)latest.resources=manifest.resources;
if(manifest.resourceIndex)latest.resourceIndex=manifest.resourceIndex;
for(const kind of ['cia','3dsx']){
 const artifact=manifest.artifacts[kind],bytes=await readFile(folder+'/'+artifact.file);
 if(bytes.length!==artifact.bytes||createHash('sha256').update(bytes).digest('hex')!==artifact.sha256)throw Error('Artifact changed after validation: '+kind);
 latest[kind]={url:base+'/'+artifact.file,sha256:artifact.sha256,bytes:artifact.bytes};
}
await writeFile(folder+'/latest.json',JSON.stringify(latest,null,2)+'\n');
const previous=process.argv.slice(3);if(previous.length)execFileSync('python3',[root+'/tools/prepare-incremental.py','--manifest',folder+'/latest.json','--app','gopenguin','--repository','Tweebbo/gopenguin-3ds','--title-id',manifest.titleId,...previous.flatMap(base=>['--base',path.resolve(base)])],{stdio:'inherit'});
await writeFile(folder+'/publication-plan.json',JSON.stringify({status:'AWAITING_OWNER_REVIEW',version,titleId:manifest.titleId,repository:'Tweebbo/gopenguin-3ds',manifest:'https://gopenguin.lol/3ds/latest-v2.json',legacyManifest:'https://gopenguin.lol/3ds/latest.json',layout:manifest.resources?'split':'bridge',actionsAfterApproval:['Publish the exact validated CIA, 3DSX, patches, resource index, resource bundles and latest.json on GitHub.','Enable the local Caddy route for both manifest URLs.','Publish a full bridge to both channels before any split release; retain that full bridge on the legacy channel.','Publish split releases only on the v2 channel after validating every companion asset.','Verify public sizes, SHA-256, TLS and redirects.'],tweebboHostChanges:false},null,2)+'\n');
console.log('Prepared local latest.json; nothing published.');
