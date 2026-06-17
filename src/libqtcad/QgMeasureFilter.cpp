/*                 Q G M E A S U R E F I L T E R . C P P
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
/** @file QgMeasureFilter.cpp
 *
 * Measurement tool for Qt views.
 *
 */

#include "common.h"

extern "C" {
#include "bu/malloc.h"
#include "bsg.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "raytrace.h"
}

#include <string>
#include <vector>
#include "qtcad/QgMeasureFilter.h"
#include "qtcad/QgSignalFlags.h"

static void
qg_measure_feature_update(bsg_feature_ref feature, const point_t p1, const point_t p2, const point_t p3, int point_count)
{
	if (bsg_feature_ref_is_null(feature) || point_count < 1)
		return;
	point_t pts[3];
	int cmds[3] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_DRAW};
	VMOVE(pts[0], p1);
	if (point_count > 1)
		VMOVE(pts[1], p2);
	if (point_count > 2)
		VMOVE(pts[2], p3);
	bsg_feature_points_replace(feature, BSG_FEATURE_MEASUREMENT, (const point_t *)pts, cmds, (size_t)point_count);
}

double
QgMeasureFilter::length1()
{
	if (mr12.mr_valid)
		return mr12.mr_distance;
	return DIST_PNT_PNT(p1, p2);
}

double
QgMeasureFilter::length2()
{
	if (mode < 3)
		return 0.0;

	if (mr23.mr_valid)
		return mr23.mr_distance;
	return DIST_PNT_PNT(p2, p3);
}

double
QgMeasureFilter::angle(bool radians)
{
	if (mode < 3)
		return 0.0;

	vect_t v1, v2;
	VSUB2(v1, p1, p2);
	VSUB2(v2, p3, p2);
	VUNITIZE(v1);
	VUNITIZE(v2);
	double a = acos(VDOT(v1, v2));
	if (radians)
		return a*180/M_PI;
	return a;
}

void
QgMeasureFilter::update_color(struct bu_color *c)
{
	if (bsg_feature_ref_is_null(feature) || !c)
		return;
	unsigned char rgb[3] = {0, 0, 0};
	bu_color_to_rgb_chars(c, rgb);
	bsg_feature_set_color(feature, rgb[0], rgb[1], rgb[2]);
}

bool
QgMeasureFilter::eventFilter(QObject *, QEvent *e)
{
	QMouseEvent *m_e = view_sync(e);
	if (!m_e)
		return false;

	struct bsg_view *v = view();
	if (!v)
		return false;

	if (e->type() == QEvent::MouseButtonPress) {
		if (m_e->button() == Qt::RightButton) {
			if (!bsg_feature_ref_is_null(feature))
				bsg_feature_remove(v, oname.c_str());
			feature = BSG_FEATURE_REF_NULL_INIT;
			mode = 0;
			VSETALL(p1, 0.0);
			VSETALL(p2, 0.0);
			VSETALL(p3, 0.0);
			mr12 = {0.0, 0.0, 0.0, 0};
			mr23 = {0.0, 0.0, 0.0, 0};
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}
		if (mode == 4) {
			if (!bsg_feature_ref_is_null(feature))
				bsg_feature_remove(v, oname.c_str());
			feature = BSG_FEATURE_REF_NULL_INIT;
			mode = 0;
			mr12 = {0.0, 0.0, 0.0, 0};
			mr23 = {0.0, 0.0, 0.0, 0};
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}
		if (!mode) {
			if (!get_point())
				return true;

			VSETALL(p1, 0.0);
			VSETALL(p2, 0.0);
			VSETALL(p3, 0.0);
			mr12 = {0.0, 0.0, 0.0, 0};
			mr23 = {0.0, 0.0, 0.0, 0};

			if (!bsg_feature_ref_is_null(feature))
				bsg_feature_remove(v, oname.c_str());
			feature = bsg_feature_create_lines(v, oname.c_str(), 0);
			bsg_feature_set_family(feature, BSG_FEATURE_MEASUREMENT);
			if (!bsg_feature_ref_is_null(feature)) {
				bsg_feature_overlay_register_owner(feature, this,
					BSG_OVERLAY_ROLE_SCREEN,
					BSG_OVERLAY_CLASS_MEASURE,
					BSG_OVERLAY_LC_PER_TOOL,
					BSG_OVERLAY_ORDER_POST_TRANSPARENT,
					NULL,
					0);
			}

			mode = 1;
			VMOVE(p1, mpnt);
			VMOVE(p2, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 1);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}
		if (mode == 1) {
			if (!get_point())
				return true;

			VMOVE(p2, mpnt);
			return true;
		}
		if (mode == 2) {
			if (!get_point())
				return true;
			mode = 3;
			VMOVE(p3, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 3);
			emit view_updated(QG_VIEW_REFRESH);
		}
		return true;
	}

	if (e->type() == QEvent::MouseMove) {
		if (!mode)
			return false;
		if (mode == 1) {
			if (!get_point())
				return true;

			VMOVE(p2, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 2);
			emit view_updated(QG_VIEW_REFRESH);
		}
		if (mode == 3) {
			if (!get_point())
				return true;

			VMOVE(p3, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 3);
			emit view_updated(QG_VIEW_REFRESH);
		}
		return true;
	}

	if (e->type() == QEvent::MouseButtonRelease) {
		if (m_e->button() == Qt::RightButton) {
			mode = 0;
			if (!bsg_feature_ref_is_null(feature)) {
				bsg_overlay_clear_owned(v, this);
				emit view_updated(QG_VIEW_REFRESH);
			}
			feature = BSG_FEATURE_REF_NULL_INIT;
			return true;
		}
		if (!mode)
			return false;
		if (mode == 1 && DIST_PNT_PNT(p1, p2) < SMALL_FASTF) {
			return true;
		}
		if (mode == 1) {
			if (!get_point())
				return true;

			if (length_only) {
				// Angle measurement disabled, starting over
				mode = 0;
				emit view_updated(QG_VIEW_REFRESH);
				return true;
			}

			mode = 2;
			VMOVE(p2, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 2);
			/* Record p1→p2 measure via typed API for D3 consumers. */
			bsg_measure_candidates(v, p1, p2, &mr12);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}
		if (mode == 3) {
			if (!get_point())
				return true;
			mode = 4;
			VMOVE(p3, mpnt);
			qg_measure_feature_update(feature, p1, p2, p3, 3);
			/* Record p2→p3 measure via typed API for D3 consumers. */
			bsg_measure_candidates(v, p2, p3, &mr23);
			emit view_updated(QG_VIEW_REFRESH);
			return true;
		}

		return true;
	}

	// Shouldn't get here
	return false;
}

bool
QMeasure2DFilter::get_point()
{
	struct bsg_view *v = view();
	fastf_t vx, vy;
	bsg_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
	point_t vpnt;
	VSET(vpnt, vx, vy, 0);
	MAT4X3PNT(mpnt, v->gv_view2model, vpnt);
	return true;
}

bool
QMeasure2DFilter::eventFilter(QObject *o, QEvent *e)
{
	return QgMeasureFilter::eventFilter(o, e);
}

QMeasure3DFilter::QMeasure3DFilter()
{
}

QMeasure3DFilter::~QMeasure3DFilter()
{
}

struct measure_rec_state {
	double cdist;
	point_t pt;
};

static int
_cpnt_hit(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
	struct measure_rec_state *rc = (struct measure_rec_state *)ap->a_uptr;
	for (struct partition *pp = p_hp->pt_forw; pp != p_hp; pp = pp->pt_forw) {
		struct hit *hitp = pp->pt_inhit;
		if (hitp->hit_dist < rc->cdist) {
			rc->cdist = hitp->hit_dist;
			VJOIN1(rc->pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
		}
	}
	return 1;
}

static int
_cpnt_ovlp(struct application *ap, struct partition *pp, struct region *UNUSED(reg1), struct region *UNUSED(reg2), struct partition *UNUSED(ihp))
{
	struct measure_rec_state *rc = (struct measure_rec_state *)ap->a_uptr;
	struct hit *hitp = pp->pt_inhit;
	rc->cdist = hitp->hit_dist;
	VJOIN1(rc->pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	return 1;
}

bool
QMeasure3DFilter::get_point()
{
	if (!dbip)
		return false;

	struct bsg_view *v = view();
	fastf_t vx, vy;
	bsg_screen_to_view(v, &vx, &vy, v->gv_mouse_x, v->gv_mouse_y);
	point_t vpnt;
	VSET(vpnt, vx, vy, 0);
	MAT4X3PNT(mpnt, v->gv_view2model, vpnt);

	// With this filter we want a 3D point based on scene geometry (hard case)
	// - need to interrogate the scene with the raytracer.
	//
	// Rather than prepping the whole of what is drawn, we will instead prep
	// only those objects whose bounding boxes are currently under the mouse.
	// Under most circumstances that should substantially cut down the
	// interrogation time for large models.
	struct bsg_pick_result *candidates = bsg_pick_point(v, v->gv_mouse_x, v->gv_mouse_y, 0);
	struct bsg_interaction_result *interactions = bsg_interaction_from_pick_result(candidates);
	std::vector<std::string> candidate_names;
	if (interactions) {
		candidate_names.reserve(bsg_interaction_result_count(interactions));
		for (size_t i = 0; i < bsg_interaction_result_count(interactions); i++) {
			const struct bsg_interaction_record *rec = bsg_interaction_result_get(interactions, i);
			const char *path = bsg_interaction_record_path(rec);
			if (!path || !path[0])
				continue;
			candidate_names.emplace_back(path);
		}
	}
	int scnt = (int)candidate_names.size();

	// If we didn't see anything, we have a no-op
	if (!scnt) {
		prev_cnt = scnt;
		if (interactions)
			bsg_interaction_result_free(interactions);
		if (candidates)
			bsg_pick_result_free(candidates);
		return false;
	}

	bool need_prep = (!ap || !rtip) ? true : false;
	if (need_prep || prev_cnt != scnt || scnt != (int)scene_obj_paths.size()) {
		// Something changed - need to reset the raytrace data
		scene_obj_paths = candidate_names;
		need_prep = true;
	}
	if (!need_prep) {
		// We may be able to reuse the existing prep - make sure the scene obj
		// path sets match.  The above check should ensure that the lengths
		// match - if they don't, we already know we need to re-prep.
		for (size_t i = 0; i < scene_obj_paths.size(); i++) {
			if (candidate_names[i] != scene_obj_paths[i]) {
				need_prep = true;
				break;
			}
		}
	}
	if (need_prep)
		scene_obj_paths = candidate_names;

	prev_cnt = scnt;

	if (need_prep) {
		if (!ap) {
			BU_GET(ap, struct application);
			RT_APPLICATION_INIT(ap);
			ap->a_onehit = 1;
			ap->a_hit = _cpnt_hit;
			ap->a_miss = nullptr;
			ap->a_overlap = _cpnt_ovlp;
			ap->a_logoverlap = nullptr;
		}
		if (rtip) {
			rt_i_destroy(rtip);
			rtip = nullptr;
		}
		rtip = rt_i_create(dbip);
		struct resource *resp = nullptr;
		BU_GET(resp, struct resource);
		rt_init_resource(resp, 0, rtip);
		ap->a_resource = resp;
		ap->a_rt_i = rtip;

		const char **objs = (const char **)bu_calloc(scene_obj_paths.size() + 1, sizeof(char *), "objs");
		for (size_t i = 0; i < scene_obj_paths.size(); i++) {
			objs[i] = scene_obj_paths[i].c_str();
		}
		if (rt_gettrees_and_attrs(rtip, nullptr, scnt, objs, 1)) {
			bu_free(objs, "objs");
			rt_i_destroy(rtip);
			rtip = nullptr;
			BU_PUT(resp, struct resource);
			if (interactions)
				bsg_interaction_result_free(interactions);
			if (candidates)
				bsg_pick_result_free(candidates);
			return false;
		}
		size_t ncpus = bu_avail_cpus();
		rt_prep_parallel(rtip, (int)ncpus);
		bu_free(objs, "objs");
	}

	if (interactions)
		bsg_interaction_result_free(interactions);
	if (candidates)
		bsg_pick_result_free(candidates);

	// Set up data container for result
	struct measure_rec_state rc;
	rc.cdist = INFINITY;
	ap->a_uptr = (void *)&rc;

	// Set up the ray itself
	vect_t dir;
	VMOVEN(dir, v->gv_rotation + 8, 3);
	VUNITIZE(dir);
	VSCALE(dir, dir, v->radius);
	VADD2(ap->a_ray.r_pt, mpnt, dir);
	VUNITIZE(dir);
	VSCALE(ap->a_ray.r_dir, dir, -1);

	(void)rt_shootray(ap);

	if (rc.cdist < INFINITY) {
		VMOVE(mpnt, rc.pt);
		return true;
	}

	return false;
}

bool
QMeasure3DFilter::eventFilter(QObject *o, QEvent *e)
{
	return QgMeasureFilter::eventFilter(o, e);
}




// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
