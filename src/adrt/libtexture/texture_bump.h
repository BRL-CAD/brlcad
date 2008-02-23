/*                     T E X T U R E _ B U M P . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2008 United States Government as represented by
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
/** @file texture_bump.h
 *
 *  Comments -
 *      Texture Library - Bump Mapping Header
 *
 */

#ifndef _TEXTURE_BUMP_H
#define _TEXTURE_BUMP_H


#include "texture_internal.h"


typedef struct texture_bump_s {
    TIE_3 coef;
} texture_bump_t;


extern	void	texture_bump_init(texture_t *texture, TIE_3 rgb);
extern	void	texture_bump_free(texture_t *texture);
extern	void	texture_bump_work(__TEXTURE_WORK_PROTOTYPE__);


#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
