/*
 *			G - X X X , C
 *
 *	Sample code for converting BRL-CAD models to some other format
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
static const char RCSid[] = "$Header$";
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
#include "wdb.h"
#include "../librt/debug.h"

BU_EXTERN( void comb_func , ( struct db_i *dbip , struct directory *dp ) );
BU_EXTERN( void solid_func , ( struct db_i *dbip , struct directory *dp ) );

static char	*tok_sep = " \t";
static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;		/* Number of processors */
static char	*out_file = NULL;	/* Output filename */
static FILE	*fp_out;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static char	usage[] = "Usage: %s [-v] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		i;
	register int	c;
	double		percent;

	port_setlinebuf( stderr );

	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* tesselation tolerances
	 * only needed if you tessellate any solids
	 */
	ttol.magic = RT_TESS_TOL_MAGIC;
	/* Defaults, updated by command line options. */
	ttol.abs = 0.0;
	ttol.rel = 0.01;
	ttol.norm = 0.0;

	/* calculational tolerances
	 * mostly used by NMG routines
	 */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "t:a:n:o:r:vx:X:")) != EOF) {
		switch (c) {
		case 't':		/* calculational tolerance */
			tol.dist = atof( optarg );
			tol.dist_sq = tol.dist * tol.dist;
		case 'a':		/* Absolute tolerance. */
			ttol.abs = atof(optarg);
			ttol.rel = 0.0;
			break;
		case 'n':		/* Surface normal tolerance. */
			ttol.norm = atof(optarg)*bn_pi/180.0;
			ttol.rel = 0.0;
			break;
		case 'o':		/* Output file name */
			out_file = optarg;
			break;
		case 'r':		/* Relative tolerance. */
			ttol.rel = atof(optarg);
			break;
		case 'v':		/* verbosity */
			verbose++;
			break;
		case 'x':		/* librt debug flag (see librt/debug.h) */
			sscanf( optarg, "%x", &rt_g.debug );
			bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
			bu_log("\n");
			break;
		case 'X':		/* NMG debug flag (see h/nmg.h) */
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			NMG_debug = rt_g.NMG_debug;
			bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
			bu_log("\n");
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
		bu_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		exit(1);
	}

	/* scan all the records in the database and build a directory */
	db_dirbuild( dbip );

	/* open output file */
	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			bu_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			exit( 2 );
		}
	}

	optind++;

	/* Walk the trees named on the command line
	 * outputting combinations and solids
	 */
	for( i=optind ; i<argc ; i++ )
	{
		struct directory *dp;

		dp = db_lookup( dbip , argv[i] , 0 );
		if( dp == DIR_NULL )
		{
			bu_log( "WARNING!!! Could not find %s, skipping\n", argv[i] );
			continue;
		}
		db_functree( dbip , dp , comb_func , solid_func , NULL );
	}

	bn_vlist_cleanup();
	db_close(dbip);

	return 0;
}

/* This routine just produces an ascii description of the Boolean tree.
 * In a real converter, this would output the tree in the desired format.
 */
void
describe_tree( dbip, comb, tree, str )
struct db_i             *dbip;
struct rt_comb_internal *comb;
union tree              *tree;
struct bu_vls		*str;
{
	struct bu_vls left, right;
	char *unionn=" u ";
	char *sub=" - ";
	char *inter=" + ";
	char *xor=" ^ ";
	char *op=NULL;

	BU_CK_VLS(str);

	if( !tree )
	{
		/* this tree has no members */
		bu_vls_strcat( str, "-empty-" );
		return;
	}

	RT_CK_TREE(tree);

	/* Handle all the possible node types.
	 * the first four are the most common types, and are typically
	 * the only ones found in a BRL-CAD database.
	 */
	switch( tree->tr_op )
	{
		case OP_DB_LEAF:	/* leaf node, this is a member */
			/* Note: tree->tr_l.tl_mat is a pointer to a
			 * transformation matrix to apply to this member
			 */
			bu_vls_strcat( str,  tree->tr_l.tl_name );
			break;
		case OP_UNION:		/* union operator node */
			op = unionn;
			goto binary;
		case OP_INTERSECT:	/* intersection operator node */
			op = inter;
			goto binary;
		case OP_SUBTRACT:	/* subtraction operator node */
			op = sub;
			goto binary;
		case OP_XOR:		/* exclusive "or" operator node */
			op = xor;
binary:				/* common for all binary nodes */
			bu_vls_init( &left );
			bu_vls_init( &right );
			describe_tree( dbip, comb, tree->tr_b.tb_left, &left );
			describe_tree( dbip, comb, tree->tr_b.tb_right, &right );
			bu_vls_putc( str, '(' );
			bu_vls_vlscatzap( str, &left );
			bu_vls_strcat( str, op );
			bu_vls_vlscatzap( str, &right );
			bu_vls_putc( str, ')' );
			break;
		case OP_NOT:
			bu_vls_strcat( str, "(!" );
			describe_tree( dbip, comb, tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_GUARD:
			bu_vls_strcat( str, "(G" );
			describe_tree( dbip, comb, tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_XNOP:
			bu_vls_strcat( str, "(X" );
			describe_tree( dbip, comb, tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_NOP:
			bu_vls_strcat( str, "NOP" );
			break;
		default:
			bu_log( "ERROR: describe_tree() got unrecognized op (%d)\n", tree->tr_op );
			bu_bomb( "ERROR: bad op\n" );
	}
}

/* This function is called by the tree walker "db_functree" for each
 * combination encountered. This should either call a routine
 * to output the combination in your format, or do it itself.
 */
void
comb_func( dbip, dp, ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t	ptr;
{
	struct rt_db_internal itrn;
	struct rt_comb_internal *comb;
	int id;
	struct bu_vls str;

	/* check if we already output this object */
	if( dp->d_uses )
		return;

	/* get the internal format of this combination */
	if( (id=rt_db_get_internal( &itrn, dp, dbip, bn_mat_identity ) ) < 0 )
	{
		bu_log( "rt_db_get_internal failed for %s\n", dp->d_namep );
		return;
	}

	RT_CK_DB_INTERNAL( &itrn );

	if( id != ID_COMBINATION )
	{
		bu_log( "ERROR: comb_func called for a non-combination (%s)\n", dp->d_namep );
		exit( 1 );
	}

	/* get the combination structure */
	comb = (struct rt_comb_internal *)itrn.idb_ptr;

	/* here is where the conversion should be done */
	if( comb->region_flag )
		printf( "Write this region (name=%s) as a part in your format:\n", dp->d_namep );
	else
		printf( "Write this group (name=%s) as an assembly in your format:\n", dp->d_namep );

	bu_vls_init( &str );

	describe_tree( dbip, comb, comb->tree, &str );

	printf( "%s\n", bu_vls_addr( &str ) );

	bu_vls_free( &str );

	/* mark this object as converted */
	dp->d_uses++;
}

/* This routine is called by the tree walker (db_functree)
 * for every solid encountered in the trees specified on the command line */
void
solid_func( dbip, dp, ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t	ptr;
{
	struct rt_db_internal itrn;
	int id;

	/* check if we already converted this solid */
	if( dp->d_uses )
		return;

	/* get the internal form of the solid */
	if( (id=rt_db_get_internal( &itrn, dp, dbip, bn_mat_identity ) ) < 0 )
	{
		bu_log( "rt_db_get_internal failed for %s\n", dp->d_namep );
		return;
	}

	RT_CK_DB_INTERNAL( &itrn );

	/* handle each type of solid (see h/rtgeom.h) */
	switch( itrn.idb_type )
	{
		/* most commonly used solids */
		case ID_TOR:	/* torus */
		{
			struct rt_tor_internal *tor = (struct rt_tor_internal *)itrn.idb_ptr;

			printf( "Write this torus (name=%s) in your format:\n", dp->d_namep );
			printf( "\tV=(%g %g %g)\n", V3ARGS( tor->v ) );
			printf( "\tnormal=(%g %g %g)\n", V3ARGS( tor->h ) );
			printf( "\tradius1 = %g\n", tor->r_a );
			printf( "\tradius2 = %g\n", tor->r_h );
			break;
		}
		case ID_TGC: /* truncated general cone frustum */
		case ID_REC: /* right elliptical cylinder */
		{
			/* This solid includes circular cross-section
			 * cones and cylinders
			 */
			struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)itrn.idb_ptr;

			printf( "Write this TGC (name=%s) in your format:\n", dp->d_namep );
			printf( "\tV=(%g %g %g)\n", V3ARGS( tgc->v ) );
			printf( "\tH=(%g %g %g)\n", V3ARGS( tgc->h ) );
			printf( "\tA=(%g %g %g)\n", V3ARGS( tgc->a ) );
			printf( "\tB=(%g %g %g)\n", V3ARGS( tgc->b ) );
			printf( "\tC=(%g %g %g)\n", V3ARGS( tgc->c ) );
			printf( "\tD=(%g %g %g)\n", V3ARGS( tgc->d ) );
			break;
		}
		case ID_ELL:
		case ID_SPH:
		{
			/* spheres and ellipsoids */
			struct rt_ell_internal *ell = (struct rt_ell_internal *)itrn.idb_ptr;

			printf( "Write this ellipsoid (name=%s) in your format:\n", dp->d_namep );
			printf( "\tV=(%g %g %g)\n", V3ARGS( ell->v ) );
			printf( "\tA=(%g %g %g)\n", V3ARGS( ell->a ) );
			printf( "\tB=(%g %g %g)\n", V3ARGS( ell->b ) );
			printf( "\tC=(%g %g %g)\n", V3ARGS( ell->c ) );
			break;
		}
		case ID_ARB8:	/* convex solid with from four to six faces */
		{
			/* this solid may have degenerate faces
			 * faces are: 0123, 7654, 0347, 1562, 0451, 3267
			 * (points listed above in counter-clockwise order)
			 */
			struct rt_arb_internal *arb = (struct rt_arb_internal *)itrn.idb_ptr;
			int i;

			printf( "Write this ARB (name=%s) in your format:\n", dp->d_namep );
			for( i=0 ; i<8 ; i++ )
				printf( "\tpoint #%d: (%g %g %g)\n", i, V3ARGS( arb->pt[i] ) );
			break;
		}

		/* less commonly used solids */
		case ID_ARS:
		{
			/* series of curves
			 * each with the same number of points
			 */
			struct rt_ars_internal *ars = (struct rt_ars_internal *)itrn.idb_ptr;
			break;
		}
		case ID_HALF:
		{
			/* half universe defined by a plane */
			struct rt_half_internal *half = (struct rt_half_internal *)itrn.idb_ptr;
			break;
		}
		case ID_POLY:
		{
			/* polygons (up to 5 vertices per) */
			struct rt_pg_internal *pg = (struct rt_pg_internal *)itrn.idb_ptr;
			break;
		}
		case ID_BSPLINE:
		{
			/* NURB surfaces */
			struct rt_nurb_internal *nurb = (struct rt_nurb_internal *)itrn.idb_ptr;
			break;
		}
		case ID_NMG:
		{
			/* N-manifold geometry */
			struct model *m = (struct model *)itrn.idb_ptr;
			break;
		}
		case ID_ARBN:
		{
			struct rt_arbn_internal *arbn = (struct rt_arbn_internal *)itrn.idb_ptr;
			break;
		}

		/* normally used for terrain only */
		case ID_DSP:
		{
			/* Displacement map (terrain solid) */
			struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)itrn.idb_ptr;
			break;
		}
		case ID_HF:
		{
			/* height field (terrain solid) */
			struct rt_hf_internal *hf = (struct rt_hf_internal *)itrn.idb_ptr;
			break;
		}

		/* rarely used solids */
		case ID_EBM:
		{
			/* extruded bit-map */
			struct rt_ebm_internal *ebm = (struct rt_ebm_internal *)itrn.idb_ptr;
			break;
		}
		case ID_VOL:
		{
			struct rt_vol_internal *vol = (struct rt_vol_internal *)itrn.idb_ptr;
			break;
		}
		case ID_PIPE:
		{
			struct rt_pipe_internal *pipe = (struct rt_pipe_internal *)itrn.idb_ptr;
			break;
		}
		case ID_PARTICLE:
		{
			struct rt_part_internal *part = (struct rt_part_internal *)itrn.idb_ptr;
			break;
		}
		case ID_RPC:
		{
			struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)itrn.idb_ptr;
			break;
		}
		case ID_RHC:
		{
			struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)itrn.idb_ptr;
			break;
		}
		case ID_EPA:
		{
			struct rt_epa_internal *epa = (struct rt_epa_internal *)itrn.idb_ptr;
			break;
		}
		case ID_EHY:
		{
			struct rt_ehy_internal *ehy = (struct rt_ehy_internal *)itrn.idb_ptr;
			break;
		}
		case ID_ETO:
		{
			struct rt_eto_internal *eto = (struct rt_eto_internal *)itrn.idb_ptr;
			break;
		}
		case ID_GRIP:
		{
			struct rt_grip_internal *grip = (struct rt_grip_internal *)itrn.idb_ptr;
			break;
		}

		/* Unimplemented solids */
		case ID_SKETCH:
		{
			struct rt_sketch_internal *sketch = (struct rt_sketch_internal *)itrn.idb_ptr;
			break;
		}
		case ID_EXTRUDE:
		{
			struct rt_extrude_internal *extrude = (struct rt_extrude_internal *)itrn.idb_ptr;
			break;
		}

		default:
			bu_log( "Solid %s is unrecognized type (%d)\n", dp->d_namep, itrn.idb_type );
			break;
	}

	/* mark this solid as converted */
	dp->d_uses++;
}
