/*
 *			E U C L I D - G . C
 *
 *  Program to convert Euclid file into a BRL-CAD NMG object.
 *
 *  Authors -
 *	Michael Markowski
 *	John R. Anderson
 *  
 *  Source -
 *	The US Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"
#include "../librt/debug.h"

#define MAX_FACES_PER_REGION 8192
#define MAX_PTS_PER_FACE 8192

#define MAX(A, B) ((A) > (B) ? (A) : (B))

extern struct faceuse *nmg_add_loop_to_face();
extern int errno;

struct vlist {
	fastf_t		pt[3*MAX_PTS_PER_FACE];
	struct vertex	*vt[MAX_PTS_PER_FACE];
};

static int polysolids;
static char	usage[] = "Usage: %s [-i euclid_db] [-o brlcad_db] [-p]\n\t\t(-p indicates write as polysolids)\n ";

main(argc, argv)
int	argc;
char	*argv[];
{
	char		*bfile, *efile;
	FILE		*fpin, *fpout;
	register int	c;

	fpin = stdin;
	fpout = stdout;
	efile = NULL;
	bfile = NULL;
	polysolids = 0;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "i:o:p")) != EOF) {
		switch (c) {
		case 'i':
			efile = optarg;
			if ((fpin = fopen(efile, "r")) == NULL)
			{
				fprintf(stderr,	"%s: cannot open %s for reading\n",
					argv[0], efile);
				perror( argv[0] );
				exit(1);
			}
			break;
		case 'o':
			bfile = optarg;
			if ((fpout = fopen(bfile, "w")) == NULL) {
				fprintf(stderr,	"%s: cannot open %s for writing\n",
					argv[0], bfile);
				perror( argv[0] );
				exit(1);
			}
			break;
		case 'p':
			polysolids = 1;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	/* Output BRL-CAD database header.  No problem if more than one. */
	if( efile == NULL )
		mk_id( fpout , "Conversion from EUCLID" );
	else
		mk_id(fpout, efile);

	euclid_to_brlcad(fpin, fpout);

	fclose(fpin);
	fclose(fpout);
}

/*
 *	E u c l i d _ t o _ B r l C a d
 *
 *	Convert a Euclid data base into a BRL-CAD data base.  This might or
 *	might not be correct, but a Euclid data base of faceted objects is
 *	assumed to be an ascii file of records of the following form:
 *
 *		RID FT A? B? NPT
 *		I0 X0 Y0 Z0
 *		I1 X1 Y1 Z1
 *	    	...
 *		Inpt Xnpt Ynpt Znpt
 *		Ip A B C D
 *
 *	where RID is an integer closed region id number,
 *
 *		FT is the facet type with the following values:
 *		   0: simple facet (no holes).
 *		   1: this facet is a hole.
 *		   2: this is a surface facet which will be given holes.
 *
 *		A? B? are unknown variables.
 *
 *		NPT is the number of data point coordinates which will follow.
 *
 *		Ij is a data point index number.
 *
 *		Xi Yi Zi are data point coordinates in mm.
 *
 *		A, B, C, D are the facet's plane equation coefficients and
 *		<A B C> is an outward pointing surface normal.
 */
euclid_to_brlcad(fpin, fpout)
FILE	*fpin, *fpout;
{
	char	str[80];
	int	reg_id;

	/* skip first string in file (what is it??) */
	if( fscanf( fpin , "%s" , str ) == EOF )
		rt_bomb( "Failed on first attempt to read input" );

	/* Id of first region. */
	if (fscanf(fpin, "%d", &reg_id) != 1) {
		fprintf(stderr, "euclid_to_brlcad: no region id\n");
		exit(1);
	}

	/* Convert each region to an individual nmg. */
	do {
		fprintf( stderr , "Converting region %d...\n", reg_id);
		reg_id = cvt_euclid_region(fpin, fpout, reg_id);
	} while (reg_id != -1);
}

/*
 *	R e a d _ E u c l i d _ R e g i o n
 *
 *	Make a list of indices into global vertex coordinate array.
 *	This list represents the face under construction.
 */
int
cvt_euclid_region(fp, fpdb, reg_id)
FILE	*fp, *fpdb;
int	reg_id;
{
	int	cur_id, face, facet_type, fail, hole_face, i, j,
		lst[MAX_PTS_PER_FACE], np, nv;
	struct faceuse	*outfaceuses[MAX_PTS_PER_FACE];
	struct model	*m;	/* Input/output, nmg model. */
	struct nmgregion *r;
	struct rt_tol	tol;
	struct shell	*s;
	struct vertex	*vertlist[MAX_PTS_PER_FACE];
	struct vlist	vert;

	/* Copied from proc-db/nmgmodel.c */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.01;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 0.001;
	tol.para = 0.999;

	m = nmg_mm();		/* Make nmg model. */
	r = nmg_mrsv(m);	/* Make region, empty shell, vertex. */
	s = RT_LIST_FIRST(shell, &r->s_hd);

	nv = 0;			/* Initially no vertices for this region. */
	face = 0;		/* No faces either. */
	/* Grab all the faces for one region. */
	do {
		/* Get vertices for a single face. */
		facet_type = read_euclid_face(lst, &np, fp, &vert, &nv);

		if( np > 2 )
		{

			/* Make face out of vertices in lst. */
			for (i = 0; i < np; i++)
				vertlist[i] = vert.vt[lst[i]];

			switch(facet_type) {
			case 0:	/* Simple facet (no holes). */
				outfaceuses[face] = nmg_cface(s, vertlist, np);
				face++;
				break;

			case 1:	/* Facet is a hole. */
				nmg_add_loop_to_face(s, outfaceuses[hole_face],
					vertlist, np, OT_OPPOSITE);
				break;

			case 2:	/* Facet will be given at least one hole. */
				outfaceuses[face] = nmg_cface(s, vertlist, np);
				hole_face = face;
				face++;
				break;

			default:
				fprintf(stderr, "cvt_euclid_face: in region %d, face %d is an unknown facet type\n", reg_id, face);
				break;
			}

			/* Save (possibly) newly created vertex structs. */
			for (i = 0; i < np; i++)
				vert.vt[lst[i]] = vertlist[i];
		}

		/* Get next face's region id. */
		if (fscanf(fp, "%d", &cur_id) != 1)
			cur_id = -1;
	} while (reg_id == cur_id);

	/* Associate the vertex geometry, ccw. */
	for (i = 0; i < nv; i++)
		if (vert.vt[i])
			nmg_vertex_gv(vert.vt[i], &vert.pt[3*i]);

	/* Associate the face geometry. */
	for (i = 0, fail = 0; i < face; i++)
		if (nmg_fu_planeeqn(outfaceuses[i], &tol) < 0) {
			fprintf(stderr, "Warning: in region %d, face %d is degenerate.\n", reg_id, i);
		}

	/* Glue edges of outward pointing face uses together. */
	nmg_gluefaces(outfaceuses, face);

	/* Compute "geometry" for region and shell */
	nmg_region_a( r , &tol );

	/* fix the normals */
	s = RT_LIST_FIRST( shell , &r->s_hd );
	nmg_fix_normals( s );
	
	if( nmg_ck_closed_surf( s , &tol ) )
		fprintf( stderr , "Warning: Region %d is not a closed surface\n" , reg_id );

	add_nmg_to_db(fpdb, m, reg_id);		/* Put region in db. */
	nmg_km(m);				/* Safe to kill model now. */

	return(cur_id);
}

/*
 *	R e a d _ E u c l i d _ F a c e
 *
 *	Read in vertices from a Euclid facet file and store them in an
 *	array of nmg vertex structures.  Then make a list of indices of these
 *	vertices in the vertex array.  This list represents the face under
 *	construction.
 *
 *	XXX Fix this!  Only allows set max of points and assumes
 *	no errors during reading...
 */
int
read_euclid_face(lst, ni, fp, vert, nv)
FILE	*fp;
int	*lst, *ni, *nv;
struct vlist	*vert;
{
	fastf_t	num_points, x, y, z, a, b, c, d;
	int	i, j, k, facet_type;
	vect_t	N;

	/* Description of record. */
	fscanf(fp, "%d %*lf %*lf %lf", &facet_type, &num_points);
	*ni = (int)num_points;
	
	/* Read in data points. */
	for (i = 0; i < *ni; i++) {
		fscanf(fp, "%*d %lf %lf %lf", &x, &y, &z);

		if ((lst[i] = find_vert(vert, *nv, x, y, z)) == -1)
			lst[i] = store_vert(vert, nv, x, y, z);
	}

	/* Read in plane equation. */
	fscanf(fp, "%*d %lf %lf %lf %lf", &a, &b, &c, &d);

	/* Remove duplicate points (XXX this should be improved). */
	for (i = 0; i < *ni; i++)
		for (j = i+1; j < *ni; j++)
			if (lst[i] == lst[j]) {
				int increment;
				
				if( j == i+1 || (i == 0 && j == (*ni-1))  )
					increment = 1;
				else if( j == i+2 )
				{
					j = i+1;
					increment = 2;
				}
				else
				{
					fprintf( stderr , "warning: removing distant duplicates\n" );
					increment = 1;
				}

				for (k = j ; k < *ni-increment; k++)
					lst[k] = lst[k + increment];
				*ni -= increment;
			}

	return(facet_type);
}

/*
 *	F i n d _ V e r t
 *
 *	Try to locate a geometric point in the list of vertices.  If found,
 *	return the index number within the vertex array, otherwise return
 *	a -1.
 */
int
find_vert(vert, nv, x, y, z)
struct vlist	*vert;
int		nv;
fastf_t		x, y, z;
{
	int	found, i;

/* XXX What's good here? */
#define ZERO_TOL 0.0001

	found = 0;
	for (i = 0; i < nv; i++) {
		if (NEAR_ZERO(x - vert->pt[3*i+0], ZERO_TOL)
			&& NEAR_ZERO(y - vert->pt[3*i+1], ZERO_TOL)
			&& NEAR_ZERO(z - vert->pt[3*i+2], ZERO_TOL))
		{
			found = 1;
			break;
		}
	}
	if (!found)
		return( -1 );
	else
		return( i );
}

/*
 *	S t o r e _ V e r t
 *
 *	Store vertex in an array of vertices.
 */
int
store_vert(vert, nv, x, y, z)
struct vlist	*vert;
int	*nv;
fastf_t	x, y, z;
{
	vert->pt[*nv*3+0] = x;
	vert->pt[*nv*3+1] = y;
	vert->pt[*nv*3+2] = z;
	vert->vt[*nv] = (struct vertex *)NULL;

	++*nv;

	if (*nv > MAX_PTS_PER_FACE) {
		fprintf(stderr,
		"read_euclid_face: no more vertex room\n");
		exit(1);
	}

	return(*nv - 1);
}

/*
 *	A d d _ N M G _ t o _ D b
 *
 *	Write the nmg to a brl-cad style data base.
 */
int
add_nmg_to_db(fpout, m, reg_id)
FILE		*fpout;
struct model	*m;
int		reg_id;
{
	char	id[80], *rname, *sname;
	struct nmgregion *r;
	struct shell *s;
	struct rt_tol  tol;

        /* XXX These need to be improved */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	r = RT_LIST_FIRST( nmgregion , &m->r_hd );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	sprintf(id, "%d", reg_id);
	rname = malloc(sizeof(id) + 3);	/* Region name. */
	sname = malloc(sizeof(id) + 3);	/* Solid name. */

	sprintf(sname, "%s.s", id);
	if( polysolids )
		write_shell_as_polysolid( fpout , sname , s );
	else
		mk_nmg(fpout, sname,  m);		/* Make nmg object. */

	sprintf(rname, "%s.r", id);
	mk_comb1(fpout, rname, sname, 1);	/* Put object in a region. */
	
	rt_free( (char *)rname , "euclid-g: region name" );
	rt_free( (char *)sname , "euclid-g: solid name" );
}
