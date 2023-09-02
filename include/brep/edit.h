/*                            E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @addtogroup brep_edit
 * @brief
 * Implementation of edit support for brep.
 * C functions and Cpp functions are provided.
 */
#ifndef BREP_EDIT_H
#define BREP_EDIT_H

#include "common.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/edit.h */

/// function below are C interface
__BEGIN_DECLS

/**
 * create an empty ON_Brep. return pointer to the ON_Brep object
 */
BREP_EXPORT extern void *
brep_create(void);

__END_DECLS

/// function below are C++ interface
#ifdef __cplusplus
extern "C++"
{
#include <vector>

    /**
     * create a brep vertex
     */
    BREP_EXPORT extern int
    brep_vertex_create(ON_Brep *brep, ON_3dPoint point);

    /**
     * remove a brep vertex
     */
    BREP_EXPORT extern bool
    brep_vertex_remove(ON_Brep *brep, int v_id);

    /**
     * create a 2D parameter space geometric line
     * return id of the curve2d
     */
    BREP_EXPORT extern int
    brep_curve2d_make_line(ON_Brep *brep, const ON_2dPoint &start, const ON_2dPoint &end);

    /**
     * remove a curve2d from brep
     */
    BREP_EXPORT extern bool 
    brep_curve2d_remove(ON_Brep *brep, int curve_id);

    /**
     * create a nurbs curve using a template.
     * position can be specified if position != NULL
     * return id of the curve
     */
    BREP_EXPORT extern int
    brep_curve_make(ON_Brep *brep, const ON_3dPoint &position);

    /**
     * create a nurbs curve given detailed information
     * return id of the curve
     */
    BREP_EXPORT extern int
    brep_curve_in(ON_Brep *brep, bool is_rational, int order, int cv_count, std::vector<ON_4dPoint> cv);

    /**
     * create a cubic nurbs curve by interpolating a set of points
     * return id of the curve
     * method: Local cubic interpolation with C1 continuity
     * reference: The NURBS Book (2nd Edition), chapter 9.3.4
     */
    BREP_EXPORT extern int
    brep_curve_interpCrv(ON_Brep *brep, std::vector<ON_3dPoint> points);

    /**
     * copy a curve from brep
     * return id of the new curve
     */
    BREP_EXPORT extern int
    brep_curve_copy(ON_Brep *brep, int curve_id);

    /**
     * remove a curve from brep
     * @attention the index of m_C3 is changed after remove!!!
     */
    BREP_EXPORT extern bool
    brep_curve_remove(ON_Brep *brep, int curve_id);

    /**
     * move curve along a vector.
     */
    BREP_EXPORT extern bool
    brep_curve_move(ON_Brep *brep, int curve_id, const ON_3dVector &point);

    /**
     * set control vertex of a curve
     */
    BREP_EXPORT extern bool
    brep_curve_set_cv(ON_Brep *brep, int curve_id, int cv_id, const ON_4dPoint &point);

    /**
     * reverse parameterizatrion by negating all knots
     * and reversing the order of the control vertices.
     */
    BREP_EXPORT extern bool
    brep_curve_reverse(ON_Brep *brep, int curve_id);

    /**
     * insert knots into a curve
     */
    BREP_EXPORT extern bool
    brep_curve_insert_knot(ON_Brep *brep, int curve_id, double knot, int multiplicity);

    /**
     * trim a curve using a parameter range
     */
    BREP_EXPORT extern bool
    brep_curve_trim(ON_Brep *brep, int curve_id, double t0, double t1);

    /**
     * split a curve at a parameter. Old curve will be deleted.
     */
    BREP_EXPORT extern bool
    brep_curve_split(ON_Brep *brep, int curve_id, double t);

    /**
     * join the end of curve_id_1 to the start of curve_id_2.
     * return id of the new curve, delete the two old curves.
     * @attention the index of m_C3 is changed after join!!!
     */
    BREP_EXPORT extern int
    brep_curve_join(ON_Brep *brep, int curve_id_1, int curve_id_2);
    
    /**
     * create a nurbs curve using a template
     * position can be specified if argc == 3
     * return id of the surface
     */
    BREP_EXPORT extern int
    brep_surface_make(ON_Brep *brep, const ON_3dPoint &position);

    /**
     * extract a vertex from a surface
     */
    BREP_EXPORT extern int
    brep_surface_extract_vertex(ON_Brep *brep, int surface_id, double u, double v);

    /**
     * extract a curve from a surface
     */
    BREP_EXPORT extern int
    brep_surface_extract_curve(ON_Brep *brep, int surface_id, int dir, double t);

    /**
     * create a bicubic nurbs surface by interpolating a set of points
     * return id of the surface
     * method: Global cubic interpolation with C2 continuity
     * reference: The NURBS Book (2nd Edition), chapter 9.2.5
     */
    BREP_EXPORT extern int
    brep_surface_interpCrv(ON_Brep *brep, int cv_count_x, int cv_count_y, std::vector<ON_3dPoint> points);

    /**
     * copy a surface from brep
     * return id of the new surface
     */
    BREP_EXPORT extern int
    brep_surface_copy(ON_Brep *brep, int surface_id);

    /**
     * move surface to a new position
     */
    BREP_EXPORT extern bool
    brep_surface_move(ON_Brep *brep, int surface_id, const ON_3dVector &point);

    /**
     * set control vertex of a curve
     */
    BREP_EXPORT extern bool
    brep_surface_set_cv(ON_Brep *brep, int surface_id, int cv_id_u, int cv_id_v, const ON_4dPoint &point);

    /**
     * trim a surface using a parameter range
     * dir = 0: u direction
     * dir = 1: v direction
     */
    BREP_EXPORT extern bool
    brep_surface_trim(ON_Brep *brep, int surface_id, int dir, double t0, double t1);

    /**
     * split a surface at a parameter. Old surface will be deleted.
     */
    BREP_EXPORT extern bool
    brep_surface_split(ON_Brep *brep, int surface_id, int dir, double t);

    /**
     * create a ruled surface.
     * The two curves must have the same NURBS form knots.
     * srf(s,t) = (1.0-t)*curveA(s) + t*curveB(s).
     * return: if successful, id of the new surface; otherwise, -1.
     */
    BREP_EXPORT extern int
    brep_surface_create_ruled(ON_Brep *brep, int curve_id0, int curve_id1);

    /**
     * create a surface by extruding a curve along another curve.
     * return: if successful, id of the new surface; otherwise, -1.
     */
    BREP_EXPORT extern int
    brep_surface_tensor_product(ON_Brep *brep, int curve_id0, int curve_id1);
    
    /**
     * create a surface by rotating a curve around an axis.
     * return: if successful, id of the new surface; otherwise, -1.
     */
    BREP_EXPORT extern int
    brep_surface_revolution(ON_Brep *brep, int curve_id0, ON_3dPoint line_start, ON_3dPoint line_end, double angle = 2 * ON_PI);

    /**
     * remove a surface from brep
     * @attention the index of m_S is changed after remove!!!
     */
    BREP_EXPORT extern bool
    brep_surface_remove(ON_Brep *brep, int surface_id);

    /**
     * create a brep edge
     */
    BREP_EXPORT extern int
    brep_edge_create(ON_Brep *brep, int from, int to, int curve);

    /**
     * create a brep face
     */
    BREP_EXPORT extern int
    brep_face_create(ON_Brep *brep, int surface);

    /**
     * reverse a brep face
     */
    BREP_EXPORT extern bool
    brep_face_reverse(ON_Brep *brep, int face);

    /**
     * create a brep face loop
     */
    BREP_EXPORT extern int
    brep_loop_create(ON_Brep *brep, int face_id);

    /**
     * create a trim of a loop
     */
    BREP_EXPORT extern int
    brep_trim_create(ON_Brep *brep, int loop_id, int edge_id, int orientation, int para_curve_id);
} /* extern C++ */
#endif

#endif /* BREP_EDIT_H */
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
