/*----------------------------------------------------------------------------
                                  libpamn.c
------------------------------------------------------------------------------
   These are the library functions, which belong in the libnetpbm library,
   that deal with the PAM image format via maxval-normalized, floating point
   sample values.
-----------------------------------------------------------------------------*/

#include <assert.h>

#include "pm_c_util.h"
#include "mallocvar.h"
#include "pam.h"
#include "fileio.h"
#include "pm_gamma.h"

#define EPSILON 1e-7



tuplen *
pnm_allocpamrown(const struct pam * const pamP) {
/*----------------------------------------------------------------------------
   We assume that the dimensions of the image are such that arithmetic
   overflow will not occur in our calculations.  NOTE: pnm_readpaminit()
   ensures this assumption is valid.
-----------------------------------------------------------------------------*/
    const int bytes_per_tuple = pamP->depth * sizeof(samplen);
    tuplen * tuplerown;

    /* The tuple row data structure starts with 'width' pointers to
       the tuples, immediately followed by the 'width' tuples
       themselves.  Each tuple consists of 'depth' samples.
    */

    tuplerown = malloc(pamP->width * (sizeof(tuplen *) + bytes_per_tuple));
    if (tuplerown == NULL)
        pm_error("Out of memory allocating space for a tuple row of\n"
                 "%d tuples by %d samples per tuple by %d bytes per sample.",
                 pamP->width, pamP->depth, sizeof(samplen));

    {
        /* Now we initialize the pointers to the individual tuples to make this
           a regulation C two dimensional array.
        */
        
        char *p;
        int i;
        
        p = (char*) (tuplerown + pamP->width);  /* location of Tuple 0 */
        for (i = 0; i < pamP->width; i++) {
            tuplerown[i] = (tuplen) p;
            p += bytes_per_tuple;
        }
    }
    return(tuplerown);
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
        int col;
        bit *bitrow;
        if (pamP->depth != 1)
            pm_error("Invalid pam structure passed to pnm_readpamrow().  "
                     "It says PBM format, but 'depth' member is not 1.");
        bitrow = pbm_allocrow(pamP->width);
        pbm_readpbmrow(pamP->file, bitrow, pamP->width, pamP->format);
        for (col = 0; col < pamP->width; col++)
            tuplenrow[col][0] = 
                bitrow[col] == PBM_BLACK ? 0.0 : 1.0;
        pbm_freerow(bitrow);
    } else {
        float const scaler = 1.0 / pamP->maxval;
            /* Note: multiplication is faster than division, so we divide
               once here so we can multiply many times later.
            */
        int col;
        tuple * tuplerow;

        tuplerow = pnm_allocpamrow(pamP);

        pnm_readpamrow(pamP, tuplerow);
        for (col = 0; col < pamP->width; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane)
                tuplenrow[col][plane] = tuplerow[col][plane] * scaler;
        }
        pnm_freepamrow(tuplerow);
    }
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
    if (PAM_FORMAT_TYPE(pamP->format) == PBM_TYPE) {
        int col;
        bit *bitrow;
        bitrow = pbm_allocrow(pamP->width);
        for (col = 0; col < pamP->width; col++)
            bitrow[col] = 
                tuplenrow[col][0] < 0.5 ? PBM_BLACK : PBM_WHITE;
        pbm_writepbmrow(pamP->file, bitrow, pamP->width, 
                        pamP->format == PBM_FORMAT);
        pbm_freerow(bitrow);
    } else {
        tuple * tuplerow;
        int col;

        tuplerow = pnm_allocpamrow(pamP);

        for (col = 0; col < pamP->width; ++col) {
            unsigned int plane;
            for (plane = 0; plane < pamP->depth; ++plane)
                tuplerow[col][plane] = (sample)
                    (tuplenrow[col][plane] * pamP->maxval + 0.5);
        }    
        pnm_writepamrow(pamP, tuplerow);
        pnm_freepamrow(tuplerow);
    }
}



tuplen **
pnm_allocpamarrayn(const struct pam * const pamP) {
    
    tuplen **tuplenarray;
    int row;

    /* If the speed of this is ever an issue, it might be sped up a little
       by allocating one large chunk.
    */
    
    MALLOCARRAY(tuplenarray, pamP->height);
    if (tuplenarray == NULL) 
        pm_error("Out of memory allocating the row pointer section of "
                 "a %u row array", pamP->height);

    for (row = 0; row < pamP->height; row++) {
        tuplenarray[row] = pnm_allocpamrown(pamP);
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
    int row;

    pnm_readpaminit(file, pamP, size);
    
    tuplenarray = pnm_allocpamarrayn(pamP);
    
    for (row = 0; row < pamP->height; row++) 
        pnm_readpamrown(pamP, tuplenarray[row]);

    return(tuplenarray);
}



void 
pnm_writepamn(struct pam * const pamP, 
              tuplen **    const tuplenarray) {

    int row;

    pnm_writepaminit(pamP);
    
    for (row = 0; row < pamP->height; row++) 
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
    bool haveOpacity;
    
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

    unsigned int opacityPlane;
    bool haveOpacity;
    
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
-----------------------------------------------------------------------------*/
    pnm_transformMap * retval;
    pnm_transformMap ungammaTransformMap;

    MALLOCARRAY(retval, pamP->depth);

    if (retval != NULL) {
        MALLOCARRAY(ungammaTransformMap, pamP->maxval+1);

        if (ungammaTransformMap != NULL) {
            bool haveOpacity;
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
