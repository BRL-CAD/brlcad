/*                         G - X X X . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
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
/** @file g-xxx.c
 *
 *	Sample code for converting BRL-CAD models to some other format.
 *	This code assumes that your receiving format can handle CSG primitives
 *	and Boolean trees with transformation matrices
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

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"


BU_EXTERN( void comb_func , ( struct db_i *dbip , struct directory *dp, genptr_t data ) );
BU_EXTERN( void primitive_func , ( struct db_i *dbip , struct directory *dp, genptr_t data ) );

static int	verbose;
static struct db_i		*dbip;
static struct bn_tol		tol;


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

	port_setlinebuf( stderr );

	rt_init_resource(&rt_uniresource, 0, NULL);
	
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
		case 'o':		/* Output file name */
			/* grab output file name */
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

	optind++;

	/* Walk the trees named on the command line
	 * outputting combinations and primitives
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
		db_functree( dbip, dp, comb_func, primitive_func, &rt_uniresource, NULL );
	}

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
	if( (id=rt_db_get_internal( &itrn, dp, dbip, bn_mat_identity, &rt_uniresource ) ) < 0 )
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
		printf( "Write this combination (name=%s) as an assembly in your format:\n", dp->d_namep );

	bu_vls_init( &str );

	describe_tree( dbip, comb, comb->tree, &str );

	printf( "\t%s\n\n", bu_vls_addr( &str ) );

	bu_vls_free( &str );

	/* mark this object as converted */
	dp->d_uses++;
}

/* This routine is called by the tree walker (db_functree)
 * for every primitive encountered in the trees specified on the command line */
void
primitive_func( dbip, dp, ptr )
struct db_i *dbip;
struct directory *dp;
genptr_t	ptr;
{
	struct rt_db_internal itrn;

	/* check if we already converted this primitive */
	if( dp->d_uses )
		return;

	/* get the internal form of the primitive */
	if( rt_db_get_internal( &itrn, dp, dbip, bn_mat_identity, &rt_uniresource ) < 0 )
	{
		bu_log( "rt_db_get_internal failed for %s\n", dp->d_namep );
		return;
	}

	RT_CK_DB_INTERNAL( &itrn );

	/* handle each type of primitive (see h/rtgeom.h) */
	if( itrn.idb_major_type == DB5_MAJORTYPE_BRLCAD ) {
		switch( itrn.idb_type )
			{
				/* most commonly used primitives */
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
					/* This primitive includes circular cross-section
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
			case ID_ARB8:	/* convex primitive with from four to six faces */
				{
					/* this primitive may have degenerate faces
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
			case ID_BOT:	/* Bag O' Triangles */
				{
					struct rt_bot_internal *bot = (struct rt_bot_internal *)itrn.idb_ptr;
					break;
				}

				/* less commonly used primitives */
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

			case ID_DSP:
				{
					/* Displacement map (terrain primitive) */
					/* normally used for terrain only */
					/* the DSP primitive may reference an external file */
					struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)itrn.idb_ptr;
					break;
				}
			case ID_HF:
				{
					/* height field (terrain primitive) */
					/* the HF primitive references an external file */
					struct rt_hf_internal *hf = (struct rt_hf_internal *)itrn.idb_ptr;
					break;
				}

				/* rarely used primitives */
			case ID_EBM:
				{
					/* extruded bit-map */
					/* the EBM primitive references an external file */
					struct rt_ebm_internal *ebm = (struct rt_ebm_internal *)itrn.idb_ptr;
					break;
				}
			case ID_VOL:
				{
					/* the VOL primitive references an external file */
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
				
			case ID_SKETCH:
				{
					struct rt_sketch_internal *sketch = (struct rt_sketch_internal *)itrn.idb_ptr;
					break;
				}
			case ID_EXTRUDE:
				{
					/* note that an extrusion references a sketch, make sure you convert
					 * the sketch also
					 */
					struct rt_extrude_internal *extrude = (struct rt_extrude_internal *)itrn.idb_ptr;
					break;
				}
				
			default:
				bu_log( "Primitive %s is unrecognized type (%d)\n", dp->d_namep, itrn.idb_type );
				break;
			}
	} else {
		switch( itrn.idb_major_type ) {
			case DB5_MAJORTYPE_BINARY_UNIF:
				{
					/* not actually a primitive, just a block of storage for data
					 * a uniform array of chars, ints, floats, doubles, ...
					 */
					struct rt_binunif_internal *bin = (struct rt_binunif_internal *)itrn.idb_ptr;

					printf( "Found a binary object (%s)\n\n", dp->d_namep );
					break;
				}
			default:
				bu_log( "Major type of %s is unrecognized type (%d)\n", dp->d_namep, itrn.idb_major_type );
				break;
		}
	}

	/* mark this primitive as converted */
	dp->d_uses++;
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
