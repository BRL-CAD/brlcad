/*                    V S H O T _ T E S T . C P P
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
/** @file vshot_test.cpp
 *
 * Standalone micro-benchmark comparing the per-ray ft_shot() callback
 * against the (presently unused) vectorized ft_vshot() callback, to
 * gauge the potential cache-coherency benefit of batching rays.
 *
 * The sphere primitive (ID_SPH) is used because rt_sph_vshot() is a
 * verbatim per-ray copy of rt_sph_shot() -- identical arithmetic, the
 * only difference being that vshot writes results into a caller-supplied
 * flat seg[] array while shot allocates segs from a resource free list
 * and links them into a list.  That packaging difference is exactly the
 * cache/allocation overhead this test is meant to expose.
 *
 * A spherical rt_ell_internal is prepped via OBJ[ID_ELL].ft_prep(),
 * which delegates to rt_sph_prep() and rewrites stp->st_meth to
 * &OBJ[ID_SPH]; all subsequent calls go through the exported functab.
 *
 * Both callbacks are exercised over batches of 1, 2, 4, 8, 16, 32, 64,
 * 128 and 256 random rays, single threaded.
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>
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
#include "bn/tol.h"
#include "raytrace.h"
#include "rt/geom.h"


#define MAXN 256


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    /* minimum wall-clock seconds each timed phase (shot, vshot) must run;
     * the repetition count is auto-calibrated to reach it.  Override via
     * argv[1].  With 9 N-levels x 2 phases, the default ~10s/phase yields a
     * total run of roughly 3 minutes. */
    double min_seconds = 10.0;
    if (argc > 1) {
	double v = atof(argv[1]);
	if (v > 0.0)
	    min_seconds = v;
    }
    const double min_us = min_seconds * 1.0e6;

    /* ------------------------------------------------------------------
     * Build a sphere as a spherical ellipsoid internal.
     * ------------------------------------------------------------------ */
    const double R = 100.0;
    struct rt_ell_internal ell;
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, 0.0, 0.0, 0.0);
    VSET(ell.a,   R, 0.0, 0.0);
    VSET(ell.b, 0.0,   R, 0.0);
    VSET(ell.c, 0.0, 0.0,   R);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_ELL;
    intern.idb_meth = &OBJ[ID_ELL];
    intern.idb_ptr = (void *)&ell;

    /* ------------------------------------------------------------------
     * Minimal rt instance: supplies valid tolerances + resource infra.
     * ------------------------------------------------------------------ */
    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip)
	bu_exit(1, "rt_dirbuild_inmem() failed\n");

    rtip->rti_tol.magic = BN_TOL_MAGIC;
    rtip->rti_tol.dist = 0.0005;
    rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
    rtip->rti_tol.perp = 1.0e-6;
    rtip->rti_tol.para = 1.0 - 1.0e-6;

    rt_init_resource(&rt_uniresource, 0, rtip);
    struct resource *resp = &rt_uniresource;	/* RT_*_SEG macros need a ptr lvalue */

    /* ------------------------------------------------------------------
     * Hand-build and prep one soltab via the exported functab.
     * ------------------------------------------------------------------ */
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab), "soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;

    if (OBJ[ID_ELL].ft_prep(stp, &intern, rtip))
	bu_exit(1, "ft_prep() failed\n");

    const struct rt_functab *ft = stp->st_meth;
    if (stp->st_id != ID_SPH)
	bu_exit(1, "prep did not resolve to ID_SPH (st_id=%d)\n", stp->st_id);
    if (!ft->ft_shot || !ft->ft_vshot)
	bu_exit(1, "primitive '%s' missing ft_shot/ft_vshot\n", ft->ft_name);

    printf("# primitive: %s   (st_id=%d, radius=%.1f)\n", ft->ft_label, stp->st_id, R);

    /* ------------------------------------------------------------------
     * Application + reusable per-ray scratch arrays.
     * ------------------------------------------------------------------ */
    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;

    struct xray   rays[MAXN];
    struct soltab *stparr[MAXN];	/* all point at the single soltab */
    struct xray   *rparr[MAXN];		/* pointers into rays[]          */
    struct seg     segarr[MAXN];	/* flat vshot result array       */

    /* Reproducible random rays: origins on a shell at 5R, aimed at a
     * jittered point near the center so we get a realistic hit/miss mix. */
    std::mt19937_64 rng(0x5EED1234ULL);
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    for (int i = 0; i < MAXN; i++) {
	vect_t d, off;
	do {
	    VSET(d, u(rng), u(rng), u(rng));
	} while (MAGSQ(d) < 1.0e-6);
	VUNITIZE(d);

	point_t origin;
	VSCALE(origin, d, 5.0 * R);		/* 5R out in direction d   */

	/* aim back toward center +/- 2R jitter -> ~mix of hits/misses */
	VSET(off, u(rng) * 2.0 * R, u(rng) * 2.0 * R, u(rng) * 2.0 * R);
	vect_t dir;
	VSUB2(dir, off, origin);
	VUNITIZE(dir);

	rays[i].magic = RT_RAY_MAGIC;
	rays[i].index = i;
	VMOVE(rays[i].r_pt, origin);
	VMOVE(rays[i].r_dir, dir);

	stparr[i] = stp;
	rparr[i] = &rays[i];
    }

    /* ------------------------------------------------------------------
     * Correctness: confirm shot and vshot agree, ray by ray.
     * ------------------------------------------------------------------ */
    {
	int hits = 0;
	double maxdiff = 0.0;
	int mismatch = 0;
	for (int i = 0; i < MAXN; i++) {
	    struct seg shead;
	    BU_LIST_INIT(&shead.l);
	    int nh = ft->ft_shot(stp, &rays[i], &ap, &shead);

	    struct soltab *one_stp[1] = { stp };
	    struct xray   *one_rp[1]  = { &rays[i] };
	    struct seg     one_seg[1];
	    one_seg[0].seg_stp = (struct soltab *)0;
	    ft->ft_vshot(one_stp, one_rp, one_seg, 1, &ap);

	    int shot_hit = (nh > 0);
	    int vshot_hit = (one_seg[0].seg_stp != (struct soltab *)0);
	    if (shot_hit != vshot_hit) {
		mismatch++;
	    } else if (shot_hit) {
		struct seg *s = BU_LIST_FIRST(seg, &shead.l);
		double din = fabs(s->seg_in.hit_dist - one_seg[0].seg_in.hit_dist);
		double dout = fabs(s->seg_out.hit_dist - one_seg[0].seg_out.hit_dist);
		if (din > maxdiff) maxdiff = din;
		if (dout > maxdiff) maxdiff = dout;
		hits++;
	    }
	    /* recycle the shot seg list */
	    struct seg *s_tmp;
	    while (BU_LIST_WHILE(s_tmp, seg, &shead.l)) {
		BU_LIST_DEQUEUE(&s_tmp->l);
		RT_FREE_SEG(s_tmp, resp);
	    }
	}

	/* shot and vshot run identical arithmetic, so distances should match
	 * to the bit; allow a tiny slack for any compiler FP reassociation. */
	const double match_tol = 1.0e-6;

	printf("# correctness: %d/%d rays hit, %d in/out mismatches, max |dist diff| = %.3e\n",
	       hits, MAXN, mismatch, maxdiff);
	if (mismatch)
	    bu_exit(1, "FAIL: shot/vshot disagree on %d hit/miss result(s)\n", mismatch);
	if (maxdiff > match_tol)
	    bu_exit(1, "FAIL: shot/vshot hit distances differ by %.3e (> %.3e)\n",
		    maxdiff, match_tol);
    }

    /* ------------------------------------------------------------------
     * Timing.
     * ------------------------------------------------------------------ */
    static const int Ns[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    const int nNs = (int)(sizeof(Ns) / sizeof(Ns[0]));

    printf("\n# >= %.1f s per timed phase (reps auto-calibrated)\n\n", min_seconds);
    printf("%6s %12s %11s %7s %12s %11s %7s %9s\n",
	   "N", "shot reps", "shot ns", "shot s", "vshot reps", "vshot ns", "vshot s", "speedup");
    printf("%6s %12s %11s %7s %12s %11s %7s %9s\n",
	   "------", "------------", "-----------", "-------",
	   "------------", "-----------", "-------", "---------");

    volatile double sink = 0.0;	/* defeat dead-code elimination */

    for (int k = 0; k < nNs; k++) {
	const int N = Ns[k];

	/* warm up both paths once */
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

	/* ---- ft_shot: N individual calls per rep; grow reps until the
	 * timed run spans at least min_seconds, then keep that run ---- */
	int64_t shot_reps = 1024;
	int64_t shot_dt = 0;
	double chk_shot = 0.0;
	for (;;) {
	    chk_shot = 0.0;
	    int64_t t0 = bu_gettime();
	    for (int64_t r = 0; r < shot_reps; r++) {
		struct seg shead;
		BU_LIST_INIT(&shead.l);
		for (int i = 0; i < N; i++)
		    ft->ft_shot(stp, &rays[i], &ap, &shead);
		/* consume + recycle (intrinsic to the per-ray seg-list design) */
		struct seg *s;
		while (BU_LIST_WHILE(s, seg, &shead.l)) {
		    chk_shot += s->seg_in.hit_dist;
		    BU_LIST_DEQUEUE(&s->l);
		    RT_FREE_SEG(s, resp);
		}
	    }
	    shot_dt = bu_gettime() - t0;
	    if ((double)shot_dt >= min_us || shot_reps > (INT64_C(1) << 40))
		break;
	    /* scale toward target with 15% headroom (>=2x each step) */
	    double factor = (min_us / (double)(shot_dt > 0 ? shot_dt : 1)) * 1.15;
	    if (factor < 2.0)
		factor = 2.0;
	    shot_reps = (int64_t)((double)shot_reps * factor) + 1;
	}

	/* ---- ft_vshot: one batched call per rep; same calibration ---- */
	int64_t vshot_reps = 1024;
	int64_t vshot_dt = 0;
	double chk_vshot = 0.0;
	for (;;) {
	    chk_vshot = 0.0;
	    int64_t t2 = bu_gettime();
	    for (int64_t r = 0; r < vshot_reps; r++) {
		ft->ft_vshot(stparr, rparr, segarr, N, &ap);
		for (int i = 0; i < N; i++)
		    if (segarr[i].seg_stp)
			chk_vshot += segarr[i].seg_in.hit_dist;
	    }
	    vshot_dt = bu_gettime() - t2;
	    if ((double)vshot_dt >= min_us || vshot_reps > (INT64_C(1) << 40))
		break;
	    double factor = (min_us / (double)(vshot_dt > 0 ? vshot_dt : 1)) * 1.15;
	    if (factor < 2.0)
		factor = 2.0;
	    vshot_reps = (int64_t)((double)vshot_reps * factor) + 1;
	}

	sink += chk_shot + chk_vshot;

	double shot_ns = (double)shot_dt * 1000.0 / ((double)shot_reps * (double)N);
	double vshot_ns = (double)vshot_dt * 1000.0 / ((double)vshot_reps * (double)N);
	double speedup = (vshot_ns > 0.0) ? shot_ns / vshot_ns : 0.0;

	printf("%6d %12" PRId64 " %11.3f %7.1f %12" PRId64 " %11.3f %7.1f %8.2fx\n",
	       N, shot_reps, shot_ns, (double)shot_dt / 1.0e6,
	       vshot_reps, vshot_ns, (double)vshot_dt / 1.0e6, speedup);
	fflush(stdout);
    }

    (void)sink;		/* volatile reads/writes keep the checksum work live */

    bu_free(stp, "soltab");
    return 0;
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
