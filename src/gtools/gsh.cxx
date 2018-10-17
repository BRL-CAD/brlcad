/*                           G S H . C X X
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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
/** @file gtools/bgsh.cxx
 *
 * This program (the BRL-CAD Geometry SHell) is a low level way to
 * interact with BRL-CAD geometry (.g) files.
 *
 */

#include "common.h"

#include "bio.h"
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "linenoise.h"
}

#include "bu.h"
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

    /* Let bu_brlcad_root and friends know where we are */
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
	    bu_vls_sprintf(&open_gfile, "%s", argv[0]);
	}
    }

    /* TODO - can probably fold geval into this - if we have more than 1 argc, do a geval
     * and exit, else go interactive */

    /* Start the interactive loop */
    while ((line = linenoise(gpmpt)) != NULL) {
	int (*func)(struct ged *, int, char *[]);

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

	/* OK, try a GED command - make an argv array from the input line */
	struct bu_vls ged_prefixed = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ged_prefixed, "ged_%s", bu_vls_addr(&iline));
	char *input = bu_strdup(bu_vls_addr(&ged_prefixed));
	bu_vls_free(&ged_prefixed);
	char **av = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(input), input);

	/* The "open" and close commands require a bit of
	 * awareness at this level, since the gedp pointer
	 * must respond to them. */
	if (BU_STR_EQUAL(av[0], "ged_open")) {
	    if (ac > 1) {
		if (gedp) ged_close(gedp);
		gedp = ged_open("db", av[1], 0);
		bu_vls_sprintf(&open_gfile, "%s", av[1]);
		printf("Opened file %s\n", bu_vls_addr(&open_gfile));
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


	if (BU_STR_EQUAL(av[0], "ged_close")) {
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
	*(void **)(&func) = bu_dlsym(libged, av[0]);
	if (!func) {
	    printf("unrecognzied command: %s\n", av[0]);
	} else {
	    (void)func(gedp, ac, av);
	    printf("%s\n", bu_vls_addr(gedp->ged_result_str));
	}

	bu_free(input, "input copy");
	bu_free(av, "input argv");
	bu_vls_trunc(&iline, 0);
    }

done:
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
