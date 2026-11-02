#include "package.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 GPPackage package;if(!package_open(&package,argv[1]))return 1;
 for(unsigned i=0;i<package.count;i++){
  size_t size=0;void *data=package_read(&package,i,24u*1024u*1024u,&size);if(!data)return 1;
  char file[1024];snprintf(file,sizeof file,"%s/%u.bin",argv[2],i);FILE *out=fopen(file,"wb");if(!out){free(data);return 1;}bool ok=fwrite(data,1,size,out)==size&&fclose(out)==0;free(data);if(!ok)return 1;
 }
 return 0;
}
