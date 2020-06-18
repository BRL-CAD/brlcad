/*                         G - O F F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
 */
/** @file g-off.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an OFF file by
 * calling on the NMG booleans.  Inspired by Michael J. Markowski's
 * g-jack.c converter.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bn/plot3.h"


extern union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);


static const char *usage = "[-v] [-d] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-p prefix] [-P #_of_CPUs] brlcad_db.g object(s)\n";

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	debug_plots;	/* Make debugging plots */
static int	ncpu = 1;	/* Number of processors */
static char	*prefix = NULL;	/* output filename prefix. */
static FILE	*fp_fig;	/* Jack Figure file. */
static struct db_i		*dbip;
static struct bu_vls		base_seg = BU_VLS_INIT_ZERO;
static struct bg_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	jack_tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_done = 0;

static void nmg_to_psurf(struct nmgregion *r, FILE *fp_psurf);
static void jack_faces(struct nmgregion *r, FILE *fp_psurf, int *map);

static void
print_usage(const char *progname)
{
    bu_exit(1, "Usage: %s %s", progname, usage);
}

int
main(int argc, char **argv)
{
    char		*dot, *fig_file;
    int	c;
    double		percent;
    int size;

    bu_setprogname(argv[0]);
    bu_setlinebuf( stderr );

    jack_tree_state = rt_initial_tree_state;	/* struct copy */
    jack_tree_state.ts_tol = &tol;
    jack_tree_state.ts_ttol = &ttol;
    jack_tree_state.ts_m = &the_model;

    ttol.magic = BG_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* FIXME These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    the_model = nmg_mm();
    BU_LIST_INIT( &RTG.rtg_vlfree );	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:dn:p:r:vx:P:X:h?")) != -1) {
	switch (c) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		break;
	    case 'd':
		debug_plots = 1;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		break;
	    case 'p':		/* Prefix for Jack file names. */
		prefix = bu_optarg;
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi( bu_optarg );
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_debug );
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&nmg_debug );
		NMG_debug = nmg_debug;
		break;
	    default:
		print_usage(argv[0]);
	}
    }

    if (bu_optind+1 >= argc)
	print_usage(argv[0]);

    /* Open BRL-CAD database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "ERROR: Unable to open geometry database file (%s)\n", argv[0]);
    }
    if ( db_dirbuild( dbip ) )
	bu_exit(1, "db_dirbuild failed\n" );

    /* Create .fig file name and open it. */
    size = sizeof(prefix) + sizeof(argv[0] + 4);
    fig_file = (char *)bu_malloc(size, "st");
    /* Ignore leading path name. */
    if ((dot = strrchr(argv[0], '/')) != (char *)NULL) {
	if (prefix)
	    snprintf(fig_file, size, "%s%s", prefix, 1+dot);
	else
	    snprintf(fig_file, size, "%s", 1+dot);
    } else {
	if (prefix)
	    snprintf(fig_file, size, "%s%s", prefix, argv[0]);
	else
	    snprintf(fig_file, size, "%s", argv[0]);
    }

    /* Get rid of any file name extension (probably .g). */
    if ((dot = strrchr(fig_file, '.')) != (char *)NULL)
	*dot = '\0';
    bu_strlcat(fig_file, ".fig", size);	/* Add required Jack suffix. */

    if ((fp_fig = fopen(fig_file, "wb")) == NULL)
	perror(fig_file);
    fprintf(fp_fig, "figure {\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void)db_walk_tree(dbip, argc-1, (const char **)(argv+1),
		       1,			/* ncpu */
		       &jack_tree_state,
		       0,			/* take all regions */
		       do_region_end,
		       nmg_booltree_leaf_tess,
		       (void *)NULL);	/* in librt/nmg_bool.c */

    fprintf(fp_fig, "\troot=%s_seg.base;\n", bu_vls_addr(&base_seg));
    fprintf(fp_fig, "}\n");
    fclose(fp_fig);
    bu_free(fig_file, "st");
    bu_vls_free(&base_seg);

    percent = 0;
    if (regions_tried>0)  percent = ((double)regions_done * 100) / regions_tried;
    printf("Tried %d regions, %d converted successfully.  %g%%\n",
	   regions_tried, regions_done, percent);

    return 0;
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp)
{
    static union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if ( !BU_SETJUMP ) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, &RTG.rtg_vlfree, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, &RTG.rtg_vlfree, tsp->ts_tol, &rt_uniresource);

    } else  {
	/* catch */
	char *name = db_path_to_string( pathp );

	/* Error, bail out */
	bu_log( "conversion of %s FAILED!\n", name );

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	nmg_debug = NMG_debug;/* restore mode */

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
 *  Called from db_walk_tree().
 *
 *  This routine must be prepared to run in parallel.
 */
union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *UNUSED(client_data))
{
    union tree		*ret_tree;
    struct nmgregion	*r;

    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || verbose) {
	char	*sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_done * 100) / regions_tried : 0,
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

    regions_done++;

    if (r != 0) {
	FILE	*fp_psurf;
	size_t	i;
	struct bu_vls	file_base = BU_VLS_INIT_ZERO;
	struct bu_vls	file = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&file_base, prefix);
	bu_vls_strcat(&file_base, DB_FULL_PATH_CUR_DIR(pathp)->d_namep);
	/* Dots confuse Jack's Peabody language.  Change to '_'. */
	for (i = 0; i < file_base.vls_len; i++)
	    if (file_base.vls_str[i] == '.')
		file_base.vls_str[i] = '_';

	/* Write color attribute to .fig figure file. */
	if (tsp->ts_mater.ma_color_valid != 0) {
	    fprintf(fp_fig, "\tattribute %s {\n",
		    bu_vls_addr(&file_base));
	    fprintf(fp_fig, "\t\trgb = (%f, %f, %f);\n",
		    V3ARGS(tsp->ts_mater.ma_color));
	    fprintf(fp_fig, "\t\tambient = 0.18;\n");
	    fprintf(fp_fig, "\t\tdiffuse = 0.72;\n");
	    fprintf(fp_fig, "\t}\n");
	}

	/* Write segment attributes to .fig figure file. */
	fprintf(fp_fig, "\tsegment %s_seg {\n", bu_vls_addr(&file_base));
	fprintf(fp_fig, "\t\tpsurf=\"%s.pss\";\n", bu_vls_addr(&file_base));
	if (tsp->ts_mater.ma_color_valid != 0)
	    fprintf(fp_fig,
		    "\t\tattribute=%s;\n", bu_vls_addr(&file_base));
	fprintf(fp_fig, "\t\tsite base->location=trans(0, 0, 0);\n");
	fprintf(fp_fig, "\t}\n");

	if ( bu_vls_strlen(&base_seg) <= 0 )  {
	    bu_vls_vlscat( &base_seg, &file_base );
	} else {
	    fprintf(fp_fig, "\tjoint %s_jt {\n",
		    bu_vls_addr(&file_base));
	    fprintf(fp_fig,
		    "\t\tconnect %s_seg.base to %s_seg.base;\n",
		    bu_vls_addr(&file_base),
		    bu_vls_addr(&base_seg) );
	    fprintf(fp_fig, "\t}\n");
	}

	bu_vls_vlscat(&file, &file_base);
	bu_vls_strcat(&file, ".pss");	/* Required Jack suffix. */

	/* Write psurf to .pss file. */
	if ((fp_psurf = fopen(bu_vls_addr(&file), "wb")) == NULL)
	    perror(bu_vls_addr(&file));
	else {
	    nmg_to_psurf(r, fp_psurf);
	    fclose(fp_psurf);
	    if (verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
	}
	bu_vls_free(&file);

	/* Also write as UNIX-plot file, if desired */
	if ( debug_plots )  {
	    FILE	*fp;
	    bu_vls_vlscat(&file, &file_base);
	    bu_vls_strcat(&file, ".plot3");

	    if ((fp = fopen(bu_vls_addr(&file), "wb")) == NULL)
		perror(bu_vls_addr(&file));
	    else {
		struct bu_list	vhead;
		pl_color( fp,
			  (int)(tsp->ts_mater.ma_color[0] * 255),
			  (int)(tsp->ts_mater.ma_color[1] * 255),
			  (int)(tsp->ts_mater.ma_color[2] * 255) );
		BU_LIST_INIT( &vhead );
		nmg_r_to_vlist(&vhead, r, 0, &RTG.rtg_vlfree);
		bn_vlist_to_uplot( fp, &vhead );
		fclose(fp);
		if (verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
	    }
	    bu_vls_free(&file);
	}

	/* NMG region is no longer necessary */
	nmg_kr(r);
    }

    /*
     *  Dispose of original tree, so that all associated dynamic
     *  memory is released now, not at the end of all regions.
     *  A return of TREE_NULL from this routine signals an error,
     *  so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree, &rt_uniresource);		/* Does an nmg_kr() */

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}

/*
 *	Convert an nmg region into Jack format.  This routine makes a
 *	list of unique vertices and writes them to the ascii Jack
 *	data base file.  Then a routine to generate the face vertex
 *	data is called.
 */

static void
nmg_to_psurf(struct nmgregion *r, FILE *fp_psurf)
/* NMG region to be converted. */
/* Jack format file to write vertex list to. */
{
    size_t		i;
    int			*map;	/* map from v->index to Jack vert # */
    struct bu_ptbl		vtab;	/* vertex table */

    map = (int *)bu_calloc(r->m_p->maxindex, sizeof(int), "Jack vert map");

    /* Built list of vertex structs */
    nmg_vertex_tabulate( &vtab, &r->l.magic, &RTG.rtg_vlfree );

    /* FIXME: What to do if 0 vertices?  */

    /* Print list of unique vertices and convert from mm to cm. */
    for (i = 0; i < BU_PTBL_LEN(&vtab); i++)  {
	struct vertex			*v;
	struct vertex_g	*vg;
	v = (struct vertex *)BU_PTBL_GET(&vtab, i);
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	NMG_INDEX_ASSIGN( map, v, i+1 );  /* map[v->index] = i+1 */
	fprintf(fp_psurf, "%f\t%f\t%f\n",
		vg->coord[X] / 10.,
		vg->coord[Y] / 10.,
		vg->coord[Z] / 10.);
    }
    fprintf(fp_psurf, ";;\n");

    jack_faces(r, fp_psurf, map);

    bu_ptbl_free(&vtab);
    bu_free( (char *)map, "Jack vert map" );
}


/*
 *	Continues the conversion of an nmg into Jack format.  Before
 *	this routine is called, a list of unique vertices has been
 *	stored in a heap.  Using this heap and the nmg structure, a
 *	list of face vertices is written to the Jack data base file.
 */
static void
jack_faces(struct nmgregion *r, FILE *fp_psurf, int *map)
/* NMG region to be converted. */
/* Jack format file to write face vertices to. */

{
    struct edgeuse		*eu;
    struct faceuse		*fu;
    struct loopuse		*lu;
    struct shell		*s;
    struct vertex		*v;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	/* Shell is made of faces. */
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		continue;
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
			fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map, eu->vu_p->v_p));
		    }
		} else if (BU_LIST_FIRST_MAGIC(&lu->down_hd)
			   == NMG_VERTEXUSE_MAGIC) {
		    v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
		    NMG_CK_VERTEX(v);
		    NMG_CK_VERTEX_G(v->vg_p);
		    fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map, v));
		} else
		    bu_log("jack_faces: loopuse mess up! (1)\n");
		fprintf(fp_psurf, ";\n");
	    }
	}

	/* Shell contains loops. */
	for (BU_LIST_FOR(lu, loopuse, &s->lu_hd)) {
	    NMG_CK_LOOPUSE(lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);
		    NMG_CK_EDGE(eu->e_p);
		    NMG_CK_VERTEXUSE(eu->vu_p);
		    NMG_CK_VERTEX(eu->vu_p->v_p);
		    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
		    fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map, eu->vu_p->v_p));
		}
	    } else if (BU_LIST_FIRST_MAGIC(&lu->down_hd)
		       == NMG_VERTEXUSE_MAGIC) {
		v = BU_LIST_PNEXT(vertexuse, &lu->down_hd)->v_p;
		NMG_CK_VERTEX(v);
		NMG_CK_VERTEX_G(v->vg_p);
		fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map, v));
	    } else
		bu_log("jack_faces: loopuse mess up! (1)\n");
	    fprintf(fp_psurf, ";\n");
	}

	/* Shell contains edges. */
	for (BU_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
	    NMG_CK_EDGEUSE(eu);
	    NMG_CK_EDGE(eu->e_p);
	    NMG_CK_VERTEXUSE(eu->vu_p);
	    NMG_CK_VERTEX(eu->vu_p->v_p);
	    NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);
	    fprintf(fp_psurf, "%d ", NMG_INDEX_GET(map, eu->vu_p->v_p));
	}
	if (BU_LIST_FIRST_MAGIC(&s->eu_hd) == NMG_EDGEUSE_MAGIC)
	    fprintf(fp_psurf, ";\n");

	/* Shell contains a single vertex. */
	if (s->vu_p) {
	    NMG_CK_VERTEXUSE(s->vu_p);
	    NMG_CK_VERTEX(s->vu_p->v_p);
	    NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
	    fprintf(fp_psurf, "%d;\n", NMG_INDEX_GET(map, s->vu_p->v_p));
	}

	if (BU_LIST_IS_EMPTY(&s->fu_hd) &&
	    BU_LIST_IS_EMPTY(&s->lu_hd) &&
	    BU_LIST_IS_EMPTY(&s->eu_hd) && !s->vu_p) {
	    bu_log("WARNING jack_faces: empty shell\n");
	}

    }
    fprintf(fp_psurf, ";;\n");
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
