/*                         A S C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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

#include "common.h"
#include "vmath.h"

#include <fstream>
#include <cstring>
#include <string>

#include "bio.h"

#include "rt/db4.h"
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
asc_read(struct gcv_context *context, const struct gcv_opts *options,
    const void *UNUSED(options_data), const char *source_path)
{
    if (!context || !options || !source_path)
	return 0;
    std::ifstream input(source_path);
    if (!input) {
	bu_log("Cannot open ASC input %s\n", source_path);
	return 0;
    }
    std::string line;
    while (std::getline(input, line)) {
	if (line.find('\0') != std::string::npos) return 0;
	size_t first = line.find_first_not_of(" \t\r");
	if (first == std::string::npos || line[first] == '#')
	    continue;
	size_t end = line.find_first_of(" \t\r", first);
	std::string command = line.substr(first, end == std::string::npos ? end : end - first);
	bool script = command == "title" || command == "units" || command == "put" ||
	    command == "attr" || command == "color";
	static const char record_types[] = {ID_IDENT, ID_SOLID, ID_COMB, ID_MEMB, ID_ARS_A, ID_ARS_B,
	    ID_P_HEAD, ID_P_DATA, ID_COLORTAB, ID_BSOLID, ID_BSURF, DBID_PIPE, DBID_STRSOL,
	    DBID_NMG, DBID_PARTICLE, DBID_ARBN, DBID_CLINE, DBID_BOT, DBID_EXTR, DBID_SKETCH, 0};
	bool legacy = command.size() == 1 && strchr(record_types, command[0]);
	if (!script && !legacy) {
	    bu_log("Unsupported ASC syntax or initial command: %s\n", command.c_str());
	    return 0;
	}
	input.clear();
	input.seekg(0);
	if (!input)
	    return 0;
	return (script ? asc_read_v5(context, options, input) : asc_read_v4(context, options, input)) == 1;
    }
    bu_log("ASC input contains no database records: %s\n", source_path);
    return 0;
}

static int
asc_write(struct gcv_context *c, const struct gcv_opts *o,
	       const void *UNUSED(odata), const char *dest_path)
{
    if (!c || !o || !dest_path) return 0;
    if (db_version(c->dbip) < 5) {
	return asc_write_v4(c, o, dest_path);
    }
    return asc_write_v5(c, o, dest_path);
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
