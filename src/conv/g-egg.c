/*                         G - E G G . C
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
/** @file conv/g-egg.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to a panda3d egg file by
 * calling on the NMG booleans.  Based on g-stl.c.
 *
 * Format information is (currently) available at
 * http://panda3d.cvs.sourceforge.net/panda3d/panda/src/doc/eggSyntax.txt?view=markup
 */

#include "common.h"

#include "bio.h"

/* interface headers */
#include "bu/arg.h"
#include "bu/app.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "gcv.h"

struct egg_conv_data {
    FILE *fp;
    unsigned long tot_polygons;
    struct bn_tol tol;
    struct bu_list *vlfree;
};


struct g_egg_args {
    struct g_egg_args *self;
    int help;
    int verbose;
    int marching_cubes;
    int cpu_count;
    int bottess_compat;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    int distance_tolerance_set;
    unsigned int rt_debug;
    unsigned int nmg_debug;
    const char *output_file;
};

enum g_egg_tolerance_kind {
    G_EGG_ABSOLUTE_TOLERANCE,
    G_EGG_NORMAL_TOLERANCE,
    G_EGG_DISTANCE_TOLERANCE
};

static struct g_egg_args *
g_egg_args_from_storage(void *storage)
{
    return storage ? *(struct g_egg_args **)storage : NULL;
}

static int
g_egg_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_egg_tolerance_kind kind)
{
    struct g_egg_args *args = g_egg_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if ((kind == G_EGG_NORMAL_TOLERANCE && value > 180.0) ||
	(kind != G_EGG_NORMAL_TOLERANCE && value > SQRT_MAX_FASTF)) {
	if (msg)
	    bu_vls_printf(msg, "tolerance is outside the supported range: %s\n",
		argv[0]);
	return -1;
    }
    if (!args)
	return 0;

    switch (kind) {
	case G_EGG_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_EGG_NORMAL_TOLERANCE:
	    args->normal_tolerance = value * DEG2RAD;
	    args->relative_tolerance = 0.0;
	    break;
	case G_EGG_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    args->distance_tolerance_set = 1;
	    break;
    }
    return 0;
}

#define G_EGG_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_egg_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_EGG_TOLERANCE_CONSUMER(g_egg_absolute_tolerance_consume,
    G_EGG_ABSOLUTE_TOLERANCE)
G_EGG_TOLERANCE_CONSUMER(g_egg_normal_tolerance_consume,
    G_EGG_NORMAL_TOLERANCE)
G_EGG_TOLERANCE_CONSUMER(g_egg_distance_tolerance_consume,
    G_EGG_DISTANCE_TOLERANCE)

static int
g_egg_serial_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    struct g_egg_args *args = g_egg_args_from_storage(storage);
    int count;

    if (argc != 1 ||
	!bu_arg_integer_range_from_str(msg, &count, argv[0], 10, 1, 1))
	return -1;
    if (args)
	args->cpu_count = count;
    return 0;
}

static int
g_egg_bottess_reject(struct bu_vls *msg, const char *UNUSED(arg),
	void *UNUSED(storage))
{
    if (msg)
	bu_vls_printf(msg, "Experimental bottess EGG output is not supported.\n");
    return -1;
}

static const struct bu_arg_option g_egg_options[] = {
    BU_ARG_FLAG("h", "help", struct g_egg_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct g_egg_args, verbose,
	"Report detailed conversion settings"),
    BU_ARG_FLAG("8", "marching-cubes", struct g_egg_args, marching_cubes,
	"Use marching cubes instead of NMG Boolean evaluation"),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_egg_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_egg_absolute_tolerance_consume),
    BU_ARG_NUMBER_RANGE("r", "relative-tolerance", struct g_egg_args,
	relative_tolerance, 1, 0.0, 1, 1.0, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_egg_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance",
	BU_ARG_REQUIRED, NULL, g_egg_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_egg_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)",
	BU_ARG_REQUIRED, NULL, g_egg_distance_tolerance_consume),
    BU_ARG_VALUE("x", "rt-debug", struct g_egg_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_egg_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("o", "output", struct g_egg_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.egg",
	"Write EGG data to this file instead of standard output"),
    BU_ARG_OPTION_SHAPED("P", NULL, "processors-compat",
	struct g_egg_args, self, BU_ARG_VALUE_INTEGER, "1",
	"Accepted for compatibility; EGG output is serial",
	BU_ARG_REQUIRED, NULL, g_egg_serial_consume),
    BU_ARG_CUSTOM_FLAG("9", NULL, "bottess-compat", struct g_egg_args,
	bottess_compat, g_egg_bottess_reject,
	"Rejected compatibility spelling for the retired bottess evaluator"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_egg_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_option_visibility g_egg_option_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("processors-compat", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY("bottess-compat", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY_NULL
};

static const struct bu_arg_presentation g_egg_presentation =
    BU_ARG_PRESENTATION(NULL, g_egg_option_visibility, NULL);

static const struct bu_arg_schema g_egg_schema = BU_ARG_SCHEMA_PRESENTED(
    "g-egg", "Convert BRL-CAD geometry to Panda3D EGG format",
    g_egg_options, g_egg_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL),
    &g_egg_presentation);

static const struct bu_arg_grammar g_egg_grammar = {
    &g_egg_schema, NULL, NULL
};

static int
g_egg_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_egg_schema, "g-egg");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-egg help");
    return status;
}


static void
nmg_to_egg(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state* UNUSED(tsp), void *client_data)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int vert_count=0;
    struct egg_conv_data *conv_data = (struct egg_conv_data *)client_data;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* triangulate model */
    nmg_triangulate_model(m, conv_data->vlfree, &conv_data->tol);

    /* Write pertinent info for this region */
    fprintf(conv_data->fp, "  <VertexPool> %s {\n", (region_name+1));

    /* Build the VertexPool */
    for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    vect_t facet_normal;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal and save it for all the vertex loops */
	    NMG_GET_FU_NORMAL(facet_normal, fu);

	    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    fprintf(conv_data->fp, "    <Vertex> %d {\n      %f %f %f\n      <Normal> { %f %f %f }\n    }\n",
			    vert_count,
			    V3ARGS(v->vg_p->coord),
			    V3ARGS(facet_normal));
		}
	    }
	}
    }
    fprintf(conv_data->fp, "  }\n");
    vert_count = 0;

    for (BU_LIST_FOR (s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		fprintf(conv_data->fp, "  <Polygon> { \n    <RGBA> { 1 1 1 1 } \n    <VertexRef> { ");
		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    fprintf(conv_data->fp, " %d", vert_count);
		}
		fprintf(conv_data->fp, " <Ref> { \"%s\" } }\n  }\n", region_name+1);

		region_polys++;
	    }
	}
    }

    conv_data->tot_polygons += region_polys;
    bu_free(region_name, "region name");
}


int
main(int argc, char *argv[])
{
    struct g_egg_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *output_file;
    struct db_i *dbip;
    struct model *the_model;
    struct bg_tess_tol ttol;		/* tessellation tolerance in mm */
    struct db_tree_state tree_state;	/* includes tol & model */

    int i;
    int introspection;
    int operand_index;
    struct egg_conv_data conv_data;
    struct gcv_region_end_data gcvwriter = {nmg_to_egg, NULL, NULL};

    gcvwriter.vlfree = &rt_vlfree;
    gcvwriter.client_data = (void *)&conv_data;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &conv_data.tol;
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
    conv_data.tol.magic = BN_TOL_MAGIC;
    conv_data.tol.dist = BN_TOL_DIST;
    conv_data.tol.dist_sq = conv_data.tol.dist * conv_data.tol.dist;
    conv_data.tol.perp = 1e-6;
    conv_data.tol.para = 1 - conv_data.tol.perp;

    conv_data.tot_polygons = 0;

    conv_data.vlfree = &rt_vlfree;

    args.self = &args;
    args.cpu_count = 1;
    args.absolute_tolerance = ttol.abs;
    args.relative_tolerance = ttol.rel;
    args.normal_tolerance = ttol.norm;
    args.distance_tolerance = conv_data.tol.dist;

    introspection = bu_arg_introspect(&msg, &g_egg_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_egg_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_egg_help(stdout, 0);

    output_file = args.output_file;
    ttol.abs = args.absolute_tolerance;
    ttol.rel = args.relative_tolerance;
    ttol.norm = args.normal_tolerance;
    conv_data.tol.dist = args.distance_tolerance;
    conv_data.tol.dist_sq = conv_data.tol.dist * conv_data.tol.dist;
    rt_debug = args.rt_debug;
    nmg_debug = args.nmg_debug;
    if (args.distance_tolerance_set)
	rt_pr_tol(&conv_data.tol);

    /* make empty NMG model */
    the_model = nmg_mm();

    conv_data.fp = stdout;
    if (output_file) {
	if ((conv_data.fp=fopen(output_file, "wb+")) == NULL) {
	    perror(argv[0]);
	    bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", output_file);
	}
    }

    /* Open brl-cad database */
    argc -= operand_index + 1;
    argv += operand_index + 1;

    gcvwriter.write_region = nmg_to_egg;

    if ((dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry database file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "ERROR: db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    BG_CK_TESS_TOL(tree_state.ts_ttol);

    if (args.verbose) {
	bu_log("Model: %s\n", argv[0]);
	bu_log("Objects:");
	for (i=1; i<argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* print the egg header stuff, including the command line to execute it */
    fprintf(conv_data.fp, "<CoordinateSystem> { Z-Up }\n\n");
    fprintf(conv_data.fp, "<Comment> {\n  \"%s", *argv);
    for (i=1; i<argc; i++)
	fprintf(conv_data.fp, " %s", argv[i]);
    fprintf(conv_data.fp, "\"\n}\n");

    /* Walk indicated tree(s).  Each region will be output separately */
    while (--argc) {
	fprintf(conv_data.fp, "<Group> %s {\n", *(argv+1));
	(void) db_walk_tree(dbip,		/* db_i */
			    1,		/* argc */
			    (const char **)(++argv), /* argv */
			    args.cpu_count,	/* ncpu */
			    &tree_state,	/* state */
			    NULL,		/* start func */
			    args.marching_cubes?gcv_region_end_mc:gcv_region_end,	/* end func */
			    args.marching_cubes?NULL:rt_booltree_leaf_tess, /* leaf func */
			    (void *)&gcvwriter);  /* client_data */
	fprintf(conv_data.fp, "}\n");
    }

    bu_log("%ld triangles written\n", conv_data.tot_polygons);

    if (output_file)
	fclose(conv_data.fp);

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
