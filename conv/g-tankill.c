/*
 *			G - T A N K I L L . C
 *
 *  Program to convert a BRL-CAD model (in a .g file) to a TANKILL facetted model
 *  by calling on the NMG booleans.
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
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
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

RT_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( struct face *nmg_find_top_face , (struct shell *s, int *dir, long *flags ));

static char	usage[] = "Usage:\n\
	%s [-v] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-s surroundings_code] [-i idents_output_file] [-o out_file] brlcad_db.g object(s)\n\
		v - verbose\n\
		x - librt debug level\n\
		X - NMG debug level\n\
		a - absolute tolerance for tesselation\n\
		r - relative tolerance for tesselation\n\
		n - surface normal tolerance for tesselation\n\
		s - surroundings code to use in tankill file\n\
		i - assign new idents sequentially and output ident list\n\
		o - TANKILL output file\n";

static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;		/* Number of processors */
static int	surr_code = 1000;	/* Surroundings code */
static int	curr_id;		/* Current region ident code */
static int	id_counter;		/* Ident counter */
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static char	*id_file = NULL;	/* Output ident file */
static FILE	*fp_id = NULL;		/* Output ident file pointer */
static int	*idents;		/* Array of region ident numbers */
static int	ident_count=0;		/* Number of idents in above array */
static int	ident_length=0;		/* Length of idents array */
#define		IDENT_BLOCK	256	/* Number of idents array slots to allocate */

static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct rt_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;
static int	regions_written = 0;

/* macro to determine if one bounding box is within another */
#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )

void
insert_id( id )
int id;
{
	int i;

	for( i=0 ; i<ident_count ; i++ )
	{
		if( idents[i] == id )
			return;
	}

	if( ident_count == ident_length )
	{
		idents = (int *)rt_realloc( (char *)idents , (ident_length + IDENT_BLOCK)*sizeof( int ) , "insert_id: idents" );
		ident_length += IDENT_BLOCK;
	}

	idents[ident_count] = id;
	ident_count++;
}

/* routine used in tree walker to select regions with the current ident number */
static int
select_region( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	if( tsp->ts_regionid == curr_id )
		return( 0 );
	else
		return( -1 );
}

/* routine used in tree walker to collect region ident numbers */
static int
get_reg_id( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	insert_id( tsp->ts_regionid );
	return( -1 );
}

/* stubs to warn of the unexpected */
static union tree *
region_stub( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	struct directory *fp_name;	/* name from pathp */

	fp_name = DB_FULL_PATH_CUR_DIR( pathp );
	rt_log( "region stub called (for object %s), this shouldn't happen\n" , fp_name->d_namep );
	return( (union tree *)NULL );
}

static union tree *
leaf_stub( tsp, pathp, ep, id )
struct db_tree_state    *tsp;
struct db_full_path     *pathp;
struct rt_external      *ep;
int                     id;
{
	struct directory *fp_name;	/* name from pathp */

	fp_name = DB_FULL_PATH_CUR_DIR( pathp );
	rt_log( "Only regions may be converted to TANKILL format\n\t%s is not a region and will be ignored\n" , fp_name->d_namep );
	return( (union tree *)NULL );
}

/* Routine to identify external/void shells
 *	Marks external shells with a +1 in the flags array
 *	Marks void shells with a -1 in the flags array
 */
static void
nmg_find_void_shells( r , flags , ttol )
CONST struct nmgregion *r;
CONST struct rt_tol *ttol;
long *flags;
{
	struct model *m;
	struct shell *s;

	NMG_CK_REGION( r );

	m = r->m_p;
	NMG_CK_MODEL( m );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct face *f;
		struct faceuse *fu;
		vect_t normal;
		int dir;

		f = nmg_find_top_face( s, &dir, flags );
		fu = f->fu_p;
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;
		if( fu->orientation != OT_SAME )
			rt_bomb( "nmg_find_void_shells: Neither faceuse nor mate have OT_SAME orient\n" );

		NMG_GET_FU_NORMAL( normal , fu );
		if( normal[dir] > 0.0 )
		{
			NMG_INDEX_ASSIGN( flags , s , 1 )	/* external shell */
		}
		else
		{
			NMG_INDEX_ASSIGN( flags , s , -1 )	/* void shell */
		}
	}
}

static void
nmg_assoc_void_shells( r , flags , ttol )
struct nmgregion *r;
long *flags;
CONST struct rt_tol *ttol;
{
	struct model *m;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int ext_shell_id=1;

	NMG_CK_REGION( r );

	m = r->m_p;

	if( !r->ra_p )
		nmg_region_a( r , ttol );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{

		NMG_CK_SHELL( s );
		if( !s->sa_p )
			nmg_shell_a( s , ttol );

		if( NMG_INDEX_GET( flags , s ) == 1 )
		{
			struct shell *void_s;
			struct face *ext_f;
			int dir;

			/* identify this external shell */
			NMG_INDEX_ASSIGN( flags , s , ++ext_shell_id );

			ext_f = nmg_find_top_face( s, &dir, flags );

			/* found an external shell, look for voids */
			for( RT_LIST_FOR( void_s , shell , &r->s_hd ) )
			{
				int wrong_void=0;

				if( void_s == s )
					continue;

				NMG_CK_SHELL( s );
				if( !s->sa_p )
					nmg_shell_a( s , ttol );

				if( NMG_INDEX_GET( flags , void_s ) == (-1) )
				{
					struct shell *test_s;
					int breakout=0;
					int not_in_this_shell=0;

					/* this is a void shell
					 * but does it belong with external shell s */
					if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , s->sa_p->min_pt , s->sa_p->max_pt ) )
						continue;

					for( RT_LIST_FOR( fu , faceuse , &void_s->fu_hd ) )
					{
						for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
						{
							if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
								continue;
							for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
							{
								int class;

								class = nmg_class_pt_s( eu->vu_p->v_p->vg_p->coord , s, 1 , ttol );

								if( class == NMG_CLASS_AoutB )
								{
									breakout = 1;
									not_in_this_shell = 1;
									break;
								}
								else if( class == NMG_CLASS_AinB )
								{
									breakout = 1;
									break;
								}
							}
							if( breakout )
								break;
						}
						if( breakout )
							break;
					}

					if( not_in_this_shell )
						continue;

					/* Make sure there are no other external shells between these two */
					for( RT_LIST_FOR( test_s , shell , &r->s_hd ) )
					{
						if( NMG_INDEX_GET( flags , test_s ) > 1 )
						{

							if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , test_s->sa_p->min_pt , test_s->sa_p->max_pt ) )
								continue;

							/* XXXX check for wrong_void, set to one if wrong */
						}
					}
					if( wrong_void )
						continue;

					/* This void shell belongs with shell s
					 * mark it with the negative of external shells flag id */
					NMG_INDEX_ASSIGN( flags , void_s , (-NMG_INDEX_GET( flags , s )) );
				}
			}
		}
	}
}

/*	Routine to write an nmgregion in the TANKILL format */
static void
Write_tankill_region( r , tsp , pathp )
struct nmgregion *r;
struct db_tree_state *tsp;
struct db_full_path *pathp;
{
	struct model *m;
	struct shell *s;
	struct nmg_ptbl shells;		/* list of shells to be decomposed */
	struct nmg_ptbl vertices;	/* vertex list in TANKILL order */
	long *flags;			/* array to insure that no loops are missed */
	int i;

	NMG_CK_REGION( r );
	m = r->m_p;
	NMG_CK_MODEL( m );

	/* if bounds haven't been calculated, do it now */
	if( r->ra_p == NULL )
		nmg_region_a( r , &tol );

	/* Check if region extents are beyond the limitations of the TANKILL format */
	for( i=X ; i<ELEMENTS_PER_PT ; i++ )
	{
		if( r->ra_p->min_pt[i] < (-12000.0) )
		{
			rt_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->min_pt[i] );
			return;
		}
		if( r->ra_p->max_pt[i] > 12000.0 )
		{
			rt_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->max_pt[i] );
			return;
		}
	}
#if 0

	/* First make sure that each shell is broken down into maximally connected shells
	 * and while we're at it, split touching loops
	 */
	nmg_tbl( &shells , TBL_INIT , NULL );
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		NMG_CK_SHELL( s );
		nmg_tbl( &shells , TBL_INS , (long *)s );
		nmg_s_split_touchingloops( s , &tol );
	}

	for( i=0 ; i<NMG_TBL_END( &shells ) ; i++ )
	{
		s = (struct shell *)NMG_TBL_GET( &shells , i );
		(void)nmg_decompose_shell( s , &tol );
	}

	nmg_tbl( &shells , TBL_FREE , NULL );

	/* Now triangulate the entire model */
	nmg_triangulate_model( r->m_p , &tol );

	/* XXXXX temporary fix for OT_UNSPEC loops */
	for( RT_LIST_FOR( r , nmgregion , &l->r_hd ) )
	{
		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				struct loopuse *lu;

				for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					if( lu->orientation == OT_UNSPEC )
						lu->orientation = OT_SAME;
				}
			}
		}
	}

	r = RT_LIST_FIRST( nmgregion , &the_model->r_hd );

	/* Need a flag array to insure that no loops are missed */
	flags = (long *)rt_calloc( (*tsp->ts_m)->maxindex , sizeof( long ) , "g-tankill: flags" );

	/* Worry about external/void shells here
	 * void shells should be merged back into their respective external shells
	 * first mark all shells as external or void
	 */
	nmg_find_void_shells( r , flags , &tol );

	/* Now asociate void shells with their respective external shells */
	nmg_assoc_void_shells( r , flags , &tol );

	/* Now merge external shell with all its void shells */
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		if( NMG_INDEX_GET( flags , s ) > 1 )
		{
			struct shell *s2;

			s2 = RT_LIST_FIRST( shell , &r->s_hd );
			while( RT_LIST_NOT_HEAD( s2 , &r->s_hd ) )
			{
				if( NMG_INDEX_GET( flags , s2 ) == (-NMG_INDEX_GET( flags , s ) ) )
				{
					struct shell *s_next;

					s_next = RT_LIST_PNEXT( shell , s2 );
					nmg_js( s , s2 , &tol );
					s2 = s_next;
				}
				else
					s2 = RT_LIST_PNEXT( shell , s2 );
			}
		}
		else if( NMG_INDEX_GET( flags , s ) > (-2) )
			rt_log( "Shell x%x is incorrectly marked as %d\n" , s , NMG_INDEX_GET( flags , s ) );
	}
#else
	/* Now triangulate the entire model */
	nmg_triangulate_model( m , &tol );

	/* Need a flag array to insure that no loops are missed */
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "g-tankill: flags" );
#endif

	/* Output each shell as a TANKILL object */
	nmg_tbl( &vertices , TBL_INIT , NULL );
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu1;
		int missed_loops;

		NMG_CK_SHELL( s );

		/* Make the "patch" style list of vertices */

		/* Put entire first loop on list */
		fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
		NMG_CK_FACEUSE( fu );
		while( fu->orientation != OT_SAME && RT_LIST_NOT_HEAD( fu , &s->fu_hd ) )
			fu = RT_LIST_PNEXT( faceuse , fu );

		if( RT_LIST_IS_HEAD( fu , &s->fu_hd ) )
			continue;

		lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
		NMG_CK_LOOPUSE( lu );
		while( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC &&
			RT_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			lu = RT_LIST_PNEXT( loopuse , lu );
		}

		if( RT_LIST_IS_HEAD( lu , &fu->lu_hd ) )
		{
			rt_log( "g-tankill: faceuse has no loops with edges\n" );
			goto outt;
		}

		if( lu->orientation != OT_SAME )
		{
			rt_log( "g-tankill: Found a hole in a triangulated face!!!\n" );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			goto outt;
		}

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );
		}
		eu1 = RT_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
		NMG_CK_EDGEUSE( eu1 );

		/* mark this loopuse as processed */
		NMG_INDEX_SET( flags , lu );
		NMG_INDEX_SET( flags , lu->lumate_p );

		/* Now travel through all the loops via radial structure */
		missed_loops = 1;
		while( missed_loops )
		{
			NMG_CK_EDGEUSE( eu1 );

			/* move to the radial */
			eu = eu1->eumate_p->radial_p;
			NMG_CK_EDGEUSE( eu );

			/* make sure we stay within the intended shell */
			while(     nmg_find_s_of_eu( eu ) != s
				&& eu != eu1
				&& eu != eu1->eumate_p
				&& *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
					eu = eu->eumate_p->radial_p;

			if( nmg_find_s_of_eu( eu ) != s )
			{
				rt_log( "g-tankill: different shells are connected via radials\n" );
				goto outt;
			}

			/* get the loopuse containing this edgeuse */
			lu = eu->up.lu_p;
			NMG_CK_LOOPUSE( lu );

			/* if this loop hasn't been processed, put it on the list */
			if( NMG_INDEX_TEST_AND_SET( flags , lu ) )
			{
				NMG_INDEX_SET( flags , lu->lumate_p );
				eu1 = RT_LIST_PNEXT_CIRC( edgeuse , eu );
				nmg_tbl( &vertices , TBL_INS , (long *)eu1->eumate_p->vu_p->v_p );
			}
			else
			{
				/* back to a loop that was already done */
				fastf_t dist_to_loop=MAX_FASTF;
				vect_t to_loop;
				struct loopuse *next_lu=NULL;

				/* Check for missed loops */
				for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
				{
					if( fu->orientation != OT_SAME )
						continue;

					for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
					{
						fastf_t tmp_dist;

						/* if this loop was done continue looking */
						if( NMG_INDEX_TEST( flags , lu ) )
							continue;

						/* skips loops of a single vertex */
						if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
							continue;

						/* shouldn't be any holes!!! */
						if( lu->orientation != OT_SAME )
						{
							rt_log( "g-tankill: Found a hole in a triangulated face!!!\n" );
							nmg_pr_fu_briefly( fu , (char *)NULL );
							goto outt;
						}

						/* find the closest unprocessed loop */
						eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
						VSUB2( to_loop , eu->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );
						tmp_dist = MAGSQ( to_loop );
						if( tmp_dist < dist_to_loop )
						{
							dist_to_loop = tmp_dist;
							next_lu = lu;
						}
					}
				}
				if( next_lu == NULL )
				{
					/* we're done */
					missed_loops = 0;
					break;
				}

				/* repeat the last vertex */
				nmg_tbl( &vertices , TBL_INS , NMG_TBL_GET( &vertices , NMG_TBL_END( &vertices ) - 1 ) );

				/* put first vertex of next loop on list twice */
				lu = next_lu;
				eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu );
				nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );

				/* put loop on list */
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );

				eu1 = RT_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu1 );

				/* mark loopuse as processed */
				NMG_INDEX_SET( flags , lu );
				NMG_INDEX_SET( flags , lu->lumate_p );
			}
		}

		/* Now write the data out */
		if( fp_id )	/* Use id count instead of actual id */
			fprintf( fp_out , "%d %d %d           " ,
				NMG_TBL_END( &vertices ), id_counter , surr_code );
		else
			fprintf( fp_out , "%d %d %d           " ,
				NMG_TBL_END( &vertices ), tsp->ts_regionid , surr_code );
		for( i=0 ; i<NMG_TBL_END( &vertices ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)NMG_TBL_GET( &vertices , i );
			if( (i-1)%4 == 0 )
				fprintf( fp_out , " %.3f %.3f %.3f\n" , V3ARGS( v->vg_p->coord ) );
			else
				fprintf( fp_out , " %.3f %.3f %.3f" , V3ARGS( v->vg_p->coord ) );
		}
		if( (NMG_TBL_END( &vertices )-2)%4 != 0 )
			fprintf( fp_out, "\n" );

		/* clear the vertices list for the next shell */
		nmg_tbl( &vertices , TBL_RST , NULL );
	}

	/* write info to the idents file if one is open */
	if( fp_id != NULL )
	{
		struct directory *fp_name;	/* name from pathp */

		fp_name = DB_FULL_PATH_CUR_DIR( pathp );
		fprintf( fp_id , "%d %d %s %d %d %d %s\n" , id_counter , tsp->ts_regionid ,
			fp_name->d_namep , tsp->ts_aircode , tsp->ts_gmater , tsp->ts_los ,
			tsp->ts_mater.ma_shader );
	}

 outt:	rt_free( (char *)flags , "g-tankill: flags" );
	nmg_tbl( &vertices , TBL_FREE , NULL );
}

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		j;
	register int	c;
	double		percent;

	port_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	the_model = nmg_mm();
	tree_state = rt_initial_tree_state;	/* struct copy */
	tree_state.ts_tol = &tol;
	tree_state.ts_ttol = &ttol;
	tree_state.ts_m = &the_model;

	ttol.magic = RT_TESS_TOL_MAGIC;
	/* Defaults, updated by command line options. */
	ttol.abs = 0.0;
	ttol.rel = 0.01;
	ttol.norm = 0.0;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	/* XXX For visualization purposes, in the debug plot files */
	{
		extern fastf_t	nmg_eue_dist;	/* librt/nmg_plot.c */
		/* XXX This value is specific to the Bradley */
		nmg_eue_dist = 2.0;
	}

	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "a:i:n:o:r:s:vx:P:X:")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'i':		/* Idents output file */
			id_file = optarg;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'o':		/* Output file name */
			out_file = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
			break;
		case 's':		/* Surroundings Code */
			surr_code = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'P':
			ncpu = atoi( optarg );
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

	/* Open brl-cad database */
	if ((dbip = db_open( argv[optind] , "r")) == DBI_NULL)
	{
		rt_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		exit(1);
	}
	db_scan(dbip, (int (*)())db_diradd, 1);

	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			rt_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			return 2;
		}
	}

	if( id_file != NULL )
	{
		if ((fp_id = fopen( id_file , "w")) == NULL)
		{
			rt_log( "Cannot open %s\n" , id_file );
			perror( argv[0] );
			return 2;
		}
	}

	optind++;

	/* First produce a list of region ident codes */
	(void)db_walk_tree(dbip, argc-optind, (CONST char **)(&argv[optind]),
		1,				/* ncpu */
		&tree_state,
		get_reg_id,			/* put id in table */
		region_stub,			/* do nothing */
		leaf_stub );			/* do nothing */

	/* TANKILL only allows up to 2000 distinct component codes */
	if( ident_count > 2000 )
	{
		rt_log( "Too many ident codes for TANKILL\n" );
		rt_log( "\tProcessing all regions anyway\n" );
	}

	/* Process regions in ident order */
	curr_id = 0;
	for( id_counter=0 ; id_counter<ident_count ; id_counter++ )
	{
		int next_id = 99999999;
		for( j=0 ; j<ident_count ; j++ )
		{
			int test_id;

			test_id = idents[j];
			if( test_id > curr_id && test_id < next_id )
				next_id = test_id;
		}
		curr_id = next_id;

		/* give user something to look at */
		rt_log( "Processing id %d\n" , curr_id );

		/* Walk indicated tree(s).  Each region will be output separately */
		(void)db_walk_tree(dbip, argc-optind, (CONST char **)(&argv[optind]),
			1,				/* ncpu */
			&tree_state,
			select_region,			/* selects regions with curr_id */
			do_region_end,			/* calls Write_tankill_region */
			nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */
	}

	percent = 0;
	if( regions_tried > 0 )
		percent = ((double)regions_converted * 100) / regions_tried;
	printf( "Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		regions_tried, regions_converted, percent );
	percent = 0;
	if( regions_tried > 0 )
		percent = ((double)regions_written * 100) / regions_tried;
	printf( "                  %d triangulated successfully. %g%%\n",
		regions_written, percent );
#if 0
	/* Release dynamic storage */
	nmg_km(the_model);
	rt_vlist_cleanup();
	db_close(dbip); 
#endif
#if MEMORY_LEAK_CHECKING
	rt_prmem("After complete G-TANKILL conversion");
#endif

	return 0;
}

/*
*			D O _ R E G I O N _ E N D
*
*  Called from db_walk_tree().
*
*  This routine must be prepared to run in parallel.
*/
union tree *do_region_end(tsp, pathp, curtree)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	extern FILE		*fp_fig;
	struct nmgregion	*r;
	struct rt_list		vhead;
	union tree		*ret_tree;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	RT_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		rt_log("\ndo_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
			sofar);
		rt_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( RT_SETJUMP )
	{
		char *sofar;

		/* Error, bail out */
		RT_UNSETJUMP;		/* Relinquish the protection */

		sofar = db_path_to_string(pathp);
		rt_log( "FAILED in Boolean evaluation: %s\n", sofar );
		rt_free( (char *)sofar, "sofar" );

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release any intersector 2d tables */
		nmg_isect2d_final_cleanup();

		/* Get rid of (m)any other intermediate structures */
		if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )
		{
			nmg_km(*tsp->ts_m);
		}
		else
		{
			rt_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
		}
	
		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	}
	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol);	/* librt/nmg_bool.c */

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	RT_UNSETJUMP;		/* Relinquish the protection */
	regions_converted++;
	if (r != 0)
	{
		struct shell *s;
		int empty_region=0;
		int empty_model=0;

		/* Kill cracks */
		s = RT_LIST_FIRST( shell, &r->s_hd );
		while( RT_LIST_NOT_HEAD( &s->l, &r->s_hd ) )
		{
			struct shell *next_s;

			next_s = RT_LIST_PNEXT( shell, &s->l );
			if( nmg_kill_cracks( s ) )
			{
				if( nmg_ks( s ) )
				{
					empty_region = 1;
					break;
				}
			}
			s = next_s;
		}

		/* kill zero length edgeuses */
		if( !empty_region )
		{
			 empty_model = nmg_kill_zero_length_edgeuses( *tsp->ts_m );
		}

		if( !empty_region && !empty_model )
		{
			if( RT_SETJUMP )
			{
				char *sofar;

				RT_UNSETJUMP;

				sofar = db_path_to_string(pathp);
				rt_log( "FAILED in triangulator: %s\n", sofar );
				rt_free( (char *)sofar, "sofar" );

				/* Sometimes the NMG library adds debugging bits when
				 * it detects an internal error, before rt_bomb().
				 */
				rt_g.NMG_debug = NMG_debug;	/* restore mode */

				/* Release any intersector 2d tables */
				nmg_isect2d_final_cleanup();

				/* Get rid of (m)any other intermediate structures */
				if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )
				{
					nmg_km(*tsp->ts_m);
				}
				else
				{
					rt_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
				}
			
				/* Now, make a new, clean model structure for next pass. */
				*tsp->ts_m = nmg_mm();
				goto out;
			}

			/* Write the region to the TANKILL file */
			Write_tankill_region( r , tsp , pathp );

			regions_written++;

			RT_UNSETJUMP;
		}

		if( !empty_model )
			nmg_kr( r );
	}

out:
	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  so we need to cons up an OP_NOP node to return.
	 */
	db_free_tree(curtree);		/* Does an nmg_kr() */

	GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}
