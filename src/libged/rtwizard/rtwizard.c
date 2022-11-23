/*                         R T W I Z A R D . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "bu/app.h"
#include "bu/process.h"


#include "../ged_private.h"

int
_ged_run_rtwizard(struct ged *gedp, int cmd_len, const char **gd_rt_cmd)
{
    struct ged_subprocess *run_rtp;
    struct bu_process *p;

    bu_process_exec(&p, gd_rt_cmd[0], cmd_len, (const char **)gd_rt_cmd, 0, 0);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int i = 0; i < cmd_len; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	return BRLCAD_ERROR;
    }

    BU_GET(run_rtp, struct ged_subprocess);
    run_rtp->magic = GED_CMD_MAGIC;
    run_rtp->stdin_active = 0;
    run_rtp->stdout_active = 0;
    run_rtp->stderr_active = 0;
    bu_ptbl_ins(&gedp->ged_subp, (long *)run_rtp);

    run_rtp->p = p;
    run_rtp->aborted = 0;
    run_rtp->gedp = gedp;

    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(run_rtp, BU_PROCESS_STDERR, _ged_rt_output_handler, (void *)run_rtp);
    }

    return BRLCAD_OK;
}


int
ged_rtwizard_core(struct ged *gedp, int argc, const char *argv[])
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
    int ret = BRLCAD_OK;

    const char *bin;
    char rtscript[256] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (gedp->ged_gvp->gv_perspective > 0)
	/* rtwizard --no_gui -perspective p -i db.g --viewsize size --orientation "A B C D" --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2;
    else
	/* rtwizard --no_gui -i db.g --viewsize size --orientation "A B C D" --eye_pt "X Y Z" */
	args = argc + 1 + 1 + 1 + 2 + 2 + 2 + 2;

    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    bin = bu_dir(NULL, 0, BU_DIR_BIN, NULL);
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
    *vp++ = gedp->dbip->dbi_filename;

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


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rtwizard_cmd_impl = {
    "rtwizard",
    ged_rtwizard_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rtwizard_cmd = { &rtwizard_cmd_impl };
const struct ged_cmd *rtwizard_cmds[] = { &rtwizard_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rtwizard_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
