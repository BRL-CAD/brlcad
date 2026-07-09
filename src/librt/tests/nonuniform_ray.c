/*                   N O N U N I F O R M _ R A Y . C
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
/** @file librt/tests/nonuniform_ray.c
 *
 * Ray-grid and silhouette-mask tests for matrix:nonuniform primitives.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../librt_private.h"


#define POINT_TOL 1.0e-4
#define NORMAL_DOT_TOL 0.995
#define SPARSE_GRID 16
#define MASK_GRID 40


struct prim_case {
    const char *name;
    int mask_tolerance;
};


struct scale_case {
    const char *name;
    fastf_t sx;
    fastf_t sy;
    fastf_t sz;
    fastf_t rx;
    fastf_t ry;
    fastf_t rz;
};


struct view_case {
    const char *name;
    vect_t dir;
    vect_t u;
    vect_t v;
};


struct ray_sample {
    int hit;
    point_t in_pt;
    point_t out_pt;
    vect_t in_norm;
    vect_t out_norm;
};


static const struct prim_case prims[] = {
    {"tor", 0},
    {"eto", 0},
    {"epa", 0},
    {"ehy", 0},
    {"rpc", 0},
    {"rhc", 0},
    {"particle", 0},
    {"hyp", 0},
    {"cline", 0},
    {"pipe", 0},
    /* Metaball shooting is iterative; sparse affine hit/normal checks remain exact. */
    {"metaball", 2}
};


static const struct scale_case scales[] = {
    {"x", 1.7, 1.0, 1.0, 0.0, 0.0, 0.0},
    {"y", 1.0, 0.65, 1.0, 0.0, 0.0, 0.0},
    {"z", 1.0, 1.0, 1.3, 0.0, 0.0, 0.0},
    {"xyz", 1.7, 0.65, 1.3, 0.0, 0.0, 0.0},
    {"rot_xyz", 1.7, 0.65, 1.3, 17.0, -23.0, 31.0},
    {"rot_xz", 1.7, 1.0, 0.55, -12.0, 28.0, 9.0}
};


static void
make_scale_mat(mat_t mat, const struct scale_case *sc)
{
    mat_t rot, scale;

    MAT_IDN(scale);
    scale[0] = sc->sx;
    scale[5] = sc->sy;
    scale[10] = sc->sz;

    bn_mat_angles(rot, sc->rx, sc->ry, sc->rz);
    bn_mat_mul(mat, rot, scale);
    mat[3] = 12.0;
    mat[7] = -5.0;
    mat[11] = 3.0;
}


static void
make_views(struct view_case views[4])
{
    vect_t up;

    views[0].name = "x";
    VSET(views[0].dir, -1.0, 0.0, 0.0);
    VSET(views[0].u, 0.0, 1.0, 0.0);
    VSET(views[0].v, 0.0, 0.0, 1.0);

    views[1].name = "y";
    VSET(views[1].dir, 0.0, -1.0, 0.0);
    VSET(views[1].u, 1.0, 0.0, 0.0);
    VSET(views[1].v, 0.0, 0.0, 1.0);

    views[2].name = "z";
    VSET(views[2].dir, 0.0, 0.0, -1.0);
    VSET(views[2].u, 1.0, 0.0, 0.0);
    VSET(views[2].v, 0.0, 1.0, 0.0);

    views[3].name = "obl";
    VSET(views[3].dir, -0.8, -0.55, -0.35);
    VUNITIZE(views[3].dir);
    VSET(up, 0.0, 0.0, 1.0);
    VCROSS(views[3].u, up, views[3].dir);
    VUNITIZE(views[3].u);
    VCROSS(views[3].v, views[3].dir, views[3].u);
    VUNITIZE(views[3].v);
}


static void
make_body_ray(struct xray *ray, const struct view_case *view, int ix, int iy, int n)
{
    point_t center = {0.0, 0.0, 5.5};
    fastf_t span = 18.0;
    fastf_t x = 0.0;
    fastf_t y = 0.0;

    memset(ray, 0, sizeof(struct xray));
    ray->magic = RT_RAY_MAGIC;

    if (n > 0) {
	x = ((((fastf_t)ix + 0.5) / (fastf_t)n) * 2.0 - 1.0) * span;
	y = ((((fastf_t)iy + 0.5) / (fastf_t)n) * 2.0 - 1.0) * span;
    }

    VJOIN3(ray->r_pt, center, -45.0, view->dir, x, view->u, y, view->v);
    VMOVE(ray->r_dir, view->dir);
    ray->r_min = 0.0;
    ray->r_max = INFINITY;
}


static void
transform_ray(struct xray *out, const struct xray *in, const mat_t mat)
{
    vect_t dir;

    memset(out, 0, sizeof(struct xray));
    out->magic = RT_RAY_MAGIC;
    MAT4X3PNT(out->r_pt, mat, in->r_pt);
    MAT4X3VEC(dir, mat, in->r_dir);
    VMOVE(out->r_dir, dir);
    VUNITIZE(out->r_dir);
    out->r_min = 0.0;
    out->r_max = INFINITY;
}


static void
transform_normal(vect_t out, const vect_t normal, const mat_t inv_mat)
{
    VEC3X3MAT(out, normal, inv_mat);
    VUNITIZE(out);
}


static int
valid_vec(const vect_t v)
{
    return isfinite(v[X]) && isfinite(v[Y]) && isfinite(v[Z]);
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
sample_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct ray_sample *sample = (struct ray_sample *)ap->a_uptr;
    struct partition *pp = PartHeadp->pt_forw;

    if (pp == PartHeadp)
	return 0;

    sample->hit = 1;
    VJOIN1(sample->in_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VJOIN1(sample->out_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(sample->in_norm, pp->pt_inhit, pp->pt_inseg->seg_stp, NULL, pp->pt_inflip);
    RT_HIT_NORMAL(sample->out_norm, pp->pt_outhit, pp->pt_outseg->seg_stp, NULL, pp->pt_outflip);
    return 1;
}


static int
sample_miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
shoot_sample(struct rt_i *rtip, const struct xray *ray, struct ray_sample *sample)
{
    struct application ap;

    memset(sample, 0, sizeof(struct ray_sample));
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_onehit = 1;
    ap.a_uptr = sample;
    ap.a_hit = sample_hit;
    ap.a_miss = sample_miss;
    ap.a_ray = *ray;

    rt_shootray(&ap);
    return 0;
}


static struct rt_i *
prep_object(struct db_i *dbip, const char *name)
{
    struct rt_i *rtip = rt_i_create(dbip);

    if (!rtip)
	return RTI_NULL;

    if (rt_gettree(rtip, name) != 0) {
	rt_i_destroy(rtip);
	return RTI_NULL;
    }

    rt_prep_parallel(rtip, 1);
    return rtip;
}


static int
compare_hit_point(const point_t expected, const point_t got)
{
    return DIST_PNT_PNT(expected, got) <= POINT_TOL;
}


static int
compare_normal(const vect_t expected, const vect_t got)
{
    if (!valid_vec(expected) || !valid_vec(got))
	return 0;

    return VDOT(expected, got) >= NORMAL_DOT_TOL;
}


static int
compare_sparse_sample(const char *label, const mat_t mat, const mat_t inv_mat, const struct ray_sample *base, const struct ray_sample *scaled)
{
    point_t expected_in, expected_out;
    vect_t expected_in_norm, expected_out_norm;

    if (base->hit != scaled->hit) {
	bu_log("[nonuniform_ray] %s hit mismatch: base=%d scaled=%d\n", label, base->hit, scaled->hit);
	return -1;
    }

    if (!base->hit)
	return 0;

    MAT4X3PNT(expected_in, mat, base->in_pt);
    MAT4X3PNT(expected_out, mat, base->out_pt);
    transform_normal(expected_in_norm, base->in_norm, inv_mat);
    transform_normal(expected_out_norm, base->out_norm, inv_mat);

    if (!compare_hit_point(expected_in, scaled->in_pt) ||
	!compare_hit_point(expected_out, scaled->out_pt)) {
	if (compare_hit_point(expected_out, scaled->out_pt) &&
	    compare_normal(expected_out_norm, scaled->out_norm))
	    return 0;

	bu_log("[nonuniform_ray] %s hit point mismatch\n", label);
	return -1;
    }

    if (!compare_normal(expected_in_norm, scaled->in_norm) ||
	!compare_normal(expected_out_norm, scaled->out_norm)) {
	bu_log("[nonuniform_ray] %s normal mismatch\n", label);
	return -1;
    }

    return 0;
}


static int
run_grid_compare(struct rt_i *base_rtip, struct rt_i *scaled_rtip, const struct view_case *view, const mat_t mat, const mat_t inv_mat, int n, int sparse, int mask_tolerance, const char *label)
{
    int failures = 0;
    int base_hits = 0;
    int scaled_hits = 0;
    int mask_mismatches = 0;
    int ix, iy;

    for (iy = 0; iy < n; iy++) {
	for (ix = 0; ix < n; ix++) {
	    struct xray base_ray, scaled_ray;
	    struct ray_sample base_sample, scaled_sample;

	    make_body_ray(&base_ray, view, ix, iy, n);
	    transform_ray(&scaled_ray, &base_ray, mat);

	    shoot_sample(base_rtip, &base_ray, &base_sample);
	    shoot_sample(scaled_rtip, &scaled_ray, &scaled_sample);

	    if (base_sample.hit)
		base_hits++;
	    if (scaled_sample.hit)
		scaled_hits++;

	    if (base_sample.hit != scaled_sample.hit) {
		if (sparse)
		    failures++;
		else
		    mask_mismatches++;
		continue;
	    }

	    if (sparse && compare_sparse_sample(label, mat, inv_mat, &base_sample, &scaled_sample) < 0)
		failures++;
	}
    }

    if (!sparse && mask_mismatches > mask_tolerance) {
	bu_log("[nonuniform_ray] %s silhouette mismatch count exceeded tolerance: mismatches=%d tolerance=%d\n", label, mask_mismatches, mask_tolerance);
	failures++;
    }

    if (base_hits != scaled_hits && (sparse || abs(base_hits - scaled_hits) > mask_tolerance)) {
	bu_log("[nonuniform_ray] %s silhouette count mismatch: base=%d scaled=%d\n", label, base_hits, scaled_hits);
	failures++;
    }

    return failures ? -1 : 0;
}


static int
run_case(struct db_i *dbip, const struct prim_case *pc, const struct scale_case *sc, const struct view_case *view)
{
    char base_name[64];
    char scaled_name[64];
    char label[128];
    mat_t mat, inv_mat;
    struct rt_i *base_rtip;
    struct rt_i *scaled_rtip;
    int ret = 0;

    snprintf(base_name, sizeof(base_name), "%s_base.s", pc->name);
    snprintf(scaled_name, sizeof(scaled_name), "%s_%s.s", pc->name, sc->name);
    snprintf(label, sizeof(label), "%s/%s/%s", pc->name, sc->name, view->name);

    make_scale_mat(mat, sc);
    if (!bn_mat_inverse(inv_mat, mat)) {
	bu_log("[nonuniform_ray] %s scale matrix is not invertible\n", label);
	return -1;
    }

    base_rtip = prep_object(dbip, base_name);
    scaled_rtip = prep_object(dbip, scaled_name);
    if (!base_rtip || !scaled_rtip) {
	bu_log("[nonuniform_ray] %s prep failed\n", label);
	if (base_rtip)
	    rt_i_destroy(base_rtip);
	if (scaled_rtip)
	    rt_i_destroy(scaled_rtip);
	return -1;
    }

    if (run_grid_compare(base_rtip, scaled_rtip, view, mat, inv_mat, SPARSE_GRID, 1, 0, label) < 0)
	ret = -1;

    if (run_grid_compare(base_rtip, scaled_rtip, view, mat, inv_mat, MASK_GRID, 0, pc->mask_tolerance, label) < 0)
	ret = -1;

    rt_i_destroy(base_rtip);
    rt_i_destroy(scaled_rtip);
    return ret;
}


static int
build_db(struct db_i **dbip_out, struct rt_wdb **wdbp_out)
{
    struct db_i *dbip = db_open_inmem();
    struct rt_wdb *wdbp;
    size_t i, j;

    if (!dbip)
	return -1;

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return -1;
    }

    for (i = 0; i < sizeof(prims) / sizeof(prims[0]); i++) {
	char base_name[64];

	snprintf(base_name, sizeof(base_name), "%s_base.s", prims[i].name);
	if (write_primitive(wdbp, prims[i].name, base_name) < 0) {
	    wdb_close(wdbp);
	    return -1;
	}

	for (j = 0; j < sizeof(scales) / sizeof(scales[0]); j++) {
	    char scaled_name[64];
	    mat_t mat;

	    snprintf(scaled_name, sizeof(scaled_name), "%s_%s.s", prims[i].name, scales[j].name);
	    make_scale_mat(mat, &scales[j]);

	    if (write_primitive(wdbp, prims[i].name, scaled_name) < 0 ||
		set_nonuniform_attr(dbip, scaled_name, mat) < 0) {
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
    struct view_case views[4];
    int failures = 0;
    size_t i, j, k;

    bu_setprogname(argv[0]);
    (void)argc;

    make_views(views);

    if (build_db(&dbip, &wdbp) < 0)
	bu_exit(1, "[nonuniform_ray] failed to build test database\n");

    for (i = 0; i < sizeof(prims) / sizeof(prims[0]); i++) {
	for (j = 0; j < sizeof(scales) / sizeof(scales[0]); j++) {
	    for (k = 0; k < sizeof(views) / sizeof(views[0]); k++) {
		if (run_case(dbip, &prims[i], &scales[j], &views[k]) < 0)
		    failures++;
	    }
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
