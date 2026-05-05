/*               D S P _ R A Y T R A C E _ C H E C K . C
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
/** @file librt/tests/dsp_raytrace_check.c
 *
 * Consistency and timing check for the DSP HBB-pyramid + 2D Amanatides-Woo
 * DDA raytracer.  DSPs are treated as height fields, not generic meshes.
 *
 * For each test geometry the program runs independent estimates:
 *
 *   1. Mesh-Ref: exact SA and volume computed directly from the height grid
 *      using the same triangulation as the DSP cut mode (deterministic, no sampling).
 *      This is the ground-truth reference.
 *
 *   2. DDA Crofton: rt_crofton_shoot with the default HBB-pyramid + 2D-DDA
 *      shot path.  Should agree with Mesh-Ref within ~5%.
 *
 *   3. For synthetic flat cases: closed-form analytical SA/vol for a
 *      sanity double-check of the Mesh-Ref computation itself.
 *
 * Timing metrics are collected for the DDA path.
 *
 * Usage:
 *   dsp_raytrace_check                          (synthetic cases only)
 *   dsp_raytrace_check <file.g> <object-name>  (also test a real .g file)
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/time.h"
#include "rt/defines.h"
#include "raytrace.h"
#include "wdb.h"

/* ------------------------------------------------------------------ */
/* Tuneable parameters                                                  */
/* ------------------------------------------------------------------ */

/*
 * Accuracy pass: use convergence-based sampling rather than a fixed
 * single-pass ray count.  CROFTON_ACCURACY_THRESH is kept as a positive
 * sentinel value so that run_crofton() knows to use the convergence path;
 * its numeric magnitude is not used directly.
 *
 * The convergence criterion is stability_mm: sampling stops once the
 * equivalent-sphere-radius estimate changes by less than CROFTON_STAB_MM
 * between successive 2 000-ray batches.  At typical DSP geometry sizes
 * (r_sa ~ 10–100 units) this requires ~20k–200k rays (< 0.1 s) and keeps
 * Crofton SA noise well below 1 %, avoiding borderline threshold failures
 * that arise from a fixed low ray count hitting an unlucky random sequence.
 */
#define CROFTON_ACCURACY_THRESH  3.0   /* positive sentinel: use convergence path */
#define CROFTON_STAB_MM          0.05  /* equivalent-radius stability target (mm) */
#define CROFTON_TIMEOUT_MS    5000.0   /* safety wall-clock timeout (ms)          */

/* Rays for the single-iteration timing measurement. */
#define CROFTON_TIMING_RAYS     50000u

/* 129 vertices define 128 cells, a power-of-two cell count common in terrain
 * grids and useful for exercising multiple HBB/DDA levels without an expensive
 * normal regression test.
 */
#define DSP_TEST_LARGE_GRID_DIM 129u
#define DSP_RAY_CHECK_TOLERANCE 1.0e-5
#define DSP_RAY_CHECK_NEAR_TOP_OFFSET 1.0e-4
/* Private dsp.c surface number for terrain-top hits.  The test keeps this
 * local rather than including private primitive internals; keep synchronized
 * with ZTOP in primitives/dsp/dsp.c.
 */
#define DSP_TEST_ZTOP 7


/*
 * The DDA / HBB-pyramid path is the default DSP raytracer.  It should agree
 * with the mesh reference to within typical Crofton sampling noise (~5 %).
 * Errors larger than DSP_DDA_MESHREF_PCT are test failures.
 */
#define DSP_DDA_MESHREF_PCT     10.0   /* DDA vs mesh-ref tolerance */


/* ------------------------------------------------------------------ */
/* Accessors in dsp.c (not in public headers).                         */
/* ------------------------------------------------------------------ */
extern void  dsp_query_terrain(struct soltab *stp,
       const unsigned short **pbuf,
       unsigned int *pxcnt,
       unsigned int *pycnt,
       fastf_t *stom16,
       int *cuttype,
       int *smooth);


/* ------------------------------------------------------------------ */
/* Exact mesh-reference SA and volume from a height grid               */
/* ------------------------------------------------------------------ */

/**
 * Compute exact surface area (SA) and volume of the closed DSP solid
 * from its raw uint16 height array.
 *
 * The computation uses the same triangulation as the DSP cut mode:
 *   llUR cells are split along the A–D diagonal into two triangles:
 *     T1: A=(x,y,hA), B=(x+1,y,hB), D=(x+1,y+1,hD)
 *     T2: A=(x,y,hA), D=(x+1,y+1,hD), C=(x,y+1,hC)
 *   ULlr cells are split along the B–C diagonal.  Adaptive cells select
 *   between those two splits using the same curvature test as permute_cell().
 *
 * Wall panels (XMIN/XMAX/YMIN/YMAX) and the bottom face are also included.
 *
 * @param buf    Row-major height array, buf[y*xcnt + x].
 * @param xcnt   Number of grid columns (>= 2).
 * @param ycnt   Number of grid rows    (>= 2).
 * @param dx     Model-space width  of one cell (stom[0], mm if identity).
 * @param dy     Model-space height of one cell (stom[5]).
 * @param dz     Model-space scale  per height unit (stom[10]).
 * @param cuttype DSP_CUT_DIR_* triangulation selector.
 * @param sa_out Receives total surface area in model-space units^2.
 * @param vol_out Receives total volume in model-space units^3.
 */
static int
cell_cuttype(const unsigned short *buf, unsigned int xcnt, unsigned int ycnt,
	     unsigned int x, unsigned int y, int cuttype)
{
    if (cuttype != DSP_CUT_DIR_ADAPT)
	return cuttype;

    /* Mirror dsp.c:permute_cell().  Adaptive mode estimates curvature along
     * the two possible diagonals using the neighboring diagonal samples.  The
     * diagonal with lower accumulated second-difference curvature is selected
     * so the two triangles follow the locally smoother direction.
     */
    unsigned int lo_x = (x > 0) ? x - 1 : 0;
    unsigned int lo_y = (y > 0) ? y - 1 : 0;
    /* For interior cells, hi_* is one sample beyond the cell's high edge
     * (x+2/y+2), matching dsp.c.  At the outer boundary no beyond-cell sample
     * exists, so this clamps to the cell's high edge (x+1/y+1).
     */
    unsigned int hi_x = (x + 2 < xcnt) ? x + 2 : xcnt - 1;
    unsigned int hi_y = (y + 2 < ycnt) ? y + 2 : ycnt - 1;

    double A = buf[y * xcnt + x];
    double B = buf[y * xcnt + (x + 1)];
    double C = buf[(y + 1) * xcnt + x];
    double D = buf[(y + 1) * xcnt + (x + 1)];

    double cAD = fabs(D + buf[lo_y * xcnt + lo_x] - 2.0 * A) +
	fabs(buf[hi_y * xcnt + hi_x] + A - 2.0 * D);
    double cBC = fabs(C + buf[lo_y * xcnt + hi_x] - 2.0 * B) +
	fabs(buf[hi_y * xcnt + lo_x] + B - 2.0 * C);

    return (cAD < cBC) ? DSP_CUT_DIR_llUR : DSP_CUT_DIR_ULlr;
}


static void
add_ref_triangle(double ax, double ay, double az,
		 double bx, double by, double bz,
		 double cx, double cy, double cz,
		 double *sa, double *vol)
{
    double abx = bx - ax, aby = by - ay, abz = bz - az;
    double acx = cx - ax, acy = cy - ay, acz = cz - az;
    double nx = aby * acz - abz * acy;
    double ny = abz * acx - abx * acz;
    double nz = abx * acy - aby * acx;

    *sa += 0.5 * sqrt(nx*nx + ny*ny + nz*nz);
    *vol += nz * (az + bz + cz) / 6.0;
}


static void
compute_grid_sa_vol(const unsigned short *buf,
    unsigned int xcnt, unsigned int ycnt,
    double dx, double dy, double dz, int cuttype,
    double *sa_out, double *vol_out)
{
    if (!buf || xcnt < 2 || ycnt < 2) {
if (sa_out)  *sa_out  = 0.0;
if (vol_out) *vol_out = 0.0;
return;
    }

    const unsigned int nx = xcnt - 1;  /* cells wide */
    const unsigned int ny = ycnt - 1;  /* cells tall */
    double sa  = 0.0;
    double vol = 0.0;

    /* --- Terrain top surface ------------------------------------------ */
    for (unsigned int y = 0; y < ny; y++) {
for (unsigned int x = 0; x < nx; x++) {
    /* corner heights in model-space Z */
    double hA = buf[ y      * xcnt +  x    ] * dz;
    double hB = buf[ y      * xcnt + (x+1) ] * dz;
    double hC = buf[(y+1)   * xcnt +  x    ] * dz;
    double hD = buf[(y+1)   * xcnt + (x+1) ] * dz;

    double x0 = x * dx, x1 = (x + 1) * dx;
    double y0 = y * dy, y1 = (y + 1) * dy;
    if (cell_cuttype(buf, xcnt, ycnt, x, y, cuttype) == DSP_CUT_DIR_ULlr) {
	add_ref_triangle(x0, y0, hA, x1, y0, hB, x0, y1, hC, &sa, &vol);
	add_ref_triangle(x1, y0, hB, x1, y1, hD, x0, y1, hC, &sa, &vol);
    } else {
	add_ref_triangle(x0, y0, hA, x1, y0, hB, x1, y1, hD, &sa, &vol);
	add_ref_triangle(x0, y0, hA, x1, y1, hD, x0, y1, hC, &sa, &vol);
    }
}
    }

    /* --- Four side walls ----------------------------------------------- */
    /* XMIN wall (x=0): panels in the y-z model plane.
     * Panel y: quad from (0,y*dy,0)→(0,y*dy,hA)→(0,(y+1)*dy,hC)→(0,(y+1)*dy,0)
     * Split into:
     *   T1: (y*dy,0),(y*dy,hA),((y+1)*dy,hC) → area = hA*dy/2
     *   T2: (y*dy,0),((y+1)*dy,hC),((y+1)*dy,0) → area = hC*dy/2
     * (computed as 2D triangles in the y-z plane)
     */
    for (unsigned int y = 0; y < ny; y++) {
double hA = buf[ y    * xcnt] * dz;
double hC = buf[(y+1) * xcnt] * dz;
sa += 0.5 * dy * (hA + hC);
    }
    /* XMAX wall (x=nx): */
    for (unsigned int y = 0; y < ny; y++) {
double hB = buf[ y    * xcnt + nx] * dz;
double hD = buf[(y+1) * xcnt + nx] * dz;
sa += 0.5 * dy * (hB + hD);
    }
    /* YMIN wall (y=0): panels in the x-z plane */
    for (unsigned int x = 0; x < nx; x++) {
double hA = buf[x    ] * dz;
double hB = buf[x + 1] * dz;
sa += 0.5 * dx * (hA + hB);
    }
    /* YMAX wall (y=ny): */
    for (unsigned int x = 0; x < nx; x++) {
double hC = buf[ny * xcnt +  x   ] * dz;
double hD = buf[ny * xcnt + (x+1)] * dz;
sa += 0.5 * dx * (hC + hD);
    }

    /* --- Bottom face --------------------------------------------------- */
    sa += (double)nx * dx * (double)ny * dy;

    if (sa_out)  *sa_out  = sa;
    if (vol_out) *vol_out = vol;
}



/* ------------------------------------------------------------------ */
/* One timed Crofton run                                                */
/* ------------------------------------------------------------------ */

struct crofton_result {
    double sa;
    double vol;
    double wall_sec;
    int    ok;
};

static struct crofton_result
run_crofton(struct rt_i *rtip, size_t nrays, double thresh_pct)
{
    struct crofton_result r;
    memset(&r, 0, sizeof(r));

    struct rt_crofton_params p;
    if (thresh_pct > 0.0) {
	/* Accuracy run: convergence-based sampling with a safety timeout.
	 * Fire 2 000-ray batches until the equivalent-sphere-radius estimate
	 * changes by less than CROFTON_STAB_MM between successive batches, or
	 * until CROFTON_TIMEOUT_MS wall-clock milliseconds have elapsed.
	 *
	 * For the DSP test geometries (r_sa ~ 10–100 units) this typically
	 * converges in 20k–200k total rays (< 0.1 s), keeping Crofton SA
	 * noise well below 1 % and avoiding the outlier results produced by a
	 * fixed low ray count that hits an unlucky random sequence.            */
	p.n_rays       = 0;
	p.stability_mm = CROFTON_STAB_MM;
	p.time_ms      = CROFTON_TIMEOUT_MS;
    } else {
	/* Timing run: fixed ray count so the throughput measurement is
	 * reproducible regardless of geometry or convergence behaviour.        */
	p.n_rays       = nrays;
	p.stability_mm = 0.0;
	p.time_ms      = 0.0;
    }

    int64_t t0 = bu_gettime();
    r.ok = rt_crofton_shoot(rtip, &p, &r.sa, &r.vol);
    r.wall_sec = (double)(bu_gettime() - t0) / 1e6;
    return r;
}


/* ------------------------------------------------------------------ */
/* Core comparison: mesh-ref vs DDA Crofton                            */
/* ------------------------------------------------------------------ */

static double
pct_err(double a, double b)
{
    double ref = fabs(b);
    if (ref < SMALL_FASTF)
return (fabs(a) < SMALL_FASTF) ? 0.0 : 100.0;
    return fabs(a - b) / ref * 100.0;
}


/**
 * Run DDA Crofton passes on an already-prepped @p rtip, compare
 * against @p ref_sa / @p ref_vol (from compute_grid_sa_vol), and report
 * timing.  Also compare flat-case analytical values when provided.
 *
 * @returns Number of test failures.
 */
static int
compare_paths(const char  *label,
      struct rt_i *rtip,
      double       ref_sa,    /* mesh-ref SA,  -1 if unavailable */
      double       ref_vol,   /* mesh-ref vol, -1 if unavailable */
      double       ana_sa,    /* analytic SA,  -1 if unavailable */
      double       ana_vol,   /* analytic vol, -1 if unavailable */
      double       prep_sec)
{
    int failures = 0;

    printf("\n  %-55s\n", label);
    if (prep_sec > 0.0)
	printf("    Prep (DDA/HBB): %.3f s\n", prep_sec);

    struct crofton_result dda_acc = run_crofton(rtip,
	0 /* n_rays: 0 → use convergence-based path (thresh_pct > 0) */,
	CROFTON_ACCURACY_THRESH);
    struct crofton_result dda_tim = run_crofton(rtip,
	CROFTON_TIMING_RAYS, 0.0);

    printf("\n    %-12s  %14s  %14s  %10s  %10s\n",
	   "PATH", "SA", "VOL", "SA_err%", "VOL_err%");

    if (ref_sa > 0.0)
	printf("    %-12s  %14.6g  %14.6g\n",
	       "Mesh-Ref", ref_sa, ref_vol);

    if (ana_sa > 0.0) {
	double asa_err  = pct_err(ref_sa,  ana_sa);
	double avol_err = pct_err(ref_vol, ana_vol);
	printf("    %-12s  %14.6g  %14.6g  %9.2f%%  %9.2f%%"
	       "  (ref vs analytic)\n",
	       "Analytic", ana_sa, ana_vol, asa_err, avol_err);
	if (asa_err > 1.0) {
	    printf("    FAIL: Mesh-Ref SA deviates from analytic by %.2f%%\n",
	   asa_err);
	    failures++;
	}
	if (avol_err > 1.0) {
	    printf("    FAIL: Mesh-Ref vol deviates from analytic by %.2f%%\n",
	   avol_err);
	    failures++;
	}
    }

    double dda_sa_err  = (ref_sa  > 0.0) ? pct_err(dda_acc.sa,  ref_sa)  : -1.0;
    double dda_vol_err = (ref_vol > 0.0) ? pct_err(dda_acc.vol, ref_vol) : -1.0;

    if (dda_sa_err >= 0.0)
	printf("    %-12s  %14.6g  %14.6g  %9.2f%%  %9.2f%%\n",
	       "DDA", dda_acc.sa, dda_acc.vol, dda_sa_err, dda_vol_err);
    else
	printf("    %-12s  %14.6g  %14.6g\n",
	       "DDA", dda_acc.sa, dda_acc.vol);

    double dda_rps = (dda_tim.wall_sec > 1e-9)
	? CROFTON_TIMING_RAYS / dda_tim.wall_sec : 0.0;

    printf("\n    %-12s  %8s  %12s  %12s\n",
	   "PATH", "rays", "wall_sec", "rays/sec");
    printf("    %-12s  %8u  %12.4f  %12.0f\n",
	   "DDA", CROFTON_TIMING_RAYS, dda_tim.wall_sec, dda_rps);

    const char *dsa  = (dda_sa_err  < 0.0 || dda_sa_err  <= DSP_DDA_MESHREF_PCT) ? "OK" : "FAIL";
    const char *dvol = (dda_vol_err < 0.0 || dda_vol_err <= DSP_DDA_MESHREF_PCT) ? "OK" : "FAIL";
    printf("    Checks: DDA/Ref SA[%s] DDA/Ref Vol[%s]\n", dsa, dvol);

    if (ref_sa > 0.0) {
	if (dda_sa_err  > DSP_DDA_MESHREF_PCT) {
	    printf("    FAIL: DDA SA vs Mesh-Ref: %.2f%% > %.1f%%\n",
		   dda_sa_err, DSP_DDA_MESHREF_PCT);
	    failures++;
	}
	if (dda_vol_err > DSP_DDA_MESHREF_PCT) {
	    printf("    FAIL: DDA vol vs Mesh-Ref: %.2f%% > %.1f%%\n",
		   dda_vol_err, DSP_DDA_MESHREF_PCT);
	    failures++;
	}
    }

    return failures;
}



/* ------------------------------------------------------------------ */
/* Analytical SA/vol for a flat DSP in solid (grid) space              */
/* ------------------------------------------------------------------ */

/**
 * Closed-form SA and volume for a flat DSP with a diagonal stom
 * (no shear, dx = stom[0], dy = stom[5], dz = stom[10]):
 *
 *   SA  = 2*nx*dx*ny*dy            (top + bottom)
 *       + 2*h*dz*ny*dy             (XMIN + XMAX walls)
 *       + 2*h*dz*nx*dx             (YMIN + YMAX walls)
 *   Vol = nx*dx * ny*dy * h*dz
 */
static void
flat_dsp_analytic(uint32_t xcnt, uint32_t ycnt, unsigned short h,
  double dx, double dy, double dz,
  double *sa_out, double *vol_out)
{
    double nx  = xcnt - 1.0;
    double ny  = ycnt - 1.0;
    double hdz = (double)h * dz;
    *sa_out  = 2.0*nx*dx*ny*dy + 2.0*hdz*ny*dy + 2.0*hdz*nx*dx;
    *vol_out = nx*dx * ny*dy * hdz;
}


/* ------------------------------------------------------------------ */
/* Build an in-memory DB, prep, and run comparison                     */
/* ------------------------------------------------------------------ */

struct dsp_case {
    const char     *label;
    uint32_t        xcnt, ycnt;
    const unsigned short *buf;
    int             cuttype;
    int             smooth;
    int             ray_checks;
    /* stom diagonal (identity: dx=dy=dz=1): */
    double          dx, dy, dz;
    /* analytic SA/vol (-1 if not available): */
    double          analytic_sa, analytic_vol;
};


static struct soltab *
first_dsp_soltab(struct rt_i *rtip)
{
    struct soltab *stp;
    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	if (stp->st_id == ID_DSP)
	    return stp;
    } RT_VISIT_ALL_SOLTABS_END;
    return NULL;
}


static int
check_direct_ray(const char *label, struct soltab *stp, struct rt_i *rtip,
		 const point_t origin, const vect_t direction,
		 int expect_segments, double expect_in, double expect_out,
		 int check_top_normal)
{
    int failures = 0;
    struct application ap;
    struct seg seghead;
    struct seg *segp;
    struct resource *resp = &rt_uniresource;
    int nseg = 0;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = resp;
    ap.a_onehit = 0;
    VMOVE(ap.a_ray.r_pt, origin);
    VMOVE(ap.a_ray.r_dir, direction);
    VUNITIZE(ap.a_ray.r_dir);
    ap.a_ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);

    (void)rt_obj_shot(stp, &ap.a_ray, &ap, &seghead);
    for (BU_LIST_FOR(segp, seg, &seghead.l))
	nseg++;

    if (nseg != expect_segments) {
	printf("    FAIL: %s segments %d != %d\n", label, nseg, expect_segments);
	failures++;
	goto done;
    }

    if (expect_segments > 0) {
	segp = BU_LIST_FIRST(seg, &seghead.l);
	if (expect_in >= 0.0 && fabs(segp->seg_in.hit_dist - expect_in) > DSP_RAY_CHECK_TOLERANCE) {
	    printf("    FAIL: %s in dist %.9g != %.9g\n",
		   label, segp->seg_in.hit_dist, expect_in);
	    failures++;
	}
	if (expect_out >= 0.0 && fabs(segp->seg_out.hit_dist - expect_out) > DSP_RAY_CHECK_TOLERANCE) {
	    printf("    FAIL: %s out dist %.9g != %.9g\n",
		   label, segp->seg_out.hit_dist, expect_out);
	    failures++;
	}
	if (check_top_normal) {
	    struct hit nhit = segp->seg_in;
	    if (nhit.hit_surfno != DSP_TEST_ZTOP) {
		printf("    FAIL: %s expected ZTOP in-hit, got surfno %d\n",
		       label, nhit.hit_surfno);
		failures++;
	    } else {
		rt_obj_norm(&nhit, stp, &ap.a_ray);
		double nmag = MAGNITUDE(nhit.hit_normal);
		if (fabs(nmag - 1.0) > DSP_RAY_CHECK_TOLERANCE || nhit.hit_normal[Z] <= 0.0) {
		    printf("    FAIL: %s normal invalid (%g %g %g), |N|=%g\n",
			   label, V3ARGS(nhit.hit_normal), nmag);
		    failures++;
		}
	    }
	}
    }

done:
    RT_FREE_SEG_LIST(&seghead, resp);
    return failures;
}


static int
check_smooth_normal(const struct dsp_case *tc, struct soltab *stp)
{
    struct xray ray;
    struct hit hit;
    point_t p;

    VSET(p, 1.5 * tc->dx, 1.5 * tc->dy, 0.0);
    VSET(ray.r_pt, p[X], p[Y], 10000.0);
    VSET(ray.r_dir, 0.0, 0.0, -1.0);
    ray.magic = RT_RAY_MAGIC;

    memset(&hit, 0, sizeof(hit));
    hit.hit_magic = RT_HIT_MAGIC;
    hit.hit_surfno = DSP_TEST_ZTOP;
    hit.hit_vpriv[X] = 1.0;
    hit.hit_vpriv[Y] = 1.0;
    hit.hit_vpriv[Z] = 0.0;

    double hA = tc->buf[1 * tc->xcnt + 1] * tc->dz;
    double hB = tc->buf[1 * tc->xcnt + 2] * tc->dz;
    double hC = tc->buf[2 * tc->xcnt + 1] * tc->dz;
    double hD = tc->buf[2 * tc->xcnt + 2] * tc->dz;
    double h = 0.25 * (hA + hB + hC + hD);
    hit.hit_dist = ray.r_pt[Z] - h;

    rt_obj_norm(&hit, stp, &ray);
    double nmag = MAGNITUDE(hit.hit_normal);
    if (fabs(nmag - 1.0) > DSP_RAY_CHECK_TOLERANCE || hit.hit_normal[Z] <= 0.0) {
	printf("    FAIL: smooth=%d synthetic ZTOP normal invalid (%g %g %g), |N|=%g\n",
	       tc->smooth, V3ARGS(hit.hit_normal), nmag);
	return 1;
    }

    return 0;
}


static int
run_prepped_ray_checks(const struct dsp_case *tc, struct rt_i *rtip)
{
    if (!tc->ray_checks)
	return 0;

    int failures = 0;
    struct soltab *stp = first_dsp_soltab(rtip);
    if (!stp) {
	printf("    FAIL: deterministic ray checks could not find DSP soltab\n");
	return 1;
    }

    double x_extent = (tc->xcnt - 1) * tc->dx;
    double y_extent = (tc->ycnt - 1) * tc->dy;
    /* ray_checks == 1 is reserved for flat DSP cases, so buf[0] is the
     * expected terrain height for the deterministic vertical/side rays.
     */
    double top_z = (double)tc->buf[0] * tc->dz;
    double expected_vertical_in = 10.0;
    double expected_vertical_out = expected_vertical_in + top_z;
    point_t o;
    vect_t d;

    if (tc->ray_checks == 1) {
	VSET(o, 0.5 * x_extent, 0.5 * y_extent, top_z + 10.0);
	VSET(d, 0.0, 0.0, -1.0);
	failures += check_direct_ray("vertical down flat DSP", stp, rtip,
				     o, d, 1, expected_vertical_in,
				     expected_vertical_out, tc->smooth);

	VSET(o, -tc->dx, 0.5 * y_extent, 0.5 * top_z);
	VSET(d, 1.0, 0.0, 0.0);
	failures += check_direct_ray("side entry flat DSP", stp, rtip,
				     o, d, 1, tc->dx, tc->dx + x_extent, 0);

	VSET(o, 0.5 * x_extent, 0.5 * y_extent, 0.5 * top_z);
	VSET(d, 1.0, 0.0, 0.0);
	failures += check_direct_ray("inside start flat DSP", stp, rtip,
				     o, d, 1, -0.5 * x_extent, 0.5 * x_extent, 0);

	VSET(o, -tc->dx, 0.25 * y_extent, top_z - DSP_RAY_CHECK_NEAR_TOP_OFFSET);
	VSET(d, 1.0, 0.0, 0.0);
	failures += check_direct_ray("near-top side entry flat DSP", stp, rtip,
				     o, d, 1, tc->dx, tc->dx + x_extent, 0);

	VSET(o, 0.5 * x_extent, 0.5 * y_extent, -10.0);
	VSET(d, 0.0, 0.0, 1.0);
	failures += check_direct_ray("vertical up flat DSP", stp, rtip,
				     o, d, 1, expected_vertical_in,
				     expected_vertical_out, 0);
    } else if (tc->ray_checks == 2) {
	VSET(o, 0.5 * x_extent, 0.5 * y_extent, 10000.0);
	VSET(d, 0.0, 0.0, -1.0);
	failures += check_direct_ray("smooth-mode direct vertical shot", stp, rtip,
				     o, d, 1, -1.0, -1.0, 0);
	failures += check_smooth_normal(tc, stp);
    }

    return failures;
}


static int
run_inmem_case(const struct dsp_case *tc)
{
    struct db_i  *dbip = db_open_inmem();
    if (!dbip) { printf("  FAIL: db_open_inmem\n"); return 1; }

    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) { db_close(dbip); return 1; }

    const char *data_name = "dsp_test.data";
    const char *dsp_name  = "dsp_test.s";

    if (mk_binunif(wdbp, data_name,
   (const void *)tc->buf,
   WDB_BINUNIF_UINT16,
   (long)(tc->xcnt * tc->ycnt)) < 0) {
db_close(dbip);
return 1;
    }

    /* Build DSP primitive with a diagonal stom derived from dx/dy/dz. */
    struct rt_dsp_internal *dsp;
    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic       = RT_DSP_INTERNAL_MAGIC;
    dsp->dsp_xcnt    = tc->xcnt;
    dsp->dsp_ycnt    = tc->ycnt;
    dsp->dsp_smooth  = tc->smooth;
    dsp->dsp_cuttype = tc->cuttype;
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, data_name);
    MAT_ZERO(dsp->dsp_stom);
    dsp->dsp_stom[0]  = tc->dx;
    dsp->dsp_stom[5]  = tc->dy;
    dsp->dsp_stom[10] = tc->dz;
    dsp->dsp_stom[15] = 1.0;
    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);
    dsp->dsp_bip = NULL;
    dsp->dsp_mp  = NULL;

    /* wdb_export → wdb_put_internal → rt_db_free_internal frees the internal
     * (runs rt_dsp_ifree which frees dsp_name and the struct itself).
     * Do NOT touch dsp after this call. */
    int export_ok = wdb_export(wdbp, dsp_name, (void *)dsp, ID_DSP, 1.0);
    dsp = NULL; /* ownership transferred: pointer is now dangling */

    if (export_ok < 0) { db_close(dbip); return 1; }

    db_update_nref(dbip);

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) { db_close(dbip); return 1; }

    if (rt_gettree(rtip, dsp_name) != 0) {
rt_free_rti(rtip);
db_close(dbip);
return 1;
    }

    int64_t t0 = bu_gettime();
    rt_prep_parallel(rtip, 1);
    double prep_sec = (double)(bu_gettime() - t0) / 1e6;

    /* Compute exact mesh reference from the raw height buffer. */
    double ref_sa = 0.0, ref_vol = 0.0;
    compute_grid_sa_vol(tc->buf, tc->xcnt, tc->ycnt,
tc->dx, tc->dy, tc->dz, tc->cuttype,
&ref_sa, &ref_vol);

    int failures = compare_paths(tc->label, rtip,
 ref_sa,  ref_vol,
 tc->analytic_sa, tc->analytic_vol,
 prep_sec);
    failures += run_prepped_ray_checks(tc, rtip);

    rt_free_rti(rtip);
    db_close(dbip);
    return failures;
}


/* ------------------------------------------------------------------ */
/* Run a real .g file                                                   */
/* ------------------------------------------------------------------ */

static int
run_file_case(const char *gfile, const char *objname)
{
    char label[320];
    snprintf(label, sizeof(label), "real terrain: %s / %s", gfile, objname);

    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) {
printf("  FAIL: cannot open %s\n", gfile);
return 1;
    }
    db_dirbuild(dbip);

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) { db_close(dbip); return 1; }

    if (rt_gettree(rtip, objname) != 0) {
printf("  FAIL: rt_gettree('%s') failed in %s\n", objname, gfile);
rt_free_rti(rtip);
db_close(dbip);
return 1;
    }

    int64_t t0 = bu_gettime();
    rt_prep_parallel(rtip, 1);
    double prep_sec = (double)(bu_gettime() - t0) / 1e6;

    /* Retrieve the height buffer from the first DSP soltab. */
    const unsigned short *buf = NULL;
    unsigned int xcnt = 0, ycnt = 0;
    fastf_t stom[16];
    int cuttype = DSP_CUT_DIR_llUR;
    int smooth = 0;
    MAT_IDN(stom);

    struct soltab *stp;
    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
if (stp->st_id == ID_DSP && !buf) {
    dsp_query_terrain(stp, &buf, &xcnt, &ycnt, stom, &cuttype, &smooth);
}
    } RT_VISIT_ALL_SOLTABS_END;

    double ref_sa = -1.0, ref_vol = -1.0;
    if (buf && xcnt >= 2 && ycnt >= 2) {
double dx = stom[0], dy = stom[5], dz = stom[10];
compute_grid_sa_vol(buf, xcnt, ycnt, dx, dy, dz, cuttype, &ref_sa, &ref_vol);
printf("  Mesh-Ref (from %u x %u grid, "
       "dx=%.4g dy=%.4g dz=%.4g cut=%d smooth=%d):\n"
       "    SA = %.6g  Vol = %.6g\n",
       xcnt, ycnt, dx, dy, dz, cuttype, smooth, ref_sa, ref_vol);
    } else {
printf("  WARNING: could not retrieve height buffer for mesh reference\n");
    }

    int failures = compare_paths(label, rtip,
 ref_sa, ref_vol,
 -1.0, -1.0,
 prep_sec);

    rt_free_rti(rtip);
    db_close(dbip);
    return failures;
}


/* ------------------------------------------------------------------ */
/* Synthetic test cases                                                 */
/* ------------------------------------------------------------------ */

static int
test_synthetic(void)
{
    int failures = 0;

    printf("\n=== Synthetic DSP cases ===\n");

    /* --- 1. Flat 5x5 grid (4x4 cells), height=100, identity stom --- */
    {
const uint32_t GW = 5, GH = 5;
unsigned short buf[25];
for (int i = 0; i < 25; i++) buf[i] = 100;

double asa, avol;
flat_dsp_analytic(GW, GH, 100, 1.0, 1.0, 1.0, &asa, &avol);

struct dsp_case tc = {
    "flat 5x5 h=100 (4x4 cells, identity stom)",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 1, 1.0, 1.0, 1.0, asa, avol
};
failures += run_inmem_case(&tc);
    }

    /* --- 2. Flat 10x10 grid (9x9 cells), height=200 --- */
    {
const uint32_t GW = 10, GH = 10;
unsigned short buf[100];
for (int i = 0; i < 100; i++) buf[i] = 200;

double asa, avol;
flat_dsp_analytic(GW, GH, 200, 1.0, 1.0, 1.0, &asa, &avol);

struct dsp_case tc = {
    "flat 10x10 h=200 (9x9 cells)",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 0, 1.0, 1.0, 1.0, asa, avol
};
failures += run_inmem_case(&tc);
    }

    /* --- 3. Flat 10x10 grid with non-unit stom (dx=5, dy=5, dz=2) --- */
    {
const uint32_t GW = 10, GH = 10;
unsigned short buf[100];
for (int i = 0; i < 100; i++) buf[i] = 50;

double asa, avol;
flat_dsp_analytic(GW, GH, 50, 5.0, 5.0, 2.0, &asa, &avol);

struct dsp_case tc = {
    "flat 10x10 h=50 (stom dx=5 dy=5 dz=2)",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 1, 5.0, 5.0, 2.0, asa, avol
};
failures += run_inmem_case(&tc);
    }

    /* --- 4. Linear ramp 9x9 grid --- */
    {
const uint32_t GW = 9, GH = 9;
unsigned short buf[81];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++)
buf[y * GW + x] = (unsigned short)(100 + 20*x + 10*y);

struct dsp_case tc = {
    "ramp 9x9 (h=100+20x+10y)",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 5. Explicit ULlr cut on an asymmetric 5x5 terrain --- */
    {
const uint32_t GW = 5, GH = 5;
unsigned short buf[25];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++)
	buf[y * GW + x] = (unsigned short)(80 + 35*x + 7*y + 11*x*y);

struct dsp_case tc = {
    "asymmetric 5x5 explicit ULlr cut",
    GW, GH, buf, DSP_CUT_DIR_ULlr, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 6. Adaptive cut on an asymmetric 5x5 terrain --- */
    {
const uint32_t GW = 5, GH = 5;
unsigned short buf[25];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++)
	buf[y * GW + x] = (unsigned short)(120 + 60*((x + y) & 1) + 9*x*x + 5*y);

struct dsp_case tc = {
    "asymmetric 5x5 adaptive cut",
    GW, GH, buf, DSP_CUT_DIR_ADAPT, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 7. Smooth normal interpolation mode 1 --- */
    {
const uint32_t GW = 5, GH = 5;
unsigned short buf[25];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++)
	buf[y * GW + x] = (unsigned short)(200 + 30*x + 10*y);

struct dsp_case tc = {
    "smooth=1 ramp normal check",
    GW, GH, buf, DSP_CUT_DIR_llUR, 1, 2, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 8. Smooth normal interpolation mode 2 --- */
    {
const uint32_t GW = 5, GH = 5;
unsigned short buf[25];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++)
	buf[y * GW + x] = (unsigned short)(150 + 20*x + 25*y);

struct dsp_case tc = {
    "smooth=2 ramp normal check",
    GW, GH, buf, DSP_CUT_DIR_llUR, 2, 2, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 9. 16x16 sinusoidal terrain --- */
    {
const uint32_t GW = 16, GH = 16;
unsigned short buf[256];
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++) {
double fx = (double)x / (GW - 1) * M_PI;
double fy = (double)y / (GH - 1) * M_PI;
buf[y*GW + x] = (unsigned short)(int)(
    300.0 + 150.0 * sin(fx) * sin(fy));
    }

struct dsp_case tc = {
    "sinusoidal 16x16",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
    }

    /* --- 10. Complex 33x33 wave terrain --- */
    {
const uint32_t GW = 33, GH = 33;
unsigned short *buf = (unsigned short *)bu_calloc(
    GW * GH, sizeof(unsigned short), "33x33 buf");
for (uint32_t y = 0; y < GH; y++)
    for (uint32_t x = 0; x < GW; x++) {
double fx = (double)x / (GW - 1) * 6.2832;
double fy = (double)y / (GH - 1) * 6.2832;
buf[y*GW + x] = (unsigned short)(int)(
    500.0 + 200.0*sin(fx) + 150.0*cos(2.0*fy));
    }

struct dsp_case tc = {
    "complex 33x33 wave",
    GW, GH, buf, DSP_CUT_DIR_llUR, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
bu_free(buf, "33x33 buf");
    }

    /* --- 11. Larger terrain to keep the default DDA path under test --- */
    {
const uint32_t grid_width = DSP_TEST_LARGE_GRID_DIM;
const uint32_t grid_height = DSP_TEST_LARGE_GRID_DIM;
const double ridge_height = 120.0;
/* Different X/Y wave frequencies (12 vs. 8 cycles across the unit grid) avoid
 * a symmetric height field and produce rays that visit varied HBB/DDA child
 * paths across the power-of-two cell grid.
 */
const double ridge_x_freq = 12.0;
const double ridge_y_freq = 8.0;
const double ramp_x_height = 300.0;
const double ramp_y_height = 150.0;
const double base_height = 700.0;
unsigned short *buf = (unsigned short *)bu_calloc(
    grid_width * grid_height, sizeof(unsigned short), "129x129 buf");
double *sin_x = (double *)bu_malloc(grid_width * sizeof(double), "129x129 sin x");
double *cos_y = (double *)bu_malloc(grid_height * sizeof(double), "129x129 cos y");
for (uint32_t x = 0; x < grid_width; x++) {
    double fx = (double)x / (grid_width - 1);
    sin_x[x] = sin(ridge_x_freq * fx);
}
for (uint32_t y = 0; y < grid_height; y++) {
    double fy = (double)y / (grid_height - 1);
    cos_y[y] = cos(ridge_y_freq * fy);
}
for (uint32_t y = 0; y < grid_height; y++)
    for (uint32_t x = 0; x < grid_width; x++) {
double fx = (double)x / (grid_width - 1);
double fy = (double)y / (grid_height - 1);
double ridge = ridge_height * sin_x[x] * cos_y[y];
double ramp = ramp_x_height * fx + ramp_y_height * fy;
buf[y*grid_width + x] = (unsigned short)(base_height + ramp + ridge);
    }

struct dsp_case tc = {
    "larger 129x129 mixed ramp/wave terrain",
    grid_width, grid_height, buf, DSP_CUT_DIR_llUR, 0, 0, 1.0, 1.0, 1.0, -1.0, -1.0
};
failures += run_inmem_case(&tc);
bu_free(sin_x, "129x129 sin x");
bu_free(cos_y, "129x129 cos y");
bu_free(buf, "129x129 buf");
    }

    printf("\n=== Synthetic: %d failure(s) ===\n", failures);
    return failures;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    int failures = 0;

    failures += test_synthetic();

    if (argc == 3) {
printf("\n=== Real terrain: %s  object: %s ===\n",
       argv[1], argv[2]);
failures += run_file_case(argv[1], argv[2]);
    } else if (argc != 1) {
bu_exit(1, "Usage: %s [<file.g> <object-name>]\n", argv[0]);
    }

    printf("\n=== Overall: %d failure(s) ===\n", failures);
    return (failures > 0) ? 1 : 0;
}
