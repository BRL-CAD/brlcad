/*                    B R E P _ L O C A L . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file brep_local.h
 *
 * Local data structures for brep primitive
 */

#ifndef LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H
#define LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H

#include "bu/vls.h"
#include "raytrace.h"
#include "rtgeom.h"

/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    ON_Brep* brep;
    BrepBoundingVolume* bvh;
};

#ifdef __cplusplus
RT_EXPORT extern int brep_surface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres);
RT_EXPORT extern int brep_surface_uv_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, double u, double v);
RT_EXPORT extern int brep_isosurface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres);
RT_EXPORT extern int brep_surface_normal_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres);
RT_EXPORT extern  int brep_surface_knot_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, bool dim3d);
RT_EXPORT extern  int brep_facetrim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres, bool dim3d);
RT_EXPORT extern int brep_surfaceleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, bool dim3d, int index, int);
RT_EXPORT extern  int brep_trim_direction_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres);
RT_EXPORT extern  int brep_trim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres, bool dim3d);
RT_EXPORT extern int brep_loop_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres, bool dim3d);
RT_EXPORT extern int brep_trimleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, bool dim3d, int index, int);
RT_EXPORT extern int brep_edge3d_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres);
RT_EXPORT extern int brep_surface_cv_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index);
#endif

#ifdef __cplusplus
extern "C" {
#endif
    RT_EXPORT extern int single_conversion(struct rt_db_internal* intern, ON_Brep** brep, const struct db_i *dbip);
    RT_EXPORT extern int brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip);
    RT_EXPORT extern int brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm);
    RT_EXPORT extern int brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bn_vlblock *vbp);
    extern void rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct db_i *dbip);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
int brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
	const struct rt_tess_tol *ttol, const struct bn_tol *tol,
	struct brep_specific* bs, struct rt_brep_internal*UNUSED(bi),
	struct bn_vlblock *vbp, int index, int plottype, int num_points = -1);
int brep_surface_uv_plot_interval(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, ON_Interval &U, ON_Interval &V);
void plotsurface(ON_Surface &surf, struct bn_vlblock *vbp, int isocurveres, int gridres, const int red = 200, const int green = 200, const int blue = 200);
void plotcurveonsurface(ON_Curve *curve,
	ON_Surface *surface,
	struct bn_vlblock *vbp,
	int plotres,
	const int red = 255,
	const int green = 255,
	const int blue = 0);
void plotcurve(ON_Curve &curve, struct bn_vlblock *vbp, int plotres, const int red = 255, const int green = 255, const int blue = 0);

void plotpoint(const ON_3dPoint &point, struct bn_vlblock *vbp, const int red = 255, const int green = 255, const int blue = 0);
#endif

#endif /* LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
