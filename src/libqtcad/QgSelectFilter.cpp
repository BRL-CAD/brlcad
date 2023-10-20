/*                 Q G S E L E C T F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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
/** @file QgSelectFilter.cpp
 *
 * Graphical selection tool for Qt views.
 *
 */

#include "common.h"

extern "C" {
#include "bu/malloc.h"
#include "bg/aabb_ray.h"
#include "bg/lod.h"
#include "bg/polygon.h"
#include "bv.h"
#include "raytrace.h"
}

#include <unordered_set>
#include "qtcad/QgSelectFilter.h"
#include "qtcad/QgSignalFlags.h"

// Find the first bbox intersection under the XY view point.
static struct bv_scene_obj *
closest_obj_bbox(struct bu_ptbl *sset, struct bview *v)
{
    fastf_t vx = -FLT_MAX;
    fastf_t vy = -FLT_MAX;
    struct bv_scene_obj *s_closest = NULL;
    double dist = DBL_MAX;
    bv_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
    point_t vpnt, mpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
    point_t rmin, rmax;
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, v->radius);
    VADD2(mpnt, mpnt, dir);
    VUNITIZE(dir);
    bg_ray_invdir(&dir, dir);
    for (size_t i = 0; i < BU_PTBL_LEN(sset); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sset, i);
	if (bg_isect_aabb_ray(rmin, rmax, mpnt, dir, s->bmin, s->bmax)){
	    double ndist = DIST_PNT_PNT(rmin, v->gv_vc_backout);
	    if (ndist < dist) {
		dist = ndist;
		s_closest = s;
	    }
	}
    }

    return s_closest;
}

QMouseEvent *
QgSelectFilter::view_sync(QEvent *e)
{
    if (!v)
	return NULL;

    // If we don't have one of the relevant mouse operations, there's nothing to do
    QMouseEvent *m_e = NULL;
    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease || e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove)
	m_e = (QMouseEvent *)e;
    if (!m_e)
	return NULL;

    // We're going to need the mouse position
    int e_x, e_y;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    e_x = m_e->x();
    e_y = m_e->y();
#else
    e_x = m_e->position().x();
    e_y = m_e->position().y();
#endif

    // Update relevant bview variables
    v->gv_prevMouseX = v->gv_mouse_x;
    v->gv_prevMouseY = v->gv_mouse_y;
    v->gv_mouse_x = e_x;
    v->gv_mouse_y = e_y;

    // If we have modifiers, we're most likely doing shift grips
    if (m_e->modifiers() != Qt::NoModifier)
	return NULL;

    return m_e;
}


bool
QgSelectPntFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;


    // Eat everything except the mouse release
    if (e->type() != QEvent::MouseButtonRelease)
	return true;

    // Left mouse button only
    if (m_e->button() != Qt::LeftButton)
	return true;

    // If we don't have a view there's nothing we can do...
    if (!v)
	return true;

    // Do the actual selection, using a one pixel sized box projected into the
    // scene.  This is faster than the raytrace-based test in some situations,
    // but trades off that speed by only producing an approximate answer based
    // on bounding boxes.
    int scnt = bg_view_objs_select(&selected_set, v, v->gv_mouse_x, v->gv_mouse_y);

    // If the caller wants everything, or we got less than 2 objs, we're done
    if (scnt < 2 || !first_only)
	return true;

    // If we want only the closest object (or more precisely, in this mode, the
    // object with the closest bounding box) there's more work to do.
    struct bv_scene_obj *s_closest = closest_obj_bbox(&selected_set, v);
    bu_ptbl_reset(&selected_set);
    bu_ptbl_ins(&selected_set, (long *)s_closest);

    return true;
}

bool
QgSelectBoxFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    if (!v)
	return false;

    // Eat double clicks
    if (e->type() == QEvent::MouseButtonDblClick)
	return true;

    // Left mouse button and move events are the ones of interest
    if (m_e->button() != Qt::LeftButton && e->type() != QEvent::MouseMove)
	return true;

    if (e->type() == QEvent::MouseButtonPress) {
	px = v->gv_mouse_x;
	py = v->gv_mouse_y;
	struct bv_interactive_rect_state *grsp = &v->gv_s->gv_rect;
	grsp->line_width = 1;
	grsp->dim[0] = 0;
	grsp->dim[1] = 0;
	grsp->x = px;
	grsp->y = v->gv_height - py;
	grsp->pos[0] = grsp->x;
	grsp->pos[1] = grsp->y;
	grsp->cdim[0] = v->gv_width;
	grsp->cdim[1] = v->gv_height;
	grsp->aspect = (fastf_t)v->gv_s->gv_rect.cdim[X] / v->gv_s->gv_rect.cdim[Y];
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    if (e->type() == QEvent::MouseMove) {
	struct bv_interactive_rect_state *grsp = &v->gv_s->gv_rect;
	grsp->draw = 1;
	grsp->dim[0] = v->gv_mouse_x - px;
	grsp->dim[1] = (v->gv_height - v->gv_mouse_y) - v->gv_s->gv_rect.pos[1];
	grsp->x = (grsp->pos[X] / (fastf_t)grsp->cdim[X] - 0.5) * 2.0;
	grsp->y = ((0.5 - (grsp->cdim[Y] - grsp->pos[Y]) / (fastf_t)grsp->cdim[Y]) / grsp->aspect * 2.0);
	grsp->width = grsp->dim[X] * 2.0 / (fastf_t)grsp->cdim[X];
	grsp->height = grsp->dim[Y] * 2.0 / (fastf_t)grsp->cdim[X];
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
	// Mouse release - time to use the rectangle to assemble the selected set
	int ipx = (int)px;
	int ipy = (int)py;
	bg_view_objs_rect_select(&selected_set, v, ipx, ipy, v->gv_mouse_x, v->gv_mouse_y);

#if 0
	// If we want only the closest object (or more precisely, in this mode,
	// the object with the closest bounding box) there's more work to do.
	// TODO - this is the wrong test for the selection rectangle - should
	// be the distance between an aabb and a view plane.  Looks like we
	// need to add that one to libbg... there's DIST_PNT_PLANE and
	// MAT4X3VEC(view_pl, v->gv_view2model, dir) as starting points...
	struct bv_scene_obj *s_closest = closest_obj_bbox(&selected_set, v);
	bu_ptbl_reset(&selected_set);
	bu_ptbl_ins(&selected_set, (long *)s_closest);
#endif

	// reset rectangle
	struct bv_interactive_rect_state *grsp = &v->gv_s->gv_rect;
	grsp->draw = 0;
	grsp->line_width = 0;
	grsp->pos[0] = 0;
	grsp->pos[1] = 0;
	grsp->dim[0] = 0;
	grsp->dim[1] = 0;
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    // Shouldn't get here
    return false;
}


struct select_rec_state {
    std::unordered_set<std::string> active;
    int rec_all;
    double cdist;
    std::string closest;
};

static int
_obj_record(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct select_rec_state *rc = (struct select_rec_state *)ap->a_uptr;
    for (struct partition *pp = p_hp->pt_forw; pp != p_hp; pp = pp->pt_forw) {
	if (rc->rec_all) {
	    rc->active.insert(std::string(pp->pt_regionp->reg_name));
	} else {
	    struct hit *hitp = pp->pt_inhit;
	    if (hitp->hit_dist < rc->cdist) {
		rc->closest = std::string(pp->pt_regionp->reg_name);
		rc->cdist = hitp->hit_dist;
	    }
	}
    }
    bu_log("hit\n");
    return 1;
}

static int
_ovlp_record(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *UNUSED(ihp))
{
    struct select_rec_state *rc = (struct select_rec_state *)ap->a_uptr;
    if (rc->rec_all) {
	rc->active.insert(std::string(reg1->reg_name));
	rc->active.insert(std::string(reg2->reg_name));
    } else {
	rc->closest = std::string(reg1->reg_name);
	rc->cdist = pp->pt_inhit->hit_dist;
    }
    bu_log("ovlp\n");
    return 1;
}

bool
QgSelectRayFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    // If we're raytracing, the view itself isn't enough - we have
    // to have the dbip as well.
    if (!v || !dbip)
	return false;

    // Eat everything except the mouse release
    if (e->type() != QEvent::MouseButtonRelease)
	return true;

    // Left mouse button only
    if (m_e->button() != Qt::LeftButton)
	return true;

    // Pre-filter what we're going to be shooting using the bounding box tests.
    // If we have no intersections, there's no point in doing the raytrace.
    int scnt = bg_view_objs_select(&selected_set, v, v->gv_mouse_x, v->gv_mouse_y);
    if (!scnt)
	return true;

    // librt intersection test.
    struct application *ap;
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_onehit = 0;
    ap->a_hit = _obj_record;
    ap->a_miss = NULL;
    ap->a_overlap = _ovlp_record;
    ap->a_logoverlap = NULL;

    struct rt_i *rtip = rt_new_rti(dbip);
    struct resource *resp = NULL;
    BU_GET(resp, struct resource);
    rt_init_resource(resp, 0, rtip);
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    const char **objs = (const char **)bu_calloc(BU_PTBL_LEN(&selected_set) + 1, sizeof(char *), "objs");
    for (size_t i = 0; i < BU_PTBL_LEN(&selected_set); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&selected_set, i);
	objs[i] = bu_vls_cstr(&s->s_name);
    }
    if (rt_gettrees_and_attrs(rtip, NULL, scnt, objs, 1)) {
	bu_free(objs, "objs");
	rt_free_rti(rtip);
	BU_PUT(resp, struct resource);
	BU_PUT(ap, struct appliation);
	return false;
    }
    size_t ncpus = bu_avail_cpus();
    rt_prep_parallel(rtip, (int)ncpus);
    fastf_t vx = -FLT_MAX;
    fastf_t vy = -FLT_MAX;
    bv_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
    point_t vpnt, mpnt;
    VSET(vpnt, vx, vy, 0);
    MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
    vect_t dir;
    VMOVEN(dir, v->gv_rotation + 8, 3);
    VUNITIZE(dir);
    VSCALE(dir, dir, v->radius);
    VADD2(ap->a_ray.r_pt, mpnt, dir);
    VUNITIZE(dir);
    VSCALE(ap->a_ray.r_dir, dir, -1);

    struct select_rec_state rc;

    // Decide what we record in the hit function based on whether we want the
    // closest or all hits.
    if (!first_only) {
	rc.rec_all = 1;
    } else {
	rc.rec_all = 0;
	rc.cdist = INFINITY;
    }
    ap->a_uptr = (void *)&rc;

    (void)rt_shootray(ap);
    bu_free(objs, "objs");
    rt_free_rti(rtip);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct appliation);

    // We only have reg_names from the raytrace - translate into scene objects.
    bu_ptbl_reset(&selected_set);
    struct bu_vls dpath = BU_VLS_INIT_ZERO;
    if (first_only) {
	bu_vls_sprintf(&dpath, "%s",  rc.closest.c_str());
	if (bu_vls_cstr(&dpath)[0] == '/')
	    bu_vls_nibble(&dpath, 1);
	struct bv_scene_obj *so = bv_find_obj(v, bu_vls_cstr(&dpath));
	if (so)
	    bu_ptbl_ins(&selected_set, (long *)so);
    } else {
	std::unordered_set<std::string>::iterator a_it;
	for (a_it = rc.active.begin(); a_it != rc.active.end(); a_it++) {
	    bu_vls_sprintf(&dpath, "%s",  a_it->c_str());
	    if (bu_vls_cstr(&dpath)[0] == '/')
		bu_vls_nibble(&dpath, 1);
	    struct bv_scene_obj *so = bv_find_obj(v, bu_vls_cstr(&dpath));
	    if (so)
		bu_ptbl_ins(&selected_set, (long *)so);
	}
    }
    bu_vls_free(&dpath);

    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
