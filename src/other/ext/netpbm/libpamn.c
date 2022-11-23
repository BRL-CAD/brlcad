/*=============================================================================
                                  libpamn.c
===============================================================================
   These are the library functions, which belong in the libnetpbm library,
   that deal with the PAM image format via maxval-normalized, floating point
   sample values.

   This file was originally written by Bryan Henderson and is contributed
   to the public domain by him and subsequent authors.
=============================================================================*/

#include <assert.h>

#include "pm_c_util.h"
#include "mallocvar.h"


#include "pam.h"
#include "fileio.h"
#include "pm_gamma.h"

#define EPSILON 1e-7



static void
allocpamrown(const struct pam * const pamP,
             tuplen **          const tuplerownP,
             const char **      const errorP) {
/*----------------------------------------------------------------------------
   We assume that the dimensions of the image are such that arithmetic
   overflow will not occur in our calculations.  NOTE: pnm_readpaminit()
   ensures this assumption is valid.
-----------------------------------------------------------------------------*/
    int const bytes_per_tuple = pamP->depth * sizeof(samplen);

    tuplen * tuplerown;
    const char * error = NULL;

    /* The tuple row data structure starts with 'width' pointers to
       the tuples, immediately followed by the 'width' tuples
       themselves.  Each tuple consists of 'depth' samples.
    */

    tuplerown = malloc(pamP->width * (sizeof(tuplen *) + bytes_per_tuple));
    if (tuplerown == NULL)
        pm_error("Out of memory allocating space for a tuple row of"
                    "%u tuples by %u samples per tuple "
                    "by %u bytes per sample.",
                    pamP->width, pamP->depth, (unsigned)sizeof(samplen));
    else {
        /* Now we initialize the pointers to the individual tuples to make this
           a regulation C two dimensional array.
        */
        
        unsigned char * p;
        unsigned int i;
        
        p = (unsigned char*) (tuplerown + pamP->width);
            /* location of Tuple 0 */
        for (i = 0; i < pamP->width; ++i) {
            tuplerown[i] = (tuplen) p;
            p += bytes_per_tuple;
        }
        *errorP = NULL;
        *tuplerownP = tuplerown;
    }
}



tuplen *
pnm_allocpamrown(const struct pam * const pamP) {
/*----------------------------------------------------------------------------
   We assume that the dimensions of the image are such that arithmetic
   overflow will not occur in our calculations.  NOTE: pnm_readpaminit()
   ensures this assumption is valid.
-----------------------------------------------------------------------------*/
    const char * error = NULL;
    tuplen * tuplerown = NULL;

    allocpamrown(pamP, &tuplerown, &error);

    if (error) {
        pm_error("pnm_allocpamrown() failed.  %s", error);
        free((void *)error);
        pm_longjmp();
    }

    return tuplerown;
}



static void
readpbmrow(const struct pam * const pamP, 
           tuplen *           const tuplenrow) {

    bit * bitrow;
    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;

    bitrow = pbm_allocrow(pamP->width);
    
    if (setjmp(jmpbuf) != 0) {
        pbm_freerow(bitrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;
        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        pbm_readpbmrow(pamP->file, bitrow, pamP->width, pamP->format);

        for (col = 0; col < pamP->width; ++col)
            tuplenrow[col][0] = bitrow[col] == PBM_BLACK ? 0.0 : 1.0;

        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow(bitrow);
}



static void
readpamrow(const struct pam * const pamP, 
           tuplen *           const tuplenrow) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    tuple * tuplerow;
    
    tuplerow = pnm_allocpamrow(pamP);
    
    if (setjmp(jmpbuf) != 0) {
        pnm_freepamrow(tuplerow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        float const scaler = 1.0 / pamP->maxval;
            /* Note: multiplication is faster than division, so we divide
               once here so we can multiply many times later.
            */

        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        pnm_readpamrow(pamP, tuplerow);
        for (col = 0; col < pamP->width; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane)
                tuplenrow[col][plane] = tuplerow[col][plane] * scaler;
        }
        pm_setjmpbuf(origJmpbufP);
    }
    pnm_freepamrow(tuplerow);
}



void 
pnm_readpamrown(const struct pam * const pamP, 
                tuplen *           const tuplenrow) {

    /* For speed, we don't check any of the inputs for consistency 
       here (unless it's necessary to avoid crashing).  Any consistency
       checking should have been done by a prior call to 
       pnm_writepaminit().
    */
    assert(pamP->maxval != 0);

    /* Need a special case for raw PBM because it has multiple tuples (8)
       packed into one byte.
    */
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE) {
        if (pamP->depth != 1)
            pm_error("Invalid pam structure passed to pnm_readpamrow().  "
                     "It says PBM format, but 'depth' member is not 1.");

        readpbmrow(pamP, tuplenrow);
    } else
        readpamrow(pamP, tuplenrow);
}



static void
writepbmrow(const struct pam * const pamP, 
            const tuplen *     const tuplenrow) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    bit * bitrow;

    bitrow = pbm_allocrow(pamP->width);

    if (setjmp(jmpbuf) != 0) {
        pbm_freerow(bitrow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (col = 0; col < pamP->width; ++col)
            bitrow[col] = tuplenrow[col][0] < 0.5 ? PBM_BLACK : PBM_WHITE;
        pbm_writepbmrow(pamP->file, bitrow, pamP->width, 
                        pamP->format == PBM_FORMAT);

        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow(bitrow);
} 



static void
writepamrow(const struct pam * const pamP, 
            const tuplen *     const tuplenrow) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    tuple * tuplerow;
    
    tuplerow = pnm_allocpamrow(pamP);
    
    if (setjmp(jmpbuf) != 0) {
        pnm_freepamrow(tuplerow);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int col;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (col = 0; col < pamP->width; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane)
                tuplerow[col][plane] = (sample)
                    (tuplenrow[col][plane] * pamP->maxval + 0.5);
        }    
        pnm_writepamrow(pamP, tuplerow);

        pm_setjmpbuf(origJmpbufP);
    }
    pnm_freepamrow(tuplerow);
}



void 
pnm_writepamrown(const struct pam * const pamP, 
                 const tuplen *     const tuplenrow) {

    /* For speed, we don't check any of the inputs for consistency 
       here (unless it's necessary to avoid crashing).  Any consistency
       checking should have been done by a prior call to 
       pnm_writepaminit().
    */
    assert(pamP->maxval != 0);

    /* Need a special case for raw PBM because it has multiple tuples (8)
       packed into one byte.
    */
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE)
        writepbmrow(pamP, tuplenrow);
    else
        writepamrow(pamP, tuplenrow);
}



tuplen **
pnm_allocpamarrayn(const struct pam * const pamP) {
    
    tuplen ** tuplenarray = NULL;
    const char * error = NULL;

    /* If the speed of this is ever an issue, it might be sped up a little
       by allocating one large chunk.
    */
    
    MALLOCARRAY(tuplenarray, pamP->height);
    if (tuplenarray == NULL) 
        pm_error("Out of memory allocating the row pointer section of "
                    "a %u row array", pamP->height);
    else {
        unsigned int rowsDone;

        rowsDone = 0;
        error = NULL;

        while (rowsDone < pamP->height && !error) {
            allocpamrown(pamP, &tuplenarray[rowsDone], &error);
            if (!error)
                ++rowsDone;
        }
        if (error) {
            unsigned int row;
            for (row = 0; row < rowsDone; ++row)
                pnm_freepamrown(tuplenarray[rowsDone]);
            free(tuplenarray);
        }
    }
    if (error) {
        pm_error("pnm_allocpamarrayn() failed.  %s", error);
        free((void *)error);
        pm_longjmp();
    }

    return(tuplenarray);
}



void
pnm_freepamarrayn(tuplen **          const tuplenarray, 
                  const struct pam * const pamP) {

    int row;
    for (row = 0; row < pamP->height; row++)
        pnm_freepamrown(tuplenarray[row]);

    free(tuplenarray);
}



tuplen** 
pnm_readpamn(FILE *       const file, 
             struct pam * const pamP, 
             int          const size) {

    tuplen **tuplenarray;
    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;

    pnm_readpaminit(file, pamP, size);
    
    tuplenarray = pnm_allocpamarrayn(pamP);
    
    if (setjmp(jmpbuf) != 0) {
        pnm_freepamarrayn(tuplenarray, pamP);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {
        unsigned int row;

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        for (row = 0; row < pamP->height; ++row) 
            pnm_readpamrown(pamP, tuplenarray[row]);

        pm_setjmpbuf(origJmpbufP);
    }
    return tuplenarray;
}



void 
pnm_writepamn(struct pam * const pamP, 
              tuplen **    const tuplenarray) {

    unsigned int row;

    pnm_writepaminit(pamP);
    
    for (row = 0; row < pamP->height; ++row) 
        pnm_writepamrown(pamP, tuplenarray[row]);
}



void
pnm_normalizetuple(struct pam * const pamP,
                   tuple        const tuple,
                   tuplen       const tuplen) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) 
        tuplen[plane] = (samplen)tuple[plane] / pamP->maxval;
}



void
pnm_unnormalizetuple(struct pam * const pamP,
                     tuplen       const tuplen,
                     tuple        const tuple) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane) 
        tuple[plane] = tuplen[plane] * pamP->maxval + 0.5;
}



void
pnm_normalizeRow(struct pam *             const pamP,
                 const tuple *            const tuplerow,
                 const pnm_transformMap * const transform,
                 tuplen *                 const tuplenrow) {

    float const scaler = 1.0 / pamP->maxval;
        /* Note: multiplication is faster than division, so we divide
           once here so we can multiply many times later.
        */
    unsigned int plane;
    
    for (plane = 0; plane < pamP->depth; ++plane) {
        if (transform && transform[plane]) {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col) {
                sample const sample = tuplerow[col][plane];
                tuplenrow[col][plane] = transform[plane][sample];
            }
        } else {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col)
                tuplenrow[col][plane] = tuplerow[col][plane] * scaler;
        }
    }
}



static sample
reversemap(samplen          const samplen,
           pnm_transformMap const transformMap,
           sample           const maxval) {
/*----------------------------------------------------------------------------
   Find the integer sample value that maps to the normalized samplen value
   'samplen' through the map 'transformMap'.  We interpret the map as
   mapping the value N+1 to all the values transformMap[N] through 
   transformMap[N+1], and we expect transformMap[N+1] to be greater than
   transformMap[N] for all N.
-----------------------------------------------------------------------------*/
    /* Do a binary search, since the values are in sorted (increasing)
       order
    */
    
    sample low, high;

    low = 0; high = maxval;  /* Consider whole range to start */

    while (low < high) {
        unsigned int const middle = (low + high) / 2;

        if (samplen < transformMap[middle])
            /* Restrict  our consideration to the lower half of the range */
            high = middle;
        else
            /* Restrict our consideration to the upper half of the range */
            low = middle + 1;
    }
    return low;
}



void
pnm_unnormalizeRow(struct pam *             const pamP,
                   const tuplen *           const tuplenrow,
                   const pnm_transformMap * const transform,
                   tuple *                  const tuplerow) {

    unsigned int plane;
    
    for (plane = 0; plane < pamP->depth; ++plane) {
        if (transform && transform[plane]) {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col)
                tuplerow[col][plane] = 
                    reversemap(tuplenrow[col][plane], 
                               transform[plane], pamP->maxval);
        } else {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col)
                tuplerow[col][plane] = 
                    tuplenrow[col][plane] * pamP->maxval + 0.5;
        }
    }
}



typedef samplen (*gammaFunction)(samplen);

static void
gammaCommon(struct pam *  const pamP,
            tuplen *      const tuplenrow,
            gammaFunction       gammafn) {

    unsigned int plane;
    unsigned int opacityPlane;
    int haveOpacity;
    
    pnm_getopacity(pamP, &haveOpacity, &opacityPlane);

    for (plane = 0; plane < pamP->depth; ++plane) {
        if (haveOpacity && plane == opacityPlane) {
            /* It's an opacity (alpha) plane, which means there is
               no gamma adjustment in it.  
            */
        } else {
            unsigned int col;
            for (col = 0; col < pamP->width; ++col)
                tuplenrow[col][plane] = gammafn(tuplenrow[col][plane]);
        }
    }
}



void
pnm_gammarown(struct pam * const pamP,
              tuplen *     const tuplenrow) {

    gammaCommon(pamP, tuplenrow, &pm_gamma709);
}



void
pnm_ungammarown(struct pam * const pamP,
                tuplen *     const tuplenrow) {

    gammaCommon(pamP, tuplenrow, &pm_ungamma709);
}



enum applyUnapply {OPACITY_APPLY, OPACITY_UNAPPLY};

static void
applyopacityCommon(enum applyUnapply const applyUnapply,
                   struct pam *      const pamP,
                   tuplen *          const tuplenrow) {
/*----------------------------------------------------------------------------
   Either apply or unapply opacity to the row tuplenrow[], per
   'applyUnapply'.  Apply means to multiply each foreground sample by
   the opacity value for that pixel; Unapply means to do the inverse, as
   if the foreground values had already been so multiplied.
-----------------------------------------------------------------------------*/
    unsigned int opacityPlane;
    int haveOpacity;
    
    pnm_getopacity(pamP, &haveOpacity, &opacityPlane);

    if (haveOpacity) {
        unsigned int plane;
        for (plane = 0; plane < pamP->depth; ++plane) {
            if (plane != opacityPlane) {
                unsigned int col;
                for (col = 0; col < pamP->width; ++col) {
                    tuplen const thisTuple = tuplenrow[col];

                    switch (applyUnapply) {
                    case OPACITY_APPLY:
                        thisTuple[plane] *= thisTuple[opacityPlane];
                        break;
                    case OPACITY_UNAPPLY:
                        if (thisTuple[opacityPlane] < EPSILON) {
                            /* There is no foreground here at all.  So
                               the color plane values must be zero and
                               as output it makes absolutely no
                               difference what they are (they must be
                               multiplied by the opacity -- zero -- to
                               be used).
                            */
                            assert(thisTuple[plane] < EPSILON);
                        } else
                            thisTuple[plane] /= thisTuple[opacityPlane];
                        break;
                    }
                }
            }
        }
    }
}


void
pnm_applyopacityrown(struct pam * const pamP,
                     tuplen *     const tuplenrow) {

    applyopacityCommon(OPACITY_APPLY, pamP, tuplenrow);

}



void
pnm_unapplyopacityrown(struct pam * const pamP,
                       tuplen *     const tuplenrow) {

    applyopacityCommon(OPACITY_UNAPPLY, pamP, tuplenrow);
}



static void
fillInMap(pnm_transformMap const ungammaTransformMap,
          sample           const maxval,
          float            const offset) {

    float const scaler = 1.0/maxval;  /* divide only once, it's slow */

    sample sample;

    /* Fill in the map */
    for (sample = 0; sample <= maxval; ++sample) {
        samplen const samplen = (sample + offset) * scaler;
        ungammaTransformMap[sample] = pm_ungamma709(samplen);
    }
}



static pnm_transformMap *
createUngammaMapOffset(const struct pam * const pamP,
                       float              const offset) {
/*----------------------------------------------------------------------------
   Create a transform table that computes ungamma(arg+offset) for arg
   in [0..maxval]; So with offset == 0, you get a function that can be
   used in converting integer sample values to normalized ungamma'ed
   samplen values.  But with offset == 0.5, you get a function that
   can be used in a reverse lookup to convert normalized ungamma'ed
   samplen values to integer sample values.  The 0.5 effectively does
   the rounding.

   This never throws an error.  Return value NULL means failed.
-----------------------------------------------------------------------------*/
    pnm_transformMap * retval;
    pnm_transformMap ungammaTransformMap;

    MALLOCARRAY(retval, pamP->depth);

    if (retval != NULL) {
        MALLOCARRAY(ungammaTransformMap, pamP->maxval+1);

        if (ungammaTransformMap != NULL) {
            int haveOpacity;
            unsigned int opacityPlane;
            unsigned int plane;

            pnm_getopacity(pamP, &haveOpacity, &opacityPlane);

            for (plane = 0; plane < pamP->depth; ++plane) {
                if (haveOpacity && plane == opacityPlane)
                    retval[plane] = NULL;
                else
                    retval[plane] = ungammaTransformMap;
            }            
            fillInMap(ungammaTransformMap, pamP->maxval, offset);
        } else {
            free(retval);
            retval = NULL;
        }
    }
    return retval;
}



pnm_transformMap *
pnm_createungammatransform(const struct pam * const pamP) {

    return createUngammaMapOffset(pamP, 0.0);
}



pnm_transformMap *
pnm_creategammatransform(const struct pam * const pamP) {

    /* Since we're creating a map to be used backwards (you search for
       the normalized value in the array, and the result is the array
       index at which you found it), the gamma transform map is almost
       identical to the ungamma transform map -- just with a 0.5 offset
       to effect rounding.
    */
    return createUngammaMapOffset(pamP, 0.5);
}



void
pnm_freegammatransform(const pnm_transformMap * const transform,
                       const struct pam *       const pamP) {

    unsigned int plane;

    for (plane = 0; plane < pamP->depth; ++plane)
        if (transform[plane])
            free(transform[plane]);

    free((void*)transform);
}
