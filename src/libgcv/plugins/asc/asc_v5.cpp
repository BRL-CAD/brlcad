/*                      A S C _ V 5 . C P P
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

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>

#include "bio.h"

#include "bu/units.h"
#include "rt/db_attr.h"
#include "rt/mater.h"
#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "../mime_probe.h"

/* Track Tcl-list word boundaries incrementally so large serialized objects
 * are scanned once. Quotes inside brace words are literal characters. */
struct asc_parse_state {
    bool in_quote = false;
    bool escape = false;
    bool word_start = true;
    bool continued = false;
    size_t brace_depth = 0;
};

static void
asc_scan_fragment(asc_parse_state &state, const char *fragment)
{
    state.continued = false;
    for (const char *at = fragment; *at; ++at) {
	char value = *at;
	if (state.escape) {
	    state.escape = false;
	    if (value == '\n') {
		state.continued = true;
		if (!state.in_quote && !state.brace_depth) state.word_start = true;
	    }
	    continue;
	}
	if (value == '\\') {
	    state.escape = true;
	    state.word_start = false;
	} else if (state.brace_depth) {
	    if (value == '{') ++state.brace_depth;
	    if (value == '}') --state.brace_depth;
	} else if (state.in_quote) {
	    if (value == '\"') state.in_quote = false;
	} else if (state.word_start && value == '{') {
	    state.brace_depth = 1;
	    state.word_start = false;
	} else if (state.word_start && value == '\"') {
	    state.in_quote = true;
	    state.word_start = false;
	} else {
	    state.word_start = std::isspace(static_cast<unsigned char>(value)) != 0;
	}
    }
}

static bool
asc_command_complete(const asc_parse_state &state)
{
    return !state.in_quote && !state.brace_depth && !state.escape && !state.continued;
}

/* Tcl replaces continued newlines before finding words, even inside braces.
 * List parsing alone would turn a continuation between words into an extra
 * word beginning with a space. Preserve other backslash pairs for that parser. */
static std::string
asc_list_text(const char *text)
{
    std::string normalized;
    for (const char *at = text; *at; ++at) {
	if (*at == '\\' && at[1]) {
	    if (at[1] == '\n') {
		++at;
		while (at[1] == ' ' || at[1] == '\t') ++at;
		normalized += ' ';
	    } else {
		normalized += *at++;
		normalized += *at;
	    }
	} else {
	    normalized += *at;
	}
    }
    return normalized;
}

static int
asc_set_attributes(struct db_i *database, int count, const char **arguments)
{
    if (count < 3 || count % 2 == 0) return BRLCAD_ERROR;
    struct directory *entry = db_lookup(database, arguments[2], LOOKUP_QUIET);
    if (!entry) return BRLCAD_ERROR;
    bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    if (db5_get_attributes(database, &attributes, entry)) {
	bu_avs_free(&attributes);
	return BRLCAD_ERROR;
    }
    for (int i = 3; i + 1 < count; i += 2)
	bu_avs_add(&attributes, arguments[i], arguments[i + 1]);
    db5_standardize_avs(&attributes);
    int flags = entry->d_flags;
    if (flags & RT_DIR_COMB) {
	flags &= ~RT_DIR_REGION;
	if (bu_avs_get(&attributes, "region")) flags |= RT_DIR_REGION;
    }
    const bool global = BU_STR_EQUAL(entry->d_namep, DB5_GLOBAL_OBJECT_NAME);
    std::string title;
    std::string colors;
    double scale = database->dbi_local2base;
    if (global) {
	const char *value = bu_avs_get(&attributes, "title");
	title = value ? value : database->dbi_title;
	value = bu_avs_get(&attributes, "units");
	if (value) {
	    char *end = NULL;
	    errno = 0;
	    scale = std::strtod(value, &end);
	    while (isspace((unsigned char)*end)) ++end;
	    if (end == value || *end || errno == ERANGE || !std::isfinite(scale) || scale <= 0) {
		bu_avs_free(&attributes);
		return BRLCAD_ERROR;
	    }
	}
	value = bu_avs_get(&attributes, "regionid_colortable");
	if (value) colors = value;
    }
    if (db5_update_attributes(entry, &attributes, database)) {
	bu_avs_free(&attributes);
	return BRLCAD_ERROR;
    }
    entry->d_flags = flags;
    if (global) {
	if (db_update_ident(database, title.c_str(), scale) < 0) return BRLCAD_ERROR;
	db_mater_free(database);
	if (!colors.empty()) {
	    char *copy = bu_strdup(colors.c_str());
	    db5_import_color_table(database, copy);
	    bu_free(copy, "ASC color table");
	}
    }
    return BRLCAD_OK;
}

/* Note - in its full generality, a "v5 ASCII BRL-CAD geometry file" may
 * technically be a completely arbitrary Tcl script. For this plugin we
 * assume only commands that g2asc emits: color, title, units, attr, put.
 */
static int
asc_read_v5_pass(struct gcv_context *context, std::ifstream &input, bool execute, const char *source_path)
{
    if (!context)
	return 0;
    struct rt_wdb *wdbp = execute ? wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM) : NULL;
    if (execute && !wdbp)
	return 0;
    struct bu_vls command = BU_VLS_INIT_ZERO;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    asc_parse_state state;
    std::set<std::string> commands;
    std::string line;
    int status = 1;
    while (std::getline(input, line)) {
	if (!line.empty() && line.back() == '\r') line.pop_back();
	if (line.find('\0') != std::string::npos) {
	    bu_vls_strcat(&messages, "ASC text contains a NUL byte\n");
	    status = 0;
	    break;
	}
	if (!bu_vls_strlen(&command)) {
	    size_t first = line.find_first_not_of(" \t\r");
	    if (first == std::string::npos || line[first] == '#')
		continue;
	}
	bu_vls_strcat(&command, line.c_str());
	bu_vls_putc(&command, '\n');
	asc_scan_fragment(state, line.c_str());
	asc_scan_fragment(state, "\n");
	if (!asc_command_complete(state))
	    continue;
	int count = 0;
	const char **arguments = NULL;
	const std::string list = asc_list_text(bu_vls_cstr(&command));
	if (bu_argv_from_tcl_list(list.c_str(), &count, &arguments) || count < 1) {
	    bu_log("Invalid ASC command: %s", bu_vls_cstr(&command));
	    bu_free((void *)arguments, "ASC arguments");
	    status = 0;
	    break;
	}
	if (!execute) commands.insert(arguments[0]);
	int result = BRLCAD_OK;
	if (BU_STR_EQUAL(arguments[0], "attr")) {
	    /* Serialized object names are literal references, including names
	     * containing characters that interactive attr treats as patterns. */
	    if (execute) result = count > 1 && BU_STR_EQUAL(arguments[1], "set") ?
		asc_set_attributes(context->dbip, count, arguments) : rt_cmd_attr(&messages, context->dbip, count, arguments);
	}
	else if (BU_STR_EQUAL(arguments[0], "color")) {
	    if (execute) result = rt_cmd_color(&messages, context->dbip, count, arguments);
	} else if (BU_STR_EQUAL(arguments[0], "put")) {
	    if (execute) result = rt_cmd_put(&messages, wdbp, count, arguments);
	} else if (BU_STR_EQUAL(arguments[0], "title")) {
	    if (execute) result = rt_cmd_title(&messages, context->dbip, count, arguments);
	} else if (BU_STR_EQUAL(arguments[0], "units")) {
	    if (execute) result = rt_cmd_units(&messages, context->dbip, count, arguments);
	} else {
	    bu_vls_printf(&messages, "Unsupported ASC command: %s\n", arguments[0]);
	    result = BRLCAD_ERROR;
	}
	bu_free((void *)arguments, "ASC arguments");
	if (result != BRLCAD_OK) {
	    status = 0;
	    break;
	}
	bu_vls_trunc(&command, 0);
	state = asc_parse_state();
    }
    if (input.bad() || (status && bu_vls_strlen(&command))) {
	bu_vls_strcat(&messages, "Incomplete or unreadable ASC command\n");
	status = 0;
    }
    if (status && !execute) {
	struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	std::vector<const char *> names;
	for (const auto &name : commands) names.push_back(name.c_str());
	status = bu_mime_traits_add(&observed, "asc.syntax", "tcl-commands") &&
	    bu_mime_traits_add_set(&observed, "asc.commands", names.data(), names.size()) &&
	    gcv_probe_file_traits_match(context, source_path, "application/vnd.brlcad.script", &observed, gcv_target_request_traits(context));
	if (!status) bu_vls_strcat(&messages, "ASC command set contradicts the selected profile\n");
	bu_mime_traits_free(&observed);
    }
    if (bu_vls_strlen(&messages))
	bu_log("%s", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    bu_vls_free(&command);
    return status;
}

int
asc_read_v5(struct gcv_context *context, const struct gcv_opts *UNUSED(options), std::ifstream &input, const char *source_path)
{
    if (!context) return 0;
    const bu_mime_traits *requested = gcv_target_request_traits(context);
    if (requested && requested->count) {
	/* Complete command-set assertions must fail before executing commands.
	 * Unasserted input keeps the existing single-pass conversion path. */
	const std::streampos start = input.tellg();
	if (start == std::streampos(-1) || !asc_read_v5_pass(context, input, false, source_path)) return 0;
	input.clear();
	input.seekg(start);
	if (!input) return 0;
    }
    return asc_read_v5_pass(context, input, true, source_path);
}


static std::string
tcl_word(const char *value)
{
    std::string quoted = "\"";
    for (const char *at = value; *at; ++at) {
	switch (*at) {
	    case '\\': quoted += "\\\\"; break;
	    case '\"': quoted += "\\\""; break;
	    case '$': case '[': case ']': quoted += '\\'; quoted += *at; break;
	    case '\n': quoted += "\\n"; break;
	    case '\r': quoted += "\\r"; break;
	    case '\t': quoted += "\\t"; break;
	    default: quoted += *at; break;
	}
    }
    quoted += '\"';
    return quoted;
}

static bool
write_attributes(FILE *output, const char *name, const bu_attribute_value_set &attributes, bool global)
{
    bool started = false;
    for (size_t i = 0; i < attributes.count; ++i) {
	const auto &attribute = attributes.avp[i];
	if (!attribute.name[0] || (global && (BU_STR_EQUAL(attribute.name, "title") || BU_STR_EQUAL(attribute.name, "units"))))
	    continue;
	if (!started) {
	    if (fprintf(output, "attr set %s", tcl_word(name).c_str()) < 0) return false;
	    started = true;
	}
	if (fprintf(output, " %s %s", tcl_word(attribute.name).c_str(), tcl_word(attribute.value).c_str()) < 0)
	    return false;
    }
    return (!started || fputc('\n', output) != EOF) && !ferror(output);
}

int
asc_write_v5(struct gcv_context *context, const struct gcv_opts *UNUSED(options), const char *destination)
{
    if (!context || !destination || db_version(context->dbip) != 5) return 0;
    FILE *output = fopen(destination, "wb");
    if (!output) return 0;
    struct db_i *database = context->dbip;
    /* A numeric scale also preserves valid units absent from the unit table. */
    int status = fprintf(output, "title %s\n", tcl_word(database->dbi_title).c_str()) >= 0 &&
	fprintf(output, "units %.17gmm\n", database->dbi_local2base) >= 0;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, database) {
	if (!status) break;
	if (entry->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && BU_STR_EQUAL(entry->d_namep, DB5_GLOBAL_OBJECT_NAME)) {
	    bu_attribute_value_set attributes;
	    bu_avs_init_empty(&attributes);
	    status = !db5_get_attributes(database, &attributes, entry) &&
		write_attributes(output, entry->d_namep, attributes, true);
	    bu_avs_free(&attributes);
	    continue;
	}
	struct rt_db_internal object;
	RT_DB_INTERNAL_INIT(&object);
	if (rt_db_get_internal(&object, entry, database, NULL) < 0) {
	    bu_log("Cannot read ASC output object %s\n", entry->d_namep);
	    status = 0;
	    break;
	}
	struct bu_vls description = BU_VLS_INIT_ZERO;
	bool combination = (entry->d_flags & RT_DIR_COMB) != 0;
	if (!object.idb_meth->ft_get || object.idb_meth->ft_get(&description, &object, combination ? "tree" : NULL)) {
	    (void)gcv_report_loss(context, GCV_LOSS_GEOMETRY, "asc.unserializable-object", 1,
		"An object has no supported ASC serialization");
	    bu_log("Cannot serialize ASC object %s: %s\n", entry->d_namep, bu_vls_cstr(&description));
	    status = 0;
	} else if (combination) {
	    status = fprintf(output, "put %s comb region %s tree %s\n", tcl_word(entry->d_namep).c_str(),
		entry->d_flags & RT_DIR_REGION ? "yes" : "no", tcl_word(bu_vls_cstr(&description)).c_str()) >= 0;
	} else {
	    status = bu_vls_strlen(&description) && fprintf(output, "put %s %s\n",
		tcl_word(entry->d_namep).c_str(), bu_vls_cstr(&description)) >= 0;
	}
	if (status && object.idb_avs.magic == BU_AVS_MAGIC)
	    status = write_attributes(output, entry->d_namep, object.idb_avs, false);
	bu_vls_free(&description);
	rt_db_free_internal(&object);
    } FOR_ALL_DIRECTORY_END;
    status = status && !ferror(output);
    if (fclose(output)) status = 0;
    return status;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
