/*                      Q G V I E W C T R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2023 United States Government as represented by
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
/** @file QgViewCtrl.cpp
 *
 * Qt BRL-CAD View Control button panel implementation
 *
 */

#include "common.h"

#include "bu/env.h"
#include "qtcad/QgViewCtrl.h"
#include "qtcad/QgSignalFlags.h"


QgViewCtrl::QgViewCtrl(QWidget *pparent, struct ged *pgedp) : QToolBar(pparent)
{
    gedp = pgedp;

    this->setStyleSheet("QToolButton{margin:0px;}");

    sca = addAction(QIcon(QPixmap(":images/view/view_scale.png")), "Scale");
    rot = addAction(QIcon(QPixmap(":images/view/view_rotate.png")), "Rotate");
    tra = addAction(QIcon(QPixmap(":images/view/view_translate.png")), "Translate");
    center = addAction(QIcon(QPixmap(":images/view/view_center.png")), "Center");

    addSeparator();

    raytrace = addAction(QIcon(QPixmap(":images/view/raytrace.png")), "Raytrace");
    fb_mode = addAction(QIcon(QPixmap(":images/view/framebuffer_off.png")), "Framebuffer Off/Overlay/Underlay");
    fb_clear = addAction(QIcon(QPixmap(":images/view/framebuffer_clear.png")), "Clear Framebuffer");

    // Connect buttons to standard actions
    connect(sca, &QAction::triggered, this, &QgViewCtrl::sca_mode);
    connect(rot, &QAction::triggered, this, &QgViewCtrl::rot_mode);
    connect(tra, &QAction::triggered, this, &QgViewCtrl::tra_mode);
    connect(center, &QAction::triggered, this, &QgViewCtrl::center_mode);
    connect(raytrace, &QAction::triggered, this, &QgViewCtrl::raytrace_cmd);
    connect(fb_clear, &QAction::triggered, this, &QgViewCtrl::fbclear_cmd);
    connect(fb_mode, &QAction::triggered, this, &QgViewCtrl::fb_mode_cmd);
}

QgViewCtrl::~QgViewCtrl()
{
}

void
QgViewCtrl::sca_mode()
{
    QTCAD_SLOT("QgViewCtrl::sca_mode", 1);
    emit lmouse_mode(BV_SCALE);
}

void
QgViewCtrl::rot_mode()
{
    QTCAD_SLOT("QgViewCtrl::rot_mode", 1);
    emit lmouse_mode(BV_ROT);
}

void
QgViewCtrl::tra_mode()
{
    QTCAD_SLOT("QgViewCtrl::tra_mode", 1);
    emit lmouse_mode(BV_TRANS);
}

void
QgViewCtrl::center_mode()
{
    QTCAD_SLOT("QgViewCtrl::center_mode", 1);
    emit lmouse_mode(BV_CENTER);
}


void
QgViewCtrl::fbclear_cmd()
{
    QTCAD_SLOT("QgViewCtrl::fbclear_cmd", 1);
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    const char *av[2] = {NULL};
    av[0] = "fbclear";
    ged_exec(gedp, 1, (const char **)av);
    emit view_changed(QG_VIEW_REFRESH);
}

void
QgViewCtrl::fb_mode_cmd()
{
    QTCAD_SLOT("QgViewCtrl::fb_mode_cmd", 1);
    if (!gedp->ged_gvp)
	return;
    struct bview *v = gedp->ged_gvp;
    switch (v->gv_s->gv_fb_mode) {
	case 0:
	    v->gv_s->gv_fb_mode = 2;
	    break;
	case 2:
	    v->gv_s->gv_fb_mode = 1;
	    break;
	case 1:
	    v->gv_s->gv_fb_mode = 0;
	    break;
	default:
	    bu_log("Error - invalid fb mode: %d\n", v->gv_s->gv_fb_mode);
    }
    emit view_changed(QG_VIEW_REFRESH);
}

void
QgViewCtrl::do_view_update(unsigned long long flags)
{
    QTCAD_SLOT("QgViewCtrl::do_view_update", 1);
    if (!gedp->ged_gvp || !flags)
	return;
    struct bview *v = gedp->ged_gvp;
    switch (v->gv_s->gv_fb_mode) {
	case 0:
	    fb_mode->setIcon(QIcon(QPixmap(":images/view/framebuffer_off.png")));
	    break;
	case 1:
	    fb_mode->setIcon(QIcon(QPixmap(":images/view/framebuffer.png")));
	    break;
	case 2:
	    fb_mode->setIcon(QIcon(QPixmap(":images/view/framebuffer_underlay.png")));
	    break;
	default:
	    bu_log("Error - invalid fb mode: %d\n", v->gv_s->gv_fb_mode);
    }
}

void rt_cmd_start(int pid, void *ctx)
{
    QgViewCtrl *vctrl = (QgViewCtrl *)ctx;
    vctrl->raytrace_start(pid);
}

void rt_cmd_done(int pid, void *ctx)
{
    QgViewCtrl *vctrl = (QgViewCtrl *)ctx;
    vctrl->raytrace_done(pid);
}

void
QgViewCtrl::raytrace_cmd()
{
    QTCAD_SLOT("QgViewCtrl::raytrace_cmd", 1);
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    const char *av[4] = {NULL};
    struct bu_vls pid_str = BU_VLS_INIT_ZERO;

    gedp->ged_subprocess_init_callback = &rt_cmd_start;
    gedp->ged_subprocess_end_callback = &rt_cmd_done;
    gedp->ged_subprocess_clbk_context = (void *)this;

    if (raytrace_running) {
	if (pid < 0)
	    goto cmd_cleanup;
	bu_vls_sprintf(&pid_str, "%d", pid);
	av[0] = "process";
	av[1] = "pabort";
	av[2] = bu_vls_cstr(&pid_str);
	ged_exec(gedp, 3, (const char **)av);
	goto cmd_cleanup;
    }

    av[0] = "ert";
    ged_exec(gedp, 1, (const char **)av);
    emit view_changed(QG_VIEW_REFRESH);

cmd_cleanup:
    gedp->ged_subprocess_init_callback = NULL;
    gedp->ged_subprocess_end_callback =NULL;
    gedp->ged_subprocess_clbk_context = NULL;
    bu_vls_free(&pid_str);
}

void
QgViewCtrl::raytrace_start(int rpid)
{
    QTCAD_SLOT("QgViewCtrl::raytrace_start", 1);
    raytrace->setIcon(QIcon(QPixmap(":images/view/raytrace_abort.png")));
    raytrace_running = true;
    pid = rpid;
}

void
QgViewCtrl::raytrace_done(int)
{
    QTCAD_SLOT("QgViewCtrl::raytrace_done", 1);
    raytrace->setIcon(QIcon(QPixmap(":images/view/raytrace.png")));
    raytrace_running = false;
    pid = -1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


