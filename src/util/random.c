/*                        R A N D O M . C
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
/** @file util/random.c
 *
 * Generate a random number between the two values given. The number
 * can be uniform across the entire range or it can be a gaussian
 * distribution around the center of the range (or a named center).
 *
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/process.h"
#include "bn.h"

struct random_args {
    int help;
    int verbose;
    int gaussian;
    int uniform;
    int seed;
    fastf_t center;
};

static const struct bu_arg_option random_options[] = {
    BU_ARG_FLAG("h", "help", struct random_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("u", "uniform", struct random_args, uniform,
	"Use a uniform distribution (default)"),
    BU_ARG_FLAG("g", "gaussian", struct random_args, gaussian,
	"Use a Gaussian distribution"),
    BU_ARG_VALUE("c", "center", struct random_args, center,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "value", "Set the Gaussian center"),
    BU_ARG_VALUE("s", "seed", struct random_args, seed,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "integer", "Set the random seed"),
    BU_ARG_FLAG("v", "verbose", struct random_args, verbose,
	"Print generation parameters"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand random_operands[] = {
    BU_ARG_OPERAND("low", BU_ARG_VALUE_INTEGER, 1, 1,
	"Inclusive lower bound", NULL),
    BU_ARG_OPERAND("high", BU_ARG_VALUE_INTEGER, 1, 1,
	"Inclusive upper bound", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const random_modes[] = {"uniform", "gaussian", NULL};
static const char * const random_center_requirement[] = {
    "center", "gaussian", NULL
};

static const struct bu_arg_constraint random_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	random_modes, 0, 1, "select uniform or Gaussian mode, not both"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES,
	BU_ARG_CONDITION_ALWAYS, random_center_requirement, 0, 0,
	"--center requires --gaussian"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema random_schema = BU_ARG_SCHEMA(
    "random", "Generate a random integer within inclusive bounds",
    random_options, random_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, random_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar random_grammar = {
    &random_schema, NULL, NULL
};

int
main(int argc, char *argv[])
{
    struct bn_gauss *gp;
    struct bn_unif *up;

    int high, low;
    struct random_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.seed = bu_pid();

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &random_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&random_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    if (args.help) {
	char *help = bu_arg_schema_help(&random_schema, "random");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "random help");
	bu_vls_free(&msg);
	return 0;
    }
    if (!bu_arg_integer_from_str(&msg, &low, argv[operand_index + 1], 0) ||
	!bu_arg_integer_from_str(&msg, &high, argv[operand_index + 2], 0))
	bu_exit(1, "random: %s", bu_vls_cstr(&msg));
    if (low > high)
	bu_exit(1, "random: low bound must not exceed high bound\n");
    bu_vls_free(&msg);

    if (!bu_arg_schema_option_present(&random_schema, argc - 1,
	    (const char **)(argv + 1), "center"))
	args.center = ((double)high + (double)low) / 2.0;
    if (!args.gaussian && !args.uniform)
	args.uniform = 1;
    if (args.verbose)
	bu_log("%s: seed=%d %s %d %f %d\n", argv[0], args.seed,
	    args.gaussian ? "Gauss" : "Uniform", low, args.center, high);

    if (args.gaussian) {
	double tmp;
	double max;
	max = args.center-(double)low;
	if (max < 0) max = -max;
	tmp = (double)high - args.center;
	if (tmp<0) tmp = -tmp;
	if (tmp > max) max = tmp;
	gp = bn_gauss_init(args.seed, 0);

	tmp = BN_GAUSS_DOUBLE(gp)/3.0;
	tmp = 0.5 + args.center + max*tmp;
	if (tmp < low) tmp = low;
	if (tmp > high) tmp = high;
	fprintf(stdout, "%d\n", (int)tmp);
	return 0;
    }
    if (args.uniform) {
	double tmp;
	up = bn_unif_init(args.seed, 0);
	tmp = (double)high - (double)low + 1.0;
	tmp*=BN_UNIF_DOUBLE(up)+0.5;
	fprintf(stdout, "%d\n", low +(int)tmp);
	return 0;
    }

    return -1;
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
