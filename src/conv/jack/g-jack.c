/*                        G - J A C K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file g-jack.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a JACK Psurf
 * file, by calling on the NMG booleans.
 *
 * Jack, originally from the University of Pennsylvania's Computer
 * Graphics Research Laboratory, is a package used for human figure
 * animation and ergonomic analysis.
 *
 * This converter was written prior to JACK becoming a commercial
 * product in 1996, but it should be compatible with the more modern
 * Tecnomatix Jack version distributed by Siemens (maintainers of the
 * NX and Parasolid CAD software).  It reportedly still supports JACK
 * 5.1 JT files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/path.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bv/plot3.h"


struct g_jack_args {
    int help;
    int verbose;
    int debug_plots;
    int no_psurf_output;
    fastf_t edgeuse_distance;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    const char *prefix;
    int cpu_count;
    unsigned int rt_debug;
    unsigned int nmg_debug;
};

static const struct bu_arg_option g_jack_options[] = {
    BU_ARG_FLAG("h", "help", struct g_jack_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct g_jack_args, verbose,
	"Report conversion progress and generated files"),
    BU_ARG_FLAG("d", "debug-plots", struct g_jack_args, debug_plots,
	"Write each converted region as a color-matched .plot3 debugging "
	"file"),
    BU_ARG_FLAG("f", "no-psurf-output", struct g_jack_args,
	no_psurf_output,
	"Validate conversion without writing per-region .pss files; still "
	"write the .fig file"),
    BU_ARG_NUMBER_RANGE("u", "edgeuse-distance", struct g_jack_args,
	edgeuse_distance, 1, 0.0, 0, 0.0, "distance",
	"Set nonnegative edgeuse visualization distance (default 2.0)"),
    BU_ARG_NUMBER_RANGE("a", "absolute-tolerance", struct g_jack_args,
	absolute_tolerance, 1, 0.0, 0, 0.0, "distance",
	"Set nonnegative absolute tessellation tolerance in millimeters "
	"(default 0.0, disabled)"),
    BU_ARG_NUMBER_RANGE("r", "relative-tolerance", struct g_jack_args,
	relative_tolerance, 1, 0.0, 0, 0.0, "fraction",
	"Set nonnegative tessellation tolerance as a fraction of the bounding "
	"box (default 0.01)"),
    BU_ARG_NUMBER_RANGE("n", "normal-tolerance", struct g_jack_args,
	normal_tolerance, 1, 0.0, 0, 0.0, "degrees",
	"Set nonnegative surface-normal tessellation tolerance in degrees "
	"(default 0.0, disabled)"),
    BU_ARG_NUMBER_RANGE("D", "distance-tolerance", struct g_jack_args,
	distance_tolerance, 1, 0.0, 0, 0.0, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)"),
    BU_ARG_VALUE("p", "prefix", struct g_jack_args, prefix,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "text",
	"Prepend text to every generated output filename"),
    BU_ARG_INTEGER_RANGE("P", "cpu-count", struct g_jack_args, cpu_count,
	1, 1, 0, 0, "count",
	"Accept a positive CPU count for compatibility; conversion remains "
	"serial"),
    BU_ARG_VALUE("x", "rt-debug", struct g_jack_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_jack_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_jack_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_jack_schema = BU_ARG_SCHEMA(
    "g-jack", "Convert BRL-CAD geometry to Jack Psurf and figure files",
    g_jack_options, g_jack_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar g_jack_grammar = {
    &g_jack_schema, NULL, NULL
};

static int
g_jack_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_jack_schema, "g-jack");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-jack help");
    return status;
}

static int	NMG_debug;	/* saved arg of -X, for longjmp handling */
static int	verbose;
static int	no_file_output=0;	/* -f:  Don't bother writing output files */
static int	debug_plots;	/* Make debugging plots */
static const char *prefix = NULL;	/* output filename prefix. */
static FILE	*fp_fig;	/* Jack Figure file. */
static struct db_i		*dbip;
static struct bu_vls		base_seg = BU_VLS_INIT_ZERO;
static struct bg_tess_tol	ttol;
static struct bn_tol		tol;
static struct model		*the_model;

static struct db_tree_state	jack_tree_state;	/* includes tol & model */

static int	regions_tried = 0;
static int	regions_done = 0;


/*
 *	Continues the conversion of an nmg into Jack format.  Before
 *	this routine is called, a list of unique vertices has been
 *	stored in a heap.  Using this heap and the nmg structure, a
 *	list of face vertices is written to the Jack data base file.
 */
void
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
 *	Convert an nmg region into Jack format.  This routine makes a
 *	list of unique vertices and writes them to the ascii Jack
 *	data base file.  Then a routine to generate the face vertex
 *	data is called.
 */
void
nmg_to_psurf(struct nmgregion *r, FILE *fp_psurf, struct bu_list *vlfree)
    /* NMG region to be converted. */
    /* Jack format file to write vertex list to. */
{
    size_t		i;
    int			*map;	/* map from v->index to Jack vert # */
    struct bu_ptbl		vtab;	/* vertex table */

    map = (int *)bu_calloc(r->m_p->maxindex, sizeof(int), "Jack vert map");

    /* Built list of vertex structs */
    nmg_vertex_tabulate(&vtab, &r->l.magic, vlfree);

    /* FIXME: What to do if 0 vertices?  */

    /* Print list of unique vertices and convert from mm to cm. */
    for (i = 0; i < BU_PTBL_LEN(&vtab); i++)  {
	struct vertex			*v;
	struct vertex_g	*vg;
	v = (struct vertex *)BU_PTBL_GET(&vtab, i);
	NMG_CK_VERTEX(v);
	vg = v->vg_p;
	NMG_CK_VERTEX_G(vg);
	NMG_INDEX_ASSIGN(map, v, i+1);  /* map[v->index] = i+1 */
	fprintf(fp_psurf, "%f\t%f\t%f\n",
		vg->coord[X] / 10.,
		vg->coord[Y] / 10.,
		vg->coord[Z] / 10.);
    }
    fprintf(fp_psurf, ";;\n");

    jack_faces(r, fp_psurf, map);

    bu_ptbl_free(&vtab);
    bu_free((char *)map, "Jack vert map");
}


static union tree *
process_boolean(union tree *curtree, struct db_tree_state *tsp, const struct db_full_path *pathp, struct bu_list *vlfree)
{
    static union tree *ret_tree = TREE_NULL;

    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	(void)nmg_model_fuse(*tsp->ts_m, vlfree, tsp->ts_tol);
	ret_tree = nmg_booltree_evaluate(curtree, vlfree, tsp->ts_tol);

    } else  {
	/* catch */
	char *name = db_path_to_string(pathp);

	/* Error, bail out */
	bu_log("conversion of %s FAILED!\n", name);

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before before bombing out.
	 */
	nmg_debug = NMG_debug;/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */
	db_free_tree(curtree);/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	bu_free(name, "db_path_to_string");
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
union tree *do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    struct bu_list *vlfree = (struct bu_list *)client_data;
    union tree		*ret_tree;
    struct nmgregion	*r;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
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

    if (pathp->fp_len <= 0)
	return curtree;

    regions_tried++;

    ret_tree = process_boolean(curtree, tsp, pathp, vlfree);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else
	r = (struct nmgregion *)NULL;

    regions_done++;

    if (r && !no_file_output) {
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

	if (bu_vls_strlen(&base_seg) <= 0)  {
	    bu_vls_vlscat(&base_seg, &file_base);
	} else {
	    fprintf(fp_fig, "\tjoint %s_jt {\n",
		    bu_vls_addr(&file_base));
	    fprintf(fp_fig,
		    "\t\tconnect %s_seg.base to %s_seg.base;\n",
		    bu_vls_addr(&file_base),
		    bu_vls_addr(&base_seg));
	    fprintf(fp_fig, "\t}\n");
	}

	bu_vls_vlscat(&file, &file_base);
	bu_vls_strcat(&file, ".pss");	/* Required Jack suffix. */

	/* Write psurf to .pss file. */
	if ((fp_psurf = fopen(bu_vls_addr(&file), "wb")) == NULL)
	    perror(bu_vls_addr(&file));
	else {
	    nmg_to_psurf(r, fp_psurf, vlfree);
	    fclose(fp_psurf);
	    if (verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
	}
	bu_vls_free(&file);

	/* Also write as UNIX-plot file, if desired */
	if (debug_plots)  {
	    FILE	*fp;
	    bu_vls_vlscat(&file, &file_base);
	    bu_vls_strcat(&file, ".plot3");

	    if ((fp = fopen(bu_vls_addr(&file), "wb")) == NULL)
		perror(bu_vls_addr(&file));
	    else {
		struct bu_list	vhead;
		pl_color(fp,
			  (int)(tsp->ts_mater.ma_color[0] * 255),
			  (int)(tsp->ts_mater.ma_color[1] * 255),
			  (int)(tsp->ts_mater.ma_color[2] * 255));
		BU_LIST_INIT(&vhead);
		nmg_r_to_vlist(&vhead, r, 0, vlfree);
		bv_vlist_to_uplot(fp, &vhead);
		fclose(fp);
		BV_FREE_VLIST(vlfree, &vhead);
		if (verbose) bu_log("*** Wrote %s\n", bu_vls_addr(&file));
	    }
	    bu_vls_free(&file);
	}
	bu_vls_free(&file_base);
    }

    if (no_file_output && verbose)
	bu_log("*** Completed %s\n", DB_FULL_PATH_CUR_DIR(pathp)->d_namep);

    /*
     *  Dispose of original tree, so that all associated dynamic
     *  memory is released now, not at the end of all regions.
     *  A return of TREE_NULL from this routine signals an error,
     *  and there is no point to adding _another_ message to our output,
     *  so we need to cons up an OP_NOP node to return.
     */
    db_free_tree(curtree);		/* Does an nmg_kr() */

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}

int
main(int argc, char **argv)
{
    struct g_jack_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    char		*dot;
    char		*database_name;
    int		introspection;
    int		operand_index;
    int		distance_tolerance_set;
    double		percent;
    struct bu_vls	fig_file = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    args.edgeuse_distance = 2.0;
    args.relative_tolerance = 0.01;
    args.distance_tolerance = 0.0005;
    args.cpu_count = 1;

    introspection = bu_arg_introspect(&msg, &g_jack_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_jack_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_jack_help(stdout, 0);

    distance_tolerance_set = bu_arg_schema_option_present(&g_jack_schema,
	argc - 1, (const char **)(argv + 1), "distance-tolerance");
    verbose = args.verbose;
    debug_plots = args.debug_plots;
    no_file_output = args.no_psurf_output;
    prefix = args.prefix;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    NMG_debug = nmg_debug;

    RT_DBTS_INIT(&jack_tree_state);
    jack_tree_state.ts_tol = &tol;
    jack_tree_state.ts_ttol = &ttol;
    jack_tree_state.ts_m = &the_model;

    ttol.magic = BG_TESS_TOL_MAGIC;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance * DEG2RAD;

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = args.distance_tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_eue_dist = args.edgeuse_distance;
    if (distance_tolerance_set)
	rt_pr_tol(&tol);

    the_model = nmg_mm();
    struct bu_list *vlfree = &rt_vlfree;
    BU_LIST_INIT(vlfree);	/* for vlist macros */

    /* Open BRL-CAD database */
    argc -= operand_index + 1;
    argv += operand_index + 1;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file [%s]\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    /* Create .fig file name and open it. */

    /* Ignore leading path name. */
    database_name = bu_path_basename(argv[0], NULL);
    if (prefix)
	bu_vls_strcpy(&fig_file, prefix);
    bu_vls_strcat(&fig_file, database_name);
    bu_free(database_name, "database basename");

    /* Get rid of any file name extension (probably .g). */
    if ((dot = strrchr(bu_vls_addr(&fig_file), '.')) != (char *)NULL)  {
	*dot = '\0';
	/* Recalculate shorter VLS length.  Ugh. */
	bu_vls_trunc(&fig_file, dot - bu_vls_addr(&fig_file));
    }
    bu_vls_strcat(&fig_file, ".fig");	/* Add required Jack suffix. */

    if ((fp_fig = fopen(bu_vls_addr(&fig_file), "wb")) == NULL)  {
	perror(bu_vls_addr(&fig_file));
	bu_exit(2, "Unable to open fig file [%s]\n", bu_vls_addr(&fig_file));
    }
    fprintf(fp_fig, "figure {\n");

    BN_CK_TOL(jack_tree_state.ts_tol);
    BG_CK_TESS_TOL(jack_tree_state.ts_ttol);

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&jack_tree_state,
			0,			/* take all regions */
			do_region_end,
			rt_booltree_leaf_tess,
			(void *)vlfree);	/* in librt/nmg_bool.c */

    fprintf(fp_fig, "\troot=%s_seg.base;\n", bu_vls_addr(&base_seg));
    fprintf(fp_fig, "}\n");
    fclose(fp_fig);
    bu_log("Writing file %s\n", bu_vls_addr(&fig_file));
    if (!no_file_output)
	printf("(check for writing of other files)\n");
    bu_vls_free(&fig_file);
    bu_vls_free(&base_seg);

    percent = 0;
    if (regions_tried>0)  percent = ((double)regions_done * 100) / regions_tried;
    printf("Tried %d regions, %d converted successfully.  %g%%\n",
	   regions_tried, regions_done, percent);

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
