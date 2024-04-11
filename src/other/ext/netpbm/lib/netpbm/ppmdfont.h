#ifndef PPMDFONT_INCLUDED
#define PPMDFONT_INCLUDED

#include <stdio.h>

/* A font file has the following format, with proper packing:

    struct ppmd_fontHeader fontHeader;
    struct {
        struct ppmd_glyphHeader glyphHeader;
        struct ppmd_glyphCommand glyphCommand[N];
    } glyph[M]
    
    Where:
        M is fontHeader.characterCount
        N is glyphHeader.commandCount   

    glyph[i] is the glyph for code point Q,
      where i = Q - fontHeader.firstCodePoint

*/

struct ppmd_fontHeader {
    char signature[8];             /* "ppmdfont" */
    unsigned char format;          /* 0x01 */
    unsigned char characterCount;
        /* Number of characters in this font */
    unsigned char firstCodePoint;
        /* lowest code point in the font */
};

struct ppmd_glyphHeader {
    unsigned char commandCount;
        /* Number of struct glyphCommand that follow */
    unsigned char skipBefore;
    unsigned char skipAfter;
};

enum ppmd_glyphCommandVerb {CMD_NOOP     = 0,
                            CMD_DRAWLINE = 1,
                            CMD_MOVEPEN  = 2
};

struct ppmd_glyphCommand {
    enum ppmd_glyphCommandVerb verb;
    unsigned char x;
    unsigned char y;
};

struct ppmd_glyph {
    struct ppmd_glyphHeader header;
    const struct ppmd_glyphCommand * commandList;
};

struct ppmd_font {
    struct ppmd_fontHeader header;
    const struct ppmd_glyph * glyphTable;
};

void
ppmd_set_font(const struct ppmd_font * const newFontP);

const struct ppmd_font *
ppmd_get_font(void);

void
ppmd_read_font(FILE *                    const ifP,
               const struct ppmd_font ** const fontPP);

void
ppmd_free_font(const struct ppmd_font * const fontP);


#endif
