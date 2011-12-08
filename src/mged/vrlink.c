/*                        V R L I N K . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/vrlink.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tcl.h"

#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "pkg.h"
#include "./mged.h"
#include "./mged_dm.h"

extern int (*cmdline_hook)();	/* cmd.c */

static struct pkg_conn *vrmgr;			/* PKG connection to VR mgr */
static char *vr_host = "None";	/* host running VR mgr */
static char *tcp_port = "5555";	/* "gedd", remote mged */

#define VRMSG_ROLE	1	/* from MGED: Identify role of machine */
#define VRMSG_CMD	2	/* to MGED: Command to process */
#define VRMSG_EVENT	3	/* from MGED: device event */
#define VRMSG_POV	4	/* from MGED: point of view info */
#define VRMSG_VLIST	5	/* transfer binary vlist block */
#define VRMSG_CMD_REPLY	6	/* from MGED: reply to VRMSG_CMD */

void ph_cmd(struct pkg_conn *pc, char *buf);
void ph_vlist(struct pkg_conn *pc, char *buf);
static struct pkg_switch pkgswitch[] = {
    { VRMSG_CMD,		ph_cmd,		"Command", NULL },
    { VRMSG_VLIST,		ph_vlist,	"Import vlist", NULL },
    { 0,			0,		NULL, NULL }
};


/*
 * Called from cmdline() for now.
 * Returns -
 * !0 Print prompt for user.  Should always be this.
 * 0 Don't print a prompt
 */
int
vr_event_hook(struct bu_vls *vp)
{
    BU_CK_VLS(vp);

    if (vrmgr == PKC_NULL) {
	cmdline_hook = 0;	/* Relinquish this hook */
	return 1;
    }

    if (pkg_send_vls(VRMSG_EVENT, vp, vrmgr) < 0) {
	bu_log("event: pkg_send VRMSG_EVENT failed, disconnecting\n");
	pkg_close(vrmgr);
	vrmgr = PKC_NULL;
	cmdline_hook = 0;	/* Relinquish this hook */
    }
    return 1;
}


/*
 * Called from the Tk event handler
 */
void
vr_input_hook(void)
{
    int val;

    val = pkg_suckin(vrmgr);
    if (val < 0) {
	bu_log("pkg_suckin() error\n");
    } else if (val == 0) {
	bu_log("vrmgr sent us an EOF\n");
    }
    if (val <= 0) {
	Tcl_DeleteFileHandler(vrmgr->pkc_fd);
	pkg_close(vrmgr);
	vrmgr = PKC_NULL;
	return;
    }
    if (pkg_process(vrmgr) < 0)
	bu_log("vrmgr:  pkg_process error encountered\n");
}


/*
 * Called from refresh().
 */
void
vr_viewpoint_hook(void)
{
    struct bu_vls str;
    static struct bu_vls *old_str = NULL;
    quat_t orient;

    if (vrmgr == PKC_NULL) {
	cmdline_hook = 0;	/* Relinquish this hook */
	return;
    }

    if (!old_str) {
	BU_GET(old_str, struct bu_vls);
	bu_vls_init(old_str);
    }
    bu_vls_init(&str);

    quat_mat2quat(orient, view_state->vs_gvp->gv_rotation);

    /* Need to send current viewpoint to VR mgr */
    /* XXX more will be needed */
    /* Eye point, quaternion for orientation */
    bu_vls_printf(&str, "pov {%e %e %e}   {%e %e %e %e}   %e   {%e %e %e}  %e\n",
		  -view_state->vs_gvp->gv_center[MDX],
		  -view_state->vs_gvp->gv_center[MDY],
		  -view_state->vs_gvp->gv_center[MDZ],
		  V4ARGS(orient),
		  view_state->vs_gvp->gv_scale,
		  V3ARGS(view_state->vs_gvp->gv_eye_pos),
		  view_state->vs_gvp->gv_perspective);

    if (bu_vls_strcmp(old_str, &str) == 0) {
	bu_vls_free(&str);
	return;
    }

    if (pkg_send_vls(VRMSG_POV, &str, vrmgr) < 0) {
	bu_log("viewpoint: pkg_send VRMSG_POV failed, disconnecting\n");
	pkg_close(vrmgr);
	vrmgr = PKC_NULL;
	viewpoint_hook = 0;	/* Relinquish this hook */
    }
    bu_vls_trunc(old_str, 0);
    bu_vls_vlscat(old_str, &str);
    bu_vls_free(&str);
}


/*
 * F _ V R M G R
 *
 * Establish a network link to the VR manager, using libpkg.
 *
 * Syntax:  vrmgr host role
 */
int
f_vrmgr(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_vls str;
    const char *role;

    if (argc < 3) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help vrmgr");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_init(&str);

    if (vrmgr != PKC_NULL) {
	Tcl_AppendResult(interp, "Closing link to VRmgr ",
			 vr_host, "\n", (char *)NULL);
	pkg_close(vrmgr);
	vrmgr = PKC_NULL;
	vr_host = "none";
    }

    vr_host = bu_strdup(argv[1]);
    role = argv[2];

    if (BU_STR_EQUAL(role, "master")) {
    } else if (BU_STR_EQUAL(role, "slave")) {
    } else if (BU_STR_EQUAL(role, "overview")) {
    } else {
	Tcl_AppendResult(interp, "role '", role, "' unknown, must be master/slave/overview\n",
			 (char *)NULL);
	return TCL_ERROR;
    }

    vrmgr = pkg_open(vr_host, tcp_port, "tcp", "", "",
		     pkgswitch, NULL);
    if (vrmgr == PKC_ERROR) {
	Tcl_AppendResult(interp, "mged/f_vrmgr: unable to contact ", vr_host,
			 ", port ", tcp_port, "\n", (char *)NULL);
	vrmgr = PKC_NULL;
	return TCL_ERROR;
    }

    bu_vls_from_argv(&str, argc-2, (const char **)argv+2);

    /* Send initial message declaring our role */
    if (pkg_send_vls(VRMSG_ROLE, &str, vrmgr) < 0) {
	Tcl_AppendResult(interp, "pkg_send VRMSG_ROLE failed, disconnecting\n", (char *)NULL);
	pkg_close(vrmgr);
	vrmgr = NULL;
	return TCL_ERROR;
    }

    /* Establish appropriate hooks */
    if (BU_STR_EQUAL(role, "master")) {
	viewpoint_hook = vr_viewpoint_hook;
    } else if (BU_STR_EQUAL(role, "slave")) {
	cmdline_hook = vr_event_hook;
    } else if (BU_STR_EQUAL(role, "overview")) {
	/* No hooks required, just listen */
    }
    Tcl_CreateFileHandler(vrmgr->pkc_fd, TCL_READABLE,
			  (Tcl_FileProc (*))vr_input_hook, (ClientData)NULL);
    bu_vls_free(&str);

    return TCL_OK;
}


/*
 * P H _ C M D
 *
 * Package handler for incomming commands.  Do whatever he says,
 * and send a reply back.
 */
void
ph_cmd(struct pkg_conn *pc, char *buf)
{
    int status;
    const char *result;
#define CMD_BUFSIZE 1024
    char buffer[CMD_BUFSIZE] = {0};

    status = Tcl_Eval(INTERP, buf);
    result = Tcl_GetStringResult(INTERP);

    snprintf(buffer, CMD_BUFSIZE, "%s", result);

    if (pkg_2send(VRMSG_CMD_REPLY,
		  (status == TCL_OK) ? "Y" : "N", 1,
		  buffer, CMD_BUFSIZE, pc) < 0) {
	bu_log("ph_cmd: pkg_2send reply to vrmgr failed, disconnecting\n");
	pkg_close(vrmgr);
	vrmgr = PKC_NULL;
	cmdline_hook = 0;	/* Relinquish this hook */
    }
    if (buf) (void)free(buf);
}


/*
 * P H _ V L I S T
 *
 * Package handler for incomming vlist.
 * Install whatever phantom solids he wants.
 */
void
ph_vlist(struct pkg_conn *UNUSED(pc), char *buf)
{
    struct bu_list vhead;
    struct bu_vls name;

    bu_vls_init(&name);

    BU_LIST_INIT(&vhead);

    rt_vlist_import(&vhead, &name, (unsigned char *)buf);

    invent_solid(bu_vls_addr(&name), &vhead, 0x0000FF00L, 0);

    bu_vls_free(&name);
    if (buf) (void)free(buf);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
