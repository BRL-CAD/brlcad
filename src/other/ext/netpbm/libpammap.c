/******************************************************************************
                                  libpammap.c
*******************************************************************************

  These are functions that deal with tuple hashes and tuple tables.

  Both tuple hashes and tuple tables let you associate an arbitrary
  integer with a tuple value.  A tuple hash lets you look up the one
  integer value (if any) associated with a given tuple value, having
  the low memory and execution time characteristics of a hash table.
  A tuple table lets you scan all the values, being a table of elements
  that consist of an ordered pair of a tuple value and integer.

******************************************************************************/

#include <assert.h>

#include "pm_c_util.h"
#include "mallocvar.h"
#include "pam.h"
#include "pammap.h"


#define HASH_SIZE 20023

unsigned int
pnm_hashtuple(struct pam * const pamP, tuple const tuple) {
/*----------------------------------------------------------------------------
   Return the hash value of the tuple 'tuple' -- i.e. an index into a hash
   table.
-----------------------------------------------------------------------------*/
    int i;
    unsigned int hash;
    const unsigned int hash_factor[] = {33023, 30013, 27011};

    hash = 0;  /* initial value */
    for (i = 0; i < MIN(pamP->depth, 3); ++i) {
        hash += tuple[i] * hash_factor[i];  /* May overflow */
    }
    hash %= HASH_SIZE;
    return hash;
}




tuplehash
pnm_createtuplehash(void) {
/*----------------------------------------------------------------------------
   Create an empty tuple hash -- i.e. a hash table of zero length hash chains.
-----------------------------------------------------------------------------*/
    tuplehash retval;
    unsigned int i;

    MALLOCARRAY(retval, HASH_SIZE);

    if (retval == NULL)
        pm_error("Out of memory allocating tuple hash of size %u",
                 HASH_SIZE);

    for (i = 0; i < HASH_SIZE; ++i) 
        retval[i] = NULL;

    return retval;
}



void
pnm_destroytuplehash(tuplehash const tuplehash) {

    int i;

    /* Free the chains */

    for (i = 0; i < HASH_SIZE; ++i) {
        struct tupleint_list_item * p;
        struct tupleint_list_item * next;
        
        /* Walk this chain, freeing each element */
        for (p = tuplehash[i]; p; p = next) {
            next = p->next;

            free(p);
        }            
    }

    /* Free the table of chains */
    free(tuplehash);
}




static struct tupleint_list_item * 
allocTupleIntListItem(struct pam * const pamP) {


    /* This is complicated by the fact that the last element of a 
       tupleint_list_item is of variable length, because the last element
       of _it_ is of variable length 
    */
    struct tupleint_list_item * retval;

    unsigned int const size = 
        sizeof(*retval) - sizeof(retval->tupleint.tuple) 
        + pamP->depth * sizeof(sample);

    retval = (struct tupleint_list_item *) malloc(size);

    return retval;
}



void
pnm_addtotuplehash(struct pam *   const pamP,
                   tuplehash      const tuplehash, 
                   tuple          const tupletoadd,
                   int            const value,
                   int *          const fitsP) {
/*----------------------------------------------------------------------------
   Add a tuple value to the hash -- assume it isn't already there.

   Allocate new space for the tuple value and the hash chain element.

   If we can't allocate space for the new hash chain element, don't
   change anything and return *fitsP = FALSE;
-----------------------------------------------------------------------------*/
    struct tupleint_list_item * const listItemP = allocTupleIntListItem(pamP);
    if (listItemP == NULL)
        *fitsP = FALSE;
    else {
        unsigned int const hashvalue = pnm_hashtuple(pamP, tupletoadd);
    
        *fitsP = TRUE;

        pnm_assigntuple(pamP, listItemP->tupleint.tuple, tupletoadd);
        listItemP->tupleint.value = value;
        listItemP->next = tuplehash[hashvalue];
        tuplehash[hashvalue] = listItemP;
    }
}



void
pnm_lookuptuple(struct pam *    const pamP, 
                const tuplehash       tuplehash, 
                const tuple           searchval, 
                int *           const foundP, 
                int *           const retvalP) {
    
    unsigned int const hashvalue = pnm_hashtuple(pamP, searchval);
    struct tupleint_list_item * p;
    struct tupleint_list_item * found;

    found = NULL;  /* None found yet */
    for (p = tuplehash[hashvalue]; p && !found; p = p->next)
        if (pnm_tupleequal(pamP, p->tupleint.tuple, searchval)) {
            found = p;
        }

    if (found) {
        *foundP = TRUE;
        *retvalP = found->tupleint.value;
    } else
        *foundP = FALSE;
}



static void
addColorOccurrenceToHash(tuple          const color, 
                         tuplehash      const tuplefreqhash,
                         struct pam *   const pamP,
                         unsigned int   const maxsize,
                         unsigned int * const sizeP,
                         bool *         const fullP) {
               
    unsigned int const hashvalue = pnm_hashtuple(pamP, color);
            
    struct tupleint_list_item *p;

    for (p = tuplefreqhash[hashvalue]; 
         p && !pnm_tupleequal(pamP, p->tupleint.tuple, color);
         p = p->next);

    if (p) {
        /* It's in the hash; just tally one more occurence */
        ++p->tupleint.value;
        *fullP = FALSE;
    } else {
        /* It's not in the hash yet, so add it (if allowed) */
        ++(*sizeP);
        if (maxsize > 0 && *sizeP > maxsize) 
            *fullP = TRUE;
        else {
            *fullP = FALSE;
            p = allocTupleIntListItem(pamP);
            if (p == NULL)
                pm_error("out of memory computing hash table");
            pnm_assigntuple(pamP, p->tupleint.tuple, color);
            p->tupleint.value = 1;
            p->next = tuplefreqhash[hashvalue];
            tuplefreqhash[hashvalue] = p;
        }
    }
}



void
pnm_addtuplefreqoccurrence(struct pam *   const pamP,
                           tuple          const value,
                           tuplehash      const tuplefreqhash,
                           int *          const firstOccurrenceP) {

    unsigned int const hashvalue = pnm_hashtuple(pamP, value);
            
    struct tupleint_list_item * p;

    for (p = tuplefreqhash[hashvalue]; 
         p && !pnm_tupleequal(pamP, p->tupleint.tuple, value);
         p = p->next);

    if (p) {
        /* It's in the hash; just tally one more occurence */
        ++p->tupleint.value;
        *firstOccurrenceP = FALSE;
    } else {
        struct tupleint_list_item * p;

        /* It's not in the hash yet, so add it */
        *firstOccurrenceP = TRUE;

        p = allocTupleIntListItem(pamP);
        if (p == NULL)
            pm_error("out of memory computing hash table");

        pnm_assigntuple(pamP, p->tupleint.tuple, value);
        p->tupleint.value = 1;
        p->next = tuplefreqhash[hashvalue];
        tuplefreqhash[hashvalue] = p;
    }
}



static void
computehashrecoverable(struct pam *   const pamP,
                       tuple **       const tupleArray, 
                       unsigned int   const maxsize, 
                       sample         const newMaxval,
                       unsigned int * const sizeP,
                       tuplehash *    const tuplefreqhashP,
                       tuple **       const rowbufferP,
                       tuple *        const colorP) {
/*----------------------------------------------------------------------------
   This is computetuplefreqhash(), only it leaves a trail so that if it
   happens to longjmp out because of a failed memory allocation, the
   setjmp'er can cleanup whatever it had done so far.
-----------------------------------------------------------------------------*/
    unsigned int row;
    struct pam freqPam;
    bool full;

    freqPam = *pamP;
    freqPam.maxval = newMaxval;

    *tuplefreqhashP = pnm_createtuplehash();
    *sizeP = 0;   /* initial value */
    
    *rowbufferP = pnm_allocpamrow(pamP);
    
    *colorP = pnm_allocpamtuple(&freqPam);
    
    full = FALSE;  /* initial value */
    
    /* Go through the entire raster, building a hash table of
       tuple values. 
    */
    for (row = 0; row < pamP->height && !full; ++row) {
        int col;
        const tuple * tuplerow;  /* The row of tuples we are processing */
        
        if (tupleArray)
            tuplerow = tupleArray[row];
        else {
            pnm_readpamrow(pamP, *rowbufferP);
            tuplerow = *rowbufferP;
        }
        for (col = 0; col < pamP->width && !full; ++col) {
            pnm_scaletuple(pamP, *colorP, tuplerow[col], freqPam.maxval);
            addColorOccurrenceToHash(
                *colorP, *tuplefreqhashP, &freqPam, maxsize, sizeP, &full);
        }
    }

    pnm_freepamtuple(*colorP); *colorP = NULL;
    pnm_freepamrow(*rowbufferP); *rowbufferP = NULL;

    if (full) {
        pnm_destroytuplehash(*tuplefreqhashP);
        *tuplefreqhashP = NULL;
    }
}



static tuplehash
computetuplefreqhash(struct pam *   const pamP,
                     tuple **       const tupleArray, 
                     unsigned int   const maxsize, 
                     sample         const newMaxval,
                     unsigned int * const sizeP) {
/*----------------------------------------------------------------------------
  Compute a tuple frequency hash from a PAM.  This is a hash that gives
  you the number of times a given tuple value occurs in the PAM.  You can
  supply the input PAM in one of two ways:

  1) a two-dimensional array of tuples tupleArray[][];  In this case,
     'tupleArray' is non-NULL.

  2) an open PAM file, positioned to the raster.  In this case,
     'tupleArray' is NULL.  *pamP contains the file descriptor.
  
     We return with the file still open and its position undefined.  

  In either case, *pamP contains parameters of the tuple array.

  Return the number of unique tuple values found as *sizeP.

  However, if the number of unique tuple values is greater than 'maxsize', 
  return a null return value and *sizeP undefined.

  The tuple values that index the hash are scaled to a new maxval of
  'newMaxval'.  E.g.  if the input has maxval 100 and 'newMaxval' is
  50, and a particular tuple has sample value 50, it would be counted
  as sample value 25 in the hash.
-----------------------------------------------------------------------------*/
    tuplehash tuplefreqhash;
    tuple * rowbuffer;  /* malloc'ed */
        /* Buffer for a row read from the input file; undefined (but still
           allocated) if input is not from a file.
        */
    tuple color;  
        /* The color currently being added, scaled to the new maxval */
    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    
    /* Initialize to "none" for purposes of error recovery */
    tuplefreqhash = NULL;
    rowbuffer = NULL;
    color = NULL;

    if (setjmp(jmpbuf) == 0) {
        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);
        computehashrecoverable(pamP, tupleArray, maxsize, newMaxval, sizeP,
                               &tuplefreqhash, &rowbuffer, &color);
        pm_setjmpbuf(origJmpbufP);
    } else {
        if (color) 
            pnm_freepamtuple(color);
        if (rowbuffer)
            pnm_freepamrow(rowbuffer);
        if (tuplefreqhash)
            pnm_destroytuplehash(tuplefreqhash);
        pm_longjmp();
    }
    return tuplefreqhash;
}



tuplehash
pnm_computetuplefreqhash(struct pam *   const pamP,
                         tuple **       const tupleArray,
                         unsigned int   const maxsize,
                         unsigned int * const sizeP) {
/*----------------------------------------------------------------------------
   Compute the tuple frequency hash for the tuple array tupleArray[][].
-----------------------------------------------------------------------------*/
    return computetuplefreqhash(pamP, tupleArray, maxsize, pamP->maxval, 
                                sizeP);
}



static int 
alloctupletable(const struct pam * const pamP, 
                unsigned int       const size,
                tupletable *       const tupletableP) {

    if (UINT_MAX / sizeof(struct tupleint) < size) return 1;
    {
        unsigned int const mainTableSize = size * sizeof(struct tupleint *);
        unsigned int const tupleIntSize = 
            sizeof(struct tupleint) - sizeof(sample) 
            + pamP->depth * sizeof(sample);

        /* To save the enormous amount of time it could take to allocate
           each individual tuple, we do a trick here and allocate everything
           as a single malloc block and suballocate internally.
        */
        if ((UINT_MAX - mainTableSize) / tupleIntSize < size) return 1;

        {
            unsigned int const allocSize = mainTableSize + size * tupleIntSize;
            void * pool;

            pool = malloc(allocSize);

            if (!pool) return 1;

            {
                tupletable const tbl = (tupletable) pool;

                unsigned int i;

                for (i = 0; i < size; ++i)
                    tbl[i] = (struct tupleint *)
                        ((char*)pool + mainTableSize + i * tupleIntSize);

                *tupletableP = tbl;
            }
        }
    }

    return 0;
}



tupletable
pnm_alloctupletable(const struct pam * const pamP,
                    unsigned int       const size) {

    tupletable retval;

    if (alloctupletable(pamP, size, &retval)) {
        pm_error("Failed to allocation tuple table of size %u", size);
    }
    return retval;
}



void
pnm_freetupletable(struct pam * const pamP,
                   tupletable   const tupletable) {

    /* Note that the address 'tupletable' is, to the operating system, 
       the address of a larger block of memory that contains not only 
       tupletable, but all the samples to which it points (e.g.
       tupletable[0].tuple[0])
    */

    free(tupletable);
}



void
pnm_freetupletable2(struct pam * const pamP,
                    tupletable2  const tupletable) {

    pnm_freetupletable(pamP, tupletable.table);
}



static tupletable
tuplehashtotable(const struct pam * const pamP,
                 tuplehash          const tuplehash,
                 unsigned int       const allocsize) {
/*----------------------------------------------------------------------------
   Create a tuple table containing the info from a tuple hash.  Allocate
   space in the table for 'allocsize' elements even if there aren't that
   many tuple values in the input hash.  That's so the caller has room
   for expansion.

   Caller must ensure that 'allocsize' is at least as many tuple values
   as there are in the input hash.

   We allocate new space for all the table contents; there are no pointers
   in the table to tuples or anything else in existing space.
-----------------------------------------------------------------------------*/
    tupletable tupletable;

    if (alloctupletable(pamP, allocsize, &tupletable)) {
        pm_error("Failed to allocate table table of size %u", allocsize);
    } else {
        unsigned int i, j;
        /* Loop through the hash table. */
        j = 0;
        for (i = 0; i < HASH_SIZE; ++i) {
            /* Walk this hash chain */
            struct tupleint_list_item * p;
            for (p = tuplehash[i]; p; p = p->next) {
                assert(j < allocsize);
                tupletable[j]->value = p->tupleint.value;
                pnm_assigntuple(pamP, tupletable[j]->tuple, p->tupleint.tuple);
                ++j;
            }
        }
    }
    return tupletable;
}



tupletable
pnm_tuplehashtotable(const struct pam * const pamP,
                     tuplehash          const tuplehash,
                     unsigned int       const allocsize) {

    tupletable tupletable;

    tupletable = tuplehashtotable(pamP, tuplehash, allocsize);

    if (tupletable == NULL)
        pm_error("out of memory generating tuple table");

    return tupletable;
}



tuplehash
pnm_computetupletablehash(struct pam * const pamP, 
                          tupletable   const tupletable,
                          unsigned int const tupletableSize) {
/*----------------------------------------------------------------------------
   Create a tuple hash containing indices into the tuple table
   'tupletable'.  The hash index for the hash is the value of a tuple;
   the hash value is the tuple table index for the element in the
   tuple table that contains that tuple value.

   Assume there are no duplicate tuple values in the tuple table.

   We allocate space for the main hash table and all the elements of the
   hash chains.
-----------------------------------------------------------------------------*/
    tuplehash tupletablehash;
    unsigned int i;
    bool fits;
    
    tupletablehash = pnm_createtuplehash();

    fits = TRUE;  /* initial assumption */
    for (i = 0; i < tupletableSize && fits; ++i) {
        pnm_addtotuplehash(pamP, tupletablehash, 
                           tupletable[i]->tuple, i, &fits);
    }
    if (!fits) {
        pnm_destroytuplehash(tupletablehash);
        pm_error("Out of memory computing tuple hash from tuple table");
    }
    return tupletablehash;
}



tupletable
pnm_computetuplefreqtable2(struct pam *   const pamP,
                           tuple **       const tupleArray,
                           unsigned int   const maxsize,
                           sample         const newMaxval,
                           unsigned int * const countP) {
/*----------------------------------------------------------------------------
   Compute a tuple frequency table from a PAM image.  This is an
   array that tells how many times each tuple value occurs in the
   image.

   Except for the format of the output, this function is the same as
   computetuplefreqhash().

   If there are more than 'maxsize' unique tuple values in tupleArray[][],
   give up.

   Return the array in newly malloc'ed storage.  Allocate space for
   'maxsize' entries even if there aren't that many distinct tuple
   values in tupleArray[].  That's so the caller has room for
   expansion.

   If 'maxsize' is zero, allocate exactly as much space as there are
   distinct tuple values in tupleArray[], and don't give up no matter
   how many tuple values we find (except, of course, we abort if we
   can't get enough memory).

   Return the number of unique tuple values in tupleArray[][] as
   *countP.


   Scale the tuple values to a new maxval of 'newMaxval' before
   processing them.  E.g. if the input has maxval 100 and 'newMaxval'
   is 50, and a particular tuple has sample value 50, it would be
   listed as sample value 25 in the output table.  This makes the
   output table smaller and the processing time less.
-----------------------------------------------------------------------------*/
    tuplehash tuplefreqhash;
    tupletable tuplefreqtable;
    unsigned int uniqueCount;

    tuplefreqhash = computetuplefreqhash(pamP, tupleArray, maxsize, 
                                         newMaxval, &uniqueCount);
    if (tuplefreqhash == NULL)
        tuplefreqtable = NULL;
    else {
        unsigned int tableSize = (maxsize == 0 ? uniqueCount : maxsize);
        assert(tableSize >= uniqueCount);
        tuplefreqtable = tuplehashtotable(pamP, tuplefreqhash, tableSize);
        pnm_destroytuplehash(tuplefreqhash);
        if (tuplefreqtable == NULL)
            pm_error("Out of memory generating tuple table");
    }
    *countP = uniqueCount;

    return tuplefreqtable;
}



tupletable
pnm_computetuplefreqtable(struct pam *   const pamP,
                          tuple **       const tupleArray,
                          unsigned int   const maxsize,
                          unsigned int * const sizeP) {

    return pnm_computetuplefreqtable2(pamP, tupleArray, maxsize, pamP->maxval,
                                      sizeP);
}



char*
pam_colorname(struct pam *         const pamP, 
              tuple                const color, 
              enum colornameFormat const format) {

    unsigned int r, g, b;
    FILE* f;
    static char colorname[200];

    r = pnm_scalesample(color[PAM_RED_PLANE], pamP->maxval, 255);
    g = pnm_scalesample(color[PAM_GRN_PLANE], pamP->maxval, 255);
    b = pnm_scalesample(color[PAM_BLU_PLANE], pamP->maxval, 255);

    f = pm_openColornameFile(NULL, format == PAM_COLORNAME_ENGLISH);
    if (f != NULL) {
        unsigned int best_diff;
        bool done;

        best_diff = 32767;
        done = FALSE;
        while (!done) {
            struct colorfile_entry const ce = pm_colorget(f);
            if (ce.colorname) {
                unsigned int const this_diff = 
                    abs((int)r - (int)ce.r) + 
                    abs((int)g - (int)ce.g) + 
                    abs((int)b - (int)ce.b);

                if (this_diff < best_diff) {
                    best_diff = this_diff;
                    strcpy(colorname, ce.colorname);
                }
            } else
                done = TRUE;
        }
        fclose(f);
        if (best_diff != 32767 && 
            (best_diff == 0 || format == PAM_COLORNAME_ENGLISH))
            return colorname;
    }

    /* Color lookup failed, but caller is willing to take an X11-style
       hex specifier, so return that.
    */
    sprintf(colorname, "#%02x%02x%02x", r, g, b);
    return colorname;
}
