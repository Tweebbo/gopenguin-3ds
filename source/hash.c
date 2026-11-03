#include "hash.h"
#include <string.h>
#include <stdlib.h>
#ifdef __3DS__
#include <mbedtls/sha256.h>
#else
#include <openssl/evp.h>
#endif
bool hash_file_progress(FILE *file,const unsigned char expected[32],bool (*cancel)(void *),void (*progress)(unsigned,void *),void *user){
 unsigned char fallback[8192],digest[32],*allocated=malloc(131072),*buffer=allocated?allocated:fallback;size_t capacity=allocated?131072:sizeof fallback,n;bool stopped=false;unsigned done=0;
#ifdef __3DS__
 mbedtls_sha256_context context;mbedtls_sha256_init(&context);mbedtls_sha256_starts(&context,0);
 while((n=fread(buffer,1,capacity,file))){if(cancel&&cancel(user)){stopped=true;break;}mbedtls_sha256_update(&context,buffer,n);done+=(unsigned)n;if(progress)progress(done,user);}
 mbedtls_sha256_finish(&context,digest);mbedtls_sha256_free(&context);
#else
 EVP_MD_CTX *context=EVP_MD_CTX_new();if(!context){free(allocated);return false;}EVP_DigestInit_ex(context,EVP_sha256(),NULL);
 while((n=fread(buffer,1,capacity,file))){if(cancel&&cancel(user)){stopped=true;break;}EVP_DigestUpdate(context,buffer,n);done+=(unsigned)n;if(progress)progress(done,user);}
 EVP_DigestFinal_ex(context,digest,NULL);EVP_MD_CTX_free(context);
#endif
 free(allocated);return !stopped&&!ferror(file)&&!memcmp(digest,expected,32);
}
bool hash_file_cancel(FILE *file,const unsigned char expected[32],bool (*cancel)(void *),void *user){return hash_file_progress(file,expected,cancel,NULL,user);}
bool hash_file(FILE *file,const unsigned char expected[32]){return hash_file_cancel(file,expected,NULL,NULL);}
