/*                 P O L Y G O N _ T Y P E S. H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/* @file polygon_types.h */
/** @addtogroup bg_polygon */
/** @{ */

/**
 *  @brief Functions for working with polygons
 */

#ifndef BG_POLYGON_TYPES_H
#define BG_POLYGON_TYPES_H

#include "common.h"
#include "vmath.h"

__BEGIN_DECLS

/* The following data types are originally from bview - we keep them
 * separate here to maximize their ease of reuse */
typedef enum { bg_Union, bg_Difference, bg_Intersection, bg_Xor } bg_clip_t;

struct bg_poly_contour {
    size_t    num_points;
    point_t   *point;               /* in model coordinates */
    int       closed;
};

struct bg_polygon {
    size_t                  num_contours;
    int                     *hole;
    struct bg_poly_contour  *contour;

    // TODO - in principle these shouldn't be here, but the libtclcad code uses
    // them so it will take some thought to refactor them up from this
    // container...
    int                 gp_color[3];
    int                 gp_line_width;          /* in pixels */
    int                 gp_line_style;
};

#define BG_POLYGON_NULL {0, NULL, NULL, {0, 0, 0}, 0, 0}

struct bg_polygons {
    size_t            num_polygons;
    struct bg_polygon *polygon;
};

__END_DECLS

#endif  /* BG_POLYGON_TYPES_H */
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
