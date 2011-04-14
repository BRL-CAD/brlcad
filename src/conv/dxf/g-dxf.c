/*                         G - D X F . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2011 United States Government as represented by
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
/** @file g-dxf.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a DXF file by
 * calling on the NMG booleans.  Based on g-acad.c.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "gcv.h"

/* private headers */
#include "brlcad_version.h"
#include "./dxf.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN( a, b )	{\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


void
usage(const char *argv0)
{
    bu_log("Usage: %s [-v] [-i] [-p] [-xX lvl] \n\
       [-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol] [-D dist_calc_tol] \n\
       [-o output_file_name.dxf] brlcad_db.g object(s)\n\n", argv0);

    bu_log("Options:\n\
 -v	Verbose output\n\
 -i	Output using inches (instead of default mm)\n\
 -p	Output POLYFACE MESH (instead of default 3DFACE) entities\n\n");

    bu_log("\
 -x #	Specifies an RT debug flag\n\
 -X #	Specifies an NMG debug flag\n\n");

    bu_log("\
 -a #	Specify an absolute tessellation tolerance (in mm)\n\
 -r #	Specify a relative tessellation tolerance (in mm)\n\
 -n #	Specify a surface normal tessellation tolerance (in degrees)\n\
 -D #	Specify a calculation distance tolerance (in mm)\n\n");

    bu_log("\
 -o dxf	Output to the specified dxf filename\n\n---\n");
}

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

    for ( i=1; i<256; i++ ) {
	int tmp_dist;
	int diff[3];

	VSUB2( diff, icolor, &rgb[i*3] );
	tmp_dist = MAGSQ( diff );
	if ( tmp_dist < dist_sq ) {
	    dist_sq = tmp_dist;
	    color_num = i;
	}
    }

    return color_num;
}


static void
nmg_to_dxf( struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), float color[3] )
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
    for ( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
	    struct loopuse *lu;
	    int vert_count=0;

	    NMG_CK_FACEUSE( fu );

	    if ( fu->orientation != OT_SAME )
		continue;

	    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		struct edgeuse *eu;

		if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		    continue;

		for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) ) {
		    vert_count++;
		}

		if ( vert_count > 3 ) {
		    do_triangulate = 1;
		    goto triangulate;
		}

		tri_count++;
	    }
	}
    }

 triangulate:
    if ( do_triangulate ) {
	/* triangulate model */
	nmg_triangulate_model( m, &tol );

	/* Count triangles */
	tri_count = 0;
	for ( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	    struct faceuse *fu;

	    for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
		struct loopuse *lu;

		if ( fu->orientation != OT_SAME )
		    continue;

		for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		    if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		    tri_count++;
		}
	    }
	}
    }

    nmg_vertex_tabulate( &verts, &r->l.magic );

    color_num = find_closest_color( color );

    if ( polyface_mesh ) {
	size_t i;


	fprintf( fp, "0\nPOLYLINE\n8\n%s\n62\n%d\n70\n64\n71\n%lu\n72\n%d\n",
		 region_name, color_num, (unsigned long)BU_PTBL_LEN( &verts), tri_count );
	for ( i=0; i<BU_PTBL_LEN( &verts ); i++ ) {
	    fprintf( fp, "0\nVERTEX\n8\n%s\n", region_name );
	    v = (struct vertex *)BU_PTBL_GET( &verts, i );
	    NMG_CK_VERTEX( v );
	    if ( inches ) {
		fprintf( fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGSIN( v->vg_p->coord ) );
	    } else {
		fprintf( fp, "10\n%f\n20\n%f\n30\n%f\n70\n192\n", V3ARGS( v->vg_p->coord ) );
	    }
	}
    }

    /* Check triangles */
    for ( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) ) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE( fu );

	    if ( fu->orientation != OT_SAME )
		continue;

	    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) ) {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE( lu );

		if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		    continue;

		if ( polyface_mesh ) {
		    fprintf( fp, "0\nVERTEX\n8\n%s\n70\n128\n10\n0.0\n20\n0.0\n30\n0.0\n",
			     region_name );
		} else {
		    fprintf( fp, "0\n3DFACE\n8\n%s\n62\n%d\n", region_name, color_num );
		}

		/* check vertex numbers for each triangle */
		for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) ) {
		    NMG_CK_EDGEUSE( eu );

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX( v );

		    if ( polyface_mesh ) {
			fprintf( fp, "%d\n%d\n",
				 vert_count+70, bu_ptbl_locate( &verts, (long *)v ) + 1 );
		    } else {
			if ( inches ) {
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
		if ( vert_count > 3 ) {
		    bu_free( region_name, "region name" );
		    bu_log( "lu %p has %d vertices!\n", lu, vert_count );
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if ( vert_count < 3 ) {
		    continue;
		} else {
		    /* repeat the last vertex for the benefit of codes
		     * that interpret the dxf specification for
		     * 3DFACES as requiring a fourth vertex even when
		     * only three are input.
		     */
		    if ( !polyface_mesh ) {
			vert_count++;
			if ( inches ) {
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

    if ( polyface_mesh ) {
	fprintf( fp, "0\nSEQEND\n" );
    }

}


union tree *get_layer(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *UNUSED(curtree), genptr_t UNUSED(client_data))
{
    char *layer_name;
    int color_num;

    layer_name = db_path_to_string(pathp);
    color_num = find_closest_color( tsp->ts_mater.ma_color );

    fprintf( fp, "0\nLAYER\n2\n%s\n62\n%d\n", layer_name, color_num );

    bu_free( layer_name, "layer name" );

    return (union tree *)NULL;
}


/* FIXME: this be a dumb hack to avoid void* conversion */
struct gcv_data {
    void (*func)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);
};
static struct gcv_data gcvwriter = {nmg_to_dxf};


/**
 * M A I N
 *
 * This is the gist for what is going on (not verified):
 *
 * 1. initialize tree_state (db_tree_state)
 * 2. Deal with command line arguments. Strip off everything but regions for processing.
 * 3. Open geometry (.g) file and build directory db_dirbuild
 * 4. db_walk_tree (get_layer) for layer names only
 * 5. Initialize tree_state
 * 6. Initialize model (nmg)\
 * 7. db_walk_tree (gcv_region_end)
 * 8. Cleanup
 */
int
main(int argc, char *argv[])
{
    int	c;
    double		percent;
    int		i;

    bu_setlinebuf( stderr );

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
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-5;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource( &rt_uniresource, 0, NULL );

    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:n:o:pr:vx:D:P:X:i")) != -1) {
	switch (c) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		output_file = bu_optarg;
		break;
	    case 'p':
		polyface_mesh = 1;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi( bu_optarg );
		rt_g.debug = 1;	/* NOTE: enabling DEBUG_ALLRAYS to get core dumps */
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		break;
	    case 'D':
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol( &tol );
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
		NMG_debug = rt_g.NMG_debug;
		break;
	    case 'i':
		inches = 1;
		break;
	    default:
		usage(argv[0]);
		bu_exit(1, "%s\n", brlcad_ident("BRL-CAD to DXF Exporter"));
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	usage(argv[0]);
	bu_exit(1, "%s\n", brlcad_ident("BRL-CAD to DXF Exporter"));
    }

    if (!output_file) {
	fp = stdout;
#if defined(_WIN32) && !defined(__CYGWIN__)
	setmode(fileno(fp), O_BINARY);
#endif
    } else {
	/* Open output file */
	if ((fp=fopen(output_file, "w+b")) == NULL) {
	    perror( argv[0] );
	    bu_exit(1, " Cannot open output file (%s) for writing\n", output_file);
	}
    }

    /* Open BRL-CAD database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry file (%s) for reading\n", argv[0]);
    }

    if ( db_dirbuild( dbip ) ) {
	bu_exit(1, "db_dirbuild failed\n" );
    }

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    if ( verbose ) {
	bu_log( "Model: %s\n", argv[0] );
	bu_log( "Objects:" );
	for ( i=1; i<argc; i++ )
	    bu_log( " %s", argv[i] );
	bu_log( "\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
		tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm );
	bu_log( "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
		tree_state.ts_tol->dist, tree_state.ts_tol->perp );
    }

    /* output DXF header and start of TABLES section */
    fprintf(fp,
	    "0\nSECTION\n2\nHEADER\n999\n%s\n0\nENDSEC\n0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n",
	    argv[argc-1]);

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
			gcv_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)&gcvwriter);	/* callback for gcv_region_end */

    percent = 0;
    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	if ( verbose )
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   regions_tried, regions_converted, percent);
    }
    percent = 0;

    if ( regions_tried > 0 ) {
	percent = ((double)regions_written * 100) / regions_tried;
	if ( verbose )
	    bu_log( "                  %d triangulated successfully. %g%%\n",
		    regions_written, percent );
    }

    bu_log( "%ld triangles written\n", (long int)tot_polygons );

    fprintf( fp, "0\nENDSEC\n0\nEOF\n" );

    if ( output_file ) {
	fclose(fp);
    }

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

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
