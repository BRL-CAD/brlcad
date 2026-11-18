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
int
asc_read_v5(struct gcv_context *context, const struct gcv_opts *UNUSED(options), std::ifstream &input)
{
    if (!context)
	return 0;
    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	return 0;
    struct bu_vls command = BU_VLS_INIT_ZERO;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    asc_parse_state state;
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
	int result = BRLCAD_ERROR;
	if (BU_STR_EQUAL(arguments[0], "attr")) {
	    /* Serialized names are literal, including pattern characters. */
	    result = count > 1 && BU_STR_EQUAL(arguments[1], "set") ?
		asc_set_attributes(context->dbip, count, arguments) : rt_cmd_attr(&messages, context->dbip, count, arguments);
	}
	else if (BU_STR_EQUAL(arguments[0], "color"))
	    result = rt_cmd_color(&messages, context->dbip, count, arguments);
	else if (BU_STR_EQUAL(arguments[0], "put"))
	    result = rt_cmd_put(&messages, wdbp, count, arguments);
	else if (BU_STR_EQUAL(arguments[0], "title"))
	    result = rt_cmd_title(&messages, context->dbip, count, arguments);
	else if (BU_STR_EQUAL(arguments[0], "units"))
	    result = rt_cmd_units(&messages, context->dbip, count, arguments);
	else
	    bu_vls_printf(&messages, "Unsupported ASC command: %s\n", arguments[0]);
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
    if (bu_vls_strlen(&messages))
	bu_log("%s", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    bu_vls_free(&command);
    return status;
}


static char *tclified_name=NULL;
static size_t tclified_name_buffer_len=0;

/* Escape '{' and '}' characters in any string (static buffer return). */
char *
tclify_name(const char *name)
{
    const char *src=name;
    char *dest;

    size_t max_len=2 * strlen(name) + 1;

    if (max_len < 2) {
	return (char *)NULL;
    }

    if (max_len > tclified_name_buffer_len) {
	tclified_name_buffer_len = max_len;
	tclified_name = (char *)bu_realloc(tclified_name, tclified_name_buffer_len, "tclified_name buffer");
    }

    dest = tclified_name;

    while (*src) {
	if (*src == '{' || *src == '}') {
	    *dest++ = '\\';
	}
	*dest++ = *src++;
    }
    *dest = '\0';

    return tclified_name;
}


int
asc_write_v5(
	struct gcv_context *c,
	const struct gcv_opts *UNUSED(o),
	const char *dest_path
	)
{
    FILE *v5ofp = NULL;
    if (!dest_path) return 0;

    struct db_i	*dbip = c->dbip;
    struct directory *dp;
    const char *u;

    // TODO - eventually we should be able to do this behind the scenes, but
    // for a first cut don't add the extra complication
    if (db_version(dbip) == 4) {
	bu_log("Attempting to write v5 asc output with a v4 database - first run db_upgrade to produce a v5 file\n");
	return 0;
    }

    v5ofp = fopen(dest_path, "wb");
    if (!v5ofp) {
	bu_log("Could not open %s for writing.\n", dest_path);
	return 0;
    }

    /* write out the title and units special */
    /* title and units */
    if (dbip->dbi_title[0]) {
	fprintf(v5ofp, "title {%s}\n", tclify_name(dbip->dbi_title));
    } else {
	fprintf(v5ofp, "title {Untitled BRL-CAD Database}\n");
    }
    u = bu_units_string(dbip->dbi_local2base);
    if (u) {
	fprintf(v5ofp, "units %s\n", u);
    }
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	struct rt_db_internal	intern;
	struct bu_attribute_value_set *avs=NULL;

	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
	    const char *value;
	    struct bu_attribute_value_set g_avs;

	    if (db5_get_attributes(dbip, &g_avs, dp)) {
		bu_log("Failed to find any attributes on _GLOBAL\n");
		continue;
	    }

	    if (g_avs.count) {
		int printedHeader = 0;
		for (unsigned int i = 0; i < g_avs.count; i++) {
		    if (bu_strncmp(g_avs.avp[i].name, "title", 6) == 0) {
			continue;
		    } else if (bu_strncmp(g_avs.avp[i].name, "units", 6) == 0) {
			continue;
		    } else if (bu_strncmp(g_avs.avp[i].name, "regionid_colortable", 19) == 0) {
			continue;
		    } else if (strlen(g_avs.avp[i].name) <= 0) {
			continue;
		    }
		    if (printedHeader == 0) {
			fprintf(v5ofp, "attr set {_GLOBAL}");
			printedHeader = 1;
		    }
		    fprintf(v5ofp, " {%s} {%s}", g_avs.avp[i].name, g_avs.avp[i].value);
		}
		if (printedHeader == 1)
		    fprintf(v5ofp, "\n");
	    }

	    value = bu_avs_get(&g_avs, "regionid_colortable");
	    if (!value) {
		bu_avs_free(&g_avs);
		continue;
	    }

	    /* TODO - do this without Tcl */
#if 0
	    size_t list_len;
	    list = Tcl_NewStringObj(value, -1);
	    {
		int llen;
		if (Tcl_ListObjLength(interp, list, &llen) != TCL_OK) {
		    bu_log("Failed to get length of region color table!!\n");
		    continue;
		}
		list_len = (size_t)llen;
	    }
	    for (i = 0; i < list_len; i++) {
		if (Tcl_ListObjIndex(interp, list, i, &obj) != TCL_OK) {
		    bu_log("Cannot get entry %d from the color table!!\n",
			    i);
		    continue;
		}
		fprintf(v5ofp, "color %s\n",
			Tcl_GetStringFromObj(obj, NULL));
	    }
#endif
	    bu_avs_free(&g_avs);
	    continue;
	}

	if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	    bu_log("Unable to read '%s', skipping\n", dp->d_namep);
	    continue;
	}
	if (!intern.idb_meth->ft_get) {
	    bu_log("Unable to get '%s' (unimplemented), skipping\n", dp->d_namep);
	    rt_db_free_internal(&intern);
	    continue;
	}

	if (dp->d_flags & RT_DIR_COMB) {
	    struct bu_vls logstr = BU_VLS_INIT_ZERO;

	    if (intern.idb_meth->ft_get(&logstr, &intern, "tree") != 0) {
		rt_db_free_internal(&intern);
		bu_log("Unable to export '%s', skipping: %s\n", dp->d_namep, bu_vls_cstr(&logstr));
		bu_vls_free(&logstr);
		continue;
	    }
	    if (dp->d_flags & RT_DIR_REGION) {
		fprintf(v5ofp, "put {%s} comb region yes tree {%s}\n",
			tclify_name(dp->d_namep),
			bu_vls_cstr(&logstr));
	    } else {
		fprintf(v5ofp, "put {%s} comb region no tree {%s}\n",
			tclify_name(dp->d_namep),
			bu_vls_cstr(&logstr));
	    }
	    bu_vls_free(&logstr);
	} else {
	    struct bu_vls logstr = BU_VLS_INIT_ZERO;

	    if ((dp->d_minor_type != ID_CONSTRAINT) && (intern.idb_meth->ft_get(&logstr, &intern, NULL) != 0)) {
		rt_db_free_internal(&intern);
		bu_log("Unable to export '%s', skipping: %s\n", dp->d_namep, bu_vls_cstr(&logstr));
		bu_vls_free(&logstr);
		continue;
	    }
	    fprintf(v5ofp, "put {%s} %s\n",
		    tclify_name(dp->d_namep),
		    bu_vls_cstr(&logstr));
	    bu_vls_free(&logstr);
	}
	avs = &intern.idb_avs;
	if (avs->magic == BU_AVS_MAGIC && avs->count > 0) {
	    fprintf(v5ofp, "attr set {%s}", tclify_name(dp->d_namep));
	    for (unsigned int i = 0; i < avs->count; i++) {
		if (strlen(avs->avp[i].name) <= 0) {
		    continue;
		}
		fprintf(v5ofp, " {%s} {%s}", avs->avp[i].name, avs->avp[i].value);
	    }
	    fprintf(v5ofp, "\n");
	}
	rt_db_free_internal(&intern);
    } FOR_ALL_DIRECTORY_END;

    fclose(v5ofp);

    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
