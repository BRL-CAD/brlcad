/*                           G C V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

#include "gcv/api.h"

struct fmt_opts {
    struct bu_ptbl *args;
    struct bu_opt_desc *ds;
};


static void
fmt_opts_init(struct fmt_opts *gfo, struct bu_opt_desc *ds)
{
    BU_GET(gfo->args, struct bu_ptbl);
    bu_ptbl_init(gfo->args, 8, "init opts tbl");
    gfo->ds = ds;
}


static void
fmt_opts_free(struct fmt_opts *gfo)
{
    bu_ptbl_free(gfo->args);
    BU_PUT(gfo->args, struct bu_ptbl);
    gfo->ds = NULL;
    gfo->args = NULL;
}


static int
fmt_fun(struct bu_vls *UNUSED(msgs), size_t argc, const char **argv, void *set_var)
{
    size_t i = 0;
    int args_used = 0;
    struct fmt_opts *gfo = (struct fmt_opts *)set_var;

    if (!gfo)
	return -1;

    if (!argv || argc < 1)
	return 0;

    for (i = 0; i < argc; i++) {
	struct bu_vls cmp_arg = BU_VLS_INIT_ZERO;
	const char *arg = argv[i]+1;
	const char *equal_pos;
	int d_ind = 0;
	int in_desc = 0;
	struct bu_opt_desc *d = &(gfo->ds[d_ind]);

	if (arg[0] == '-')
	    arg++;
	equal_pos = strchr(arg, '=');
	bu_vls_sprintf(&cmp_arg, "%s", arg);
	if (equal_pos)
	    bu_vls_trunc(&cmp_arg, -1 * strlen(equal_pos));

	while (((d->shortopt && strlen(d->shortopt) > 0) || (d->longopt && strlen(d->longopt) > 0)) && !in_desc) {
	    if (BU_STR_EQUAL(bu_vls_addr(&cmp_arg), d->shortopt) || BU_STR_EQUAL(bu_vls_addr(&cmp_arg), d->longopt)) {
		/* Top level option hit - we're done */
		bu_vls_free(&cmp_arg);
		return args_used;
	    } else {
		d_ind++;
		d = &(gfo->ds[d_ind]);
	    }
	}
	bu_ptbl_ins(gfo->args, (long *)argv[i]);
	args_used++;
	bu_vls_free(&cmp_arg);
    }

    return args_used;
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
    char *colon_pos = NULL;

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
	bu_vls_trunc(&wformat, -1 * strlen(colon_pos));
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
file_stat(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "input file");

    if (!bu_file_exists(argv[0], NULL)) {
	if (msg) {
	    bu_vls_sprintf(msg, "Error - file %s does not exist!\n", argv[0]);
	}
	return -1;
    }

    if (file_set)
	(*file_set) = bu_strdup(argv[0]);

    return 1;
}


static int
file_null(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output file");

    if (bu_file_exists(argv[0], NULL)) {
	if (msg) {
	    bu_vls_sprintf(msg, "Note - file %s already exists, appending conversion output\n", argv[0]);
	}
    }

    if (file_set)
	(*file_set) = bu_strdup(argv[0]);

    return 1;
}


static int
model_mime(struct bu_vls *msg, size_t argc, const char **argv, void *set_mime)
{
    int type_int;
    bu_mime_model_t type = BU_MIME_MODEL_UNKNOWN;
    bu_mime_model_t *set_type = (bu_mime_model_t *)set_mime;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "mime format");

    type_int = bu_file_mime(argv[0], BU_MIME_MODEL);
    type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
    if (type == BU_MIME_MODEL_UNKNOWN) {
	if (msg) {
	    bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", argv[0]);
	}
	return -1;
    }
    if (set_type)
	(*set_type) = type;
    return 1;
}


struct help_state {
    int showhelp;
    char *format;
};


static int
help(struct bu_vls *UNUSED(msg), size_t argc, const char **argv, void *set_var)
{
    struct help_state *gs = (struct help_state *)set_var;
    if (gs)
	gs->showhelp = 1;

    if (!argv || !argv[0] || strlen(argv[0]) || argc == 0) {
	return 0;
    } else {
	if (gs) {
	    gs->format = bu_strdup(argv[0]);
	}
    }
    return 1;
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


void
usage(const struct bu_opt_desc *opts) {
    char *help = bu_opt_describe(opts, NULL);
    if (help) {
	bu_log("%s\n", help);
	bu_free(help, "help str");
    }
}


int
main(int ac, const char **av)
{
    size_t i;
    int fmt = 0;
    int ret = 0;

    static bu_mime_model_t in_type = BU_MIME_MODEL_UNKNOWN;
    static bu_mime_model_t out_type = BU_MIME_MODEL_UNKNOWN;

    static struct fmt_opts in_only_opts;
    static struct fmt_opts out_only_opts;
    static struct fmt_opts both_opts;
    static struct help_state hs;

    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;

    /* input/output file names as read by bu_opt */
    static char *in_str = NULL;
    static char *out_str = NULL;

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
    int unknown_ac = 0;

#define STR_HELP "Print help and exit.  If a format is specified to --help, print help specific to that format"
#define STR_INOPT "Options to apply only while processing input file.  Accepts options until another toplevel option is encountered."
#define STR_OUTOPT "Options to apply only while preparing output file.  Accepts options until another toplevel option is encountered."
#define STR_BOTH "Options to apply both during input and output handling.  Accepts options until another toplevel option is encountered."

    struct bu_opt_desc options[] = {
	{"h", "help",             "[format]",  &help,    (void *)&hs,            STR_HELP,                     },
	{"?", "",                 "[format]",  &help,    (void *)&hs,            "",                           },
	{"i", "input",            "file",      &file_stat,   (void *)&in_str,    "Input file.",                },
	{"o", "output",           "file",      &file_null,   (void *)&out_str,   "Output file.",               },
	{"",  "input-format",     "format",    &model_mime,  (void *)&in_type,   "File format of input file.", },
	{"",  "output-format",    "format",    &model_mime,  (void *)&out_type,  "File format of output file." },
	{"I", "input-only-opts",  "opts",      &fmt_fun, (void *)&in_only_opts,  STR_INOPT,                    },
	{"O", "output-only-opts", "opts",      &fmt_fun, (void *)&out_only_opts, STR_OUTOPT,                   },
	{"B", "input-and-output-opts", "opts", &fmt_fun, (void *)&both_opts,     STR_BOTH,                     },
	BU_OPT_DESC_NULL
    };

    BU_ASSERT(ac > 0 && av);

    bu_setprogname(av[0]);

    fmt_opts_init(&in_only_opts, options);
    fmt_opts_init(&out_only_opts, options);
    fmt_opts_init(&both_opts, options);

    hs.showhelp = 0;
    hs.format = NULL;

    /* skip program name */
    ac--; av++;

    if (ac == 0) {
	usage(options);
	goto cleanup;
    }

    unknown_ac = bu_opt_parse(&parse_msgs, ac, av, options);

    if (unknown_ac < 0) {
        bu_log("ERROR: option parsing failed\n%s", bu_vls_addr(&parse_msgs));
	goto cleanup;
    }

    /* First, see if help was supplied */
    if (hs.showhelp) {
	if (hs.format) {
	    /* TODO - generate format-specific help */
	}
	usage(options);

#if 0
	/* TODO - figure out how to get this info from each plugin to construct this table */
	/* on the fly... */
	bu_log("\nSupported formats:\n");
	bu_log(" ------------------------------------------------------------\n");
	bu_log(" | Extension  |          File Format      |  Input | Output |\n");
	bu_log(" |----------------------------------------------------------|\n");
	bu_log(" |    stl     |   STereoLithography       |   Yes  |   Yes  |\n");
	bu_log(" |------------|---------------------------|--------|--------|\n");
	bu_log(" |    obj     |   Wavefront Object        |   Yes  |   Yes  |\n");
	bu_log(" |------------|---------------------------|--------|--------|\n");
	bu_log(" |    step    |   STEP (AP203)            |   Yes  |   Yes  |\n");
	bu_log(" |------------|---------------------------|--------|--------|\n");
	bu_log(" |    iges    |   Initial Graphics        |   Yes  |   No   |\n");
	bu_log(" |            |   Exchange Specification  |        |        |\n");
	bu_log(" |----------------------------------------------------------|\n");
#endif

	goto cleanup;
    }

    // Remaining av handling will use the ac count from bu_opt_parse
    ac = unknown_ac;

    /* Did we get explicit options for an input and/or output file? */
    if (in_str) {
	bu_vls_sprintf(&in_path_raw, "%s", in_str);
    }
    if (out_str) {
	bu_vls_sprintf(&out_path_raw, "%s", out_str);
    }

    /* If not specified explicitly with -i or -o, the input and output paths must always
     * be the last arguments supplied */
    if (in_str && !out_str) {
	bu_vls_sprintf(&out_path_raw, "%s", av[ac - 1]);
	av[ac - 1] = NULL;
	ac--;
	unknown_ac--;
    }
    if (!in_str && out_str) {
	bu_vls_sprintf(&in_path_raw, "%s", av[ac - 1]);
	av[ac - 1] = NULL;
	ac--;
	unknown_ac--;
    }
    if (!in_str && !out_str) {
	if (ac > 1) {
	    bu_vls_sprintf(&in_path_raw, "%s", av[ac - 2]);
	    bu_vls_sprintf(&out_path_raw, "%s", av[ac - 1]);
	    av[ac - 1] = av[ac - 2] = NULL;
	    unknown_ac -= 2;
	} else if (ac == 1) {
	    bu_vls_sprintf(&in_path_raw, "%s", av[ac - 1]);
	    av[ac - 1] = NULL;
	    unknown_ac--;
	}
    }

    /* Any unknown strings not otherwise processed are passed to both
     * input and output format processors.  Unknown options are placed
     * at the beginning of the respective option sets so subsequent
     * known options have a chance to override.
     */
    if (unknown_ac) {
	for (i = 0; i < (size_t)unknown_ac; i++) {
	    bu_ptbl_ins(&in_opts, (long *)av[i]);
	    bu_ptbl_ins(&out_opts, (long *)av[i]);
	}
    }
    /* Same for any options that were supplied explicitly to go to
     * both input and output.
     */
    if (BU_PTBL_LEN(both_opts.args) > 0) {
	bu_ptbl_cat(&in_opts, both_opts.args);
	bu_ptbl_cat(&out_opts, both_opts.args);
    }

    /* If we have input and/or output specific options, append them now */
    if (BU_PTBL_LEN(in_only_opts.args) > 0) {
	bu_ptbl_cat(&in_opts, in_only_opts.args);
    }
    if (BU_PTBL_LEN(out_only_opts.args) > 0) {
	bu_ptbl_cat(&out_opts, out_only_opts.args);
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
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&slog);

    bu_ptbl_free(&in_opts);
    bu_ptbl_free(&out_opts);

    fmt_opts_free(&in_only_opts);
    fmt_opts_free(&out_only_opts);
    fmt_opts_free(&both_opts);

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
