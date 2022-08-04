/*                     W I D G E T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file widget.cpp
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include "../../../app.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/aabb_ray.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "./widget.h"

CADViewSelecter::CADViewSelecter(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    use_ray_test_ckbx = new QCheckBox("Use Raytracing");
    select_all_ckbx = new QCheckBox("Erase all under pointer");
    wl->addWidget(use_ray_test_ckbx);
    wl->addWidget(select_all_ckbx);

    this->setLayout(wl);
}

CADViewSelecter::~CADViewSelecter()
{
}

struct rec_state {
    std::unordered_set<std::string> active;
    int rec_all;
    double cdist;
    std::string closest;
};

static int
_obj_record(struct application *ap, struct partition *p_hp, struct seg *UNUSED(segs))
{
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
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
    struct rec_state *rc = (struct rec_state *)ap->a_uptr;
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
CADViewSelecter::eventFilter(QObject *, QEvent *e)
{

    QgModel *m = ((CADApp *)qApp)->mdl;
    if (!m)
	return false;
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
	return true;
    }

    // If certain kinds of mouse events take place, we know we are manipulating the
    // view to achieve something other than erasure.  Flag accordingly, so we don't
    // fire off the select event at the end of whatever we're doing instead.
    if (e->type() == QEvent::MouseMove) {
	enabled = false;
	return false;
    }

    if (e->type() == QEvent::MouseButtonRelease) {

	QMouseEvent *m_e = (QMouseEvent *)e;


	if (m_e->button() == Qt::RightButton) {
	    return true;
	}


	// If we were doing something else and the mouse release signals we're
	// done, re-enable the select behavior
	if (!enabled) {
	    enabled = true;
	    return false;
	}

	// If any other keys are down, we're not doing an select
	if (m_e->modifiers() != Qt::NoModifier)
	    return false;

	fastf_t vx, vy;
#ifdef USE_QT6
	vx = m_e->position().x();
	vy = m_e->position().y();
#else
	vx = m_e->x();
	vy = m_e->y();
#endif
	int x = (int)vx;
	int y = (int)vy;

	struct bv_scene_obj **sset = NULL;
	int scnt = bg_view_objs_select(&sset, v, x, y);

	// If we didn't hit anything, we have a no-op
	if (!scnt)
	    return false;

	if (select_all_ckbx->isChecked() && !use_ray_test_ckbx->isChecked()) {
	    // We're clearing everything - construct the select command and
	    // run it.
	    const char **av = (const char **)bu_calloc(scnt+2, sizeof(char *), "av");
	    av[0] = "erase";
	    for (int i = 0; i < scnt; i++) {
		struct bv_scene_obj *s = sset[i];
		av[i+1] = bu_vls_cstr(&s->s_name);
		bu_log("%s\n", av[i+1]);
	    }
	    ged_exec(gedp, scnt+1, av);
	    bu_free(av, "av");
	    bu_free(sset, "sset");
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}


	if (!select_all_ckbx->isChecked() && !use_ray_test_ckbx->isChecked()) {
	    // Only removing one object, not using all-up librt raytracing -
	    // need to find the first bbox intersection, then run the select
	    // command.
	    struct bv_scene_obj *s_closest = NULL;
	    double dist = DBL_MAX;
	    bv_screen_to_view(v, &vx, &vy, x, y);
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
	    for (int i = 0; i < scnt; i++) {
		struct bv_scene_obj *s = sset[i];
		if (bg_isect_aabb_ray(rmin, rmax, mpnt, dir, s->bmin, s->bmax)){
		    double ndist = DIST_PNT_PNT(rmin, v->gv_vc_backout);
		    if (ndist < dist) {
			dist = ndist;
			s_closest = s;
		    }
		}
	    }
	    if (s_closest) {
		const char **av = (const char **)bu_calloc(3, sizeof(char *), "av");
		av[0] = "erase";
		av[1] = bu_vls_cstr(&s_closest->s_name);
		ged_exec(gedp, 2, av);
		bu_free(av, "av");
		bu_free(sset, "sset");
		emit view_updated(&gedp->ged_gvp);
		return true;
	    } else {
		// no-op
		bu_free(sset, "sset");
		return false;
	    }
	}

	if (use_ray_test_ckbx->isChecked()) {
	    // librt intersection test.
	    struct application *ap;
	    BU_GET(ap, struct application);
	    RT_APPLICATION_INIT(ap);
	    ap->a_onehit = 0;
	    ap->a_hit = _obj_record;
	    ap->a_miss = NULL;
	    ap->a_overlap = _ovlp_record;
	    ap->a_logoverlap = NULL;

	    struct rt_i *rtip = rt_new_rti(gedp->dbip);
	    struct resource *resp = NULL;
	    BU_GET(resp, struct resource);
	    rt_init_resource(resp, 0, rtip);
	    ap->a_resource = resp;
	    ap->a_rt_i = rtip;
	    const char **objs = (const char **)bu_calloc(scnt + 1, sizeof(char *), "objs");
	    for (int i = 0; i < scnt; i++) {
		struct bv_scene_obj *s = sset[i];
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
	    bv_screen_to_view(v, &vx, &vy, x, y);
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

	    struct rec_state rc;

	    // Since most of the work of the raytracing approach is the same,
	    // we just change what we record in the hit function based on
	    // the checkbox settings
	    if (select_all_ckbx->isChecked()) {
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

	    if (select_all_ckbx->isChecked()) {
		if (rc.active.size()) {
		    std::unordered_set<std::string>::iterator a_it;
		    const char **av = (const char **)bu_calloc(rc.active.size()+2, sizeof(char *), "av");
		    av[0] = bu_strdup("erase");
		    int i = 0;
		    for (a_it = rc.active.begin(); a_it != rc.active.end(); a_it++) {
			av[i+1] = bu_strdup(a_it->c_str());
			i++;
		    }
		    ged_exec(gedp, rc.active.size()+1, av);
		    bu_argv_free(rc.active.size()+1, (char **)av);
		} else {
		    bu_free(sset, "sset");
		    return false;
		}
	    } else {
		if (rc.cdist < INFINITY) {
		    const char **av = (const char **)bu_calloc(3, sizeof(char *), "av");
		    av[0] = "erase";
		    av[1] = bu_strdup(rc.closest.c_str());
		    ged_exec(gedp, 2, av);
		    bu_free((void *)av[1], "ncpy");
		} else {
		    bu_free(sset, "sset");
		    return false;

		}
	    }

	    bu_free(sset, "sset");
	    emit view_updated(&gedp->ged_gvp);
	    return true;
	}

	// If we somehow didn't process by this point, no-op
	return false;
    }

     return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
