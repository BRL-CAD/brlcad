/*                           G C V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gcv.cpp
 *
 * Geometry converter.
 *
 */

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "bu/cmdschema.h"

#include "gcv/api.h"

struct fmt_opts {
    struct bu_ptbl args;
};


static void
fmt_opts_init(struct fmt_opts *gfo)
{
    bu_ptbl_init(&gfo->args, 8, "init opts tbl");
}


static void
fmt_opts_free(struct fmt_opts *gfo)
{
    bu_ptbl_free(&gfo->args);
}


struct gcv_args {
    int help;
    const char *help_format;
    const char *in_path;
    const char *out_path;
    const char *in_format;
    const char *out_format;
    bu_mime_model_t in_type;
    bu_mime_model_t out_type;
    struct fmt_opts in_only_opts;
    struct fmt_opts out_only_opts;
    struct fmt_opts both_opts;
    struct bu_ptbl operands;
};


static const struct bu_cmd_arg_shape gcv_filter_options_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_CUSTOM, 1, BU_CMD_COUNT_UNLIMITED,
	"filter options");


static const struct bu_cmd_option gcv_options[] = {
    BU_CMD_OPTIONAL_STRING("h", "help", struct gcv_args, help_format, "format",
	"Print help and exit.  A format may request format-specific help."),
    BU_CMD_ALIAS_SHORT("?", "help", 0),
    BU_CMD_STRING("i", "input", struct gcv_args, in_path, "file", "Input file."),
    BU_CMD_STRING("o", "output", struct gcv_args, out_path, "file", "Output file."),
    BU_CMD_STRING(NULL, "input-format", struct gcv_args, in_format, "format",
	"File format of input file."),
    BU_CMD_STRING(NULL, "output-format", struct gcv_args, out_format, "format",
	"File format of output file."),
    BU_CMD_OPTION_SHAPED("I", "input-only-opts", "input-only-opts", struct gcv_args,
	in_only_opts, BU_CMD_VALUE_RAW, "opts",
	"Options to apply only while processing input.  Continue until the next top-level option.",
	BU_CMD_ARG_REQUIRED, &gcv_filter_options_shape, NULL),
    BU_CMD_OPTION_SHAPED("O", "output-only-opts", "output-only-opts", struct gcv_args,
	out_only_opts, BU_CMD_VALUE_RAW, "opts",
	"Options to apply only while preparing output.  Continue until the next top-level option.",
	BU_CMD_ARG_REQUIRED, &gcv_filter_options_shape, NULL),
    BU_CMD_OPTION_SHAPED("B", "input-and-output-opts", "input-and-output-opts", struct gcv_args,
	both_opts, BU_CMD_VALUE_RAW, "opts",
	"Options to apply to both input and output handling.  Continue until the next top-level option.",
	BU_CMD_ARG_REQUIRED, &gcv_filter_options_shape, NULL),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_operand gcv_operands[] = {
    BU_CMD_OPERAND("path-or-filter-option", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Input/output paths or options shared by both filters.", NULL),
    BU_CMD_OPERAND_NULL
};


static const struct bu_cmd_schema gcv_schema = {
    "gcv", "Geometry conversion.", gcv_options, gcv_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
gcv_top_level_option(const char *arg)
{
    const char *name;
    size_t name_len;
    int longopt;
    size_t i;

    if (!arg || arg[0] != '-' || !arg[1] || BU_STR_EQUAL(arg, "--"))
	return 0;
    longopt = arg[1] == '-';
    name = arg + (longopt ? 2 : 1);
    name_len = strcspn(name, "=");
    for (i = 0; gcv_options[i].canonical; i++) {
	const struct bu_cmd_option *option = &gcv_options[i];
	const char *candidate = longopt ? option->longopt : option->shortopt;
	if (!candidate || strlen(candidate) != name_len)
	    continue;
	if (bu_strncmp(candidate, name, name_len) == 0)
	    return 1;
    }
    return 0;
}


static int
gcv_option_value(const char *arg, int *index, int argc, const char *argv[], const char **value,
	const char *name, struct bu_vls *msg)
{
    const char *equals = strchr(arg, '=');

    if (equals) {
	*value = equals + 1;
	if (!(*value)[0]) {
	    bu_vls_printf(msg, "option argument expected: %s\n", name);
	    return -1;
	}
	return 0;
    }
    if (*index + 1 >= argc) {
	bu_vls_printf(msg, "option argument expected: %s\n", name);
	return -1;
    }
    *value = argv[++(*index)];
    return 0;
}


static int
gcv_model_type_from_str(bu_mime_model_t *type, const char *arg, const char *name,
	struct bu_vls *msg)
{
    int type_int = bu_file_mime(arg, BU_MIME_MODEL);

    if (type_int < 0 || type_int == BU_MIME_MODEL_UNKNOWN) {
	bu_vls_printf(msg, "unknown geometry file type for %s: %s\n", name, arg);
	return -1;
    }
    *type = (bu_mime_model_t)type_int;
    return 0;
}


static int
gcv_args_parse(struct gcv_args *args, struct bu_vls *msg, int argc, const char *argv[])
{
    int i;

    for (i = 0; i < argc; i++) {
	const char *arg = argv[i];
	const char *value = NULL;
	struct fmt_opts *filter_opts = NULL;

	if (BU_STR_EQUAL(arg, "--")) {
	    for (i++; i < argc; i++)
		bu_ptbl_ins(&args->operands, (long *)argv[i]);
	    break;
	}
	if (!gcv_top_level_option(arg)) {
	    bu_ptbl_ins(&args->operands, (long *)arg);
	    continue;
	}
	if (BU_STR_EQUAL(arg, "-h") || BU_STR_EQUAL(arg, "-?") ||
	    BU_STR_EQUAL(arg, "--help") || bu_strncmp(arg, "--help=", 7) == 0) {
	    args->help = 1;
	    if (strchr(arg, '=')) {
		if (gcv_option_value(arg, &i, argc, argv, &value, "--help", msg) != 0)
		    return -1;
		args->help_format = value;
	    } else if (i + 1 < argc && !gcv_top_level_option(argv[i + 1])) {
		args->help_format = argv[++i];
	    }
	    continue;
	}
	if (BU_STR_EQUAL(arg, "-i") || BU_STR_EQUAL(arg, "--input") ||
	    bu_strncmp(arg, "--input=", 8) == 0) {
	    if (gcv_option_value(arg, &i, argc, argv, &value, "--input", msg) != 0)
		return -1;
	    if (!bu_file_exists(value, NULL)) {
		bu_vls_printf(msg, "input file does not exist: %s\n", value);
		return -1;
	    }
	    args->in_path = value;
	    continue;
	}
	if (BU_STR_EQUAL(arg, "-o") || BU_STR_EQUAL(arg, "--output") ||
	    bu_strncmp(arg, "--output=", 9) == 0) {
	    if (gcv_option_value(arg, &i, argc, argv, &value, "--output", msg) != 0)
		return -1;
	    args->out_path = value;
	    continue;
	}
	if (BU_STR_EQUAL(arg, "--input-format") || bu_strncmp(arg, "--input-format=", 15) == 0) {
	    if (gcv_option_value(arg, &i, argc, argv, &value, "--input-format", msg) != 0 ||
		gcv_model_type_from_str(&args->in_type, value, "--input-format", msg) != 0)
		return -1;
	    continue;
	}
	if (BU_STR_EQUAL(arg, "--output-format") || bu_strncmp(arg, "--output-format=", 16) == 0) {
	    if (gcv_option_value(arg, &i, argc, argv, &value, "--output-format", msg) != 0 ||
		gcv_model_type_from_str(&args->out_type, value, "--output-format", msg) != 0)
		return -1;
	    continue;
	}
	if (BU_STR_EQUAL(arg, "-I") || BU_STR_EQUAL(arg, "--input-only-opts") ||
	    bu_strncmp(arg, "--input-only-opts=", 18) == 0)
	    filter_opts = &args->in_only_opts;
	if (BU_STR_EQUAL(arg, "-O") || BU_STR_EQUAL(arg, "--output-only-opts") ||
	    bu_strncmp(arg, "--output-only-opts=", 19) == 0)
	    filter_opts = &args->out_only_opts;
	if (BU_STR_EQUAL(arg, "-B") || BU_STR_EQUAL(arg, "--input-and-output-opts") ||
	    bu_strncmp(arg, "--input-and-output-opts=", 24) == 0)
	    filter_opts = &args->both_opts;
	if (filter_opts) {
	    int start;
	    const char *equals = strchr(arg, '=');
	    if (equals) {
		if (!equals[1]) {
		    bu_vls_printf(msg, "filter options expected: %s\n", arg);
		    return -1;
		}
		bu_ptbl_ins(&filter_opts->args, (long *)(equals + 1));
	    }
	    start = ++i;
	    while (i < argc && !gcv_top_level_option(argv[i]))
		bu_ptbl_ins(&filter_opts->args, (long *)argv[i++]);
	    if (i == start && !equals) {
		bu_vls_printf(msg, "filter options expected: %s\n", arg);
		return -1;
	    }
	    i--;
	    continue;
	}
    }
    return 0;
}


/* given an input string with an optional format specifier of the
 * format /path/to/file.ext:format, this routine saves
 * "/path/to/file.ext" in path.
 *
 * returns 1 if a :format was found on the input string, 0 otherwise
 */
static int
extract_path(struct bu_vls *path, const char *input)
{
    int ret = 0;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;
    const char *colon_pos = NULL;

    if (UNLIKELY(!input))
	return 0;

    colon_pos = strchr(input, ':');

    if (colon_pos) {
	bu_vls_sprintf(&wpath, "%s", input);
	bu_vls_nibble(&wpath, strlen(input) - strlen(colon_pos) + 1);
	if (path && bu_vls_strlen(&wpath) > 0) {
	    ret = 1;
	    bu_vls_sprintf(path, "%s", bu_vls_addr(&wpath));
	}
	bu_vls_free(&wpath);
    } else {
	if (path)
	    bu_vls_sprintf(path, "%s", input);
	ret = 1;
    }

    if (path && !(bu_vls_strlen(path) > 0))
	return 0;

    return ret;
}


static int
extract_format_prefix(struct bu_vls *format, const char *input)
{
    struct bu_vls wformat = BU_VLS_INIT_ZERO;
    char *colon_pos = NULL;
    char *inputcpy = NULL;

    if (UNLIKELY(!input))
	return 0;

    inputcpy = bu_strdup(input);
    colon_pos = strchr(inputcpy, ':');

    if (colon_pos) {
	int ret = 0;
	bu_vls_sprintf(&wformat, "%s", input);
	bu_vls_trunc(&wformat, (int)strlen(colon_pos) * -1);
	/* Note: because Windows supports drive letters in the form {drive}: in
	 * paths, we can't use single characters as format specifiers for the
	 * {format}: prefix - they must be multi-character. */
	if (bu_vls_strlen(&wformat) > 1) {
	    ret = 1;
	    if (format)
		bu_vls_sprintf(format, "%s", bu_vls_addr(&wformat));
	}
	bu_vls_free(&wformat);
	if (inputcpy)
	    bu_free(inputcpy, "input copy");
	return ret;
    } else {
	if (inputcpy)
	    bu_free(inputcpy, "input copy");
	return 0;
    }

    /* Shouldn't get here */
    return 0;
}


static int
parse_model_string(struct bu_vls *format, struct bu_vls *slog, const char *opt, const char *input)
{
    int type_int = 0;
    bu_mime_model_t type = BU_MIME_MODEL_UNKNOWN;

    struct bu_vls format_cpy = BU_VLS_INIT_ZERO;
    struct bu_vls path = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!input) || UNLIKELY(strlen(input) == 0))
	return BU_MIME_MODEL_UNKNOWN;

    /* If an external routine has specified a format string, that string will
     * override the file extension (but not an explicit option or format prefix).
     * Stash any local format string here for later processing.  The idea is
     * to allow some other routine (say, an introspection of a file looking for
     * some signature string) to override a file extension based type identification.
     * Such introspection is beyond the scope of this function, but should override
     * the file extension mechanism. */
    if (format)
	bu_vls_sprintf(&format_cpy, "%s", bu_vls_addr(format));

    /* If we have an explicit option, that overrides any other format specifiers */
    if (opt) {
	type_int = bu_file_mime(opt, BU_MIME_MODEL);
	type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
	if (type == BU_MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (slog)
		bu_vls_printf(slog, "Error: unknown model format \"%s\" specified as an option.\n", opt);
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* Try for a format prefix */
    if (extract_format_prefix(format, input)) {
	/* If we don't already have a valid type and we had a format prefix,
	 * find out if it maps to a valid type */
	if (type == BU_MIME_MODEL_UNKNOWN && format) {
	    /* Yes - see if the prefix specifies a model format */
	    type_int = bu_file_mime(bu_vls_addr(format), BU_MIME_MODEL);
	    type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
	    if (type == BU_MIME_MODEL_UNKNOWN) {
		/* Have prefix, but doesn't result in a known format - that's an error */
		if (slog)
		    bu_vls_printf(slog, "Error: unknown model format \"%s\" specified as a format prefix.\n", bu_vls_addr(format));
		bu_vls_free(&format_cpy);
		return -1;
	    }
	}
    }

    /* If we don't already have a type and we were passed a format string, give it a try */
    if (type == BU_MIME_MODEL_UNKNOWN && format && bu_vls_strlen(&format_cpy) > 0) {
	bu_vls_sprintf(format, "%s", bu_vls_addr(&format_cpy));
	type_int = bu_file_mime(bu_vls_addr(&format_cpy), BU_MIME_MODEL);
	type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
	if (type == BU_MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (slog)
		bu_vls_printf(slog, "Error: unknown model format \"%s\" passed to parse_model_string.\n", bu_vls_addr(format));
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* If we have no prefix or the prefix didn't map to a model type, try file extension */
    if (type == BU_MIME_MODEL_UNKNOWN && extract_path(&path, input)) {
	if (bu_path_component(format, bu_vls_addr(&path), BU_PATH_EXT)) {
	    type_int = bu_file_mime(bu_vls_addr(format), BU_MIME_MODEL);
	    type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
	    if (type == BU_MIME_MODEL_UNKNOWN) {
		/* Have file extension, but doesn't result in a known format - that's an error */
		if (slog)
		    bu_vls_printf(slog, "Error: file extension \"%s\" does not map to a known model format.\n", bu_vls_addr(format));
		bu_vls_free(&format_cpy);
		bu_vls_free(&path);
		return -1;
	    }
	}
    }
    bu_vls_free(&path);
    bu_vls_free(&format_cpy);
    return (int)type;
}


static int
do_conversion(
    struct bu_vls *messages,
    const char *in_path, bu_mime_model_t in_type,
    const char *out_path, bu_mime_model_t out_type,
    size_t in_argc, const char **in_argv,
    size_t out_argc, const char **out_argv)
{
    struct gcv_context context;
    gcv_context_init(&context);
    const struct bu_ptbl * const filters = gcv_list_filters(&context);
    const struct gcv_filter * const *entry;
    const struct gcv_filter *in_filter = NULL, *out_filter = NULL;

    for (BU_PTBL_FOR(entry, (const struct gcv_filter * const *), filters)) {
	if (!entry || !*entry)
	    break;

	bu_mime_model_t emt = (*entry)->mime_type;
	if ((*entry)->filter_type == GCV_FILTER_READ) {
	    if (!in_filter && (emt != BU_MIME_MODEL_AUTO) && (emt == in_type))
		in_filter = *entry;
	    if (!in_filter && (emt == BU_MIME_MODEL_AUTO) &&
		((*entry)->data_supported && in_path && (*(*entry)->data_supported)(in_path))) {
	       	in_filter = *entry;
	    }
	}
	if ((*entry)->filter_type == GCV_FILTER_WRITE) {
	    if (!out_filter && (emt != BU_MIME_MODEL_AUTO) && (emt == out_type))
		out_filter = *entry;
	    if (!out_filter && (emt == BU_MIME_MODEL_AUTO) &&
		((*entry)->data_supported && (*(*entry)->data_supported)(bu_file_mime_str(out_type, BU_MIME_MODEL)))) {
		out_filter = *entry;
	    }
	}
    }

    if (!in_filter)
	bu_vls_printf(messages, "No filter for %s\n", bu_file_mime_str(in_type, BU_MIME_MODEL));
    if (!out_filter)
	bu_vls_printf(messages, "No filter for %s\n", bu_file_mime_str(out_type, BU_MIME_MODEL));
    if (!in_filter || !out_filter) {
	gcv_context_destroy(&context);
	return 0;
    }


    if (!gcv_execute(&context, in_filter, NULL, in_argc, in_argv, in_path)) {
	bu_vls_printf(messages, "Read filter ('%s') failed for '%s'\n", in_filter->name, in_path);
	gcv_context_destroy(&context);
	return 0;
    }

    if (!gcv_execute(&context, out_filter, NULL, out_argc, out_argv, out_path)) {
	bu_vls_printf(messages, "Write filter ('%s') failed for '%s'\n", out_filter->name, out_path);
	gcv_context_destroy(&context);
	return 0;
    }

    gcv_context_destroy(&context);

    return 1;
}


static void
usage(void)
{
    char *help = bu_cmd_schema_describe(&gcv_schema);

    if (help) {
	bu_log("%s\n", help);
	bu_free(help, "help str");
    }
}


int
main(int ac, const char **av)
{
    size_t i;
    size_t operand_count;
    int fmt = 0;
    int ret = 0;
    bu_mime_model_t in_type = BU_MIME_MODEL_UNKNOWN;
    bu_mime_model_t out_type = BU_MIME_MODEL_UNKNOWN;
    struct gcv_args args = {0};
    const char **operand_args = NULL;

    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;

    /* input/output file names at end of argv */
    struct bu_vls in_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls out_path_raw = BU_VLS_INIT_ZERO;

    /* input/output file names finalized */
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;

    struct bu_ptbl in_opts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl out_opts = BU_PTBL_INIT_ZERO;

    struct bu_vls slog = BU_VLS_INIT_ZERO;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    BU_ASSERT(ac > 0 && av);

    bu_setprogname(av[0]);

    args.in_type = BU_MIME_MODEL_UNKNOWN;
    args.out_type = BU_MIME_MODEL_UNKNOWN;
    fmt_opts_init(&args.in_only_opts);
    fmt_opts_init(&args.out_only_opts);
    fmt_opts_init(&args.both_opts);
    bu_ptbl_init(&args.operands, 8, "gcv operands");

    /* skip program name */
    ac--; av++;

    if (ac == 0) {
	usage();
	goto cleanup;
    }

	/* -I/-O/-B take raw filter argument tails, so this command owns the
	 * grammar adapter while gcv_schema remains its single syntax definition. */
    if (gcv_args_parse(&args, &parse_msgs, ac, av) != 0) {
	bu_log("ERROR: option parsing failed\n%s", bu_vls_addr(&parse_msgs));
	ret = 1;
	goto cleanup;
    }

    /* First, see if help was supplied */
	if (args.help) {
	if (args.help_format) {
	    /* TODO - generate format-specific help */
	}
	usage();
	goto cleanup;
    }

    in_type = args.in_type;
    out_type = args.out_type;
    operand_count = BU_PTBL_LEN(&args.operands);
    operand_args = (const char **)args.operands.buffer;

    /* Did we get explicit options for an input and/or output file? */
    if (args.in_path) {
	bu_vls_sprintf(&in_path_raw, "%s", args.in_path);
    }
    if (args.out_path) {
	bu_vls_sprintf(&out_path_raw, "%s", args.out_path);
    }

    /* If not specified explicitly with -i or -o, the input and output paths must always
     * be the last arguments supplied */
    if (args.in_path && !args.out_path && operand_count > 0) {
	bu_vls_sprintf(&out_path_raw, "%s", operand_args[operand_count - 1]);
	operand_count--;
    }
    if (!args.in_path && args.out_path && operand_count > 0) {
	bu_vls_sprintf(&in_path_raw, "%s", operand_args[operand_count - 1]);
	operand_count--;
    }
	if (!args.in_path && !args.out_path) {
	if (operand_count > 1) {
	    bu_vls_sprintf(&in_path_raw, "%s", operand_args[operand_count - 2]);
	    bu_vls_sprintf(&out_path_raw, "%s", operand_args[operand_count - 1]);
	    operand_count -= 2;
	} else if (operand_count == 1) {
	    bu_vls_sprintf(&in_path_raw, "%s", operand_args[operand_count - 1]);
	    operand_count--;
	}
    }

    /* Any unknown strings not otherwise processed are passed to both
     * input and output format processors.  Unknown options are placed
     * at the beginning of the respective option sets so subsequent
     * known options have a chance to override.
     */
    if (operand_count) {
	for (i = 0; i < operand_count; i++) {
	    bu_ptbl_ins(&in_opts, (long *)operand_args[i]);
	    bu_ptbl_ins(&out_opts, (long *)operand_args[i]);
	}
    }
    /* Same for any options that were supplied explicitly to go to
     * both input and output.
     */
    if (BU_PTBL_LEN(&args.both_opts.args) > 0) {
	bu_ptbl_cat(&in_opts, &args.both_opts.args);
	bu_ptbl_cat(&out_opts, &args.both_opts.args);
    }

    /* If we have input and/or output specific options, append them now */
    if (BU_PTBL_LEN(&args.in_only_opts.args) > 0) {
	bu_ptbl_cat(&in_opts, &args.in_only_opts.args);
    }
    if (BU_PTBL_LEN(&args.out_only_opts.args) > 0) {
	bu_ptbl_cat(&out_opts, &args.out_only_opts.args);
    }

    /* See if we have input and output files specified */
    if (!extract_path(&in_path, bu_vls_addr(&in_path_raw))) {
	if (bu_vls_strlen(&in_path_raw) > 0) {
	    bu_vls_printf(&slog, "Error: no input path identified: %s\n", bu_vls_addr(&in_path_raw));
	} else {
	    bu_vls_printf(&slog, "Error: no input path.\n");
	}
	ret = 1;
    }
    if (!extract_path(&out_path, bu_vls_addr(&out_path_raw))) {
	if (bu_vls_strlen(&out_path_raw) > 0) {
	    bu_vls_printf(&slog, "Error: no output path identified: %s\n", bu_vls_addr(&out_path_raw));
	} else {
	    bu_vls_printf(&slog, "Error: no output path.\n");
	}
	ret = 1;
    }

    /* Make sure we have distinct input and output paths */
    if (bu_vls_strlen(&in_path) > 0 && BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_vls_printf(&slog, "Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    /* Find out what input file type we are dealing with */
    if (in_type == BU_MIME_MODEL_UNKNOWN) {
	fmt = parse_model_string(&in_format, &slog, in_fmt, bu_vls_addr(&in_path_raw));
	in_type = (fmt < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)fmt;
	in_fmt = NULL;
    }
    /* Identify output file type */
    if (out_type == BU_MIME_MODEL_UNKNOWN) {
	fmt = parse_model_string(&out_format, &slog, out_fmt, bu_vls_addr(&out_path_raw));
	out_type = (fmt < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)fmt;
	out_fmt = NULL;
    }

    /* If we get to this point without knowing the input type, it's up to the plugins to see if
     * any of them can figure it out. */
    if (in_type == BU_MIME_MODEL_UNKNOWN) {
	in_type = BU_MIME_MODEL_AUTO;
    }

    /* If we get to this point without knowing the *output* type, we've got a problem */
    if (out_type == BU_MIME_MODEL_UNKNOWN) {
	if (bu_vls_strlen(&out_path) > 0) {
	    bu_vls_printf(&slog, "Error: no format type identified for output path: %s\n", bu_vls_addr(&out_path));
	} else {
	    bu_vls_printf(&slog, "Error: no output format type identified.\n");
	}
	ret = 1;
    }

    /* If everything isn't OK, we're done - report and clean up memory */
    if (ret == 1)
	goto cleanup;

    /* If we've gotten this far, we know enough to try to convert. Until we
     * hook in conversion calls to libgcv, print a summary of the option
     * parsing results for debugging. */
    in_fmt = bu_file_mime_str((int)in_type, BU_MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, BU_MIME_MODEL);
    if (in_fmt) {
	bu_log("Input file format: %s\n", in_fmt);
    } else {
	bu_log("Input file format: AUTO\n");
    }
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));

    {
	const size_t in_argc = BU_PTBL_LEN(&in_opts), out_argc = BU_PTBL_LEN(&out_opts);
	const char ** const in_argv = (const char **)in_opts.buffer;
	const char ** const out_argv = (const char **)out_opts.buffer;

	if (!do_conversion(&slog, bu_vls_addr(&in_path), in_type, bu_vls_addr(&out_path),
			   out_type, in_argc, in_argv, out_argc, out_argv)) {
	    bu_vls_printf(&slog, "Conversion failed\n");
	    ret = 1;
	    goto cleanup;
	}
    }

cleanup:
    if (bu_vls_strlen(&slog) > 0 && ret != 0)
	bu_log("%s", bu_vls_addr(&slog));
    if (in_fmt)
	bu_free((char *)in_fmt, "input format string");
    if (out_fmt)
	bu_free((char *)out_fmt, "output format string");

    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&in_path_raw);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&out_path_raw);
    bu_vls_free(&slog);
	bu_vls_free(&parse_msgs);

    bu_ptbl_free(&in_opts);
    bu_ptbl_free(&out_opts);
	bu_ptbl_free(&args.operands);

    fmt_opts_free(&args.in_only_opts);
    fmt_opts_free(&args.out_only_opts);
    fmt_opts_free(&args.both_opts);

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
