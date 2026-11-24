/*                         G - R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file g-raw.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an RAW file by
 * calling on the NMG booleans.  Based on g-stl.c.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bnetwork.h"
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "gcv.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b) {\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


struct g_raw_args {
    struct g_raw_args *self;
    int help;
    int verbose;
    int inches;
    int marching_cubes;
    const char *output_file;
    const char *output_directory;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    unsigned int rt_debug;
    unsigned int nmg_debug;
};

enum g_raw_tolerance_kind {
    G_RAW_ABSOLUTE_TOLERANCE,
    G_RAW_NORMAL_TOLERANCE,
    G_RAW_DISTANCE_TOLERANCE
};

static struct g_raw_args *
g_raw_args_from_storage(void *storage)
{
    return storage ? *(struct g_raw_args **)storage : NULL;
}

static int
g_raw_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_raw_tolerance_kind kind)
{
    struct g_raw_args *args = g_raw_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    switch (kind) {
	case G_RAW_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_RAW_NORMAL_TOLERANCE:
	    args->normal_tolerance = value * DEG2RAD;
	    args->relative_tolerance = 0.0;
	    break;
	case G_RAW_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    args->distance_tolerance_set = 1;
	    break;
    }
    return 0;
}

#define G_RAW_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_raw_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_RAW_TOLERANCE_CONSUMER(g_raw_absolute_tolerance_consume,
    G_RAW_ABSOLUTE_TOLERANCE)
G_RAW_TOLERANCE_CONSUMER(g_raw_normal_tolerance_consume,
    G_RAW_NORMAL_TOLERANCE)
G_RAW_TOLERANCE_CONSUMER(g_raw_distance_tolerance_consume,
    G_RAW_DISTANCE_TOLERANCE)

static const struct bu_arg_option g_raw_options[] = {
    BU_ARG_FLAG("h", "help", struct g_raw_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct g_raw_args, verbose,
	BU_ARG_VALUE_INTEGER, "Increase conversion reporting"),
    BU_ARG_FLAG("i", "inches", struct g_raw_args, inches,
	"Write coordinates in inches instead of millimeters"),
    BU_ARG_FLAG("8", "marching-cubes", struct g_raw_args, marching_cubes,
	"Tessellate with marching cubes instead of NMG Boolean evaluation"),
    BU_ARG_VALUE("o", "output", struct g_raw_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.raw",
	"Write all converted geometry to this file"),
    BU_ARG_VALUE("m", "output-directory", struct g_raw_args,
	output_directory, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "directory",
	"Write each converted region to a separate file in this directory"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_raw_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance", BU_ARG_REQUIRED, NULL,
	g_raw_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_raw_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_raw_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance", BU_ARG_REQUIRED, NULL,
	g_raw_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_raw_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)", BU_ARG_REQUIRED, NULL,
	g_raw_distance_tolerance_consume),
    BU_ARG_VALUE("x", "rt-debug", struct g_raw_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_raw_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_raw_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const g_raw_output_options[] = {
    "output", "output-directory", NULL
};

static const struct bu_arg_constraint g_raw_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT,
	BU_ARG_CONDITION_ALWAYS, g_raw_output_options, 0, 1,
	"--output and --output-directory are mutually exclusive"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema g_raw_schema = BU_ARG_SCHEMA(
    "g-raw", "Convert BRL-CAD geometry to RAW triangle facets",
    g_raw_options, g_raw_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, g_raw_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar g_raw_grammar = {
    &g_raw_schema, NULL, NULL
};

static int
g_raw_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_raw_schema, "g-raw");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-raw help");
    return status;
}

static int verbose;
static const char *output_file = NULL;	/* output filename */
static const char *output_directory = NULL;	/* directory name to hold output files */
static FILE *fp;			/* Output file pointer */
static struct db_i *dbip;
static struct model *the_model;
static struct bu_vls file_name = BU_VLS_INIT_ZERO;		/* file name built from region name */
static struct bg_tess_tol ttol;		/* tessellation tolerance in mm */
static struct bn_tol tol;		/* calculation tolerance */
static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;


/* Byte swaps a four byte value */
void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


static void
nmg_to_raw(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *UNUSED(tsp), void *UNUSED(client_data))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    struct bu_list *vlfree = (struct bu_list *)&rt_vlfree;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (output_directory) {
	char *c;

	bu_vls_trunc(&file_name, 0);
	bu_vls_strcpy(&file_name, output_directory);
	bu_vls_putc(&file_name, '/');
	c = region_name;
	c++;
	while (*c != '\0') {
	    if (*c == '/') {
		bu_vls_putc(&file_name, '@');
	    } else if (*c == '.' || isspace((int)*c)) {
		bu_vls_putc(&file_name, '_');
	    } else {
		bu_vls_putc(&file_name, *c);
	    }
	    c++;
	}
	bu_vls_strcat(&file_name, ".raw");
	/* Open ASCII output file */
	if ((fp=fopen(bu_vls_addr(&file_name), "wb+")) == NULL)
	{
	    perror("g-raw");
	    bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
	}
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Write pertinent info for this region */
    fprintf(fp, "%s\n", (region_name+1));

    /* triangulate model */
    nmg_triangulate_model(m, vlfree, &tol);

    /* Check triangles */
    for (BU_LIST_FOR (s, shell, &r->s_hd))
    {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd))
	{
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd))
	    {
		struct edgeuse *eu;
		int vert_count=0;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (inches)
			fprintf(fp, vert_count != 3 ? "%f %f %f " : "%f %f %f\n", V3ARGSIN(v->vg_p->coord));
		    else
			fprintf(fp, vert_count != 3 ? "%f %f %f " : "%f %f %f\n", V3ARGS(v->vg_p->coord));
		}
		if (vert_count > 3)
		{
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle");
		}
	    }
	}
    }

    if (output_directory) {
	fclose(fp);
    }
    bu_free(region_name, "region name");
}


static struct gcv_region_end_data gcvwriter = {nmg_to_raw, NULL, NULL};


int
main(int argc, char *argv[])
{
    struct g_raw_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    double percent;
    int i;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);

    bu_setlinebuf(stderr);

    gcvwriter.vlfree = &rt_vlfree;

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tessellation tolerance defaults */
    ttol.magic = BG_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    args.self = &args;
    args.absolute_tolerance = ttol.abs;
    args.relative_tolerance = ttol.rel;
    args.normal_tolerance = ttol.norm;
    args.distance_tolerance = tol.dist;

    introspection = bu_arg_introspect(&msg, &g_raw_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    operand_index = bu_arg_schema_parse_complete(&g_raw_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return g_raw_help(stderr, EXIT_FAILURE);
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_raw_help(stdout, EXIT_SUCCESS);

    verbose = args.verbose;
    inches = args.inches;
    output_file = args.output_file;
    output_directory = args.output_directory;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance;
    tol.dist = args.distance_tolerance;
    tol.dist_sq = tol.dist * tol.dist;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    if (args.distance_tolerance_set)
	rt_pr_tol(&tol);

    /* make empty NMG model */
    the_model = nmg_mm();

    if (!output_file && !output_directory) {
	fp = stdout;
    } else if (output_file) {
	/* Open ASCII output file */
	if ((fp=fopen(output_file, "wb+")) == NULL)
	{
	    perror(argv[0]);
	    bu_exit(1, "%s: Cannot open ASCII output file (%s) for writing\n",argv[0],output_file);
	}
    }

    /* Open brl-cad database */
    if ((dbip = db_open(argv[operand_index + 1], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[operand_index + 1]);
	bu_exit(1, "Unable to open geometry database file (%s)\n",
	    argv[operand_index + 1]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "ERROR: db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    if (verbose) {
	bu_log("Model: %s\n", argv[operand_index + 1]);
	bu_log("Objects:");
	for (i = operand_index + 2; i < argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
		tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
		tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }


    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc - operand_index - 2,
	    (const char **)(argv + operand_index + 2),
	    1,
	    &tree_state,
	    0,			/* take all regions */
	    args.marching_cubes ? gcv_region_end_mc : gcv_region_end,
	    args.marching_cubes ? NULL : rt_booltree_leaf_tess,
	    (void *)&gcvwriter);

    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	if (verbose)
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		    regions_tried, regions_converted, percent);
    }

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	if (verbose)
	    bu_log("                  %d triangulated successfully. %g%%\n",
		    regions_written, percent);
    }

    if (output_file) {
	fclose(fp);
    }

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

    return EXIT_SUCCESS;
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
