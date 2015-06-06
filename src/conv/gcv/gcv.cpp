/*                           G C V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
#include <iostream>

#include "bu.h"


/* Emulate a FASTGEN4 format option processor */
void fast4_arg_process(const char *args) {
    int i = 0;
    char *input = NULL;
    int argc = 0;
    char **argv = NULL;
    int ret_argc = 0;
    const char **non_opts;
    static int tol = 0.0;
    static int w_flag;
    struct bu_opt_desc fg4_opt_desc[3] = {
	{1, 1, "t",  "tol", &bu_opt_int, "tol", "Dimensional tolerance.",      (void *)&tol},
	{0, 0, "w",  "warn-default-names", NULL, "", "File format of input file.", (void *)&w_flag},
	BU_OPT_DESC_NULL
    };

    input = bu_strdup(args);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    non_opts = (const char **)bu_calloc(strlen(args) + 1, sizeof(const char *), "non_opts array");
    ret_argc = bu_opt_parse(&non_opts, strlen(args), NULL, argc, (const char **)argv, fg4_opt_desc);

    if (w_flag)	bu_log("FASTGEN 4 warn default names set\n");
    bu_log("FASTGEN 4 tol: %d\n", tol);

    if (ret_argc) {
	bu_log("Unknown args: ");
	for (i = 0; i < ret_argc - 1; i++) {
	    bu_log("%s, ", non_opts[i]);
	}
	bu_log("%s\n", non_opts[ret_argc - 1]);
    }

    bu_free(input, "free array");
    bu_free(argv, "free argv");
}

void stl_arg_process(const char *args) {
    int i= 0;
    char *input = NULL;
    int argc = 0;
    char **argv = NULL;
    int ret_argc = 0;
    const char **non_opts;
    static int tol = 0.0;
    static int units = 0;
    struct bu_opt_desc stl_opt_desc[3] = {
	{1, 1, "t",  "tol",   &bu_opt_int, "tol",  "Dimensional tolerance.", (void *)&tol },
	{1, 1, "u",  "units", &bu_opt_int, "unit", "Units of input file.", (void *)&units },
	BU_OPT_DESC_NULL
    };

    input = bu_strdup(args);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    non_opts = (const char **)bu_calloc(strlen(args) + 1, sizeof(const char *), "non_opts array");
    ret_argc = bu_opt_parse(&non_opts, strlen(args), NULL, argc, (const char **)argv, stl_opt_desc);

    bu_log("STL tol: %d\n", tol);
    bu_log("STL units: %d\n", units);

    if (ret_argc) {
	bu_log("Unknown args: ");
	for (i = 0; i < ret_argc - 1; i++) {
	    bu_log("%s, ", non_opts[i]);
	}
	bu_log("%s\n", non_opts[ret_argc - 1]);
    }

    bu_free(input, "free array");
    bu_free(argv, "free argv");
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
parse_model_string(struct bu_vls *format, struct bu_vls *log, const char *opt, const char *input)
{
    int type_int = 0;
    mime_model_t type = MIME_MODEL_UNKNOWN;

    struct bu_vls format_cpy = BU_VLS_INIT_ZERO;
    struct bu_vls path = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!input) || UNLIKELY(strlen(input) == 0)) return MIME_MODEL_UNKNOWN;

    /* If an external routine has specified a format string, that string will
     * override the file extension (but not an explicit option or format prefix.
     * Stash any local format string here for later processing.  The idea is
     * to allow some other routine (say, an introspection of a file looking for
     * some signature string) to override a file extension based type identification.
     * Such introspection is beyond the scope of this function, but should override
     * the file extension mechanism. */
    if (format) bu_vls_sprintf(&format_cpy, "%s", bu_vls_addr(format));

    /* If we have an explicit option, that overrides any other format specifiers */
    if (opt) {
	type_int = bu_file_mime(opt, MIME_MODEL);
	type = (type_int < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)type_int;
	if (type == MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (log) bu_vls_printf(log, "Error: unknown model format \"%s\" specified as an option.\n", opt);
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* Try for a format prefix */
    if (extract_format_prefix(format, input)) {
	/* If we don't already have a valid type and we had a format prefix,
	 * find out if it maps to a valid type */
	if (type == MIME_MODEL_UNKNOWN && format) {
	    /* Yes - see if the prefix specifies a model format */
	    type_int = bu_file_mime(bu_vls_addr(format), MIME_MODEL);
	    type = (type_int < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)type_int;
	    if (type == MIME_MODEL_UNKNOWN) {
		/* Have prefix, but doesn't result in a known format - that's an error */
		if (log) bu_vls_printf(log, "Error: unknown model format \"%s\" specified as a format prefix.\n", bu_vls_addr(format));
		bu_vls_free(&format_cpy);
		return -1;
	    }
	}
    }

    /* If we don't already have a type and we were passed a format string, give it a try */
    if (type == MIME_MODEL_UNKNOWN && format && bu_vls_strlen(&format_cpy) > 0) {
	bu_vls_sprintf(format, "%s", bu_vls_addr(&format_cpy));
	type_int = bu_file_mime(bu_vls_addr(&format_cpy), MIME_MODEL);
	type = (type_int < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)type_int;
	if (type == MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (log) bu_vls_printf(log, "Error: unknown model format \"%s\" passed to parse_model_string.\n", bu_vls_addr(format));
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* If we have no prefix or the prefix didn't map to a model type, try file extension */
    if (type == MIME_MODEL_UNKNOWN && extract_path(&path, input)) {
	if (bu_path_component(format, bu_vls_addr(&path), PATH_EXTENSION)) {
	    type_int = bu_file_mime(bu_vls_addr(format), MIME_MODEL);
	    type = (type_int < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)type_int;
	    if (type == MIME_MODEL_UNKNOWN) {
		/* Have file extension, but doesn't result in a known format - that's an error */
		if (log) bu_vls_printf(log, "Error: file extension \"%s\" does not map to a known model format.\n", bu_vls_addr(format));
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

    if (!argv || strlen(argv[0]) || argc == 0) {
	return 0;
    }
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

    if (!argv || strlen(argv[0]) || argc == 0) {
	return 0;
    }
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
    mime_model_t type = MIME_MODEL_UNKNOWN;
    mime_model_t *set_type = (mime_model_t *)set_mime;
    if (!argv || argc == 0) {
	return 0;
    }
    type_int = bu_file_mime(argv[0], MIME_MODEL);
    type = (type_int < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)type_int;
    if (type == MIME_MODEL_UNKNOWN) {
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

#define gcv_help_str "Print help and exit.  If a format is specified to --help, print help specific to that format"


int
main(int ac, char **av)
{
    size_t i;
    int fmt = 0;
    int ret = 0;
    int skip_in = 0;
    int skip_out = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    static mime_model_t in_type = MIME_MODEL_UNKNOWN;
    static mime_model_t out_type = MIME_MODEL_UNKNOWN;
    static char *in_path_str = NULL;
    static char *out_path_str = NULL;
    static char *in_only_opts = NULL;
    static char *out_only_opts = NULL;
    static struct gcv_help_state hs;

    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    struct bu_vls input_opts = BU_VLS_INIT_ZERO;
    struct bu_vls output_opts = BU_VLS_INIT_ZERO;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int uac = 0;
    const char **uav = (const char **)bu_calloc(ac, sizeof(char *), "unknown results");

    struct bu_opt_desc gcv_opt_desc[9] = {
	{0, 1, "h", "help",             &gcv_help,   "format", gcv_help_str, (void *)&hs },
	{0, 1, "?", "",                 &gcv_help,   "format", "",           (void *)&hs },
	{1, 1, "i", "input",            &file_stat,  "file",   "Input file.", (void *)&in_path_str },
	{1, 1, "o", "output",           &file_null,  "file",   "Output file.", (void *)&out_path_str },
	{1, 1, "",  "input-format",     &model_mime, "format", "File format of input file.", (void *)&in_type },
	{1, 1, "",  "output-format",    &model_mime, "format", "File format of output file.", (void *)&out_type },
	{1, 1, "I", "input-only-opts",  &bu_opt_str,          "\"[opts]\"", "Options to apply only while processing input file.  Quotes around the opts are always necessary, but brackets are only necessary when supplying a single option without arguments that would otherwise be interpreted as an argv entry by the shell, even with quotes.  Brackets will never hurt, and for robustness when scripting they should always be used.", (void *)&in_only_opts },
	{1, 1, "O", "output-only-opts", &bu_opt_str,          "\"[opts]\"", "Options to apply only while preparing output file.  Quotes around the opts are always necessary, but brackets are only necessary when supplying a single option without arguments that would otherwise be interpreted as an argv entry by the shell, even with quotes.  Brackets will never hurt, and for robustness when scripting they should always be used.", (void *)&out_only_opts },
	BU_OPT_DESC_NULL
    };

    hs.flag = 0;
    hs.format = NULL;

    ac-=(ac>0); av+=(ac>0); // skip program name argv[0] if present

    if (ac == 0) {
	const char *help = bu_opt_describe(gcv_opt_desc, NULL);
	bu_log("%s\n", help);
	if (help) bu_free((char *)help, "help str");
	// TODO - print some help
	goto cleanup;
    }

    uac = bu_opt_parse(&uav, ac, &parse_msgs, ac, (const char **)av, gcv_opt_desc);

    /* First, see if help was supplied */
    if (hs.flag) {
	if (hs.format) {
	    // TODO - generate some help based on format
	} else {
	    { /* Test static help print  */
		bu_log("Options:\n");
		const char *help = bu_opt_describe(gcv_opt_desc, NULL);
		bu_log("%s\n", help);
		if (help) bu_free((char *)help, "help str");
	    }

#if 0

	    // TODO - figure out how to get this info from each plugin to construct this table
	    // on the fly...
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
    if (uac > 0 && !(skip_in && skip_out)) {
	if (skip_in && !skip_out) {
	    bu_vls_sprintf(&out_path_raw, "%s", uav[uac - 1]);
	    uac--;
	}
	if (!skip_in && skip_out) {
	    bu_vls_sprintf(&in_path_raw, "%s", uav[uac - 1]);
	    uac--;
	}
	if (!skip_in && !skip_out) {
	    if (uac > 1) {
		bu_vls_sprintf(&in_path_raw, "%s", uav[uac - 2]);
		bu_vls_sprintf(&out_path_raw, "%s", uav[uac - 1]);
		uac = uac -2;
	    } else {
		bu_vls_sprintf(&in_path_raw, "%s", uav[uac - 1]);
		uac--;
	    }
	}
    }

    /* Any unknown strings not otherwise processed are passed to both input and output.
     * These are deliberately placed at the beginning of the input strings, so any
     * input/output specific options have a chance to override them. */
    if (uac) {
	for (i = 0; i < (size_t)uac; i++) {
	    if (i != 0) {
		bu_vls_printf(&input_opts, " ");
		bu_vls_printf(&output_opts, " ");
	    }
	    bu_vls_printf(&input_opts, "%s", uav[i]);
	    bu_vls_printf(&output_opts, "%s", uav[i]);
	}
	if (bu_vls_strlen(&input_opts) > 0) bu_log("Unknown options (input): %s\n", bu_vls_addr(&input_opts));
	if (bu_vls_strlen(&output_opts) > 0) bu_log("Unknown options (output): %s\n", bu_vls_addr(&output_opts));
    }

    /* If we have input and/or output specific options, append them now */
    if (in_only_opts) {
	struct bu_vls o_tmp = BU_VLS_INIT_ZERO;
	if (bu_vls_strlen(&input_opts) > 0) 
	    bu_vls_printf(&input_opts, " ");

	bu_vls_sprintf(&o_tmp, "%s", in_only_opts);
	if (bu_vls_addr(&o_tmp)[0] == '[') bu_vls_nibble(&o_tmp, 1);
	if (bu_vls_addr(&o_tmp)[strlen(bu_vls_addr(&o_tmp)) - 1] == ']') bu_vls_trunc(&o_tmp, -1);
	bu_vls_printf(&input_opts, "%s", bu_vls_addr(&o_tmp));
	if (bu_vls_strlen(&input_opts) > 0) bu_log("Input only opts: %s\n", bu_vls_addr(&o_tmp));
	bu_vls_free(&o_tmp);
    }
    if (out_only_opts) {
	struct bu_vls o_tmp = BU_VLS_INIT_ZERO;
	if (bu_vls_strlen(&output_opts) > 0) 
	    bu_vls_printf(&output_opts, " ");
	
	bu_vls_sprintf(&o_tmp, "%s", out_only_opts);
	if (bu_vls_addr(&o_tmp)[0] == '[') bu_vls_nibble(&o_tmp, 1);
	if (bu_vls_addr(&o_tmp)[strlen(bu_vls_addr(&o_tmp)) - 1] == ']') bu_vls_trunc(&o_tmp, -1);
	bu_vls_printf(&output_opts, "%s", bu_vls_addr(&o_tmp));
	if (bu_vls_strlen(&output_opts) > 0) bu_log("Output only opts: %s\n", bu_vls_addr(&o_tmp));
	bu_vls_free(&o_tmp);
    }


    /* See if we have input and output files specified */
    if (!extract_path(&in_path, bu_vls_addr(&in_path_raw))) {
	if (bu_vls_strlen(&in_path_raw) > 0) {
	    bu_vls_printf(&log, "Error: no input path identified: %s\n", bu_vls_addr(&in_path_raw));
	} else {
	    bu_vls_printf(&log, "Error: no input path.\n");
	}
	ret = 1;
    }
    if (!extract_path(&out_path, bu_vls_addr(&out_path_raw))) {
	if (bu_vls_strlen(&out_path_raw) > 0) {
	    bu_vls_printf(&log, "Error: no output path identified: %s\n", bu_vls_addr(&out_path_raw));
	} else {
	    bu_vls_printf(&log, "Error: no output path.\n");
	}
	ret = 1;
    }

    /* Make sure we have distinct input and output paths */
    if (bu_vls_strlen(&in_path) > 0 && BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_vls_printf(&log, "Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    /* Find out what input file type we are dealing with */
    if (in_type == MIME_MODEL_UNKNOWN) {
	fmt = parse_model_string(&in_format, &log, in_fmt, bu_vls_addr(&in_path_raw));
	in_type = (fmt < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)fmt;
	in_fmt = NULL;
    }
    /* Identify output file type */
    if (out_type == MIME_MODEL_UNKNOWN) {
	fmt = parse_model_string(&out_format, &log, out_fmt, bu_vls_addr(&out_path_raw));
	out_type = (fmt < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)fmt;
	out_fmt = NULL;
    }

    /* If we get to this point without knowing both input and output types, we've got a problem */
    if (in_type == MIME_MODEL_UNKNOWN) {
	if (bu_vls_strlen(&in_path) > 0) {
	    bu_vls_printf(&log, "Error: no format type identified for input path: %s\n", bu_vls_addr(&in_path));
	} else {
	    bu_vls_printf(&log, "Error: no input format type identified.\n");
	}
	ret = 1;
    }
    if (out_type == MIME_MODEL_UNKNOWN) {
	if (bu_vls_strlen(&out_path) > 0) {
	    bu_vls_printf(&log, "Error: no format type identified for output path: %s\n", bu_vls_addr(&out_path));
	} else {
	    bu_vls_printf(&log, "Error: no output format type identified.\n");
	}
	ret = 1;
    }

    /* If everything isn't OK, we're done - report and clean up memory */
    if (ret == 1) goto cleanup;

    /* If we've gotten this far, we know enough to try to convert. Until we
     * hook in conversion calls to libgcv, print a summary of the option
     * parsing results for debugging. */
    in_fmt = bu_file_mime_str((int)in_type, MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, MIME_MODEL);
    bu_log("Input file format: %s\n", in_fmt);
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));

    switch (in_type) {
	case MIME_MODEL_VND_FASTGEN:
	    fast4_arg_process(bu_vls_addr(&input_opts));
	    break;
	case MIME_MODEL_STL:
	    stl_arg_process(bu_vls_addr(&input_opts));
	default:
	    break;
    }

    switch (out_type) {
	case MIME_MODEL_VND_FASTGEN:
	    fast4_arg_process(bu_vls_addr(&output_opts));
	    break;
	case MIME_MODEL_STL:
	    stl_arg_process(bu_vls_addr(&output_opts));
	default:
	    break;
    }



    /* Clean up */
cleanup:
    if (bu_vls_strlen(&log) > 0) bu_log("%s", bu_vls_addr(&log));
    if (in_fmt) bu_free((char *)in_fmt, "input format string");
    if (out_fmt) bu_free((char *)out_fmt, "output format string");
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&log);
    bu_vls_free(&input_opts);
    bu_vls_free(&output_opts);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
