/*                        B R L C A D . C
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
/** @file brlcad.c
 *
 * BRL-CAD .g database plugin for libgcv.
 *
 */


#include "common.h"

#include "gcv/api.h"


HIDDEN int
brlcad_read(struct gcv_context *context,
		const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data),
		const char *source_path)
{
    int ret;
    struct db_i * const in_dbip = db_open(source_path, DB_OPEN_READONLY);

    if (!in_dbip) {
	bu_log("db_open() failed for '%s'\n", source_path);
	return 0;
    }

    if (db_dirbuild(in_dbip)) {
	bu_log("db_dirbuild() failed for '%s'\n", source_path);
	db_close(in_dbip);
	return 0;
    }

    ret = db_dump(context->dbip->dbi_wdbp, in_dbip);
    db_close(in_dbip);

    return ret == 0;
}


HIDDEN int
brlcad_write(struct gcv_context *context,
		 const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data),
		 const char *dest_path)
{
    int ret;
    struct rt_wdb * const out_wdbp = wdb_fopen(dest_path);

    if (!out_wdbp) {
	bu_log("wdb_fopen() failed for '%s'\n", dest_path);
	return 0;
    }

    ret = db_dump(out_wdbp, context->dbip);
    wdb_close(out_wdbp);

    return ret == 0;
}


const struct gcv_filter gcv_conv_brlcad_read =
{"BRL-CAD Reader", GCV_FILTER_READ, MIME_MODEL_VND_BRLCAD_PLUS_BINARY, NULL, NULL, brlcad_read};


const struct gcv_filter gcv_conv_brlcad_write =
{"BRL-CAD Writer", GCV_FILTER_WRITE, MIME_MODEL_VND_BRLCAD_PLUS_BINARY, NULL, NULL, brlcad_write};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
