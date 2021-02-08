/*             S H A P E _ R E C O G N I T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file shape_recognition.h
 *
 * Internal header for shape recognition logic
 *
 */

#include "common.h"
#include "bu/ptbl.h"
#include "brep/defines.h"
#include "brep/csg.h"

#ifndef SHAPE_RECOGNITION_H
#define SHAPE_RECOGNITION_H

#define CSG_BREP_MAX_OBJS 1500

#define BREP_PLANAR_TOL 0.05
#define BREP_CYLINDRICAL_TOL 0.05
#define BREP_CONIC_TOL 0.05
#define BREP_SPHERICAL_TOL 0.05
#define BREP_ELLIPSOIDAL_TOL 0.05
#define BREP_TOROIDAL_TOL 0.05

#define pout(p) p.x << " " << p.y << " " << p.z

void set_to_array(int **array, int *array_cnt, std::set<int> *set);
void array_to_set(std::set<int> *set, int *array, int array_cnt);

#define ON_VMOVE(on, bn) { \
	on.x = bn[0]; \
	on.y = bn[1]; \
	on.z = bn[2]; \
    }
#define BN_VMOVE(bn, on) { \
	bn[0] = on.x; \
	bn[1] = on.y; \
	bn[2] = on.z; \
    }


typedef enum {
    SURFACE_PLANE = 0,
    SURFACE_CYLINDRICAL_SECTION,
    SURFACE_CYLINDER,
    SURFACE_CONE,
    SURFACE_SPHERICAL_SECTION,
    SURFACE_SPHERE,
    SURFACE_ELLIPSOIDAL_SECTION,
    SURFACE_ELLIPSOID,
    SURFACE_TORUS,
    //Insert any new types here
    SURFACE_GENERAL  /* A surface that does not fit in any of the previous categories */
} surface_t;

typedef enum {
    COMB = 0,  /* A comb is a boolean combination of other solids */
    ARB4,
    ARB5,
    ARB6,
    ARB7,
    ARB8,
    ARBN,
    PLANAR_VOLUME, /* NMG */
    CYLINDER,      /* RCC */
    CONE,          /* TRC */
    SPHERE,        /* SPH */
    ELLIPSOID,     /* ELL */
    TORUS,         /* TOR */
    //Insert any new types here
    BREP /* A brep is a complex solid that cannot be represented by CSG */
} volume_t;

surface_t GetSurfaceType(const ON_Surface *surface);
surface_t subbrep_highest_order_face(struct subbrep_island_data *data);
void subbrep_bbox(struct subbrep_island_data *obj);

void convex_plane_usage(ON_SimpleArray<ON_Plane> *planes, int **pu);
int island_nucleus(struct bu_vls *msgs, struct subbrep_island_data *data);

int subbrep_brep_boolean(struct subbrep_island_data *data);
int subbrep_make_brep(struct bu_vls *msgs, struct subbrep_island_data *data);

void subbrep_island_init(struct subbrep_island_data *obj, const ON_Brep *brep);
void subbrep_island_free(struct subbrep_island_data *obj);
void subbrep_shoal_init(struct subbrep_shoal_data *obj, struct subbrep_island_data *island);
void subbrep_shoal_free(struct subbrep_shoal_data *obj);
void csg_object_params_init(struct csg_object_params *obj, struct subbrep_shoal_data *shoal);
void csg_object_params_free(struct csg_object_params *obj);


int shoal_csg(struct bu_vls *msgs, surface_t surface_type, struct subbrep_shoal_data *data);

// Cylinder specific functionality
int cyl_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand);
int negative_cylinder(const ON_Brep *brep, int face_index, double cyl_tol);
int cyl_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cyl_planes);
int cyl_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *cyl_planes, int implicit_plane_ind, int ndc, int *nde, int shoal_nonplanar_face, int nonlinear_edge);

// Cone specific functionality
int cone_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand);
int negative_cone(const ON_Brep *brep, int face_index, double cyl_tol);
int cone_implicit_plane(const ON_Brep *brep, int lc, int *le, ON_SimpleArray<ON_Plane> *cyl_planes);
int cone_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *cyl_planes, int implicit_plane_ind, int ndc, int *nde, int shoal_nonplanar_face, int nonlinear_edge);

// Sphere specific functionality
int sph_validate_face(const ON_BrepFace *forig, const ON_BrepFace *fcand);
int negative_sphere(const ON_Brep *brep, int face_index, double cyl_tol);
int sph_implicit_plane(const ON_Brep *brep, int ec, int *edges, ON_SimpleArray<ON_Plane> *sph_planes);
int sph_implicit_params(struct subbrep_shoal_data *data, ON_SimpleArray<ON_Plane> *sph_planes, int shoal_nonplanar_face);


#endif /* SHAPE_RECOGNITION_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
