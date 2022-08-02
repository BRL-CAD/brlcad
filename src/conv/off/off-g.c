/*                         O F F - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

static struct bn_tol tol;

/*
 * Reads the geometry from the geometry file and creates the appropriate
 *  vertices and faces.
 */

int read_faces(struct model *m, FILE *fgeom)
{
    int nverts = -1, nfaces = -1, nedges = -1;
    int i, j, fail=0;
    fastf_t *pts;
    struct vertex 	 **verts;
    struct faceuse 	 **outfaceuses;
    struct nmgregion  *r;
    struct shell 	  *s;
    size_t ret;

    /* Get numbers of vertices and faces, and grab the appropriate amount of memory */
    if (fscanf(fgeom, "%d %d %d", &nverts, &nfaces, &nedges) != 3)
	bu_exit(1, "Cannot read number of vertices, faces, edges.\n");

    if (nverts < 1 || nverts > INT_MAX - 1)
	bu_exit(1, "Invalid vertex count: %d\n", nverts);

    if (nfaces < 1 || nfaces > INT_MAX - 1)
	bu_exit(1, "Invalid face count: %d\n", nfaces);


    pts = (fastf_t *) bu_malloc(sizeof(fastf_t) * 3 * nverts, "points list");
    verts = (struct vertex **) bu_malloc(sizeof(struct vertex *) * nverts, "vertices");
    outfaceuses = (struct faceuse **) bu_malloc(sizeof(struct faceuse *) * nfaces, "faceuses");

    /* Read in vertex geometry, store in geometry list */
    for (i = 0; i < nverts; i++) {
	double scan[3];
	if (fscanf(fgeom, "%lf %lf %lf", &scan[0], &scan[1], &scan[2]) != 3) {
	    bu_exit(1, "Not enough data points in geometry file.\n");
	}
	pts[3*i] = scan[0];
	pts[3*i+1] = scan[1];
	pts[3*i+2] = scan[2];

	verts[i] = (struct vertex *) 0;
	ret = fscanf(fgeom, "%*[^\n]");
	if (ret > 0)
	    bu_log("unknown parsing error\n");
    }

    r = nmg_mrsv(m);		/* Make region, empty shell, vertex. */
    s = BU_LIST_FIRST(shell, &r->s_hd);


    for (i = 0; i < nfaces; i++) {
	/* Read in each of the faces */
	struct vertex **vlist;
	int *pinds;

	if (fscanf(fgeom, "%d", &nedges) != 1) {
	    bu_exit(1, "Not enough faces in geometry file.\n");
	}
	/* Grab memory for list for this face. */
	vlist = (struct vertex **) bu_malloc(sizeof(struct vertex *) * nedges, "vertex list");
	pinds = (int *) bu_malloc(sizeof(int) * nedges, "point indices");

	for (j = 0; j < nedges; j++) {
	    /* Read list of point indices. */
	    if (fscanf(fgeom, "%d", &pinds[j]) != 1) {
		bu_exit(1, "Not enough points on face.\n");
	    }
	    vlist[j] = verts[pinds[j]-1];
	}

	outfaceuses[i] = nmg_cface(s, vlist, nedges);	/* Create face. */
	NMG_CK_FACEUSE(outfaceuses[i]);

	for (j = 0; j < nedges; j++)		/* Save (possibly) newly created vertex structs. */
	    verts[pinds[j]-1] = vlist[j];

	ret = fscanf(fgeom, "%*[^\n]");
	if (ret > 0)
	    bu_log("unknown parsing error\n");

	bu_free((char *)vlist, "vertext list");
	bu_free((char *)pinds, "point indices");
    }

    for (i = 0; i < nverts; i++)
	if (verts[i] != 0)
	    nmg_vertex_gv(verts[i], &pts[3*i]);
	else
	    fprintf(stderr, "Warning: vertex %d unused.\n", i+1);

    for (i = 0; i < nfaces; i++) {
	plane_t pl;

	fprintf(stderr, "planeeqning face %d.\n", i);
	if ( nmg_loop_plane_area( BU_LIST_FIRST( loopuse, &outfaceuses[i]->lu_hd ), pl ) < 0.0 )
	    fail = 1;
	else
	    nmg_face_g( outfaceuses[i], pl );

    }

    if (fail) return -1;

    nmg_gluefaces(outfaceuses, nfaces, &RTG.rtg_vlfree, &tol);
    nmg_region_a(r, &tol);

    bu_free((char *)pts, "points list");
    return 0;
}


int off2nmg(FILE *fpin, struct rt_wdb *fpout)
{
#define SZ 63
    char title[SZ+1] = {0};
    char geom_fname[SZ+1] = {0};
    char rname[SZ+1] = {0};
    char sname[SZ+1] = {0};

#define BUF_SZ 255
    char buf[BUF_SZ+1] = {0};
    char buf2[BUF_SZ+1] = {0};

    FILE *fgeom;
    struct model *m;

    bu_fgets(buf, sizeof(buf), fpin);
    while (!feof(fpin)) {
	/* Retrieve the important data */
	if (sscanf(buf, "name %[^\n]s", buf2) > 0)
	    bu_strlcpy(title, buf2, sizeof(title));

	if (sscanf(buf, "geometry %" CPP_XSTR(BUF_SZ) "[^\n]s", buf2) > 0) {
#define FMT_SZ 40
	    char dtype[FMT_SZ+1] = {0};
	    char format[FMT_SZ+1] = {0};

	    if (sscanf(buf2, CPP_SCAN(FMT_SZ) CPP_SCAN(FMT_SZ) CPP_SCAN(SZ), dtype, format, geom_fname) != 3)
		bu_exit(1, "Incomplete geometry field in input file.");
	    if (!BU_STR_EQUAL(dtype, "indexed_poly"))
		bu_exit(1, "Unknown geometry data type. Must be \"indexed_poly\".");
	}
	bu_fgets(buf, sizeof(buf), fpin);
    }

    if (strlen(title) < (unsigned)1)
	fprintf(stderr, "Warning: no title\n");

    if (strlen(geom_fname) < (unsigned)1)
	bu_exit(1, "ERROR: no geometry filename given");

    if ((fgeom = fopen(geom_fname, "rb")) == NULL) {
	bu_exit(1, "off2nmg: cannot open %s (geometry description) for reading\n",
		geom_fname);
    }

    m = nmg_mm();
    read_faces(m, fgeom);
    fclose(fgeom);

    snprintf(sname, sizeof(sname)-1, "s.%s", title);
    snprintf(rname, sizeof(sname)-1, "r.%s", title);

    mk_id(fpout, title);
    mk_nmg(fpout, sname, m);
    mk_comb1(fpout, rname, sname, 1);

    nmg_km(m);
    return 0;
}


int main(int argc, char **argv)
{
    FILE *fpin;
    struct rt_wdb *fpout;

    bu_setprogname(argv[0]);

    tol.magic = BN_TOL_MAGIC;	/* Copied from proc-db/nmgmodel.c */
    tol.dist = 0.01;
    tol.dist_sq = 0.01 * 0.01;
    tol.perp = 0.001;
    tol.para = 0.999;

    /* Get filenames and open the files. */
    if (argc != 3)  {
	bu_exit(2, "Usage: off-g file.off file.g\n");
    }
    if ((fpin = fopen(argv[1], "r")) == NULL) {
	bu_exit(1, "%s: cannot open %s for reading\n",
		argv[0], argv[1]);
    }
    if ((fpout = wdb_fopen(argv[2])) == NULL) {
	bu_exit(1, "%s: cannot create %s\n",
		argv[0], argv[2]);
    }


    off2nmg(fpin, fpout);

    fclose(fpin);
    wdb_close(fpout);

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
