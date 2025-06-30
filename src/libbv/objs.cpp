/*                      O B J S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file util.cpp
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"

#include <set>

#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bg/sat.h"
#include "bv/defines.h"
#include "bv/lod.h"
#include "bv/objs.h"
#include "bv/snap.h"
#include "bv/util.h"
#include "bv/vlist.h"
#include "./bv_private.h"

int
bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src)
{
    int ret = 0;
    if (!dest || !src)
	return ret;

    if (dest->s_line_width != src->s_line_width) {
	dest->s_line_width = src->s_line_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_length, src->s_arrow_tip_length, SMALL_FASTF)) {
	dest->s_arrow_tip_length = src->s_arrow_tip_length;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_width, src->s_arrow_tip_width, SMALL_FASTF)) {
	dest->s_arrow_tip_width = src->s_arrow_tip_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->transparency, src->transparency, SMALL_FASTF)) {
	dest->transparency = src->transparency;
	ret = 1;
    }
    if (dest->s_dmode != src->s_dmode) {
	dest->s_dmode = src->s_dmode;
	ret = 1;
    }
    if (dest->color_override != src->color_override) {
	dest->color_override = src->color_override;
	ret = 1;
    }
    if (!VNEAR_EQUAL(dest->color, src->color, SMALL_FASTF)) {
	VMOVE(dest->color, src->color);
	ret = 1;
    }
    if (dest->draw_solid_lines_only != src->draw_solid_lines_only) {
	dest->draw_solid_lines_only = src->draw_solid_lines_only;
	ret = 1;
    }
    if (dest->draw_non_subtract_only != src->draw_non_subtract_only) {
	dest->draw_non_subtract_only = src->draw_non_subtract_only;
	ret = 1;
    }

    return ret;
}

struct bv_obj_pool *
bv_obj_pool_create()
{
    struct bv_obj_pool *p;
    BU_GET(p, struct bv_obj_pool);
    BU_GET(p->i, struct bv_obj_pool_internal);
    BU_LIST_INIT(&p->i->head.l);
    BU_LIST_INIT(&p->i->vlfree);
    return p;
}

void
bv_obj_pool_destroy(struct bv_obj_pool *p)
{
    if (!p)
	return;

    /* Free scene objects */
    struct bv_scene_obj *sp = BU_LIST_NEXT(bv_scene_obj, &p->i->head.l);
    while (BU_LIST_NOT_HEAD(sp, &p->i->head.l)) {
	struct bv_scene_obj *nsp = BU_LIST_PNEXT(bv_scene_obj, sp);
	BU_LIST_DEQUEUE(&((sp)->l));
	sp->obj_pool = NULL;
	bv_obj_put(sp);
	sp = nsp;
    }

    /* Free vlists */
    struct bu_list *vp;
    while (BU_LIST_WHILE(vp, bu_list, &p->i->vlfree)) {
	BU_LIST_DEQUEUE(vp);
	struct bv_vlist *pv = (struct bv_vlist *)vp;
	BU_FREE(pv, struct bv_vlist);
    }

    /* Final cleanup */
    BU_PUT(p->i, struct bv_obj_pool_internal);
    BU_PUT(p, struct bv_obj_pool);
}

struct bv_scene_obj *
bv_obj_pool_get(struct bv_obj_pool *p)
{
    if (UNLIKELY(!p))
	return NULL;

    struct bv_scene_obj *s = BU_LIST_FIRST(bv_scene_obj, &p->i->head.l);
    if (BU_LIST_IS_HEAD(&s->l, &p->i->head.l)) {
	s = bv_obj_get(NULL);
    } else {
	BU_LIST_DEQUEUE(&(s->l));
    }

    s->obj_pool = p;
    s->vlfree = &p->i->vlfree;
    return s;
}

void
bv_obj_pool_put(struct bv_obj_pool *p, struct bv_scene_obj *s)
{
    if (UNLIKELY(!p || !s))
	return;

    s->obj_pool = NULL;
    bv_obj_reset(s);
    BU_LIST_APPEND(&s->l, &(p->i->head.l));
}

struct bv_scene_obj *
bv_obj_get(struct bv_obj_pool *p)
{
    struct bv_scene_obj *s = NULL;
    if (!p) {
	BU_ALLOC(s, struct bv_scene_obj);
	s->l.magic = BV_SOBJ_MAGIC;

	// Zero out callback pointers
	s->s_type_flags = 0;
	s->s_free_callback = NULL;
	s->s_dlist_free_callback = NULL;

	// Use reset to do most of the initialization
	bv_obj_reset(s);
    } else {
	s = bv_obj_pool_get(p);
    }

    return s;
}

struct bv_scene_obj *
bv_obj_get_child(struct bv_scene_obj *sp)
{
    if (!sp)
	return NULL;

    bv_log(1, "bv_obj_create_child %s(%s)", bu_vls_cstr(&sp->s_name), bu_vls_cstr(&sp->s_v->gv_name));

    // Children use their parent's info
    struct bv_scene_obj *s = bv_obj_pool_get(sp->obj_pool);
    s->s_v = sp->s_v;
    s->dp = sp->dp;

    // Assign child name
    bu_vls_sprintf(&s->s_name, "child:%s:%zd", bu_vls_cstr(&sp->s_name), BU_PTBL_LEN(&sp->children));

    // Tell sp about the child
    bu_ptbl_ins(&sp->children, (long *)s);

    return s;
}

struct bv_scene_obj *
bv_obj_find_child(struct bv_scene_obj *s, const char *vname)
{
    if (!s || !vname || !BU_PTBL_IS_INITIALIZED(&s->children))
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (!bu_path_match(vname, bu_vls_cstr(&s_c->s_name), 0))
	    return s_c;
    }

    return NULL;
}

void
bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src)
{
    bv_obj_settings_sync(dest->s_os, src->s_os);
    VMOVE(dest->s_center, src->s_center);
    VMOVE(dest->s_color, src->s_color);
    VMOVE(dest->bmin, src->bmin);
    VMOVE(dest->bmax, src->bmax);
    MAT_COPY(dest->s_mat, src->s_mat);
    dest->s_size = src->s_size;
    dest->s_soldash = src->s_soldash;
    dest->s_arrow = src->s_arrow;
    dest->adaptive_wireframe = src->adaptive_wireframe;
    dest->view_scale = src->view_scale;
    dest->bot_threshold = src->bot_threshold;
    dest->curve_scale = src->curve_scale;
    dest->point_scale = src->point_scale;
}

void
bv_obj_reset(struct bv_scene_obj *s)
{
    // handle children
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    bv_obj_put(s_c);
	}
    } else {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);


    // References are not cleaned up themselves, but we do clear
    // the table
    if (!BU_PTBL_IS_INITIALIZED(&s->obj_refs)) {
	BU_PTBL_INIT(&s->obj_refs);
    }
    bu_ptbl_reset(&s->obj_refs);

    // If we have a callback for the internal data, use it
    if (s->s_free_callback)
	(*s->s_free_callback)(s);
    s->s_free_callback = NULL;

    // If we have a callback for the display list data, use it
    if (s->s_dlist_free_callback)
	(*s->s_dlist_free_callback)(s);
    s->s_dlist_free_callback = NULL;

    // Free vlist data.  If we have an associated vlfree list for
    // reuse use that, otherwise free it for real.
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	if (s->vlfree) {
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	} else {
	    struct bu_list *p;
	    while (BU_LIST_WHILE(p, bu_list, &s->s_vlist)) {
		BU_LIST_DEQUEUE(p);
		struct bv_vlist *pv = (struct bv_vlist *)p;
		BU_FREE(pv, struct bv_vlist);
	    }

	}
    }
    BU_LIST_INIT(&(s->s_vlist));

    if (!BU_VLS_IS_INITIALIZED(&s->s_name))
	BU_VLS_INIT(&s->s_name);
    bu_vls_trunc(&s->s_name, 0);

    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->s_local_os, &defaults);
    s->s_os = &s->s_local_os;
    s->s_override_obj_ref_settings = 0;

    MAT_IDN(s->s_mat);
    VSET(s->s_color, 255, 0, 0);
    VSETALL(s->bmax, -INFINITY);
    VSETALL(s->bmin, INFINITY);
    VSETALL(s->s_center, 0);
    s->adaptive_wireframe = 0;
    s->bot_threshold = 0;
    s->csg_obj = 0;
    s->current = 0;
    s->curve_scale = 0;
    s->dp = NULL;
    s->draw_data = NULL;
    s->have_bbox = 0;
    s->mesh_obj = 0;
    s->point_scale = 0;
    s->s_arrow = 0;
    s->s_csize = 0;
    s->s_flag = UP;
    s->s_force_draw = 0;
    s->s_i_data = NULL;
    s->s_iflag = DOWN;
    s->s_path = NULL;
    s->s_size = 0;
    s->s_soldash = 0;
    s->s_update_callback = NULL;
    s->s_v = NULL;
    s->view_scale = 0;

    // Set timestamp
    s->timestamp = bu_gettime();
}

void
bv_obj_put(struct bv_scene_obj *s)
{
    bv_log(1, "bv_obj_put %s[%s]", bu_vls_cstr(&s->s_name), (s->s_v) ? bu_vls_cstr(&s->s_v->gv_name) : "NULL");
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	bv_obj_put(cg);
    }

    // If this object was selected for snapping, it is no longer a valid candidate
    // TODO - manage this at a higher level
    if (s->s_v)
	bu_ptbl_rm(&s->s_v->gv_s->gv_snap_objs, (long *)s);

    if (s->obj_pool) {
	bv_obj_pool_put(s->obj_pool, s);
    } else {
	bv_obj_reset(s);
	bu_vls_free(&s->s_name);
	BU_FREE(s, struct bv_scene_obj);
    }
}

int
bv_obj_bound(struct bv_scene_obj *s)
{
    int cmd;
    VSET(s->bmin, INFINITY, INFINITY, INFINITY);
    VSET(s->bmax, -INFINITY, -INFINITY, -INFINITY);
    int calc = 0;
    if (s->s_type_flags & BV_MESH_LOD) {
	struct bv_lod_mesh *i = (struct bv_lod_mesh *)s->draw_data;
	if (i) {
	    point_t obmin, obmax;
	    VMOVE(obmin, i->bmin);
	    VMOVE(obmax, i->bmax);
	    // Apply the scene matrix to the bounding box values to bound this
	    // instance, since the BV_MESH_LOD data is based on the
	    // non-instanced mesh.
	    MAT4X3PNT(s->bmin, s->s_mat, obmin);
	    MAT4X3PNT(s->bmax, s->s_mat, obmax);
	    calc = 1;
	}
    } else if (bu_list_len(&s->s_vlist)) {
	int dismode;
	cmd = bv_vlist_bbox(&s->s_vlist, &s->bmin, &s->bmax, NULL, &dismode);
	if (cmd) {
	    bu_log("unknown vlist op %d\n", cmd);
	}
	s->s_displayobj = dismode;
	calc = 1;
    }
    if (calc) {
	s->s_center[X] = (s->bmin[X] + s->bmax[X]) * 0.5;
	s->s_center[Y] = (s->bmin[Y] + s->bmax[Y]) * 0.5;
	s->s_center[Z] = (s->bmin[Z] + s->bmax[Z]) * 0.5;

	s->s_size = s->bmax[X] - s->bmin[X];
	V_MAX(s->s_size, s->bmax[Y] - s->bmin[Y]);
	V_MAX(s->s_size, s->bmax[Z] - s->bmin[Z]);

	return 1;
    }

    return 0;
}

int
bv_illum_obj(struct bv_scene_obj *s, char ill_state)
{
    bool changed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	int cchanged = bv_illum_obj(s_c, ill_state);
	if (cchanged)
	    changed = 1;
    }
    if (ill_state != s->s_iflag) {
	changed = 1;
	s->s_iflag = ill_state;
    }

    return changed;
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
obj_bb(int *have_objs, vect_t *min, vect_t *max, struct bv_scene_obj *s)
{
    vect_t minus, plus;
    if (bv_obj_bound(s)) {
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
	obj_bb(have_objs, min, max, sc);
    }
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
_scene_radius(point_t *sbbc, fastf_t *radius, const struct bu_ptbl *so)
{
    if (!sbbc || !radius || !so)
	return;
    VSET(*sbbc, 0, 0, 0);
    *radius = 1.0;
    vect_t min, max, work;
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);
    int have_objs = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *g = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	obj_bb(&have_objs, &min, &max, g);
    }
    if (have_objs) {
	VADD2SCALE(*sbbc, max, min, 0.5);
	VSUB2SCALE(work, max, min, 0.5);
	(*radius) = MAGNITUDE(work);
    }
}

void
bv_view_bounds(struct bview *v, const struct bu_ptbl *so)
{
    if (!v || !v->gv_width || !v->gv_height || !so)
	return;

    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, so);

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
	bv_obj_bound(s);
	if (bg_sat_aabb_obb(s->bmin, s->bmax, obb_c, obb_e1, obb_e2, obb_e3))
	    active.insert(s);
    }
}

int
bv_view_objs_select(struct bu_ptbl *sset, struct bview *v, struct bu_ptbl *so, int x, int y)
{
    if (!v || !sset || !v->gv_width || !v->gv_height || !so)
	return 0;

    bu_ptbl_reset(sset);

    if (x < 0 || y < 0 || x > v->gv_width || y > v->gv_height)
	return 0;


    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, so);

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
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
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
bv_view_objs_rect_select(struct bu_ptbl *sset, struct bview *v, struct bu_ptbl *so, int x1, int y1, int x2, int y2)
{
    if (!v || !sset || !v->gv_width || !v->gv_height || !so)
	return 0;

    bu_ptbl_reset(sset);

    if (x1 < 0 || y1 < 0 || x1 > v->gv_width || y1 > v->gv_height)
	return 0;

    if (x2 < 0 || y2 < 0 || x2 > v->gv_width || y2 > v->gv_height)
	return 0;

    // Get the radius of the scene.
    point_t sbbc = VINIT_ZERO;
    fastf_t radius = 1.0;
    _scene_radius(&sbbc, &radius, so);

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
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_find_active_objs(active, s, v, obb_c, obb_e1, obb_e2, obb_e3);
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
bv_view_obj_vis(const struct bview *v, const struct bv_scene_obj *s)
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



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
