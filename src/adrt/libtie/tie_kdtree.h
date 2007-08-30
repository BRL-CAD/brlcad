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


void tie_kdtree_free(tie_t *tie);
uint32_t tie_kdtree_cache_free(tie_t *tie, void **cache);
void tie_kdtree_cache_load(tie_t *tie, void *cache, uint32_t size);
void tie_kdtree_prep(tie_t *tie);
extern tfloat TIE_PREC;


#ifdef __cplusplus
}
#endif

#endif
