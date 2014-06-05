/*                       P A T C H - G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#ifndef CONV_PATCH_PATCH_G_H
#define CONV_PATCH_PATCH_G_H

#include "vmath.h"

/* structures are currently using approximately this much memory:
 *
 *   ((NAMESIZE * 2) + 320) * MAX_INPUTS
 *
 * which is approximately 80MB with 256 and 100000 respectively
 */
#define NAMESIZE 256
#define MAX_INPUTS 100000


struct input {
    fastf_t x;
    fastf_t y;
    fastf_t z;
    fastf_t rsurf_thick;
    int surf_type;
    int surf_thick;
    int spacecode;
    int cc;
    int ept[10];
    int mirror;
    int vc;
    int prevsurf_type;
    char surf_mode;
} *in = NULL;

struct patch_verts {
    struct vertex *vp;
    point_t coord;
};


struct patch_faces
{
    struct faceuse *fu;
    fastf_t thick;
};


struct patches{
    fastf_t x;
    fastf_t y;
    fastf_t z;
    int flag;
    fastf_t radius;
    int mirror;
    fastf_t thick;
};


struct names{
    char ug[NAMESIZE+1];
    char lg[NAMESIZE+1];
    int eqlos;
    int matcode;
} *nm = NULL;

struct subtract_list{
    int outsolid;
    int insolid;
    int inmirror;
    struct subtract_list *next;
};


point_t pt[4];
fastf_t vertice[5][3];
fastf_t first[5][3];
point_t ce[4];
point_t Centroid;			/* object, description centroids */
unsigned char rgb[3];
int debug = 0;
float mmtin = 25.4;
double conv_mm2in;
fastf_t third = 0.333333333;

char cname[NAMESIZE+1];
char tname[NAMESIZE+1];
char surf[2];
char thick[3];
char space[2];

int numobj = 0;
int nflg = 1;
int aflg = 1;				/* use phantom armor */
int num_unions = 5;			/* number of unions per region */
char *title = "patch-g conversion";	/* database title */
char *top_level = "all";		/* top-level node name in the database */
int rev_norms = 0;			/* reverse normals for plate mode triangles */
int polysolid = 0;			/* convert triangle-facetted objects to polysolids */
int arb6 = 0;				/* flag: convert plate-mode objects to arb6s */

char *patchfile = NULL;
char *labelfile = NULL;
char *matfile = NULL;

struct patches *list = NULL;
fastf_t *XVAL = NULL;
fastf_t *YVAL = NULL;
fastf_t *ZVAL = NULL;

fastf_t *thicks = NULL;			/* array of unique plate thicknesses */
int nthicks;				/* number of unique plate thicknesses
					   for a single plate mode solid */
fastf_t *RADIUS = NULL;
fastf_t *thk = NULL;

int *mirror = NULL;

struct wmember head;			/* solids for current region */
struct wmember heada;			/* for component, regions on one side */
struct wmember headb;			/* for component, mirror regions */
struct wmember headc;			/* second level grouping ? */
struct wmember headd;			/* current thousand series group */
struct wmember heade;			/* group containing everything */
struct wmember headf;			/* check solids group */

struct bn_tol TOL;
int scratch_num;

struct rt_wdb *outfp;

#endif /* CONV_PATCH_PATCH_G_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
