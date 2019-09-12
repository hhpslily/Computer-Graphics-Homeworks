#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

GLuint TextureNumber;

#pragma pack(push)
#pragma pack(2)
typedef struct _FileHeader
{
	unsigned char ID[2];
	unsigned long FileSizepr;
	unsigned long Reserved;
	unsigned long DataOffset;
} FileHeader;

typedef struct _InfoHeader
{
	unsigned long HeaderSize;
	unsigned long Width;
	unsigned long Height;
	unsigned short Planes;
	unsigned short BitsPerPixel;
	unsigned long Compression;
	unsigned long BmpDataSize;
	unsigned long HResolusion;
	unsigned long VResolusion;
	unsigned long UsedColor;
	unsigned long ImportantColors;
} InfoHeader;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // #define TEXTURE_H
//////////////////////////////////////////////////////////////////////////
//Jenny Liu . April, 2013