/*                     S P E C I M E N . C
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
/** @file proc-db/specimen.c
 *
 * Build a "specimen shelf" -- a regular grid scene that places ONE of
 * each second-order / less-common BRL-CAD analytic primitive, each in
 * its own colored 'plastic' region.  The shelf includes the rpc, rhc,
 * epa, ehy, hyp, eto, hrt (heart), arbn (a procedural truncated
 * octahedron built from plane equations), particle, tor, tgc, and ell
 * primitives, laid out on an evenly spaced grid so they never overlap.
 * A ground plane and a light are added and everything is unioned under
 * a single top-level group named "all".
 *
 * This doubles as a primitive regression / benchmark scene: rendering
 * "all" exercises the ray-trace routines of a dozen distinct solid
 * types in one pass.
 *
 * Note that since the values in the database are stored in millimeters,
 * all of the arguments to the mk_* routines below are likewise in
 * millimeters.
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


/* Default edge length of one grid cell, in millimeters.  Each specimen
 * is sized to comfortably fit inside a cell of this size with margin.
 */
#define DEFAULT_SPACING 600.0

/* Number of columns in the display grid.  Specimens are laid out left
 * to right, bottom to top, wrapping after this many columns.
 */
#define GRID_COLS 4


/*
 * Compute the world-space center of grid cell 'idx'.  Cells are tiled
 * in the X-Y plane (the ground), GRID_COLS per row, with the center of
 * each specimen lifted to z=0 (objects build upward from there).  The
 * grid is roughly centered about the world origin for a pleasant view.
 */
static void
cell_center(point_t out, int idx, double spacing)
{
    int row, col;
    double x_off, y_off;

    col = idx % GRID_COLS;
    row = idx / GRID_COLS;

    /* shift so the grid straddles the origin in X */
    x_off = (col - (GRID_COLS - 1) / 2.0) * spacing;
    y_off = row * spacing;

    VSET(out, x_off, y_off, 0.0);
}


/*
 * Create a single-solid region wrapping the named solid, giving it a
 * 'plastic' shader with the supplied color.  The new region is added as
 * a union member to the caller's assembly list so it ends up under the
 * top-level "all" group.
 */
static void
shelf_region(struct rt_wdb *db_fp, struct bu_list *all_hd,
	     const char *solid, const char *region,
	     int r, int g, int b)
{
    struct wmember reg_hd;	/* members of this one region */
    unsigned char rgb[3];

    BU_LIST_INIT(&reg_hd.l);
    (void)mk_addmember(solid, &reg_hd.l, NULL, WMOP_UNION);

    rgb[0] = (unsigned char)r;
    rgb[1] = (unsigned char)g;
    rgb[2] = (unsigned char)b;

    /* a plastic shader with a bit of specular highlight reads well */
    mk_lcomb(db_fp, region, &reg_hd, 1,
	     "plastic", "di=0.7 sp=0.3",
	     rgb, 0);

    /* fold the finished region into the overall assembly */
    (void)mk_addmember(region, all_hd, NULL, WMOP_UNION);
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct wmember all_hd;	/* head of the top-level "all" group */
    double spacing = DEFAULT_SPACING;
    double s;			/* a generic per-cell working scale */
    int idx = 0;		/* running grid-cell index */
    int i;

    /* geometry working storage */
    point_t cen;
    point_t vert;
    vect_t height, breadth, avec, norm, smajor;
    vect_t xv, yv, zv;
    vect_t hv, aav, bv, cv, dv;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--spacing mm]\n", av[0]);
    }

    /* Optional arguments.  The only knob is --spacing, the grid pitch;
     * everything else has a sensible default so that running with just
     * the output path produces a complete, attractive scene.
     */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--spacing") && (i + 1) < ac) {
	    spacing = atof(av[i + 1]);
	    i++;
	    if (spacing < 100.0) {
		bu_log("spacing %g too small, clamping to 100\n", spacing);
		spacing = 100.0;
	    }
	} else {
	    bu_exit(1, "Usage: %s output.g [--spacing mm]\n", av[0]);
	}
    }

    /* Open/create the database for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, "Primitive Specimen Shelf", "mm");

    bu_log("Building specimen shelf in [%s], grid pitch %g mm\n",
	   av[1], spacing);

    /* The top-level assembly list collects every region we create. */
    BU_LIST_INIT(&all_hd.l);

    /* A convenient interior working scale; primitives are sized to fit
     * comfortably inside a cell with margin on all sides.
     */
    s = spacing * 0.30;

    /*
     * RPC -- right parabolic cylinder.  Needs a vertex, a height
     * vector H, a breadth vector B with B . H == 0, and a scalar
     * rectangular half-width.  We orient H along +Z and B along +X.
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(height, 0.0, 0.0, 2.0 * s);
    VSET(breadth, s, 0.0, 0.0);		/* perpendicular to height */
    mk_rpc(db_fp, "rpc.s", vert, height, breadth, s * 0.6);
    shelf_region(db_fp, &all_hd.l, "rpc.s", "rpc.r", 220, 40, 40);

    /*
     * RHC -- right hyperbolic cylinder.  Like the rpc but with an
     * additional asymptote distance controlling the hyperbola.
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(height, 0.0, 0.0, 2.0 * s);
    VSET(breadth, s, 0.0, 0.0);
    mk_rhc(db_fp, "rhc.s", vert, height, breadth, s * 0.6, s * 0.4);
    shelf_region(db_fp, &all_hd.l, "rhc.s", "rhc.r", 240, 130, 30);

    /*
     * EPA -- elliptical paraboloid.  Height vector H, a breadth (semi-
     * major axis) vector A with A . H == 0, and scalar semi-major /
     * semi-minor radii r1 >= r2.
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(height, 0.0, 0.0, 2.0 * s);
    VSET(breadth, 1.0, 0.0, 0.0);	/* unit semi-major axis; radii set the size */
    mk_epa(db_fp, "epa.s", vert, height, breadth, s, s * 0.6);
    shelf_region(db_fp, &all_hd.l, "epa.s", "epa.r", 230, 215, 40);

    /*
     * EHY -- elliptical hyperboloid.  As epa, plus the apex-to-cone
     * distance c.
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(height, 0.0, 0.0, 2.0 * s);
    VSET(breadth, 1.0, 0.0, 0.0);	/* unit semi-major axis; radii set the size */
    mk_ehy(db_fp, "ehy.s", vert, height, breadth, s, s * 0.6, s * 0.4);
    shelf_region(db_fp, &all_hd.l, "ehy.s", "ehy.r", 120, 210, 40);

    /*
     * HYP -- hyperboloid of one sheet.  Vertex, height vector, an A
     * vector (semi-major axis of the waist), magnitude of the B
     * (semi-minor) axis, and a neck-to-base ratio in (0,1).
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(height, 0.0, 0.0, 2.0 * s);
    VSET(avec, s, 0.0, 0.0);
    mk_hyp(db_fp, "hyp.s", vert, height, avec, s * 0.6, 0.5);
    shelf_region(db_fp, &all_hd.l, "hyp.s", "hyp.r", 40, 200, 120);

    /*
     * ETO -- elliptical torus.  Center, plane normal N, a vector C
     * along the semi-major axis of the elliptical cross-section, the
     * radius of revolution r, and the semi-minor cross-section length.
     */
    cell_center(cen, idx++, spacing);
    VMOVE(vert, cen);
    VSET(norm, 0.0, 0.0, 1.0);
    VSET(smajor, s * 0.4, 0.0, s * 0.4);	/* tilted ellipse */
    mk_eto(db_fp, "eto.s", vert, norm, smajor, s, s * 0.2);
    shelf_region(db_fp, &all_hd.l, "eto.s", "eto.r", 40, 180, 220);

    /*
     * HRT -- heart primitive.  Center plus three mutually perpendicular
     * axis vectors (x, y, z) and a scalar distance.
     */
    cell_center(cen, idx++, spacing);
    VSET(xv, s, 0.0, 0.0);
    VSET(yv, 0.0, s, 0.0);
    VSET(zv, 0.0, 0.0, s);
    mk_hrt(db_fp, "hrt.s", cen, xv, yv, zv, s * 0.25);
    shelf_region(db_fp, &all_hd.l, "hrt.s", "hrt.r", 220, 40, 140);

    /*
     * ARBN -- an N-faced convex polyhedron defined entirely by its
     * bounding half-spaces.  Each plane_t holds an outward unit normal
     * in [0..2] and the signed distance from the origin in [3]; the
     * solid is the intersection of all the half-spaces.
     *
     * Here we build a truncated octahedron: the six axis-aligned faces
     * of a cube intersected with the eight oblique (+/-1,+/-1,+/-1)
     * faces that slice off the cube's corners.  Because the cube faces
     * sit farther out than the corner cuts, the corners are truncated,
     * yielding the familiar 14-faced Archimedean solid.  Distances are
     * expressed relative to the cell center, so we add the cell center
     * onto each plane distance via the dot product of the normal with
     * the center point.
     */
    cell_center(cen, idx++, spacing);
    {
	plane_t eqn[14];
	double cube_d = s;		/* cube face offset */
	double cut_d = s * 1.30;	/* corner-cut offset (> cube_d) */
	int n = 0;
	int sx, sy, sz;

	/* six axis-aligned cube faces */
	VSET(eqn[n], 1.0, 0.0, 0.0); eqn[n][W] = cube_d; n++;
	VSET(eqn[n], -1.0, 0.0, 0.0); eqn[n][W] = cube_d; n++;
	VSET(eqn[n], 0.0, 1.0, 0.0); eqn[n][W] = cube_d; n++;
	VSET(eqn[n], 0.0, -1.0, 0.0); eqn[n][W] = cube_d; n++;
	VSET(eqn[n], 0.0, 0.0, 1.0); eqn[n][W] = cube_d; n++;
	VSET(eqn[n], 0.0, 0.0, -1.0); eqn[n][W] = cube_d; n++;

	/* eight oblique corner-cutting faces */
	for (sx = -1; sx <= 1; sx += 2) {
	    for (sy = -1; sy <= 1; sy += 2) {
		for (sz = -1; sz <= 1; sz += 2) {
		    vect_t nrm;
		    VSET(nrm, (double)sx, (double)sy, (double)sz);
		    VUNITIZE(nrm);
		    VMOVE(eqn[n], nrm);
		    eqn[n][W] = cut_d;
		    n++;
		}
	    }
	}

	/* translate every plane so the solid is centered on the cell:
	 * a plane N.X = d through the origin becomes N.X = d + N.C when
	 * the body is shifted by the center point C.
	 */
	for (i = 0; i < n; i++) {
	    eqn[i][W] += VDOT(eqn[i], cen);
	}

	mk_arbn(db_fp, "arbn.s", (size_t)n, eqn);
    }
    shelf_region(db_fp, &all_hd.l, "arbn.s", "arbn.r", 150, 90, 220);

    /*
     * PARTICLE -- a "lozenge": a cylinder capped by hemispheres, sized
     * by a vertex, a height vector, and the sphere radii at the base
     * (vradius) and tip (hradius).  Equal radii give a clean capsule.
     */
    cell_center(cen, idx++, spacing);
    VSET(vert, cen[X], cen[Y], cen[Z] - s);	/* start below center */
    VSET(height, 0.0, 0.0, 2.0 * s);
    mk_particle(db_fp, "part.s", vert, height, s * 0.35, s * 0.35);
    shelf_region(db_fp, &all_hd.l, "part.s", "part.r", 70, 110, 230);

    /*
     * TOR -- a classic circular torus: center, normal, radius of
     * revolution r1, and tube radius r2.
     */
    cell_center(cen, idx++, spacing);
    VSET(norm, 0.0, 0.0, 1.0);
    mk_tor(db_fp, "tor.s", cen, norm, s, s * 0.35);
    shelf_region(db_fp, &all_hd.l, "tor.s", "tor.r", 200, 200, 200);

    /*
     * TGC -- truncated general cone.  Base point, height vector H, and
     * two pairs of ellipse axis vectors (a, b) at the base and (c, d)
     * at the top.  We taper a wide base to a narrower elliptical top.
     */
    cell_center(cen, idx++, spacing);
    VSET(vert, cen[X], cen[Y], cen[Z] - s);
    VSET(hv, 0.0, 0.0, 2.0 * s);
    VSET(aav, s * 0.6, 0.0, 0.0);
    VSET(bv, 0.0, s * 0.6, 0.0);
    VSET(cv, s * 0.3, 0.0, 0.0);
    VSET(dv, 0.0, s * 0.45, 0.0);
    mk_tgc(db_fp, "tgc.s", vert, hv, aav, bv, cv, dv);
    shelf_region(db_fp, &all_hd.l, "tgc.s", "tgc.r", 120, 200, 200);

    /*
     * ELL -- a general ellipsoid: center plus three perpendicular
     * radius vectors of differing lengths.
     */
    cell_center(cen, idx++, spacing);
    VSET(aav, s * 0.8, 0.0, 0.0);
    VSET(bv, 0.0, s * 0.5, 0.0);
    VSET(cv, 0.0, 0.0, s);
    mk_ell(db_fp, "ell.s", cen, aav, bv, cv);
    shelf_region(db_fp, &all_hd.l, "ell.s", "ell.r", 230, 160, 110);

    /*
     * Ground plane.  A broad, thin slab sitting just under the row of
     * specimens, sized from the number of cells placed so it always
     * spans the whole shelf with margin.
     */
    {
	point_t pmin, pmax;
	struct wmember gnd_hd;
	unsigned char rgb[3];
	int rows = (idx + GRID_COLS - 1) / GRID_COLS;
	double half_x = (GRID_COLS / 2.0 + 1.0) * spacing;
	double y_lo = -spacing;
	double y_hi = rows * spacing;

	VSET(pmin, -half_x, y_lo, -1.5 * s - 20.0);
	VSET(pmax, half_x, y_hi, -1.5 * s);
	mk_rpp(db_fp, "ground.s", pmin, pmax);

	BU_LIST_INIT(&gnd_hd.l);
	(void)mk_addmember("ground.s", &gnd_hd.l, NULL, WMOP_UNION);
	VSET(rgb, 90, 90, 90);
	mk_lcomb(db_fp, "ground.r", &gnd_hd, 1,
		 "plastic", "di=0.8 sp=0.1", rgb, 0);
	(void)mk_addmember("ground.r", &all_hd.l, NULL, WMOP_UNION);
    }

    /*
     * Light.  A small bright sphere placed up and to the side, given a
     * "light" shader so the ray tracer treats it as an illumination
     * source rather than ordinary geometry.
     */
    {
	point_t lpos;
	struct wmember lt_hd;
	unsigned char rgb[3];
	int rows = (idx + GRID_COLS - 1) / GRID_COLS;

	VSET(lpos, -2.0 * spacing, -1.5 * spacing,
	     rows * spacing + 2.0 * spacing);
	mk_sph(db_fp, "light.s", lpos, spacing * 0.15);

	BU_LIST_INIT(&lt_hd.l);
	(void)mk_addmember("light.s", &lt_hd.l, NULL, WMOP_UNION);
	VSET(rgb, 255, 255, 255);
	mk_lcomb(db_fp, "light.r", &lt_hd, 1,
		 "light", "inten=1.0 shadows=1", rgb, 0);
	(void)mk_addmember("light.r", &all_hd.l, NULL, WMOP_UNION);
    }

    /*
     * Finally, union every region into a single top-level group named
     * "all" so the renderer can be aimed at one object.  This is a
     * group (region flag 0), not a region, so it carries no shader of
     * its own.
     */
    mk_lcomb(db_fp, "all", &all_hd, 0,
	     (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    bu_log("Placed %d specimens plus ground and light under group \"all\"\n",
	   idx);

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
