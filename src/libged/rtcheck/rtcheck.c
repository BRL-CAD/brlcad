/*                         R T C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/rtcheck.c
 *
 * The rtcheck command.
 *
 */

#include "common.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"

#include "bu/app.h"

#include "../ged_private.h"

static void
dl_set_flag(struct bu_list *hdlp, int flag)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    /* calculate the bounding for of all solids being displayed */
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    sp->s_flag = flag;
	}

	gdlp = next_gdlp;
    }
}

struct ged_rtcheck {
    struct ged_subprocess *rrtp;
    FILE *fp;
    struct bv_vlblock *vbp;
    struct bu_list *vhead;
    double csize;
    void *chan;
    int read_failed;
    int draw_read_failed;
};

static void
rtcheck_handler_cleanup(struct ged_rtcheck *rtcp)
{
    int retcode;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    struct ged *gedp = rrtp->gedp;

    if (gedp->ged_delete_io_handler) {
	(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDOUT);
	(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDERR);
    }

    // FIXME: Windows throws 'invalid handle specified' exception here - is ged_delete_io_handler already closing STDOUT?
    bu_process_file_close(rrtp->p, BU_PROCESS_STDOUT);

    /* wait for the forked process */
    retcode = bu_process_wait_n(&rrtp->p, 0);
    if (retcode != 0) {
	_ged_wait_status(gedp->ged_result_str, retcode);
    }

    bu_ptbl_rm(&gedp->ged_subp, (long *)rrtp);
    BU_PUT(rrtp, struct ged_subprocess);
    BU_PUT(rtcp, struct ged_rtcheck);
}

static void
rtcheck_vector_handler(void *clientData, int UNUSED(mask))
{
    int value = 0;
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");
    struct ged *gedp = rrtp->gedp;

    /* Get vector output from rtcheck */
    if (!rtcp->draw_read_failed && (feof(rtcp->fp) || (value = getc(rtcp->fp)) == EOF)) {
	size_t i;
	int have_visual = 0;
	const char *sname = "OVERLAPS";

	rtcp->draw_read_failed = 1;

	dl_set_flag(gedp->i->ged_gdp->gd_headDisplay, DOWN);

	/* Add overlay (or, if nothing to draw, clear any stale overlay) */
	if (rtcp->vbp) {
	    for (i = 0; i < rtcp->vbp->nused; i++) {
		if (!BU_LIST_IS_EMPTY(&(rtcp->vbp->head[i]))) {
		    have_visual = 1;
		    break;
		}
	    }
	}

	if (have_visual) {
	    _ged_cvt_vlblock_to_solids(gedp, rtcp->vbp, sname, 0);
	    bv_vlblock_free(rtcp->vbp);
	} else {
	    /* TODO - yuck.  This name is a product of the internals of the
	     * "_ged_cvt_vlblock_to_solids" routine.  We should have a way to kill
	     * command-specific display related objects cleanly...  hard-coding
	     * this name ties proper stale drawing cleanup to the internals of the
	     * current phony object drawing system.*/
	    const char *sname_obj = "OVERLAPSffff00";
	    struct directory *dp = db_lookup(gedp->dbip, sname_obj, LOOKUP_QUIET);
	    if (dp != RT_DIR_NULL) {
		dl_erasePathFromDisplay(gedp, sname_obj, 0);
	    }
	}

	rtcp->vbp = NULL;
    }

    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp);
	return;
    }

    if (!rtcp->draw_read_failed) {
	(void)rt_process_uplot_value(&rtcp->vhead,
		rtcp->vbp,
		rtcp->fp,
		value,
		rtcp->csize,
		gedp->i->ged_gdp->gd_uplotOutputMode);
    }
}

static void
rtcheck_output_handler(void *clientData, int UNUSED(mask))
{
    int count;
    char line[RT_MAXLINE] = {0};
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");
    struct ged *gedp = rrtp->gedp;

    /* Get textual output from rtcheck */
    if ((count = bu_process_read_n(rrtp->p, BU_PROCESS_STDERR, RT_MAXLINE, (char *)line)) <= 0) {
	rtcp->read_failed = 1;
	if (gedp->i->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->i->ged_gdp->gd_rtCmdNotify(0);
    }


    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp);
	return;
    }

    line[count] = '\0';
    if (gedp->ged_output_handler != (void (*)(struct ged *, char *))0) {
	ged_output_handler_cb(gedp, line);
    }	else {
	bu_vls_printf(gedp->ged_result_str, "%s", line);
    }
}

extern int ged_rtcheck2_core(struct ged *gedp, int argc, const char *argv[]);

/*
 * Check for overlaps in the current view.
 *
 * Usage:
 * rtcheck options
 *
 */
int
ged_rtcheck_core(struct ged *gedp, int argc, const char *argv[])
{
    if (gedp->new_cmd_forms)
	return ged_rtcheck2_core(gedp, argc, argv);

    char **vp;
    int i;
    FILE *fp;
    struct ged_rtcheck *rtcp;
    struct bu_process *p = NULL;
    char ** gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
    struct bu_list *vlfree = &rt_vlfree;

    vect_t eye_model;

    const char *bin;
    char rtcheck[256] = {0};
    size_t args = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bin = bu_dir(NULL, 0, BU_DIR_BIN, NULL);
    if (bin) {
	snprintf(rtcheck, 256, "%s/%s", bin, argv[0]);
    }

    args = argc + 7 + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    *vp++ = rtcheck;
    *vp++ = "-M";
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];

    *vp++ = gedp->dbip->dbi_filename;

    /*
     * Now that we've grabbed all the options, if no args remain,
     * append the names of all stuff currently displayed.
     * Otherwise, simply append the remaining args.
     */
    if (i == argc) {
	gd_rt_cmd_len = vp - gd_rt_cmd;
	int cmd_prev_len = gd_rt_cmd_len;
	gd_rt_cmd_len += ged_who_argv(gedp, vp, (const char **)&gd_rt_cmd[args]);
	if (gd_rt_cmd_len == cmd_prev_len) {
	    // Nothing specified, nothing displayed
	    bu_vls_printf(gedp->ged_result_str, "no objects displayed\n");
	    bu_free(gd_rt_cmd, "free gd_rt_cmd");
	    return BRLCAD_ERROR;
	}
    } else {
	while (i < argc)
	    *vp++ = (char *)argv[i++];
	*vp = 0;
	vp = &gd_rt_cmd[0];
	while (*vp) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", *vp);
	    vp++;
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    bu_process_create(&p, (const char **)gd_rt_cmd, BU_PROCESS_DEFAULT);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int pi = 0; pi < gd_rt_cmd_len; pi++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[pi]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	bu_free(gd_rt_cmd, "free gd_rt_cmd");
	return BRLCAD_ERROR;
    }

    fp = bu_process_file_open(p, BU_PROCESS_STDIN);

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp, eye_model, -1, NULL);

    bu_process_file_close(p, BU_PROCESS_STDIN);

    /* create the rtcheck struct */
    BU_GET(rtcp, struct ged_rtcheck);
    rtcp->fp = bu_process_file_open(p, BU_PROCESS_STDOUT);
    /* Needed on Windows for successful rtcheck drawing data communication */
    setmode(fileno(rtcp->fp), O_BINARY);
    rtcp->vbp = bv_vlblock_init(vlfree, 32);
    rtcp->vhead = bv_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
    rtcp->csize = gedp->ged_gvp->gv_scale * 0.01;
    rtcp->read_failed = 0;
    rtcp->draw_read_failed = 0;

    /* create the ged_subprocess container */
    BU_GET(rtcp->rrtp, struct ged_subprocess);
    rtcp->rrtp->magic = GED_CMD_MAGIC;
    rtcp->rrtp->p = p;
    rtcp->rrtp->aborted = 0;
    rtcp->rrtp->gedp = gedp;
    rtcp->rrtp->stdin_active = 0;
    rtcp->rrtp->stdout_active = 0;
    rtcp->rrtp->stderr_active = 0;

    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(rtcp->rrtp, BU_PROCESS_STDOUT, rtcheck_vector_handler, (void *)rtcp);
    }
    bu_ptbl_ins(&gedp->ged_subp, (long *)rtcp->rrtp);

    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(rtcp->rrtp, BU_PROCESS_STDERR, rtcheck_output_handler, (void *)rtcp);
    }

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rtcheck_cmd_impl = {
    "rtcheck",
    ged_rtcheck_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rtcheck_cmd = { &rtcheck_cmd_impl };
const struct ged_cmd *rtcheck_cmds[] = { &rtcheck_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rtcheck_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
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
