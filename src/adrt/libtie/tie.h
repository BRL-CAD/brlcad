/*                     T I E . H
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tie.h
 *
 *  Comments -
 *      Triangle Intersection Engine Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */
/** addtogroup libtie */ /** @{ */

#ifndef _TIE_H
#define _TIE_H

#include "define.h"
#include "struct.h"
#include "kdtree.h"

#ifdef __cplusplus
extern "C" {
#endif


void		tie_init(tie_t *tie, unsigned int tri_num);
void		tie_free(tie_t *tie);
void		tie_prep(tie_t *tie);
void*		tie_work(tie_t *tie, tie_ray_t *ray, tie_id_t *id,
			             void *(*hitfunc)(tie_ray_t*, tie_id_t*, tie_tri_t*, void *ptr),
				     void *ptr);
void		tie_push(tie_t *tie, TIE_3 *tlist, int tnum, void *plist, int pstride);


#ifdef __cplusplus
}
#endif

#endif

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
