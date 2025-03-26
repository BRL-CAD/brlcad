/*                        O P T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file opts.cpp
 *
 * Natalie's Interactive Ray-Tracer (NIRT) option parsing.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <iostream>
#include <map>
#include <string>

#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "rt/debug.h"
#include "analyze/nirt.h"

#include "./nirt.h"

static int
decode_overlap(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    int *oval = (int *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt overlap handle");

    if (BU_STR_EQUAL(argv[0], "resolve") || BU_STR_EQUAL(argv[0], "0")) {
	if (oval) {
	    (*oval) = NIRT_OVLP_RESOLVE;
	}
    } else if (BU_STR_EQUAL(argv[0], "rebuild_fastgen") || BU_STR_EQUAL(argv[0], "1")) {
	if (oval) {
	    (*oval) = NIRT_OVLP_REBUILD_FASTGEN;
	}
    } else if (BU_STR_EQUAL(argv[0], "rebuild_all") || BU_STR_EQUAL(argv[0], "2")) {
	if (oval) {
	    (*oval) = NIRT_OVLP_REBUILD_ALL;
	}
    } else if (BU_STR_EQUAL(argv[0], "retain") || BU_STR_EQUAL(argv[0], "3")) {
	if (oval) {
	    (*oval) = NIRT_OVLP_RETAIN;
	}
    } else {
	bu_log("Illegal overlap_claims specification: '%s'\n", argv[0]);
	return -1;
    }
    return 1;
}


static int
dequeue_scripts(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    if (!set_var)
	return 0;

    struct bu_ptbl *init_scripts = (struct bu_ptbl *)set_var;
    for (size_t i = 0; i < BU_PTBL_LEN(init_scripts); i++) {
	struct bu_vls *s = (struct bu_vls *)BU_PTBL_GET(init_scripts, i);
	bu_vls_free(s);
	BU_PUT(s, struct bu_vls);
    }
    bu_ptbl_reset(init_scripts);

    return 0;
}


static int
enqueue_script(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct bu_ptbl *init_scripts = (struct bu_ptbl *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script enqueue");
    if (set_var) {
	struct bu_vls *script;
	BU_GET(script, struct bu_vls);
	bu_vls_init(script);
	bu_vls_sprintf(script, "%s", argv[0]);
	bu_ptbl_ins(init_scripts, (long *)script);
    }
    return 1;
}


static int
enqueue_attrs(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct bu_ptbl *attrs = (struct bu_ptbl *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt attr enqueue");
    if (set_var) {
	// Original container was a C++ set, so mimic sorted uniqueness
	// while keeping the vls pointers and bu_ptbl container.
	std::map<std::string, struct bu_vls *> vlsmp;
	for (size_t i = 0; i < BU_PTBL_LEN(attrs); i++) {
	    struct bu_vls *a = (struct bu_vls *)BU_PTBL_GET(attrs, i);
	    vlsmp[std::string(bu_vls_cstr(a))] = a;
	}

	if (vlsmp.find(std::string(argv[0])) == vlsmp.end()) {
	    struct bu_vls *attr;
	    BU_GET(attr, struct bu_vls);
	    bu_vls_init(attr);
	    bu_vls_sprintf(attr, "%s", argv[0]);
	    vlsmp[std::string(bu_vls_cstr(attr))] = attr;

	    bu_ptbl_reset(attrs);

	    std::map<std::string, struct bu_vls *>::iterator v_it;
	    for (v_it = vlsmp.begin(); v_it != vlsmp.end(); v_it++) {
		struct bu_vls *vp = v_it->second;
		bu_ptbl_ins(attrs, (long *)vp);
	    }
	}
    }
    return 1;
}


static int
enqueue_format(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    std::string s;
    std::ifstream file;
    struct nirt_opt_vals *v = (struct nirt_opt_vals *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script file");

    if (!v)
	return 1;

    file.open(argv[0]);
    if (!file.is_open()) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	bu_vls_printf(&str, "%s/%s.nrt", bu_dir(NULL, 0, BU_DIR_DATA, "nirt", NULL), argv[0]);
	file.open(bu_vls_cstr(&str));
	bu_vls_free(&str);

	if (!file.is_open()) {
	    if (msg)
		bu_vls_printf(msg, "ERROR: -f [%s] does not exist as a file or predefined format\n", argv[0]);
	    return -1;
	}

	while (std::getline(file, s)) {
	    struct bu_vls *script;
	    BU_GET(script, struct bu_vls);
	    bu_vls_init(script);
	    bu_vls_sprintf(script, "%s", s.c_str());
	    bu_ptbl_ins(&v->init_scripts, (long *)script);
	}
    } else {
	while (std::getline(file, s)) {
	    struct bu_vls *script;
	    BU_GET(script, struct bu_vls);
	    bu_vls_init(script);
	    bu_vls_sprintf(script, "%s", s.c_str());
	    bu_ptbl_ins(&v->init_scripts, (long *)script);
	}
    }

    bu_vls_sprintf(&v->filename, "%s", argv[0]);
    v->file_cnt++;

    return 1;
}


struct bu_opt_desc *
nirt_opt_desc(struct nirt_opt_vals *v)
{
    if (!v)
	return NULL;

    struct bu_opt_desc *d = (struct bu_opt_desc *)bu_calloc(18, sizeof(struct bu_opt_desc), "opt array");
    BU_OPT(d[0],  "?", "",     "",       NULL,             &v->print_help,     "print help and exit");
    BU_OPT(d[1],  "h", "help", "",       NULL,             &v->print_help,     "print help and exit");
    BU_OPT(d[2],  "A", "",     "n",      &enqueue_attrs,   &v->attrs,          "add attribute_name=n");
    BU_OPT(d[3],  "M", "",     "",       NULL,             &v->read_matrix,    "read matrix, cmds on stdin");
    BU_OPT(d[4],  "b", "",     "",       NULL,             NULL,               "back out of geometry before first shot (deprecated, default behavior)");
    BU_OPT(d[5],  "c", "",     "",       NULL,             &v->current_center, "shoot ray from current center");
    BU_OPT(d[6],  "e", "",     "script", &enqueue_script,  &v->init_scripts,   "run script before interacting");
    BU_OPT(d[7],  "f", "",     "format", &enqueue_format,  v,                  "load predefined format (see -L) or file");
    BU_OPT(d[8],  "E", "",     "",       &dequeue_scripts, &v->init_scripts,   "ignore any -e or -f options specified earlier on the command line");
    BU_OPT(d[9],  "L", "",     "",       NULL,             &v->show_formats,   "list output formatting options");
    BU_OPT(d[10], "s", "",     "",       NULL,             &v->silent_mode,    "run in silent (non-verbose) mode");
    BU_OPT(d[11], "v", "",     "",       NULL,             &v->verbose_mode,   "run in verbose mode");
    BU_OPT(d[12], "H", "",     "n",      &bu_opt_int,      &v->header_mode,    "flag (n) for enable/disable informational header - (n=1 [on] by default, always off in silent mode)");
    BU_OPT(d[13], "u", "",     "n",      &bu_opt_int,      &v->use_air,        "set use_air=n (default 0)");
    BU_OPT(d[14], "O", "",     "action", &decode_overlap,  &v->overlap_claims, "handle overlap claims via action");
    BU_OPT(d[15], "x", "",     "v",      &bu_opt_int,      &rt_debug,          "set librt(3) diagnostic flag=v");
    BU_OPT(d[16], "X", "",     "v",      &bu_opt_vls,      &v->nirt_debug,     "set nirt diagnostic flag=v");
    BU_OPT_NULL(d[17]);

    return d;
}

void
nirt_opt_vals_reset(struct nirt_opt_vals *v)
{
    if (!v)
	return;

    struct nirt_opt_vals opt_defaults = NIRT_OPT_INIT;
    v->current_center = opt_defaults.current_center;
    v->file_cnt = opt_defaults.file_cnt;
    v->header_mode = opt_defaults.header_mode;
    v->overlap_claims = opt_defaults.overlap_claims;
    v->print_help = opt_defaults.print_help;
    v->read_matrix = opt_defaults.read_matrix;
    v->show_formats = opt_defaults.show_formats;
    v->silent_mode = opt_defaults.silent_mode;
    v->use_air = opt_defaults.use_air;
    v->verbose_mode = opt_defaults.verbose_mode;

    bu_vls_trunc(&v->nirt_debug, 0);
    bu_vls_trunc(&v->filename, 0);
    for (size_t i = 0; i < BU_PTBL_LEN(&v->init_scripts); i++) {
	struct bu_vls *s = (struct bu_vls *)BU_PTBL_GET(&v->init_scripts, i);
	bu_vls_free(s);
	BU_PUT(s, struct bu_vls);
    }
    bu_ptbl_reset(&v->init_scripts);
    for (size_t i = 0; i < BU_PTBL_LEN(&v->attrs); i++) {
	struct bu_vls *s = (struct bu_vls *)BU_PTBL_GET(&v->attrs, i);
	bu_vls_free(s);
	BU_PUT(s, struct bu_vls);
    }
    bu_ptbl_reset(&v->attrs);
}

void
nirt_opt_vals_free(struct nirt_opt_vals *v)
{
    if (!v)
	return;

    nirt_opt_vals_reset(v);

    bu_vls_free(&v->nirt_debug);
    bu_vls_free(&v->filename);
    bu_ptbl_free(&v->init_scripts);
    bu_ptbl_free(&v->attrs);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
