/*                      V O X E L I Z E . C
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
/** @file proc-db/voxelize.c
 *
 * Demonstrate the BRL-CAD "vol" (3-D volume) and "ebm" (extruded
 * bitmap) primitives by sampling a procedural 3-D scalar field over a
 * regular grid.
 *
 * Each cell of an xdim*ydim*zdim grid is evaluated against a selectable
 * implicit field (a triply-periodic gyroid by default, with metaball
 * and pseudo-Mandelbulb alternatives), quantized to an unsigned byte,
 * and written to a sidecar ".vol" data file in x-fastest order (X
 * varies fastest, then Y, then Z).  An mk_vol() primitive then renders
 * every cell whose byte value falls inside the [lo,hi] iso-window as a
 * solid voxel.
 *
 * The same field is also collapsed (max-projected and thresholded)
 * along Z into a 2-D bitmap that is written to a sidecar ".ebm" file
 * and extruded with mk_ebm() to give a companion "height field" style
 * solid placed off to the side for contrast.
 *
 * Both sidecar files are written next to the output .g database so the
 * file-backed primitives can find them.  Note that since the values in
 * the database are stored in millimeters, all coordinates and the vol
 * cellsize are expressed in millimeters.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


/* Selectable procedural fields. */
#define FIELD_GYROID	0
#define FIELD_METABALL	1
#define FIELD_MANDEL	2


/*
 * Evaluate the chosen scalar field at normalized coordinates u,v,w
 * which each range over [0,1] across the grid.  The return value is
 * mapped to [0,1] by the caller's quantization, but here we just
 * return a raw fastf_t that the caller normalizes.
 */
static fastf_t
sample_field(int field, fastf_t u, fastf_t v, fastf_t w)
{
    fastf_t x, y, z;
    fastf_t val;

    val = 0.0;

    switch (field) {
	case FIELD_GYROID:
	    /* A triply-periodic minimal surface.  Scale the unit cube
	     * up by a few periods so the structure repeats and looks
	     * interesting.  The gyroid implicit is:
	     *   sin(x)cos(y) + sin(y)cos(z) + sin(z)cos(x)
	     * which naturally ranges over roughly [-3,3].
	     */
	    x = u * M_PI * 4.0;
	    y = v * M_PI * 4.0;
	    z = w * M_PI * 4.0;
	    val = sin(x) * cos(y) + sin(y) * cos(z) + sin(z) * cos(x);
	    /* normalize from [-3,3] to [0,1] */
	    val = (val + 3.0) / 6.0;
	    break;

	case FIELD_METABALL: {
	    /* Sum of inverse-square potentials from a few fixed charge
	     * centers placed within the unit cube.  Classic "blobby"
	     * isosurface field.
	     */
	    static const fastf_t centers[4][3] = {
		{ 0.30, 0.30, 0.35 },
		{ 0.70, 0.40, 0.60 },
		{ 0.45, 0.70, 0.40 },
		{ 0.60, 0.65, 0.70 }
	    };
	    int i;
	    fastf_t dx, dy, dz, d2;

	    val = 0.0;
	    for (i = 0; i < 4; i++) {
		dx = u - centers[i][0];
		dy = v - centers[i][1];
		dz = w - centers[i][2];
		d2 = dx * dx + dy * dy + dz * dz + 1.0e-4;
		val += 0.02 / d2;
	    }
	    /* clamp the potential into [0,1] */
	    if (val > 1.0)
		val = 1.0;
	    break;
	}

	case FIELD_MANDEL: {
	    /* A simple distance-estimator flavored Mandelbulb-ish
	     * escape-time field.  Map the unit cube to roughly the
	     * [-1.2,1.2] region of interest and iterate the power-8
	     * triplex formula, returning a smooth escape fraction.
	     */
	    fastf_t cx, cy, cz;
	    fastf_t zx, zy, zz;
	    fastf_t r, theta, phi, r8;
	    int iter;
	    const int maxiter = 8;
	    const fastf_t power = 8.0;

	    cx = (u - 0.5) * 2.4;
	    cy = (v - 0.5) * 2.4;
	    cz = (w - 0.5) * 2.4;
	    zx = cx;
	    zy = cy;
	    zz = cz;
	    iter = 0;
	    r = 0.0;
	    while (iter < maxiter) {
		r = sqrt(zx * zx + zy * zy + zz * zz);
		if (r > 2.0)
		    break;
		theta = (r > 1.0e-9) ? acos(zz / r) : 0.0;
		phi = atan2(zy, zx);
		r8 = pow(r, power);
		zx = r8 * sin(theta * power) * cos(phi * power) + cx;
		zy = r8 * sin(theta * power) * sin(phi * power) + cy;
		zz = r8 * cos(theta * power) + cz;
		iter++;
	    }
	    /* smooth-ish escape fraction in [0,1]: interior (never
	     * escaped) reads high, exterior reads low.
	     */
	    val = (fastf_t)iter / (fastf_t)maxiter;
	    break;
	}

	default:
	    val = 0.0;
	    break;
    }

    if (val < 0.0)
	val = 0.0;
    if (val > 1.0)
	val = 1.0;

    return val;
}


/*
 * Parse the textual field name into one of the FIELD_* enumerants.
 * Returns -1 on an unrecognized name.
 */
static int
parse_field(const char *name)
{
    if (BU_STR_EQUAL(name, "gyroid"))
	return FIELD_GYROID;
    if (BU_STR_EQUAL(name, "metaball"))
	return FIELD_METABALL;
    if (BU_STR_EQUAL(name, "mandel"))
	return FIELD_MANDEL;
    return -1;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;

    /* tunable parameters with sensible defaults */
    int field = FIELD_GYROID;
    int res = 64;		/* grid is res x res x res */
    int lo = 110;		/* iso-window low byte (inclusive) */
    int hi = 145;		/* iso-window high byte (inclusive) */

    size_t xdim, ydim, zdim;
    size_t x, y, z;
    size_t nvox;

    unsigned char *voldata = NULL;	/* xdim*ydim*zdim bytes */
    unsigned char *ebmdata = NULL;	/* xdim*ydim bytes */

    char volfile[MAXPATHLEN];
    char ebmfile[MAXPATHLEN];
    char *volbase;	/* basename stored in the vol primitive */
    char *ebmbase;	/* basename stored in the ebm primitive */
    char *sep;
    const char *gpath;
    size_t glen;

    FILE *fp;
    size_t written;

    vect_t cellsize;		/* mm per cell for the vol primitive */
    fastf_t tallness;		/* extrusion height for the ebm, in mm */
    mat_t mat;			/* identity placement matrix */

    point_t cutlo, cuthi;	/* optional cutaway box corners */
    point_t base;
    unsigned char rgb[3];
    struct wmember region_hd;	/* members of the vol region */
    struct wmember all_hd;	/* the top-level "all" group */

    int i;
    fastf_t span;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--field gyroid|metaball|mandel] [--res N] [--lo B] [--hi B]\n", av[0]);
    }

    /* Very small, dependency-free argument parser.  Every option is
     * optional; running with just the output path yields a complete
     * default scene.
     */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--field") && i + 1 < ac) {
	    field = parse_field(av[++i]);
	    if (field < 0)
		bu_exit(1, "Unknown --field '%s' (use gyroid, metaball, or mandel)\n", av[i]);
	} else if (BU_STR_EQUAL(av[i], "--res") && i + 1 < ac) {
	    res = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--lo") && i + 1 < ac) {
	    lo = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--hi") && i + 1 < ac) {
	    hi = atoi(av[++i]);
	} else {
	    bu_exit(1, "Unrecognized argument '%s'\n", av[i]);
	}
    }

    /* sanity-clamp the parameters so bad input cannot crash us */
    if (res < 4)
	res = 4;
    if (res > 256)
	res = 256;
    if (lo < 0)
	lo = 0;
    if (hi > 255)
	hi = 255;
    if (hi < lo)
	hi = lo;

    xdim = (size_t)res;
    ydim = (size_t)res;
    zdim = (size_t)res;
    nvox = xdim * ydim * zdim;

    /* Open/Create the database file for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, "Procedural Voxel Volume", "mm");

    /* Derive the two sidecar data-file paths from the output .g path
     * by stripping a trailing ".g" (if present) and appending the new
     * extensions.  The file-backed vol and ebm primitives store only
     * this name, so the files must live where the renderer is run; we
     * keep them right next to the database.
     */
    gpath = av[1];
    glen = strlen(gpath);
    bu_strlcpy(volfile, gpath, sizeof(volfile));
    bu_strlcpy(ebmfile, gpath, sizeof(ebmfile));
    if (glen >= 2 && BU_STR_EQUAL(gpath + glen - 2, ".g")) {
	volfile[glen - 2] = '\0';
	ebmfile[glen - 2] = '\0';
    }
    bu_strlcat(volfile, ".vol", sizeof(volfile));
    bu_strlcat(ebmfile, ".ebm", sizeof(ebmfile));

    /* The vol/ebm file references are resolved at render time against a
     * search path (cwd and the .g's directory).  Store only the bare
     * basename: an absolute path gets a "./" prefix and fails to open
     * (ebm in particular).  The sidecars sit next to the .g, so the
     * basename resolves when the renderer runs from that directory.
     */
    volbase = volfile;
    sep = strrchr(volfile, '/');
    if (sep) volbase = sep + 1;
    sep = strrchr(volbase, '\\');
    if (sep) volbase = sep + 1;
    ebmbase = ebmfile;
    sep = strrchr(ebmfile, '/');
    if (sep) ebmbase = sep + 1;
    sep = strrchr(ebmbase, '\\');
    if (sep) ebmbase = sep + 1;

    bu_log("voxelize: sampling %lu x %lu x %lu = %lu cells\n",
	   (unsigned long)xdim, (unsigned long)ydim, (unsigned long)zdim,
	   (unsigned long)nvox);
    bu_log("voxelize: field=%s  iso-window=[%d,%d]\n",
	   (field == FIELD_GYROID) ? "gyroid" :
	   (field == FIELD_METABALL) ? "metaball" : "mandel",
	   lo, hi);

    /* allocate the sample buffers */
    voldata = (unsigned char *)bu_calloc(nvox, sizeof(unsigned char), "voldata");
    ebmdata = (unsigned char *)bu_calloc(xdim * ydim, sizeof(unsigned char), "ebmdata");

    /* Sample the field.  We store in x-fastest order: index =
     * x + y*xdim + z*xdim*ydim, which is exactly the on-disk order the
     * vol primitive expects (it freads xdim bytes per (y,z) row).
     *
     * The 2-D ebm bitmap is a max-projection of the iso-window mask
     * down the Z axis: a column is "on" (255) if any voxel in it is
     * inside the iso-window.
     */
    for (z = 0; z < zdim; z++) {
	fastf_t w = (zdim > 1) ? (fastf_t)z / (fastf_t)(zdim - 1) : 0.0;
	for (y = 0; y < ydim; y++) {
	    fastf_t v = (ydim > 1) ? (fastf_t)y / (fastf_t)(ydim - 1) : 0.0;
	    for (x = 0; x < xdim; x++) {
		fastf_t u = (xdim > 1) ? (fastf_t)x / (fastf_t)(xdim - 1) : 0.0;
		fastf_t f;
		unsigned char b;
		size_t vidx;
		size_t eidx;

		f = sample_field(field, u, v, w);
		/* quantize the [0,1] field into a 0..255 byte */
		b = (unsigned char)(f * 255.0 + 0.5);

		vidx = x + y * xdim + z * xdim * ydim;
		voldata[vidx] = b;

		if ((int)b >= lo && (int)b <= hi) {
		    eidx = x + y * xdim;
		    ebmdata[eidx] = 255;
		}
	    }
	}
    }

    /* Write the sidecar .vol file (x-fastest, then y, then z). */
    fp = fopen(volfile, "wb");
    if (!fp) {
	perror(volfile);
	bu_exit(3, "voxelize: could not open vol data file for writing\n");
    }
    written = fwrite(voldata, sizeof(unsigned char), nvox, fp);
    fclose(fp);
    if (written != nvox)
	bu_exit(3, "voxelize: short write to %s\n", volfile);
    bu_log("voxelize: wrote %lu bytes to %s\n", (unsigned long)nvox, volfile);

    /* Write the sidecar .ebm file (x-fastest, then y). */
    fp = fopen(ebmfile, "wb");
    if (!fp) {
	perror(ebmfile);
	bu_exit(3, "voxelize: could not open ebm data file for writing\n");
    }
    written = fwrite(ebmdata, sizeof(unsigned char), xdim * ydim, fp);
    fclose(fp);
    if (written != xdim * ydim)
	bu_exit(3, "voxelize: short write to %s\n", ebmfile);
    bu_log("voxelize: wrote %lu bytes to %s\n",
	   (unsigned long)(xdim * ydim), ebmfile);

    /* The vol primitive's cellsize is mm-per-cell.  Pick a size that
     * makes the whole volume span a pleasant ~256 mm cube regardless
     * of resolution.
     */
    span = 256.0;
    VSET(cellsize, span / (fastf_t)xdim, span / (fastf_t)ydim, span / (fastf_t)zdim);

    /* identity placement matrix for the file-backed primitives */
    MAT_IDN(mat);

    /* Create the vol primitive backed by our sidecar file.  The
     * datasrc char 'f' (RT_VOL_SRC_FILE) selects a file data source.
     */
    if (mk_vol(db_fp, "vol.s", RT_VOL_SRC_FILE, volbase,
	       xdim, ydim, zdim, (size_t)lo, (size_t)hi, cellsize, mat) < 0) {
	bu_exit(4, "voxelize: mk_vol() failed\n");
    }

    /* Create the ebm primitive.  Give it a tallness that roughly
     * matches the in-plane footprint so the extrusion reads well.
     */
    tallness = span * 0.5;
    if (mk_ebm(db_fp, "ebm.s", ebmbase, xdim, ydim, tallness, mat) < 0) {
	bu_exit(4, "voxelize: mk_ebm() failed\n");
    }

    /* Optional cutaway: an arb8 box (via mk_rpp) covering the +X half
     * of the volume.  Subtracting it from the vol exposes the interior
     * structure of the isosurface.  This is robust: if anything about
     * it is undesirable, simply union the vol without the subtraction.
     */
    VSET(cutlo, span * 0.5, -span * 0.1, -span * 0.1);
    VSET(cuthi, span * 1.1, span * 1.1, span * 1.1);
    mk_rpp(db_fp, "cut.s", cutlo, cuthi);

    /* Build the vol region: (vol.s - cut.s) so we see inside it.  A
     * colorful plastic shader gives it visual punch.
     */
    BU_LIST_INIT(&region_hd.l);
    (void)mk_addmember("vol.s", &region_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("cut.s", &region_hd.l, NULL, WMOP_SUBTRACT);

    VSET(rgb, 235, 140, 40);	/* warm orange */
    mk_lcomb(db_fp,
	     "vol.r",
	     &region_hd,
	     1,			/* is_region */
	     "plastic",
	     "di=0.7 sp=0.4 sh=12",
	     rgb,
	     0);

    /* Build the ebm region, offset off to the side along +X so it sits
     * next to the volume rather than overlapping it.  The ebm grows in
     * its own local space starting at the origin, so we translate the
     * member with a matrix.
     */
    VSET(base, span * 1.4, 0.0, 0.0);
    MAT_IDN(mat);
    MAT_DELTAS_VEC(mat, base);

    BU_LIST_INIT(&all_hd.l);	/* reuse below; first build ebm region */
    {
	struct wmember ebm_hd;
	BU_LIST_INIT(&ebm_hd.l);
	(void)mk_addmember("ebm.s", &ebm_hd.l, mat, WMOP_UNION);
	VSET(rgb, 70, 170, 220);	/* cool blue */
	mk_lcomb(db_fp,
		 "ebm.r",
		 &ebm_hd,
		 1,		/* is_region */
		 "plastic",
		 "di=0.6 sp=0.5 sh=20",
		 rgb,
		 0);
    }

    /* Assemble the top-level group "all" so that 'tops' returns a
     * single renderable name unioning both regions.
     */
    BU_LIST_INIT(&all_hd.l);
    (void)mk_addmember("vol.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("ebm.r", &all_hd.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp,
	     "all",
	     &all_hd,
	     0,			/* not a region, just a group */
	     (char *)0,
	     (char *)0,
	     (unsigned char *)0,
	     0);

    bu_log("voxelize: wrote database %s (top-level group 'all')\n", av[1]);

    bu_free(voldata, "voldata");
    bu_free(ebmdata, "ebmdata");

    /* Close the database file. */
    db_close(db_fp->dbip);

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
