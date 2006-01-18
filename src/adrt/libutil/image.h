/*                     I M A G E . H
 *
 * @file image.h
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
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

void	util_image_load_ppm(char *filename, void *image, int *w, int *h);
void	util_image_save_ppm(char *filename, void *image, int w, int h);

void	util_image_load_raw(char *filename, void *image, int *w, int *h);
void	util_image_save_raw(char *filename, void *image, int w, int h);

void	util_image_convert_128to24(void *image24, void *image128, int w, int h);
void	util_image_convert_32to24(void *image24, void *image32, int w, int h, int endian);

extern tfloat	*rise_image_raw;

#endif
