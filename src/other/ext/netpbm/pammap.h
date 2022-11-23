/*=============================================================================
                                pammap.h
===============================================================================

  Interface header file for hash table and lookup table pam functions
  in libpnm.

=============================================================================*/

#ifndef PAMMAP_H
#define PAMMAP_H
#include "colorname.h"
#include "pam.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

struct tupleint {
    /* An ordered pair of a tuple value and an integer, such as you 
       would find in a tuple table or tuple hash.

       Note that this is a variable length structure.
    */
    int value;
    sample tuple[1];  
        /* This is actually a variable size array -- its size is the 
           depth of the tuple in question.  Some compilers do not let us
           declare a variable length array.
        */
};
typedef struct tupleint ** tupletable;

typedef struct {
    unsigned int size;
    tupletable table;
} tupletable2;

struct tupleint_list_item {
    struct tupleint_list_item * next;
    struct tupleint tupleint;
};
typedef struct tupleint_list_item * tupleint_list;

typedef tupleint_list * tuplehash;

unsigned int
pnm_hashtuple(struct pam * const pamP, tuple const tuple);

void
pnm_addtotuplehash(struct pam *   const pamP,
                   tuplehash      const tuplehash, 
                   tuple          const tuple,
                   int            const value,
                   int *          const fitsP);

void
pnm_addtuplefreqoccurrence(struct pam *   const pamP,
                           tuple          const value, 
                           tuplehash      const tuplefreqhash,
                           int *          const firstOccurrenceP);

void
pnm_lookuptuple(struct pam * const pamP, const tuplehash tuplehash, 
                const tuple searchval, 
                int * const foundP, int * const retvalP);

tupletable
pnm_alloctupletable(const struct pam * const pamP, unsigned int const size);

void
pnm_freetupletable(const struct pam * const pamP,
                   tupletable         const tupletable);

void
pnm_freetupletable2(const struct pam * const pamP,
                    tupletable2        const tupletable);

tuplehash
pnm_createtuplehash(void);

void
pnm_destroytuplehash(tuplehash const tuplehash);

tupletable
pnm_computetuplefreqtable(struct pam *   const pamP,
                          tuple **       const tupleArray,
                          unsigned int   const maxsize,
                          unsigned int * const sizeP);

tupletable
pnm_computetuplefreqtable2(struct pam *   const pamP,
                           tuple **       const tupleArray,
                           unsigned int   const maxsize,
                           sample         const newMaxval,
                           unsigned int * const sizeP);

tupletable
pnm_computetuplefreqtable3(struct pam *   const pamP,
                           tuple **       const tupleArray,
                           unsigned int   const maxsize,
                           unsigned int   const newDepth,
                           sample         const newMaxval,
                           unsigned int * const countP);

tuplehash
pnm_computetuplefreqhash(struct pam *   const pamP,
                         tuple **       const tupleArray,
                         unsigned int   const maxsize,
                         unsigned int * const sizeP);

tuplehash
pnm_computetupletablehash(struct pam * const pamP, 
                          tupletable   const tupletable,
                          unsigned int const tupletableSize);

tupletable
pnm_tuplehashtotable(const struct pam * const pamP,
                     tuplehash          const tuplehash,
                     unsigned int       const maxsize);

char*
pam_colorname(struct pam *         const pamP, 
              tuple                const color, 
              enum colornameFormat const format);

#ifdef __cplusplus
}
#endif

#endif
