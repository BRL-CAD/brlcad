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

#include "bu/app.h"
#include "bu/file.h"
#include "bu/str.h"
#include "rt/db4.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/wdb.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "../mime_probe.h"


extern int asc_read_v4(struct gcv_context *c, const struct gcv_opts *o, std::ifstream &fs);
extern int asc_read_v5(struct gcv_context *c, const struct gcv_opts *o, std::ifstream &fs, const char *source_path);
extern int asc_write_v4(struct db_i *database, const char *dest_path);
extern int asc_write_v5(struct gcv_context *c, const struct gcv_opts *o, const char *dest_path);


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
	const char *syntax = bu_mime_traits_get(gcv_target_traits(context), "asc.syntax");
	if (syntax && !BU_STR_EQUAL(syntax, script ? "tcl-commands" : "legacy-records")) {
	    bu_log("ASC syntax contradicts the selected profile\n");
	    return 0;
	}
	if (legacy) {
	    const bu_mime_trait value = BU_MIME_TRAIT_STRING("asc.syntax", "legacy-records");
	    const bu_mime_traits observed = {&value, 1};
	    if (!gcv_probe_file_traits_match(context, source_path, "application/vnd.brlcad.script", &observed, gcv_target_request_traits(context))) return 0;
	}
	input.clear();
	input.seekg(0);
	if (!input)
	    return 0;
	return (script ? asc_read_v5(context, options, input, source_path) : asc_read_v4(context, options, input)) == 1;
    }
    bu_log("ASC input contains no database records: %s\n", source_path);
    return 0;
}

static int
asc_write(struct gcv_context *context, const struct gcv_opts *options,
    const void *UNUSED(options_data), const char *destination)
{
    if (!context || !options || !destination)
	return 0;
    const char *syntax = bu_mime_traits_get(gcv_target_traits(context), "asc.syntax");
    if (!syntax || BU_STR_EQUAL(syntax, "tcl-commands"))
	return asc_write_v5(context, options, destination);
    if (!BU_STR_EQUAL(syntax, "legacy-records")) return 0;
    struct temporary_database {
	char path[MAXPATHLEN] = {0};
	struct db_i *database = NULL;
	~temporary_database() {
	    if (database) db_close(database);
	    if (path[0]) bu_file_delete(path);
	}
    } temporary;
    FILE *file = bu_temp_file(temporary.path, sizeof(temporary.path));
    if (!file) return 0;
    if (fclose(file)) return 0;
    struct rt_wdb *writer = wdb_fopen_v(temporary.path, 4);
    if (!writer) return 0;
    temporary.database = writer->dbip;
    if (!gcv_db_copy(context, writer, context->dbip)) return 0;
    return asc_write_v4(temporary.database, destination);
}


extern "C"
{
    struct gcv_filter gcv_conv_asc_read =
    {
	"ASC Reader",
	GCV_FILTER_READ,
	BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC,
	NULL,
	NULL,
	NULL,
	asc_read
    };

    static const struct gcv_conversion gcv_conv_asc_read_execution = {"ASC Reader", GCV_FILTER_READ, NULL, NULL, asc_read};

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

    static const struct gcv_conversion gcv_conv_asc_write_execution = {"ASC Writer", GCV_FILTER_WRITE, NULL, NULL, asc_write};

    static const struct gcv_filter * const filters[] = {&gcv_conv_asc_read, &gcv_conv_asc_write, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}
}

extern "C" COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
gcv_plugin_info_v2(void)
{
    static const char *const commands[] = {"title", "units", "put", "attr", "color"};
    static const struct bu_mime_trait tcl_reader[] = {
	BU_MIME_TRAIT_STRING("asc.syntax", "tcl-commands"),
	BU_MIME_TRAIT_SET("asc.commands", commands, sizeof(commands) / sizeof(commands[0]))};
    static const struct bu_mime_trait tcl_writer[] = {BU_MIME_TRAIT_STRING("asc.syntax", "tcl-commands")};
    static const struct bu_mime_trait legacy[] = {BU_MIME_TRAIT_STRING("asc.syntax", "legacy-records")};
    static const struct bu_mime_capability readers[] = {
	{"application/vnd.brlcad.script", tcl_reader, 2, 0, BU_MIME_DEFER_INPUT_VALIDATION},
	{"application/vnd.brlcad.script", legacy, 1, 0, BU_MIME_DEFER_INPUT_VALIDATION}};
    static const struct bu_mime_capability writers[] = {
	BU_MIME_CAPABILITY("application/vnd.brlcad.script", tcl_writer, 1, 0),
	BU_MIME_CAPABILITY("application/vnd.brlcad.script", legacy, 1, 0)};
    static const struct gcv_handler handlers[] = {
	{&gcv_conv_asc_read_execution, readers, 2, BU_MIME_DEFAULT_PRIORITY, NULL, NULL},
	{&gcv_conv_asc_write_execution, writers, 2, BU_MIME_DEFAULT_PRIORITY, NULL, NULL}};
    static const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2),
	NULL, 0, handlers, sizeof(handlers) / sizeof(handlers[0])};
    return &plugin;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
