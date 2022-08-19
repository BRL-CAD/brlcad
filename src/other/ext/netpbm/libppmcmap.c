/* libppm3.c - ppm utility library part 3
**
** Colormap routines.
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "pm_config.h"
#include "pm_c_util.h"

#include "mallocvar.h"
#include "ppm.h"
#include "ppmcmap.h"

#define HASH_SIZE 20023



static __inline__ unsigned int
ppm_hashpixel(pixel const p) {

    return (unsigned int) (PPM_GETR(p) * 33 * 33
                           + PPM_GETG(p) * 33
                           + PPM_GETB(p)) % HASH_SIZE;
}



colorhist_vector
ppm_computecolorhist( pixel ** const pixels, 
                      const int cols, const int rows, const int maxcolors, 
                      int * const colorsP ) {
/*----------------------------------------------------------------------------
   Compute a color histogram for the image described by 'pixels',
   'cols', and 'rows'.  I.e. a colorhist_vector containing an entry
   for each color in the image and for each one the number of pixels
   of that color (i.e. a color histogram).

   If 'maxcolors' is zero, make the output have 5 spare slots at the end
   for expansion.
   
   If 'maxcolors' is nonzero, make the output have 'maxcolors' slots in
   it, and if there are more colors than that in the image, don't return
   anything except a NULL pointer.
-----------------------------------------------------------------------------*/
    colorhash_table cht;
    colorhist_vector chv;

    cht = ppm_computecolorhash(pixels, cols, rows, maxcolors, colorsP);
    if (cht == NULL)
        chv = NULL;
    else {
        chv = ppm_colorhashtocolorhist(cht, maxcolors);
        ppm_freecolorhash(cht);
    }
    return chv;
}



colorhist_vector
ppm_computecolorhist2(FILE * const ifp,
                      const int cols, const int rows, 
                      const pixval maxval, const int format, 
                      const int maxcolors, int * const colorsP ) {

    colorhash_table cht;
    colorhist_vector chv;

    cht = ppm_computecolorhash2(ifp, cols, rows, maxval, format, 
                                maxcolors, colorsP);
    if (cht ==NULL)
        return NULL;
    chv = ppm_colorhashtocolorhist(cht, maxcolors);
    ppm_freecolorhash(cht);
    return chv;
}



void
ppm_addtocolorhist( colorhist_vector chv, 
                    int * const colorsP, const int maxcolors, 
                    const pixel * const colorP, 
                    const int value, const int position ) {
    int i, j;

    /* Search colorhist for the color. */
    for ( i = 0; i < *colorsP; ++i )
        if ( PPM_EQUAL( chv[i].color, *colorP ) ) {
            /* Found it - move to new slot. */
            if ( position > i ) {
                for ( j = i; j < position; ++j )
                    chv[j] = chv[j + 1];
            } else if ( position < i ) {
                for ( j = i; j > position; --j )
                    chv[j] = chv[j - 1];
            }
            chv[position].color = *colorP;
            chv[position].value = value;
            return;
        }
    if ( *colorsP < maxcolors ) {
        /* Didn't find it, but there's room to add it; so do so. */
        for ( i = *colorsP; i > position; --i )
            chv[i] = chv[i - 1];
        chv[position].color = *colorP;
        chv[position].value = value;
        ++(*colorsP);
    }
}



static colorhash_table
alloccolorhash(void)  {
    colorhash_table cht;
    int i;

    MALLOCARRAY(cht, HASH_SIZE);
    if (cht) {
        for (i = 0; i < HASH_SIZE; ++i)
            cht[i] = NULL;
    }
    return cht;
}



colorhash_table
ppm_alloccolorhash(void)  {
    colorhash_table cht;

    cht = alloccolorhash();

    if (cht == NULL)
        pm_error( "out of memory allocating hash table" );

    return cht;
}



static void
readppmrow(FILE *        const fileP, 
           pixel *       const pixelrow, 
           int           const cols, 
           pixval        const maxval, 
           int           const format,
           const char ** const errorP) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    
    if (setjmp(jmpbuf) != 0) {
        pm_setjmpbuf(origJmpbufP);
        pm_error( "Failed to read row of image.");
    } else {
        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        ppm_readppmrow(fileP, pixelrow, cols, maxval, format);

        *errorP = NULL; /* Would have longjmped if anything went wrong */
                
        pm_setjmpbuf(origJmpbufP);
    }
}



static void
buildHashTable(FILE *          const ifP,
               pixel **        const pixels,
               unsigned int    const cols,
               unsigned int    const rows,
               pixval          const maxval,
               int             const format,
               unsigned int    const maxcolors,
               colorhash_table const cht,
               pixel *         const rowbuffer,
               int *           const nColorsP,
               bool *          const tooManyColorsP,
               const char **   const errorP) {
/*----------------------------------------------------------------------------
  Look at all the colors in the file *ifP or array pixels[][] and add
  them to the hash table 'cht'.

  Even if we fail, we may add some colors to 'cht'.

  As soon as we've seen more that 'maxcolors' colors, we quit.  In that
  case, only, we return *tooManyColorsP == true.  That is not a failure.
  'maxcolors' == 0 means infinity.
-----------------------------------------------------------------------------*/
    unsigned int row;
    unsigned int nColors;

    nColors = 0;   /* initial value */
    *tooManyColorsP = FALSE; /* initial value */
    *errorP = NULL;  /* initial value */

    /* Go through the entire image, building a hash table of colors. */
    for (row = 0; row < rows && !*tooManyColorsP && !*errorP; ++row) {
        unsigned int col;
        pixel * pixelrow;  /* The row of pixels we are processing */

        if (ifP) {
            readppmrow(ifP, rowbuffer, cols, maxval, format, errorP);
            pixelrow = rowbuffer;
        } else 
            pixelrow = pixels[row];

        for (col = 0; col < cols && !*tooManyColorsP && !*errorP; ++col) {
            const pixel apixel = pixelrow[col];
            const int hash = ppm_hashpixel(apixel);
            colorhist_list chl; 

            for (chl = cht[hash]; 
                 chl && !PPM_EQUAL(chl->ch.color, apixel);
                 chl = chl->next);

            if (chl)
                ++chl->ch.value;
            else {
                /* It's not in the hash yet, so add it (if allowed) */
                ++nColors;
                if (maxcolors > 0 && nColors > maxcolors)
                    *tooManyColorsP = TRUE;
                else {
                    MALLOCVAR(chl);
                    if (chl == NULL)
                        pm_error(
                                    "out of memory computing hash table");
                    chl->ch.color = apixel;
                    chl->ch.value = 1;
                    chl->next = cht[hash];
                    cht[hash] = chl;
                }
            }
        }
    }
    *nColorsP = nColors;
}



static void
computecolorhash(pixel **          const pixels, 
                 unsigned int      const cols,
                 unsigned int      const rows, 
                 unsigned int      const maxcolors,
                 int *             const nColorsP,
                 FILE *            const ifP,
                 pixval            const maxval,
                 int               const format,
                 colorhash_table * const chtP,
                 const char **     const errorP) {
/*----------------------------------------------------------------------------
   Compute a color histogram from an image.  The input is one of two types:

   1) a two-dimensional array of pixels 'pixels';  In this case, 'pixels'
      is non-NULL and 'ifP' is NULL.

   2) an open file, positioned to the image data.  In this case,
      'pixels' is NULL and 'ifP' is non-NULL.  ifP is the stream
      descriptor for the input file, and 'maxval' and 'format' are
      parameters of the image data in it.
      
      We return with the file still open and its position undefined.  

   In either case, the image is 'cols' by 'rows'.

   Return the number of colors found as *colorsP.

   However, if 'maxcolors' is nonzero and the number of colors is
   greater than 'maxcolors', return a null return value and *colorsP
   undefined.
-----------------------------------------------------------------------------*/
    pixel * rowbuffer;  /* malloc'ed */
        /* Buffer for a row read from the input file; undefined (but still
           allocated) if input is not from a file.
        */

    MALLOCARRAY(rowbuffer, cols);
        
    if (rowbuffer == NULL)
        pm_error( "Unable to allocate %u-column row buffer.", cols);
    else {
        colorhash_table cht;
        bool tooManyColors;

        cht = alloccolorhash();

        if (cht == NULL)
            pm_error( "Unable to allocate color hash.");
        else {
            buildHashTable(ifP, pixels, cols, rows, maxval, format, maxcolors,
                           cht, rowbuffer,
                           nColorsP, &tooManyColors, errorP);
                
            if (tooManyColors) {
                ppm_freecolorhash(cht);
                *chtP = NULL;
            } else
                *chtP = cht;

            if (*errorP)
                ppm_freecolorhash(cht);
        }
        free(rowbuffer);
    }
}



colorhash_table
ppm_computecolorhash(pixel ** const pixels, 
                     int      const cols,
                     int      const rows, 
                     int      const maxcolors,
                     int *    const colorsP) {

    colorhash_table cht;
    const char * error;

    computecolorhash(pixels, cols, rows, maxcolors, colorsP, 
                     NULL, 0, 0, &cht, &error);

    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    }
    return cht;
}



colorhash_table
ppm_computecolorhash2(FILE * const ifP,
                      int    const cols,
                      int    const rows, 
                      pixval const maxval,
                      int    const format, 
                      int    const maxcolors,
                      int *  const colorsP ) {

    colorhash_table cht;
    const char * error;

    computecolorhash(NULL, cols, rows, maxcolors, colorsP,
                     ifP, maxval, format, &cht, &error);

    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    }
    return cht;
}



int
ppm_addtocolorhash(colorhash_table const cht, 
                   const pixel *   const colorP, 
                   int             const value) {
/*----------------------------------------------------------------------------
   Add color *colorP to the color hash 'cht' with associated value 'value'.

   Assume the color is not already in the hash.
-----------------------------------------------------------------------------*/
    int retval;
    colorhist_list chl;

    MALLOCVAR(chl);
    if (chl == NULL)
        retval = -1;
    else {
        int const hash = ppm_hashpixel(*colorP);

        chl->ch.color = *colorP;
        chl->ch.value = value;
        chl->next = cht[hash];
        cht[hash] = chl;
        retval = 0;
    }
    return retval;
}



void
ppm_delfromcolorhash(colorhash_table const cht, 
                     const pixel *   const colorP) {
/*----------------------------------------------------------------------------
   Delete the color *colorP from the colorhahs 'cht', if it's there.
-----------------------------------------------------------------------------*/
    int hash;
    colorhist_list * chlP;

    hash = ppm_hashpixel(*colorP);
    for (chlP = &cht[hash]; *chlP; chlP = &(*chlP)->next) {
        if (PPM_EQUAL((*chlP)->ch.color, *colorP)) {
            /* chlP points to a pointer to the hash chain element we want
               to remove.
            */
            colorhist_list const chl = *chlP;
            *chlP = chl->next;
            free(chl);
            return;
        }
    }
}



static unsigned int
colorHashSize(colorhash_table const cht) {
/*----------------------------------------------------------------------------
   Return the number of colors in the colorhash table 'cht'
-----------------------------------------------------------------------------*/
    unsigned int nColors;
        /* Number of colors found so far */
    int i;
    /* Loop through the hash table. */
    nColors = 0;
    for (i = 0; i < HASH_SIZE; ++i) {
        colorhist_list chl;
        for (chl = cht[i]; chl; chl = chl->next) 
            ++nColors;
    }
    return nColors;
}



colorhist_vector
ppm_colorhashtocolorhist(colorhash_table const cht, int const maxcolors) {

    colorhist_vector chv;
    colorhist_list chl;
    unsigned int chvSize;

    if (maxcolors == 0)
        /* We leave space for 5 more colors so caller can add in special
           colors like background color and transparent color.
        */
        chvSize = colorHashSize(cht) + 5;
    else
        /* Caller is responsible for making sure there are no more
           than 'maxcolors' colors in the colorhash table.  NOTE:
           Before March 2002, the maxcolors == 0 invocation didn't
           exist.  
        */
        chvSize = maxcolors;

    /* Collate the hash table into a simple colorhist array. */
    MALLOCARRAY(chv, chvSize);
    if (chv == NULL)
        pm_error("out of memory generating histogram");

    {
        int i, j;
        /* Loop through the hash table. */
        j = 0;
        for (i = 0; i < HASH_SIZE; ++i)
            for (chl = cht[i]; chl; chl = chl->next) {
                /* Add the new entry. */
                chv[j] = chl->ch;
                ++j;
            }
    }
    return chv;
}



colorhash_table
ppm_colorhisttocolorhash(colorhist_vector const chv, 
                         int              const colors) {

    colorhash_table retval;
    colorhash_table cht;
    const char * error;

    cht = alloccolorhash( );  /* Initializes to NULLs */
    if (cht == NULL)
        pm_error("Unable to allocate color hash");
    else {
        unsigned int i;

        for (i = 0, error = NULL; i < colors && !error; ++i) {
            pixel const color = chv[i].color;
            int const hash = ppm_hashpixel(color);
            
            colorhist_list chl;

            for (chl = cht[hash]; chl && !error; chl = chl->next)
                if (PPM_EQUAL(chl->ch.color, color))
                    pm_error( "same color found twice: (%u %u %u)",
                                PPM_GETR(color),
                                PPM_GETG(color),
                                PPM_GETB(color));
            MALLOCVAR(chl);
            if (chl == NULL)
                pm_error( "out of memory");
            else {
                chl->ch.color = color;
                chl->ch.value = i;
                chl->next = cht[hash];
                cht[hash] = chl;
            }
        }
        if (error)
            ppm_freecolorhash(cht);
    }
    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    } else
        retval = cht;

    return retval;
}



int
ppm_lookupcolor(colorhash_table const cht, 
                const pixel *   const colorP) {
    int hash;
    colorhist_list chl;

    hash = ppm_hashpixel(*colorP);
    for (chl = cht[hash]; chl; chl = chl->next)
        if (PPM_EQUAL(chl->ch.color, *colorP))
            return chl->ch.value;

    return -1;
}



void
ppm_freecolorhist(colorhist_vector const chv) {
    free(chv);
}



void
ppm_freecolorhash(colorhash_table const cht) {

    int i;
    colorhist_list chl, chlnext;

    for (i = 0; i < HASH_SIZE; ++i)
        for (chl = cht[i]; chl != (colorhist_list) 0; chl = chlnext) {
            chlnext = chl->next;
            free(chl);
        }
    free(cht);
}


/*****************************************************************************
  The following "color row" routines are taken from Ingo Wilken's ilbm
  package, dated December 1994.  Since they're only used by ppmtoilbm
  and ilbmtoppm today, they aren't documented or well maintained, but
  they seem pretty useful and ought to be used in other programs.

  -Bryan 2001.03.10
****************************************************************************/

/* libcmap2.c - support routines for color rows
**
** Copyright (C) 1994 Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

colorhash_table
ppm_colorrowtocolorhash(colorrow, ncolors)
    pixel *colorrow;
    int ncolors;
{
    colorhash_table cht;
    int i;

    cht = ppm_alloccolorhash();
    for( i = 0; i < ncolors; i++ ) {
        if( ppm_lookupcolor(cht, &colorrow[i]) < 0 ) {
            if( ppm_addtocolorhash(cht, &colorrow[i], i) < 0 )
                pm_error("out of memory adding to hash table");
        }
    }
    return cht;
}


pixel *
ppm_computecolorrow(pixels, cols, rows, maxcolors, ncolorsP)
    pixel **pixels;
    int cols, rows, maxcolors;
    int *ncolorsP;
{
    int ncolors, row, col;
    colorhash_table cht;
    pixel *pixrow;

    pixrow = ppm_allocrow(maxcolors);
    cht = ppm_alloccolorhash(); ncolors = 0;
    for( row = 0; row < rows; row++ ) {
        for( col = 0; col < cols; col++ ) {
            if( ppm_lookupcolor(cht, &pixels[row][col]) < 0 ) {
                if( ncolors >= maxcolors ) {
                    ppm_freerow(pixrow);
                    pixrow = (pixel *)0;
                    ncolors = -1;
                    goto fail;
                }
                if( ppm_addtocolorhash(cht, &pixels[row][col], ncolors) < 0 )
                    pm_error("out of memory adding to hash table");
                pixrow[ncolors] = pixels[row][col];
                ++ncolors;
            }
        }
    }
fail:
    ppm_freecolorhash(cht);

    *ncolorsP = ncolors;
    return pixrow;
}


pixel *
ppm_mapfiletocolorrow(file, maxcolors, ncolorsP, maxvalP)
    FILE *file;
    int maxcolors;
    int *ncolorsP;
    pixval *maxvalP;
{
    int cols, rows, format, row, col, ncolors;
    pixel *pixrow, *temprow;
    colorhash_table cht;

    pixrow = ppm_allocrow(maxcolors);

    ppm_readppminit(file, &cols, &rows, maxvalP, &format);
    temprow = ppm_allocrow(cols);
    cht = ppm_alloccolorhash(); ncolors = 0;
    for( row = 0; row < rows; row++ ) {
        ppm_readppmrow(file, temprow, cols, *maxvalP, format);
        for( col = 0; col < cols; col++ ) {
            if( ppm_lookupcolor(cht, &temprow[col]) < 0 ) {
                if( ncolors >= maxcolors ) {
                    ppm_freerow(pixrow);
                    pixrow = (pixel *)0;
                    ncolors = -1;
                    goto fail;
                }
                if( ppm_addtocolorhash(cht, &temprow[col], ncolors) < 0 )
                    pm_error("out of memory adding to hash table");
                pixrow[ncolors] = temprow[col];
                ++ncolors;
            }
        }
    }
fail:
    ppm_freecolorhash(cht);
    ppm_freerow(temprow);

    *ncolorsP = ncolors;
    return pixrow;
}



static int (*customCmp)(pixel *, pixel *);

#ifndef LITERAL_FN_DEF_MATCH
static qsort_comparison_fn customStub;
#endif

static int
customStub(const void * const a,
           const void * const b) {

    return (*customCmp)((pixel *)a, (pixel *)b);
}



#ifndef LITERAL_FN_DEF_MATCH
static qsort_comparison_fn pixelCmp;
#endif

static int
pixelCmp(const void * const a,
         const void * const b) {

    const pixel * const p1 = (const pixel *)a;
    const pixel * const p2 = (const pixel *)b;

    int diff;

    diff = PPM_GETR(*p1) - PPM_GETR(*p2);
    if( diff == 0 ) {
        diff = PPM_GETG(*p1) - PPM_GETG(*p2);
        if( diff == 0 )
            diff = PPM_GETB(*p1) - PPM_GETB(*p2);
    }
    return diff;
}



void
ppm_sortcolorrow(pixel * const colorrow,
                 int     const ncolors, 
                 int (*cmpfunc)(pixel *, pixel *)) {

    if (cmpfunc) {
        customCmp = cmpfunc;
        qsort((void *)colorrow, ncolors, sizeof(pixel), customStub);
    } else
        qsort((void *)colorrow, ncolors, sizeof(pixel), pixelCmp);
}



int
ppm_addtocolorrow(colorrow, ncolorsP, maxcolors, pixelP)
    pixel *colorrow;
    int *ncolorsP;
    int maxcolors;
    pixel *pixelP;
{
    int i;
    pixval r, g, b;

    r = PPM_GETR(*pixelP);
    g = PPM_GETG(*pixelP);
    b = PPM_GETB(*pixelP);

    for( i = 0; i < *ncolorsP; i++ ) {
        if( PPM_GETR(colorrow[i]) == r &&
            PPM_GETG(colorrow[i]) == g &&
            PPM_GETB(colorrow[i]) == b )
                return i;
    }

    i = *ncolorsP;
    if( i >= maxcolors )
        return -1;
    colorrow[i] = *pixelP;
    ++*ncolorsP;
    return i;
}


int
ppm_findclosestcolor(const pixel * const colormap, 
                     int           const ncolors, 
                     const pixel * const pP) {
    
    /* Search colormap for closest match.       */

    int i;
    int ind;
    unsigned int bestDist;

    bestDist = UINT_MAX;
    ind = -1;

    for(i = 0; i < ncolors && bestDist > 0; ++i) {
        unsigned int const dist = PPM_DISTANCE(*pP, colormap[i]);
        
        if (dist < bestDist ) {
            ind = i;
            bestDist = dist;
        }
    }
    return ind;
}


void
ppm_colorrowtomapfile(FILE *ofp, pixel *colormap, int ncolors, pixval maxval)
{
    int i;

    ppm_writeppminit(ofp, ncolors, 1, maxval, 1);
    for( i = 0; i < ncolors; i++ )
        ppm_writeppmrow(ofp, &colormap[i], 1, maxval, 1);
}



