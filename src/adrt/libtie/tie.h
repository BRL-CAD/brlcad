/*
 *  tie.h
 *
 *  RCSid:          $Id$
*/

#ifndef _TIE_H
#define _TIE_H

#include "tie_define.h"
#include "tie_struct.h"
#include "tie_kdtree.h"

#ifdef __cplusplus
extern "C" {
#endif

TIE_FUNC(void tie_init, tie_t *tie, unsigned int tri_num, int kdmethod);
TIE_FUNC(void tie_free, tie_t *tie);
TIE_FUNC(void tie_prep, tie_t *tie);
TIE_FUNC(void* tie_work, tie_t *tie, tie_ray_t *ray, tie_id_t *id, void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr), void *ptr);
TIE_FUNC(void tie_push, tie_t *tie, TIE_3 **tlist, int tnum, void *plist, int pstride);

/* backwards compatible macros */
#define tie_init TIE_VAL(tie_init)
#define tie_free TIE_VAL(tie_free)
#define tie_prep TIE_VAL(tie_prep)
#define tie_work TIE_VAL(tie_work)
#define tie_push TIE_VAL(tie_push)

#ifdef __cplusplus
}
#endif

#endif
