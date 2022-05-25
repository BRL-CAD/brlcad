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
    fb_on = addAction(QIcon(QPixmap(":images/view/framebuffer.png")), "Framebuffer On/Off");
    fb_overlay = addAction(QIcon(QPixmap(":images/view/framebuffer_overlay.png")), "Framebuffer Overlay/Underlay");
    fb_clear = addAction(QIcon(QPixmap(":images/view/framebuffer_clear.png")), "Clear Framebuffer");

    // Connect some of the buttons to standard actions
    connect(raytrace, &QAction::triggered, this, &QViewCtrl::raytrace_cmd);
    connect(fb_clear, &QAction::triggered, this, &QViewCtrl::fbclear_cmd);
}

QViewCtrl::~QViewCtrl()
{
}

void
QViewCtrl::fbclear_cmd()
{
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    const char *av[2] = {NULL};
    av[0] = "fbclear";
    ged_exec(gedp, 1, (const char **)av);
    emit gui_changed_view(&gedp->ged_gvp);
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
    emit gui_changed_view(&gedp->ged_gvp);

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


