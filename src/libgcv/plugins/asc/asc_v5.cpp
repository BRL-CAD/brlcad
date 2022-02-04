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

#if 0

// TODO:
// This is the asc2g code that uses libtclcad
// to read in v5 asc files as Tcl scripts.
// It needs to be replaced with a non-Tcl based
// parsing code that reads what g2asc writes.

char *aliases[] = {
    "attr",
    "color",
    "put",
    "title",
    "units",
    "find",
    "dbfind",
    "rm",
    (char *)0
};

Tcl_Interp *interp;
Tcl_Interp *safe_interp;

/* this is a Tcl script */

rewind(ifp);
bu_vls_trunc(&line, 0);
BU_LIST_INIT(&RTG.rtg_headwdb.l);

interp = Tcl_CreateInterp();
Go_Init(interp);
wdb_close(ofp);

{
    int ac = 4;
    const char *av[5];

    av[0] = "to_open";
    av[1] = db_name;
    av[2] = "db";
    av[3] = argv[2];
    av[4] = (char *)0;

    if (to_open_tcl((ClientData)0, interp, ac, av) != TCL_OK) {
	fclose(ifp);
	bu_log("Failed to initialize tclcad_obj!\n");
	Tcl_Exit(1);
    }
}

/* Create the safe interpreter */
if ((safe_interp = Tcl_CreateSlave(interp, slave_name, 1)) == NULL) {
    fclose(ifp);
    bu_log("Failed to create safe interpreter");
    Tcl_Exit(1);
}

/* Create aliases */
{
    int i;
    int ac = 1;
    const char *av[2];

    av[1] = (char *)0;
    for (i = 0; aliases[i] != (char *)0; ++i) {
	av[0] = aliases[i];
	Tcl_CreateAlias(safe_interp, aliases[i], interp, db_name, ac, av);
    }
    /* add "find" separately */
    av[0] = "dbfind";
    Tcl_CreateAlias(safe_interp, "find", interp, db_name, ac, av);
}

while ((gettclblock(&line, ifp)) >= 0) {
    if (Tcl_Eval(safe_interp, (const char *)bu_vls_addr(&line)) != TCL_OK) {
	fclose(ifp);
	bu_log("Failed to process input file (%s)!\n", argv[1]);
	bu_log("%s\n", Tcl_GetStringResult(safe_interp));
	Tcl_Exit(1);
    }
    bu_vls_trunc(&line, 0);
}

/* free up our resources */
bu_vls_free(&line);
bu_vls_free(&str_title);
bu_vls_free(&str_put);

fclose(ifp);

Tcl_Exit(0);
#endif



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
#if 0
    // TODO - attempting to run the libged commands corresponding to the Tcl
    // commands exposes a limitation in the way libgcv and libged can interact.
    // Both are using dynamically loaded plugins, and if we have a libgcv
    // plugin call libged, at least on Linux, something about the way the
    // libged dynamic libraries are loaded causes problems for the OS - the
    // application ends up segfaulting on exit.  It will go through all the
    // steps I can see in gdb (apparently) successfully, but something about
    // the libged cleanup for the GED commands eventually results in a low
    // level segfault in OS level process clean up code.
    //
    // As near as I can tell, it is the dlclose call when the application is
    // exiting that triggers the problem.  gdb backtraces are uninformative
    // about the actual crash, so I don't know what precisely is unhappy.  Thus
    // far the only solution I've been able to think of is to refactor the guts
    // of the logic for the four or five commands needed for ascv5 parsing down
    // into librt (either as their own functions or as part of the rt_do_cmd
    // mechanism) so we won't need to load the libged plugins at the GCV plugin
    // level.  That's probably reasonable in this case, since the way the v5
    // asc format has been defined those commands ARE the .g serialization
    // format, but the problem hints at a broader issue.
    //
    // If this IS a general OS limitation and not some mistake on my part - no
    // loading plugin systems via dlopen/dlclose from shared objects that are
    // themselves loaded in via dlopen/dlclose - we should probably document
    // that as a general rule.  If that is the case substantive logic in libged
    // that may need to be called from gcv plugins should be moved to lower
    // level libraries, with just the GED command superstructure living in the
    // GED plugin itself...
#endif
    /* Commands to handle:
     *
     * title
     * units
     * attr
     * put
     */
    struct bu_vls cur_line = BU_VLS_INIT_ZERO;
    while (std::getline(fs, sline)) {
	std::cout << sline << "\n";
	bu_vls_sprintf(&cur_line, "%s", sline.c_str());
	int list_c = 0;
	char **list_v = NULL;
	if (bu_argv_from_tcl_list(bu_vls_cstr(&cur_line), &list_c, (const char ***)&list_v) != 0 || list_c < 1) {
	    bu_free(list_v, "tcl argv list");
	    continue;
	}

	if (BU_STR_EQUAL(list_v[0], "attr")) {
	    std::cout << "Found attr\n";
	    rt_cmd_attr(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "put")) {
	    std::cout << "Found put\n";
	    rt_cmd_put(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "title")) {
	    std::cout << "Found title\n";
	    rt_cmd_title(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    continue;
	}
	if (BU_STR_EQUAL(list_v[0], "units")) {
	    std::cout << "Found units\n";
	    rt_cmd_units(NULL, c->dbip, list_c, (const char **)list_v);
	    bu_free(list_v, "tcl argv list");
	    continue;
	}
	std::cout << "Unknown command: " << list_v[0] << "\n";
	bu_free(list_v, "tcl argv list");
    }
    bu_vls_free(&cur_line);
    return 1;
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
