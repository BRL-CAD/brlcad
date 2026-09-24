/*                    P A R A M _ M A T R I X . C P P
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
/** @file param_matrix.cpp
 *
 * Check displayed local-unit values independently of geometry
 * reconstructed by the matching parameter reader, then check repair
 * results on geometries at both unit scales.
 */

#include "common.h"

#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"

static const fastf_t inch_to_mm = 25.4;

struct param_case {
    const char *name;
    int type;
    void *original;
    void *parsed;
    size_t parsed_size;
    bool (*same)(const void *, const void *);
    const char *mm_expected;
    const char *inch_expected;
    const char *unitless;
    bool exact_text;
};

struct invalid_param_case {
    const char *name;
    const char *text;
};

static bool
same_ell(const void *a, const void *b)
{
    const struct rt_ell_internal *x = (const struct rt_ell_internal *)a;
    const struct rt_ell_internal *y = (const struct rt_ell_internal *)b;
    return VNEAR_EQUAL(x->v, y->v, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->a, y->a, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->b, y->b, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->c, y->c, VUNITIZE_TOL);
}

static bool
same_part(const void *a, const void *b)
{
    const struct rt_part_internal *x = (const struct rt_part_internal *)a;
    const struct rt_part_internal *y = (const struct rt_part_internal *)b;
    return VNEAR_EQUAL(x->part_V, y->part_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->part_H, y->part_H, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->part_vrad, y->part_vrad, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->part_hrad, y->part_hrad, VUNITIZE_TOL) &&
	x->part_type == y->part_type;
}

static bool
same_tor(const void *a, const void *b)
{
    const struct rt_tor_internal *x = (const struct rt_tor_internal *)a;
    const struct rt_tor_internal *y = (const struct rt_tor_internal *)b;
    return VNEAR_EQUAL(x->v, y->v, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->h, y->h, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->r_a, y->r_a, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->r_h, y->r_h, VUNITIZE_TOL);
}

static bool
same_half(const void *a, const void *b)
{
    const struct rt_half_internal *x = (const struct rt_half_internal *)a;
    const struct rt_half_internal *y = (const struct rt_half_internal *)b;
    return HNEAR_EQUAL(x->eqn, y->eqn, VUNITIZE_TOL);
}

static bool
same_grip(const void *a, const void *b)
{
    const struct rt_grip_internal *x = (const struct rt_grip_internal *)a;
    const struct rt_grip_internal *y = (const struct rt_grip_internal *)b;
    return VNEAR_EQUAL(x->center, y->center, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->normal, y->normal, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->mag, y->mag, VUNITIZE_TOL);
}

static bool
same_eto(const void *a, const void *b)
{
    const struct rt_eto_internal *x = (const struct rt_eto_internal *)a;
    const struct rt_eto_internal *y = (const struct rt_eto_internal *)b;
    return VNEAR_EQUAL(x->eto_V, y->eto_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->eto_N, y->eto_N, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->eto_C, y->eto_C, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->eto_rd, y->eto_rd, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->eto_r, y->eto_r, VUNITIZE_TOL);
}

static bool
same_rpc(const void *a, const void *b)
{
    const struct rt_rpc_internal *x = (const struct rt_rpc_internal *)a;
    const struct rt_rpc_internal *y = (const struct rt_rpc_internal *)b;
    return VNEAR_EQUAL(x->rpc_V, y->rpc_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->rpc_H, y->rpc_H, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->rpc_B, y->rpc_B, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->rpc_r, y->rpc_r, VUNITIZE_TOL);
}

static bool
same_rhc(const void *a, const void *b)
{
    const struct rt_rhc_internal *x = (const struct rt_rhc_internal *)a;
    const struct rt_rhc_internal *y = (const struct rt_rhc_internal *)b;
    return VNEAR_EQUAL(x->rhc_V, y->rhc_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->rhc_H, y->rhc_H, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->rhc_B, y->rhc_B, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->rhc_r, y->rhc_r, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->rhc_c, y->rhc_c, VUNITIZE_TOL);
}

static bool
same_superell(const void *a, const void *b)
{
    const struct rt_superell_internal *x = (const struct rt_superell_internal *)a;
    const struct rt_superell_internal *y = (const struct rt_superell_internal *)b;
    return VNEAR_EQUAL(x->v, y->v, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->a, y->a, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->b, y->b, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->c, y->c, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->n, y->n, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->e, y->e, VUNITIZE_TOL);
}

static bool
same_epa(const void *a, const void *b)
{
    const struct rt_epa_internal *x = (const struct rt_epa_internal *)a;
    const struct rt_epa_internal *y = (const struct rt_epa_internal *)b;
    return VNEAR_EQUAL(x->epa_V, y->epa_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->epa_H, y->epa_H, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->epa_Au, y->epa_Au, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->epa_r1, y->epa_r1, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->epa_r2, y->epa_r2, VUNITIZE_TOL);
}

static bool
same_ehy(const void *a, const void *b)
{
    const struct rt_ehy_internal *x = (const struct rt_ehy_internal *)a;
    const struct rt_ehy_internal *y = (const struct rt_ehy_internal *)b;
    return VNEAR_EQUAL(x->ehy_V, y->ehy_V, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->ehy_H, y->ehy_H, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->ehy_Au, y->ehy_Au, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->ehy_r1, y->ehy_r1, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->ehy_r2, y->ehy_r2, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->ehy_c, y->ehy_c, VUNITIZE_TOL);
}

static bool
same_hyp(const void *a, const void *b)
{
    const struct rt_hyp_internal *x = (const struct rt_hyp_internal *)a;
    const struct rt_hyp_internal *y = (const struct rt_hyp_internal *)b;
    return VNEAR_EQUAL(x->hyp_Vi, y->hyp_Vi, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->hyp_Hi, y->hyp_Hi, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->hyp_A, y->hyp_A, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->hyp_b, y->hyp_b, VUNITIZE_TOL) &&
	NEAR_EQUAL(x->hyp_bnr, y->hyp_bnr, VUNITIZE_TOL);
}

static bool
same_tgc(const void *a, const void *b)
{
    const struct rt_tgc_internal *x = (const struct rt_tgc_internal *)a;
    const struct rt_tgc_internal *y = (const struct rt_tgc_internal *)b;
    return VNEAR_EQUAL(x->v, y->v, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->h, y->h, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->a, y->a, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->b, y->b, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->c, y->c, VUNITIZE_TOL) &&
	VNEAR_EQUAL(x->d, y->d, VUNITIZE_TOL);
}

static bool
same_datum(const void *a, const void *b)
{
    const struct rt_datum_internal *x = (const struct rt_datum_internal *)a;
    const struct rt_datum_internal *y = (const struct rt_datum_internal *)b;
    while (x && y) {
	if (!VNEAR_EQUAL(x->pnt, y->pnt, VUNITIZE_TOL) ||
	    !VNEAR_EQUAL(x->dir, y->dir, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(x->w, y->w, VUNITIZE_TOL))
	    return false;
	x = x->next;
	y = y->next;
    }
    return !x && !y;
}

static bool
same_arb(const void *a, const void *b)
{
    const struct rt_arb_internal *x = (const struct rt_arb_internal *)a;
    const struct rt_arb_internal *y = (const struct rt_arb_internal *)b;
    for (int i = 0; i < 8; ++i) {
	if (!VNEAR_EQUAL(x->pt[i], y->pt[i], VUNITIZE_TOL))
	    return false;
    }
    return true;
}

static void
init_arb_cube(struct rt_arb_internal *arb, fastf_t origin, fastf_t edge)
{
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    VSET(arb->pt[0], origin, origin, origin);
    VSET(arb->pt[1], origin + edge, origin, origin);
    VSET(arb->pt[2], origin + edge, origin + edge, origin);
    VSET(arb->pt[3], origin, origin + edge, origin);
    VSET(arb->pt[4], origin, origin, origin + edge);
    VSET(arb->pt[5], origin + edge, origin, origin + edge);
    VSET(arb->pt[6], origin + edge, origin + edge, origin + edge);
    VSET(arb->pt[7], origin, origin + edge, origin + edge);
}

static bool
same_arb_vertices(const struct rt_arb_internal *arb,
		  const struct rt_arb_internal *expected)
{
    for (int i = 0; i < 8; ++i) {
	int matches = 0;
	for (int j = 0; j < 8; ++j)
	    matches += VNEAR_EQUAL(arb->pt[j], expected->pt[i], VUNITIZE_TOL);
	if (matches != 1)
	    return false;
    }
    return true;
}

static int
run_case(const struct param_case *test, fastf_t local2base, const char *unit)
{
    const struct rt_edit_functab *edit = &EDOBJ[test->type];
    struct rt_db_internal ip;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bu_vls text = BU_VLS_INIT_ZERO;
    const char *expected = EQUAL(local2base, 1.0) ?
	test->mm_expected : test->inch_expected;
    int failed = 0;

    if (!edit->ft_write_params || !edit->ft_read_params) {
	bu_log("%s\tparams\t%s\tmissing callback\n", test->name, unit);
	return 1;
    }

    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = test->type;
    ip.idb_meth = &OBJ[test->type];
    ip.idb_ptr = test->original;
    edit->ft_write_params(&text, &ip, &tol, 1.0 / local2base);
    const char *output = bu_vls_addr(&text);
    if ((test->exact_text ? bu_strcmp(output, expected) != 0 : !strstr(output, expected)) ||
	(test->unitless && !strstr(output, test->unitless))) {
	bu_log("%s\tparams\t%s\twrong text:\n%s", test->name, unit, output);
	failed = 1;
    }

    ip.idb_ptr = test->parsed;
    if (edit->ft_read_params(&ip, output, &tol, local2base) != BRLCAD_OK ||
	!test->same(test->original, test->parsed)) {
	bu_log("%s\tparams\t%s\twrong reconstructed geometry\n", test->name, unit);
	failed = 1;
    }
    if (!failed)
	bu_log("%s\tparams\t%s\tpass\n", test->name, unit);
    bu_vls_free(&text);
    return failed;
}

static int
run_ell_param_cases(fastf_t local2base, const char *unit)
{
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 10, 0, 0);
    VSET(ell.a, 20, 0, 0);
    VSET(ell.b, 0, 30, 0);
    VSET(ell.c, 0, 0, 40);

    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_ELL;
    ip.idb_meth = &OBJ[ID_ELL];
    ip.idb_ptr = &ell;
    struct rt_ell_internal expected = ell;
    VSET(expected.v, local2base, 0, 0);
    VSET(expected.a, 2 * local2base, 0, 0);
    VSET(expected.b, 0, 3 * local2base, 0);
    VSET(expected.c, 0, 0, 4 * local2base);
    const char *plain_crlf =
	"1 0 0\r\n2 0 0\r\n0 3 0\r\n0 0 4\r\n";
    bool passed = EDOBJ[ID_ELL].ft_read_params(&ip, plain_crlf, NULL,
	local2base) == BRLCAD_OK && same_ell(&ell, &expected);

    const char *bad_params[] = {
	"Vertex: 2 0 0\nA: 2 0 0\nB: 0 3 0\nC: invalid\n",
	"Vertex: 2 0 0\nA: 2 0 0\nB: 0 3 0\nD: 0 0 4\n",
	"Vertex: 2 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 nan\n",
	"Vertex: 2 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\nextra\n"
    };
    for (const char *text : bad_params) {
	ell = expected;
	int result = EDOBJ[ID_ELL].ft_read_params(&ip, text, NULL,
	    local2base);
	passed = result == BRLCAD_ERROR && same_ell(&ell, &expected) &&
	    passed;
    }
    if (!EQUAL(local2base, 1.0)) {
	ell = expected;
	const char *overflow =
	    "Vertex: 1e308 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\n";
	int result = EDOBJ[ID_ELL].ft_read_params(&ip, overflow, NULL,
	    local2base);
	passed = result == BRLCAD_ERROR && same_ell(&ell, &expected) &&
	    passed;
    }
    bu_log("ell\tparams malformed\t%s\t%s\n", unit,
	passed ? "pass" : "fail");
    return passed ? 0 : 1;
}

static int
run_invalid_param_cases(const char *name, int type, void *geometry,
			size_t size, bool (*same)(const void *, const void *),
			const struct invalid_param_case *cases, size_t count,
			fastf_t local2base, const char *unit)
{
    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = type;
    ip.idb_meth = &OBJ[type];
    ip.idb_ptr = geometry;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    void *before = bu_malloc(size, "parameter matrix input snapshot");
    memcpy(before, geometry, size);

    int failures = 0;
    for (size_t i = 0; i < count; ++i) {
	memcpy(geometry, before, size);
	int result = EDOBJ[type].ft_read_params(&ip, cases[i].text, &tol,
	    local2base);
	bool unchanged = ip.idb_ptr == geometry && same(geometry, before);
	bool passed = result == BRLCAD_ERROR && unchanged;
	if (!passed)
	    bu_log("%s parameter rejection: status=%d unchanged=%d\n",
		name, result, unchanged);
	bu_log("%s\tparams reject\t%s\t%s\t%s\n", name, unit,
	    passed ? "pass" : "fail", cases[i].name);
	failures += !passed;
    }
    memcpy(geometry, before, size);
    bu_free(before, "parameter matrix input snapshot");
    return failures;
}

static int
check_invalid_repair_option(int type, void *geometry, size_t size,
			    const char *unit)
{
    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = type;
    ip.idb_meth = &OBJ[type];
    ip.idb_ptr = geometry;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    void *before = bu_malloc(size, "repair option geometry snapshot");
    memcpy(before, geometry, size);
    const char *argv[] = {"--not-a-repair-option"};
    int result = EDOBJ[type].ft_repair ?
	EDOBJ[type].ft_repair(&log, &ip, &tol, 1, argv) : BRLCAD_OK;
    bool ok = result == -1 && ip.idb_ptr == geometry &&
	strstr(bu_vls_cstr(&log), "Invalid repair options") &&
	memcmp(before, geometry, size) == 0;
    bu_log("%s\trepair invalid option\t%s\t%s\n",
	OBJ[type].ft_name, unit, ok ? "pass" : "fail");
    bu_free(before, "repair option geometry snapshot");
    bu_vls_free(&log);
    return ok ? 0 : 1;
}

static int
run_repairs(fastf_t length, const char *unit)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_db_internal ip;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    int failures = 0;

    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, length, 0, 0);
    VSET(ell.a, 2 * length, 0, 0);
    VSET(ell.b, length, 3 * length, 0);
    VSET(ell.c, 0, 0, 4 * length);
    fastf_t b_length = MAGNITUDE(ell.b);
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_ELL;
    ip.idb_meth = &OBJ[ID_ELL];
    ip.idb_ptr = &ell;
    failures += check_invalid_repair_option(ID_ELL, &ell, sizeof(ell), unit);
    if (!EDOBJ[ID_ELL].ft_repair ||
	EDOBJ[ID_ELL].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!NEAR_ZERO(VDOT(ell.a, ell.b) / (MAGNITUDE(ell.a) * MAGNITUDE(ell.b)), tol.perp) ||
	!NEAR_EQUAL(MAGNITUDE(ell.b), b_length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(ell.v[X], length, VUNITIZE_TOL)) {
	bu_log("ell\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("ell\trepair\t%s\tpass\n", unit);
    }
    bu_vls_trunc(&log, 0);

    struct rt_epa_internal epa = {};
    epa.epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa.epa_V, length, 0, 0);
    VSET(epa.epa_H, 0, 0, 3 * length);
    VSET(epa.epa_Au, 1, 0, 1);
    epa.epa_r1 = 2 * length;
    epa.epa_r2 = length;
    ip.idb_type = ID_EPA;
    ip.idb_meth = &OBJ[ID_EPA];
    ip.idb_ptr = &epa;
    failures += check_invalid_repair_option(ID_EPA, &epa, sizeof(epa), unit);
    vect_t expected_axis;
    VSET(expected_axis, 1, 0, 0);
    if (!EDOBJ[ID_EPA].ft_repair ||
	EDOBJ[ID_EPA].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!VNEAR_EQUAL(epa.epa_Au, expected_axis, VUNITIZE_TOL) ||
	!NEAR_EQUAL(epa.epa_H[Z], 3 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(epa.epa_r1, 2 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(epa.epa_r2, length, VUNITIZE_TOL)) {
	bu_log("epa\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("epa\trepair\t%s\tpass\n", unit);
    }
    bu_vls_trunc(&log, 0);

    struct rt_ehy_internal ehy = {};
    ehy.ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy.ehy_V, length, 0, 0);
    VSET(ehy.ehy_H, 0, 0, 3 * length);
    VSET(ehy.ehy_Au, 1, 0, 1);
    ehy.ehy_r1 = 2 * length;
    ehy.ehy_r2 = length;
    ehy.ehy_c = length;
    ip.idb_type = ID_EHY;
    ip.idb_meth = &OBJ[ID_EHY];
    ip.idb_ptr = &ehy;
    failures += check_invalid_repair_option(ID_EHY, &ehy, sizeof(ehy), unit);
    if (!EDOBJ[ID_EHY].ft_repair ||
	EDOBJ[ID_EHY].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!VNEAR_EQUAL(ehy.ehy_Au, expected_axis, VUNITIZE_TOL) ||
	!NEAR_EQUAL(ehy.ehy_H[Z], 3 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(ehy.ehy_r1, 2 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(ehy.ehy_r2, length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(ehy.ehy_c, length, VUNITIZE_TOL)) {
	bu_log("ehy\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("ehy\trepair\t%s\tpass\n", unit);
    }

    bu_vls_trunc(&log, 0);
    struct rt_tgc_internal tgc = {};
    tgc.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(tgc.v, length, 0, 0);
    VSET(tgc.a, 2 * length, 0, 0);
    VSET(tgc.b, 0, 2 * length, 0);
    VSET(tgc.c, length, 0, 0);
    VSET(tgc.d, 0, length, 0);
    ip.idb_type = ID_TGC;
    ip.idb_meth = &OBJ[ID_TGC];
    ip.idb_ptr = &tgc;
    failures += check_invalid_repair_option(ID_TGC, &tgc, sizeof(tgc), unit);
    if (!EDOBJ[ID_TGC].ft_repair ||
	EDOBJ[ID_TGC].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!NEAR_EQUAL(tgc.h[Z], 2 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(tgc.v[X], length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(MAGNITUDE(tgc.a), 2 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(MAGNITUDE(tgc.d), length, VUNITIZE_TOL)) {
	bu_log("tgc\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("tgc\trepair\t%s\tpass\n", unit);
    }

    bu_vls_trunc(&log, 0);
    struct rt_eto_internal eto = {};
    eto.eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(eto.eto_V, length, 0, 0);
    VSET(eto.eto_N, 0, 0, 2);
    VSET(eto.eto_C, 2 * length, 0, length);
    eto.eto_r = 3 * length;
    eto.eto_rd = length;
    fastf_t c_length = MAGNITUDE(eto.eto_C);
    ip.idb_type = ID_ETO;
    ip.idb_meth = &OBJ[ID_ETO];
    ip.idb_ptr = &eto;
    failures += check_invalid_repair_option(ID_ETO, &eto, sizeof(eto), unit);
    if (!EDOBJ[ID_ETO].ft_repair ||
	EDOBJ[ID_ETO].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!NEAR_EQUAL(MAGNITUDE(eto.eto_N), 1.0, VUNITIZE_TOL) ||
	!NEAR_ZERO(VDOT(eto.eto_N, eto.eto_C), VUNITIZE_TOL) ||
	!NEAR_EQUAL(MAGNITUDE(eto.eto_C), c_length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(eto.eto_r, 3 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(eto.eto_rd, length, VUNITIZE_TOL)) {
	bu_log("eto\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("eto\trepair\t%s\tpass\n", unit);
    }

    bu_vls_trunc(&log, 0);
    struct rt_rpc_internal rpc = {};
    rpc.rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc.rpc_V, length, 0, 0);
    VSET(rpc.rpc_H, 0, 0, 3 * length);
    VSET(rpc.rpc_B, 0, 2 * length, length);
    rpc.rpc_r = length;
    fastf_t rpc_b_length = MAGNITUDE(rpc.rpc_B);
    ip.idb_type = ID_RPC;
    ip.idb_meth = &OBJ[ID_RPC];
    ip.idb_ptr = &rpc;
    failures += check_invalid_repair_option(ID_RPC, &rpc, sizeof(rpc), unit);
    if (!EDOBJ[ID_RPC].ft_repair ||
	EDOBJ[ID_RPC].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!NEAR_ZERO(VDOT(rpc.rpc_B, rpc.rpc_H) /
	    (MAGNITUDE(rpc.rpc_B) * MAGNITUDE(rpc.rpc_H)), tol.perp) ||
	!NEAR_EQUAL(MAGNITUDE(rpc.rpc_B), rpc_b_length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(rpc.rpc_r, length, VUNITIZE_TOL)) {
	bu_log("rpc\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("rpc\trepair\t%s\tpass\n", unit);
    }

    bu_vls_trunc(&log, 0);
    struct rt_rhc_internal rhc = {};
    rhc.rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc.rhc_V, length, 0, 0);
    VSET(rhc.rhc_H, 0, 0, 3 * length);
    VSET(rhc.rhc_B, 0, 2 * length, length);
    rhc.rhc_r = 2 * length;
    rhc.rhc_c = length;
    fastf_t rhc_b_length = MAGNITUDE(rhc.rhc_B);
    ip.idb_type = ID_RHC;
    ip.idb_meth = &OBJ[ID_RHC];
    ip.idb_ptr = &rhc;
    failures += check_invalid_repair_option(ID_RHC, &rhc, sizeof(rhc), unit);
    if (!EDOBJ[ID_RHC].ft_repair ||
	EDOBJ[ID_RHC].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	!NEAR_ZERO(VDOT(rhc.rhc_B, rhc.rhc_H) /
	    (MAGNITUDE(rhc.rhc_B) * MAGNITUDE(rhc.rhc_H)), tol.perp) ||
	!NEAR_EQUAL(MAGNITUDE(rhc.rhc_B), rhc_b_length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(rhc.rhc_r, 2 * length, VUNITIZE_TOL) ||
	!NEAR_EQUAL(rhc.rhc_c, length, VUNITIZE_TOL)) {
	bu_log("rhc\trepair\t%s\twrong geometry\n", unit);
	++failures;
    } else {
	bu_log("rhc\trepair\t%s\tpass\n", unit);
    }

    struct rt_arb_internal arb = {};
    struct rt_arb_internal expected_arb = {};
    init_arb_cube(&expected_arb, 0, length);
    const int scrambled[] = {0, 2, 1, 3, 6, 4, 7, 5};
    arb.magic = RT_ARB_INTERNAL_MAGIC;
    for (int i = 0; i < 8; ++i)
	VMOVE(arb.pt[i], expected_arb.pt[scrambled[i]]);
    ip.idb_type = ID_ARB8;
    ip.idb_meth = &OBJ[ID_ARB8];
    ip.idb_ptr = &arb;
    failures += check_invalid_repair_option(ID_ARB8, &arb, sizeof(arb), unit);
    int issues = 0;
    bu_vls_trunc(&log, 0);
    if (!EDOBJ[ID_ARB8].ft_repair ||
	EDOBJ[ID_ARB8].ft_repair(&log, &ip, &tol, 0, NULL) != BRLCAD_OK ||
	rt_arb_validate(NULL, &arb, &tol, &issues) != BRLCAD_OK ||
	issues || !same_arb_vertices(&arb, &expected_arb)) {
	bu_log("arb8\trepair\t%s\twrong geometry or invalid result\n", unit);
	++failures;
    } else {
	bu_log("arb8\trepair\t%s\tpass\n", unit);
    }

    bu_vls_free(&log);
    return failures;
}

int
rt_edit_test_param_matrix(void)
{
    struct rt_ell_internal ell = {};
    struct rt_ell_internal ell_read = {};
    ell.magic = ell_read.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, inch_to_mm, 0, 0);
    VSET(ell.a, 2 * inch_to_mm, 0, 0);
    VSET(ell.b, 0, 3 * inch_to_mm, 0);
    VSET(ell.c, 0, 0, 4 * inch_to_mm);

    struct rt_tor_internal tor = {};
    struct rt_tor_internal tor_read = {};
    tor.magic = tor_read.magic = RT_TOR_INTERNAL_MAGIC;
    VSET(tor.v, inch_to_mm, 0, 0);
    VSET(tor.h, 0, 0, 1);
    tor.r_a = 2 * inch_to_mm;
    tor.r_h = inch_to_mm;

    struct rt_half_internal half = {};
    struct rt_half_internal half_read = {};
    half.magic = half_read.magic = RT_HALF_INTERNAL_MAGIC;
    VSET(half.eqn, 0, 0, 1);
    half.eqn[W] = inch_to_mm;

    struct rt_grip_internal grip = {};
    struct rt_grip_internal grip_read = {};
    grip.magic = grip_read.magic = RT_GRIP_INTERNAL_MAGIC;
    VSET(grip.center, inch_to_mm, 0, 0);
    VSET(grip.normal, 0, 0, 1);
    grip.mag = 2 * inch_to_mm;

    struct rt_part_internal part = {};
    struct rt_part_internal part_read = {};
    part.part_magic = part_read.part_magic = RT_PART_INTERNAL_MAGIC;
    VSET(part.part_V, inch_to_mm, 0, 0);
    VSET(part.part_H, 0, 0, 2 * inch_to_mm);
    part.part_vrad = 2 * inch_to_mm;
    part.part_hrad = inch_to_mm;
    part.part_type = part_read.part_type = RT_PARTICLE_TYPE_CONE;

    struct rt_superell_internal superell = {};
    struct rt_superell_internal superell_read = {};
    superell.magic = superell_read.magic = RT_SUPERELL_INTERNAL_MAGIC;
    VSET(superell.v, inch_to_mm, 0, 0);
    VSET(superell.a, 2 * inch_to_mm, 0, 0);
    VSET(superell.b, 0, 3 * inch_to_mm, 0);
    VSET(superell.c, 0, 0, 4 * inch_to_mm);
    superell.n = 1.5;
    superell.e = 0.75;

    struct rt_epa_internal epa = {};
    struct rt_epa_internal epa_read = {};
    epa.epa_magic = epa_read.epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa.epa_V, inch_to_mm, 0, 0);
    VSET(epa.epa_H, 0, 0, 3 * inch_to_mm);
    VSET(epa.epa_Au, 1, 0, 0);
    epa.epa_r1 = 2 * inch_to_mm;
    epa.epa_r2 = inch_to_mm;

    struct rt_ehy_internal ehy = {};
    struct rt_ehy_internal ehy_read = {};
    ehy.ehy_magic = ehy_read.ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy.ehy_V, inch_to_mm, 0, 0);
    VSET(ehy.ehy_H, 0, 0, 3 * inch_to_mm);
    VSET(ehy.ehy_Au, 1, 0, 0);
    ehy.ehy_r1 = 2 * inch_to_mm;
    ehy.ehy_r2 = inch_to_mm;
    ehy.ehy_c = inch_to_mm;

    struct rt_eto_internal eto = {};
    struct rt_eto_internal eto_read = {};
    eto.eto_magic = eto_read.eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(eto.eto_V, inch_to_mm, 0, 0);
    VSET(eto.eto_N, 0, 0, 1);
    VSET(eto.eto_C, 2 * inch_to_mm, 0, 0);
    eto.eto_rd = inch_to_mm;
    eto.eto_r = 3 * inch_to_mm;

    struct rt_rpc_internal rpc = {};
    struct rt_rpc_internal rpc_read = {};
    rpc.rpc_magic = rpc_read.rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc.rpc_V, inch_to_mm, 0, 0);
    VSET(rpc.rpc_H, 0, 0, 3 * inch_to_mm);
    VSET(rpc.rpc_B, 0, 2 * inch_to_mm, 0);
    rpc.rpc_r = inch_to_mm;

    struct rt_rhc_internal rhc = {};
    struct rt_rhc_internal rhc_read = {};
    rhc.rhc_magic = rhc_read.rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc.rhc_V, inch_to_mm, 0, 0);
    VSET(rhc.rhc_H, 0, 0, 3 * inch_to_mm);
    VSET(rhc.rhc_B, 0, 2 * inch_to_mm, 0);
    rhc.rhc_r = 2 * inch_to_mm;
    rhc.rhc_c = inch_to_mm;

    struct rt_hyp_internal hyp = {};
    struct rt_hyp_internal hyp_read = {};
    hyp.hyp_magic = hyp_read.hyp_magic = RT_HYP_INTERNAL_MAGIC;
    VSET(hyp.hyp_Vi, inch_to_mm, 0, 0);
    VSET(hyp.hyp_Hi, 0, 0, 3 * inch_to_mm);
    VSET(hyp.hyp_A, 2 * inch_to_mm, 0, 0);
    hyp.hyp_b = inch_to_mm;
    hyp.hyp_bnr = 0.5;

    struct rt_tgc_internal tgc = {};
    struct rt_tgc_internal tgc_read = {};
    tgc.magic = tgc_read.magic = RT_TGC_INTERNAL_MAGIC;
    VSET(tgc.v, inch_to_mm, 0, 0);
    VSET(tgc.h, 0, 0, 3 * inch_to_mm);
    VSET(tgc.a, 2 * inch_to_mm, 0, 0);
    VSET(tgc.b, 0, 2 * inch_to_mm, 0);
    VSET(tgc.c, inch_to_mm, 0, 0);
    VSET(tgc.d, 0, inch_to_mm, 0);

    struct rt_datum_internal datum_point = {};
    struct rt_datum_internal datum_point_read = {};
    datum_point.magic = datum_point_read.magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(datum_point.pnt, inch_to_mm, 0, 0);

    struct rt_datum_internal datum_line = {};
    struct rt_datum_internal datum_line_read = {};
    datum_line.magic = datum_line_read.magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(datum_line.pnt, inch_to_mm, 0, 0);
    VSET(datum_line.dir, 0, 2 * inch_to_mm, 0);

    struct rt_datum_internal datum_plane = {};
    struct rt_datum_internal datum_plane_read = {};
    datum_plane.magic = datum_plane_read.magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(datum_plane.pnt, inch_to_mm, 0, 0);
    VSET(datum_plane.dir, 0, 0, 2 * inch_to_mm);
    datum_plane.w = 1.5;

    struct rt_datum_internal datum_chain[3] = {};
    struct rt_datum_internal datum_chain_read[3] = {};
    for (int i = 0; i < 3; ++i) {
	datum_chain[i].magic = RT_DATUM_INTERNAL_MAGIC;
	datum_chain_read[i].magic = RT_DATUM_INTERNAL_MAGIC;
	if (i < 2) {
	    datum_chain[i].next = &datum_chain[i + 1];
	    datum_chain_read[i].next = &datum_chain_read[i + 1];
	}
    }
    VSET(datum_chain[0].pnt, inch_to_mm, 0, 0);
    VSET(datum_chain[1].pnt, 0, inch_to_mm, 0);
    VSET(datum_chain[1].dir, 0, 2 * inch_to_mm, 0);
    VSET(datum_chain[2].pnt, 0, 0, inch_to_mm);
    VSET(datum_chain[2].dir, 0, 0, 2 * inch_to_mm);
    datum_chain[2].w = 1.5;

    struct rt_arb_internal arb = {};
    struct rt_arb_internal arb_read = {};
    init_arb_cube(&arb, inch_to_mm, 2 * inch_to_mm);
    init_arb_cube(&arb_read, 10, 20);

    const struct param_case cases[] = {
	{"ell", ID_ELL, &ell, &ell_read, sizeof(ell_read), same_ell,
	    "A: 50.800000000 0.000000000 0.000000000\n",
	    "A: 2.000000000 0.000000000 0.000000000\n", NULL, false},
	{"tor", ID_TOR, &tor, &tor_read, sizeof(tor_read), same_tor,
	    "Vertex: 25.400000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "radius_1: 50.800000000\n"
	    "radius_2: 25.400000000\n",
	    "Vertex: 1.000000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "radius_1: 2.000000000\n"
	    "radius_2: 1.000000000\n", NULL, true},
	{"half", ID_HALF, &half, &half_read, sizeof(half_read), same_half,
	    "Plane: 0.000000000 0.000000000 1.000000000 25.400000000\n",
	    "Plane: 0.000000000 0.000000000 1.000000000 1.000000000\n", NULL, true},
	{"grip", ID_GRIP, &grip, &grip_read, sizeof(grip_read), same_grip,
	    "Center: 25.400000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "Magnitude: 50.800000000\n",
	    "Center: 1.000000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "Magnitude: 2.000000000\n", NULL, true},
	{"part", ID_PARTICLE, &part, &part_read, sizeof(part_read), same_part,
	    "v radius: 50.800000000\n", "v radius: 2.000000000\n", NULL, false},
	{"superell", ID_SUPERELL, &superell, &superell_read, sizeof(superell_read), same_superell,
	    "A: 50.800000000 0.000000000 0.000000000\n",
	    "A: 2.000000000 0.000000000 0.000000000\n",
	    "<n, e>: <1.500000000, 0.750000000>\n", false},
	{"epa", ID_EPA, &epa, &epa_read, sizeof(epa_read), same_epa,
	    "Semi-major length: 50.800000000\n",
	    "Semi-major length: 2.000000000\n",
	    "Semi-major axis: 1.000000000 0.000000000 0.000000000\n", false},
	{"ehy", ID_EHY, &ehy, &ehy_read, sizeof(ehy_read), same_ehy,
	    "Dist to asymptotes: 25.400000000\n",
	    "Dist to asymptotes: 1.000000000\n",
	    "Semi-major axis: 1.000000000 0.000000000 0.000000000\n", false},
	{"eto", ID_ETO, &eto, &eto_read, sizeof(eto_read), same_eto,
	    "Vertex: 25.400000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "Semi-major axis: 50.800000000 0.000000000 0.000000000\n"
	    "Semi-minor length: 25.400000000\n"
	    "Radius of rotation: 76.200000000\n",
	    "Vertex: 1.000000000 0.000000000 0.000000000\n"
	    "Normal: 0.000000000 0.000000000 1.000000000\n"
	    "Semi-major axis: 2.000000000 0.000000000 0.000000000\n"
	    "Semi-minor length: 1.000000000\n"
	    "Radius of rotation: 3.000000000\n", NULL, true},
	{"rpc", ID_RPC, &rpc, &rpc_read, sizeof(rpc_read), same_rpc,
	    "Vertex: 25.400000000 0.000000000 0.000000000\n"
	    "Height: 0.000000000 0.000000000 76.200000000\n"
	    "Breadth: 0.000000000 50.800000000 0.000000000\n"
	    "Half-width: 25.400000000\n",
	    "Vertex: 1.000000000 0.000000000 0.000000000\n"
	    "Height: 0.000000000 0.000000000 3.000000000\n"
	    "Breadth: 0.000000000 2.000000000 0.000000000\n"
	    "Half-width: 1.000000000\n", NULL, true},
	{"rhc", ID_RHC, &rhc, &rhc_read, sizeof(rhc_read), same_rhc,
	    "Vertex: 25.400000000 0.000000000 0.000000000\n"
	    "Height: 0.000000000 0.000000000 76.200000000\n"
	    "Breadth: 0.000000000 50.800000000 0.000000000\n"
	    "Half-width: 50.800000000\n"
	    "Dist_to_asymptotes: 25.400000000\n",
	    "Vertex: 1.000000000 0.000000000 0.000000000\n"
	    "Height: 0.000000000 0.000000000 3.000000000\n"
	    "Breadth: 0.000000000 2.000000000 0.000000000\n"
	    "Half-width: 2.000000000\n"
	    "Dist_to_asymptotes: 1.000000000\n", NULL, true},
	{"hyp", ID_HYP, &hyp, &hyp_read, sizeof(hyp_read), same_hyp,
	    "Semi-minor length: 25.400000000\n",
	    "Semi-minor length: 1.000000000\n",
	    "Ratio of Neck to Base: 0.500000000\n", false},
	{"tgc", ID_TGC, &tgc, &tgc_read, sizeof(tgc_read), same_tgc,
	    "A: 50.800000000 0.000000000 0.000000000\n",
	    "A: 2.000000000 0.000000000 0.000000000\n", NULL, false},
	{"datum-point", ID_DATUM, &datum_point, &datum_point_read, sizeof(datum_point_read), same_datum,
	    "Point: 25.400000000 0.000000000 0.000000000\n",
	    "Point: 1.000000000 0.000000000 0.000000000\n", NULL, false},
	{"datum-line", ID_DATUM, &datum_line, &datum_line_read, sizeof(datum_line_read), same_datum,
	    "Line: 25.400000000 0.000000000 0.000000000",
	    "Line: 1.000000000 0.000000000 0.000000000",
	    "(dir)\n", false},
	{"datum-plane", ID_DATUM, &datum_plane, &datum_plane_read, sizeof(datum_plane_read), same_datum,
	    "Plane: 25.400000000 0.000000000 0.000000000",
	    "Plane: 1.000000000 0.000000000 0.000000000",
	    "(scale)\n", false},
	{"datum-chain", ID_DATUM, datum_chain, datum_chain_read, sizeof(datum_chain_read), same_datum,
	    "Point: 25.400000000 0.000000000 0.000000000\n",
	    "Point: 1.000000000 0.000000000 0.000000000\n",
	    "(scale)\n", false},
	{"arb8", ID_ARB8, &arb, &arb_read, sizeof(arb_read), same_arb,
	    "pt[1]: 25.400000000 25.400000000 25.400000000\n",
	    "pt[1]: 1.000000000 1.000000000 1.000000000\n", NULL, false}
    };

    const fastf_t param_factors[] = {1.0, inch_to_mm};
    const char *param_units[] = {"mm", "in"};
    int failures = 0;
    bu_log("primitive\toperation\tunits\tresult\n");
    for (const struct param_case &test : cases) {
	void *initial = bu_malloc(test.parsed_size, "parameter matrix initial state");
	memcpy(initial, test.parsed, test.parsed_size);
	for (size_t unit = 0;
	    unit < sizeof(param_factors) / sizeof(param_factors[0]); ++unit) {
	    memcpy(test.parsed, initial, test.parsed_size);
	    failures += run_case(&test, param_factors[unit], param_units[unit]);
	}
	bu_free(initial, "parameter matrix initial state");
    }
    const struct invalid_param_case bad_tor[] = {
	{"invalid radius", "Vertex: 3 0 0\nNormal: 0 0 1\nradius_1: 2\nradius_2: invalid\n"},
	{"wrong radius label", "Vertex: 3 0 0\nNormal: 0 0 1\nradius_1: 2\nradius_3: 1\n"},
	{"nonfinite normal", "Vertex: 3 0 0\nNormal: 0 0 nan\nradius_1: 2\nradius_2: 1\n"},
	{"zero normal", "Vertex: 3 0 0\nNormal: 0 0 0\nradius_1: 2\nradius_2: 1\n"},
	{"negative radius", "Vertex: 3 0 0\nNormal: 0 0 1\nradius_1: -2\nradius_2: 1\n"},
	{"extra line", "Vertex: 3 0 0\nNormal: 0 0 1\nradius_1: 2\nradius_2: 1\nextra\n"}
    };
    const struct invalid_param_case bad_grip[] = {
	{"invalid magnitude", "Center: 3 0 0\nNormal: 0 0 1\nMagnitude: invalid\n"},
	{"wrong magnitude label", "Center: 3 0 0\nNormal: 0 0 1\nLength: 2\n"},
	{"nonfinite normal", "Center: 3 0 0\nNormal: 0 0 nan\nMagnitude: 2\n"},
	{"extra line", "Center: 3 0 0\nNormal: 0 0 1\nMagnitude: 2\nextra\n"}
    };
    const struct invalid_param_case bad_half[] = {
	{"missing distance", "Plane: 0 0 1\n"},
	{"invalid distance", "Plane: 0 0 1 invalid\n"},
	{"wrong label", "Normal: 0 0 1 2\n"},
	{"nonfinite normal", "Plane: 0 0 nan 2\n"},
	{"zero normal", "Plane: 0 0 0 2\n"},
	{"extra value", "Plane: 0 0 1 2 3\n"},
	{"extra line", "Plane: 0 0 1 2\nextra\n"}
    };
    const struct invalid_param_case bad_part[] = {
	{"missing h radius", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\n"},
	{"invalid h radius", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\nh radius: invalid\n"},
	{"wrong radius label", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\nradius: 1\n"},
	{"nonfinite height", "Vertex: 1 0 0\nHeight: 0 0 nan\nv radius: 2\nh radius: 1\n"},
	{"negative radius", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: -2\nh radius: 1\n"},
	{"extra line", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\nh radius: 1\nextra\n"}
    };
    for (size_t i = 0; i < sizeof(param_factors) / sizeof(param_factors[0]); ++i) {
	struct rt_db_internal plain_ip;
	RT_DB_INTERNAL_INIT(&plain_ip);
	plain_ip.idb_type = ID_TOR;
	plain_ip.idb_meth = &OBJ[ID_TOR];
	plain_ip.idb_ptr = &tor_read;
	struct rt_tor_internal expected_tor = tor;
	VSET(expected_tor.v, param_factors[i], 0, 0);
	VSET(expected_tor.h, 0, 0, 1);
	expected_tor.r_a = 2 * param_factors[i];
	expected_tor.r_h = param_factors[i];
	tor_read = tor;
	int result = EDOBJ[ID_TOR].ft_read_params(&plain_ip,
	    "1 0 0\r\n0 0 2\r\n2\r\n1\r\n", NULL, param_factors[i]);
	bool passed = result == BRLCAD_OK && same_tor(&tor_read,
	    &expected_tor);
	bu_log("tor\tparams plain CRLF\t%s\t%s\n", param_units[i],
	    passed ? "pass" : "fail");
	failures += !passed;

	plain_ip.idb_type = ID_GRIP;
	plain_ip.idb_meth = &OBJ[ID_GRIP];
	plain_ip.idb_ptr = &grip_read;
	struct rt_grip_internal expected_grip = grip;
	VSET(expected_grip.center, param_factors[i], 0, 0);
	VSET(expected_grip.normal, 0, 0, 2);
	expected_grip.mag = 3 * param_factors[i];
	grip_read = grip;
	result = EDOBJ[ID_GRIP].ft_read_params(&plain_ip,
	    "1 0 0\r\n0 0 2\r\n3\r\n", NULL, param_factors[i]);
	passed = result == BRLCAD_OK && same_grip(&grip_read,
	    &expected_grip);
	bu_log("grip\tparams plain CRLF\t%s\t%s\n", param_units[i],
	    passed ? "pass" : "fail");
	failures += !passed;

	plain_ip.idb_type = ID_HALF;
	plain_ip.idb_meth = &OBJ[ID_HALF];
	plain_ip.idb_ptr = &half_read;
	struct rt_half_internal expected_half = half;
	VSET(expected_half.eqn, 0, 0, 1);
	expected_half.eqn[W] = 2 * param_factors[i];
	half_read = half;
	result = EDOBJ[ID_HALF].ft_read_params(&plain_ip,
	    "0 0 2 4\r\n", NULL, param_factors[i]);
	passed = result == BRLCAD_OK && same_half(&half_read,
	    &expected_half);
	bu_log("half\tparams plain CRLF\t%s\t%s\n", param_units[i],
	    passed ? "pass" : "fail");
	failures += !passed;

	plain_ip.idb_type = ID_PARTICLE;
	plain_ip.idb_meth = &OBJ[ID_PARTICLE];
	plain_ip.idb_ptr = &part_read;
	struct rt_part_internal expected_part = part;
	VSET(expected_part.part_V, param_factors[i], 0, 0);
	VSET(expected_part.part_H, 0, 0, 2 * param_factors[i]);
	expected_part.part_vrad = 2 * param_factors[i];
	expected_part.part_hrad = param_factors[i];
	part_read = part;
	result = EDOBJ[ID_PARTICLE].ft_read_params(&plain_ip,
	    "1 0 0\r\n0 0 2\r\n2\r\n1\r\n", NULL, param_factors[i]);
	passed = result == BRLCAD_OK && same_part(&part_read,
	    &expected_part);
	bu_log("part\tparams plain CRLF\t%s\t%s\n", param_units[i],
	    passed ? "pass" : "fail");
	failures += !passed;

	failures += run_invalid_param_cases("tor", ID_TOR, &tor_read,
	    sizeof(tor_read), same_tor, bad_tor,
	    sizeof(bad_tor) / sizeof(bad_tor[0]), param_factors[i],
	    param_units[i]);
	failures += run_invalid_param_cases("grip", ID_GRIP, &grip_read,
	    sizeof(grip_read), same_grip, bad_grip,
	    sizeof(bad_grip) / sizeof(bad_grip[0]), param_factors[i],
	    param_units[i]);
	failures += run_invalid_param_cases("half", ID_HALF, &half_read,
	    sizeof(half_read), same_half, bad_half,
	    sizeof(bad_half) / sizeof(bad_half[0]), param_factors[i],
	    param_units[i]);
	failures += run_invalid_param_cases("part", ID_PARTICLE, &part_read,
	    sizeof(part_read), same_part, bad_part,
	    sizeof(bad_part) / sizeof(bad_part[0]), param_factors[i],
	    param_units[i]);
	if (!EQUAL(param_factors[i], 1.0)) {
	    const struct invalid_param_case tor_overflow[] = {
		{"unit conversion overflow", "Vertex: 3 0 0\nNormal: 0 0 1\nradius_1: 1e308\nradius_2: 1\n"}
	    };
	    const struct invalid_param_case grip_overflow[] = {
		{"unit conversion overflow", "Center: 3 0 0\nNormal: 0 0 1\nMagnitude: 1e308\n"}
	    };
	    const struct invalid_param_case half_overflow[] = {
		{"unit conversion overflow", "Plane: 0 0 1 1e308\n"}
	    };
	    const struct invalid_param_case part_overflow[] = {
		{"unit conversion overflow", "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 1e308\nh radius: 1\n"}
	    };
	    failures += run_invalid_param_cases("tor", ID_TOR, &tor_read,
		sizeof(tor_read), same_tor, tor_overflow,
		sizeof(tor_overflow) / sizeof(tor_overflow[0]),
		param_factors[i], param_units[i]);
	    failures += run_invalid_param_cases("grip", ID_GRIP, &grip_read,
		sizeof(grip_read), same_grip, grip_overflow,
		sizeof(grip_overflow) / sizeof(grip_overflow[0]),
		param_factors[i], param_units[i]);
	    failures += run_invalid_param_cases("half", ID_HALF, &half_read,
		sizeof(half_read), same_half, half_overflow,
		sizeof(half_overflow) / sizeof(half_overflow[0]),
		param_factors[i], param_units[i]);
	    failures += run_invalid_param_cases("part", ID_PARTICLE, &part_read,
		sizeof(part_read), same_part, part_overflow,
		sizeof(part_overflow) / sizeof(part_overflow[0]),
		param_factors[i], param_units[i]);
	}
	struct rt_part_internal sphere = part;
	VSETALL(sphere.part_H, 0);
	sphere.part_vrad = sphere.part_hrad = 2 * param_factors[i];
	sphere.part_type = RT_PARTICLE_TYPE_SPHERE;
	struct rt_part_internal expected_sphere = sphere;
	VSET(expected_sphere.part_V, param_factors[i], 0, 0);
	part_read = part;
	plain_ip.idb_type = ID_PARTICLE;
	plain_ip.idb_meth = &OBJ[ID_PARTICLE];
	plain_ip.idb_ptr = &part_read;
	int sphere_result = EDOBJ[ID_PARTICLE].ft_read_params(&plain_ip,
	    "Vertex: 1 0 0\nHeight: 0 0 0\nv radius: 2\nh radius: 1\n",
	    NULL, param_factors[i]);
	bool sphere_passed = sphere_result == BRLCAD_OK &&
	    same_part(&part_read, &expected_sphere);
	bu_log("part\tparams cone to sphere\t%s\t%s\n", param_units[i],
	    sphere_passed ? "pass" : "fail");
	failures += !sphere_passed;

	struct rt_part_internal expected_cone = expected_part;
	part_read = expected_sphere;
	int cone_result = EDOBJ[ID_PARTICLE].ft_read_params(&plain_ip,
	    "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\nh radius: 1\n",
	    NULL, param_factors[i]);
	bool cone_passed = cone_result == BRLCAD_OK &&
	    same_part(&part_read, &expected_cone);
	bu_log("part\tparams sphere to cone\t%s\t%s\n", param_units[i],
	    cone_passed ? "pass" : "fail");
	failures += !cone_passed;

	struct rt_part_internal expected_cylinder = expected_part;
	expected_cylinder.part_hrad = 2 * param_factors[i];
	expected_cylinder.part_type = RT_PARTICLE_TYPE_CYLINDER;
	part_read = expected_part;
	int cylinder_result = EDOBJ[ID_PARTICLE].ft_read_params(&plain_ip,
	    "Vertex: 1 0 0\nHeight: 0 0 2\nv radius: 2\nh radius: 2\n",
	    NULL, param_factors[i]);
	bool cylinder_passed = cylinder_result == BRLCAD_OK &&
	    same_part(&part_read, &expected_cylinder);
	bu_log("part\tparams cone to cylinder\t%s\t%s\n", param_units[i],
	    cylinder_passed ? "pass" : "fail");
	failures += !cylinder_passed;
    }
    for (size_t unit = 0;
	unit < sizeof(param_factors) / sizeof(param_factors[0]); ++unit)
	failures += run_ell_param_cases(param_factors[unit], param_units[unit]);
    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_SUPERELL;
    ip.idb_meth = &OBJ[ID_SUPERELL];
    ip.idb_ptr = &superell_read;
    const struct {
	const char *name;
	const char *text;
    } bad_superell[] = {
	{"invalid exponents", "Vertex: 3 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\n<n, e>: invalid\n"},
	{"incomplete exponents", "Vertex: 3 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\n<n, e>: <1.5, 0.75\n"},
	{"nonfinite exponents", "Vertex: 3 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\n<n, e>: <nan, 0.75>\n"},
	{"extra line", "Vertex: 3 0 0\nA: 2 0 0\nB: 0 3 0\nC: 0 0 4\n<n, e>: <1.5, 0.75>\nextra\n"}
    };
    for (size_t unit = 0;
	unit < sizeof(param_factors) / sizeof(param_factors[0]); ++unit) {
	for (const auto &bad : bad_superell) {
	    superell_read = superell;
	    int result = EDOBJ[ID_SUPERELL].ft_read_params(&ip, bad.text,
		NULL, param_factors[unit]);
	    bool passed = result == BRLCAD_ERROR &&
		same_superell(&superell_read, &superell);
	    bu_log("superell\tparams-invalid\t%s\t%s\t%s\n",
		param_units[unit], passed ? "pass" : "fail", bad.name);
	    failures += !passed;
	}
    }
    for (size_t unit = 0;
	unit < sizeof(param_factors) / sizeof(param_factors[0]); ++unit)
	failures += run_repairs(param_factors[unit], param_units[unit]);
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
