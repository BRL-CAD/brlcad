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
#include "bu/str.h"
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

    struct nirt_opt_vals *v = (struct nirt_opt_vals *)set_var;
    for (size_t i = 0; i < BU_PTBL_LEN(&v->init_scripts); i++) {
	char *s = (char *)BU_PTBL_GET(&v->init_scripts, i);
	bu_free(s, "script");
    }
    bu_ptbl_reset(&v->init_scripts);
    v->fmt_set = 0;
    v->script_set = 0;

    return 0;
}


static int
enqueue_script(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct nirt_opt_vals *v = (struct nirt_opt_vals *)set_var;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nirt script enqueue");
    if (set_var) {
	char *script = bu_strdup(argv[0]);
	bu_ptbl_ins(&v->init_scripts, (long *)script);
	v->script_set = 1;
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
	std::map<std::string, char *> vlsmp;
	for (size_t i = 0; i < BU_PTBL_LEN(attrs); i++) {
	    char *a = (char *)BU_PTBL_GET(attrs, i);
	    vlsmp[std::string(a)] = a;
	}

	if (vlsmp.find(std::string(argv[0])) == vlsmp.end()) {
	    char *attr = bu_strdup(argv[0]);
	    vlsmp[std::string(attr)] = attr;

	    bu_ptbl_reset(attrs);

	    std::map<std::string, char *>::iterator v_it;
	    for (v_it = vlsmp.begin(); v_it != vlsmp.end(); v_it++) {
		char *vp = v_it->second;
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
	    char *script = bu_strdup(s.c_str());
	    bu_ptbl_ins(&v->init_scripts, (long *)script);
	}
    } else {
	while (std::getline(file, s)) {
	    char *script = bu_strdup(s.c_str());
	    bu_ptbl_ins(&v->init_scripts, (long *)script);
	}
    }
    file.close();

    bu_vls_sprintf(&v->filename, "%s", argv[0]);
    v->file_cnt++;

    v->fmt_set = 1;

    return 1;
}


struct bu_opt_desc *
nirt_opt_desc(struct nirt_opt_vals *v)
{
    if (!v)
	return NULL;

    struct bu_opt_desc *d = (struct bu_opt_desc *)bu_calloc(25, sizeof(struct bu_opt_desc), "opt array");
    BU_OPT(d[0],  "h", "help",      "",         NULL,             &v->print_help,     "print help and exit");
    BU_OPT(d[1],  "?", "",          "",         NULL,             &v->print_help,     "print help and exit");
    BU_OPT(d[2],  "A", "",          "n",        &enqueue_attrs,   &v->attrs,          "add attribute_name=n");
    BU_OPT(d[3],  "M", "",          "",         NULL,             &v->read_matrix,    "read matrix, cmds on stdin");
    BU_OPT(d[4],  "b", "",          "",         NULL,             NULL,               "back out of geometry before first shot (deprecated, default behavior)");
    BU_OPT(d[5],  "c", "",          "",         NULL,             &v->current_center, "shoot ray from current center");
    BU_OPT(d[6],  "e", "",          "script",   &enqueue_script,  v,                  "run script before interacting");
    BU_OPT(d[7],  "f", "",          "format",   &enqueue_format,  v,                  "load predefined format (see -L) or file");
    BU_OPT(d[8],  "E", "",          "",         &dequeue_scripts, v,                  "ignore any -e or -f options specified earlier on the command line");
    BU_OPT(d[9],  "L", "",          "",         NULL,             &v->show_formats,   "list output formatting options");
    BU_OPT(d[10], "s", "",          "",         NULL,             &v->silent_mode,    "run in silent (non-verbose) mode");
    BU_OPT(d[11], "v", "",          "",         NULL,             &v->verbose_mode,   "run in verbose mode");
    BU_OPT(d[12], "H", "",          "n",        &bu_opt_int,      &v->header_mode,    "flag (n) for enable/disable informational header - (n=1 [on] by default, always off in silent mode)");
    BU_OPT(d[13], "u", "",          "n",        &bu_opt_int,      &v->use_air,        "set use_air=n (default 0)");
    BU_OPT(d[14], "O", "",          "action",   &decode_overlap,  &v->overlap_claims, "handle overlap claims via action");
    BU_OPT(d[15], "x", "",          "v",        &bu_opt_int,      &rt_debug,          "set librt(3) diagnostic flag=v");
    BU_OPT(d[16], "X", "",          "v",        &bu_opt_vls,      &v->nirt_debug,     "set nirt diagnostic flag=v");
    BU_OPT(d[17], "", "center",     "x,y,z",    &bu_opt_vect_t,   &v->center_model,   "specify xyz center point");
    BU_OPT(d[18], "", "xyz",        "x,y,z",    &bu_opt_vect_t,   &v->center_model,   "specify xyz center point");
    BU_OPT(d[19], "", "plotfile",   "filename", &bu_opt_vls,      &v->plotfile,       "optional file for graphical plots of segment outputs");
    BU_OPT(d[20], "", "color_odd",  "r/g/b",    &bu_opt_color,    &v->color_odd,      "Color to use when plotting odd segments (default rgb:0/255/255");
    BU_OPT(d[21], "", "color_even", "r/g/b",    &bu_opt_color,    &v->color_even,     "Color to use when plotting even segments (default rgb:255/255/0");
    BU_OPT(d[22], "", "color_gap",  "r/g/b",    &bu_opt_color,    &v->color_gap,      "Color to use when plotting gaps between segments (default rgb:255/0/255");
    BU_OPT(d[23], "", "color_ovlp", "r/g/b",    &bu_opt_color,    &v->color_ovlp,     "Color to use when plotting overlap segments (default rgb:255/255/255");
    BU_OPT_NULL(d[24]);

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

    // Reset colors
    struct bu_color cyan = BU_COLOR_CYAN;
    struct bu_color yellow = BU_COLOR_YELLOW;
    struct bu_color purple = BU_COLOR_PURPLE;
    struct bu_color white = BU_COLOR_WHITE;
    BU_COLOR_CPY(&v->color_odd, &cyan);
    BU_COLOR_CPY(&v->color_even, &yellow);
    BU_COLOR_CPY(&v->color_gap, &purple);
    BU_COLOR_CPY(&v->color_ovlp, &white);

    bu_vls_trunc(&v->nirt_debug, 0);
    bu_vls_trunc(&v->filename, 0);
    bu_vls_trunc(&v->plotfile, 0);

    for (size_t i = 0; i < BU_PTBL_LEN(&v->init_scripts); i++) {
	char *s = (char *)BU_PTBL_GET(&v->init_scripts, i);
	bu_free(s, "script");
    }
    bu_ptbl_reset(&v->init_scripts);

    for (size_t i = 0; i < BU_PTBL_LEN(&v->attrs); i++) {
	char *s = (char *)BU_PTBL_GET(&v->attrs, i);
	bu_free(s, "script");
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
    bu_vls_free(&v->plotfile);
    bu_ptbl_free(&v->init_scripts);
    bu_ptbl_free(&v->attrs);
}


void
nirt_opt_mk_args(struct bu_ptbl *tbl, struct nirt_opt_vals *tgt, struct nirt_opt_vals *src, int add_scripts)
{
    if (!tbl || !tgt || !src)
	return;

    char *str = NULL;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;

    if (src->print_help && !tgt->print_help) {
	str = bu_strdup("-h");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->read_matrix && !tgt->read_matrix) {
	str = bu_strdup("-M");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->current_center && !tgt->current_center) {
	str = bu_strdup("-c");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->show_formats && !tgt->show_formats) {
	str = bu_strdup("-L");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->silent_mode && !tgt->silent_mode) {
	str = bu_strdup("-s");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->verbose_mode && !tgt->verbose_mode) {
	str = bu_strdup("-v");
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->header_mode != tgt->header_mode) {
	str = bu_strdup("-H");
	bu_ptbl_ins(tbl, (long *)str);
	bu_vls_sprintf(&tmp, "%d", tgt->header_mode);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->use_air != tgt->use_air) {
	str = bu_strdup("-u");
	bu_ptbl_ins(tbl, (long *)str);
	bu_vls_sprintf(&tmp, "%d", tgt->use_air);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (src->overlap_claims != tgt->overlap_claims) {
	str = bu_strdup("-O");
	bu_ptbl_ins(tbl, (long *)str);
	bu_vls_sprintf(&tmp, "%d", tgt->overlap_claims);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (bu_vls_strcmp(&src->nirt_debug, &tgt->nirt_debug)) {
	str = bu_strdup("-X");
	bu_ptbl_ins(tbl, (long *)str);
	str = bu_strdup(bu_vls_cstr(&tgt->nirt_debug));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (bu_vls_strcmp(&src->plotfile , &tgt->plotfile)) {
	str = bu_strdup("--plotfile");
	bu_ptbl_ins(tbl, (long *)str);
	str = bu_strdup(bu_vls_cstr(&tgt->plotfile));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (!BU_COLOR_NEAR_EQUAL(src->color_odd, tgt->color_odd, VUNITIZE_TOL)) {
	str = bu_strdup("--color_odd");
	bu_ptbl_ins(tbl, (long *)str);
	int rgb[3] = {0, 0, 0};
	bu_color_to_rgb_ints(&tgt->color_odd, &rgb[RED], &rgb[GRN], &rgb[BLU]);
	bu_vls_sprintf(&tmp, "%d/%d/%d", rgb[RED], rgb[GRN], rgb[BLU]);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (!BU_COLOR_NEAR_EQUAL(src->color_even, tgt->color_even, VUNITIZE_TOL)) {
	str = bu_strdup("--color_even");
	bu_ptbl_ins(tbl, (long *)str);
	int rgb[3] = {0, 0, 0};
	bu_color_to_rgb_ints(&tgt->color_even, &rgb[RED], &rgb[GRN], &rgb[BLU]);
	bu_vls_sprintf(&tmp, "%d/%d/%d", rgb[RED], rgb[GRN], rgb[BLU]);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (!BU_COLOR_NEAR_EQUAL(src->color_gap, tgt->color_gap, VUNITIZE_TOL)) {
	str = bu_strdup("--color_gap");
	bu_ptbl_ins(tbl, (long *)str);
	int rgb[3] = {0, 0, 0};
	bu_color_to_rgb_ints(&tgt->color_gap, &rgb[RED], &rgb[GRN], &rgb[BLU]);
	bu_vls_sprintf(&tmp, "%d/%d/%d", rgb[RED], rgb[GRN], rgb[BLU]);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (!BU_COLOR_NEAR_EQUAL(src->color_ovlp, tgt->color_ovlp, VUNITIZE_TOL)) {
	str = bu_strdup("--color_ovlp");
	bu_ptbl_ins(tbl, (long *)str);
	int rgb[3] = {0, 0, 0};
	bu_color_to_rgb_ints(&tgt->color_ovlp, &rgb[RED], &rgb[GRN], &rgb[BLU]);
	bu_vls_sprintf(&tmp, "%d/%d/%d", rgb[RED], rgb[GRN], rgb[BLU]);
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    if (!VNEAR_EQUAL(src->center_model, tgt->center_model, VUNITIZE_TOL)) {
	str = bu_strdup("--xyz");
	bu_ptbl_ins(tbl, (long *)str);
	bu_vls_sprintf(&tmp, "%0.17f,%0.17f,%0.17f", V3ARGS(tgt->center_model));
	str = bu_strdup(bu_vls_cstr(&tmp));
	bu_ptbl_ins(tbl, (long *)str);
    }

    std::set<std::string> attrset;
    for (size_t i = 0; i < BU_PTBL_LEN(&src->attrs); i++) {
	char *a = (char *)BU_PTBL_GET(&src->attrs, i);
	attrset.insert(std::string(a));
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&tgt->attrs); i++) {
	char *a = (char *)BU_PTBL_GET(&tgt->attrs, i);
	if (attrset.find(std::string(a)) == attrset.end()) {
	    str = bu_strdup("-A");
	    bu_ptbl_ins(tbl, (long *)str);
	    str = bu_strdup(a);
	    bu_ptbl_ins(tbl, (long *)str);
	}
    }

    if (add_scripts) {
	for (size_t i = 0; i < BU_PTBL_LEN(&tgt->init_scripts); i++) {
	    char *script = (char *)BU_PTBL_GET(&tgt->init_scripts, i);
	    str = bu_strdup("-e");
	    bu_ptbl_ins(tbl, (long *)str);
	    str = bu_strdup(script);
	    bu_ptbl_ins(tbl, (long *)str);
	}
    }

    bu_vls_free(&tmp);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
