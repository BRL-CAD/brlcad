/*                        B R L C A D . C
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
/** @file brlcad.c
 *
 * BRL-CAD .g database plugin for libgcv.
 *
 */


#include "common.h"

#include "../plugin.h"


HIDDEN int
gcv_brlcad_read(const char *path, struct rt_wdb *dest_wdbp,
		const struct gcv_opts *UNUSED(options))
{
    int ret;
    struct db_i * const in_dbip = db_open(path, DB_OPEN_READONLY);

    if (!in_dbip) {
	bu_log("db_open() failed for '%s'\n", path);
	return 0;
    }

    if (db_dirbuild(in_dbip)) {
	bu_log("db_dirbuild() failed for '%s'\n", path);
	db_close(in_dbip);
	return 0;
    }

    ret = db_dump(dest_wdbp, in_dbip);
    db_close(in_dbip);

    return ret == 0;
}


HIDDEN int
gcv_brlcad_write(const char *path, struct db_i *source_dbip,
		 const struct gcv_opts *UNUSED(options))
{
    int ret;
    struct rt_wdb * const out_wdbp = wdb_fopen(path);

    if (!out_wdbp) {
	bu_log("wdb_fopen() failed for '%s'\n", path);
	return 0;
    }

    ret = db_dump(out_wdbp, source_dbip);
    wdb_close(out_wdbp);

    return ret == 0;
}


static const struct gcv_converter converters[] = {
    {"g", gcv_brlcad_read, gcv_brlcad_write},
    {NULL, NULL, NULL}
};

const struct gcv_plugin_info gcv_plugin_conv_brlcad = {converters};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
