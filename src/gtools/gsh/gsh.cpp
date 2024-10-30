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


/* BRL-CAD's libged will launch commands that potentially need to be terminated
 * as subprocesses - that is the ONLY robust way we can stop an infinite
 * looping algorithm when the out-of-control code is not designed to respond to
 * application level instructions to stop.  However, that means we also need to
 * capture the output from those commands via IPC.
 *
 * We use the simplest method - reading from the child process stdout and
 * stderr to collect data - but because that process is blocking and of
 * unbounded duration we need to do the reading in separate threads.  Once it
 * is collected, we can make it available to the still running parent thread
 * for output.
 */
#define GSH_READ_SIZE 1024
void
p_watch(
	std::shared_ptr<std::string> obuf,
	std::atomic<bool> &thread_done,
	int fd,
	std::mutex &buf_mutex
       )
{
    // If we don't have a sane file descriptor, don't proceed
    if (fd < 0) {
	thread_done = true;
	return;
    }

    // Have an fd - read from it until we're done
    char buffer[GSH_READ_SIZE];
    ssize_t bcnt;
    std::string line;
    while ((bcnt = read(fd, buffer, GSH_READ_SIZE)) > 0) {
	line.append(buffer, bcnt);
	if (line[line.length() - 1] == '\n') {
	    buf_mutex.lock();
	    obuf.get()->append(line);
	    buf_mutex.unlock();
	    line.clear();
	}
    }
    if (line.length()) {
	buf_mutex.lock();
	obuf.get()->append(line);
	buf_mutex.unlock();
    }

    // Let the parent context know this thread has finished its work
    thread_done = true;
}

class ProcessIOHandler {
    public:
	ProcessIOHandler(int, ged_io_func_t, void *);
	~ProcessIOHandler();

	std::string read();
	std::atomic<bool> thread_done = false;

    private:
	int fd = -1;

	std::shared_ptr<std::string> curr_buf;
	std::mutex buf_mutex;
	std::thread watch_thread;
};

ProcessIOHandler::ProcessIOHandler(int f, ged_io_func_t, void *)
    : fd(f)
{
    // Make the buffer the thread will use to communicate output
    // back to us.
    curr_buf = std::make_shared<std::string>();

    // Set up the actual watching thread itself - this will be where the read
    // happens.
    watch_thread = std::thread(p_watch, curr_buf, std::ref(thread_done), fd, std::ref(buf_mutex));
}

ProcessIOHandler::~ProcessIOHandler()
{
    // We should only be trying to delete the process handler after the process
    // itself is either completed or terminated.
    watch_thread.join();
}

// Safely get the incremental output from the process watcher
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

class DisplayHash {
    public:
	bool hash(struct ged *, bool, bool);
	void dirty(struct ged *, const DisplayHash &);
	unsigned long long d = 0;
	unsigned long long v = 0;
	unsigned long long l = 0;
	unsigned long long g = 0;
};

bool
DisplayHash::hash(struct ged *gedp, bool dbi_state_check, bool new_cmd_forms)
{
    d = 0; v = 0; l = 0; g = 0;
    struct bview *bv = gedp->ged_gvp;
    if (!bv)
	return false;

    struct dm *dmp = (struct dm *)bv->dmp;
    if (!dmp)
	return false;

    d = dm_hash(dmp);
    v = bv_hash(bv);

    if (new_cmd_forms && gedp->dbi_state) {
	if (dbi_state_check) {
	    unsigned long long updated = gedp->dbi_state->update();
	    l = (updated) ? l + 1 : 0;
	    if (bv->gv_s->gv_cleared) {
		l = 1;
		bv->gv_s->gv_cleared = 0;
	    }
	} else {
	    l = 0;
	}
    } else {
	l = dl_name_hash(gedp);
    }

    g = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);

    return true;
}

void
DisplayHash::dirty(struct ged *gedp, const DisplayHash &o)
{
    struct bview *bv = gedp->ged_gvp;
    if (!bv)
	return;

    struct dm *dmp = (struct dm *)bv->dmp;
    if (!dmp)
	return;

    if (d != o.d) {
	dm_set_dirty(dmp, 1);
    }
    if (v != o.v) {
	dm_set_dirty(dmp, 1);
    }
    if (l != o.l) {
	dm_set_dirty(dmp, 1);
    }
    if (g != o.g) {
	dm_set_dirty(dmp, 1);
    }
}

/* The overall state of the gsh application is encapsulated by a state class
 * called GshState.  It defines the method for executing libged commands and
 * manages the linenoise interactive thread, as well as the necessary state
 * tracking for triggering view updating. */
class GshState {
    public:
	GshState();
	~GshState();

	// Run GED commands
	int eval(int argc, const char **argv);

	// Command line interactive prompt
	std::shared_ptr<linenoise::linenoiseState> l;
	std::atomic<bool> linenoise_done = false;
	std::atomic<bool> io_working = false;
	std::mutex print_mutex;

	// Create a listener for a subprocess
	void listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d);
	void disconnect(struct ged_subprocess *p, bu_process_io_t t);
	// Print subprocesses outputs (if any)
	void subprocess_output();
	size_t listeners_cnt();

	// Display management
	void view_checkpoint();
	void view_update();
	DisplayHash prev_hash;

	struct ged *gedp;
	std::string gfile;  // Mostly used to test the post_opendb callback
	bool new_cmd_forms = false;  // Set if we're testing QGED style commands
    private:
	// Active listeners
	std::map<std::pair<struct ged_subprocess *, bu_process_io_t>, ProcessIOHandler *> listeners;
	std::mutex listeners_lock;

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
}

extern "C" void
gsh_delete_io_handler(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p) return;

    GshState *gs = (GshState *)p->gedp->ged_io_data;

    gs->disconnect(p, t);

    // If we want some kind of signal here, look into
    // https://github.com/fr00b0/nod
    //gs->view_update_signal.emit(QG_VIEW_REFRESH);
}

void
Gsh_ClearScreen(struct ged *, void *d)
{
    GshState *gs = (GshState *)d;
    gs->l.get()->ClearScreen();
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

    view_checkpoint();

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

    // Tell libged how to clear the screen
    gedp->ged_screen_clear_callback = &Gsh_ClearScreen;
    gedp->ged_screen_clear_callback_udata = (void *)this;
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
    if (ged_cmd_valid(argv[0], NULL)) {
	const char *ccmd = NULL;
	int edist = ged_cmd_lookup(&ccmd, argv[0]);
	if (edist) {
	    printf("Command %s not found, did you mean %s (edit distance %d)?\n", argv[0], ccmd, edist);
	}
	return BRLCAD_ERROR;
    }

    // We've got a valid GED command Before any BRL-CAD logic is executed;
    // stash the state of the view info so we can recognize changes. */
    view_checkpoint();

    int gret = ged_exec(gedp, argc, argv);

    if (!(gret & BRLCAD_ERROR))
	view_update();

    return gret;
}

void
GshState::listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d)
{
    listeners_lock.lock();
    ProcessIOHandler *nh = new ProcessIOHandler(fd, c, d);
    listeners[std::make_pair(p,t)] = nh;
    listeners_lock.unlock();

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

void
GshState::disconnect(struct ged_subprocess *p, bu_process_io_t t)
{
    if (!p)
	return;

    listeners_lock.lock();
    std::map<std::pair<struct ged_subprocess *, bu_process_io_t>, ProcessIOHandler *>::iterator l_it;
    l_it = listeners.find(std::make_pair(p, t));
    if (l_it != listeners.end()) {
	ProcessIOHandler *eh = l_it->second;
	delete eh;
	listeners.erase(std::make_pair(p, t));
	switch (t) {
	    case BU_PROCESS_STDIN:
		p->stdin_active = 0;
		break;
	    case BU_PROCESS_STDOUT:
		p->stdout_active = 0;
		break;
	    case BU_PROCESS_STDERR:
		p->stderr_active = 0;
		break;
	}
    }
    listeners_lock.unlock();

    // If anything is still going on, we're not done yet.
    if (p->stdin_active || p->stdout_active || p->stderr_active)
	return;

    // All communication has ceased between the app and the subprocess,
    // time to call the end callback (if any)
    if (p->end_clbk)
	p->end_clbk(0, p->end_clbk_data);
}

void
GshState::view_checkpoint()
{
#ifdef USE_DM
    prev_hash.hash(gedp, false, new_cmd_forms);
#endif
}

void
GshState::subprocess_output()
{
    // We collect output from our handlers for reporting here, as well
    // as cleaning up any instances that are done.
    std::map<std::pair<struct ged_subprocess *, bu_process_io_t>, ProcessIOHandler *>::iterator l_it;

    // We won't want to refresh the command prompt line unless we actually have
    // some output to print
    bool prompt_cleared = false;

    // Check every active command.  Ordering is not guaranteed - the user
    // should avoid multiple long running commands in one gsh session if that
    // is a concern.
    listeners_lock.lock();
    std::set<std::pair<struct ged_subprocess *, bu_process_io_t>> stale;
    for (l_it = listeners.begin(); l_it != listeners.end(); ++l_it) {
	// Once we do the read, the thread buffer is cleared - we now
	// have the only copy, so it's on us to print it or lose it.
	std::string oline = l_it->second->read();
	if (oline.length()) {
	    // If we DO have something to print, we need to clear the
	    // existing command line string before we proceed.
	    print_mutex.lock();
	    if (!prompt_cleared) {
		l.get()->WipeLine();
		prompt_cleared = true;
	    }
	    std::cout << oline;
	    print_mutex.unlock();
	}
	if (l_it->second->thread_done)
	    stale.insert(l_it->first);
    }
    listeners_lock.unlock();

    // Clear any finished handlers - they are no longer needed
    std::set<std::pair<struct ged_subprocess *, bu_process_io_t>>::iterator s_it;
    for (s_it = stale.begin(); s_it != stale.end(); ++s_it)
	disconnect(s_it->first, s_it->second);

    // If we did clear the prompt, we MAY need to restore it.  If the linenoise
    // thread indicates it is doing its own processing (i.e. it is not
    // currently handling user input with a prmopt) we don't want to inject a
    // spurious prompt ourselves and mess up its output.  The linenoise thread
    // will take care of restoration when it resets after its own work is done.
    if (prompt_cleared && io_working) {
	print_mutex.lock();
	l.get()->RefreshLine();
	print_mutex.unlock();
    }
}

size_t
GshState::listeners_cnt()
{
    return listeners.size();
}

void
GshState::view_update()
{
#ifdef USE_DM
    DisplayHash hashes;
    if (!hashes.hash(gedp, true, new_cmd_forms))
	return;

    hashes.dirty(gedp, prev_hash);

    struct bview *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
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
#endif
}

// The primary interaction thread managing the command line
// input from the user.  Must not be the main thread of the
// application, since linenoise I/O is blocking.
void
g_cmdline(
	std::shared_ptr<GshState> gs,
	std::shared_ptr<linenoise::linenoiseState> l,
	std::atomic<bool> &thread_done,
	std::atomic<bool> &io_working,
	std::mutex &print_mutex
	)
{
    // Reusable working containers for linenoise input processing
    struct bu_vls iline = BU_VLS_INIT_ZERO;
    std::vector<char *> tmp_av;

    while (true) {
	std::string line;
	// Blocking linenoise I/O
	io_working = true;
	auto quit = l.get()->Readline(line);
	if (quit)
	    break;
	// While we're processing, we don't have an active
	// command prompt, so the main loop shouldn't be
	// restoring the prompt for us.
	io_working = false;

	// Take what linenoise gives us and prepare it for LIBGED processing
	bu_vls_sprintf(&iline, "%s", line.c_str());
	bu_vls_trimspace(&iline);
	// If the line is empty or all whitespace, there's nothing to do.
	if (!bu_vls_strlen(&iline))
	    continue;
	l.get()->AddHistory(bu_vls_cstr(&iline));

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
	    thread_done = true;
	    return;
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

    /* If anything went wrong during LIBGED initialization, let the user know */
    const char *ged_init_str = ged_init_msgs();
    if (strlen(ged_init_str)) {
	fprintf(stderr, "%s", ged_init_str);
    }

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    // Use a C++ class to manage info we will need
    std::shared_ptr<GshState> gs = std::make_shared<GshState>();

    // If we're non-interactive, just evaluate and exit without getting into
    // linenoise and threading.  First, see if we've got a viable .g file.
    if (argc && bu_file_exists(argv[0], NULL)) {
          int ac = 2;
          const char *av[3];
	  av[0] = "open";
	  av[1] = argv[0];
	  av[2] = NULL;
	  int ret = gs.get()->eval(ac, (const char **)av);
	  if (ret != BRLCAD_OK)
	      return EXIT_FAILURE;

	  /* If we reach this part of the code, argv[0] is a .g file and
	   * has been handled - skip ahead to the commands. */
	  argv++; argc--;
      }

    /* If we have been given more than a .g filename execute the provided argv
     * commands and exit. Deliberately making this case a very simple execution
     * path rather than having the linenoise interactive block deal with it, to
     * minimize the possibility of any unforeseen complications. */
    if (argc) {
	int ret = gs.get()->eval(argc, argv);
	std::string cmd_out(bu_vls_cstr(gs.get()->gedp->ged_result_str));
	std::cout << cmd_out;
	if (cmd_out[cmd_out.length()-1] != '\n')
	    std::cout << "\n";
	// Need to loop over subprocess listeners and print their
	// output until they finish.
	while (gs.get()->listeners_cnt()) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(200));
	    gs.get()->subprocess_output();
	}
	return (ret == BRLCAD_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // If we've gotten this far, we're in interactive mode and it is time to
    // start up linenoise to get user input.

    // Generally speaking we'll want to remember a lot of commands.

    gs.get()->l = std::make_shared<linenoise::linenoiseState>();
    gs.get()->l->SetHistoryMaxLen(INT_MAX);

    // TODO - should we enable multi-line mode?
    gs.get()->l->EnableMultiLine(true);

    // If we're handling commands from stdin, we want an empty prompt
    gs.get()->l->prompt = (bu_interactive()) ? std::string(DEFAULT_GSH_PROMPT) : std::string("");

    // Launch blocking linenoise input gathering in a separate thread, to allow
    // us to integrate input from async commands into terminal output while
    // remaining interactive.
    std::thread g_cmdline_thread(
	    g_cmdline, gs, gs.get()->l,
	    std::ref(gs.get()->linenoise_done),
	    std::ref(gs.get()->io_working),
	    std::ref(gs.get()->print_mutex)
	    );

    // Give the linenoise thread a little time to set up - it's cleaner
    // when we enter the main loop if the prompt is already set.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // From this point on the linenoise thread will drive user interaction.
    // The main thread's role is to periodically collect and output subprocess
    // outputs until the linenoise thread lets us know it's time to quit.
    while (true) {
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	gs.get()->subprocess_output();
	if (gs.get()->linenoise_done) {
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

