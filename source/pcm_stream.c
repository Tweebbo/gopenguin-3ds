#include "pcm_stream.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define PCM_BLOCK 8192u
#define PCM_LIMIT (128u*1024u*1024u)
struct GPPCM {
 FILE *file;GPAudioWav info;
 uint32_t position,blocks,cached;
 bool compressed,failed;
 unsigned char *decoded,*encoded;
};
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static bool compressed_info(GPPCM *s){
 unsigned char header[32];if(fseek(s->file,0,SEEK_END))return false;long size=ftell(s->file);
 if(size<32||(unsigned long)size>PCM_LIMIT+PCM_LIMIT/PCM_BLOCK*8+32||fseek(s->file,0,SEEK_SET)||fread(header,1,32,s->file)!=32)return false;
 uint32_t bytes=u32(header+12),blocks=u32(header+20);
 if(memcmp(header,"GPAZ",4)||u32(header+4)!=1||u32(header+8)!=GP_AUDIO_RATE||!bytes||(bytes&1)||bytes>PCM_LIMIT||u32(header+16)!=PCM_BLOCK||blocks!=(bytes+PCM_BLOCK-1)/PCM_BLOCK||u32(header+24)||u32(header+28))return false;
 uint32_t end=32+blocks*8;if(end>(unsigned long)size)return false;
 for(unsigned i=0;i<blocks;i++){
  unsigned char entry[8];if(fread(entry,1,8,s->file)!=8)return false;uint32_t offset=u32(entry),flags=u32(entry+4)&0xc0000000u,length=u32(entry+4)&0x3fffffffu,raw=bytes-i*PCM_BLOCK;if(raw>PCM_BLOCK)raw=PCM_BLOCK;
  if(offset!=end||flags==0xc0000000u||!length||length>raw||(flags==0x80000000u&&length!=raw)||length>(unsigned long)size-end)return false;
  end+=length;
 }
 if(end!=(unsigned long)size)return false;
 s->info=(GPAudioWav){.bytes=bytes,.rate=GP_AUDIO_RATE};s->blocks=blocks;s->compressed=true;s->cached=UINT32_MAX;
 s->decoded=malloc(PCM_BLOCK);s->encoded=malloc(PCM_BLOCK);return s->decoded&&s->encoded;
}
GPPCM *gp_pcm_open(const char *path,GPAudioWav *info){
 if(!path||!info)return NULL;
 GPPCM *s=calloc(1,sizeof *s);if(!s)return NULL;
 s->file=fopen(path,"rb");
 if(!s->file&&errno==ENOENT){char compressed[384];int n=snprintf(compressed,sizeof compressed,"%s.gpz",path);if(n<0||(size_t)n>=sizeof compressed)goto fail;s->file=fopen(compressed,"rb");}
 if(!s->file)goto fail;
 unsigned char magic[4];if(fread(magic,1,4,s->file)!=4)goto fail;
 if(!memcmp(magic,"GPAZ",4)){if(!compressed_info(s))goto fail;}
 else if(!gp_audio_wav_info(s->file,&s->info))goto fail;
 *info=s->info;return s;
fail:gp_pcm_close(s);return NULL;
}
static bool block(GPPCM *s,unsigned index){
 if(index==s->cached)return true;
 unsigned char entry[8];if(index>=s->blocks||fseek(s->file,32+index*8,SEEK_SET)||fread(entry,1,8,s->file)!=8)return false;
 uint32_t flags=u32(entry+4)&0xc0000000u,length=u32(entry+4)&0x3fffffffu,raw=s->info.bytes-index*PCM_BLOCK;if(raw>PCM_BLOCK)raw=PCM_BLOCK;
 if(flags==0xc0000000u||!length||length>raw||(flags==0x80000000u&&length!=raw)||fseek(s->file,(long)u32(entry),SEEK_SET)||fread(s->encoded,1,length,s->file)!=length)return false;
 if(flags&0x80000000u)memcpy(s->decoded,s->encoded,raw);
 else{uLongf size=raw;if(uncompress(s->decoded,&size,s->encoded,length)!=Z_OK||size!=raw)return false;}
 if(flags==0x40000000u){
  unsigned samples=raw/2,previous=0;
  for(unsigned i=0;i<samples;i++){unsigned value=s->decoded[i]|(unsigned)s->decoded[samples+i]<<8;int delta=(int)(value>>1)^-(int)(value&1);previous=(previous+(unsigned)delta)&65535u;s->encoded[i*2]=(unsigned char)previous;s->encoded[i*2+1]=(unsigned char)(previous>>8);}
  memcpy(s->decoded,s->encoded,raw);
 }
 s->cached=index;return true;
}
size_t gp_pcm_read(GPPCM *s,int16_t *samples,size_t count){
 if(!s||!samples||s->failed||count>SIZE_MAX/2)return 0;
 size_t bytes=count*2,left=s->info.bytes-s->position;if(bytes>left)bytes=left;
 if(!s->compressed){size_t read=fread(samples,1,bytes,s->file);s->position+=(uint32_t)read;if(read!=bytes)s->failed=true;return read/2;}
 size_t read=0;while(read<bytes){unsigned index=s->position/PCM_BLOCK,offset=s->position%PCM_BLOCK;if(!block(s,index)){s->failed=true;break;}size_t n=PCM_BLOCK-offset;if(n>bytes-read)n=bytes-read;memcpy((unsigned char *)samples+read,s->decoded+offset,n);read+=n;s->position+=(uint32_t)n;}
 return read/2;
}
bool gp_pcm_rewind(GPPCM *s){if(!s||s->failed)return false;if(!s->compressed&&fseek(s->file,(long)s->info.offset,SEEK_SET))return false;s->position=0;return true;}
void gp_pcm_close(GPPCM *s){if(!s)return;if(s->file)fclose(s->file);free(s->decoded);free(s->encoded);free(s);}
