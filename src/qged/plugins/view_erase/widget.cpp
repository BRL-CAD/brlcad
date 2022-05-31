/*                 V I E W _ W I D G E T . C P P
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
/** @file view_widget.cpp
 *
 */

#include "common.h"
#include <QMouseEvent>
#include <QVBoxLayout>
#include "../../app.h"

#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bg/plane.h"
#include "bg/lod.h"

#include "./widget.h"

CADViewEraser::CADViewEraser(QWidget *)
{
    QVBoxLayout *wl = new QVBoxLayout;
    wl->setAlignment(Qt::AlignTop);

    use_ray_test_ckbx = new QCheckBox("Use Raytracing");
    erase_all_ckbx = new QCheckBox("Erase all under pointer");
    wl->addWidget(use_ray_test_ckbx);
    wl->addWidget(erase_all_ckbx);

    this->setLayout(wl);
}

CADViewEraser::~CADViewEraser()
{
}

bool
CADViewEraser::eventFilter(QObject *, QEvent *e)
{
    QgSelectionProxyModel *mdl = ((CADApp *)qApp)->mdl;
    if (!mdl)
	return false;

    QgModel *m = (QgModel *)mdl->sourceModel();
    struct ged *gedp = m->gedp;
    if (!gedp || !gedp->ged_gvp)
	return false;
    struct bview *v = gedp->ged_gvp;

    if (e->type() == QEvent::MouseButtonRelease) {

	QMouseEvent *m_e = (QMouseEvent *)e;

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

	if (erase_all_ckbx->isChecked() && !use_ray_test_ckbx->isChecked()) {
	    // We're clearing everything - construct the erase command and
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

	if (erase_all_ckbx->isChecked() && use_ray_test_ckbx->isChecked()) {
	    // We're clearing everything based on ray intersections - shoot the
	    // ray, find the new set, construct the erase command and run it.
	}


	if (!erase_all_ckbx->isChecked() && !use_ray_test_ckbx->isChecked()) {
	    // Only removing one object - need to find the first bbox
	    // intersection, then run the erase command.
	}

	if (!erase_all_ckbx->isChecked() && use_ray_test_ckbx->isChecked()) {
	    // Only removing one object - need to shoot the ray, find the first
	    // intersection, then run the erase command.
	}


	return true; }

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
