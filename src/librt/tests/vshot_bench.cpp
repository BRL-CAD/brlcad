/*                    V S H O T _ B E N C H . C P P
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
/** @file vshot_bench.cpp
 *
 * Primitive-generic micro-benchmark comparing the per-ray ft_shot()
 * callback (called N times) against the vectorized ft_vshot() callback
 * (one batched call over N rays), to quantify the cache/allocation
 * coherency benefit of batching.
 *
 * For each registered primitive the harness:
 *   - hand-builds a representative rt_db_internal and preps a soltab via
 *     the exported functab (resolving st_meth to whatever prep chooses);
 *   - generates two ray populations aimed at the primitive's bounding
 *     sphere: COHERENT (a parallel bundle, all rays share one direction)
 *     and INCOHERENT (random directions/origins);
 *   - verifies ft_shot and ft_vshot agree ray-by-ray on both populations;
 *   - sweeps batch sizes N = 1,2,4,8,16,32,64 and, for each (N, coherency)
 *     cell, times shot*N vs vshot(N), auto-calibrating the repetition
 *     count so every timed phase runs at least --secs seconds (default
 *     tuned so total work per primitive exceeds 60s for stable numbers).
 *
 * Both wall-clock (bu_gettime) and CPU (rt_prep_timer/rt_get_timer) are
 * reported, along with the shot/vshot speedup.
 *
 * Usage: vshot_bench [prim|all] [secs_per_phase]
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <cmath>
#include <random>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/magic.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/timer.h"


#define MAXN 64


/* ------------------------------------------------------------------ *
 * Per-primitive representative-instance builders.
 *
 * Each builder fills *ip with a valid internal for one primitive.  The
 * idb_ptr points at a bu_calloc'd specific struct (leaked at exit, which
 * is fine for a short-lived benchmark).
 * ------------------------------------------------------------------ */

typedef void (*prim_builder)(struct rt_db_internal *ip);

struct prim_case {
    const char *name;		/* CLI selector + display label */
    int type;			/* ID_* for idb_type / OBJ[] */
    prim_builder build;
};


static void
init_internal(struct rt_db_internal *ip, int type, void *ptr)
{
    RT_DB_INTERNAL_INIT(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = type;
    ip->idb_meth = &OBJ[type];
    ip->idb_ptr = ptr;
}


static void
build_epa(struct rt_db_internal *ip)
{
    struct rt_epa_internal *epa;
    BU_ALLOC(epa, struct rt_epa_internal);
    epa->epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa->epa_V, 0.0, 0.0, 0.0);
    VSET(epa->epa_H, 0.0, 0.0, 100.0);	/* height, |H| = 100 */
    VSET(epa->epa_Au, 1.0, 0.0, 0.0);	/* semi-major axis unit */
    epa->epa_r1 = 60.0;
    epa->epa_r2 = 40.0;
    init_internal(ip, ID_EPA, epa);
}


static void
build_rpc(struct rt_db_internal *ip)
{
    struct rt_rpc_internal *rpc;
    BU_ALLOC(rpc, struct rt_rpc_internal);
    rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;
    VSET(rpc->rpc_V, 0.0, 0.0, 0.0);
    VSET(rpc->rpc_H, 100.0, 0.0, 0.0);	/* height, |H| = 100 */
    VSET(rpc->rpc_B, 0.0, 0.0, 80.0);	/* breadth, |B| = 80, B _|_ H */
    rpc->rpc_r = 40.0;			/* rectangular half-width */
    init_internal(ip, ID_RPC, rpc);
}


static void
build_rhc(struct rt_db_internal *ip)
{
    struct rt_rhc_internal *rhc;
    BU_ALLOC(rhc, struct rt_rhc_internal);
    rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;
    VSET(rhc->rhc_V, 0.0, 0.0, 0.0);	/* vertex at origin */
    VSET(rhc->rhc_H, 0.0, 0.0, 100.0);	/* height vector, |H| = 100 */
    VSET(rhc->rhc_B, 0.0, 60.0, 0.0);	/* breadth vector, |B| = 60, B _|_ H */
    rhc->rhc_r = 40.0;			/* half-width of rectangular face */
    rhc->rhc_c = 20.0;			/* dist from hyperbola to asymptote vertex, c > 0 */
    init_internal(ip, ID_RHC, rhc);
}


static void
build_ehy(struct rt_db_internal *ip)
{
    struct rt_ehy_internal *ehy;
    BU_ALLOC(ehy, struct rt_ehy_internal);
    ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy->ehy_V, 0.0, 0.0, 0.0);	/* vertex at origin */
    VSET(ehy->ehy_H, 0.0, 0.0, 100.0);	/* height, |H| = 100, perp to Au */
    VSET(ehy->ehy_Au, 1.0, 0.0, 0.0);	/* semi-major axis unit */
    ehy->ehy_r1 = 60.0;			/* semi-major (>= r2) */
    ehy->ehy_r2 = 40.0;			/* semi-minor */
    ehy->ehy_c  = 10.0;			/* dist to asymptote vertex (> 0) */
    init_internal(ip, ID_EHY, ehy);
}


static void
build_part(struct rt_db_internal *ip)
{
    struct rt_part_internal *part;
    BU_ALLOC(part, struct rt_part_internal);
    part->part_magic = RT_PART_INTERNAL_MAGIC;
    VSET(part->part_V, 0.0, 0.0, 0.0);		/* base center */
    VSET(part->part_H, 0.0, 0.0, 100.0);	/* axis, |H| = 100 */
    part->part_vrad = 30.0;			/* V-end radius */
    part->part_hrad = 30.0;			/* H-end radius (== vrad -> cylinder) */
    part->part_type = RT_PARTICLE_TYPE_CYLINDER;
    init_internal(ip, ID_PARTICLE, part);
}


static void
build_arbn(struct rt_db_internal *ip)
{
    struct rt_arbn_internal *arbn;
    static const fastf_t d = 100.0;	/* half-extent, mm; bounding sphere ~173mm */
    int i;

    BU_ALLOC(arbn, struct rt_arbn_internal);
    arbn->magic = RT_ARBN_INTERNAL_MAGIC;

    /* A convex cube expressed as 6 outward-facing halfspaces:
     * VDOT(pt, N) <= N[3].  Unit normals, each face at distance d. */
    arbn->neqn = 6;
    arbn->eqn = (plane_t *)bu_malloc(arbn->neqn * sizeof(plane_t), "arbn eqn[]");

    for (i = 0; i < 6; i++) {
	HSETALL(arbn->eqn[i], 0.0);
	arbn->eqn[i][3] = d;
    }
    arbn->eqn[0][X] =  1.0;	/* +X face */
    arbn->eqn[1][X] = -1.0;	/* -X face */
    arbn->eqn[2][Y] =  1.0;	/* +Y face */
    arbn->eqn[3][Y] = -1.0;	/* -Y face */
    arbn->eqn[4][Z] =  1.0;	/* +Z face */
    arbn->eqn[5][Z] = -1.0;	/* -Z face */

    init_internal(ip, ID_ARBN, arbn);
}


static void
build_hyp(struct rt_db_internal *ip)
{
    struct rt_hyp_internal *hyp;
    BU_ALLOC(hyp, struct rt_hyp_internal);
    hyp->hyp_magic = RT_HYP_INTERNAL_MAGIC;
    VSET(hyp->hyp_Vi, 0.0, 0.0, 0.0);	/* vertex (base center) */
    VSET(hyp->hyp_Hi, 0.0, 0.0, 100.0);	/* full height vector */
    VSET(hyp->hyp_A, 60.0, 0.0, 0.0);	/* semi-major axis (perp to Hi) */
    hyp->hyp_b = 40.0;			/* semi-minor length */
    hyp->hyp_bnr = 0.5;			/* neck/base ratio, 0 < bnr < 1 */
    init_internal(ip, ID_HYP, hyp);
}


static void
build_eto(struct rt_db_internal *ip)
{
    struct rt_eto_internal *eto;
    BU_ALLOC(eto, struct rt_eto_internal);
    eto->eto_magic = RT_ETO_INTERNAL_MAGIC;
    VSET(eto->eto_V, 0.0, 0.0, 0.0);		/* center */
    VSET(eto->eto_N, 0.0, 0.0, 1.0);		/* plane normal (unit) */
    VSET(eto->eto_C, 14.142136, 0.0, 14.142136);/* semi-major axis vec, |C|=20 @45deg */
    eto->eto_r = 100.0;				/* radius of revolution */
    eto->eto_rd = 10.0;				/* semi-minor of ellipse */
    init_internal(ip, ID_ETO, eto);
}


static void
build_superell(struct rt_db_internal *ip)
{
    struct rt_superell_internal *se;
    BU_ALLOC(se, struct rt_superell_internal);
    se->magic = RT_SUPERELL_INTERNAL_MAGIC;
    VSET(se->v, 0.0, 0.0, 0.0);
    VSET(se->a, 80.0, 0.0, 0.0);
    VSET(se->b, 0.0, 60.0, 0.0);
    VSET(se->c, 0.0, 0.0, 40.0);
    se->n = 2.0;	/* n=e=2 -> plain ellipsoid (matches dgr-2 shot exactly) */
    se->e = 2.0;
    init_internal(ip, ID_SUPERELL, se);
}


/* A tessellated UV sphere (~6240 triangles): enough BVH depth that
 * traversal, not seg packaging, dominates -- the regime where the packet
 * vshot's amortized node/triangle loads can pay off. */
static void
build_bot(struct rt_db_internal *ip)
{
    struct rt_bot_internal *bot;
    const int nlat = 40;		/* latitude divisions (pole to pole) */
    const int nlon = 80;		/* longitude divisions */
    const double R = 100.0;
    const int nring = nlat - 1;		/* interior rings i = 1..nring */
    const size_t nverts = 2 + (size_t)nring * nlon;
    const size_t nfaces = (size_t)2 * nlon * (nlat - 1);
    fastf_t *v;
    int *f;
    int i, j, fi, south, base, base0, base1;

    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->bot_flags = 0;
    bot->num_vertices = nverts;
    bot->num_faces = nfaces;
    v = (fastf_t *)bu_malloc(nverts * 3 * sizeof(fastf_t), "bot verts");
    f = (int *)bu_malloc(nfaces * 3 * sizeof(int), "bot faces");
    bot->vertices = v;
    bot->faces = f;

    v[0] = 0.0; v[1] = 0.0; v[2] = R;		/* north pole = vertex 0 */
    south = 1 + nring * nlon;			/* south pole index */
    for (i = 1; i <= nring; i++) {
	double theta = M_PI * (double)i / (double)nlat;
	double st = sin(theta), ct = cos(theta);
	for (j = 0; j < nlon; j++) {
	    double phi = M_2PI * (double)j / (double)nlon;
	    int idx = 1 + (i - 1) * nlon + j;
	    v[idx*3+0] = R * st * cos(phi);
	    v[idx*3+1] = R * st * sin(phi);
	    v[idx*3+2] = R * ct;
	}
    }
    v[south*3+0] = 0.0; v[south*3+1] = 0.0; v[south*3+2] = -R;

    fi = 0;
    for (j = 0; j < nlon; j++) {			/* north cap */
	f[fi*3+0] = 0; f[fi*3+1] = 1 + j; f[fi*3+2] = 1 + ((j+1) % nlon); fi++;
    }
    for (i = 1; i < nring; i++) {			/* middle bands */
	base0 = 1 + (i - 1) * nlon;
	base1 = 1 + i * nlon;
	for (j = 0; j < nlon; j++) {
	    int jn = (j + 1) % nlon;
	    f[fi*3+0] = base0+j; f[fi*3+1] = base1+j;  f[fi*3+2] = base0+jn; fi++;
	    f[fi*3+0] = base0+jn; f[fi*3+1] = base1+j; f[fi*3+2] = base1+jn; fi++;
	}
    }
    base = 1 + (nring - 1) * nlon;			/* south cap */
    for (j = 0; j < nlon; j++) {
	f[fi*3+0] = base+j; f[fi*3+1] = base + ((j+1) % nlon); f[fi*3+2] = south; fi++;
    }

    init_internal(ip, ID_BOT, bot);
}


static void
build_cline(struct rt_db_internal *ip)
{
    struct rt_cline_internal *cl;
    BU_ALLOC(cl, struct rt_cline_internal);
    cl->magic = RT_CLINE_INTERNAL_MAGIC;
    VSET(cl->v, 0.0, 0.0, -100.0);	/* base */
    VSET(cl->h, 0.0, 0.0, 200.0);	/* axis, length 200 */
    cl->radius = 25.0;
    cl->thickness = 0.0;		/* volume mode */
    init_internal(ip, ID_CLINE, cl);
}


static const struct prim_case prim_cases[] = {
    { "bot",      ID_BOT,      build_bot },
    { "cline",    ID_CLINE,    build_cline },
    { "epa",      ID_EPA,      build_epa },
    { "rpc",      ID_RPC,      build_rpc },
    { "rhc",      ID_RHC,      build_rhc },
    { "ehy",      ID_EHY,      build_ehy },
    { "part",     ID_PARTICLE, build_part },
    { "arbn",     ID_ARBN,     build_arbn },
    { "hyp",      ID_HYP,      build_hyp },
    { "eto",      ID_ETO,      build_eto },
    { "superell", ID_SUPERELL, build_superell },
};
static const int num_prim_cases = (int)(sizeof(prim_cases) / sizeof(prim_cases[0]));


/* ------------------------------------------------------------------ *
 * Ray population generation.
 * ------------------------------------------------------------------ */

/* Build an orthonormal basis (u, v) spanning the plane perpendicular to
 * unit vector d. */
static void
perp_basis(vect_t u, vect_t v, const vect_t d)
{
    vect_t a;
    if (fabs(d[X]) < 0.9) {
	VSET(a, 1.0, 0.0, 0.0);
    } else {
	VSET(a, 0.0, 1.0, 0.0);
    }
    fastf_t ad = VDOT(a, d);
    VJOIN1(u, a, -ad, d);	/* a - (a.d) d */
    VUNITIZE(u);
    VCROSS(v, d, u);
}


/* Coherent packet: all rays share direction D and their origins form a
 * TIGHT tile (a camera-tile-like pencil) placed 5R behind the center.
 * A small footprint is what makes a packet "coherent" in the ray-tracing
 * sense -- the rays traverse the same acceleration-structure nodes and
 * touch the same triangles, which is exactly the locality a vectorized
 * shot is meant to exploit.  The tile is offset toward the limb so it
 * still straddles the primitive, giving a hit/miss mix. */
static void
gen_coherent(struct xray rays[MAXN], const point_t center, double bradius,
	     std::mt19937_64 &rng)
{
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    vect_t D;
    VSET(D, 0.3, 0.4, -0.86602540378);	/* off-axis, unitized below */
    VUNITIZE(D);
    vect_t bu_, bv_;
    perp_basis(bu_, bv_, D);
    const double spread = 0.25 * bradius;	/* tight tile half-width */
    const double offset = 0.50 * bradius;	/* shift toward the limb */

    for (int i = 0; i < MAXN; i++) {
	double s = offset + spread * u(rng);
	double t = spread * u(rng);
	point_t origin;
	VJOIN2(origin, center, -5.0 * bradius, D, s, bu_);
	VJOIN1(origin, origin, t, bv_);

	rays[i].magic = RT_RAY_MAGIC;
	rays[i].index = i;
	VMOVE(rays[i].r_pt, origin);
	VMOVE(rays[i].r_dir, D);
    }
}


/* Incoherent: random directions and origins on a shell at 5R, aimed
 * back toward a jittered point near the center. */
static void
gen_incoherent(struct xray rays[MAXN], const point_t center, double bradius,
	       std::mt19937_64 &rng)
{
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    for (int i = 0; i < MAXN; i++) {
	vect_t d, off;
	do {
	    VSET(d, u(rng), u(rng), u(rng));
	} while (MAGSQ(d) < 1.0e-6);
	VUNITIZE(d);

	point_t origin;
	VJOIN1(origin, center, 5.0 * bradius, d);

	VSET(off, u(rng) * 1.5 * bradius, u(rng) * 1.5 * bradius,
	     u(rng) * 1.5 * bradius);
	point_t aim;
	VADD2(aim, center, off);
	vect_t dir;
	VSUB2(dir, aim, origin);
	VUNITIZE(dir);

	rays[i].magic = RT_RAY_MAGIC;
	rays[i].index = i;
	VMOVE(rays[i].r_pt, origin);
	VMOVE(rays[i].r_dir, dir);
    }
}


/* ------------------------------------------------------------------ *
 * Correctness: confirm shot and vshot agree ray-by-ray.
 * Returns number of hits; sets *mismatch and *maxdiff.
 * ------------------------------------------------------------------ */
static int
check_agreement(const struct rt_functab *ft, struct soltab *stp,
		struct xray rays[MAXN], struct application *ap,
		struct resource *resp, int *mismatch, double *maxdiff)
{
    int hits = 0;
    *mismatch = 0;
    *maxdiff = 0.0;

    for (int i = 0; i < MAXN; i++) {
	struct seg shead;
	BU_LIST_INIT(&shead.l);
	int nh = ft->ft_shot(stp, &rays[i], ap, &shead);

	struct soltab *one_stp[1] = { stp };
	struct xray *one_rp[1] = { &rays[i] };
	struct seg one_seg[1];
	one_seg[0].seg_stp = (struct soltab *)0;
	ft->ft_vshot(one_stp, one_rp, one_seg, 1, ap);

	int shot_hit = (nh > 0);
	int vshot_hit = (one_seg[0].seg_stp != (struct soltab *)0);
	if (shot_hit != vshot_hit) {
	    (*mismatch)++;
	} else if (shot_hit) {
	    /* Compare the OUTER SPAN (nearest in, farthest out across
	     * all shot segs) against the vshot single seg.  This matches
	     * the rt_tor_vshot convention for non-convex primitives that
	     * scalar-shot returns as multiple segments, and is identical
	     * to the first-seg for convex primitives (one seg). */
	    struct seg *s;
	    double smin = INFINITY, smax = -INFINITY;
	    for (BU_LIST_FOR(s, seg, &shead.l)) {
		if (s->seg_in.hit_dist < smin) smin = s->seg_in.hit_dist;
		if (s->seg_out.hit_dist > smax) smax = s->seg_out.hit_dist;
	    }
	    double din = fabs(smin - one_seg[0].seg_in.hit_dist);
	    double dout = fabs(smax - one_seg[0].seg_out.hit_dist);
	    if (din > *maxdiff) *maxdiff = din;
	    if (dout > *maxdiff) *maxdiff = dout;
	    hits++;
	}
	struct seg *s_tmp;
	while (BU_LIST_WHILE(s_tmp, seg, &shead.l)) {
	    BU_LIST_DEQUEUE(&s_tmp->l);
	    RT_FREE_SEG(s_tmp, resp);
	}
    }
    return hits;
}


/* ------------------------------------------------------------------ *
 * Timing helpers.  Each returns nanoseconds-per-ray and fills wall/cpu
 * seconds for the kept (>= min_us) run.
 * ------------------------------------------------------------------ */

struct timing {
    int64_t reps;
    double wall_s;
    double cpu_s;
    double ns_per_ray;
};


static struct timing
time_shot(const struct rt_functab *ft, struct soltab *stp,
	  struct xray rays[MAXN], int N, struct application *ap,
	  struct resource *resp, double min_us, volatile double *sink)
{
    struct timing tm;
    int64_t reps = 1024;
    for (;;) {
	double chk = 0.0;
	rt_prep_timer();
	int64_t t0 = bu_gettime();
	for (int64_t r = 0; r < reps; r++) {
	    struct seg shead;
	    BU_LIST_INIT(&shead.l);
	    for (int i = 0; i < N; i++)
		ft->ft_shot(stp, &rays[i], ap, &shead);
	    struct seg *s;
	    while (BU_LIST_WHILE(s, seg, &shead.l)) {
		chk += s->seg_in.hit_dist;
		BU_LIST_DEQUEUE(&s->l);
		RT_FREE_SEG(s, resp);
	    }
	}
	int64_t wall_us = bu_gettime() - t0;
	double cpu = rt_get_timer(NULL, NULL);
	*sink += chk;
	if ((double)wall_us >= min_us || reps > (INT64_C(1) << 40)) {
	    tm.reps = reps;
	    tm.wall_s = (double)wall_us / 1.0e6;
	    tm.cpu_s = cpu;
	    tm.ns_per_ray = (double)wall_us * 1000.0 / ((double)reps * (double)N);
	    break;
	}
	double factor = (min_us / (double)(wall_us > 0 ? wall_us : 1)) * 1.15;
	if (factor < 2.0) factor = 2.0;
	reps = (int64_t)((double)reps * factor) + 1;
    }
    return tm;
}


static struct timing
time_vshot(const struct rt_functab *ft, struct soltab *stparr[MAXN],
	   struct xray *rparr[MAXN], struct seg segarr[MAXN], int N,
	   struct application *ap, double min_us, volatile double *sink)
{
    struct timing tm;
    int64_t reps = 1024;
    for (;;) {
	double chk = 0.0;
	rt_prep_timer();
	int64_t t0 = bu_gettime();
	for (int64_t r = 0; r < reps; r++) {
	    ft->ft_vshot(stparr, rparr, segarr, N, ap);
	    for (int i = 0; i < N; i++)
		if (segarr[i].seg_stp)
		    chk += segarr[i].seg_in.hit_dist;
	}
	int64_t wall_us = bu_gettime() - t0;
	double cpu = rt_get_timer(NULL, NULL);
	*sink += chk;
	if ((double)wall_us >= min_us || reps > (INT64_C(1) << 40)) {
	    tm.reps = reps;
	    tm.wall_s = (double)wall_us / 1.0e6;
	    tm.cpu_s = cpu;
	    tm.ns_per_ray = (double)wall_us * 1000.0 / ((double)reps * (double)N);
	    break;
	}
	double factor = (min_us / (double)(wall_us > 0 ? wall_us : 1)) * 1.15;
	if (factor < 2.0) factor = 2.0;
	reps = (int64_t)((double)reps * factor) + 1;
    }
    return tm;
}


/* ------------------------------------------------------------------ *
 * Run the full sweep for one primitive.
 * ------------------------------------------------------------------ */
static int
run_primitive(const struct prim_case *pc, struct rt_i *rtip,
	      struct resource *resp, double min_seconds)
{
    const double min_us = min_seconds * 1.0e6;

    struct rt_db_internal intern;
    pc->build(&intern);

    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab), "soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;

    /* Give the soltab a directory entry so shot routines that reference
     * st_name (== st_dp->d_namep, e.g. eto/superell poly-root logging)
     * have a valid name pointer instead of dereferencing NULL. */
    struct directory *dp = (struct directory *)bu_calloc(1, sizeof(struct directory), "dummy dir");
    dp->d_magic = RT_DIR_MAGIC;
    dp->d_namep = (char *)pc->name;
    stp->st_dp = dp;

    /* rt_prep() normally sets these before dispatching to ft_prep(); some
     * preps (part/arbn/eto/superell) rely on st_meth already being set. */
    stp->st_id = pc->type;
    stp->st_meth = &OBJ[pc->type];

    if (OBJ[pc->type].ft_prep(stp, &intern, rtip)) {
	printf("## %-9s SKIP: ft_prep() failed\n", pc->name);
	bu_free(stp, "soltab");
	return 1;
    }

    const struct rt_functab *ft = stp->st_meth;
    if (!ft->ft_shot || !ft->ft_vshot) {
	printf("## %-9s SKIP: missing ft_shot/ft_vshot (resolved to %s)\n",
	       pc->name, ft->ft_name);
	bu_free(stp, "soltab");
	return 1;
    }

    printf("\n================================================================\n");
    printf("# primitive: %s   (label='%s', resolved st_id=%d)\n",
	   pc->name, ft->ft_label, stp->st_id);
    printf("#   bounding sphere: center=(%.2f %.2f %.2f) radius=%.3f\n",
	   V3ARGS(stp->st_center), stp->st_bradius);

    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;

    struct xray rays_c[MAXN], rays_i[MAXN];
    std::mt19937_64 rng(0x5EED1234ULL);
    gen_coherent(rays_c, stp->st_center, stp->st_bradius, rng);
    gen_incoherent(rays_i, stp->st_center, stp->st_bradius, rng);

    /* correctness on both populations */
    const double match_tol = 1.0e-6;
    for (int mode = 0; mode < 2; mode++) {
	struct xray *rays = mode ? rays_i : rays_c;
	int mismatch;
	double maxdiff;
	int hits = check_agreement(ft, stp, rays, &ap, resp, &mismatch, &maxdiff);
	printf("#   correctness[%-10s]: %d/%d hit, %d mismatch, max|dist diff|=%.3e\n",
	       mode ? "incoherent" : "coherent", hits, MAXN, mismatch, maxdiff);
	if (mismatch) {
	    bu_exit(1, "FAIL: %s shot/vshot disagree on %d hit/miss (%s)\n",
		    pc->name, mismatch, mode ? "incoherent" : "coherent");
	}
	if (maxdiff > match_tol) {
	    bu_exit(1, "FAIL: %s shot/vshot dist differ %.3e > %.3e (%s)\n",
		    pc->name, maxdiff, match_tol, mode ? "incoherent" : "coherent");
	}
    }

    static const int Ns[] = {1, 2, 4, 8, 16, 32, 64};
    const int nNs = (int)(sizeof(Ns) / sizeof(Ns[0]));

    printf("\n# >= %.2f s per timed phase; wall & cpu seconds shown\n", min_seconds);
    printf("%-10s %4s %10s %10s %10s %10s %10s %8s\n",
	   "coherency", "N", "shot ns", "vshot ns", "shot wall", "vsh wall",
	   "shot cpu", "speedup");
    printf("%-10s %4s %10s %10s %10s %10s %10s %8s\n",
	   "----------", "----", "----------", "----------", "----------",
	   "----------", "----------", "--------");

    volatile double sink = 0.0;
    double total_wall = 0.0;

    for (int mode = 0; mode < 2; mode++) {
	struct xray *rays = mode ? rays_i : rays_c;
	const char *cname = mode ? "incoherent" : "coherent";

	struct soltab *stparr[MAXN];
	struct xray *rparr[MAXN];
	struct seg segarr[MAXN];
	for (int i = 0; i < MAXN; i++) {
	    stparr[i] = stp;
	    rparr[i] = &rays[i];
	}

	for (int k = 0; k < nNs; k++) {
	    const int N = Ns[k];

	    /* warm up */
	    for (int i = 0; i < N; i++) {
		struct seg shead;
		BU_LIST_INIT(&shead.l);
		ft->ft_shot(stp, &rays[i], &ap, &shead);
		struct seg *s;
		while (BU_LIST_WHILE(s, seg, &shead.l)) {
		    BU_LIST_DEQUEUE(&s->l);
		    RT_FREE_SEG(s, resp);
		}
	    }
	    ft->ft_vshot(stparr, rparr, segarr, N, &ap);

	    struct timing st = time_shot(ft, stp, rays, N, &ap, resp, min_us, &sink);
	    struct timing vt = time_vshot(ft, stparr, rparr, segarr, N, &ap, min_us, &sink);
	    total_wall += st.wall_s + vt.wall_s;

	    double speedup = (vt.ns_per_ray > 0.0) ? st.ns_per_ray / vt.ns_per_ray : 0.0;
	    printf("%-10s %4d %10.3f %10.3f %10.2f %10.2f %10.2f %7.2fx\n",
		   cname, N, st.ns_per_ray, vt.ns_per_ray,
		   st.wall_s, vt.wall_s, st.cpu_s, speedup);
	    fflush(stdout);
	}
    }

    printf("# total timed wall for %s: %.1f s\n", pc->name, total_wall);
    (void)sink;

    bu_free(stp, "soltab");
    return 0;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    setvbuf(stdout, NULL, _IONBF, 0);	/* unbuffered: survive a mid-run crash */

    const char *sel = "all";
    double min_seconds = 2.5;	/* 28 phases/prim * 2.5s ~= 70s > 60s */

    if (argc > 1) sel = argv[1];
    if (argc > 2) {
	double v = atof(argv[2]);
	if (v > 0.0) min_seconds = v;
    }

    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
	bu_exit(1, "rt_dirbuild_inmem() failed\n");

    rtip->rti_tol.magic = BN_TOL_MAGIC;
    rtip->rti_tol.dist = 0.0005;
    rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
    rtip->rti_tol.perp = 1.0e-6;
    rtip->rti_tol.para = 1.0 - 1.0e-6;

    rt_init_resource(&rt_uniresource, 0, rtip);
    struct resource *resp = &rt_uniresource;

    printf("# vshot_bench: shot*N vs vshot(N), N in {1,2,4,8,16,32,64}\n");
    printf("# coherent = parallel ray bundle; incoherent = random rays\n");

    int ran = 0, failed = 0;
    for (int i = 0; i < num_prim_cases; i++) {
	if (strcmp(sel, "all") && strcmp(sel, prim_cases[i].name))
	    continue;
	ran++;
	failed += run_primitive(&prim_cases[i], rtip, resp, min_seconds);
    }

    if (!ran)
	bu_exit(1, "no primitive matched '%s'\n", sel);

    return failed ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
