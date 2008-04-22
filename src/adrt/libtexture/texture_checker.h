/*                     T E X T U R E _ C H E C K E R . H
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
/** @file texture_checker.h
 *
 *  Comments -
 *      Texture Library - Checker Header
 *
 */

#ifndef TEXTURE_CHECKER_H
#define TEXTURE_CHECKER_H


#include "texture.h"


typedef struct texture_checker_s {
    int tile;
} texture_checker_t;


extern	void	texture_checker_init(texture_t *texture, int checker);
extern	void	texture_checker_free(texture_t *texture);
extern	void	texture_checker_work(__TEXTURE_WORK_PROTOTYPE__);


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
