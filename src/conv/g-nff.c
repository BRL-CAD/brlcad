/*                         G - N F F . C
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
/** @file conv/g-nff.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an NFF file by
 * calling on the NMG booleans.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"


extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);

static char	usage[] = "\
Usage: %s [-v][-i][-xX lvl][-a abs_tess_tol][-r rel_tess_tol][-n norm_tess_tol]\n\
[-e error_file ][-D dist_calc_tol] -o output_file_name brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	ncpu = 1;	/* Number of processors */
static char	*output_file = NULL;	/* output filename */
static char	*error_file = NULL;	/* error filename */
static FILE	*fp;		/* temp Output file pointer */
static FILE	*fpe;		/* Error file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;
static int	regions_written = 0;
static int	inches = 0;
static long	tot_polygons = 0;

static point_t	model_max;
static point_t	model_min;

#define	COPY_BUF_SIZE	512

/*
 *			M A I N
 */
int
main(argc, argv)
    int	argc;
    char	*argv[];
{
    FILE *fpf = NULL;	/* final output file */
    point_t		model_center;
    vect_t		view_dir;
    vect_t 		model_diag;
    fastf_t		bounding_radius;
    fastf_t		dist_to_eye;
    point_t		from;
    point_t		light_loc;
    char		buf[COPY_BUF_SIZE];
    size_t		read_size;
    int	c;
    double		percent;
    int		i;

    bu_setlinebuf( stderr );

    VSETALL( model_min, MAX_FASTF );
    VREVERSE( model_max, model_min );

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

    rt_init_resource( &rt_uniresource, 0, NULL );

    the_model = nmg_mm();
    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:n:o:r:vx:D:P:X:e:i")) != -1) {
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
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi( bu_optarg );
		rt_g.debug = 1;
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
	    case 'e':		/* Error file name. */
		error_file = bu_optarg;
		break;
	    case 'i':
		inches = 1;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    if ( !output_file ) {
	bu_exit(1, "No output file specified!\n" );
    }

    /* Open output file */
    if ( (fpf=fopen( output_file, "wb+" )) == NULL ) {
	perror( argv[0] );
	bu_exit(1, "Cannot open output file (%s) for writing\n", output_file );
    }

    /* Open temporary ouitput file */
    fp = bu_temp_file(NULL, 0);

    /* Open error log file */
    if (!error_file) {
	fpe = stderr;
#if defined(_WIN32) && !defined(__CYGWIN__)
	setmode(fileno(fpe), O_BINARY);
#endif
    } else if ((fpe=fopen( error_file, "wb")) == NULL) {
	perror( argv[0] );
	bu_exit(1, "Cannot open output file (%s) for writing\n", error_file );
    }

    /* Open BRL-CAD database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "ERROR: unable to open geometry database file (%s)\n", argv[0]);
    }
    if ( db_dirbuild( dbip ) ) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    fprintf( fpe, "Model: %s\n", argv[0] );
    fprintf( fpe, "Objects:" );
    for ( i=1; i<argc; i++ )
	fprintf( fpe, " %s", argv[i] );
    fprintf( fpe, "\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	     tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm );
    fprintf( fpe, "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	     tree_state.ts_tol->dist, tree_state.ts_tol->perp );

    bu_log( "Model: %s\n", argv[0] );
    bu_log( "Objects:" );
    for ( i=1; i<argc; i++ )
	bu_log( " %s", argv[i] );
    bu_log( "\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	    tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm );
    bu_log( "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	    tree_state.ts_tol->dist, tree_state.ts_tol->perp );

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			nmg_booltree_leaf_tess,
			(genptr_t)NULL);	/* in librt/nmg_bool.c */

    percent = 0;
    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	printf("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
    }
    percent = 0;

    if ( regions_tried > 0 ) {
	percent = ((double)regions_written * 100) / regions_tried;
	printf( "                  %d triangulated successfully. %g%%\n",
		regions_written, percent );
    }

    bu_log( "%ld triangles written\n", tot_polygons );
    fprintf( fpe, "%ld triangles written\n", tot_polygons );

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    /* write view information in the NFF file */
    fprintf( fpf, "v\n" );
    VADD2( model_center, model_max, model_min );
    VSCALE( model_center, model_center, 0.5 );
    VSET( view_dir, cos( M_PI_4 ) * cos( M_PI_4 ), cos( M_PI_4) * sin( M_PI_4 ), sin( M_PI_4 ) );
    VSUB2( model_diag, model_max, model_min );
    bounding_radius = 0.5 * MAGNITUDE( model_diag );
    dist_to_eye = 2.0 * bounding_radius / tan( M_PI_4 );
    VJOIN1( from, model_center, dist_to_eye, view_dir );

    /* from */
    fprintf( fpf, "from %g %g %g\n", V3ARGS( from ) );

    /* at */
    fprintf( fpf, "at %g %g %g\n", V3ARGS( model_center ) );

    /* up
     * this will only work for 45, 45 view
     */
    fprintf( fpf, "up %g %g %g\n", -view_dir[X], -view_dir[Y], view_dir[Z] );

    /* angle */
    fprintf( fpf, "angle 45\n" );

    /* hither */
    fprintf( fpf, "hither 0.0\n" );

    /* resolution */
    fprintf( fpf, "resolution 512 512\n" );

    /* a light */
    VJOIN1( light_loc, model_center, dist_to_eye + 10.0, view_dir );
    fprintf( fpf, "l %g %g %g 1 1 1\n", V3ARGS( light_loc ) );


    /* copy the temporary file to the final file */
    rewind( fp );
    while ( (read_size=fread( buf, 1, COPY_BUF_SIZE, fp ) ) ) {
	size_t ret;
	ret = fwrite( buf, read_size, 1, fpf );
	if (ret < 1)
	    perror("fwrite");
    }

    fclose( fpf );
    fclose(fp);

    return 0;
}


static void
nmg_to_nff(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;

    NMG_CK_REGION( r );
    RT_CK_FULL_PATH(pathp);

    m = r->m_p;
    NMG_CK_MODEL( m );

    /* triangulate model */
    nmg_triangulate_model( m, &tol );

    /* output triangles */
    for ( BU_LIST_FOR( s, shell, &r->s_hd ) )
    {
	struct faceuse *fu;

	NMG_CK_SHELL( s );

	for ( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
	    struct loopuse *lu;

	    NMG_CK_FACEUSE( fu );

	    if ( fu->orientation != OT_SAME )
		continue;

	    for ( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	    {
		struct edgeuse *eu;

		NMG_CK_LOOPUSE( lu );

		if ( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
		    continue;

		fprintf( fp, "p 3\n" );
		/* list vertices for each triangle */
		for ( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
		    NMG_CK_EDGEUSE( eu );

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX( v );

		    VMINMAX( model_min, model_max, v->vg_p->coord );
		    fprintf( fp, "%g %g %g\n", V3ARGS( v->vg_p->coord ) );

		}
		tot_polygons++;
	    }
	}
    }

}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the NFF file */
	nmg_to_nff( r, pathp, tsp->ts_regionid, tsp->ts_gmater );

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
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t UNUSED(client_data))
{
    union tree		*ret_tree;
    struct bu_list		vhead;
    struct nmgregion	*r;
    char			*region_name;

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
	return curtree;

    regions_tried++;

    printf("Attempting to process region %s\n", db_path_to_string( pathp ));
    fflush(stdout);

    ret_tree = process_boolean(curtree, tsp, pathp);

    if ( ret_tree )
	r = ret_tree->tr_d.td_r;
    else
    {
	if (verbose) {
	    printf( "\tNothing left of this region after Boolean evaluation\n" );
	    fprintf( fpe, "WARNING: Nothing left after Boolean evaluation: %s\n",
		     db_path_to_string( pathp ));
	    fflush(fpe);
	}
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

    BU_UNSETJUMP;		/* Relinquish the protection */
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
	    region_name = db_path_to_string( pathp );
	    fprintf( fp, "# %s\n", region_name);
	    bu_free( region_name, "region name" );

	    /* write the material properties to the NFF file */
	    fprintf( fp, "f %g %g %g 0.5 0.5 4.81884 0 0\n",
		     V3ARGS( tsp->ts_mater.ma_color ) );

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
     *  and there is no point to adding _another_ message to our output,
     *  so we need to cons up an OP_NOP node to return.
     */


    if (regions_tried>0) {
	float npercent, tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	printf("Tried %d regions, %d conv. to NMG's %d conv. to tri. nmgper = %.2f%% triper = %.2f%% \n",
	       regions_tried, regions_converted, regions_written, npercent, tpercent);
    }

    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    BU_GET(curtree, union tree);
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
