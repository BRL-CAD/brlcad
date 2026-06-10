/*                         G E A R S . C
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
/** @file proc-db/gears.c
 *
 * Build a meshing involute spur-gear train, demonstrating the
 * sketch->extrude procedural-modeling workflow in libwdb.
 *
 * For each gear a SINGLE involute tooth profile is constructed as an
 * rt_sketch_internal (2D vertices + line_seg curve segments), written
 * with mk_sketch(), and extruded to the gear thickness with
 * mk_extrusion().  The gear body is then assembled as the boolean
 * union of 'teeth' rotated copies of that one extruded tooth (placed
 * by Z-axis rotation matrices via mk_addmember), unioned with a hub
 * cylinder, with a central bore subtracted out.  Successive gears are
 * laid left-to-right with centers separated by the sum of their pitch
 * radii, and each is phase-shifted by a half tooth so the teeth
 * interlock (mesh).  Everything is collected, along with a ground
 * plane and a light, under a single top-level group named "all".
 *
 * All coordinates are in millimeters.
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


/* maximum number of gears we will allow on the command line */
#define MAX_GEARS 16

/* number of sample points used to approximate one involute flank */
#define FLANK_SAMPLES 8


/* A small palette of pleasant plastic colors, cycled per gear. */
static unsigned char gear_colors[][3] = {
    { 200,  60,  60 },   /* red    */
    {  60, 140, 210 },   /* blue   */
    {  90, 190,  90 },   /* green  */
    { 220, 170,  50 },   /* gold   */
    { 170,  90, 200 },   /* purple */
    {  60, 200, 200 }    /* cyan   */
};
#define NUM_COLORS ((int)(sizeof(gear_colors) / sizeof(gear_colors[0])))


/*
 * Build one involute spur gear as a named combination.
 *
 * The gear is centered at the origin in its own coordinate frame; the
 * caller positions the whole gear combination in the scene.  A single
 * tooth profile is sketched and extruded, then unioned 'teeth' times
 * around the Z axis.  A hub cylinder is unioned in and a bore cylinder
 * is subtracted.
 *
 * Returns 0 on success.
 */
static int
make_gear(struct rt_wdb *db_fp,
	  int gear_index,
	  int teeth,
	  double module,
	  double thickness,
	  double bore_frac)
{
    /* gear sizing, from standard involute spur-gear geometry */
    double pitch_r;	/* pitch circle radius */
    double base_r;	/* base circle radius (involute originates here) */
    double outer_r;	/* addendum (tip) radius */
    double root_r;	/* dedendum (root) radius */
    double hub_r;	/* radius of the solid central hub */
    double bore_r;	/* radius of the subtracted central bore */
    double tooth_ang;	/* angular pitch of one tooth, in radians */

    /* sketch construction */
    struct rt_sketch_internal skt;
    point2d_t *verts;
    struct line_seg *lsegs;
    void **segments;
    int *reverse;
    int nverts;
    int nseg;

    /* extrusion frame */
    point_t V;
    vect_t h, u_vec, v_vec;

    /* tooth flank geometry */
    double tip_half_ang;	/* half tooth angle at the tip */
    double base_half_ang;	/* half tooth angle at the base circle */

    /* combination assembly */
    struct wmember gear_head;
    char skt_name[64];
    char tooth_name[64];
    char hub_name[64];
    char bore_name[64];
    char gear_name[64];

    int i;
    int vidx;

    pitch_r = module * teeth / 2.0;
    base_r  = pitch_r * cos(20.0 * DEG2RAD);   /* 20 degree pressure angle */
    outer_r = pitch_r + module;                /* addendum = 1 module */
    root_r  = pitch_r - 1.25 * module;         /* dedendum = 1.25 module */
    if (root_r < base_r)
	root_r = base_r * 0.98;                 /* keep root inside the base */
    if (root_r < module * 0.5)
	root_r = module * 0.5;

    hub_r  = root_r * 0.92;
    bore_r = pitch_r * bore_frac;
    if (bore_r < module * 0.6)
	bore_r = module * 0.6;

    tooth_ang = M_2PI / (double)teeth;

    /*
     * Build one tooth profile in the sketch (u,v) plane.  We use a
     * simple but recognizable involute-style outline: the tooth is
     * symmetric about the +u axis.  We sample points up one flank from
     * the root to the tip, cross the (narrow) tip, then come back down
     * the mirror-image flank to the root, and finally close across the
     * root.  All segments are straight line_seg's, which is the most
     * robust approach for an extrudable closed loop.
     *
     * The tooth occupies a fraction of the angular pitch.  At the base
     * circle the tooth half-thickness spans ~1/4 of the angular pitch;
     * at the tip it narrows.  This gives a clean meshing trapezoid that
     * approximates a true involute well enough for a teaching demo.
     */
    base_half_ang = tooth_ang * 0.25;
    tip_half_ang  = tooth_ang * 0.10;

    /*
     * Vertex budget:
     *   FLANK_SAMPLES points up the +v flank (root -> tip)
     *   FLANK_SAMPLES points down the -v flank (tip -> root)
     * The two flanks share neither endpoint, so the closed loop has
     * 2*FLANK_SAMPLES vertices and 2*FLANK_SAMPLES line segments
     * (the last segment closes tip-side and root-side automatically).
     */
    nverts = 2 * FLANK_SAMPLES;
    nseg   = nverts;          /* closed loop: one segment per vertex */

    verts    = (point2d_t *)bu_calloc(nverts, sizeof(point2d_t), "gear verts");
    lsegs    = (struct line_seg *)bu_calloc(nseg, sizeof(struct line_seg), "gear lsegs");
    segments = (void **)bu_calloc(nseg, sizeof(void *), "gear segments");
    reverse  = (int *)bu_calloc(nseg, sizeof(int), "gear reverse");

    /*
     * Generate the +v flank from root to tip.  At each radial step we
     * linearly interpolate the half-angle from base_half_ang (root) to
     * tip_half_ang (tip); the flank vertex sits at +half_ang off the
     * tooth centerline (the +u axis).
     */
    vidx = 0;
    for (i = 0; i < FLANK_SAMPLES; i++) {
	double t;	/* 0 at root, 1 at tip */
	double r;	/* radius at this sample */
	double a;	/* half-angle at this sample */

	t = (double)i / (double)(FLANK_SAMPLES - 1);
	r = root_r + t * (outer_r - root_r);
	a = base_half_ang + t * (tip_half_ang - base_half_ang);

	verts[vidx][0] = r * cos(a);
	verts[vidx][1] = r * sin(a);
	vidx++;
    }

    /*
     * Generate the -v flank from tip back down to root (mirror image).
     */
    for (i = FLANK_SAMPLES - 1; i >= 0; i--) {
	double t;
	double r;
	double a;

	t = (double)i / (double)(FLANK_SAMPLES - 1);
	r = root_r + t * (outer_r - root_r);
	a = base_half_ang + t * (tip_half_ang - base_half_ang);

	verts[vidx][0] = r * cos(-a);
	verts[vidx][1] = r * sin(-a);
	vidx++;
    }

    /*
     * Connect the vertices into a closed loop of line segments.  Each
     * segment i runs from vertex i to vertex (i+1) mod nverts, so the
     * final segment closes the loop back to vertex 0.
     */
    for (i = 0; i < nseg; i++) {
	lsegs[i].magic = CURVE_LSEG_MAGIC;
	lsegs[i].start = i;
	lsegs[i].end   = (i + 1) % nverts;
	segments[i] = (void *)&lsegs[i];
	reverse[i] = 0;
    }

    /* assemble the sketch internal */
    skt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSETALL(skt.V, 0.0);
    VSET(skt.u_vec, 1.0, 0.0, 0.0);
    VSET(skt.v_vec, 0.0, 1.0, 0.0);
    skt.vert_count = (size_t)nverts;
    skt.verts = verts;
    skt.curve.count = (size_t)nseg;
    skt.curve.reverse = reverse;
    skt.curve.segment = segments;

    snprintf(skt_name, sizeof(skt_name), "tooth_%d.skt", gear_index);
    snprintf(tooth_name, sizeof(tooth_name), "tooth_%d.s", gear_index);
    snprintf(hub_name, sizeof(hub_name), "hub_%d.s", gear_index);
    snprintf(bore_name, sizeof(bore_name), "bore_%d.s", gear_index);
    snprintf(gear_name, sizeof(gear_name), "gear_%d.r", gear_index);

    if (mk_sketch(db_fp, skt_name, &skt) < 0)
	bu_exit(1, "Failed to write sketch %s\n", skt_name);

    /*
     * Extrude the tooth profile in +Z to the gear thickness.  The
     * sketch is defined in its own (u,v) plane; the extrusion uses the
     * same u/v vectors and lifts the loop by 'h' along Z.  V is the
     * embedding origin of the sketch in 3D.
     */
    VSET(V, 0.0, 0.0, 0.0);
    VSET(h, 0.0, 0.0, thickness);
    VSET(u_vec, 1.0, 0.0, 0.0);
    VSET(v_vec, 0.0, 1.0, 0.0);

    if (mk_extrusion(db_fp, tooth_name, skt_name, V, h, u_vec, v_vec, 0) < 0)
	bu_exit(1, "Failed to extrude tooth %s\n", tooth_name);

    /* the solid hub the teeth grow out of */
    VSET(V, 0.0, 0.0, 0.0);
    VSET(h, 0.0, 0.0, thickness);
    if (mk_rcc(db_fp, hub_name, V, h, hub_r) < 0)
	bu_exit(1, "Failed to make hub %s\n", hub_name);

    /* the central bore, made slightly taller so it cleanly subtracts */
    VSET(V, 0.0, 0.0, -thickness * 0.1);
    VSET(h, 0.0, 0.0, thickness * 1.2);
    if (mk_rcc(db_fp, bore_name, V, h, bore_r) < 0)
	bu_exit(1, "Failed to make bore %s\n", bore_name);

    /*
     * Assemble the gear region: hub + N rotated teeth, minus the bore.
     */
    BU_LIST_INIT(&gear_head.l);

    (void)mk_addmember(hub_name, &gear_head.l, NULL, WMOP_UNION);

    for (i = 0; i < teeth; i++) {
	mat_t rot;
	struct wmember *wm;
	double deg;

	deg = 360.0 * (double)i / (double)teeth;

	/* bn_mat_angles takes degrees: rotate about Z (gamma) only */
	bn_mat_angles(rot, 0.0, 0.0, deg);

	wm = mk_addmember(tooth_name, &gear_head.l, rot, WMOP_UNION);
	if (wm == NULL)
	    bu_exit(1, "Failed to add tooth instance to %s\n", gear_name);
    }

    (void)mk_addmember(bore_name, &gear_head.l, NULL, WMOP_SUBTRACT);

    /* make the gear a colored plastic region */
    if (mk_lcomb(db_fp, gear_name, &gear_head, 1,
		 "plastic", "di=0.8 sp=0.3",
		 gear_colors[gear_index % NUM_COLORS], 0) < 0)
	bu_exit(1, "Failed to make gear region %s\n", gear_name);

    bu_log("  gear %d: %d teeth, pitch_r=%.2f, outer_r=%.2f, bore_r=%.2f\n",
	   gear_index, teeth, pitch_r, outer_r, bore_r);

    /* free the per-gear sketch scratch storage */
    bu_free(verts, "gear verts");
    bu_free(lsegs, "gear lsegs");
    bu_free(segments, "gear segments");
    bu_free(reverse, "gear reverse");

    /* report the pitch radius back to the caller for spacing */
    return 0;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;

    /* tunable parameters with sensible defaults */
    double module = 5.0;	/* tooth size (mm) */
    int ngears = 3;		/* number of gears in the train */
    int teeth[MAX_GEARS];	/* teeth per gear */
    double thickness = 12.0;	/* extrusion depth (mm) */
    double bore_frac = 0.18;	/* bore radius as a fraction of pitch radius */

    /* scene assembly */
    struct wmember all_head;
    point_t pmin, pmax;
    point_t lpos;
    unsigned char ground_rgb[3];
    unsigned char light_rgb[3];

    double pitch_r[MAX_GEARS];
    double center_x[MAX_GEARS];
    double total_span;
    double max_outer;

    int i;
    int default_teeth[3];

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--module m] [--gears n] [--teeth \"t1 t2 ...\"] [--thickness t]\n", av[0]);
    }

    /* default gear train: 12, 18, 24 teeth */
    default_teeth[0] = 12;
    default_teeth[1] = 18;
    default_teeth[2] = 24;
    for (i = 0; i < 3; i++)
	teeth[i] = default_teeth[i];

    /*
     * Parse optional arguments.  Everything past the output path is
     * optional; running with only the output path produces a complete,
     * attractive default scene.
     */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--module") && i + 1 < ac) {
	    module = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--gears") && i + 1 < ac) {
	    ngears = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--thickness") && i + 1 < ac) {
	    thickness = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--teeth") && i + 1 < ac) {
	    /* parse a space-separated list of tooth counts */
	    char *tok;
	    char *buf;
	    int n;

	    buf = bu_strdup(av[++i]);
	    n = 0;
	    tok = strtok(buf, " ,");
	    while (tok != NULL && n < MAX_GEARS) {
		teeth[n++] = atoi(tok);
		tok = strtok(NULL, " ,");
	    }
	    if (n > 0)
		ngears = n;
	    bu_free(buf, "teeth buf");
	} else {
	    bu_log("Warning: ignoring unrecognized argument \"%s\"\n", av[i]);
	}
    }

    /* sanity-clamp the parameters */
    if (module <= 0.0)
	module = 5.0;
    if (ngears < 1)
	ngears = 1;
    if (ngears > MAX_GEARS)
	ngears = MAX_GEARS;
    if (thickness <= 0.0)
	thickness = 12.0;

    /* if more gears than supplied tooth counts, fill the rest */
    for (i = 0; i < ngears; i++) {
	if (teeth[i] < 6)
	    teeth[i] = 12 + 6 * i;   /* reasonable increasing default */
    }

    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, "Involute Spur Gear Train", "mm");

    bu_log("Building gear train: %d gears, module=%.2f, thickness=%.2f\n",
	   ngears, module, thickness);

    /* compute each gear's pitch radius up front for placement */
    max_outer = 0.0;
    for (i = 0; i < ngears; i++) {
	pitch_r[i] = module * teeth[i] / 2.0;
	if (pitch_r[i] + module > max_outer)
	    max_outer = pitch_r[i] + module;
    }

    /*
     * Lay the gears left to right.  Adjacent gear centers are separated
     * by the sum of their pitch radii so the pitch circles are tangent
     * (the meshing condition).
     */
    center_x[0] = 0.0;
    for (i = 1; i < ngears; i++)
	center_x[i] = center_x[i - 1] + pitch_r[i - 1] + pitch_r[i];
    total_span = center_x[ngears - 1];

    /* build each gear at the origin, then place it in the scene */
    BU_LIST_INIT(&all_head.l);

    for (i = 0; i < ngears; i++) {
	mat_t place;
	char gear_name[64];

	if (make_gear(db_fp, i, teeth[i], module, thickness, bore_frac) != 0)
	    bu_exit(1, "Failed to build gear %d\n", i);

	snprintf(gear_name, sizeof(gear_name), "gear_%d.r", i);

	/*
	 * Position the gear: translate to its center on the X axis,
	 * and apply a half-tooth phase rotation on every other gear so
	 * neighboring teeth interlock instead of colliding.  When two
	 * meshing gears turn, a tooth on one must land in the gap of
	 * the other; a half angular-pitch offset achieves that mesh.
	 */
	MAT_IDN(place);
	if (i % 2 == 1) {
	    mat_t phase;
	    double half_pitch_deg;

	    half_pitch_deg = 0.5 * 360.0 / (double)teeth[i];
	    bn_mat_angles(phase, 0.0, 0.0, half_pitch_deg);
	    MAT_COPY(place, phase);
	}
	MAT_DELTAS(place, center_x[i], 0.0, 0.0);

	(void)mk_addmember(gear_name, &all_head.l, place, WMOP_UNION);
    }

    /*
     * Add a ground plane beneath the gears.  The gears sit in the
     * z=[0,thickness] slab centered on the X axis; drop a thin slab
     * below them spanning the whole train with some margin.
     */
    VSET(pmin, -max_outer - module, -max_outer - module, -2.0 * thickness);
    VSET(pmax, total_span + max_outer + module, max_outer + module, -thickness);
    if (mk_rpp(db_fp, "ground.s", pmin, pmax) < 0)
	bu_exit(1, "Failed to make ground plane\n");

    VSET(ground_rgb, 120, 120, 130);
    if (mk_region1(db_fp, "ground.r", "ground.s",
		   "plastic", "di=0.7 sp=0.1", ground_rgb) < 0)
	bu_exit(1, "Failed to make ground region\n");
    (void)mk_addmember("ground.r", &all_head.l, NULL, WMOP_UNION);

    /*
     * Add a light: a small bright sphere up and to the side of the
     * train, given the 'light' shader so the raytracer treats it as an
     * illumination source.
     */
    VSET(lpos,
	 total_span * 0.5,
	 -max_outer * 2.0,
	 max_outer * 3.0 + thickness);
    if (mk_sph(db_fp, "light.s", lpos, max_outer * 0.25) < 0)
	bu_exit(1, "Failed to make light\n");

    VSET(light_rgb, 255, 255, 255);
    if (mk_region1(db_fp, "light.r", "light.s",
		   "light", "inten=1.0 shadows=1", light_rgb) < 0)
	bu_exit(1, "Failed to make light region\n");
    (void)mk_addmember("light.r", &all_head.l, NULL, WMOP_UNION);

    /* collect everything under a single renderable top-level group */
    if (mk_lcomb(db_fp, "all", &all_head, 0,
		 (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0) < 0)
	bu_exit(1, "Failed to make top-level group 'all'\n");

    bu_log("Done.  Top-level group 'all' contains %d gears, a ground plane, and a light.\n",
	   ngears);

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
