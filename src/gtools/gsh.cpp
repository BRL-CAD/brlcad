/*                           G S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file gtools/gsh.cpp
 *
 * This program (the BRL-CAD Geometry SHell) is a low level way to
 * interact with BRL-CAD geometry (.g) files.
 *
 * TODO - right now this program can't handle async output from commands like
 * rtcheck.  We need something similar to (but more cross platform than)
 * https://github.com/antirez/linenoise/pull/95/
 *
 * In Qt we're using signals and slots to make this work with a single text
 * widget, which is similar in some ways to what needs to happen here.
 * (Indeed, it's most likely essential - in Qt we could use multiple text
 * widgets, but that's not really an option for gsh.)  We don't want to pull in
 * Qt as a gsh dependency... could we make use of https://github.com/fr00b0/nod
 * here?  Perhaps trying timed I/O reading from a thread and then sending back
 * any results to linenoise via signals, and then have linenoise do something
 * similar to the 95 logic to get it on the screen?  nod (and a number of other
 * libs) do stand-alone signals and slots, but so far I've not found a
 * cross-platform wrapper equivalent to QSocketNotifier...
 */

#include "common.h"

#include "bio.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "linenoise.h"
}

#include "bu.h"
#include "bv.h"
#include "dm.h"
#include "ged.h"

#define DEFAULT_GSH_PROMPT "g> "

int
main(int argc, const char **argv)
{
    struct ged *gedp = NULL;
    void *libged = bu_dlopen(NULL, BU_RTLD_LAZY);
    int ret = EXIT_SUCCESS;
    char *line = NULL;
    int print_help = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_vls iline= BU_VLS_INIT_ZERO;
    struct bu_vls open_gfile = BU_VLS_INIT_ZERO;
    const char *gpmpt = DEFAULT_GSH_PROMPT;
    struct bu_opt_desc d[2];
    BU_OPT(d[0],  "h", "help", "",       NULL,              &print_help,     "print help and exit");
    BU_OPT_NULL(d[1]);

    if (argc == 0 || !argv) return -1;

    /* Let libbu know where we are */
    bu_setprogname(argv[0]);

    /* Done with program name */
    argv++; argc--;

    /* Parse options, fail if anything goes wrong */
    if ((argc = bu_opt_parse(&msg, argc, argv, d)) == -1) {
	fputs(bu_vls_addr(&msg), stderr);
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, NULL);
    }
    bu_vls_trunc(&msg, 0);

    if (print_help) {
	const char *help = bu_opt_describe(d, NULL);
	bu_vls_sprintf(&msg, "Usage: 'gsh [options] model.g'\n\nOptions:\n%s\n", help);
	bu_free((char *)help, "done with help string");
	fputs(bu_vls_addr(&msg), stderr);
	bu_vls_free(&msg);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* If we can't load libged there's not point in continuing */
    if (!libged) {
	bu_vls_free(&msg);
	bu_exit(EXIT_FAILURE, "ERROR, could not load libged: %s\n", bu_dlerror());
    }

    const char *ged_init_str = ged_init_msgs();
    if (strlen(ged_init_str)) {
	fprintf(stderr, "%s", ged_init_str);
    }

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    /* Need a view for commands that expect a view */
    struct bview *gsh_view;
    BU_GET(gsh_view, struct bview);
    bv_init(gsh_view);

    /* See if we've been told to pre-load a specific .g file. */
    if (argc) {
	if (!bu_file_exists(argv[0], NULL)) {
	    bu_vls_free(&msg);
	    bu_dlclose(libged);
	    bu_exit(EXIT_FAILURE, "File %s does not exist, expecting .g file\n", argv[0]) ;
	}
	gedp = ged_open("db", argv[0], 1);
	if (!gedp) {
	    bu_vls_free(&msg);
	    bu_dlclose(libged);
	    bu_exit(EXIT_FAILURE, "Could not open %s as a .g file\n", argv[0]) ;
	} else {
	    gedp->ged_gvp = gsh_view;
	    bu_vls_sprintf(&open_gfile, "%s", argv[0]);
	}
    }

    /* Do the old geval bit - if we have more than 1 argc, eval and exit. */
    if (argc > 1) {
	int (*func)(struct ged *, int, char *[]);
	char gedfunc[MAXPATHLEN] = {0};

	/* If we got this far, argv[0] was a .g file */
	argv++; argc--;
	bu_strlcat(gedfunc, "ged_", MAXPATHLEN);
	bu_strlcat(gedfunc, argv[0], MAXPATHLEN - strlen(gedfunc));
	if (strlen(gedfunc) < 1 || gedfunc[0] != 'g') {
	    bu_vls_free(&msg);
	    bu_dlclose(libged);
	    bu_exit(EXIT_FAILURE, "Couldn't get GED command name from [%s]\n", argv[0]);
	}

	*(void **)(&func) = bu_dlsym(libged, gedfunc);
	if (!func) {
	    bu_exit(EXIT_FAILURE, "Unrecognized command [%s]\n", argv[0]);
	}
	/* TODO - is it ever unsafe to do this cast?  Does ged mess with argv somehow? */
	ret = func(gedp, argc, (char **)argv);

	bu_dlclose(libged);

	fprintf(stdout, "%s", bu_vls_addr(gedp->ged_result_str));

	ged_close(gedp);

	BU_PUT(gedp, struct ged);

	return ret;
    }


    /*
     * TODO - also add non-tty mode - could make gsh a 'generic' subprocess
     * execution mechanism for libged commands that want to do subprocess but
     * don't have their own (1) executable.  The simplicity of gsh's bare bones
     * argc/argv processing would be an asset in that scenario.
     *
     * Note that for such a scheme to work, we would also have to figure out
     * how to have MGED et. al. deal with async return of command results
     * when it comes to things like executing Tcl scripts on results - the
     * full MGED Tcl prompt acts on command results as part of Tcl scripts,
     * and if the output is delayed the script execution (anything from
     * assigning search results to a variable on up) must also be delayed
     * and know to act on the 'final' result rather than the return from
     * the 'kicking off the process' initialization.
     *
     * Talk a look at what rt_read_cmd is doing - use that to guide an
     * "arg reassembly" function to avoid a super-long argv entry filling
     * up a pipe.
     * */

    /* Start the interactive loop */
    unsigned long long prev_dhash = 0;
    unsigned long long prev_vhash = 0;
    unsigned long long prev_lhash = 0;
    unsigned long long prev_ghash = 0;

    while ((line = linenoise(gpmpt)) != NULL) {

	bu_vls_sprintf(&iline, "%s", line);
	free(line);
	bu_vls_trimspace(&iline);
	if (!bu_vls_strlen(&iline)) continue;
	linenoiseHistoryAdd(bu_vls_addr(&iline));

	/* The "clear" command is only for the shell, not
	 * for libged */
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	/* The "quit" command is also not for libged */
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "q")) goto done;
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "quit")) goto done;
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "exit")) goto done;

	/* Looks like we'll be running a GED command - stash the state
	 * of the view info */
	if (gedp->ged_dmp) {
	    prev_dhash = (gedp->ged_dmp) ? dm_hash((struct dm *)gedp->ged_dmp) : 0;
	    prev_vhash = bv_hash(gedp->ged_gvp);
	    prev_lhash = dl_name_hash(gedp);
	    prev_ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
	}

	/* OK, try a GED command - make an argv array from the input line */
	struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
	//bu_vls_sprintf(&ged_prefixed, "ged_%s", bu_vls_addr(&iline));
	bu_vls_sprintf(&ged_prefixed, "%s", bu_vls_addr(&iline));
	char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
	bu_vls_free(&ged_prefixed);
	char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(input), input);

	/* The "open" and close commands require a bit of
	 * awareness at this level, since the gedp pointer
	 * must respond to them. */
	if (BU_STR_EQUAL(av[0], "open")) {
	    if (ac > 1) {
		if (gedp) ged_close(gedp);
		gedp = ged_open("db", av[1], 0);
		if (!gedp) {
		    bu_vls_free(&msg);
		    bu_dlclose(libged);
		    bu_exit(EXIT_FAILURE, "Could not open %s as a .g file\n", av[1]) ;
		} else {
		    gedp->ged_gvp = gsh_view;
		    bu_vls_sprintf(&open_gfile, "%s", av[1]);
		    printf("Opened file %s\n", bu_vls_addr(&open_gfile));
		}
	    } else {
		printf("Error: invalid ged_open call\n");
	    }
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	if (!gedp) {
	    printf("Error: no database is currently open.\n");
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&iline, 0);
	    continue;
	}


	if (BU_STR_EQUAL(av[0], "close")) {
	    ged_close(gedp);
	    gedp = NULL;
	    printf("closed database %s\n", bu_vls_addr(&open_gfile));
	    bu_vls_trunc(&open_gfile, 0);
	    bu_free(input, "input copy");
	    bu_free(av, "input argv");
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	/* If we're not opening or closing, and we have an active gedp,
	 * make a standard libged call */
	if (ged_cmd_valid(av[0], NULL)) {
	    const char *ccmd = NULL;
	    int edist = ged_cmd_lookup(&ccmd, av[0]);
	    if (edist) {
		printf("Command %s not found, did you mean %s (edit distance %d)?\n", av[0], ccmd, edist);
	    }
	} else {
	    ged_exec(gedp, ac, (const char **)av);
	    printf("%s\n", bu_vls_cstr(gedp->ged_result_str));
	    bu_vls_trunc(gedp->ged_result_str, 0);

	    // The command ran, see if the display needs updating
	    if (gedp->ged_dmp) {
		struct dm *dmp = (struct dm *)gedp->ged_dmp;
		struct bview *v = gedp->ged_gvp;
		unsigned long long dhash = dm_hash(dmp);
		unsigned long long vhash = bv_hash(gedp->ged_gvp);
		unsigned long long lhash = dl_name_hash(gedp);
		unsigned long long ghash = ged_dl_hash((struct display_list *)gedp->ged_gdp->gd_headDisplay);
		unsigned long long lhash_edit = lhash;
		lhash_edit += dl_update(gedp);
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
		    matp_t mat = gedp->ged_gvp->gv_model2view;
		    dm_loadmatrix(dmp, mat, 0);
		    unsigned char geometry_default_color[] = { 255, 0, 0 };
		    dm_draw_begin(dmp);
		    dm_draw_display_list(dmp, gedp->ged_gdp->gd_headDisplay,
			    1.0, gedp->ged_gvp->gv_isize, -1, -1, -1, 1,
			    0, 0, geometry_default_color, 1, 0);

		    // Faceplate drawing
		    dm_draw_viewobjs(gedp->ged_wdbp, v, NULL, gedp->ged_wdbp->dbip->dbi_base2local, gedp->ged_wdbp->dbip->dbi_local2base);

		    dm_draw_end(dmp);
		}
	    }

	}
#if 0
	int (*func)(struct ged *, int, char *[]);
	*(void **)(&func) = bu_dlsym(libged, av[0]);
	if (!func) {
	    printf("unrecognzied command: %s\n", av[0]);
	} else {
	    (void)func(gedp, ac, av);
	    printf("%s\n", bu_vls_addr(gedp->ged_result_str));
	}
#endif

	bu_free(input, "input copy");
	bu_free(av, "input argv");
	bu_vls_trunc(&iline, 0);
    }

done:
    BU_PUT(gsh_view, struct bv);
    bu_dlclose(libged);
    if (gedp) ged_close(gedp);
    linenoiseHistoryFree();
    bu_vls_free(&msg);
    bu_vls_free(&open_gfile);
    bu_vls_free(&iline);
    return ret;
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
