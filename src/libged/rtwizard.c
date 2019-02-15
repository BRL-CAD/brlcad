/*                         R T W I Z A R D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file libged/rtwizard.c
 *
 * The rtwizard command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "tcl.h"
#include "bu/app.h"
#include "bu/process.h"


#include "./ged_private.h"

struct _ged_rt_client_data {
    struct ged_subprocess *rrtp;
    struct ged *gedp;
};


int
_ged_run_rtwizard(struct ged *gedp, int cmd_len, const char **gd_rt_cmd)
{
    struct ged_subprocess *run_rtp;
    struct _ged_rt_client_data *drcdp;
    struct bu_process *p;

    bu_process_exec(&p, gd_rt_cmd[0], cmd_len, (const char **)gd_rt_cmd, 0, 0);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int i = 0; i < cmd_len; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	return GED_ERROR;
    }

    BU_GET(run_rtp, struct ged_subprocess);
    BU_LIST_INIT(&run_rtp->l);
    BU_LIST_APPEND(&gedp->gd_headSubprocess.l, &run_rtp->l);

    run_rtp->p = p;

    /* must be BU_GET() to match release in _ged_rt_output_handler */
    BU_GET(drcdp, struct _ged_rt_client_data);
    drcdp->gedp = gedp;
    drcdp->rrtp = run_rtp;

    _ged_create_io_handler(&(run_rtp->chan), p, BU_PROCESS_STDERR, TCL_READABLE, (void *)drcdp, _ged_rt_output_handler);

    return GED_OK;
}


int
ged_rtwizard(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    char pstring[32];
    int args;
    quat_t quat;
    vect_t eye_model;
    struct bu_vls perspective_vls = BU_VLS_INIT_ZERO;
    struct bu_vls size_vls = BU_VLS_INIT_ZERO;
    struct bu_vls orient_vls = BU_VLS_INIT_ZERO;
    struct bu_vls eye_vls = BU_VLS_INIT_ZERO;
    char **gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
    int ret = GED_OK;

    const char *bin;
    char rtscript[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (gedp->ged_gvp->gv_perspective > 0)
	/* rtwizard --no_gui -perspective p -i db.g --viewsize size --orientation "A B C D} --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2;
    else
	/* rtwizard --no_gui -i db.g --viewsize size --orientation "A B C D} --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2;

    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    bin = bu_brlcad_root("bin", 1);
    if (bin) {
	snprintf(rtscript, 256, "%s/rtwizard", bin);
    } else {
	snprintf(rtscript, 256, "rtwizard");
    }

    _ged_rt_set_eye_model(gedp, eye_model);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);

    bu_vls_printf(&size_vls, "%.15e", gedp->ged_gvp->gv_size);
    bu_vls_printf(&orient_vls, "%.15e %.15e %.15e %.15e", V4ARGS(quat));
    bu_vls_printf(&eye_vls, "%.15e %.15e %.15e", V3ARGS(eye_model));

    vp = &gd_rt_cmd[0];
    *vp++ = rtscript;
    *vp++ = "--no-gui";
    *vp++ = "--viewsize";
    *vp++ = bu_vls_addr(&size_vls);
    *vp++ = "--orientation";
    *vp++ = bu_vls_addr(&orient_vls);
    *vp++ = "--eye_pt";
    *vp++ = bu_vls_addr(&eye_vls);

    if (gedp->ged_gvp->gv_perspective > 0) {
	*vp++ = "--perspective";
	(void)sprintf(pstring, "%g", gedp->ged_gvp->gv_perspective);
	*vp++ = pstring;
    }

    *vp++ = "-i";
    *vp++ = gedp->ged_wdbp->dbip->dbi_filename;

    /* Append all args */
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp = 0;

    /*
     * Accumulate the command string.
     */
    vp = &gd_rt_cmd[0];
    while (*vp)
	bu_vls_printf(gedp->ged_result_str, "%s ", *vp++);
    bu_vls_printf(gedp->ged_result_str, "\n");

    gd_rt_cmd_len = vp - gd_rt_cmd;

    ret = _ged_run_rtwizard(gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd);

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    bu_vls_free(&perspective_vls);
    bu_vls_free(&size_vls);
    bu_vls_free(&orient_vls);
    bu_vls_free(&eye_vls);

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
