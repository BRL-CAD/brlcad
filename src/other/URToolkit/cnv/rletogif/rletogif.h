/* rle2gif.h */



/*
 * Pointer to function returning an int
 */
typedef int (* ifunptr)();

typedef unsigned char gif_pixel;

int compgif();
void error();
int getpixel();
int GIFNextPixel();

extern CONST_DECL char *MY_NAME;
