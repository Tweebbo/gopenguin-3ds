#pragma once
#include <citro2d.h>
typedef void *Tex3DS_Texture;
Tex3DS_Texture Tex3DS_TextureImport(const void *,size_t,C3D_Tex *,void *,bool);
unsigned Tex3DS_GetNumSubTextures(Tex3DS_Texture);
const Tex3DS_SubTexture *Tex3DS_GetSubTexture(Tex3DS_Texture,unsigned);
bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture *);
void Tex3DS_TextureFree(Tex3DS_Texture);
