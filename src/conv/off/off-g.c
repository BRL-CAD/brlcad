/*                         O F F - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file off-g.c
 *
 *  Program to convert from Digital Equipment Corporation's OFF
 *  (Object File Format) to BRL-CAD NMG objects.
 *  Inspired by Mike Markowski's jack-g Jack to NMG converter.
 *
 *  Author -
 *	Glenn Edward Durfee
 *
 *  Source -
 *	The U.S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

static struct bn_tol tol;

/*
 *         R E A D _ F A C E S
 *
 * Reads the geometry from the the geometry file and creates the appropriate
 *  vertices and faces.
 */

int read_faces(struct model *m, FILE *fgeom)
{
	int 		   nverts, nfaces, nedges;
	register int 	   i, j, fail=0;
	fastf_t 	  *pts;
	struct vertex 	 **verts;
	struct faceuse 	 **outfaceuses;
	struct nmgregion  *r;
	struct shell 	  *s;

		/* Get numbers of vertices and faces, and grab the appropriate amount of memory */
	if (fscanf(fgeom, "%d %d %d", &nverts, &nfaces, &nedges) != 3)
		rt_bomb("Cannot read number of vertices, faces, edges.\n");

	pts = (fastf_t *) bu_malloc(sizeof(fastf_t) * 3 * nverts, "points list");
	verts = (struct vertex **) bu_malloc(sizeof(struct vertex *) * nverts, "vertices");
	outfaceuses = (struct faceuse **) bu_malloc(sizeof(struct faceuse *) * nfaces, "faceuses");

		/* Read in vertex geometry, store in geometry list */
	for (i = 0; i < nverts; i++) {
		if (fscanf(fgeom, "%lf %lf %lf", &pts[3*i], &pts[3*i+1], &pts[3*i+2]) != 3)
			rt_bomb("Not enough data points in geometry file.\n");

		verts[i] = (struct vertex *) 0;
		fscanf(fgeom, "%*[^\n]");
	}

	r = nmg_mrsv(m);		/* Make region, empty shell, vertex. */
	s = BU_LIST_FIRST(shell, &r->s_hd);


	for (i = 0; i < nfaces; i++) {		/* Read in each of the faces */
		struct vertex **vlist;
		int *pinds;

		if (fscanf(fgeom, "%d", &nedges) != 1) {
			fprintf(stderr, "Not enough faces in geometry file.\n");
			exit(1);
		}
					/* Grab memory for list for this face. */
		vlist = (struct vertex **) bu_malloc(sizeof(struct vertex *) * nedges, "vertex list");
		pinds = (int *) bu_malloc(sizeof(int) * nedges, "point indicies");

		for (j = 0; j < nedges; j++) {			/* Read list of point indicies. */
			if (fscanf(fgeom, "%d", &pinds[j]) != 1) {
				fprintf(stderr, "Not enough points on face.\n");
				exit(1);
			}
			vlist[j] = verts[pinds[j]-1];
		}

		outfaceuses[i] = nmg_cface(s, vlist, nedges);	/* Create face. */
		NMG_CK_FACEUSE(outfaceuses[i]);

		for (j = 0; j < nedges; j++)		/* Save (possibly) newly created vertex structs. */
			verts[pinds[j]-1] = vlist[j];

		fscanf(fgeom, "%*[^\n]");

		bu_free((char *)vlist, "vertext list");
		bu_free((char *)pinds, "point indicies");
	}

	for (i = 0; i < nverts; i++)
		if (verts[i] != 0)
			nmg_vertex_gv(verts[i], &pts[3*i]);
		else
			fprintf(stderr, "Warning: vertex %d unused.\n", i+1);

	for (i = 0; i < nfaces; i++) {
		plane_t pl;

		fprintf(stderr, "planeeqning face %d.\n", i);
		if( nmg_loop_plane_area( BU_LIST_FIRST( loopuse , &outfaceuses[i]->lu_hd ) , pl ) < 0.0 )
			fail = 1;
		else
			nmg_face_g( outfaceuses[i] , pl );

	}

	if (fail) return (-1);

	nmg_gluefaces(outfaceuses, nfaces, &tol);
	nmg_region_a(r, &tol);

	bu_free((char *)pts, "points list");
	return (0);
}


int off2nmg(FILE *fpin, struct rt_wdb *fpout)
{
	char title[64], geom_fname[64];
	char rname[67], sname[67];
	char buf[200], buf2[200];

	FILE *fgeom;
	struct model *m;

	title[0] = geom_fname[0] = '\0';

	fgets(buf, sizeof(buf), fpin);
	while (!feof(fpin)) {				/* Retrieve the important data */
		if (sscanf(buf, "name %[^\n]s", buf2) > 0)
			strncpy(title, buf2, sizeof(title));
/*		if (sscanf(buf, "author %[^\n]s", buf2) > 0)
			strncpy(author, buf2, sizeof(author));
*/		if (sscanf(buf, "geometry %[^\n]s", buf2) > 0) {
			char dtype[40], format[40];
			if (sscanf(buf2, "%s %s %s", dtype, format, geom_fname) != 3)
				rt_bomb("Incomplete geometry field in input file.");
			if (strcmp(dtype, "indexed_poly") != 0)
				rt_bomb("Unknown geometry data type. Must be \"indexed_poly\".");
		}
		fgets(buf, sizeof(buf), fpin);
	}

	if (strlen(title) < (unsigned)1)
		fprintf(stderr, "Warning: no title\n");

	if (strlen(geom_fname) < (unsigned)1)
		rt_bomb("Error: no geometry filename.");

	if ((fgeom = fopen(geom_fname, "r")) == NULL) {
		fprintf(stderr, "off2nmg: cannot open %s (geometry description) for reading\n",
			geom_fname);
		exit(1);
	}

	m = nmg_mm();
	read_faces(m, fgeom);
	fclose(fgeom);

	strcpy(sname, "s.");	strcat(sname, title);
	strcpy(rname, "r.");	strcat(rname, title);

	mk_id(fpout, title);
	mk_nmg(fpout, sname, m);
	mk_comb1(fpout, rname, sname, 1);

	nmg_km(m);
	return (0);
}


int main(int argc, char **argv)
{
	FILE *fpin;
	struct rt_wdb *fpout;

	tol.magic = BN_TOL_MAGIC;	/* Copied from proc-db/nmgmodel.c */
	tol.dist = 0.01;
	tol.dist_sq = 0.01 * 0.01;
	tol.perp = 0.001;
	tol.para = 0.999;

	/* Get filenames and open the files. */
	if (argc != 3)  {
		fprintf(stderr, "Usage: off-g file.off file.g\n");
		return 2;
	}
	if ((fpin = fopen(argv[1], "rt")) == NULL) {
		fprintf(stderr, "%s: cannot open %s for reading\n",
			argv[0], argv[1]);
		return (1);
	}
	if ((fpout = wdb_fopen(argv[2])) == NULL) {
		fprintf(stderr, "%s: cannot create %s\n",
			argv[0], argv[2]);
		return (1);
	}


	off2nmg(fpin, fpout);

	fclose(fpin);
	wdb_close(fpout);

	return (0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
