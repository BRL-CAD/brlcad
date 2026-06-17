/*                         R T C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
 * TODO - should these outputs always be in the shared view_obj set,
 * even when adaptive plotting is on?
 */

#include "common.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"

#include <set>
#include <string>

#include "bu/app.h"

#include "../ged_private.h"
#include "bsg/feature.h"

struct ged_rtcheck {
    struct ged_subprocess *rrtp;
    FILE *fp;
    struct ged_uplot_stream *uplot;
    double csize;
    void *chan;
    int read_failed;
    int draw_read_failed;
};

/* Finalize an rtcheck subprocess: wait, free the uplot stream, free the
 * ged_subprocess and ged_rtcheck.  This MUST be called on the GUI
 * (main) thread for asynchronous hosts (qged) — see _ged_rt_finalize
 * for the rationale.
 */
static void
_ged_rtcheck_finalize(struct ged_rtcheck *rtcp)
{
    struct ged_subprocess *p = rtcp->rrtp;
    struct ged *gedp = p->gedp;

    bu_log("doing cleanup\n");

    bu_process_file_close(p->p, BU_PROCESS_STDOUT);
    /* wait for the forked process */
    int retcode = bu_process_wait_n(&p->p, 0);
    if (retcode != 0) {
	_ged_wait_status(gedp->ged_result_str, retcode);
    }
    bu_ptbl_rm(&gedp->ged_subp, (long *)p);
    BU_PUT(p, struct ged_subprocess);
    _ged_uplot_stream_free(rtcp->uplot);
    BU_PUT(rtcp, struct ged_rtcheck);
}

static void
rtcheck_handler_cleanup(struct ged_rtcheck *rtcp, int type)
{
    struct ged_subprocess *p = rtcp->rrtp;
    struct ged *gedp = p->gedp;
    bu_log("handler cleanup: %d\n", type);

    /* type == -1 is the canonical "all stream listeners gone, finalize
     * on the GUI thread" entry, dispatched by qged's QgConsole::detach.
     */
    if (type == -1) {
	_ged_rtcheck_finalize(rtcp);
	return;
    }

    /* Done watching for output on this stream — detach this stream's
     * listener.  Do not try to drive the other streams' listeners from
     * here; each stream's worker-thread invocation will reach this
     * function for its own EOF.
     *
     * For synchronous hosts (tclcad/gsh) ged_delete_io_handler clears
     * the corresponding stdXXX_active flag inline, so by the time we
     * return all streams may already be inactive and we can finalize
     * synchronously on the main thread.  For asynchronous hosts (qged)
     * ged_delete_io_handler only disconnects the QSocketNotifier; the
     * stream-active flag is cleared later, on the GUI thread, by
     * QgConsole::detach, which then re-enters us with type == -1.
     */
    if (gedp->ged_delete_io_handler) {
	(*gedp->ged_delete_io_handler)(p, (bu_process_io_t)type);
    } else {
	switch (type) {
	    case BU_PROCESS_STDIN:  p->stdin_active  = 0; break;
	    case BU_PROCESS_STDOUT: p->stdout_active = 0; break;
	    case BU_PROCESS_STDERR: p->stderr_active = 0; break;
	}
    }

    /* If all streams are now inactive synchronously (no async
     * delete_io_handler in effect), finalize here.  Otherwise the
     * type == -1 re-entry will finalize on the GUI thread.
     */
    if (!p->stdin_active && !p->stdout_active && !p->stderr_active)
	_ged_rtcheck_finalize(rtcp);
}

static void
rtcheck_vector_handler(void *clientData, int type)
{
    int value = 0;
    struct ged_rtcheck *rtcp = (struct ged_rtcheck *)clientData;
    struct ged_subprocess *rrtp = rtcp->rrtp;
    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");
    struct ged *gedp = rrtp->gedp;

    /* Get vector output from rtcheck */
    if (!rtcp->draw_read_failed && (feof(rtcp->fp) || (value = getc(rtcp->fp)) == EOF)) {
	rtcp->draw_read_failed = 1;

	// Clear any prior rtcheck outputs - whether or not we have new
	// overlaps to draw, we're eliminating all the old objects
	//
	// Phase A1 (drawing_stack_modernization): use feature-scope traversal
	// instead of the legacy BSG_SOURCE_VIEW ptbl shim.  Both shared and
	// local view scopes can hold prior rtcheck output, so visit ALL.
	const char *sname = "rtcheck::";
	struct bsg_view *v = gedp->ged_gvp;
	std::set<std::string> robjs;
	struct _rtcheck_collect_ctx {
	    const char *sname;
	    size_t sname_len;
	    std::set<std::string> *robjs;
	} cctx = {sname, strlen(sname), &robjs};
	auto _collect_rtcheck = [](bsg_feature_ref, const struct bsg_feature_record *rec, void *data) -> int {
	    struct _rtcheck_collect_ctx *c = (struct _rtcheck_collect_ctx *)data;
	    const char *feature_name = rec ? rec->name : NULL;
	    if (feature_name && !bu_strncmp(c->sname, feature_name, c->sname_len))
		c->robjs->insert(std::string(feature_name));
	    return 1;
	};
	bsg_feature_visit(v, BSG_FEATURE_SCOPE_ALL, _collect_rtcheck, &cctx);
	std::set<std::string>::iterator r_it;
	for (r_it = robjs.begin(); r_it != robjs.end(); r_it++) {
	    bsg_feature_remove(v, r_it->c_str());
	}

	/* Add any visual output generated by rtcheck, or clear stale output if
	 * the stream contained no drawable commands. */
	(void)_ged_uplot_stream_publish_feature(gedp, rtcp->uplot, sname);
    }

    if (rtcp->read_failed && rtcp->draw_read_failed) {
	rtcheck_handler_cleanup(rtcp, type);
	return;
    }

    if (!rtcp->draw_read_failed) {
	(void)_ged_uplot_stream_process(rtcp->uplot, rtcp->fp, value);
    }
}

static void
rtcheck_output_handler(void *clientData, int type)
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
	rtcheck_handler_cleanup(rtcp, type);
	return;
    }

    line[count] = '\0';
    if (gedp->ged_output_handler != (void (*)(struct ged *, char *))0) {
	ged_output_handler_cb(gedp, line);
    }	else {
	bu_vls_printf(gedp->ged_result_str, "%s", line);
    }
}

/*
 * Check for overlaps in the current view.
 *
 * Usage:
 * rtcheck options
 *
 */
extern "C" int
ged_rtcheck2_core(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    FILE *fp;
    struct ged_rtcheck *rtcp;
    struct bu_process *p = NULL;
    char ** gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
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
    *vp++ = (char *)"-M";
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
    rtcp->csize = gedp->ged_gvp->gv_scale * 0.01;
    rtcp->uplot = _ged_uplot_stream_create(rtcp->csize, gedp->i->ged_gdp->gd_uplotOutputMode);
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
