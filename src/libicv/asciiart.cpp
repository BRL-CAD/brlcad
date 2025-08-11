/*                    A S C I I A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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
/** @file libicv/asciiart.cpp
 *
 * Express an image as ASCII characters.
 */

#include "common.h"

#define PIXCII_IMPLEMENTATION
#include "pixcii.hpp"

#include "vmath.h"
#include "bu/str.h"
#include "icv.h"

extern "C" char *
icv_ascii_art(icv_image_t *img, struct icv_ascii_art_params *p)
{
    if (!img)
	return NULL;

    pixcii::AsciiArtParams ap;

    if (p) {
	ap.color = (p->output_color) ? true : false;
	ap.invert_color = (p->invert_color) ? true : false;
	ap.brightness_boost = p->brightness_multiplier;
    }

    std::string txt_art = pixcii::generateAsciiText(img, ap);
    char *out = bu_strdup(txt_art.c_str());
    return out;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
