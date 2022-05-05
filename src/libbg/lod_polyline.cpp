/*                   L O D _ P O L Y L I N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Based off of code from https://github.com/bhaettasch/pop-buffer-demo
 * Copyright (c) 2016 Benjamin Hättasch and X3DOM
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file lod_polyline.cpp
 *
 * This file implements level-of-detail routines for polyline based displays.
 */

#include "common.h"

#include "bu/malloc.h"
#include "bg/geom.h"
#include "bg/lod.h"
#include "bg/sample.h"

static fastf_t
bbox_curve_count(
	point_t bbox_min, point_t bbox_max,
        fastf_t curve_scale,
        fastf_t s_size)
{
    fastf_t x_len, y_len, z_len, avg_len;

    x_len = fabs(bbox_max[X] - bbox_min[X]);
    y_len = fabs(bbox_max[Y] - bbox_min[Y]);
    z_len = fabs(bbox_max[Z] - bbox_min[Z]);

    avg_len = (x_len + y_len + z_len) / 3.0;

    fastf_t curve_spacing = s_size / 2.0;
    curve_spacing /= curve_scale;
    return avg_len / curve_spacing;
}


static int
tor_ellipse_points(
        vect_t ellipse_A,
        vect_t ellipse_B,
        fastf_t point_spacing)
{
    fastf_t avg_radius, circumference;

    avg_radius = (MAGNITUDE(ellipse_A) + MAGNITUDE(ellipse_B)) / 2.0;
    circumference = M_2PI * avg_radius;

    return circumference / point_spacing;
}

int
bg_tor_lod_gen(struct bv_scene_obj *s, struct bg_torus *tor, const struct bview *v, fastf_t s_size)
{
    vect_t a, b, tor_a, tor_b, tor_h, center;
    fastf_t mag_a, mag_b, mag_h;
    int points_per_ellipse;

    BG_TOR_CK_MAGIC(tor);

    // Set up the container to hold the polyline info
    struct bv_polyline_lod *l = bg_polyline_lod_create();
    s->draw_data = (void *)l;
    // Mark the object as a CSG LoD so the drawing routine knows to handle it differently
    s->s_type_flags |= BV_CSG2_LOD;

    // Calculate some convenience values from the torus
    VMOVE(tor_a, tor->a);
    mag_a = tor->r_a;
    VSCALE(tor_h, tor->h, tor->r_h);
    mag_h = tor->r_h;
    VCROSS(tor_b, tor_a, tor_h);
    VUNITIZE(tor_b);
    VSCALE(tor_b, tor_b, mag_a);
    mag_b = mag_a;

    // Using view and size info, determine a sampling interval
    fastf_t point_spacing = bg_sample_spacing(v, s_size);

    // Based on the initial recommended sampling, see how many
    // points this particular object will require
    VJOIN1(a, tor_a, mag_h / mag_a, tor_a);
    VJOIN1(b, tor_b, mag_h / mag_b, tor_b);
    points_per_ellipse = tor_ellipse_points(a, b, point_spacing);
    if (points_per_ellipse < 6) {
	// If we're this small, just draw a point
	l->array_cnt = 1;
	l->parrays = (point_t **)bu_calloc(l->array_cnt, sizeof(point_t *), "points array alloc");
	l->pcnts = (int *)bu_calloc(l->array_cnt, sizeof(int), "point array counts array alloc");
	l->pcnts[0] = 1;
	l->parrays[0] = (point_t *)bu_calloc(l->pcnts[0], sizeof(point_t), "point array alloc");
	VMOVE(l->parrays[0][0], tor->v);
	return 0;
    }

    // To allocate the correct memory, we first determine how many polylines we will
    // be creating for this particular object
    l->array_cnt = 4;

    int num_ellipses = bbox_curve_count(s->bmin, s->bmax, v->gv_s->curve_scale, s_size);
    if (num_ellipses < 3)
	num_ellipses = 3;
    l->array_cnt += num_ellipses;
    l->parrays = (point_t **)bu_calloc(l->array_cnt, sizeof(point_t *), "points array alloc");
    l->pcnts = (int *)bu_calloc(l->array_cnt, sizeof(int), "point array counts array alloc");

    /* plot outer circular contour */
    l->pcnts[0] = bg_ell_sample(&l->parrays[0], tor->v, a, b, points_per_ellipse);

    /* plot inner circular contour */
    VJOIN1(a, tor_a, -1.0 * mag_h / mag_a, tor_a);
    VJOIN1(b, tor_b, -1.0 * mag_h / mag_b, tor_b);
    points_per_ellipse = tor_ellipse_points(a, b, point_spacing);
    l->pcnts[1] = bg_ell_sample(&l->parrays[1], tor->v, a, b, points_per_ellipse);

    /* Draw parallel circles to show the primitive's most extreme points along
     * +h/-h.
     */
    points_per_ellipse = tor_ellipse_points(tor_a, tor_b, point_spacing);
    VADD2(center, tor->v, tor_h);
    l->pcnts[2] = bg_ell_sample(&l->parrays[2], center, tor_a, tor_b, points_per_ellipse);

    VJOIN1(center, tor->v, -1.0, tor_h);
    l->pcnts[3] = bg_ell_sample(&l->parrays[3], center, tor_a, tor_b, points_per_ellipse);

    /* draw circular radial cross sections */
    VMOVE(b, tor_h);
    fastf_t radian_step = M_2PI / num_ellipses;
    fastf_t radian = 0;
    for (int i = 0; i < num_ellipses; ++i) {
	bg_ell_radian_pnt(center, tor->v, tor_a, tor_b, radian);

	VJOIN1(a, center, -1.0, tor->v);
	VUNITIZE(a);
	VSCALE(a, a, mag_h);

	l->pcnts[i+4] = bg_ell_sample(&l->parrays[i+4], center, a, b, points_per_ellipse);

	radian += radian_step;
    }

    return 0;
}

extern "C" struct bv_polyline_lod *
bg_polyline_lod_create()
{
    struct bv_polyline_lod *lod;
    BU_GET(lod, struct bv_polyline_lod);
    return lod;
}

extern "C" void
bg_polyline_lod_destroy(struct bv_polyline_lod *l)
{
    if (!l)
	return;

    // Unlike meshes (at least for the moment) non libbg routines are
    // generating the info, so the primary memory is allocated elsewhere rather
    // than being pointers to internals and we directly free the public
    // structures when cleaning up.
    //
    // In the longer term we may shift the generation logic down and use an
    // internal, which is why the bv_polyline_lod structure has provisions for
    // such containers.  We'd need to determine if we can do so without relying
    // on librt data types, and for now we need an incremental approach, so
    // keeping it simple...
    if (l->array_cnt && l->parrays) {
	for (int i = 0; i < l->array_cnt; i++) {
	    bu_free(l->parrays[i], "point array");
	}
	bu_free(l->parrays, "point arrays array");
    }
    if (l->pcnts)
	bu_free(l->pcnts, "point counts array");

    BU_PUT(l, struct bv_polyline_lod);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
