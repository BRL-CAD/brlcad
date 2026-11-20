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
fmt_fun(struct bu_vls *msgs, size_t argc, const char **argv, void *set_var)
{
    size_t i = 0;
    int args_used = 0;
    struct fmt_opts *gfo = (struct fmt_opts *)set_var;

    if (!gfo)
	return -1;

    if (!argv || argc < 1) {
	if (msgs)
	    bu_vls_printf(msgs, "expected at least one filter option\n");
	return msgs ? -1 : 0;
    }

    for (i = 0; i < argc; i++) {
	struct bu_vls cmp_arg = BU_VLS_INIT_ZERO;
	const char *arg = argv[i];
	const char *equal_pos;
	int d_ind = 0;
	int in_desc = 0;
	struct bu_opt_desc *d = &(gfo->ds[d_ind]);

	/* Values such as "0" are filter arguments, not an empty spelling of
	 * a top-level option.  Strip dashes only from actual option tokens. */
	if (arg[0] != '-' || arg[1] == '\0') {
	    bu_ptbl_ins(gfo->args, (long *)argv[i]);
	    args_used++;
	    continue;
	}
	arg++;
	if (arg[0] == '-')
	    arg++;
	equal_pos = strchr(arg, '=');
	bu_vls_sprintf(&cmp_arg, "%s", arg);
	if (equal_pos)
	    bu_vls_trunc(&cmp_arg, (int)strlen(equal_pos) * -1);

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

/* A single-letter prefix is a drive letter, not a format selector. */
static size_t
format_prefix_length(const char *input)
{
    const char *colon = input ? strchr(input, ':') : NULL;
    return colon && colon - input > 1 ? (size_t)(colon - input) : 0;
}

static int
extract_path(struct bu_vls *path, const char *input)
{
    size_t prefix;
    if (!input || !input[0])
	return 0;
    prefix = format_prefix_length(input);
    bu_vls_strcpy(path, input + (prefix ? prefix + 1 : 0));
    return bu_vls_strlen(path) > 0;
	}

	static int
	extract_format_prefix(struct bu_vls *format, const char *input)
	{
	    size_t length = format_prefix_length(input);
	    if (length)
		bu_vls_strncpy(format, input, length);
	    return length != 0;
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

    if (file_set) {
	if (*file_set)
	    bu_free(*file_set, "previous input file option");
	(*file_set) = bu_strdup(argv[0]);
    }

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

    if (file_set) {
	if (*file_set)
	    bu_free(*file_set, "previous output file option");
	(*file_set) = bu_strdup(argv[0]);
    }

    return 1;
}


struct help_state {
    int showhelp;
    char *format;
};


static int
help(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct help_state *gs = (struct help_state *)set_var;
    if (!argv || !argv[0] || argv[0][0] == '\0' || argv[0][0] == '-' || argc == 0) {
	if (gs && msg)
	    gs->showhelp = 1;
	return 0;
    }

    if (gs && msg) {
	gs->showhelp = 1;
	if (gs->format)
	    bu_free(gs->format, "previous help format");
	gs->format = bu_strdup(argv[0]);
    }

    return 1;
}

/* Preserve old CLI selectors at the compatibility boundary, without
 * registering backend names as data formats. */
static int
legacy_backend_format(struct bu_vls *format, struct bu_vls *handler, int writing, struct bu_vls *messages)
{
    const char *backend = NULL;
    if (BU_STR_EQUIV(bu_vls_cstr(format), "gdal"))
	backend = writing ? "GDAL Writer" : "GDAL Reader";
    else if (BU_STR_EQUIV(bu_vls_cstr(format), "assetimport"))
	backend = writing ? "Assetimport Writer" : "Assetimport Reader";
    if (!backend)
	return 1;
    if (bu_vls_strlen(handler) && !BU_STR_EQUAL(bu_vls_cstr(handler), backend)) {
	bu_vls_printf(messages, "Backend selector %s conflicts with handler %s\n", bu_vls_cstr(format), bu_vls_cstr(handler));
	return 0;
    }
    bu_vls_strcpy(handler, backend);
    bu_vls_trunc(format, 0);
    return 1;
}

static int
execute_conversion_filter(struct gcv_context *context, const struct gcv_filter *filter,
    const char *type, const struct bu_mime_traits *traits, const struct gcv_opts *options,
    size_t argc, const char *const *argv, const char *path, struct bu_vls *messages)
{
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    enum gcv_conversion_status status = gcv_execute_result(context, filter, type, traits,
	options, argc, argv, path, &result);
    if (status != GCV_CONVERSION_COMPLETE || bu_vls_strlen(&result.messages)) {
	struct bu_vls description = BU_VLS_INIT_ZERO;
	gcv_conversion_result_describe(&result, &description);
	if (status == GCV_CONVERSION_FAILED)
	    bu_vls_vlscat(messages, &description);
	else
	    bu_log("%s", bu_vls_cstr(&description));
	bu_vls_free(&description);
    }
    gcv_conversion_result_free(&result);
    return status != GCV_CONVERSION_FAILED;
}

static int
do_conversion(struct bu_vls *messages,
	      const char *in_path, const char *in_format, const char *in_handler,
	      const char *out_path, const char *out_format, const char *out_handler,
	      size_t in_argc, const char **in_argv, size_t out_argc, const char **out_argv,
              const struct bu_mime_traits *input_traits, const struct bu_mime_traits *output_traits,
              unsigned int reject_losses)
{
    struct gcv_context context;
    struct bu_vls in_type = BU_VLS_INIT_ZERO;
    struct bu_vls out_type = BU_VLS_INIT_ZERO;
    const struct gcv_filter *reader;
    const struct gcv_filter *writer;
    int status = 0;
    struct gcv_opts options;
    gcv_opts_default(&options);
    options.reject_losses = reject_losses;
    gcv_context_init(&context);
    reader = gcv_resolve_variant(&context, GCV_FILTER_READ, in_path, in_format, input_traits, in_handler, &in_type, NULL, messages);
    writer = gcv_resolve_variant(&context, GCV_FILTER_WRITE, out_path, out_format, output_traits, out_handler, &out_type, NULL, messages);
    if (!reader || !writer)
	goto done;
    bu_log("Input: %s (%s)\nOutput: %s (%s)\n", bu_vls_cstr(&in_type), reader->name,
	   bu_vls_cstr(&out_type), writer->name);
    if (!execute_conversion_filter(&context, reader, bu_vls_cstr(&in_type), input_traits,
	    &options, in_argc, in_argv, in_path, messages)) {
	bu_vls_printf(messages, "Read filter '%s' failed for '%s'\n", reader->name, in_path);
	goto done;
    }
    if (!execute_conversion_filter(&context, writer, bu_vls_cstr(&out_type), output_traits,
	    &options, out_argc, out_argv, out_path, messages)) {
	bu_vls_printf(messages, "Write filter '%s' failed for '%s'\n", writer->name, out_path);
	goto done;
    }
    status = 1;
done:
    bu_vls_free(&in_type);
    bu_vls_free(&out_type);
    gcv_context_destroy(&context);
    return status;
}


void
usage(const struct bu_opt_desc *opts) {
    char *help = bu_opt_describe(opts, NULL);
    if (help) {
	bu_log("%s\n", help);
	bu_free(help, "help str");
    }
}


static int
inspect_formats(int list, const char *filename, struct bu_vls *messages)
{
    struct gcv_context context;
    struct bu_mime_result *result = NULL;
    struct gcv_handler_info *handlers = NULL;
    size_t count = 0;
    int status = 0;
    gcv_context_init(&context);
    if (!list) {
	enum bu_mime_status detected = gcv_detect_file(&context, filename, &result, messages);
	bu_mime_result_describe(result, messages);
	status = detected == BU_MIME_MATCH ? 0 : 1;
    }
    if (!gcv_list_handlers(&context, NULL, &handlers, &count)) {
	status = 1;
    } else {
	for (size_t i = 0; i < count; ++i) {
	    int show = list;
	    for (size_t j = 0; !show && result && j < result->count; ++j)
		show = BU_STR_EQUAL(handlers[i].mime_type, result->candidates[j].name);
	    if (show) {
		bu_vls_printf(messages, "%s  %s  %s (priority %u)", handlers[i].mime_type,
		    handlers[i].operation == GCV_FILTER_READ ? "read" : "write",
		    handlers[i].name, handlers[i].priority);
		bu_mime_traits_describe(&handlers[i].traits, messages);
		bu_vls_putc(messages, '\n');
	    }
	}
    }
    gcv_handler_list_free(handlers, count);
    bu_mime_result_free(result);
    gcv_context_destroy(&context);
    return status;
}

int
main(int ac, const char **av)
{
    size_t i;
    int ret = 0;

    static struct fmt_opts in_only_opts;
    static struct fmt_opts out_only_opts;
    static struct fmt_opts both_opts;
    static struct help_state hs;

    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_mime_traits input_traits = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_mime_traits output_traits = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_vls in_handler = BU_VLS_INIT_ZERO;
    struct bu_vls out_handler = BU_VLS_INIT_ZERO;

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
    int list_formats = 0;
    unsigned int reject_losses = 0;
    char *identify = NULL;

#define STR_HELP "Print help and exit.  If a format is specified to --help, print help specific to that format"
#define STR_INOPT "Options to apply only while processing input file.  Accepts options until another toplevel option is encountered."
#define STR_OUTOPT "Options to apply only while preparing output file.  Accepts options until another toplevel option is encountered."
#define STR_BOTH "Options to apply both during input and output handling.  Accepts options until another toplevel option is encountered."

    struct bu_opt_desc options[] = {
	{"", "list-formats", "", NULL, &list_formats, "List registered formats and handlers."},
	{"", "identify", "file", &bu_opt_str, &identify, "Inspect file type and available handlers."},
	{"", "reject-loss", "category", &gcv_opt_loss_policy, &reject_losses,
	    "Reject reported geometry, appearance, behavior, metadata or all losses (repeatable)."},
	{"h", "help",          "[format]", &help, &hs, STR_HELP},
	{"?", "",              "[format]", &help, &hs, ""},
	{"i", "input",         "file", &file_stat, &in_str, "Input file."},
	{"o", "output",        "file", &file_null, &out_str, "Output file."},
	{"", "input-format",   "format", &bu_opt_vls, &in_format, "Input MIME name or extension."},
	{"", "output-format",  "format", &bu_opt_vls, &out_format, "Output MIME name or extension."},
	{"", "input-trait", "key=value", &bu_opt_mime_trait, &input_traits, "Required input format trait (repeatable)."},
	{"", "output-trait", "key=value", &bu_opt_mime_trait, &output_traits, "Requested output format trait (repeatable)."},
	{"", "input-handler",  "name", &bu_opt_vls, &in_handler, "Input handler implementation."},
	{"", "output-handler", "name", &bu_opt_vls, &out_handler, "Output handler implementation."},
	{"I", "input-only-opts", "opts", &fmt_fun, &in_only_opts, STR_INOPT},
	{"O", "output-only-opts", "opts", &fmt_fun, &out_only_opts, STR_OUTOPT},
	{"B", "input-and-output-opts", "opts", &fmt_fun, &both_opts, STR_BOTH},
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
	ret = 1;
	goto cleanup;
    }

    if (list_formats || identify) {
	ret = inspect_formats(list_formats, identify, &slog);
	bu_log("%s", bu_vls_cstr(&slog));
	bu_vls_trunc(&slog, 0);
	goto cleanup;
    }

    /* First, see if help was supplied */
    if (hs.showhelp) {
	if (hs.format) {
	    /* TODO - generate format-specific help */
	}
	usage(options);

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
    if (in_str && !out_str && ac > 0) {
	bu_vls_sprintf(&out_path_raw, "%s", av[ac - 1]);
	av[ac - 1] = NULL;
	ac--;
	unknown_ac--;
    }
    if (!in_str && out_str && ac > 0) {
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

    if (!bu_vls_strlen(&in_format))
	extract_format_prefix(&in_format, bu_vls_cstr(&in_path_raw));
    if (!bu_vls_strlen(&out_format))
	extract_format_prefix(&out_format, bu_vls_cstr(&out_path_raw));
    if (ret)
	goto cleanup;
    if (!legacy_backend_format(&in_format, &in_handler, 0, &slog) ||
	!legacy_backend_format(&out_format, &out_handler, 1, &slog)) {
	ret = 1;
	goto cleanup;
    }

    {
	const size_t in_argc = BU_PTBL_LEN(&in_opts), out_argc = BU_PTBL_LEN(&out_opts);
	const char ** const in_argv = (const char **)in_opts.buffer;
	const char ** const out_argv = (const char **)out_opts.buffer;

	if (!do_conversion(&slog, bu_vls_cstr(&in_path),
			   bu_vls_strlen(&in_format) ? bu_vls_cstr(&in_format) : NULL,
			   bu_vls_strlen(&in_handler) ? bu_vls_cstr(&in_handler) : NULL,
			   bu_vls_cstr(&out_path),
			   bu_vls_strlen(&out_format) ? bu_vls_cstr(&out_format) : NULL,
			   bu_vls_strlen(&out_handler) ? bu_vls_cstr(&out_handler) : NULL,
			   in_argc, in_argv, out_argc, out_argv, &input_traits, &output_traits, reject_losses)) {
	    bu_vls_printf(&slog, "Conversion failed\n");
	    ret = 1;
	    goto cleanup;
	}
    }

cleanup:

    if (bu_vls_strlen(&slog) > 0 && ret != 0)
	bu_log("%s", bu_vls_addr(&slog));
    bu_mime_traits_free(&input_traits);
    bu_mime_traits_free(&output_traits);
    bu_vls_free(&in_handler);
    bu_vls_free(&out_handler);
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&in_path_raw);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&out_path_raw);
    bu_vls_free(&slog);
    bu_vls_free(&parse_msgs);

    if (hs.format)
	bu_free(hs.format, "help format");
    if (in_str)
	bu_free(in_str, "input file option");
    if (out_str)
	bu_free(out_str, "output file option");

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
