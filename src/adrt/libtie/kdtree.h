/*                     T I E . H
 *
 * @file kdtree.h
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
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
/** addtogroup libtie */ /** @{ */

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
extern tfloat TIE_PREC;


#ifdef __cplusplus
}
#endif

#endif

/** @} */
