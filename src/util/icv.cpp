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
    if (!bu_path_component(&in_path, bu_vls_addr(&in_path_raw), PATH_ALL)) {
	if (bu_vls_strlen(&in_path_raw) > 0) {
	    bu_vls_printf(&slog, "Error: no input path identified: %s\n", bu_vls_addr(&in_path_raw));
	} else {
	    bu_vls_printf(&slog, "Error: no input path.\n");
	}
	ret = 1;
    }
    if (!bu_path_component(&out_path, bu_vls_addr(&out_path_raw), PATH_ALL)) {
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
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (bu_path_component(&c, bu_vls_addr(&in_path_raw), (path_component_t)MIME_IMAGE)) {
	    in_type = (mime_image_t)bu_file_mime_int(bu_vls_addr(&c));
	}
	bu_vls_free(&c);
    }
    if (out_type == MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (bu_path_component(&c, bu_vls_addr(&out_path_raw), (path_component_t)MIME_IMAGE)) {
	    out_type = (mime_image_t)bu_file_mime_int(bu_vls_addr(&c));
	}
	bu_vls_free(&c);
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
