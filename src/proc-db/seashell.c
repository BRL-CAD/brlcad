/*                      S E A S H E L L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/seashell.c
 *
 * Procedurally grow coiled seashells as watertight BoT solids.
 *
 * Each shell is a single logarithmic (equiangular) helico-spiral tube:
 * a generating curve -- the aperture cross-section -- is swept along a
 * spiral whose radius grows exponentially with the coil angle, while the
 * cross-section itself grows by the same factor.  This self-similar growth
 * (the same idea a real mollusk uses) makes successive whorls nest without
 * intersecting, producing the classic gnomonic seashell.
 *
 * The cross-section is a Gielis superformula curve, so a single set of
 * parameters spans a whole zoo of forms: a smooth round tube gives a
 * nautilus/ammonite, an m-fold ribbed profile gives a fluted conch, and a
 * tall slow-growing conispiral gives a pointed turret/auger.  Frames are
 * carried in the local radial/axial plane, vertex normals are computed
 * analytically for smooth shading, and both ends are capped so every shell
 * is a closed, ray-traceable solid.
 *
 * With no mode argument the program lays out a small shelf of four
 * different species on a ground plane; "--mode <species>" writes a single
 * hero shell instead.  All coordinates are millimeters.
 *
 *   seashell out.g                      # a shelf of shells
 *   seashell out.g --mode nautilus      # one pearly nautilus
 *   seashell out.g --mode conch         # one ribbed conch
 *   seashell out.g --mode turret        # one tall auger spire
 *   seashell out.g --mode snail         # one round garden snail
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


/* Everything needed to grow one shell.  Sizes are millimeters; angles are
 * in the noted units.  Sensible per-species presets are filled in below.
 */
struct shell_params {
    const char *name;		/* solid/region basename                */

    double turns;		/* number of whorls (coil revolutions)  */
    double expand;		/* growth factor E per full revolution  */
    double r0;			/* starting centerline radius from axis */
    double fill;		/* aperture size as a fraction [0,1) of  */
				/* the whorl-to-whorl gap; < 1 keeps      */
				/* successive whorls from intersecting    */
    double aspect;		/* aperture axial:radial size ratio      */
    double zfac;		/* axial rise per unit radius (spire)   */

    /* superformula cross-section (m == 0 -> a plain smooth ellipse) */
    double sf_m;		/* rotational symmetry / rib count      */
    double sf_n1, sf_n2, sf_n3;	/* profile exponents                    */
    double rib;			/* extra sinusoidal rib depth [0,~0.3]  */
    double rib_freq;		/* number of ribs around the aperture   */

    int seg_per_turn;		/* rings sampled per revolution         */
    int nsec;			/* samples around the cross-section     */

    unsigned char rgb[3];	/* shell color                          */
    const char *shader;		/* optic shader arguments               */
};


/* Gielis superformula radius at angle phi.  Returns a positive radius that
 * traces a closed m-fold curve as phi runs 0..2pi.
 */
static double
superformula(double m, double n1, double n2, double n3, double phi)
{
    double t1 = fabs(cos(m * phi / 4.0));
    double t2 = fabs(sin(m * phi / 4.0));
    double s;

    t1 = pow(t1, n2);
    t2 = pow(t2, n3);
    s = t1 + t2;
    if (s < 1.0e-9)
	return 0.0;
    return pow(s, -1.0 / n1);
}


/* Cross-section shape radius (normalized so its maximum over one loop is
 * 1.0), evaluated at parameter angle sang in [0, 2pi).  The optional rib
 * term adds shallow flutes on top of the superformula profile.
 */
static double
section_radius(const struct shell_params *p, double sang, double sfnorm)
{
    double r;

    if (p->sf_m <= 0.0)
	r = 1.0;			/* plain ellipse */
    else
	r = superformula(p->sf_m, p->sf_n1, p->sf_n2, p->sf_n3, sang) / sfnorm;

    if (p->rib > 0.0) {
	double f = (p->rib_freq > 0.0) ? p->rib_freq : p->sf_m;
	r *= 1.0 + p->rib * cos(f * sang);
    }

    return r;
}


/* Rounding envelope along the coil: 1.0 over the middle, easing to ~0 at
 * both ends over the fraction w with a quarter-circle profile, so the shell
 * closes into smooth rounded tips instead of flat cut disks.  t is the
 * normalized coil parameter in [0,1].
 */
static double
end_envelope(double t, double w)
{
    double u, r;
    if (w <= 0.0)
	return 1.0;
    if (t < w)
	u = t / w;			/* ramp up over the first tip  */
    else if (t > 1.0 - w)
	u = (1.0 - t) / w;		/* ramp down into the aperture */
    else
	return 1.0;
    if (u < 0.0) u = 0.0;
    r = sqrt(u * (2.0 - u));		/* quarter circle: 0->1 rounded */
    return (r < 0.06) ? 0.06 : r;	/* keep a tiny cap, avoid a cusp */
}


/* Place one point of the sweep in the shell's local frame.
 *
 * theta is the coil angle (0 .. 2pi*turns); sang is the angle around the
 * aperture.  g = exp(k*theta) is the common self-similar growth factor, so
 * the centerline radius, the axial rise, and the aperture size all scale
 * together.  center (may be NULL) receives the aperture-center point used
 * as the "outward" reference for normal orientation.
 */
static void
sweep_point(const struct shell_params *p, double k, double sfnorm,
	    double arad0, double zrad0, double env,
	    double theta, double sang, point_t out, point_t center)
{
    double g = exp(k * theta);
    double R = p->r0 * g;			/* centerline radius     */
    double cz = p->zfac * R;			/* axial rise (spire)    */
    double ct = cos(theta), st = sin(theta);
    double rs = section_radius(p, sang, sfnorm) * env;
    double ex = arad0 * g * rs * cos(sang);	/* radial offset in tube */
    double ez = zrad0 * g * rs * sin(sang);	/* axial offset in tube  */

    /* aperture center on the helico-spiral centerline */
    if (center) {
	center[X] = R * ct;
	center[Y] = R * st;
	center[Z] = cz;
    }

    /* cross-section embedded in the local (radial, axial) plane */
    out[X] = (R + ex) * ct;
    out[Y] = (R + ex) * st;
    out[Z] = cz + ez;
}


/* Grow one shell into freshly malloc'd vertex/normal/face arrays (local
 * frame, not yet placed).  Returns the axis-aligned bounds so the caller
 * can lay the shell out and stand it on the ground.
 */
static void
build_shell_geom(const struct shell_params *p,
		 fastf_t **overts, fastf_t **onorms, int **ofaces, int **ofnorms,
		 size_t *onv, size_t *onf,
		 point_t bmin, point_t bmax)
{
    int nt = (int)(p->turns * p->seg_per_turn + 0.5);	/* segment count */
    int ns = p->nsec;
    int nrings;
    size_t nsurf, nverts, nfaces, sidef;
    fastf_t *verts, *norms;
    int *faces, *fnorms;
    point_t *cen;

    /* clamp to a mesh that has at least two rings and a real loop so the
     * cap-hub math and neighbor differences below always stay in bounds
     * (guards against tiny --turns / --res overrides).
     */
    if (nt < 2) nt = 2;
    if (ns < 3) ns = 3;

    nrings = nt + 1;
    nsurf = (size_t)nrings * (size_t)ns;
    nverts = nsurf + 2;					/* + two cap hubs */
    sidef = (size_t)nt * (size_t)ns * 2;		/* body quad faces  */
    nfaces = sidef + (size_t)ns * 2;			/* + two cap fans   */
    verts = (fastf_t *)bu_malloc(nverts * 3 * sizeof(fastf_t), "seashell verts");
    norms = (fastf_t *)bu_malloc(nverts * 3 * sizeof(fastf_t), "seashell norms");
    faces = (int *)bu_malloc(nfaces * 3 * sizeof(int), "seashell faces");
    fnorms = (int *)bu_malloc(nfaces * 3 * sizeof(int), "seashell fnorms");
    cen = (point_t *)bu_malloc(nrings * sizeof(point_t), "seashell centers");
    double k = log(p->expand) / M_2PI;
    double thmax = M_2PI * p->turns;
    double sfnorm = 1.0;
    size_t tip = nsurf, ap = nsurf + 1;			/* cap hub indices */
    size_t f = 0;
    int i, j, jn, jp;
    int ip, in;

    /* Size the aperture from the whorl-to-whorl spacing so successive whorls
     * never intersect.  Adjacent whorls (one turn apart) lie a distance
     * proportional to R0*(E-1) apart along the (radial, zfac*radial)
     * direction; requiring the two elliptical cross-sections to fit within
     * that gap gives  sqrt(arad0^2 + (zrad0*zfac)^2) <=
     * R0*(E-1)*(1+zfac^2)/(E+1).  We take a fraction (fill<1) of that bound
     * and split it by the axial:radial aspect ratio.
     */
    double E = p->expand;
    double zf = p->zfac;
    double bound = (E > 1.0) ? p->r0 * (E - 1.0) * (1.0 + zf*zf) / (E + 1.0)
			     : p->r0 * 0.2;
    double base = p->fill * bound;
    double arad0 = base / sqrt(1.0 + (p->aspect * zf) * (p->aspect * zf));
    double zrad0 = p->aspect * arad0;

    /* normalize the superformula so its widest point is exactly 1.0 */
    if (p->sf_m > 0.0) {
	double mx = 0.0;
	int samp = ns * 4;
	for (i = 0; i < samp; i++) {
	    double r = superformula(p->sf_m, p->sf_n1, p->sf_n2, p->sf_n3,
				    M_2PI * i / samp);
	    if (r > mx) mx = r;
	}
	if (mx > 1.0e-9) sfnorm = mx;
    }

    /* --- lay down the surface vertices ring by ring --- */
    for (i = 0; i < nrings; i++) {
	double theta = thmax * i / nt;
	double env = end_envelope((double)i / nt, 0.14);
	for (j = 0; j < ns; j++) {
	    double sang = M_2PI * j / ns;
	    point_t pt;
	    /* record the ring center (aperture centerline) once, at j==0 */
	    sweep_point(p, k, sfnorm, arad0, zrad0, env, theta, sang, pt,
			(j == 0) ? cen[i] : NULL);
	    VMOVE(&verts[(size_t)(i * ns + j) * 3], pt);
	}
    }

    /* --- analytic-ish smooth normals via grid tangents --- */
    for (i = 0; i < nrings; i++) {
	ip = (i > 0) ? i - 1 : i;
	in = (i < nrings - 1) ? i + 1 : i;
	for (j = 0; j < ns; j++) {
	    fastf_t *P = &verts[(size_t)(i * ns + j) * 3];
	    vect_t dtheta, dsec, n, outward;
	    jn = (j + 1) % ns;
	    jp = (j + ns - 1) % ns;
	    VSUB2(dtheta, &verts[(size_t)(in * ns + j) * 3],
		  &verts[(size_t)(ip * ns + j) * 3]);
	    VSUB2(dsec, &verts[(size_t)(i * ns + jn) * 3],
		  &verts[(size_t)(i * ns + jp) * 3]);
	    VCROSS(n, dtheta, dsec);
	    VSUB2(outward, P, cen[i]);
	    if (VDOT(n, outward) < 0.0)
		VREVERSE(n, n);
	    VUNITIZE(n);
	    VMOVE(&norms[(size_t)(i * ns + j) * 3], n);
	}
    }

    /* --- decide global side winding from one representative quad --- */
    {
	int mi = nt / 2;
	fastf_t *a = &verts[(size_t)(mi * ns + 0) * 3];
	fastf_t *b = &verts[(size_t)((mi + 1) * ns + 0) * 3];
	fastf_t *c = &verts[(size_t)((mi + 1) * ns + 1) * 3];
	vect_t ab, ac, fn;
	VSUB2(ab, b, a);
	VSUB2(ac, c, a);
	VCROSS(fn, ab, ac);
	if (VDOT(fn, &norms[(size_t)(mi * ns + 0) * 3]) >= 0.0) {
	    /* (a,b,c) already faces outward */
	    for (i = 0; i < nt; i++) {
		for (j = 0; j < ns; j++) {
		    int v00 = i * ns + j, v01 = i * ns + (j + 1) % ns;
		    int v10 = (i + 1) * ns + j, v11 = (i + 1) * ns + (j + 1) % ns;
		    faces[f*3+0]=v00; faces[f*3+1]=v10; faces[f*3+2]=v11; f++;
		    faces[f*3+0]=v00; faces[f*3+1]=v11; faces[f*3+2]=v01; f++;
		}
	    }
	} else {
	    for (i = 0; i < nt; i++) {
		for (j = 0; j < ns; j++) {
		    int v00 = i * ns + j, v01 = i * ns + (j + 1) % ns;
		    int v10 = (i + 1) * ns + j, v11 = (i + 1) * ns + (j + 1) % ns;
		    faces[f*3+0]=v00; faces[f*3+1]=v11; faces[f*3+2]=v10; f++;
		    faces[f*3+0]=v00; faces[f*3+1]=v01; faces[f*3+2]=v11; f++;
		}
	    }
	}
    }

    /* --- cap hubs: centers of the first and last rings --- */
    VMOVE(&verts[tip * 3], cen[0]);
    VMOVE(&verts[ap * 3], cen[nt]);
    {
	vect_t tn, an;
	VSUB2(tn, cen[0], cen[1]); VUNITIZE(tn);	/* points off the tip */
	VSUB2(an, cen[nt], cen[nt - 1]); VUNITIZE(an);	/* out the aperture  */
	VMOVE(&norms[tip * 3], tn);
	VMOVE(&norms[ap * 3], an);
    }

    /* tip fan (ring 0), wound to face outward per the hub normal */
    {
	fastf_t *tn = &norms[tip * 3];
	fastf_t *a = &verts[(size_t)(0 * ns + 0) * 3];
	fastf_t *b = &verts[(size_t)(0 * ns + 1) * 3];
	vect_t ta, tb, fn;
	int flip;
	VSUB2(ta, a, &verts[tip * 3]);
	VSUB2(tb, b, &verts[tip * 3]);
	VCROSS(fn, ta, tb);
	flip = (VDOT(fn, tn) < 0.0);
	for (j = 0; j < ns; j++) {
	    int j2 = (j + 1) % ns;
	    faces[f*3+0] = (int)tip;
	    if (!flip) { faces[f*3+1] = 0*ns+j; faces[f*3+2] = 0*ns+j2; }
	    else       { faces[f*3+1] = 0*ns+j2; faces[f*3+2] = 0*ns+j; }
	    f++;
	}
    }

    /* aperture fan (last ring) */
    {
	fastf_t *an = &norms[ap * 3];
	fastf_t *a = &verts[(size_t)(nt * ns + 0) * 3];
	fastf_t *b = &verts[(size_t)(nt * ns + 1) * 3];
	vect_t ta, tb, fn;
	int flip;
	VSUB2(ta, a, &verts[ap * 3]);
	VSUB2(tb, b, &verts[ap * 3]);
	VCROSS(fn, ta, tb);
	flip = (VDOT(fn, an) < 0.0);
	for (j = 0; j < ns; j++) {
	    int j2 = (j + 1) % ns;
	    faces[f*3+0] = (int)ap;
	    if (!flip) { faces[f*3+1] = nt*ns+j; faces[f*3+2] = nt*ns+j2; }
	    else       { faces[f*3+1] = nt*ns+j2; faces[f*3+2] = nt*ns+j; }
	    f++;
	}
    }

    /* --- face-vertex normals ---
     * Body faces smooth-shade from their own vertices' surface normals.
     * The two flat cap fans instead use the single axial hub normal for
     * all three corners (the ring vertices' normals point radially along
     * the cap plane, which would shade the caps at a grazing angle).
     */
    {
	size_t z;
	for (z = 0; z < sidef * 3; z++)
	    fnorms[z] = faces[z];			/* body: per-vertex */
	for (z = sidef * 3; z < (sidef + (size_t)ns) * 3; z++)
	    fnorms[z] = (int)tip;			/* tip cap: axial   */
	for (z = (sidef + (size_t)ns) * 3; z < nfaces * 3; z++)
	    fnorms[z] = (int)ap;			/* aperture: axial  */
    }

    /* --- bounds over all surface + hub vertices --- */
    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    for (i = 0; i < (int)nverts; i++) {
	fastf_t *v = &verts[(size_t)i * 3];
	VMINMAX(bmin, bmax, v);
    }

    bu_free(cen, "seashell centers");

    *overts = verts;
    *onorms = norms;
    *ofaces = faces;
    *ofnorms = fnorms;
    *onv = nverts;
    *onf = f;
}


/* Grow one shell, translate it by off, and write it as a region.  Adds the
 * region to the given assembly.  Returns 0 on success.
 */
static int
emit_shell(struct rt_wdb *db_fp, const struct shell_params *p,
	   const vect_t off, struct wmember *assembly)
{
    fastf_t *verts = NULL, *norms = NULL;
    int *faces = NULL, *fnorms = NULL;
    size_t nv = 0, nf = 0, i;
    point_t bmin, bmax;
    char sname[64], rname[64];
    int ret;

    build_shell_geom(p, &verts, &norms, &faces, &fnorms, &nv, &nf, bmin, bmax);

    /* place the shell */
    for (i = 0; i < nv; i++) {
	verts[i*3+0] += off[X];
	verts[i*3+1] += off[Y];
	verts[i*3+2] += off[Z];
    }

    snprintf(sname, sizeof(sname), "%s.bot", p->name);
    snprintf(rname, sizeof(rname), "%s.r", p->name);

    ret = mk_bot_w_normals(db_fp, sname, RT_BOT_SOLID, RT_BOT_CCW,
			   RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
			   nv, nf, verts, faces,
			   (fastf_t *)NULL, (struct bu_bitv *)NULL,
			   nv, norms, fnorms);
    if (ret) {
	bu_log("seashell: failed to write BoT \"%s\"\n", sname);
    } else {
	mk_region1(db_fp, rname, sname, "plastic",
		   p->shader ? p->shader : "sh=18 sp=0.4 di=0.6",
		   (unsigned char *)p->rgb);
	(void)mk_addmember(rname, &assembly->l, NULL, WMOP_UNION);
    }

    bu_free(verts, "seashell verts");
    bu_free(norms, "seashell norms");
    bu_free(faces, "seashell faces");
    bu_free(fnorms, "seashell fnorms");
    return ret;
}


/* The four species used for the shelf and for "--mode <name>".
 * Fields: turns, expand, r0, fill, aspect, zfac | sf_m, n1, n2, n3, rib,
 * rib_freq | seg_per_turn, nsec | rgb | shader.  fill < 1 leaves a gap
 * between whorls so the solid never self-intersects.
 */
static const struct shell_params species[] = {
    {
	"nautilus",
	3.9, 1.85, 6.0, 0.95, 1.35, 0.0,	/* planispiral, wide throat */
	0.0, 1.0, 1.0, 1.0, 0.0, 0.0,		/* smooth round tube        */
	160, 96,
	{234, 227, 210}, "sh=30 sp=0.55 di=0.45"
    },
    {
	"conch",
	3.1, 2.05, 7.5, 0.93, 1.15, 0.5,	/* big flaring conispiral   */
	5.0, 18.0, 18.0, 18.0, 0.06, 5.0,	/* rounded-pentagon aperture */
	160, 110,
	{224, 120, 88}, "sh=22 sp=0.45 di=0.55"
    },
    {
	"turret",
	7.0, 1.46, 6.0, 0.90, 1.05, 1.85,	/* tall auger spire         */
	0.0, 1.0, 1.0, 1.0, 0.10, 13.0,		/* thirteen fine flutes     */
	150, 96,
	{168, 122, 78}, "sh=16 sp=0.35 di=0.65"
    },
    {
	"snail",
	3.8, 2.15, 5.6, 0.92, 1.00, 0.95,	/* round garden snail       */
	0.0, 1.0, 1.0, 1.0, 0.0, 0.0,
	150, 88,
	{178, 154, 110}, "sh=18 sp=0.35 di=0.65"
    }
};
static const int nspecies = (int)(sizeof(species) / sizeof(species[0]));


static void
usage(const char *pn)
{
    bu_exit(1,
	"Usage: %s output.g [--mode collection|nautilus|conch|turret|snail]\n"
	"                 [--turns n] [--expand f] [--res n]\n"
	"  With no --mode (or --mode collection) a shelf of shells is built.\n",
	pn);
}


int
main(int argc, char **argv)
{
    struct rt_wdb *db_fp;
    struct wmember all_hd;
    const char *fname = "seashell.g";
    const char *mode = "collection";
    double turns_ov = 0.0, expand_ov = 0.0;
    int res_ov = 0;
    int i;

    bu_setprogname(argv[0]);

    if (argc < 2)
	usage(argv[0]);
    fname = argv[1];

    for (i = 2; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--mode") && i + 1 < argc)
	    mode = argv[++i];
	else if (BU_STR_EQUAL(argv[i], "--turns") && i + 1 < argc)
	    turns_ov = atof(argv[++i]);
	else if (BU_STR_EQUAL(argv[i], "--expand") && i + 1 < argc)
	    expand_ov = atof(argv[++i]);
	else if (BU_STR_EQUAL(argv[i], "--res") && i + 1 < argc)
	    res_ov = atoi(argv[++i]);
	else
	    bu_log("seashell: ignoring unknown option \"%s\"\n", argv[i]);
    }

    if ((db_fp = wdb_fopen(fname)) == NULL) {
	perror(fname);
	return 2;
    }
    mk_id_units(db_fp, "Procedural Seashells", "mm");
    BU_LIST_INIT(&all_hd.l);

    if (BU_STR_EQUAL(mode, "collection")) {
	/* Lay the whole species shelf out left-to-right, each shell
	 * standing on the ground (z = 0), centered on Y.
	 */
	double cursor = 0.0;
	const double gap = 18.0;
	double gxmin = INFINITY, gxmax = -INFINITY;
	double gymin = INFINITY, gymax = -INFINITY;

	for (i = 0; i < nspecies; i++) {
	    struct shell_params sp = species[i];
	    fastf_t *v = NULL, *n = NULL;
	    int *fc = NULL, *fn = NULL;
	    size_t nv = 0, nf = 0, kv;
	    point_t bmin, bmax;
	    vect_t off;
	    char sn[64], rn[64];

	    if (res_ov > 0) sp.seg_per_turn = res_ov;

	    build_shell_geom(&sp, &v, &n, &fc, &fn, &nv, &nf, bmin, bmax);

	    /* stand this shell on the ground and slide it along the shelf */
	    VSET(off,
		 cursor - bmin[X],
		 -(bmin[Y] + bmax[Y]) / 2.0,
		 -bmin[Z]);
	    for (kv = 0; kv < nv; kv++) {
		v[kv*3+0] += off[X];
		v[kv*3+1] += off[Y];
		v[kv*3+2] += off[Z];
	    }

	    snprintf(sn, sizeof(sn), "%s.bot", sp.name);
	    snprintf(rn, sizeof(rn), "%s.r", sp.name);
	    mk_bot_w_normals(db_fp, sn, RT_BOT_SOLID, RT_BOT_CCW,
			     RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
			     nv, nf, v, fc, NULL, NULL, nv, n, fn);
	    mk_region1(db_fp, rn, sn, "plastic", sp.shader,
		       (unsigned char *)sp.rgb);
	    (void)mk_addmember(rn, &all_hd.l, NULL, WMOP_UNION);

	    /* running bounds for the ground slab */
	    if (off[X] + bmin[X] < gxmin) gxmin = off[X] + bmin[X];
	    if (off[X] + bmax[X] > gxmax) gxmax = off[X] + bmax[X];
	    if (off[Y] + bmin[Y] < gymin) gymin = off[Y] + bmin[Y];
	    if (off[Y] + bmax[Y] > gymax) gymax = off[Y] + bmax[Y];

	    cursor += (bmax[X] - bmin[X]) + gap;

	    bu_free(v, "v"); bu_free(n, "n"); bu_free(fc, "fc"); bu_free(fn, "fn");
	}

	/* a thin ground slab the shells sit on */
	{
	    point_t pmin, pmax;
	    double m = 30.0;
	    VSET(pmin, gxmin - m, gymin - m, -12.0);
	    VSET(pmax, gxmax + m, gymax + m, 0.0);
	    mk_rpp(db_fp, "ground.s", pmin, pmax);
	    {
		unsigned char g[3] = {150, 150, 155};
		mk_region1(db_fp, "ground.r", "ground.s", "plastic",
			   "sh=4 sp=0.1 di=0.9", g);
		(void)mk_addmember("ground.r", &all_hd.l, NULL, WMOP_UNION);
	    }
	}

	bu_log("seashell: wrote a shelf of %d shells\n", nspecies);
    } else {
	/* single hero shell of the requested species */
	struct shell_params sp;
	vect_t off;
	int found = -1;
	for (i = 0; i < nspecies; i++)
	    if (BU_STR_EQUAL(mode, species[i].name)) { found = i; break; }
	if (found < 0) {
	    bu_log("seashell: unknown mode \"%s\"\n", mode);
	    db_close(db_fp->dbip);
	    usage(argv[0]);
	}
	sp = species[found];
	if (turns_ov > 0.0) sp.turns = turns_ov;
	if (expand_ov > 0.0) sp.expand = expand_ov;
	if (res_ov > 0) sp.seg_per_turn = res_ov;

	VSETALL(off, 0.0);
	if (emit_shell(db_fp, &sp, off, &all_hd) != 0) {
	    db_close(db_fp->dbip);
	    return 3;
	}
	bu_log("seashell: wrote a single %s\n", sp.name);
    }

    if (mk_lcomb(db_fp, "all", &all_hd, 0, NULL, NULL, NULL, 0) != 0) {
	bu_log("seashell: failed to write group \"all\"\n");
	db_close(db_fp->dbip);
	return 4;
    }

    db_close(db_fp->dbip);
    bu_log("seashell: done, wrote %s\n", fname);
    return 0;
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
