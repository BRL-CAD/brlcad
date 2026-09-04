/*               C A N O N I C A L I Z E . C
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

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/func.h"


#define TEST_EPSILON 1.0e-9


static int
near_value(fastf_t a, fastf_t b)
{
    fastf_t scale = fmax(1.0, fmax(fabs(a), fabs(b)));

    return fabs(a - b) <= TEST_EPSILON * scale;
}


static int
near_vector(const fastf_t *a, const fastf_t *b)
{
    return near_value(a[X], b[X]) &&
	near_value(a[Y], b[Y]) &&
	near_value(a[Z], b[Z]);
}


static void
make_ell(struct rt_db_internal *intern, int type, const point_t center,
	 const vect_t a, const vect_t b, const vect_t c)
{
    struct rt_ell_internal *ell;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = type;
    intern->idb_meth = &OBJ[type];
    BU_ALLOC(intern->idb_ptr, struct rt_ell_internal);
    ell = (struct rt_ell_internal *)intern->idb_ptr;
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell->v, center);
    VMOVE(ell->a, a);
    VMOVE(ell->b, b);
    VMOVE(ell->c, c);
}


static void
make_half(struct rt_db_internal *intern, const plane_t equation)
{
    struct rt_half_internal *half;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_HALF;
    intern->idb_meth = &OBJ[ID_HALF];
    BU_ALLOC(intern->idb_ptr, struct rt_half_internal);
    half = (struct rt_half_internal *)intern->idb_ptr;
    half->magic = RT_HALF_INTERNAL_MAGIC;
    HMOVE(half->eqn, equation);
}


static void
make_tor(struct rt_db_internal *intern, const point_t center, const vect_t normal,
	 fastf_t major_radius, fastf_t minor_radius)
{
    struct rt_tor_internal *tor;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_TOR;
    intern->idb_meth = &OBJ[ID_TOR];
    BU_ALLOC(intern->idb_ptr, struct rt_tor_internal);
    tor = (struct rt_tor_internal *)intern->idb_ptr;
    tor->magic = RT_TOR_INTERNAL_MAGIC;
    VMOVE(tor->v, center);
    VMOVE(tor->h, normal);
    tor->r_a = major_radius;
    tor->r_h = minor_radius;
    bn_vec_ortho(tor->a, tor->h);
    VUNITIZE(tor->a);
    VCROSS(tor->b, tor->h, tor->a);
    VUNITIZE(tor->b);
    VSCALE(tor->a, tor->a, tor->r_a);
    VSCALE(tor->b, tor->b, tor->r_a);
    tor->r_b = tor->r_a;
}


static void
make_tgc(struct rt_db_internal *intern, int type, const point_t v,
	 const vect_t h, const vect_t a, const vect_t b,
	 const vect_t c, const vect_t d)
{
    struct rt_tgc_internal *tgc;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = type;
    intern->idb_meth = &OBJ[type];
    BU_ALLOC(intern->idb_ptr, struct rt_tgc_internal);
    tgc = (struct rt_tgc_internal *)intern->idb_ptr;
    tgc->magic = RT_TGC_INTERNAL_MAGIC;
    VMOVE(tgc->v, v);
    VMOVE(tgc->h, h);
    VMOVE(tgc->a, a);
    VMOVE(tgc->b, b);
    VMOVE(tgc->c, c);
    VMOVE(tgc->d, d);
}


static void
make_arb(struct rt_db_internal *intern, const point_t points[8])
{
    struct rt_arb_internal *arb;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_ARB8;
    intern->idb_meth = &OBJ[ID_ARB8];
    BU_ALLOC(intern->idb_ptr, struct rt_arb_internal);
    arb = (struct rt_arb_internal *)intern->idb_ptr;
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    for (size_t i = 0; i < 8; i++)
	VMOVE(arb->pt[i], points[i]);
}


static int
make_transform_output(struct rt_db_internal *output,
		      const struct rt_db_internal *input)
{
    RT_DB_INTERNAL_INIT(output);
    output->idb_major_type = input->idb_major_type;
    output->idb_minor_type = input->idb_minor_type;
    output->idb_meth = input->idb_meth;

    switch (input->idb_minor_type) {
	case ID_ELL:
	case ID_SPH: {
	    struct rt_ell_internal *ell;
	    BU_ALLOC(output->idb_ptr, struct rt_ell_internal);
	    ell = (struct rt_ell_internal *)output->idb_ptr;
	    ell->magic = RT_ELL_INTERNAL_MAGIC;
	    return 0;
	}
	case ID_HALF: {
	    struct rt_half_internal *half;
	    BU_ALLOC(output->idb_ptr, struct rt_half_internal);
	    half = (struct rt_half_internal *)output->idb_ptr;
	    half->magic = RT_HALF_INTERNAL_MAGIC;
	    return 0;
	}
	case ID_TOR: {
	    struct rt_tor_internal *tor;
	    BU_ALLOC(output->idb_ptr, struct rt_tor_internal);
	    tor = (struct rt_tor_internal *)output->idb_ptr;
	    tor->magic = RT_TOR_INTERNAL_MAGIC;
	    return 0;
	}
	case ID_TGC:
	case ID_REC: {
	    struct rt_tgc_internal *tgc;
	    BU_ALLOC(output->idb_ptr, struct rt_tgc_internal);
	    tgc = (struct rt_tgc_internal *)output->idb_ptr;
	    tgc->magic = RT_TGC_INTERNAL_MAGIC;
	    return 0;
	}
	case ID_ARB8: {
	    struct rt_arb_internal *arb;
	    BU_ALLOC(output->idb_ptr, struct rt_arb_internal);
	    arb = (struct rt_arb_internal *)output->idb_ptr;
	    arb->magic = RT_ARB_INTERNAL_MAGIC;
	    return 0;
	}
	case ID_BOT:
	    output->idb_ptr = rt_bot_dup((const struct rt_bot_internal *)input->idb_ptr);
	    return output->idb_ptr ? 0 : 1;
	default:
	    return 1;
    }
}


static void
ell_shape_matrix(fastf_t shape[9], const struct rt_ell_internal *ell)
{
    const fastf_t *axes[3] = {ell->a, ell->b, ell->c};

    for (size_t row = 0; row < 3; row++) {
	for (size_t column = 0; column < 3; column++) {
	    shape[3 * row + column] = 0.0;
	    for (size_t axis = 0; axis < 3; axis++)
		shape[3 * row + column] += axes[axis][row] * axes[axis][column];
	}
    }
}


static int
same_ell_geometry(const struct rt_db_internal *a,
		  const struct rt_db_internal *b)
{
    const struct rt_ell_internal *aell = (const struct rt_ell_internal *)a->idb_ptr;
    const struct rt_ell_internal *bell = (const struct rt_ell_internal *)b->idb_ptr;
    fastf_t ashape[9];
    fastf_t bshape[9];

    if (!near_vector(aell->v, bell->v))
	return 0;

    ell_shape_matrix(ashape, aell);
    ell_shape_matrix(bshape, bell);
    for (size_t i = 0; i < 9; i++) {
	if (!near_value(ashape[i], bshape[i]))
	    return 0;
    }

    return 1;
}


static int
same_half_geometry(const struct rt_db_internal *a,
		   const struct rt_db_internal *b)
{
    const struct rt_half_internal *ahalf = (const struct rt_half_internal *)a->idb_ptr;
    const struct rt_half_internal *bhalf = (const struct rt_half_internal *)b->idb_ptr;
    plane_t aeqn;
    plane_t beqn;
    fastf_t amag = MAGNITUDE(ahalf->eqn);
    fastf_t bmag = MAGNITUDE(bhalf->eqn);

    if (amag <= SMALL_FASTF || bmag <= SMALL_FASTF)
	return 0;
    VSCALE(aeqn, ahalf->eqn, 1.0 / amag);
    aeqn[W] = ahalf->eqn[W] / amag;
    VSCALE(beqn, bhalf->eqn, 1.0 / bmag);
    beqn[W] = bhalf->eqn[W] / bmag;

    return near_vector(aeqn, beqn) && near_value(aeqn[W], beqn[W]);
}


static int
same_tor_geometry(const struct rt_db_internal *a,
		  const struct rt_db_internal *b)
{
    const struct rt_tor_internal *ator = (const struct rt_tor_internal *)a->idb_ptr;
    const struct rt_tor_internal *btor = (const struct rt_tor_internal *)b->idb_ptr;
    vect_t anormal;
    vect_t bnormal;

    if (!near_vector(ator->v, btor->v) ||
	!near_value(ator->r_a, btor->r_a) || !near_value(ator->r_h, btor->r_h))
	return 0;

    VMOVE(anormal, ator->h);
    VMOVE(bnormal, btor->h);
    VUNITIZE(anormal);
    VUNITIZE(bnormal);
    return near_value(fabs(VDOT(anormal, bnormal)), 1.0);
}


static void
tgc_section(point_t center, fastf_t shape[9],
	    const struct rt_tgc_internal *tgc, fastf_t parameter)
{
    vect_t axes[2];

    VJOIN1(center, tgc->v, parameter, tgc->h);
    VBLEND2(axes[0], 1.0 - parameter, tgc->a, parameter, tgc->c);
    VBLEND2(axes[1], 1.0 - parameter, tgc->b, parameter, tgc->d);
    for (size_t row = 0; row < 3; row++) {
	for (size_t column = 0; column < 3; column++) {
	    shape[3 * row + column] =
		axes[0][row] * axes[0][column] +
		axes[1][row] * axes[1][column];
	}
    }
}


static int
same_tgc_direction(const struct rt_tgc_internal *a,
		   const struct rt_tgc_internal *b, int reverse_b)
{
    const fastf_t parameters[] = {0.0, 0.5, 1.0};

    for (size_t sample = 0; sample < 3; sample++) {
	point_t acenter, bcenter;
	fastf_t ashape[9], bshape[9];
	fastf_t bparameter = reverse_b ? 1.0 - parameters[sample] : parameters[sample];

	tgc_section(acenter, ashape, a, parameters[sample]);
	tgc_section(bcenter, bshape, b, bparameter);
	if (!near_vector(acenter, bcenter))
	    return 0;
	for (size_t i = 0; i < 9; i++) {
	    if (!near_value(ashape[i], bshape[i]))
		return 0;
	}
    }

    return 1;
}


static int
same_tgc_geometry(const struct rt_db_internal *a,
		  const struct rt_db_internal *b)
{
    const struct rt_tgc_internal *atgc = (const struct rt_tgc_internal *)a->idb_ptr;
    const struct rt_tgc_internal *btgc = (const struct rt_tgc_internal *)b->idb_ptr;

    return same_tgc_direction(atgc, btgc, 0) || same_tgc_direction(atgc, btgc, 1);
}


static int
same_arb_geometry(const struct rt_db_internal *a,
		  const struct rt_db_internal *b)
{
    const struct rt_arb_internal *aarb = (const struct rt_arb_internal *)a->idb_ptr;
    const struct rt_arb_internal *barb = (const struct rt_arb_internal *)b->idb_ptr;

    for (size_t i = 0; i < 8; i++) {
	if (!near_vector(aarb->pt[i], barb->pt[i]))
	    return 0;
    }
    return 1;
}


static int
same_bot_geometry(const struct rt_db_internal *a,
		  const struct rt_db_internal *b)
{
    const struct rt_bot_internal *abot = (const struct rt_bot_internal *)a->idb_ptr;
    const struct rt_bot_internal *bbot = (const struct rt_bot_internal *)b->idb_ptr;

    if (abot->num_vertices != bbot->num_vertices || abot->num_faces != bbot->num_faces ||
	abot->mode != bbot->mode || abot->orientation != bbot->orientation)
	return 0;
    for (size_t i = 0; i < 3 * abot->num_faces; i++) {
	if (abot->faces[i] != bbot->faces[i])
	    return 0;
    }
    for (size_t i = 0; i < 3 * abot->num_vertices; i++) {
	if (!near_value(abot->vertices[i], bbot->vertices[i]))
	    return 0;
    }

    return 1;
}


static int
matrix_is_identity(const mat_t matrix)
{
    for (size_t i = 0; i < 16; i++) {
	fastf_t expected = (i % 5 == 0) ? 1.0 : 0.0;
	if (!near_value(matrix[i], expected))
	    return 0;
    }
    return 1;
}


static int
test_ell_mode(enum rt_canonicalize_mode mode, const vect_t expected_lengths)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t center = {11.0, -7.0, 5.0};
    const vect_t a = {0.0, 0.0, 2.0};
    const vect_t b = {-3.0, 0.0, 0.0};
    const vect_t c = {0.0, -5.0, 0.0};
    struct rt_db_internal input;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    struct rt_db_internal recanonical;
    const struct rt_ell_internal *cell;
    mat_t placement;
    mat_t second_placement;
    int failed = 0;

    make_ell(&input, ID_ELL, center, a, b, c);
    RT_DB_INTERNAL_INIT(&canonical);
    RT_DB_INTERNAL_INIT(&recanonical);

    if (rt_obj_canonicalize(&canonical, placement, &input, &tol, mode) != RT_CANONICALIZE_OK) {
	bu_log("ELL mode %d canonicalization failed\n", (int)mode);
	failed = 1;
	goto cleanup;
    }

    cell = (const struct rt_ell_internal *)canonical.idb_ptr;
    if (!VNEAR_ZERO(cell->v, TEST_EPSILON) ||
	!near_value(cell->a[X], expected_lengths[X]) ||
	!near_value(cell->b[Y], expected_lengths[Y]) ||
	!near_value(cell->c[Z], expected_lengths[Z]) ||
	!near_value(cell->a[Y], 0.0) || !near_value(cell->a[Z], 0.0) ||
	!near_value(cell->b[X], 0.0) || !near_value(cell->b[Z], 0.0) ||
	!near_value(cell->c[X], 0.0) || !near_value(cell->c[Y], 0.0)) {
	bu_log("ELL mode %d produced a non-canonical result\n", (int)mode);
	failed = 1;
	goto cleanup;
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_ell_geometry(&input, &reconstructed)) {
	bu_log("ELL mode %d placement did not reconstruct the input\n", (int)mode);
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

    if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol, mode) != RT_CANONICALIZE_OK ||
	!same_ell_geometry(&canonical, &recanonical) || !matrix_is_identity(second_placement)) {
	bu_log("ELL mode %d canonicalization is not idempotent\n", (int)mode);
	failed = 1;
    }

cleanup:
    if (recanonical.idb_ptr)
	rt_db_free_internal(&recanonical);
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    rt_db_free_internal(&input);
    return failed;
}


static int
test_half(void)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const plane_t equation = {2.0, 0.0, 0.0, 8.0};
    struct rt_db_internal input;
    int failed = 0;

    make_half(&input, equation);
    for (int mode = RT_CANONICALIZE_RIGID; mode <= RT_CANONICALIZE_AFFINE; mode++) {
	struct rt_db_internal canonical;
	struct rt_db_internal reconstructed;
	struct rt_db_internal recanonical;
	const struct rt_half_internal *chalf;
	mat_t placement;
	mat_t second_placement;

	RT_DB_INTERNAL_INIT(&canonical);
	RT_DB_INTERNAL_INIT(&recanonical);
	if (rt_obj_canonicalize(&canonical, placement, &input, &tol,
		(enum rt_canonicalize_mode)mode) != RT_CANONICALIZE_OK) {
	    bu_log("HALF mode %d canonicalization failed\n", mode);
	    failed = 1;
	    continue;
	}

	chalf = (const struct rt_half_internal *)canonical.idb_ptr;
	if (!near_value(chalf->eqn[X], 0.0) || !near_value(chalf->eqn[Y], 0.0) ||
	    !near_value(chalf->eqn[Z], 1.0) || !near_value(chalf->eqn[W], 0.0)) {
	    bu_log("HALF mode %d produced a non-canonical result\n", mode);
	    failed = 1;
	}

	if (make_transform_output(&reconstructed, &canonical)) {
	    failed = 1;
	} else {
	    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
		!same_half_geometry(&input, &reconstructed)) {
		bu_log("HALF mode %d placement did not reconstruct the input\n", mode);
		failed = 1;
	    }
	    rt_db_free_internal(&reconstructed);
	}

	if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol,
		(enum rt_canonicalize_mode)mode) != RT_CANONICALIZE_OK ||
	    !same_half_geometry(&canonical, &recanonical) || !matrix_is_identity(second_placement)) {
	    bu_log("HALF mode %d canonicalization is not idempotent\n", mode);
	    failed = 1;
	}

	if (recanonical.idb_ptr)
	    rt_db_free_internal(&recanonical);
	rt_db_free_internal(&canonical);
    }

    rt_db_free_internal(&input);
    return failed;
}


static int
test_tor_mode(enum rt_canonicalize_mode mode,
	      fastf_t expected_major_radius, fastf_t expected_minor_radius)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t center = {-3.0, 7.0, 12.0};
    const vect_t normal = {0.0, 2.0, 0.0};
    struct rt_db_internal input;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    struct rt_db_internal recanonical;
    const struct rt_tor_internal *ctor;
    mat_t placement;
    mat_t second_placement;
    int failed = 0;

    make_tor(&input, center, normal, 5.0, 2.0);
    RT_DB_INTERNAL_INIT(&canonical);
    RT_DB_INTERNAL_INIT(&recanonical);
    if (rt_obj_canonicalize(&canonical, placement, &input, &tol, mode) != RT_CANONICALIZE_OK) {
	bu_log("TOR mode %d canonicalization failed\n", (int)mode);
	failed = 1;
	goto cleanup;
    }

    ctor = (const struct rt_tor_internal *)canonical.idb_ptr;
    if (!VNEAR_ZERO(ctor->v, TEST_EPSILON) ||
	!near_value(ctor->h[X], 0.0) || !near_value(ctor->h[Y], 0.0) ||
	!near_value(ctor->h[Z], 1.0) ||
	!near_value(ctor->r_a, expected_major_radius) ||
	!near_value(ctor->r_h, expected_minor_radius)) {
	bu_log("TOR mode %d produced a non-canonical result\n", (int)mode);
	failed = 1;
	goto cleanup;
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_tor_geometry(&input, &reconstructed)) {
	bu_log("TOR mode %d placement did not reconstruct the input\n", (int)mode);
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

    if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol, mode) != RT_CANONICALIZE_OK ||
	!same_tor_geometry(&canonical, &recanonical) || !matrix_is_identity(second_placement)) {
	bu_log("TOR mode %d canonicalization is not idempotent\n", (int)mode);
	failed = 1;
    }

cleanup:
    if (recanonical.idb_ptr)
	rt_db_free_internal(&recanonical);
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    rt_db_free_internal(&input);
    return failed;
}


static int
test_tgc_mode(int type, enum rt_canonicalize_mode mode)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t v = {3.0, -4.0, 5.0};
    const vect_t tgc_h = {2.0, 1.0, 6.0};
    const vect_t rec_h = {0.0, 0.0, 6.0};
    const vect_t a = {0.0, 4.0, 0.0};
    const vect_t b = {-2.0, 0.0, 0.0};
    const vect_t tgc_c = {0.0, 2.0, 0.0};
    const vect_t tgc_d = {-3.0, 0.0, 0.0};
    const vect_t rec_c = {0.0, 4.0, 0.0};
    const vect_t rec_d = {-2.0, 0.0, 0.0};
    const vect_t *h = (type == ID_REC) ? &rec_h : &tgc_h;
    const vect_t *c = (type == ID_REC) ? &rec_c : &tgc_c;
    const vect_t *d = (type == ID_REC) ? &rec_d : &tgc_d;
    struct rt_db_internal input;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    struct rt_db_internal recanonical;
    const struct rt_tgc_internal *ctgc;
    mat_t placement;
    mat_t second_placement;
    fastf_t scale = 1.0;
    fastf_t expected_h[3];
    fastf_t expected_a, expected_b, expected_c, expected_d;
    int failed = 0;

    make_tgc(&input, type, v, *h, a, b, *c, *d);
    RT_DB_INTERNAL_INIT(&canonical);
    RT_DB_INTERNAL_INIT(&recanonical);
    if (rt_obj_canonicalize(&canonical, placement, &input, &tol, mode) != RT_CANONICALIZE_OK) {
	bu_log("%s mode %d canonicalization failed\n", type == ID_REC ? "REC" : "TGC", (int)mode);
	failed = 1;
	goto cleanup;
    }

    ctgc = (const struct rt_tgc_internal *)canonical.idb_ptr;
    if (mode == RT_CANONICALIZE_AFFINE) {
	VSET(expected_h, 0.0, 0.0, 1.0);
	expected_a = 1.0;
	expected_b = 1.0;
	if (type == ID_REC) {
	    expected_c = 1.0;
	    expected_d = 1.0;
	} else {
	    expected_c = 1.5;
	    expected_d = 0.5;
	}
    } else {
	if (mode == RT_CANONICALIZE_SIMILARITY)
	    scale = (type == ID_REC) ? 6.0 : sqrt(41.0);
	if (type == ID_REC) {
	    VSET(expected_h, 0.0, 0.0, 6.0 / scale);
	    expected_a = 4.0 / scale;
	    expected_b = 2.0 / scale;
	    expected_c = 4.0 / scale;
	    expected_d = 2.0 / scale;
	} else {
	    VSET(expected_h, 2.0 / scale, 1.0 / scale, 6.0 / scale);
	    expected_a = 2.0 / scale;
	    expected_b = 4.0 / scale;
	    expected_c = 3.0 / scale;
	    expected_d = 2.0 / scale;
	}
    }

    if (!VNEAR_ZERO(ctgc->v, TEST_EPSILON) ||
	!near_vector(ctgc->h, expected_h) ||
	!near_value(ctgc->a[X], expected_a) ||
	!near_value(ctgc->b[Y], expected_b) ||
	!near_value(ctgc->c[X], expected_c) ||
	!near_value(ctgc->d[Y], expected_d) ||
	!near_value(ctgc->a[Y], 0.0) || !near_value(ctgc->a[Z], 0.0) ||
	!near_value(ctgc->b[X], 0.0) || !near_value(ctgc->b[Z], 0.0) ||
	!near_value(ctgc->c[Y], 0.0) || !near_value(ctgc->c[Z], 0.0) ||
	!near_value(ctgc->d[X], 0.0) || !near_value(ctgc->d[Z], 0.0)) {
	bu_log("%s mode %d produced a non-canonical result\n",
	    type == ID_REC ? "REC" : "TGC", (int)mode);
	failed = 1;
	goto cleanup;
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_tgc_geometry(&input, &reconstructed)) {
	bu_log("%s mode %d placement did not reconstruct the input\n",
	    type == ID_REC ? "REC" : "TGC", (int)mode);
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

    if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol, mode) != RT_CANONICALIZE_OK ||
	!same_tgc_geometry(&canonical, &recanonical) || !matrix_is_identity(second_placement)) {
	bu_log("%s mode %d canonicalization is not idempotent\n",
	    type == ID_REC ? "REC" : "TGC", (int)mode);
	failed = 1;
    }

cleanup:
    if (recanonical.idb_ptr)
	rt_db_free_internal(&recanonical);
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    rt_db_free_internal(&input);
    return failed;
}


static int
test_tgc_affine_invariance(void)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t v = {3.0, -4.0, 5.0};
    const vect_t h = {2.0, 1.0, 6.0};
    const vect_t a = {0.0, 4.0, 0.0};
    const vect_t b = {-2.0, 0.0, 0.0};
    const vect_t c = {0.0, 2.0, 0.0};
    const vect_t d = {-3.0, 0.0, 0.0};
    struct rt_db_internal input;
    struct rt_db_internal transformed;
    struct rt_db_internal input_canonical;
    struct rt_db_internal transformed_canonical;
    mat_t transform;
    mat_t input_placement;
    mat_t transformed_placement;
    int failed = 0;

    make_tgc(&input, ID_TGC, v, h, a, b, c, d);
    if (make_transform_output(&transformed, &input)) {
	rt_db_free_internal(&input);
	return 1;
    }
    MAT_IDN(transform);
    transform[0] = 3.0;
    transform[5] = 0.5;
    transform[10] = 2.0;
    MAT_DELTAS(transform, 7.0, -2.0, 9.0);
    if (input.idb_meth->ft_mat(&transformed, transform, &input) != BRLCAD_OK) {
	bu_log("TGC affine invariance input transform failed\n");
	failed = 1;
	goto cleanup_inputs;
    }

    RT_DB_INTERNAL_INIT(&input_canonical);
    RT_DB_INTERNAL_INIT(&transformed_canonical);
    if (rt_obj_canonicalize(&input_canonical, input_placement, &input, &tol,
	    RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_OK ||
	rt_obj_canonicalize(&transformed_canonical, transformed_placement,
	    &transformed, &tol, RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_OK ||
	!same_tgc_direction(
	    (const struct rt_tgc_internal *)input_canonical.idb_ptr,
	    (const struct rt_tgc_internal *)transformed_canonical.idb_ptr, 0)) {
	bu_log("TGC affine canonicalization is not invariant under affine placement\n");
	failed = 1;
    }

    if (transformed_canonical.idb_ptr)
	rt_db_free_internal(&transformed_canonical);
    if (input_canonical.idb_ptr)
	rt_db_free_internal(&input_canonical);
cleanup_inputs:
    rt_db_free_internal(&transformed);
    rt_db_free_internal(&input);
    return failed;
}


static int
test_tgc_degenerate_tip(void)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t v = VINIT_ZERO;
    const vect_t h = {1.0, 0.5, 7.0};
    const vect_t a = {4.0, 0.0, 0.0};
    const vect_t b = {0.0, 2.0, 0.0};
    const vect_t tip = VINIT_ZERO;
    struct rt_db_internal input;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    mat_t placement;
    int failed = 0;

    make_tgc(&input, ID_TGC, v, h, a, b, tip, tip);
    RT_DB_INTERNAL_INIT(&canonical);
    if (rt_obj_canonicalize(&canonical, placement, &input, &tol,
	    RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_OK) {
	bu_log("degenerate-tip TGC canonicalization failed\n");
	failed = 1;
	goto cleanup;
    }

    {
	const struct rt_tgc_internal *ctgc =
	    (const struct rt_tgc_internal *)canonical.idb_ptr;
	if (!VNEAR_ZERO(ctgc->c, TEST_EPSILON) ||
	    !VNEAR_ZERO(ctgc->d, TEST_EPSILON)) {
	    bu_log("degenerate-tip TGC did not retain its tip\n");
	    failed = 1;
	}
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_tgc_geometry(&input, &reconstructed)) {
	bu_log("degenerate-tip TGC placement did not reconstruct the input\n");
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

cleanup:
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    rt_db_free_internal(&input);
    return failed;
}


static int
test_arb_mode(enum rt_canonicalize_mode mode)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t points[8] = {
	{0.0, 0.0, 0.0},
	{4.0, 0.0, 0.0},
	{4.0, 3.0, 0.0},
	{0.0, 3.0, 0.0},
	{0.0, 0.0, 2.0},
	{4.0, 0.0, 2.0},
	{4.0, 3.0, 2.0},
	{0.0, 3.0, 2.0}
    };
    struct rt_db_internal base;
    struct rt_db_internal input;
    struct rt_db_internal base_canonical;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    struct rt_db_internal recanonical;
    mat_t transform;
    mat_t base_placement;
    mat_t placement;
    mat_t second_placement;
    int failed = 0;

    make_arb(&base, points);
    if (make_transform_output(&input, &base)) {
	rt_db_free_internal(&base);
	return 1;
    }
    MAT_IDN(transform);
    transform[0] = 2.0;
    transform[1] = 0.3;
    transform[2] = 0.2;
    transform[4] = 0.1;
    transform[5] = 1.5;
    transform[6] = 0.4;
    transform[8] = 0.2;
    transform[9] = 0.1;
    transform[10] = 1.7;
    MAT_DELTAS(transform, 5.0, -7.0, 11.0);
    if (base.idb_meth->ft_mat(&input, transform, &base) != BRLCAD_OK) {
	bu_log("ARB input transform failed\n");
	failed = 1;
	goto cleanup_inputs;
    }

    RT_DB_INTERNAL_INIT(&base_canonical);
    RT_DB_INTERNAL_INIT(&canonical);
    RT_DB_INTERNAL_INIT(&recanonical);
    if (rt_obj_canonicalize(&canonical, placement, &input, &tol, mode) !=
	RT_CANONICALIZE_OK) {
	bu_log("ARB mode %d canonicalization failed\n", (int)mode);
	failed = 1;
	goto cleanup_canonical;
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup_canonical;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_arb_geometry(&input, &reconstructed)) {
	bu_log("ARB mode %d placement did not reconstruct the input\n", (int)mode);
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

    if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol,
	    mode) != RT_CANONICALIZE_OK ||
	!same_arb_geometry(&canonical, &recanonical) ||
	!matrix_is_identity(second_placement)) {
	bu_log("ARB mode %d canonicalization is not idempotent\n", (int)mode);
	failed = 1;
    }

    if (mode == RT_CANONICALIZE_AFFINE) {
	if (rt_obj_canonicalize(&base_canonical, base_placement, &base, &tol,
		RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_OK ||
	    !same_arb_geometry(&base_canonical, &canonical)) {
	    bu_log("ARB affine canonicalization is not invariant under affine placement\n");
	    failed = 1;
	}
    }

cleanup_canonical:
    if (recanonical.idb_ptr)
	rt_db_free_internal(&recanonical);
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    if (base_canonical.idb_ptr)
	rt_db_free_internal(&base_canonical);
cleanup_inputs:
    rt_db_free_internal(&input);
    rt_db_free_internal(&base);
    return failed;
}


static void
make_bot(struct rt_db_internal *intern)
{
    static const fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	4.0, 0.0, 0.0,
	0.0, 2.0, 0.0,
	0.3, 0.4, 3.0
    };
    static const int faces[] = {
	0, 2, 1,
	0, 1, 3,
	1, 2, 3,
	2, 0, 3
    };
    struct rt_bot_internal *bot;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_minor_type = ID_BOT;
    intern->idb_meth = &OBJ[ID_BOT];
    BU_ALLOC(intern->idb_ptr, struct rt_bot_internal);
    bot = (struct rt_bot_internal *)intern->idb_ptr;
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = 0;
    bot->num_faces = 4;
    bot->faces = (int *)bu_malloc(sizeof(faces), "canonicalize test BOT faces");
    memcpy(bot->faces, faces, sizeof(faces));
    bot->num_vertices = 4;
    bot->vertices = (fastf_t *)bu_malloc(sizeof(vertices), "canonicalize test BOT vertices");
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->thickness = NULL;
    bot->face_mode = NULL;
    bot->num_normals = 0;
    bot->normals = NULL;
    bot->num_face_normals = 0;
    bot->face_normals = NULL;
    bot->num_uvs = 0;
    bot->uvs = NULL;
    bot->num_face_uvs = 0;
    bot->face_uvs = NULL;
}


static int
test_bot_mode(enum rt_canonicalize_mode mode)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_db_internal base;
    struct rt_db_internal input;
    struct rt_db_internal base_canonical;
    struct rt_db_internal canonical;
    struct rt_db_internal reconstructed;
    struct rt_db_internal recanonical;
    mat_t transform;
    mat_t placement;
    mat_t base_placement;
    mat_t second_placement;
    int failed = 0;

    make_bot(&base);
    if (make_transform_output(&input, &base)) {
	rt_db_free_internal(&base);
	return 1;
    }
    MAT_IDN(transform);
    transform[0] = 0.0;
    transform[1] = -1.0;
    transform[4] = 1.0;
    transform[5] = 0.0;
    MAT_DELTAS(transform, 8.0, -4.0, 6.0);
    if (base.idb_meth->ft_mat(&input, transform, &base) != BRLCAD_OK) {
	bu_log("BOT test input transform failed\n");
	failed = 1;
	goto cleanup_inputs;
    }

    RT_DB_INTERNAL_INIT(&base_canonical);
    RT_DB_INTERNAL_INIT(&canonical);
    RT_DB_INTERNAL_INIT(&recanonical);
    if (rt_obj_canonicalize(&base_canonical, base_placement, &base, &tol, mode) != RT_CANONICALIZE_OK ||
	rt_obj_canonicalize(&canonical, placement, &input, &tol, mode) != RT_CANONICALIZE_OK) {
	bu_log("BOT mode %d canonicalization failed\n", (int)mode);
	failed = 1;
	goto cleanup_canonical;
    }
    if (!same_bot_geometry(&base_canonical, &canonical)) {
	bu_log("BOT mode %d is not invariant under rigid placement\n", (int)mode);
	failed = 1;
    }

    if (make_transform_output(&reconstructed, &canonical)) {
	failed = 1;
	goto cleanup_canonical;
    }
    if (canonical.idb_meth->ft_mat(&reconstructed, placement, &canonical) != BRLCAD_OK ||
	!same_bot_geometry(&input, &reconstructed)) {
	bu_log("BOT mode %d placement did not reconstruct the input\n", (int)mode);
	failed = 1;
    }
    rt_db_free_internal(&reconstructed);

    if (rt_obj_canonicalize(&recanonical, second_placement, &canonical, &tol, mode) != RT_CANONICALIZE_OK ||
	!same_bot_geometry(&canonical, &recanonical) || !matrix_is_identity(second_placement)) {
	bu_log("BOT mode %d canonicalization is not idempotent\n", (int)mode);
	failed = 1;
    }

cleanup_canonical:
    if (recanonical.idb_ptr)
	rt_db_free_internal(&recanonical);
    if (canonical.idb_ptr)
	rt_db_free_internal(&canonical);
    if (base_canonical.idb_ptr)
	rt_db_free_internal(&base_canonical);
cleanup_inputs:
    rt_db_free_internal(&input);
    rt_db_free_internal(&base);
    return failed;
}


static int
test_errors(void)
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    const point_t center = VINIT_ZERO;
    const vect_t a = {2.0, 0.0, 0.0};
    const vect_t b = {1.0, 3.0, 0.0};
    const vect_t c = {0.0, 0.0, 4.0};
    struct rt_db_internal invalid;
    struct rt_db_internal output;
    struct rt_db_internal unsupported;
    mat_t placement;
    int failed = 0;

    make_ell(&invalid, ID_ELL, center, a, b, c);
    RT_DB_INTERNAL_INIT(&output);
    if (rt_obj_canonicalize(&output, placement, &invalid, &tol,
	    RT_CANONICALIZE_RIGID) != RT_CANONICALIZE_ERROR || output.idb_ptr) {
	bu_log("invalid ELL was not rejected cleanly\n");
	failed = 1;
    }
    rt_db_free_internal(&invalid);

    {
	const vect_t h = {1.0, 1.0, 0.0};
	const vect_t tgc_a = {2.0, 0.0, 0.0};
	const vect_t tgc_b = {0.0, 3.0, 0.0};
	const vect_t tgc_c = {1.0, 0.0, 0.0};
	const vect_t tgc_d = {0.0, 1.5, 0.0};

	make_tgc(&invalid, ID_TGC, center, h, tgc_a, tgc_b, tgc_c, tgc_d);
	if (rt_obj_canonicalize(&output, placement, &invalid, &tol,
		RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_ERROR || output.idb_ptr) {
	    bu_log("invalid TGC was not rejected cleanly\n");
	    failed = 1;
	}
	rt_db_free_internal(&invalid);
    }

    {
	const point_t invalid_arb[8] = {
	    {0.0, 0.0, 0.0}, {4.0, 0.0, 0.0},
	    {4.0, 3.0, 0.5}, {0.0, 3.0, 0.0},
	    {0.0, 0.0, 2.0}, {4.0, 0.0, 2.0},
	    {4.0, 3.0, 2.0}, {0.0, 3.0, 2.0}
	};

	make_arb(&invalid, invalid_arb);
	if (rt_obj_canonicalize(&output, placement, &invalid, &tol,
		RT_CANONICALIZE_AFFINE) != RT_CANONICALIZE_ERROR || output.idb_ptr) {
	    bu_log("invalid ARB was not rejected cleanly\n");
	    failed = 1;
	}
	rt_db_free_internal(&invalid);
    }

    RT_DB_INTERNAL_INIT(&unsupported);
    unsupported.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    unsupported.idb_minor_type = ID_ARS;
    unsupported.idb_meth = &OBJ[ID_ARS];
    if (rt_obj_canonicalize(&output, placement, &unsupported, &tol,
	    RT_CANONICALIZE_RIGID) != RT_CANONICALIZE_UNSUPPORTED || output.idb_ptr) {
	bu_log("unsupported primitive did not report unsupported cleanly\n");
	failed = 1;
    }

    output.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    if (rt_obj_canonicalize(&output, placement, &unsupported, &tol,
	    RT_CANONICALIZE_RIGID) != RT_CANONICALIZE_ERROR) {
	bu_log("non-empty canonical output was accepted\n");
	failed = 1;
    }

    return failed;
}


int
main(int UNUSED(argc), const char *argv[])
{
    const vect_t rigid_lengths = {5.0, 3.0, 2.0};
    const vect_t similarity_lengths = {1.0, 0.6, 0.4};
    const vect_t affine_lengths = {1.0, 1.0, 1.0};
    int failures = 0;

    bu_setprogname(argv[0]);

    failures += test_ell_mode(RT_CANONICALIZE_RIGID, rigid_lengths);
    failures += test_ell_mode(RT_CANONICALIZE_SIMILARITY, similarity_lengths);
    failures += test_ell_mode(RT_CANONICALIZE_AFFINE, affine_lengths);
    failures += test_half();
    failures += test_tor_mode(RT_CANONICALIZE_RIGID, 5.0, 2.0);
    failures += test_tor_mode(RT_CANONICALIZE_SIMILARITY, 1.0, 0.4);
    failures += test_tor_mode(RT_CANONICALIZE_AFFINE, 1.0, 0.4);
    failures += test_tgc_mode(ID_TGC, RT_CANONICALIZE_RIGID);
    failures += test_tgc_mode(ID_TGC, RT_CANONICALIZE_SIMILARITY);
    failures += test_tgc_mode(ID_TGC, RT_CANONICALIZE_AFFINE);
    failures += test_tgc_mode(ID_REC, RT_CANONICALIZE_RIGID);
    failures += test_tgc_mode(ID_REC, RT_CANONICALIZE_SIMILARITY);
    failures += test_tgc_mode(ID_REC, RT_CANONICALIZE_AFFINE);
    failures += test_tgc_affine_invariance();
    failures += test_tgc_degenerate_tip();
    failures += test_arb_mode(RT_CANONICALIZE_RIGID);
    failures += test_arb_mode(RT_CANONICALIZE_SIMILARITY);
    failures += test_arb_mode(RT_CANONICALIZE_AFFINE);
    failures += test_bot_mode(RT_CANONICALIZE_RIGID);
    failures += test_bot_mode(RT_CANONICALIZE_SIMILARITY);
    failures += test_bot_mode(RT_CANONICALIZE_AFFINE);
    failures += test_errors();

    return failures ? 1 : 0;
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
