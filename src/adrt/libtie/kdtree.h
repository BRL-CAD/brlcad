/*                        K D T R E E . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** addtogroup libtie */
/** @{ */
/** @file kdtree.h
 *
 *  Comments -
 *      KD-Tree Routines
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

#ifndef _KDTREE_H
#define _KDTREE_H

#include "define.h"
#include "struct.h"

#ifdef __cplusplus
extern "C" {
#endif


void tie_kdtree_free(tie_t *tie);
void tie_kdtree_cache_free(tie_t *tie, void **cache);
void tie_kdtree_cache_load(tie_t *tie, void *cache);
void tie_kdtree_prep(tie_t *tie);
extern TFLOAT TIE_PREC;


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
