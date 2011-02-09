/* image.h */

#ifndef IMAGE_H
#  define IMAGE_H

typedef struct _TK_RGBImageRec
{
    int     sizeX, sizeY, sizeZ;
    unsigned char *data;
} TK_RGBImageRec;

extern TK_RGBImageRec *tkRGBImageLoad(const char *fileName);

#endif
