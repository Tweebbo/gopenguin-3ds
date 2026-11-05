#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/movie_bank.c"

/* The libctru seam checks the payload address and length; texture import remains
   a device test. A compressed sentinel expands to known pixels here. */
ssize_t decompressHeader(decompressType *type,size_t *size,decompressCallback cb,void *input,size_t bytes){
 assert(!cb);const unsigned char *p=input;
 if(bytes<4||(p[0]!=0&&p[0]!=0x10))return -1;
 *type=(decompressType)p[0];*size=p[1]|(size_t)p[2]<<8|(size_t)p[3]<<16;return 4;
}
bool decompressV(const decompressIOVec *out,size_t count,decompressCallback cb,void *input,size_t bytes){
 assert(count==1&&!cb);const unsigned char *p=input;
 if(bytes!=5||p[0]!=0x10||p[4]!=0xab)return false;
 memset(out->data,0xab,out->size);return true;
}
int main(void){
 unsigned char texture[29]={1,0,0,0,0};unsigned wx=0,hy=0;
 while((8u<<wx)<SWL_ATLAS_WIDTH)wx++;
 while((8u<<hy)<SWL_ATLAS_HEIGHT)hy++;
 texture[2]=(unsigned char)(wx|(hy<<3));
 texture[17]=DECOMPRESS_DUMMY;texture[18]=8;
 for(unsigned i=0;i<8;i++)texture[21+i]=(unsigned char)(i+1);
 size_t bytes=0;unsigned char *raw=inflate_page(texture,sizeof texture,8,&bytes);
 assert(raw&&bytes==sizeof texture&&!memcmp(raw,texture,sizeof texture));free(raw);
 assert(!inflate_page(texture,sizeof texture,7,&bytes));
 assert(!inflate_page(texture,20,8,&bytes));
 assert(!inflate_page(texture,28,8,&bytes));
 texture[0]=2;assert(!inflate_page(texture,sizeof texture,8,&bytes));texture[0]=1;
 texture[2]^=1;assert(!inflate_page(texture,sizeof texture,8,&bytes));texture[2]^=1;
 texture[17]=0x10;texture[21]=0xab;
 raw=inflate_page(texture,22,8,&bytes);
 assert(raw&&bytes==29&&!memcmp(raw,texture,17)&&raw[17]==DECOMPRESS_DUMMY&&raw[18]==8);
 for(unsigned i=21;i<bytes;i++)assert(raw[i]==0xab);
 free(raw);
 texture[21]=0;assert(!inflate_page(texture,22,8,&bytes));
 puts("PASS staged movie textures: preserve Tex3DS metadata, decode at payload offset, retain dummy header, enforce size/budget, reject truncated and invalid pages.");
}
