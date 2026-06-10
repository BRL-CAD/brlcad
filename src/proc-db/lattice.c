/*                       L A T T I C E . C
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
/** @file proc-db/lattice.c
 *
 * Build a geodesic dome space-frame from a SINGLE instanced strut
 * prototype.  The program starts from the 12 vertices and 20 faces of
 * a regular icosahedron, subdivides every face into a frequency-v
 * triangular grid, projects (normalizes) every grid vertex onto the
 * circumscribed sphere to make a geodesic mesh, and de-duplicates the
 * shared vertices and edges by hashing quantized coordinates.
 *
 * A single unit-length cylinder prototype is created once along +Z.
 * For each unique edge of the mesh a transformation matrix is built
 * (rotate +Z onto the edge direction via bn_mat_fromto, scale to the
 * edge length, translate to the start node) and the prototype is
 * instanced into the dome combination with that matrix, so the entire
 * frame references one solid.  Spheres act as the connecting joints at
 * each unique node.  The result is unioned with a ground slab and an
 * area light beneath a top-level group named "all".
 *
 * This also serves as a demonstration of mk_addmember()-with-matrix
 * instancing and (optionally) of mk_cline() struts.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


/* Maximum supported subdivision frequency.  Beyond this the node and
 * edge counts become unwieldy for a teaching demo.
 */
#define MAX_FREQ 12

/* Quantization grid used when hashing coordinates so that vertices
 * shared between adjacent faces collapse to a single node.  Coordinates
 * are rounded to this many millimeters before comparison.
 */
#define QUANT 0.01

/* Upper bounds on the de-duplicated node and edge tables.  These are
 * generous; the actual counts for frequency-v over an icosahedron are
 * 10*v*v + 2 nodes and 30*v*v edges for the full sphere.
 */
#define MAX_NODES 4096
#define MAX_EDGES 12288


/* The 12 canonical icosahedron vertices, expressed with the golden
 * ratio.  They are normalized to the unit sphere at startup.
 */
static point_t ico_vert[12];

/* The 20 triangular faces, as indices into ico_vert[]. */
static int ico_face[20][3] = {
    {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
    {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
    {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
    {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
};


/* De-duplicated node table:  unit-sphere positions of every unique
 * node, scaled later by the requested dome radius.
 */
static point_t nodes[MAX_NODES];
static int num_nodes = 0;

/* De-duplicated edge table:  each edge stores the two node indices it
 * connects.
 */
static int edges[MAX_EDGES][2];
static int num_edges = 0;


/*
 * Initialize the 12 icosahedron vertices on the unit sphere.
 */
static void
init_icosahedron(void)
{
    /* golden ratio */
    double t = (1.0 + sqrt(5.0)) / 2.0;
    int i;

    VSET(ico_vert[0], -1,  t,  0);
    VSET(ico_vert[1],  1,  t,  0);
    VSET(ico_vert[2], -1, -t,  0);
    VSET(ico_vert[3],  1, -t,  0);

    VSET(ico_vert[4],  0, -1,  t);
    VSET(ico_vert[5],  0,  1,  t);
    VSET(ico_vert[6],  0, -1, -t);
    VSET(ico_vert[7],  0,  1, -t);

    VSET(ico_vert[8],  t,  0, -1);
    VSET(ico_vert[9],  t,  0,  1);
    VSET(ico_vert[10], -t,  0, -1);
    VSET(ico_vert[11], -t,  0,  1);

    /* project onto the unit sphere so every vertex is at radius 1 */
    for (i = 0; i < 12; i++) {
	VUNITIZE(ico_vert[i]);
    }
}


/* Quantize a coordinate onto the fixed dedup grid.  The floor() result
 * is stored before the cast so we are not casting a function-call value
 * directly.
 */
static long
quantize(double v)
{
    double q = floor(v / QUANT + 0.5);
    return (long)q;
}


/*
 * Find (or insert) a node at the given unit-sphere position, returning
 * its index in the de-duplicated node table.  Sharing is detected by
 * quantizing coordinates onto a fixed grid and comparing.
 */
static int
find_or_add_node(const point_t p)
{
    long qx, qy, qz;
    int i;

    /* quantize to collapse coincident vertices from adjacent faces */
    qx = quantize(p[X]);
    qy = quantize(p[Y]);
    qz = quantize(p[Z]);

    for (i = 0; i < num_nodes; i++) {
	long nx = quantize(nodes[i][X]);
	long ny = quantize(nodes[i][Y]);
	long nz = quantize(nodes[i][Z]);

	if (nx == qx && ny == qy && nz == qz) {
	    return i;
	}
    }

    if (num_nodes >= MAX_NODES) {
	bu_exit(1, "ERROR: exceeded MAX_NODES (%d); lower the frequency\n", MAX_NODES);
    }

    VMOVE(nodes[num_nodes], p);
    return num_nodes++;
}


/*
 * Add a unique edge between two node indices.  Edges are stored with
 * the smaller index first so that an edge shared by two faces is only
 * recorded once.
 */
static void
add_edge(int a, int b)
{
    int lo, hi, i;

    if (a == b)
	return;

    if (a < b) {
	lo = a; hi = b;
    } else {
	lo = b; hi = a;
    }

    for (i = 0; i < num_edges; i++) {
	if (edges[i][0] == lo && edges[i][1] == hi)
	    return;
    }

    if (num_edges >= MAX_EDGES) {
	bu_exit(1, "ERROR: exceeded MAX_EDGES (%d); lower the frequency\n", MAX_EDGES);
    }

    edges[num_edges][0] = lo;
    edges[num_edges][1] = hi;
    num_edges++;
}


/*
 * Subdivide one icosahedron face into a frequency-v triangular grid.
 * Each grid vertex is the barycentric blend of the three face corners,
 * normalized onto the unit sphere.  The small triangles' three edges
 * are registered (de-duplicated) into the global edge table.
 *
 * The grid is indexed by (i, j) with 0 <= i <= freq and 0 <= j <= i,
 * forming the classic triangular lattice of the face.
 */
static void
subdivide_face(const point_t c0, const point_t c1, const point_t c2, int freq)
{
    /* grid[i][j] holds the node index of the projected grid vertex */
    int grid[MAX_FREQ + 1][MAX_FREQ + 1];
    int i, j;

    for (i = 0; i <= freq; i++) {
	for (j = 0; j <= i; j++) {
	    point_t p;
	    double a, b, c;

	    /* barycentric weights: row i out of freq, column j out of i */
	    b = (double)j / (double)freq;
	    c = (double)(i - j) / (double)freq;
	    a = 1.0 - b - c;

	    p[X] = a * c0[X] + b * c1[X] + c * c2[X];
	    p[Y] = a * c0[Y] + b * c1[Y] + c * c2[Y];
	    p[Z] = a * c0[Z] + b * c1[Z] + c * c2[Z];

	    /* geodesic projection back onto the unit sphere */
	    VUNITIZE(p);

	    grid[i][j] = find_or_add_node(p);
	}
    }

    /* the edges of every small triangle in the face grid */
    for (i = 1; i <= freq; i++) {
	for (j = 0; j < i; j++) {
	    /* "upward" triangle: (i,j)-(i,j+1)-(i-1,j) */
	    add_edge(grid[i][j], grid[i][j + 1]);
	    add_edge(grid[i][j], grid[i - 1][j]);
	    add_edge(grid[i][j + 1], grid[i - 1][j]);

	    /* "downward" triangle shares edges already added above for
	     * the inner rows; its unique edge is (i-1,j)-(i-1,j+1) when
	     * it exists.  Those are registered when row i-1 is the base
	     * of its own upward triangles, so no extra work is needed.
	     */
	}
    }
}


int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    struct bn_tol tol;
    struct wmember dome_head;   /* struts + joints */
    struct wmember all_head;    /* top-level group */

    /* tunable parameters with attractive defaults */
    int frequency = 3;
    double radius = 1000.0;        /* dome radius, mm */
    double strut_radius = 12.0;    /* strut cylinder radius, mm */
    int use_cline = 0;             /* 0 = rcc struts, 1 = cline struts */
    int hemisphere = 0;           /* 1 = drop nodes below the equator */
    double lat_cut;                /* z threshold for hemisphere */
    double zlift;                  /* amount to raise the frame above ground */

    int i;
    int kept_edges = 0;
    int kept_joints = 0;
    vect_t zaxis;
    unsigned char strut_rgb[3];
    unsigned char ground_rgb[3];
    point_t gmin, gmax;
    point_t lpos;

    bu_setprogname(av[0]);

    if (ac < 2) {
	bu_exit(1, "Usage: %s output.g [--frequency v] [--radius mm] [--strut-radius mm] [--strut rcc|cline] [--hemisphere 0|1]\n", av[0]);
    }

    /* Parse the optional parameters.  Everything past av[1] is
     * optional so that running with only the output path yields a
     * complete, attractive scene.
     */
    for (i = 2; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--frequency") && i + 1 < ac) {
	    frequency = atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--radius") && i + 1 < ac) {
	    radius = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--strut-radius") && i + 1 < ac) {
	    strut_radius = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--strut") && i + 1 < ac) {
	    i++;
	    if (BU_STR_EQUAL(av[i], "cline")) {
		use_cline = 1;
	    } else {
		use_cline = 0;
	    }
	} else if (BU_STR_EQUAL(av[i], "--hemisphere") && i + 1 < ac) {
	    hemisphere = atoi(av[++i]);
	} else {
	    bu_log("WARNING: ignoring unrecognized argument '%s'\n", av[i]);
	}
    }

    if (frequency < 1)
	frequency = 1;
    if (frequency > MAX_FREQ)
	bu_exit(1, "ERROR: frequency %d exceeds maximum of %d\n", frequency, MAX_FREQ);
    if (radius <= 0.0)
	radius = 1000.0;
    if (strut_radius <= 0.0)
	strut_radius = 12.0;

    /* a slim slice below the equator so the rim ring is kept */
    lat_cut = -0.01;

    /* For a full sphere (the default) the frame spans z in [-radius,
     * +radius]; lift it so its lowest node sits just above the ground
     * slab (which has its top at z=0).  For a hemisphere the cull keeps
     * everything at/above the equator, so it already rests on z=0 and
     * needs no lift.
     */
    if (hemisphere)
	zlift = 0.0;
    else
	zlift = radius + 0.05 * radius;

    /* standard distance tolerance for bn_mat_fromto */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1.0 - tol.perp;

    bu_log("Building geodesic dome: frequency=%d radius=%.1fmm strut=%s %s\n",
	   frequency, radius,
	   use_cline ? "cline" : "rcc",
	   hemisphere ? "(hemisphere)" : "(full sphere)");

    /* Open/Create the database file for writing. */
    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 2;
    }

    mk_id_units(db_fp, "Geodesic Dome Space-Frame", "mm");

    /* Generate the geodesic mesh on the unit sphere. */
    init_icosahedron();

    for (i = 0; i < 20; i++) {
	subdivide_face(ico_vert[ico_face[i][0]],
		       ico_vert[ico_face[i][1]],
		       ico_vert[ico_face[i][2]],
		       frequency);
    }

    bu_log("Mesh: %d unique nodes, %d unique edges (unit sphere)\n",
	   num_nodes, num_edges);

    /* Scale every node out to the requested dome radius, then lift the
     * whole frame so it sits just above the ground slab.
     */
    for (i = 0; i < num_nodes; i++) {
	VSCALE(nodes[i], nodes[i], radius);
	nodes[i][Z] += zlift;
    }

    /* Create the single unit strut prototype: a unit-length right
     * circular cylinder along +Z, based at the origin.  Only used when
     * emitting rcc struts; cline struts are written directly.
     */
    VSET(zaxis, 0.0, 0.0, 1.0);
    if (!use_cline) {
	point_t base;
	vect_t height;
	VSET(base, 0.0, 0.0, 0.0);
	VSET(height, 0.0, 0.0, 1.0);
	mk_rcc(db_fp, "strut.proto", base, height, strut_radius);
    }

    /* Assemble the dome combination from the unique edges and nodes. */
    BU_LIST_INIT(&dome_head.l);

    /* Instance one strut per unique edge. */
    for (i = 0; i < num_edges; i++) {
	point_t a, b;
	vect_t dir;
	double len;
	int na = edges[i][0];
	int nb = edges[i][1];

	VMOVE(a, nodes[na]);
	VMOVE(b, nodes[nb]);

	/* hemisphere cull: drop any edge with a node below the cut */
	if (hemisphere && (a[Z] < lat_cut * radius || b[Z] < lat_cut * radius))
	    continue;

	VSUB2(dir, b, a);
	len = MAGNITUDE(dir);
	if (len < VUNITIZE_TOL)
	    continue;

	if (use_cline) {
	    /* mk_cline takes a base point, a height vector, radius, and
	     * a (zero) thickness for a solid cylinder-like strut.
	     */
	    char cname[64];
	    snprintf(cname, sizeof(cname), "cline.%d", i);
	    mk_cline(db_fp, cname, a, dir, strut_radius, 0.0);
	    (void)mk_addmember(cname, &dome_head.l, NULL, WMOP_UNION);
	} else {
	    /* Build the instancing matrix: rotate +Z onto the edge
	     * direction, scale by the edge length, translate to node A.
	     */
	    mat_t rot;
	    mat_t mat;
	    vect_t udir;
	    int r, c;

	    VMOVE(udir, dir);
	    VUNITIZE(udir);

	    /* rotation taking +Z to the unit edge direction */
	    bn_mat_fromto(rot, zaxis, udir, &tol);

	    /* compose: scale the rotated unit cylinder to the edge
	     * length, then translate the base to node A.  Because the
	     * prototype is one unit tall along +Z, scaling the rotation
	     * columns by len stretches it to span the edge.
	     */
	    MAT_IDN(mat);
	    for (r = 0; r < 3; r++) {
		for (c = 0; c < 3; c++) {
		    mat[r * 4 + c] = rot[r * 4 + c] * len;
		}
		mat[r * 4 + 3] = a[r];
	    }

	    (void)mk_addmember("strut.proto", &dome_head.l, mat, WMOP_UNION);
	}

	kept_edges++;
    }

    /* Place a joint sphere at every unique node (slightly larger than
     * the strut radius so joints visually fuse with the struts).
     */
    for (i = 0; i < num_nodes; i++) {
	char jname[64];

	if (hemisphere && nodes[i][Z] < lat_cut * radius)
	    continue;

	snprintf(jname, sizeof(jname), "joint.%d", i);
	mk_sph(db_fp, jname, nodes[i], strut_radius * 1.6);
	(void)mk_addmember(jname, &dome_head.l, NULL, WMOP_UNION);
	kept_joints++;
    }

    bu_log("Instanced %d struts and %d joints\n", kept_edges, kept_joints);

    /* Make the dome a metallic region. */
    VSET(strut_rgb, 200, 205, 215);  /* brushed metal */
    mk_lcomb(db_fp, "dome.r", &dome_head, 1,
	     "plastic", "di=0.4 sp=0.6 sh=18 me=1",
	     strut_rgb, 0);

    /* Ground slab: a wide, thin arb8 just under the dome rim. */
    VSET(gmin, -2.0 * radius, -2.0 * radius, -0.06 * radius);
    VSET(gmax,  2.0 * radius,  2.0 * radius,  0.0);
    mk_rpp(db_fp, "ground.s", gmin, gmax);
    VSET(ground_rgb, 90, 110, 80);
    mk_region1(db_fp, "ground.r", "ground.s", "plastic", "di=0.8 sp=0.1", ground_rgb);

    /* A light source above and to the side of the dome. */
    {
	struct wmember light_head;
	unsigned char light_rgb[3];

	VSET(lpos, 1.5 * radius, -1.5 * radius, 2.5 * radius);
	mk_sph(db_fp, "light.s", lpos, 0.08 * radius);

	BU_LIST_INIT(&light_head.l);
	(void)mk_addmember("light.s", &light_head.l, NULL, WMOP_UNION);
	VSET(light_rgb, 255, 255, 255);
	mk_lcomb(db_fp, "light.r", &light_head, 1,
		 "light", "inten=1.0 shadows=1",
		 light_rgb, 0);
    }

    /* Top-level group "all" unions everything into one renderable. */
    BU_LIST_INIT(&all_head.l);
    (void)mk_addmember("dome.r", &all_head.l, NULL, WMOP_UNION);
    (void)mk_addmember("ground.r", &all_head.l, NULL, WMOP_UNION);
    (void)mk_addmember("light.r", &all_head.l, NULL, WMOP_UNION);
    mk_lcomb(db_fp, "all", &all_head, 0, NULL, NULL, NULL, 0);

    bu_log("Wrote dome to %s; render the top-level object 'all'.\n", av[1]);

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
