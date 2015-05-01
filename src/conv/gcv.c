/*                           G C V . C
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
/** @file gcv.c
 *
 * Geometry converter.
 *
 */

#include "common.h"
#include <stdlib.h>
#include <string.h>

#include "bu.h"

HIDDEN int
format_prefix(struct bu_vls *format, struct bu_vls *path, const char *input)
{
    struct bu_vls wformat = BU_VLS_INIT_ZERO;
    struct bu_vls wpath = BU_VLS_INIT_ZERO;
    char *colon_pos = NULL;
    if (UNLIKELY(!input)) return 0;
    colon_pos = strchr(input, ':');
    if (colon_pos) {
	int ret = 0;
	bu_vls_sprintf(&wformat, "%s", input);
	bu_vls_sprintf(&wpath, "%s", input);
	bu_vls_trunc(&wformat, -1 * strlen(colon_pos));
	bu_vls_nibble(&wpath, strlen(input) - strlen(colon_pos) + 1);
	if (bu_vls_strlen(&wformat) > 0) {
	    ret = 1;
	    if (format) bu_vls_sprintf(format, "%s", bu_vls_addr(&wformat));
	}
	if (path && bu_vls_strlen(&wpath) > 0) bu_vls_sprintf(path, "%s", bu_vls_addr(&wpath));
	bu_vls_free(&wformat);
	bu_vls_free(&wpath);
	return ret;
    } else {
	if (path) bu_vls_sprintf(path, "%s", input);
	return 0;
    }
    /* Shouldn't get here */
    return 0;
}

int
parse_model_string(struct bu_vls *format, struct bu_vls *path, const char *input)
{
    int type_int = 0;
    mime_model_t type = MIME_MODEL_UNKNOWN;

    if (UNLIKELY(!input) || UNLIKELY(strlen(input) == 0)) return MIME_MODEL_UNKNOWN;

    /* See if we have a protocol prefix */
    if (format_prefix(format, path, input)) {
	/* Yes - see if the prefix specifies a model format */
	type_int = bu_file_mime(bu_vls_addr(format), MIME_MODEL);
	type = (mime_model_t)type_int;
	if (type == MIME_MODEL_UNKNOWN) {
	    /* Have prefix, but doesn't result in a known format - that's an error */
	    return -1;
	}
    }
    /* If we have no prefix or the prefix didn't map to a model type, try extension */
    if (type == MIME_MODEL_UNKNOWN) {
	if (bu_path_component(format, bu_vls_addr(path), PATH_EXTENSION)) {
	    type_int = bu_file_mime(bu_vls_addr(format), MIME_MODEL);
	    type = (mime_model_t)type_int;
	}
    }

    return (int)type;
}

int
main(int ac, char **av)
{
    int fmt = 0;
    int fmt_error = 0;
    const char *in_fmt, *out_fmt;
    mime_model_t in_type, out_type;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;

    if (ac < 3 || (ac == 2 && av[1][0] == '-' && av[1][1] == 'h')) {
	bu_log("Usage: %s [-h] [options] [fmt:]input [fmt:]output \n", av[0]);
	return 1;
    }

    fmt = parse_model_string(&in_format, &in_path, av[1]);
    if (fmt < 0) {
	bu_log("Error: unknown model format \"%s\" specified as prefix to input path.\n", bu_vls_addr(&in_format));
	fmt_error++;
    } else {
	in_type = (mime_model_t)fmt;
    }
    fmt = parse_model_string(&out_format, &out_path, av[2]);
    if (fmt < 0) {
	bu_log("Error: unknown model format \"%s\" specified as prefix to output path.\n", bu_vls_addr(&out_format));
	fmt_error++;
    } else {
    out_type = (mime_model_t)fmt;
    }
    if (BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_log("Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	fmt_error++;
    }
    if (in_type == MIME_MODEL_UNKNOWN) {
	bu_log("Error: unable to identify file format for input path.\n");
	fmt_error++;
    }
    if (out_type == MIME_MODEL_UNKNOWN) {
	bu_log("Error: unable to identify file format for output path.\n");
	fmt_error++;
    }
    if (fmt_error > 0) return 1;


    in_fmt = bu_file_mime_str((int)in_type, MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, MIME_MODEL);

    bu_log("Input file format: %s\n", in_fmt);
    bu_log("Output file format: %s\n", out_fmt);
    bu_log("Input file path: %s\n", bu_vls_addr(&in_path));
    bu_log("Output file path: %s\n", bu_vls_addr(&out_path));


    /* Clean up */
    if (in_fmt) bu_free((char *)in_fmt, "input format string");
    if (out_fmt) bu_free((char *)out_fmt, "output format string");
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);

    return 0;
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

