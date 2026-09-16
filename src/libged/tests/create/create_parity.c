/*                 C R E A T E _ P A R I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file create_parity.c
 *
 * Generic regression-test driver for GED object-creation commands.
 *
 * Each created object is snapshotted with idb_meth->ft_get() - the same
 * serializer the 'get' command uses.
 * NOTE: this means this test is deeply coupled with librt/rt_functab:ft_get
 *
 * Validation happens at several levels:
 *   1) passing labels: the full snapshot is compared for exact string equality
 *      against an "expected" captured from the current implementation - this locks
 *      the command's default geometry so deltas catch ANY output changes.
 *   2) failing labels: deprecated/unsupported types must be rejected, creating
 *      nothing.
 *   3) command-specific extra_checks() (e.g. make's origin/scale passes).
 *   4) coverage against librt's rt_functab: all exposed labels in librt should
 *	be accounted for in this test.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

#include "create_parity.h"


/* we expect all exposed prims to be addressed by pass/fail/skip */
static int case_covered(const char *label) {
    const struct cp_case *test_case;
    int i;

    for (test_case = pass_cases; test_case->label; test_case++)
	if (BU_STR_EQUAL(test_case->label, label))
	    return 1;

    for (i = 0; fail_labels[i]; i++)
	if (BU_STR_EQUAL(fail_labels[i], label))
	    return 1;

    for (i = 0; skip_labels[i]; i++)
	if (BU_STR_EQUAL(skip_labels[i], label))
	    return 1;

    return 0;
}

/* Coverage gate: every primitive label librt exposes in OBJ[] (ft_label) must
 * have a test case, or be called out to fail/skip.
 * Anything uncovered is reported and counted as a run failure, so a newly added
 * primitive can't slip in untested.  NULL/UNUSED placeholder rows are skipped.
 * Returns the number of uncovered labels.
 */
static int check_coverage(void) {
    const struct rt_functab *ftp;
    int missing = 0;

    for (ftp = OBJ; ftp->magic != 0; ftp++) {
	if (ftp->ft_label[0] == ' ' || ftp->ft_label[0] == '\0')
	    continue;
	if (BU_STR_EQUAL(ftp->ft_label, "NULL") ||
	    bu_strncmp(ftp->ft_label, "UNUSED", 6) == 0)
	    continue;
	if (!case_covered(ftp->ft_label)) {
	    bu_log("FAIL(coverage): no %s test case for ft_label '%s'\n", cmd_name, ftp->ft_label);
	    missing++;
	}
    }
    return missing;
}

/* create tempfile .g */
static struct ged* open_test_db(void) {
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    struct rt_wdb *wdbp;
    struct ged *gedp;

    if (!fp)
	return NULL;
    fclose(fp);

    wdbp = wdb_fopen(tmppath);
    if (!wdbp)
	return NULL;
    db_close(wdbp->dbip);

    gedp = ged_open("db", tmppath, 1);
    return gedp;
}


/* Snapshot object 'name' into 'out' via ft_get, applying 'mat' at import (NULL
 * = identity).  Returns 0 on success */
int snapshot(struct ged *gedp, const char *name, const mat_t mat, struct bu_vls *out) {
    struct directory *dp;
    struct rt_db_internal intern;
    int ret;

    dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return 1;

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, mat) < 0)
	return 1;

    if (!intern.idb_meth || !intern.idb_meth->ft_get) {
	rt_db_free_internal(&intern);
	return 1;
    }

    ret = (intern.idb_meth->ft_get(out, &intern, NULL) == BRLCAD_OK) ? 0 : 1;
    rt_db_free_internal(&intern);
    return ret;
}


int main(int argc, char *argv[]) {
    struct ged *gedp;
    const struct cp_case* test_case;
    int failures = 0;
    int i;
    int dump_mode = 0;	/* useful for setting 'expected' rows */

    bu_setprogname(argv[0]);
    /* optional "dump" arg for manual testing */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "dump"))
	dump_mode = 1;

    /* set up a scratch database */
    gedp = open_test_db();
    if (!gedp) {
	bu_log("ERROR: could not open test database\n");
	return 1;
    }

    /* (first pass)passing cases: output == expected */
    for (test_case = pass_cases; test_case->label; test_case++) {
	struct bu_vls got = BU_VLS_INIT_ZERO;

	build_obj(gedp, test_case->label, test_case->label, test_case->args);

	if (dump_mode) {
	    if (snapshot(gedp, test_case->label, NULL, &got)) {
		bu_log("WARN(%s): '%s' produced no object to dump (missing/invalid args?)\n",
		       cmd_name, test_case->label);
	    } else {
		bu_log("    { \"%s\", %s%s%s, \"%s\" },\n", test_case->label,
		       test_case->args ? "\"" : "", test_case->args ? test_case->args : "NULL",
		       test_case->args ? "\"" : "", bu_vls_cstr(&got));
	    }
	} else if (snapshot(gedp, test_case->label, NULL, &got)) {
	    bu_log("FAIL(%s): could not snapshot created object\n", test_case->label);
	    failures++;
	} else if (!test_case->expected || !BU_STR_EQUAL(bu_vls_cstr(&got), test_case->expected)) {
	    bu_log("FAIL(%s):\n  exp {%s}\n  got {%s}\n",
		   test_case->label, test_case->expected ? test_case->expected : "(none)", bu_vls_cstr(&got));
	    failures++;
	}
	bu_vls_free(&got);
    }

    /* just want to dump values, we're done */
    if (dump_mode) {
	ged_close(gedp);
	return 0;
    }

    /* (second pass)failing cases: the command must reject the label and create nothing */
    for (i = 0; fail_labels[i]; i++) {
	if (build_obj(gedp, fail_labels[i], fail_labels[i], NULL) == BRLCAD_OK) {
	    bu_log("FAIL(%s): expected %s to reject this label\n", fail_labels[i], cmd_name);
	    failures++;
	}
	if (db_lookup(gedp->dbip, fail_labels[i], LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_log("FAIL(%s): object created despite rejection\n", fail_labels[i]);
	    failures++;
	}
    }

    /* (command-specific passes) e.g. make's origin/scale checks */
    failures += extra_checks(gedp);

    ged_close(gedp);

    /* (final)coverage: every ft_label librt exposes must have a case above */
    failures += check_coverage();

    if (failures) {
	bu_log("[FAIL] %s: %d failure(s)\n", cmd_name, failures);
	return 1;   /* collapse return */
    }

    /* pretty output (arrays are extern/incomplete here, so count by walking) */
    size_t num_pass = 0, num_fail = 0, num_skip = 0;
    for (test_case = pass_cases; test_case->label; test_case++)	num_pass++;
    for (i = 0; fail_labels[i]; i++) num_fail++;
    for (i = 0; skip_labels[i]; i++) num_skip++;
    bu_log("[PASS] %zu passed, %zu expected fail, %zu skipped (%s)\n", num_pass, num_fail, num_skip, cmd_name);

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
