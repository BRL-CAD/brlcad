/*
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#ifndef __APPLE__
#  define _BSD_SOURCE 1      /* Make sure strdup() is in string.h */
#  define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pm_c_util.h"
#include "mallocvar.h"

#include "ppm.h"
#include "colorname.h"


static void
computeHexTable(int hexit[]) {

    unsigned int i;

    for ( i = 0; i < 256; ++i )
        hexit[i] = -1;

    hexit['0'] = 0;
    hexit['1'] = 1;
    hexit['2'] = 2;
    hexit['3'] = 3;
    hexit['4'] = 4;
    hexit['5'] = 5;
    hexit['6'] = 6;
    hexit['7'] = 7;
    hexit['8'] = 8;
    hexit['9'] = 9;
    hexit['a'] = hexit['A'] = 10;
    hexit['b'] = hexit['B'] = 11;
    hexit['c'] = hexit['C'] = 12;
    hexit['d'] = hexit['D'] = 13;
    hexit['e'] = hexit['E'] = 14;
    hexit['f'] = hexit['F'] = 15;
}



static long
invRgbnorm(pixval       const rgb,
           pixval       const maxval,
           unsigned int const hexDigits) {
/*----------------------------------------------------------------------------
  This is the inverse of 'rgbnorm', below.
-----------------------------------------------------------------------------*/
    long retval;

    switch (hexDigits) {
    case 1:
        retval = (long)((double) rgb * 15 / maxval + 0.5);
        break;
    case 2:
        retval = (long) ((double) rgb * 255 / maxval + 0.5);
        break;
    case 3:
        retval = (long) ((double) rgb * 4095 / maxval + 0.5);
        break;
    case 4:
        retval = (long) ((double) rgb * 65535UL / maxval + 0.5);
        break;
    default:
        pm_message("Internal error in invRgbnorm()");
        abort();
    }
    return retval;
}



static pixval
rgbnorm(long         const rgb, 
        pixval       const maxval, 
        unsigned int const hexDigitCount, 
        bool         const closeOk,
        const char * const colorname) {
/*----------------------------------------------------------------------------
   Normalize the color (r, g, or b) value 'rgb', which was specified
   with 'hexDigitCount' digits, to a maxval of 'maxval'.  If the
   number of digits isn't valid, issue an error message and identify
   the complete color color specification in error as 'colorname'.

   For example, if the user says "0ff" and the maxval is 100,
   then rgb is 0xff, n is 3, and our result is 
   0xff / (16**3-1) * 100 = 6.
-----------------------------------------------------------------------------*/
    pixval retval;

    switch (hexDigitCount) {
    case 0:
        pm_error("A hexadecimal color specifier in color '%s' is "
                 "an empty string", colorname);
        break;
    case 1:
        retval = (pixval)((double) rgb * maxval / 15 + 0.5);
        break;
    case 2:
        retval = (pixval) ((double) rgb * maxval / 255 + 0.5);
        break;
    case 3:
        retval = (pixval) ((double) rgb * maxval / 4095 + 0.5);
        break;
    case 4:
        retval = (pixval) ((double) rgb * maxval / 65535L + 0.5);
        break;
    default:
        pm_error("color specifier '%s' has too many digits", colorname);
    }

    if (!closeOk) {
        long const newrgb = invRgbnorm(retval, maxval, hexDigitCount);
        if (newrgb != rgb)
            pm_message("WARNING: Component 0x%lx of color '%s' "
                       "cannot be represented precisely with maxval %u.  "
                       "Approximating as %u.",
                       rgb, colorname, maxval, retval);
    }
    return retval;
}



static void
parseHexDigits(const char *   const string,
               char           const delim,
               int            const hexit[],
               pixval *       const nP,
               unsigned int * const digitCountP) {

    unsigned int digitCount;
    pixval n;
    
    digitCount = 0;  /* initial value */
    n = 0;           /* initial value */
    while (string[digitCount] != delim) {
        char const digit = string[digitCount];
        if (digit == '\0')
            pm_error("rgb: color spec ends prematurely");
        else {
            int const hexval = hexit[(unsigned int)digit];
            if (hexval == -1)
                pm_error("Invalid hex digit in rgb: color spec: 0x%02x",
                         digit);
            n = n * 16 + hexval;
            ++digitCount;
        }
    }
    *nP = n;
    *digitCountP = digitCount;
}



static void
parseNewHexX11(char       const colorname[], 
               pixval     const maxval,
               bool       const closeOk,
               pixel *    const colorP) {
/*----------------------------------------------------------------------------
   Determine what color colorname[] specifies in the new style hex
   color specification format (e.g. rgb:55/40/55).

   Return that color as *colorP.

   Assume colorname[] starts with "rgb:", but otherwise it might be
   gibberish.
-----------------------------------------------------------------------------*/
    int hexit[256];

    const char * cp;
    pixval n;
    unsigned int digitCount;
    pixval rNorm, gNorm, bNorm;

    computeHexTable(hexit);

    cp = &colorname[4];

    parseHexDigits(cp, '/', hexit, &n, &digitCount);

    rNorm = rgbnorm(n, maxval, digitCount, closeOk, colorname);

    cp += digitCount;
    ++cp;  /* Skip the slash */

    parseHexDigits(cp, '/', hexit, &n, &digitCount);

    gNorm = rgbnorm(n, maxval, digitCount, closeOk, colorname);

    cp += digitCount;
    ++cp;  /* Skip the slash */

    parseHexDigits(cp, '\0', hexit, &n, &digitCount);

    bNorm = rgbnorm(n, maxval, digitCount, closeOk, colorname);

    PPM_ASSIGN(*colorP, rNorm, gNorm, bNorm);
}



static void
parseNewDecX11(char       const colorname[], 
               pixval     const maxval,
               bool       const closeOk,
               pixel *    const colorP) {

    float const epsilon = 1.0/65536.0;
    float fr, fg, fb;
    pixval rNorm, gNorm, bNorm;

    if (sscanf( colorname, "rgbi:%f/%f/%f", &fr, &fg, &fb) != 3)
        pm_error("invalid color specifier '%s'", colorname);
    if (fr < 0.0 || fr > 1.0 || fg < 0.0 || fg > 1.0 
        || fb < 0.0 || fb > 1.0)
        pm_error("invalid color specifier '%s' - "
                 "values must be between 0.0 and 1.0", colorname );

    rNorm = fr * maxval + 0.5;
    gNorm = fg * maxval + 0.5;
    bNorm = fb * maxval + 0.5;

    if (!closeOk) {
        if (fabs((double)rNorm/maxval - fr) > epsilon ||
            fabs((double)gNorm/maxval - fg) > epsilon ||
            fabs((double)bNorm/maxval - fb) > epsilon)
            pm_message("WARNING: Color '%s' cannot be represented "
                       "precisely with maxval %u.  "
                       "Approximating as (%u,%u,%u).",
                       colorname, maxval, rNorm, gNorm, bNorm);
    }
    PPM_ASSIGN(*colorP, rNorm, gNorm, bNorm);
}



static bool
isHexString(char const string[],
            int  const hexit[]) {

    bool retval;
    const char * p;

    for (p = &string[0], retval = true; *p && retval == true; ++p) {
        if (hexit[(unsigned int)*p] == -1)
            retval = false;
    }
    return retval;
}



static void
parseOldX11(char       const colorname[], 
            pixval     const maxval,
            bool       const closeOk,
            pixel *    const colorP) {
/*----------------------------------------------------------------------------
   Return as *colorP the color specified by the old X11 style color
   specififier colorname[] (e.g. #554055).
-----------------------------------------------------------------------------*/
    int hexit[256];
    long r,g,b;
    pixval rNorm, gNorm, bNorm;
    
    computeHexTable(hexit);

    if (!isHexString(&colorname[1], hexit))
        pm_error("Non-hexadecimal characters in #-type color specification");

    switch (strlen(colorname) - 1 /* (Number of hex digits) */) {
    case 3:
        r = hexit[(int)colorname[1]];
        g = hexit[(int)colorname[2]];
        b = hexit[(int)colorname[3]];
        rNorm = rgbnorm(r, maxval, 1, closeOk, colorname);
        gNorm = rgbnorm(g, maxval, 1, closeOk, colorname);
        bNorm = rgbnorm(b, maxval, 1, closeOk, colorname);
        break;

    case 6:
        r = (hexit[(int)colorname[1]] << 4 ) + hexit[(int)colorname[2]];
        g = (hexit[(int)colorname[3]] << 4 ) + hexit[(int)colorname[4]];
        b = (hexit[(int)colorname[5]] << 4 ) + hexit[(int)colorname[6]];
        rNorm = rgbnorm(r, maxval, 2, closeOk, colorname);
        gNorm = rgbnorm(g, maxval, 2, closeOk, colorname);
        bNorm = rgbnorm(b, maxval, 2, closeOk, colorname);
        break;

    case 9:
        r = (hexit[(int)colorname[1]] << 8) +
            (hexit[(int)colorname[2]] << 4) +
            (hexit[(int)colorname[3]] << 0);
        g = (hexit[(int)colorname[4]] << 8) + 
            (hexit[(int)colorname[5]] << 4) +
            (hexit[(int)colorname[6]] << 0);
        b = (hexit[(int)colorname[7]] << 8) + 
            (hexit[(int)colorname[8]] << 4) +
            (hexit[(int)colorname[9]] << 0);
        rNorm = rgbnorm(r, maxval, 3, closeOk, colorname);
        gNorm = rgbnorm(g, maxval, 3, closeOk, colorname);
        bNorm = rgbnorm(b, maxval, 3, closeOk, colorname);
        break;

    case 12:
        r = (hexit[(int)colorname[1]] << 12) + 
            (hexit[(int)colorname[2]] <<  8) +
            (hexit[(int)colorname[3]] <<  4) + hexit[(int)colorname[4]];
        g = (hexit[(int)colorname[5]] << 12) + 
            (hexit[(int)colorname[6]] <<  8) +
            (hexit[(int)colorname[7]] <<  4) + hexit[(int)colorname[8]];
        b = (hexit[(int)colorname[9]] << 12) + 
            (hexit[(int)colorname[10]] << 8) +
            (hexit[(int)colorname[11]] << 4) + hexit[(int)colorname[12]];
        rNorm = rgbnorm(r, maxval, 4, closeOk, colorname);
        gNorm = rgbnorm(g, maxval, 4, closeOk, colorname);
        bNorm = rgbnorm(b, maxval, 4, closeOk, colorname);
        break;

    default:
        pm_error("invalid color specifier '%s'", colorname);
    }
    PPM_ASSIGN(*colorP, rNorm, gNorm, bNorm);
}




static void
parseOldX11Dec(const char       colorname[], 
               pixval     const maxval,
               bool       const closeOk,
               pixel *    const colorP) {

    float const epsilon = 1.0/65536.0;

    float fr, fg, fb;
    pixval rNorm, gNorm, bNorm;

    if (sscanf(colorname, "%f,%f,%f", &fr, &fg, &fb) != 3)
        pm_error("invalid color specifier '%s'", colorname);
    if (fr < 0.0 || fr > 1.0 || fg < 0.0 || fg > 1.0 
        || fb < 0.0 || fb > 1.0)
        pm_error("invalid color specifier '%s' - "
                 "values must be between 0.0 and 1.0", colorname );

    rNorm = fr * maxval + 0.5;
    gNorm = fg * maxval + 0.5;
    bNorm = fb * maxval + 0.5;

    if (!closeOk) {
        if (fabs((float)rNorm/maxval - fr) > epsilon ||
            fabs((float)gNorm/maxval - fg) > epsilon ||
            fabs((float)bNorm/maxval - fb) > epsilon)
            pm_message("WARNING: Color '%s' cannot be represented "
                       "precisely with maxval %u.  "
                       "Approximating as (%u,%u,%u).",
                       colorname, maxval, rNorm, gNorm, bNorm);
    }
    PPM_ASSIGN(*colorP, rNorm, gNorm, bNorm);
}



pixel
ppm_parsecolor2(const char * const colorname,
                pixval       const maxval,
                int          const closeOk) {

    pixel color;
    
    if (strncmp(colorname, "rgb:", 4) == 0)
        /* It's a new-X11-style hexadecimal rgb specifier. */
        parseNewHexX11(colorname, maxval, closeOk, &color);
    else if (strncmp(colorname, "rgbi:", 5) == 0)
        /* It's a new-X11-style decimal/float rgb specifier. */
        parseNewDecX11(colorname, maxval, closeOk, &color);
    else if (colorname[0] == '#')
        /* It's an old-X11-style hexadecimal rgb specifier. */
        parseOldX11(colorname, maxval, closeOk, &color);
    else if ((colorname[0] >= '0' && colorname[0] <= '9') ||
             colorname[0] == '.')
        /* It's an old-style decimal/float rgb specifier. */
        parseOldX11Dec(colorname, maxval, closeOk, &color);
    else 
        /* Must be a name from the X-style rgb file. */
        pm_parse_dictionary_name(colorname, maxval, closeOk, &color);
    
    return color;
}



pixel
ppm_parsecolor(const char * const colorname, 
               pixval       const maxval) {

    return ppm_parsecolor2(colorname, maxval, TRUE);
}



char *
ppm_colorname(const pixel * const colorP, 
              pixval        const maxval, 
              int           const hexok)   {

    int r, g, b;
    FILE * f;
    static char colorname[200];

    if (maxval == 255) {
        r = PPM_GETR(*colorP);
        g = PPM_GETG(*colorP);
        b = PPM_GETB(*colorP);
    } else {
        r = (int) PPM_GETR(*colorP) * 255 / (int) maxval;
        g = (int) PPM_GETG(*colorP) * 255 / (int) maxval;
        b = (int) PPM_GETB(*colorP) * 255 / (int) maxval;
    }

    f = pm_openColornameFile(NULL, !hexok);
    if (f != NULL) {
        int best_diff, this_diff;
        bool eof;

        best_diff = 32767;
        eof = FALSE;
        while (!eof && best_diff > 0 ) {
            struct colorfile_entry const ce = pm_colorget(f);
            if (ce.colorname)  {
                this_diff = abs(r - ce.r) + abs(g - ce.g) + abs(b - ce.b);
                if (this_diff < best_diff) {
                    best_diff = this_diff;
                    strcpy(colorname, ce.colorname);
                }
            } else
                eof = TRUE;
        }
        fclose(f);
        if (best_diff != 32767 && (best_diff == 0 || ! hexok))
            return colorname;
    }

    /* Color lookup failed, but caller is willing to take an X11-style
       hex specifier, so return that.
    */
    sprintf(colorname, "#%02x%02x%02x", r, g, b);
    return colorname;}



#define MAXCOLORNAMES 1000u

static const char **
allocColorNames() {

    const char ** colornames;

    MALLOCARRAY(colornames, MAXCOLORNAMES);

    if (colornames) {
        unsigned int i;
        for (i = 0; i < MAXCOLORNAMES; ++i)
            colornames[i] = NULL;
    }
    return colornames;
}



static colorhash_table
allocColorHash(void) {

    colorhash_table cht;
    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;

    if (setjmp(jmpbuf) != 0)
        cht = NULL;
    else {
        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);
        cht = ppm_alloccolorhash();
    }
    pm_setjmpbuf(origJmpbufP);

    return cht;
}



static void
processColorfileEntry(struct colorfile_entry const ce,
                      colorhash_table        const cht,
                      const char **          const colornames,
                      pixel *                const colors,
                      unsigned int *         const colornameIndexP,
                      const char **          const errorP) {

    if (*colornameIndexP >= MAXCOLORNAMES)
        pm_error("Too many colors in colorname dictionary.  "
                 "Max allowed is %u", MAXCOLORNAMES);
    else {
        pixel color;

        PPM_ASSIGN(color, ce.r, ce.g, ce.b);

        if (ppm_lookupcolor(cht, &color) >= 0) {
            /* The color is already in the hash, which means we saw it
               earlier in the file.  We prefer the first name that the
               file gives for each color, so we just ignore the
               current entry.  
            */
            *errorP = NULL;
        } else {
            ppm_addtocolorhash(cht, &color, *colornameIndexP);
            colornames[*colornameIndexP] = strdup(ce.colorname);
            colors[*colornameIndexP] = color;
            if (colornames[*colornameIndexP] == NULL)
                pm_error("Unable to allocate space for color name");
            else {
                *errorP = NULL;
                ++(*colornameIndexP);
            }
        }
    }
}



static void
openColornameFile(const char *  const fileName,
                  bool          const mustOpen,
                  FILE **       const filePP,
                  const char ** const errorP) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;

    if (setjmp(jmpbuf) != 0) {
        pm_error( "Failed to open color name file");
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        *filePP = pm_openColornameFile(fileName, mustOpen);

        *errorP = NULL;  /* Would have longjmped if there were a problem */

        pm_setjmpbuf(origJmpbufP);
    }
}



static void
readOpenColorFile(FILE *          const colorFileP,
                  unsigned int *  const nColorsP,
                  const char **   const colornames,
                  pixel *         const colors,
                  colorhash_table const cht,
                  const char **   const errorP) {
/*----------------------------------------------------------------------------
   Read the color dictionary file *colorFileP and add the colors in it
   to colornames[], colors[], and 'cht'.

   colornames[] and colors[] must be allocated with MAXCOLORNAMES entries
   at entry.

   We may add colors to 'cht' even if we fail.
-----------------------------------------------------------------------------*/
    unsigned int nColorsDone;
    bool done;

    nColorsDone = 0;
    done = FALSE;
    *errorP = NULL;

    while (!done && !*errorP) {
        struct colorfile_entry const ce = pm_colorget(colorFileP);
        
        if (!ce.colorname)  
            done = TRUE;
        else 
            processColorfileEntry(ce, cht, colornames, colors,
                                  &nColorsDone, errorP);
    }
    *nColorsP = nColorsDone;
    
    if (*errorP) {
        unsigned int colorIndex;

        for (colorIndex = 0; colorIndex < nColorsDone; ++colorIndex)
            free((void *)colornames[colorIndex]);
    }
}



static void
readColorFile(const char *    const fileName,
              bool            const mustOpen,
              unsigned int *  const nColorsP,
              const char **   const colornames,
              pixel *         const colors,
              colorhash_table const cht,
              const char **   const errorP) {
/*----------------------------------------------------------------------------
   Read the color dictionary file named 'fileName' and add the colors in it
   to colornames[], colors[], and 'cht'.  Return as *nColorsP the number
   of colors in it.

   If the file is not openable (e.g. not file by that name exists), abort the
   program if 'mustOpen' is true; otherwise, return values indicating a
   dictionary with no colors.

   colornames[] and colors[] must be allocated with MAXCOLORNAMES entries
   at entry.

   We may add colors to 'cht' even if we fail.
-----------------------------------------------------------------------------*/
    FILE * colorFileP;

    openColornameFile(fileName, mustOpen, &colorFileP, errorP);
    if (!*errorP) {
        if (colorFileP == NULL) {
            /* Couldn't open it, but Caller says treat same as
               empty file
            */
            *nColorsP = 0;
            *errorP = NULL;
        } else {
            readOpenColorFile(colorFileP, nColorsP, colornames, colors, cht,
                              errorP);
            
            fclose(colorFileP);
        }
    }
}

    

static void
readcolordict(const char *      const fileName,
              bool              const mustOpen,
              unsigned int *    const nColorsP,
              const char ***    const colornamesP,
              pixel **          const colorsP,
              colorhash_table * const chtP,
              const char **     const errorP) {

    const char ** colornames;

    colornames = allocColorNames();

    if (colornames == NULL)
        pm_error( "Unable to allocate space for colorname table.");
    else {
        pixel * colors;

        MALLOCARRAY(colors, MAXCOLORNAMES);
        
        if (colors == NULL)
            pm_error( "Unable to allocate space for color table.");
        else {
            colorhash_table cht;

            cht = allocColorHash();
            
            if (cht == NULL)
                pm_error( "Unable to allocate space for color hash");
            else {
                readColorFile(fileName, mustOpen,
                              nColorsP, colornames, colors, cht,
                              errorP);

                if (*errorP)
                    ppm_freecolorhash(cht);
                else
                    *chtP = cht;
            }
            if (*errorP)
                free(colors);
            else
                *colorsP = colors;
        }
        if (*errorP)
            free(colornames);
        else
            *colornamesP = colornames;
    }
}



void
ppm_readcolordict(const char *      const fileName,
                  int               const mustOpen,
                  unsigned int *    const nColorsP,
                  const char ***    const colornamesP,
                  pixel **          const colorsP,
                  colorhash_table * const chtP) {
/*----------------------------------------------------------------------------
   Read the color dictionary from the file named 'fileName'.  If we can't open
   the file (e.g. because it does not exist), and 'mustOpen' is false, return
   an empty dictionary (it contains no colors).  But if 'mustOpen' is true,
   abort the program instead of returning an empty dictionary.

   Return as *nColorsP the number of colors in the dictionary.

   Return as *colornamesP the names of those colors.  *colornamesP is a
   malloced array that Caller must free with ppm_freecolornames().
   The first *nColorsP entries are valid; *chtP contains indices into this
   array.

   Return as *colorsP the colors.  *colorsP is a malloced array of size
   MAXCOLORS with the first elements filled in and the rest undefined.

   Return as *chtP a color hash table mapping each color in the dictionary
   to the index into *colornamesP for the name of the color.

   Each of 'nColorsP, 'colornamesP', and 'colorsP' may be null, in which case
   we do not return the corresponding information (or allocate memory for it).
-----------------------------------------------------------------------------*/
    colorhash_table cht;
    const char ** colornames;
    pixel * colors;
    unsigned int nColors;
    const char * error;

    readcolordict(fileName, mustOpen, &nColors, &colornames, &colors, &cht,
                  &error);

    if (error) {
        pm_error("%s", error);
        free((void *)error);
        ppm_freecolorhash(cht);
    } else {
        if (chtP)
            *chtP = cht;
        else
            ppm_freecolorhash(cht);
        if (colornamesP)
            *colornamesP = colornames;
        else
            ppm_freecolornames(colornames);
        if (colorsP)
            *colorsP = colors;
        else
            ppm_freerow(colors);
        if (nColorsP)
            *nColorsP = nColors;
    }
}



void
ppm_readcolornamefile(const char *      const fileName, 
                      int               const mustOpen,
                      colorhash_table * const chtP, 
                      const char ***    const colornamesP) {

    ppm_readcolordict(fileName, mustOpen, NULL, colornamesP, NULL, chtP);
}



void
ppm_freecolornames(const char ** const colornames) {

    unsigned int i;

    for (i = 0; i < MAXCOLORNAMES; ++i)
        if (colornames[i])
            free((char *)colornames[i]);

    free(colornames);
}



static unsigned int 
nonnegative(unsigned int const arg) {

    if ((int)(arg) < 0)
        return 0;
    else
        return arg;
}



pixel
ppm_color_from_ycbcr(unsigned int const y, 
                     int          const cb, 
                     int          const cr) {
/*----------------------------------------------------------------------------
   Return the color that has luminance 'y', blue chrominance 'cb', and
   red chrominance 'cr'.

   The 3 input values can be on any scale (as long as it's the same
   scale for all 3) and the maxval of the returned pixel value is the
   same as that for the input values.

   Rounding may cause an output value to be greater than the maxval.
-----------------------------------------------------------------------------*/
    pixel retval;

    PPM_ASSIGN(retval, 
               y + 1.4022 * cr,
               nonnegative(y - 0.7145 * cr - 0.3456 * cb),
               y + 1.7710 * cb
        );
    
    return retval;
}



pixel
ppm_color_from_hsv(struct hsv const hsv,
                   pixval     const maxval) {

    pixel retval;
    double R, G, B;

    if (hsv.s == 0) {
        R = hsv.v;
        G = hsv.v;
        B = hsv.v;
    } else {
        unsigned int const sectorSize = 60;
            /* Color wheel is divided into six 60 degree sectors. */
        unsigned int const sector = (hsv.h/sectorSize);
            /* The sector in which our color resides.  Value is in 0..5 */
        double const f = (hsv.h - sector*sectorSize)/60;
            /* The fraction of the way the color is from one side of
               our sector to the other side, going clockwise.  Value is
               in [0, 1).
            */
        double const m = (hsv.v * (1 - hsv.s));
        double const n = (hsv.v * (1 - (hsv.s * f)));
        double const k = (hsv.v * (1 - (hsv.s * (1 - f))));

        switch (sector) {
        case 0:
            R = hsv.v;
            G = k;
            B = m;
            break;
        case 1:
            R = n;
            G = hsv.v;
            B = m;
            break;
        case 2:
            R = m;
            G = hsv.v;
            B = k;
            break;
        case 3:
            R = m;
            G = n;
            B = hsv.v;
            break;
        case 4:
            R = k;
            G = m;
            B = hsv.v;
            break;
        case 5:
            R = hsv.v;
            G = m;
            B = n;
            break;
        default:
            pm_error("Invalid H value passed to color_from_HSV: %f", hsv.h);
        }
    }
    PPM_ASSIGN(retval, 
               ROUNDU(R * maxval),
               ROUNDU(G * maxval),
               ROUNDU(B * maxval));

    return retval;
}



struct hsv
ppm_hsv_from_color(pixel  const color,
                   pixval const maxval) {

    double const epsilon = 1e-5;

    double const R = (double)PPM_GETR(color) / maxval;
    double const G = (double)PPM_GETG(color) / maxval;
    double const B = (double)PPM_GETB(color) / maxval;

    enum hueSector {SECTOR_RED, SECTOR_GRN, SECTOR_BLU};
    enum hueSector hueSector;

    struct hsv retval;
    double range;

    if (R >= G) {
        if (R >= B) {
            hueSector = SECTOR_RED;
            retval.v = R;
        } else {
            hueSector = SECTOR_BLU;
            retval.v = B;
        }
    } else {
        if (G >= B) {
            hueSector = SECTOR_GRN;
            retval.v = G;
        } else {
            hueSector = SECTOR_BLU;
            retval.v = B;
        }
    }

    range = retval.v - MIN(R, MIN(G, B));

    if (retval.v < epsilon)
        retval.s = 0.0;
    else
        retval.s = range/retval.v;

    if (range < epsilon)
        /* It's gray, so hue really has no meaning.  We arbitrarily pick 0 */
        retval.h = 0.0;
    else {
        double const cr = (retval.v - R) / range;
        double const cg = (retval.v - G) / range;
        double const cb = (retval.v - B) / range;

        double angle;  /* hue angle, in range -30 - +330 */

        switch(hueSector) {
        case SECTOR_RED: angle =   0.0 + 60.0 * (cb - cg); break;
        case SECTOR_GRN: angle = 120.0 + 60.0 * (cr - cb); break;
        case SECTOR_BLU: angle = 240.0 + 60.0 * (cg - cr); break;
        }
        retval.h = angle >= 0.0 ? angle : 360 + angle;
    }

    return retval;
}


