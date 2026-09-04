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

    RT_DB_INTERNAL_INIT(&unsupported);
    unsupported.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    unsupported.idb_minor_type = ID_ARB8;
    unsupported.idb_meth = &OBJ[ID_ARB8];
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
