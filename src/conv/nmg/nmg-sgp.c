/*                       N M G - S G P . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
 * Program to convert an NMG BRL-CAD model (in a .g file) to a SGP
 * facetted model
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


static int debug=0;
static int verbose=0;
static FILE *fp_out;
static char *out_file;
static struct bn_tol tol;
static const char *usage="Usage:\n\t%s [-d] [-v] [-x librt_debug_flag] [-X NMG_debug_flag] [-o output_file] brlcad_model.g object1 [object2 object3...]\n";
static long polygons=0;
static int stats=0;

static void
write_fu_as_sgp( fu )
    struct faceuse *fu;
{
    struct loopuse *lu;

    NMG_CK_FACEUSE( fu );

    nmg_triangulate_fu( fu, &tol );

    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
    {
	struct edgeuse *eu;

	if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
	    continue;

	fprintf( fp_out, "polygon 3\n" );
	polygons++;
	for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
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

    for ( BU_LIST_FOR( r, nmgregion, &m->r_hd ) )
    {
	for ( BU_LIST_FOR( s, shell, &r->s_hd ) )
	{
	    for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	    {
		if ( fu->orientation != OT_SAME )
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
main(int argc, char *argv[])
{
    int	c;
    struct db_i	*dbip;

    bu_setlinebuf( stderr );

    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "do:vx:X:")) != -1) {
	switch (c) {
	    case 'd':		/* increment debug level */
		debug++;
		break;
	    case 'o':		/* Output file name */
		out_file = bu_optarg;
		break;
	    case 's':		/* edge length statistics */
		stats = 1;
		break;
	    case 'v':
		verbose++;
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", &rt_g.debug );
		bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
		bu_log("\n");
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", &rt_g.NMG_debug );
		bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
		bu_log("\n");
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
    if ( (dbip = db_open( argv[bu_optind], "r" )) == DBI_NULL )
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
	    perror( argv[0] );
	    bu_exit(1, "Cannot open %s\n", out_file );
	}
    }

    fprintf( fp_out, "object\n" );
    while ( ++bu_optind < argc )
    {
	struct directory *dp;
	struct rt_db_internal ip;
	int id;
	struct model *m;

	if ( (dp=db_lookup( dbip, argv[bu_optind], LOOKUP_NOISY)) == RT_DIR_NULL )
	    continue;

	if ( (id=rt_db_get_internal( &ip, dp, dbip, bn_mat_identity, &rt_uniresource )) < 0 )
	{
	    bu_log( "Cannot get object (%s).....ignoring\n", dp->d_namep );
	    continue;
	}

	if ( id != ID_NMG )
	{
	    bu_log( "%s is not an NMG......ignoring\n", dp->d_namep );
	    rt_db_free_internal(&ip);
	    continue;
	}

	m = (struct model *)ip.idb_ptr;
	NMG_CK_MODEL( m );

	write_model_as_sgp( m );
	rt_db_free_internal(&ip);
    }

    fprintf( fp_out, "end_object\n" );

    bu_log( "\t%d polygons\n", polygons );

    return 0;
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
