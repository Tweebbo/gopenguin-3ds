import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),stage=root+'/build/resource-layout',mode=process.argv[2];
const run=(command,args)=>execFileSync(command,args,{cwd:root,stdio:'inherit'});
if(mode==='prepare'){
 const version=(await readFile(root+'/VERSION','utf8')).trim(),layout=(await readFile(root+'/RESOURCE_LAYOUT','utf8')).trim();
 if(!['bridge','split'].includes(layout))throw Error('RESOURCE_LAYOUT must be bridge or split.');
 const tags=JSON.parse(execFileSync('gh',['release','list','-R','Tweebbo/gopenguin-3ds','--exclude-drafts','--exclude-pre-releases','--limit','10','--json','tagName'],{encoding:'utf8'}));
 const bases=[];
 for(const {tagName:tag} of tags){
  if(bases.length===4)break;if(tag==='v'+version||!/^v\d+\.\d+\.\d+$/.test(tag))continue;
  const release=JSON.parse(execFileSync('gh',['release','view',tag,'-R','Tweebbo/gopenguin-3ds','--json','assets'],{encoding:'utf8'}));
  if(!release.assets.some(a=>a.name==='gopenguin-resources.idx'))continue;
  const folder=root+'/build/resource-base-'+tag;await mkdir(folder,{recursive:true});
  run('gh',['release','download',tag,'-R','Tweebbo/gopenguin-3ds','--pattern','latest.json','--pattern','gopenguin-resources.idx','--dir',folder,'--clobber']);
  const previous=JSON.parse(await readFile(folder+'/latest.json','utf8')),offer=previous.resources?.index??previous.resourceIndex,bytes=await readFile(folder+'/gopenguin-resources.idx');
  if(previous.version!==tag.slice(1)||offer?.url!==`https://github.com/Tweebbo/gopenguin-3ds/releases/download/${tag}/gopenguin-resources.idx`||offer.bytes!==bytes.length||offer.sha256!==createHash('sha256').update(bytes).digest('hex'))throw Error('Resource base differs from its published manifest.');
  bases.push(tag.slice(1)+'='+folder+'/gopenguin-resources.idx');
 }
 run('python3',['tools/stage-resource-layout.py','--romfs','romfs','--output',stage,'--version',version,...bases.flatMap(base=>['--base-index',base])]);
 await writeFile(stage+'/build.json',JSON.stringify({layout,version},null,2)+'\n');
}else if(mode==='build'||mode==='package'){
 const config=JSON.parse(await readFile(stage+'/build.json','utf8')),split=config.layout==='split';
 if(mode==='build')run('bash',['build.sh',...(split?['RESOURCE_SPLIT=1','ROMFS=build/resource-layout/romfs']:[])]);
 else run(process.execPath,['tools/package-native.mjs','--release','--elf',split?'out/gopenguin-split-candidate.elf':'out/gopenguin-3ds-demo.elf','--romfs',split?'build/resource-layout/romfs':'romfs','--output','build/package-release','--tools',process.argv[3]??'tools/ctrtools',...(split?['--resources',stage]:['--resource-index',stage+'/gopenguin-resources.idx'])]);
}else throw Error('Use prepare, build or package. These commands do not publish.');
