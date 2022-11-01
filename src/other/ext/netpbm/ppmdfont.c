#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "mallocvar.h"
#include "pm.h"
#include "ppmdfont.h"


extern struct ppmd_font ppmd_standardfont;

static const struct ppmd_font * currentFontP = &ppmd_standardfont;

static void
readGlyphHeader(FILE *                    const ifP,
                struct ppmd_glyphHeader * const glyphHeaderP) {

    glyphHeaderP->commandCount = fgetc(ifP);
    glyphHeaderP->skipBefore   = fgetc(ifP);
    glyphHeaderP->skipAfter    = fgetc(ifP);
}



static void
readGlyphCommand(FILE *                     const ifP,
                 struct ppmd_glyphCommand * const glyphCommandP) {

    glyphCommandP->verb = fgetc(ifP);
    glyphCommandP->x    = fgetc(ifP);
    glyphCommandP->y    = fgetc(ifP);
}



static void
readCharacter(FILE *              const ifP,
              struct ppmd_glyph * const glyphP) {

    unsigned int commandNum;
    struct ppmd_glyphCommand * commandList;

    readGlyphHeader(ifP, &glyphP->header);

    MALLOCARRAY(commandList, glyphP->header.commandCount);

    if (commandList == NULL)
        pm_error("Insufficient memory to create a %u-command "
                 "command list.", glyphP->header.commandCount);

    for (commandNum = 0;
         commandNum < glyphP->header.commandCount;
         ++commandNum) {

        readGlyphCommand(ifP, &commandList[commandNum]);
    }
    glyphP->commandList = commandList;
}



static void
readFontHeader(FILE *                   const ifP,
               struct ppmd_fontHeader * const fontHeaderP) {
    
    size_t rc;

    rc = fread(&fontHeaderP->signature, 1, sizeof(fontHeaderP->signature),
               ifP);

    if (rc != sizeof(fontHeaderP->signature))
        pm_error("Unable to read the header from the font file.  "
                 "errno=%d (%s)", errno, strerror(errno));

    fontHeaderP->format         = fgetc(ifP);
    fontHeaderP->characterCount = fgetc(ifP);
    fontHeaderP->firstCodePoint = fgetc(ifP);
}



void
ppmd_set_font(const struct ppmd_font * const newFontP) {

    currentFontP = newFontP;
}



const struct ppmd_font *
ppmd_get_font(void) {

    return currentFontP;
}



void
ppmd_read_font(FILE *                        const ifP,
               const struct ppmd_font **     const fontPP) {

    unsigned int relativeCodePoint;
    struct ppmd_glyph * glyphTable;
    struct ppmd_font * fontP;

    MALLOCVAR(fontP);
    if (fontP == NULL)
        pm_error("Insufficient memory for font header");

    readFontHeader(ifP, &fontP->header);

    MALLOCARRAY(glyphTable, fontP->header.characterCount);
    if (glyphTable == NULL)
        pm_error("Insufficient memory to store %u characters",
                 fontP->header.characterCount);

    for (relativeCodePoint = 0;
         relativeCodePoint < fontP->header.characterCount;
         ++relativeCodePoint) {

        readCharacter(ifP, &glyphTable[relativeCodePoint]);
    }
    fontP->glyphTable = glyphTable;
    *fontPP = fontP;
}



void
ppmd_free_font(const struct ppmd_font * const fontP) {

    unsigned int relativeCodePoint;

    for (relativeCodePoint = 0;
         relativeCodePoint < fontP->header.characterCount;
         ++relativeCodePoint) {
        
        free((void*)fontP->glyphTable[relativeCodePoint].commandList);
    }
    free((void*)fontP->glyphTable);
    free((void*)fontP);
}
