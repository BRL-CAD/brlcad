/*
 *			G - N M G . C
 *
 *  Program to convert a BRL-CAD model (in a .g file) to an NMG facetted model
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
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

RT_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree));
RT_EXTERN( struct face *nmg_find_top_face , (struct shell *s , long *flags ));

static char	usage[] = "Usage: %s [-v] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

static char	*tok_sep = " \t";
static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;		/* Number of processors */
static int	nmg_count=0;		/* Count of nmgregions written to output */
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct rt_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;

/* macro to determine if one bounding box is within another */
#define V3RPP1_IN_RPP2( _lo1 , _hi1 , _lo2 , _hi2 )	( \
	(_lo1)[X] >= (_lo2)[X] && (_hi1)[X] <= (_hi2)[X] && \
	(_lo1)[Y] >= (_lo2)[Y] && (_hi1)[Y] <= (_hi2)[Y] && \
	(_lo1)[Z] >= (_lo2)[Z] && (_hi1)[Z] <= (_hi2)[Z] )

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
	char			*sofar;
	char nmg_name[16];
	unsigned char rgb[3];
	unsigned char *color;
	char *shader;
	char *matparm;
	struct wmember headp;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	RT_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	RT_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK || verbose) {
		sofar = db_path_to_string(pathp);
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
		/* Error, bail out */
		RT_UNSETJUMP;		/* Relinquish the protection */

		sofar = db_path_to_string(pathp);
		rt_log( "FAILED: %s\n", sofar );
		rt_free( (char *)sofar, "sofar" );

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
	(void)nmg_model_fuse(*tsp->ts_m, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol);	/* librt/nmg_bool.c */

	RT_UNSETJUMP;		/* Relinquish the protection */
	if( ret_tree )
		r = ret_tree->tr_d.td_r;
	else
		r = (struct nmgregion *)NULL;

	regions_converted++;

	shader = strtok( tsp->ts_mater.ma_shader, tok_sep );
	matparm = strtok( (char *)NULL, tok_sep );
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
			/* Write the nmgregion to the output file */
			nmg_count++;
			sprintf( nmg_name , "nmg.%d" , nmg_count );
			mk_nmg( fp_out , nmg_name , r->m_p );
		}

		/* NMG region is no longer necessary */
		if( !empty_model )
			nmg_kr(r);

		/* Now make a normal brlcad region */
		if( tsp->ts_mater.ma_override )
		{
			rgb[0] = (int)(tsp->ts_mater.ma_color[0] * 255.0);
			rgb[1] = (int)(tsp->ts_mater.ma_color[1] * 255.0);
			rgb[2] = (int)(tsp->ts_mater.ma_color[2] * 255.0);
			color = rgb;
		}
		else
			color = (unsigned char *)NULL;

		RT_LIST_INIT( &headp.l );
		(void)mk_addmember( nmg_name , &headp , WMOP_UNION );
		if( mk_lrcomb( fp_out,
		    pathp->fp_names[pathp->fp_len-1]->d_namep, &headp, 1,
		    shader, matparm, color,
		    tsp->ts_regionid, tsp->ts_aircode, tsp->ts_gmater,
		    tsp->ts_los, tsp->ts_mater.ma_cinherit ) )
		{
			rt_log( "G-nmg: error in making region (%s)\n" , pathp->fp_names[pathp->fp_len-1]->d_namep );
		}
	}
	else
	{
		RT_LIST_INIT( &headp.l );
		if( mk_lrcomb( fp_out,
		    pathp->fp_names[pathp->fp_len-1]->d_namep, &headp, 1,
		    shader, matparm, color,
		    tsp->ts_regionid, tsp->ts_aircode, tsp->ts_gmater,
		    tsp->ts_los, tsp->ts_mater.ma_cinherit ) )
		{
			rt_log( "G-nmg: error in making region (%s)\n" , pathp->fp_names[pathp->fp_len-1]->d_namep );
		}
	}

	/*
	 *  Dispose of original tree, so that all associated dynamic
	 *  memory is released now, not at the end of all regions.
	 *  A return of TREE_NULL from this routine signals an error,
	 *  so we need to cons up an OP_NOP node to return.
	 */
	db_free_tree(curtree);		/* Does an nmg_kr() */

out:

	if( rt_g.debug&DEBUG_MEM_FULL )
		rt_prmem( "At end of do_region_end()" );

	GETUNION(curtree, tree);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return(curtree);
}

void
csg_comb_func( dbip , dp )
struct db_i *dbip;
struct directory *dp;
{
	union record *rp;
	int comb_len;
	int i,j;
	int region_flag;
	struct wmember headp;
	struct wmember *wm;
	unsigned char *color;

	if( dp->d_uses < 0 )
		return;

	dp->d_uses = (-1);

	if( dp->d_flags & DIR_REGION )
	{
		char **name;

		/* convert a region to NMG's */

		the_model = nmg_mm();
		tree_state = rt_initial_tree_state;	/* struct copy */
		tree_state.ts_tol = &tol;
		tree_state.ts_ttol = &ttol;
		tree_state.ts_m = &the_model;

		name = (&(dp->d_namep));

		(void) db_walk_tree( dbip, 1, (CONST char **)name,
			1,
			&tree_state,
			0,
			do_region_end,
			nmg_booltree_leaf_tess);

		/* Release dynamic storage */
		nmg_km(the_model);

		return;
	}

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
                return;

	if( verbose )
		rt_log( "Combination - %s\n" , dp->d_namep );

	comb_len = dp->d_len - 1;
	if( comb_len == 0 )
	{
		rt_log( "Warning: empty combination (%s)\n" , dp->d_namep );
		dp->d_uses = 0;
		return;
	}

	RT_LIST_INIT( &headp.l );

	for( i=1 ; i<dp->d_len ; i++ )
	{
		wm = mk_addmember( rp[i].M.m_instname , &headp , rp[i].M.m_relation );
		for( j=0 ; j<16 ; j++ )
			wm->wm_mat[j] = rp[i].M.m_mat[j];
	}

	if( rp[0].c.c_flags == 'R' )
		region_flag = 1;
	else
		region_flag = 0;

	if( rp[0].c.c_override )
		color = rp[0].c.c_rgb;
	else
		color = (unsigned char *)NULL;

	if( mk_lrcomb( fp_out, rp[0].c.c_name, &headp, region_flag,
	    rp[0].c.c_matname, rp[0].c.c_matparm,
	    color, rp[0].c.c_regionid,
	    rp[0].c.c_aircode, rp[0].c.c_material,rp[0].c.c_los,
	    rp[0].c.c_inherit ) )
		{
			rt_log( "G-nmg: error in making region (%s)\n" , rp[0].c.c_name );
		}
}

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		i;
	CONST char	*units;
	register int	c;
	double		percent;

	port_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	RT_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

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

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "t:a:n:o:r:vx:P:X:")) != EOF) {
		switch (c) {
		case 't':		/* calculational tolerance */
			tol.dist = atof( optarg );
			tol.dist_sq = tol.dist * tol.dist;
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			ttol.rel = 0.0;
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
		case 'v':
			verbose++;
			break;
		case 'P':
			ncpu = atoi( optarg );
			rt_g.debug = 1;	/* XXX DEBUG_ALLRAYS -- to get core dumps */
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			rt_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
			rt_log("\n");
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			rt_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
			rt_log("\n");
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

	if( units=rt_units_string( dbip->dbi_local2base ) )
		mk_id_units( fp_out , dbip->dbi_title , units );
	else
		mk_id( fp_out , dbip->dbi_title );

	/* Walk the trees outputting regions and combinations */
	for( i=optind ; i<argc ; i++ )
	{
		struct directory *dp;

		dp = db_lookup( dbip , argv[i] , 0 );
		if( dp == DIR_NULL )
		{
			rt_log( "WARNING!!! Could not find %s, skipping\n", argv[i] );
			continue;
		}
		db_functree( dbip , dp , csg_comb_func , 0 );
	}

	rt_vlist_cleanup();
	db_close(dbip);

#if MEMORY_LEAK_CHECKING
	rt_prmem("After complete G-NMG conversion");
#endif

	percent = 100;
	if( regions_tried > 0 )
		percent = ((double)regions_converted * 100) / regions_tried;

	printf( "Tried %d regions, %d converted successfully.  %g%%\n",
		regions_tried, regions_converted, percent );

	return 0;
}

