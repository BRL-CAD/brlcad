/*                      M A I N . C X X
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
/** @file gtools/inirt/main.cxx
 *
 * This program is Natalie's Interactive Ray-Tracer
 *
 */

#include "common.h"

#include <vector>
#include <set>
#include <string>
#include <iostream>
#include <fstream>

extern "C" {
#include "linenoise.h"
#include "brlcad_ident.h"
#include "bu/env.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/units.h"
#include "analyze.h"
}

#define SILENT_UNSET    0
#define SILENT_YES      1
#define SILENT_NO       -1

/** FLAG VALUES FOR overlap_claims */
#define OVLP_RESOLVE            0
#define OVLP_REBUILD_FASTGEN    1
#define OVLP_REBUILD_ALL        2
#define OVLP_RETAIN             3

size_t
nirt_list_formats(char ***names)
{
    size_t files, i;
    char **filearray = NULL;
    char suffix[6]="*.nrt";
    FILE *f = NULL;
    int fnddesc;

    struct bu_vls nfp = BU_VLS_INIT_ZERO;
    struct bu_vls ptf = BU_VLS_INIT_ZERO;
    struct bu_vls fl = BU_VLS_INIT_ZERO;

    /* get a nirt directory listing */
    bu_vls_printf(&nfp, "%s", bu_brlcad_data("nirt", 0));
    files = bu_file_list(bu_vls_addr(&nfp), suffix, &filearray);
    if (names) *names = filearray;

    /* open every nirt file we find, extract, and print the description */
    for (i = 0; i < files && !names; i++) {
	bu_vls_trunc(&ptf, 0);
	bu_vls_printf(&ptf, "%s/%s", bu_vls_addr(&nfp), filearray[i]);
	f = fopen(bu_vls_addr(&ptf), "rb");

	fnddesc = 0;
	bu_vls_trunc(&fl, 0);
	while (bu_vls_gets(&fl, f) && fnddesc == 0) {
	    if (bu_strncmp(bu_vls_addr(&fl), "# Description: ", 15) == 0) {
		fnddesc = 1;
		bu_log("%s\n", bu_vls_addr(&fl)+15);
	    }
	    bu_vls_trunc(&fl, 0);
	}
	fclose(f);
    }

    /* release resources */
    if (!names)
	bu_argv_free(files, filearray);
    bu_vls_free(&fl);
    bu_vls_free(&nfp);
    bu_vls_free(&ptf);

    return files;
}

extern "C" int
dequeue_scripts(struct bu_vls *UNUSED(msg), int UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    std::vector<std::string> *init_scripts = (std::vector<std::string> *)set_var;
    init_scripts->clear();
    return 0;
}

extern "C" int
enqueue_script(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    std::vector<std::string> *init_scripts = (std::vector<std::string> *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script enqueue");
    init_scripts->push_back(argv[0]);
    return 1;
}

extern "C" int
enqueue_attrs(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    std::set<std::string> *attrs = (std::set<std::string> *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt attr enqueue");
    attrs->insert(argv[0]);
    return 1;
}


struct script_file_data {
    std::vector<std::string> *init_scripts;
    struct bu_vls *filename;
    int file_cnt;
};

extern "C" int
enqueue_file(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    std::string s;
    std::ifstream file;
    struct script_file_data *sfd = (struct script_file_data *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script file");

    file.open(argv[0]);
    if (!file.is_open()) {
	struct bu_vls str;
	bu_vls_printf(&str, "%s/%s.nrt", bu_brlcad_data("nirt", 0), argv[0]);
	file.open(bu_vls_addr(&str));
	bu_vls_free(&str);
	if (!file.is_open()) return -1;
	while (std::getline(file, s)) {
	    sfd->init_scripts->push_back(s);
	}
    } else {
	while (std::getline(file, s)) {
	    sfd->init_scripts->push_back(s);
	}
    }

    bu_vls_sprintf(sfd->filename, "%s", argv[0]);
    sfd->file_cnt++;

    return 1;
}

extern "C" int
decode_overlap(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    int *oval = (int *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt overlap handle");
    if (BU_STR_EQUAL(argv[0], "resolve") || BU_STR_EQUAL(argv[0], "0")) {
	(*oval) = OVLP_RESOLVE;
    } else if (BU_STR_EQUAL(argv[0], "rebuild_fastgen") || BU_STR_EQUAL(argv[0], "1")) {
	(*oval) = OVLP_REBUILD_FASTGEN;
    } else if (BU_STR_EQUAL(argv[0], "rebuild_all") || BU_STR_EQUAL(argv[0], "2")) {
	(*oval) = OVLP_REBUILD_ALL;
    } else if (BU_STR_EQUAL(argv[0], "retain") || BU_STR_EQUAL(argv[0], "3")) {
	(*oval) = OVLP_RETAIN;
    } else {
	bu_log("Illegal overlap_claims specification: '%s'\n", argv[0]);
	return -1;
    }
    return 1;
}

int
nirt_stdout_hook(NIRT *ns, void *UNUSED(u_data))
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_OUT);
    printf("%s", bu_vls_addr(&out));
    fflush(stdout);
    bu_vls_free(&out);
    return 0;
}

int
nirt_stderr_hook(NIRT *ns, void *UNUSED(u_data))
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_ERR);
    bu_log("%s", bu_vls_addr(&out));
    bu_vls_free(&out);
    return 0;
}

int
main(int argc, const char **argv)
{
    std::vector<std::string> init_scripts;
    struct bu_vls last_script_file = BU_VLS_INIT_ZERO;
    struct script_file_data sfd = {&init_scripts, &last_script_file, 0};
    char *line;
    int ac = 0;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    std::set<std::string> attrs;
    std::set<std::string>::iterator a_it;
    struct bu_vls iline = BU_VLS_INIT_ZERO;
    struct bu_vls nirt_debug = BU_VLS_INIT_ZERO;
    int overlap_claims = OVLP_RESOLVE;
    int backout = 0;
    int bot_mintie = 0;
    int header_mode = 1;
    int list_formats = 0;
    int minpieces = -1;
    int print_help = 0;
    int read_matrix = 0;
    int silent_mode = SILENT_UNSET;
    int use_air = 0;
    int verbose_mode = 0;
    /* These bu_opt_desc_opts settings approximate the old NIRT help formatting */
    struct bu_opt_desc_opts dopts = { BU_OPT_ASCII, 1, 11, 67, NULL, NULL, NULL, 1, NULL, NULL };
    struct bu_opt_desc d[18];
    BU_OPT(d[0],  "h", "help", "",       &bu_opt_bool,     &print_help,     "print help and exit");
    BU_OPT(d[1],  "A", "",     "n",      &enqueue_attrs,   &attrs,          "add attribute_name=n"); /* TODO - support reading a list of attributes? */
    BU_OPT(d[2],  "M", "",     "",       &bu_opt_bool,     &read_matrix,    "read matrix, cmds on stdin");
    BU_OPT(d[3],  "b", "",     "",       &bu_opt_bool,     &backout,        "back out of geometry before first shot");
    BU_OPT(d[4],  "B", "",     "n",      &bu_opt_int,      &minpieces,      "set rt_bot_minpieces=n");
    BU_OPT(d[5],  "T", "",     "n",      &bu_opt_int,      &bot_mintie,     "set rt_bot_mintie=n (deprecated, use LIBRT_BOT_MINTIE instead)");
    BU_OPT(d[6],  "e", "",     "script", &enqueue_script,  &init_scripts,   "run script before interacting");
    BU_OPT(d[7],  "f", "",     "sfile",  &enqueue_file,    &sfd,            "run script sfile before interacting");
    BU_OPT(d[8],  "E", "",     "",       &dequeue_scripts, &init_scripts,   "ignore any -e or -f options specified earlier on the command line");
    BU_OPT(d[9],  "L", "",     "",       &bu_opt_bool,     &list_formats,   "list output formatting options");
    BU_OPT(d[10], "s", "",     "",       &bu_opt_bool,     &silent_mode,    "run in silent (non-verbose) mode");
    BU_OPT(d[11], "v", "",     "",       &bu_opt_bool,     &verbose_mode,   "run in verbose mode");
    BU_OPT(d[12], "H", "",     "n",      &bu_opt_int,      &header_mode,    "flag (n) for enable/disable informational header - (n=1 [on] by default, always off in silent mode)");
    BU_OPT(d[13], "u", "",     "n",      &bu_opt_int,      &use_air,        "set use_air=n (default 0)");
    BU_OPT(d[14], "O", "",     "action", &decode_overlap,  &overlap_claims, "handle overlap claims via action");
    BU_OPT(d[15], "x", "",     "v",      NULL,    &RTG.debug,      "set librt(3) diagnostic flag=v");
    BU_OPT(d[16], "X", "",     "v",      &bu_opt_vls,      &nirt_debug,     "set nirt diagnostic flag=v");
    BU_OPT_NULL(d[17]);

    if (argc == 0 || !argv) return -1;

    argv++; argc--;
    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
       	bu_exit(EXIT_FAILURE, bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    /* If we've been asked to print help or don't know what to do, print help
     * and exit */
    if (print_help || argc < 2 || (silent_mode == SILENT_YES && verbose_mode)) {
	int ret = (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
	const char *help = bu_opt_describe(d, &dopts);
	bu_log("Usage: 'nirt [options] model.g objects...'\nOptions:\n%s\n", help);
	if (help) bu_free((char *)help, "help str");
	bu_exit(ret, NULL);
    }

    if (list_formats) {
	/* Print available header formats and exit */
	bu_log("Formats available:\n");
	nirt_list_formats(NULL);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* Check if we're on a terminal or not - it has implications for the modes */
    if (silent_mode == SILENT_UNSET) {
	silent_mode = (isatty(0)) ? SILENT_NO : SILENT_YES;
    }

    if (silent_mode != SILENT_YES && header_mode) {
	(void)fputs(brlcad_ident("Natalie's Interactive Ray Tracer"), stdout);
    }

    /* let users know that other output styles are available */
    if (silent_mode != SILENT_YES) {
	printf("Output format:");
	if (sfd.file_cnt == 0) {
	    printf(" default");
	} else {
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_path_component(&fname, bu_vls_addr(sfd.filename), BU_PATH_BASENAME_EXTLESS);
	    printf(" %s", bu_vls_addr(&fname));
	    bu_vls_free(&fname);
	}
	printf(" (specify -L option for descriptive listing)\n");

	{
	    size_t fmtcnt = 0;
	    char **names = NULL;
	    if ((fmtcnt = nirt_list_formats(&names)) > 0) {
		size_t i = 0;
		printf("Formats available:");
		do {
		    char *dot = strchr(names[i], '.');
		    /* trim off any filename suffix */
		    if (dot) *dot = '\0';
		    printf(" %s", names[i]);
		} while (++i < fmtcnt);

		printf(" (specify via -f option)\n");
	    }
	    bu_argv_free(fmtcnt, names);
	}
    }

    /* OK, from here on out we are actually going to be working with NIRT
     * itself.  Set up the initial environment */
    struct db_i *dbip;
    NIRT *ns;
    if (rt_uniresource.re_magic == 0) rt_init_resource(&rt_uniresource, 0, NULL);

    if (silent_mode != SILENT_YES) printf("Database file:  '%s'\n", argv[0]);
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(EXIT_FAILURE, "Unable to open db file %s\n", argv[0]);
    }
    RT_CK_DBI(dbip);

    if (silent_mode != SILENT_YES) printf("Building the directory...\n");
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(EXIT_FAILURE, "db_dirbuild failed: %s\n", argv[0]);
    }

    if (nirt_alloc(&ns) == -1) {
	bu_exit(EXIT_FAILURE, "nirt allocation failed\n");
    }

    /* Set up hooks so we can capture I/O from nirt_exec */
    nirt_hook(ns, &nirt_stdout_hook, NIRT_OUT);
    nirt_hook(ns, &nirt_stderr_hook, NIRT_ERR);

    /* If any of the options require state setup, run the appropriate commands
     * to put the NIRT environment in the correct state. */
    if (bot_mintie) {
	struct bu_vls envs = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&envs, "%d", minpieces);
	bu_setenv("LIBRT_BOT_MINTIE", bu_vls_addr(&envs), 1);
	bu_vls_free(&envs);
    }

    if (minpieces >= 0) {
	struct bu_vls ncmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncmd, "bot_minpieces %d", minpieces);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
	bu_vls_free(&ncmd);
    }

    if (backout) {
	(void)nirt_exec(ns, "backout 1");
    }

    if (use_air) {
	struct bu_vls ncmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncmd, "useair %d", use_air);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
	bu_vls_free(&ncmd);
    }

    if (overlap_claims) {
	switch (overlap_claims) {
	    case OVLP_RESOLVE:
		(void)nirt_exec(ns, "overlap_claims resolve");
		break;
	    case OVLP_REBUILD_FASTGEN:
		(void)nirt_exec(ns, "overlap_claims rebuild_fastgen");
		break;
	    case OVLP_REBUILD_ALL:
		(void)nirt_exec(ns, "overlap_claims rebuild_all");
		break;
	    case OVLP_RETAIN:
		(void)nirt_exec(ns, "overlap_claims retain");
		break;
	    default:
		bu_exit(EXIT_FAILURE, "Unknown overlap_claims value: %d\n", overlap_claims);
		break;
	}
    }

    if (bu_vls_strlen(&nirt_debug) > 0) {
	struct bu_vls ncmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncmd, "debug %s", bu_vls_addr(&nirt_debug));
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
	bu_vls_free(&ncmd);
    }

    /* Initialize the attribute list before we do the drawing, since
     * setting attrs with no objects drawn won't trigger a prep */
    if (attrs.size() > 0) {
	struct bu_vls ncmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncmd, "attr");
	for (a_it = attrs.begin(); a_it != attrs.end(); a_it++) {
	    bu_vls_printf(&ncmd, " %s", (*a_it).c_str());
	}
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
	bu_vls_free(&ncmd);
    }

    /* We know enough now to initialize */
    if (nirt_init(ns, dbip) == -1) {
	bu_exit(EXIT_FAILURE, "nirt_init failed: %s\n", argv[0]);
    }
    db_close(dbip); /* nirt will now manage its own copies of the dbip */

    /* Ready now - draw the initial set of objects, if supplied */
    if (ac > 1) {
	int i = 0;
	struct bu_vls ncmd = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ncmd, "draw");
	for (i = 1; i < ac; i++) {
	    bu_vls_printf(&ncmd, " %s", argv[i]);
	}
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
	bu_vls_free(&ncmd);
    }

    /* Report Database info */
    if (silent_mode != SILENT_YES) {
	const char *ustr = bu_units_string(dbip->dbi_local2base);
	printf("Database title: '%s'\n", dbip->dbi_title);
	printf("Database units: '%s'\n", (ustr) ? ustr : "Unknown units");
	(void)nirt_exec(ns, "nreport model_bounds");
    }

    /* If we're supposed to read input from stdin, do that */
    if (read_matrix) {
    }

    /* If we ended up with scripts to run before interacting, run them */
    if (init_scripts.size() > 0) {
	int nret = 0;
	for (unsigned int i = 0; i < init_scripts.size(); i++) {
	    nret = nirt_exec(ns, init_scripts.at(i).c_str());
	    if (nret == 1) goto done;
	}
	init_scripts.clear();
    }

    /* Start the interactive loop */
    while ((line = linenoise("nirt> ")) != NULL) {
	int nret = 0;
	bu_vls_sprintf(&iline, "%s", line);
	free(line);
	bu_vls_trimspace(&iline);
	if (!bu_vls_strlen(&iline)) continue;
	linenoiseHistoryAdd(bu_vls_addr(&iline));
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&iline, 0);
	    continue;
	}
	nret = nirt_exec(ns, bu_vls_addr(&iline));
	if (nret == 1) goto done;
	if (nret == -1) {
	    /* TODO - report error */
	}
	bu_vls_trunc(&iline, 0);
    }

done:
    linenoiseHistoryFree();
    bu_vls_free(&iline);
    nirt_destroy(ns);
    return 0;
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
