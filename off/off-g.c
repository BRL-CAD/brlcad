/*
 *		O F F - G . C
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
 *
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited
 *
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"

static struct bn_tol tol;

/*
 *         R E A D _ F A C E S 
 *
 * Reads the geometry from the the geometry file and creates the appropriate
 *  vertices and faces.
 */

int read_faces(m, fgeom)
struct model *m;
FILE *fgeom;	
{
	int 		   nverts, nfaces, nedges;
	register int 	   i, j, fail;
	fastf_t 	  *pts;		
	struct vertex 	 **verts;
	struct faceuse 	 **outfaceuses;
	struct nmgregion  *r;
	struct shell 	  *s;

		/* Get numbers of vertices and faces, and grab the appropriate amount of memory */
	if (fscanf(fgeom, "%d %d %d", &nverts, &nfaces, &nedges) != 3)
		rt_bomb("Cannot read number of vertices, faces, edges.\n");

	pts = (fastf_t *) rt_malloc(sizeof(fastf_t) * 3 * nverts, "points list");
	verts = (struct vertex **) rt_malloc(sizeof(struct vertex *) * nverts, "vertices");
	outfaceuses = (struct faceuse **) rt_malloc(sizeof(struct faceuse *) * nfaces, "faceuses");

		/* Read in vertex geometry, store in geometry list */		
	for (i = 0; i < nverts; i++) {
		if (fscanf(fgeom, "%lf %lf %lf", &pts[3*i], &pts[3*i+1], &pts[3*i+2]) != 3)
			rt_bomb("Not enough data points in geometry file.\n");

		verts[i] = (struct vertex *) 0;
		fscanf(fgeom, "%*[^\n]");
	}

	r = nmg_mrsv(m);		/* Make region, empty shell, vertex. */
	s = RT_LIST_FIRST(shell, &r->s_hd);


	for (i = 0; i < nfaces; i++) {		/* Read in each of the faces */
		struct vertex **vlist;
		int *pinds;

		if (fscanf(fgeom, "%d", &nedges) != 1) {
			fprintf(stderr, "Not enough faces in geometry file.\n");
			exit(1);
		}
					/* Grab memory for list for this face. */		
		vlist = (struct vertex **) rt_malloc(sizeof(struct vertex *) * nedges, "vertex list");
		pinds = (int *) rt_malloc(sizeof(int) * nedges, "point indicies");

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

		rt_free((char *)vlist, "vertext list");
		rt_free((char *)pinds, "point indicies");
	}

	for (i = 0; i < nverts; i++)
		if (verts[i] != 0)
			nmg_vertex_gv(verts[i], &pts[3*i]);
		else
			fprintf(stderr, "Warning: vertex %d unused.\n", i+1);

	for (i = 0; i < nfaces; i++) {
		plane_t pl;

		fprintf(stderr, "planeeqning face %d.\n", i);
		if( nmg_loop_plane_area( RT_LIST_FIRST( loopuse , &outfaceuses[i]->lu_hd ) , pl ) < 0.0 )
			fail = 1;
		else
			nmg_face_g( outfaceuses[i] , pl );

	}

	if (fail) return (-1);

	nmg_gluefaces(outfaceuses, nfaces, &tol);
	nmg_region_a(r, &tol);

	rt_free((char *)pts, "points list");
	return (0);
}
		



int off2nmg(fpin, fpout)
FILE *fpin, *fpout;
{
	char title[64], author[64], geom_fname[64];
	char rname[67], sname[67];
	char buf[200], buf2[200];

	FILE *fgeom;
	struct model *m;

	title[0] = author[0] = geom_fname[0] = '\0';

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
		fprintf(stderr, "%s: cannot open %s (geometry description) for reading\n", geom_fname);
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
		

int main(argc, argv)
int argc;
char **argv;
{
	FILE *fpin, *fpout;
	char title[64], buf[200], *bp;

	tol.magic = RT_TOL_MAGIC;	/* Copied from proc-db/nmgmodel.c */
	tol.dist = 0.01;
	tol.dist_sq = 0.01 * 0.01;
	tol.perp = 0.001;
	tol.para = 0.999;

	/* Get filenames and open the files. */
	if (argc < 2) {
		fpin = stdin;
		fpout = stdout;
	} else if (argc < 3) {
		if ((fpin = fopen(argv[1], "rt")) == NULL) {
			fprintf(stderr, "%s: cannot open %s for reading\n",
				argv[0], argv[1]);
			return (1);
		}
		fpout = stdout;
	} else if (argc < 4) {
		if ((fpin = fopen(argv[1], "rt")) == NULL) {
			fprintf(stderr, "%s: cannot open %s for reading\n",
				argv[0], argv[1]);
			return (1);
		}
		if ((fpout = fopen(argv[2], "a")) == NULL) {
			fprintf(stderr, "%s: cannot open %s for appending\n",
				argv[0], argv[2]);
			return (1);
		}
	}

				
	off2nmg(fpin, fpout);

	fclose(fpin);
	fclose(fpout);

	return (0);
}