/*                       B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file brep.h
 *
 * Define surface and curve structures for Non Rational Uniform
 * B-Spline (NURBS) curves and surfaces. Uses openNURBS library.
 *
 */

#ifndef BREP_H
#define BREP_H

#include "common.h"

#ifdef __cplusplus
extern "C++" {
#include "opennurbs.h"
#include <iostream>
#include <fstream>

    namespace brlcad {
	template <class T>
	class BVNode;

	typedef class BVNode<ON_BoundingBox> BBNode;
    }
}


__BEGIN_DECLS

#endif

#include "vmath.h"
#include "bu.h"


#ifndef __cplusplus
typedef struct _on_brep_placeholder {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} ON_Brep;
#endif


/** Maximum number of newton iterations on root finding */
#define BREP_MAX_ITERATIONS 100
/** Root finding threshold */
#define BREP_INTERSECTION_ROOT_EPSILON 1e-6
/* if threshold not reached what will we settle for close enough */
#define BREP_INTERSECTION_ROOT_SETTLE 1e-2
/** Jungle Gym epsilon */

/* Use vector operations? For debugging */
#define DO_VECTOR 1


#ifndef __cplusplus
typedef struct _bounding_volume_placeholder {
    int dummy;
} BrepBoundingVolume;
#else
/* namespace brlcad { */
/*     class BBNode; */
/* }; */
typedef brlcad::BBNode BrepBoundingVolume;
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


__END_DECLS

#endif  /* BREP_H */
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
