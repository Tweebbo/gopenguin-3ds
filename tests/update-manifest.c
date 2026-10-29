#include "update_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc>=3&&!strcmp(argv[1],"manifest")){
  FILE *f=fopen(argv[2],"rb");if(!f)return 2;char data[20000];size_t n=fread(data,1,sizeof data,f);fclose(f);GPUpdateManifest manifest;bool valid=gp_update_manifest(data,n,&manifest);printf("%s\n",valid?"VALID":"INVALID");return 0;
 }
 if((argc==6||argc==7)&&!strcmp(argv[1],"file")){GPUpdateArtifact a={.bytes=(unsigned)strtoul(argv[4],NULL,10)};snprintf(a.sha,sizeof a.sha,"%s",argv[3]);if(argc==7){unsigned major,minor,micro;if(!gp_version_valid(argv[6])||sscanf(argv[6],"%u.%u.%u",&major,&minor,&micro)!=3)return 2;a.titleVersion=(major<<10)|(minor<<4)|micro;a.checkTitleVersion=true;}puts(gp_update_file(argv[2],&a,!strcmp(argv[5],"3dsx"))?"VALID":"INVALID");return 0;}
 if(argc==4&&!strcmp(argv[1],"compare")){printf("%d\n",gp_version_compare(argv[2],argv[3]));return 0;}
 return 2;
}
