/*                      A S C _ V 5 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
#include <sstream>
#include <string>
#include <cstring>

#include "bio.h"

#include "bu/units.h"
#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"

// NOTE: This importer must support multi-line quoted strings and multi-line
// brace blocks. Large brep serializations can span thousands of lines.

/* ----------------------------------------------------------------
 * Incremental parse state for Tcl list completeness tracking.
 * We avoid rescanning the whole accumulated buffer for each new line:
 * O(total_length) rather than O(total_length^2).
 * ---------------------------------------------------------------- */
struct asc_parse_state {
    bool in_quote = false;
    bool escape = false;
    int  brace_depth = 0;
    bool saw_top_level_quote = false; /* true if we started a top-level quoted
					 string (brace_depth==0 when it opened) */
};

/* Scan a fragment updating parse state. */
static void
asc_scan_fragment(asc_parse_state &st, const char *frag)
{
    if (!frag || !*frag) return;

    const char *p = frag;

    /* If we are in a large top-level quoted payload (brace_depth==0, in_quote,
     * and saw_top_level_quote) we can use a faster scan: look for an unescaped
     * closing quote without analyzing braces. */
    if (st.in_quote && st.brace_depth == 0 && st.saw_top_level_quote) {
	while (*p) {
	    char c = *p++;
	    if (st.escape) {
		st.escape = false;
		continue;
	    }
	    if (c == '\\') {
		st.escape = true;
		continue;
	    }
	    if (c == '"') {
		st.in_quote = false;
		/* End of top-level quoted payload */
		break;
	    }
	}
	return;
    }

    /* General scanner */
    for (; *p; ++p) {
	char c = *p;

	if (st.escape) {
	    st.escape = false;
	    continue;
	}
	if (c == '\\') {
	    st.escape = true;
	    continue;
	}

	if (st.in_quote) {
	    if (c == '"') {
		st.in_quote = false;
	    }
	    continue; /* braces ignored inside quotes */
	}

	if (c == '"') {
	    st.in_quote = true;
	    if (st.brace_depth == 0)
		st.saw_top_level_quote = true;
	    continue;
	}
	if (c == '{') {
	    st.brace_depth++;
	    continue;
	}
	if (c == '}') {
	    if (st.brace_depth > 0)
		st.brace_depth--;
	    continue;
	}
    }
}

static inline bool
asc_command_complete(const asc_parse_state &st)
{
    return (!st.in_quote && st.brace_depth == 0);
}

/* Note - in its full generality, a "v5 ASCII BRL-CAD geometry file" may
 * technically be a completely arbitrary Tcl script. For this plugin we
 * assume only commands that g2asc emits: color, title, units, attr, put.
 */
int
asc_read_v5(
	struct gcv_context *c,
	const struct gcv_opts *UNUSED(o),
	std::ifstream &fs
	)
{
    if (!c)
	return -1;

    bu_log("Reading v5...\n");

    std::string sline;
    struct bu_vls cur_line = BU_VLS_INIT_ZERO;
    asc_parse_state pst;
    struct rt_wdb *wdbp = wdb_dbopen(c->dbip, RT_WDB_TYPE_DB_INMEM);

    while (std::getline(fs, sline)) {
	/* Efficient append: avoid printf formatting overhead */
	bu_vls_strcat(&cur_line, sline.c_str());
	bu_vls_putc(&cur_line, '\n');

	/* Incremental scan only for the new line */
	asc_scan_fragment(pst, sline.c_str());

	if (!asc_command_complete(pst)) {
	    continue; /* Need more lines */
	}

	/* We believe we have a complete command. Parse once. */
	int list_c = 0;
	char **list_v = NULL;
	const char *cmd_str = bu_vls_cstr(&cur_line);
	if (bu_argv_from_tcl_list(cmd_str, &list_c, (const char ***)&list_v) != 0 || list_c < 1) {
	    bu_log("Skipping invalid Tcl list command: %s\n", cmd_str);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    pst = asc_parse_state(); /* reset state */
	    continue;
	}

	/* Command dispatch */
	const char *c0 = list_v[0];
	switch (c0[0]) {
	    case 'a':
		if (BU_STR_EQUAL(c0, "attr")) {
		    rt_cmd_attr(NULL, c->dbip, list_c, (const char **)list_v);
		    break;
		}
		goto unknown;
	    case 'c':
		if (BU_STR_EQUAL(c0, "color")) {
		    rt_cmd_color(NULL, c->dbip, list_c, (const char **)list_v);
		    break;
		}
		goto unknown;
	    case 'p':
		if (BU_STR_EQUAL(c0, "put")) {
		    rt_cmd_put(NULL, wdbp, list_c, (const char **)list_v);
		    break;
		}
		goto unknown;
	    case 't':
		/* 'title' or 'units' */
		if (c0[1] == 'i' && BU_STR_EQUAL(c0, "title")) {
		    rt_cmd_title(NULL, c->dbip, list_c, (const char **)list_v);
		    break;
		} else if (c0[1] == 'u' && BU_STR_EQUAL(c0, "units")) {
		    rt_cmd_units(NULL, c->dbip, list_c, (const char **)list_v);
		    break;
		}
		goto unknown;
	    default:
unknown:
		bu_log("Unknown command: %s\n", c0);
		break;
	}

	bu_free(list_v, "tcl argv list");
	bu_vls_trunc(&cur_line, 0);
	pst = asc_parse_state(); /* reset for next command */
    }

    /* Incomplete trailing command (if any) is ignored */
    bu_vls_free(&cur_line);

    return 1;
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

	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
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
