#include "package.h"
#include "pcm_stream.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* The GPZ fixture never enters the WAV branch. */
bool gp_audio_wav_info(FILE *file,GPAudioWav *out){(void)file;(void)out;return false;}

int main(int argc,char **argv){
 assert(argc==4);
 if(!strcmp(argv[1],"movie")){
  GPPackage a,b;assert(package_open(&a,argv[2]));assert(package_open(&b,argv[3]));assert(a.count==b.count);
  for(unsigned i=0;i<a.count;i++){
   size_t na=0,nb=0;void *pa=package_read(&a,i,GP_PACKAGE_MAX,&na),*pb=package_read(&b,i,GP_PACKAGE_MAX,&nb);
   assert(pa&&pb&&na==nb&&!memcmp(pa,pb,na));free(pa);free(pb);
  }
 }else{
  GPAudioWav ia,ib;GPPCM *a=gp_pcm_open(argv[2],&ia),*b=gp_pcm_open(argv[3],&ib);assert(a&&b&&ia.bytes==ib.bytes&&ia.rate==ib.rate);
  int16_t va[4096],vb[4096];size_t na,nb,total=0;
  do{na=gp_pcm_read(a,va,4096);nb=gp_pcm_read(b,vb,4096);assert(na==nb&&!memcmp(va,vb,na*sizeof *va));total+=na;}while(na);
  assert(total*2==ia.bytes);assert(gp_pcm_rewind(a)&&gp_pcm_rewind(b));
  na=gp_pcm_read(a,va,4096);nb=gp_pcm_read(b,vb,4096);assert(na==nb&&!memcmp(va,vb,na*sizeof *va));gp_pcm_close(a);gp_pcm_close(b);
 }
 return 0;
}
