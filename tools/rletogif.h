/* rletogif.h */



/*
 * Pointer to function returning an int
 */
typedef int (* ifunptr)();

typedef unsigned char gif_pixel;

int compgif(int code_size, FILE *os, ifunptr infun);
void error(char *s);
int getpixel(int x, int y);
int GIFNextPixel(ifunptr getpixel);

extern const char *MY_NAME;
