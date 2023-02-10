/* pbmfont.h - header file for font routines in libpbm
*/

#include "pbm.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

struct glyph {
    /* A glyph consists of white borders and the "central glyph" which
       can be anything, but normally does not have white borders because
       it's more efficient to represent white borders explicitly.
    */
    unsigned int width;
    unsigned int height;
        /* The dimensions of the central glyph, i.e. the 'bmap' array */
    int x;
        /* Width in pixels of the white left border of this glyph.
           This can actually be negative to indicate that the central
           glyph backs up over the previous character in the line.  In
           that case, if there is no previous character in the line, it
           is as if 'x' is 0.
        */
    int y;
        /* Height in pixels of the white bottom border of this glyph.
           Can be negative.
        */
    unsigned int xadd;
        /* Width of glyph -- white left border plus central glyph
           plus white right border
        */
    const char * bmap;
        /* The raster of the central glyph.  It is an 'width' by
           'height' array in row major order, with each byte being 1
           for black; 0 for white.  E.g. if 'width' is 20 pixels and
           'height' is 40 pixels and it's a rectangle that is black on
           the top half and white on the bottom, this is an array of
           800 bytes, with the first 400 having value 0x01 and the
           last 400 having value 0x00.
        */
};

struct font {
    /* This describes a combination of font and character set.  Given
       an code point in the range 0..255, this structure describes the
       glyph for that character.
    */
    int maxwidth, maxheight;
    int x;
        /* ?? Not used by Pbmtext */
    int y;
        /* Amount of white space that should be added between lines of
           this font.  Can be negative.
        */
    struct glyph * glyph[256];
        /* glyph[i] is the glyph for code point i */
    const bit ** oldfont;
        /* for compatibility with old pbmtext routines */
        /* oldfont is NULL if the font is BDF derived */
    int fcols, frows;
};

struct font* pbm_defaultfont(const char* const which);
struct font*
pbm_dissectfont(const bit ** const font,
                unsigned int const frows,
                unsigned int const fcols);
struct font* pbm_loadfont(const char * const filename);
struct font* pbm_loadpbmfont(const char * const filename);
struct font* pbm_loadbdffont(const char * const filename);
void pbm_dumpfont(struct font * const fnP);

#ifdef __cplusplus
}
#endif
