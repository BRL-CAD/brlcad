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


void		tie_init(tie_t *tie, unsigned int tri_num);
void		tie_free(tie_t *tie);
void		tie_prep(tie_t *tie);
void*		tie_work(tie_t *tie, tie_ray_t *ray, tie_id_t *id,
			             void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr),
				     void *ptr);
void		tie_push(tie_t *tie, TIE_3 **tlist, int tnum, void *plist, int pstride);


#ifdef __cplusplus
}
#endif

#endif
