/*                      Q V I E W C T R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file QViewCtrl.cpp
 *
 * Qt BRL-CAD View Control button panel implementation
 *
 */

#include "common.h"

#include "bu/env.h"
#include "qtcad/QViewCtrl.h"
#include "qtcad/SignalFlags.h"


QViewCtrl::QViewCtrl(QWidget *pparent, struct ged *pgedp) : QToolBar(pparent)
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
    connect(sca, &QAction::triggered, this, &QViewCtrl::sca_mode);
    connect(rot, &QAction::triggered, this, &QViewCtrl::rot_mode);
    connect(tra, &QAction::triggered, this, &QViewCtrl::tra_mode);
    connect(center, &QAction::triggered, this, &QViewCtrl::center_mode);
    connect(raytrace, &QAction::triggered, this, &QViewCtrl::raytrace_cmd);
    connect(fb_clear, &QAction::triggered, this, &QViewCtrl::fbclear_cmd);
    connect(fb_mode, &QAction::triggered, this, &QViewCtrl::fb_mode_cmd);
}

QViewCtrl::~QViewCtrl()
{
}

void
QViewCtrl::sca_mode()
{
    emit lmouse_mode(BV_SCALE);
}

void
QViewCtrl::rot_mode()
{
    emit lmouse_mode(BV_ROT);
}

void
QViewCtrl::tra_mode()
{
    emit lmouse_mode(BV_TRANS);
}

void
QViewCtrl::center_mode()
{
    emit lmouse_mode(BV_CENTER);
}


void
QViewCtrl::fbclear_cmd()
{
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    const char *av[2] = {NULL};
    av[0] = "fbclear";
    ged_exec(gedp, 1, (const char **)av);
    emit view_changed(QTCAD_VIEW_REFRESH);
}

void
QViewCtrl::fb_mode_cmd()
{
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
    emit view_changed(QTCAD_VIEW_REFRESH);
}

void
QViewCtrl::do_view_update(unsigned long long flags)
{
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
    QViewCtrl *vctrl = (QViewCtrl *)ctx;
    vctrl->raytrace_start(pid);
}

void rt_cmd_done(int pid, void *ctx)
{
    QViewCtrl *vctrl = (QViewCtrl *)ctx;
    vctrl->raytrace_done(pid);
}

void
QViewCtrl::raytrace_cmd()
{
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
    emit view_changed(QTCAD_VIEW_REFRESH);

cmd_cleanup:
    gedp->ged_subprocess_init_callback = NULL;
    gedp->ged_subprocess_end_callback =NULL;
    gedp->ged_subprocess_clbk_context = NULL;
    bu_vls_free(&pid_str);
}

void
QViewCtrl::raytrace_start(int rpid)
{
    raytrace->setIcon(QIcon(QPixmap(":images/view/raytrace_abort.png")));
    raytrace_running = true;
    pid = rpid;
}

void
QViewCtrl::raytrace_done(int)
{
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


