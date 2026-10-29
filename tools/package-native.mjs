import {readFile,writeFile,mkdir,readdir,copyFile,stat,access} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {spawn} from 'node:child_process';
import path from 'node:path';

const root=path.resolve(import.meta.dirname,'..');
const version=(await readFile(root+'/VERSION','utf8')).trim();
if(!/^\d+\.\d+\.\d+$/.test(version))throw Error('VERSION must contain a semantic release version.');
const [major,minor,micro]=version.split('.').map(Number);
if(major>63||minor>63||micro>15)throw Error('CIA versions support major/minor up to 63 and micro up to 15.');
const options={elf:root+'/out/gopenguin-3ds-demo.elf',romfs:root+'/romfs',output:root+'/build/package-candidate',tools:path.resolve(root,'../Tweebbo-3DS/build/ctrtools'),'unique-id':'0x7e901',release:false};
for(let i=2;i<process.argv.length;i++){
 const name=process.argv[i];if(name==='--help'){console.log('node tools/package-native.mjs [--elf FILE] [--romfs DIRECTORY] [--output build/DIRECTORY] [--tools DIRECTORY] [--unique-id HEX] [--release]');process.exit(0);}
 if(name==='--release'){options.release=true;continue;}
 if(!name.startsWith('--')||!(name.slice(2) in options)||!process.argv[i+1])throw Error('Unknown or incomplete option: '+name);
 options[name.slice(2)]=process.argv[++i];
}
// Development packaging keeps the generated banner and the "GoPenguin Dev" titles.
// --release uses the original-art branding in assets/cia-release/ and the public title.
if(options.release&&options['unique-id']==='0x7e901')options['unique-id']='0x7e902';
if(options.release&&Number(options['unique-id'])!==0x7e902)throw Error('The GoPenguin release updater requires its stable Title ID 0004000007E90200.');
const brand=options.release
 ?{short:'GoPenguin',long:'GoPenguin',publisher:'GoPenguin',icon:root+'/assets/cia-release/icon.png',banner:root+'/assets/cia-release/banner.png',wav:root+'/assets/cia-release/banner.wav',base:'gopenguin',status:'release-candidate'}
 :{short:'GoPenguin Dev',long:'Local development candidate',publisher:'GoPenguin',icon:root+'/assets/icon.png',banner:null,wav:null,base:'gopenguin-dev',status:'local-development-candidate'};
for(const key of ['elf','romfs','output','tools'])options[key]=path.resolve(options[key]);
if(!options.output.startsWith(root+'/build/'))throw Error('Candidate output must be a directory below this workspace build/.');
const uniqueId=Number(options['unique-id']);if(!Number.isInteger(uniqueId)||uniqueId<0x1000||uniqueId>0xfffff||[0x7ebb0,0x7ebb1,0x7ebb2].includes(uniqueId))throw Error('Use a development application ID distinct from Tweebbo.');
const titleId=(0x0004000000000000n|BigInt(uniqueId)<<8n).toString(16).padStart(16,'0').toUpperCase();
const makerom=options.tools+'/makerom/makerom',bannertool=options.tools+'/bannertool/linux-x86_64/bannertool';await access(makerom);await access(bannertool);
async function run(command,args){const child=spawn(command,args,{cwd:root,stdio:'inherit'});const code=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('exit',resolve);});if(code!==0)throw Error(command+' failed with '+code);}
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
const started=new Date(),folder=options.output,inputs=folder+'/inputs-'+started.toISOString().replace(/[:.]/g,'-');await mkdir(inputs+'/romfs',{recursive:true});
const elf=await readFile(options.elf);if(elf.toString('ascii',0,4)!=='\x7fELF'||elf[4]!==1||elf.readUInt16LE(18)!==40)throw Error('Expected a 32-bit ARM ELF input.');
if(options.release&&(!elf.includes(Buffer.from('GOPENGUIN_RELEASE:'+version))||elf.includes(Buffer.from('http://10.0.2.2:18891/latest.json'))))throw Error('Release packaging requires the matching versioned public ELF, rebuilt with its updater.');
await writeFile(inputs+'/application.elf',elf);await copyFile(brand.icon,inputs+'/icon.png');
if(brand.banner){await copyFile(brand.banner,inputs+'/banner.png');await copyFile(brand.wav,inputs+'/banner.wav');}
const inventory=[],excluded=[];
async function snapshot(from,to,prefix=''){
 for(const entry of (await readdir(from,{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){
  const source=path.join(from,entry.name),destination=path.join(to,entry.name),name=prefix+entry.name;
  if(entry.name.startsWith('.')){excluded.push(name);continue;}
  if(entry.isDirectory()){await mkdir(destination);await snapshot(source,destination,name+'/');}
  else if(entry.isFile()){const bytes=await readFile(source);await writeFile(destination,bytes);inventory.push({file:name,bytes:bytes.length,sha256:sha(bytes)});}
  else throw Error('Unsupported non-regular RomFS entry: '+source);
 }
}
await snapshot(options.romfs,inputs+'/romfs');
for(const file of inventory)if(sha(await readFile(path.join(options.romfs,file.file)))!==file.sha256)throw Error('RomFS changed while snapshotting: '+file.file);
if(sha(await readFile(options.elf))!==sha(elf))throw Error('ELF changed while snapshotting. Run again after compilation finishes.');
await writeFile(inputs+'/romfs-inventory.json',JSON.stringify(inventory,null,2)+'\n');

const rsf=`BasicInfo:
  Title: "GoPengui"
  ProductCode: "CTR-P-GPPD"
  Logo: Nintendo
RomFs:
  RootPath: $(APP_ROMFS)
TitleInfo:
  Category: Application
  UniqueId: 0x${uniqueId.toString(16)}
Option:
  UseOnSD: true
  FreeProductCode: true
  MediaFootPadding: false
  EnableCrypt: false
  EnableCompress: true
AccessControlInfo:
  CoreVersion: 2
  DescVersion: 2
  ReleaseKernelMajor: "02"
  ReleaseKernelMinor: "33"
  UseExtSaveData: false
  FileSystemAccess:
    - CategoryFileSystemTool
    - DirectSdmc
    - DirectSdmcWrite
  MemoryType: Application
  SystemMode: 64MB
  IdealProcessor: 0
  AffinityMask: 1
  Priority: 16
  MaxCpu: 0x9E
  HandleTableSize: 0x200
  DisableDebug: false
  EnableForceDebug: false
  CanWriteSharedPage: true
  CanUsePrivilegedPriority: false
  CanUseNonAlphabetAndNumber: true
  PermitMainFunctionArgument: true
  CanShareDeviceMemory: true
  RunnableOnSleep: false
  SpecialMemoryArrange: true
  SystemModeExt: Legacy
  CpuSpeed: 804MHz
  EnableL2Cache: true
  CanAccessCore2: false
  IORegisterMapping:
    - 1ff00000-1ff7ffff
  MemoryMapping:
    - 1f000000-1f5fffff:r
  SystemCallAccess:
    InvalidateProcessDataCache: 82
    StoreProcessDataCache: 83
    FlushProcessDataCache: 84
    ControlMemory: 1
    QueryMemory: 2
    ExitProcess: 3
    CreateThread: 8
    ExitThread: 9
    SleepThread: 10
    GetThreadPriority: 11
    SetThreadPriority: 12
    CreateMutex: 19
    ReleaseMutex: 20
    CreateSemaphore: 21
    ReleaseSemaphore: 22
    CreateEvent: 23
    SignalEvent: 24
    ClearEvent: 25
    CreateTimer: 26
    SetTimer: 27
    CancelTimer: 28
    ClearTimer: 29
    CreateMemoryBlock: 30
    MapMemoryBlock: 31
    UnmapMemoryBlock: 32
    CreateAddressArbiter: 33
    ArbitrateAddress: 34
    CloseHandle: 35
    WaitSynchronization1: 36
    WaitSynchronizationN: 37
    DuplicateHandle: 39
    GetSystemTick: 40
    GetHandleInfo: 41
    GetSystemInfo: 42
    GetProcessInfo: 43
    GetThreadInfo: 44
    ConnectToPort: 45
    SendSyncRequest1: 46
    SendSyncRequest2: 47
    SendSyncRequest3: 48
    SendSyncRequest4: 49
    SendSyncRequest: 50
    GetResourceLimit: 56
    GetResourceLimitLimitValues: 57
    GetResourceLimitCurrentValues: 58
    Break: 60
    OutputDebugString: 61
  ServiceAccessControl:
    - APT:U
    - ac:u
    - am:net
    - soc:U
    - ssl:C
    - cfg:u
    - dsp::DSP
    - fs:USER
    - gsp::Gpu
    - hid:USER
    - ptm:u
    - ps:ps
SystemControlInfo:
  SaveDataSize: 0KB
  RemasterVersion: 0
  StackSize: 0x40000
  Dependency:
    ac: 0x0004013000002402
    am: 0x0004013000001502
    socket: 0x0004013000002e02
    ssl: 0x0004013000002f02
    cfg: 0x0004013000001702
    dsp: 0x0004013000001a02
    gsp: 0x0004013000001c02
    hid: 0x0004013000001d02
    ptm: 0x0004013000002202
`;
await writeFile(folder+'/'+brand.base+'.rsf',rsf);
if(!brand.banner)await run('python3',['-c',`from PIL import Image
import wave,sys
folder=sys.argv[1]
icon=Image.open(folder+'/icon.png').convert('RGBA')
banner=Image.new('RGBA',(256,128),(0,0,0,0))
banner.paste(icon.resize((96,96),Image.Resampling.NEAREST),(80,16))
banner.save(folder+'/banner.png')
with wave.open(folder+'/banner.wav','wb') as f:
 f.setnchannels(1);f.setsampwidth(2);f.setframerate(22050);f.writeframes(bytes(4410))
`,inputs]);
const base=folder+'/'+brand.base;
await run(bannertool,['makebanner','-i',inputs+'/banner.png','-a',inputs+'/banner.wav','-o',base+'.bnr']);
await run(bannertool,['makesmdh','-s',brand.short,'-l',brand.long,'-p',brand.publisher,'-i',inputs+'/icon.png','-f','visible,recordusage','-o',base+'.smdh']);
await run(makerom,['-f','cia','-o',base+'.cia','-elf',inputs+'/application.elf','-rsf',folder+'/'+brand.base+'.rsf','-icon',base+'.smdh','-banner',base+'.bnr','-exefslogo','-target','t','-DAPP_ROMFS='+inputs+'/romfs','-ver',String((major<<10)|(minor<<4)|micro)]);
const relative=p=>path.relative(root,p);
await run('podman',['run','--rm','--network','none','--cpus','2','--memory','1g','-v',root+':/work:z','-w','/work','docker.io/devkitpro/devkitarm:latest','3dsxtool',relative(inputs+'/application.elf'),relative(base+'.3dsx'),'--smdh='+relative(base+'.smdh'),'--romfs='+relative(inputs+'/romfs')]);
const manifest={createdAt:new Date().toISOString(),status:brand.status,titleId,uniqueId:'0x'+uniqueId.toString(16),title:brand.short,internalTitle:'GoPengui',productCode:'CTR-P-GPPD',snapshot:relative(inputs),sourceElf:relative(options.elf),elfSha256:sha(elf),romfsBytes:inventory.reduce((sum,f)=>sum+f.bytes,0),romfsFiles:inventory.length,excludedBuildMetadata:excluded,old3ds:{systemMode:'64MB',systemModeExt:'Legacy',idealProcessor:0,affinityMask:1},new3ds:{cpu:'804MHz',l2Cache:true,core2:false},packagingTools:{makerom:sha(await readFile(makerom)),bannertool:sha(await readFile(bannertool))},artifacts:{},limits:['Games and other port work may still be incomplete; packaging is not complete-port acceptance.','DSP playback remains unverified where firmware is unavailable.','No CIA installation, console test, upload, publication or auto-update has been performed.','Title ID is a development convention, not a registered public allocation.']};
manifest.version=version;manifest.titleVersion=(major<<10)|(minor<<4)|micro;
for(const extension of ['cia','3dsx','smdh','bnr']){const file=base+'.'+extension,bytes=await readFile(file);manifest.artifacts[extension]={file:path.basename(file),bytes:bytes.length,sha256:sha(bytes)};}
await writeFile(folder+'/manifest.json',JSON.stringify(manifest,null,2)+'\n');
await run(process.execPath,[root+'/tests/package-native.mjs',folder]);
console.log(`PASS ${brand.status} CIA +3DSX ${titleId}; ${manifest.romfsFiles} RomFS files snapshotted. Nothing installed or published.`);
