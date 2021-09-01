/*                         C O N C A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/concat.c
 *
 * The concat command.
 *
 */

#include "common.h"

#include <string>
#include <set>
#include <unordered_map>

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "rt/geom.h"

#include "../ged_private.h"


/**
 * structure used by the dbconcat command for keeping tract of where
 * objects are being copied to and from, and what type of affix to
 * use.
 */
struct ged_concat_data {
    int copy_mode;
    struct db_i *old_dbip;
    struct db_i *new_dbip;
    struct bu_vls affix;
};


#define NO_AFFIX	1<<0
#define AUTO_PREFIX	1<<1
#define AUTO_SUFFIX	1<<2
#define CUSTOM_PREFIX	1<<3
#define CUSTOM_SUFFIX	1<<4


/**
 * find a new unique name given a list of previously used names, and
 * the type of naming mode that described what type of affix to use.
 */
static std::string
get_new_name(const char *name,
		 struct db_i *dbip,
		 std::unordered_map<std::string, std::string> &name_map,
		 std::set<std::string> &used_names,
		 struct ged_concat_data *cc_data)
{
    struct bu_vls new_name = BU_VLS_INIT_ZERO;
    char *aname = NULL;
    long num=0;

    RT_CK_DBI(dbip);
    BU_ASSERT(cc_data);

    if (!name) {
	bu_log("WARNING: encountered NULL name, renaming to \"UNKNOWN\"\n");
	name = "UNKNOWN";
    }

    std::string nname = std::string(name);

    // If we already have a mapping assigned for this object name, reuse it.
    if (name_map.find(nname) != name_map.end()) {
	return name_map[nname];
    }

    do {
	/* iterate until we find an object name that is not in
	 * use, trying to accommodate the user's requested affix
	 * naming mode.
	 */

	bu_vls_trunc(&new_name, 0);

	if (cc_data->copy_mode & NO_AFFIX) {
	    if (num > 0 && cc_data->copy_mode & CUSTOM_PREFIX) {
		/* auto-increment prefix */
		bu_vls_printf(&new_name, "%ld_", num);
	    }
	    bu_vls_strcat(&new_name, name);
	    if (num > 0 && cc_data->copy_mode & CUSTOM_SUFFIX) {
		/* auto-increment suffix */
		bu_vls_printf(&new_name, "_%ld", num);
	    }
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    /* use custom suffix */
	    bu_vls_strcpy(&new_name, name);
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld_", num);
	    }
	    bu_vls_vlscat(&new_name, &cc_data->affix);
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    /* use custom prefix */
	    bu_vls_vlscat(&new_name, &cc_data->affix);
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld_", num);
	    }
	    bu_vls_strcat(&new_name, name);
	} else if (cc_data->copy_mode & AUTO_SUFFIX) {
	    /* use auto-incrementing suffix */
	    bu_vls_strcat(&new_name, name);
	    bu_vls_printf(&new_name, "_%ld", num);
	} else if (cc_data->copy_mode & AUTO_PREFIX) {
	    /* use auto-incrementing prefix */
	    bu_vls_printf(&new_name, "%ld_", num);
	    bu_vls_strcat(&new_name, name);
	} else {
	    /* no custom suffix/prefix specified, use prefix */
	    if (num > 0) {
		bu_vls_printf(&new_name, "_%ld", num);
	    }
	    bu_vls_strcpy(&new_name, name);
	}

	/* make sure it fits for v4 */
	if (db_version(cc_data->old_dbip) < 5) {
	    if (bu_vls_strlen(&new_name) > _GED_V4_MAXNAME) {
		bu_log("ERROR: generated new name [%s] is too long (%zu > %d)\n", bu_vls_addr(&new_name), bu_vls_strlen(&new_name), _GED_V4_MAXNAME);
	    }
	    return NULL;
	}
	aname = bu_vls_addr(&new_name);

	num++;

    } while (db_lookup(dbip, aname, LOOKUP_QUIET) != RT_DIR_NULL ||
	     (used_names.find(std::string(aname)) != used_names.end()));

    /* if they didn't get what they asked for, warn them */
    if (num > 1) {
	if (cc_data->copy_mode & NO_AFFIX) {
	    bu_log("WARNING: unable to import [%s] without an affix, imported as [%s]\n", name, bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, name, bu_vls_addr(&cc_data->affix), bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, bu_vls_addr(&cc_data->affix), name, bu_vls_addr(&new_name));
	}
    }

    /* we should now have a unique name.  store it in the map and the used_names set */
    name_map[nname] = std::string(bu_vls_cstr(&new_name));
    used_names.insert(name_map[nname]);
    bu_vls_free(&new_name);

    return name_map[nname];
}


static void
adjust_names(union tree *trp,
		 struct db_i *dbip,
		 std::unordered_map<std::string, std::string> &name_map,
		 std::set<std::string> &used_names,
		 struct ged_concat_data *cc_data)
{
    std::string new_name;
    std::string old_name;

    if (trp == NULL) {
	return;
    }

    switch (trp->tr_op) {
	case OP_DB_LEAF:
	    old_name = std::string(trp->tr_l.tl_name);
	    new_name = get_new_name(trp->tr_l.tl_name, dbip,
					name_map, used_names, cc_data);
	    if (old_name != new_name) {
		bu_free(trp->tr_l.tl_name, "leaf name");
		trp->tr_l.tl_name = bu_strdup(new_name.c_str());
	    }
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    adjust_names(trp->tr_b.tb_left, dbip,
			     name_map, used_names, cc_data);
	    adjust_names(trp->tr_b.tb_right, dbip,
			     name_map, used_names, cc_data);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    adjust_names(trp->tr_b.tb_left, dbip,
			     name_map, used_names, cc_data);
	    break;
    }
}


static int
copy_object(struct ged *gedp,
	    struct directory *input_dp,
	    struct db_i *input_dbip,
	    struct db_i *curr_dbip,
	    std::unordered_map<std::string, std::string> &name_map,
	    std::set<std::string> &used_names,
	    struct ged_concat_data *cc_data)
{
    struct rt_db_internal ip;
    struct rt_extrude_internal *extr;
    struct rt_dsp_internal *dsp;
    struct rt_comb_internal *comb;
    struct directory *new_dp;
    std::string new_name;

    if (rt_db_get_internal(&ip, input_dp, input_dbip, NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to get internal form of object (%s) - aborting!!!\n",
		      input_dp->d_namep);
	return GED_ERROR;
    }

    if (ip.idb_major_type == DB5_MAJORTYPE_BRLCAD) {
	/* adjust names of referenced object in any object that reference other objects */
	switch (ip.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		adjust_names(comb->tree, curr_dbip, name_map, used_names, cc_data);
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		extr = (struct rt_extrude_internal *)ip.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);

		new_name = get_new_name(extr->sketch_name, curr_dbip, name_map, used_names, cc_data);
		if (new_name.length()) {
		    bu_free(extr->sketch_name, "sketch name");
		    extr->sketch_name = bu_strdup(new_name.c_str());
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		dsp = (struct rt_dsp_internal *)ip.idb_ptr;
		RT_DSP_CK_MAGIC(dsp);

		if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ) {
		    /* This dsp references a database object, may need to change its name */
		    new_name = get_new_name(bu_vls_addr(&dsp->dsp_name), curr_dbip,
						name_map, used_names, cc_data);
		    if (new_name.length()) {
			bu_vls_free(&dsp->dsp_name);
			bu_vls_strcpy(&dsp->dsp_name, new_name.c_str());
		    }
		}
		break;
	}
    }

    new_name = get_new_name(input_dp->d_namep, curr_dbip, name_map, used_names, cc_data);
    if (!new_name.length()) {
	new_name = std::string(input_dp->d_namep);
    }
    if ((new_dp = db_diradd(curr_dbip, new_name.c_str(), RT_DIR_PHONY_ADDR, 0, input_dp->d_flags,
			    (void *)&input_dp->d_minor_type)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to add new object name (%s) to directory - aborting!!\n",
		      new_name.c_str());
	return GED_ERROR;
    }

    /* db_diradd doesn't produce the correct major type for binary objects -
     * make sure they match. */
    new_dp->d_major_type = input_dp->d_major_type;

    if (rt_db_put_internal(new_dp, curr_dbip, &ip, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      new_name.c_str());
	return GED_ERROR;
    }

    return GED_OK;
}


extern "C" int
ged_concat_core(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i *newdbp;
    struct directory *dp;
    std::unordered_map<std::string, std::string> name_map;
    std::set<std::string> used_names;
    struct bu_attribute_value_set g_avs;
    const char *cp;
    char *colorTab;
    struct ged_concat_data cc_data;
    const char *oldfile;
    static const char *usage = "[-u] [-t] [-c] [-s|-p] file.g [suffix|prefix]";
    int importUnits = 0;
    int importTitle = 0;
    int importColorTable = 0;
    int saveGlobalAttrs = 0;
    int c;
    const char *commandName;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_init(&cc_data.affix);
    cc_data.copy_mode = 0;
    commandName = argv[0];

    /* process args */
    bu_optind = 1;
    bu_opterr = 0;
    while ((c=bu_getopt(argc, (char * const *)argv, "utcsp")) != -1) {
	switch (c) {
	    case 'u':
		importUnits = 1;
		break;
	    case 't':
		importTitle = 1;
		break;
	    case 'c':
		importColorTable = 1;
		break;
	    case 'p':
		cc_data.copy_mode |= AUTO_PREFIX;
		break;
	    case 's':
		cc_data.copy_mode |= AUTO_SUFFIX;
		break;
	    default: {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", commandName, usage);
		    bu_vls_free(&cc_data.affix);
		    return GED_ERROR;
		}
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", commandName, usage);
	return GED_ERROR;
    }

    oldfile = argv[0];

    argc--;
    argv++;

    if (cc_data.copy_mode) {
	/* specified suffix or prefix explicitly */

	if (cc_data.copy_mode & AUTO_PREFIX) {

	    if (argc == 0 || BU_STR_EQUAL(argv[0], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[0]);
		cc_data.copy_mode |= CUSTOM_PREFIX;
	    }

	} else if (cc_data.copy_mode & AUTO_SUFFIX) {

	    if (argc == 0 || BU_STR_EQUAL(argv[0], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_SUFFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[0]);
		cc_data.copy_mode |= CUSTOM_SUFFIX;
	    }

	} else {
	    bu_vls_free(&cc_data.affix);
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", commandName, usage);
	    return GED_ERROR;
	}

    } else {
	/* no prefix/suffix preference, use prefix */

	cc_data.copy_mode |= AUTO_PREFIX;

	if (argc == 0 || BU_STR_EQUAL(argv[0], "/")) {
	    cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	} else {
	    (void)bu_vls_strcpy(&cc_data.affix, argv[0]);
	    cc_data.copy_mode |= CUSTOM_PREFIX;
	}

    }

    if (db_version(gedp->dbip) < 5) {
	if (bu_vls_strlen(&cc_data.affix) > _GED_V4_MAXNAME-1) {
	    bu_log("ERROR: affix [%s] is too long for v%d\n", bu_vls_addr(&cc_data.affix), db_version(gedp->dbip));
	    bu_vls_free(&cc_data.affix);
	    return GED_ERROR;
	}
    }

    /* open the input file */
    if ((newdbp = db_open(oldfile, DB_OPEN_READONLY)) == DBI_NULL) {
	bu_vls_free(&cc_data.affix);
	perror(oldfile);
	bu_vls_printf(gedp->ged_result_str, "%s: Can't open geometry database file %s", commandName, oldfile);
	return GED_ERROR;
    }

    if (db_version(newdbp) > 4 && db_version(gedp->dbip) < 5) {
	bu_vls_free(&cc_data.affix);
	bu_vls_printf(gedp->ged_result_str, "%s: databases are incompatible, use dbupgrade on %s first",
		      commandName, gedp->dbip->dbi_filename);
	return GED_ERROR;
    }

    db_dirbuild(newdbp);

    cc_data.new_dbip = newdbp;
    cc_data.old_dbip = gedp->dbip;

    /* visit each directory pointer in the input database */
    if (importUnits || importTitle || importColorTable) {
	saveGlobalAttrs = 1;
    }
    FOR_ALL_DIRECTORY_START(dp, newdbp) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    if (saveGlobalAttrs) {
		if (db5_get_attributes(newdbp, &g_avs, dp)) {
		    bu_vls_printf(gedp->ged_result_str, "%s: Can't get global attributes from %s", commandName, oldfile);
		    return GED_ERROR;
		}
	    }
	    continue;
	}
	copy_object(gedp, dp, newdbp, gedp->dbip, name_map, used_names, &cc_data);
    } FOR_ALL_DIRECTORY_END;

    bu_vls_free(&cc_data.affix);
    rt_mempurge(&(newdbp->dbi_freep));

    /* Free all the directory entries, and close the input database */
    db_close(newdbp);

    if (importColorTable) {
	if ((cp = bu_avs_get(&g_avs, "regionid_colortable")) != NULL) {
	    colorTab = bu_strdup(cp);
	    db5_import_color_table(colorTab);
	    bu_free(colorTab, "colorTab");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s: no region color table "
		    "was found in %s to import\n", commandName, oldfile);
	}
    } else if (saveGlobalAttrs) {
	bu_avs_remove(&g_avs, "regionid_colortable");
    }

    if (importTitle) {
	if ((cp = bu_avs_get(&g_avs, "title")) != NULL) {
	    char *oldTitle = gedp->dbip->dbi_title;
	    gedp->dbip->dbi_title = bu_strdup(cp);
	    if (oldTitle) {
		bu_free(oldTitle, "old title");
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str,
		    "%s: no title was found in %s to import\n", commandName,
		    oldfile);
	}
    } else if (saveGlobalAttrs) {
	bu_avs_remove(&g_avs, "title");
    }

    if (importUnits) {
	if ((cp = bu_avs_get(&g_avs, "units")) != NULL) {
	    double dd;
	    if (sscanf(cp, "%lf", &dd) != 1 || NEAR_ZERO(dd, VUNITIZE_TOL)) {
		bu_log("copy_object(%s): improper database, %s object attribute 'units'=%s is invalid\n",
		       oldfile, DB5_GLOBAL_OBJECT_NAME, cp);
		bu_avs_remove(&g_avs, "units");
	    } else {
		gedp->dbip->dbi_local2base = dd;
		gedp->dbip->dbi_base2local = 1 / dd;
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str,
		    "%s: no units were found in %s to import\n", commandName,
		    oldfile);
	}
    } else if (saveGlobalAttrs) {
	bu_avs_remove(&g_avs, "units");
    }

    if (saveGlobalAttrs) {
	dp = db_lookup(gedp->dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_NOISY);
	db5_update_attributes(dp, &g_avs, gedp->dbip);
    }

    db_sync(gedp->dbip);	/* force changes to disk */

    /* Update references. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl concat_cmd_impl = { "concat", ged_concat_core, GED_CMD_DEFAULT };
    const struct ged_cmd concat_cmd = { &concat_cmd_impl };

    struct ged_cmd_impl dbconcat_cmd_impl = { "dbconcat", ged_concat_core, GED_CMD_DEFAULT };
    const struct ged_cmd dbconcat_cmd = { &dbconcat_cmd_impl };


    const struct ged_cmd *concat_cmds[] = { &concat_cmd,  &dbconcat_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  concat_cmds, 2 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
