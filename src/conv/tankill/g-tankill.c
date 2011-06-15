/*                     G - T A N K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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
 *
 */
/** @file g-tankill.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a TANKILL
 * facetted model by calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"


extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);

static const char usage[] = "Usage:\n\
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

    for ( i=0; i<ident_count; i++ )
    {
	if ( idents[i] == id )
	    return;
    }

    if ( ident_count == ident_length )
    {
	idents = (int *)bu_realloc( (char *)idents, (ident_length + IDENT_BLOCK)*sizeof( int ), "insert_id: idents" );
	ident_length += IDENT_BLOCK;
    }

    idents[ident_count] = id;
    ident_count++;
}

/* routine used in tree walker to select regions with the current ident number */
static int
select_region(struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), const struct rt_comb_internal *UNUSED(combp), genptr_t UNUSED(client_data))
{
    if ( tsp->ts_regionid == curr_id )
	return 0;
    else
	return -1;
}

/* routine used in tree walker to collect region ident numbers */
static int
get_reg_id(struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), const struct rt_comb_internal *UNUSED(combp), genptr_t UNUSED(client_data))
{
    insert_id( tsp->ts_regionid );
    return -1;
}

/* stubs to warn of the unexpected */
static union tree *
region_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, union tree *UNUSED(curtree), genptr_t UNUSED(client_data))
{
    struct directory *fp_name;	/* name from pathp */

    fp_name = DB_FULL_PATH_CUR_DIR( pathp );
    bu_log( "region stub called (for object %s), this shouldn't happen\n", fp_name->d_namep );
    return (union tree *)NULL;
}

static union tree *
leaf_stub(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, struct rt_db_internal *UNUSED(ip), genptr_t UNUSED(client_data))
{
    struct directory *fp_name;	/* name from pathp */

    fp_name = DB_FULL_PATH_CUR_DIR( pathp );
    bu_log( "Only regions may be converted to TANKILL format\n\t%s is not a region and will be ignored\n", fp_name->d_namep );
    return (union tree *)NULL;
}


/*	Routine to write an nmgregion in the TANKILL format */
static void
Write_tankill_region(struct nmgregion *r, struct db_tree_state *tsp, const struct db_full_path *pathp)
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
    if ( r->ra_p == NULL )
	nmg_region_a( r, &tol );

    /* Check if region extents are beyond the limitations of the TANKILL format */
    for ( i=X; i<ELEMENTS_PER_POINT; i++ )
    {
	if ( r->ra_p->min_pt[i] < (-12000.0) )
	{
	    bu_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n", r->ra_p->min_pt[i] );
	    return;
	}
	if ( r->ra_p->max_pt[i] > 12000.0 )
	{
	    bu_log( "g-tankill: Coordinates too large (%g) for TANKILL format\n", r->ra_p->max_pt[i] );
	    return;
	}
    }
    /* Now triangulate the entire model */
    nmg_triangulate_model( m, &tol );

    /* Need a flag array to insure that no loops are missed */
    flags = (long *)bu_calloc( m->maxindex, sizeof( long ), "g-tankill: flags" );

    /* Output each shell as a TANKILL object */
    bu_ptbl_init( &vertices, 64, " &vertices ");
    for ( BU_LIST_FOR( s, shell, &r->s_hd ) )
    {
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu1;
	int missed_loops;

	NMG_CK_SHELL( s );

	/* Make the "patch" style list of vertices */

	/* Put entire first loop on list */
	fu = BU_LIST_FIRST( faceuse, &s->fu_hd );
	NMG_CK_FACEUSE( fu );
	while ( fu->orientation != OT_SAME && BU_LIST_NOT_HEAD( fu, &s->fu_hd ) )
	    fu = BU_LIST_PNEXT( faceuse, fu );

	if ( BU_LIST_IS_HEAD( fu, &s->fu_hd ) )
	    continue;

	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
	NMG_CK_LOOPUSE( lu );
	while ( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC &&
		BU_LIST_NOT_HEAD( lu, &fu->lu_hd ) )
	{
	    lu = BU_LIST_PNEXT( loopuse, lu );
	}

	if ( BU_LIST_IS_HEAD( lu, &fu->lu_hd ) )
	{
	    bu_log( "g-tankill: faceuse has no loops with edges\n" );
	    goto outt;
	}

	if ( lu->orientation != OT_SAME )
	{
	    bu_log( "g-tankill: Found a hole in a triangulated face!\n" );
	    nmg_pr_fu_briefly( fu, (char *)NULL );
	    goto outt;
	}

	for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
	    NMG_CK_EDGEUSE( eu );
	    bu_ptbl_ins( &vertices, (long *)eu->vu_p->v_p );
	}
	eu1 = BU_LIST_PLAST_PLAST( edgeuse, &lu->down_hd );
	NMG_CK_EDGEUSE( eu1 );

	/* mark this loopuse as processed */
	NMG_INDEX_SET( flags, lu );
	NMG_INDEX_SET( flags, lu->lumate_p );

	/* Now travel through all the loops via radial structure */
	missed_loops = 1;
	while ( missed_loops )
	{
	    NMG_CK_EDGEUSE( eu1 );

	    /* move to the radial */
	    eu = eu1->eumate_p->radial_p;
	    NMG_CK_EDGEUSE( eu );

	    /* make sure we stay within the intended shell */
	    while (     nmg_find_s_of_eu( eu ) != s
			&& eu != eu1
			&& eu != eu1->eumate_p
			&& *eu->up.magic_p != NMG_LOOPUSE_MAGIC )
		eu = eu->eumate_p->radial_p;

	    if ( nmg_find_s_of_eu( eu ) != s )
	    {
		bu_log( "g-tankill: different shells are connected via radials\n" );
		goto outt;
	    }

	    /* get the loopuse containing this edgeuse */
	    lu = eu->up.lu_p;
	    NMG_CK_LOOPUSE( lu );

	    /* if this loop hasn't been processed, put it on the list */
	    if ( NMG_INDEX_TEST_AND_SET( flags, lu ) )
	    {
		NMG_INDEX_SET( flags, lu->lumate_p );
		eu1 = BU_LIST_PNEXT_CIRC( edgeuse, eu );
		bu_ptbl_ins( &vertices, (long *)eu1->eumate_p->vu_p->v_p );
	    }
	    else
	    {
		/* back to a loop that was already done */
		fastf_t dist_to_loop=MAX_FASTF;
		vect_t to_loop;
		struct loopuse *next_lu=NULL;

		/* Check for missed loops */
		for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
		    if ( fu->orientation != OT_SAME )
			continue;

		    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
		    {
			fastf_t tmp_dist;

			/* if this loop was done continue looking */
			if ( NMG_INDEX_TEST( flags, lu ) )
			    continue;

			/* skips loops of a single vertex */
			if ( BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
			    continue;

			/* shouldn't be any holes! */
			if ( lu->orientation != OT_SAME )
			{
			    bu_log( "g-tankill: Found a hole in a triangulated face!\n" );
			    nmg_pr_fu_briefly( fu, (char *)NULL );
			    goto outt;
			}

			/* find the closest unprocessed loop */
			eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
			VSUB2( to_loop, eu->vu_p->v_p->vg_p->coord, eu1->vu_p->v_p->vg_p->coord );
			tmp_dist = MAGSQ( to_loop );
			if ( tmp_dist < dist_to_loop )
			{
			    dist_to_loop = tmp_dist;
			    next_lu = lu;
			}
		    }
		}
		if ( next_lu == NULL )
		{
		    /* we're done */
		    missed_loops = 0;
		    break;
		}

		/* repeat the last vertex */
		bu_ptbl_ins( &vertices, BU_PTBL_GET( &vertices, BU_PTBL_END( &vertices ) - 1 ) );

		/* put first vertex of next loop on list twice */
		lu = next_lu;
		eu = BU_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE( eu );
		bu_ptbl_ins( &vertices, (long *)eu->vu_p->v_p );

		/* put loop on list */
		for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		    bu_ptbl_ins( &vertices, (long *)eu->vu_p->v_p );

		eu1 = BU_LIST_PLAST_PLAST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE( eu1 );

		/* mark loopuse as processed */
		NMG_INDEX_SET( flags, lu );
		NMG_INDEX_SET( flags, lu->lumate_p );
	    }
	}

	/* Now write the data out */
	if ( fp_id )	/* Use id count instead of actual id */
	    fprintf( fp_out, "%lu %d %d           " ,
		     (unsigned long)BU_PTBL_END( &vertices ), id_counter, surr_code );
	else
	    fprintf( fp_out, "%lu %d %d           " ,
		     (unsigned long)BU_PTBL_END( &vertices ), tsp->ts_regionid, surr_code );
	for ( i=0; i<BU_PTBL_END( &vertices ); i++ )
	{
	    struct vertex *v;

	    v = (struct vertex *)BU_PTBL_GET( &vertices, i );
	    if ( (i-1)%4 == 0 )
		fprintf( fp_out, " %.3f %.3f %.3f\n", V3ARGS( v->vg_p->coord ) );
	    else
		fprintf( fp_out, " %.3f %.3f %.3f", V3ARGS( v->vg_p->coord ) );
	}
	if ( (BU_PTBL_END( &vertices )-2)%4 != 0 )
	    fprintf( fp_out, "\n" );

	/* clear the vertices list for the next shell */
	bu_ptbl_reset( &vertices );
    }

    /* write info to the idents file if one is open */
    if ( fp_id != NULL )
    {
	struct directory *fp_name;	/* name from pathp */

	fp_name = DB_FULL_PATH_CUR_DIR( pathp );
	fprintf( fp_id, "%d %d %s %d %d %d %s\n", id_counter, tsp->ts_regionid ,
		 fp_name->d_namep, tsp->ts_aircode, tsp->ts_gmater, tsp->ts_los ,
		 tsp->ts_mater.ma_shader );
    }

outt:	bu_free( (char *)flags, "g-tankill: flags" );
    bu_ptbl_free( &vertices );
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
    int		j;
    int	c;
    double		percent;

    bu_setlinebuf( stderr );

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

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    /* For visualization purposes, in the debug plot files */
    {
	extern fastf_t	nmg_eue_dist;	/* librt/nmg_plot.c */
	/* WTF: This value is specific to the Bradley */
	nmg_eue_dist = 2.0;
    }

    rt_init_resource( &rt_uniresource, 0, NULL );

    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:i:n:o:r:s:vx:P:X:")) != -1) {
	switch (c) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'i':		/* Idents output file */
		id_file = bu_optarg;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name */
		out_file = bu_optarg;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 's':		/* Surroundings Code */
		surr_code = atoi(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
/*			ncpu = atoi( bu_optarg ); */
		bu_debug = BU_DEBUG_COREDUMP;	/* to get core dumps */
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
		NMG_debug = rt_g.NMG_debug;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    /* Open BRL-CAD database */
    if ((dbip = db_open( argv[bu_optind], "r")) == DBI_NULL)
    {
	perror(argv[0]);
	bu_exit(1, "Cannot open %s\n", argv[bu_optind] );
    }
    if ( db_dirbuild( dbip ) ) {
	bu_exit(1, "db_dirbuild failed\n" );
    }

    if (out_file == NULL) {
	fp_out = stdout;
#if defined(_WIN32) && !defined(__CYGWIN__)
	setmode(fileno(fp_out), O_BINARY);
#endif
    } else {
	if ((fp_out = fopen( out_file, "wb")) == NULL)
	{
	    bu_log( "Cannot open %s\n", out_file );
	    perror( argv[0] );
	    return 2;
	}
    }

    if ( id_file != NULL )
    {
	if ((fp_id = fopen( id_file, "wb")) == NULL)
	{
	    bu_log( "Cannot open %s\n", id_file );
	    perror( argv[0] );
	    return 2;
	}
    }

    bu_optind++;

    /* First produce a list of region ident codes */
    (void)db_walk_tree(dbip, argc-bu_optind, (const char **)(&argv[bu_optind]),
		       1,				/* ncpu */
		       &tree_state,
		       get_reg_id,			/* put id in table */
		       region_stub,			/* do nothing */
		       leaf_stub,
		       (genptr_t)NULL );			/* do nothing */

    /* TANKILL only allows up to 2000 distinct component codes */
    if ( ident_count > 2000 )
    {
	bu_log( "Too many ident codes for TANKILL\n" );
	bu_log( "\tProcessing all regions anyway\n" );
    }

    /* Process regions in ident order */
    curr_id = 0;
    for ( id_counter=0; id_counter<ident_count; id_counter++ )
    {
	int next_id = 99999999;
	for ( j=0; j<ident_count; j++ )
	{
	    int test_id;

	    test_id = idents[j];
	    if ( test_id > curr_id && test_id < next_id )
		next_id = test_id;
	}
	curr_id = next_id;

	/* give user something to look at */
	bu_log( "Processing id %d\n", curr_id );

	/* Walk indicated tree(s).  Each region will be output separately */
	(void)db_walk_tree(dbip, argc-bu_optind, (const char **)(&argv[bu_optind]),
			   1,				/* ncpu */
			   &tree_state,
			   select_region,			/* selects regions with curr_id */
			   do_region_end,			/* calls Write_tankill_region */
			   nmg_booltree_leaf_tess,
			   (genptr_t)NULL);	/* in librt/nmg_bool.c */
    }

    percent = 0;
    if ( regions_tried > 0 )
	percent = ((double)regions_converted * 100) / regions_tried;
    printf( "Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	    regions_tried, regions_converted, percent );
    percent = 0;
    if ( regions_tried > 0 )
	percent = ((double)regions_written * 100) / regions_tried;
    printf( "                  %d triangulated successfully. %g%%\n",
	    regions_written, percent );

    return 0;
}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the TANKILL file */
	Write_tankill_region( r, tsp, pathp );

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log( "FAILED in triangulator: %s\n", sofar );
	bu_free( (char *)sofar, "sofar" );

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	rt_g.NMG_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Get rid of (m)any other intermediate structures */
	if ( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC ) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    }  BU_UNSETJUMP;
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if ( !BU_SETJUMP ) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);

    } else  {
	/* catch */
	char *name = db_path_to_string( pathp );

	/* Error, bail out */
	bu_log( "conversion of %s FAILED!\n", name );

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	rt_g.NMG_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree, &rt_uniresource);/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC ) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	bu_free( name, "db_path_to_string" );
	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    } BU_UNSETJUMP;/* Relinquish the protection */

    return ret_tree;
}


/*
 *			D O _ R E G I O N _ E N D
 *
 *  Called from db_walk_tree().
 *
 *  This routine must be prepared to run in parallel.
 */
union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t UNUSED(client_data))
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
	return curtree;

    regions_tried++;

    ret_tree = process_boolean(curtree, tsp, pathp);

    if ( ret_tree )
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    regions_converted++;

    if (r != (struct nmgregion *)NULL)
    {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST( shell, &r->s_hd );
	while ( BU_LIST_NOT_HEAD( &s->l, &r->s_hd ) )
	{
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT( shell, &s->l );
	    if ( nmg_kill_cracks( s ) )
	    {
		if ( nmg_ks( s ) )
		{
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if ( !empty_region )
	{
	    empty_model = nmg_kill_zero_length_edgeuses( *tsp->ts_m );
	}

	if ( !empty_region && !empty_model )
	{
	    process_triangulation(r, pathp, tsp);

	    regions_written++;

	}

	if ( !empty_model )
	    nmg_kr( r );
    }

    /*
     *  Dispose of original tree, so that all associated dynamic
     *  memory is released now, not at the end of all regions.
     *  A return of TREE_NULL from this routine signals an error,
     *  so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    BU_GETUNION(curtree, tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
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
