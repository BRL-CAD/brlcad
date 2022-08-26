/*                       L O D . C P P
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
#include <stdlib.h>
#include <map>
#include <set>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h> /* for mkdir */
#endif

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include "lmdb.h"
}

#include "bio.h"

#include "bu/app.h"
#include "bu/bitv.h"
#include "bu/color.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bv/plot3.h"
#include "bg/lod.h"
#include "bg/plane.h"
#include "bg/sat.h"
#include "bg/trimesh.h"

// Number of levels of detail to define
#define POP_MAXLEVEL 16

// Subdirectory in BRL-CAD cache to hold this type of LoD data
#define POP_CACHEDIR ".POPLoD"

// Factor by which to bump out bounds to avoid points on box edges
#define MBUMP 1.01

// Maximum database size.  For detailed views we fall back on just
// displaying the full data set, and we need to be able to memory map the
// file, so go with a 4Gb per file limit.
#define CACHE_MAX_DB_SIZE 4294967296

// Define what format of the cache is current - if it doesn't match, we need
// to wipe and redo.
#define CACHE_CURRENT_FORMAT 1

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
lod_dir(char *dir)
{
#ifdef HAVE_WINDOWS_H
    CreateDirectory(dir, NULL);
#else
    /* mode: 775 */
    mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}

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
void
obb_arb(vect_t obb_center, vect_t obb_extent1, vect_t obb_extent2, vect_t obb_extent3)
{
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

    bu_log("center: %f %f %f\n", V3ARGS(obb_center));
    bu_log("e1: %f %f %f\n", V3ARGS(obb_extent1));
    bu_log("e2: %f %f %f\n", V3ARGS(obb_extent2));
    bu_log("e3: %f %f %f\n", V3ARGS(obb_extent3));

    bu_log("in obb.s arb8 %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
	    V3ARGS(arb[0]), V3ARGS(arb[1]), V3ARGS(arb[2]), V3ARGS(arb[3]), V3ARGS(arb[4]), V3ARGS(arb[5]), V3ARGS(arb  [6]), V3ARGS(arb[7]));
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
    bg_plane_closest_pt(&pu, &pv, p, ec);
    bg_plane_pt_at(&v->obb_center, p, pu, pv);

    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(v->obb_extent1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(v->obb_extent2, ep1, ec);
    VSUB2(v->obb_extent3, ep2, ec);

#if 0
    obb_arb(v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3);
#endif
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
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	obj_bb(&have_objs, &min, &max, g, v);
    }
    struct bu_ptbl *sol = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    if (so != sol) {
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
bg_view_bounds(struct bview *v)
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
bg_view_objs_select(struct bv_scene_obj ***sset, struct bview *v, int x, int y)
{
    if (!v || !sset || !v->gv_width || !v->gv_height)
	return 0;

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
    bg_plane_closest_pt(&pu, &pv, p, ec);
    bg_plane_pt_at(&obb_c, p, pu, pv);


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
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
    }
    if (active.size()) {
	std::set<struct bv_scene_obj *>::iterator a_it;
	(*sset) = (struct bv_scene_obj **)bu_calloc(active.size(), sizeof(struct bv_scene_obj *), "objs");
	int i = 0;
	for (a_it = active.begin(); a_it != active.end(); a_it++) {
	    (*sset)[i] = *a_it;
	    i++;
	}
    }

    return active.size();
}

int
bg_view_objs_rect_select(struct bv_scene_obj ***sset, struct bview *v, int x1, int y1, int x2, int y2)
{
    if (!v || !sset || !v->gv_width || !v->gv_height)
	return 0;

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
    bg_plane_closest_pt(&pu, &pv, p, ec);
    bg_plane_pt_at(&obb_c, p, pu, pv);


    // The first extent is just the scene radius in the lookat direction
    VSCALE(dir, dir, radius);
    VMOVE(obb_e1, dir);

    // The other two extents we find by subtracting the view center from the edge points
    VSUB2(obb_e2, ep1, ec);
    VSUB2(obb_e3, ep2, ec);

#if 0
    obb_arb(obb_c, obb_e1, obb_e2, obb_e3);
#endif

    // Having constructed the box, test the scene objects against it.  Any that intersect,
    // add them to the set
    std::set<struct bv_scene_obj *> active;
    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
    }
    if (active.size()) {
	std::set<struct bv_scene_obj *>::iterator a_it;
	(*sset) = (struct bv_scene_obj **)bu_calloc(active.size(), sizeof(struct bv_scene_obj *), "objs");
	int i = 0;
	for (a_it = active.begin(); a_it != active.end(); a_it++) {
	    (*sset)[i] = *a_it;
	    i++;
	}
    }

    return active.size();
}

static int
_obj_visible(struct bv_scene_obj *s, struct bview *v)
{
    if (bg_sat_aabb_obb(s->bmin, s->bmax, v->obb_center, v->obb_extent1, v->obb_extent2, v->obb_extent3))
	return 1;

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

struct bg_mesh_lod_context_internal {
    MDB_env *lod_env;
    MDB_txn *lod_txn;
    MDB_dbi lod_dbi;

    MDB_env *name_env;
    MDB_txn *name_txn;
    MDB_dbi name_dbi;

    struct bu_vls *fname;
};

struct bg_mesh_lod_context *
bg_mesh_lod_context_create(const char *name)
{
    size_t mreaders = 0;
    int ncpus = 0;
    if (!name)
	return NULL;

    // Hash the input filename to generate a key for uniqueness
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s", bu_path_normalize(name));
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    if (bu_vls_strlen(&fname) < 10) {
	bu_vls_printf(&fname, "GGGGGGGGGGGGG");
    }
    XXH64_update(&h_state, bu_vls_cstr(&fname), bu_vls_strlen(&fname)*sizeof(char));
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    bu_path_component(&fname, bu_path_normalize(name), BU_PATH_BASENAME_EXTLESS);
    bu_vls_printf(&fname, "_%llu", hash);

    // Create the context
    struct bg_mesh_lod_context *c;
    BU_GET(c, struct bg_mesh_lod_context);
    BU_GET(c->i, struct bg_mesh_lod_context_internal);
    struct bg_mesh_lod_context_internal *i = c->i;
    BU_GET(i->fname, struct bu_vls);
    bu_vls_init(i->fname);
    bu_vls_sprintf(i->fname, "%s", bu_vls_cstr(&fname));

    // Base maximum readers on an estimate of how many threads
    // we might want to fire off
    mreaders = std::thread::hardware_concurrency();
    if (!mreaders)
	mreaders = 1;
    ncpus = bu_avail_cpus();
    if (ncpus > 0 && (size_t)ncpus > mreaders)
	mreaders = (size_t)ncpus + 2;


    // Set up LMDB environments
    if (mdb_env_create(&i->lod_env))
	goto lod_context_fail;
    if (mdb_env_create(&i->name_env))
	goto lod_context_close_lod_fail;

    if (mdb_env_set_maxreaders(i->lod_env, mreaders))
	goto lod_context_close_fail;
    if (mdb_env_set_maxreaders(i->name_env, mreaders))
	goto lod_context_close_fail;

    // TODO - the "failure" mode if this limit is ever hit is to back down
    // the maximum stored LoD on larger objects, but that will take some
    // doing to implement...
    if (mdb_env_set_mapsize(i->lod_env, CACHE_MAX_DB_SIZE))
	goto lod_context_close_fail;

    if (mdb_env_set_mapsize(i->name_env, CACHE_MAX_DB_SIZE))
	goto lod_context_close_fail;


    // Ensure the necessary top level dirs are present
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, NULL);
    if (!bu_file_exists(dir, NULL))
	lod_dir(dir);
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    if (!bu_file_exists(dir, NULL)) {
	lod_dir(dir);
    }
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, "format", NULL);
    if (!bu_file_exists(dir, NULL)) {
	// Note a format, so we can detect if what's there isn't compatible
	// with what this logic expects (in anticipation of future changes
	// to the on-disk format).
	FILE *fp = fopen(dir, "w");
	if (!fp)
	    goto lod_context_close_fail;
	fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	fclose(fp);
    } else {
	std::ifstream format_file(dir);
	size_t disk_format_version = 0;
	format_file >> disk_format_version;
	format_file.close();
	if (disk_format_version	!= CACHE_CURRENT_FORMAT) {
	    bu_log("Old mesh lod cache (%zd) found - clearing\n", disk_format_version);
	    bg_mesh_lod_clear_cache(NULL, 0);
	}
	FILE *fp = fopen(dir, "w");
	if (!fp)
	    goto lod_context_close_fail;
	fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	fclose(fp);
    }

    // Create the specific LoD LMDB cache dir, if not already present
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&fname), NULL);
    if (!bu_file_exists(dir, NULL))
	lod_dir(dir);

    // Need to call mdb_env_sync() at appropriate points.
    if (mdb_env_open(i->lod_env, dir, MDB_NOSYNC, 0664))
	goto lod_context_close_fail;

    // Create the specific name/key LMDB mapping dir, if not already present
    bu_vls_printf(&fname, "_namekey");
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, bu_vls_cstr(&fname), NULL);
    if (!bu_file_exists(dir, NULL))
	lod_dir(dir);

    // Need to call mdb_env_sync() at appropriate points.
    if (mdb_env_open(i->name_env, dir, MDB_NOSYNC, 0664))
	goto lod_context_close_fail;

    // Success - return the context
    return c;

    // If something went wrong, clean up and return NULL
lod_context_close_fail:
    mdb_env_close(i->name_env);
lod_context_close_lod_fail:
    mdb_env_close(i->lod_env);
lod_context_fail:
    bu_vls_free(&fname);
    BU_PUT(c->i, struct bg_mesh_lod_context_internal);
    BU_PUT(c, struct bg_mesh_lod_context);
    return NULL;
}

void
bg_mesh_lod_context_destroy(struct bg_mesh_lod_context *c)
{
    if (!c)
	return;
    mdb_env_close(c->i->name_env);
    mdb_env_close(c->i->lod_env);
    bu_vls_free(c->i->fname);
    BU_PUT(c->i->fname, struct bu_vls);
    BU_PUT(c->i, struct bg_mesh_lod_context_internal);
    BU_PUT(c, struct bg_mesh_lod_context);
}

unsigned long long
bg_mesh_lod_key_get(struct bg_mesh_lod_context *c, const char *name)
{
    MDB_val mdb_key, mdb_data;

    // Database object names may be of arbitrary length - hash
    // to get the lookup key
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&keystr, "%s", name);
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    if (bu_vls_strlen(&keystr) < 10) {
	bu_vls_printf(&keystr, "GGGGGGGGGGGGG");
    }
    XXH64_update(&h_state, bu_vls_cstr(&keystr), bu_vls_strlen(&keystr)*sizeof(char));
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    bu_vls_sprintf(&keystr, "%llu", hash);

    mdb_txn_begin(c->i->name_env, NULL, 0, &c->i->name_txn);
    mdb_dbi_open(c->i->name_txn, NULL, 0, &c->i->name_dbi);
    mdb_key.mv_size = bu_vls_strlen(&keystr)*sizeof(char);
    mdb_key.mv_data = (void *)bu_vls_cstr(&keystr);
    int rc = mdb_get(c->i->name_txn, c->i->name_dbi, &mdb_key, &mdb_data);
    if (rc) {
	mdb_txn_commit(c->i->name_txn);
	return 0;
    }
    unsigned long long *fkeyp = (unsigned long long *)mdb_data.mv_data;
    unsigned long long fkey = *fkeyp;
    mdb_txn_commit(c->i->name_txn);

    bu_vls_free(&keystr);
    //bu_log("GOT %s: %llu\n", name, fkey);
    return fkey;
}

int
bg_mesh_lod_key_put(struct bg_mesh_lod_context *c, const char *name, unsigned long long key)
{
    // Database object names may be of arbitrary length - hash
    // to get something appropriate for a lookup key
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    struct bu_vls keystr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&keystr, "%s", name);
    // TODO - xxhash needs a minimum input size per Coverity - figure out what it is...
    if (bu_vls_strlen(&keystr) < 10) {
	bu_vls_printf(&keystr, "GGGGGGGGGGGGG");
    }
    XXH64_update(&h_state, bu_vls_cstr(&keystr), bu_vls_strlen(&keystr)*sizeof(char));
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    bu_vls_sprintf(&keystr, "%llu", hash);

    MDB_val mdb_key;
    MDB_val mdb_data[2];
    mdb_txn_begin(c->i->name_env, NULL, 0, &c->i->name_txn);
    mdb_dbi_open(c->i->name_txn, NULL, 0, &c->i->name_dbi);
    mdb_key.mv_size = bu_vls_strlen(&keystr)*sizeof(char);
    mdb_key.mv_data = (void *)bu_vls_cstr(&keystr);
    mdb_data[0].mv_size = sizeof(key);
    mdb_data[0].mv_data = (void *)&key;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    int rc = mdb_put(c->i->name_txn, c->i->name_dbi, &mdb_key, mdb_data, 0);
    mdb_txn_commit(c->i->name_txn);

    bu_vls_free(&keystr);
    //bu_log("PUT %s: %llu\n", name, key);
    return rc;
}

// Output record
class rec {
    public:
	unsigned short x = 0, y = 0, z = 0;
};


class POPState;
struct bg_mesh_lod_internal {
    POPState *s;
};

class POPState {
    public:

	// Create cached data (doesn't create a usable container)
	POPState(struct bg_mesh_lod_context *ctx, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_face_cnt_threshold_ratio);

	// Load cached data (DOES create a usable container)
	POPState(struct bg_mesh_lod_context *ctx, unsigned long long key);

	// Cleanup
	~POPState();

	// Based on a view size, recommend a level
	int get_level(fastf_t len);

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
	MDB_val mdb_key, mdb_data[2];

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
	struct bg_mesh_lod_context *c;
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

POPState::POPState(struct bg_mesh_lod_context *ctx, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, fastf_t pop_facecnt_threshold_ratio)
{
    // Store the context
    c = ctx;

    // Caller set parameter telling us when to switch from POP data
    // to just drawing the full mesh
    max_face_ratio = pop_facecnt_threshold_ratio;

    // Hash the data to generate a key, if the user didn't supply us with one
    if (!user_key) {
	XXH64_state_t h_state;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, v, vcnt*sizeof(point_t));
	XXH64_update(&h_state, faces, 3*fcnt*sizeof(int));
	XXH64_hash_t hash_val;
	hash_val = XXH64_digest(&h_state);
	hash = (unsigned long long)hash_val;
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

POPState::POPState(struct bg_mesh_lod_context *ctx, unsigned long long key)
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
	if (bsize != level_tricnt[i]*3*sizeof(int)) {
	    bu_log("Incorrect data size found loading level %d tri data\n", i);
	    return;
	}
	lod_tris.insert(lod_tris.end(), &b[0], &b[level_tricnt[i]*3]);
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
	    return lev;
	}
    }
    return POP_MAXLEVEL - 1;
}

void
POPState::set_level(int level)
{
    // If we're already there and we're not undoing a memshrink, no work to do
    if (level == curr_level && !force_update)
	return;

    // If we're doing a forced update, it's like starting from
    // scratch - reset to 0
    if (force_update) {
	force_update = false;
	set_level(0);
	set_level(level);
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
	lod_tri_pnts_snapped.clear();
	lod_tri_pnts_snapped.shrink_to_fit();
	lod_tri_pnts.clear();
	lod_tri_pnts.shrink_to_fit();
	lod_tri_norms.clear();
	lod_tri_norms.shrink_to_fit();
	lod_tris.clear();
	lod_tris.shrink_to_fit();

	// Use the callback to set up the full data pointers
	if (full_detail_setup_clbk)
	    (*full_detail_setup_clbk)(lod, detail_clbk_data);
    }

    //elapsed = bu_gettime() - start;
    //seconds = elapsed / 1000000.0;
    //bu_log("lod set_level(%d): %f sec\n", level, seconds);

    curr_level = level;
}

// Rather than committing all data to LMDB in one transaction, use keys with
// appended strings to the hash to denote the individual pieces - basically
// what we were doing with files, but in the db instead
//
// This will also allow easier removal of larger subcomponents if we need to
// back off on saved LoD.
bool
POPState::cache_write(const char *component, std::stringstream &s)
{
    // Prepare inputs for writing
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    std::string buffer = s.str();

    // As implemented this shouldn't be necessary, since all our keys are below
    // the default size limit (511)
    //if (keystr.length()*sizeof(char) > mdb_env_get_maxkeysize(c->i->lod_env))
    //	return false;

    // Write out key/value to LMDB database, where the key is the hash
    // and the value is the serialized LoD data
    char *keycstr = bu_strdup(keystr.c_str());
    void *bdata = bu_calloc(buffer.length()+1, sizeof(char), "bdata");
    memcpy(bdata, buffer.data(), buffer.length()*sizeof(char));
    mdb_txn_begin(c->i->lod_env, NULL, 0, &c->i->lod_txn);
    mdb_dbi_open(c->i->lod_txn, NULL, 0, &c->i->lod_dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keycstr;
    mdb_data[0].mv_size = buffer.length()*sizeof(char);
    mdb_data[0].mv_data = bdata;
    mdb_data[1].mv_size = 0;
    mdb_data[1].mv_data = NULL;
    int rc = mdb_put(c->i->lod_txn, c->i->lod_dbi, &mdb_key, mdb_data, 0);
    mdb_txn_commit(c->i->lod_txn);
    bu_free(keycstr, "keycstr");
    bu_free(bdata, "buffer data");

    return (!rc) ? true : false;
}

// This pulls the data, but doesn't close the transaction because the
// calling code will want to manipulate the data.  After that process
// is complete, cache_done() should be called to prepare for subsequent
// operations.
size_t
POPState::cache_get(void **data, const char *component)
{
    // Construct lookup key
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    // As implemented this shouldn't be necessary, since all our keys are below
    // the default size limit (511)
    //if (keystr.length()*sizeof(char) > mdb_env_get_maxkeysize(c->i->lod_env))
    //	return 0;
    char *keycstr = bu_strdup(keystr.c_str());
    mdb_txn_begin(c->i->lod_env, NULL, 0, &c->i->lod_txn);
    mdb_dbi_open(c->i->lod_txn, NULL, 0, &c->i->lod_dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keycstr;
    int rc = mdb_get(c->i->lod_txn, c->i->lod_dbi, &mdb_key, &mdb_data[0]);
    if (rc) {
	bu_free(keycstr, "keycstr");
	(*data) = NULL;
	return 0;
    }
    bu_free(keycstr, "keycstr");
    (*data) = mdb_data[0].mv_data;

    return mdb_data[0].mv_size;
}

void
POPState::cache_done()
{
    mdb_txn_commit(c->i->lod_txn);
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
		int vt[3];
		vt[0] = (int)tri_ind_map[faces_array[3*(*s_it)+0]];
		vt[1] = (int)tri_ind_map[faces_array[3*(*s_it)+1]];
		vt[2] = (int)tri_ind_map[faces_array[3*(*s_it)+2]];
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
bg_mesh_lod_cache(struct bg_mesh_lod_context *c, const point_t *v, size_t vcnt, const vect_t *vn, int *faces, size_t fcnt, unsigned long long user_key, double fratio)
{
    unsigned long long key = 0;

    if (!v || !vcnt || !faces || !fcnt)
	return 0;

    POPState p(c, v, vcnt, vn, faces, fcnt, user_key, fratio);
    if (!p.is_valid)
	return 0;

    key = p.hash;

    return key;
}


extern "C" unsigned long long
bg_mesh_lod_custom_key(void *data, size_t data_size)
{
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, data, data_size);
    XXH64_hash_t hash_val;
    hash_val = XXH64_digest(&h_state);
    unsigned long long hash = (unsigned long long)hash_val;
    return hash; 
}


extern "C" struct bv_mesh_lod *
bg_mesh_lod_create(struct bg_mesh_lod_context *c, unsigned long long key)
{
    if (!key)
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
    BU_GET(lod->i, struct bg_mesh_lod_internal);
    ((struct bg_mesh_lod_internal *)lod->i)->s = p;
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
bg_mesh_lod_destroy(struct bv_mesh_lod *lod)
{
    if (!lod)
	return;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)lod->i;
    delete i->s;
    i->s = NULL;
    BU_PUT(i, struct bg_mesh_lod_internal);
    lod->i = NULL;
    BU_PUT(lod, struct bv_mesh_lod);
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
bg_mesh_lod_level(struct bv_scene_obj *s, int level, int reset)
{
    if (!s)
	return -1;

    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)l->i;
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
	l->points_orig = (const point_t *)sp->lod_tri_pnts.data();
	l->porig_cnt = (int)sp->lod_tri_pnts.size();
	if (sp->lod_tri_norms.size()) {
	    l->normals = (const vect_t *)sp->lod_tri_norms.data();
	} else {
	    l->normals = NULL;
	}
	l->points = (const point_t *)sp->lod_tri_pnts_snapped.data();
	l->pcnt = (int)sp->lod_tri_pnts_snapped.size();
    }

    // If the data changed, any Display List we may have previously generated
    // is now obsolete
    if (old_level != sp->curr_level)
	dlist_stale(s);

    return sp->curr_level;
}


extern "C" int
bg_mesh_lod_view(struct bv_scene_obj *s, struct bview *v, int reset)
{
    if (!s || !v)
	return -1;
    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    if (!l)
	return -1;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)l->i;
    POPState *sp = i->s;
    int ret = sp->curr_level;
    int vscale = (int)((double)sp->get_level(v->gv_size) * v->gv_s->lod_scale);
    vscale = (vscale < 0) ? 0 : vscale;
    vscale = (vscale >= POP_MAXLEVEL) ? POP_MAXLEVEL-1 : vscale;

    // If the object is not visible in the scene, don't change the data
    //bu_log("min: %f %f %f max: %f %f %f\n", V3ARGS(s->bmin), V3ARGS(s->bmax));
    if (_obj_visible(s, v))
	ret = bg_mesh_lod_level(s, vscale, reset);

    return ret;
}

extern "C" void
bg_mesh_lod_memshrink(struct bv_scene_obj *s)
{
    if (!s)
	return;
    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    if (!l)
	return;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)l->i;
    POPState *sp = i->s;
    sp->shrink_memory();
    bu_log("memshrink\n");
}

static void
bg_clear(const char *d)
{
    if (bu_file_directory(d)) {
	char **filenames;
	size_t nfiles = bu_file_list(d, "*", &filenames);
	for (size_t i = 0; i < nfiles; i++) {
	    if (BU_STR_EQUAL(filenames[i], "."))
		continue;
	    if (BU_STR_EQUAL(filenames[i], ".."))
		continue;
	    char cdir[MAXPATHLEN] = {0};
	    bu_dir(cdir, MAXPATHLEN, d, filenames[i], NULL);
	    bg_clear((const char *)cdir);
	}
	bu_argv_free(nfiles, filenames);
    }
    bu_file_delete(d);
}

static void
cache_del(struct bg_mesh_lod_context *c, unsigned long long hash, const char *component)
{
    // Construct lookup key
    MDB_val mdb_key;
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    mdb_txn_begin(c->i->lod_env, NULL, 0, &c->i->lod_txn);
    mdb_dbi_open(c->i->lod_txn, NULL, 0, &c->i->lod_dbi);
    mdb_key.mv_size = keystr.length()*sizeof(char);
    mdb_key.mv_data = (void *)keystr.c_str();
    mdb_del(c->i->lod_txn, c->i->lod_dbi, &mdb_key, NULL);
    mdb_txn_commit(c->i->lod_txn);
}


extern "C" void
bg_mesh_lod_clear_cache(struct bg_mesh_lod_context *c, unsigned long long key)
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
	cache_del(c, key, CACHE_VERT_LEVEL);
	cache_del(c, key, CACHE_VERTNORM_LEVEL);
	cache_del(c, key, CACHE_TRI_LEVEL);

	// Iterate over the name/key mapper, removing anything with a value
	// of key
	MDB_val mdb_key, mdb_data;
	unsigned long long *fkeyp = NULL;
	unsigned long long fkey = 0;
	mdb_txn_begin(c->i->name_env, NULL, 0, &c->i->name_txn);
	mdb_dbi_open(c->i->name_txn, NULL, 0, &c->i->name_dbi);
	MDB_cursor *cursor;
	int rc = mdb_cursor_open(c->i->name_txn, c->i->name_dbi, &cursor);
	if (rc) {
	    mdb_txn_commit(c->i->name_txn);
	    return;
	}
	rc = mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_FIRST);
	if (rc) {
	    mdb_txn_commit(c->i->name_txn);
	    return;
	}
	fkeyp = (unsigned long long *)mdb_data.mv_data;
	fkey = *fkeyp;
	if (fkey == key)
	    mdb_cursor_del(cursor, 0);
	while (!mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_NEXT)) {
	    fkeyp = (unsigned long long *)mdb_data.mv_data;
	    fkey = *fkeyp;
	    if (fkey == key)
		mdb_cursor_del(cursor, 0);
	}
	mdb_txn_commit(c->i->name_txn);
	return;
    }

    if (c && !key) {

	MDB_val mdb_key, mdb_data;
	MDB_cursor *cursor;
	int rc;

	// Clear the actual LoD data
	mdb_txn_begin(c->i->lod_env, NULL, 0, &c->i->lod_txn);
	mdb_dbi_open(c->i->lod_txn, NULL, 0, &c->i->lod_dbi);
	rc = mdb_cursor_open(c->i->lod_txn, c->i->lod_dbi, &cursor);
	if (rc) {
	    mdb_txn_commit(c->i->lod_txn);
	    return;
	}
	rc = mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_FIRST);
	if (rc) {
	    mdb_txn_commit(c->i->lod_txn);
	    return;
	}
	mdb_cursor_del(cursor, 0);
	while (!mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_NEXT))
	    mdb_cursor_del(cursor, 0);
	mdb_txn_commit(c->i->lod_txn);

	// Iterate over the name/key mapper, removing anything with a value
	// of key
	mdb_txn_begin(c->i->name_env, NULL, 0, &c->i->name_txn);
	mdb_dbi_open(c->i->name_txn, NULL, 0, &c->i->name_dbi);
	rc = mdb_cursor_open(c->i->name_txn, c->i->name_dbi, &cursor);
	if (rc) {
	    mdb_txn_commit(c->i->name_txn);
	    return;
	}
	rc = mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_FIRST);
	if (rc) {
	    mdb_txn_commit(c->i->name_txn);
	    return;
	}
	mdb_cursor_del(cursor, 0);
	while (!mdb_cursor_get(cursor, &mdb_key, &mdb_data, MDB_NEXT))
	    mdb_cursor_del(cursor, 0);
	mdb_txn_commit(c->i->name_txn);

	return;
    }

    // Clear everything
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, POP_CACHEDIR, NULL);
    bg_clear((const char *)dir);
}

extern "C" void
bg_mesh_lod_detail_setup_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *),
	void *clbk_data
	)
{
    if (!lod || !clbk)
	return;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_setup_clbk = clbk;
    s->detail_clbk_data = clbk_data;
}

extern "C" void
bg_mesh_lod_detail_clear_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_clear_clbk = clbk;
}

extern "C" void
bg_mesh_lod_detail_free_clbk(
	struct bv_mesh_lod *lod,
	int (*clbk)(struct bv_mesh_lod *, void *)
	)
{
    if (!lod || !clbk)
	return;

    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)lod->i;
    POPState *s = i->s;
    s->full_detail_free_clbk = clbk;
}

void
bg_mesh_lod_free(struct bv_scene_obj *s)
{
    if (!s || !s->draw_data)
	return;
    struct bv_mesh_lod *l = (struct bv_mesh_lod *)s->draw_data;
    struct bg_mesh_lod_internal *i = (struct bg_mesh_lod_internal *)l->i;
    delete i->s;
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
