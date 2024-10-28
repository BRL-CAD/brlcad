/*                           G S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @file gtools/gsh/gsh.cpp
 *
 * This program (the BRL-CAD Geometry SHell) is a low level way to
 * interact with BRL-CAD geometry (.g) files.
 */

#include "common.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "linenoise.hpp"

#include "bu.h"
#include "bv.h"

#define USE_DM 1
#ifdef USE_DM
#  define DM_WITH_RT
#  include "dm.h"
#endif

#include "ged.h"

#define DEFAULT_GSH_PROMPT "g> "

#define GSH_READ_SIZE 1024
void
p_watch(
	std::shared_ptr<std::string> obuf,
	std::atomic<bool> &thread_done,
	int fd,
	std::mutex &buf_mutex
       )
{
    char buffer[GSH_READ_SIZE];
    ssize_t bcnt;
    while ((bcnt = read(fd, buffer, GSH_READ_SIZE)) > 0) {
	buf_mutex.lock();
	obuf.get()->append(buffer, bcnt);
	buf_mutex.unlock();
    }
    thread_done = true;
}

// conceptually, first thing to try is probably just setting up a thread to
// loop with a timer that tries to read from the specified channel.
class ProcessIOHandler {
    public:
	ProcessIOHandler(int f, ged_io_func_t c, void *d);

	std::string read();

    private:
	int fd;
	ged_io_func_t callback;
	std::shared_ptr<void> data;

	std::shared_ptr<std::string> curr_buf;
	std::mutex buf_mutex;
	std::atomic<bool> thread_done = false;
	std::thread watch_thread;
};

ProcessIOHandler::ProcessIOHandler(int f, ged_io_func_t c, void *d)
    : fd(f), callback(c)
{
    data = std::make_shared<void *>(d);
    watch_thread = std::thread(p_watch, curr_buf, std::ref(thread_done), fd, std::ref(buf_mutex));
}

std::string
ProcessIOHandler::read()
{
    std::string lcpy;
    buf_mutex.lock();
    lcpy = *curr_buf.get();
    curr_buf.get()->clear();
    buf_mutex.unlock();
    return lcpy;
}

class GshState {
    public:
	GshState();
	~GshState();

	// Run GED commands
	int eval(int argc, const char **argv);

	// Command line interactive prompt
	std::shared_ptr<linenoise::linenoiseState> l;
	std::atomic<bool> linenoise_done = false;
	std::mutex print_mutex;

	// Create a listener for a subprocess
	void listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d);
	// Active listeners
	std::map<std::pair<struct ged_subprocess *, bu_process_io_t>, ProcessIOHandler *> listeners;

#ifdef USE_DM
	// Display management
	void view_checkpoint();
	void view_update();
	unsigned long long prev_dhash = 0;
	unsigned long long prev_vhash = 0;
	unsigned long long prev_lhash = 0;
	unsigned long long prev_ghash = 0;
#endif

	struct ged *gedp;
	std::string gfile;  // Mostly used to test the post_opendb callback
	bool new_cmd_forms = false;  // Set if we're testing QGED style commands
};

/* For gsh these are mostly no-op, but define placeholder
 * functions in case we need to pre or post actions for
 * database opening in the future. */
extern "C" void
gsh_pre_opendb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
}

extern "C" void
gsh_post_opendb_clbk(struct ged *UNUSED(gedp), void *ctx)
{
    GshState *s = (GshState *)ctx;
    if (!s->gedp->dbip)
	return;
    s->gfile = std::string(s->gedp->dbip->dbi_filename);
}

extern "C" void
gsh_pre_closedb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
}

extern "C" void
gsh_post_closedb_clbk(struct ged *UNUSED(gedp), void *UNUSED(ctx))
{
}



/* Cross platform I/O subprocess listening is tricky.  Tcl and Qt both have
 * specific logic necessary for their environments.  Even the minimalist
 * command line environment has some basic requirements that must be met.
 *
 * May also end up finding a use for nod signals/slots if libuv doesn't do
 * it for us by itself:  https://github.com/fr00b0/nod
 */
extern "C" void
gsh_create_io_handler(struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t callback, void *data)
{
    if (!p || !p->p || !p->gedp || !p->gedp->ged_io_data)
	return;

    int fd = bu_process_fileno(p->p, t);
    if (fd < 0)
	return;

    GshState *gs = (GshState *)p->gedp->ged_io_data;
    gs->listen(fd, p, t, callback, data);

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
gsh_delete_io_handler(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p) return;

    GshState *gs = (GshState *)p->gedp->ged_io_data;

    switch (t) {
	case BU_PROCESS_STDIN:
	    bu_log("stdin\n");
	    if (p->stdin_active && gs->listeners.find(std::make_pair(p, t)) != gs->listeners.end()) {
		gs->listeners.erase(std::make_pair(p, t));
	    }
	    p->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    if (p->stdout_active && gs->listeners.find(std::make_pair(p, t)) != gs->listeners.end()) {
		gs->listeners.erase(std::make_pair(p, t));
	    }
	    p->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    if (p->stderr_active && gs->listeners.find(std::make_pair(p, t)) != gs->listeners.end()) {
		gs->listeners.erase(std::make_pair(p, t));
	    }
	    p->stderr_active = 0;
	    break;
    }

    // All communication has ceased between the app and the subprocess,
    // time to call the end callback (if any)
    if (!p->stdin_active && !p->stdout_active && !p->stderr_active) {
	if (p->end_clbk)
	    p->end_clbk(0, p->end_clbk_data);
    }

    //emit ca->view_update(QG_VIEW_REFRESH);
}



GshState::GshState()
{
    const char *ncmd = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(ncmd, "1"))
	new_cmd_forms = true;

    BU_GET(gedp, struct ged);
    ged_init(gedp);
    BU_GET(gedp->ged_gvp, struct bview);
    bv_init(gedp->ged_gvp, &gedp->ged_views);
    bv_set_add_view(&gedp->ged_views, gedp->ged_gvp);
    bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
    bv_set_add_view(&gedp->ged_views, gedp->ged_gvp);
    bu_ptbl_ins(&gedp->ged_free_views, (long *)gedp->ged_gvp);

#ifdef USE_DM
    view_checkpoint();
#endif

    // Assign gsh specific open/close db handlers to the gedp
    gedp->ged_pre_opendb_callback = &gsh_pre_opendb_clbk;
    gedp->ged_post_opendb_callback = &gsh_post_opendb_clbk;
    gedp->ged_pre_closedb_callback = &gsh_pre_closedb_clbk;
    gedp->ged_post_closedb_callback = &gsh_post_closedb_clbk;
    gedp->ged_db_callback_udata = (void *)this;

    // Assign gsh specific I/O handlers to the gedp
    gedp->ged_create_io_handler = &gsh_create_io_handler;
    gedp->ged_delete_io_handler = &gsh_delete_io_handler;
    gedp->ged_io_data = (void *)this;
}

GshState::~GshState()
{
#ifdef USE_DM
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	if (v->dmp) {
	    dm_close((struct dm *)v->dmp);
	    v->dmp = NULL;
	}
    }
#endif

    ged_close(gedp);
}

int
GshState::eval(int argc, const char **argv)
{
    // First, check for special case application-only commands.  These should
    // be rare and pertain exclusively to application level state - most
    // commands should be expressible in a GED context.

    // Clear the terminal screen's textual output
    if (UNLIKELY(BU_STR_EQUAL(argv[0], "clear"))) {
	linenoise::linenoiseClearScreen();
	return BRLCAD_OK;
    }
    // Exit the application
    if (UNLIKELY(BU_STR_EQUAL(argv[0], "quit") || BU_STR_EQUAL(argv[0], "q") || BU_STR_EQUAL(argv[0], "exit")))
	return GED_EXIT;


    // Not one of the application level cases - time to ask LIBGED
    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    printf("Command %s not found, did you mean %s (edit distance %d)?\n", argv[0], ccmd, edist);
	}
	return BRLCAD_ERROR;
    }

    // We've got a valid GED command Before any BRL-CAD logic is executed,
    // stash the state of the view info so we can recognized changes. */
#ifdef USE_DM
    view_checkpoint();
#endif

    int gret = ged_exec(gedp, argc, argv);

#ifdef USE_DM
    if (!(gret & BRLCAD_ERROR)) {
	view_update();
    }
#endif

    return gret;
}

void
GshState::listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d)
{
    ProcessIOHandler *nh = new ProcessIOHandler(fd, c, d);
    listeners[std::make_pair(p,t)] = nh;
}

#ifdef USE_DM
void
GshState::view_checkpoint()
{
    if (!gedp->ged_gvp->dmp)
	return;
    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    prev_dhash = dm_hash(dmp);
    prev_vhash = bv_hash(gedp->ged_gvp);
    prev_lhash = (new_cmd_forms) ? 0 : dl_name_hash(gedp);
    prev_ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
}

void
GshState::view_update()
{
    struct bview *v = gedp->ged_gvp;
    if (!v)
	return;

    struct dm *dmp = (struct dm *)v->dmp;
    if (!dmp)
	return;

    unsigned long long dhash = dm_hash(dmp);
    unsigned long long vhash = bv_hash(v);
    unsigned long long lhash = 0;
    if (new_cmd_forms && gedp->dbi_state) {
	unsigned long long updated = gedp->dbi_state->update();
	lhash = (updated) ? prev_lhash + 1 : 0;
	if (v->gv_s->gv_cleared) {
	    lhash = 1;
	    v->gv_s->gv_cleared = 0;
	}
    } else {
	lhash = dl_name_hash(gedp);
    }

    unsigned long long ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
    unsigned long long lhash_edit = lhash;
    if (dhash != prev_dhash) {
	dm_set_dirty(dmp, 1);
    }
    if (vhash != prev_vhash) {
	dm_set_dirty(dmp, 1);
    }
    if (lhash_edit != prev_lhash) {
	dm_set_dirty(dmp, 1);
    }
    if (ghash != prev_ghash) {
	dm_set_dirty(dmp, 1);
    }
    if (dm_get_dirty(dmp)) {
	if (new_cmd_forms) {
	    unsigned char *dm_bg1;
	    unsigned char *dm_bg2;
	    dm_get_bg(&dm_bg1, &dm_bg2, dmp);
	    dm_set_bg(dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);
	    dm_set_dirty(dmp, 0);
	    dm_draw_objs(v, NULL, NULL);
	    dm_draw_end(dmp);
	} else {
	    matp_t mat = gedp->ged_gvp->gv_model2view;
	    dm_loadmatrix(dmp, mat, 0);
	    unsigned char geometry_default_color[] = { 255, 0, 0 };
	    dm_draw_begin(dmp);
	    dm_draw_head_dl(dmp, gedp->ged_gdp->gd_headDisplay,
		    1.0, gedp->ged_gvp->gv_isize, -1, -1, -1, 1,
		    0, 0, geometry_default_color, 1, 0);

	    // Faceplate drawing
	    if (gedp->dbip) {
		struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
		v->gv_base2local = gedp->dbip->dbi_base2local;
		v->gv_local2base = gedp->dbip->dbi_local2base;
		dm_draw_viewobjs(wdbp, v, NULL);
	    } else {
		dm_draw_viewobjs(NULL, v, NULL);
	    }
	    dm_draw_end(dmp);
	}
    }
}
#endif


// The primary interaction thread managing the command line
// input from the user.  Must not be the main thread of the
// application, since linenoise I/O is blocking.
void
g_cmdline(
	std::shared_ptr<GshState> gs,
	std::shared_ptr<linenoise::linenoiseState> l,
	std::atomic<bool> &thread_done,
	std::mutex &print_mutex
	)
{
    // Reusable working containers for linenoise input processing
    struct bu_vls iline = BU_VLS_INIT_ZERO;
    std::vector<char *> tmp_av;

    while (true) {
	std::string line;
	// Blocking linenoise I/O
	auto quit = linenoise::Readline(l.get(), line);
	if (quit)
	    break;

	// Take what linenoise gives us and prepare it for LIBGED processing
	bu_vls_sprintf(&iline, "%s", line.c_str());
	bu_vls_trimspace(&iline);
	// If the line is empty or all whitespace, there's nothing to do.
	if (!bu_vls_strlen(&iline))
	    continue;
	linenoise::AddHistory(bu_vls_cstr(&iline));

	/* Make an argv array from the input line */
	char *input = bu_strdup(bu_vls_cstr(&iline));
	char **av = (char **)bu_calloc(strlen(input) + tmp_av.size() + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(input), input);

	/* If we are in the midst of a MORE command, we're incrementally
	 * building up an argv array */
	if (tmp_av.size()) {
	    for (int i = 0; i < ac; i++) {
		char *tstr = bu_strdup(av[i]);
		tmp_av.push_back(tstr);
	    }
	    // Reassemble the full command we have thus var
	    for (size_t i = 0; i < tmp_av.size(); i++) {
		av[i] = tmp_av[i];
	    }
	    ac = (int)tmp_av.size();
	}

	int gret = gs.get()->eval(ac, (const char **)av);

	/* If the eval tells us it's time to quite, clean up and break the loop */
	if (gret == GED_EXIT) {
	    /* Free the temporary argv structures */
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    break;
	}

	/* When we're interactive, the last line won't show in linenoise unless
	 * we have a '\n' character at the end.  We don't reliably get that
	 * from all commands, so check and add one if we need to in order to
	 * finish showing the command output.
	 *
	 * We don't do this inside eval in case a command execution is
	 * intentionally not appending that last character for some scripting
	 * purpose. It will more typically be a mistake, but would have to do
	 * some research to confirm there aren't any valid cases where this
	 * would be a bad idea. */
	std::string rstr(bu_vls_cstr(gs.get()->gedp->ged_result_str));

	/* Have copy - reset GED results string */
	bu_vls_trunc(gs.get()->gedp->ged_result_str, 0);

	// Check for GED_MORE mode 
	if (gret & GED_MORE) {
	    // If we're being asked for more input, the return string holds
	    // the prompt for the next input
	    l.get()->prompt = rstr;
	    if (!tmp_av.size()) {
		// The first time through, we store the argv contents here since we
		// didn't know to do it earlier.  Subsequent passes will handle the
		// tmp_av appending above.
		for (int i = 0; i < ac; i++) {
		    char *tstr = bu_strdup(av[i]);
		    tmp_av.push_back(tstr);
		}
	    }
	} else {
	    // Normal execution - just display the results.  Add a '\n' if not
	    // already present so linenoise will print all the lines before
	    // displaying the prompt
	    if (rstr.length() && rstr.c_str()[rstr.length() - 1] != '\n')
		rstr.append("\n");

	    // If we were doing a MORE command, normal execution signals the end of
	    // the argument accumulation.  Clear out the tmp_av so the next execution
	    // knows to proceed normally.
	    for (size_t i = 0; i < tmp_av.size(); i++) {
		delete tmp_av[i];
	    }
	    tmp_av.clear();

	    // In case we had a custom prompt from MORE, restore our standard prompt
	    l.get()->prompt = (bu_interactive()) ? std::string(DEFAULT_GSH_PROMPT) : std::string("");
	}

	print_mutex.lock();
	std::cout << rstr;
	print_mutex.unlock();

	/* Free the temporary argv structures */
	bu_free(input, "input copy");
	bu_free(av, "input argv");
    }

    // If we've broken out of the above loop, it means we're quitting the app -
    // use the shared atomic variable to notify the main loop.
    thread_done = true;
}

int
main(int argc, const char **argv)
{
    if (argc == 0 || !argv)
	return EXIT_FAILURE;

    /* Let libbu know where we are */
    bu_setprogname(argv[0]);

    /* Done with program name */
    argv++; argc--;

    /* Options */
    int print_help = 0;
    int new_cmd_forms = 0;
    struct bu_opt_desc d[3];
    BU_OPT(d[0],  "h",     "help",  "",  NULL, &print_help,        "print help and exit");
    BU_OPT(d[1],  "",  "new-cmds",  "",  NULL, &new_cmd_forms,     "use new (qged style) commands");
    BU_OPT_NULL(d[2]);

    /* Parse options, fail if anything goes wrong */
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    if ((argc = bu_opt_parse(&opt_msg, argc, argv, d)) == -1) {
	fputs(bu_vls_addr(&opt_msg), stderr);
	bu_vls_free(&opt_msg);
	bu_exit(EXIT_FAILURE, NULL);
    }
    bu_vls_free(&opt_msg);

    /* If we're just printing help, do that now */
    if (print_help) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	const char *help = bu_opt_describe(d, NULL);
	bu_vls_sprintf(&msg, "Usage: 'gsh [options] model.g'\n\nOptions:\n%s\n", help);
	bu_free((char *)help, "done with help string");
	fputs(bu_vls_addr(&msg), stderr);
	bu_vls_free(&msg);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* If we're using new (qged style) commands, set the relevant env variables.
     * Do this *before* we create the GshState instance. */
    if (new_cmd_forms) {
	bu_setenv("LIBGED_DBI_STATE", "1", 1);
	bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    }

    // Use a C++ class to manage info we will need
    std::shared_ptr<GshState> gs = std::make_shared<GshState>();

    // TODO - if we're non-interactive, just evaluate and exit
    // without getting into linenoise and threading.


    // Enable the multi-line mode
    //linenoise::SetMultiLine(true);

    gs.get()->l = std::make_shared<linenoise::linenoiseState>();
    gs.get()->l->Initialize(STDIN_FILENO, STDOUT_FILENO, LINENOISE_MAX_LINE, DEFAULT_GSH_PROMPT);

    // Launch blocking linenoise input gathering in a separate thread, to allow
    // us to integrate input from async commands into terminal output while
    // remaining interactive.
    std::thread g_cmdline_thread(
	    g_cmdline, gs, gs.get()->l,
	    std::ref(gs.get()->linenoise_done),
	    std::ref(gs.get()->print_mutex)
	    );

    // Give the linenoise thread a little time to set up - it's cleaner
    // when we enter the main loop if the prompt is already set.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // To simulate getting I/O from other sources and printing it while using
    // refreshLine to avoid messing up the user's input experience, we print to
    // stdout at periodic intervals.  Once we have proper listening callbacks
    // for rt and friends, their output should be handled like the output
    // below.
    int delay_cnt = 0;
    while (true) {
	delay_cnt++;
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	if (delay_cnt % 10 == 0) {
	    gs.get()->print_mutex.lock();
	    linenoiseWipeLine(gs.get()->l.get());
	    std::cout << "main loop: " << delay_cnt << "\n";
	    gs.get()->print_mutex.unlock();
	    refreshLine(gs.get()->l.get());
	}
	if (gs.get()->linenoise_done) {
	    gs.get()->print_mutex.lock();
	    std::cout << "\nall done\n";
	    gs.get()->print_mutex.unlock();
	    g_cmdline_thread.join();
	    break;
	}
    }

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

