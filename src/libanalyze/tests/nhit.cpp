/*                      N H I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file nhit.cpp
 *
 * This program is a low level interface to Natalie's Interactive Ray-Tracer
 * within libanalyze.  Unlike the main NIRT command, this is a non-interactive
 * means of executing specific subsets of the NIRT stack for testing purposes.
 *
 */

#include "common.h"

#include <vector>
#include <set>
#include <string>
#include <iostream>
#include <limits>
#include <cstdio>
#include <cstring>
#include <fstream>

#include "bu/app.h"
#include "bu/opt.h"
#include "analyze.h"

#define IO_DATA_NULL { NULL, NULL }

struct nirt_io_data {
    struct bu_vls *out;
    struct bu_vls *err;
};

static int
nirt_stdout_hook(struct nirt_state *ns, void *u_data)
{
    struct nirt_io_data *io_data = (struct nirt_io_data *)u_data;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_OUT);
    if (bu_vls_strlen(&out)) {
	bu_vls_sprintf(io_data->out, "%s", bu_vls_cstr(&out));
    }
    bu_vls_free(&out);
    return 0;
}

static int
nirt_stderr_hook(struct nirt_state *ns, void *u_data)
{
    struct nirt_io_data *io_data = (struct nirt_io_data *)u_data;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_ERR);
    bu_vls_printf(io_data->err, "%s", bu_vls_cstr(&out));
    bu_vls_free(&out);
    return 0;
}

int
main(int argc, const char **argv)
{
    struct nirt_state *ns = NULL;
    struct nirt_io_data io_data = IO_DATA_NULL;


    struct db_i *dbip;
    struct bu_vls ncmd = BU_VLS_INIT_ZERO;

    int ret = EXIT_FAILURE;
    int backout = 0;
    int print_help = 0;
    int should_miss= 0;
    int use_air = 0;
    fastf_t los = -1;

    bu_setprogname(argv[0]);

    struct bu_opt_desc d[7] = {BU_OPT_DESC_NULL};

    BU_OPT(d[0],  "?", "help",     "",       NULL,             &print_help,   "print help and exit");
    BU_OPT(d[1],  "h", "help",     "",       NULL,             &print_help,   "print help and exit");
    BU_OPT(d[2],  "b", "backout",  "",       NULL,             &backout,      "back out of geometry before shot");
    BU_OPT(d[3],  "M", "miss",     "",       NULL, 	       &should_miss,  "expected result is a miss");
    BU_OPT(d[4],  "u", "use-air",  "n",      &bu_opt_int,      &use_air,      "set use_air=n (default 0)");
    BU_OPT(d[5],  "L", "los",      "n",      &bu_opt_fastf_t,  &los,          "expected Line-of-Sight distance");
    BU_OPT_NULL(d[6]);

    if (argc == 0 || !argv)
	return -1;

    /* Let libbu know where we are */
    bu_setprogname(argv[0]);

    argv++; argc--;

    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ac = bu_opt_parse(&msg, argc, (const char **)argv, d);
    if (ac < 0) {
	bu_exit(EXIT_FAILURE, "ERROR: option parsing failed\n%s", bu_vls_addr(&msg));
    }
    bu_vls_free(&msg);

    BU_GET(io_data.out, struct bu_vls);
    BU_GET(io_data.err, struct bu_vls);
    bu_vls_init(io_data.out);
    bu_vls_init(io_data.err);

    /* If we've been asked to print help or don't know what to do, print help
     * and exit */
    if (print_help || argc < 7) {
	char *help = bu_opt_describe(d, NULL);
	ret = (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
	bu_log("Usage: 'nhit [options] model.g obj X Y Z AZ EL'\n\nOptions:\n%s\n", help);
	if (help)
	    bu_free(help, "help str");
	goto done;
    }

    /* OK, from here on out we are actually going to be working with NIRT
     * itself.  Set up the initial environment */
    if (rt_uniresource.re_magic == 0)
	rt_init_resource(&rt_uniresource, 0, NULL);

    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Unable to open db file %s\n", argv[0]);
	ret = EXIT_FAILURE;
	goto done;
    }

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_log("db_dirbuild failed: %s\n", argv[0]);
	goto done;
    }

    BU_GET(ns, struct nirt_state);
    if (nirt_init(ns) == -1) {
	BU_PUT(ns, struct nirt_state);
	bu_log("nirt state initialization failed\n");
	ret = EXIT_FAILURE;
	goto done;
    }

    /* Set up hooks so we can capture I/O from nirt_exec */
    (void)nirt_udata(ns, (void *)&io_data);
    nirt_hook(ns, &nirt_stdout_hook, NIRT_OUT);
    nirt_hook(ns, &nirt_stderr_hook, NIRT_ERR);

    /* Use silent mode */
    bu_vls_sprintf(&ncmd, "state silent_mode 1");
    (void)nirt_exec(ns, bu_vls_addr(&ncmd));

    if (backout) {
	(void)nirt_exec(ns, "backout 1");
    }

    if (use_air) {
	bu_vls_sprintf(&ncmd, "useair %d", use_air);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    bu_vls_sprintf(&ncmd, "draw %s", argv[1]);
    (void)nirt_exec(ns, bu_vls_addr(&ncmd));

    /* We know enough now to initialize */
    if (nirt_init_dbip(ns, dbip) == -1) {
	BU_PUT(ns, struct nirt_state);
	bu_log("nirt_init_dbip failed: %s\n", argv[0]);
	ret = EXIT_FAILURE;
	goto done;
    }
    db_close(dbip); /* nirt will now manage its own copies of the dbip */

    /* Set up the formats to return just what we care about */
    bu_vls_sprintf(&ncmd, "fmt r \"\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt h \"\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt p \"hit\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt o \"ovlp\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt f \"\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt m \"miss\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    bu_vls_sprintf(&ncmd, "fmt g \"\"");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;

    // IFF we're expecting a particular length back per the -L option,
    // set up to print it
    if (los > 0) {
	bu_vls_sprintf(&ncmd, "fmt p \"%%.17f\" los");
	if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;
    }

    // Set up and take the shot
    bu_vls_sprintf(&ncmd, "xyz %s %s %s", argv[2], argv[3], argv[4]);
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;

    bu_vls_sprintf(&ncmd, "ae %s %s", argv[5], argv[6]);
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;

    bu_vls_sprintf(&ncmd, "s");
    if (nirt_exec(ns, bu_vls_addr(&ncmd))) goto done;


    // See if what we got back matches what we expected
    if (should_miss) {
	if (!BU_STR_EQUAL(bu_vls_cstr(io_data.out), "miss")) {
	    ret = 1;
	    if (bu_vls_strlen(io_data.out)) {
		bu_log("Error: expected miss, but got \"%s\"!\n", bu_vls_cstr(io_data.out));
	    } else {
		bu_log("Error: expected miss!\n");
	    }
	} else {
	    ret = 0;
	}
	goto done;
    }

    if (los > 0) {
	double los_result;
	char *endptr = NULL;
	errno = 0;
	los_result = strtod(bu_vls_cstr(io_data.out), &endptr);
	if ((endptr != NULL && strlen(endptr) > 0) || errno == ERANGE) {
	    ret = 1;
	    if (bu_vls_strlen(io_data.out)) {
		bu_log("Error: invalid los result: \"%s\"!\n", bu_vls_cstr(io_data.out));
	    } else {
		bu_log("Error: empty los result!\n");
	    }
	    goto done;
	}

	if (los_result > 0 && los_result > los - BN_TOL_DIST && los_result < los + BN_TOL_DIST) {
	    ret = 0;
	    goto done;
	} else {
	    ret = 1;
	    bu_log("Error: unexpected los value: \"%.2f\" instead of \"%.2f\"!\n", los_result, los);
	    goto done;
	}

    }

    if (!BU_STR_EQUAL(bu_vls_cstr(io_data.out), "hit")) {
	ret = 1;
	if (bu_vls_strlen(io_data.out)) {
	    bu_log("Error: expected hit, but got \"%s\"!\n", bu_vls_cstr(io_data.out));
	} else {
	    bu_log("Error: expected hit!\n");
	}
	goto done;
    } else {
	ret = 0;
	goto done;
    }

done:
    bu_vls_free(&ncmd);

    bu_vls_free(io_data.out);
    bu_vls_free(io_data.err);
    BU_PUT(io_data.out, struct bu_vls);
    BU_PUT(io_data.err, struct bu_vls);
    nirt_destroy(ns);
    if (ns)
	BU_PUT(ns, struct nirt_state);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
