/*                           G C V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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

struct gcv_fmt_opts {
    struct bu_ptbl *args;
    struct bu_opt_desc *ds;
};

void gcv_fmt_opts_init(struct gcv_fmt_opts *gfo, struct bu_opt_desc *ds)
{
    BU_GET(gfo->args, struct bu_ptbl);
    bu_ptbl_init(gfo->args, 8, "init opts tbl");
    gfo->ds = ds;
}

void gcv_fmt_opts_free(struct gcv_fmt_opts *gfo)
{
    bu_ptbl_free(gfo->args);
    BU_PUT(gfo->args, struct bu_ptbl);
}

int
gcv_fmt_fun(struct bu_vls *UNUSED(msgs), int argc, const char **argv, void *set_var)
{
    int i = 0;
    int args_used = 0;
    struct gcv_fmt_opts *gfo = (struct gcv_fmt_opts *)set_var;

    if (!argv || argc < 1 ) return 0;

    for (i = 0; i < argc; i++) {
	struct bu_vls cmp_arg = BU_VLS_INIT_ZERO;
	const char *arg = argv[i]+1;
	const char *equal_pos;
	int d_ind = 0;
	int in_desc = 0;
	struct bu_opt_desc *d = &(gfo->ds[d_ind]);

	if (arg[0] == '-') arg++;
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

int
io_opt_files(int argc, const char **argv)
{
    int i_opts = 0;
    int o_opts = 0;
    const char *equal_pos = NULL;
    int i = 0;
    for (i = 0; i < argc; i++) {
	struct bu_vls cmp_arg = BU_VLS_INIT_ZERO;
	const char *arg = argv[i];
	if (arg[0] != '-') continue;
	arg++;
	if (arg[0] == '-') arg++;
	equal_pos = strchr(arg, '=');
	bu_vls_sprintf(&cmp_arg, "%s", arg);
	if (equal_pos)
	    bu_vls_trunc(&cmp_arg, -1 * strlen(equal_pos));

	if (BU_STR_EQUAL(bu_vls_addr(&cmp_arg), "input")) i_opts++;
	if (BU_STR_EQUAL(bu_vls_addr(&cmp_arg), "output")) o_opts++;
    }
    i_opts = (i_opts > 0) ? 1 : 0;
    o_opts = (o_opts > 0) ? 1 : 0;
    return i_opts + o_opts;
}

/* Emulate a FASTGEN4 format option processor */
void fast4_arg_process(int argc, const char **argv) {
    int i = 0;
    int ret_argc = 0;
    static int tol = 0.0;
    static int w_flag;
    struct bu_opt_desc fg4_opt_desc[] = {
	{"t", "tol",                "#", &bu_opt_int, (void *)&tol,    "Dimensional tolerance."},
	{"w", "warn-default-names", "" , NULL,        (void *)&w_flag, "File format of input file."},
	BU_OPT_DESC_NULL
    };

    ret_argc = bu_opt_parse(NULL, argc, (const char **)argv, fg4_opt_desc);

    if (w_flag)	bu_log("FASTGEN 4 warn default names set\n");
    bu_log("FASTGEN 4 tol: %d\n", tol);

    if (ret_argc) {
	bu_log("Unknown args: ");
	for (i = 0; i < ret_argc - 1; i++) {
	    bu_log("%s, ", argv[i]);
	}
	bu_log("%s\n", argv[ret_argc - 1]);
    }
}

void stl_arg_process(int argc, const char **argv) {
    int i= 0;
    int ret_argc = 0;
    static int tol = 0.0;
    static int units = 0;
    struct bu_opt_desc stl_opt_desc[3] = {
	{"t",  "tol",   "#", &bu_opt_int, (void *)&tol,   "Dimensional tolerance." },
	{"u",  "units", "#", &bu_opt_int, (void *)&units, "Units of input file." },
	BU_OPT_DESC_NULL
    };

    ret_argc = bu_opt_parse(NULL, argc, (const char **)argv, stl_opt_desc);

    bu_log("STL tol: %d\n", tol);
    bu_log("STL units: %d\n", units);

    if (ret_argc) {
	bu_log("Unknown args: ");
	for (i = 0; i < ret_argc - 1; i++) {
	    bu_log("%s, ", argv[i]);
	}
	bu_log("%s\n", argv[ret_argc - 1]);
    }
}

HIDDEN int
extract_path(struct bu_vls *path, const char *input)
{
    int ret = 0;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;
    char *colon_pos = NULL;
    char *inputcpy = NULL;
    if (UNLIKELY(!input)) return 0;
    inputcpy = bu_strdup(input);
    colon_pos = strchr(inputcpy, ':');
    if (colon_pos) {
	bu_vls_sprintf(&wpath, "%s", input);
	bu_vls_nibble(&wpath, strlen(input) - strlen(colon_pos) + 1);
	if (path && bu_vls_strlen(&wpath) > 0) {
	    ret = 1;
	    bu_vls_sprintf(path, "%s", bu_vls_addr(&wpath));
	}
	bu_vls_free(&wpath);
    } else {
	if (path) bu_vls_sprintf(path, "%s", input);
	ret = 1;
    }
    if (inputcpy) bu_free(inputcpy, "input copy");
    if (path && !(bu_vls_strlen(path) > 0)) return 0;
    return ret;
}

HIDDEN int
extract_format_prefix(struct bu_vls *format, const char *input)
{
    struct bu_vls wformat = BU_VLS_INIT_ZERO;
    char *colon_pos = NULL;
    char *inputcpy = NULL;
    if (UNLIKELY(!input)) return 0;
    inputcpy = bu_strdup(input);
    colon_pos = strchr(inputcpy, ':');
    if (colon_pos) {
	int ret = 0;
	bu_vls_sprintf(&wformat, "%s", input);
	bu_vls_trunc(&wformat, -1 * strlen(colon_pos));
	if (bu_vls_strlen(&wformat) > 0) {
	    ret = 1;
	    if (format) bu_vls_sprintf(format, "%s", bu_vls_addr(&wformat));
	}
	bu_vls_free(&wformat);
	if (inputcpy) bu_free(inputcpy, "input copy");
	return ret;
    } else {
	if (inputcpy) bu_free(inputcpy, "input copy");
	return 0;
    }
    /* Shouldn't get here */
    return 0;
}

int
parse_model_string(struct bu_vls *format, struct bu_vls *slog, const char *opt, const char *input)
{
    int type_int = 0;
    bu_mime_model_t type = BU_MIME_MODEL_UNKNOWN;

    struct bu_vls format_cpy = BU_VLS_INIT_ZERO;
    struct bu_vls path = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!input) || UNLIKELY(strlen(input) == 0)) return BU_MIME_MODEL_UNKNOWN;

    /* If an external routine has specified a format string, that string will
     * override the file extension (but not an explicit option or format prefix).
     * Stash any local format string here for later processing.  The idea is
     * to allow some other routine (say, an introspection of a file looking for
     * some signature string) to override a file extension based type identification.
     * Such introspection is beyond the scope of this function, but should override
     * the file extension mechanism. */
    if (format) bu_vls_sprintf(&format_cpy, "%s", bu_vls_addr(format));

    /* If we have an explicit option, that overrides any other format specifiers */
    if (opt) {
	type_int = bu_file_mime(opt, BU_MIME_MODEL);
	type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
	if (type == BU_MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (slog) bu_vls_printf(slog, "Error: unknown model format \"%s\" specified as an option.\n", opt);
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
		if (slog) bu_vls_printf(slog, "Error: unknown model format \"%s\" specified as a format prefix.\n", bu_vls_addr(format));
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
	    if (slog) bu_vls_printf(slog, "Error: unknown model format \"%s\" passed to parse_model_string.\n", bu_vls_addr(format));
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
		if (slog) bu_vls_printf(slog, "Error: file extension \"%s\" does not map to a known model format.\n", bu_vls_addr(format));
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

int
file_stat(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "input file");

    if (!bu_file_exists(argv[0], NULL)){
	if (msg) bu_vls_sprintf(msg, "Error - file %s does not exist!\n", argv[0]);
	return -1;
    }

    if (file_set) (*file_set) = bu_strdup(argv[0]);

    return 1;
}

int
file_null(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output file");

    if (bu_file_exists(argv[0], NULL)){
	if (msg) bu_vls_sprintf(msg, "Error - file %s already exists!\n", argv[0]);
	return -1;
    }

    if (file_set) (*file_set) = bu_strdup(argv[0]);

    return 1;
}

int
model_mime(struct bu_vls *msg, int argc, const char **argv, void *set_mime)
{
    int type_int;
    bu_mime_model_t type = BU_MIME_MODEL_UNKNOWN;
    bu_mime_model_t *set_type = (bu_mime_model_t *)set_mime;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "mime format");

    type_int = bu_file_mime(argv[0], BU_MIME_MODEL);
    type = (type_int < 0) ? BU_MIME_MODEL_UNKNOWN : (bu_mime_model_t)type_int;
    if (type == BU_MIME_MODEL_UNKNOWN) {
	if (msg) bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", argv[0]);
	return -1;
    }
    if (set_type) (*set_type) = type;
    return 1;
}


struct gcv_help_state {
    int flag;
    char *format;
};

int
gcv_help(struct bu_vls *UNUSED(msg), int argc, const char **argv, void *set_var)
{
    struct gcv_help_state *gs = (struct gcv_help_state *)set_var;
    if (gs) gs->flag = 1;

    if (!argv || strlen(argv[0]) || argc == 0) {
	return 0;
    } else {
	if (gs) gs->format = bu_strdup(argv[0]);
    }
    return 1;
}


HIDDEN int
gcv_do_conversion(
	struct bu_vls *messages,
	const char *in_path, bu_mime_model_t in_type,
	const char *out_path, bu_mime_model_t out_type,
	size_t in_argc, const char **in_argv,
	size_t out_argc, const char **out_argv)
{
    const struct bu_ptbl * const filters = gcv_list_filters();
    const struct gcv_filter * const *entry;
    const struct gcv_filter *in_filter = NULL, *out_filter = NULL;
    struct gcv_context context;

    for (BU_PTBL_FOR(entry, (const struct gcv_filter * const *), filters)) {
	if ((*entry)->filter_type == GCV_FILTER_READ && (*entry)->mime_type == in_type)
	    in_filter = *entry;
	else if ((*entry)->filter_type == GCV_FILTER_WRITE && (*entry)->mime_type == out_type)
	    out_filter = *entry;
    }

    if (!in_filter)
	bu_vls_printf(messages, "No filter for %s\n", bu_file_mime_str(in_type, BU_MIME_MODEL));
    if (!out_filter)
	bu_vls_printf(messages, "No filter for %s\n", bu_file_mime_str(out_type, BU_MIME_MODEL));
    if (!in_filter || !out_filter)
	return 0;

    gcv_context_init(&context);

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


#define gcv_help_str "Print help and exit.  If a format is specified to --help, print help specific to that format"

#define gcv_inopt_str "Options to apply only while processing input file.  Accepts options until another toplevel option is encountered."

#define gcv_outopt_str "Options to apply only while preparing output file.  Accepts options until another toplevel option is encountered."
#define gcv_both_str "Options to apply both during input and output handling.  Accepts options until another toplevel option is encountered."

int
main(int ac, const char **av)
{
    size_t i;
    int fmt = 0;
    int ret = 0;
    int skip_in = 0;
    int skip_out = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    static bu_mime_model_t in_type = BU_MIME_MODEL_UNKNOWN;
    static bu_mime_model_t out_type = BU_MIME_MODEL_UNKNOWN;
    static char *in_path_str = NULL;
    static char *out_path_str = NULL;
    static struct gcv_fmt_opts in_only_opts;
    static struct gcv_fmt_opts out_only_opts;
    static struct gcv_fmt_opts both_opts;
    static struct gcv_help_state hs;

    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    struct bu_ptbl input_opts = BU_PTBL_INIT_ZERO;
    struct bu_ptbl output_opts = BU_PTBL_INIT_ZERO;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int uac = 0;
    int io_opt_cnt = io_opt_files(ac, av);


    struct bu_opt_desc gcv_opt_desc[] = {
	{"h", "help",             "[format]",   &gcv_help,    (void *)&hs,            gcv_help_str,                 },
	{"?", "",                 "[format]",   &gcv_help,    (void *)&hs,            "",                           },
	{"i", "input",            "file",       &file_stat,   (void *)&in_path_str,   "Input file.",                },
	{"o", "output",           "file",       &file_null,   (void *)&out_path_str,  "Output file.",               },
	{"",  "input-format",     "format",     &model_mime,  (void *)&in_type,       "File format of input file.", },
	{"",  "output-format",    "format",     &model_mime,  (void *)&out_type,      "File format of output file." },
	{"I", "input-only-opts",  "opts",       &gcv_fmt_fun, (void *)&in_only_opts,  gcv_inopt_str,                },
	{"O", "output-only-opts", "opts",       &gcv_fmt_fun, (void *)&out_only_opts, gcv_outopt_str,               },
	{"B", "input-and-output-opts", "opts",  &gcv_fmt_fun, (void *)&both_opts,     gcv_both_str,                 },
	BU_OPT_DESC_NULL
    };

    gcv_fmt_opts_init(&in_only_opts, gcv_opt_desc);
    gcv_fmt_opts_init(&out_only_opts, gcv_opt_desc);
    gcv_fmt_opts_init(&both_opts, gcv_opt_desc);

    hs.flag = 0;
    hs.format = NULL;

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    if (ac == 0) {
	const char *help = bu_opt_describe(gcv_opt_desc, NULL);
	bu_log("%s\n", help);
	if (help) bu_free((char *)help, "help str");
	/* TODO - print some help */
	goto cleanup;
    }

    uac = bu_opt_parse(&parse_msgs, ac - 2 + io_opt_cnt, av, gcv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	goto cleanup;
    }

    /* First, see if help was supplied */
    if (hs.flag) {
	if (hs.format) {
	    /* TODO - generate some help based on format */
	} else {
	    { /* Test static help print  */
		const char *help = bu_opt_describe(gcv_opt_desc, NULL);
		bu_log("Options:\n");
		bu_log("%s\n", help);
		if (help) bu_free((char *)help, "help str");
	    }

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
	}
	goto cleanup;
    }

    /* TODO - Do a general check on option validity here - if anything fails, halt and
     * report it */


    /* Did we get explicit options for an input and/or output file? */
    if (in_path_str) {
	bu_vls_sprintf(&in_path_raw, "%s", in_path_str);
	skip_in++;
    }
    if (out_path_str) {
	bu_vls_sprintf(&out_path_raw, "%s", out_path_str);
	skip_out++;
    }

    /* If not specified explicitly with -i or -o, the input and output paths must always
     * be the last two arguments supplied */
    if (!(skip_in && skip_out)) {
	if (skip_in && !skip_out) {
	    bu_vls_sprintf(&out_path_raw, "%s", av[ac - 1]);
	}
	if (!skip_in && skip_out) {
	    bu_vls_sprintf(&in_path_raw, "%s", av[ac - 1]);
	}
	if (!skip_in && !skip_out) {
	    if (ac > 1) {
		bu_vls_sprintf(&in_path_raw, "%s", av[ac - 2]);
		bu_vls_sprintf(&out_path_raw, "%s", av[ac - 1]);
	    } else {
		bu_vls_sprintf(&in_path_raw, "%s", av[ac - 1]);
	    }
	}
    }

    /* Any unknown strings not otherwise processed are passed to both input and output.
     * These are deliberately placed at the beginning of the input strings, so any
     * input/output specific options have a chance to override them. */
    if (uac) {
	for (i = 0; i < (size_t)uac; i++) {
	    bu_ptbl_ins(&input_opts, (long *)av[i]);
	    bu_ptbl_ins(&output_opts, (long *)av[i]);
	}
    }
    /* Same for any options that were supplied explicitly to go to both input and output */
    if (BU_PTBL_LEN(both_opts.args) > 0) {
	bu_ptbl_cat(&input_opts, both_opts.args);
	bu_ptbl_cat(&output_opts, both_opts.args);
    }

    /* If we have input and/or output specific options, append them now */
    if (BU_PTBL_LEN(in_only_opts.args) > 0) {
	bu_ptbl_cat(&input_opts, in_only_opts.args);
    }
    if (BU_PTBL_LEN(out_only_opts.args) > 0) {
	bu_ptbl_cat(&output_opts, out_only_opts.args);
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

    /* If we get to this point without knowing both input and output types, we've got a problem */
    if (in_type == BU_MIME_MODEL_UNKNOWN) {
	if (bu_vls_strlen(&in_path) > 0) {
	    bu_vls_printf(&slog, "Error: no format type identified for input path: %s\n", bu_vls_addr(&in_path));
	} else {
	    bu_vls_printf(&slog, "Error: no input format type identified.\n");
	}
	ret = 1;
    }
    if (out_type == BU_MIME_MODEL_UNKNOWN) {
	if (bu_vls_strlen(&out_path) > 0) {
	    bu_vls_printf(&slog, "Error: no format type identified for output path: %s\n", bu_vls_addr(&out_path));
	} else {
	    bu_vls_printf(&slog, "Error: no output format type identified.\n");
	}
	ret = 1;
    }

    /* If everything isn't OK, we're done - report and clean up memory */
    if (ret == 1) goto cleanup;

    /* If we've gotten this far, we know enough to try to convert. Until we
     * hook in conversion calls to libgcv, print a summary of the option
     * parsing results for debugging. */
    in_fmt = bu_file_mime_str((int)in_type, BU_MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, BU_MIME_MODEL);
    bu_log("Input file format: %s\n", in_fmt);
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));


#if 1
    {
	const size_t in_argc = BU_PTBL_LEN(&input_opts), out_argc = BU_PTBL_LEN(&output_opts);
	const char ** const in_argv = (const char **)input_opts.buffer;
	const char ** const out_argv = (const char **)output_opts.buffer;

	if (!gcv_do_conversion(&slog, bu_vls_addr(&in_path), in_type, bu_vls_addr(&out_path),
		    out_type, in_argc, in_argv, out_argc, out_argv)) {
	    bu_vls_printf(&slog, "Conversion failed\n");
	    ret = 1;
	    goto cleanup;
	}
    }

#else
    switch (in_type) {
	case BU_MIME_MODEL_VND_FASTGEN:
	    fast4_arg_process(BU_PTBL_LEN(&input_opts), (const char **)input_opts.buffer);
	    break;
	case BU_MIME_MODEL_STL:
	    stl_arg_process(BU_PTBL_LEN(&input_opts), (const char **)input_opts.buffer);
	default:
	    break;
    }

    switch (out_type) {
	case BU_MIME_MODEL_VND_FASTGEN:
	    fast4_arg_process(BU_PTBL_LEN(&output_opts), (const char **)output_opts.buffer);
	    break;
	case BU_MIME_MODEL_STL:
	    stl_arg_process(BU_PTBL_LEN(&output_opts), (const char **)output_opts.buffer);
	default:
	    break;
    }
#endif


    /* Clean up */
cleanup:
    if (bu_vls_strlen(&slog) > 0) bu_log("%s", bu_vls_addr(&slog));
    if (in_fmt) bu_free((char *)in_fmt, "input format string");
    if (out_fmt) bu_free((char *)out_fmt, "output format string");
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&slog);
    bu_ptbl_free(&input_opts);
    bu_ptbl_free(&output_opts);
    gcv_fmt_opts_free(&in_only_opts);
    gcv_fmt_opts_free(&out_only_opts);
    gcv_fmt_opts_free(&both_opts);
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
