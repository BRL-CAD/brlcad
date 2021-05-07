/*                      D M G L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file dmgl.cpp
 *
 * Brief description
 *
 */

#include <QOpenGLWidget>
#include <QKeyEvent>

extern "C" {
#include "bu/env.h"
#include "bu/parallel.h"
#include "bu/ptbl.h"
#include "bview/util.h"
#include "dm.h"
#include "ged.h"
}
#include "dmgl.h"

dmGL::dmGL(QWidget *parent)
    : QOpenGLWidget(parent)
{
    BU_GET(v, struct bview);
    bview_init(v);

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

dmGL::~dmGL()
{
    dm_close(dmp);
    BU_PUT(v, struct bview);
}


void dmGL::paintGL()
{
    int w = width();
    int h = height();
    // Zero size == nothing to do
    if (!w || !h)
	return;

    if (!gedp) {
	return;
    }

    if (!m_init) {
	initializeOpenGLFunctions();
	m_init = true;
	if (!dmp) {
	    const char *acmd = "attach";
	    dmp = dm_open((void *)this, NULL, "qtgl", 1, &acmd);
	    if (!dmp)
		return;
	    if (gedp) {
		gedp->ged_dmp = (void *)dmp;
		bu_ptbl_ins(gedp->ged_all_dmp, (long int *)dmp);
		dm_set_vp(dmp, &gedp->ged_gvp->gv_scale);
	    }
	    dm_configure_win(dmp, 0);
	    dm_set_pathname(dmp, "QTDM");
	    dm_set_zbuffer(dmp, 1);

	    fastf_t windowbounds[6] = { -1, 1, -1, 1, (int)GED_MIN, (int)GED_MAX };
	    dm_set_win_bounds(dmp, windowbounds);

	    v->dmp = dmp;
	}
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    if (gedp) {
	struct db_i *dbip = gedp->ged_wdbp->dbip;
	matp_t mat = gedp->ged_gvp->gv_model2view;
	dm_loadmatrix(dmp, mat, 0);
	dm_draw_viewobjs(gedp->ged_wdbp, v, NULL, dbip->dbi_base2local, dbip->dbi_local2base);
	dm_draw_end(dmp);
    }
}

void dmGL::resizeGL(int, int)
{
    if (gedp && gedp->ged_dmp) {
	dm_configure_win((struct dm *)gedp->ged_dmp, 0);
	gedp->ged_gvp->gv_width = dm_get_width((struct dm *)gedp->ged_dmp);
	gedp->ged_gvp->gv_height = dm_get_height((struct dm *)gedp->ged_dmp);
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
    }
}

void dmGL::ged_run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    if (msg)
		bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0], ccmd, edist);
	    return;
	}
    } else {
	// TODO - need to add hashing to check the dm variables as well (i.e. if lighting
	// was turned on/off by the dm command...)
	prev_dhash = dm_hash(dmp);
	prev_vhash = bview_hash(v);
	prev_lhash = dl_name_hash(gedp);

	// Clear the edit flags ahead of the ged_exec call, so we can tell if
	// any geometry changed.
	struct directory *dp;
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		dp->edit_flag = 0;
	    }
	}

	ged_exec(gedp, argc, argv);
	if (msg)
	    bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

	/* Check if the ged_exec call changed either the display manager or
	 * the view settings - in either case we'll need to redraw */
	if (prev_dhash != dm_hash((struct dm *)gedp->ged_dmp)) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}
	if (prev_vhash != bview_hash(v)) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}

	// Checks the dp edit flags and does any necessary redrawing.  If
	// anything changed with the geometry, we also need to redraw
	if (ged_view_update(gedp) > 0) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}

	// If anybody set the flag, trigger an update
	if (dm_get_dirty((struct dm *)gedp->ged_dmp))
	    update();
    }
}


void dmGL::keyPressEvent(QKeyEvent *k) {

    if (!gedp || !gedp->ged_dmp) {
	QOpenGLWidget::keyPressEvent(k);
	return;
    }

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();
    v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;

    if (CADkeyPressEvent(v, x_prev, y_prev, k)) {
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	update();
    }

    QOpenGLWidget::keyPressEvent(k);
}

void dmGL::mousePressEvent(QMouseEvent *e) {

    if (!gedp || !gedp->ged_dmp) {
	QOpenGLWidget::mousePressEvent(e);
	return;
    }

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();
    v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;

    if (CADmousePressEvent(v, x_prev, y_prev, e)) {
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	update();
    }

    bu_log("X,Y: %d, %d\n", e->x(), e->y());

    QOpenGLWidget::mousePressEvent(e);
}

void dmGL::mouseMoveEvent(QMouseEvent *e)
{
    if (!gedp || !gedp->ged_dmp) {
	QOpenGLWidget::mouseMoveEvent(e);
	return;
    }

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();
    v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;

    if (CADmouseMoveEvent(v, x_prev, y_prev, e)) {
	 dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	 update();
    }

    // Current positions are the new previous positions
    x_prev = e->x();
    y_prev = e->y();

    QOpenGLWidget::mouseMoveEvent(e);
}

void dmGL::wheelEvent(QWheelEvent *e) {

    if (!gedp || !gedp->ged_dmp) {
	QOpenGLWidget::wheelEvent(e);
	return;
    }

    // Let bview know what the current view width and height are, in
    // case the dx/dy mouse translations need that information
    v->gv_width = width();
    v->gv_height = height();
    v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;

    if (CADwheelEvent(v, e)) {
	 dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	 update();
    }

    QOpenGLWidget::wheelEvent(e);
}

void dmGL::mouseReleaseEvent(QMouseEvent *e) {

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    QOpenGLWidget::mouseReleaseEvent(e);
}

void dmGL::save_image() {
    QImage image = this->grabFramebuffer();
    image.save("file.png");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

