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
#include <stdlib.h>
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

#define CHECK_CONTAINS(str, needle, msg) \
    CHECK(strstr((str), (needle)) != NULL, (msg))


static size_t
test_tree_count_named_leaves(const union tree *tp, const char *name)
{
    if (!tp || !name)
	return 0;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    return BU_STR_EQUAL(tp->tr_l.tl_name, name) ? 1 : 0;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return test_tree_count_named_leaves(tp->tr_b.tb_left, name) +
		test_tree_count_named_leaves(tp->tr_b.tb_right, name);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return test_tree_count_named_leaves(tp->tr_b.tb_left, name);
	default:
	    break;
    }

    return 0;
}


static int
test_tree_has_op(const union tree *tp, int op)
{
    if (!tp)
	return 0;

    RT_CK_TREE(tp);

    if (tp->tr_op == op)
	return 1;

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return test_tree_has_op(tp->tr_b.tb_left, op) ||
		test_tree_has_op(tp->tr_b.tb_right, op);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    return test_tree_has_op(tp->tr_b.tb_left, op);
	default:
	    break;
    }

    return 0;
}


static size_t
member_count(struct ged *gedp, const char *comb_name, const char *member_name)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct directory *dp;
    size_t count = 0;

    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB))
	return 0;

    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0)
	return 0;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    count = test_tree_count_named_leaves(comb->tree, member_name);
    rt_db_free_internal(&intern);

    return count;
}


static int
comb_has_op(struct ged *gedp, const char *comb_name, int op)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct directory *dp;
    int ret = 0;

    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB))
	return 0;

    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0)
	return 0;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    ret = test_tree_has_op(comb->tree, op);
    rt_db_free_internal(&intern);

    return ret;
}


#define CHECK_MEMBER_COUNT(gedp, comb, member, expected) \
    CHECK(member_count((gedp), (comb), (member)) == (size_t)(expected), \
	    "unexpected member count for '" comb "/" member "'")


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
    unsigned short dsp_data[4] = {0, 1, 2, 3};
    mat_t dsp_stom;
    MAT_IDN(dsp_stom);

    mk_sph(wdbp, "sph.s", origin, 10.0);
    mk_sph(wdbp, "sph2.s", origin, 5.0);
    mk_sph(wdbp, "sph3.s", origin, 3.0);
    mk_tor(wdbp, "tor.s", origin, zhat, 20.0, 4.0);
    mk_sph(wdbp, "dup_prim.s", origin, 2.0);
    mk_sph(wdbp, "bool_a.s", origin, 2.0);
    mk_sph(wdbp, "bool_b.s", origin, 1.0);
    mk_sph(wdbp, "bool_c.s", origin, 0.5);
    mk_binunif(wdbp, "dsp_data.bin", dsp_data, WDB_BINUNIF_UINT16, 4);
    mk_dsp_obj(wdbp, "dsp_child.s", "dsp_data.bin", 2, 2, dsp_stom);

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
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("dup_prim.s", &wm.l, NULL, WMOP_UNION);
	(void)mk_addmember("dup_prim.s", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "dup_comb.c", &wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
    }
    {
	struct wmember wm;
	BU_LIST_INIT(&wm.l);
	(void)mk_addmember("bool_a.s", &wm.l, NULL, WMOP_UNION);
	(void)mk_addmember("bool_b.s", &wm.l, NULL, WMOP_SUBTRACT);
	(void)mk_addmember("bool_c.s", &wm.l, NULL, WMOP_UNION);
	mk_comb(wdbp, "bool_preserve.c", &wm.l, 0, NULL, NULL, NULL,
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


/** T6: recursive safe delete fails when the subtree is externally referenced */
static void
test_recursive_safe_delete_blocks_external_refs(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T6: open failed\n"); return; }

    const char *av[] = {"rm", "-r", "child_comb.c", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret != BRLCAD_OK, "T6: rm -r child_comb.c should fail while parent_comb.c references it");
    CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "referenced outside the subtree",
	    "T6: recursive safety diagnostic should explain the external reference");
    CHECK_PRESENT(gedp, "child_comb.c");
    CHECK_PRESENT(gedp, "sph2.s");
    CHECK_PRESENT(gedp, "sph3.s");

    ged_close(gedp);
}


/** T7: recursive force deletes the top object and unshared descendants */
static void
test_recursive_force_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T7: open failed\n"); return; }

    const char *av[] = {"rm", "-rf", "child_comb.c", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T7: rm -rf child_comb.c should succeed");
    CHECK_ABSENT(gedp, "child_comb.c");
    CHECK_ABSENT(gedp, "sph2.s");
    CHECK_ABSENT(gedp, "sph3.s");
    CHECK_PRESENT(gedp, "parent_comb.c");

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
    CHECK_PRESENT(gedp, "leaf_comb.c");
    CHECK_MEMBER_COUNT(gedp, "parent_comb.c", "leaf_comb.c", 0);

    ged_close(gedp);
}


/** T9: legacy rm comb obj shape is rejected before deleting anything */
static void
test_legacy_rm_comb_obj_guard(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T9: open failed\n"); return; }

    const char *av[] = {"rm", "parent_comb.c", "leaf_comb.c", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret != BRLCAD_OK, "T9: rm comb obj should be rejected as ambiguous");
    CHECK_PRESENT(gedp, "parent_comb.c");
    CHECK_PRESENT(gedp, "leaf_comb.c");
    CHECK_MEMBER_COUNT(gedp, "parent_comb.c", "leaf_comb.c", 1);
    CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "ambiguous request",
	    "T9: diagnostic should call out the ambiguous legacy form");
    CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "comb rm --comb parent_comb.c leaf_comb.c",
	    "T9: diagnostic should point users at comb rm");
    CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "rm parent_comb.c/leaf_comb.c",
	    "T9: diagnostic should show explicit path syntax");

    ged_close(gedp);
}


/** T10: duplicate same-name path members require @N */
static void
test_duplicate_path_instance_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T10: open failed\n"); return; }

    {
	const char *av[] = {"rm", "dup_comb.c/dup_prim.s", NULL};
	bu_vls_trunc(gedp->ged_result_str, 0);
	int ret = ged_exec_rm(gedp, 2, av);
	CHECK(ret != BRLCAD_OK, "T10: ambiguous duplicate path should fail without @N");
	CHECK_MEMBER_COUNT(gedp, "dup_comb.c", "dup_prim.s", 2);
	CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "ambiguous",
		"T10: duplicate path diagnostic should explain ambiguity");
    }

    {
	const char *av[] = {"rm", "dup_comb.c/dup_prim.s@2", NULL};
	int ret = ged_exec_rm(gedp, 2, av);
	CHECK(ret == BRLCAD_OK, "T10: duplicate path with @2 should remove the second instance");
	CHECK_MEMBER_COUNT(gedp, "dup_comb.c", "dup_prim.s", 1);
	CHECK_PRESENT(gedp, "dup_prim.s");
    }

    ged_close(gedp);
}


/** T11: nested path delete edits the final parent combination */
static void
test_nested_path_instance_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T11: open failed\n"); return; }

    const char *av[] = {"rm", "parent_comb.c/child_comb.c/sph2.s", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret == BRLCAD_OK, "T11: nested path instance remove should succeed");
    CHECK_PRESENT(gedp, "sph2.s");
    CHECK_MEMBER_COUNT(gedp, "parent_comb.c", "child_comb.c", 1);
    CHECK_MEMBER_COUNT(gedp, "child_comb.c", "sph2.s", 0);
    CHECK_MEMBER_COUNT(gedp, "child_comb.c", "sph3.s", 1);

    ged_close(gedp);
}


/** T12: path removal preserves existing boolean tree operators */
static void
test_path_delete_preserves_boolean_ops(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T12: open failed\n"); return; }

    CHECK(comb_has_op(gedp, "bool_preserve.c", OP_SUBTRACT),
	    "T12: setup bool_preserve.c should contain subtraction");

    {
	const char *av[] = {"rm", "bool_preserve.c/bool_c.s", NULL};
	int ret = ged_exec_rm(gedp, 2, av);
	CHECK(ret == BRLCAD_OK, "T12: path delete from boolean comb should succeed");
    }

    CHECK_MEMBER_COUNT(gedp, "bool_preserve.c", "bool_c.s", 0);
    CHECK_MEMBER_COUNT(gedp, "bool_preserve.c", "bool_a.s", 1);
    CHECK_MEMBER_COUNT(gedp, "bool_preserve.c", "bool_b.s", 1);
    CHECK(comb_has_op(gedp, "bool_preserve.c", OP_SUBTRACT),
	    "T12: removing an unrelated member must preserve subtraction");

    ged_close(gedp);
}


/** T12: recursive safe delete fails if a descendant is shared outside */
static void
test_recursive_safe_delete_blocks_shared_descendant(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T12: open failed\n"); return; }

    {
	const char *av[] = {"rm", "-f", "parent_comb.c", NULL};
	int ret = ged_exec_rm(gedp, 3, av);
	CHECK(ret == BRLCAD_OK, "T12: setup rm -f parent_comb.c should succeed");
    }

    {
	const char *av[] = {"rm", "-r", "leaf_comb.c", NULL};
	bu_vls_trunc(gedp->ged_result_str, 0);
	int ret = ged_exec_rm(gedp, 3, av);
	CHECK(ret != BRLCAD_OK, "T12: rm -r leaf_comb.c should fail while sph.s is shared");
	CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "referenced outside the subtree",
		"T12: recursive shared descendant diagnostic should explain the block");
	CHECK_PRESENT(gedp, "leaf_comb.c");
	CHECK_PRESENT(gedp, "sph.s");
    }

    ged_close(gedp);
}


/** T13: recursive force deletes the top while preserving shared descendants */
static void
test_recursive_force_preserves_shared_descendant(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T13: open failed\n"); return; }

    {
	const char *av[] = {"rm", "-f", "parent_comb.c", NULL};
	int ret = ged_exec_rm(gedp, 3, av);
	CHECK(ret == BRLCAD_OK, "T13: setup rm -f parent_comb.c should succeed");
    }

    {
	const char *av[] = {"rm", "-rf", "leaf_comb.c", NULL};
	int ret = ged_exec_rm(gedp, 3, av);
	CHECK(ret == BRLCAD_OK, "T13: rm -rf leaf_comb.c should succeed");
	CHECK_ABSENT(gedp, "leaf_comb.c");
	CHECK_PRESENT(gedp, "sph.s");
	CHECK_PRESENT(gedp, "shared.c");
    }

    ged_close(gedp);
}


/** T14: primitive support-object dependencies count as children */
static void
test_primitive_dependency_blocks_plain_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T14: open failed\n"); return; }

    const char *av[] = {"rm", "dsp_child.s", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret != BRLCAD_OK, "T14: rm dsp_child.s should fail because it references dsp_data.bin");
    CHECK_CONTAINS(bu_vls_cstr(gedp->ged_result_str), "has child references",
	    "T14: DSP dependency diagnostic should describe child references");
    CHECK_PRESENT(gedp, "dsp_child.s");
    CHECK_PRESENT(gedp, "dsp_data.bin");

    ged_close(gedp);
}


/** T15: recursive delete removes unshared primitive support objects */
static void
test_recursive_primitive_dependency_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T15: open failed\n"); return; }

    const char *av[] = {"rm", "-r", "dsp_child.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T15: rm -r dsp_child.s should remove the DSP and binunif data");
    CHECK_ABSENT(gedp, "dsp_child.s");
    CHECK_ABSENT(gedp, "dsp_data.bin");

    ged_close(gedp);
}


/** T16: dry-run (-n) reports objects without deleting */
static void
test_dry_run(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T16: open failed\n"); return; }

    const char *av[] = {"rm", "-n", "standalone_prim.s", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T16: dry-run should succeed");
    CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "standalone_prim.s") != NULL,
	    "T16: dry-run output should name the object");
    /* Object must still exist */
    CHECK_PRESENT(gedp, "standalone_prim.s");

    ged_close(gedp);
}


/** T17: missing operand without -f produces error */
static void
test_missing_operand_error(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T17: open failed\n"); return; }

    const char *av[] = {"rm", "does_not_exist.s", NULL};
    int ret = ged_exec_rm(gedp, 2, av);
    CHECK(ret != BRLCAD_OK, "T17: rm of nonexistent object should fail");

    ged_close(gedp);
}


/** T18: missing operand with -f is silently ignored */
static void
test_missing_operand_force(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T18: open failed\n"); return; }

    const char *av[] = {"rm", "-f", "does_not_exist.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T18: rm -f of nonexistent object should succeed silently");

    ged_close(gedp);
}


/** T19: glob expansion - rm *.s (force) deletes all matching objects */
static void
test_glob_delete(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T19: open failed\n"); return; }

    /* Force-delete all primitives matching *.s */
    const char *av[] = {"rm", "-f", "*.s", NULL};
    int ret = ged_exec_rm(gedp, 3, av);
    CHECK(ret == BRLCAD_OK, "T19: rm -f *.s should succeed");
    CHECK_ABSENT(gedp, "sph.s");
    CHECK_ABSENT(gedp, "sph2.s");
    CHECK_ABSENT(gedp, "sph3.s");
    CHECK_ABSENT(gedp, "tor.s");
    CHECK_ABSENT(gedp, "dup_prim.s");
    CHECK_ABSENT(gedp, "bool_a.s");
    CHECK_ABSENT(gedp, "bool_b.s");
    CHECK_ABSENT(gedp, "bool_c.s");
    CHECK_ABSENT(gedp, "dsp_child.s");
    CHECK_ABSENT(gedp, "standalone_prim.s");
    /* Combinations should be untouched */
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T20: comb rm removes members without deleting the child object */
static void
test_comb_rm(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T20: open failed\n"); return; }

    const char *av[] = {"comb", "rm", "--comb", "parent_comb.c", "leaf_comb.c", NULL};
    int ret = ged_exec_comb(gedp, 5, av);
    CHECK(ret == BRLCAD_OK, "T20: comb rm should succeed");
    CHECK_MEMBER_COUNT(gedp, "parent_comb.c", "leaf_comb.c", 0);
    /* leaf_comb.c object exists */
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T21: short -C identifies a combination whose name is a boolean operator */
static void
test_comb_rm_operator_named_combination(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T21: open failed\n"); return; }

    const char *create[] = {"comb", "u", "u", "leaf_comb.c", NULL};
    int ret = ged_exec_comb(gedp, 4, create);
    CHECK(ret == BRLCAD_OK, "T21: should create combination named u");
    CHECK_MEMBER_COUNT(gedp, "u", "leaf_comb.c", 1);

    const char *remove[] = {"comb", "rm", "-C", "u", "leaf_comb.c", NULL};
    ret = ged_exec_comb(gedp, 5, remove);
    CHECK(ret == BRLCAD_OK, "T21: comb rm -C should remove from combination named u");
    CHECK_MEMBER_COUNT(gedp, "u", "leaf_comb.c", 0);
    CHECK_PRESENT(gedp, "leaf_comb.c");

    ged_close(gedp);
}


/** T22: long options work */
static void
test_long_options(void)
{
    struct ged *gedp = open_test_db();
    if (!gedp) { fprintf(stderr, "SKIP T22: open failed\n"); return; }

    const char *av[] = {"rm", "--dry-run", "--force", "standalone_prim.s", NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    int ret = ged_exec_rm(gedp, 4, av);
    CHECK(ret == BRLCAD_OK, "T22: rm --dry-run --force standalone_prim.s should succeed");
    CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "standalone_prim.s") != NULL,
	    "T22: long-option dry-run output should name the object");
    CHECK_PRESENT(gedp, "standalone_prim.s");

    ged_close(gedp);
}


/* -----------------------------------------------------------------------
 * Opt-in performance measurements
 * --------------------------------------------------------------------- */

static struct ged *
open_perf_db(size_t primitive_count)
{
    char tmppath[MAXPATHLEN] = {0};
    point_t origin = VINIT_ZERO;
    const size_t group_size = 1000;
    size_t group_count = (primitive_count + group_size - 1) / group_size;
    FILE *fp = bu_temp_file(tmppath, MAXPATHLEN);
    struct rt_wdb *wdbp;
    struct wmember root_wm;
    size_t i;
    size_t g;

    if (!fp)
	return NULL;
    fclose(fp);

    wdbp = wdb_fopen(tmppath);
    if (!wdbp)
	return NULL;

    BU_LIST_INIT(&root_wm.l);
    for (g = 0; g < group_count; g++) {
	struct wmember group_wm;
	char group_name[64] = {0};
	size_t start = g * group_size;
	size_t end = start + group_size;

	if (end > primitive_count)
	    end = primitive_count;

	BU_LIST_INIT(&group_wm.l);
	for (i = start; i < end; i++) {
	    char name[64] = {0};
	    (void)snprintf(name, sizeof(name), "perf_%06zu.s", i);
	    mk_sph(wdbp, name, origin, 1.0);
	    (void)mk_addmember(name, &group_wm.l, NULL, WMOP_UNION);
	}

	(void)snprintf(group_name, sizeof(group_name), "perf_group_%06zu.c", g);
	mk_comb(wdbp, group_name, &group_wm.l, 0, NULL, NULL, NULL,
		0, 0, 0, 0, 0, 0, 0);
	(void)mk_addmember(group_name, &root_wm.l, NULL, WMOP_UNION);
    }
    mk_sph(wdbp, "perf_standalone.s", origin, 1.0);
    mk_comb(wdbp, "perf_root.c", &root_wm.l, 0, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0);

    db_close(wdbp->dbip);
    return ged_open("db", tmppath, 1);
}


static double
run_timed_rm(struct ged *gedp, int argc, const char *argv[], int *retp)
{
    int64_t start = bu_gettime();

    bu_vls_trunc(gedp->ged_result_str, 0);
    *retp = ged_exec_rm(gedp, argc, argv);

    return (double)(bu_gettime() - start) / 1000000.0;
}


static int
run_perf(size_t primitive_count)
{
    const size_t group_size = 1000;
    size_t group_count = (primitive_count + group_size - 1) / group_size;
    struct ged *gedp = open_perf_db(primitive_count);
    double standalone_sec;
    double path_sec;
    double child_fail_sec;
    double recursive_sec;
    int ret;

    if (!gedp) {
	fprintf(stderr, "ged_test_rm --perf: failed to create test database\n");
	return 1;
    }

    {
	const char *av[] = {"rm", "-n", "perf_standalone.s", NULL};
	standalone_sec = run_timed_rm(gedp, 3, av, &ret);
	CHECK(ret == BRLCAD_OK, "perf: rm -n perf_standalone.s should succeed");
    }

    {
	const char *av[] = {"rm", "-n", "perf_root.c/perf_group_000000.c/perf_000000.s", NULL};
	path_sec = run_timed_rm(gedp, 3, av, &ret);
	CHECK(ret == BRLCAD_OK, "perf: rm -n perf_root.c/perf_group_000000.c/perf_000000.s should succeed");
    }

    {
	const char *av[] = {"rm", "perf_root.c", NULL};
	child_fail_sec = run_timed_rm(gedp, 2, av, &ret);
	CHECK(ret != BRLCAD_OK, "perf: rm perf_root.c should fail because it has children");
    }

    {
	const char *av[] = {"rm", "-n", "-r", "perf_root.c", NULL};
	recursive_sec = run_timed_rm(gedp, 4, av, &ret);
	CHECK(ret == BRLCAD_OK, "perf: rm -n -r perf_root.c should succeed");
    }

    printf("ged_test_rm_perf objects=%zu groups=%zu total_dir_entries=%zu\n",
	    primitive_count, group_count, primitive_count + group_count + 2);
    printf("rm -n perf_standalone.s seconds=%.6f\n", standalone_sec);
    printf("rm -n perf_root.c/perf_group_000000.c/perf_000000.s seconds=%.6f\n", path_sec);
    printf("rm perf_root.c seconds=%.6f\n", child_fail_sec);
    printf("rm -n -r perf_root.c seconds=%.6f\n", recursive_sec);

    ged_close(gedp);

    if (g_failures) {
	fprintf(stderr, "\nged_test_rm --perf: %d check(s) FAILED\n", g_failures);
	return 1;
    }

    return 0;
}


/* -----------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc > 1 && BU_STR_EQUAL(argv[1], "--perf")) {
	size_t primitive_count = 10000;
	if (argc > 2)
	    primitive_count = (size_t)strtoull(argv[2], NULL, 10);
	if (!primitive_count)
	    primitive_count = 10000;
	return run_perf(primitive_count);
    }

    test_safe_primitive_delete();
    test_referenced_primitive_fails();
    test_force_delete();
    test_empty_comb_delete();
    test_nonempty_comb_fails();
    test_recursive_safe_delete_blocks_external_refs();
    test_recursive_force_delete();
    test_path_instance_delete();
    test_legacy_rm_comb_obj_guard();
    test_duplicate_path_instance_delete();
    test_nested_path_instance_delete();
    test_path_delete_preserves_boolean_ops();
    test_recursive_safe_delete_blocks_shared_descendant();
    test_recursive_force_preserves_shared_descendant();
    test_primitive_dependency_blocks_plain_delete();
    test_recursive_primitive_dependency_delete();
    test_dry_run();
    test_missing_operand_error();
    test_missing_operand_force();
    test_glob_delete();
    test_comb_rm();
    test_comb_rm_operator_named_combination();
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
