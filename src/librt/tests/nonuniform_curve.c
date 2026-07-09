/*                 N O N U N I F O R M _ C U R V E . C
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
/** @file librt/tests/nonuniform_curve.c
 *
 * Stand-alone tests for matrix:nonuniform curvature callbacks.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../librt_private.h"


#define CURVE_TOL 1.0e-5
#define TANGENT_TOL 1.0e-5


struct prim_case {
    const char *name;
    int affine_enabled;
};


struct body_ray {
    point_t p;
    vect_t d;
};


struct curve_sample {
    int hit;
    int curve_ret;
    struct curvature curve;
    vect_t normal;
};


struct affine_case {
    const char *name;
    fastf_t sx;
    fastf_t sy;
    fastf_t sz;
    fastf_t rx;
    fastf_t ry;
    fastf_t rz;
};


static const struct prim_case prims[] = {
    {"tor", 1},
    {"eto", 1},
    {"epa", 1},
    {"ehy", 1},
    {"rpc", 1},
    {"rhc", 1},
    {"particle", 1},
    {"hyp", 1},
    {"cline", 0},
    {"pipe", 0},
    {"metaball", 0}
};


static const struct affine_case affine_cases[] = {
    {"nonuniform", 1.7, 0.65, 1.3, 0.0, 0.0, 0.0},
    {"rot_xyz", 1.7, 0.65, 1.3, 17.0, -23.0, 31.0},
    {"rot_xz", 1.7, 1.0, 0.55, -12.0, 28.0, 9.0}
};


static const struct body_ray rays[] = {
    {{8.0, 0.0, 8.0}, {0.0, 0.0, -1.0}},
    {{8.0, 0.0, 10.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 8.0, 10.0}, {0.0, -1.0, 0.0}},
    {{8.0, 0.0, 6.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 8.0, 6.0}, {0.0, -1.0, 0.0}},
    {{8.0, 0.0, 3.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 8.0, 3.0}, {0.0, -1.0, 0.0}},
    {{8.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 8.0, 1.0}, {0.0, -1.0, 0.0}},
    {{10.0, 0.0, 0.0}, {-1.0, 0.0, 0.0}},
    {{0.0, 10.0, 0.0}, {0.0, -1.0, 0.0}},
    {{0.0, 0.0, 20.0}, {0.0, 0.0, -1.0}}
};


static void
make_uniform_mat(mat_t mat, fastf_t scale)
{
    MAT_IDN(mat);
    mat[0] = scale;
    mat[5] = scale;
    mat[10] = scale;
    mat[3] = 4.0;
    mat[7] = -3.0;
    mat[11] = 2.0;
}


static void
make_affine_mat(mat_t mat, const struct affine_case *ac)
{
    mat_t rot, scale;

    MAT_IDN(scale);
    scale[0] = ac->sx;
    scale[5] = ac->sy;
    scale[10] = ac->sz;

    bn_mat_angles(rot, ac->rx, ac->ry, ac->rz);
    bn_mat_mul(mat, rot, scale);
    mat[3] = 12.0;
    mat[7] = -5.0;
    mat[11] = 3.0;
}


static void
transform_ray(struct xray *ray, const struct body_ray *body, const mat_t mat)
{
    memset(ray, 0, sizeof(struct xray));
    ray->magic = RT_RAY_MAGIC;
    MAT4X3PNT(ray->r_pt, mat, body->p);
    MAT4X3VEC(ray->r_dir, mat, body->d);
    VUNITIZE(ray->r_dir);
    ray->r_min = 0.0;
    ray->r_max = INFINITY;
}


static int
curve_finite(const struct curvature *curve)
{
    return isfinite(curve->crv_c1) && isfinite(curve->crv_c2) &&
	isfinite(curve->crv_pdir[X]) &&
	isfinite(curve->crv_pdir[Y]) &&
	isfinite(curve->crv_pdir[Z]);
}


static int
significant_curve(const struct curvature *curve)
{
    return fabs(curve->crv_c1) > CURVE_TOL || fabs(curve->crv_c2) > CURVE_TOL;
}


static int
near_value(fastf_t a, fastf_t b)
{
    fastf_t scale = fabs(b);
    if (scale < 1.0)
	scale = 1.0;
    return fabs(a - b) <= CURVE_TOL * scale;
}


static int
unitize_checked(vect_t v)
{
    fastf_t mag;

    if (!isfinite(v[X]) || !isfinite(v[Y]) || !isfinite(v[Z]))
	return -1;

    mag = MAGNITUDE(v);
    if (!isfinite(mag) || NEAR_ZERO(mag, VDIVIDE_TOL))
	return -1;

    VSCALE(v, v, 1.0 / mag);
    return 0;
}


static int
expected_affine_curve(struct curvature *out, const struct curvature *body_curve, const vect_t body_normal, const mat_t body_to_model, const mat_t model_to_body)
{
    vect_t n, e1, e2;
    vect_t t1, t2, model_n;
    vect_t ev1, ev2;
    fastf_t normal_dot;
    fastf_t alpha;
    fastf_t g11, g12, g22;
    fastf_t l11, l21, l22, rad;
    fastf_t a, c, d;
    fastf_t b1, b2;
    fastf_t h00, h01, h11;
    fastf_t v0, v1;

    *out = (struct curvature)RT_CURVATURE_INIT_ZERO;

    VMOVE(n, body_normal);
    if (unitize_checked(n) < 0)
	return -1;

    VMOVE(e1, body_curve->crv_pdir);
    if (unitize_checked(e1) < 0)
	bn_vec_ortho(e1, n);

    normal_dot = VDOT(e1, n);
    VJOIN1(e1, e1, -normal_dot, n);
    if (unitize_checked(e1) < 0)
	bn_vec_ortho(e1, n);

    VCROSS(e2, n, e1);
    if (unitize_checked(e2) < 0)
	return -1;

    MAT4X3VEC(t1, body_to_model, e1);
    MAT4X3VEC(t2, body_to_model, e2);

    VEC3X3MAT(model_n, n, model_to_body);
    alpha = MAGNITUDE(model_n);
    if (!isfinite(alpha) || NEAR_ZERO(alpha, VDIVIDE_TOL))
	return -1;
    VSCALE(model_n, model_n, 1.0 / alpha);

    g11 = VDOT(t1, t1);
    g12 = VDOT(t1, t2);
    g22 = VDOT(t2, t2);

    l11 = sqrt(g11);
    if (!isfinite(l11) || NEAR_ZERO(l11, VDIVIDE_TOL))
	return -1;

    l21 = g12 / l11;
    rad = g22 - l21 * l21;
    if (!isfinite(rad) || rad < VDIVIDE_TOL)
	return -1;
    l22 = sqrt(rad);
    if (!isfinite(l22) || NEAR_ZERO(l22, VDIVIDE_TOL))
	return -1;

    a = 1.0 / l11;
    c = -l21 / (l11 * l22);
    d = 1.0 / l22;
    b1 = body_curve->crv_c1 / alpha;
    b2 = body_curve->crv_c2 / alpha;

    h00 = a * a * b1;
    h01 = a * c * b1;
    h11 = c * c * b1 + d * d * b2;

    bn_eigen2x2(&out->crv_c1, &out->crv_c2, ev1, ev2, h00, h01, h11);

    v1 = ev1[Y] / l22;
    v0 = (ev1[X] - l21 * v1) / l11;
    VCOMB2(out->crv_pdir, v0, t1, v1, t2);
    normal_dot = VDOT(out->crv_pdir, model_n);
    VJOIN1(out->crv_pdir, out->crv_pdir, -normal_dot, model_n);

    return unitize_checked(out->crv_pdir);
}


static int
near_curve(const struct curvature *expected, const struct curvature *got)
{
    if (!near_value(expected->crv_c1, got->crv_c1) ||
	!near_value(expected->crv_c2, got->crv_c2))
	return 0;

    return fabs(VDOT(expected->crv_pdir, got->crv_pdir)) > 1.0 - TANGENT_TOL;
}


static int
valid_affine_sample(const struct curve_sample *sample)
{
    fastf_t pdir_mag = MAGNITUDE(sample->curve.crv_pdir);

    if (sample->curve_ret != 0 || !curve_finite(&sample->curve))
	return 0;
    if (fabs(sample->curve.crv_c1) > fabs(sample->curve.crv_c2) + CURVE_TOL)
	return 0;
    if (fabs(pdir_mag - 1.0) > TANGENT_TOL)
	return 0;
    if (fabs(VDOT(sample->curve.crv_pdir, sample->normal)) > TANGENT_TOL)
	return 0;

    return 1;
}


static int
write_pipe(struct rt_wdb *wdbp, const char *name)
{
    struct bu_list head;
    point_t p0 = {0.0, 0.0, 0.0};
    point_t p1 = {0.0, 0.0, 16.0};
    int ret;

    mk_pipe_init(&head);
    mk_add_pipe_pnt(&head, p0, 4.0, 0.0, 5.0);
    mk_add_pipe_pnt(&head, p1, 4.0, 0.0, 5.0);
    ret = mk_pipe(wdbp, name, &head);
    mk_pipe_free(&head);
    return ret;
}


static int
write_metaball(struct rt_wdb *wdbp, const char *name)
{
    fastf_t mb0[5] = {-2.0, 0.0, 0.0, 3.0, 0.0};
    fastf_t mb1[5] = {2.0, 0.0, 0.0, 3.0, 0.0};
    fastf_t mb2[5] = {0.0, 2.0, 1.0, 2.5, 0.0};
    const fastf_t *verts[5] = {mb0, mb1, mb2, NULL, NULL};

    return mk_metaball(wdbp, name, 3, METABALL_ISOPOTENTIAL, 1.0, verts);
}


static int
write_primitive(struct rt_wdb *wdbp, const char *type, const char *name)
{
    point_t v = {0.0, 0.0, 0.0};
    vect_t h = {0.0, 0.0, 12.0};
    vect_t b = {0.0, 6.0, 0.0};
    vect_t a = {4.0, 0.0, 0.0};
    vect_t au = {1.0, 0.0, 0.0};
    vect_t n = {0.0, 0.0, 1.0};
    vect_t c = {3.0, 0.0, 0.0};

    if (BU_STR_EQUAL(type, "tor"))
	return mk_tor(wdbp, name, v, n, 6.0, 1.5);
    if (BU_STR_EQUAL(type, "eto"))
	return mk_eto(wdbp, name, v, n, c, 6.0, 1.2);
    if (BU_STR_EQUAL(type, "epa"))
	return mk_epa(wdbp, name, v, h, au, 4.0, 2.5);
    if (BU_STR_EQUAL(type, "ehy"))
	return mk_ehy(wdbp, name, v, h, au, 4.0, 2.5, 16.0);
    if (BU_STR_EQUAL(type, "rpc"))
	return mk_rpc(wdbp, name, v, h, b, 2.5);
    if (BU_STR_EQUAL(type, "rhc"))
	return mk_rhc(wdbp, name, v, h, b, 2.5, 5.0);
    if (BU_STR_EQUAL(type, "particle"))
	return mk_particle(wdbp, name, v, h, 2.0, 3.0);
    if (BU_STR_EQUAL(type, "hyp"))
	return mk_hyp(wdbp, name, v, h, a, 2.5, 0.45);
    if (BU_STR_EQUAL(type, "cline"))
	return mk_cline(wdbp, name, v, h, 2.0, 0.5);
    if (BU_STR_EQUAL(type, "pipe"))
	return write_pipe(wdbp, name);
    if (BU_STR_EQUAL(type, "metaball"))
	return write_metaball(wdbp, name);

    return -1;
}


static int
set_nonuniform_attr(struct db_i *dbip, const char *name, const mat_t mat)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    struct rt_db_internal intern;
    struct bu_vls value = BU_VLS_INIT_ZERO;
    int ret = 0;
    int i;

    if (!dp)
	return -1;

    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return -1;

    for (i = 0; i < 16; i++) {
	if (i)
	    bu_vls_putc(&value, ',');
	bu_vls_printf(&value, "%.17g", mat[i]);
    }

    if (bu_avs_add(&intern.idb_avs, RT_MATRIX_NONUNIFORM_ATTR, bu_vls_cstr(&value)) < 0)
	ret = -1;
    else if (rt_db_put_internal(dp, dbip, &intern) < 0)
	ret = -1;

    bu_vls_free(&value);
    rt_db_free_internal(&intern);
    return ret;
}


static int
curve_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct curve_sample *sample = (struct curve_sample *)ap->a_uptr;
    struct partition *pp = PartHeadp->pt_forw;

    if (pp == PartHeadp)
	return 0;

    RT_HIT_NORMAL(sample->normal, pp->pt_inhit, pp->pt_inseg->seg_stp, NULL, pp->pt_inflip);
    sample->curve_ret = rt_obj_curve(&sample->curve, pp->pt_inhit, pp->pt_inseg->seg_stp);
    if (pp->pt_inflip) {
	sample->curve.crv_c1 = -sample->curve.crv_c1;
	sample->curve.crv_c2 = -sample->curve.crv_c2;
    }
    sample->hit = 1;
    return 1;
}


static int
curve_miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
shoot_curve(struct db_i *dbip, const char *objname, const struct xray *ray, struct curve_sample *sample)
{
    struct rt_i *rtip;
    struct application ap;

    memset(sample, 0, sizeof(struct curve_sample));
    sample->curve_ret = -999;

    rtip = rt_i_create(dbip);
    if (!rtip)
	return -1;

    if (rt_gettree(rtip, objname) != 0) {
	rt_i_destroy(rtip);
	return -1;
    }
    rt_prep_parallel(rtip, 1);

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_onehit = 1;
    ap.a_uptr = sample;
    ap.a_hit = curve_hit;
    ap.a_miss = curve_miss;
    ap.a_ray = *ray;

    rt_shootray(&ap);
    rt_i_destroy(rtip);

    return sample->hit ? 0 : -1;
}


static int
test_enabled_primitive(struct db_i *dbip, const char *name)
{
    char base[64], uni[64];
    mat_t identity, uniform_mat;
    int uniform_ok = 0;
    int affine_ok[sizeof(affine_cases) / sizeof(affine_cases[0])] = {0};

    snprintf(base, sizeof(base), "%s_base.s", name);
    snprintf(uni, sizeof(uni), "%s_uniform.s", name);

    MAT_IDN(identity);
    make_uniform_mat(uniform_mat, 2.0);

    for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); i++) {
	struct xray base_ray, uni_ray, nonu_ray;
	struct curve_sample base_sample, uni_sample, nonu_sample;

	transform_ray(&base_ray, &rays[i], identity);
	if (shoot_curve(dbip, base, &base_ray, &base_sample) < 0 ||
	    base_sample.curve_ret != 0 ||
	    !curve_finite(&base_sample.curve) ||
	    !significant_curve(&base_sample.curve))
	    continue;

	transform_ray(&uni_ray, &rays[i], uniform_mat);
	if (shoot_curve(dbip, uni, &uni_ray, &uni_sample) == 0 &&
	    valid_affine_sample(&uni_sample) &&
	    near_value(uni_sample.curve.crv_c1, base_sample.curve.crv_c1 / 2.0) &&
	    near_value(uni_sample.curve.crv_c2, base_sample.curve.crv_c2 / 2.0))
	    uniform_ok = 1;

	for (size_t j = 0; j < sizeof(affine_cases) / sizeof(affine_cases[0]); j++) {
	    char nonu[64];
	    mat_t affine_mat, inv_mat;
	    struct curvature expected;

	    if (affine_ok[j])
		continue;

	    snprintf(nonu, sizeof(nonu), "%s_%s.s", name, affine_cases[j].name);
	    make_affine_mat(affine_mat, &affine_cases[j]);
	    if (!bn_mat_inverse(inv_mat, affine_mat))
		continue;

	    transform_ray(&nonu_ray, &rays[i], affine_mat);
	    if (expected_affine_curve(&expected, &base_sample.curve, base_sample.normal, affine_mat, inv_mat) == 0 &&
		shoot_curve(dbip, nonu, &nonu_ray, &nonu_sample) == 0 &&
		valid_affine_sample(&nonu_sample) &&
		near_curve(&expected, &nonu_sample.curve))
		affine_ok[j] = 1;
	}

	if (uniform_ok) {
	    int all_affine_ok = 1;
	    for (size_t j = 0; j < sizeof(affine_cases) / sizeof(affine_cases[0]); j++)
		all_affine_ok = all_affine_ok && affine_ok[j];
	    if (all_affine_ok)
		return 0;
	}
    }

    if (!uniform_ok)
	bu_log("[nonuniform_curve] %s failed uniform-scale curvature check\n", name);
    for (size_t j = 0; j < sizeof(affine_cases) / sizeof(affine_cases[0]); j++) {
	if (!affine_ok[j])
	    bu_log("[nonuniform_curve] %s failed %s curvature check\n", name, affine_cases[j].name);
    }

    return -1;
}


static int
test_disabled_primitive(struct db_i *dbip, const char *name)
{
    for (size_t j = 0; j < sizeof(affine_cases) / sizeof(affine_cases[0]); j++) {
	char nonu[64];
	mat_t affine_mat;
	int unavailable_ok = 0;

	snprintf(nonu, sizeof(nonu), "%s_%s.s", name, affine_cases[j].name);
	make_affine_mat(affine_mat, &affine_cases[j]);

	for (size_t i = 0; i < sizeof(rays) / sizeof(rays[0]); i++) {
	    struct xray ray;
	    struct curve_sample sample;

	    transform_ray(&ray, &rays[i], affine_mat);
	    if (shoot_curve(dbip, nonu, &ray, &sample) < 0)
		continue;

	    if (sample.curve_ret < 0) {
		unavailable_ok = 1;
		break;
	    }

	    bu_log("[nonuniform_curve] %s unexpectedly returned %s affine curvature\n", name, affine_cases[j].name);
	    return -1;
	}

	if (!unavailable_ok) {
	    bu_log("[nonuniform_curve] %s did not produce a transformed hit for %s disabled-curve check\n", name, affine_cases[j].name);
	    return -1;
	}
    }

    return 0;
}


static int
build_db(struct db_i **dbip_out, struct rt_wdb **wdbp_out)
{
    struct db_i *dbip = db_open_inmem();
    struct rt_wdb *wdbp;
    mat_t uniform_mat;

    if (!dbip)
	return -1;

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return -1;
    }

    make_uniform_mat(uniform_mat, 2.0);

    for (size_t i = 0; i < sizeof(prims) / sizeof(prims[0]); i++) {
	char base[64], uni[64];

	snprintf(base, sizeof(base), "%s_base.s", prims[i].name);
	snprintf(uni, sizeof(uni), "%s_uniform.s", prims[i].name);

	if (write_primitive(wdbp, prims[i].name, base) < 0 ||
	    write_primitive(wdbp, prims[i].name, uni) < 0) {
	    wdb_close(wdbp);
	    return -1;
	}

	if (set_nonuniform_attr(dbip, uni, uniform_mat) < 0) {
	    wdb_close(wdbp);
	    return -1;
	}

	for (size_t j = 0; j < sizeof(affine_cases) / sizeof(affine_cases[0]); j++) {
	    char nonu[64];
	    mat_t affine_mat;

	    snprintf(nonu, sizeof(nonu), "%s_%s.s", prims[i].name, affine_cases[j].name);
	    make_affine_mat(affine_mat, &affine_cases[j]);

	    if (write_primitive(wdbp, prims[i].name, nonu) < 0 ||
		set_nonuniform_attr(dbip, nonu, affine_mat) < 0) {
		wdb_close(wdbp);
		return -1;
	    }
	}
    }

    db_update_nref(dbip);
    *dbip_out = dbip;
    *wdbp_out = wdbp;
    return 0;
}


int
main(int argc, char *argv[])
{
    struct db_i *dbip = DBI_NULL;
    struct rt_wdb *wdbp = RT_WDB_NULL;
    int failures = 0;

    bu_setprogname(argv[0]);
    (void)argc;

    if (build_db(&dbip, &wdbp) < 0)
	bu_exit(1, "[nonuniform_curve] failed to build test database\n");

    for (size_t i = 0; i < sizeof(prims) / sizeof(prims[0]); i++) {
	if (prims[i].affine_enabled) {
	    if (test_enabled_primitive(dbip, prims[i].name) < 0)
		failures++;
	} else {
	    if (test_disabled_primitive(dbip, prims[i].name) < 0)
		failures++;
	}
    }

    wdb_close(wdbp);
    return failures ? 1 : 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
