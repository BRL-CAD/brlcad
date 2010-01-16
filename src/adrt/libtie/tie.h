/*                           T I E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tie.h
 *
 * Brief description
 *
 */

#ifndef _TIE_H
#define _TIE_H

#include "tie_define.h"
#include "tie_struct.h"
#include "tie_kdtree.h"

#ifdef __cplusplus
extern "C" {
#endif

    TIE_VAL(extern int tie_check_degenerate);
#define tie_check_degenerate TIE_VAL(tie_check_degenerate)

    TIE_FUNC(void tie_init, tie_t *tie, unsigned int tri_num, unsigned int kdmethod);
    TIE_FUNC(void tie_free, tie_t *tie);
    TIE_FUNC(void tie_prep, tie_t *tie);
    TIE_FUNC(void* tie_work, tie_t *tie, tie_ray_t *ray, tie_id_t *id, void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr), void *ptr);
    TIE_FUNC(void tie_push, tie_t *tie, TIE_3 **tlist, unsigned int tnum, void *plist, unsigned int pstride);

/* backwards compatible macros */
#define tie_init TIE_VAL(tie_init)
#define tie_free TIE_VAL(tie_free)
#define tie_prep TIE_VAL(tie_prep)
#define tie_work TIE_VAL(tie_work)
#define tie_push TIE_VAL(tie_push)

#ifdef __cplusplus
}
#endif

#endif /* _TIE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
