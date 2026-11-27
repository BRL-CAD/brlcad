/*                       L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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

/** @file lod.cpp
 *
 * This file implements level-of-detail routines.  Eventually it may have libbg
 * wrappers around more sophisticated algorithms...
 *
 * The POP Buffer: Rapid Progressive Clustering by Geometry Quantization
 * https://x3dom.org/pop/files/popbuffer2013.pdf
 *
 * Useful discussion of applying POP buffers here:
 * https://medium.com/@petroskataras/the-ayotzinapa-case-447a72d89e58
 *
 * Notes on caching:
 *
 * Management of LoD cached data is actually a bit of a challenge.  A full
 * content hash of the original ver/tri arrays is the most reliable approach
 * but is a potentially expensive operation, which also requires reading the
 * entire original geometry to get the hash value to do lookups.  Ideally, we'd
 * like for the application to never have to access more of the data than is
 * needed for display purposes.  However, object names are not unique across .g
 * files and so are not useful for this purpose.  Also, at this level of the
 * logic we (deliberately) are separated from any notion of .g objects.
 *
 * What we do is generate a hash value of the data on initialization, when we
 * need the full data set to perform the initial LoD setup.  We then provide
 * that value back to the caller for them to manage at a higher level.
 */

#include "common.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdlib.h>
#include <map>
#include <set>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <math.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "bio.h"

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/cache.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/datetime.h"
#include "bg/plane.h"
#include "bg/sat.h"
#include "bg/trimesh.h"
#include "bn/mat.h"
#include "bv/plot3.h"
#include "bv/lod.h"
#include "bv/util.h"
#include "bv/view_sets.h"

// Number of levels of detail to define
#define POP_MAXLEVEL 16

// Subdirectory in BRL-CAD cache to hold this type of LoD data
#define POP_CACHEDIR ".POPLoD"

// Factor by which to bump out bounds to avoid points on box edges
#define MBUMP 1.01

/* On-disk format version.  Increment whenever the binary layout of any cached
 * payload changes.  bv_mesh_lod_context_create() reads this from
 * BU_DIR_CACHE/.POPLoD/format; a mismatch clears the entire .POPLoD tree. */
#define CACHE_CURRENT_FORMAT 3

/* There are various individual pieces of data in the cache associated with
 * each object key.  For lookup they use short suffix strings to distinguish
 * them - we define those strings here to have consistent definitions for use
 * in multiple functions.
 *
 * Changing any of these requires incrementing CACHE_CURRENT_FORMAT. */
#define CACHE_POP_MAX_LEVEL "th"
#define CACHE_POP_SWITCH_LEVEL "sw"
#define CACHE_VERTEX_COUNT "vc"
#define CACHE_TRI_COUNT "tc"
#define CACHE_OBJ_BOUNDS "bb"
#define CACHE_VERT_LEVEL "v"
#define CACHE_VERTNORM_LEVEL "vn"
#define CACHE_TRI_LEVEL "t"

typedef int (*full_detail_clbk_t)(struct bv_mesh_lod *, void *);

static void
obj_bb(int *have_objs, vect_t *min, vect_t *max, struct bv_scene_obj *s, struct bview *v)
{
    vect_t minus, plus;
    if (bv_scene_obj_bound(s, v)) {
	*have_objs = 1;
	minus[X] = s->s_center[X] - s->s_size;
	minus[Y] = s->s_center[Y] - s->s_size;
	minus[Z] = s->s_center[Z] - s->s_size;
	VMIN(*min, minus);
	plus[X] = s->s_center[X] + s->s_size;
	plus[Y] = s->s_center[Y] + s->s_size;
	plus[Z] = s->s_center[Z] + s->s_size;
	VMAX(*max, plus);
    }
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *sc = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	obj_bb(have_objs, min, max, sc, v);
    }
}

// Debugging function to see constructed arb
#define ARB_MAX_STRLEN 400
const char *
obb_arb(vect_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3)
{
    static char str[ARB_MAX_STRLEN+1];

    // For debugging purposes, construct an arb
    point_t arb[8];
    // 1 - c - e1 - e2 - e3
    VSUB2(arb[0], obb_center, obb_extent1);
    VSUB2(arb[0], arb[0], obb_extent2);
    VSUB2(arb[0], arb[0], obb_extent3);
    // 2 - c - e1 - e2 + e3
    VSUB2(arb[1], obb_center, obb_extent1);
    VSUB2(arb[1], arb[1], obb_extent2);
    VADD2(arb[1], arb[1], obb_extent3);
    // 3 - c - e1 + e2 + e3
    VSUB2(arb[2], obb_center, obb_extent1);
    VADD2(arb[2], arb[2], obb_extent2);
    VADD2(arb[2], arb[2], obb_extent3);
    // 4 - c - e1 + e2 - e3
    VSUB2(arb[3], obb_center, obb_extent1);
    VADD2(arb[3], arb[3], obb_extent2);
    VSUB2(arb[3], arb[3], obb_extent3);
    // 1 - c + e1 - e2 - e3
    VADD2(arb[4], obb_center, obb_extent1);
    VSUB2(arb[4], arb[4], obb_extent2);
    VSUB2(arb[4], arb[4], obb_extent3);
    // 2 - c + e1 - e2 + e3
    VADD2(arb[5], obb_center, obb_extent1);
    VSUB2(arb[5], arb[5], obb_extent2);
    VADD2(arb[5], arb[5], obb_extent3);
    // 3 - c + e1 + e2 + e3
    VADD2(arb[6], obb_center, obb_extent1);
    VADD2(arb[6], arb[6], obb_extent2);
    VADD2(arb[6], arb[6], obb_extent3);
    // 4 - c + e1 + e2 - e3
    VADD2(arb[7], obb_center, obb_extent1);
    VADD2(arb[7], arb[7], obb_extent2);
    VSUB2(arb[7], arb[7], obb_extent3);

#if 0
    bu_log("center: %f %f %f\n", V3ARGS(obb_center));
    bu_log("e1: %f %f %f\n", V3ARGS(obb_extent1));
    bu_log("e2: %f %f %f\n", V3ARGS(obb_extent2));
    bu_log("e3: %f %f %f\n", V3ARGS(obb_extent3));
#endif

    snprintf(str, ARB_MAX_STRLEN, "in obb.s arb8 %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
	    V3ARGS(arb[0]), V3ARGS(arb[1]), V3ARGS(arb[2]), V3ARGS(arb[3]), V3ARGS(arb[4]), V3ARGS(arb[5]), V3ARGS(arb  [6]), V3ARGS(arb[7]));
    return str;
}

static void
view_obb(struct bview *v,
	point_t sbbc, fastf_t radius,
	vect_t dir,
	point_t ec, point_t ep1, point_t ep2)
{

    // Box center is the closest point to the view center on the plane defined
    // by the scene's center and the view dir
    plane_t p;
    bg_plane_pt_nrml(&p, sbbc, dir);
    fastf_t pu, pv;
    point_t lec;
    VMOVE(lec, ec);
    bg_plane_closest_pt(&pu, &pv, &p, &lec);
    bg_plane_pt_at(&v->obb_center, &p, pu, pv);

    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(v->obb_extent1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(v->obb_extent2, ep1, ec);
    VSUB2(v->obb_extent3, ep2, ec);

    bv_log(3, "view_obb inputs[%s]: sbbc(%f %f %f) radius(%f) dir(%f %f %f)", bu_vls_cstr(&v->gv_name), V3ARGS(sbbc), radius, V3ARGS(dir));
    bv_log(3, "view_obb[%s]: %f %f %f -> [%f %f %f] [%f %f %f] [%f %f %f]", bu_vls_cstr(&v->gv_name), V3ARGS(v->obb_center), V3ARGS(v->obb_extent1), V3ARGS(v->obb_extent2), V3ARGS(v->obb_extent3));
    bv_log(3, "%s", obb_arb(v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3));
}

static void
_scene_radius(point_t *sbbc, fastf_t *radius, struct bview *v)
{
    if (!sbbc || !radius || !v)
	return;
    VSET(*sbbc, 0, 0, 0);
    *radius = 1.0;
    vect_t min, max, work;
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    int have_objs = 0;
    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    if (!so)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	obj_bb(&have_objs, &min, &max, g, v);
    }
    struct bu_ptbl *sol = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    if (sol) {
	for (size_t i = 0; i < BU_PTBL_LEN(sol); i++) {
	    struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(sol, i);
	    obj_bb(&have_objs, &min, &max, g, v);
	}
    }
    if (have_objs) {
	VADD2SCALE(*sbbc, max, min, 0.5);
	VSUB2SCALE(work, max, min, 0.5);
	(*radius) = MAGNITUDE(work);
    }
}

void
bv_view_bounds(struct bview *v)
{
    if (!v || !v->gv_width || !v->gv_height)
	return;

    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, v);

    // Using the pixel width and height of the current "window", construct some
    // view space dimensions related to that window
    int w = v->gv_width;
    int h = v->gv_height;
    int x = (int)(w * 0.5);
    int y = (int)(h * 0.5);
    //bu_log("w,h,x,y: %d %d %d %d\n", w,h, x, y);
    fastf_t x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, xc = 0.0, yc = 0.0;
    bv_screen_to_view(v, &x1, &y1, x, h);
    bv_screen_to_view(v, &x2, &y2, w, y);
    bv_screen_to_view(v, &xc, &yc, x, y);

    // Also stash the window's view space bbox
    fastf_t w0, w1, w2, w3;
    bv_screen_to_view(v, &w0, &w1, 0, 0);
    bv_screen_to_view(v, &w2, &w3, w, h);
    v->gv_wmin[0] = (w0 < w2) ? w0 : w2;
    v->gv_wmin[1] = (w1 < w3) ? w1 : w3;
    v->gv_wmax[0] = (w0 > w2) ? w0 : w2;
    v->gv_wmax[1] = (w1 > w3) ? w1 : w3;
    //bu_log("vbbmin: %f %f\n", v->gv_wmin[0], v->gv_wmin[1]);
    //bu_log("vbbmax: %f %f\n", v->gv_wmax[0], v->gv_wmax[1]);

    // Get the model space points for the mid points of the top and right edges
    // of the view.  If we don't have a width or height, we will use the
    // existing min and max since we don't have a "screen" to base the box on
    //bu_log("x1,y1: %f %f\n", x1, y1);
    //bu_log("x2,y2: %f %f\n", x2, y2);
    //bu_log("xc,yc: %f %f\n", xc, yc);
    point_t vp1, vp2, vc, ep1, ep2, ec;
    VSET(vp1, x1, y1, 0);
    VSET(vp2, x2, y2, 0);
    VSET(vc, xc, yc, 0);
    MAT4X3PNT(ep1, v->gv_view2model, vp1);
    MAT4X3PNT(ep2, v->gv_view2model, vp2);
    MAT4X3PNT(ec, v->gv_view2model, vc);
    //bu_log("view center: %f %f %f\n", V3ARGS(ec));
    //bu_log("edge point 1: %f %f %f\n", V3ARGS(ep1));
    //bu_log("edge point 2: %f %f %f\n", V3ARGS(ep2));

    // Need the direction vector - i.e., where the camera is looking.  Got this
    // trick from the libged nirt code...
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, -1.0);

    // If perspective mode is not enabled, update the oriented bounding box.
    if (!(SMALL_FASTF < v->gv_perspective)) {
	view_obb(v, sbbc, radius, dir, ec, ep1, ep2);
    }


    // While we have the info, construct the "backed out" point that will let
    // us construct the "backed out" view plane, and save that point and the
    // lookat direction to the view
    VMOVE(v->gv_lookat, dir);
    VSCALE(dir, dir, -radius);
    VADD2(v->gv_vc_backout, sbbc, dir);
    v->radius = radius;
}

static void
_find_active_objs(std::set<struct bv_scene_obj *> &active, struct bv_scene_obj *s, struct bview *v, point_t obb_c, point_t obb_e1, point_t obb_e2, point_t obb_e3)
{
    if (BU_PTBL_LEN(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *sc = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    _find_active_objs(active, sc, v, obb_c, obb_e1, obb_e2, obb_e3);
	}
    } else {
	bv_scene_obj_bound(s, v);
	if (bg_sat_aabb_obb(s->bmin, s->bmax, obb_c, obb_e1, obb_e2, obb_e3))
	    active.insert(s);
    }
}

int
bv_view_objs_select(struct bu_ptbl *sset, struct bview *v, int x, int y)
{
    if (!v || !sset || !v->gv_width || !v->gv_height)
	return 0;

    bu_ptbl_reset(sset);

    if (x < 0 || y < 0 || x > v->gv_width || y > v->gv_height)
	return 0;


    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, v);

    // Using the pixel width and height of the current "window", construct some
    // view space dimensions related to that window
    fastf_t x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, xc = 0.0, yc = 0.0;
    bv_screen_to_view(v, &x1, &y1, x, y+1);
    bv_screen_to_view(v, &x2, &y2, x+1, y);
    bv_screen_to_view(v, &xc, &yc, x, y);

    // Get the model space points for the mid points of the top and right edges
    // of the "pixel + 1" box.
    //bu_log("x1,y1: %f %f\n", x1, y1);
    //bu_log("x2,y2: %f %f\n", x2, y2);
    //bu_log("xc,yc: %f %f\n", xc, yc);
    point_t vp1, vp2, vc, ep1, ep2, ec;
    VSET(vp1, x1, y1, 0);
    VSET(vp2, x2, y2, 0);
    VSET(vc, xc, yc, 0);
    MAT4X3PNT(ep1, v->gv_view2model, vp1);
    MAT4X3PNT(ep2, v->gv_view2model, vp2);
    MAT4X3PNT(ec, v->gv_view2model, vc);

    // Need the direction vector - i.e., where the camera is looking.  Got this
    // trick from the libged nirt code...
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, -1.0);


    // Construct the box values needed for the SAT test
    point_t obb_c, obb_e1, obb_e2, obb_e3;

    // Box center is the closest point to the view center on the plane defined
    // by the scene's center and the view dir
    plane_t p;
    bg_plane_pt_nrml(&p, sbbc, dir);
    fastf_t pu, pv;
    bg_plane_closest_pt(&pu, &pv, &p, &ec);
    bg_plane_pt_at(&obb_c, &p, pu, pv);


    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(obb_e1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(obb_e2, ep1, ec);
    VSUB2(obb_e3, ep2, ec);

    // Having constructed the box, test the scene objects against it.  Any that intersect,
    // add them to the set
    std::set<struct bv_scene_obj *> active;
    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    if (so) {
	for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	    _find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
	}
    }
    struct bu_ptbl *sol = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    if (sol) {
	for (size_t i = 0; i < BU_PTBL_LEN(sol); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sol, i);
	    _find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
	}
    }
    if (active.size()) {
	std::set<struct bv_scene_obj *>::iterator a_it;
	for (a_it = active.begin(); a_it != active.end(); a_it++) {
	    bu_ptbl_ins(sset, (long *)*a_it);
	}
    }

    return active.size();
}

int
bv_view_objs_rect_select(struct bu_ptbl *sset, struct bview *v, int x1, int y1, int x2, int y2)
{
    if (!v || !sset || !v->gv_width || !v->gv_height)
	return 0;

    bu_ptbl_reset(sset);

    if (x1 < 0 || y1 < 0 || x1 > v->gv_width || y1 > v->gv_height)
	return 0;

    if (x2 < 0 || y2 < 0 || x2 > v->gv_width || y2 > v->gv_height)
	return 0;

    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, v);

    // Using the pixel width and height of the current "window", construct some
    // view space dimensions related to that window
    fastf_t fx1 = 0.0, fy1 = 0.0, fx2 = 0.0, fy2 = 0.0, fxc = 0.0, fyc = 0.0;
    bv_screen_to_view(v, &fx1, &fy1, (int)(0.5*(x1+x2)), y2);
    bv_screen_to_view(v, &fx2, &fy2, x2, (int)(0.5*(y1+y2)));
    bv_screen_to_view(v, &fxc, &fyc, (int)(0.5*(x1+x2)), (int)(0.5*(y1+y2)));

    // Get the model space points for the mid points of the top and right edges
    // of the box.
    point_t vp1, vp2, vc, ep1, ep2, ec;
    VSET(vp1, fx1, fy1, 0);
    VSET(vp2, fx2, fy2, 0);
    VSET(vc, fxc, fyc, 0);
    MAT4X3PNT(ep1, v->gv_view2model, vp1);
    MAT4X3PNT(ep2, v->gv_view2model, vp2);
    MAT4X3PNT(ec, v->gv_view2model, vc);
    //bu_log("in sph1.s sph %f %f %f 1\n", V3ARGS(ep1));
    //bu_log("in sph2.s sph %f %f %f 2\n", V3ARGS(ep2));
    //bu_log("in sphc.s sph %f %f %f 3\n", V3ARGS(ec));

    // Need the direction vector - i.e., where the camera is looking.  Got this
    // trick from the libged nirt code...
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, -1.0);


    // Construct the box values needed for the SAT test
    point_t obb_c, obb_e1, obb_e2, obb_e3;

    // Box center is the closest point to the view center on the plane defined
    // by the scene's center and the view dir
    plane_t p;
    bg_plane_pt_nrml(&p, sbbc, dir);
    fastf_t pu, pv;
    bg_plane_closest_pt(&pu, &pv, &p, &ec);
    bg_plane_pt_at(&obb_c, &p, pu, pv);


    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(obb_e1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(obb_e2, ep1, ec);
    VSUB2(obb_e3, ep2, ec);

#if 0
    bu_log("%s", obb_arb(obb_c, obb_e1, obb_e2, obb_e3));
#endif

    // Having constructed the box, test the scene objects against it.  Any that intersect,
    // add them to the set
    std::set<struct bv_scene_obj *> active;
    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    if (so) {
	for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	    _find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
	}
    }
    struct bu_ptbl *sol = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    if (sol) {
	for (size_t i = 0; i < BU_PTBL_LEN(sol); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sol, i);
	    _find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
	}
    }
    if (active.size()) {
	std::set<struct bv_scene_obj *>::iterator a_it;
	for (a_it = active.begin(); a_it != active.end(); a_it++) {
	    bu_ptbl_ins(sset, (long *)*a_it);
	}
    }

    return active.size();
}

static int
_obj_visible(struct bv_scene_obj *s, struct bview *v)
{
    if (bg_sat_aabb_obb(s->bmin, s->bmax, v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3)) {
	bv_log(3, "obj_visible[%s] - passed bg_sat_abb_obb: %f %f %f -> %f %f %f", bu_vls_cstr(&v->gv_name), V3ARGS(s->bmin), V3ARGS(s->bmax));
	bv_log(3, "                          view abb : %f %f %f -> [%f %f %f] [%f %f %f] [%f %f %f]", V3ARGS(v->obb_center), V3ARGS(v->obb_extent1), V3ARGS(v->obb_extent2), V3ARGS(v->obb_extent3));
	//bv_log(3, "%s", obb_arb(v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3));

	return 1;
    } else {
	bv_log(3, "obj_visible[%s] - FAILED bg_sat_abb_obb: %f %f %f -> %f %f %f", bu_vls_cstr(&v->gv_name),V3ARGS(s->bmin), V3ARGS(s->bmax));
	bv_log(3, "                          view abb : %f %f %f -> [%f %f %f] [%f %f %f] [%f %f %f]", V3ARGS(v->obb_center), V3ARGS(v->obb_extent1), V3ARGS(v->obb_extent2), V3ARGS(v->obb_extent3));
	//bv_log(3, "%s", obb_arb(v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3));
    }

    if (SMALL_FASTF < v->gv_perspective) {
	// For perspective mode, project the vertices of the distorted bounding
	// box into the view plane, bound them, and see if the box overlaps with
	// the view screen's box.
	point_t arb[8];
	VSET(arb[0], s->bmin[0], s->bmin[1], s->bmin[2]);
	VSET(arb[1], s->bmin[0], s->bmin[1], s->bmax[2]);
	VSET(arb[2], s->bmin[0], s->bmax[1], s->bmin[2]);
	VSET(arb[3], s->bmin[0], s->bmax[1], s->bmax[2]);
	VSET(arb[4], s->bmax[0], s->bmin[1], s->bmin[2]);
	VSET(arb[5], s->bmax[0], s->bmin[1], s->bmax[2]);
	VSET(arb[6], s->bmax[0], s->bmax[1], s->bmin[2]);
	VSET(arb[7], s->bmax[0], s->bmax[1], s->bmax[2]);
	point2d_t omin = {INFINITY, INFINITY};
	point2d_t omax = {-INFINITY, -INFINITY};
	for (int i = 0; i < 8; i++) {
	    point_t pp, ppnt;
	    point2d_t pxy;
	    MAT4X3PNT(pp, v->gv_pmat, arb[i]);
	    MAT4X3PNT(ppnt, v->gv_model2view, pp);
	    V2SET(pxy, ppnt[0], ppnt[1]);
	    V2MINMAX(omin, omax, pxy);
	}
	// IFF the omin/omax box and the corresponding view box overlap, this
	// object may be visible in the current view and needs to be updated
	for (int i = 0; i < 2; i++) {
	    if (omax[i] < v->gv_wmin[i] || omin[i] > v->gv_wmax[i])
		return 0;
	}
	return 1;
    }

    return 0;
}

struct bv_mesh_lod_context_internal {
    struct bu_cache *lod_cache;
    struct bu_cache_txn *lod_rtxn;  /* active read transaction, held across cache_get/cache_done */

    struct bu_cache *name_cache;

    struct bu_vls *fname;
};

struct bv_mesh_lod_context *
bv_mesh_lod_context_create(const char *name)
{
    if (!name)
	return NULL;

    // Hash the input filename to generate a key for uniqueness
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s", bu_path_normalize(name));
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    // (may have fixed this - need to check...)
    if (bu_vls_strlen(&fname) < 10) {
	bu_vls_printf(&fname, "GGGGGGGGGGGGG");
    }
    unsigned long long hash = bu_data_hash(bu_vls_cstr(&fname), bu_vls_strlen(&fname)*sizeof(char));
    bu_path_component(&fname, bu_path_normalize(name), BU_PATH_BASENAME_EXTLESS);
    bu_vls_printf(&fname, "_%llu", hash);

    // Create the context
    struct bv_mesh_lod_context *c;
    BU_GET(c, struct bv_mesh_lod_context);
    BU_GET(c->i, struct bv_mesh_lod_context_internal);
    struct bv_mesh_lod_context_internal *i = c->i;
    BU_GET(i->fname, struct bu_vls);
    bu_vls_init(i->fname);
    bu_vls_sprintf(i->fname, "%s", bu_vls_cstr(&fname));
    i->lod_rtxn = NULL;

    // Check the on-disk format version.  If it doesn't match CACHE_CURRENT_FORMAT,
    // clear the entire .POPLoD tree so stale entries don't accumulate.
    {
	char format_path[MAXPATHLEN];
	bu_dir(format_path, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, "format", NULL);
	long disk_format_version = -1;
	{
	    std::ifstream format_file(format_path);
	    if (format_file.is_open())
		format_file >> disk_format_version;
	}
	if (disk_format_version > 0 && disk_format_version != CACHE_CURRENT_FORMAT) {
	    bu_log("Old mesh lod cache version (%ld) found in format file at %s - clearing\n", disk_format_version, format_path);
	    bv_mesh_lod_clear_cache(NULL, 0);
	}
	FILE *fp = fopen(format_path, "w");
	if (fp) {
	    fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	    fclose(fp);
	}
    }

    // Build relative cache paths (relative to BU_DIR_CACHE)
    struct bu_vls lod_cpath = BU_VLS_INIT_ZERO;
    struct bu_vls name_cpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&lod_cpath,  "%s/%s",          POP_CACHEDIR, bu_vls_cstr(&fname));
    bu_vls_sprintf(&name_cpath, "%s/%s_namekey",  POP_CACHEDIR, bu_vls_cstr(&fname));

    // Open (creating if necessary) the LoD data cache and the name/key cache
    i->lod_cache  = bu_cache_open(bu_vls_cstr(&lod_cpath),  1, 0);
    i->name_cache = bu_cache_open(bu_vls_cstr(&name_cpath), 1, 0);

    bu_vls_free(&lod_cpath);
    bu_vls_free(&name_cpath);
    bu_vls_free(&fname);

    if (!i->lod_cache || !i->name_cache) {
	if (i->lod_cache)
	    bu_cache_close(i->lod_cache);
	if (i->name_cache)
	    bu_cache_close(i->name_cache);
	bu_vls_free(i->fname);
	BU_PUT(i->fname, struct bu_vls);
	BU_PUT(c->i, struct bv_mesh_lod_context_internal);
	BU_PUT(c, struct bv_mesh_lod_context);
	return NULL;
    }

    return c;
}

void
bv_mesh_lod_context_destroy(struct bv_mesh_lod_context *c)
{
    if (!c)
	return;
    bu_cache_close(c->i->name_cache);
    bu_cache_close(c->i->lod_cache);
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    BU_PUT(c->i, struct bv_mesh_lod_context_internal);
    BU_PUT(c, struct bv_mesh_lod_context);
}

unsigned long long
bv_mesh_lod_key_get(struct bv_mesh_lod_context *c, const char *name)
{
    if (!c || !name)
	return 0;

    // Database object names may be of arbitrary length - hash
    // to get the lookup key
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&keystr, "%s", name);
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    // (may have fixed this - need to check...)
    if (bu_vls_strlen(&keystr) < 10) {
	bu_vls_printf(&keystr, "GGGGGGGGGGGGG");
    }
    unsigned long long hash = bu_data_hash(bu_vls_cstr(&keystr), bu_vls_strlen(&keystr)*sizeof(char));
    bu_vls_sprintf(&keystr, "%llu:namekey", hash);

    void *data = NULL;
    size_t dsize = bu_cache_get(&data, bu_vls_cstr(&keystr), c->i->name_cache, NULL);
    bu_vls_free(&keystr);
    if (!dsize || !data)
	return 0;
    unsigned long long fkey = *(unsigned long long *)data;
    bu_free(data, "lod key data");
    return fkey;
}

int
bv_mesh_lod_key_put(struct bv_mesh_lod_context *c, const char *name, unsigned long long key)
{
    if (!c || !name || !key)
	return -1;

    // Database object names may be of arbitrary length - hash
    // to get something appropriate for a lookup key
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&keystr, "%s", name);
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    // (may have fixed this - need to check...)
    if (bu_vls_strlen(&keystr) < 10) {
	bu_vls_printf(&keystr, "GGGGGGGGGGGGG");
    }
    unsigned long long hash = bu_data_hash(bu_vls_cstr(&keystr), bu_vls_strlen(&keystr)*sizeof(char));
    bu_vls_sprintf(&keystr, "%llu:namekey", hash);

    size_t wsize = bu_cache_write((void *)&key, sizeof(key), bu_vls_cstr(&keystr), c->i->name_cache, NULL);
    bu_vls_free(&keystr);
    //bu_log("PUT %s: %llu\n", name, key);
    return (wsize > 0) ? 0 : -1;
}

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};


class POPState;
struct bv_mesh_lod_internal {
    POPState *s;
};

class POPState {
    public:

	// Create cached data (doesn't create a usable container)
	POPState(struct bv_mesh_lod_context *ctx, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_face_cnt_threshold_ratio);

	// Load cached data (DOES create a usable container)
	POPState(struct bv_mesh_lod_context *ctx, unsigned long long key);

	// Cleanup
	~POPState();

	// Based on a view size, recommend a level
	int get_level(fastf_t len);
	int texture_level(const struct bview *v, int level) const;

	// Load/unload data level
	void set_level(int level);

	// Shrink memory usage (level set routines will have to do more work
	// after this is run, but the POPState is still viable).  Used after a
	// client code has done all that is needed with the level data, such as
	// generating an OpenGL display list, but isn't done with the object.
	void shrink_memory();

	// Get the "current" position of a point, given its level
	void level_pnt(point_t *o, const point_t *p, int level);

	// Debugging
	void plot(const char *root);

	// Active faces needed by the current LoD (indexes into lod_tri_pnts).
	std::vector<int> lod_tris;
	std::vector<int> lod_tri_sources;
	std::vector<fastf_t> lod_corner_uvs;

	// This is where we store the active points - i.e., those needed for
	// the current LoD.  When initially creating the breakout from BoT data
	// we calculate all levels, but the goal is to not hold in memory any
	// more than we need to support the LoD drawing.  lod_tris will index
	// into lod_tri_pnts.
	std::vector<fastf_t> lod_tri_pnts;
	std::vector<fastf_t> lod_tri_pnts_snapped;
	std::vector<fastf_t> lod_tri_norms;

	// Current level of detail information loaded into nfaces/npnts
	int curr_level = -1;

	// Force a data reload even if the level hasn't changed (i.e. undo a
	// shrink memory operation when level is set.)
	bool force_update = false;

	// Maximum level for which POP info is defined.  Above that level,
	// need to shift to full rendering
	int max_pop_threshold_level = 0;

	// Used by calling functions to detect initialization errors
	bool is_valid = false;

	// Content based hash key
	unsigned long long hash;

	// Methods for full detail
	full_detail_clbk_t full_detail_setup_clbk = NULL;
	full_detail_clbk_t full_detail_clear_clbk = NULL;
	full_detail_clbk_t full_detail_free_clbk = NULL;
	void *detail_clbk_data = NULL;

	// Bounding box of original mesh
	point_t bbmin, bbmax;

	// Info for drawing
	struct bv_mesh_lod *lod = NULL;

    private:

	void tri_process();

	float minx = FLT_MAX, miny = FLT_MAX, minz = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX, maxz = -FLT_MAX;

	fastf_t max_face_ratio = 0.66;

	// Clamping of points to various detail levels
	int to_level(int val, int level);

	// Snap value to level value
	fastf_t snap(fastf_t val, fastf_t min, fastf_t max, int level);

	// Check if two points are equal at a clamped level
	bool is_equal(rec r1, rec r2, int level);

	// Degeneracy test for triangles
	bool tri_degenerate(rec r0, rec r1, rec r2, int level);

	std::vector<unsigned short> PRECOMPUTED_MASKS;

	// Write data out to cache (only used during initialization from
	// external data)
	void cache();
	bool cache_tri();
	bool cache_write(const char *component, std::stringstream &s);
	size_t cache_get(void **data, const char *component);
	void cache_done();
	void cache_del(const char *component);

	// Specific loading and unloading methods
	void tri_pop_load(int start_level, int level);
	void tri_pop_trim(int level);
	size_t level_vcnt[POP_MAXLEVEL+1] = {0};
	size_t level_tricnt[POP_MAXLEVEL+1] = {0};

	// Processing containers used for initial triangle data characterization
	std::vector<size_t> tri_ind_map;
	std::vector<size_t> vert_tri_minlevel;
	std::map<size_t, std::unordered_set<size_t>> level_tri_verts;
	std::vector<std::vector<size_t>> level_tris;

	// Pointers to original input data
	size_t vert_cnt = 0;
	const point_t *verts_array = NULL;
	const vect_t *vnorms_array = NULL;
	size_t faces_cnt = 0;
	int *faces_array = NULL;

	// Context
	struct bv_mesh_lod_context *c;
};

void
POPState::tri_process()
{
    // Until we prove otherwise, all edges are assumed to appear only at the
    // last level (and consequently, their vertices are only needed then).  Set
    // the level accordingly.
    vert_tri_minlevel.reserve(vert_cnt);
    for (size_t i = 0; i < vert_cnt; i++) {
	vert_tri_minlevel.push_back(POP_MAXLEVEL - 1);
    }

    // Reserve memory for level containers
    level_tris.reserve(POP_MAXLEVEL);
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	level_tris.push_back(std::vector<size_t>(0));
    }

    // Walk the triangles and perform the LoD characterization
    for (size_t i = 0; i < faces_cnt; i++) {
	rec triangle[3];
	// Transform triangle vertices
	bool bad_face = false;
	for (size_t j = 0; j < 3; j++) {
	    int f_ind = faces_array[3*i+j];
	    if ((size_t)f_ind >= vert_cnt || f_ind < 0) {
		bu_log("bad face %zd - skipping\n", i);
		bad_face = true;
		break;
	    }
	    triangle[j].x = floor((verts_array[f_ind][X] - minx) / (maxx - minx) * USHRT_MAX);
	    triangle[j].y = floor((verts_array[f_ind][Y] - miny) / (maxy - miny) * USHRT_MAX);
	    triangle[j].z = floor((verts_array[f_ind][Z] - minz) / (maxz - minz) * USHRT_MAX);
	}
	if (bad_face)
	    continue;

	// Find the pop up level for this triangle (i.e., when it will first
	// appear as we step up the zoom levels.)
	size_t level = POP_MAXLEVEL - 1;
	for (int j = 0; j < POP_MAXLEVEL; j++) {
	    if (!tri_degenerate(triangle[0], triangle[1], triangle[2], j)) {
		level = j;
		break;
	    }
	}
	// Add this triangle to its "pop" level
	level_tris[level].push_back(i);

	// Let the vertices know they will be needed at this level, if another
	// triangle doesn't already need them sooner
	for (size_t j = 0; j < 3; j++) {
	    if (vert_tri_minlevel[faces_array[3*i+j]] > level) {
		vert_tri_minlevel[faces_array[3*i+j]] = level;
	    }
	}
    }

    // The vertices now know when they will first need to appear.  Build level
    // sets of vertices
    for (size_t i = 0; i < vert_tri_minlevel.size(); i++) {
	level_tri_verts[vert_tri_minlevel[i]].insert(i);
    }

    // Having sorted the vertices into level sets, we may now define a new global
    // vertex ordering that respects the needs of the levels.
    tri_ind_map.reserve(vert_cnt);
    for (size_t i = 0; i < vert_cnt; i++) {
	tri_ind_map.push_back(i);
    }
    size_t vind = 0;
    std::map<size_t, std::unordered_set<size_t>>::iterator l_it;
    std::unordered_set<size_t>::iterator s_it;
    for (l_it = level_tri_verts.begin(); l_it != level_tri_verts.end(); l_it++) {
	for (s_it = l_it->second.begin(); s_it != l_it->second.end(); s_it++) {
	    tri_ind_map[*s_it] = vind;
	    vind++;
	}
    }

    // Beyond a certain depth, there is little benefit to the POP process.  If
    // we check the count of level_tris, we will find a level at which most of
    // the triangles are active.
    // TODO: Not clear yet when the tradeoff between memory and the work of LoD
    // point snapping trades off - 0.66 is just a guess.  We also need to
    // calculate this maximum size ratio as a function of the overall database
    // mesh data size, which will need to be passed in as a parameter - if the
    // original is too large, we may not be able to fit higher LoD levels in
    // the database and need to make this ratio smaller - probably a maximum of
    // 0.66(?) and drop it lower of the original database is very large (i.e.
    // we need to hold a lot of mesh data).
    size_t trisum = 0;
    if (max_face_ratio > 0.99 || max_face_ratio < 0) {
	max_pop_threshold_level = level_tris.size() - 1;
    } else {
	size_t faces_array_cnt2 = (size_t)((fastf_t)faces_cnt * max_face_ratio);
	for (size_t i = 0; i < level_tris.size(); i++) {
	    trisum += level_tris[i].size();
	    if (trisum > faces_array_cnt2) {
		// If we're using two thirds of the triangles, this is our
		// threshold level.  If we've got ALL the triangles, back
		// down one level.
		if (trisum < (size_t)faces_cnt) {
		    max_pop_threshold_level = i;
		} else {
		    // Handle the case where we get i == 0
		    max_pop_threshold_level = (i) ? i - 1 : 0;
		}
		break;
	    }
	}
    }
    //bu_log("Max LoD POP level: %zd\n", max_pop_threshold_level);
}

POPState::POPState(struct bv_mesh_lod_context *ctx, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_facecnt_threshold_ratio)
{
    // Store the context
    c = ctx;

    // Caller set parameter telling us when to switch from POP data
    // to just drawing the full mesh
    max_face_ratio = pop_facecnt_threshold_ratio;

    // Hash the data to generate a key, if the user didn't supply us with one
    if (!user_key) {
	struct bu_data_hash_state *s = bu_data_hash_create();
	bu_data_hash_update(s, v, vcnt*sizeof(point_t));
	bu_data_hash_update(s, faces, 3*fcnt*sizeof(int));
	hash = bu_data_hash_val(s);
	bu_data_hash_destroy(s);
    } else {
	hash = user_key;
    }

    // Make sure there's no cache before performing the full initializing from
    // the original data.  In this mode the POPState creation is used to
    // initialize the cache, not to create a workable data state from that
    // data.  The hash is set, which is all we really need - loading data from
    // the cache is handled elsewhere.
    void *cdata = NULL;
    size_t csize = cache_get(&cdata, CACHE_POP_MAX_LEVEL);
    if (csize && cdata) {
	cache_done();
	is_valid = true;
	return;
    }

    // Cache isn't already populated - go to work.
    cache_done();

    curr_level = POP_MAXLEVEL - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    // Store source data info
    vert_cnt = vcnt;
    verts_array = v;
    vnorms_array = vn;
    faces_cnt = fcnt;
    faces_array = faces;

    // Calculate the full mesh bounding box for later use
    bg_trimesh_aabb(&bbmin, &bbmax, faces_array, faces_cnt, verts_array, vert_cnt);

    // Find our min and max values, initialize levels
    for (size_t i = 0; i < vcnt; i++) {
	minx = (v[i][X] < minx) ? v[i][X] : minx;
	miny = (v[i][Y] < miny) ? v[i][Y] : miny;
	minz = (v[i][Z] < minz) ? v[i][Z] : minz;
	maxx = (v[i][X] > maxx) ? v[i][X] : maxx;
	maxy = (v[i][Y] > maxy) ? v[i][Y] : maxy;
	maxz = (v[i][Z] > maxz) ? v[i][Z] : maxz;
    }

    // Bump out the min and max bounds slightly so none of our actual
    // points are too close to these limits
    minx = minx-fabs(MBUMP*minx);
    miny = miny-fabs(MBUMP*miny);
    minz = minz-fabs(MBUMP*minz);
    maxx = maxx+fabs(MBUMP*maxx);
    maxy = maxy+fabs(MBUMP*maxy);
    maxz = maxz+fabs(MBUMP*maxz);

    // Characterize triangle faces
    tri_process();

    // We're now ready to write out the data
    is_valid = true;
    cache();

    if (!is_valid)
	return;

#if 0
    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("bucket %zu count: %zu\n", i, level_tris[i].size());
    }

    for (size_t i = 0; i < POP_MAXLEVEL; i++) {
	bu_log("vert %zu count: %zu\n", i, level_tri_verts[i].size());
    }
#endif
}

POPState::POPState(struct bv_mesh_lod_context *ctx, unsigned long long key)
{
    // Store the context
    c = ctx;

    if (!key)
	return;

    // Initialize so set_level will read in the first level of triangles
    curr_level = - 1;

    // Precompute precision masks for each level
    for (int i = 0; i < POP_MAXLEVEL; i++) {
	PRECOMPUTED_MASKS.push_back(pow(2, (POP_MAXLEVEL - i - 1)));
    }

    hash = key;

    // Find the maximum POP level
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_POP_MAX_LEVEL);
	if (!bsize) {
	    cache_done();
	    return;
	}
	if (bsize != sizeof(max_pop_threshold_level)) {
	    bu_log("Incorrect data size found loading max LoD POP threshold\n");
	    cache_done();
	    return;
	}
	memcpy(&max_pop_threshold_level, b, sizeof(max_pop_threshold_level));
	cache_done();
    }

    // Load the POP level where we switch from POP to full
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_POP_SWITCH_LEVEL);
	if (bsize && bsize != sizeof(max_face_ratio)) {
	    bu_log("Incorrect data size found loading LoD POP switch threshold\n");
	    cache_done();
	    return;
	}
	if (bsize) {
	    memcpy(&max_face_ratio, b, sizeof(max_face_ratio));
	} else {
	    max_face_ratio = 0.66;
	}
	cache_done();
    }

    // Load level counts for vectors and tris
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_VERTEX_COUNT);
	if (bsize != sizeof(level_vcnt)) {
	    bu_log("Incorrect data size found loading level vertex counts\n");
	    cache_done();
	    return;
	}
	memcpy(&level_vcnt, b, sizeof(level_vcnt));
	cache_done();
    }
    {
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_TRI_COUNT);
	if (bsize != sizeof(level_tricnt)) {
	    bu_log("Incorrect data size found loading level triangle counts\n");
	    cache_done();
	    return;
	}
	memcpy(&level_tricnt, b, sizeof(level_tricnt));
	cache_done();
    }

    // Read in min/max bounds
    {
	float minmax[6];
	const char *b = NULL;
	size_t bsize = cache_get((void **)&b, CACHE_OBJ_BOUNDS);
	if (bsize != (sizeof(bbmin) + sizeof(bbmax) + sizeof(minmax))) {
	    bu_log("Incorrect data size found loading cached bounds data\n");
	    cache_done();
	    return;
	}
	memcpy(&bbmin, b, sizeof(bbmin));
	b += sizeof(bbmin);
	memcpy(&bbmax, b, sizeof(bbmax));
	b += sizeof(bbmax);
	//bu_log("bbmin: %f %f %f bbmax: %f %f %f\n", V3ARGS(bbmin), V3ARGS(bbmax));
	memcpy(&minmax, b, sizeof(minmax));
	minx = minmax[0];
	miny = minmax[1];
	minz = minmax[2];
	maxx = minmax[3];
	maxy = minmax[4];
	maxz = minmax[5];
	cache_done();
    }

    // Read in the zero level vertices, vertex normals (if defined) and triangles
    set_level(0);

    // All set - ready for LoD
    is_valid = 1;
}

POPState::~POPState()
{
    if (full_detail_free_clbk) {
	(*full_detail_free_clbk)(lod, detail_clbk_data);
	detail_clbk_data = NULL;
    }
}

void
POPState::tri_pop_load(int start_level, int level)
{
    struct bu_vls kbuf = BU_VLS_INIT_ZERO;

    // Read in the level vertices
    for (int i = start_level+1; i <= level; i++) {
	if (!level_vcnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERT_LEVEL, i);
	fastf_t *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize != level_vcnt[i]*sizeof(point_t)) {
	    bu_log("Incorrect data size found loading level %d point data\n", i);
	    return;
	}
	lod_tri_pnts.insert(lod_tri_pnts.end(), &b[0], &b[level_vcnt[i]*3]);
	cache_done();
    }
    // Re-snap all vertices currently loaded at the new level
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.reserve(lod_tri_pnts.size());
    for (size_t i = 0; i < lod_tri_pnts.size()/3; i++) {
	point_t p, sp;
	VSET(p, lod_tri_pnts[3*i+0], lod_tri_pnts[3*i+1], lod_tri_pnts[3*i+2]);
	level_pnt(&sp, &p, level);
	for (int k = 0; k < 3; k++) {
	    lod_tri_pnts_snapped.push_back(sp[k]);
	}
    }

    // Read in the level triangles
    for (int i = start_level+1; i <= level; i++) {
	if (!level_tricnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_TRI_LEVEL, i);
	int *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize != level_tricnt[i]*4*sizeof(int)) {
	    bu_log("Incorrect data size found loading level %d tri data\n", i);
	    cache_done();
	    return;
	}
	for (size_t j = 0; j < level_tricnt[i]; j++, b += 4) {
	    lod_tris.insert(lod_tris.end(), b, b + 3);
	    lod_tri_sources.push_back(b[3]);
	}
	cache_done();
    }

    // Read in the vertex normals, if we have them
    for (int i = start_level+1; i <= level; i++) {
	if (!level_tricnt[i])
	    continue;
	bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERTNORM_LEVEL, i);
	fastf_t *b = NULL;
	size_t bsize = cache_get((void **)&b, bu_vls_cstr(&kbuf));
	if (bsize > 0 && bsize != level_tricnt[i]*sizeof(vect_t)*3) {
	    bu_log("Incorrect data size found loading level %d normal data\n", i);
	    return;
	}
	if (bsize) {
	    lod_tri_norms.insert(lod_tri_norms.end(), &b[0], &b[level_tricnt[i]*3*3]);
	}
	cache_done();
    }
}

void
POPState::shrink_memory()
{
    lod_tri_pnts.clear();
    lod_tri_pnts.shrink_to_fit();
    lod_tri_norms.clear();
    lod_tri_norms.shrink_to_fit();
    lod_tris.clear();
    lod_tris.shrink_to_fit();
    lod_tri_sources.clear();
    lod_tri_sources.shrink_to_fit();
    lod_corner_uvs.clear();
    lod_corner_uvs.shrink_to_fit();
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.shrink_to_fit();
}

void
POPState::tri_pop_trim(int level)
{
    // Tally all the lower level verts and tris - those are the ones we need to keep
    size_t vkeep_cnt = 0;
    size_t fkeep_cnt = 0;
    for (size_t i = 0; i <= (size_t)level; i++) {
	vkeep_cnt += level_vcnt[i];
	fkeep_cnt += level_tricnt[i];
    }

    // Shrink the main arrays (note that in C++11 shrink_to_fit may or may
    // not actually shrink memory usage on any given call.)
    lod_tri_pnts.resize(vkeep_cnt*3);
    lod_tri_pnts.shrink_to_fit();
    lod_tri_norms.resize(fkeep_cnt*3*3);
    lod_tri_norms.shrink_to_fit();
    lod_tris.resize(fkeep_cnt*3);
    lod_tris.shrink_to_fit();
    lod_tri_sources.resize(fkeep_cnt);
    lod_tri_sources.shrink_to_fit();

    // Re-snap all vertices loaded at the new level
    lod_tri_pnts_snapped.clear();
    lod_tri_pnts_snapped.reserve(lod_tri_pnts.size());
    for (size_t i = 0; i < lod_tri_pnts.size()/3; i++) {
	point_t p, sp;
	VSET(p, lod_tri_pnts[3*i+0], lod_tri_pnts[3*i+1], lod_tri_pnts[3*i+2]);
	level_pnt(&sp, &p, level);
	for (int k = 0; k < 3; k++) {
	    lod_tri_pnts_snapped.push_back(sp[k]);
	}
    }
}

int
POPState::get_level(fastf_t vlen)
{
    fastf_t delta = 0.01*vlen;
    point_t bmin, bmax;
    fastf_t bdiag = 0;
    VSET(bmin, minx, miny, minz);
    VSET(bmax, maxx, maxy, maxz);
    bdiag = DIST_PNT_PNT(bmin, bmax);

    // If all views are orthogonal we just need the diagonal of the bbox, but
    // if we have any active perspective matrices that may change the answer.
    // We need to provide as much detail as is required by the most demanding
    // view.  In principle we might also be able to back the LoD down further
    // for very distant objects, but a quick test with that resulted in too much
    // loss of detail so for now just look at the "need more" case.
    struct bu_ptbl *vsets = (lod && lod->s) ? bv_set_views(lod->s->s_v->vset) : NULL;
    if (!vsets) {
	struct bview *v = (lod && lod->s) ? lod->s->s_v : NULL;
	if (v && SMALL_FASTF < v->gv_perspective) {
	    fastf_t cdist = 0;
	    point_t pbmin, pbmax;
	    MAT4X3PNT(pbmin, v->gv_pmat, bmin);
	    MAT4X3PNT(pbmax, v->gv_pmat, bmax);
	    bdiag = DIST_PNT_PNT(pbmin, pbmax);
	    if (cdist > bdiag)
		bdiag = cdist;
	}
    } else {
	for (size_t i = 0; i < BU_PTBL_LEN(vsets); i++) {
	    fastf_t cdist = 0;
	    struct bview *cv = (struct bview *)BU_PTBL_GET(vsets, i);
	    if (!_obj_visible(lod->s, cv))
		continue;
	    if (SMALL_FASTF < cv->gv_perspective) {
		point_t pbmin, pbmax;
		MAT4X3PNT(pbmin, cv->gv_pmat, bmin);
		MAT4X3PNT(pbmax, cv->gv_pmat, bmax);
		cdist = DIST_PNT_PNT(pbmin, pbmax);
		if (cdist > bdiag)
		    bdiag = cdist;
	    }
	}
    }

    for (int lev = 0; lev < POP_MAXLEVEL; lev++) {
	fastf_t diag_slice = bdiag/pow(2,lev);
	if (diag_slice < delta) {
	    bv_log(2, "POPState::get_level %g->%d", vlen, lev);
	    return lev;
	}
    }
    bv_log(2, "POPState::get_level %g->%d", vlen, POP_MAXLEVEL - 1);
    return POP_MAXLEVEL - 1;
}

int
POPState::texture_level(const struct bview *v, int level) const
{
    if (level > max_pop_threshold_level || v->gv_width <= 0 || v->gv_height <= 0)
	return level;

    mat_t model, clip;
    bn_mat_mul(model, v->gv_model2view, lod->s->s_mat);
    const fastf_t aspect = (fastf_t)v->gv_width / (fastf_t)v->gv_height;
    HSCALE(&model[4], &model[4], aspect);
    if (v->gv_perspective > SMALL_FASTF)
	bn_mat_mul(clip, v->gv_pmat, model);
    else
	MAT_COPY(clip, model);

    point_t center;
    vect_t half_extent, snap_range;
    VADD2SCALE(center, bbmin, bbmax, 0.5);
    VSUB2SCALE(half_extent, bbmax, bbmin, 0.5);
    VSET(snap_range, (fastf_t)maxx - minx, (fastf_t)maxy - miny, (fastf_t)maxz - minz);

    vect_t middle, span, shift;
    const int offsets[3] = {0, 4, 12};  // X, Y and homogeneous W rows
    for (int i = 0; i < 3; i++) {
	const fastf_t *row = &clip[offsets[i]];
	vect_t abs_row;
	VSET(abs_row, fabs(row[0]), fabs(row[1]), fabs(row[2]));
	middle[i] = VDOT(row, center) + row[3];
	span[i] = VDOT(abs_row, half_extent);
	shift[i] = VDOT(abs_row, snap_range);
    }

    const fastf_t min_w = middle[2] - span[2];
    const fastf_t limit = v->gv_s->lod_texture_error;
    for (; level <= max_pop_threshold_level; level++) {
	// The quantizer moves a coordinate by at most half a grid cell.
	const fastf_t step = PRECOMPUTED_MASKS[level] / (2.0 * USHRT_MAX);
	const fastf_t dw = shift[2] * step;
	// Perspective displacement is unbounded if the snapped box can
	// cross the eye plane. Continue refining until the projection is safe.
	if (min_w <= dw)
	    continue;
	const fastf_t x_max = fabs(middle[0]) + span[0];
	const fastf_t y_max = fabs(middle[1]) + span[1];
	const fastf_t displaced_w = min_w - dw;
	const fastf_t dx = (shift[0] * step + x_max / min_w * dw) / displaced_w;
	const fastf_t dy = (shift[1] * step + y_max / min_w * dw) / displaced_w;
	if (hypot(dx * v->gv_width * 0.5, dy * v->gv_height * 0.5) <= limit)
	    break;
    }
    return level;
}

void
POPState::set_level(int level)
{
    // If we're already there and we're not undoing a memshrink, no work to do
    if (level == curr_level && !force_update)
	return;

    lod_corner_uvs.clear();

    // If we're doing a forced update, it's like starting from
    // scratch - reset to 0
    if (force_update) {
	force_update = false;
	if (curr_level > max_pop_threshold_level && full_detail_clear_clbk)
	    (*full_detail_clear_clbk)(lod, detail_clbk_data);
	shrink_memory();
	curr_level = -1;
    }

    //    int64_t start, elapsed;
    //    fastf_t seconds;
    //    start = bu_gettime();

    // Triangles

    // If we need to pull more data, do so
    if (level > curr_level && level <= max_pop_threshold_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_load(curr_level, level);
	}
    }

    // If we need to trim back the POP data, do that
    if (level < curr_level && level <= max_pop_threshold_level && curr_level <= max_pop_threshold_level) {
	if (!lod_tri_pnts.size()) {
	    tri_pop_load(-1, level);
	} else {
	    tri_pop_trim(level);
	}
    }

    // If we were operating beyond POP detail levels (i.e. using RTree
    // management) we need to reset our POP data and clear the more detailed
    // info from the containers to free up memory.
    if (level < curr_level && level <= max_pop_threshold_level && curr_level > max_pop_threshold_level) {
	// We're (re)entering the POP range - clear the high detail data and start over
	if (full_detail_clear_clbk)
	    (*full_detail_clear_clbk)(lod, detail_clbk_data);

	// Full reset, not an incremental load.
	tri_pop_load(-1, level);
    }

    // If we're jumping into details levels beyond POP range, clear the POP containers
    // and load the more detailed data management info
    if (level > curr_level && level > max_pop_threshold_level && curr_level <= max_pop_threshold_level) {

	// Clear the LoD data - we need the full set now
	shrink_memory();

	// Use the callback to set up the full data pointers
	if (full_detail_setup_clbk)
	    (*full_detail_setup_clbk)(lod, detail_clbk_data);
    }

    //elapsed = bu_gettime() - start;
    //seconds = elapsed / 1000000.0;
    //bu_log("lod set_level(%d): %f sec\n", level, seconds);

    curr_level = level;
}

// Rather than committing all data to the cache in one transaction, use keys with
// appended strings to the hash to denote the individual pieces - basically
// what we were doing with files, but in the db instead
//
// This will also allow easier removal of larger subcomponents if we need to
// back off on saved LoD.
bool
POPState::cache_write(const char *component, std::stringstream &s)
{
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    std::string buffer = s.str();
    size_t wsize = bu_cache_write((void *)buffer.data(), buffer.length(), keystr.c_str(), c->i->lod_cache, NULL);
    return (wsize > 0);
}

// This pulls the data, but keeps a transaction open because the
// calling code will want to manipulate the data directly.  After that
// process is complete, cache_done() should be called.
size_t
POPState::cache_get(void **data, const char *component)
{
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    return bu_cache_get(data, keystr.c_str(), c->i->lod_cache, &c->i->lod_rtxn);
}

void
POPState::cache_done()
{
    bu_cache_get_done(&c->i->lod_rtxn);
}

bool
POPState::cache_tri()
{
    // Write out the threshold level - above this level,
    // we need to switch to full-detail drawing
    {
	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&max_pop_threshold_level), sizeof(max_pop_threshold_level));
	if (!cache_write(CACHE_POP_MAX_LEVEL, s))
	    return false;
    }

    // Write out the switch level
    {
	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&max_face_ratio), sizeof(max_face_ratio));
	if (!cache_write(CACHE_POP_SWITCH_LEVEL, s))
	    return false;
    }

    // Write out the vertex counts for all active levels
    {
	std::stringstream s;
	for (size_t i = 0; i <= POP_MAXLEVEL; i++) {
	    size_t icnt = 0;
	    if (level_tri_verts.find(i) == level_tri_verts.end()) {
		s.write(reinterpret_cast<const char *>(&icnt), sizeof(icnt));
		continue;
	    }
	    if ((int)i > max_pop_threshold_level || !level_tri_verts[i].size()) {
		s.write(reinterpret_cast<const char *>(&icnt), sizeof(icnt));
		continue;
	    }
	    icnt = level_tri_verts[i].size();
	    s.write(reinterpret_cast<const char *>(&icnt), sizeof(icnt));
	}
	if (!cache_write(CACHE_VERTEX_COUNT, s))
	    return false;
    }

    // Write out the triangle counts for all active levels
    {
	std::stringstream s;
	for (size_t i = 0; i <= POP_MAXLEVEL; i++) {
	    size_t tcnt = 0;
	    if ((int)i > max_pop_threshold_level || !level_tris[i].size()) {
		s.write(reinterpret_cast<const char *>(&tcnt), sizeof(tcnt));
		continue;
	    }
	    // Store the size of the level tri vector
	    tcnt = level_tris[i].size();
	    s.write(reinterpret_cast<const char *>(&tcnt), sizeof(tcnt));
	}
	if (!cache_write(CACHE_TRI_COUNT, s))
	    return false;
    }

    struct bu_vls kbuf = BU_VLS_INIT_ZERO;

    // Write out the vertices in LoD order for each level
    {
	for (int i = 0; i <= max_pop_threshold_level; i++) {
	    std::stringstream s;
	    if (level_tri_verts.find(i) == level_tri_verts.end())
		continue;
	    if (!level_tri_verts[i].size())
		continue;
	    // Write out the vertex points
	    std::unordered_set<size_t>::iterator s_it;
	    for (s_it = level_tri_verts[i].begin(); s_it != level_tri_verts[i].end(); s_it++) {
		point_t v;
		VMOVE(v, verts_array[*s_it]);
		s.write(reinterpret_cast<const char *>(&v[0]), sizeof(point_t));
	    }
	    bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERT_LEVEL, i);
	    if (!cache_write(bu_vls_cstr(&kbuf), s))
		return false;
	}
    }

    // Write out the triangles in LoD order for each level
    {
	for (int i = 0; i <= max_pop_threshold_level; i++) {
	    std::stringstream s;
	    if (!level_tris[i].size())
		continue;
	    // Write out the mapped triangle indices
	    std::vector<size_t>::iterator s_it;
	    for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		int vt[4];
		vt[0] = (int)tri_ind_map[faces_array[3*(*s_it)+0]];
		vt[1] = (int)tri_ind_map[faces_array[3*(*s_it)+1]];
		vt[2] = (int)tri_ind_map[faces_array[3*(*s_it)+2]];
		vt[3] = (int)*s_it;
		s.write(reinterpret_cast<const char *>(&vt[0]), sizeof(vt));
	    }
	    bu_vls_sprintf(&kbuf, "%s%d", CACHE_TRI_LEVEL, i);
	    if (!cache_write(bu_vls_cstr(&kbuf), s))
		return false;
	}
    }

    // Write out the vertex normals in LoD order for each level, if we have them
    {
	if (vnorms_array) {
	    for (int i = 0; i <= max_pop_threshold_level; i++) {
		std::stringstream s;
		if (!level_tris[i].size())
		    continue;
		// Write out the normals associated with the triangle indices
		std::vector<size_t>::iterator s_it;
		for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		    vect_t v;
		    int tind;
		    tind = 3*(*s_it)+0;
		    VMOVE(v, vnorms_array[tind]);
		    s.write(reinterpret_cast<const char *>(&v[0]), sizeof(vect_t));
		    tind = 3*(*s_it)+1;
		    VMOVE(v, vnorms_array[tind]);
		    s.write(reinterpret_cast<const char *>(&v[0]), sizeof(vect_t));
		    tind = 3*(*s_it)+2;
		    VMOVE(v, vnorms_array[tind]);
		    s.write(reinterpret_cast<const char *>(&v[0]), sizeof(vect_t));
		}
		bu_vls_sprintf(&kbuf, "%s%d", CACHE_VERTNORM_LEVEL, i);
		if (!cache_write(bu_vls_cstr(&kbuf), s))
		    return false;
	    }
	}
    }


    return true;
}

// Write out the generated LoD data to the BRL-CAD cache
void
POPState::cache()
{
    if (!hash) {
	is_valid = false;
	return;
    }

    // Stash the original mesh bbox and the min and max bounds, which will be used in decoding
    {
	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&bbmin), sizeof(bbmin));
	s.write(reinterpret_cast<const char *>(&bbmax), sizeof(bbmax));
	s.write(reinterpret_cast<const char *>(&minx), sizeof(minx));
	s.write(reinterpret_cast<const char *>(&miny), sizeof(miny));
	s.write(reinterpret_cast<const char *>(&minz), sizeof(minz));
	s.write(reinterpret_cast<const char *>(&maxx), sizeof(maxx));
	s.write(reinterpret_cast<const char *>(&maxy), sizeof(maxy));
	s.write(reinterpret_cast<const char *>(&maxz), sizeof(maxz));
	is_valid = cache_write(CACHE_OBJ_BOUNDS, s);
    }

    if (!is_valid)
	return;

    // Serialize triangle-specific data
    is_valid = cache_tri();
}

// Transfer coordinate into level precision
int
POPState::to_level(int val, int level)
{
    int ret = floor(val/double(PRECOMPUTED_MASKS[level]));
    //bu_log("to_level: %d, %d : %d\n", val, level, ret);
    return ret;
}

fastf_t
POPState::snap(fastf_t val, fastf_t min, fastf_t max, int level)
{
    unsigned int vf = floor((val - min) / (max - min) * USHRT_MAX);
    int lv = floor(vf/double(PRECOMPUTED_MASKS[level]));
    unsigned int vc = ceil((val - min) / (max - min) * USHRT_MAX);
    int hc = ceil(vc/double(PRECOMPUTED_MASKS[level]));
    fastf_t v = ((fastf_t)lv + (fastf_t)hc)*0.5 * double(PRECOMPUTED_MASKS[level]);
    fastf_t vs = ((v / USHRT_MAX) * (max - min)) + min;
    return vs;
}

// Transfer coordinate into level-appropriate value
void
POPState::level_pnt(point_t *o, const point_t *p, int level)
{
    fastf_t nx = snap((*p)[X], minx, maxx, level);
    fastf_t ny = snap((*p)[Y], miny, maxy, level);
    fastf_t nz = snap((*p)[Z], minz, maxz, level);
    VSET(*o, nx, ny, nz);
#if 0
    double poffset = DIST_PNT_PNT(*o, *p);
    if (poffset > (maxx - minx) && poffset > (maxy - miny) && poffset > (maxz - minz)) {
	bu_log("Error: %f %f %f -> %f %f %f\n", V3ARGS(*p), V3ARGS(*o));
	bu_log("bound: %f %f %f -> %f %f %f\n", minx, miny, minz, maxx, maxy, maxz);
    }
#endif
}

// Compares two coordinates for equality (on a given precision level)
bool
POPState::is_equal(rec r1, rec r2, int level)
{
    bool tl_x = (to_level(r1.x, level) == to_level(r2.x, level));
    bool tl_y = (to_level(r1.y, level) == to_level(r2.y, level));
    bool tl_z = (to_level(r1.z, level) == to_level(r2.z, level));
    return (tl_x && tl_y && tl_z);
}

// Checks whether a triangle is degenerate (at least two coordinates are the
// same on the given precision level)
bool
POPState::tri_degenerate(rec r0, rec r1, rec r2, int level)
{
    return is_equal(r0, r1, level) || is_equal(r1, r2, level) || is_equal(r0, r2, level);
}

void
POPState::plot(const char *root)
{
    if (curr_level < 0)
	return;

    struct bu_vls name = BU_VLS_INIT_ZERO;
    FILE *plot_file = NULL;
    bu_vls_init(&name);
    if (!root) {
	bu_vls_sprintf(&name, "init_tris_level_%.2d.plot3", curr_level);
    } else {
	bu_vls_sprintf(&name, "%s_tris_level_%.2d.plot3", root, curr_level);
    }
    plot_file = fopen(bu_vls_addr(&name), "wb");

    if (curr_level <= max_pop_threshold_level) {
	pl_color(plot_file, 0, 255, 0);

	for (int i = 0; i <= curr_level; i++) {
	    std::vector<size_t>::iterator s_it;
	    for (s_it = level_tris[i].begin(); s_it != level_tris[i].end(); s_it++) {
		size_t f_ind = *s_it;
		int v1ind, v2ind, v3ind;
		if (faces_array) {
		    v1ind = faces_array[3*f_ind+0];
		    v2ind = faces_array[3*f_ind+1];
		    v3ind = faces_array[3*f_ind+2];
		} else {
		    v1ind = lod_tris[3*f_ind+0];
		    v2ind = lod_tris[3*f_ind+1];
		    v3ind = lod_tris[3*f_ind+2];
		}
		point_t p1, p2, p3, o1, o2, o3;
		if (verts_array) {
		    VMOVE(p1, verts_array[v1ind]);
		    VMOVE(p2, verts_array[v2ind]);
		    VMOVE(p3, verts_array[v3ind]);
		} else {
		    VSET(p1, lod_tri_pnts[3*v1ind+0], lod_tri_pnts[3*v1ind+1], lod_tri_pnts[3*v1ind+2]);
		    VSET(p2, lod_tri_pnts[3*v2ind+0], lod_tri_pnts[3*v2ind+1], lod_tri_pnts[3*v2ind+2]);
		    VSET(p3, lod_tri_pnts[3*v3ind+0], lod_tri_pnts[3*v3ind+1], lod_tri_pnts[3*v3ind+2]);
		}
		// We iterate over the level i triangles, but our target level is
		// curr_level so we "decode" the points to that level, NOT level i
		level_pnt(&o1, &p1, curr_level);
		level_pnt(&o2, &p2, curr_level);
		level_pnt(&o3, &p3, curr_level);
		pdv_3move(plot_file, o1);
		pdv_3cont(plot_file, o2);
		pdv_3cont(plot_file, o3);
		pdv_3cont(plot_file, o1);
	    }
	}

    } else {
	for (size_t i = 0; i < lod_tris.size() / 3; i++) {
	    int v1ind, v2ind, v3ind;
	    point_t p1, p2, p3;
	    v1ind = lod_tris[3*i+0];
	    v2ind = lod_tris[3*i+1];
	    v3ind = lod_tris[3*i+2];
	    VSET(p1, lod_tri_pnts[3*v1ind+0], lod_tri_pnts[3*v1ind+1], lod_tri_pnts[3*v1ind+2]);
	    VSET(p2, lod_tri_pnts[3*v2ind+0], lod_tri_pnts[3*v2ind+1], lod_tri_pnts[3*v2ind+2]);
	    VSET(p3, lod_tri_pnts[3*v3ind+0], lod_tri_pnts[3*v3ind+1], lod_tri_pnts[3*v3ind+2]);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p2);
	    pdv_3cont(plot_file, p3);
	    pdv_3cont(plot_file, p1);
	}
    }

    fclose(plot_file);

    bu_vls_free(&name);
}


extern "C" unsigned long long
bv_mesh_lod_cache(struct bv_mesh_lod_context *c, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, double fratio)
{
    unsigned long long key = 0;

    if (!c || !v || !vcnt || !faces || !fcnt)
	return 0;

    POPState p(c, v, vcnt, vn, faces, fcnt, user_key, fratio);
    if (!p.is_valid)
	return 0;

    key = p.hash;

    return key;
}

extern "C" struct bv_mesh_lod *
bv_mesh_lod_create(struct bv_mesh_lod_context *c, unsigned long long key)
{
    if (!c || !key)
	return NULL;

    POPState *p = new POPState(c, key);
    if (!p)
	return NULL;

    if (!p->is_valid) {
	delete p;
	return NULL;
    }

    // Set up info container
    struct bv_mesh_lod *lod;
    BU_GET(lod, struct bv_mesh_lod);
    BU_GET(lod->i, struct bv_mesh_lod_internal);
    ((struct bv_mesh_lod_internal *)lod->i)->s = p;
    lod->c = (void *)c;
    p->lod = lod;

    // Important - parent codes need to have a sense of the size of
    // the object, and we want that size to be consistent regardless
    // of the LoD actually in use.  Set the bbox dimensions at a level
    // where external codes can see and use them.
    VMOVE(lod->bmin, p->bbmin);
    VMOVE(lod->bmax, p->bbmax);

    return lod;
}

extern "C" void
bv_mesh_lod_destroy(struct bv_mesh_lod *lod)
{
    if (!lod)
	return;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)lod->i;
    delete i->s;
    i->s = NULL;
    BU_PUT(i, struct bv_mesh_lod_internal);
    lod->i = NULL;
    BU_PUT(lod, struct bv_mesh_lod);
}

struct lod_uv_fit {
    point2d_t shift = {0.0, 0.0};
    fastf_t weight = 0.0;
    fastf_t max_edge_sq = 0.0;
};

static uint64_t
lod_uv_vertex_key(int point, int uv)
{
    return ((uint64_t)(uint32_t)point << std::numeric_limits<uint32_t>::digits) | (uint32_t)uv;
}

const point2d_t *
bv_mesh_lod_snapped_uvs(struct bv_mesh_lod *lod)
{
    if (!lod || !lod->i || !lod->faces || !lod->face_sources || !lod->uvs || !lod->face_uvs ||
	!lod->points || !lod->points_orig || lod->fcnt <= 0)
	return NULL;

    auto &result = ((struct bv_mesh_lod_internal *)lod->i)->s->lod_corner_uvs;
    if (!result.empty())
	return (const point2d_t *)result.data();

    // Match the position and UV index together. Sharing a position alone
    // would let a seam's unrelated texture coordinates influence the fit.
    std::unordered_map<uint64_t, lod_uv_fit> fits;
    for (int i = 0; i < lod->fcnt; i++) {
	const int source = lod->face_sources[i];
	const int *face = &lod->faces[(size_t)i * 3];
	if (source < 0 || (size_t)source >= lod->uv_face_count ||
	    face[0] < 0 || face[0] >= lod->pcnt || face[1] < 0 ||
	    face[1] >= lod->pcnt || face[2] < 0 || face[2] >= lod->pcnt)
	    return NULL;

	const int *indices = &lod->face_uvs[(size_t)source * 3];
	const auto &a = lod->points_orig[face[0]];
	const auto &b = lod->points_orig[face[1]];
	const auto &c = lod->points_orig[face[2]];
	vect_t ab, ac, normal;
	VSUB2(ab, b, a);
	VSUB2(ac, c, a);
	VCROSS(normal, ab, ac);
	const fastf_t area_sq = MAGSQ(normal);
	const fastf_t edge_sq = std::max({MAGSQ(ab), MAGSQ(ac), DIST_PNT_PNT_SQ(b, c)});
	if (!(area_sq > SMALL_FASTF * edge_sq * edge_sq) || !std::isfinite(area_sq))
	    continue;

	// These vectors give the change in barycentric coordinates for a
	// displacement projected onto the original triangle's plane.
	vect_t dual_b, dual_c;
	VCROSS(dual_b, ac, normal);
	VCROSS(dual_c, normal, ab);
	vect2d_t uv_ab, uv_ac, uv_bc;
	V2SUB2(uv_ab, lod->uvs[indices[1]], lod->uvs[indices[0]]);
	V2SUB2(uv_ac, lod->uvs[indices[2]], lod->uvs[indices[0]]);
	V2SUB2(uv_bc, uv_ac, uv_ab);
	const fastf_t uv_edge_sq[] = {V2DOT(uv_ab, uv_ab), V2DOT(uv_bc, uv_bc), V2DOT(uv_ac, uv_ac)};

	// Weight by squared altitude so thin triangles with steep gradients
	// have less influence on a corner shared by better-conditioned faces.
	const fastf_t weight = area_sq / edge_sq;
	for (int j = 0; j < 3; j++) {
	    vect_t displacement;
	    VSUB2(displacement, lod->points[face[j]], lod->points_orig[face[j]]);
	    const fastf_t wb = VDOT(displacement, dual_b) / area_sq;
	    const fastf_t wc = VDOT(displacement, dual_c) / area_sq;
	    auto &fit = fits[lod_uv_vertex_key(face[j], indices[j])];
	    fit.shift[0] += weight * (wb * uv_ab[0] + wc * uv_ac[0]);
	    fit.shift[1] += weight * (wb * uv_ab[1] + wc * uv_ac[1]);
	    fit.weight += weight;
	    fit.max_edge_sq = std::max({fit.max_edge_sq, uv_edge_sq[j], uv_edge_sq[(j + 2) % 3]});
	}
    }

    // Allow local extrapolation but limit it to one incident UV edge length
    // to avoid long jumps across the texture atlas at coarser levels.
    for (auto &entry : fits) {
	auto &fit = entry.second;
	V2SCALE(fit.shift, fit.shift, 1.0 / fit.weight);
	const fastf_t move_sq = V2DOT(fit.shift, fit.shift);
	if (!std::isfinite(move_sq)) {
	    V2SET(fit.shift, 0.0, 0.0);
	} else if (move_sq > fit.max_edge_sq) {
	    V2SCALE(fit.shift, fit.shift, sqrt(fit.max_edge_sq / move_sq));
	}
    }

    result.resize((size_t)lod->fcnt * 3 * 2);
    for (int i = 0; i < lod->fcnt; i++) {
	const size_t offset = (size_t)i * 3;
	const int *indices = &lod->face_uvs[(size_t)lod->face_sources[i] * 3];
	for (int j = 0; j < 3; j++) {
	    const int uv = indices[j];
	    auto fit = fits.find(lod_uv_vertex_key(lod->faces[offset + j], uv));
	    fastf_t *out = &result[(offset + j) * 2];
	    V2MOVE(out, lod->uvs[uv]);
	    if (fit != fits.end())
		V2ADD2(out, out, fit->second.shift);
	}
    }
    return (const point2d_t *)result.data();
}

static void
dlist_stale(struct bv_scene_obj *s)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	dlist_stale(cg);
    }
    s->s_dlist_stale = 1;
}

extern "C" int
bv_mesh_lod_level(struct bv_scene_obj *s, int level, int reset)
{
    if (!s)
	return -1;

    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    if (!l)
	return -1;
    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)l->i;
    POPState *sp = i->s;
    if (level < 0)
	return sp->curr_level;

    int old_level = sp->curr_level;

    sp->force_update = (reset) ? true : false;
    sp->set_level(level);

    // If we're in POP territory use the local arrays - otherwise, they
    // were already set by the full detail callback.
    if (sp->curr_level <= sp->max_pop_threshold_level) {
	l->fcnt = (int)sp->lod_tris.size()/3;
	l->faces = sp->lod_tris.data();
	l->face_sources = sp->lod_tri_sources.data();
	l->points_orig = (const point_t *)sp->lod_tri_pnts.data();
	l->porig_cnt = (int)(sp->lod_tri_pnts.size()/3);
#if 0
	// TODO - there's still some error with normals - they seem to work,
	// but when zooming way out and back in (at least on Windows) we're
	// getting an access violation with some geometry...
	if (sp->lod_tri_norms.size() >= sp->lod_tris.size()) {
	    l->normals = (const vect_t *)sp->lod_tri_norms.data();
	} else {
	    l->normals = NULL;
	}
#else
	l->normals = NULL;
#endif
	l->points = sp->lod_tri_pnts_snapped.empty() ? NULL :
	    (const point_t *)sp->lod_tri_pnts_snapped.data();
	l->pcnt = (int)(sp->lod_tri_pnts_snapped.size()/3);
    } else {
	l->face_sources = NULL;
    }

    bv_log(2, "bv_mesh_lod_level %s[%d](%d): %d", bu_vls_cstr(&s->s_name), level, reset, l->fcnt);

    // If the data changed, any Display List we may have previously generated
    // is now obsolete
    if (old_level != sp->curr_level)
	dlist_stale(s);

    return sp->curr_level;
}


extern "C" int
bv_mesh_lod_view(struct bv_scene_obj *s, struct bview *v, int reset)
{
    if (!s || !v)
	return -1;
    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    if (!l)
	return -1;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)l->i;
    POPState *sp = i->s;
    int ret = sp->curr_level;
    int vscale = (int)((double)sp->get_level(v->gv_size) * v->gv_s->lod_scale);
    vscale = (vscale < 0) ? 0 : vscale;
    vscale = (vscale >= POP_MAXLEVEL) ? POP_MAXLEVEL-1 : vscale;
    if (s->s_texture && s->s_texture->rgb && s->s_os->s_dmode > 0 &&
	v->gv_s->lod_texture_error > 0.0)
	vscale = sp->texture_level(v, vscale);

    bv_log(2, "bv_mesh_lod_view %s[%s][%d]", bu_vls_cstr(&s->s_name), bu_vls_cstr(&v->gv_name), vscale);

    // If the object is not visible in the scene, don't change the data
    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(s->bmin), V3ARGS(s->bmax));
    if (_obj_visible(s, v))
	ret = bv_mesh_lod_level(s, vscale, reset);

    return ret;
}

extern "C" void
bv_mesh_lod_memshrink(struct bv_scene_obj *s)
{
    if (!s)
	return;
    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    if (!l)
	return;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)l->i;
    POPState *sp = i->s;
    if (sp->curr_level > sp->max_pop_threshold_level && sp->full_detail_clear_clbk)
	(*sp->full_detail_clear_clbk)(l, sp->detail_clbk_data);
    sp->shrink_memory();
    l->fcnt = l->pcnt = l->porig_cnt = 0;
    l->faces = l->face_sources = NULL;
    l->points = l->points_orig = NULL;
    l->normals = NULL;
}

static void
cache_del(struct bv_mesh_lod_context *c, unsigned long long hash, const char *component)
{
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    bu_cache_clear(keystr.c_str(), c->i->lod_cache, NULL);
}


extern "C" void
bv_mesh_lod_clear_cache(struct bv_mesh_lod_context *c, unsigned long long key)
{
    char dir[MAXPATHLEN];

    if (c && key) {
	// For this case, we're clearing the data associated with a
	// specific key (for example, if we're about to edit a BoT but
	// don't want to redo the whole database's cache.
	cache_del(c, key, CACHE_POP_MAX_LEVEL);
	cache_del(c, key, CACHE_POP_SWITCH_LEVEL);
	cache_del(c, key, CACHE_VERTEX_COUNT);
	cache_del(c, key, CACHE_TRI_COUNT);
	cache_del(c, key, CACHE_OBJ_BOUNDS);

	// The level-based components (v0..vN, t0..tN, vn0..vnN) use dynamic
	// suffixes - iterate all lod_cache keys and clear those matching this hash
	{
	    char **keysv = NULL;
	    int nkeys = bu_cache_keys(&keysv, c->i->lod_cache);
	    std::string prefix = std::to_string(key) + std::string(":");
	    for (int ki = 0; ki < nkeys; ki++) {
		if (bu_strncmp(keysv[ki], prefix.c_str(), prefix.length()) == 0)
		    bu_cache_clear(keysv[ki], c->i->lod_cache, NULL);
	    }
	    if (nkeys)
		bu_argv_free((size_t)nkeys, keysv);
	}

	// Iterate over the name/key mapper, removing anything with a value of key
	{
	    char **keysv = NULL;
	    int nkeys = bu_cache_keys(&keysv, c->i->name_cache);
	    for (int ki = 0; ki < nkeys; ki++) {
		void *data = NULL;
		size_t dsize = bu_cache_get(&data, keysv[ki], c->i->name_cache, NULL);
		if (dsize == sizeof(unsigned long long) && data) {
		    unsigned long long stored_key = *(unsigned long long *)data;
		    bu_free(data, "name cache data");
		    if (stored_key == key)
			bu_cache_clear(keysv[ki], c->i->name_cache, NULL);
		} else if (data) {
		    bu_free(data, "name cache data");
		}
	    }
	    if (nkeys)
		bu_argv_free((size_t)nkeys, keysv);
	}
	return;
    }

    if (c && !key) {
	// Clear all entries from both caches for this context
	{
	    char **keysv = NULL;
	    int nkeys = bu_cache_keys(&keysv, c->i->lod_cache);
	    for (int ki = 0; ki < nkeys; ki++)
		bu_cache_clear(keysv[ki], c->i->lod_cache, NULL);
	    if (nkeys)
		bu_argv_free((size_t)nkeys, keysv);
	}
	{
	    char **keysv = NULL;
	    int nkeys = bu_cache_keys(&keysv, c->i->name_cache);
	    for (int ki = 0; ki < nkeys; ki++)
		bu_cache_clear(keysv[ki], c->i->name_cache, NULL);
	    if (nkeys)
		bu_argv_free((size_t)nkeys, keysv);
	}
	return;
    }

    // Clear everything - nuke the whole POPLoD cache directory
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    bu_dirclear((const char *)dir);
}

extern "C" void
bv_mesh_lod_detail_setup_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *),
	void *clbk_data
	)
{
    if (!lod || !clbk)
	return;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_setup_clbk = clbk;
    s->detail_clbk_data = clbk_data;
}

extern "C" void
bv_mesh_lod_detail_clear_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_clear_clbk = clbk;
}

extern "C" void
bv_mesh_lod_detail_free_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bv_mesh_lod_internal *i = (struct bv_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_free_clbk = clbk;
}

void
bv_mesh_lod_free(struct bv_scene_obj *s)
{
    if (!s || !s->draw_data)
	return;
    bv_mesh_lod_destroy((struct bv_mesh_lod *)s->draw_data);
    s->draw_data = NULL;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
