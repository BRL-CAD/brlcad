/*                       D M A P P . C P P
 * BRL-DM
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadapp.cxx
 *
 * Application level data and functionality implementations.
 *
 */

#include "bu/env.h"
#include "bu/ptbl.h"
#include "ged.h"
#include "./fbserv.h"
#include "dmapp.h"

extern "C" void
qt_create_io_handler(struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;

    QtConsole *c = (QtConsole *)p->gedp->ged_io_data;

    int fd = bu_process_fileno(p->p, t);
    if (fd < 0)
	return;

    c->listen(fd, p, t, callback, data);

    switch (t) {
	case BU_PROCESS_STDIN:
	    p->stdin_active = 1;
	    break;
	case BU_PROCESS_STDOUT:
	    p->stdout_active = 1;
	    break;
	case BU_PROCESS_STDERR:
	    p->stderr_active = 1;
	    break;
    }
}

extern "C" void
qt_delete_io_handler(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p) return;

    QtConsole *c = (QtConsole *)p->gedp->ged_io_data;

    bu_log("qt_delete_io_handler\n");

    // Since these callbacks are invoked from the listener, we can't call
    // the listener destructors directly.  We instead call a routine that
    // emits a single that will notify the console widget it's time to
    // detach the listener.
    switch (t) {
	case BU_PROCESS_STDIN:
	    bu_log("stdin\n");
	    if (p->stdin_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stdout: %d\n", p->stdout_active);
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && c->listeners.find(std::make_pair(p, t)) != c->listeners.end()) {
		c->listeners[std::make_pair(p, t)]->m_notifier->disconnect();
		c->listeners[std::make_pair(p, t)]->on_finished();
		bu_log("stderr: %d\n", p->stderr_active);
	    }
	    p->stderr_active = 0;
	    break;
    }

}

int
DMApp::load_g(const char *filename, int argc, const char *argv[])
{
    if (argc < 0 || !argv)
	return -1;

    gedp = ged_open("db", filename, 1);
    if (!gedp) {
	w->statusBar()->showMessage("open failed");
	return -1;
    }

    gedp->fbs_is_listening = &qdm_is_listening;
    gedp->fbs_listen_on_port = &qdm_listen_on_port;
    gedp->fbs_open_server_handler = &qdm_open_server_handler;
    gedp->fbs_close_server_handler = &qdm_close_server_handler;
    if (w->canvas || w->c4) {
	gedp->fbs_open_client_handler = &qdm_open_client_handler;
    }
    if (w->canvas_sw) {
	gedp->fbs_open_client_handler = &qdm_open_sw_client_handler;
    }
    gedp->fbs_close_client_handler = &qdm_close_client_handler;

    return 0;
}

void
DMApp::ged_run_cmd(struct bu_vls *msg, int argc, const char **argv)
{
    struct ged *prev_gedp = gedp;

    if (gedp) {
	gedp->ged_create_io_handler = &qt_create_io_handler;
	gedp->ged_delete_io_handler = &qt_delete_io_handler;
	gedp->ged_io_data = (void *)this->w->console;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    if (msg)
		bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
	    return;
	}
    } else {

	prev_dhash = 0;
	prev_vhash = 0;

	if (gedp) {

	    // TODO - need to add hashing to check the dm variables as well (i.e. if lighting
	    // was turned on/off by the dm command...)
	    prev_dhash = dm_hash((struct dm *)gedp->ged_dmp);
	    prev_vhash = bv_hash(gedp->ged_gvp);

	    // Clear the edit flags ahead of the ged_exec call, so we can tell if
	    // any geometry changed.
	    struct directory *dp;
	    for (int i = 0; i < RT_DBNHASH; i++) {
		for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    dp->edit_flag = 0;
		}
	    }
	}

	ged_exec(gedp, argc, argv);
	if (msg && gedp)
	    bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));


	// It's possible that a ged_exec will introduce a new gedp - set up accordingly
	if (gedp && prev_gedp != gedp) {
	    bu_ptbl_reset(gedp->ged_all_dmp);
	    if (w->canvas) {
		gedp->ged_dmp = w->canvas->dmp;
		gedp->ged_gvp = w->canvas->v;
		dm_set_vp(w->canvas->dmp, &gedp->ged_gvp->gv_scale);
	    }
	    if (w->canvas_sw) {
		gedp->ged_dmp = w->canvas_sw->dmp;
		gedp->ged_gvp = w->canvas_sw->v;
		dm_set_vp(w->canvas_sw->dmp, &gedp->ged_gvp->gv_scale);
	    }
	    if (w->c4) {
		gedp->ged_dmp = w->c4->c->dmp;
		gedp->ged_gvp = w->c4->c->v;
		dm_set_vp(w->c4->c->dmp, &gedp->ged_gvp->gv_scale);
	    }
	    bu_ptbl_ins_unique(gedp->ged_all_dmp, (long int *)gedp->ged_dmp);
	}

	if (gedp) {
	    if (w->canvas && w->canvas->v) {
		w->canvas->v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;
		w->canvas->v->gv_local2base = gedp->ged_wdbp->dbip->dbi_local2base;
	    }
	    if (w->canvas_sw && w->canvas_sw->v) {
		w->canvas_sw->v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;
		w->canvas_sw->v->gv_local2base = gedp->ged_wdbp->dbip->dbi_local2base;
	    }
	    if (w->c4 && w->c4->c->v) {
		w->c4->c->v->gv_base2local = gedp->ged_wdbp->dbip->dbi_base2local;
		w->c4->c->v->gv_local2base = gedp->ged_wdbp->dbip->dbi_local2base;
	    }

	    /* Check if the ged_exec call changed either the display manager or
	     * the view settings - in either case we'll need to redraw */
	    if (prev_dhash != dm_hash((struct dm *)gedp->ged_dmp)) {
		dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	    }
	    if (prev_vhash != bv_hash(gedp->ged_gvp)) {
		dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	    }

	    // Checks the dp edit flags and does any necessary redrawing.  If
	    // anything changed with the geometry, we also need to redraw
	    if (ged_view_update(gedp) > 0) {
		dm_set_dirty((struct dm *)gedp->ged_dmp, 1);
	    }

	    // If anybody set the flag, trigger an update
	    if (dm_get_dirty((struct dm *)gedp->ged_dmp)) {
		if (w->canvas)
		    w->canvas->update();
		if (w->canvas_sw)
		    w->canvas_sw->update();
		if (w->c4) {
		    w->c4->c->update();
		}
	    }
	} else {
	    // gedp == NULL - can't cheat and use the gedp pointer
	    if (prev_gedp != gedp) {
		if (w->canvas) {
		    dm_set_dirty(w->canvas->dmp, 1);
		    w->canvas->update();
		}
		if (w->canvas_sw) {
		    dm_set_dirty(w->canvas_sw->dmp, 1);
		    w->canvas_sw->update();
		}
		if (w->c4) {
		    dm_set_dirty(w->c4->c->dmp, 1);
		    w->c4->c->update();
		}
	    }
	}
    }
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

