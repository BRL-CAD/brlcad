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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

RT_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( struct face *nmg_find_top_face , (struct shell *s , long *flags ));

static char	usage[] = "Usage: %s [-v] [-d] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	debug_plots;		/* Make debugging plots */
static int	ncpu = 1;		/* Number of processors */
static char	*out_file = NULL;	/* output filename */
static FILE	*fp_out;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct rt_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;
static int	regions_written = 0;

#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )

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
rt_log( "In nmg_find_void_shells\n" );

	NMG_CK_REGION( r );

	m = r->m_p;
	NMG_CK_MODEL( m );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct face *f;
		struct faceuse *fu;
		vect_t normal;

		f = nmg_find_top_face( s , flags );
		fu = f->fu_p;
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;
		if( fu->orientation != OT_SAME )
			rt_bomb( "nmg_find_void_shells: Neither faceuse nor mate have OT_SAME orient\n" );

		NMG_GET_FU_NORMAL( normal , fu );
		if( normal[Z] > 0.0 )
		{
			NMG_INDEX_ASSIGN( flags , s , 1 )	/* external shell */
rt_log( "Shell x%x is an external shell\n" , s );
		}
		else
		{
			NMG_INDEX_ASSIGN( flags , s , -1 )	/* void shell */
rt_log( "Shell x%x is a void shell\n" , s );
		}
	}
}

static void
nmg_assoc_void_shells( r , flags , ttol )
struct nmgregion *r;
long *flags;
CONST struct rt_tol *ttol;
{
	struct shell *s;
	int ext_shell_id=1;

	NMG_CK_REGION( r );

	if( !r->ra_p )
		nmg_region_a( r , ttol );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{

		NMG_CK_SHELL( s );
		if( !s->sa_p )
			nmg_shell_a( s , ttol );

rt_log( "Checking Shell x%x\n" , s );

		if( NMG_INDEX_GET( flags , s ) == 1 )
		{
			struct shell *void_s;
			struct face *ext_f;

			/* identify this external shell */
			NMG_INDEX_ASSIGN( flags , s , ++ext_shell_id );
rt_log( "\tIt is an external shell, give it id %d\n" , NMG_INDEX_GET( flags , s ) );

			ext_f = nmg_find_top_face( s , flags );

			/* found an external shell, look for voids */
			for( RT_LIST_FOR( void_s , shell , &r->s_hd ) )
			{
				int wrong_void=0;

				if( void_s == s )
					continue;

rt_log( "\t\tChecking shell x%x as possible void\n" , void_s );

				NMG_CK_SHELL( s );
				if( !s->sa_p )
					nmg_shell_a( s , ttol );

				if( NMG_INDEX_GET( flags , void_s ) == (-1) )
				{
					struct face *int_f;
					struct shell *test_s;
rt_log( "\t\t\tIt is a void shell...\n" );

					/* this is a void shell
					 * but does it belong with external shell s */
					if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , s->sa_p->min_pt , s->sa_p->max_pt ) )
					{
rt_log( "\t\t\t\tNot for this external shell\n" );
						continue;
					}

					int_f = nmg_find_top_face( void_s , flags );

					/* Make sure there are no other external shells between these two */
					for( RT_LIST_FOR( test_s , shell , &r->s_hd ) )
					{
						if( NMG_INDEX_GET( flags , test_s ) > 1 )
						{
							struct face *test_f;
							fastf_t test_z;

							if( !V3RPP1_IN_RPP2( void_s->sa_p->min_pt , void_s->sa_p->max_pt , test_s->sa_p->min_pt , test_s->sa_p->max_pt ) )
								continue;

							test_f = nmg_find_top_face( test_s , flags );
							if( test_f->fg_p->max_pt[Z] > int_f->fg_p->max_pt[Z]
							    && test_f->fg_p->max_pt[Z] < ext_f->fg_p->max_pt[Z] )
							{
								wrong_void = 1;
								break;
							}
						}
					}
					if( wrong_void )
					{
rt_log( "\t\t\t\tWrong void\n" );
						continue;
					}

					/* This void shell belongs with shell s
					 * mark it with the negative of external shells flag id */
					NMG_INDEX_ASSIGN( flags , void_s , (-NMG_INDEX_GET( flags , s )) );
rt_log( "\t\t\t\tMarked this Shell (x%x) as %d\n" , void_s , NMG_INDEX_GET( flags , void_s ) );
				}
			}
		}
	}
}

/*	Routine to write an nmgregion in the TANKILL format */
static void
Write_tankill_region( r , tsp )
struct nmgregion *r;
struct db_tree_state *tsp;
{
	struct shell *s;
	struct nmg_ptbl shells;		/* list of shells to be decomposed */
	struct nmg_ptbl vertices;	/* vertex list in TANKILL order */
	long *flags;			/* array to insure that no loops are missed */
	int i;

	NMG_CK_REGION( r );

	/* if bounds haven't been calculated, do it now */
	if( r->ra_p == NULL )
		nmg_region_a( r , &tol );

	/* Check if region extents are beyond the limitations of the TANKILL format */
	for( i=X ; i<ELEMENTS_PER_PT ; i++ )
	{
		if( r->ra_p->min_pt[i] < (-9999.0) )
		{
			rt_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->min_pt[i] );
			return;
		}
		if( r->ra_p->max_pt[i] > 99999.0 )
		{
			rt_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->max_pt[i] );
			return;
		}
	}

	/* First make sure that each shell is broken down into maximally connected shells */
	nmg_tbl( &shells , TBL_INIT , NULL );
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		NMG_CK_SHELL( s );
		nmg_tbl( &shells , TBL_INS , (long *)s );
	}

	for( i=0 ; i<NMG_TBL_END( &shells ) ; i++ )
	{
		s = (struct shell *)NMG_TBL_GET( &shells , i );
		(void)nmg_decompose_shell( s );
	}

	nmg_tbl( &shells , TBL_FREE , NULL );

	/* Now triangulate the entire model */
	nmg_triangulate_model( the_model , &tol );

	/* Output each shell as a TANKILL object */
	/* Need a flag array to insure that no loops are missed */
	flags = (long *)rt_calloc( (*tsp->ts_m)->maxindex , sizeof( long ) , "g-tankill: flags" );

	/* XXXXXX Need to worry about external/void shells here
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

			rt_log( "Shell x%x is an external shell\n" , s );

			s2 = RT_LIST_FIRST( shell , &r->s_hd );
			while( RT_LIST_NOT_HEAD( s2 , &r->s_hd ) )
			{
				if( NMG_INDEX_GET( flags , s2 ) == (-NMG_INDEX_GET( flags , s ) ) )
				{
					struct shell *s_next;

					rt_log( "\tShell x%x is a void shell\n" , s2 );
					s_next = RT_LIST_PNEXT( shell , s2 );
					nmg_merge_shells( s , s2 );
					s2 = s_next;
				}
				else
					s2 = RT_LIST_PNEXT( shell , s2 );
			}
		}
		else if( NMG_INDEX_GET( flags , s ) > (-2) )
			rt_log( "Shell x%x is incorrectly marked as %d\n" , s , NMG_INDEX_GET( flags , s ) );
	}

	nmg_tbl( &vertices , TBL_INIT , NULL );
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu1;
		int missed_loops;

		NMG_CK_SHELL( s );

nmg_pr_s_briefly( s , (char *)NULL );

		/* Make the "patch" style list of vertices */

		/* Put entire first loop on list */
		fu = RT_LIST_FIRST( faceuse , &s->fu_hd );
		NMG_CK_FACEUSE( fu );
		while( fu->orientation != OT_SAME )
			fu = RT_LIST_PNEXT( faceuse , fu );

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
			goto outt;
		}

		for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
rt_log( "vertex: ( %g %g %g ) (first loop x%x eu = x%x)\n" , V3ARGS( eu->vu_p->v_p->vg_p->coord ) , lu , eu );
			nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );
		}
		eu1 = RT_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
		NMG_CK_EDGEUSE( eu1 );
rt_log( "edgeuse: ( %g %g %g ) -> ( %g %g %g ) x%x\n" , V3ARGS( eu1->vu_p->v_p->vg_p->coord ) , 
V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) , eu1 );

		/* mark this loopuse as processed */
		NMG_INDEX_SET( flags , lu );
		NMG_INDEX_SET( flags , lu->lumate_p );

		/* Now travel through all the loops via radial structure */
		missed_loops = 1;
		while( missed_loops )
		{
struct vertex *v;
rt_log( "In loop loop\n" );
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

			if( eu == eu1 || eu == eu1->eumate_p )
				continue;

			if( nmg_find_s_of_eu( eu ) != s )
			{
				rt_log( "g-tankill: different shells are connected via radials\n" );
				goto outt;
			}

			lu = eu->up.lu_p;
			NMG_CK_LOOPUSE( lu );
			if( NMG_INDEX_TEST_AND_SET( flags , lu ) )
			{
				NMG_INDEX_SET( flags , lu->lumate_p );
				eu1 = RT_LIST_PNEXT_CIRC( edgeuse , eu );
				nmg_tbl( &vertices , TBL_INS , (long *)eu1->eumate_p->vu_p->v_p );
rt_log( "vertex: ( %g %g %g ) (makes another face lu = x%x, eu = x%x)\n" , V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) , lu , eu1 );
rt_log( "edgeuse: ( %g %g %g ) -> ( %g %g %g ) x%x\n" , V3ARGS( eu1->vu_p->v_p->vg_p->coord ) , 
V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) , eu1 );
			}
			else
			{
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

						if( NMG_INDEX_TEST( flags , lu ) )
							continue;

						if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
							continue;

						if( lu->orientation != OT_SAME )
						{
							rt_log( "g-tankill: Found a hole in a triangulated face!!!\n" );
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
					missed_loops = 0;
					break;
				}

				/* repeat the last vertex */
				nmg_tbl( &vertices , TBL_INS , NMG_TBL_GET( &vertices , NMG_TBL_END( &vertices ) - 1 ) );
v = (struct vertex *)NMG_TBL_GET( &vertices , NMG_TBL_END( &vertices ) - 1 );
rt_log( "vertex: ( %g %g %g ) (repeat of previous)\n" , V3ARGS( v->vg_p->coord ) );

				/* put first vertex of next loop on list twice */
				lu = next_lu;
				eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu );
				nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );
rt_log( "vertex: ( %g %g %g ) (extra copy of first vertex in loop)\n" , V3ARGS( eu->vu_p->v_p->vg_p->coord ) );

				/* put loop on list */
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					nmg_tbl( &vertices , TBL_INS , (long *)eu->vu_p->v_p );
rt_log( "vertex: ( %g %g %g ) (new loop) lu = x%x , eu = x%x\n" , V3ARGS( eu->vu_p->v_p->vg_p->coord ) , lu , eu );
				}
				eu1 = RT_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu1 );
rt_log( "edgeuse: ( %g %g %g ) -> ( %g %g %g )\n" , V3ARGS( eu1->vu_p->v_p->vg_p->coord ) , 
V3ARGS( eu1->eumate_p->vu_p->v_p->vg_p->coord ) , eu1 );

				/* mark loopuse as processed */
				NMG_INDEX_SET( flags , lu );
				NMG_INDEX_SET( flags , lu->lumate_p );
			}
		}

		/* Now write the data out */
		fprintf( fp_out , "%11d%7d   1000           " ,
			NMG_TBL_END( &vertices ), tsp->ts_regionid );
		for( i=0 ; i<NMG_TBL_END( &vertices ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)NMG_TBL_GET( &vertices , i );
			if( (i-1)%4 == 0 )
				fprintf( fp_out , "%5.0f.%5.0f.%5.0f.\n" , V3ARGS( v->vg_p->coord ) );
			else
				fprintf( fp_out , "%5.0f.%5.0f.%5.0f." , V3ARGS( v->vg_p->coord ) );
		}

		/* clear the vertices list for the next shell */
		nmg_tbl( &vertices , TBL_RST , NULL );
	}

	regions_written++;

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
	char		*dot;
	int		i, ret;
	register int	c;
	double		percent;
	struct rt_vls	fig_file;

#ifdef BSD
	setlinebuf( stderr );
#else
#	if defined( SYSV ) && !defined( sgi ) && !defined(CRAY2) && \
	 !defined(n16)
		(void) setvbuf( stderr, (char *) NULL, _IOLBF, BUFSIZ );
#	endif
#	if defined(sgi) && defined(mips)
		if( setlinebuf( stderr ) != 0 )
			perror("setlinebuf(stderr)");
#	endif
#endif

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
	while ((c = getopt(argc, argv, "a:dn:o:r:vx:P:X:")) != EOF) {
		switch (c) {
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			break;
		case 'd':
			debug_plots = 1;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg);
			break;
		case 'o':		/* Output file name */
			out_file = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
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
	optind++;

	/* Walk indicated tree(s).  Each region will be output separately */
	ret = db_walk_tree(dbip, argc-optind, (CONST char **)(&argv[optind]),
		1,			/* ncpu */
		&tree_state,
		0,			/* take all regions */
		do_region_end,
		nmg_booltree_leaf_tess);	/* in librt/nmg_bool.c */

	percent = 0;
	if( regions_tried > 0 )
		percent = ((double)regions_converted * 100) / regions_tried;
	printf( "Tried %d regions, %d converted successfully.  %g%%\n",
		regions_tried, regions_converted, percent );
	percent = 0;
	if( regions_tried > 0 )
		percent = ((double)regions_written * 100) / regions_tried;
	printf( "                  %d written successfully. %g%%\n",
		regions_written, percent );

	/* Release dynamic storage */
	nmg_km(the_model);
	rt_vlist_cleanup();
	db_close(dbip);

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
	if( ncpu == 1 && RT_SETJUMP )
	{
		/* Error, bail out */
		RT_UNSETJUMP;		/* Relinquish the protection */

		/* Sometimes the NMG library adds debugging bits when
		 * it detects an internal error, before rt_bomb().
		 */
		rt_g.NMG_debug = NMG_debug;	/* restore mode */

		/* Release any intersector 2d tables */
		nmg_isect2d_final_cleanup();

		/* Release the tree memory & input regions */
		db_free_tree(curtree);		/* Does an nmg_kr() */

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
	r = nmg_booltree_evaluate(curtree, tsp->ts_tol);	/* librt/nmg_bool.c */
	RT_UNSETJUMP;		/* Relinquish the protection */
	regions_converted++;
	if (r != 0)
	{
		/* Write the region to the TANKILL file */
		Write_tankill_region( r , tsp );

		/* NMG region is no longer necessary */
		nmg_kr(r);
	}

	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  so we need to cons up an OP_NOP node to return.
	 */
	db_free_tree(curtree);		/* Does an nmg_kr() */

out:
	GETUNION(curtree, tree);
	curtree->tr_op = OP_NOP;
	return(curtree);
}
