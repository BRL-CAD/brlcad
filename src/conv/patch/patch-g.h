/*                       P A T C H - G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file patch-g.h
 *
 */

struct input {

	fastf_t x, y, z;
	fastf_t	rsurf_thick;
	int	surf_type;
	int	surf_thick;
	int	spacecode;
	int	cc;
	int  	ept[10];
	int	mirror;
	int	vc;

	int	prevsurf_type;
	char	surf_mode;

} in[10000];

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

	fastf_t x,y,z;
	int flag;
	fastf_t radius;
	int mirror;
	fastf_t thick;

};

struct names{

	char	ug[16];
	char	lg[16];
	int	eqlos,
		matcode;

} nm[9999];

struct subtract_list{

	int			outsolid,
				insolid,
				inmirror;
	struct subtract_list	*next;
};

point_t		pt[4];
fastf_t		vertice[5][3];
fastf_t		first[5][3];
fastf_t		normal[5][3];
point_t		ce[4];
point_t		centroid,Centroid;	/* object, description centroids */
unsigned char	rgb[3];
int debug = 0;
float mmtin = 25.4;
double conv_mm2in;
fastf_t third = 0.333333333;

/* char  name[17];	*/
char  cname[17];
char  tname[17];
char  surf[2];
char thick[3];
char  space[2];

int numobj = 0;
int nflg = 1;
int aflg = 1;		/* use phantom armor */
int num_unions = 5;	/* number of unions per region */
char *title = "Untitled MGED database";	/* database title */
char *top_level = "all"; /* top-level node name in the database */
int rev_norms = 0;	/* reverse normals for plate mode triangles */
int polysolid = 0;	/* convert triangle-facetted objects to polysolids */
int arb6 = 0;		/* flag: convert plate-mode objects to arb6s */

mat_t	m;
char *patchfile;
char *labelfile=NULL;
char *matfile;

#define MAX_THICKNESSES		500	/* Maximum number of different thicknesses
					   for a single plate mode solid */
fastf_t thicks[MAX_THICKNESSES];	/* array of unique plate thicknesses */
int nthicks;				/* number of unique plate thicknesses
					   for a single plate mode solid */

struct patches list[15000];
fastf_t x[1500],y[1500],z[1500];
int mirror[1500];
fastf_t radius[1500];
fastf_t thk[1500];

struct wmember head;			/* solids for current region */
struct wmember heada;			/* for component,regions on one side */
struct wmember headb;			/* for component,mirror regions */
struct wmember headc;			/* second level grouping ? */
struct wmember headd;			/* current thousand series group */
struct wmember heade;			/* group containing everything */
struct wmember headf;			/* check solids group */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
