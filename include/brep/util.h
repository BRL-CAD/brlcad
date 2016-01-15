/*                      U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/** @addtogroup brep_util
 *
 * @brief
 * These are utility routines for libbrep, used throughout the library.
 *
 */
#ifndef BREP_UTIL_H
#define BREP_UTIL_H

#include "common.h"
#include "brep/defines.h"

/** @{ */
/** @file brep/util.h */

#ifdef __cplusplus

__BEGIN_DECLS

extern "C++" {

    BREP_EXPORT bool ON_NearZero(double x, double tolerance = ON_ZERO_TOLERANCE);


    extern BREP_EXPORT ON_BOOL32 face_GetBoundingBox(const ON_BrepFace &face,ON_BoundingBox& bbox,ON_BOOL32 bGrowBox);
    extern BREP_EXPORT ON_BOOL32 surface_GetBoundingBox(const ON_Surface *surf,const ON_Interval &u_interval,const ON_Interval &v_interval,ON_BoundingBox& bbox,ON_BOOL32 bGrowBox);
    extern BREP_EXPORT ON_BOOL32 surface_EvNormal(const ON_Surface *surf,double s,double t,ON_3dPoint& point,ON_3dVector& normal,int side=0,int* hint=0);

    extern BREP_EXPORT ON_Curve *interpolateCurve(ON_2dPointArray &samples);
    extern BREP_EXPORT ON_NurbsCurve *
	interpolateLocalCubicCurve(const ON_3dPointArray &Q);


    extern BREP_EXPORT void
	ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max);

    extern BREP_EXPORT int
	ON_Curve_PolyLine_Approx(ON_Polyline *polyline, const ON_Curve *curve, double tol);


    /* Experimental function to generate Tikz plotting information
     * from B-Rep objects.  This may or may not be something we
     * expose as a feature long term - probably should be a more
     * generic API that supports multiple formats... */
    extern BREP_EXPORT int
	ON_BrepTikz(ON_String &s, const ON_Brep *brep, const char *color, const char *prefix);

    /**
     * Get the curve segment between param a and param b
     *
     * @param in [in] the curve to split
     * @param a  [in] either a or b can be the larger one
     * @param b  [in] either a or b can be the larger one
     *
     * @return the result curve segment. NULL for error.
     */
    extern BREP_EXPORT ON_Curve *
	sub_curve(const ON_Curve *in, double a, double b);

    /**
     * Get the sub-surface whose u in [a,b] or v in [a, b]
     *
     * @param in [in] the surface to split
     * @param dir [in] 0: u-split, 1: v-split
     * @param a [in] either a or b can be the larger one
     * @param b [in] either a or b can be the larger one
     *
     * @return the result sub-surface. NULL for error.
     */
    extern BREP_EXPORT ON_Surface *
	sub_surface(const ON_Surface *in, int dir, double a, double b);


    BREP_EXPORT void set_key(struct bu_vls *key, int k, int *karray);

} /* extern C++ */

__END_DECLS

#endif  /* __cplusplus */

/** @} */

#endif  /* BREP_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
