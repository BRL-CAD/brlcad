/*                      M A I N . C P P
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
/** @file nirt/main.cpp
 *
 * This program is Natalie's Interactive Ray-Tracer
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

/* needed on mac in c90 mode */
#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include "brlcad_ident.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/units.h"
#include "analyze.h"

extern "C" {
#include "linenoise.h"
}


#if defined(HAVE_POPEN) && !defined(HAVE_DECL_POPEN)
extern "C" {
    extern FILE *popen(const char *command, const char *mode);
    extern int pclose(FILE *stream);
}
#endif


#define SILENT_UNSET    0
#define SILENT_YES      1
#define SILENT_NO       -1

/** FLAG VALUES FOR overlap_claims */
#define OVLP_RESOLVE            0
#define OVLP_REBUILD_FASTGEN    1
#define OVLP_REBUILD_ALL        2
#define OVLP_RETAIN             3

/* state for -M reading */
#define RMAT_SAW_EYE    0x01
#define RMAT_SAW_ORI    0x02
#define RMAT_SAW_VR     0x02

#define NIRT_PROMPT "nirt> "
#define IO_DATA_NULL { stdout, NULL, stderr, NULL, 0 }


struct nirt_io_data {
    FILE *out;
    struct bu_vls *outfile;
    FILE *err;
    struct bu_vls *errfile;
    int using_pipe;
};


struct script_file_data {
    std::vector<std::string> *init_scripts;
    struct bu_vls *filename;
    int file_cnt;
};


/* TODO - need some libbu mechanism for subcommands a.l.a. bu_opt... */
struct nirt_help_desc {
    const char *cmd;
    const char *desc;
};
const struct nirt_help_desc nirt_help_descs[] = {
    { "dest",           "set/query output destination" },
    { "statefile",      "set/query name of state file" },
    { "dump",           "write current state of struct nirt_state to the state file" },
    { "load",           "read new state for struct nirt_state from the state file" },
    { "clear",          "clears text from the terminal screen (interactive mode only)" },
    { (char *)NULL,     NULL }
};


static void
nirt_out(struct nirt_io_data *io_data, const char *output)
{
    fprintf(io_data->out, "%s", output);
    if (io_data->out == stdout)
	fflush(stdout);
}


static void
nirt_msg(struct nirt_io_data *io_data, const char *output)
{
    nirt_out(io_data, output);
}


static void
nirt_err(struct nirt_io_data *io_data, const char *output)
{
    fprintf(io_data->err, "%s", output);
    if (io_data->err == stderr)
	fflush(stderr);
}


static size_t
list_formats(struct nirt_io_data *io_data, char ***names)
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
    bu_vls_printf(&nfp, "%s", bu_dir(NULL, 0, BU_DIR_DATA, "nirt", NULL));
    files = bu_file_list(bu_vls_addr(&nfp), suffix, &filearray);
    if (names)
	*names = filearray;

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
		nirt_msg(io_data, bu_vls_addr(&fl)+15);
		nirt_msg(io_data, "\n");
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


static int
dequeue_scripts(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    std::vector<std::string> *init_scripts = (std::vector<std::string> *)set_var;
    if (set_var) {
	init_scripts->clear();
    }
    return 0;
}


static int
enqueue_script(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    std::vector<std::string> *init_scripts = (std::vector<std::string> *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script enqueue");
    if (set_var) {
	init_scripts->push_back(argv[0]);
    }
    return 1;
}


static int
enqueue_attrs(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    std::set<std::string> *attrs = (std::set<std::string> *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt attr enqueue");
    if (set_var) {
	attrs->insert(argv[0]);
    }
    return 1;
}


static int
enqueue_format(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    std::string s;
    std::ifstream file;
    struct script_file_data *sfd = (struct script_file_data *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script file");

    file.open(argv[0]);
    if (!file.is_open()) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	bu_vls_printf(&str, "%s/%s.nrt", bu_dir(NULL, 0, BU_DIR_DATA, "nirt", NULL), argv[0]);
	file.open(bu_vls_addr(&str));
	bu_vls_free(&str);

	if (!file.is_open()) {
	    bu_vls_printf(msg, "ERROR: -f [%s] does not exist as a file or predefined format\n", argv[0]);

	    return -1;
	}

	while (std::getline(file, s)) {
	    if (sfd) {
		sfd->init_scripts->push_back(s);
	    }
	}
    } else {
	while (std::getline(file, s)) {
	    if (sfd) {
		sfd->init_scripts->push_back(s);
	    }
	}
    }

    if (sfd) {
	bu_vls_sprintf(sfd->filename, "%s", argv[0]);
	sfd->file_cnt++;
    }

    return 1;
}


static int
decode_overlap(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    int *oval = (int *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt overlap handle");

    if (BU_STR_EQUAL(argv[0], "resolve") || BU_STR_EQUAL(argv[0], "0")) {
	if (oval)
	    (*oval) = OVLP_RESOLVE;
    } else if (BU_STR_EQUAL(argv[0], "rebuild_fastgen") || BU_STR_EQUAL(argv[0], "1")) {
	if (oval)
	    (*oval) = OVLP_REBUILD_FASTGEN;
    } else if (BU_STR_EQUAL(argv[0], "rebuild_all") || BU_STR_EQUAL(argv[0], "2")) {
	if (oval)
	    (*oval) = OVLP_REBUILD_ALL;
    } else if (BU_STR_EQUAL(argv[0], "retain") || BU_STR_EQUAL(argv[0], "3")) {
	if (oval)
	    (*oval) = OVLP_RETAIN;
    } else {
	bu_log("Illegal overlap_claims specification: '%s'\n", argv[0]);
	return -1;
    }
    return 1;
}


static int
nirt_stdout_hook(struct nirt_state *ns, void *u_data)
{
    struct nirt_io_data *io_data = (struct nirt_io_data *)u_data;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_OUT);
    nirt_out(io_data, bu_vls_addr(&out));
    bu_vls_free(&out);
    return 0;
}


static int
nirt_msg_hook(struct nirt_state *ns, void *u_data)
{
    struct nirt_io_data *io_data = (struct nirt_io_data *)u_data;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_MSG);
    nirt_msg(io_data, bu_vls_addr(&out));
    bu_vls_free(&out);
    return 0;
}


static int
nirt_stderr_hook(struct nirt_state *ns, void *u_data)
{
    struct nirt_io_data *io_data = (struct nirt_io_data *)u_data;
    struct bu_vls out = BU_VLS_INIT_ZERO;
    nirt_log(&out, ns, NIRT_ERR);
    nirt_err(io_data, bu_vls_addr(&out));
    bu_vls_free(&out);
    return 0;
}


static int
nirt_show_menu_hook(struct nirt_state *ns, void *u_data)
{
    struct bu_vls *log = (struct bu_vls *)u_data;
    struct bu_vls nline = BU_VLS_INIT_ZERO;
    nirt_log(&nline, ns, NIRT_OUT);
    bu_vls_printf(log, "%s", bu_vls_addr(&nline));
    bu_vls_free(&nline);
    return 0;
}


static int
nirt_dump_hook(struct nirt_state *ns, void *u_data)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct bu_vls *log = (struct bu_vls *)u_data;
    nirt_log(&tmp, ns, NIRT_OUT);
    bu_vls_printf(log, "%s", bu_vls_addr(&tmp));
    bu_vls_free(&tmp);
    return 0;
}


/* TODO - eventually, should support separate destinations for output
 * and error. */
static void
nirt_dest_cmd(struct nirt_io_data *io_data, struct bu_vls *iline)
{
    // Destination is handled above the library level.
    FILE *newf = NULL;
    int use_pipe = 0;
    bu_vls_nibble(iline, 4);
    bu_vls_trimspace(iline);
    if (!bu_vls_strlen(iline)) {
	if (io_data->using_pipe) {
	    fprintf(io_data->out, "destination = '| %s'\n", bu_vls_addr(io_data->outfile));
	} else {
	    fprintf(io_data->out, "destination = '%s'\n", bu_vls_addr(io_data->outfile));
	}
	return;
    }
    if (bu_vls_addr(iline)[0] == '|') {
	/* We're looking for a pipe... */
	use_pipe = 1;
	bu_vls_nibble(iline, 1);
	bu_vls_trimspace(iline);
    }
    if (bu_file_exists(bu_vls_addr(iline), NULL)) {
	fprintf(io_data->err, "File %s already exists.\n", bu_vls_addr(iline));
	bu_vls_trunc(iline, 0);
	return;
    }
    if (use_pipe) {
#ifdef HAVE_POPEN
	newf = popen(bu_vls_addr(iline), "w");
	if (!newf) {
	    fprintf(io_data->err, "Cannot open pipe '%s'\n", bu_vls_addr(iline));
	} else {
	    if (io_data->using_pipe) {
		pclose(io_data->out);
		pclose(io_data->err);
	    } else {
		if (io_data->out && io_data->out != stdout)
		    fclose(io_data->out);
		if (io_data->err && io_data->err != stderr)
		    fclose(io_data->err);
	    }
	    io_data->out = newf;
	    io_data->err = newf;
	    io_data->using_pipe = 1;
	    bu_vls_sprintf(io_data->outfile, "%s", bu_vls_addr(iline));
	    bu_vls_sprintf(io_data->errfile, "%s", bu_vls_addr(iline));
	}
#else
	fprintf(stderr, "Error, support for pipe output is disabled.  Try a redirect instead.\n");
#endif
    } else {
	/* File or stdout/stderr */
	if (io_data->using_pipe) {
	    pclose(io_data->out);
	    pclose(io_data->err);
	    io_data->using_pipe = 0;
	}
	if (BU_STR_EQUAL(bu_vls_addr(iline), "default")) {
	    if (io_data->out != stdout)
		fclose(io_data->out);
	    if (io_data->err != stderr)
		fclose(io_data->err);
	    io_data->out = stdout;
	    io_data->err = stderr;
	    bu_vls_sprintf(io_data->outfile, "stdout");
	    bu_vls_sprintf(io_data->errfile, "stderr");
	} else {
	    newf = fopen(bu_vls_addr(iline), "w");
	    if (!newf) {
		fprintf(io_data->err, "Cannot open file '%s'\n", bu_vls_addr(iline));
	    } else {
		io_data->out = newf;
		io_data->err = newf;
	    }
	    bu_vls_sprintf(io_data->outfile, "%s", bu_vls_addr(iline));
	    bu_vls_sprintf(io_data->errfile, "%s", bu_vls_addr(iline));
	}
    }
    bu_vls_trunc(iline, 0);
}


static int
nirt_app_exec(struct nirt_state *ns, struct bu_vls *iline, struct bu_vls *state_file, struct nirt_io_data *io_data)
{
    int nret = 0;

    /* A couple of the commands are application level, not
     * library level - handle them here. */
    if (BU_STR_EQUAL(bu_vls_addr(iline), "dest") || !bu_path_match("dest *", bu_vls_addr(iline), 0)) {
	nirt_dest_cmd(io_data, iline);
	return 0;
    }
    if (BU_STR_EQUAL(bu_vls_addr(iline), "dump") || !bu_path_match("dump *", bu_vls_addr(iline), 0)) {
	bu_vls_nibble(iline, 4);
	bu_vls_trimspace(iline);
	if (bu_vls_strlen(iline)) {
	    // TODO - this is lame, but matches the existing nirt behavior - need to fix it to work right...
	    fprintf(io_data->err, "Error: dump does not take arguments - output goes to the statefile %s\n", bu_vls_addr(state_file));
	    return -1;
	}
	struct bu_vls dumpstr = BU_VLS_INIT_ZERO;
	// Capture output for post-processing
	(void)nirt_udata(ns, (void *)&dumpstr);
	nirt_hook(ns, &nirt_dump_hook, NIRT_OUT);
	(void)nirt_exec(ns, "state -d");
	// Restore "normal" settings
	(void)nirt_udata(ns, (void *)io_data);
	nirt_hook(ns, &nirt_stdout_hook, NIRT_OUT);
	if(BU_STR_EQUAL(bu_vls_addr(io_data->outfile), "stdout")) {
	    bu_vls_printf(&dumpstr, "dest default");
	} else {
	    bu_vls_printf(&dumpstr, "dest %s", bu_vls_addr(io_data->outfile));
	}

	FILE *sfPtr = fopen(bu_vls_addr(state_file), "wb");
	if (!sfPtr) {
	    fprintf(io_data->err, "Error: Cannot open statefile '%s'\n", bu_vls_addr(state_file));
	    bu_vls_free(&dumpstr);
	    return -1;
	}
	fprintf(sfPtr, "%s\n", bu_vls_addr(&dumpstr));
	fclose(sfPtr);
	bu_vls_free(&dumpstr);
	return 0;
    }
    if (BU_STR_EQUAL(bu_vls_addr(iline), "load") || !bu_path_match("load *", bu_vls_addr(iline), 0)) {
	bu_vls_nibble(iline, 4);
	bu_vls_trimspace(iline);
	if (bu_vls_strlen(iline)) {
	    // TODO - this is lame, but matches the existing nirt behavior - need to fix it to work right...
	    fprintf(io_data->err, "Error: load does not take arguments - input is read from the statefile %s\n", bu_vls_addr(state_file));
	    return -1;
	}

	FILE *sfPtr = fopen(bu_vls_addr(state_file), "rb");
	if (!sfPtr) {
	    fprintf(io_data->err, "Error: could not open statefile %s\n", bu_vls_addr(state_file));
	    return -1;
	} else {
	    int ret = 0;
	    struct bu_vls fl = BU_VLS_INIT_ZERO;
	    while (bu_vls_gets(&fl, sfPtr)) {
		ret = nirt_exec(ns, bu_vls_addr(&fl));
		if (ret < 0) {
		    fprintf(io_data->err, "Error: failed to execute nirt script:\n\n%s\n\nwhen reading from statefile %s\n", bu_vls_addr(&fl), bu_vls_addr(state_file));
		    bu_vls_free(&fl);
		    fclose(sfPtr);
		    return ret;
		}
		bu_vls_trunc(&fl, 0);
	    }
	    bu_vls_free(&fl);
	}
	fclose(sfPtr);
	return 0;
    }
    if (BU_STR_EQUAL(bu_vls_addr(iline), "statefile") || !bu_path_match("statefile *", bu_vls_addr(iline), 0)) {
	bu_vls_nibble(iline, 9);
	bu_vls_trimspace(iline);
	if (!bu_vls_strlen(iline)) {
	    fprintf(io_data->out, "statefile = '%s'\n", bu_vls_addr(state_file));
	    return 0;
	}
	bu_vls_sprintf(state_file, "%s", bu_vls_addr(iline));
	return 0;
    }
    if (BU_STR_EQUAL(bu_vls_addr(iline), "?")) {
	// Get library documentation and add app level help to it.
	struct bu_vls helpstr = BU_VLS_INIT_ZERO;
	(void)nirt_udata(ns, (void *)&helpstr);
	nirt_hook(ns, &nirt_show_menu_hook, NIRT_OUT);
	(void)nirt_exec(ns, "?");
	(void)nirt_udata(ns, (void *)io_data);
	nirt_hook(ns, &nirt_stdout_hook, NIRT_OUT);
	fprintf(io_data->out, "%s\n", bu_vls_addr(&helpstr));
	bu_vls_free(&helpstr);

	const struct nirt_help_desc *hd;
	for (hd = nirt_help_descs; hd->cmd != NULL; hd++) {
	    fprintf(io_data->out, "%*s %s\n", 14, hd->cmd, hd->desc);
	}

	return 0;
    }

    /* Not one of the command level commands - up to the library */
    nret = nirt_exec(ns, bu_vls_addr(iline));

    return nret;
}


/* gateway into the old struct nirt_state code, if we need to run it */
extern "C" {int old_nirt_main(int argc, const char *argv[]);}

int
main(int argc, const char **argv)
{
    /* Make the old behavior accessible */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "--old")) {
	argv[1] = argv[0];
	argc--;	argv++;
	return old_nirt_main(argc, argv);
    }

    struct nirt_io_data io_data = IO_DATA_NULL;
    std::vector<std::string> init_scripts;
    struct bu_vls last_script_file = BU_VLS_INIT_ZERO;
    struct script_file_data sfd = {&init_scripts, &last_script_file, 0};
    char *line;
    int ac = 0;
    size_t i = 0;
    struct bu_vls state_file = BU_VLS_INIT_ZERO;
    std::set<std::string> attrs;
    std::set<std::string>::iterator a_it;
    struct bu_vls iline = BU_VLS_INIT_ZERO;
    struct bu_vls nirt_debug = BU_VLS_INIT_ZERO;
    int overlap_claims = OVLP_RESOLVE;
    int backout = 0;
    int bot_mintie = 0;
    int header_mode = 1;
    int show_formats = 0;
    int minpieces = -1;
    int print_help = 0;
    int read_matrix = 0;
    int silent_mode = SILENT_UNSET;
    int use_air = 0;
    int verbose_mode = 0;
    int ret = 0;
    int nret = 0;
    struct db_i *dbip;
    struct nirt_state *ns = NULL;
    const char *np = NULL;
    const char *units_str = NULL;
    char *dot = NULL;
    size_t fmtcnt = 0;
    char **names = NULL;
    struct bu_vls launch_cmd = BU_VLS_INIT_ZERO;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_vls ncmd = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    double scan[16] = MAT_INIT_ZERO;
    size_t prec = std::numeric_limits<fastf_t>::max_digits10;
    char *buf = NULL;
    int status = 0;
    mat_t m;
    mat_t q;
    /* These bu_opt_desc_opts settings approximate the old struct nirt_state help formatting */
    struct bu_opt_desc_opts dopts = { BU_OPT_ASCII, 1, 11, 67, NULL, NULL, NULL, 1, NULL, NULL };
    struct bu_opt_desc d[19] = {BU_OPT_DESC_NULL};

    BU_OPT(d[0],  "?", "",     "",       NULL,             &print_help,     "print help and exit");
    BU_OPT(d[1],  "h", "help", "",       NULL,             &print_help,     "print help and exit");
    BU_OPT(d[2],  "A", "",     "n",      &enqueue_attrs,   &attrs,          "add attribute_name=n");
    BU_OPT(d[3],  "M", "",     "",       NULL,             &read_matrix,    "read matrix, cmds on stdin");
    BU_OPT(d[4],  "b", "",     "",       NULL,             &backout,        "back out of geometry before first shot");
    BU_OPT(d[5],  "B", "",     "n",      &bu_opt_int,      &minpieces,      "set rt_bot_minpieces=n");
    BU_OPT(d[6],  "T", "",     "n",      &bu_opt_int,      &bot_mintie,     "set rt_bot_mintie=n (deprecated, use LIBRT_BOT_MINTIE instead)");
    BU_OPT(d[7],  "e", "",     "script", &enqueue_script,  &init_scripts,   "run script before interacting");
    BU_OPT(d[8],  "f", "",     "format", &enqueue_format,  &sfd,            "load predefined format (see -L) or file");
    BU_OPT(d[9],  "E", "",     "",       &dequeue_scripts, &init_scripts,   "ignore any -e or -f options specified earlier on the command line");
    BU_OPT(d[10], "L", "",     "",       NULL,             &show_formats,   "list output formatting options");
    BU_OPT(d[11], "s", "",     "",       NULL,             &silent_mode,    "run in silent (non-verbose) mode");
    BU_OPT(d[12], "v", "",     "",       NULL,             &verbose_mode,   "run in verbose mode");
    BU_OPT(d[13], "H", "",     "n",      &bu_opt_int,      &header_mode,    "flag (n) for enable/disable informational header - (n=1 [on] by default, always off in silent mode)");
    BU_OPT(d[14], "u", "",     "n",      &bu_opt_int,      &use_air,        "set use_air=n (default 0)");
    BU_OPT(d[15], "O", "",     "action", &decode_overlap,  &overlap_claims, "handle overlap claims via action");
    BU_OPT(d[16], "x", "",     "v",      &bu_opt_int,      &rt_debug,      "set librt(3) diagnostic flag=v");
    BU_OPT(d[17], "X", "",     "v",      &bu_opt_vls,      &nirt_debug,     "set nirt diagnostic flag=v");
    BU_OPT_NULL(d[18]);

    if (argc == 0 || !argv)
	return -1;

    /* Store the full execution command as a string so we can report it back in
     * output if we want to.  Do this before anything alters the argc/argv
     * values. */
    for (int ic = 0; ic < argc - 1; ic++) {
	if (strchr(argv[ic], ' ')) {
	    bu_vls_printf(&launch_cmd, "\"%s\" ", argv[ic]);
	} else {
	    bu_vls_printf(&launch_cmd, "%s ", argv[ic]);
	}
    }
    if (strchr(argv[argc-1], ' ')) {
	bu_vls_printf(&launch_cmd, "\"%s\" ", argv[argc-1]);
    } else {
	bu_vls_printf(&launch_cmd, "%s", argv[argc-1]);
    }

    /* Let libbu know where we are */
    bu_setprogname(argv[0]);

    argv++; argc--;

    ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);
    if (ac < 0) {
       	bu_exit(EXIT_FAILURE, "ERROR: option parsing failed\n%s", bu_vls_addr(&optparse_msg));
    } else if (ac > 0) {
	size_t badopts = 0;
	for (i=0; i<(size_t)ac; i++) {
	    if (argv[i][0] == '-') {
		bu_log("ERROR: unrecognized option %s\n", argv[i]);
		badopts++;
	    }
	}
	if (badopts)
	    bu_exit(EXIT_FAILURE, "Exiting.\n");
    }
    bu_vls_free(&optparse_msg);

    BU_GET(io_data.outfile, struct bu_vls);
    BU_GET(io_data.errfile, struct bu_vls);
    // Start out using the standard channels until a dest command tells us otherwise.
    io_data.out = stdout;
    io_data.err = stderr;
    bu_vls_init(io_data.outfile);
    bu_vls_init(io_data.errfile);
    bu_vls_sprintf(io_data.outfile, "stdout");
    bu_vls_sprintf(io_data.errfile, "stderr");

    if (show_formats) {
	/* Print available header formats and exit */
	nirt_msg(&io_data, "Formats available:\n");
	list_formats(&io_data, NULL);
	ret = EXIT_SUCCESS;
	goto done;
    }

    /* If we've been asked to print help or don't know what to do, print help
     * and exit */
    if (print_help || argc < 2 || (silent_mode == SILENT_YES && verbose_mode)) {
	char *help = bu_opt_describe(d, &dopts);
	ret = (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
	bu_vls_sprintf(&msg, "Usage: 'nirt [options] model.g objects...'\n\nNote: by default NIRT is using a new implementation which may have behavior changes.  During migration, old behavior can be enabled by adding the option \"--old\" as the first option to the nirt program.\n\nOptions:\n%s\n", help);
	nirt_out(&io_data, bu_vls_addr(&msg));
	if (help)
	    bu_free(help, "help str");
	goto done;
    }

    if (verbose_mode)
	silent_mode = SILENT_NO;

    /* Check if we're on a terminal or not - it has implications for the modes */
    if (silent_mode == SILENT_UNSET) {
	silent_mode = (isatty(0)) ? SILENT_NO : SILENT_YES;
    }

    if (silent_mode != SILENT_YES && header_mode) {
	nirt_msg(&io_data, brlcad_ident("Natalie's Interactive Ray Tracer"));
    }

    /* let users know that other output styles are available */
    if (silent_mode != SILENT_YES) {
	nirt_msg(&io_data, "Output format:");
	if (sfd.file_cnt == 0) {
	    nirt_msg(&io_data, " default");
	} else {
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_path_component(&fname, bu_vls_addr(sfd.filename), BU_PATH_BASENAME_EXTLESS);
	    nirt_msg(&io_data, " ");
	    nirt_msg(&io_data, bu_vls_addr(&fname));
	    bu_vls_free(&fname);
	}
	nirt_msg(&io_data, " (specify -L option for descriptive listing)\n");

	{
	    fmtcnt = list_formats(&io_data, &names);
	    if (fmtcnt > 0) {
		i = 0;
		nirt_msg(&io_data, "Formats available:");
		do {
		    /* trim off any filename suffix */
		    dot = strchr(names[i], '.');
		    if (dot)
			*dot = '\0';

		    nirt_msg(&io_data, " ");
		    nirt_msg(&io_data, names[i]);
		} while (++i < fmtcnt);

		nirt_msg(&io_data, " (specify via -f option)\n");
	    }
	    bu_argv_free(fmtcnt, names);
	}
    }

    /* OK, from here on out we are actually going to be working with NIRT
     * itself.  Set up the initial environment */
    if (rt_uniresource.re_magic == 0)
	rt_init_resource(&rt_uniresource, 0, NULL);

    if (silent_mode != SILENT_YES) {
	bu_vls_sprintf(&msg, "Database file:  '%s'\n", argv[0]);
	nirt_msg(&io_data, bu_vls_addr(&msg));
    }
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_vls_sprintf(&msg, "Unable to open db file %s\n", argv[0]);
	nirt_err(&io_data, bu_vls_addr(&msg));
	ret = EXIT_FAILURE;
	goto done;
    }
    RT_CK_DBI(dbip);

    if (silent_mode != SILENT_YES)
	nirt_msg(&io_data, "Building the directory...\n");
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_vls_sprintf(&msg, "db_dirbuild failed: %s\n", argv[0]);
	nirt_err(&io_data, bu_vls_addr(&msg));
	ret = EXIT_FAILURE;
	goto done;
    }

    BU_GET(ns, struct nirt_state);
    if (nirt_init(ns) == -1) {
	BU_PUT(ns, struct nirt_state);
	nirt_err(&io_data, "nirt state initialization failed\n");
	ret = EXIT_FAILURE;
	goto done;
    }

    /* Store the execution command as a commented nirt output line */
    bu_vls_sprintf(&ns->nirt_cmd, "# %s", bu_vls_cstr(&launch_cmd));
    bu_vls_sprintf(&ns->nirt_format_file, "%s", bu_vls_cstr(sfd.filename));

    /* Set up hooks so we can capture I/O from nirt_exec */
    (void)nirt_udata(ns, (void *)&io_data);
    nirt_hook(ns, &nirt_stdout_hook, NIRT_OUT);
    if (silent_mode != SILENT_YES) {
	nirt_hook(ns, &nirt_msg_hook, NIRT_MSG);
    }
    nirt_hook(ns, &nirt_stderr_hook, NIRT_ERR);

    /* If any of the options require state setup, run the appropriate commands
     * to put the struct nirt_state environment in the correct state. */

    if (silent_mode != SILENT_UNSET) {
	bu_vls_sprintf(&ncmd, "state silent_mode %d", silent_mode);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    if (bot_mintie) {
	bu_vls_sprintf(&ncmd, "%d", minpieces);
	bu_setenv("LIBRT_BOT_MINTIE", bu_vls_addr(&ncmd), 1);
    }

    if (minpieces >= 0) {
	bu_vls_sprintf(&ncmd, "bot_minpieces %d", minpieces);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    if (backout) {
	(void)nirt_exec(ns, "backout 1");
    }

    if (use_air) {
	bu_vls_sprintf(&ncmd, "useair %d", use_air);
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
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
	bu_vls_sprintf(&ncmd, "debug -V ANALYZE %s", bu_vls_cstr(&nirt_debug));
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    /* Initialize the attribute list before we do the drawing, since
     * setting attrs with no objects drawn won't trigger a prep */
    if (attrs.size() > 0) {
	bu_vls_sprintf(&ncmd, "attr");
	for (a_it = attrs.begin(); a_it != attrs.end(); a_it++) {
	    bu_vls_printf(&ncmd, " %s", (*a_it).c_str());
	}
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    /* Draw the initial set of objects, if supplied */
    if (ac > 1) {
	bu_vls_sprintf(&ncmd, "draw");
	for (i = 1; i < (size_t)ac; i++) {
	    bu_vls_printf(&ncmd, " %s", argv[i]);
	}
	(void)nirt_exec(ns, bu_vls_addr(&ncmd));
    }

    /* We know enough now to initialize */
    if (nirt_init_dbip(ns, dbip) == -1) {
	BU_PUT(ns, struct nirt_state);
	bu_vls_sprintf(&msg, "nirt_init_dbip failed: %s\n", argv[0]);
	nirt_err(&io_data, bu_vls_addr(&msg));
	ret = EXIT_FAILURE;
	goto done;
    }
    db_close(dbip); /* nirt will now manage its own copies of the dbip */

    /* Report Database info */
    if (silent_mode != SILENT_YES) {
	units_str = bu_units_string(dbip->dbi_local2base);
	bu_vls_sprintf(&msg, "Database title: '%s'\n", dbip->dbi_title);
	nirt_msg(&io_data, bu_vls_addr(&msg));
	bu_vls_sprintf(&msg, "Database units: '%s'\n", (units_str) ? units_str : "Unknown units");
	nirt_msg(&io_data, bu_vls_addr(&msg));
	(void)nirt_exec(ns, "state model_bounds");
    }

    /* Initialize the state file to "nirt_state" */
    bu_vls_sprintf(&state_file, "nirt_state");

    /* If we ended up with scripts to run before interacting, run them */
    if (init_scripts.size() > 0) {
	for (i = 0; i < init_scripts.size(); i++) {
	    if (nirt_exec(ns, init_scripts.at(i).c_str()) == 1)
		goto done;
	}
	init_scripts.clear();
    }

    /* If we're supposed to read matrix input from stdin instead of interacting, do that */
    if (read_matrix) {
	while ((buf = rt_read_cmd(stdin)) != (char *) 0) {
	    if (bu_strncmp(buf, "eye_pt", 6) == 0) {
		struct bu_vls eye_pt_cmd = BU_VLS_INIT_ZERO;

		bu_vls_sprintf(&eye_pt_cmd, "xyz %s", buf + 6);

		if (nirt_exec(ns, bu_vls_addr(&eye_pt_cmd)) < 0) {
		    bu_vls_free(&eye_pt_cmd);
		    nirt_err(&io_data, "nirt: read_mat(): Failed to read eye_pt\n");
		    ret = EXIT_FAILURE;
		    goto done;
		}

		bu_vls_free(&eye_pt_cmd);
		status |= RMAT_SAW_EYE;
	    } else if (bu_strncmp(buf, "orientation", 11) == 0) {
		if (sscanf(buf + 11, "%lf%lf%lf%lf", &scan[X], &scan[Y], &scan[Z], &scan[W]) != 4) {
		    nirt_err(&io_data, "nirt: read_mat(): Failed to read orientation\n");
		    ret = EXIT_FAILURE;
		    goto done;
		}

		MAT_COPY(q, scan);
		quat_quat2mat(m, q);
		//bn_mat_print("view matrix", m);
		status |= RMAT_SAW_ORI;
	    } else if (bu_strncmp(buf, "viewrot", 7) == 0) {
		if (sscanf(buf + 7,
			   "%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
			   &scan[0], &scan[1], &scan[2], &scan[3],
			   &scan[4], &scan[5], &scan[6], &scan[7],
			   &scan[8], &scan[9], &scan[10], &scan[11],
			   &scan[12], &scan[13], &scan[14], &scan[15]) != 16) {
		    bu_exit(1, "nirt: read_mat(): Failed to read viewrot\n");
		}

		MAT_COPY(m, scan);
		//bn_mat_print("view matrix", m);
		status |= RMAT_SAW_VR;
	    }
	}
	if ((status & RMAT_SAW_EYE) == 0) {
	    nirt_err(&io_data, "nirt: read_mat(): Was given no eye_pt\n");
	    ret = EXIT_FAILURE;
	    goto done;
	}
	if ((status & (RMAT_SAW_ORI | RMAT_SAW_VR)) == 0) {
	    nirt_err(&io_data, "nirt: read_mat(): Was given no orientation or viewrot\n");
	    ret = EXIT_FAILURE;
	    goto done;
	}

	// Now, construct a dir command line from the m matrix
	bu_vls_sprintf(&ncmd, "dir %.*f %.*f %.*f", (int)prec, -m[8], (int)prec, -m[9], (int)prec, -m[10]);
	if (nirt_exec(ns, bu_vls_addr(&ncmd)) < 0) {
	    nirt_err(&io_data, "nirt: read_mat(): Failed to set the view direction\n");
	    ret = EXIT_FAILURE;
	    goto done;
	}

	if (nirt_exec(ns, "s") < 0) {
	    nirt_err(&io_data, "nirt: read_mat(): Shot failed\n");
	    ret = EXIT_FAILURE;
	    goto done;
	}
	ret = EXIT_SUCCESS;
	goto done;
    }


    /* Start the interactive loop */
    np = (silent_mode == SILENT_YES) ? "": NIRT_PROMPT;
    while ((line = linenoise(np)) != NULL) {
	bu_vls_sprintf(&iline, "%s", line);
	free(line);
	bu_vls_trimspace(&iline);

	if (!bu_vls_strlen(&iline))
	    continue;

	linenoiseHistoryAdd(bu_vls_addr(&iline));

	/* The "clear" command only makes sense in interactive
	 * mode */
	if (BU_STR_EQUAL(bu_vls_addr(&iline), "clear")) {
	    linenoiseClearScreen();
	    bu_vls_trunc(&iline, 0);
	    continue;
	}

	nret = nirt_app_exec(ns, &iline, &state_file, &io_data);
	if (nret == 1)
	    goto done;
	bu_vls_trunc(&iline, 0);
    }

done:
    linenoiseHistoryFree();
    bu_vls_free(&launch_cmd);
    bu_vls_free(&msg);
    bu_vls_free(&ncmd);
    bu_vls_free(&iline);

    if (io_data.using_pipe) {
	pclose(io_data.out);
	pclose(io_data.err);
    } else {
	if (io_data.out != stdout)
	    fclose(io_data.out);
	if (io_data.err != stderr)
	    fclose(io_data.err);
    }

    bu_vls_free(io_data.outfile);
    bu_vls_free(io_data.errfile);
    BU_PUT(io_data.outfile, struct bu_vls);
    BU_PUT(io_data.errfile, struct bu_vls);
    nirt_destroy(ns);
    if (ns)
	BU_PUT(ns, struct nirt_state);

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
