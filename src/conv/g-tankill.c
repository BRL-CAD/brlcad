/*                     G - T A N K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
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
 *
 */
/** @file g-tankill.c
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
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif

/* interface headers */
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local headers */
#include "../librt/debug.h"


BU_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));

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
/* static int	ncpu = 1; */		/* Number of processors */
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
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;
static int	regions_written = 0;

void
insert_id(int id)
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
select_region(register struct db_tree_state *tsp, struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
	if( tsp->ts_regionid == curr_id )
		return( 0 );
	else
		return( -1 );
}

/* routine used in tree walker to collect region ident numbers */
static int
get_reg_id(register struct db_tree_state *tsp, struct db_full_path *pathp, const struct rt_comb_internal *combp, genptr_t client_data)
{
	insert_id( tsp->ts_regionid );
	return( -1 );
}

/* stubs to warn of the unexpected */
static union tree *
region_stub(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	struct directory *fp_name;	/* name from pathp */

	fp_name = DB_FULL_PATH_CUR_DIR( pathp );
	bu_log( "region stub called (for object %s), this shouldn't happen\n" , fp_name->d_namep );
	return( (union tree *)NULL );
}

static union tree *
leaf_stub(struct db_tree_state *tsp, struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
	struct directory *fp_name;	/* name from pathp */

	fp_name = DB_FULL_PATH_CUR_DIR( pathp );
	bu_log( "Only regions may be converted to TANKILL format\n\t%s is not a region and will be ignored\n" , fp_name->d_namep );
	return( (union tree *)NULL );
}

#if 0
/* Routine to identify external/void shells
 *	Marks external shells with a +1 in the flags array
 *	Marks void shells with a -1 in the flags array
 */
static void
nmg_find_void_shells( r , flags , ttol )
const struct nmgregion *r;
const struct bn_tol *ttol;
long *flags;
{
	struct model *m;
	struct shell *s;

	NMG_CK_REGION( r );

	m = r->m_p;
	NMG_CK_MODEL( m );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
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
const struct bn_tol *ttol;
{
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int ext_shell_id=1;

	NMG_CK_REGION( r );

	if( !r->ra_p )
		nmg_region_a( r , ttol );

	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{

		NMG_CK_SHELL( s );
		if( !s->sa_p )
			nmg_shell_a( s , ttol );

		if( NMG_INDEX_GET( flags , s ) == 1 )
		{
			struct shell *void_s;

			/* identify this external shell */
			NMG_INDEX_ASSIGN( flags , s , ++ext_shell_id );

			/* found an external shell, look for voids */
			for( BU_LIST_FOR( void_s , shell , &r->s_hd ) )
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

					for( BU_LIST_FOR( fu , faceuse , &void_s->fu_hd ) )
					{
						for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
						{
							if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
								continue;
							for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
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
					for( BU_LIST_FOR( test_s , shell , &r->s_hd ) )
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
#endif

/*	Routine to write an nmgregion in the TANKILL format */
static void
Write_tankill_region(struct nmgregion *r, struct db_tree_state *tsp, struct db_full_path *pathp)
{
	struct model *m;
	struct shell *s;
	struct bu_ptbl vertices;	/* vertex list in TANKILL order */
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
			bu_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->min_pt[i] );
			return;
		}
		if( r->ra_p->max_pt[i] > 12000.0 )
		{
			bu_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n" , r->ra_p->max_pt[i] );
			return;
		}
	}
#if 0

	/* First make sure that each shell is broken down into maximally connected shells
	 * and while we're at it, split touching loops
	 */
	bu_ptbl_init( &shells , 64, " &shells ");
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		NMG_CK_SHELL( s );
		bu_ptbl_ins( &shells , (long *)s );
		nmg_s_split_touchingloops( s , &tol );
	}

	for( i=0 ; i<BU_PTBL_END( &shells ) ; i++ )
	{
		s = (struct shell *)BU_PTBL_GET( &shells , i );
		(void)nmg_decompose_shell( s , &tol );
	}

	bu_ptbl_free( &shells );

	/* Now triangulate the entire model */
	nmg_triangulate_model( r->m_p , &tol );

	/* XXXXX temporary fix for OT_UNSPEC loops */
	for( BU_LIST_FOR( r , nmgregion , &l->r_hd ) )
	{
		for( BU_LIST_FOR( s , shell , &r->s_hd ) )
		{
			struct faceuse *fu;

			for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
			{
				struct loopuse *lu;

				for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
				{
					if( lu->orientation == OT_UNSPEC )
						lu->orientation = OT_SAME;
				}
			}
		}
	}

	r = BU_LIST_FIRST( nmgregion , &the_model->r_hd );

	/* Need a flag array to insure that no loops are missed */
	flags = (long *)bu_calloc( (*tsp->ts_m)->maxindex , sizeof( long ) , "g-tankill: flags" );

	/* Worry about external/void shells here
	 * void shells should be merged back into their respective external shells
	 * first mark all shells as external or void
	 */
	nmg_find_void_shells( r , flags , &tol );

	/* Now asociate void shells with their respective external shells */
	nmg_assoc_void_shells( r , flags , &tol );

	/* Now merge external shell with all its void shells */
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		if( NMG_INDEX_GET( flags , s ) > 1 )
		{
			struct shell *s2;

			s2 = BU_LIST_FIRST( shell , &r->s_hd );
			while( BU_LIST_NOT_HEAD( s2 , &r->s_hd ) )
			{
				if( NMG_INDEX_GET( flags , s2 ) == (-NMG_INDEX_GET( flags , s ) ) )
				{
					struct shell *s_next;

					s_next = BU_LIST_PNEXT( shell , s2 );
					nmg_js( s , s2 , &tol );
					s2 = s_next;
				}
				else
					s2 = BU_LIST_PNEXT( shell , s2 );
			}
		}
		else if( NMG_INDEX_GET( flags , s ) > (-2) )
			bu_log( "Shell x%x is incorrectly marked as %d\n" , s , NMG_INDEX_GET( flags , s ) );
	}
#else
	/* Now triangulate the entire model */
	nmg_triangulate_model( m , &tol );

	/* Need a flag array to insure that no loops are missed */
	flags = (long *)bu_calloc( m->maxindex , sizeof( long ) , "g-tankill: flags" );
#endif

	/* Output each shell as a TANKILL object */
	bu_ptbl_init( &vertices , 64, " &vertices ");
	for( BU_LIST_FOR( s , shell , &r->s_hd ) )
	{
		struct faceuse *fu;
		struct loopuse *lu;
		struct edgeuse *eu;
		struct edgeuse *eu1;
		int missed_loops;

		NMG_CK_SHELL( s );

		/* Make the "patch" style list of vertices */

		/* Put entire first loop on list */
		fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
		NMG_CK_FACEUSE( fu );
		while( fu->orientation != OT_SAME && BU_LIST_NOT_HEAD( fu , &s->fu_hd ) )
			fu = BU_LIST_PNEXT( faceuse , fu );

		if( BU_LIST_IS_HEAD( fu , &s->fu_hd ) )
			continue;

		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		NMG_CK_LOOPUSE( lu );
		while( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC &&
			BU_LIST_NOT_HEAD( lu , &fu->lu_hd ) )
		{
			lu = BU_LIST_PNEXT( loopuse , lu );
		}

		if( BU_LIST_IS_HEAD( lu , &fu->lu_hd ) )
		{
			bu_log( "g-tankill: faceuse has no loops with edges\n" );
			goto outt;
		}

		if( lu->orientation != OT_SAME )
		{
			bu_log( "g-tankill: Found a hole in a triangulated face!!!\n" );
			nmg_pr_fu_briefly( fu , (char *)NULL );
			goto outt;
		}

		for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
		{
			NMG_CK_EDGEUSE( eu );
			bu_ptbl_ins( &vertices , (long *)eu->vu_p->v_p );
		}
		eu1 = BU_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
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
				bu_log( "g-tankill: different shells are connected via radials\n" );
				goto outt;
			}

			/* get the loopuse containing this edgeuse */
			lu = eu->up.lu_p;
			NMG_CK_LOOPUSE( lu );

			/* if this loop hasn't been processed, put it on the list */
			if( NMG_INDEX_TEST_AND_SET( flags , lu ) )
			{
				NMG_INDEX_SET( flags , lu->lumate_p );
				eu1 = BU_LIST_PNEXT_CIRC( edgeuse , eu );
				bu_ptbl_ins( &vertices , (long *)eu1->eumate_p->vu_p->v_p );
			}
			else
			{
				/* back to a loop that was already done */
				fastf_t dist_to_loop=MAX_FASTF;
				vect_t to_loop;
				struct loopuse *next_lu=NULL;

				/* Check for missed loops */
				for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
				{
					if( fu->orientation != OT_SAME )
						continue;

					for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
					{
						fastf_t tmp_dist;

						/* if this loop was done continue looking */
						if( NMG_INDEX_TEST( flags , lu ) )
							continue;

						/* skips loops of a single vertex */
						if( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
							continue;

						/* shouldn't be any holes!!! */
						if( lu->orientation != OT_SAME )
						{
							bu_log( "g-tankill: Found a hole in a triangulated face!!!\n" );
							nmg_pr_fu_briefly( fu , (char *)NULL );
							goto outt;
						}

						/* find the closest unprocessed loop */
						eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
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
				bu_ptbl_ins( &vertices , BU_PTBL_GET( &vertices , BU_PTBL_END( &vertices ) - 1 ) );

				/* put first vertex of next loop on list twice */
				lu = next_lu;
				eu = BU_LIST_FIRST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu );
				bu_ptbl_ins( &vertices , (long *)eu->vu_p->v_p );

				/* put loop on list */
				for( BU_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
					bu_ptbl_ins( &vertices , (long *)eu->vu_p->v_p );

				eu1 = BU_LIST_PLAST_PLAST( edgeuse , &lu->down_hd );
				NMG_CK_EDGEUSE( eu1 );

				/* mark loopuse as processed */
				NMG_INDEX_SET( flags , lu );
				NMG_INDEX_SET( flags , lu->lumate_p );
			}
		}

		/* Now write the data out */
		if( fp_id )	/* Use id count instead of actual id */
			fprintf( fp_out , "%d %d %d           " ,
				BU_PTBL_END( &vertices ), id_counter , surr_code );
		else
			fprintf( fp_out , "%d %d %d           " ,
				BU_PTBL_END( &vertices ), tsp->ts_regionid , surr_code );
		for( i=0 ; i<BU_PTBL_END( &vertices ) ; i++ )
		{
			struct vertex *v;

			v = (struct vertex *)BU_PTBL_GET( &vertices , i );
			if( (i-1)%4 == 0 )
				fprintf( fp_out , " %.3f %.3f %.3f\n" , V3ARGS( v->vg_p->coord ) );
			else
				fprintf( fp_out , " %.3f %.3f %.3f" , V3ARGS( v->vg_p->coord ) );
		}
		if( (BU_PTBL_END( &vertices )-2)%4 != 0 )
			fprintf( fp_out, "\n" );

		/* clear the vertices list for the next shell */
		bu_ptbl_reset( &vertices );
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

 outt:	bu_free( (char *)flags , "g-tankill: flags" );
	bu_ptbl_free( &vertices );
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	int		j;
	register int	c;
	double		percent;

	bu_setlinebuf( stderr );

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
	tol.magic = BN_TOL_MAGIC;
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

	rt_init_resource( &rt_uniresource, 0, NULL );

	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

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
/*			ncpu = atoi( optarg ); */
			bu_debug = BU_DEBUG_COREDUMP;	/* to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
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

	/* Open BRL-CAD database */
	if ((dbip = db_open( argv[optind] , "r")) == DBI_NULL)
	{
		bu_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		exit(1);
	}
	db_dirbuild( dbip );

	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			bu_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			return 2;
		}
	}

	if( id_file != NULL )
	{
		if ((fp_id = fopen( id_file , "w")) == NULL)
		{
			bu_log( "Cannot open %s\n" , id_file );
			perror( argv[0] );
			return 2;
		}
	}

	optind++;

	/* First produce a list of region ident codes */
	(void)db_walk_tree(dbip, argc-optind, (const char **)(&argv[optind]),
		1,				/* ncpu */
		&tree_state,
		get_reg_id,			/* put id in table */
		region_stub,			/* do nothing */
		leaf_stub,
		(genptr_t)NULL );			/* do nothing */

	/* TANKILL only allows up to 2000 distinct component codes */
	if( ident_count > 2000 )
	{
		bu_log( "Too many ident codes for TANKILL\n" );
		bu_log( "\tProcessing all regions anyway\n" );
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
		bu_log( "Processing id %d\n" , curr_id );

		/* Walk indicated tree(s).  Each region will be output separately */
		(void)db_walk_tree(dbip, argc-optind, (const char **)(&argv[optind]),
			1,				/* ncpu */
			&tree_state,
			select_region,			/* selects regions with curr_id */
			do_region_end,			/* calls Write_tankill_region */
			nmg_booltree_leaf_tess,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */
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
	bn_vlist_cleanup();
	db_close(dbip);
#endif
#if MEMORY_LEAK_CHECKING
	bu_prmem("After complete G-TANKILL conversion");
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
union tree *do_region_end(register struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	union tree		*ret_tree;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BU_LIST_INIT(&vhead);

	if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("\ndo_region_end(%d %d%%) %s\n",
			regions_tried,
			regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
			sofar);
		bu_free(sofar, "path string");
	}

	if (curtree->tr_op == OP_NOP)
		return  curtree;

	regions_tried++;
	/* Begin rt_bomb() protection */
	if( BU_SETJUMP )
	{
		char *sofar;

		/* Error, bail out */
		BU_UNSETJUMP;		/* Relinquish the protection */

		sofar = db_path_to_string(pathp);
		bu_log( "FAILED in Boolean evaluation: %s\n", sofar );
		bu_free( (char *)sofar, "sofar" );

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
			bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
		}

		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	}
	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	/* librt/nmg_bool.c */
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);

	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	BU_UNSETJUMP;		/* Relinquish the protection */
	regions_converted++;
	if (r != (struct nmgregion *)NULL)
	{
		struct shell *s;
		int empty_region=0;
		int empty_model=0;

		/* Kill cracks */
		s = BU_LIST_FIRST( shell, &r->s_hd );
		while( BU_LIST_NOT_HEAD( &s->l, &r->s_hd ) )
		{
			struct shell *next_s;

			next_s = BU_LIST_PNEXT( shell, &s->l );
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
			if( BU_SETJUMP )
			{
				char *sofar;

				BU_UNSETJUMP;

				sofar = db_path_to_string(pathp);
				bu_log( "FAILED in triangulator: %s\n", sofar );
				bu_free( (char *)sofar, "sofar" );

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
					bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
				}

				/* Now, make a new, clean model structure for next pass. */
				*tsp->ts_m = nmg_mm();
				goto out;
			}

			/* Write the region to the TANKILL file */
			Write_tankill_region( r , tsp , pathp );

			regions_written++;

			BU_UNSETJUMP;
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
	db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

	BU_GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
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
