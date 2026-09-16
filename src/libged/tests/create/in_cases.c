/*                     I N _ C A S E S . C
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
/** @file in_cases.c
 *
 * All 'in'-specific pieces of the create-parity test
 */

#include "common.h"

#include <stddef.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "ged.h"

#include "create_parity.h"


const char *cmd_name = "in";

const struct cp_case pass_cases[] = {
    { "tor", "0 0 0 0 0 1 2 1", "tor V {0 0 0}  H {0 0 1}  r_a 2 r_h 1" },
    { "tgc", "0 0 0 0 0 2 1 0 0 0 1 0 0.5 0.5", "tgc V {0 0 0}  H {0 0 2}  A {1 0 0}  B {0 1 0}  C {0.5 0 0}  D {0 0.5 0} " },
    { "ell", "0 0 0 1 0 0 0 1 0 0 0 1", "ell V {0 0 0}  A {1 0 0}  B {0 1 0}  C {0 0 1} " },
    { "sph", "0 0 0 0.5", "ell V {0 0 0}  A {0.5 0 0}  B {0 0.5 0}  C {0 0 0.5} " },
    { "rec", "0 0 0 0 0 2 1 0 0 0 1 0", "tgc V {0 0 0}  H {0 0 2}  A {1 0 0}  B {0 1 0}  C {1 0 0}  D {0 1 0} " },
    { "arb8", "0 0 0 2 0 0 2 2 0 0 2 0 0 0 2 2 0 2 2 2 2 0 2 2", "arb8 V1 {0 0 0}  V2 {2 0 0}  V3 {2 2 0}  V4 {0 2 0}  V5 {0 0 2}  V6 {2 0 2}  V7 {2 2 2}  V8 {0 2 2} " },
    { "arbn", "6 1 0 0 1 -1 0 0 1 0 1 0 1 0 -1 0 1 0 0 1 1 0 0 -1 1", "arbn N 6 P0 {1 0 0 1} P1 {-1 0 0 1} P2 {0 1 0 1} P3 {0 -1 0 1} P4 {0 0 1 1} P5 {0 0 -1 1}" },
    { "half", "0 0 1 1", "half N {0 0 1}  d 1" },
    { "rpc", "0 0 0 0 0 1 0 1 0 0.5", "rpc V {0 0 0}  H {0 0 1}  B {0 1 0}  r 0.5" },
    { "rhc", "0 0 0 0 0 1 0 1 0 0.5 0.5", "rhc V {0 0 0}  H {0 0 1}  B {0 1 0}  r 0.5 c 0.5" },
    { "epa", "0 0 0 0 0 1 1 0 0 0.5", "epa V {0 0 0}  H {0 0 1}  A {1 0 0}  r_1 1 r_2 0.5" },
    { "ehy", "0 0 0 0 0 1 1 0 0 0.5 0.5", "ehy V {0 0 0}  H {0 0 1}  A {1 0 0}  r_1 1 r_2 0.5 c 0.5" },
    { "eto", "0 0 0 0 0 1 2 0.5 0 0.5 0.2", "eto V {0 0 0}  N {0 0 1}  C {0.5 0 0.5}  r 2 r_d 0.2000000000000000111022302" },
    { "part", "0 0 0 0 0 2 1 0.5", "part V {0 0 0}  H {0 0 2}  r_v 1 r_h 0.5" },
    { "hyp", "0 0 0 0 0 2 1 0 0 0.5 0.5", "hyp V {0 0 0}  H {0 0 2}  A {1 0 0}  b 0.5 bnr 0.5" },
    { "superell", "0 0 0 1 0 0 0 1 0 0 0 1 2 2", "superell V {0 0 0}  A {1 0 0}  B {0 1 0}  C {0 0 1}  n 2 e 2" },
    { "grip", "0 0 0 1 0 0 1", "grip V {0 0 0}  N {1 0 0}  L 1" },
    { "metaball", "1 1 1 0 0 0 1", "metaball method 1 thresh 1 PL { {0 0 0 1 1} }" },
    { "hrt", "0 0 0 1 0 0 0 1 0 0 0 1 0.5", "hrt V {0 0 0}  X {1 0 0}  Y {0 1 0}  Z {0 0 1}  d 0.5" },
    { "pipe", "2 0 0 0 0.5 1 2 0 0 2 0.5 1 2", "pipe V0 { 0 0 0 } O0 1 I0 0.5 R0 2 V1 { 0 0 2 } O1 1 I1 0.5 R1 2" },
    { "bot", "4 4 2 2 0 0 0 2 0 0 0 2 0 0 0 2 0 1 2 0 1 3 0 2 3 1 2 3", "bot mode volume orient rh flags {} V { { 0 0 0 } { 2 0 0 } { 0 2 0 } { 0 0 2 }} F { { 0 1 2 } { 0 1 3 } { 0 2 3 } { 1 2 3 }}" },
    { "ars", "3 3 0 0 0 0 0 1 1 0 1 1 1 1 0 0 2", "ars NC 3 PPC 3 C0 { { 0 0 0 } { 0 0 0 } { 0 0 0 } } C1 { { 0 0 1 } { 1 0 1 } { 1 1 1 } } C2 { { 0 0 2 } { 0 0 2 } { 0 0 2 } }" },
    { "datum", "point 0 0 0", "datum data { {point 0 0 0}}" },
    { NULL, NULL, NULL }
};

/* TODO/FIXME: labels 'in' currently rejects */
const char* fail_labels[] = {
    "poly", "pg", "bspline", "spline", "nurb", "nmg", "sketch", "cline",
    "comb", "brep", "constrnt", "hf", "notaprim", NULL
};

/* TODO/FIXME: most of these are valid 'in' types, but the pass_cases harness 
 * can't drive them as-is (need external ref / rely on another object / ...) */
const char* skip_labels[] = {
    "extrude", "revolve", "binunif", "vol", "ebm", "dsp", "material",
    "submodel", "joint", "annot", "script", "pnts", NULL
};

int build_obj(struct ged *gedp, const char *name, const char *label, const char *args) {
    /* in <label> [list of args] */
    const char *av[64];
    char *tokbuf = NULL;
    char *tokv[56];
    size_t ntok = 0, i;
    int n = 0;
    int ret;

    av[n++] = "in";
    av[n++] = name;
    av[n++] = label;

    if (args) {
	tokbuf = bu_strdup(args);
	ntok = bu_argv_from_string(tokv, sizeof(tokv) / sizeof(tokv[0]), tokbuf);
	for (i = 0; i < ntok && n < (int)(sizeof(av) / sizeof(av[0])) - 1; i++)
	    av[n++] = tokv[i];
    }
    av[n] = NULL;

    ret = ged_exec(gedp, n, av);

    if (tokbuf)
	bu_free(tokbuf, "in args copy");

    return ret;
}

/* TODO: exercise the interactive input loop
 *  1) an input can be fully defined by input - current pass_cases args form ("in sph 0 0 0 0.5")
 *  2) an input can start and then should prompt for more ("in sph 0 0 0" [prompt for r] <- 0.5)
 *  3) an input can be fully prompted ("in sph" [prompt for pt] <- 0 <- 0 <- 0 [prompt for r] <- 0.5)
 * should be able to use our pass_cases and randomly split the args [0, length] and then stdin
 */
int extra_checks(struct ged* UNUSED(gedp)) {
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
