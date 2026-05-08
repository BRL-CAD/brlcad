/*                     T E S T _ R M . C
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
/** @file tests/test_rm.c
 *
 * Regression tests for the new rm command.
 *
 * The test creates an in-memory .g database, populates it with a small
 * object hierarchy, then exercises rm options and verifies the correct
 * objects were (or were not) deleted.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "ged.h"
#include "wdb.h"


/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
	fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
	g_failures++; \
    } \
} while (0)

#define CHECK_PRESENT(gedp, name) \
    CHECK(db_lookup((gedp)->dbip, (name), LOOKUP_QUIET) != RT_DIR_NULL, \
	    "expected '" name "' to exist")

#define CHECK_ABSENT(gedp, name) \
    CHECK(db_lookup((gedp)->dbip, (name), LOOKUP_QUIET) == RT_DIR_NULL, \
	    "expected '" name "' to be gone")


/** Open a fresh temporary .g database.  Caller must ged_close(). */
static struct ged *
open_test_db(void)
{
    char tmppath[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    if (!fp)
	return NULL;
    fclose(fp);

    /* Create a fresh .g at the temp path */
    struct rt_wdb *wdbp = wdb_fopen(tmppath);
    if (!wdbp)
	return NULL;

    /* --- primitives --- */
    point_t origin = VINIT_ZERO;
    vect_t  zhat   = {0, 0, 1};
    mk_sph(wdbp, "sph.s", origin, 10.0);
    mk_sph(wdbp, "sph2.s", origin, 5.0);
    mk_sph(wdbp, "sph3.s", origin, 3.0);
    mk_tor(wdbp, "tor.s", origin, zhat, 20.0, 4.0);

    /* --- combinations ---
     *   leaf_comb.c  -> sph.s
     *   child_comb.c -> sph2.s, sph3.s
     *   parent_comb.c -> leaf_comb.c, child_comb.c
     *   shared.c    -> sph.s  (same primitive as leaf_comb.c)
     */
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("sph.s", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "leaf_comb.c", &wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("sph2.s", &wm.l, NULL, WMOP_UNION);
	(void)mk_addmember("sph3.s", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "child_comb.c", &wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("leaf_comb.c", &wm.l, NULL, WMOP_UNION);
	(void)mk_addmember("child_comb.c", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "parent_comb.c", &wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("sph.s", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "shared.c", &wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }
    /* standalone_prim.s - unreferenced primitive */
    mk_sph(wdbp, "standalone_prim.s", origin, 1.0);
    /* empty_comb.c - combination with no members */
    {
	struct bu_list emptylist;
	BU_LIST_INIT(&emptylist);
	mk_comb(wdbp, "empty_comb.c", &emptylist, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }

    db_close(wdbp->dbip);

    struct ged *gedp = ged_open("db", tmppath, 1);
    return gedp;
}


/* -----------------------------------------------------------------------
 * Test cases
 * --------------------------------------------------------------------- */

/** T1: safe delete of unreferenced primitive succeeds */
static void
test_safe_primitive_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T1: open failed\n"); return; }

    const char *av[] = {"rm", "standalone_prim.s", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret == BRLCAD_OK, "T1: rm standalone_prim.s should succeed");
    CHECK_ABSENT(gedp, "standalone_prim.s");

    ged_close(gedp);
}


/** T2: safe delete of referenced primitive fails */
static void
test_referenced_primitive_fails(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T2: open failed\n"); return; }

    const char *av[] = {"rm", "sph.s", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret != BRLCAD_OK, "T2: rm of referenced sph.s should fail");
    CHECK_PRESENT(gedp, "sph.s");

    ged_close(gedp);
}


/** T3: force delete of referenced object succeeds */
static void
test_force_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T3: open failed\n"); return; }

    const char *av[] = {"rm", "-f", "sph.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T3: rm -f sph.s should succeed");
    CHECK_ABSENT(gedp, "sph.s");

    ged_close(gedp);
}


/** T4: safe delete of empty unreferenced combination succeeds */
static void
test_empty_comb_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T4: open failed\n"); return; }

    const char *av[] = {"rm", "empty_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret == BRLCAD_OK, "T4: rm empty_comb.c should succeed");
    CHECK_ABSENT(gedp, "empty_comb.c");

    ged_close(gedp);
}


/** T5: safe delete of non-empty combination fails */
static void
test_nonempty_comb_fails(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T5: open failed\n"); return; }

    const char *av[] = {"rm", "leaf_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret != BRLCAD_OK, "T5: rm of non-empty comb should fail");
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T6: recursive safe delete removes subtree, preserves externally-ref'd */
static void
test_recursive_safe_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T6: open failed\n"); return; }

    /* child_comb.c -> sph2.s, sph3.s.  Neither is shared outside.
     * child_comb.c itself is referenced by parent_comb.c, so it stays
     * when we delete it via the subtree walk (it has external refs).
     * But sph2.s and sph3.s are only inside child_comb.c. */
    const char *av[] = {"rm", "-r", "child_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T6: rm -r child_comb.c should succeed");
    /* sph2.s and sph3.s should be gone */
    CHECK_ABSENT(gedp, "sph2.s");
    CHECK_ABSENT(gedp, "sph3.s");
    /* child_comb.c is referenced by parent_comb.c -> preserved */
    CHECK_PRESENT(gedp, "child_comb.c");

    ged_close(gedp);
}


/** T7: recursive delete - externally-referenced child is preserved */
static void
test_recursive_preserves_shared(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T7: open failed\n"); return; }

    /* leaf_comb.c -> sph.s.  shared.c also references sph.s.
     * rm -r leaf_comb.c: leaf_comb.c is not externally referenced (only
     * through parent_comb.c), so it gets deleted.  But sph.s is also
     * in shared.c, so sph.s stays. */
    /* First remove parent_comb.c so leaf_comb.c becomes unreferenced. */
    {
	const char *av2[] = {"rm", "-f", "parent_comb.c", NULL};
	ged_exec_rm(gedp, 3, av2);
    }

    const char *av[] = {"rm", "-r", "leaf_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T7: rm -r leaf_comb.c after unlinking from parent");
    CHECK_ABSENT(gedp, "leaf_comb.c");
    /* sph.s is still in shared.c => preserved */
    CHECK_PRESENT(gedp, "sph.s");

    ged_close(gedp);
}


/** T8: path instance delete */
static void
test_path_instance_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T8: open failed\n"); return; }

    /* Remove leaf_comb.c from parent_comb.c */
    const char *av[] = {"rm", "parent_comb.c/leaf_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret == BRLCAD_OK, "T8: path instance remove should succeed");
    /* leaf_comb.c object itself should still exist */
    CHECK_PRESENT(gedp, "leaf_comb.c");
    /* Verify it's no longer in parent_comb.c tree */
    {
	struct rt_db_internal intern;
	struct directory *dp = db_lookup(gedp->dbip, "parent_comb.c", LOOKUP_QUIET);
	CHECK(dp != RT_DIR_NULL, "T8: parent_comb.c should still exist");
	if (dp != RT_DIR_NULL) {
	    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) >= 0) {
		struct rt_comb_internal *comb =
		    (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);
		/* A second removal attempt should fail because the member is already gone. */
		int found = (db_tree_rm_dbleaf(&comb->tree, "leaf_comb.c", 1) >= 0);
		CHECK(!found, "T8: leaf_comb.c should be removed from parent_comb.c tree");
		rt_db_free_internal(&intern);
	    }
	}
    }

    ged_close(gedp);
}


/** T9: dry-run (-n) reports objects without deleting */
static void
test_dry_run(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T9: open failed\n"); return; }

    const char *av[] = {"rm", "-n", "standalone_prim.s", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T9: dry-run should succeed");
    CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "standalone_prim.s") != NULL,
	    "T9: dry-run output should name the object");
    /* Object must still exist */
    CHECK_PRESENT(gedp, "standalone_prim.s");

    ged_close(gedp);
}


/** T10: missing operand without -f produces error */
static void
test_missing_operand_error(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T10: open failed\n"); return; }

    const char *av[] = {"rm", "does_not_exist.s", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret != BRLCAD_OK, "T10: rm of nonexistent object should fail");

    ged_close(gedp);
}


/** T11: missing operand with -f is silently ignored */
static void
test_missing_operand_force(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T11: open failed\n"); return; }

    const char *av[] = {"rm", "-f", "does_not_exist.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T11: rm -f of nonexistent object should succeed silently");

    ged_close(gedp);
}


/** T12: glob expansion - rm *.s (force) deletes all matching objects */
static void
test_glob_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T12: open failed\n"); return; }

    /* Force-delete all primitives matching *.s */
    const char *av[] = {"rm", "-f", "*.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T12: rm -f *.s should succeed");
    CHECK_ABSENT(gedp, "sph.s");
    CHECK_ABSENT(gedp, "sph2.s");
    CHECK_ABSENT(gedp, "sph3.s");
    CHECK_ABSENT(gedp, "tor.s");
    CHECK_ABSENT(gedp, "standalone_prim.s");
    /* Combinations should be untouched */
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T13: legacy "remove" command still works (comb-child remove) */
static void
test_legacy_remove_still_works(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T13: open failed\n"); return; }

    const char *av[] = {"remove", "parent_comb.c", "leaf_comb.c", NULL};
    int ret = ged_exec_remove(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T13: legacy remove should still succeed");
    /* leaf_comb.c object exists */
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T14: long options work */
static void
test_long_options(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T14: open failed\n"); return; }

    const char *av[] = {"rm", "--dry-run", "--force", "standalone_prim.s", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 4, av);
    CHECK(ret == BRLCAD_OK, "T14: rm --dry-run --force standalone_prim.s should succeed");
    CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "standalone_prim.s") != NULL,
	    "T14: long-option dry-run output should name the object");
    CHECK_PRESENT(gedp, "standalone_prim.s");

    ged_close(gedp);
}


/* -----------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    (void)argc;

    test_safe_primitive_delete();
    test_referenced_primitive_fails();
    test_force_delete();
    test_empty_comb_delete();
    test_nonempty_comb_fails();
    test_recursive_safe_delete();
    test_recursive_preserves_shared();
    test_path_instance_delete();
    test_dry_run();
    test_missing_operand_error();
    test_missing_operand_force();
    test_glob_delete();
    test_legacy_remove_still_works();
    test_long_options();

    if (g_failures) {
	fprintf(stderr, "\nged_test_rm: %d test(s) FAILED\n", g_failures);
	return 1;
    }
    printf("ged_test_rm: all tests PASSED\n");
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
