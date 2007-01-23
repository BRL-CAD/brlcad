/*                       N M G - S G P . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2007 United States Government as represented by
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
/** @file nmg-sgp.c
 *
 *  Program to convert an NMG BRL-CAD model (in a .g file) to a SGP facetted model
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <errno.h>
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

static int debug=0;
static int verbose=0;
static FILE *fp_out;
static char *out_file;
static FILE *fd_in;
static struct db_i *dbip;
static struct bn_tol tol;
static char *usage="Usage:\n\t%s [-d] [-v] [-x librt_debug_flag] [-X NMG_debug_flag] [-o output_file] brlcad_model.g object1 [object2 object3...]\n";
static long polygons=0;
static int stats=0;
static int triangles=0;
static fastf_t min_edge_len=MAX_FASTF;
static fastf_t max_edge_len=0.0;
static int *edge_count;
static fastf_t *edge_len_limits;
static point_t min_pt, max_pt;

static void
write_fu_as_sgp( fu )
struct faceuse *fu;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE( fu );

	nmg_triangulate_fu( fu, &tol );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		fprintf( fp_out, "polygon 3\n" );
		polygons++;
		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			struct vertex_g *vg;

			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G( vg );
			fprintf( fp_out, "%f %f %f\n", vg->coord[X], -vg->coord[Y], -vg->coord[Z] );
		}
	}
}

static void
write_model_as_sgp( m )
struct model *m;
{
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;

	NMG_CK_MODEL( m );

	for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
	{
		for( BU_LIST_FOR( s, shell, &r->s_hd ) )
		{
			for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
			{
				if( fu->orientation != OT_SAME )
					continue;

				write_fu_as_sgp( fu );
			}
		}
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
	register int	c;
	struct db_i	*dbip;
	char		idbuf[132];
	vect_t		h_delta, v_delta;
	point_t		start_pt;
	int		dir;
	fastf_t		cell_width=100.0, cell_height=100.0;

	bu_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
	rt_g.debug |= DEBUG_MEM_FULL;
#endif
	BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

	/* XXX These need to be improved */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.1;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "do:vx:X:")) != EOF) {
		switch (c) {
		case 'd':		/* increment debug level */
			debug++;
			break;
		case 'o':		/* Output file name */
			out_file = optarg;
			break;
		case 's':		/* edge length statistics */
			stats = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
			bu_log("\n");
			break;
		case 'X':
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
			bu_log("\n");
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			return 1;
			break;
		}
	}

	if (optind+1 >= argc) {
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	/* Open BRL-CAD database */
	if( (dbip = db_open( argv[optind], "r" )) == DBI_NULL )
	{
		bu_log( "Cannot open %s\n" , argv[optind] );
		perror(argv[0]);
		return 1;
	}
#if 0
	if( stats )
	{
		edge_len_limits =
	}
#endif
	db_dirbuild( dbip );

	if( out_file == NULL )
		fp_out = stdout;
	else
	{
		if ((fp_out = fopen( out_file , "w")) == NULL)
		{
			bu_log( "Cannot open %s\n" , out_file );
			perror( argv[0] );
			return 1;
		}
	}

	fprintf( fp_out, "object\n" );
	while( ++optind < argc )
	{
		struct directory *dp;
		struct rt_db_internal ip;
		int id;
		struct model *m;

		if( (dp=db_lookup( dbip, argv[optind], LOOKUP_NOISY)) == DIR_NULL )
			continue;

		if( (id=rt_db_get_internal( &ip, dp, dbip, bn_mat_identity, &rt_uniresource )) < 0 )
		{
			bu_log( "Cannot get object (%s).....ignoring\n", dp->d_namep );
			continue;
		}

		if( id != ID_NMG )
		{
			bu_log( "%s is not an NMG......ignoring\n", dp->d_namep );
			rt_db_free_internal( &ip, &rt_uniresource );
			continue;
		}

		m = (struct model *)ip.idb_ptr;
		NMG_CK_MODEL( m );

		write_model_as_sgp( m );
		rt_db_free_internal( &ip, &rt_uniresource );
	}

	fprintf( fp_out, "end_object\n" );

	bu_log( "\t%d polygons\n", polygons );
	
	return 0;
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
