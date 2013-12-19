/*                          D M - X . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2013 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-X.h
 *
 */

#ifndef DM_X_H
#define DM_X_H

#include "common.h"


#define CMAP_BASE 40
#define CUBE_DIMENSION 6
#define NUM_PIXELS 216    /* CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION */
#define ColormapNull (Colormap *)NULL

struct x_vars {
    GC gc;
    Pixmap pix;
    mat_t xmat;
    int is_trueColor;
    unsigned long bd, bg, fg;   /* color of border, background, foreground */
    unsigned long pixels[NUM_PIXELS];
};

#endif /* DM_X_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
