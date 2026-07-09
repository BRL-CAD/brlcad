/*                   N O N U N I F O R M . C P P
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
/** @file nonuniform.cpp
 *
 * Integration coverage for v5 matrix:nonuniform preservation.  For each
 * scalar-radius primitive, build an axis-aligned and an oblique baseline,
 * facetize it, then npush -x transformed CSG and BoT wrapper instances
 * through X, Y, Z, and stacked XYZ scale combinations.
 */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "nmg.h"
#include "wdb.h"
#include "ged.h"

#define NONUNIFORM_ATTR "matrix:nonuniform"
#define VOL_TOL_CSG 0.03
#define VOL_TOL_BOT 0.03
#define RATIO_TOL 0.05
#define SA_TOL_CSG_TESS 0.18
#define SA_TOL_BOT_TESS 0.08
#define NORMAL_DOT_TOL 0.995

struct frame {
    point_t o;
    vect_t x;
    vect_t y;
    vect_t z;
};

struct prim_case {
    const char *name;
};

struct scale_case {
    const char *name;
    fastf_t sx;
    fastf_t sy;
    fastf_t sz;
};

static const prim_case prims[] = {
    {"tor"},
    {"eto"},
    {"epa"},
    {"ehy"},
    {"rpc"},
    {"rhc"},
    {"particle"},
    {"hyp"},
    {"cline"},
    {"pipe"},
    {"metaball"}
};

static const scale_case scales[] = {
    {"x", 1.7, 1.0, 1.0},
    {"y", 1.0, 0.65, 1.0},
    {"z", 1.0, 1.0, 1.3},
    {"xy", 1.7, 0.65, 1.0},
    {"xz", 1.7, 1.0, 1.3},
    {"yz", 1.0, 0.65, 1.3},
    {"xyz", 1.7, 0.65, 1.3}
};

static void
make_frame(frame *f, bool oblique)
{
    if (!oblique) {
	VSET(f->o, 0.0, 0.0, 0.0);
	VSET(f->x, 1.0, 0.0, 0.0);
	VSET(f->y, 0.0, 1.0, 0.0);
	VSET(f->z, 0.0, 0.0, 1.0);
	return;
    }

    vect_t up;
    VSET(f->o, 100.0, -40.0, 25.0);
    VSET(f->z, 0.35, 0.45, 0.82);
    VUNITIZE(f->z);
    VSET(up, 0.0, 0.0, 1.0);
    VCROSS(f->x, up, f->z);
    VUNITIZE(f->x);
    VCROSS(f->y, f->z, f->x);
    VUNITIZE(f->y);
}

static void
local_pnt(point_t out, const frame *f, fastf_t lx, fastf_t ly, fastf_t lz)
{
    VJOIN3(out, f->o, lx, f->x, ly, f->y, lz, f->z);
}

static void
local_vec(vect_t out, const frame *f, fastf_t lx, fastf_t ly, fastf_t lz)
{
    vect_t zero;
    VSETALL(zero, 0.0);
    VJOIN3(out, zero, lx, f->x, ly, f->y, lz, f->z);
}

static int
write_pipe(struct rt_wdb *wdbp, const char *name, const frame *f)
{
    struct bu_list head;
    point_t p0, p1, p2;

    local_pnt(p0, f, -4.0, 0.0, 0.0);
    local_pnt(p1, f, 0.0, 0.0, 10.0);
    local_pnt(p2, f, 5.0, 0.0, 15.0);

    mk_pipe_init(&head);
    mk_add_pipe_pnt(&head, p0, 4.0, 1.0, 5.0);
    mk_add_pipe_pnt(&head, p1, 4.0, 1.0, 5.0);
    mk_add_pipe_pnt(&head, p2, 4.0, 1.0, 5.0);
    int ret = mk_pipe(wdbp, name, &head);
    mk_pipe_free(&head);
    return ret;
}

static int
write_metaball(struct rt_wdb *wdbp, const char *name, const frame *f)
{
    point_t p0, p1, p2;
    local_pnt(p0, f, -2.0, 0.0, 0.0);
    local_pnt(p1, f, 2.0, 0.0, 0.0);
    local_pnt(p2, f, 0.0, 2.0, 1.0);

    fastf_t mb0[5] = {p0[X], p0[Y], p0[Z], 3.0, 0.0};
    fastf_t mb1[5] = {p1[X], p1[Y], p1[Z], 3.0, 0.0};
    fastf_t mb2[5] = {p2[X], p2[Y], p2[Z], 2.5, 0.0};
    const fastf_t *verts[3] = {mb0, mb1, mb2};

    return mk_metaball(wdbp, name, 3, METABALL_ISOPOTENTIAL, 1.0, verts);
}

static int
write_primitive(struct rt_wdb *wdbp, const prim_case &pc, const char *name, const frame *f)
{
    point_t v;
    vect_t h, b, a, n, c;

    local_pnt(v, f, 0.0, 0.0, 0.0);
    local_vec(h, f, 0.0, 0.0, 12.0);
    local_vec(b, f, 0.0, 6.0, 0.0);
    local_vec(a, f, 4.0, 0.0, 0.0);
    local_vec(n, f, 0.0, 0.0, 1.0);
    local_vec(c, f, 3.0, 0.0, 0.0);

    if (BU_STR_EQUAL(pc.name, "tor"))
	return mk_tor(wdbp, name, v, n, 6.0, 1.5);
    if (BU_STR_EQUAL(pc.name, "eto"))
	return mk_eto(wdbp, name, v, n, c, 6.0, 1.2);
    if (BU_STR_EQUAL(pc.name, "epa"))
	return mk_epa(wdbp, name, v, h, f->x, 4.0, 2.5);
    if (BU_STR_EQUAL(pc.name, "ehy"))
	return mk_ehy(wdbp, name, v, h, f->x, 4.0, 2.5, 16.0);
    if (BU_STR_EQUAL(pc.name, "rpc"))
	return mk_rpc(wdbp, name, v, h, b, 2.5);
    if (BU_STR_EQUAL(pc.name, "rhc"))
	return mk_rhc(wdbp, name, v, h, b, 2.5, 5.0);
    if (BU_STR_EQUAL(pc.name, "particle"))
	return mk_particle(wdbp, name, v, h, 2.0, 3.0);
    if (BU_STR_EQUAL(pc.name, "hyp"))
	return mk_hyp(wdbp, name, v, h, a, 2.5, 0.45);
    if (BU_STR_EQUAL(pc.name, "cline"))
	return mk_cline(wdbp, name, v, h, 2.0, 0.5);
    if (BU_STR_EQUAL(pc.name, "pipe"))
	return write_pipe(wdbp, name, f);
    if (BU_STR_EQUAL(pc.name, "metaball"))
	return write_metaball(wdbp, name, f);

    return BRLCAD_ERROR;
}

static int
run_ged_cmd(struct ged *gedp, std::vector<const char *> av, const char *ctx)
{
    int ret = ged_exec(gedp, (int)av.size(), av.data());
    if (ret != BRLCAD_OK) {
	const char *log = bu_vls_cstr(gedp->ged_result_str);
	bu_log("[nonuniform] %s failed", ctx);
	if (log && log[0])
	    bu_log(": %s", log);
	bu_log("\n");
    }
    bu_vls_trunc(gedp->ged_result_str, 0);
    return ret;
}

static int
copy_obj(struct ged *gedp, const char *src, const char *dst)
{
    std::vector<const char *> av = {"copy", src, dst};
    return run_ged_cmd(gedp, av, "copy");
}

static int
facetize_obj(struct ged *gedp, const char *src, const char *dst)
{
    std::vector<const char *> av = {"facetize", src, dst};
    return run_ged_cmd(gedp, av, "facetize");
}

static void
scale_mat(mat_t mat, const scale_case &sc)
{
    MAT_IDN(mat);
    mat[0] = sc.sx;
    mat[5] = sc.sy;
    mat[10] = sc.sz;
}

static int
write_wrapper(struct ged *gedp, const char *comb, const char *leaf, mat_t mat)
{
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return BRLCAD_ERROR;

    struct wmember wm;
    BU_LIST_INIT(&wm.l);
    mk_addmember(leaf, &wm.l, mat, WMOP_UNION);
    return mk_lcomb(wdbp, comb, &wm, 0, NULL, NULL, NULL, 0);
}

static int
npush_x(struct ged *gedp, const char *comb)
{
    std::vector<const char *> av = {"npush", "-x", comb};
    return run_ged_cmd(gedp, av, "npush -x");
}

static int
object_volume(struct db_i *dbip, const char *name, fastf_t *vol)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return BRLCAD_ERROR;

    int ret = rt_obj_volume(vol, &intern);
    rt_db_free_internal(&intern);
    if (ret < 0 || !std::isfinite(*vol) || *vol <= 0.0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
object_surface_area(struct db_i *dbip, const char *name, fastf_t *area)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return BRLCAD_ERROR;

    int ret = rt_obj_surf_area(area, &intern);
    rt_db_free_internal(&intern);
    if (ret < 0 || !std::isfinite(*area) || *area <= 0.0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
object_tess_surface_area(struct db_i *dbip, const char *name, fastf_t *area)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return BRLCAD_ERROR;

    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    ttol.abs = 0.05;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    struct model *m = nmg_mm();
    struct nmgregion *r = NULL;
    int ret = rt_obj_tess(&r, m, &intern, &ttol, &tol);
    if (ret == 0 && r)
	*area = nmg_region_area(r);
    nmg_km(m);
    rt_db_free_internal(&intern);

    if (ret < 0 || !std::isfinite(*area) || *area <= 0.0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
object_bbox(struct db_i *dbip, const char *name, point_t bmin, point_t bmax)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    if (rt_bound_internal(dbip, dp, bmin, bmax) < 0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
has_nonuniform_attr(struct db_i *dbip, const char *name)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp)
	return 0;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return 0;

    const char *attr = bu_avs_get(&intern.idb_avs, NONUNIFORM_ATTR);
    int ret = (attr && attr[0]);
    rt_db_free_internal(&intern);
    return ret;
}

struct normal_sample {
    int hit;
    vect_t normal;
};

static int
normal_hit_fn(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    normal_sample *sample = (normal_sample *)ap->a_uptr;
    struct partition *pp = PartHeadp->pt_forw;
    vect_t normal;

    if (pp == PartHeadp)
	return 0;

    RT_HIT_NORMAL(normal, pp->pt_inhit, pp->pt_inseg->seg_stp, NULL, pp->pt_inflip);
    VMOVE(sample->normal, normal);
    sample->hit = 1;
    return 1;
}

static int
normal_miss_fn(struct application *ap)
{
    (void)ap;
    return 0;
}

static int
object_normal(struct db_i *dbip, const char *name, const point_t origin, const vect_t dir, vect_t normal)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return BRLCAD_ERROR;
    if (rt_gettree(rtip, name) < 0) {
	rt_i_destroy(rtip);
	return BRLCAD_ERROR;
    }
    rt_prep_parallel(rtip, 1);

    normal_sample sample;
    sample.hit = 0;
    VSETALL(sample.normal, 0.0);
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    VMOVE(ap.a_ray.r_pt, origin);
    VMOVE(ap.a_ray.r_dir, dir);
    ap.a_onehit = 1;
    ap.a_uptr = &sample;
    ap.a_hit = normal_hit_fn;
    ap.a_miss = normal_miss_fn;

    rt_shootray(&ap);
    rt_i_destroy(rtip);

    if (!sample.hit)
	return BRLCAD_ERROR;

    VMOVE(normal, sample.normal);
    return BRLCAD_OK;
}

static int
check_scaled_volume(const char *label, fastf_t base, fastf_t got, fastf_t det, fastf_t tol)
{
    fastf_t expect = base * det;
    fastf_t rel = std::fabs((got - expect) / expect);
    if (rel > tol) {
	bu_log("[nonuniform] %s volume mismatch: got %.17g expected %.17g det %.17g rel %.6g tol %.6g\n",
	       label, got, expect, det, rel, tol);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
check_no_nonuniform_attr(struct db_i *dbip, const char *name, const char *label)
{
    if (has_nonuniform_attr(dbip, name)) {
	bu_log("[nonuniform] %s unexpectedly has %s\n", label, NONUNIFORM_ATTR);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static fastf_t
csg_volume_tol_for_tag(const char *tag)
{
    if (BU_STR_EQUAL(tag, "cline_axis") || BU_STR_EQUAL(tag, "cline_oblique") ||
	BU_STR_EQUAL(tag, "metaball_axis") || BU_STR_EQUAL(tag, "metaball_oblique"))
	return 0.10;

    return VOL_TOL_CSG;
}

static fastf_t
ratio_tol_for_tag(const char *tag)
{
    if (BU_STR_EQUAL(tag, "cline_axis") || BU_STR_EQUAL(tag, "cline_oblique") ||
	BU_STR_EQUAL(tag, "metaball_axis") || BU_STR_EQUAL(tag, "metaball_oblique"))
	return 0.12;

    return RATIO_TOL;
}

static int
run_attr_minimality_case(struct ged *gedp, const char *base_csg, fastf_t base_csg_vol, const char *tag)
{
    char uniform_leaf[128], uniform_comb[128], scaled_leaf[128], scaled_comb[128], inverse_comb[128], label[192];
    fastf_t csg_vol_tol = csg_volume_tol_for_tag(tag);

    std::snprintf(uniform_leaf, sizeof(uniform_leaf), "%s_uniform_csg.s", tag);
    std::snprintf(uniform_comb, sizeof(uniform_comb), "%s_uniform_csg.c", tag);
    if (copy_obj(gedp, base_csg, uniform_leaf) != BRLCAD_OK)
	return BRLCAD_ERROR;

    mat_t uniform_mat;
    MAT_IDN(uniform_mat);
    uniform_mat[15] = 1.0 / 1.25;
    if (write_wrapper(gedp, uniform_comb, uniform_leaf, uniform_mat) != BRLCAD_OK ||
	npush_x(gedp, uniform_comb) != BRLCAD_OK)
	return BRLCAD_ERROR;

    std::snprintf(label, sizeof(label), "%s/uniform", tag);
    if (check_no_nonuniform_attr(gedp->dbip, uniform_leaf, label) != BRLCAD_OK)
	return BRLCAD_ERROR;

    fastf_t uniform_vol = -1.0;
    if (object_volume(gedp->dbip, uniform_leaf, &uniform_vol) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (check_scaled_volume(label, base_csg_vol, uniform_vol, 1.25 * 1.25 * 1.25, csg_vol_tol) != BRLCAD_OK)
	return BRLCAD_ERROR;

    std::snprintf(scaled_leaf, sizeof(scaled_leaf), "%s_cancel_csg.s", tag);
    std::snprintf(scaled_comb, sizeof(scaled_comb), "%s_cancel_scale_csg.c", tag);
    std::snprintf(inverse_comb, sizeof(inverse_comb), "%s_cancel_inverse_csg.c", tag);
    if (copy_obj(gedp, base_csg, scaled_leaf) != BRLCAD_OK)
	return BRLCAD_ERROR;

    scale_case xscale = {"x", 1.7, 1.0, 1.0};
    mat_t xmat;
    scale_mat(xmat, xscale);
    if (write_wrapper(gedp, scaled_comb, scaled_leaf, xmat) != BRLCAD_OK ||
	npush_x(gedp, scaled_comb) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (!has_nonuniform_attr(gedp->dbip, scaled_leaf)) {
	bu_log("[nonuniform] %s missing %s before inverse cancellation\n", scaled_leaf, NONUNIFORM_ATTR);
	return BRLCAD_ERROR;
    }

    mat_t invmat;
    MAT_IDN(invmat);
    invmat[0] = 1.0 / 1.7;
    if (write_wrapper(gedp, inverse_comb, scaled_leaf, invmat) != BRLCAD_OK ||
	npush_x(gedp, inverse_comb) != BRLCAD_OK)
	return BRLCAD_ERROR;

    std::snprintf(label, sizeof(label), "%s/nonuniform-cancel", tag);
    if (check_no_nonuniform_attr(gedp->dbip, scaled_leaf, label) != BRLCAD_OK)
	return BRLCAD_ERROR;

    fastf_t cancel_vol = -1.0;
    if (object_volume(gedp->dbip, scaled_leaf, &cancel_vol) != BRLCAD_OK)
	return BRLCAD_ERROR;

    return check_scaled_volume(label, base_csg_vol, cancel_vol, 1.0, csg_vol_tol);
}

static int
check_area_match(const char *label, fastf_t sampled, fastf_t tess, fastf_t tol)
{
    fastf_t rel = std::fabs((sampled - tess) / tess);
    if (rel > tol) {
	bu_log("[nonuniform] %s surface area mismatch: sampled %.17g tess %.17g rel %.6g tol %.6g\n",
	       label, sampled, tess, rel, tol);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
check_nonuniform_normal(struct ged *gedp, const char *base, const char *scaled,
			const char *label, const scale_case &sc)
{
    point_t bmin, bmax, center;
    fastf_t span;
    static const vect_t dirs[] = {
	{1.0, 0.0, 0.0},
	{-1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, -1.0, 0.0},
	{0.0, 0.0, 1.0},
	{0.0, 0.0, -1.0}
    };
    static const fastf_t offsets[] = {-0.25, 0.0, 0.25};

    if (object_bbox(gedp->dbip, scaled, bmin, bmax) != BRLCAD_OK) {
	bu_log("[nonuniform] %s normal bbox failure\n", label);
	return BRLCAD_ERROR;
    }

    VADD2(center, bmin, bmax);
    VSCALE(center, center, 0.5);
    span = DIST_PNT_PNT(bmin, bmax);
    if (span <= SMALL_FASTF)
	span = 100.0;

    for (const auto &dir : dirs) {
	vect_t u, v;
	if (!ZERO(dir[X])) {
	    VSET(u, 0.0, 1.0, 0.0);
	    VSET(v, 0.0, 0.0, 1.0);
	} else if (!ZERO(dir[Y])) {
	    VSET(u, 1.0, 0.0, 0.0);
	    VSET(v, 0.0, 0.0, 1.0);
	} else {
	    VSET(u, 1.0, 0.0, 0.0);
	    VSET(v, 0.0, 1.0, 0.0);
	}

	for (fastf_t ou : offsets) {
	    for (fastf_t ov : offsets) {
		point_t origin, body_origin;
		vect_t body_dir, base_normal, got_normal, expect_normal;

		VJOIN3(origin, center, -1.25 * span, dir, ou * span, u, ov * span, v);
		VSET(body_origin, origin[X] / sc.sx, origin[Y] / sc.sy, origin[Z] / sc.sz);
		VSET(body_dir, dir[X] / sc.sx, dir[Y] / sc.sy, dir[Z] / sc.sz);
		VUNITIZE(body_dir);

		if (object_normal(gedp->dbip, scaled, origin, dir, got_normal) != BRLCAD_OK ||
		    object_normal(gedp->dbip, base, body_origin, body_dir, base_normal) != BRLCAD_OK)
		    continue;

		VSET(expect_normal, base_normal[X] / sc.sx, base_normal[Y] / sc.sy, base_normal[Z] / sc.sz);
		VUNITIZE(expect_normal);

		fastf_t dot = VDOT(got_normal, expect_normal);
		if (dot < NORMAL_DOT_TOL) {
		    bu_log("[nonuniform] %s normal mismatch: got [%g %g %g] expected [%g %g %g] dot %.17g tol %.17g\n",
			   label,
			   got_normal[X], got_normal[Y], got_normal[Z],
			   expect_normal[X], expect_normal[Y], expect_normal[Z],
			   dot, NORMAL_DOT_TOL);
		    return BRLCAD_ERROR;
		}

		return BRLCAD_OK;
	    }
	}
    }

    bu_log("[nonuniform] %s normal ray miss/setup failure\n", label);
    return BRLCAD_ERROR;
}

static int
check_ratio(const char *label, fastf_t base_csg, fastf_t base_bot, fastf_t csg, fastf_t bot, fastf_t tol)
{
    fastf_t base_ratio = base_bot / base_csg;
    fastf_t ratio = bot / csg;
    fastf_t rel = std::fabs((ratio - base_ratio) / base_ratio);
    if (rel > tol) {
	bu_log("[nonuniform] %s BoT/CSG ratio mismatch: got %.17g baseline %.17g rel %.6g tol %.6g\n",
	       label, ratio, base_ratio, rel, tol);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
run_scaled_case(struct ged *gedp, const char *base_csg, const char *base_bot,
		fastf_t base_csg_vol, fastf_t base_bot_vol,
		const char *tag, const scale_case &sc)
{
    char csg_leaf[128], bot_leaf[128], csg_comb[128], bot_comb[128];
    std::snprintf(csg_leaf, sizeof(csg_leaf), "%s_%s_csg.s", tag, sc.name);
    std::snprintf(bot_leaf, sizeof(bot_leaf), "%s_%s_bot.s", tag, sc.name);
    std::snprintf(csg_comb, sizeof(csg_comb), "%s_%s_csg.c", tag, sc.name);
    std::snprintf(bot_comb, sizeof(bot_comb), "%s_%s_bot.c", tag, sc.name);

    if (copy_obj(gedp, base_csg, csg_leaf) != BRLCAD_OK ||
	copy_obj(gedp, base_bot, bot_leaf) != BRLCAD_OK)
	return BRLCAD_ERROR;

    mat_t mat;
    scale_mat(mat, sc);
    if (write_wrapper(gedp, csg_comb, csg_leaf, mat) != BRLCAD_OK ||
	write_wrapper(gedp, bot_comb, bot_leaf, mat) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (npush_x(gedp, csg_comb) != BRLCAD_OK ||
	npush_x(gedp, bot_comb) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!has_nonuniform_attr(gedp->dbip, csg_leaf)) {
	bu_log("[nonuniform] %s missing %s after npush -x\n", csg_leaf, NONUNIFORM_ATTR);
	return BRLCAD_ERROR;
    }

    fastf_t csg_vol = -1.0;
    fastf_t bot_vol = -1.0;
    fastf_t det = sc.sx * sc.sy * sc.sz;
    if (object_volume(gedp->dbip, csg_leaf, &csg_vol) != BRLCAD_OK ||
	object_volume(gedp->dbip, bot_leaf, &bot_vol) != BRLCAD_OK)
	return BRLCAD_ERROR;

    char label[192];
    fastf_t csg_vol_tol = csg_volume_tol_for_tag(tag);
    fastf_t ratio_tol = ratio_tol_for_tag(tag);

    std::snprintf(label, sizeof(label), "%s/%s/csg", tag, sc.name);
    if (check_scaled_volume(label, base_csg_vol, csg_vol, det, csg_vol_tol) != BRLCAD_OK)
	return BRLCAD_ERROR;
    std::snprintf(label, sizeof(label), "%s/%s/bot", tag, sc.name);
    if (check_scaled_volume(label, base_bot_vol, bot_vol, det, VOL_TOL_BOT) != BRLCAD_OK)
	return BRLCAD_ERROR;
    std::snprintf(label, sizeof(label), "%s/%s", tag, sc.name);
    if (check_ratio(label, base_csg_vol, base_bot_vol, csg_vol, bot_vol, ratio_tol) != BRLCAD_OK)
	return BRLCAD_ERROR;

    fastf_t csg_area = -1.0;
    fastf_t csg_tess_area = -1.0;
    fastf_t bot_area = -1.0;
    fastf_t bot_tess_area = -1.0;
    if (object_surface_area(gedp->dbip, csg_leaf, &csg_area) != BRLCAD_OK ||
	object_tess_surface_area(gedp->dbip, csg_leaf, &csg_tess_area) != BRLCAD_OK ||
	object_surface_area(gedp->dbip, bot_leaf, &bot_area) != BRLCAD_OK ||
	object_tess_surface_area(gedp->dbip, bot_leaf, &bot_tess_area) != BRLCAD_OK)
	return BRLCAD_ERROR;

    std::snprintf(label, sizeof(label), "%s/%s/csg", tag, sc.name);
    if (check_area_match(label, csg_area, csg_tess_area, SA_TOL_CSG_TESS) != BRLCAD_OK)
	return BRLCAD_ERROR;
    std::snprintf(label, sizeof(label), "%s/%s/bot", tag, sc.name);
    if (check_area_match(label, bot_area, bot_tess_area, SA_TOL_BOT_TESS) != BRLCAD_OK)
	return BRLCAD_ERROR;
    std::snprintf(label, sizeof(label), "%s/%s/csg", tag, sc.name);
    return check_nonuniform_normal(gedp, base_csg, csg_leaf, label, sc);
}

static int
run_prim_orientation(struct ged *gedp, const prim_case &pc, const char *orient)
{
    char base[128], bot[128], tag[128];
    std::snprintf(tag, sizeof(tag), "%s_%s", pc.name, orient);
    std::snprintf(base, sizeof(base), "%s_base.s", tag);
    std::snprintf(bot, sizeof(bot), "%s_base.bot", tag);

    if (facetize_obj(gedp, base, bot) != BRLCAD_OK)
	return BRLCAD_ERROR;

    fastf_t base_csg_vol = -1.0;
    fastf_t base_bot_vol = -1.0;
    if (object_volume(gedp->dbip, base, &base_csg_vol) != BRLCAD_OK) {
	bu_log("[nonuniform] %s baseline CSG volume unavailable\n", base);
	return BRLCAD_ERROR;
    }
    if (object_volume(gedp->dbip, bot, &base_bot_vol) != BRLCAD_OK) {
	bu_log("[nonuniform] %s baseline BoT volume unavailable\n", bot);
	return BRLCAD_ERROR;
    }

    if (run_attr_minimality_case(gedp, base, base_csg_vol, tag) != BRLCAD_OK)
	return BRLCAD_ERROR;

    for (const auto &sc : scales) {
	if (run_scaled_case(gedp, base, bot, base_csg_vol, base_bot_vol, tag, sc) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }

    bu_log("[nonuniform] %s %s baseline csg %.17g bot %.17g ratio %.17g\n",
	   pc.name, orient, base_csg_vol, base_bot_vol, base_bot_vol / base_csg_vol);
    return BRLCAD_OK;
}

static int
build_db(const char *gfile)
{
    struct rt_wdb *wdbp = wdb_fopen(gfile);
    if (!wdbp)
	return BRLCAD_ERROR;

    int failures = 0;
    for (const auto &pc : prims) {
	for (int oi = 0; oi < 2; oi++) {
	    frame f;
	    make_frame(&f, oi != 0);
	    char name[128];
	    std::snprintf(name, sizeof(name), "%s_%s_base.s", pc.name, oi ? "oblique" : "axis");
	    if (write_primitive(wdbp, pc, name, &f) != 0) {
		bu_log("[nonuniform] failed to write %s\n", name);
		failures++;
	    }
	}
    }

    db_close(wdbp->dbip);
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_log("Usage: %s tmpdir\n", argv[0]);
	return 1;
    }

    struct bu_vls bu_cache = BU_VLS_INIT_ZERO;
    struct bu_vls rt_cache = BU_VLS_INIT_ZERO;
    bu_vls_printf(&bu_cache, "%s/cache", argv[1]);
    bu_vls_printf(&rt_cache, "%s/rtcache", argv[1]);
    bu_dirclear(bu_vls_cstr(&bu_cache));
    bu_dirclear(bu_vls_cstr(&rt_cache));
    bu_mkdir(bu_vls_cstr(&bu_cache));
    bu_mkdir(bu_vls_cstr(&rt_cache));
    bu_setenv("BU_DIR_CACHE", bu_vls_cstr(&bu_cache), 1);
    bu_setenv("LIBRT_CACHE", bu_vls_cstr(&rt_cache), 1);

    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gfile, "%s/ged_nonuniform.g", argv[1]);
    bu_file_delete(bu_vls_cstr(&gfile));

    if (build_db(bu_vls_cstr(&gfile)) != BRLCAD_OK) {
	bu_vls_free(&bu_cache);
	bu_vls_free(&rt_cache);
	bu_vls_free(&gfile);
	return 1;
    }

    struct ged *gedp = ged_open("db", bu_vls_cstr(&gfile), 1);
    if (!gedp) {
	bu_log("[nonuniform] ged_open failed for %s\n", bu_vls_cstr(&gfile));
	bu_vls_free(&bu_cache);
	bu_vls_free(&rt_cache);
	bu_vls_free(&gfile);
	return 1;
    }

    int failures = 0;
    for (const auto &pc : prims) {
	if (run_prim_orientation(gedp, pc, "axis") != BRLCAD_OK)
	    failures++;
	if (run_prim_orientation(gedp, pc, "oblique") != BRLCAD_OK)
	    failures++;
    }

    ged_close(gedp);
    bu_dirclear(bu_vls_cstr(&bu_cache));
    bu_dirclear(bu_vls_cstr(&rt_cache));
    bu_vls_free(&bu_cache);
    bu_vls_free(&rt_cache);
    bu_vls_free(&gfile);

    if (failures) {
	bu_log("[nonuniform] %d primitive/orientation failures\n", failures);
	return 1;
    }

    bu_log("[nonuniform] all primitive non-uniform volume checks passed\n");
    return 0;
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
