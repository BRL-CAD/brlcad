/*                      D M S W R A S T . C P P
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
/** @file dmswrast.cpp
 *
 * Brief description
 *
 */

/* TODO - support gv_db_grps */

#define USE_MGL_NAMESPACE 1

#include <QKeyEvent>
#include <QGuiApplication> // for qGuiApp

extern "C" {
#include "bu/parallel.h"
#include "bview/util.h"
#include "dm.h"
#include "ged.h"
}
#include "dmswrast.h"

dmSW::dmSW(QWidget *parent)
    : QWidget(parent)
{
    BU_GET(v, struct bview);
    bview_init(v);

    // swrast will need to know the window size
    v->gv_width = width();
    v->gv_height = height();

    // Since this isn't a "proper" OpenGL context,
    // we don't have to wait until after things are
    // initialized to set up
    const char *acmd = "attach";
    dmp = dm_open((void *)v, NULL, "swrast", 1, &acmd);
    if (!dmp)
	return;
    dm_configure_win(dmp, 0);
    dm_set_pathname(dmp, "SWDM");
    dm_set_zbuffer(dmp, 1);

    // Using the full GED_MIN/GED_MAX was causing drawing artifacts with moss
    // in shaded mode, but -1,1 clips geometry quickly as we start to zoom in.
    // -100,100 seems to work, but may need a better long term solution to
    // this... maybe basing it on the currently visible object bounds?
    fastf_t windowbounds[6] = { -1, 1, -1, 1, -100, 100 };
    dm_set_win_bounds(dmp, windowbounds);

    v->dmp = dmp;

    // This is an important Qt setting for interactivity - it allowing key
    // bindings to propagate to this widget and trigger actions such as
    // resolution scaling, rotation, etc.
    setFocusPolicy(Qt::WheelFocus);
}

dmSW::~dmSW()
{
    BU_PUT(v, struct bview);
}

void dmSW::paintEvent(QPaintEvent *e)
{
    // Go ahead and set the flag, but (unlike the rendering thread
    // implementation) we need to do the draw routine every time in paintGL, or
    // we end up with unrendered frames.
    dm_set_dirty(dmp, 0);

    if (gedp) {
	if (!m_init) {
	    gedp->ged_dmp = (void *)dmp;
	    bu_ptbl_ins(gedp->ged_all_dmp, (long int *)dmp);
	    dm_set_vp(dmp, &gedp->ged_gvp->gv_scale);
	    dm_set_width((struct dm *)gedp->ged_dmp, width());
	    dm_set_height((struct dm *)gedp->ged_dmp, height());
	    gedp->ged_gvp->gv_width = dm_get_width((struct dm *)gedp->ged_dmp);
	    gedp->ged_gvp->gv_height = dm_get_height((struct dm *)gedp->ged_dmp);
	    dm_configure_win((struct dm *)gedp->ged_dmp, 0);
	    m_init = true;
	}

	matp_t mat = gedp->ged_gvp->gv_model2view;
	dm_loadmatrix(dmp, mat, 0);
	unsigned char geometry_default_color[] = { 255, 0, 0 };
	dm_draw_begin(dmp);
	dm_draw_display_list(dmp, gedp->ged_gdp->gd_headDisplay,
		1.0, gedp->ged_gvp->gv_isize, -1, -1, -1, 1,
		0, 0, geometry_default_color, 1, 0);

	// Faceplate drawing
	dm_draw_viewobjs(gedp->ged_wdbp, v, NULL, gedp->ged_wdbp->dbip->dbi_base2local, gedp->ged_wdbp->dbip->dbi_local2base);

	dm_draw_end(dmp);
    }

    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(dmp, &dm_image, 1, 1)) {
	return;
    }
    QImage image(dm_image, dm_get_width(dmp), dm_get_height(dmp), QImage::Format_RGBA8888);
    QImage img32 = image.convertToFormat(QImage::Format_RGB32);
    QPainter painter(this);
    painter.drawImage(this->rect(), img32);
    bu_free(dm_image, "copy of backend image");
    QWidget::paintEvent(e);
}

void dmSW::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (gedp && gedp->ged_dmp) {
	dm_set_width((struct dm *)gedp->ged_dmp, width());
	dm_set_height((struct dm *)gedp->ged_dmp, height());
	gedp->ged_gvp->gv_width = width();
	gedp->ged_gvp->gv_height = height();
	dm_configure_win((struct dm *)gedp->ged_dmp, 0);
	dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
    }
}

void dmSW::ged_run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
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

	// Clear the edit flags (TODO - really should only do this for objects active in
	// the scene...)
	struct directory *dp;
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		dp->edit_flag = 0;
	    }
	}

	size_t scene_cnt = BU_PTBL_LEN(v->gv_db_grps) + BU_PTBL_LEN(v->gv_view_objs);

	ged_exec(gedp, argc, argv);
	if (msg)
	    bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

	if (v->gv_cleared && (!v->gv_view_objs || !BU_PTBL_LEN(v->gv_view_objs))) {
	    const char *aav[2];
	    aav[0] = "autoview";
	    aav[1] = NULL;
	    ged_autoview(gedp, 1, (const char **)aav);
	    v->gv_cleared = 0;
	    bview_update(v);
	}
	unsigned long long dhash = dm_hash((struct dm *)gedp->ged_dmp);
	if (dhash != prev_dhash) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}
	unsigned long long vhash = bview_hash(v);
	if (vhash != prev_vhash) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}

	/* Note - these are for the display_list drawing list, which isn't
	 * being used for the newer command forms.  It is needed, however,
	 * for the older draw command changes to show up */
	unsigned long long lhash = dl_name_hash(gedp);
	unsigned long long lhash_edit = lhash;
	lhash_edit += dl_update(gedp);
	if (lhash_edit != prev_lhash) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}
	unsigned long long ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
	if (ghash != prev_ghash) {
	    prev_ghash = ghash;
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}

	// For db obj view list objects, check the dp edit flags and do any necessary
	// redrawing.
	if (ged_view_update(gedp, scene_cnt) > 0) {
	    dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	}

	if (dm_get_dirty((struct dm *)gedp->ged_dmp))
	    update();
    }
}


void dmSW::keyPressEvent(QKeyEvent *k) {

    if (!gedp || !gedp->ged_dmp) {
	QWidget::keyPressEvent(k);
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

    QWidget::keyPressEvent(k);
}

void dmSW::mousePressEvent(QMouseEvent *e) {

    if (!gedp || !gedp->ged_dmp) {
	QWidget::mousePressEvent(e);
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

    QWidget::mousePressEvent(e);
}

void dmSW::mouseMoveEvent(QMouseEvent *e)
{
    if (!gedp || !gedp->ged_dmp) {
	QWidget::mouseMoveEvent(e);
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

    QWidget::mouseMoveEvent(e);
}

void dmSW::wheelEvent(QWheelEvent *e) {

    if (!gedp || !gedp->ged_dmp) {
	QWidget::wheelEvent(e);
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

    QWidget::wheelEvent(e);
}

void dmSW::mouseReleaseEvent(QMouseEvent *e) {

    // To avoid an abrupt jump in scene motion the next time movement is
    // started with the mouse, after we release we return to the default state.
    x_prev = -INT_MAX;
    y_prev = -INT_MAX;

    QWidget::mouseReleaseEvent(e);
}

void dmSW::save_image() {
    // Set up a QImage with the rendered output..
    unsigned char *dm_image;
    if (dm_get_display_image(dmp, &dm_image, 1, 1)) {
	return;
    }
    QImage image(dm_image, dm_get_width(dmp), dm_get_height(dmp), QImage::Format_RGBA8888);
    QImage img32 = image.convertToFormat(QImage::Format_RGB32);
    img32.save("file.png");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

