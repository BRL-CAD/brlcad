/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2018 United States Government as represented by
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
/** @file libged/lint.cpp
 *
 * The lint command for finding and reporting problems in .g files.
 */

#include "common.h"

extern "C" {
#include "bu/opt.h"
#include "raytrace.h"
#include "./ged_private.h"
}

struct _ged_lint_opts {
    int verbosity;
    int cyclic_check;
    int missing_check;
    int non_solid_check;
};

struct _ged_lint_opts *
_ged_lint_opts_create()
{
    struct _ged_lint_opts *o = NULL;
    BU_GET(o, struct _ged_lint_opts);
    o->verbosity = 0;
    o->cyclic_check = 0;
    o->missing_check = 0;
    o->non_solid_check = 0;
    return o;
}

void
_ged_lint_opts_destroy(struct _ged_lint_opts *o)
{
    if (!o) return;
    BU_PUT(o, struct _ged_lint_opts);
}

/* After option processing, turn on all checks
 * unless some specific subset have already
 * been enabled */
void
_ged_lint_opts_verify(struct _ged_lint_opts *o)
{
    int checks_set = 0;
    if (!o) return;
    if (o->cyclic_check) checks_set = 1;
    if (o->missing_check) checks_set = 1;
    if (o->non_solid_check) checks_set = 1;
    if (!checks_set) {
	o->cyclic_check = 1;
	o->missing_check = 1;
	o->non_solid_check = 1;
    }
}

int
_ged_cyclic_check(struct ged *gedp, int argc, struct directory *dpa)
{
    int ret = GED_OK;
    if (!gedp || argc == 0 || !dpa) return GED_ERROR;
    return ret;
}

int
_ged_missing_check(struct ged *gedp, int argc, struct directory *dpa)
{
    int ret = GED_OK;
    if (!gedp || argc == 0 || !dpa) return GED_ERROR;
    return ret;
}

int
_ged_non_solid_check(struct ged *gedp, int argc, struct directory *dpa)
{
    int ret = GED_OK;
    if (!gedp || argc == 0 || !dpa) return GED_ERROR;
    return ret;
}

extern "C" int
ged_lint(struct ged *gedp, int argc, const char *argv[])
{

    int ret = GED_OK;
    static const char *usage = "Usage: lint [ -CMS ] [obj1] [obj2] [...]\n";
    int print_help = 0;
    struct _ged_lint_opts *opts;
    struct bu_opt_desc d[6];
    struct directory *dpa;
    int nonexist_obj_cnt = 0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    opts = _ged_lint_opts_create();

    BU_OPT(d[0],  "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,  &(opts->verbosity),  "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "C", "cyclic",        "",  NULL,  &(opts->cyclic_check),     "Check for cyclic paths (combs whose children reference their parents - potential for infinite looping)");
    BU_OPT(d[3],  "M", "missing",       "",  NULL,  &(opts->missing_check),    "Check for objects reference by combs that are not in the database");
    BU_OPT(d[4],  "S", "non-solid",     "",  NULL,  &(opts->non_solid_check),  "Check for objects that are intended to be solid but do not satisfy that criteria (for example, non-solid BoTs)");
    BU_OPT_NULL(d[5]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help || !argc) {
	_ged_cmd_help(gedp, usage, d);
	ret = (print_help) ? GED_OK : GED_ERROR;
	goto ged_lint_memfree;
    }

    dpa = (struct directory *)bu_calloc(argc+1, sizeof(struct directory *), "dp array");
    nonexist_obj_cnt = _ged_sort_existing_objs(gedp, argc, argv, &dpa);

    if (nonexist_obj_cnt) {
	int i;
	bu_vls_printf(gedp->ged_result_str, "Object argument(s) supplied to lint that do not exist in the database:\n");
	for (i = argc - nonexist_obj_cnt - 1; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, " %s\n", argv[i]);
	}
	ret = GED_ERROR;
	goto ged_lint_memfree;
    }

    _ged_lint_opts_verify(opts);

    if (opts->cyclic_check) {
	ret = _ged_cyclic_check(gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
    }

    if (opts->missing_check) {
	ret = _ged_missing_check(gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
    }

    if (opts->non_solid_check) {
	ret = _ged_non_solid_check(gedp, argc, dpa);
	if (ret != GED_OK) {
	    goto ged_lint_memfree;
	}
    }

ged_lint_memfree:
    _ged_lint_opts_destroy(opts);

    if (dpa) bu_free(dpa, "dp array");

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
