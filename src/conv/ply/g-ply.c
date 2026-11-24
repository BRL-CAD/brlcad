/*                  G - P L Y . C
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
/** @file conv/ply/g-ply.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to the Stanford
 * PLY format by calling on the NMG booleans. Based on g-stl.c.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "gcv/api.h"
#include "vmath.h"


struct g_ply_args {
    struct g_ply_args *self;
    int help;
    int separate;
    int verbose;
    const char *type;
    const char *output;
    fastf_t absolute_tolerance;
    fastf_t relative_tolerance;
    fastf_t normal_tolerance;
    fastf_t distance_tolerance;
    unsigned int rt_debug;
    unsigned int nmg_debug;
};

enum g_ply_tolerance_kind {
    G_PLY_ABSOLUTE_TOLERANCE,
    G_PLY_NORMAL_TOLERANCE,
    G_PLY_DISTANCE_TOLERANCE
};

static struct g_ply_args *
g_ply_args_from_storage(void *storage)
{
    return storage ? *(struct g_ply_args **)storage : NULL;
}

static int
g_ply_tolerance_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage, enum g_ply_tolerance_kind kind)
{
    struct g_ply_args *args = g_ply_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || bu_arg_nonnegative_number_validate(msg, argv[0]) < 0 ||
	!bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (!args)
	return 0;

    switch (kind) {
	case G_PLY_ABSOLUTE_TOLERANCE:
	    args->absolute_tolerance = value;
	    args->relative_tolerance = 0.0;
	    break;
	case G_PLY_NORMAL_TOLERANCE:
	    args->normal_tolerance = value * DEG2RAD;
	    args->relative_tolerance = 0.0;
	    break;
	case G_PLY_DISTANCE_TOLERANCE:
	    args->distance_tolerance = value;
	    break;
    }
    return 0;
}

#define G_PLY_TOLERANCE_CONSUMER(_name, _kind) \
    static int _name(struct bu_vls *msg, size_t argc, const char **argv, \
	    void *storage) \
    { \
	return g_ply_tolerance_consume(msg, argc, argv, storage, _kind); \
    }

G_PLY_TOLERANCE_CONSUMER(g_ply_absolute_tolerance_consume,
    G_PLY_ABSOLUTE_TOLERANCE)
G_PLY_TOLERANCE_CONSUMER(g_ply_normal_tolerance_consume,
    G_PLY_NORMAL_TOLERANCE)
G_PLY_TOLERANCE_CONSUMER(g_ply_distance_tolerance_consume,
    G_PLY_DISTANCE_TOLERANCE)

static const struct bu_arg_value_keyword g_ply_types[] = {
    {"de", NULL, "Default binary encoding for this platform"},
    {"asc", NULL, "ASCII"},
    {"le", NULL, "Binary little-endian"},
    {"be", NULL, "Binary big-endian"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option g_ply_options[] = {
    BU_ARG_FLAG("h", "help", struct g_ply_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct g_ply_args, verbose,
	"Report objects, tolerances, and conversion statistics"),
    BU_ARG_FLAG("s", "separate", struct g_ply_args, separate,
	"Write a separate PLY file for each object"),
    BU_ARG_KEYWORD_VALUES("t", "type", struct g_ply_args, type, "encoding",
	"Select the PLY storage encoding", g_ply_types),
    BU_ARG_OPTION_SHAPED("a", "absolute-tolerance", "absolute-tolerance",
	struct g_ply_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative maximum chord deviation in millimeters and clear "
	"relative tolerance", BU_ARG_REQUIRED, NULL,
	g_ply_absolute_tolerance_consume),
    BU_ARG_VALUE_VALIDATE("r", "relative-tolerance", struct g_ply_args,
	relative_tolerance, BU_ARG_VALUE_NUMBER,
	bu_arg_nonnegative_number_validate, "fraction",
	"Set nonnegative tolerance as a fraction of the bounding box "
	"(default 0.01)"),
    BU_ARG_OPTION_SHAPED("n", "normal-tolerance", "normal-tolerance",
	struct g_ply_args, self, BU_ARG_VALUE_NUMBER, "degrees",
	"Set nonnegative surface-normal tolerance in degrees and clear "
	"relative tolerance", BU_ARG_REQUIRED, NULL,
	g_ply_normal_tolerance_consume),
    BU_ARG_OPTION_SHAPED("D", "distance-tolerance", "distance-tolerance",
	struct g_ply_args, self, BU_ARG_VALUE_NUMBER, "distance",
	"Set nonnegative vertex-coincidence distance in millimeters "
	"(default 0.0005)", BU_ARG_REQUIRED, NULL,
	g_ply_distance_tolerance_consume),
    BU_ARG_VALUE("x", "rt-debug", struct g_ply_args, rt_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "nmg-debug", struct g_ply_args, nmg_debug,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libnmg hexadecimal debug flags"),
    BU_ARG_VALUE("o", "output", struct g_ply_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "output.ply",
	"Write converted geometry to this PLY file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_ply_operands[] = {
    BU_ARG_OPERAND("database.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database object to convert", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const g_ply_required_options[] = {
    "output", NULL
};

static const struct bu_arg_constraint g_ply_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT,
	BU_ARG_CONDITION_ALWAYS, g_ply_required_options, 1, 1,
	"--output is required"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema g_ply_schema = BU_ARG_SCHEMA(
    "g-ply", "Convert BRL-CAD geometry to Stanford PLY",
    g_ply_options, g_ply_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, g_ply_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar g_ply_grammar = {
    &g_ply_schema, NULL, NULL
};

static int
g_ply_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&g_ply_schema, "g-ply");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "g-ply help");
    return status;
}

int
main(int argc, char **argv)
{
    struct g_ply_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const struct gcv_filter *in_filter;
    const struct gcv_filter *out_filter;
    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *input_path;
    const char *unique_options_av[3] = {"-t", NULL, NULL};
    int introspection;
    int operand_index;
    size_t unique_options_ac = 2;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);
    args.self = &args;
    args.type = "de";
    args.absolute_tolerance = gcv_options.tessellation_tolerance.abs;
    args.relative_tolerance = gcv_options.tessellation_tolerance.rel;
    args.normal_tolerance = gcv_options.tessellation_tolerance.norm;
    args.distance_tolerance = gcv_options.calculational_tolerance.dist;

    introspection = bu_arg_introspect(&msg, &g_ply_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    operand_index = bu_arg_schema_parse_complete(&g_ply_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return g_ply_help(stderr, EXIT_FAILURE);
    }
    bu_vls_free(&msg);
    if (args.help)
	return g_ply_help(stdout, EXIT_SUCCESS);

    input_path = argv[operand_index + 1];
    gcv_options.num_objects = (size_t)(argc - operand_index - 2);
    gcv_options.object_names = (const char * const *)(argv + operand_index + 2);
    gcv_options.verbosity_level = args.verbose;
    gcv_options.rt_debug_flag = args.rt_debug;
    gcv_options.nmg_debug_flag = args.nmg_debug;
    gcv_options.tessellation_tolerance.abs = args.absolute_tolerance;
    gcv_options.tessellation_tolerance.rel = args.relative_tolerance;
    gcv_options.tessellation_tolerance.norm = args.normal_tolerance;
    gcv_options.calculational_tolerance.dist = args.distance_tolerance;
    gcv_options.calculational_tolerance.dist_sq = args.distance_tolerance *
	args.distance_tolerance;
    unique_options_av[1] = args.type;
    if (args.separate) {
	unique_options_av[2] = "-s";
	unique_options_ac++;
    }

    /* setup to call gcv */
    gcv_context_init(&context);

    /* ensure filters exist */
    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, args.output, &context);

    if (!out_filter || !in_filter) {
	gcv_context_destroy(&context);
        bu_log("ERROR: could not find the conversion filters\n");
        return 1;
    }

    /* do the conversion */
    if (!gcv_execute(&context, in_filter, &gcv_options, 0, NULL, input_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to load input file");
    }

    gcv_options.debug_mode = 0;

    if (!gcv_execute(&context, out_filter, &gcv_options, unique_options_ac,
	    unique_options_av, args.output)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to export to output file");
    }

    gcv_context_destroy(&context);
    
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
