/*                       M O D _ A R G S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef SIG_MOD_ARGS_H
#define SIG_MOD_ARGS_H

#include <math.h>

#include "bu/arg.h"
#include "bu/malloc.h"
#include "vmath.h"

#define SIG_MOD_MAX_OPERATIONS 256

enum sig_mod_operation_type {
    SIG_MOD_ADD,
    SIG_MOD_MULTIPLY,
    SIG_MOD_ABSOLUTE,
    SIG_MOD_POWER
};

struct sig_mod_operation {
    enum sig_mod_operation_type type;
    double value;
};

struct sig_mod_operations {
    size_t count;
    struct sig_mod_operation values[SIG_MOD_MAX_OPERATIONS];
};

struct sig_mod_args {
    int help;
    struct sig_mod_operations operations;
};

static int
sig_mod_append(struct bu_vls *msg, struct sig_mod_operations *operations,
	enum sig_mod_operation_type type, double value)
{
    if (!operations)
	return 0;
    if (operations->count >= SIG_MOD_MAX_OPERATIONS) {
	if (msg)
	    bu_vls_printf(msg, "at most %d arithmetic operations are supported\n",
		SIG_MOD_MAX_OPERATIONS);
	return -1;
    }
    operations->values[operations->count].type = type;
    operations->values[operations->count].value = value;
    operations->count++;
    return 0;
}

static int
sig_mod_read_value(struct bu_vls *msg, const char *arg, double *value)
{
    return bu_arg_double_from_str(msg, value, arg) ? 0 : -1;
}

static int
sig_mod_add(struct bu_vls *msg, const char *arg, void *storage)
{
    double value;
    if (sig_mod_read_value(msg, arg, &value) < 0)
	return -1;
    return sig_mod_append(msg, (struct sig_mod_operations *)storage,
	SIG_MOD_ADD, value);
}

static int
sig_mod_subtract(struct bu_vls *msg, const char *arg, void *storage)
{
    double value;
    if (sig_mod_read_value(msg, arg, &value) < 0)
	return -1;
    return sig_mod_append(msg, (struct sig_mod_operations *)storage,
	SIG_MOD_ADD, -value);
}

static int
sig_mod_multiply(struct bu_vls *msg, const char *arg, void *storage)
{
    double value;
    if (sig_mod_read_value(msg, arg, &value) < 0)
	return -1;
    return sig_mod_append(msg, (struct sig_mod_operations *)storage,
	SIG_MOD_MULTIPLY, value);
}

static int
sig_mod_reciprocal(struct bu_vls *msg, const char *arg, void *storage,
	enum sig_mod_operation_type type, const char *label)
{
    double value;
    if (sig_mod_read_value(msg, arg, &value) < 0)
	return -1;
    if (ZERO(value)) {
	if (msg)
	    bu_vls_printf(msg, "%s must not be zero\n", label);
	return -1;
    }
    value = 1.0 / value;
    if (!isfinite(value)) {
	if (msg)
	    bu_vls_printf(msg, "%s reciprocal is outside the finite range\n", label);
	return -1;
    }
    return sig_mod_append(msg, (struct sig_mod_operations *)storage, type,
	value);
}

static int
sig_mod_divide(struct bu_vls *msg, const char *arg, void *storage)
{
    return sig_mod_reciprocal(msg, arg, storage, SIG_MOD_MULTIPLY, "divisor");
}

static int
sig_mod_absolute(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    return sig_mod_append(msg, (struct sig_mod_operations *)storage,
	SIG_MOD_ABSOLUTE, 0.0);
}

static int
sig_mod_exponent(struct bu_vls *msg, const char *arg, void *storage)
{
    double value;
    if (sig_mod_read_value(msg, arg, &value) < 0)
	return -1;
    return sig_mod_append(msg, (struct sig_mod_operations *)storage,
	SIG_MOD_POWER, value);
}

static int
sig_mod_root(struct bu_vls *msg, const char *arg, void *storage)
{
    return sig_mod_reciprocal(msg, arg, storage, SIG_MOD_POWER, "root");
}

static const struct bu_arg_option sig_mod_options[] = {
    BU_ARG_FLAG("h", "help", struct sig_mod_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("a", "add", struct sig_mod_args, operations, sig_mod_add,
	"value", "Add a value"),
    BU_ARG_CUSTOM("s", "subtract", struct sig_mod_args, operations,
	sig_mod_subtract, "value", "Subtract a value"),
    BU_ARG_CUSTOM("m", "multiply", struct sig_mod_args, operations,
	sig_mod_multiply, "value", "Multiply by a value"),
    BU_ARG_CUSTOM("d", "divide", struct sig_mod_args, operations,
	sig_mod_divide, "value", "Divide by a nonzero value"),
    BU_ARG_CUSTOM_FLAG("A", "absolute", "absolute", struct sig_mod_args,
	operations, sig_mod_absolute, "Take the absolute value"),
    BU_ARG_CUSTOM("e", "exponent", struct sig_mod_args, operations,
	sig_mod_exponent, "value", "Raise to a power"),
    BU_ARG_CUSTOM("r", "root", struct sig_mod_args, operations, sig_mod_root,
	"value", "Take a nonzero-index root"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand sig_mod_operands[] = {
    BU_ARG_OPERAND("file", BU_ARG_VALUE_FILE, 0, 1,
	"Optional binary input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static int
sig_mod_args_parse(struct sig_mod_args *args, const char **input_file,
	const char *name, const char *description, int argc, char *argv[])
{
    struct bu_arg_schema schema = BU_ARG_SCHEMA(
	name, description, sig_mod_options, sig_mod_operands,
	BU_ARG_PARSE_OPTIONS_FIRST,
	BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));
    struct bu_arg_grammar grammar = {&schema, NULL, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    int introspection = bu_arg_introspect(&msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : -1;
    }
    argc--; argv++;
    int operand_index = bu_arg_schema_parse(&schema, args, &msg,
	argc, (const char **)argv);
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (args->help) {
	char *help = bu_arg_schema_help(&schema, name);
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "signal modifier help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;
    if (argc > 1) {
	bu_flog(stderr, "%s: too many input files\n", name);
	return -1;
    }
    *input_file = argc ? argv[0] : NULL;
    return 1;
}

#endif /* SIG_MOD_ARGS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
