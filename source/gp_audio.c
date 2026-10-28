#include "gp_audio.h"
#include "pcm_stream.h"
#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __3DS__
#include <3ds.h>
#endif

#define MUSIC_BUFFERS 3
#define MUSIC_SAMPLES 4096
#define VOICES 6
#define METADATA_BYTES (256u*1024u)
#define WAV_BYTES (128u*1024u*1024u)

static uint16_t le16(const unsigned char *p){return (uint16_t)(p[0]|p[1]<<8);}
static uint32_t le32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
bool gp_audio_wav_info(FILE *file,GPAudioWav *out){
 if(!file||!out||fseek(file,0,SEEK_END))return false;
 long size=ftell(file);if(size<44||(unsigned long)size>WAV_BYTES||fseek(file,0,SEEK_SET))return false;
 unsigned char header[16];if(fread(header,1,12,file)!=12||memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4))return false;
 uint64_t limit=(uint64_t)le32(header+4)+8;if(limit>(unsigned long)size||limit<44)return false;
 bool format=false,data=false;GPAudioWav result={0};uint64_t at=12;
 while(at+8<=limit){
  if(fseek(file,(long)at,SEEK_SET)||fread(header,1,8,file)!=8)return false;
  uint32_t bytes=le32(header+4);uint64_t next=at+8+bytes+(bytes&1u);if(next>limit)return false;
  if(!memcmp(header,"fmt ",4)){
   if(format||bytes<16||fread(header,1,16,file)!=16||le16(header)!=1||le16(header+2)!=1||le32(header+4)!=GP_AUDIO_RATE||le32(header+8)!=GP_AUDIO_RATE*2||le16(header+12)!=2||le16(header+14)!=16)return false;
   format=true;result.rate=GP_AUDIO_RATE;
  }else if(!memcmp(header,"data",4)){
   if(data||!bytes||(bytes&1u))return false;
   result.offset=(uint32_t)(at+8);result.bytes=bytes;data=true;
  }
  at=next;
 }
 if(!format||!data||at!=limit||fseek(file,(long)result.offset,SEEK_SET))return false;
 *out=result;return true;
}

static struct {
 char directory[192],bank[128];bool enabled,paused,ready,musicFailed;
 unsigned music,step;json_t *programs,*rooms,*games,*igloo,*names,*files;GPPCM *file;uint32_t remaining;
#ifdef __3DS__
 int16_t *musicData;ndspWaveBuf musicBuffers[MUSIC_BUFFERS];
 struct {int16_t *data;ndspWaveBuf buffers[MUSIC_BUFFERS];GPPCM *file;GPAudioWav wav;unsigned id,remaining,samples;uint32_t unread;bool active,forever;} voices[VOICES];
 unsigned nextVoice;
#endif
} A;

static bool component(const char *text){
 if(!text||!*text||strstr(text,".."))return false;
 for(const char *p=text;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'||*p=='/'))return false;
 return text[0]!='/'&&text[strlen(text)-1]!='/';
}
static json_t *metadata(const char *name,bool array){
 char path[256];if(snprintf(path,sizeof path,"%s/%s",A.directory,name)>=(int)sizeof path)return NULL;
 FILE *file=fopen(path,"rb");if(!file)return NULL;
 if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}long bytes=ftell(file);rewind(file);
 json_t *value=bytes>=2&&(unsigned long)bytes<=METADATA_BYTES?json_loadf(file,JSON_REJECT_DUPLICATES,NULL):NULL;fclose(file);
 bool valid=!value||(array?json_is_array(value):json_is_object(value));
 if(array&&value&&valid){
  size_t count=json_array_size(value);valid=count<=128;
  for(size_t i=0;i<count&&valid;i++){
   json_t *row=json_array_get(value,i),*id=json_object_get(row,"id");json_int_t number=json_integer_value(id);const char *label=json_string_value(json_object_get(row,"name"));
   valid=json_is_integer(id)&&number>0&&number<=1000000&&label&&*label&&strlen(label)<128;
   for(size_t j=0;j<i&&valid;j++)if(json_integer_value(json_object_get(json_array_get(value,j),"id"))==number)valid=false;
  }
 }
 if(!valid){json_decref(value);value=NULL;}return value;
}
static json_t *named_metadata(void){
 json_t *names=metadata("named-sounds.json",false);bool valid=json_object_size(names)<=128;size_t aliases=0;const char *bank,*name;json_t *table,*id;
 json_object_foreach(names,bank,table){
  if(!component(bank)||strlen(bank)>=sizeof A.bank||!json_is_object(table)){valid=false;break;}
  json_object_foreach(table,name,id){
   json_int_t number=json_integer_value(id);if(!*name||strlen(name)>=128||!json_is_integer(id)||number<1||number>1000000||++aliases>2048){valid=false;break;}
  }
  if(!valid)break;
 }
 if(!valid){json_decref(names);names=NULL;}return names;
}
static bool audio_relative(const char *value){
 if(!component(value)||strlen(value)>=192||strstr(value,"//"))return false;
 size_t length=strlen(value);return length>4&&!strcmp(value+length-4,".wav")&&(!strncmp(value,"music/",6)||!strncmp(value,"sfx/",4));
}
static json_t *file_metadata(void){
 json_t *files=metadata("files.json",false);bool valid=json_object_size(files)<=2048;const char *logical;json_t *value;
 json_object_foreach(files,logical,value){const char *canonical=json_string_value(value);if(!audio_relative(logical)||!audio_relative(canonical)||!strcmp(logical,canonical)||json_object_get(files,canonical)){valid=false;break;}}
 if(!valid){json_decref(files);files=NULL;}return files;
}
#ifdef __3DS__
static bool audio_path(const char *logical,char *path,size_t cap){
 const char *canonical=json_string_value(json_object_get(A.files,logical));if(!canonical)canonical=logical;
 if(!audio_relative(canonical))return false;
 int written=snprintf(path,cap,"%s/%s",A.directory,canonical);return written>=0&&(size_t)written<cap;
}
#endif

/* The first random clip and the path restart follow the source music SWF. */
#if defined(__3DS__) || defined(GP_AUDIO_TEST)
static const char *pick(json_t *program,unsigned step,unsigned entropy){
 json_t *clips=json_object_get(program,"clips");size_t count=json_array_size(clips);if(!count||count>256)return NULL;
 const char *order=json_string_value(json_object_get(program,"order"));unsigned index=0;
 if(order&&!strcmp(order,"random"))index=step?entropy%(unsigned)count:0;
 else if(order&&!strcmp(order,"sequence"))index=step%(unsigned)count;
 else if(order&&!strcmp(order,"path")){
  json_t *path=json_object_get(program,"path");size_t length=json_array_size(path);json_int_t restart=json_integer_value(json_object_get(program,"restart"));if(!length||length>4096||restart<0||(size_t)restart>=length)return NULL;
  size_t position=step<length?step:(size_t)restart+(step-length)%(length-(size_t)restart);
  json_t *value=json_array_get(path,position);if(!json_is_integer(value))return NULL;json_int_t clip=json_integer_value(value);if(clip<0||(size_t)clip>=count)return NULL;index=(unsigned)clip;
 }
 const char *clip=json_string_value(json_array_get(clips,index));return clip&&component(clip)&&!strchr(clip,'/')?clip:NULL;
}
#endif

static void close_music(void){if(A.file)gp_pcm_close(A.file);A.file=NULL;A.remaining=0;}
#ifdef __3DS__
static void clear_voice(unsigned voice){
 ndspChnWaveBufClear((int)voice+1);memset(A.voices[voice].buffers,0,sizeof A.voices[voice].buffers);A.voices[voice].active=false;A.voices[voice].remaining=0;
 A.voices[voice].forever=false;
 if(A.voices[voice].file)gp_pcm_close(A.voices[voice].file);
 A.voices[voice].file=NULL;
}
static void stop_effects(void){if(A.ready)for(unsigned i=0;i<VOICES;i++)clear_voice(i);}
static void clear_music(void){if(A.ready)ndspChnWaveBufClear(0);memset(A.musicBuffers,0,sizeof A.musicBuffers);close_music();}
static void apply_pause(void){if(A.ready)for(int i=0;i<=VOICES;i++)ndspChnSetPaused(i,!A.enabled||A.paused);}
static void voice_volume(unsigned voice,float volume){float mix[12]={volume,volume};ndspChnSetMix((int)voice+1,mix);}
static bool start_dsp(void){
 if(A.ready)return true;
 if(R_FAILED(ndspInit()))return false;
 A.musicData=linearAlloc(MUSIC_BUFFERS*MUSIC_SAMPLES*sizeof(int16_t));
 for(unsigned i=0;i<VOICES;i++)A.voices[i].data=linearAlloc(GP_AUDIO_SFX_BYTES);
 bool allocated=A.musicData!=NULL;for(unsigned i=0;i<VOICES;i++)allocated=allocated&&A.voices[i].data!=NULL;
 if(!allocated){linearFree(A.musicData);A.musicData=NULL;for(unsigned i=0;i<VOICES;i++){linearFree(A.voices[i].data);A.voices[i].data=NULL;}ndspExit();return false;}
 A.ready=true;ndspSetOutputMode(NDSP_OUTPUT_STEREO);
 for(int i=0;i<=VOICES;i++){ndspChnReset(i);ndspChnSetInterp(i,NDSP_INTERP_LINEAR);ndspChnSetRate(i,GP_AUDIO_RATE);ndspChnSetFormat(i,NDSP_FORMAT_MONO_PCM16);float mix[12]={.65f,.65f};ndspChnSetMix(i,mix);}
 apply_pause();return true;
}
static bool next_music(void){
 close_music();char id[16];snprintf(id,sizeof id,"%u",A.music);json_t *program=json_object_get(A.programs,id);
 const char *clip=pick(program,A.step++,(unsigned)rand());if(!clip)return false;
 char logical[192],path[352];if(snprintf(logical,sizeof logical,"music/%u/%s",A.music,clip)>=(int)sizeof logical||!audio_path(logical,path,sizeof path))return false;
 GPAudioWav wav;GPPCM *file=gp_pcm_open(path,&wav);if(!file)return false;
 A.file=file;A.remaining=wav.bytes;return true;
}
static bool fill_music(unsigned index){
 int16_t *data=A.musicData+index*MUSIC_SAMPLES;size_t filled=0;
 while(filled<MUSIC_SAMPLES){
  if(!A.remaining&&!next_music()){A.musicFailed=true;break;}
  size_t count=A.remaining/2;if(count>MUSIC_SAMPLES-filled)count=MUSIC_SAMPLES-filled;
  size_t got=gp_pcm_read(A.file,data+filled,count);A.remaining-=(uint32_t)(got*2);filled+=got;
  if(got!=count){close_music();A.musicFailed=true;break;}
 }
 if(!filled)return false;
 ndspWaveBuf *buffer=&A.musicBuffers[index];memset(buffer,0,sizeof *buffer);buffer->data_vaddr=data;buffer->nsamples=(u32)filled;DSP_FlushDataCache(data,filled*sizeof(int16_t));ndspChnWaveBufAdd(0,buffer);return true;
}
static bool queue_voice(unsigned voice,unsigned index){
 int16_t *data=A.voices[voice].data;size_t filled=A.voices[voice].samples;
 if(A.voices[voice].file){
  data+=index*MUSIC_SAMPLES;filled=0;
  while(filled<MUSIC_SAMPLES&&A.voices[voice].remaining){
   if(!A.voices[voice].unread){
    if(!A.voices[voice].forever)A.voices[voice].remaining--;
    if(!A.voices[voice].remaining)break;
    if(!gp_pcm_rewind(A.voices[voice].file)){A.voices[voice].remaining=0;break;}A.voices[voice].unread=A.voices[voice].wav.bytes;
   }
   size_t count=A.voices[voice].unread/2;if(count>MUSIC_SAMPLES-filled)count=MUSIC_SAMPLES-filled;
   size_t got=gp_pcm_read(A.voices[voice].file,data+filled,count);A.voices[voice].unread-=(uint32_t)(got*2);filled+=got;
   if(got!=count){A.voices[voice].remaining=0;break;}
  }
  if(!filled)return false;
  DSP_FlushDataCache(data,filled*sizeof(int16_t));
 }else{if(!A.voices[voice].remaining)return false;if(!A.voices[voice].forever)A.voices[voice].remaining--;}
 ndspWaveBuf *buffer=&A.voices[voice].buffers[index];memset(buffer,0,sizeof *buffer);buffer->data_vaddr=data;buffer->nsamples=(u32)filled;ndspChnWaveBufAdd((int)voice+1,buffer);return true;
}
#else
static void stop_effects(void){}
static void clear_music(void){close_music();}
#endif

bool gp_audio_init(const char *directory,bool enabled){
 gp_audio_shutdown();if(!directory||!*directory||strlen(directory)>=sizeof A.directory)return false;
 snprintf(A.directory,sizeof A.directory,"%s",directory);A.enabled=enabled;
 A.programs=metadata("music.json",false);A.rooms=metadata("room-music.json",false);A.games=metadata("game-music.json",false);A.igloo=metadata("igloo-music.json",true);
 A.names=named_metadata();A.files=file_metadata();
#ifdef __3DS__
 return !enabled||start_dsp();
#else
 return !enabled;
#endif
}
bool gp_audio_available(void){return A.ready;}
bool gp_audio_enabled(void){return A.enabled;}
void gp_audio_set_enabled(bool enabled){
 A.enabled=enabled;
#ifdef __3DS__
 if(enabled&&!A.ready&&start_dsp()&&A.music){unsigned id=A.music;A.music=0;gp_audio_music(id);}apply_pause();
#endif
}
void gp_audio_pause(bool paused){A.paused=paused;
#ifdef __3DS__
 apply_pause();
#endif
}
void gp_audio_music(unsigned id){
 if(id==A.music)return;
 clear_music();A.music=id;A.step=0;A.musicFailed=false;
#ifdef __3DS__
 if(!id||!A.ready)return;
 char key[16];snprintf(key,sizeof key,"%u",id);json_t *program=json_object_get(A.programs,key);double volume=json_number_value(json_object_get(program,"volume"));if(volume<0)volume=0;if(volume>1)volume=1;float mix[12]={(float)volume,(float)volume};ndspChnSetMix(0,mix);
 for(unsigned i=0;i<MUSIC_BUFFERS;i++)if(!fill_music(i))break;
#endif
}
static unsigned music_id(json_t *table,const char *key){json_t *id=key?json_object_get(table,key):NULL;json_int_t number=json_integer_value(id);return number>0&&number<=1000000?(unsigned)number:0;}
void gp_audio_room(const char *key){gp_audio_music(music_id(A.rooms,key));}
void gp_audio_game(const char *key){gp_audio_music(music_id(A.games,key));}
unsigned gp_audio_igloo_tracks(void){return (unsigned)json_array_size(A.igloo);}
bool gp_audio_igloo_track(unsigned index,unsigned *id,const char **name){
 json_t *track=json_array_get(A.igloo,index);if(!track)return false;
 if(id)*id=(unsigned)json_integer_value(json_object_get(track,"id"));
 if(name)*name=json_string_value(json_object_get(track,"name"));
 return true;
}
void gp_audio_scene(const char *bank){
 if(!bank||!component(bank)||strlen(bank)>=sizeof A.bank)bank="";
 if(!strcmp(bank,A.bank))return;
 stop_effects();snprintf(A.bank,sizeof A.bank,"%s",bank);
}
static bool play_effect(unsigned id,unsigned flags,unsigned loops,float volume,bool forever){
#ifdef __3DS__
 if(!A.ready||!id)return false;
 bool stopped=false;
 for(unsigned i=0;i<VOICES;i++)if(A.voices[i].active&&A.voices[i].id==id){if(flags&1){clear_voice(i);stopped=true;}else if(flags&2){if(forever){A.voices[i].forever=true;if(!A.voices[i].remaining)A.voices[i].remaining=1;voice_volume(i,volume);}return true;}}
 if(flags&1)return stopped;
 if(!A.enabled||A.paused||!A.bank[0])return false;
 char logical[192],filename[352];if(snprintf(logical,sizeof logical,"sfx/%s/%u.wav",A.bank,id)>=(int)sizeof logical||!audio_path(logical,filename,sizeof filename))return false;
 GPAudioWav wav;GPPCM *file=gp_pcm_open(filename,&wav);if(!file)return false;
 unsigned voice=A.nextVoice++%VOICES;for(unsigned i=0;i<VOICES;i++)if(!A.voices[i].active){voice=i;break;}clear_voice(voice);
 if(wav.bytes>GP_AUDIO_SFX_BYTES){A.voices[voice].file=file;A.voices[voice].wav=wav;A.voices[voice].unread=wav.bytes;}
 else{bool read=gp_pcm_read(file,A.voices[voice].data,wav.bytes/2)==wav.bytes/2;gp_pcm_close(file);if(!read)return false;DSP_FlushDataCache(A.voices[voice].data,wav.bytes);A.voices[voice].samples=wav.bytes/2;}
 A.voices[voice].id=id;A.voices[voice].remaining=loops?loops:1;if(A.voices[voice].remaining>65535)A.voices[voice].remaining=65535;A.voices[voice].active=true;A.voices[voice].forever=forever;voice_volume(voice,volume);
 for(unsigned i=0;i<MUSIC_BUFFERS;i++)if(!queue_voice(voice,i))break;
 return true;
#else
 (void)id;(void)flags;(void)loops;(void)volume;(void)forever;return false;
#endif
}
void gp_audio_sound(void *user,const char *path,unsigned id,unsigned flags,unsigned loops){(void)user;(void)path;(void)play_effect(id,flags,loops,.65f,false);}
bool gp_audio_named_sound(const char *bank,const char *name,int volume,bool loop){
 if(!bank||!component(bank)||strlen(bank)>=sizeof A.bank||!name||!*name||strlen(name)>=128)return false;
 json_t *id=json_object_get(json_object_get(A.names,bank),name);if(!json_is_integer(id))return false;
 if(!A.enabled||A.paused||!A.ready)return false;
 if(volume<0)volume=0;
 if(volume>100)volume=100;
 gp_audio_scene(bank);return play_effect((unsigned)json_integer_value(id),loop?2:0,1,volume/100.0f,loop);
}
void gp_audio_update(void){
#ifdef __3DS__
 if(!A.ready||!A.enabled||A.paused)return;
 if(A.music&&!A.musicFailed)for(unsigned i=0;i<MUSIC_BUFFERS;i++)if(A.musicBuffers[i].status==NDSP_WBUF_DONE||A.musicBuffers[i].status==NDSP_WBUF_FREE)if(!fill_music(i))break;
 for(unsigned v=0;v<VOICES;v++)if(A.voices[v].active){
  bool queued=false;for(unsigned i=0;i<MUSIC_BUFFERS;i++){unsigned status=A.voices[v].buffers[i].status;if(status==NDSP_WBUF_DONE||status==NDSP_WBUF_FREE){if(queue_voice(v,i))queued=true;}else queued=true;}
  if(!queued)clear_voice(v);
 }
#endif
}
void gp_audio_stop(void){clear_music();stop_effects();A.music=0;A.step=0;}
void gp_audio_shutdown(void){
 gp_audio_stop();
#ifdef __3DS__
 if(A.ready)ndspExit();
 linearFree(A.musicData);for(unsigned i=0;i<VOICES;i++)linearFree(A.voices[i].data);
#endif
 json_decref(A.programs);json_decref(A.rooms);json_decref(A.games);json_decref(A.igloo);json_decref(A.names);json_decref(A.files);memset(&A,0,sizeof A);
}
