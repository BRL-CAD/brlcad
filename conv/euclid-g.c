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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtlist.h"
#include "wdb.h"
#include "../librt/debug.h"

#define MAX_FACES_PER_REGION 8192
#define MAX_PTS_PER_FACE 8192

#define MAX(A, B) ((A) > (B) ? (A) : (B))

/* macro to determine if one bounding box in entirely within another
 * also returns true if the boxes are the same
 */
#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )


RT_EXTERN( fastf_t nmg_loop_plane_area , ( struct loopuse *lu , plane_t pl ) );
RT_EXTERN( struct faceuse *nmg_add_loop_to_face , (struct shell *s, struct faceuse *fu, struct vertex *verts[], int n, int dir ) );

extern int errno;

struct vlist {
	fastf_t		pt[3*MAX_PTS_PER_FACE];
	struct vertex	*vt[MAX_PTS_PER_FACE];
};

struct nmg_ptbl groups[11];

static int polysolids;
static char	usage[] = "Usage: %s [-i euclid_db] [-o brlcad_db] [-p]\n\t\t(-p indicates write as polysolids)\n ";

main(argc, argv)
int	argc;
char	*argv[];
{
	char		*bfile, *efile;
	FILE		*fpin, *fpout;
	register int	c;
	int i;

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

	for( i=0 ; i<11 ; i++ )
		nmg_tbl( &groups[i] , TBL_INIT , (long *)NULL );

	euclid_to_brlcad(fpin, fpout);

	fclose(fpin);
	fclose(fpout);
}

/*
 *	A d d _ N M G _ t o _ D b
 *
 *	Write the nmg to a brl-cad style data base.
 */
static void
add_nmg_to_db(fpout, m, reg_id)
FILE		*fpout;
struct model	*m;
int		reg_id;
{
	char	id[80], *rname, *sname;
	int gift_ident;
	int group_id;
	struct nmgregion *r;
	struct shell *s;
	struct rt_tol  tol;
	struct wmember head;

	RT_LIST_INIT( &head.l );

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

	gift_ident = reg_id % 100000;
	group_id = gift_ident/1000;
	if( group_id > 10 )
		group_id = 10;

	sprintf(rname, "%s.r", id);

	if( mk_addmember( sname, &head, WMOP_UNION ) == WMEMBER_NULL )
	{
		rt_log( "add_nmg_to_db: mk_addmember failed for solid %s\n" , sname );
		rt_bomb( "add_nmg_to_db: FAILED\n" );
	}

	if( mk_lrcomb( fpout, rname, &head, 1, (char *)NULL, (char *)NULL,
	    (unsigned char *)NULL, gift_ident, 0, 0, 100, 0 ) )
	{
		rt_log( "add_nmg_to_db: mk_rlcomb failed for region %s\n" , rname );
		rt_bomb( "add_nmg_to_db: FAILED\n" );
	}

	nmg_tbl( &groups[group_id] , TBL_INS , (long *)rname );

	rt_free( (char *)sname , "euclid-g: solid name" );
}

static void
build_groups( fpout )
FILE *fpout;
{
	int i,j;
	struct wmember head;
	struct wmember head_all;
	char group_name[18];

	RT_LIST_INIT( &head.l );
	RT_LIST_INIT( &head_all.l );

	for( i=0 ; i<11 ; i++ )
	{

		if( NMG_TBL_END( &groups[i] ) < 1 )
			continue;

		for( j=0 ; j<NMG_TBL_END( &groups[i] ) ; j++ )
		{
			char *region_name;

			region_name = (char *)NMG_TBL_GET( &groups[i] , j );
			if( mk_addmember( region_name , &head , WMOP_UNION ) == WMEMBER_NULL )
			{
				rt_log( "build_groups: mk_addmember failed for region %s\n" , region_name );
				rt_bomb( "build_groups: FAILED\n" );
			}
		}

		if( i < 10 )
			sprintf( group_name , "%dxxx_series" , i );
		else
			sprintf( group_name , "ids_over_9999" );

		j = mk_lfcomb( fpout , group_name , &head , 0 )
		if( j )
		{
			rt_log( "build_groups: mk_lcomb failed for group %s\n" , group_name );
			rt_bomb( "build_groups: mk_lcomb FAILED\n" );
		}

		if( mk_addmember( group_name , &head_all , WMOP_UNION ) == WMEMBER_NULL )
		{
			rt_log( "build_groups: mk_addmember failed for group %s\n" , group_name );
			rt_bomb( "build_groups: FAILED\n" );
		}
	}

	j = mk_lfcomb( fpout , "all" , &head_all , 0 )
	if( j )
		rt_bomb( "build_groups: mk_lcomb failed for group 'all'\n" );
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

	/* Build groups based on idents */
	build_groups( fpout );
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
	int	cur_id, face, facet_type, hole_face, i,
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
	for (i = 0; i < face; i++)
	{
		plane_t pl;
		fastf_t area;
		struct loopuse *lu;

		for( RT_LIST_FOR( lu , loopuse , &outfaceuses[i]->lu_hd ) )
		{
			area = nmg_loop_plane_area( lu , pl );
			if( area > 0.0 )
			{
				if( lu->orientation == OT_OPPOSITE )
					HREVERSE( pl , pl );
				nmg_face_g( outfaceuses[i] , pl );
				break;
			}
		}
	}

	/* Glue edges of outward pointing face uses together. */
	nmg_gluefaces(outfaceuses, face);

	/* Compute "geometry" for region and shell */
	nmg_region_a( r , &tol );

	/* fix the normals */
	s = RT_LIST_FIRST( shell , &r->s_hd );
	nmg_fix_normals( s, &tol );

	/* if the shell we just built has a void shell inside, nmg_fix_normals will
	 * point the normals of the void shell in the wrong direction. This section
	 * of code looks for such a situation and reverses the normals of the void shell
	 *
	 * first decompose the shell into maximally connected shells
	 */
	if( nmg_decompose_shell( s , &tol ) > 1 )
	{
		/* This shell has more than one part */
		struct shell *outer_shell=NULL;
		long *flags;

		flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "euclid-g: flags" );

		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct shell *s2;

			int is_outer=1;

			/* insure that bounding boxes are available */
			if( !s->sa_p )
				nmg_shell_a( s , &tol );

			/* Check if this shells contains all the others
			 * In TANKILL, there should only be one outer shell
			 */
			for( RT_LIST_FOR( s2 , shell , &r->s_hd ) )
			{
				if( !s2->sa_p )

					nmg_shell_a( s2 , &tol );

				if( !V3RPP1_IN_RPP2( s2->sa_p->min_pt , s2->sa_p->max_pt ,
						    s->sa_p->min_pt , s->sa_p->max_pt ) )
				{
					/* doesn't contain shell s2, so it's not an outer shell */
					is_outer = 0;
					break;
				}
			}
			if( is_outer )
			{
				outer_shell = s;
				break;
			}
		}
		if( !outer_shell )
		{
			rt_log( "euclid-g: Could not find outer shell for component code %d\n" , cur_id );
			outer_shell = RT_LIST_FIRST( shell , &r->s_hd );
		}

		/* reverse the normals for each void shell
		 * and merge back into one shell */
		s = RT_LIST_FIRST( shell , &r->s_hd );
		while( RT_LIST_NOT_HEAD( s , &r->s_hd ) )
		{
			struct faceuse *fu;
			struct shell *next_s;

			if( s == outer_shell )
			{
				s = RT_LIST_PNEXT( shell , s );
				continue;
			}

			next_s = RT_LIST_PNEXT( shell , s );
			fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
			nmg_reverse_face( fu );
			nmg_propagate_normals( fu , flags , &tol );
			nmg_js( outer_shell , s , &tol );
			s = next_s;
		}
		rt_free( (char *)flags , "euclid-g: flags" );
	}
	s = RT_LIST_FIRST( shell , &r->s_hd );

	if( nmg_check_closed_shell( s , &tol ) )
	{
		rt_log( "Warning: Region %d is not a closed surface\n" , reg_id );
		rt_log( "\tCreating new faces to close region\n" );
		nmg_close_shell( s , &tol );
		if( nmg_check_closed_shell( s , &tol ) )
			rt_bomb( "Cannot close shell\n" );
	}

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
