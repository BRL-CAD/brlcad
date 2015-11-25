/*                           I C V . C P P
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
/** @file icv.cpp
 *
 * Image converter.
 *
 */

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "icv.h"

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
parse_image_string(struct bu_vls *format, struct bu_vls *slog, const char *opt, const char *input)
{
    int type_int = 0;
    mime_image_t type = MIME_IMAGE_UNKNOWN;

    struct bu_vls format_cpy = BU_VLS_INIT_ZERO;
    struct bu_vls path = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!input) || UNLIKELY(strlen(input) == 0)) return MIME_IMAGE_UNKNOWN;

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
	type_int = bu_file_mime(opt, MIME_IMAGE);
	type = (type_int < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)type_int;
	if (type == MIME_IMAGE_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (slog) bu_vls_printf(slog, "Error: unknown image format \"%s\" specified as an option.\n", opt);
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* Try for a format prefix */
    if (extract_format_prefix(format, input)) {
	/* If we don't already have a valid type and we had a format prefix,
	 * find out if it maps to a valid type */
	if (type == MIME_IMAGE_UNKNOWN && format) {
	    /* Yes - see if the prefix specifies a image format */
	    type_int = bu_file_mime(bu_vls_addr(format), MIME_IMAGE);
	    type = (type_int < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)type_int;
	    if (type == MIME_IMAGE_UNKNOWN) {
		/* Have prefix, but doesn't result in a known format - that's an error */
		if (slog) bu_vls_printf(slog, "Error: unknown image format \"%s\" specified as a format prefix.\n", bu_vls_addr(format));
		bu_vls_free(&format_cpy);
		return -1;
	    }
	}
    }

    /* If we don't already have a type and we were passed a format string, give it a try */
    if (type == MIME_IMAGE_UNKNOWN && format && bu_vls_strlen(&format_cpy) > 0) {
	bu_vls_sprintf(format, "%s", bu_vls_addr(&format_cpy));
	type_int = bu_file_mime(bu_vls_addr(&format_cpy), MIME_IMAGE);
	type = (type_int < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)type_int;
	if (type == MIME_IMAGE_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    if (slog) bu_vls_printf(slog, "Error: unknown image format \"%s\" passed to parse_image_string.\n", bu_vls_addr(format));
	    bu_vls_free(&format_cpy);
	    return -1;
	}
    }

    /* If we have no prefix or the prefix didn't map to a image type, try file extension */
    if (type == MIME_IMAGE_UNKNOWN && extract_path(&path, input)) {
	/* TODO - this piece (and maybe most of this function's logic - is
	 * conceptually a duplicate of the icv_guess_file_format logic,
	 * although this may actually be a better implementation...  It's
	 * also quite similar to the gcv file format discovery code.  There may
	 * be a libbu-level mime functionality to be written here to
	 * generically do this - support ImageMagic style FMT:filename and
	 * file.ext resolutions for *all* mime types */
	if (bu_path_component(format, bu_vls_addr(&path), PATH_EXTENSION)) {
	    type_int = bu_file_mime(bu_vls_addr(format), MIME_IMAGE);
	    type = (type_int < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)type_int;
	    if (type == MIME_IMAGE_UNKNOWN) {
		/* Have file extension, but doesn't result in a known format - that's an error */
		if (slog) bu_vls_printf(slog, "Error: file extension \"%s\" does not map to a known image format.\n", bu_vls_addr(format));
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
image_mime(struct bu_vls *msg, int argc, const char **argv, void *set_mime)
{
    int type_int;
    mime_image_t type = MIME_IMAGE_UNKNOWN;
    mime_image_t *set_type = (mime_image_t *)set_mime;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "mime format");

    type_int = bu_file_mime(argv[0], MIME_IMAGE);
    type = (type_int < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)type_int;
    if (type == MIME_IMAGE_UNKNOWN) {
	if (msg) bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", argv[0]);
	return -1;
    }
    if (set_type) (*set_type) = type;
    return 1;
}


int
main(int ac, const char **av)
{
    int uac = 0;
    int fmt = 0;
    int ret = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    static mime_image_t in_type = MIME_IMAGE_UNKNOWN;
    static mime_image_t out_type = MIME_IMAGE_UNKNOWN;
    static char *in_path_str = NULL;
    static char *out_path_str = NULL;
    int need_help = 0;
    int skip_in = 0;
    int skip_out = 0;

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path_raw = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    struct bu_opt_desc icv_opt_desc[] = {
	{"h", "help",             "",           NULL,          &need_help,            "Print help and exit."        },
	{"?", "",                 "",           NULL,          &need_help,            "",                           },
	{"i", "input",            "file",       &file_stat,   (void *)&in_path_str,   "Input file.",                },
	{"o", "output",           "file",       &file_null,   (void *)&out_path_str,  "Output file.",               },
	{"",  "input-format",     "format",     &image_mime,  (void *)&in_type,       "File format of input file.", },
	{"",  "output-format",    "format",     &image_mime,  (void *)&out_type,      "File format of output file." },
	BU_OPT_DESC_NULL
    };

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    if (ac == 0) {
	const char *help = bu_opt_describe(icv_opt_desc, NULL);
	bu_log("%s\n", help);
	if (help) bu_free((char *)help, "help str");
	/* TODO - print some help */
	goto cleanup;
    }

    uac = bu_opt_parse(&parse_msgs, ac, av, icv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	goto cleanup;
    }

    /* First, see if help was requested or needed */
    if (need_help) {
	/* Test static help print  */
	const char *help = bu_opt_describe(icv_opt_desc, NULL);
	bu_log("Options:\n");
	bu_log("%s\n", help);
	if (help) bu_free((char *)help, "help str");
#if 0

	/* TODO - figure out how to get this info from each plugin to construct this table */
	/* on the fly... */
	bu_log("\nSupported formats:\n");
	bu_log(" ------------------------------------------------------------\n");
	bu_log(" | Extension  |          File Format      |  Input | Output |\n");
	bu_log(" |----------------------------------------------------------|\n");
	bu_log(" |    pix     |                  PIX file |   Yes  |   Yes  |\n");
	bu_log(" |------------|---------------------------|--------|--------|\n");
	bu_log(" |    png     | Portable Network Graphics |   Yes  |   Yes  |\n");
	bu_log(" |------------|---------------------------|--------|--------|\n");
	bu_log(" |    bw      |   Black and White data    |   Yes  |   Yes  |\n");
	bu_log(" |----------------------------------------------------------|\n");
#endif
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
	    bu_vls_sprintf(&out_path_raw, "%s", av[uac - 1]);
	}
	if (!skip_in && skip_out) {
	    bu_vls_sprintf(&in_path_raw, "%s", av[uac - 1]);
	}
	if (!skip_in && !skip_out) {
	    if (ac > 1) {
		bu_vls_sprintf(&in_path_raw, "%s", av[uac - 2]);
		bu_vls_sprintf(&out_path_raw, "%s", av[uac - 1]);
	    } else {
		bu_vls_sprintf(&in_path_raw, "%s", av[uac - 1]);
	    }
	}
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
    if (in_type == MIME_IMAGE_UNKNOWN) {
	fmt = parse_image_string(&in_format, &slog, in_fmt, bu_vls_addr(&in_path_raw));
	in_type = (fmt < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)fmt;
	in_fmt = NULL;
    }
    /* Identify output file type */
    if (out_type == MIME_IMAGE_UNKNOWN) {
	fmt = parse_image_string(&out_format, &slog, out_fmt, bu_vls_addr(&out_path_raw));
	out_type = (fmt < 0) ? MIME_IMAGE_UNKNOWN : (mime_image_t)fmt;
	out_fmt = NULL;
    }

    /* If we get to this point without knowing both input and output types, we've got a problem */
    if (in_type == MIME_IMAGE_UNKNOWN) {
	if (bu_vls_strlen(&in_path) > 0) {
	    bu_vls_printf(&slog, "Error: no format type identified for input path: %s\n", bu_vls_addr(&in_path));
	} else {
	    bu_vls_printf(&slog, "Error: no input format type identified.\n");
	}
	ret = 1;
    }
    if (out_type == MIME_IMAGE_UNKNOWN) {
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
     * hook in conversion calls to libicv, print a summary of the option
     * parsing results for debugging. */
    in_fmt = bu_file_mime_str((int)in_type, MIME_IMAGE);
    out_fmt = bu_file_mime_str((int)out_type, MIME_IMAGE);
    bu_log("Input file format: %s\n", in_fmt);
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));


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
