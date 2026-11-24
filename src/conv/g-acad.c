/*                        G - A C A D . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
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
/** @file conv/g-acad.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to the Lockheed
 * Martin "Advanced Computer-Aided Design" (ACAD) file format by
 * calling on the NMG booleans (formerly from General Dynamics).
 *
 * Note that this converter has nothing to do with the Autodesk
 * AutoCAD software.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/file.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define ACAD_COPY_BUFFER_SIZE 4096
#define ACAD_REGION_COUNT_WIDTH 14


struct g_acad_args {
    struct g_acad_args *self;
    int help;
    int verbose;
    int inches;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    int processors_compat;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *error_file;
    const char *output_file;
};

enum g_acad_tolerance_kind {
    G_ACAD_ABSOLUTE_TOLERANCE,
    G_ACAD_NORMAL_TOLERANCE,
    G_ACAD_DISTANCE_TOLERANCE
};

static struct g_acad_args *
g_acad_args_from_storage(void *storage)
{
    return storage ? *(struct g_acad_args **)storage : NULL;
}


static int
g_acad_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_acad_tolerance_kind kind)
{
    struct g_acad_args *args = g_acad_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    switch (kind) {
	case G_ACAD_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_ACAD_NORMAL_TOLERANCE:
	    args->normal_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_ACAD_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    args->distance_tolerance_set = 1;
	    break;
    }
    return 0;
}

#define G_ACAD_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_acad_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_ACAD_TOLERANCE_CONSUMER(g_acad_absolute_tolerance_consume,
    G_ACAD_ABSOLUTE_TOLERANCE)
G_ACAD_TOLERANCE_CONSUMER(g_acad_normal_tolerance_consume,
    G_ACAD_NORMAL_TOLERANCE)
G_ACAD_TOLERANCE_CONSUMER(g_acad_distance_tolerance_consume,
    G_ACAD_DISTANCE_TOLERANCE)

static const struct bu_arg_option g_acad_options[] = {
    BU_ARG_FLAG("h", "help", struct g_acad_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct g_acad_args, verbose,
	BU_ARG_VALUE_FLAG, "Increase conversion progress reporting"),
    BU_ARG_FLAG("i", "inches", struct g_acad_args, inches,
	"Write vertex coordinates in inches instead of millimeters"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_acad_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_acad_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_acad_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_acad_args, self, BU_ARG_VALUE_NUMBER, "radians",
	"Set nonnegative surface-normal tolerance in radians and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_acad_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_acad_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters",
	BU_ARG_REQUIRED, NULL, g_acad_distance_tolerance_consume),
    BU_ARG_INTEGER_RANGE("P", "processors", struct g_acad_args,
	processors_compat, 1, 1, 0, 0, "count",
	"Accept a positive worker count for compatibility; conversion remains "
	"serial"),
    BU_ARG_VALUE("e", "error-file", struct g_acad_args, error_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "diagnostics.log",
	"Write diagnostics to this file instead of standard error"),
    BU_ARG_VALUE("x", "rt-debug", struct g_acad_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_acad_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("o", "output", struct g_acad_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.acad",
	"Write ACAD facet data to this file instead of standard output"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_acad_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_acad_schema = BU_ARG_SCHEMA(
    "g-acad", "Convert BRL-CAD geometry to Lockheed Martin ACAD facets",
    g_acad_options, g_acad_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar g_acad_grammar = {
    &g_acad_schema, NULL, NULL
};

static int
g_acad_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_acad_schema, "g-acad");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-acad help");
    return status;
}


static int NMG_debug;	/* saved arg of -X, for longjmp handling */
static int verbose;
static const char *output_file = NULL;	/* output filename */
static const char *error_file = NULL;	/* error filename */
static FILE *fp;		/* Output file pointer */
static FILE *fpe;		/* Error file pointer */
static struct db_i *dbip;
static struct bg_tess_tol ttol;
static struct bn_tol tol;
static struct model *the_model;

static struct db_tree_state tree_state;	/* includes tol & model */

static size_t regions_tried = 0;
static size_t regions_converted = 0;
static size_t regions_written = 0;
static int inches = 0;
static size_t tot_polygons = 0;


static void
nmg_to_acad(struct nmgregion *r, const struct db_full_path *pathp, int region_id, struct bu_list *vlfree)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    struct bu_ptbl verts;
    char *region_name;
    size_t numverts = 0;		/* Number of vertices to output */
    size_t numtri   = 0;		/* Number of triangles to output */
    size_t tricount = 0;		/* Triangle number */
    size_t i;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &tol);


    /* list all vertices in result */
    nmg_vertex_tabulate(&verts, &r->l.magic, vlfree);

    /* Get number of vertices */

    numverts = BU_PTBL_LEN(&verts);

/* BEGIN CHECK SECTION */

/* Check vertices */

    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
    }

/* Check triangles */
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
		size_t vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    intmax_t idx;

		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    idx = bu_ptbl_locate(&verts, (long *)v);
		    if (idx < 0) {
			bu_ptbl_free(&verts);
			bu_free(region_name, "region name");
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_exit(1, "ERROR: Triangle vertex was not located\n");
		    }
		}
		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has too many (%zu) vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3)
		    continue;
		numtri++;
	    }
	}
    }

/* END CHECK SECTION */
/* Write pertinent info for this region */

    fprintf(fp, "%s\n", (region_name+1));
/* No mirror plane */
    fprintf(fp, "%d\n", 0);
/* Number of vertices */
    fprintf(fp, "%zu\n", numverts);


    /* Write numverts, then vertices */

    for (i=0; i<numverts; i++) {
	v = (struct vertex *)BU_PTBL_GET(&verts, i);
	NMG_CK_VERTEX(v);
	if (inches)
	    fprintf(fp, "%f %f %f\n", V3ARGSIN(v->vg_p->coord));
	else
	    fprintf(fp, "%f %f %f\n", V3ARGS(v->vg_p->coord));
    }

/* Number of sub-parts (always 1 with BRL-CAD) */
    fprintf(fp, "%d\n", 1);
/* Write out name again */
    fprintf(fp, "%s\n", (region_name+1));
/* Number of triangles, number of vert/tri (3) */
    fprintf(fp, "%zu       %d\n", numtri, 3);

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
		size_t vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* list vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    intmax_t idx;

		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    vert_count++;
		    idx = bu_ptbl_locate(&verts, (long *)v);
		    if (idx < 0) {
			bu_ptbl_free(&verts);
			bu_log("Vertex from eu %p is not in nmgregion %p\n", (void *)eu, (void *)r);
			bu_free(region_name, "region name");
			bu_exit(1, "ERROR: Can't find vertex in list!\n");
		    }

		    fprintf(fp, " %ld", (long)(idx+1));
		}

		/* Output other info. for triangle ICOAT, component#, facet# */
		/* Map Icoat from material table later */
		/* fprintf(fp, "%s icomp=%d material=%d:\n", (region_name+1), region_id);*/

		fprintf(fp, " %d    %d    %zu\n", 0, region_id, ++tricount);

		if (vert_count > 3) {
		    bu_ptbl_free(&verts);
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %zu vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle\n");
		} else if (vert_count < 3)
		    continue;
		tot_polygons++;
	    }
	}
    }
/* regions_converted++;
   printf("Processed region %s\n", region_name);
   printf("Regions attempted = %zu Regions done = %zu\n", regions_tried, regions_converted);
   fflush(stdout);
*/
    bu_ptbl_free(&verts);
    bu_free(region_name, "region name");
}


static union tree *
process_region(const struct db_full_path *pathp, union tree *curtree, struct db_tree_state *tsp, struct bu_list *vlfree)
{
    /* Begin bomb protection */
    if (!BU_SETJUMP) {
	/* try */

	union tree *ret_tree;
	char *region_path = db_path_to_string(pathp);

	fprintf(fpe, "Attempting to process region %s\n", region_path);
	bu_free(region_path, "region path");
	fflush(fpe);
	ret_tree = nmg_booltree_evaluate(curtree, vlfree, tsp->ts_tol);
	if (ret_tree != curtree) {
	    db_free_tree(curtree);
	}
	return ret_tree;
    } else {
	/* catch */

	/* Error, bail out */
	char *sofar;
	BU_UNSETJUMP;		/* Relinquish the protection */

	sofar = db_path_to_string(pathp);
	bu_log("FAILED in Boolean evaluation: %s\n", sofar);
	fprintf(fpe, "Failed Bool. Eval.: %s\n", sofar);
	fflush(fpe);
	bu_free((char *)sofar, "sofar");

	/* Sometimes the NMG library adds debugging bits when
	 * it detects an internal error, before bombing out.
	 */
	nmg_debug = NMG_debug;	/* restore mode */

	/* Release any intersector 2d tables */
	nmg_isect2d_final_cleanup();

	/* Release the tree memory & input regions */

	/* FIXME: memory leak? */
	/* db_free_tree(curtree);*/		/* Does an nmg_kr() */

	/* Get rid of (m)any other intermediate structures */
	if ((*tsp->ts_m)->magic == NMG_MODEL_MAGIC) {
	    nmg_km(*tsp->ts_m);
	} else {
	    bu_log("WARNING: tsp->ts_m pointer corrupted, ignoring it.\n");
	}

	/* Now, make a new, clean model structure for next pass. */
	*tsp->ts_m = nmg_mm();

	return TREE_NULL;
    }
}


/*
 * Called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
do_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    union tree *ret_tree = NULL;
    struct bu_list vhead;
    /* static due to longjmp */
    static struct nmgregion *r = NULL;
    struct bu_list *vlfree = (struct bu_list *)client_data;

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK || verbose) {
	char *sofar = db_path_to_string(pathp);
	bu_log("\ndo_region_end(%zu %zu%%) %s\n",
	       regions_tried,
	       regions_tried>0 ? (regions_converted * 100) / regions_tried : 0,
	       sofar);
	bu_free(sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP)
	return curtree;

    regions_tried++;

    /* do the deed */
    ret_tree = process_region(pathp, curtree, tsp, vlfree);

    if (ret_tree)
	r = ret_tree->tr_d.td_r;
    else {
	if (verbose) {
	    char *region_path = db_path_to_string(pathp);
	    fprintf(fpe, "WARNING: Nothing left after Boolean evaluation: %s\n",
		    region_path);
	    bu_free(region_path, "region path");
	    fflush(fpe);
	}
	regions_written++; /* don't count as a failure */
	r = (struct nmgregion *)NULL;
    }

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
	    if (!BU_SETJUMP) {
		/* try */

		/* Write the region to the TANKILL file */
		nmg_to_acad(r, pathp, tsp->ts_regionid, vlfree);
		regions_written++;

	    } else {
		/* catch */

		char *sofar;

		BU_UNSETJUMP;

		sofar = db_path_to_string(pathp);
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

		/* FIXME: leaking memory with curtree */

		return TREE_NULL;
	    } BU_UNSETJUMP;
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

    if (regions_tried>0) {
	float npercent, tpercent;

	npercent = (float)(regions_converted * 100) / regions_tried;
	tpercent = (float)(regions_written * 100) / regions_tried;
	fprintf(fpe, "Tried %zu regions, %zu conv. to NMG's %zu conv. to tri. nmgper = %.2f%% triper = %.2f%% \n",
	       regions_tried, regions_converted, regions_written, npercent, tpercent);
    }

    BU_ALLOC(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_NOP;

    return curtree;
}


int
main(int argc, char **argv)
{
    struct g_acad_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_list *vlfree = &rt_vlfree;
    FILE *final_output = NULL;
    b_off_t region_count_offset;
    char copy_buffer[ACAD_COPY_BUFFER_SIZE];
    double percent;
    int i;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    rt_debug = 0;

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

    introspection = bu_arg_introspect(&msg, &g_acad_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_acad_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_acad_help(stdout, 0);

    verbose = args.verbose;
    inches = args.inches;
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

    if (!output_file) {
	final_output = stdout;
	(void)setmode(fileno(final_output), O_BINARY);
	if ((fp = bu_temp_file(NULL, 0)) == NULL)
	    bu_exit(1, "Cannot create temporary ACAD output file\n");
    } else {
	/* Open output file */
	if ((fp=fopen(output_file, "wb+")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, "Cannot open output file (%s) for writing\n", output_file);
	}
    }

    /* Open g-acad error log file */
    if (!error_file) {
	fpe = stderr;
	(void)setmode(fileno(fpe), O_BINARY);
    } else if ((fpe=fopen(error_file, "wb")) == NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open output file (%s) for writing\n", error_file);
    }


    /* Open BRL-CAD database */
    argc -= operand_index + 1;
    argv += operand_index + 1;
    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Cannot open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "db_dirbuild failed\n");
    }

    /* sanity */
    if (!fp || !fpe) {
	bu_bomb("ERROR: output unset\n");
	return 1;
    }

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    fprintf(fpe, "Model: %s\n", argv[0]);
    fprintf(fpe, "Objects:");
    for (i=1; i<argc; i++)
	fprintf(fpe, " %s", argv[i]);
    fprintf(fpe, "\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	    tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
    fprintf(fpe, "Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	    tree_state.ts_tol->dist, tree_state.ts_tol->perp);

    bu_log("Model: %s\n", argv[0]);
    bu_log("Objects:");
    for (i=1; i<argc; i++)
	bu_log(" %s", argv[i]);
    bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	   tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
    bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	   tree_state.ts_tol->dist, tree_state.ts_tol->perp);

/* Write out ACAD facet header */
    if (inches)
	fprintf(fp, "BRL-CAD generated ACAD FACET FILE (Units in)\n");
    else
	fprintf(fp, "BRL-CAD generated ACAD FACET FILE (Units mm)\n");

/* Generate space for number of facet entities, will write over later */

    region_count_offset = bu_ftell(fp);
    if (region_count_offset < 0)
	bu_exit(1, "Cannot locate ACAD region-count field\n");
    fprintf(fp, "%*s\n", ACAD_REGION_COUNT_WIDTH, "");

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			do_region_end,
			rt_booltree_leaf_tess,
			(void *)vlfree);	/* in librt/nmg_bool.c */

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	fprintf(fpe, "Tried %zu regions, %zu converted to NMG's successfully.  %g%%\n",
	       regions_tried, regions_converted, percent);
    }

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	fprintf(fpe, "                  %zu triangulated successfully. %g%%\n",
	       regions_written, percent);
    }

    bu_log("%zu triangles written\n", tot_polygons);
    fprintf(fpe, "%zu triangles written\n", tot_polygons);

    /* Write out number of facet entities to .facet file */

    rewind(fp);
    if (bu_fseek(fp, region_count_offset, SEEK_SET) != 0)
	bu_exit(1, "Cannot update ACAD region count\n");
    fprintf(fp, "%*zu\n", ACAD_REGION_COUNT_WIDTH, regions_written);

    if (final_output) {
	size_t read_size;

	rewind(fp);
	while ((read_size = fread(copy_buffer, 1, sizeof(copy_buffer), fp)) > 0) {
	    if (fwrite(copy_buffer, 1, read_size, final_output) != read_size)
		bu_exit(1, "Error writing ACAD output\n");
	}
	if (ferror(fp))
	    bu_exit(1, "Error reading temporary ACAD output\n");
    }
    fclose(fp);

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);
    if (fpe != stderr)
	fclose(fpe);

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
