/*                      A S C _ V 5 . C P P
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
/** @file asc_v5.cpp
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

#include "bu/units.h"
#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"

static int
check_bracket_balance(int *ocnt, int *ccnt, std::string &s)
{
    for (size_t i = 0; i < s.length(); i++) {
	char c = s.at(i);
	if (c == '{')
	    (*ocnt)++;
	if (c == '}')
	    (*ccnt)++;
    }
    return (*ocnt == *ccnt);
}

/* Note - in its full generality, a "v5 ASCII BRL-CAD geometry file" may
 * technically be a completely arbitrary Tcl script, given the way the
 * traditional "asc2g" program is implemented.  However, real world practice is
 * generally go use g2asc to output geometry and asc2g to read it back in,
 * rather than constructing arbitrary procedural Tcl files to be read in by
 * asc2g (such procedural routines are more properly the in the bailiwick of
 * MGED.)  For the purposes of this plugin, the set of commands considered to
 * be legal in a v5 ASCII file are those that can be written out by g2asc.  As
 * of now, the known commands to handle are:
 *
 * color
 * title
 * units
 * attr
 * put
 *
 * The asc2g importer also defines the commands "find", "dbfind" and "rm" in
 * its Tcl environment, but it is not clear that these are necessary to read
 * g2asc exports - they do not appear to be written out by g2asc.  Until we
 * find a real-world use case, those commands will not be supported in this
 * plugin.
 */
int
asc_read_v5(
	struct gcv_context *c,
       	const struct gcv_opts *UNUSED(o),
	std::ifstream &fs
	)
{
    std::string sline;
    bu_log("Reading v5...\n");
    if (!c)
	return -1;
    struct bu_vls cur_line = BU_VLS_INIT_ZERO;
    int ocnt = 0;
    int ccnt = 0;
    int balanced = 0;
    while (std::getline(fs, sline)) {

	bu_vls_printf(&cur_line, " %s", sline.c_str());

	// If we don't have balanced brackets, we're either invalid or have
	// a multi-line command.  Assume the latter and try to read another
	// line.
	balanced = check_bracket_balance(&ocnt, &ccnt, sline);
	if (!balanced) {
	    continue;
	} else {
	    ocnt = 0;
	    ccnt = 0;
	}

	int list_c = 0;
	char **list_v = NULL;
	if (bu_argv_from_tcl_list(bu_vls_cstr(&cur_line), &list_c, (const char ***)&list_v) != 0 || list_c < 1) {
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}

	if (BU_STR_EQUAL(list_v[0], "attr")) {
	    rt_cmd_attr(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "color")) {
	    rt_cmd_color(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "put")) {
	    rt_cmd_put(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "title")) {
	    rt_cmd_title(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "units")) {
	    rt_cmd_units(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    bu_vls_trunc(&cur_line, 0);
	    continue;
	}

	bu_log("Unknown command: %s\n", list_v[0]);
	bu_free(list_v, "tcl argv list");
	bu_vls_trunc(&cur_line, 0);
    }

    bu_vls_free(&cur_line);

    return balanced;
}


static char *tclified_name=NULL;
static size_t tclified_name_buffer_len=0;

/*	This routine escapes the '{' and '}' characters in any string and returns a static buffer containing the
 *	resulting string. Used for names and db title on output.
 *
 *	NOTE: RETURN OF STATIC BUFFER
 */
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

	/* Process the _GLOBAL object */
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
	    const char *value;
	    struct bu_attribute_value_set g_avs;

	    /* get _GLOBAL attributes */
	    if (db5_get_attributes(dbip, &g_avs, dp)) {
		bu_log("Failed to find any attributes on _GLOBAL\n");
		continue;
	    }

	    /* save the associated attributes of
	     * _GLOBAL (except for title and units
	     * which were already written out) and
	     * regionid_colortable (which is written out below)
	     */
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
	    if (!value)
		continue;
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
		fprintf(v5ofp, " {%s}", avs->avp[i].name);
		fprintf(v5ofp, " {%s}", avs->avp[i].value);
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
