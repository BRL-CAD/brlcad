/*                  V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file rt/view.cpp
 *
 * Information routines to support adaptive plotting.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "brep.h"
#include "bview.h"

#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/geom.h"
#include "rt/view.h"

static fastf_t
brep_avg_curve_bbox_diagonal_len(ON_Brep *brep)
{
    fastf_t avg_curve_len = 0.0;
    int i, num_curves = 0;

    for (i = 0; i < brep->m_E.Count(); ++i) {
	ON_BrepEdge &e = brep->m_E[i];
	const ON_Curve *crv = e.EdgeCurveOf();

	if (!crv->IsLinear()) {
	    ++num_curves;

	    ON_BoundingBox bbox;
	    if (crv->GetTightBoundingBox(bbox)) {
		avg_curve_len += bbox.Diagonal().Length();
	    } else {
		ON_3dVector linear_approx =
		    crv->PointAtEnd() - crv->PointAtStart();
		avg_curve_len += linear_approx.Length();
	    }
	}
    }
    avg_curve_len /= num_curves;

    return avg_curve_len;
}

static fastf_t
brep_est_avg_curve_len(struct rt_brep_internal *bi)
{
    return brep_avg_curve_bbox_diagonal_len(bi->brep) * 2.0;
}

static fastf_t
view_avg_size(struct bview *gvp)
{
    fastf_t view_aspect, x_size, y_size;

    view_aspect = (fastf_t)gvp->gvs.x_samples / gvp->gvs.y_samples;
    x_size = gvp->gv_size;
    y_size = x_size / view_aspect;

    return (x_size + y_size) / 2.0;
}

static fastf_t
view_avg_sample_spacing(struct bview *gvp)
{
    fastf_t avg_view_size, avg_view_samples;

    avg_view_size = view_avg_size(gvp);
    avg_view_samples = (gvp->gvs.x_samples + gvp->gvs.y_samples) / 2.0;

    return avg_view_size / avg_view_samples;
}

static fastf_t
solid_point_spacing(struct bview *gvp, fastf_t solid_width)
{
    fastf_t radius, avg_view_size, avg_sample_spacing;
    point2d_t p1, p2;

    if (solid_width < SQRT_SMALL_FASTF)
        solid_width = SQRT_SMALL_FASTF;

    avg_view_size = view_avg_size(gvp);

    /* Now, for the sake of simplicity we're going to make
     * several assumptions:
     *  - our samples represent a grid of square pixels
     *  - a circle with a diameter half the width of the solid is a
     *    good proxy for the kind of curve that will be plotted
     */
    radius = solid_width / 4.0;
    if (avg_view_size < solid_width) {
        /* If the solid is larger than the view, it is
         * probably only partly visible and likely isn't the
         * primary focus of the user. We'll cap the point
         * spacing and avoid wasting effort.
         */
        radius = avg_view_size / 4.0;
    }

    /* We imagine our representative circular curve lying in
     * the XY plane centered at the origin.
     *
     * Suppose we're viewing the circle head on, and that the
     * apex of the curve (0, radius) lies just inside the
     * top edge of a pixel. Here we place a plotted point p1.
     *
     * As we continue clockwise around the circle we pass
     * through neighboring pixels in the same row, until we
     * vertically drop a distance equal to the pixel spacing,
     * in which case we just barely enter a pixel in the next
     * row. Here we place a plotted point p2 (y = radius -
     * avg_sample_spacing).
     *
     * In theory, the line segment between p1 and p2 passes
     * through all the same pixels that the actual curve does,
     * and thus produces the exact same rasterization as if
     * the curve between p1 and p2 was approximated with an
     * infinite number of line segments.
     *
     * We assume that the distance between p1 and p2 is the
     * maximum point sampling distance we can use for the
     * curve which will give a perfect rasterization, i.e.
     * the same rasterization as if we chose a point distance
     * of 0.
    */
    p1[Y] = radius;
    p1[X] = 0.0;

    avg_sample_spacing = view_avg_sample_spacing(gvp);
    if (avg_sample_spacing < radius) {
        p2[Y] = radius - (avg_sample_spacing);
    } else {
        /* no particular reason other than symmetry, just need
         * to prevent sqrt(negative).
         */
        p2[Y] = radius;
    }
    p2[X] = sqrt((radius * radius) - (p2[Y] * p2[Y]));

    return DIST_PNT2_PNT2(p1, p2);
}


/* TODO: view_avg_sample_spacing() might be sufficient if we can
 * develop a general decimation routine for the resulting plots, in
 * which case, this function could be removed.
 */
extern "C" fastf_t
rt_solid_point_spacing_for_view(
        struct bview_scene_obj *sp,
        struct rt_db_internal *ip,
        struct bview *gvp)
{
    fastf_t point_spacing = 0.0;

    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD) {
        switch (ip->idb_minor_type) {
            case DB5_MINORTYPE_BRLCAD_TGC: {
                struct rt_tgc_internal *tgc;
                fastf_t avg_diameter;
                fastf_t tgc_mag_a, tgc_mag_b, tgc_mag_c, tgc_mag_d;

                RT_CK_DB_INTERNAL(ip);
                tgc = (struct rt_tgc_internal *)ip->idb_ptr;
                RT_TGC_CK_MAGIC(tgc);

                tgc_mag_a = MAGNITUDE(tgc->a);
                tgc_mag_b = MAGNITUDE(tgc->b);
                tgc_mag_c = MAGNITUDE(tgc->c);
                tgc_mag_d = MAGNITUDE(tgc->d);

                avg_diameter = tgc_mag_a + tgc_mag_b + tgc_mag_c + tgc_mag_d;
                avg_diameter /= 2.0;
                point_spacing = solid_point_spacing(gvp, avg_diameter);
            }
                break;
            case DB5_MINORTYPE_BRLCAD_BOT:
                point_spacing = view_avg_sample_spacing(gvp);
                break;
            case DB5_MINORTYPE_BRLCAD_BREP: {
                struct rt_brep_internal *bi;

                RT_CK_DB_INTERNAL(ip);
                bi = (struct rt_brep_internal *)ip->idb_ptr;
                RT_BREP_CK_MAGIC(bi);

                point_spacing = solid_point_spacing(gvp,
                        brep_est_avg_curve_len(bi) * M_2_PI * 2.0);
            }
                break;
            default:
                point_spacing = solid_point_spacing(gvp, sp->s_size);
        }
    } else {
        point_spacing = solid_point_spacing(gvp, sp->s_size);
    }

    return point_spacing;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

