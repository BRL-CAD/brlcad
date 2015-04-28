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

mime_model_t
get_model_type(const char *path)
{
    int type_int = 0;
    mime_model_t type = MIME_MODEL_UNKNOWN;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!path) || UNLIKELY(strlen(path) == 0)) return MIME_MODEL_UNKNOWN;

    /* See if we have a protocol prefix */
    if (!bu_path_component(&tmp, path, PATH_PROTOCOL)) {
	/* No prefix - try extension */
	if (bu_path_component(&tmp , path, PATH_FILE_EXTENSION)) {
	    type_int = bu_file_mime(bu_vls_addr(&tmp), MIME_MODEL);
	    type = (mime_model_t)type_int;
	}
    } else {
	/* See if the prefix specifies a model type */
	type_int = bu_file_mime(bu_vls_addr(&tmp), MIME_MODEL);
	type = (mime_model_t)type_int;
	if (type == MIME_MODEL_UNKNOWN) {
	    /* Prefix didn't map to a model type - try extension */
	    if (bu_path_component(&tmp , path, PATH_FILE_EXTENSION)) {
		type_int = bu_file_mime(bu_vls_addr(&tmp), MIME_MODEL);
		type = (mime_model_t)type_int;
	    }
	}
    }
    bu_vls_free(&tmp);
    return type;
}

int
main(int ac, char **av)
{
    const char *in_fmt, *out_fmt;
    mime_model_t in_type;
    mime_model_t out_type;
    if (ac < 2 || (ac == 2 && av[1][0] == '-' && av[1][1] == 'h')) {
	bu_log("Usage: %s [-h] input [output] \n", av[0]);
	return 1;
    }

    in_type = get_model_type(av[1]);
    if (ac > 2) {
	out_type = get_model_type(av[2]);
    } else {
	/* If no output is specified, assume the requested
	 * operation is like ps2ps - read in the file and
	 * write out a cleaned-up version in the same format*/
	out_type = in_type;
    }
    in_fmt = bu_file_mime_str((int)in_type, MIME_MODEL);
    out_fmt = bu_file_mime_str((int)out_type, MIME_MODEL);

    bu_log("input file type: %s\n", in_fmt);
    bu_log("output file type: %s\n", out_fmt);

    if (in_fmt) bu_free((char *)in_fmt, "input format string");
    if (out_fmt) bu_free((char *)out_fmt, "output format string");

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

