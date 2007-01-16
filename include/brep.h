/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nurb */
/*@{*/
/** @file on_nurb.h
 *
 * @brief
 *	Define surface and curve structures for
 * 	Non Rational Uniform B-Spline (NURB)
 *	curves and surfaces. Uses openNURBS library.
 *
 * @author	Jason Owens
 *
 * @par Source
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *@n	The U.S. Army Ballistic Research Laboratory
 *@n 	Aberdeen Proving Ground, Maryland 21005
 *
 *  $Header$
 */

#ifndef BREP_H
#define BREP_H

#ifdef __cplusplus
#include "opennurbs.h"
#else
typedef struct _on_brep_placeholder {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} ON_Brep;
#endif 

#ifdef __cplusplus
extern "C" {
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"

/**
 * Bounding volume used as an acceleration data structure. It's
 * implemented here as an axis-aligned bounding box containing the
 * parametric bounds of the surface enclosed by the box.
 */
typedef struct _brep_hbv { /* b-rep hierarchical bounding volume */
    struct bu_list l;
    point_t min;
    point_t max;
    fastf_t umin, umax, vmin, vmax;
    struct bu_list children;
} brep_hbv;

typedef struct _brep_cdbitem {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} brep_cdbitem;

/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    brep_hbv* hbv;
};

#ifdef __cplusplus
}
#endif

#endif  /* BREP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
