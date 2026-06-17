/*                 Q G S E L E C T F I L T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
#include "bsg.h"
#include "raytrace.h"
}

#include <string>
#include <unordered_map>
#include <unordered_set>
#include "qtcad/QgSelectFilter.h"
#include "qtcad/QgSignalFlags.h"

static struct bsg_pick_record *
_qg_pick_record_create(struct bsg_view *v, int sx, int sy,
	const char *source_path, fastf_t hit_dist = -1.0)
{
    if (!source_path || !source_path[0])
	return nullptr;

    struct bsg_pick_record *pr;
    BU_GET(pr, struct bsg_pick_record);
    bu_vls_init(&pr->pr_source_path);
    bu_vls_init(&pr->pr_instance_path);
    pr->pr_scene = BSG_SCENE_REF_NULL_INIT;
    pr->pr_feature = BSG_FEATURE_REF_NULL_INIT;
    pr->pr_valid = 1;
    pr->pr_view = v;
    pr->pr_screen_x = sx;
    pr->pr_screen_y = sy;
    pr->pr_primitive_id = -1;
    pr->pr_subelement_id = -1;
    pr->pr_hit_dist = hit_dist;
    bu_vls_sprintf(&pr->pr_source_path, "%s", source_path);
    bu_vls_sprintf(&pr->pr_instance_path, "%s", bu_vls_cstr(&pr->pr_source_path));
    if (bu_vls_strlen(&pr->pr_source_path) == 0)
	pr->pr_valid = 0;
    return pr;
}

static struct bsg_pick_result *
_qg_pick_result_filter_first(const struct bsg_pick_result *src)
{
    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res || !src || !bsg_pick_result_count(src))
	return res;

    struct bsg_pick_record *src_pr = bsg_pick_result_get(src, 0);
    struct bsg_pick_record *pr = _qg_pick_record_create(src_pr->pr_view,
	src_pr->pr_screen_x, src_pr->pr_screen_y, bu_vls_cstr(&src_pr->pr_source_path),
	src_pr->pr_hit_dist);
    if (pr) {
	pr->pr_feature = src_pr->pr_feature;
	pr->pr_valid = src_pr->pr_valid;
	bu_ptbl_ins(&res->pr_records, (long *)pr);
    }
    return res;
}

static const char *
_qg_pick_record_target(const struct bsg_pick_record *pr)
{
    if (!pr)
	return NULL;

    const char *spath = bu_vls_cstr(&pr->pr_source_path);
    if (spath && spath[0])
	return spath;

    const char *ipath = bu_vls_cstr(&pr->pr_instance_path);
    return (ipath && ipath[0]) ? ipath : NULL;
}

static std::string
_qg_normalize_path(const char *path)
{
    if (!path)
	return std::string();
    return (path[0] == '/') ? std::string(path + 1) : std::string(path);
}

QgSelectFilter::~QgSelectFilter()
{
    clear_selected_result();
}

void
QgSelectFilter::clear_selected_result()
{
    if (selected_result) {
	bsg_pick_result_free(selected_result);
	selected_result = nullptr;
    }
    if (selected_interactions) {
	bsg_interaction_result_free(selected_interactions);
	selected_interactions = nullptr;
    }
}

void
QgSelectFilter::set_selected_result(struct bsg_view *v, struct bsg_pick_result *res)
{
    clear_selected_result();
    selected_result = res;
    if (selected_result)
	selected_interactions = bsg_interaction_from_pick_result(selected_result);

    struct bsg_selection *selection = bsg_view_selection(v);
    if (selection) {
	if (selected_interactions) {
	    bsg_interaction_selection_apply(selection,
		    selected_interactions, BSG_INTERACTION_APPLY_SET);
	} else {
	    bsg_selection_clear(selection);
	}
    }
}

bool
QgSelectPntFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    struct bsg_view *v = view();

    if (e->type() != QEvent::MouseButtonRelease)
	return true;
    if (m_e->button() != Qt::LeftButton)
	return true;
    if (!v)
	return true;

    struct bsg_pick_result *res = first_only ?
	bsg_pick_nearest(v, v->gv_mouse_x, v->gv_mouse_y) :
	bsg_pick_point(v, v->gv_mouse_x, v->gv_mouse_y, 0);
    set_selected_result(v, res);

    return true;
}

bool
QgSelectBoxFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    struct bsg_view *v = view();
    if (!v)
	return false;

    if (e->type() == QEvent::MouseButtonDblClick)
	return true;
    if (m_e->button() != Qt::LeftButton && e->type() != QEvent::MouseMove)
	return true;

    if (e->type() == QEvent::MouseButtonPress) {
	px = v->gv_mouse_x;
	py = v->gv_mouse_y;
	struct bsg_interactive_rect_state rect;
	if (!bsg_view_interactive_rect_get(v, &rect))
	    return true;
	rect.line_width = 1;
	rect.dim[0] = 0;
	rect.dim[1] = 0;
	rect.x = px;
	rect.y = v->gv_height - py;
	rect.pos[0] = rect.x;
	rect.pos[1] = rect.y;
	rect.cdim[0] = v->gv_width;
	rect.cdim[1] = v->gv_height;
	rect.aspect = (fastf_t)rect.cdim[X] / rect.cdim[Y];
	bsg_view_interactive_rect_set(v, &rect);
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    if (e->type() == QEvent::MouseMove) {
	struct bsg_interactive_rect_state rect;
	if (!bsg_view_interactive_rect_get(v, &rect))
	    return true;
	rect.draw = 1;
	rect.dim[0] = v->gv_mouse_x - px;
	rect.dim[1] = (v->gv_height - v->gv_mouse_y) - rect.pos[1];
	rect.x = (rect.pos[X] / (fastf_t)rect.cdim[X] - 0.5) * 2.0;
	rect.y = ((0.5 - (rect.cdim[Y] - rect.pos[Y]) / (fastf_t)rect.cdim[Y]) / rect.aspect * 2.0);
	rect.width = rect.dim[X] * 2.0 / (fastf_t)rect.cdim[X];
	rect.height = rect.dim[Y] * 2.0 / (fastf_t)rect.cdim[X];
	bsg_view_interactive_rect_set(v, &rect);
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    if (e->type() == QEvent::MouseButtonRelease) {
	int ipx = (int)px;
	int ipy = (int)py;
	struct bsg_pick_result *res =
	    bsg_pick_rect(v, ipx, ipy, v->gv_mouse_x, v->gv_mouse_y);
	if (first_only && res && bsg_pick_result_count(res) > 1) {
	    struct bsg_pick_result *nearest = _qg_pick_result_filter_first(res);
	    bsg_pick_result_free(res);
	    res = nearest;
	}
	set_selected_result(v, res);

	struct bsg_interactive_rect_state rect;
	if (!bsg_view_interactive_rect_get(v, &rect))
	    return true;
	rect.draw = 0;
	rect.line_width = 0;
	rect.pos[0] = 0;
	rect.pos[1] = 0;
	rect.dim[0] = 0;
	rect.dim[1] = 0;
	bsg_view_interactive_rect_set(v, &rect);
	emit view_updated(QG_VIEW_DRAWN);
	return true;
    }

    return false;
}

struct select_rec_state {
    std::unordered_map<std::string, fastf_t> hits;
    int rec_all;
    double cdist;
    std::string closest;
};

static void
_select_record_hit(struct select_rec_state *rc, const char *name, fastf_t hit_dist)
{
    if (!rc || !name || !name[0])
	return;

    std::string key = _qg_normalize_path(name);
    if (key.empty())
	return;

    std::unordered_map<std::string, fastf_t>::iterator h_it = rc->hits.find(key);
    if (h_it == rc->hits.end() || hit_dist < h_it->second)
	rc->hits[key] = hit_dist;

    if (hit_dist < rc->cdist) {
	rc->closest = key;
	rc->cdist = hit_dist;
    }
}

static int
_obj_record(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct select_rec_state *rc = (struct select_rec_state *)ap->a_uptr;
    for (struct partition *pp = p_hp->pt_forw; pp != p_hp; pp = pp->pt_forw) {
	if (rc->rec_all) {
	    _select_record_hit(rc, pp->pt_regionp->reg_name, pp->pt_inhit->hit_dist);
	} else {
	    _select_record_hit(rc, pp->pt_regionp->reg_name, pp->pt_inhit->hit_dist);
	}
    }
    return 1;
}

static int
_ovlp_record(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *UNUSED(ihp))
{
    struct select_rec_state *rc = (struct select_rec_state *)ap->a_uptr;
    if (rc->rec_all) {
	_select_record_hit(rc, reg1->reg_name, pp->pt_inhit->hit_dist);
	_select_record_hit(rc, reg2->reg_name, pp->pt_inhit->hit_dist);
    } else {
	_select_record_hit(rc, reg1->reg_name, pp->pt_inhit->hit_dist);
    }
    return 1;
}

static struct bsg_pick_result *
_qg_pick_result_from_ray_hits(const struct bsg_pick_result *candidates,
			      const struct select_rec_state *rc,
			      int first_only)
{
    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res || !candidates || !rc)
	return res;

    std::unordered_set<std::string> seen_paths;
    for (size_t i = 0; i < bsg_pick_result_count(candidates); i++) {
	struct bsg_pick_record *src = bsg_pick_result_get(candidates, i);
	std::string key = _qg_normalize_path(_qg_pick_record_target(src));
	if (key.empty())
	    continue;

	if (first_only) {
	    if (key != rc->closest)
		continue;
	} else {
	    if (rc->hits.find(key) == rc->hits.end())
		continue;
	}

	if (!seen_paths.insert(key).second)
	    continue;

	fastf_t hit_dist = src->pr_hit_dist;
	std::unordered_map<std::string, fastf_t>::const_iterator h_it = rc->hits.find(key);
	if (h_it != rc->hits.end())
	    hit_dist = h_it->second;

	struct bsg_pick_record *pr = _qg_pick_record_create(src->pr_view,
		src->pr_screen_x, src->pr_screen_y, bu_vls_cstr(&src->pr_source_path),
		hit_dist);
	if (pr) {
	    pr->pr_feature = src->pr_feature;
	    pr->pr_valid = src->pr_valid;
	    bu_ptbl_ins(&res->pr_records, (long *)pr);
	}

	if (first_only)
	    break;
    }

    return res;
}

bool
QgSelectRayFilter::eventFilter(QObject *, QEvent *e)
{
    QMouseEvent *m_e = view_sync(e);
    if (!m_e)
	return false;

    struct bsg_view *v = view();
    if (!v || !dbip)
	return false;
    if (e->type() != QEvent::MouseButtonRelease)
	return true;
    if (m_e->button() != Qt::LeftButton)
	return true;

    struct bsg_pick_result *candidates =
	bsg_pick_point(v, v->gv_mouse_x, v->gv_mouse_y, 0);
    if (!candidates || !bsg_pick_result_count(candidates)) {
	set_selected_result(v, candidates);
	return true;
    }

    struct application *ap;
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_onehit = 0;
    ap->a_hit = _obj_record;
    ap->a_miss = nullptr;
    ap->a_overlap = _ovlp_record;
    ap->a_logoverlap = nullptr;

    struct rt_i *rtip = rt_i_create(dbip);
    struct resource *resp = nullptr;
    BU_GET(resp, struct resource);
    rt_init_resource(resp, 0, rtip);
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    const char **objs = (const char **)bu_calloc(bsg_pick_result_count(candidates) + 1, sizeof(char *), "objs");
    for (size_t i = 0; i < bsg_pick_result_count(candidates); i++) {
	struct bsg_pick_record *pr = bsg_pick_result_get(candidates, i);
	objs[i] = _qg_pick_record_target(pr);
    }
    if (rt_gettrees_and_attrs(rtip, nullptr, (int)bsg_pick_result_count(candidates), objs, 1)) {
	bu_free(objs, "objs");
	rt_i_destroy(rtip);
	BU_PUT(resp, struct resource);
	BU_PUT(ap, struct application);
	bsg_pick_result_free(candidates);
	return false;
    }
    size_t ncpus = bu_avail_cpus();
    rt_prep_parallel(rtip, (int)ncpus);
    fastf_t vx = -FLT_MAX;
    fastf_t vy = -FLT_MAX;
    bsg_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
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
    rc.cdist = INFINITY;
    if (!first_only) {
	rc.rec_all = 1;
    } else {
	rc.rec_all = 0;
    }
    ap->a_uptr = (void *)&rc;

    (void)rt_shootray(ap);
    bu_free(objs, "objs");
    rt_i_destroy(rtip);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct application);

    struct bsg_pick_result *res =
	_qg_pick_result_from_ray_hits(candidates, &rc, first_only);
    bsg_pick_result_free(candidates);
    set_selected_result(v, res);

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
