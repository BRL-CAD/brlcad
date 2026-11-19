/*                         G - N F F . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2026 United States Government as represented by
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
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/file.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"


struct g_nff_args {
    struct g_nff_args *self;
    int help;
    int verbose;
    int inches_compat;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *error_file;
    const char *output_file;
};

enum g_nff_tolerance_kind {
    G_NFF_ABSOLUTE_TOLERANCE,
    G_NFF_NORMAL_TOLERANCE,
    G_NFF_DISTANCE_TOLERANCE
};

static struct g_nff_args *
g_nff_args_from_storage(void *storage)
{
    return storage ? *(struct g_nff_args **)storage : NULL;
}

static int
g_nff_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_nff_tolerance_kind kind)
{
    struct g_nff_args *args = g_nff_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    switch (kind) {
	case G_NFF_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_NFF_NORMAL_TOLERANCE:
	    args->normal_tolerance = value * DEG2RAD;
	    args->relative_tolerance = 0.0;
	    break;
	case G_NFF_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    args->distance_tolerance_set = 1;
	    break;
    }
    return 0;
}

#define G_NFF_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_nff_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_NFF_TOLERANCE_CONSUMER(g_nff_absolute_tolerance_consume,
    G_NFF_ABSOLUTE_TOLERANCE)
G_NFF_TOLERANCE_CONSUMER(g_nff_normal_tolerance_consume,
    G_NFF_NORMAL_TOLERANCE)
G_NFF_TOLERANCE_CONSUMER(g_nff_distance_tolerance_consume,
    G_NFF_DISTANCE_TOLERANCE)

static const struct bu_arg_option g_nff_options[] = {
    BU_ARG_FLAG("h", "help", struct g_nff_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct g_nff_args, verbose,
	"Report detailed conversion progress"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_nff_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_nff_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_nff_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_nff_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_nff_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_nff_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)",
	BU_ARG_REQUIRED, NULL, g_nff_distance_tolerance_consume),
    BU_ARG_VALUE("e", "error-file", struct g_nff_args, error_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "warnings.log",
	"Write Boolean-elimination warnings to this file instead of standard "
	"error"),
    BU_ARG_VALUE("x", "rt-debug", struct g_nff_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_nff_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("o", "output", struct g_nff_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.nff",
	"Write the converted geometry and generated view to this NFF file"),
    BU_ARG_FLAG("i", "inches", struct g_nff_args, inches_compat,
	"Accepted for compatibility; output remains in model coordinates"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_nff_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const g_nff_required_options[] = {
    "output", NULL
};

static const struct bu_arg_constraint g_nff_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT,
	BU_ARG_CONDITION_ALWAYS, g_nff_required_options, 1, 1,
	"--output is required"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_option_visibility g_nff_option_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("inches", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY_NULL
};

static const struct bu_arg_presentation g_nff_presentation =
    BU_ARG_PRESENTATION(NULL, g_nff_option_visibility, NULL);

static const struct bu_arg_schema g_nff_schema = BU_ARG_SCHEMA_PRESENTED(
    "g-nff", "Convert BRL-CAD geometry to Neutral File Format",
    g_nff_options, g_nff_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, g_nff_constraints, NULL, NULL, NULL),
    &g_nff_presentation);

static const struct bu_arg_grammar g_nff_grammar = {
    &g_nff_schema, NULL, NULL
};

static int
g_nff_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_nff_schema, "g-nff");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-nff help");
    return status;
}

static int NMG_debug;			/* saved arg of -X, for longjmp handling */
static int verbose;
static FILE *fp;			/* temp Output file pointer */
static FILE *fpe;			/* Error file pointer */
static struct db_i *dbip;
static struct bg_tess_tol ttol;
static struct bn_tol tol;
static struct model *the_model;

static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static long tot_polygons = 0;

static point_t model_max;
static point_t model_min;

#define COPY_BUF_SIZE 512


static void
nmg_to_nff(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), struct bu_list *vlfree)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &tol);

    /* output triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		fprintf(fp, "p 3\n");
		/* list vertices for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    VMINMAX(model_min, model_max, v->vg_p->coord);
		    fprintf(fp, "%g %g %g\n", V3ARGS(v->vg_p->coord));

		}
		tot_polygons++;
	    }
	}
    }

}


static void
process_triangulation(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp, struct bu_list *vlfree)
{
    if (!BU_SETJUMP) {
	/* try */

	/* Write the region to the NFF file */
	nmg_to_nff(r, pathp, tsp->ts_regionid, tsp->ts_gmater, vlfree);

    } else {
	/* catch */

	char *sofar;

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in triangulator: %s\n", sofar);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	nmg_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();
    }  BU_UNSETJUMP;
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

    } else {
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
 * Called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    union tree *ret_tree;
    struct bu_list vhead;
    struct nmgregion *r;
    char *region_name;
    struct bu_list *vlfree = (struct bu_list *)client_data;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || verbose) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%d %d%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    regions_tried++;

    printf("Attempting to process region %s\n", db_path_to_string(pathp));
    fflush(stdout);

    ret_tree = process_boolean(curtree, tsp, pathp, vlfree);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else {
	if (verbose) {
	    printf("\tNothing left of this region after Boolean evaluation\n");
	    fprintf(fpe, "WARNING: Nothing left after Boolean evaluation: %s\n",
		    db_path_to_string(pathp));
	    fflush(fpe);
	}
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

    BU_UNSETJUMP;		/* Relinquish the protection */
    regions_converted++;

    if (r != (struct nmgregion *)NULL) {
	struct shell *s;
	int empty_region=0;
	int empty_model=0;

	/* Kill cracks */
	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(&s->l, &r->s_hd)) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);
	    if (nmg_kill_cracks(s)) {
		if (nmg_ks(s)) {
		    empty_region = 1;
		    break;
		}
	    }
	    s = next_s;
	}

	/* kill zero length edgeuses */
	if (!empty_region) {
	    empty_model = nmg_kill_zero_length_edgeuses(*tsp->ts_m);
	}

	if (!empty_region && !empty_model) {
	    region_name = db_path_to_string(pathp);
	    fprintf(fp, "# %s\n", region_name);
	    bu_free(region_name, "region name");

	    /* write the material properties to the NFF file */
	    fprintf(fp, "f %g %g %g 0.5 0.5 4.81884 0 0\n",
		    V3ARGS(tsp->ts_mater.ma_color));

	    process_triangulation(r, pathp, tsp, vlfree);

	    regions_written++;
	}

	if (!empty_model)
	    nmg_kr(r);
    }

    /*
     * Dispose of original tree, so that all associated dynamic
     * memory is released now, not at the end of all regions.
     * A return of TREE_NULL from this routine signals an error,
     * and there is no point to adding _another_ message to our output,
     * so we need to cons up an OP_NOP node to return.
     */


    if (regions_tried > 0) {
	float npercent, tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	printf("Tried %d regions, %d conv. to NMG's %d conv. to tri. nmgper = %.2f%% triper = %.2f%%\n",
	       regions_tried, regions_converted, regions_written, npercent, tpercent);
    }

    db_free_tree(curtree);		/* Does an nmg_kr() */

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;
    return curtree;
}


int
main(int argc, char *argv[])
{
    struct g_nff_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_list *vlfree = &rt_vlfree;
    FILE *fpf = NULL;	/* final output file */
    const char *error_file;
    const char *output_file;
    point_t model_center;
    vect_t view_dir;
    vect_t model_diag;
    fastf_t bounding_radius;
    fastf_t dist_to_eye;
    point_t from;
    point_t light_loc;
    char buf[COPY_BUF_SIZE];
    size_t read_size;
    double percent;
    int i;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    VSETALL(model_min, INFINITY);
    VSETALL(model_max, -INFINITY);

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    ttol.magic = BG_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    args.self = &args;
    args.absolute_tolerance = ttol.abs;
    args.relative_tolerance = ttol.rel;
    args.normal_tolerance = ttol.norm;
    args.distance_tolerance = tol.dist;

    introspection = bu_arg_introspect(&msg, &g_nff_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_nff_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_nff_help(stdout, 0);

    verbose = args.verbose;
    output_file = args.output_file;
    error_file = args.error_file;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance;
    tol.dist = args.distance_tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    NMG_debug = nmg_debug;
    if (args.distance_tolerance_set)
	rt_pr_tol(&tol);

    the_model = nmg_mm();

    /* Open output file */
    if ((fpf = fopen(output_file, "wb+")) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open output file (%s) for writing\n", output_file);
    }

    /* Open temporary output file */
    fp = bu_temp_file(NULL, 0);

    /* Open error log file */
    if (!error_file) {
	fpe = stderr;
	setmode(fileno(fpe), O_BINARY);
    } else if ((fpe=fopen(error_file, "wb")) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open output file (%s) for writing\n", error_file);
    }

    /* Open BRL-CAD database */
    argc -= operand_index + 1;
    argv += operand_index + 1;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "ERROR: unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip))
	bu_exit(1, "db_dirbuild failed\n");

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    bu_log("Model: %s\n", argv[0]);
    bu_log("Objects:");
    for (i = 1; i < argc; i++)
	bu_log(" %s", argv[i]);
    bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	   tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
    bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	   tree_state.ts_tol->dist, tree_state.ts_tol->perp);

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			rt_booltree_leaf_tess,
			(void *)vlfree);	/* in librt/nmg_bool.c */

    if (regions_tried > 0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	printf("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
    }

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	printf("                  %d triangulated successfully. %g%%\n",
	       regions_written, percent);
    }

    bu_log("%ld triangles written\n", tot_polygons);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);
    if (fpe != stderr)
	fclose(fpe);

    if (tot_polygons == 0) {
	fclose(fp);
	fclose(fpf);
	bu_file_delete(output_file);
	bu_exit(1, "No triangles were generated; no NFF output written.\n");
    }

    /* write view information in the NFF file */
    fprintf(fpf, "v\n");
    VADD2(model_center, model_max, model_min);
    VSCALE(model_center, model_center, 0.5);
    VSET(view_dir, cos(M_PI_4) * cos(M_PI_4), cos(M_PI_4) * sin(M_PI_4), sin(M_PI_4));
    VSUB2(model_diag, model_max, model_min);
    bounding_radius = 0.5 * MAGNITUDE(model_diag);
    dist_to_eye = 2.0 * bounding_radius / tan(M_PI_4);
    VJOIN1(from, model_center, dist_to_eye, view_dir);

    /* from */
    fprintf(fpf, "from %g %g %g\n", V3ARGS(from));

    /* at */
    fprintf(fpf, "at %g %g %g\n", V3ARGS(model_center));

    /* up
     * this will only work for 45, 45 view
     */
    fprintf(fpf, "up %g %g %g\n", -view_dir[X], -view_dir[Y], view_dir[Z]);

    /* angle */
    fprintf(fpf, "angle 45\n");

    /* hither */
    fprintf(fpf, "hither 0.0\n");

    /* resolution */
    fprintf(fpf, "resolution 512 512\n");

    /* a light */
    VJOIN1(light_loc, model_center, dist_to_eye + 10.0, view_dir);
    fprintf(fpf, "l %g %g %g 1 1 1\n", V3ARGS(light_loc));


    /* copy the temporary file to the final file */
    rewind(fp);
    while ((read_size=fread(buf, 1, COPY_BUF_SIZE, fp))) {
	size_t ret;
	ret = fwrite(buf, read_size, 1, fpf);
	if (ret < 1)
	    perror("fwrite");
    }

    fclose(fpf);
    fclose(fp);

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
