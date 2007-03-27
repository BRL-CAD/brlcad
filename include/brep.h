/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
 * 	Non Rational Uniform B-Spline (NURBS)
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
extern "C++" {
/* XXX ack. a hack. */
#undef X
#undef Y
#undef Z
#undef W
#undef H
#include "opennurbs.h"
}
extern "C" {
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"

#ifndef __cplusplus
typedef struct _on_brep_placeholder {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} ON_Brep;
#endif

    
    /* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
    /* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.8
    /* Maximum number of newton iterations on root finding */
#define BREP_MAX_ITERATIONS 10
    /* Root finding threshold */
#define BREP_INTERSECTION_ROOT_EPSILON 0.00001
    /* Use vector operations? For debugging */
#define DO_VECTOR 1
    

#ifndef __cplusplus
typedef struct _bounding_volume_placeholder {
    int dummy;
} BrepBoundingVolume;
#else 
namespace brep {
    class BoundingVolume;
};
typedef class brep::BoundingVolume BrepBoundingVolume;
#endif

typedef struct _brep_cdbitem {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} brep_cdbitem;

/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    ON_Brep* brep;
    BrepBoundingVolume* bvh;
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
