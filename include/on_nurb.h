/*                       O N _ N U R B . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2010 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file on_nurb.h
 *
 * @brief
 *	Define surface and curve structures for
 * 	Non Rational Uniform B-Spline (NURB)
 *	curves and surfaces. Uses openNURBS library.
 *
 * @author	Jason Owens
 *
 */

#ifndef ON_NURB_H
#define ON_NURB_H

#include "opennurbs.h"
#include "bu.h"

/**
 * Bounding volume used as an acceleration data structure. It's
 * implemented here as an axis-aligned bounding box containing the
 * parametric bounds of the surface enclosed by the box.
 */
struct on_nurbs_hbv {
    struct bu_list l;
    point_t min;
    point_t max;
    fastf_t umin, umax, vmin, vmax;
    struct bu_list children;
};

#endif  /* ON_NURB_H */
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
