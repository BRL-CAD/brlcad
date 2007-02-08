/*                     T E X T U R E _ P E R L I N . H
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
/** @file texture_perlin.h
 *
 *  Comments -
 *      Texture Library - Perlin Utility Header
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

#ifndef _TEXTURE_PERLIN_H
#define _TEXTURE_PERLIN_H

#include "texture_internal.h"

typedef struct texture_perlin_s {
  int *PV;
  TIE_3 *RV;
} texture_perlin_t;

extern	void	texture_perlin_init(texture_perlin_t *P);
extern	void	texture_perlin_free(texture_perlin_t *P);
extern	TFLOAT	texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, TFLOAT Size, int Depth);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
