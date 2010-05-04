/*                    T I E _ K D T R E E . H
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
/** @file tie_kdtree.h
 *
 * Brief description
 *
 */

#ifndef _TIE_KDTREE_H
#define _TIE_KDTREE_H

#include "tie_define.h"
#include "tie_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

void TIE_VAL(tie_kdtree_free)(tie_t *tie);
uint32_t TIE_VAL(tie_kdtree_cache_free)(tie_t *tie, void **cache);
void TIE_VAL(tie_kdtree_cache_load)(tie_t *tie, void *cache, uint32_t size);
void TIE_VAL(tie_kdtree_prep)(tie_t *tie);
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

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
