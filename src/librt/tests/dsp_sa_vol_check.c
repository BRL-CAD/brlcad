/*                      D S P _ S A _ V O L _ C H E C K . C
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
/** @file librt/tests/dsp_sa_vol_check.c
 *
 * Regression tests for DSP primitive surface area and volume.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"


#define DSP_TEST_TOL 1.0e-9


struct test_options {
    int compare_crofton;
    int strict_crofton;
    fastf_t crofton_max_delta_pct;
};


static int
near_equal(fastf_t a, fastf_t b, fastf_t tol)
{
    fastf_t scale = fabs(b);

    if (scale < 1.0)
	scale = 1.0;

    return fabs(a - b) <= tol * scale;
}


static fastf_t
rel_delta_pct(fastf_t got, fastf_t ref)
{
    fastf_t denom = fabs(ref);

    if (denom < SMALL_FASTF)
	return fabs(got) < SMALL_FASTF ? 0.0 : INFINITY;

    return 100.0 * fabs(got - ref) / denom;
}


static int
check_near(const char *name, fastf_t got, fastf_t expected)
{
    if (!near_equal(got, expected, DSP_TEST_TOL)) {
	bu_log("FAIL: %s: got %.17g, expected %.17g, delta %.17g\n",
	       name, got, expected, got - expected);
	return 1;
    }

    bu_log("PASS: %s: %.17g\n", name, got);
    return 0;
}


static void
print_crofton_compare(const char *label,
		      fastf_t direct_area,
		      fastf_t crofton_area,
		      fastf_t direct_volume,
		      fastf_t crofton_volume)
{
    bu_log("CROFTON: %s\n", label);
    bu_log("  area:   direct %.17g  crofton %.17g  delta %.6f%%\n",
	   direct_area,
	   crofton_area,
	   rel_delta_pct(crofton_area, direct_area));
    bu_log("  volume: direct %.17g  crofton %.17g  delta %.6f%%\n",
	   direct_volume,
	   crofton_volume,
	   rel_delta_pct(crofton_volume, direct_volume));
}


static int
check_crofton_compare(const char *label,
		      fastf_t direct_area,
		      fastf_t crofton_area,
		      fastf_t direct_volume,
		      fastf_t crofton_volume,
		      fastf_t max_delta_pct)
{
    int failures = 0;
    fastf_t area_delta = rel_delta_pct(crofton_area, direct_area);
    fastf_t volume_delta = rel_delta_pct(crofton_volume, direct_volume);

    if (area_delta > max_delta_pct) {
	bu_log("FAIL: %s Crofton area delta %.6f%% exceeds %.6f%%\n",
	       label, area_delta, max_delta_pct);
	failures++;
    }

    if (volume_delta > max_delta_pct) {
	bu_log("FAIL: %s Crofton volume delta %.6f%% exceeds %.6f%%\n",
	       label, volume_delta, max_delta_pct);
	failures++;
    }

    return failures;
}


static struct rt_db_internal *
make_test_binunif(const char *name,
		  const unsigned short *heights,
		  size_t count)
{
    struct rt_db_internal *bip_intern = NULL;
    struct rt_binunif_internal *bip = NULL;

    BU_ALLOC(bip_intern, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(bip_intern);

    bip_intern->idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
    bip_intern->idb_type = ID_BINUNIF;
    bip_intern->idb_minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
    bip_intern->idb_meth = &OBJ[ID_BINUNIF];

    BU_ALLOC(bip, struct rt_binunif_internal);
    memset(bip, 0, sizeof(*bip));

    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
    bip->type = DB5_MINORTYPE_BINU_16BITINT_U;
    bip->count = count;
    bip->u.uint16 = (unsigned short *)bu_calloc(count, sizeof(unsigned short), name);

    memcpy(bip->u.uint16, heights, count * sizeof(unsigned short));

    bip_intern->idb_ptr = (void *)bip;

    return bip_intern;
}


static void
free_test_binunif(struct rt_db_internal *bip_intern)
{
    struct rt_binunif_internal *bip = NULL;

    if (!bip_intern)
	return;

    bip = (struct rt_binunif_internal *)bip_intern->idb_ptr;

    if (bip) {
	if (bip->u.uint16)
	    bu_free(bip->u.uint16, "test DSP binunif uint16 data");

	bip->magic = 0;
	bu_free(bip, "test DSP binunif internal");
    }

    bip_intern->idb_ptr = NULL;
    bu_free(bip_intern, "test DSP binunif db internal");
}


static void
make_test_dsp(struct rt_db_internal *ip,
	      const char *label,
	      unsigned int xcnt,
	      unsigned int ycnt,
	      const unsigned short *heights,
	      int cuttype,
	      const mat_t stom)
{
    struct rt_dsp_internal *dsp = NULL;
    struct rt_binunif_internal *bip = NULL;
    size_t count = (size_t)xcnt * (size_t)ycnt;
    char data_name[256];

    snprintf(data_name, sizeof(data_name), "%s_data", label);

    memset(ip, 0, sizeof(*ip));
    RT_DB_INTERNAL_INIT(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_DSP;
    ip->idb_minor_type = ID_DSP;
    ip->idb_meth = &OBJ[ID_DSP];

    BU_ALLOC(dsp, struct rt_dsp_internal);
    memset(dsp, 0, sizeof(*dsp));

    dsp->magic = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, data_name);

    dsp->dsp_xcnt = xcnt;
    dsp->dsp_ycnt = ycnt;
    dsp->dsp_cuttype = cuttype;
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;

    MAT_COPY(dsp->dsp_stom, stom);
    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    dsp->dsp_bip = make_test_binunif(data_name, heights, count);

    bip = (struct rt_binunif_internal *)dsp->dsp_bip->idb_ptr;
    dsp->dsp_buf = bip->u.uint16;

    ip->idb_ptr = (void *)dsp;
}


static void
free_test_dsp(struct rt_db_internal *ip)
{
    struct rt_dsp_internal *dsp = NULL;

    if (!ip || !ip->idb_ptr)
	return;

    dsp = (struct rt_dsp_internal *)ip->idb_ptr;

    free_test_binunif(dsp->dsp_bip);
    dsp->dsp_bip = NULL;
    dsp->dsp_buf = NULL;

    if (BU_VLS_IS_INITIALIZED(&dsp->dsp_name))
	bu_vls_free(&dsp->dsp_name);

    dsp->magic = 0;
    bu_free(dsp, "test DSP internal");

    memset(ip, 0, sizeof(*ip));
}


static int
compare_with_crofton(const char *label,
		     const struct rt_db_internal *intern,
		     fastf_t direct_area,
		     fastf_t direct_volume,
		     const struct test_options *opts)
{
    fastf_t crofton_area = 0.0;
    fastf_t crofton_volume = 0.0;

    if (!opts || !opts->compare_crofton)
	return 0;

    // FIXME: crofton hangs on zero test!
    if (!bu_strcmp(label, "all_zero")) {
	bu_log("CROFTON: (SKIPPING) 'all_zero' - Crofton will inf hang\n");
	return 0;
    }

    rt_crofton_sample(&crofton_area, &crofton_volume, intern, NULL);

    print_crofton_compare(label,
			  direct_area,
			  crofton_area,
			  direct_volume,
			  crofton_volume);

    if (!opts->strict_crofton)
	return 0;

    return check_crofton_compare(label,
				 direct_area,
				 crofton_area,
				 direct_volume,
				 crofton_volume,
				 opts->crofton_max_delta_pct);
}


static int
eval_case(const char *label,
	  unsigned int xcnt,
	  unsigned int ycnt,
	  const unsigned short *heights,
	  int cuttype,
	  const mat_t stom,
	  fastf_t expected_volume,
	  fastf_t expected_area,
	  const struct test_options *opts)
{
    struct rt_db_internal intern;
    fastf_t volume = 0.0;
    fastf_t area = 0.0;
    int failures = 0;

    make_test_dsp(&intern, label, xcnt, ycnt, heights, cuttype, stom);

    {
	char buf[256];

	rt_dsp_volume(&volume, &intern);
	snprintf(buf, sizeof(buf), "%s volume", label);
	failures += check_near(buf, volume, expected_volume);

	rt_dsp_surf_area(&area, &intern);
	snprintf(buf, sizeof(buf), "%s surface area", label);
	failures += check_near(buf, area, expected_area);
    }

    if (!failures)
	failures += compare_with_crofton(label, &intern, area, volume, opts);

    free_test_dsp(&intern);

    return failures;
}


static int
test_flat_identity(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	5, 5, 5,
	5, 5, 5,
	5, 5, 5,
	5, 5, 5
    };

    MAT_IDN(stom);

    /* xcnt=3, ycnt=4 => xsiz=2, ysiz=3, height=5.
     *
     * volume = 2 * 3 * 5 = 30
     * area = top 6 + bottom 6 + y-side skirts 20 + x-side skirts 30 = 62
     */
    return eval_case("flat_identity",
		     3, 4, h, DSP_CUT_DIR_llUR, stom,
		     30.0, 62.0, opts);
}


static int
test_single_cell_ad_cut(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	1, 2,
	4, 8
    };

    MAT_IDN(stom);

    /* A=1, B=2, C=4, D=8.
     *
     * A-D cut volume = (2A + B + C + 2D) / 6 = 4
     * surface area = top 5.631716758280881 + bottom 1 + sides 15
     */
    return eval_case("single_cell_ad_cut",
		     2, 2, h, DSP_CUT_DIR_llUR, stom,
		     4.0, 21.63171675828088, opts);
}


static int
test_single_cell_bc_cut(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	1, 2,
	4, 8
    };

    MAT_IDN(stom);

    /* A=1, B=2, C=4, D=8.
     *
     * B-C cut volume = (A + 2B + 2C + D) / 6 = 3.5
     * surface area = top 5.298367339817959 + bottom 1 + sides 15
     */
    return eval_case("single_cell_bc_cut",
		     2, 2, h, DSP_CUT_DIR_ULlr, stom,
		     3.5, 21.298367339817958, opts);
}


static int
test_adaptive_chooses_ad(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	0, 0,
	20, 2
    };

    MAT_IDN(stom);

    /* Single-cell adaptive clamp:
     * cAD = 2 * |D - A| = 4
     * cBC = 2 * |C - B| = 40
     * therefore adaptive chooses A-D.
     */
    return eval_case("adaptive_chooses_ad",
		     2, 2, h, DSP_CUT_DIR_ADAPT, stom,
		     4.0, 37.58094600658615, opts);
}


static int
test_adaptive_chooses_bc(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	0, 7,
	9, 20
    };

    MAT_IDN(stom);

    /* Single-cell adaptive clamp:
     * cAD = 2 * |D - A| = 40
     * cBC = 2 * |C - B| = 4
     * therefore adaptive chooses B-C.
     */
    return eval_case("adaptive_chooses_bc",
		     2, 2, h, DSP_CUT_DIR_ADAPT, stom,
		     8.666666666666666, 51.252122625745784, opts);
}


static int
test_scaled_transform(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	2, 2,
	2, 2
    };

    MAT_IDN(stom);

    /* Scale x=2, y=3, z=4 plus translation.
     * Translation should not affect area or volume.
     */
    stom[0] = 2.0;
    stom[5] = 3.0;
    stom[10] = 4.0;

    stom[3] = 10.0;
    stom[7] = -5.0;
    stom[11] = 2.0;

    /* Local flat 1x1x2 solid.
     * det = 2 * 3 * 4 = 24, volume = 2 * 24 = 48.
     *
     * In model space:
     * top = 2*3 = 6
     * bottom = 6
     * two x-length sides: 2 * (2*8) = 32
     * two y-length sides: 2 * (3*8) = 48
     * total = 92
     */
    return eval_case("scaled_transform",
		     2, 2, h, DSP_CUT_DIR_llUR, stom,
		     48.0, 92.0, opts);
}


static int
test_ramp_3x3(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	0, 2, 1,
	3, 5, 2,
	1, 4, 6
    };

    MAT_IDN(stom);

    /* 3x3 non-planar grid, fixed A-D (llUR) cut on all four cells.  This
     * exercises the multi-cell top/bottom/wall accumulation (the earlier
     * cases are all single-cell or flat).  Golden values are from an
     * independent reference implementation of the same geometry model.
     */
    return eval_case("ramp_3x3",
		     3, 3, h, DSP_CUT_DIR_llUR, stom,
		     12.833333333333336, 36.656960455295263, opts);
}


static int
test_adaptive_multicell(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[] = {
	0, 9, 0, 9,
	9, 0, 9, 0,
	0, 9, 0, 9,
	9, 0, 9, 0
    };

    MAT_IDN(stom);

    /* 4x4 checkerboard.  The adaptive rule is the real 4-point discrete
     * curvature over diagonal-neighbor cells (dsp.c permute_cell), not the
     * single-cell 2|D-A| shortcut, so the nine cells pick MIXED diagonals
     * (corners A-D, the rest B-C).  This is the case the single-cell
     * adaptive tests cannot cover.  Golden from independent reference.
     */
    return eval_case("adaptive_multicell",
		     4, 4, h, DSP_CUT_DIR_ADAPT, stom,
		     36.0, 177.90430801323333, opts);
}


static int
test_all_zero(const struct test_options *opts)
{
    mat_t stom;
    unsigned short h[12] = {0};

    MAT_IDN(stom);

    /* Degenerate flat-at-zero solid (xcnt=3, ycnt=4): no volume and no
     * walls; surface area is just the coincident top and bottom footprints
     * = 2 * xsiz * ysiz = 2 * 2 * 3 = 12.
     */
    return eval_case("all_zero",
		     3, 4, h, DSP_CUT_DIR_llUR, stom,
		     0.0, 12.0, opts);
}


static int
test_rotation_invariant(const struct test_options *opts)
{
    /* A rigid rotation (plus translation) that mixes all three axes. */
    mat_t stom = {
	0.67121216615895773, -0.50708187275444627, 0.54068678763591338, 7.0,
	0.56535420838114381, 0.82195436950412748, 0.069033568057884742, -2.0,
	-0.47942553860420301, 0.25934338005223079, 0.83838664359420356, 11.0,
	0.0, 0.0, 0.0, 1.0
    };
    unsigned short h[] = {
	5, 5, 5,
	5, 5, 5,
	5, 5, 5,
	5, 5, 5
    };

    /* Rotation preserves both volume and area, so the flat_identity solid
     * must still measure 30 and 62 under this non-axis-aligned transform.
     * Uses ADAPT: equal heights drive cAD==cBC, exercising the tie path
     * (which selects B-C).
     */
    return eval_case("rotation_invariant",
		     3, 4, h, DSP_CUT_DIR_ADAPT, stom,
		     30.0, 62.0, opts);
}


static int
test_affine_transform(const struct test_options *opts)
{
    /* General affine: rotation * non-uniform scale(2,3,1.5), plus offset. */
    mat_t stom = {
	1.3424243323179155, -1.5212456182633387, 0.81103018145387007, 1.0,
	1.1307084167622876, 2.4658631085123823, 0.10355035208682711, 2.0,
	-0.95885107720840601, 0.77803014015669236, 1.2575799653913053, 3.0,
	0.0, 0.0, 0.0, 1.0
    };
    unsigned short h[] = {
	0, 2, 1,
	3, 5, 2,
	1, 4, 6
    };

    /* Unlike the axis-aligned scaled_transform case, a general affine on a
     * sloped grid only matches if surface area is accumulated per-vertex in
     * MODEL space (area scaling is anisotropic) and volume is scaled by the
     * |det| of the transform.  Golden from independent reference.
     */
    return eval_case("affine_transform",
		     3, 3, h, DSP_CUT_DIR_llUR, stom,
		     115.50000000000003, 149.97355750603421, opts);
}


static int
run_sample_object(const char *gfile,
		  const char *object,
		  const struct test_options *opts)
{
    struct db_i *dbip = DBI_NULL;
    struct directory *dp = RT_DIR_NULL;
    struct rt_db_internal intern;
    struct resource resp;
    mat_t mat;
    fastf_t volume = 0.0;
    fastf_t area = 0.0;
    int ret = 0;

    rt_init_resource(&resp, 0, NULL);

    dbip = db_open(gfile, DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	bu_log("ERROR: unable to open %s\n", gfile);
	return 1;
    }

    if (db_dirbuild(dbip) < 0) {
	bu_log("ERROR: unable to build directory for %s\n", gfile);
	db_close(dbip);
	return 1;
    }

    dp = db_lookup(dbip, object, LOOKUP_NOISY);
    if (dp == RT_DIR_NULL) {
	bu_log("ERROR: object not found: %s\n", object);
	db_close(dbip);
	return 1;
    }

    MAT_IDN(mat);
    RT_DB_INTERNAL_INIT(&intern);

    if (rt_db_get_internal(&intern, dp, dbip, mat) < 0) {
	bu_log("ERROR: unable to import object: %s\n", object);
	db_close(dbip);
	return 1;
    }

    if (intern.idb_type != ID_DSP && intern.idb_minor_type != ID_DSP) {
	bu_log("ERROR: object is not a DSP primitive: %s\n", object);
	rt_db_free_internal(&intern);
	db_close(dbip);
	return 1;
    }

    rt_dsp_volume(&volume, &intern);
    rt_dsp_surf_area(&area, &intern);

    if (!ret) {
	bu_log("%s/%s\n", gfile, object);
	bu_log("  direct volume:       %.17g\n", volume);
	bu_log("  direct surface area: %.17g\n", area);

	if (opts && opts->compare_crofton) {
	    struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)intern.idb_ptr;

	    if (dsp_ip && dsp_ip->dsp_datasrc != RT_DSP_SRC_OBJ) {
		bu_log("  Crofton comparison skipped: DSP is not RT_DSP_SRC_OBJ-backed.\n");
		bu_log("  The current Crofton bridge only exports referenced DSP binunif data for object-backed DSPs.\n");
	    } else {
		ret += compare_with_crofton(object, &intern, area, volume, opts);
	    }
	}
    }

    rt_db_free_internal(&intern);
    db_close(dbip);

    return ret;
}


static void
usage(const char *argv0)
{
    bu_log("Usage:\n");
    bu_log("  %s\n", argv0);
    bu_log("  %s -c\n", argv0);
    bu_log("  %s -C max_delta_pct\n", argv0);
    bu_log("  %s -g file.g -o dsp_object [-c]\n", argv0);
    bu_log("  %s -g file.g -o dsp_object [-C max_delta_pct]\n", argv0);
    bu_log("  %s file.g dsp_object\n", argv0);
}


static int
parse_args(int argc,
	   const char **argv,
	   const char **gfile,
	   const char **object,
	   struct test_options *opts)
{
    int i = 1;

    *gfile = NULL;
    *object = NULL;

    memset(opts, 0, sizeof(*opts));
    opts->crofton_max_delta_pct = 0.0;

    if (argc == 3 && argv[1][0] != '-') {
	*gfile = argv[1];
	*object = argv[2];
	return 0;
    }

    while (i < argc) {
	if (BU_STR_EQUAL(argv[i], "-c")) {
	    opts->compare_crofton = 1;
	    i++;
	} else if (BU_STR_EQUAL(argv[i], "-C")) {
	    char *endp = NULL;

	    if (i + 1 >= argc)
		return 1;

	    opts->compare_crofton = 1;
	    opts->strict_crofton = 1;
	    opts->crofton_max_delta_pct = strtod(argv[i + 1], &endp);

	    if (!endp || *endp != '\0' || opts->crofton_max_delta_pct < 0.0)
		return 1;

	    i += 2;
	} else if (BU_STR_EQUAL(argv[i], "-g")) {
	    if (i + 1 >= argc)
		return 1;

	    *gfile = argv[i + 1];
	    i += 2;
	} else if (BU_STR_EQUAL(argv[i], "-o")) {
	    if (i + 1 >= argc)
		return 1;

	    *object = argv[i + 1];
	    i += 2;
	} else {
	    return 1;
	}
    }

    if ((*gfile && !*object) || (!*gfile && *object))
	return 1;

    return 0;
}


int
main(int argc, const char **argv)
{
    struct test_options opts;
    const char *gfile = NULL;
    const char *object = NULL;
    int failures = 0;

    bu_setprogname(argv[0]);

    if (parse_args(argc, argv, &gfile, &object, &opts) != 0) {
	usage(argv[0]);
	return 1;
    }

    if (gfile && object)
	return run_sample_object(gfile, object, &opts);

    failures += test_flat_identity(&opts);
    failures += test_single_cell_ad_cut(&opts);
    failures += test_single_cell_bc_cut(&opts);
    failures += test_adaptive_chooses_ad(&opts);
    failures += test_adaptive_chooses_bc(&opts);
    failures += test_scaled_transform(&opts);
    failures += test_ramp_3x3(&opts);
    failures += test_adaptive_multicell(&opts);
    failures += test_all_zero(&opts);
    failures += test_rotation_invariant(&opts);
    failures += test_affine_transform(&opts);

    if (failures) {
	bu_log("DSP area/volume tests failed: %d\n", failures);
	return 1;
    }

    bu_log("DSP area/volume tests passed\n");
    return 0;
}
