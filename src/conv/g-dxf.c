/*                         G - D X F . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2006 United States Government as represented by
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
/** @file g-dxf.c
 *
 *  Program to convert a BRL-CAD model (in a .g file) to a DXF file
 *  by calling on the NMG booleans.  Based on g-acad.c.
 *
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
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


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN( a, b )	{\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}

BU_EXTERN(union tree *do_region_end, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));
BU_EXTERN(union tree *get_layer, (struct db_tree_state *tsp, struct db_full_path *pathp, union tree *curtree, genptr_t client_data));


extern double nmg_eue_dist;		/* from nmg_plot.c */

static char	usage[] = "Usage: %s [-v][-i][-p][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol]\n\n[-D dist_calc_tol] [-o output_file_name.dxf] brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;	/* Number of processors */
static int	polyface_mesh = 0;	/* flag for output type (default is 3DFACE) */
static char	*output_file = NULL;	/* output filename */
static FILE	*fp;		/* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;	/* tesselation tolerance in mm */
static struct bn_tol		tol;	/* calculation tolerance */
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int		regions_tried = 0;
static int		regions_converted = 0;
static int		regions_written = 0;
static int		inches = 0;
static unsigned int	tot_polygons = 0;


static unsigned char rgb[]={
    0, 0, 0,
    255, 0, 0,
    255, 255, 0,
    0, 255, 0,
    0, 255, 255,
    0, 0, 255,
    255, 0, 255,
    255, 255, 255,
    65, 65, 65,
    128, 128, 128,
    255, 0, 0,
    255, 128, 128,
    166, 0, 0,
    166, 83, 83,
    128, 0, 0,
    128, 64, 64,
    77, 0, 0,
    77, 38, 38,
    38, 0, 0,
    38, 19, 19,
    255, 64, 0,
    255, 159, 128,
    166, 41, 0,
    166, 104, 83,
    128, 32, 0,
    128, 80, 64,
    77, 19, 0,
    77, 48, 38,
    38, 10, 0,
    38, 24, 19,
    255, 128, 0,
    255, 191, 128,
    166, 83, 0,
    166, 124, 83,
    128, 64, 0,
    128, 96, 64,
    77, 38, 0,
    77, 57, 38,
    38, 19, 0,
    38, 29, 19,
    255, 191, 0,
    255, 223, 128,
    166, 124, 0,
    166, 145, 83,
    128, 96, 0,
    128, 112, 64,
    77, 57, 0,
    77, 67, 38,
    38, 29, 0,
    38, 33, 19,
    255, 255, 0,
    255, 255, 128,
    166, 166, 0,
    166, 166, 83,
    128, 128, 0,
    128, 128, 64,
    77, 77, 0,
    77, 77, 38,
    38, 38, 0,
    38, 38, 19,
    191, 255, 0,
    223, 255, 128,
    124, 166, 0,
    145, 166, 83,
    96, 128, 0,
    112, 128, 64,
    57, 77, 0,
    67, 77, 38,
    29, 38, 0,
    33, 38, 19,
    128, 255, 0,
    191, 255, 128,
    83, 166, 0,
    124, 166, 83,
    64, 128, 0,
    96, 128, 64,
    38, 77, 0,
    57, 77, 38,
    19, 38, 0,
    29, 38, 19,
    64, 255, 0,
    159, 255, 128,
    41, 166, 0,
    104, 166, 83,
    32, 128, 0,
    80, 128, 64,
    19, 77, 0,
    48, 77, 38,
    10, 38, 0,
    24, 38, 19,
    0, 255, 0,
    128, 255, 128,
    0, 166, 0,
    83, 166, 83,
    0, 128, 0,
    64, 128, 64,
    0, 77, 0,
    38, 77, 38,
    0, 38, 0,
    19, 38, 19,
    0, 255, 64,
    128, 255, 159,
    0, 166, 41,
    83, 166, 104,
    0, 128, 32,
    64, 128, 80,
    0, 77, 19,
    38, 77, 48,
    0, 38, 10,
    19, 38, 24,
    0, 255, 128,
    128, 255, 191,
    0, 166, 83,
    83, 166, 124,
    0, 128, 64,
    64, 128, 96,
    0, 77, 38,
    38, 77, 57,
    0, 38, 19,
    19, 38, 29,
    0, 255, 191,
    128, 255, 223,
    0, 166, 124,
    83, 166, 145,
    0, 128, 96,
    64, 128, 112,
    0, 77, 57,
    38, 77, 67,
    0, 38, 29,
    19, 38, 33,
    0, 255, 255,
    128, 255, 255,
    0, 166, 166,
    83, 166, 166,
    0, 128, 128,
    64, 128, 128,
    0, 77, 77,
    38, 77, 77,
    0, 38, 38,
    19, 38, 38,
    0, 191, 255,
    128, 223, 255,
    0, 124, 166,
    83, 145, 166,
    0, 96, 128,
    64, 112, 128,
    0, 57, 77,
    38, 67, 77,
    0, 29, 38,
    19, 33, 38,
    0, 128, 255,
    128, 191, 255,
    0, 83, 166,
    83, 124, 166,
    0, 64, 128,
    64, 96, 128,
    0, 38, 77,
    38, 57, 77,
    0, 19, 38,
    19, 29, 38,
    0, 64, 255,
    128, 159, 255,
    0, 41, 166,
    83, 104, 166,
    0, 32, 128,
    64, 80, 128,
    0, 19, 77,
    38, 48, 77,
    0, 10, 38,
    19, 24, 38,
    0, 0, 255,
    128, 128, 255,
    0, 0, 166,
    83, 83, 166,
    0, 0, 128,
    64, 64, 128,
    0, 0, 77,
    38, 38, 77,
    0, 0, 38,
    19, 19, 38,
    64, 0, 255,
    159, 128, 255,
    41, 0, 166,
    104, 83, 166,
    32, 0, 128,
    80, 64, 128,
    19, 0, 77,
    48, 38, 77,
    10, 0, 38,
    24, 19, 38,
    128, 0, 255,
    191, 128, 255,
    83, 0, 166,
    124, 83, 166,
    64, 0, 128,
    96, 64, 128,
    38, 0, 77,
    57, 38, 77,
    19, 0, 38,
    29, 19, 38,
    191, 0, 255,
    223, 128, 255,
    124, 0, 166,
    145, 83, 166,
    96, 0, 128,
    112, 64, 128,
    57, 0, 77,
    67, 38, 77,
    29, 0, 38,
    33, 19, 38,
    255, 0, 255,
    255, 128, 255,
    166, 0, 166,
    166, 83, 166,
    128, 0, 128,
    128, 64, 128,
    77, 0, 77,
    77, 38, 77,
    38, 0, 38,
    38, 19, 38,
    255, 0, 191,
    255, 128, 223,
    166, 0, 124,
    166, 83, 145,
    128, 0, 96,
    128, 64, 112,
    77, 0, 57,
    77, 38, 67,
    38, 0, 29,
    38, 19, 33,
    255, 0, 128,
    255, 128, 191,
    166, 0, 83,
    166, 83, 124,
    128, 0, 64,
    128, 64, 96,
    77, 0, 38,
    77, 38, 57,
    38, 0, 19,
    38, 19, 29,
    255, 0, 64,
    255, 128, 159,
    166, 0, 41,
    166, 83, 104,
    128, 0, 32,
    128, 64, 80,
    77, 0, 19,
    77, 38, 48,
    38, 0, 10,
    38, 19, 24,
    84, 84, 84,
    118, 118, 118,
    152, 152, 152,
    187, 187, 187,
    221, 221, 221,
    255, 255, 255 };

/*
 *			M A I N
 */
int
main(argc, argv)
     int	argc;
     char	*argv[];
{
    register int	c;
    double		percent;
    int		i;

    bu_setlinebuf( stderr );

#if MEMORY_LEAK_CHECKING
    rt_g.debug |= DEBUG_MEM_FULL;
#endif
    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tesselation tolerance defaults */
    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* XXX These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-5;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource( &rt_uniresource, 0, NULL );

    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = getopt(argc, argv, "a:n:o:pr:vx:D:P:X:i:")) != EOF) {
	switch (c) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		output_file = optarg;
		break;
	    case 'p':
		polyface_mesh = 1;
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
		sscanf( optarg, "%x", (unsigned int *)&rt_g.debug );
		break;
	    case 'D':
		tol.dist = atof(optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol( &tol );
		break;
	    case 'X':
		sscanf( optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
		NMG_debug = rt_g.NMG_debug;
		break;
	    case 'i':
		inches = 1;
		break;
	    default:
		bu_log(  usage, argv[0]);
		exit(1);
		break;
	}
    }

    if (optind+1 >= argc) {
	bu_log( usage, argv[0]);
	exit(1);
    }

    if( !output_file  ) {
	fp = stdout;
    } else {
	/* Open output file */
	if( (fp=fopen( output_file, "w+" )) == NULL ) {
	    bu_log( "Cannot open output file (%s) for writing\n", output_file );
	    perror( argv[0] );
	    exit( 1 );
	}
    }

    /* Open BRL-CAD database */
    argc -= optind;
    argv += optind;
    if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
	perror(argv[0]);
	exit(1);
    }
    db_dirbuild( dbip );

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    if( verbose ) {
	bu_log( "Model: %s\n", argv[0] );
	bu_log( "Objects:" );
	for( i=1 ; i<argc ; i++ )
	    bu_log( " %s", argv[i] );
	bu_log( "\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
		tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm );
	bu_log( "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
		tree_state.ts_tol->dist, tree_state.ts_tol->perp );
    }

    /* output DXF header and start of TABLES section */
    fprintf( fp,
	     "0\nSECTION\n2\nHEADER\n999\n%s\n0\nENDSEC\n0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n",
	     argv[optind] );

    /* Walk indicated tree(s) just for layer names to put in TABLES section */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			get_layer,
			NULL,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */

    /* end of layers section, start of ENTOTIES SECTION */
    fprintf( fp, "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n" );

    /* Walk indicated tree(s).  Each region will be output separately */
    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    /* make empty NMG model */
    the_model = nmg_mm();
    tree_state.ts_m = &the_model;
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */

    percent = 0;
    if(regions_tried>0){
	percent = ((double)regions_converted * 100) / regions_tried;
	if( verbose )
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   regions_tried, regions_converted, percent);
    }
    percent = 0;

    if( regions_tried > 0 ){
	percent = ((double)regions_written * 100) / regions_tried;
	if( verbose )
	    bu_log( "                  %d triangulated successfully. %g%%\n",
		    regions_written, percent );
    }

    bu_log( "%ld triangles written\n", tot_polygons );

    fprintf( fp, "0\nENDSEC\n0\nEOF\n" );

    if( output_file ) {
	fclose(fp);
    }

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

#if MEMORY_LEAK_CHECKING
    bu_prmem("After complete G-DXF conversion");
#endif

    return 0;
}

static int
find_closest_color( float color[3] )
{
    int icolor[3];
    int i;
    int dist_sq;
    int color_num;

    VSCALE( icolor, color, 255 );

    color_num = 0;
    dist_sq = MAGSQ( icolor );

    for( i=1 ; i<256 ; i++ ) {
	int tmp_dist;
	int diff[3];

	VSUB2( diff, icolor, &rgb[i*3] );
	tmp_dist = MAGSQ( diff );
	if( tmp_dist < dist_sq ) {
	    dist_sq = tmp_dist;
	    color_num = i;
	}
    }

    return color_num;
}

static void
nmg_to_dxf( r, pathp, region_id, color )
     struct nmgregion *r;
     struct db_full_path *pathp;
     int region_id;
     float color[3];
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    char *region_name;
    int region_polys=0;
    int tri_count=0;
    int color_num;
    int do_triangulate=0;

    NMG_CK_REGION( r );
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string( pathp );

    m = r->m_p;
    NMG_CK_MODEL( m );

    /* Count triangles */
    for( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
	    struct loopuse *lu;
	    int vert_count=0;

	    NMG_CK_FACEUSE( fu );

	    if( fu->orientation != OT_SAME )
		continue;

	    for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		struct edgeuse *eu;

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		    continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) ) {
		    vert_count++;
		}

		if( vert_count > 3 ) {
		    do_triangulate = 1;
		    goto triangulate;
		}

		tri_count++;
	    }
	}
    }

 triangulate:
    if( do_triangulate ) {
	/* triangulate model */
	nmg_triangulate_model( m, &tol );

	/* Count triangles */
	tri_count = 0;
	for( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	    struct faceuse *fu;

	    for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
		struct loopuse *lu;

		if( fu->orientation != OT_SAME )
		    continue;

		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		    if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		    tri_count++;
		}
	    }
	}
    }

    nmg_vertex_tabulate( &verts, &r->l.magic );

    color_num = find_closest_color( color );

    if( polyface_mesh ) {
	int i;


	fprintf( fp, "0\nPOLYLINE\n8\n%s\n62\n%d\n70\n64\n71\n%d\n72\n%d\n",
		 region_name, color_num, BU_PTBL_LEN( &verts), tri_count );
	for( i=0 ; i<BU_PTBL_LEN( &verts ) ; i++ ) {
	    fprintf( fp, "0\nVERTEX\n8\n%s\n", region_name );
	    v = (struct vertex *)BU_PTBL_GET( &verts, i );
	    NMG_CK_VERTEX( v );
	    if( inches ) {
		fprintf( fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGSIN( v->vg_p->coord ) );
	    } else {
		fprintf( fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGS( v->vg_p->coord ) );
	    }
	}
    }

    /* Check triangles */
    for( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE( fu );

	    if( fu->orientation != OT_SAME )
		continue;

	    for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE( lu );

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		    continue;

		if( polyface_mesh ) {
		    fprintf( fp, "0\nVERTEX\n8\n%s\n70\n128\n10\n0.0\n20\n0.0\n30\n0.0\n",
			     region_name );
		} else {
		    fprintf( fp, "0\n3DFACE\n8\n%s\n62\n%d\n", region_name, color_num );
		}

		/* check vertex numbers for each triangle */
		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) ) {
		    NMG_CK_EDGEUSE( eu );

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX( v );

		    if( polyface_mesh ) {
			fprintf( fp, "%d\n%d\n",
				 vert_count+70, bu_ptbl_locate( &verts, (long *)v ) + 1 );
		    } else {
			if( inches ) {
			    fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X] / 25.4,
				     20 + vert_count - 1,
				     v->vg_p->coord[Y] / 25.4,
				     30 + vert_count -1,
				     v->vg_p->coord[Z] / 25.4 );
			} else {
			    fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X],
				     20 + vert_count - 1,
				     v->vg_p->coord[Y],
				     30 + vert_count -1,
				     v->vg_p->coord[Z] );
			}
		    }
		}
		if( vert_count > 3 ) {
		    bu_free( region_name, "region name" );
		    bu_log( "lu x%x has %d vertices!!!!\n", lu, vert_count );
		    rt_bomb( "LU is not a triangle" );
		} else if( vert_count < 3 ) {
		    continue;
		} else {
		    /* repeat the last vertex for the benefit of codes
		     * that interpret the dxf specification for
		     * 3DFACES as requiring a fourth vertex even when
		     * only three are input.
		     */
		    if( !polyface_mesh ) {
			vert_count++;
			if( inches ) {
			    fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X] / 25.4,
				     20 + vert_count - 1,
				     v->vg_p->coord[Y] / 25.4,
				     30 + vert_count -1,
				     v->vg_p->coord[Z] / 25.4 );
			} else {
			    fprintf( fp, "%d\n%f\n%d\n%f\n%d\n%f\n",
				     10 + vert_count - 1,
				     v->vg_p->coord[X],
				     20 + vert_count - 1,
				     v->vg_p->coord[Y],
				     30 + vert_count -1,
				     v->vg_p->coord[Z] );
			}
		    }
		}

		tot_polygons++;
		region_polys++;
	    }
	}
    }

    bu_ptbl_free( &verts );
    bu_free( region_name, "region name" );

    if( polyface_mesh ) {
	fprintf( fp, "0\nSEQEND\n" );
    }

}

union tree *get_layer(tsp, pathp, curtree, client_data)
     register struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     union tree		*curtree;
     genptr_t		client_data;
{
    char *layer_name;
    int color_num;

    layer_name = db_path_to_string(pathp);
    color_num = find_closest_color( tsp->ts_mater.ma_color );

    fprintf( fp, "0\nLAYER\n2\n%s\n62\n%d\n", layer_name, color_num );

    bu_free( layer_name, "layer name" );

    return( (union tree *)NULL );
}

/*
 *			D O _ R E G I O N _ E N D
 *
 *  Called from db_walk_tree().
 *
 *  This routine must be prepared to run in parallel.
 */
union tree *do_region_end(tsp, pathp, curtree, client_data)
     register struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     union tree		*curtree;
     genptr_t		client_data;
{
    union tree		*ret_tree;
    struct bu_list		vhead;
    struct nmgregion	*r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
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
    if( ncpu == 1 ) {
	if( BU_SETJUMP )  {
	    /* Error, bail out */
	    char *sofar;
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

	    /* Release the tree memory & input regions */
	    /*XXX*/			/* db_free_tree(curtree);*/		/* Does an nmg_kr() */

	    /* Get rid of (m)any other intermediate structures */
	    if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC )  {
		nmg_km(*tsp->ts_m);
	    } else {
		bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	    }

	    /* Now, make a new, clean model structure for next pass. */
	    *tsp->ts_m = nmg_mm();
	    goto out;
	}
    }
    if( verbose )
	bu_log("Attempting to process region %s\n",db_path_to_string( pathp ));

    ret_tree = nmg_booltree_evaluate( curtree, tsp->ts_tol, &rt_uniresource );	/* librt/nmg_bool.c */
    BU_UNSETJUMP;		/* Relinquish the protection */

    if( ret_tree ) {
	r = ret_tree->tr_d.td_r;
    } else {
	if( verbose ) {
	    bu_log( "\tNothing left of this region after Boolean evaluation\n" );
	}
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }
    /*	regions_done++;  XXX */

    regions_converted++;

    if (r != (struct nmgregion *)NULL) {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST( shell, &r->s_hd );
	while( BU_LIST_NOT_HEAD( &s->l, &r->s_hd ) ) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT( shell, &s->l );
	    if( nmg_kill_cracks( s ) ) {
		if( nmg_ks( s ) ) {
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if( !empty_region ) {
	    empty_model = nmg_kill_zero_length_edgeuses( *tsp->ts_m );
	}

	if( !empty_region && !empty_model ) {
	    if( BU_SETJUMP ) {
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
		if( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC ) {
		    nmg_km(*tsp->ts_m);
		} else {
		    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
		}

		/* Now, make a new, clean model structure for next pass. */
		*tsp->ts_m = nmg_mm();
		goto out;
	    }
	    /* Write the region to the DXF file */
	    nmg_to_dxf( r, pathp, tsp->ts_regionid, tsp->ts_mater.ma_color );

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
     *  and there is no point to adding _another_ message to our output,
     *  so we need to cons up an OP_NOP node to return.
     */


    if(regions_tried>0){
	float npercent, tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	if( verbose )
	    bu_log("Tried %d regions, %d conv. to NMG's %d conv. to tri. nmgper = %.2f%% triper = %.2f%% \n",
		   regions_tried, regions_converted, regions_written, npercent,tpercent);
    }

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
