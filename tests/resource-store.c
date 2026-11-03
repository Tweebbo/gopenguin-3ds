#include "resource_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc<3)return 2;
 ResourceIndex *index=resource_index_load(argv[2]);if(!index)return 1;
 int result=0;ResourceStats stats={0};
 if(!strcmp(argv[1],"index"))printf("%u %llu\n",resource_index_count(index),(unsigned long long)resource_index_bytes(index));
 else if(!strcmp(argv[1],"reuse")&&(argc==5||argc==6)){
  if(argc==6&&!resource_store_init(argv[5],argv[3]))return 2;
  if(!resource_store_reuse(index,argv[3],argv[4],NULL,&stats))result=1;
  printf("{\"ready\":%s,\"copied\":%llu,\"existing\":%llu,\"files\":%u}\n",resource_store_ready(index,argv[3])?"true":"false",(unsigned long long)stats.copied,(unsigned long long)stats.existing,stats.files);
 }else if(!strcmp(argv[1],"bundle")&&argc==7){
  if(!resource_store_bundle(index,argv[3],argv[4],argv[5],(unsigned)strtoul(argv[6],NULL,10),NULL,&stats))result=1;
  printf("{\"ready\":%s,\"written\":%llu,\"files\":%u}\n",resource_store_ready(index,argv[3])?"true":"false",(unsigned long long)stats.written,stats.files);
 }else if(!strcmp(argv[1],"open")&&argc==6){
  if(!resource_store_init(argv[2],argv[3]))return 2;
  FILE *input=resource_store_open(argv[4],"rb"),*output=fopen(argv[5],"wb");if(!input||!output)return 1;
  unsigned char buffer[4096];size_t n;while((n=fread(buffer,1,sizeof buffer,input)))if(fwrite(buffer,1,n,output)!=n)return 1;
  if(ferror(input)||fclose(input)||fclose(output))result=1;
 }else result=2;
 resource_store_exit();resource_index_free(index);return result;
}
