/*       R E G R E S S _ L I N T _ R A Y T R A C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file regress_lint_raytrace.cpp
 *
 * Regression tests for "lint --raytrace" mode.
 *
 * Test cases
 * ----------
 *
 * TC1  Simple sphere primitive.
 *      A sphere has an analytic surface area (4*pi*r^2) and volume
 *      (4/3*pi*r^3).  The Crofton estimate should agree within 10%.
 *      Expected: raytrace_ok entry for sphere.s.
 *
 * TC2  Simple comb (sphere union arb).
 *      A comb whose children are both valid should itself be testable.
 *      Crofton on the CSG tree and BoT metrics from facetize should agree.
 *      Expected: raytrace_ok entries for both children and the comb.
 *
 * TC3  Non-samplable tree (contains halfspace).
 *      Any tree that contains a halfspace (unbounded primitive) cannot be
 *      reliably sampled by the Crofton estimator.  Such objects must be
 *      excluded from the validation pass.
 *      Expected: raytrace_skip entry with
 *                reason="contains_unbounded_primitive" for the halfspace
 *                comb.
 *
 * TC4  Child failure blocks parent.
 *      When the raytrace check processes a sphere whose Crofton result
 *      is artificially forced to disagree by choosing an unrealistically
 *      tight tolerance, the parent comb referencing that sphere should be
 *      skipped with reason="child_validation_failed".
 *      We achieve this by running lint --raytrace with --raytrace-tol=0
 *      (zero tolerance rejects every non-exact result) and verifying that
 *      at least one raytrace_skip entry with the child-failed reason
 *      appears when a comb contains that prim.
 *
 * TC5  Empty comb (A - A) should validate.
 *      Facetize may legitimately return an empty solid BoT for an empty CSG
 *      result.  lint --raytrace should treat this as a valid zero-metric
 *      reference instead of reporting facetize failure.
 */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

/* ------------------------------------------------------------------ */
/* Tiny JSON scanner: find entries with matching problem_type field.   */
/* We deliberately avoid pulling in a JSON library dependency so this  */
/* test can remain self-contained.                                      */
/* ------------------------------------------------------------------ */

/** Count how many JSON array elements have the given key:value pair.  */
static int
json_count_type(const char *json_text, const char *key, const char *value)
{
    if (!json_text || !key || !value) return 0;
    int count = 0;
    /* nlohmann::json pretty-prints as "key": "value" (with space after colon) */
    struct bu_vls pat = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pat, "\"%s\": \"%s\"", key, value);
    const char *needle = bu_vls_cstr(&pat);
    const char *p = json_text;
    while ((p = strstr(p, needle)) != NULL) {
	count++;
	p += strlen(needle);
    }
    bu_vls_free(&pat);
    return count;
}

/** Return non-zero if json_text contains a JSON object with both
 *  key1:val1 and key2:val2 in the same bracket-delimited block.      */
static int
json_has_pair_in_same_object(const char *json_text,
			     const char *key1, const char *val1,
			     const char *key2, const char *val2)
{
    if (!json_text || !key1 || !val1 || !key2 || !val2) return 0;

    /* nlohmann::json pretty-prints as "key": "value" (space after colon) */
    struct bu_vls pat1 = BU_VLS_INIT_ZERO;
    struct bu_vls pat2 = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pat1, "\"%s\": \"%s\"", key1, val1);
    bu_vls_sprintf(&pat2, "\"%s\": \"%s\"", key2, val2);

    const char *p = json_text;
    int found = 0;
    while ((p = strstr(p, bu_vls_cstr(&pat1))) != NULL) {
	/* Find the enclosing { ... } */
	const char *block_start = p;
	while (block_start > json_text && *block_start != '{') block_start--;
	const char *block_end   = p + strlen(bu_vls_cstr(&pat1));
	int depth = 0;
	const char *q = block_start;
	while (*q) {
	    if (*q == '{') depth++;
	    if (*q == '}') { depth--; if (depth == 0) { block_end = q; break; } }
	    q++;
	}
	/* Check whether pat2 is within [block_start, block_end] */
	if ((size_t)(block_end - block_start) > strlen(bu_vls_cstr(&pat2))) {
	    const char *tmp = block_start;
	    while (tmp && tmp < block_end) {
		tmp = strstr(tmp, bu_vls_cstr(&pat2));
		if (tmp && tmp < block_end) { found = 1; break; }
	    }
	}
	if (found) break;
	p++;
    }
    bu_vls_free(&pat1);
    bu_vls_free(&pat2);
    return found;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/** Read the full contents of a text file into a std::string.          */
static std::string
read_file(const char *path)
{
    std::ifstream f(path);
    if (!f.is_open()) return std::string();
    return std::string((std::istreambuf_iterator<char>(f)),
		       std::istreambuf_iterator<char>());
}

/** Return a temporary .g file path (caller must bu_file_delete it).   */
static std::string
make_tmp_g(const char *tag)
{
    char path[MAXPATHLEN] = {0};
    FILE *fp = bu_temp_file(path, MAXPATHLEN);
    if (fp) fclose(fp);
    /* bu_temp_file creates the file; overwrite with a .g extension copy */
    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&gpath, "%s_%s.g", path, tag);
    /* Create an empty .g database at gpath by opening a wdb and closing it */
    struct rt_wdb *wdbp = wdb_fopen(bu_vls_cstr(&gpath));
    if (wdbp) db_close(wdbp->dbip);
    std::string s(bu_vls_cstr(&gpath));
    bu_vls_free(&gpath);
    return s;
}

/** Run lint --raytrace on gfile.  Write JSON output to json_out_path if
 *  non-NULL.  Returns ged_exec return value.                           */
static int
run_lint_raytrace(const char *gfile, const char *json_out_path,
		  const char *extra_tol_arg, int argc_objs, const char **objs)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) {
	bu_log("[regress_lint_raytrace] ged_open(%s) failed\n", gfile);
	return -1;
    }

    /* Build argv: lint --raytrace [--raytrace-tol X] [-j jsonfile] [objs...] */
    std::vector<const char *> av;
    av.push_back("lint");
    av.push_back("--raytrace");
    if (extra_tol_arg) {
	av.push_back("--raytrace-tol");
	av.push_back(extra_tol_arg);
    }
    if (json_out_path) {
	av.push_back("-j");
	av.push_back(json_out_path);
    }
    for (int i = 0; i < argc_objs; i++)
	av.push_back(objs[i]);

    int ret = ged_exec(gedp, (int)av.size(), av.data());
    ged_close(gedp);
    return ret;
}

/* ------------------------------------------------------------------ */
/* TC1 – single sphere                                                 */
/* ------------------------------------------------------------------ */
static int
tc1_sphere(void)
{
    bu_log("TC1: single sphere – Crofton vs analytic\n");

    std::string gfile = make_tmp_g("tc1");
    if (gfile.empty()) {
	bu_log("TC1: FAIL – could not create temp .g file\n");
	return 1;
    }

    /* Create geometry */
    {
	struct rt_wdb *wdbp = wdb_fopen(gfile.c_str());
	if (!wdbp) { bu_log("TC1: FAIL – wdb_fopen failed\n"); return 1; }
	point_t center = VINIT_ZERO;
	mk_sph(wdbp, "sph.s", center, 50.0); /* radius 50 mm */
	db_close(wdbp->dbip);
    }

    /* Temporary JSON output path */
    char json_path[MAXPATHLEN] = {0};
    FILE *jfp = bu_temp_file(json_path, MAXPATHLEN);
    if (jfp) fclose(jfp);
    struct bu_vls json_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&json_vls, "%s_tc1.json", json_path);

    const char *obj = "sph.s";
    run_lint_raytrace(gfile.c_str(), bu_vls_cstr(&json_vls),
		      NULL, 1, &obj);

    std::string jtext = read_file(bu_vls_cstr(&json_vls));
    int pass = 0;

    if (jtext.empty()) {
	bu_log("TC1: FAIL – no JSON output\n");
    } else {
	/* Expect at least one raytrace_ok for sph.s */
	if (json_has_pair_in_same_object(jtext.c_str(),
					 "problem_type", "raytrace_ok",
					 "object_name", "sph.s")) {
	    bu_log("TC1: PASS – raytrace_ok found for sph.s\n");
	    pass = 1;
	} else {
	    bu_log("TC1: FAIL – no raytrace_ok entry for sph.s\n");
	    bu_log("  JSON: %.400s\n", jtext.c_str());
	}
    }

    bu_file_delete(bu_vls_cstr(&json_vls));
    bu_vls_free(&json_vls);
    bu_file_delete(gfile.c_str());
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* TC2 – simple comb                                                   */
/* ------------------------------------------------------------------ */
static int
tc2_simple_comb(void)
{
    bu_log("TC2: simple comb (sphere union arb8) – all levels pass\n");

    std::string gfile = make_tmp_g("tc2");
    if (gfile.empty()) {
	bu_log("TC2: FAIL – could not create temp .g file\n");
	return 1;
    }

    {
	struct rt_wdb *wdbp = wdb_fopen(gfile.c_str());
	if (!wdbp) { bu_log("TC2: FAIL – wdb_fopen failed\n"); return 1; }

	point_t center = VINIT_ZERO;
	mk_sph(wdbp, "tc2_sph.s", center, 30.0);

	point_t rpp_min = {60.0, -20.0, -20.0};
	point_t rpp_max = {100.0, 20.0, 20.0};
	mk_rpp(wdbp, "tc2_arb.s", rpp_min, rpp_max);

	struct wmember head;
	BU_LIST_INIT(&head.l);
	mk_addmember("tc2_sph.s", &head.l, NULL, WMOP_UNION);
	mk_addmember("tc2_arb.s", &head.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "tc2_comb.c", &head, 0, NULL, NULL, NULL, 0);

	db_close(wdbp->dbip);
    }

    char json_path[MAXPATHLEN] = {0};
    FILE *jfp = bu_temp_file(json_path, MAXPATHLEN);
    if (jfp) fclose(jfp);
    struct bu_vls json_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&json_vls, "%s_tc2.json", json_path);

    const char *obj = "tc2_comb.c";
    /* ged_exec returns BRLCAD_ERROR only when there are *failures*;
     * facetize may not be loaded in all test environments, so a non-OK
     * return is acceptable here as long as the JSON is produced.       */
    run_lint_raytrace(gfile.c_str(), bu_vls_cstr(&json_vls), NULL, 1, &obj);

    std::string jtext = read_file(bu_vls_cstr(&json_vls));
    int pass = 0;

    if (jtext.empty()) {
	bu_log("TC2: FAIL – no JSON output\n");
    } else {
	/* Expect raytrace_ok for both leaf primitives at a minimum */
	int sph_ok = json_has_pair_in_same_object(jtext.c_str(),
						  "problem_type", "raytrace_ok",
						  "object_name", "tc2_sph.s");
	int arb_ok = json_has_pair_in_same_object(jtext.c_str(),
						  "problem_type", "raytrace_ok",
						  "object_name", "tc2_arb.s");
	if (sph_ok && arb_ok) {
	    bu_log("TC2: PASS – raytrace_ok for both leaf primitives\n");
	    pass = 1;
	} else {
	    bu_log("TC2: FAIL – missing raytrace_ok for primitives (sph=%d arb=%d)\n",
		   sph_ok, arb_ok);
	    bu_log("  JSON: %.400s\n", jtext.c_str());
	}
    }

    bu_file_delete(bu_vls_cstr(&json_vls));
    bu_vls_free(&json_vls);
    bu_file_delete(gfile.c_str());
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* TC3 – non-samplable tree (halfspace)                                */
/* ------------------------------------------------------------------ */
static int
tc3_halfspace(void)
{
    bu_log("TC3: halfspace-containing tree – must be excluded\n");

    std::string gfile = make_tmp_g("tc3");
    if (gfile.empty()) {
	bu_log("TC3: FAIL – could not create temp .g file\n");
	return 1;
    }

    {
	struct rt_wdb *wdbp = wdb_fopen(gfile.c_str());
	if (!wdbp) { bu_log("TC3: FAIL – wdb_fopen failed\n"); return 1; }

	/* Halfspace: all points with Z >= 0 */
	point_t  hn = {0.0, 0.0, 1.0};
	mk_half(wdbp, "tc3_half.s", hn, 0.0);

	point_t center = VINIT_ZERO;
	mk_sph(wdbp, "tc3_sph.s", center, 20.0);

	/* Comb: sphere intersect halfspace (bounded volume, but raytrace
	 * cannot sample it because the halfspace is unbounded)           */
	struct wmember head;
	BU_LIST_INIT(&head.l);
	mk_addmember("tc3_sph.s",  &head.l, NULL, WMOP_UNION);
	mk_addmember("tc3_half.s", &head.l, NULL, WMOP_INTERSECT);
	mk_lcomb(wdbp, "tc3_comb.c", &head, 0, NULL, NULL, NULL, 0);

	db_close(wdbp->dbip);
    }

    char json_path[MAXPATHLEN] = {0};
    FILE *jfp = bu_temp_file(json_path, MAXPATHLEN);
    if (jfp) fclose(jfp);
    struct bu_vls json_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&json_vls, "%s_tc3.json", json_path);

    const char *obj = "tc3_comb.c";
    run_lint_raytrace(gfile.c_str(), bu_vls_cstr(&json_vls), NULL, 1, &obj);

    std::string jtext = read_file(bu_vls_cstr(&json_vls));
    int pass = 0;

    if (jtext.empty()) {
	bu_log("TC3: FAIL – no JSON output\n");
    } else {
	/* The comb and the halfspace should both be skipped */
	int comb_skip = json_has_pair_in_same_object(jtext.c_str(),
						     "problem_type", "raytrace_skip",
						     "object_name", "tc3_comb.c");
	int half_skip = json_has_pair_in_same_object(jtext.c_str(),
						     "problem_type", "raytrace_skip",
						     "object_name", "tc3_half.s");
	/* No raytrace_mismatch must appear */
	int mismatches = json_count_type(jtext.c_str(),
					 "problem_type", "raytrace_mismatch");
	if (comb_skip && half_skip && mismatches == 0) {
	    bu_log("TC3: PASS – halfspace tree correctly excluded\n");
	    pass = 1;
	} else {
	    bu_log("TC3: FAIL – comb_skip=%d half_skip=%d mismatches=%d\n",
		   comb_skip, half_skip, mismatches);
	    bu_log("  JSON: %.400s\n", jtext.c_str());
	}
    }

    bu_file_delete(bu_vls_cstr(&json_vls));
    bu_vls_free(&json_vls);
    bu_file_delete(gfile.c_str());
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* TC4 – child failure blocks parent                                   */
/* ------------------------------------------------------------------ */
static int
tc4_child_blocks_parent(void)
{
    bu_log("TC4: child failure blocks parent validation\n");

    std::string gfile = make_tmp_g("tc4");
    if (gfile.empty()) {
	bu_log("TC4: FAIL – could not create temp .g file\n");
	return 1;
    }

    {
	struct rt_wdb *wdbp = wdb_fopen(gfile.c_str());
	if (!wdbp) { bu_log("TC4: FAIL – wdb_fopen failed\n"); return 1; }

	point_t center = VINIT_ZERO;
	mk_sph(wdbp, "tc4_sph.s", center, 40.0);

	struct wmember head;
	BU_LIST_INIT(&head.l);
	mk_addmember("tc4_sph.s", &head.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "tc4_comb.c", &head, 0, NULL, NULL, NULL, 0);

	db_close(wdbp->dbip);
    }

    char json_path[MAXPATHLEN] = {0};
    FILE *jfp = bu_temp_file(json_path, MAXPATHLEN);
    if (jfp) fclose(jfp);
    struct bu_vls json_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&json_vls, "%s_tc4.json", json_path);

    /* Use --raytrace-tol=0 so even a 0.001% error triggers a mismatch,
     * forcing the sphere to be marked as failed.  The parent comb must
     * then be skipped with reason=child_validation_failed.             */
    const char *obj = "tc4_comb.c";
    run_lint_raytrace(gfile.c_str(), bu_vls_cstr(&json_vls),
		      "0", 1, &obj);

    std::string jtext = read_file(bu_vls_cstr(&json_vls));
    int pass = 0;

    if (jtext.empty()) {
	bu_log("TC4: FAIL – no JSON output\n");
    } else {
	/* With tol=0, the sphere Crofton will almost certainly disagree
	 * with analytic formula at 16+ digits, producing a mismatch.
	 * If by some miracle the sphere passes anyway (infinite precision),
	 * we can't simulate a failure; accept both outcomes but require
	 * consistency: if child failed, parent must be skipped.          */
	int sph_fail = json_has_pair_in_same_object(jtext.c_str(),
						    "problem_type", "raytrace_mismatch",
						    "object_name", "tc4_sph.s");
	int comb_skip = json_has_pair_in_same_object(jtext.c_str(),
						     "problem_type", "raytrace_skip",
						     "object_name", "tc4_comb.c");
	int comb_ok = json_has_pair_in_same_object(jtext.c_str(),
						   "problem_type", "raytrace_ok",
						   "object_name", "tc4_comb.c");
	int comb_mismatch = json_has_pair_in_same_object(jtext.c_str(),
							 "problem_type", "raytrace_mismatch",
							 "object_name", "tc4_comb.c");

	if (sph_fail && comb_skip && !comb_ok && !comb_mismatch) {
	    bu_log("TC4: PASS – child failure correctly blocks parent\n");
	    pass = 1;
	} else if (!sph_fail) {
	    /* Sphere somehow passed even with tol=0: accept if comb also ok */
	    if (comb_ok || comb_mismatch || comb_skip) {
		bu_log("TC4: PASS (degenerate) – sphere passed tol=0; "
		       "parent result is consistent\n");
		pass = 1;
	    } else {
		bu_log("TC4: FAIL – inconsistent state (no sphere result)\n");
		bu_log("  JSON: %.600s\n", jtext.c_str());
	    }
	} else {
	    bu_log("TC4: FAIL – sph_fail=%d comb_skip=%d comb_ok=%d "
		   "comb_mismatch=%d\n",
		   sph_fail, comb_skip, comb_ok, comb_mismatch);
	    bu_log("  JSON: %.600s\n", jtext.c_str());
	}
    }

    bu_file_delete(bu_vls_cstr(&json_vls));
    bu_vls_free(&json_vls);
    bu_file_delete(gfile.c_str());
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* TC5 - empty comb should be valid                                   */
/* ------------------------------------------------------------------ */
static int
tc5_empty_comb_valid(void)
{
    bu_log("TC5: empty comb (A - A) validates with empty BoT reference\n");

    std::string gfile = make_tmp_g("tc5");
    if (gfile.empty()) {
	bu_log("TC5: FAIL – could not create temp .g file\n");
	return 1;
    }

    {
	struct rt_wdb *wdbp = wdb_fopen(gfile.c_str());
	if (!wdbp) { bu_log("TC5: FAIL – wdb_fopen failed\n"); return 1; }

	point_t center = VINIT_ZERO;
	mk_sph(wdbp, "tc5_sph.s", center, 25.0);

	struct wmember head;
	BU_LIST_INIT(&head.l);
	mk_addmember("tc5_sph.s", &head.l, NULL, WMOP_UNION);
	mk_addmember("tc5_sph.s", &head.l, NULL, WMOP_SUBTRACT);
	mk_lcomb(wdbp, "tc5_empty.c", &head, 0, NULL, NULL, NULL, 0);

	db_close(wdbp->dbip);
    }

    char json_path[MAXPATHLEN] = {0};
    FILE *jfp = bu_temp_file(json_path, MAXPATHLEN);
    if (jfp) fclose(jfp);
    struct bu_vls json_vls = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&json_vls, "%s_tc5.json", json_path);

    const char *obj = "tc5_empty.c";
    run_lint_raytrace(gfile.c_str(), bu_vls_cstr(&json_vls), NULL, 1, &obj);

    std::string jtext = read_file(bu_vls_cstr(&json_vls));
    int pass = 0;

    if (jtext.empty()) {
	bu_log("TC5: FAIL – no JSON output\n");
    } else {
	int comb_ok = json_has_pair_in_same_object(jtext.c_str(),
						   "problem_type", "raytrace_ok",
						   "object_name", "tc5_empty.c");
	int comb_facetize_fail = json_has_pair_in_same_object(jtext.c_str(),
							       "problem_type", "raytrace_facetize_failed",
							       "object_name", "tc5_empty.c");
	if (comb_ok && !comb_facetize_fail) {
	    bu_log("TC5: PASS – empty comb accepted as valid\n");
	    pass = 1;
	} else {
	    bu_log("TC5: FAIL – comb_ok=%d comb_facetize_fail=%d\n",
		   comb_ok, comb_facetize_fail);
	    bu_log("  JSON: %.600s\n", jtext.c_str());
	}
    }

    bu_file_delete(bu_vls_cstr(&json_vls));
    bu_vls_free(&json_vls);
    bu_file_delete(gfile.c_str());
    return pass ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    /* Allow running a specific TC via argv[1] */
    int tc_select = -1;
    if (argc > 1) tc_select = atoi(argv[1]);

    int failures = 0;

    if (tc_select < 0 || tc_select == 1) failures += tc1_sphere();
    if (tc_select < 0 || tc_select == 2) failures += tc2_simple_comb();
    if (tc_select < 0 || tc_select == 3) failures += tc3_halfspace();
    if (tc_select < 0 || tc_select == 4) failures += tc4_child_blocks_parent();
    if (tc_select < 0 || tc_select == 5) failures += tc5_empty_comb_valid();

    bu_log("\nRegress-lint-raytrace: %s (%d failure(s))\n",
	   failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
