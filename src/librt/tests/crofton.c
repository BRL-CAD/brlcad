/*                      C R O F T O N . C
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
/** @file librt/tests/crofton.c
 *
 * Stand-alone tests for Crofton-based area/volume estimation.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/time.h"
#include "raytrace.h"
#include "wdb.h"


static double
rel_err(double estimated, double exact)
{
    if (fabs(exact) < SMALL_FASTF)
	return (fabs(estimated) < SMALL_FASTF) ? 0.0 : 1.0;
    return fabs(estimated - exact) / fabs(exact);
}


static int
verify_crofton_estimates(void)
{
    int failures = 0;
    const double tol_pct = 5.0;  /* ±5% acceptable */

    printf("\n--- Crofton estimator verification ---\n");

    struct rt_crofton_params cparams = {0, 0.0, 0.0};

#define CROFTON_CHECK(label, ip_ptr, analytic_sa, analytic_vol) \
    do { \
	struct rt_db_internal *_ip = (ip_ptr); \
	fastf_t _csa = 0.0, _cvol = 0.0; \
	rt_crofton_sample(&_csa, &_cvol, _ip, &cparams); \
	double _sa_err  = fabs(_csa  - (analytic_sa))  / ((analytic_sa)  > 0 ? (analytic_sa)  : 1.0) * 100.0; \
	double _vol_err = fabs(_cvol - (analytic_vol)) / ((analytic_vol) > 0 ? (analytic_vol) : 1.0) * 100.0; \
	const char *_sa_tag  = (_sa_err  <= tol_pct) ? "SA-OK"  : "SA-FAIL"; \
	const char *_vol_tag = (_vol_err <= tol_pct) ? "VOL-OK" : "VOL-FAIL"; \
	printf("  %-42s  SA_err=%.1f%%[%s]  VOL_err=%.1f%%[%s]\n", \
	       (label), _sa_err, _sa_tag, _vol_err, _vol_tag); \
	fflush(stdout); \
	if (_sa_err  > tol_pct) failures++; \
	if (_vol_err > tol_pct) failures++; \
    } while (0)

    {
	struct rt_ell_internal ell;
	memset(&ell, 0, sizeof(ell));
	ell.magic = RT_ELL_INTERNAL_MAGIC;
	VSET(ell.v, 0, 0, 0);
	VSET(ell.a, 10, 0, 0);
	VSET(ell.b, 0, 10, 0);
	VSET(ell.c, 0, 0, 10);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_ELL;
	ip.idb_type       = ID_ELL;
	ip.idb_ptr        = &ell;
	ip.idb_meth       = &OBJ[ID_ELL];

	double analytic_sa  = 4.0 * M_PI * 10.0 * 10.0;
	double analytic_vol = (4.0 / 3.0) * M_PI * 10.0 * 10.0 * 10.0;
	CROFTON_CHECK("sphere r=10", &ip, analytic_sa, analytic_vol);
    }

    {
	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, 0, 0, 20);
	VSET(tgc.a, 5, 0, 0);
	VSET(tgc.b, 0, 5, 0);
	VSET(tgc.c, 5, 0, 0);
	VSET(tgc.d, 0, 5, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double analytic_sa  = 2.0 * M_PI * 5.0 * (5.0 + 20.0);
	double analytic_vol = M_PI * 5.0 * 5.0 * 20.0;
	CROFTON_CHECK("RCC r=5 h=20", &ip, analytic_sa, analytic_vol);

	fastf_t tgc_vol = 0.0;
	ip.idb_meth->ft_volume(&tgc_vol, &ip);
	double tgc_vol_err = fabs(tgc_vol - analytic_vol) / analytic_vol * 100.0;
	printf("  %-42s  analytic_formula_err=%.2f%%  [%s]\n",
	       "RCC r=5 h=20 (rt_tgc_volume)",
	       tgc_vol_err,
	       (tgc_vol_err <= 0.01) ? "OK" : "FORMULA-FAIL");
	if (tgc_vol_err > 0.01) failures++;
    }

    {
	const double sin30 = 0.5;
	const double cos30 = sqrt(3.0) / 2.0;
	const double h_len = 20.0;
	const double r_cyl = 5.0;
	const double Hx    = h_len * sin30;
	const double Hz    = h_len * cos30;

	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, Hx, 0, Hz);
	VSET(tgc.a, r_cyl, 0, 0);
	VSET(tgc.b, 0, r_cyl, 0);
	VSET(tgc.c, r_cyl, 0, 0);
	VSET(tgc.d, 0, r_cyl, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double h_perp       = Hz;
	double analytic_vol = M_PI * r_cyl * r_cyl * h_perp;

	{
	    const int N = 10000;
	    double sum = 0.0;
	    for (int k = 0; k < N; k++) {
		double phi = 2.0 * M_PI * k / N;
		sum += sqrt(Hz * Hz + Hx * Hx * cos(phi) * cos(phi));
	    }
	    double lateral_sa = r_cyl * sum * (2.0 * M_PI / N);
	    double end_caps   = 2.0 * M_PI * r_cyl * r_cyl;
	    double analytic_sa = lateral_sa + end_caps;
	    CROFTON_CHECK("oblique RCC 30deg tilt (Crofton)", &ip, analytic_sa, analytic_vol);
	}

	fastf_t tgc_vol = 0.0;
	ip.idb_meth->ft_volume(&tgc_vol, &ip);
	double tgc_vol_err = fabs(tgc_vol - analytic_vol) / analytic_vol * 100.0;
	double old_vol     = M_PI * r_cyl * r_cyl * h_len;
	double old_err     = fabs(old_vol - analytic_vol) / analytic_vol * 100.0;
	printf("  %-42s  analytic_formula_err=%.2f%%  [%s]  (old_err=%.1f%%)\n",
	       "oblique RCC 30deg (rt_tgc_volume)",
	       tgc_vol_err,
	       (tgc_vol_err <= 0.1) ? "OK" : "FORMULA-FAIL",
	       old_err);
	if (tgc_vol_err > 0.1) failures++;
    }

    /* Sub-mm TRC: reproduces the xyzringtrc.s class of geometry that
     * previously failed because rt_prep_parallel inflates mdl_min/mdl_max to
     * integer-mm boundaries (floor/ceil), blowing up the Crofton bounding
     * sphere and giving only ~90 expected crossings per 50 000 rays instead
     * of ~20 000.  The fix in rt_crofton_shoot uses tight soltab bounding
     * boxes instead of the inflated rti_radius.
     *
     * Geometry: right circular cone, base r=0.030 mm, apex r=0.0003 mm,
     *           height h=0.052 mm (100:1 taper, centered around origin).
     * Default cparams (zero n_rays → convergence-loop mode).              */
    {
	const double r1   = 0.030;    /* base radius   [mm] */
	const double r2   = 0.0003;   /* apex radius   [mm] */
	const double h    = 0.052;    /* height        [mm] */

	struct rt_tgc_internal tgc;
	memset(&tgc, 0, sizeof(tgc));
	tgc.magic = RT_TGC_INTERNAL_MAGIC;
	VSET(tgc.v, 0, 0, 0);
	VSET(tgc.h, 0, 0, h);
	VSET(tgc.a, r1, 0, 0);
	VSET(tgc.b, 0, r1, 0);
	VSET(tgc.c, r2, 0, 0);
	VSET(tgc.d, 0, r2, 0);

	struct rt_db_internal ip;
	RT_DB_INTERNAL_INIT(&ip);
	ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip.idb_minor_type = ID_TGC;
	ip.idb_type       = ID_TGC;
	ip.idb_ptr        = &tgc;
	ip.idb_meth       = &OBJ[ID_TGC];

	double analytic_sa = M_PI * ((r1 + r2) * sqrt((r1-r2)*(r1-r2) + h*h)
				     + r1*r1 + r2*r2);
	double analytic_vol = M_PI * h * (r1*r1 + r1*r2 + r2*r2) / 3.0;
	CROFTON_CHECK("sub-mm TRC (r1=0.030,r2=0.0003,h=0.052)", &ip,
		      analytic_sa, analytic_vol);
    }

#undef CROFTON_CHECK

    printf("  Crofton verification: %d failure(s)\n", failures);
    return failures;
}


static int
build_convergence_db(struct db_i **dbip_out)
{
    struct db_i *dbip = db_open_inmem();
    if (!dbip) return -1;

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return -1;
    }

    point_t center = VINIT_ZERO;
    if (mk_sph(wdbp, "cvg_sphere.s", center, 25.0) < 0) {
	db_close(dbip);
	return -1;
    }

    point_t base = {0.0, 0.0, -20.0};
    vect_t h = {0.0, 0.0, 40.0};
    if (mk_rcc(wdbp, "cvg_rcc.s", base, h, 10.0) < 0) {
	db_close(dbip);
	return -1;
    }

    db_update_nref(dbip, &rt_uniresource);
    *dbip_out = dbip;
    return 0;
}


static int
run_convergence_case(struct db_i *dbip,
		     const char *objname,
		     const char *label,
		     double sa_exact,
		     double v_exact,
		     size_t min_samples,
		     const double *targets,
		     size_t ntargets,
		     double *elapsed_total_sec)
{
    int failures = 0;

    for (size_t i = 0; i < ntargets; i++) {
	double target_pct = targets[i];
	double sa = 0.0, vol = 0.0;
	double run_sec = 0.0;
	int cr = -1;

	struct rt_i *rtip = rt_new_rti(dbip);
	if (!rtip) {
	    printf("  %-24s  target=%.1f%%  init-fail\n", label, target_pct);
	    failures++;
	    continue;
	}

	if (rt_gettree(rtip, objname) != 0) {
	    printf("  %-24s  target=%.1f%%  gettree-fail\n", label, target_pct);
	    rt_free_rti(rtip);
	    failures++;
	    continue;
	}
	rt_prep_parallel(rtip, 1);

	/* Use n_rays = min_samples; target_pct is informational only now */
	struct rt_crofton_params p = { min_samples, 0.0, 0.0 };
	int64_t t0 = bu_gettime();
	cr = rt_crofton_shoot(rtip, &p, &sa, &vol);
	run_sec = (double)(bu_gettime() - t0) / 1000000.0;
	rt_free_rti(rtip);

	if (elapsed_total_sec)
	    *elapsed_total_sec += run_sec;

	if (cr != 0) {
	    printf("  %-24s  target=%.1f%%  crofton-fail (ret=%d)\n",
		   label, target_pct, cr);
	    failures++;
	    continue;
	}

	double sa_err_pct = rel_err(sa, sa_exact) * 100.0;
	double v_err_pct = rel_err(vol, v_exact) * 100.0;
	double max_err_pct = (target_pct < 6.0) ? 10.0 : 12.0;

	printf("  %-24s  target=%.1f%%  time=%.3fs  SA_err=%.2f%%  V_err=%.2f%%\n",
	       label, target_pct, run_sec, sa_err_pct, v_err_pct);
	if (sa_err_pct > max_err_pct || v_err_pct > max_err_pct)
	    failures++;
    }

    return failures;
}


static int
test_crofton_convergence_timing(void)
{
    int failures = 0;
    struct db_i *dbip = DBI_NULL;
    const double targets[] = {10.0, 7.0, 5.0};
    double elapsed_total = 0.0;

    printf("\n--- Crofton convergence timing ---\n");

    if (build_convergence_db(&dbip) != 0 || !dbip) {
	printf("  FAIL: could not build in-memory convergence database\n");
	return 1;
    }

    failures += run_convergence_case(
	dbip, "cvg_sphere.s", "sphere r=25",
	4.0 * M_PI * 25.0 * 25.0,
	(4.0 / 3.0) * M_PI * 25.0 * 25.0 * 25.0,
	300, targets, 3, &elapsed_total);

    failures += run_convergence_case(
	dbip, "cvg_rcc.s", "rcc r=10 h=40",
	2.0 * M_PI * 10.0 * (10.0 + 40.0),
	M_PI * 10.0 * 10.0 * 40.0,
	300, targets, 3, &elapsed_total);

    printf("  total convergence runtime: %.3fs\n", elapsed_total);
    if (elapsed_total > 5.0) {
	printf("  FAIL: convergence runtime %.3fs exceeds 5s target\n", elapsed_total);
	failures++;
    }

    db_close(dbip);
    printf("  Crofton convergence timing: %d failure(s)\n", failures);
    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	bu_exit(1, "Usage: %s\n", argv[0]);

    int failures = 0;
    failures += verify_crofton_estimates();
    failures += test_crofton_convergence_timing();

    printf("\n=== Summary: %d failure(s) ===\n", failures);
    return (failures > 0) ? 1 : 0;
}
