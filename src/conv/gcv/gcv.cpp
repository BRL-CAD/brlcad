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
#include "optionparser.h"


struct GcvArgs: public option::Arg
{
    /* By default, unknown args are bad */
    static option::ArgStatus Unknown(const option::Option& UNUSED(option), bool UNUSED(msg))
    {
	return option::ARG_ILLEGAL;
    }
    /* We may also want to require an arg */
    static option::ArgStatus Required(const option::Option& option, bool UNUSED(msg))
    {
	if (!option.arg) return option::ARG_ILLEGAL;
	return option::ARG_OK;
    }
};


enum FAST4OptionIndex { FAST4_UNKNOWN, FAST4_WARN_DEFAULT_NAMES };

const option::Descriptor Fast4Usage[] = {
     { FAST4_UNKNOWN, 0, "", "",          option::Arg::Optional, "FASTGEN 4 format\n"},
     { FAST4_WARN_DEFAULT_NAMES, 0, "w", "warn-default-names", option::Arg::None, "-w\t --warn-default-names\t File format of input file." },
     { 0, 0, 0, 0, 0, 0 }
};


enum STLOptionIndex { STL_UNKNOWN, STL_WARN_DEFAULT_NAMES };

const option::Descriptor STLUsage[] = {
     { STL_UNKNOWN, 0, "", "",          option::Arg::Optional, "STL format\n"},
     { STL_WARN_DEFAULT_NAMES, 0, "u", "units", GcvArgs::Required, "-w\t --warn-default-names\t File format of input file." },
     { 0, 0, 0, 0, 0, 0 }
};


struct TopLevelArg: public option::Arg
{
    /* At the top level, if we don't recognize the option, assume
     * a format option parser at a lower level will and ignore it */
    static option::ArgStatus Unknown(const option::Option& option, bool UNUSED(msg))
    {
	if (!option.arg) return option::ARG_NONE;
	if (option.arg[0] == '-') return option::ARG_IGNORE;
	return option::ARG_OK;
    }
    /* Format specifiers, on the other hand, must be validated - that
     * means that the options used at the top level for format specification
     * will not be usable at any lower level */
    static option::ArgStatus Format(const option::Option& option, bool msg)
    {
	int type_int = 0;
	mime_model_t type = MIME_MODEL_UNKNOWN;
	type_int = bu_file_mime(option.arg, MIME_MODEL);
	type = (mime_model_t)type_int;
	if (type == MIME_MODEL_UNKNOWN) {
	    if (msg) bu_log("Unknown format %s supplied to %s\n",  option.arg, option.name);
	    return option::ARG_ILLEGAL;
	} else {
	    return option::ARG_OK;
	}
    }
};


enum TopOptionIndex { UNKNOWN, HELP, IN_FORMAT, OUT_FORMAT };

const option::Descriptor TopUsage[] = {
     { UNKNOWN, 0, "", "",          TopLevelArg::Unknown, "USAGE: gcv [options] [fmt:]input [fmt:]output\n"},
     { HELP,    0, "h", "help",     option::Arg::Optional,  "-h [category]\t --help [category]\t Print help and exit." },
     { IN_FORMAT , 0, "", "in-format", TopLevelArg::Format, "\t --in-format\t File format of input file." },
     { OUT_FORMAT , 0, "", "out-format", TopLevelArg::Format, "\t --out-format\t File format of input file." },
     { 0, 0, 0, 0, 0, 0 }
};


HIDDEN void
reassemble_argstr(struct bu_vls *ustr, option::Option *unknowns)
{
    for (option::Option* opt = unknowns; opt; opt = opt->next()) {
	//bu_log("Unknown option: %s, %s\n", opt->name, opt->arg);
	if (ustr) {
	    (strlen(opt->name) == 1) ? bu_vls_printf(ustr, "-%s ", opt->name) : bu_vls_printf(ustr, "%s ", opt->name);
	    if (opt->arg) bu_vls_printf(ustr, "%s ", opt->arg);
	}
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

void fast4_arg_process(const char *args) {
    if (!args) return;
    char *input = bu_strdup(args);
    char **argv = (char **)bu_calloc(strlen(args) + 1, sizeof(char *), "argv array");
    int argc = bu_argv_from_string(argv, strlen(args), input);

    option::Stats stats(Fast4Usage, argc, argv);
    option::Option *options = (option::Option *)bu_calloc(stats.options_max, sizeof(option::Option), "options");
    option::Option *buffer= (option::Option *)bu_calloc(stats.buffer_max, sizeof(option::Option), "options");
    option::Parser parse(Fast4Usage, argc, argv, options, buffer);
}


void stl_arg_process(const char *args) {
    if (!args) return;
    char *input = bu_strdup(args);
    char **argv = (char **)bu_calloc(strlen(args) + 1, sizeof(char *), "argv array");
    int argc = bu_argv_from_string(argv, strlen(args), input);

    option::Stats stats(STLUsage, argc, argv);
    option::Option *options = (option::Option *)bu_calloc(stats.options_max, sizeof(option::Option), "options");
    option::Option *buffer= (option::Option *)bu_calloc(stats.buffer_max, sizeof(option::Option), "options");
    option::Parser parse(STLUsage, argc, argv, options, buffer);
}



int
main(int ac, char **av)
{
    int fmt = 0;
    int ret = 0;
    int ac_offset = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    mime_model_t in_type = MIME_MODEL_UNKNOWN;
    mime_model_t out_type = MIME_MODEL_UNKNOWN;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    struct bu_vls extra_opts = BU_VLS_INIT_ZERO;

    ac-=(ac>0); av+=(ac>0); // skip program name argv[0] if present
    ac_offset = (ac > 2) ? 2 : 0;  // The last two argv entries must always be the input and output paths
    /* Handle anything else as options */
    option::Stats stats(TopUsage, ac - ac_offset, av);
    option::Option *options = (option::Option *)bu_calloc(stats.options_max, sizeof(option::Option), "options");
    option::Option *buffer= (option::Option *)bu_calloc(stats.buffer_max, sizeof(option::Option), "options");
    option::Parser parse(TopUsage, ac - ac_offset, av, options, buffer);

    if (options[HELP] || ac == 0) {
	option::printUsage(std::cout, TopUsage);
	goto cleanup;
    }

    /* Any args that weren't top level args will get passed to the
     * format specific arg processing routines, after we use the known
     * top level options and the path inputs to determine what the file
     * types in question are.  Steps:
     *
     * 1.  Reassemble the unknown args into a string. */
    reassemble_argstr(&extra_opts, options[UNKNOWN]);
    bu_log("Unknown options: %s\n", bu_vls_addr(&extra_opts));
    /*
     * 2.  Use bu_argv_from_string to create a new
     * array to be fed to the format specific option parsers.*/

    /* TODO - determine whether we want to have a specific option prefix,
     * such as in- and out-, to identify an option as specific to the
     * input file format suboption parser only.  i.e.:
     *
     * --in-tol=1.0  -> --tol=1.0 to the input file's suboptions only
     * --out-tol=2.0 -> --tol=2.0 to the input file's suboptions only
     * --tol=1.5     -> --tol=1.5 to both input and output suboptions
     *
     *  consistent with top level --in-format and --out-format options
     *
     */

    /* See if we have input and output files specified */
    if (!extract_path(&in_path, av[ac-2])) {
	bu_vls_printf(&log, "Error: no input path identified: %s\n", av[ac-2]);
	ret = 1;
    }
    if (!extract_path(&out_path, av[ac-1])) {
	bu_vls_printf(&log, "Error: no output path identified: %s\n", av[ac-1]);
	ret = 1;
    }

    /* Make sure we have distinct input and output paths */
    if (bu_vls_strlen(&in_path) > 0 && BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_vls_printf(&log, "Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    /* Find out what input file type we are dealing with */
    if (options[IN_FORMAT]) {
	in_fmt = options[IN_FORMAT].arg;
    } else {
	/* If we aren't overridden by an option, it's worth doing file
	 * introspection to see if the file contents identify the input
	 * type solidly, provided we have that capability.*/

	/* fake type introspection for testing: */
	//bu_vls_sprintf(&in_format, "step");
    }
    fmt = parse_model_string(&in_format, &log, in_fmt, av[ac-2]);
    in_type = (fmt < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)fmt;
    in_fmt = NULL;

    /* Identify output file type */
    if (options[OUT_FORMAT]) out_fmt = options[OUT_FORMAT].arg;
    fmt = parse_model_string(&out_format, &log, out_fmt, av[ac-1]);
    out_type = (fmt < 0) ? MIME_MODEL_UNKNOWN : (mime_model_t)fmt;
    out_fmt = NULL;

    /* If we get to this point without knowing both input and output types, we've got a problem */
    if (in_type == MIME_MODEL_UNKNOWN) {
	bu_vls_printf(&log, "Error: no format type identified for input path: %s\n", bu_vls_addr(&in_path));
	ret = 1;
    }
    if (out_type == MIME_MODEL_UNKNOWN) {
	bu_vls_printf(&log, "Error: no format type identified for output path: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    /* If everything isn't OK, we're done - report and clean up memory */
    if (ret == 1) goto cleanup;

    switch (in_type) {
	case MIME_MODEL_VND_FASTGEN:
	    fast4_arg_process(bu_vls_addr(&extra_opts));
	    break;
	case MIME_MODEL_STL:
	    stl_arg_process(bu_vls_addr(&extra_opts));
	default:
	    break;
    }

    /* If we've gotten this far, we know enough to try to convert. Until we
     * hook in conversion calls to libgcv, print a summary of the option
     * parsing results for debugging. */
    in_fmt = bu_file_mime_str((int)in_type, MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, MIME_MODEL);
    bu_log("Input file format: %s\n", in_fmt);
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));


    /* Clean up */
cleanup:
    if (bu_vls_strlen(&log) > 0) bu_log("%s", bu_vls_addr(&log));
    if (in_fmt) bu_free((char *)in_fmt, "input format string");
    if (out_fmt) bu_free((char *)out_fmt, "output format string");
    bu_free(options, "free options");
    bu_free(buffer, "free buffer");
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
    bu_vls_free(&log);
    bu_vls_free(&extra_opts);

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
