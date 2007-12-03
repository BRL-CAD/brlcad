/*
 *  tie_kdtree.h
 *
 *  RCSid:          $Id$
*/

#ifndef _TIE_KDTREE_H
#define _TIE_KDTREE_H

#include "tie_define.h"
#include "tie_struct.h"

#ifdef __cplusplus
extern "C" {
#endif


TIE_FUNC(void tie_kdtree_free, tie_t *tie);
TIE_FUNC(uint32_t tie_kdtree_cache_free, tie_t *tie, void **cache);
TIE_FUNC(void tie_kdtree_cache_load, tie_t *tie, void *cache, uint32_t size);
TIE_FUNC(void tie_kdtree_prep, tie_t *tie);
TIE_VAL(extern tfloat TIE_PREC);

/* compatability macros */
#define tie_kdtree_free TIE_VAL(tie_kdtree_free)
#define tie_kdtree_cache_free TIE_VAL(tie_kdtree_cache_free)
#define tie_kdtree_cache_load TIE_VAL(tie_kdtree_cache_load)
#define tie_kdtree_prep TIE_VAL(tie_kdtree_prep)
#define TIE_PREC TIE_VAL(TIE_PREC)

#ifdef __cplusplus
}
#endif

#endif
