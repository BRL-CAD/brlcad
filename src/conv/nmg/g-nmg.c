/*                         G - N M G . C
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
/** @file g-nmg.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an NMG
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
#include "bu.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "mater.h"


extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);

static char	usage[] = "Usage: %s [-v] [-b] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

static char	*tok_sep = " \t";
static int	NMG_debug;		/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	do_bots=0;		/* flag to output BOT's instead of NMG's */
/* static int	ncpu = 1; */		/* Number of processors */
static int	nmg_count=0;		/* Count of nmgregions written to output */
static char	*out_file = "nmg.g";	/* Output filename */
static struct rt_wdb		*fp_out; /* Output file pointer */
static struct db_i		*dbip;
static struct rt_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_converted = 0;

/* extern struct mater* rt_material_head; */


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    union tree *ret_tree = NULL;

    /* Begin bomb protection */
    if ( !BU_SETJUMP ) {
	/* try */

	ret_tree = nmg_booltree_evaluate(curtree, tsp->ts_tol, &rt_uniresource);	/* librt/nmg_bool.c */

    } else {
	/* catch */

	/* Error, bail out */
	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log( "FAILED: %s\n", sofar );
	bu_free( (char *)sofar, "sofar" );

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	rt_g.NMG_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree, &rt_uniresource);	/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ( (*tsp->ts_m)->magic == NMG_MODEL_MAGIC ) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    } BU_UNSETJUMP;		/* Relinquish the protection */

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
    char			*sofar;
    struct bu_vls		shader_params;
    char nmg_name[16];
    unsigned char rgb[3];
    unsigned char *color = (unsigned char *)NULL;
    char *shader;
    char *matparm;
    struct wmember headp;
    struct rt_bot_internal *bot = NULL;

    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&DEBUG_TREEWALK || verbose) {
	sofar = db_path_to_string(pathp);
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

    regions_converted++;

    if ( ret_tree ) {
	r = ret_tree->tr_d.td_r;
	if ( do_bots && r ) {
	    bot = nmg_bot( BU_LIST_FIRST( shell, &r->s_hd ), tsp->ts_tol );
	}
    } else {
	if (verbose) {
	    printf( "\tNothing left of this region after Boolean evaluation\n" );
	}
	r = (struct nmgregion *)NULL;
    }

    if (tsp->ts_mater.ma_shader) {
	shader = strtok( tsp->ts_mater.ma_shader, tok_sep );
	matparm = strtok( (char *)NULL, tok_sep );
    } else {
	shader = NULL;
	matparm = NULL;
    }

    bu_vls_init( &shader_params );
    if ( matparm ) {
	bu_vls_strcpy( &shader_params, matparm );
	matparm = strtok( (char *)NULL, tok_sep );
	while ( matparm ) {
	    bu_vls_putc( &shader_params, ' ' );
	    bu_vls_strcat( &shader_params, matparm );
	    matparm = strtok( (char *)NULL, tok_sep );
	}
    }
    if (r != (struct nmgregion *)NULL) {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST( shell, &r->s_hd );
	while ( BU_LIST_NOT_HEAD( &s->l, &r->s_hd ) ) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT( shell, &s->l );
	    if ( nmg_kill_cracks( s ) ) {
		if ( nmg_ks( s ) ) {
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if ( !empty_region ) {
	    empty_model = nmg_kill_zero_length_edgeuses( *tsp->ts_m );
	}

	if ( !empty_region && !empty_model ) {
	    /* Write the nmgregion to the output file */
	    nmg_count++;
	    sprintf( nmg_name, "nmg.%d", nmg_count );

	    if ( do_bots ) {
		wdb_export( fp_out, nmg_name, (genptr_t)bot, ID_BOT, 1.0 );
	    } else {
		mk_nmg( fp_out, nmg_name, r->m_p );
	    }
	}

	/* Now make a normal brlcad region */
	if ( tsp->ts_mater.ma_color_valid ) {
	    rgb[0] = (int)(tsp->ts_mater.ma_color[0] * 255.0);
	    rgb[1] = (int)(tsp->ts_mater.ma_color[1] * 255.0);
	    rgb[2] = (int)(tsp->ts_mater.ma_color[2] * 255.0);
	    color = rgb;
	}
	else
	    color = (unsigned char *)NULL;

	BU_LIST_INIT( &headp.l );
	(void)mk_addmember( nmg_name, &headp.l, NULL, WMOP_UNION );
	if ( mk_lrcomb( fp_out,
			pathp->fp_names[pathp->fp_len-1]->d_namep, &headp, 1,
			shader, bu_vls_addr( &shader_params ), color,
			tsp->ts_regionid, tsp->ts_aircode, tsp->ts_gmater,
			tsp->ts_los, tsp->ts_mater.ma_cinherit ) ) {
	    bu_log( "G-nmg: error in making region (%s)\n", pathp->fp_names[pathp->fp_len-1]->d_namep );
	}
    } else {
	BU_LIST_INIT( &headp.l );
	if ( mk_lrcomb( fp_out,
			pathp->fp_names[pathp->fp_len-1]->d_namep, &headp, 1,
			shader, bu_vls_addr( &shader_params ), color,
			tsp->ts_regionid, tsp->ts_aircode, tsp->ts_gmater,
			tsp->ts_los, tsp->ts_mater.ma_cinherit ) ) {
	    bu_log( "G-nmg: error in making region (%s)\n", pathp->fp_names[pathp->fp_len-1]->d_namep );
	}
    }

    bu_vls_free( &shader_params );

    /*
     *  Dispose of original tree, so that all associated dynamic
     *  memory is released now, not at the end of all regions.
     *  A return of TREE_NULL from this routine signals an error,
     *  so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    if ( RT_G_DEBUG&DEBUG_MEM_FULL )
	bu_prmem( "At end of do_region_end()" );

    BU_GETUNION(curtree, tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}

void
csg_comb_func(struct db_i *db, struct directory *dp, genptr_t UNUSED(ptr))
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *tree_list;
    int node_count;
    int actual_count;
    int i;
    struct wmember headp;
    struct wmember *wm;
    unsigned char *color;
    char *endp;
    size_t len;
    char matname[32];
    char matparm[60];

    if ( dp->d_uses < 0 )
	return;

    dp->d_uses = (-1);

    if ( dp->d_flags & RT_DIR_REGION ) {
	char **name;

	/* convert a region to NMG's */

	the_model = nmg_mm();
	tree_state = rt_initial_tree_state;	/* struct copy */
	tree_state.ts_tol = &tol;
	tree_state.ts_ttol = &ttol;
	tree_state.ts_m = &the_model;

	name = (&(dp->d_namep));

	(void) db_walk_tree( db, 1, (const char **)name,
			     1,
			     &tree_state,
			     0,
			     do_region_end,
			     nmg_booltree_leaf_tess,
			     (genptr_t)NULL);

	/* Release dynamic storage */
	nmg_km(the_model);

	return;
    }

    /* have a combination that is not a region */

    if ( rt_db_get_internal( &intern, dp, db, (fastf_t *)NULL, &rt_uniresource ) < 0 ) {
	bu_log( "Cannot get internal form of combination (%s)\n", dp->d_namep );
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB( comb );

    if ( verbose )
	bu_log( "Combination - %s\n", dp->d_namep );

    if ( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )	{
	db_non_union_push( comb->tree, &rt_uniresource );
	if ( db_ck_v4gift_tree( comb->tree ) < 0 ) {
	    bu_log( "Cannot flatten tree (%s) for editing\n", dp->d_namep );
	    return;
	}
    }
    node_count = db_tree_nleaves( comb->tree );
    if ( node_count > 0 ) {
	tree_list = (struct rt_tree_array *)bu_calloc( node_count,
						       sizeof( struct rt_tree_array ), "tree list" );
	actual_count = (struct rt_tree_array *)db_flatten_tree( tree_list,
								comb->tree, OP_UNION, 0, &rt_uniresource ) - tree_list;
	BU_ASSERT_LONG( actual_count, ==, node_count );
    }
    else {
	tree_list = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    if ( actual_count < 1 ) {
	bu_log( "Warning: empty combination (%s)\n", dp->d_namep );
	dp->d_uses = 0;
	rt_db_free_internal(&intern);
	return;
    }

    BU_LIST_INIT( &headp.l );

    for ( i=0; i<actual_count; i++ ) {
	char op;

	switch ( tree_list[i].tl_op ) {
	    case OP_UNION:
		op = 'u';
		break;
	    case OP_INTERSECT:
		op = '+';
		break;
	    case OP_SUBTRACT:
		op = '-';
		break;
	    default:
		bu_log( "Unrecognized Boolean operator in combination (%s)\n", dp->d_namep );
		bu_free( (char *)tree_list, "tree_list" );
		rt_db_free_internal(&intern);
		return;
	}
	wm = mk_addmember( tree_list[i].tl_tree->tr_l.tl_name, &headp.l, NULL, op );
	if ( tree_list[i].tl_tree->tr_l.tl_mat )
	    MAT_COPY( wm->wm_mat, tree_list[i].tl_tree->tr_l.tl_mat );
    }

    if ( comb->rgb_valid  )
	color = comb->rgb;
    else
	color = (unsigned char *)NULL;

    endp = strchr( bu_vls_addr(&comb->shader), ' ' );
    if ( endp ) {
	len = endp - bu_vls_addr(&comb->shader);
	if( len > sizeof(matname) ) len = sizeof(matname);
	bu_strlcpy( matname, bu_vls_addr(&comb->shader), len );
	bu_strlcpy( matparm, endp+1, sizeof(matparm) );
    }
    else {
	bu_strlcpy( matname, bu_vls_addr(&comb->shader), sizeof(matname) );
	matparm[0] = '\0';
    }

    if ( mk_lrcomb( fp_out, dp->d_namep, &headp, comb->region_flag,
		    matname, matparm,
		    color, comb->region_id,
		    comb->aircode, comb->GIFTmater, comb->los,
		    comb->inherit ) ) {
	bu_log( "G-nmg: error in making region (%s)\n", dp->d_namep );
    }
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
    int		i;
    int	c;
    double		percent;

    bu_setlinebuf( stderr );

    BU_LIST_INIT( &rt_g.rtg_vlfree );	/* for vlist macros */

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

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "t:a:n:o:r:bvx:P:X:")) != -1) {
	switch (c) {
	    case 'b':		/* make BOT's instead of NMG's */
		do_bots = 1;
		break;
	    case 't':		/* calculational tolerance */
		tol.dist = atof( bu_optarg );
		tol.dist_sq = tol.dist * tol.dist;
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg)*bn_pi/180.0;
		ttol.rel = 0.0;
		break;
	    case 'o':		/* Output file name */
		out_file = bu_optarg;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		rt_g.debug = 1;
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
		bu_log("\n");
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
		NMG_debug = rt_g.NMG_debug;
		bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
		bu_log("\n");
		break;
	    default:
		bu_exit(1, usage, argv[0]);
	}
    }

    if (bu_optind+1 >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    /* Open BRL-CAD database */
    if ((dbip = db_open( argv[bu_optind], "r")) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open %s\n", argv[bu_optind]);
    }
    if ( db_dirbuild( dbip ) ) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    if ((fp_out = wdb_fopen( out_file )) == NULL) {
	perror( out_file );
	bu_exit(2, "ERROR: Cannot open %s for reading\n", out_file );
    }

    bu_optind++;

    mk_id_editunits( fp_out, dbip->dbi_title, dbip->dbi_local2base );

    /* Walk the trees outputting regions and combinations */
    for ( i=bu_optind; i<argc; i++ ) {
	struct directory *dp;

	dp = db_lookup( dbip, argv[i], 0 );
	if ( dp == RT_DIR_NULL ) {
	    bu_log( "WARNING: Could not find %s, skipping\n", argv[i] );
	    continue;
	}
	db_functree( dbip, dp, csg_comb_func, 0, &rt_uniresource, NULL );
    }

    /* write the current color table */
    mk_write_color_table(fp_out);

    rt_vlist_cleanup();
    db_close(dbip);

    percent = 100;
    if ( regions_tried > 0 )
	percent = ((double)regions_converted * 100) / regions_tried;

    printf( "Tried %d regions, %d converted successfully.  %g%%\n",
	    regions_tried, regions_converted, percent );

    wdb_close(fp_out);
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
