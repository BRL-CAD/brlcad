/*                     I M A G E . H
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
/** @file image.h
 *
 *  Comments -
 *      Utilities Library - Image Header
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

#ifndef _UTIL_IMAGE_H
#define _UTIL_IMAGE_H


#include "tie.h"


void	util_image_init();
void	util_image_free();

void	util_image_load_ppm(const char* filename, void* image, int* w, int* h);
void	util_image_save_ppm(const char* filename, const void* image, const int w, const int h);

void	util_image_load_raw(const char* filename, void *image, int* w, int* h);
void	util_image_save_raw(const char* filename, const void* image, const int w, const int h);

void	util_image_convert_128to24(void* image24, const void* image128, const int w, const int h);
void	util_image_convert_32to24(void* image24, const void* image32, const int w, const int h, const int endian);

extern tfloat	*rise_image_raw;

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
