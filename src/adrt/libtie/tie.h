/** addtogroup libtie */ /** @{ */
/** @file tie.h
 *
 */
/*
 * $Id$
 */

#ifndef TIE_H
#define TIE_H

#include "define.h"
#include "struct.h"

#ifdef __cplusplus
extern "C" {
#endif


void		tie_init(tie_t *tie, int tri_num);
void		tie_free(tie_t *tie);
void		tie_prep(tie_t *tie);
void*		tie_work(tie_t *tie, tie_ray_t *ray, tie_id_t *id,
			             void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr),
				     void *ptr);
void		tie_push(tie_t *tie, TIE_3 *tlist, int tnum, void *plist, int pstride);
extern tfloat	TIE_PREC;


#ifdef __cplusplus
}
#endif

#endif

/** @} */
