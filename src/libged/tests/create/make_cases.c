/*                   M A K E _ C A S E S . C
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
/** @file make_cases.c
 *
 * All 'make'-specific pieces of the create-parity test
 * 
 * TODO: right now we wholesale test -s + -o with extra_checks(). We could,
 * instead, have those be 'args' in the test case with updated 'expected' 
 * for more granular and complete checks (-s is just a smoke test and -o
 * is through the origin as-is).
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "ged.h"

#include "create_parity.h"


const char *cmd_name = "make";

/* make test cases: a primitive label and its expected snapshot, captured
 * at make's default scale (1.0) and origin (0,0,0). we pass everything back through
 * extra_checks() with -s and -o, so every row's middle 'args' field is NULL for now.
 */
const struct cp_case pass_cases[] = {
    { "sph", NULL, "ell V {0 0 0}  A {0.5 0 0}  B {0 0.5 0}  C {0 0 0.5} " },
    { "ell", NULL, "ell V {0 0 0}  A {0.5 0 0}  B {0 0.25 0}  C {0 0 0.125} " },
    { "ell1", NULL, "ell V {0 0 0}  A {0.5 0 0}  B {0 0.25 0}  C {0 0 0.25} " },
    { "rcc", NULL, "tgc V {0 0 -0.5}  H {0 0 1}  A {0.25 0 0}  B {0 0.25 0}  C {0.25 0 0}  D {0 0.25 0} " },
    { "rec", NULL, "tgc V {0 0 -0.5}  H {0 0 1}  A {0.25 0 0}  B {0 0.125 0}  C {0.25 0 0}  D {0 0.125 0} " },
    { "tec", NULL, "tgc V {0 0 -0.5}  H {0 0 1}  A {0.25 0 0}  B {0 0.125 0}  C {0.125 0 0}  D {0 0.0625 0} " },
    { "trc", NULL, "tgc V {0 0 -0.5}  H {0 0 1}  A {0.25 0 0}  B {0 0.25 0}  C {0.125 0 0}  D {0 0.125 0} " },
    { "tgc", NULL, "tgc V {0 0 -0.5}  H {0 0 1}  A {0.25 0 0}  B {0 0.125 0}  C {0.125 0 0}  D {0 0.25 0} " },
    { "arb4", NULL, "arb8 V1 {0.5 -0.5 -0.5}  V2 {0.5 0.5 -0.5}  V3 {0.5 0.5 0.5}  V4 {0.5 0.5 0.5}  V5 {-0.5 0.5 -0.5}  V6 {-0.5 0.5 -0.5}  V7 {-0.5 0.5 -0.5}  V8 {-0.5 0.5 -0.5} " },
    { "arb5", NULL, "arb8 V1 {0.5 -0.5 -0.5}  V2 {0.5 0.5 -0.5}  V3 {0.5 0.5 0.5}  V4 {0.5 -0.5 0.5}  V5 {-0.5 0 0}  V6 {-0.5 0 0}  V7 {-0.5 0 0}  V8 {-0.5 0 0} " },
    { "arb6", NULL, "arb8 V1 {0.5 -0.5 -0.5}  V2 {0.5 0.5 -0.5}  V3 {0.5 0.5 0.5}  V4 {0.5 -0.5 0.5}  V5 {-0.5 0 -0.5}  V6 {-0.5 0 -0.5}  V7 {-0.5 0 0.5}  V8 {-0.5 0 0.5} " },
    { "arb7", NULL, "arb8 V1 {0.5 -0.5 -0.25}  V2 {0.5 0.5 -0.25}  V3 {0.5 0.5 0.75}  V4 {0.5 -0.5 0.25}  V5 {-0.5 -0.5 -0.25}  V6 {-0.5 0.5 -0.25}  V7 {-0.5 0.5 0.25}  V8 {-0.5 -0.5 -0.25} " },
    { "arb8", NULL, "arb8 V1 {0.5 -0.5 -0.5}  V2 {0.5 0.5 -0.5}  V3 {0.5 0.5 0.5}  V4 {0.5 -0.5 0.5}  V5 {-0.5 -0.5 -0.5}  V6 {-0.5 0.5 -0.5}  V7 {-0.5 0.5 0.5}  V8 {-0.5 -0.5 0.5} " },
    { "rpp", NULL, "arb8 V1 {0.5 -0.5 -0.5}  V2 {0.5 0.5 -0.5}  V3 {0.5 0.5 0.5}  V4 {0.5 -0.5 0.5}  V5 {-0.5 -0.5 -0.5}  V6 {-0.5 0.5 -0.5}  V7 {-0.5 0.5 0.5}  V8 {-0.5 -0.5 0.5} " },
    { "arbn", NULL, "arbn N 8 P0 {1 0 0 0.5} P1 {-1 0 0 0.5} P2 {0 1 0 0.5} P3 {0 -1 0 0.5} P4 {0 0 1 0.5} P5 {0 0 -1 0.5} P6 {0.5773500000000000298427949 0.5773500000000000298427949 0.5773500000000000298427949 0.5} P7 {-0.5773500000000000298427949 -0.5773500000000000298427949 -0.5773500000000000298427949 0.5}" },
    { "tor", NULL, "tor V {0 0 0}  H {1 0 0}  r_a 0.4000000000000000222044605 r_h 0.1000000000000000055511151" },
    { "rpc", NULL, "rpc V {0 -0.25 -0.5}  H {0 0 1}  B {0 0.5 0}  r 0.25" },
    { "rhc", NULL, "rhc V {0 -0.25 -0.25}  H {0 0 0.5}  B {0 0.5 0}  r 0.25 c 0.1000000000000000055511151" },
    { "epa", NULL, "epa V {0 0 -0.5}  H {0 0 1}  A {0 1 0}  r_1 0.5 r_2 0.25" },
    { "ehy", NULL, "ehy V {0 0 -0.5}  H {0 0 1}  A {0 1 0}  r_1 0.5 r_2 0.25 c 0.25" },
    { "eto", NULL, "eto V {0 0 0}  N {0 0 1}  C {0.1000000000000000055511151 0 0.1000000000000000055511151}  r 0.3999999999999999666933093 r_d 0.05000000000000000277555756" },
    { "superell", NULL, "superell V {0 0 0}  A {0.5 0 0}  B {0 0.25 0}  C {0 0 0.125}  n 1 e 1" },
    { "half", NULL, "half N {0 0 1}  d 0" },
    { "ars", NULL, "ars NC 3 PPC 3 C0 { { 0 0 0 } { 0 0 0 } { 0 0 0 } } C1 { { 0 0 0.5 } { 0.5 0 0.5 } { 0.5 0.5 0.5 } } C2 { { 0 0 1 } { 0 0 1 } { 0 0 1 } }" },
    { "datum", NULL, "datum data { {point 0 0 0}}" },
    { "grip", NULL, "grip V {0 0 0}  N {1 0 0}  L 0.375" },
    { "part", NULL, "part V {0 0 -0.25}  H {0 0 0.5}  r_v 0.25 r_h 0.125" },
    { "hyp", NULL, "hyp V {0 0 -0.5}  H {0 0 1}  A {0 0.5 0}  b 0.25 bnr 0.4000000000000000222044605" },
    { "cline", NULL, "cline V {0 0 0} H {0 0 1} R 0.5 T 0.1000000000000000055511151" },
    { "sketch", NULL, "sketch V {0 0 0} A {1 0 0} B {0 1 0} VL { } SL { }" },
    { "bot", NULL, "bot mode volume orient no flags {} V { { 0 0 0 } { -0.5 0.5 -1 } { -0.5 -0.5 -1 } { 0.5 0.5 -1 }} F { { 0 3 1 } { 0 1 2 } { 0 2 3 } { 1 3 2 }}" },
    { "pipe", NULL, "pipe V0 { 0 0 -0.5 } O0 0.25 I0 0.0625 R0 0.25 V1 { 0 0 0.5 } O1 0.25 I1 0.0625 R1 0.25" },
    { "metaball", NULL, "metaball method 1 thresh 1 PL { {-1 0 0 1 1} {1 0 0 1 1} }" },
    { "nmg", NULL, "nmg V { { 0 0 0 } }" },
    { "extrude", NULL, "extrude V {0 0 0} H {0 0 1} A {1 0 0} B {0 1 0} S skt_0" },
    { "joint", NULL, "joint V {0 0 0}  RP1 {}  RP2 {}  V1 {0 1 0}  V2 {0 1 0}  A 0" },
    { NULL, NULL, NULL }
};

/* TODO/FIXME: labels 'make' currently rejects (deprecated, in-only, or non-solid), create
 * nothing, or librt ft_labels that make has no arm for
 */
const char* fail_labels[] = {
    "hf", "pg", "poly", /* deprecated */
    "binunif", "comb", "constrnt", "dsp", "ebm", "material", "nurb", "revolve", "script", "spline", "submodel", "vol", /* librt/primitives/obj_make.c denylist */
    "bspline", "annot", "hrt", /* no ft_make */
    NULL
};

/* TODO/FIXME: labels that are broken or have a reason to be excluded from the test loop.
 * ideally these should be fixed and not lazily skipped.
 */
const char* skip_labels[] = {
    "brep", /* ft_get dumps a wall of binary data, making for an ugly entry in pass_cases[] */
    "pnts", /* ft_get fails (needs a 'get' function in librt?) */
    NULL
};


static int make_obj(struct ged *gedp, const char *name, const char *label,
		    const char *scale, const char *origin) {
    /* make [-s scale] [-o origin] name label */
    const char *av[8];
    int n = 0;

    av[n++] = "make";
    if (scale) {
	av[n++] = "-s";
	av[n++] = scale;
    }
    if (origin) {
	av[n++] = "-o";
	av[n++] = origin;
    }
    av[n++] = name;
    av[n++] = label;
    av[n] = NULL;

    return ged_exec(gedp, n, av);
}


int build_obj(struct ged *gedp, const char *name, const char *label, const char *args) {
    /* ignore args until we have a use for them */
    (void)args;

    return make_obj(gedp, name, label, NULL, NULL);
}


/* Importing through a matrix is not a pure translation for these and can't be reliably strcmp'd
 * from the 'expected' output.
 * - arbn's plane normals get renormalized (make stores the literal 0.57735, not true 1/sqrt3)
 * - datum's direction vectors are transformed as points.
 * - extrude creates a new sketch (original make is skt_0, second is skt_1)
 */
static int trans_exempt(const char *label) {
    return BU_STR_EQUAL(label, "arbn") ||
	   BU_STR_EQUAL(label, "datum") ||
	   BU_STR_EQUAL(label, "extrude");
}


/* make's extra passes: -o and -s 
 * NOTE: we assume gedp has persistent creation from the original driver to check against */
int extra_checks(struct ged *gedp) {
    const struct cp_case *test_case;
    char buff[64];
    mat_t xlate;
    int failures = 0;

    /* -o origin: must produce the base object (created at the origin in create_parity.c)
     * translated exactly.  We snapshot the base THROUGH a translation matrix and 
     * compare it to the directly-offset object; a derived input->creation rather than 
     * a frozen expected string */
    MAT_IDN(xlate);
    MAT_DELTAS(xlate, 10, 20, 30);
    for (test_case = pass_cases; test_case->label; test_case++) {
	struct bu_vls off = BU_VLS_INIT_ZERO;
	struct bu_vls base_xlated = BU_VLS_INIT_ZERO;

	if (trans_exempt(test_case->label))
	    continue;

	snprintf(buff, sizeof(buff), "o_%s", test_case->label);
	make_obj(gedp, buff, test_case->label, NULL, "10 20 30");

	if (snapshot(gedp, buff, NULL, &off) ||
	    snapshot(gedp, test_case->label, xlate, &base_xlated)) {
	    bu_log("FAIL(%s): could not snapshot for origin check\n", test_case->label);
	    failures++;
	} else if (!BU_STR_EQUAL(bu_vls_cstr(&off), bu_vls_cstr(&base_xlated))) {
	    bu_log("FAIL(%s): make -o is not a pure translation of the base\n"
		   "  created  %s\n  base+xlate %s\n",
		   test_case->label, bu_vls_cstr(&off), bu_vls_cstr(&base_xlated));
	    failures++;
	}
	bu_vls_free(&off);
	bu_vls_free(&base_xlated);
    }

    /* -s scale: every primitive still builds with a non-default scale.  Scale's
     * exact geometry can't be easily checked by the matrix trick above (many prims hold
     * scalar radii their importer won't matrix-scale), so this is just a smoke
     * check */
    for (test_case = pass_cases; test_case->label; test_case++) {
	snprintf(buff, sizeof(buff), "s_%s", test_case->label);
	if (make_obj(gedp, buff, test_case->label, "2.5", NULL) != BRLCAD_OK ||
	    db_lookup(gedp->dbip, buff, LOOKUP_QUIET) == RT_DIR_NULL) {
	    bu_log("FAIL(%s): make with -s failed\n", test_case->label);
	    failures++;
	}
    }

    return failures;
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
