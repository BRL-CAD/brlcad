/*                    E L L _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file ell_brep.cpp
 *
 * Convert a Generalized Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"


static bool
machine_equal_radius(double first, double second)
{
    if (!std::isfinite(first) || !std::isfinite(second) ||
	!(first > 0.0) || !(second > 0.0))
    {
	return false;
    }

    const double scale = std::max(first, second);
    const double tolerance = 32.0 * std::numeric_limits<double>::epsilon() *
	scale;
    return std::fabs(first - second) <= tolerance;
}


/* Preserve spheroids as surfaces of revolution.  The general NURBS surface
 * represents the same quadric, but retaining this semantic form lets the
 * planar/revolution intersection path construct complete section curves
 * directly at very large coordinate scales. */
static bool
make_spheroid_brep(ON_Brep **b, const struct rt_ell_internal *eip,
	double radius_a, double radius_b, double radius_c,
	fastf_t *maximum_approximation)
{
    const fastf_t *first_axis = NULL;
    const fastf_t *second_axis = NULL;
    const fastf_t *polar_axis = NULL;
    double first_radius = 0.0;
    double second_radius = 0.0;
    double polar_radius = 0.0;

    if (machine_equal_radius(radius_a, radius_b)) {
	first_axis = eip->a;
	second_axis = eip->b;
	polar_axis = eip->c;
	first_radius = radius_a;
	second_radius = radius_b;
	polar_radius = radius_c;
    } else if (machine_equal_radius(radius_a, radius_c)) {
	first_axis = eip->a;
	second_axis = eip->c;
	polar_axis = eip->b;
	first_radius = radius_a;
	second_radius = radius_c;
	polar_radius = radius_b;
    } else if (machine_equal_radius(radius_b, radius_c)) {
	first_axis = eip->b;
	second_axis = eip->c;
	polar_axis = eip->a;
	first_radius = radius_b;
	second_radius = radius_c;
	polar_radius = radius_a;
    } else {
	return false;
    }

    ON_3dVector xaxis(first_axis);
    ON_3dVector yaxis(second_axis);
    ON_3dVector source_polar(polar_axis);
    if (!xaxis.Unitize() || !yaxis.Unitize() || !source_polar.Unitize())
	return false;

    const double angular_tolerance = 32.0 *
	std::numeric_limits<double>::epsilon();
    if (std::fabs(xaxis * yaxis) > angular_tolerance ||
	std::fabs(xaxis * source_polar) > angular_tolerance ||
	std::fabs(yaxis * source_polar) > angular_tolerance)
    {
	return false;
    }

    ON_3dVector frame_polar = ON_CrossProduct(xaxis, yaxis);
    if (!frame_polar.Unitize())
	return false;
    if (frame_polar * source_polar < 0.0)
	yaxis = -yaxis;

    const double radial_radius = first_radius +
	0.5 * (second_radius - first_radius);
    const double axial_scale = polar_radius / radial_radius;
    const ON_3dPoint center(eip->v);
    ON_Plane equatorial_plane(center, xaxis, yaxis);
    ON_Sphere sphere(center, radial_radius);
    sphere.plane = equatorial_plane;
    if (!equatorial_plane.IsValid() || !sphere.IsValid() ||
	!std::isfinite(axial_scale) || !(axial_scale > 0.0))
    {
	return false;
    }

    ON_RevSurface *revolution = sphere.RevSurfaceForm(false, NULL);
    if (revolution == NULL || revolution->m_curve == NULL) {
	bu_log("rt_ell_brep: could not create spheroid revolution surface\n");
	delete revolution;
	return false;
    }

    /* An arc cannot survive a nonuniform affine transform as an arc.  Its
     * rational NURBS form can, while the containing surface remains an
     * ON_RevSurface and retains its analytic revolution semantics. */
    ON_NurbsCurve *profile = ON_NurbsCurve::New();
    if (revolution->m_curve->GetNurbForm(*profile) <= 0 ||
	!profile->IsValid())
    {
	bu_log("rt_ell_brep: could not create spheroid profile curve\n");
	delete profile;
	delete revolution;
	return false;
    }
    delete revolution->m_curve;
    revolution->m_curve = profile;

    const ON_Xform stretch = ON_Xform::ScaleTransformation(
	equatorial_plane, 1.0, 1.0, axial_scale);
    if (!stretch.IsValid() || !revolution->Transform(stretch) ||
	!revolution->IsValid())
    {
	bu_log("rt_ell_brep: could not stretch spheroid revolution surface\n");
	delete revolution;
	return false;
    }

    /* Converting a semicircle to rational control points and transforming it
     * at planetary scales can leave an endpoint about one ulp off the axis.
     * ON_BrepRevSurface then treats that pole as a tiny boundary circle rather
     * than a singular trim.  Snap only representation-level residuals back to
     * their mathematically exact axis locations. */
    double coordinate_scale = std::max(1.0, std::max(radial_radius,
	polar_radius));
    coordinate_scale = std::max(coordinate_scale, std::fabs(center.x));
    coordinate_scale = std::max(coordinate_scale, std::fabs(center.y));
    coordinate_scale = std::max(coordinate_scale, std::fabs(center.z));
    const double pole_tolerance = 128.0 *
	std::numeric_limits<double>::epsilon() * coordinate_scale;
    const int pole_indices[2] = {0, profile->CVCount() - 1};
    for (int pole_index : pole_indices) {
	const ON_3dPoint point = profile->PointAt(
	    pole_index == 0 ? profile->Domain().Min() :
	    profile->Domain().Max());
	double axis_parameter = 0.0;
	if (!point.IsValid() ||
	    !revolution->m_axis.ClosestPointTo(point, &axis_parameter))
	{
	    delete revolution;
	    return false;
	}
	const ON_3dPoint pole = revolution->m_axis.PointAt(axis_parameter);
	if (!pole.IsValid() || point.DistanceTo(pole) > pole_tolerance ||
	    !profile->SetCV(pole_index, pole))
	{
	    delete revolution;
	    return false;
	}
    }
    revolution->DestroyRuntimeCache();
    revolution->m_bbox.Destroy();
    revolution->BoundingBox();
    if (!revolution->IsValid()) {
	delete revolution;
	return false;
    }

    ON_Brep *result = ON_BrepRevSurface(revolution, false, false, *b);
    if (result == NULL) {
	bu_log("rt_ell_brep: could not create spheroid B-Rep\n");
	delete revolution;
	return false;
    }
    ON_wString validation_text;
    ON_TextLog validation_log(validation_text);
    const bool valid = result->IsValid(&validation_log);
    const bool solid = result->IsSolid();
    if (!valid || !solid) {
	ON_String message(validation_text);
	bu_log("rt_ell_brep: spheroid B-Rep is not a valid solid "
	    "(valid=%d solid=%d faces=%d edges=%d vertices=%d):\n%s",
	    valid, solid, result->m_F.Count(), result->m_E.Count(),
	    result->m_V.Count(), message.Array() ? message.Array() : "");
	result->Destroy();
	return false;
    }

    if (maximum_approximation != NULL) {
	*maximum_approximation = static_cast<fastf_t>(
	    0.5 * std::fabs(first_radius - second_radius));
    }
    return true;
}


extern "C" void
_rt_ell_brep_with_info(ON_Brep **b, const struct rt_db_internal *ip,
	const struct bn_tol *tol, fastf_t *maximum_approximation)
{
    struct rt_ell_internal *eip;

    if (maximum_approximation != NULL)
	*maximum_approximation = 0.0;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(eip);

    // For Algorithms used in rotation and translation
    // Please refer to ell.c
    // X' = S(R(X - V))
    // X = invR(invS(X')) + V = invRinvS(X') + V;
    // where R(X) = (A/(|A|))
    //              (B/(|B|)) . X
    //              (C/(|C|))
    //
    // and S(X) = (1/|A|   0     0 )
    //            (0     1/|B|   0 ) . X
    //            (0      0   1/|C|)

    // Parameters for Rotate and Translate
    vect_t Au, Bu, Cu;    /* A, B, C with unit length */
    mat_t R;
    mat_t Rinv;
    mat_t Sinv;
    mat_t invRinvS;
    fastf_t magsq_a, magsq_b, magsq_c;
    fastf_t f;

    magsq_a = MAGSQ(eip->a);
    magsq_b = MAGSQ(eip->b);
    magsq_c = MAGSQ(eip->c);

    if (magsq_a < tol->dist_sq || magsq_b < tol->dist_sq || magsq_c < tol->dist_sq) {
    bu_log("rt_ell_brep():  ell zero length A(%g), B(%g), or C(%g) vector\n",
	   magsq_a, magsq_b, magsq_c);
    }

    const double radius_a = std::sqrt(magsq_a);
    const double radius_b = std::sqrt(magsq_b);
    const double radius_c = std::sqrt(magsq_c);
    if (make_spheroid_brep(b, eip, radius_a, radius_b, radius_c,
	    maximum_approximation))
    {
	return;
    }

    f = 1.0/sqrt(magsq_a);
    VSCALE(Au, eip->a, f);
    f = 1.0/sqrt(magsq_b);
    VSCALE(Bu, eip->b, f);
    f = 1.0/sqrt(magsq_c);
    VSCALE(Cu, eip->c, f);

    MAT_IDN(R);
    VMOVE(&R[0], Au);
    VMOVE(&R[4], Bu);
    VMOVE(&R[8], Cu);
    bn_mat_trn(Rinv, R);

    MAT_IDN(Sinv);
    Sinv[0] = MAGNITUDE(eip->a);
    Sinv[5] = MAGNITUDE(eip->b);
    Sinv[10] = MAGNITUDE(eip->c);
    bn_mat_mul(invRinvS, Rinv, Sinv);

    point_t origin;
    VSET(origin, 0, 0, 0);
    ON_3dPoint oo(origin);
    ON_Sphere sph(oo, 1);

    // Get the NURBS form of the surface
    ON_NurbsSurface *ellcurvedsurf = ON_NurbsSurface::New();
    sph.GetNurbForm(*ellcurvedsurf);

    // Scale, rotate and translate control points
    for (int i = 0; i < ellcurvedsurf->CVCount(0); i++) {
	for (int j = 0; j < ellcurvedsurf->CVCount(1); j++) {
	    point_t cvpt;
	    ON_4dPoint ctrlpt;
	    ellcurvedsurf->GetCV(i, j, ctrlpt);
	    MAT3X3VEC(cvpt, invRinvS, ctrlpt);
	    point_t scale_v;
	    VSCALE(scale_v, eip->v, ctrlpt.w);
	    VADD2(cvpt, scale_v, cvpt);
	    ON_4dPoint newpt = ON_4dPoint(cvpt[0], cvpt[1], cvpt[2], ctrlpt.w);
	    ellcurvedsurf->SetCV(i, j, newpt);
	}
    }

    ellcurvedsurf->SetDomain(0, 0.0, 1.0);
    ellcurvedsurf->SetDomain(1, 0.0, 1.0);

    // Make final BREP structure
    (*b)->m_S.Append(ellcurvedsurf);
    int surfindex = (*b)->m_S.Count();
    (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);

    /* The unit sphere has outward parameter-space orientation.  A left-handed
     * A/B/C frame reflects that surface, so its otherwise valid single face
     * points inward.  This is observable when several disconnected shells
     * share one B-Rep: mixed orientations create consecutive entering or
     * leaving hits and merge distinct ray partitions. */
    vect_t bc_normal;
    VCROSS(bc_normal, eip->b, eip->c);
    if (VDOT(eip->a, bc_normal) < 0.0)
	(*b)->Flip();
}


extern "C" void
rt_ell_brep(ON_Brep **b, const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    _rt_ell_brep_with_info(b, ip, tol, NULL);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
