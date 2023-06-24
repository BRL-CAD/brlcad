/*                            E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2023 United States Government as represented by
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
 * create an empty ON_Brep. return pointer to the ON_Brep object created in void* format
 */
BREP_EXPORT extern void *
brep_create();

__END_DECLS

/// function below are C++ interface
#ifdef __cplusplus
extern "C++"
{
#include <vector>

    /**
     * create an nurbs curve using a template.
     * position can be specified if position != NULL
     * return id of the curve
     */
    BREP_EXPORT extern int
    brep_make_curve(ON_Brep *brep, const ON_3dPoint &position);

    /**
     * create an nurbs curve given detailed information
     * return id of the curve
     */
    BREP_EXPORT extern int
    brep_in_curve(ON_Brep *brep, bool is_rational, int order, int cv_count, std::vector<ON_4dPoint> cv);

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
     * Reverse parameterizatrion by negating all knots
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
     * join two curves. Join the end of curve_id_1 to the start of curve_id_2.
     * return id of the new curve, delete the two old curves.
     * Remark: the index of C3 is changed after join!!!
     */
    BREP_EXPORT extern int
    brep_curve_join(ON_Brep *brep, int curve_id_1, int curve_id_2);

    /**
     * create an nurbs curve using a template
     * position can be specified if argc == 3
     * return id of the surface
     */
    BREP_EXPORT extern int
    brep_surface_make(ON_Brep *brep, const ON_3dPoint &position);

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
     * create a ruled surface.
     * The two curves must have the same NURBS form knots.
     * srf(s,t) = (1.0-t)*curveA(s) + t*curveB(s).
     * return: if successful, id of the new surface; otherwise, -1.
     */

    BREP_EXPORT extern int
    brep_surface_create_ruled(ON_Brep *brep, int curve_id0, int curve_id1);
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
