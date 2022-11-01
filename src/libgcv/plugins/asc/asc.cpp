/*                         A S C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file asc.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include "vmath.h"

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "rt/db_io.h"
#include "gcv/api.h"
#include "gcv/util.h"

extern int asc_read_v4(struct gcv_context *c, const struct gcv_opts *o, std::ifstream &fs);
extern int asc_read_v5(struct gcv_context *c, const struct gcv_opts *o, std::ifstream &fs);
extern int asc_write_v4(struct gcv_context *c, const struct gcv_opts *o, const char *dest_path);
extern int asc_write_v5(struct gcv_context *c, const struct gcv_opts *o, const char *dest_path);

static int
asc_can_read(const char *data)
{
    if (!data) return 0;
    bu_log("asc can read: %s\n", data);
    return 1;
}

static int
asc_read(
	struct gcv_context *c,
       	const struct gcv_opts *o,
	const void *UNUSED(o_data),
       	const char *spath
	)
{
    struct bu_vls vline = BU_VLS_INIT_ZERO;
    int fmt = -1;
    if (!c || !o || !spath) return 0;
    std::string sline;
    std::ifstream fs;
    fs.open(spath);
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << spath << " for reading, skipping\n";
	return 0;
    }

    // asc2g checked for either title or put as the first line to denote a new
    // style asc file.
    struct bu_vls str_title = BU_VLS_INIT_ZERO;
    struct bu_vls str_put = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&str_title, "title");
    bu_vls_strcpy(&str_put, "put ");

    while (std::getline(fs, sline)) {
	std::cout << sline << "\n";
	bu_vls_sprintf(&vline, "%s", sline.c_str());
	bu_vls_trimspace(&vline);
	if (!bu_vls_strlen(&vline) || bu_vls_cstr(&vline)[0] == '#') {
	    // Comment line or empty line - skip
	    continue;
	}
	if (fmt < 0) {
	    // Don't have format yet - check if this is a v4 asc file or a v5
	    // asc file.  The two are handled differently.
	    if (!bu_vls_strncmp(&vline, &str_title, 5) || !bu_vls_strncmp(&vline, &str_put, 4)) {
		fmt = 5;
	    } else {
		fmt = 4;
	    }
	}
	if (fmt) {
	    switch (fmt) {
		case 4:
		    fs.seekg(0);
		    asc_read_v4(c, o, fs);
		    break;
		case 5:
		    fs.seekg(0);
		    asc_read_v5(c, o, fs);
		    break;
		default:
		    std::cerr << "Unknown asc format version: " << fmt << "\n";
		    return 0;
		    break;
	    }
	}
    }

    // Not yet implemented - always return failure until we have something working...
    return 1;
}

static int
asc_write(struct gcv_context *c, const struct gcv_opts *o,
	       const void *UNUSED(odata), const char *dest_path)
{
    if (!c || !o || !dest_path) return 0;
    if (db_version(c->dbip) < 5) {
	asc_write_v4(c, o, dest_path);
    } else {
	asc_write_v5(c, o, dest_path);
    }
    return 1;
}

extern "C"
{
    struct gcv_filter gcv_conv_asc_read =
    {
	"ASC Reader",
	GCV_FILTER_READ,
	BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC,
	asc_can_read,
	NULL,
	NULL,
	asc_read
    };

    struct gcv_filter gcv_conv_asc_write =
    {
	"ASC Writer",
       	GCV_FILTER_WRITE,
       	BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC,
       	NULL,
       	NULL,
       	NULL,
       	asc_write
    };

    static const struct gcv_filter * const filters[] = {&gcv_conv_asc_read, &gcv_conv_asc_write, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
