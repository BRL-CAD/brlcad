/*                      T R I _ P T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/* @file tri_pt.h */
/** @addtogroup bg_tri_pt */
/** @{ */

/**
 * @brief
 *
 * Calculate the minimum distance from a point to a triangle
 *
 */

#ifndef BG_TRI_PT_H
#define BG_TRI_PT_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/* The parameter closest_pt is optional = if supplied, it will hold the closest
 * point found.  The return value is the distance between TP and the closest_pt
 * to TP on the triangle. */
BG_EXPORT extern double bg_tri_closest_pt(
	point_t *closest_pt,
	const point_t TP,
	const point_t V0,
	const point_t V1,
	const point_t V2
	);

__END_DECLS

#endif  /* BG_TRI_TRI_H */
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
