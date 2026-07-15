/*                         C O N C A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#include <unordered_set>
#include <unordered_map>

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "../../librt/librt_private.h"

#include "../ged_private.h"

/**
 * structure used by the dbconcat command for keeping track of where objects
 * are being copied to and from, and what type of affix to use.
 */
struct ged_concat_data {
    struct db_i *incoming_dbip = NULL;
    struct db_i *target_dbip = NULL;
    std::string affix;
    int prefix = 0;
    int suffix = 0;
    int lazy_affix = 0;
    int overwrite = 0;
    long int overwritten = 0;
    int use_ctbl = 0;
    int use_title = 0;
    int use_units = 0;
    std::unordered_map<std::string, std::string> name_map;
    std::unordered_set<std::string> used_names;
};

struct concat_args {
    int print_help;
    int overwrite;
    int use_ctbl;
    int prefix;
    int suffix;
    int lazy_affix;
    int use_title;
    int use_units;
};

static const struct bu_cmd_option concat_options[] = {
    BU_CMD_FLAG("h", "help", struct concat_args, print_help, "Print help and exit"),
    BU_CMD_FLAG("O", "overwrite", struct concat_args, overwrite,
	"Overwrite existing objects if names conflict"),
    BU_CMD_FLAG("c", NULL, struct concat_args, use_ctbl,
	"Use the incoming region color table"),
    BU_CMD_FLAG("p", "prefix", struct concat_args, prefix,
	"Apply the affix to the beginning of each object name"),
    BU_CMD_FLAG("s", "suffix", struct concat_args, suffix,
	"Apply the affix to the end of each object name"),
    BU_CMD_FLAG("L", "lazy-affix", struct concat_args, lazy_affix,
	"Apply the affix only when resolving name conflicts"),
    BU_CMD_FLAG("t", NULL, struct concat_args, use_title,
	"Use the incoming database title"),
    BU_CMD_FLAG("u", NULL, struct concat_args, use_units,
	"Use the incoming database units"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand concat_operands[] = {
    BU_CMD_OPERAND("database_file", BU_CMD_VALUE_FILE, 1, 1,
	"Incoming geometry database", "ged.file_path"),
    BU_CMD_OPERAND("affix", BU_CMD_VALUE_STRING, 0, 1,
	"Object-name prefix or suffix", NULL),
    BU_CMD_OPERAND_NULL
};
static const char *concat_affix_position_options[] = {"prefix", "suffix", NULL};
static const struct bu_cmd_constraint concat_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(concat_affix_position_options, 0, 1,
	"-p and -s are mutually exclusive"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema concat_cmd_schema = {
    "concat", "Merge another geometry database", concat_options, concat_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, concat_constraints)
};
static const struct bu_cmd_schema dbconcat_cmd_schema = {
    "dbconcat", "Merge another geometry database", concat_options, concat_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, concat_constraints)
};

static int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    if (db_lookup(dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

static int
_cc_uniq_test(struct bu_vls *n, void *data)
{
    struct ged_concat_data *cc_data = (struct ged_concat_data *)data;
    // If it's in the target database, it's not unique
    if (db_lookup(cc_data->target_dbip, bu_vls_cstr(n), LOOKUP_QUIET) != RT_DIR_NULL)
	return 0;
    // If it's not in the target database, check it against used_names
    if (cc_data->used_names.find(std::string(bu_vls_cstr(n))) != cc_data->used_names.end())
	return 0;
    // Yep, it's unique
    return 1;
}

/**
 * find a new unique name given a list of previously used names, and
 * the type of naming mode that described what type of affix to use.
 */
static int
uniq_name(const char *name, struct ged_concat_data *cc_data)
{
    BU_ASSERT(cc_data);
    std::unordered_map<std::string, std::string> *name_map = &cc_data->name_map;
    std::unordered_set<std::string> *used_names = &cc_data->used_names;

    if (!name)
	bu_log("WARNING: encountered NULL name, renaming to \"UNKNOWN\"\n");
    const char *orig_name = (name) ? name : "UNKNOWN";
    struct bu_vls iname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&iname, "%s", orig_name);
    std::string nname = std::string(bu_vls_cstr(&iname));

    // If we already have a mapping assigned for this object name.
    if (name_map->find(nname) != name_map->end()) {
	bu_vls_free(&iname);
	return BRLCAD_OK;
    }

    // If we have a non-empty affix and auto_prefix is set, every name gets
    // the same prefix applied.  Same for suffix
    bool affix_applied = false;
    if (!cc_data->lazy_affix && cc_data->affix.length()) {
	if (cc_data->suffix) {
	    bu_vls_printf(&iname, "%s", cc_data->affix.c_str());
	} else {
	    bu_vls_prepend(&iname, cc_data->affix.c_str());
	}
	affix_applied = true;
    }

    // Check the currently proposed name against the target database - if there
    // is no conflict, we're done.
    nname = std::string(bu_vls_cstr(&iname));
    struct directory *ndp = db_lookup(cc_data->target_dbip, bu_vls_cstr(&iname), LOOKUP_QUIET);
    if (!ndp) {
	(*name_map)[(name) ? std::string(name) : std::string("UNKNOWN")] = nname;
	used_names->insert(nname);
	bu_vls_free(&iname);
	return BRLCAD_OK;
    }

    // Conflict.  Rework the names to allow for automatic incrementing, and
    // find a unique name.
    bu_vls_sprintf(&iname, "%s", nname.c_str());
    if (cc_data->affix.size() && !affix_applied) {
	// We have an affix, but we weren't forcibly told to apply it to
	// everything - see if that'll do the job
	if (!cc_data->suffix) {
	    bu_vls_prepend(&iname, cc_data->affix.c_str());
	} else {
	    bu_vls_printf(&iname, "%s", cc_data->affix.c_str());
	}
	ndp = db_lookup(cc_data->target_dbip, bu_vls_cstr(&iname), LOOKUP_QUIET);
	if (!ndp) {
	    nname = std::string(bu_vls_cstr(&iname));
	    (*name_map)[(name) ? std::string(name) : std::string("UNKNOWN")] = nname;
	    used_names->insert(nname);
	    bu_vls_free(&iname);
	    return BRLCAD_OK;
	}
    }

    // Nope, not enough.
    const char *rx = NULL;
    const char *prx = "[!0-9]*([0-9+]).*";
    bu_vls_sprintf(&iname, "%s", orig_name);
    if (cc_data->suffix) {
	rx = NULL; // default incr behavior is to go for the suffix.
	bu_vls_printf(&iname, "_0");
	if (cc_data->affix.length())
	    bu_vls_printf(&iname, "%s", cc_data->affix.c_str());
    } else {
	rx = prx;
	bu_vls_prepend(&iname, "0_");
	if (cc_data->affix.length())
	    bu_vls_prepend(&iname, cc_data->affix.c_str());
    }
    // Increment until we get something usable
    if (bu_vls_incr(&iname, rx, NULL, &_cc_uniq_test, (void *)cc_data) < 0) {
	bu_vls_free(&iname);
	return BRLCAD_ERROR;
    }

    nname = std::string(bu_vls_cstr(&iname));
    (*name_map)[(name) ? std::string(name) : std::string("UNKNOWN")] = nname;
    used_names->insert(nname);

    bu_vls_free(&iname);
    return BRLCAD_OK;
}


static void
adjust_names(union tree *trp,
	struct ged_concat_data *cc_data)
{
    /* If we are overwriting conflicts, there's nothing to adjust */
    if (cc_data->overwrite)
	return;

    std::string old_name;
    std::string new_name;
    std::unordered_map<std::string, std::string> &name_map = cc_data->name_map;

    if (trp == NULL)
	return;

    switch (trp->tr_op) {
	case OP_DB_LEAF:
	    old_name = std::string(trp->tr_l.tl_name);
	    new_name = name_map[old_name];
	    if (old_name != new_name) {
		bu_free(trp->tr_l.tl_name, "leaf name");
		trp->tr_l.tl_name = bu_strdup(new_name.c_str());
	    }
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    adjust_names(trp->tr_b.tb_left, cc_data);
	    adjust_names(trp->tr_b.tb_right, cc_data);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    adjust_names(trp->tr_b.tb_left, cc_data);
	    break;
    }
}


static int
copy_object(struct ged *gedp,
	struct directory *input_dp,
	struct ged_concat_data *cc_data)
{
    struct rt_db_internal ip;
    struct rt_extrude_internal *extr;
    struct rt_dsp_internal *dsp;
    struct rt_comb_internal *comb;
    struct directory *new_dp;
    std::string new_name;
    struct directory* oride_dp = NULL;
    std::string owrite_backup;

    if (rt_db_get_internal(&ip, input_dp, cc_data->incoming_dbip, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"Failed to get internal form of object (%s) - aborting!!!\n",
		input_dp->d_namep);
	return BRLCAD_ERROR;
    }

    std::string old_name;
    if (ip.idb_major_type == DB5_MAJORTYPE_BRLCAD) {
	/* adjust names of referenced object in any object that reference other objects */
	switch (ip.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		adjust_names(comb->tree, cc_data);
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		extr = (struct rt_extrude_internal *)ip.idb_ptr;
		RT_EXTRUDE_CK_MAGIC(extr);
		old_name = std::string(extr->sketch_name);
		new_name = cc_data->name_map[old_name];
		if (new_name != old_name) {
		    bu_free(extr->sketch_name, "sketch name");
		    extr->sketch_name = bu_strdup(new_name.c_str());
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		dsp = (struct rt_dsp_internal *)ip.idb_ptr;
		RT_DSP_CK_MAGIC(dsp);

		if (dsp->dsp_datasrc == RT_DSP_SRC_OBJ) {
		    /* This dsp references a database object, may need to change its name */
		    old_name = std::string(bu_vls_cstr(&dsp->dsp_name));
		    new_name = cc_data->name_map[old_name];
		    if (new_name.length()) {
			bu_vls_free(&dsp->dsp_name);
			bu_vls_strcpy(&dsp->dsp_name, new_name.c_str());
		    }
		}
		break;
	}
    }

    if (cc_data->overwrite) {
	new_name = std::string(input_dp->d_namep);
    } else {
	new_name = cc_data->name_map[std::string(input_dp->d_namep)];
	if (!new_name.length()) {
	    bu_log("Error - no mapped name for %s\n", input_dp->d_namep);
	    return BRLCAD_ERROR;
	}
    }

    /* if overwriting -
     * move current to temp name before copying to avoid name collisions and data loss */
    if (cc_data->overwrite) {
	oride_dp = db_lookup(gedp->dbip, input_dp->d_namep, 0);
	if (oride_dp) { /* obj exists and needs to be moved out of the way */
	    struct bu_vls bname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&bname, "%s.bak", input_dp->d_namep);
	    if (bu_vls_incr(&bname, NULL, NULL, &_db_uniq_test, (void *)cc_data->target_dbip) < 0) {
		owrite_backup = std::string(input_dp->d_namep) + std::string(".bak");
	    } else {
		owrite_backup = std::string(bu_vls_cstr(&bname));
	    }
	    bu_vls_free(&bname);
	    db_rename(cc_data->target_dbip, oride_dp, owrite_backup.c_str());
	}
    }

    if ((new_dp = db_diradd(cc_data->target_dbip, new_name.c_str(), RT_DIR_PHONY_ADDR, 0, input_dp->d_flags,
		    (void *)&input_dp->d_minor_type)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str,
		"Failed to add new object name (%s) to directory - aborting!!\n",
		new_name.c_str());
	return BRLCAD_ERROR;
    }

    /* db_diradd doesn't produce the correct major type for binary objects -
     * make sure they match. */
    new_dp->d_major_type = input_dp->d_major_type;

    if (rt_db_put_internal(new_dp, cc_data->target_dbip, &ip) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"Failed to write new object (%s) to database - aborting!!\n",
		new_name.c_str());
	return BRLCAD_ERROR;
    }

    /* if overwriting -
     * we've added the new object, its safe to delete the original */
    if (oride_dp) {
	_dl_eraseAllNamesFromDisplay(gedp, owrite_backup.c_str(), 0);
	if (db_delete(gedp->dbip, oride_dp) != 0 || db_dirdelete(gedp->dbip, oride_dp) != 0) {
	    /* Abort processing on first error */
	    bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s\n", owrite_backup.c_str());
	    return BRLCAD_ERROR;
	}
	db_update_nref(gedp->dbip);
	cc_data->overwritten++;
    }

    return BRLCAD_OK;
}


static void
concat_show_help(struct ged *gedp, const char *command, const char *usage,
	const struct bu_cmd_schema *schema)
{
    char *option_help = bu_cmd_schema_describe(schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", command, usage);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "concat native option help");
    }
}


extern "C" int
ged_concat_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct ged_concat_data cc_data;
    struct concat_args options = {};
    const char *commandName = argv[0];
    const struct bu_cmd_schema *schema = BU_STR_EQUAL(commandName, "dbconcat") ?
	&dbconcat_cmd_schema : &concat_cmd_schema;
    int operand_index = 0;
    int operand_count = 0;
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;

    static const char *usage = "[options] file.g [affix]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* Make sure we're v5 */
    if (db_version(gedp->dbip) < 5) {
	bu_log("ERROR: %s does not support v%d files as import destinations.\n", commandName, db_version(gedp->dbip));
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    operand_index = bu_cmd_schema_parse(schema, &options, gedp->ged_result_str,
	argc - 1, argv + 1);
    if (operand_index < 0) {
	concat_show_help(gedp, commandName, usage, schema);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;

    if (options.print_help) {
	concat_show_help(gedp, commandName, usage, schema);
	return BRLCAD_OK;
    }

    if (bu_cmd_schema_validate(schema, (size_t)(argc - 1), argv + 1,
	    (size_t)(argc - 1), &validation) != 0 ||
	validation.state != BU_CMD_VALIDATE_VALID) {
	if (validation.hint && validation.hint[0])
	    bu_vls_printf(gedp->ged_result_str, "%s\n", validation.hint);
	bu_cmd_validate_result_clear(&validation);
	concat_show_help(gedp, commandName, usage, schema);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&validation);
    argv += operand_index + 1;
    argc = operand_count;
    cc_data.overwrite = options.overwrite;
    cc_data.use_ctbl = options.use_ctbl;
    cc_data.prefix = options.prefix;
    cc_data.suffix = options.suffix;
    cc_data.lazy_affix = options.lazy_affix;
    cc_data.use_title = options.use_title;
    cc_data.use_units = options.use_units;

    /* Current database is the target */
    cc_data.target_dbip = gedp->dbip;

    /* Open the incoming .g file */
    const char *incoming_file = argv[0];
    if ((cc_data.incoming_dbip = db_open(incoming_file, DB_OPEN_READONLY)) == DBI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Can't open geometry database file %s", commandName, incoming_file);
	return BRLCAD_ERROR;
    }
    if (db_version(cc_data.incoming_dbip) != db_version(gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "%s: databases are incompatible, use dbupgrade on %s first", commandName, incoming_file);
	return BRLCAD_ERROR;
    }

    db_dirbuild(cc_data.incoming_dbip);

    // If we have an affix, set it
    if (argc > 1)
	cc_data.affix = std::string(argv[1]);

    // For compatibility (and because '/' isn't a sane character to use for obj
    // names in any case) clear if such a character was supplied.
    if (argc > 1 && BU_STR_EQUAL(argv[1], "/"))
	cc_data.affix = std::string("");

    // For all incoming objects, compare their names against the current
    // database.  In case of any collisions, generate a new unique name based
    // on the options.  If we can't do this successfully, we can't concat the
    // databases - do it before making any data changes.
    FOR_ALL_DIRECTORY_START(dp, cc_data.incoming_dbip) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY)
	    continue;
	if (uniq_name(dp->d_namep, &cc_data) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str, "Name mapping failed for %s, aborting", dp->d_namep);
	    return BRLCAD_ERROR;
	}
    } FOR_ALL_DIRECTORY_END;


    /* Handle the metadata options first */
    if (cc_data.use_ctbl || cc_data.use_title || cc_data.use_units) {
	struct db_i *t_dbip = cc_data.target_dbip;
	struct db_i *i_dbip = cc_data.incoming_dbip;
	const char *i_fname = i_dbip->dbi_filename;
	const char *t_fname = t_dbip->dbi_filename;
	struct directory *tglobal_dp = db_lookup(t_dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_NOISY);
	struct directory *iglobal_dp = db_lookup(i_dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_NOISY);
	if (!tglobal_dp) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Can't get global attributes from %s", commandName, t_fname);
	    return BRLCAD_ERROR;
	}
	if (!iglobal_dp) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Can't get global attributes from %s", commandName, i_fname);
	    return BRLCAD_ERROR;
	}

	if (cc_data.use_units || cc_data.use_title) {
	    double l2mm = (cc_data.use_units) ? i_dbip->dbi_local2base : t_dbip->dbi_local2base;
	    const char *title = (cc_data.use_title) ? i_dbip->dbi_title : t_dbip->dbi_title;
	    if (db_update_ident(t_dbip, title, l2mm) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: db_update_ident failed (%s)", commandName, i_fname);
		return BRLCAD_ERROR;
	    }
	}
	if (cc_data.use_ctbl) {
	    struct bu_attribute_value_set g_avs = BU_AVS_INIT_ZERO;
	    db5_get_attributes(i_dbip, &g_avs, iglobal_dp);
	    const char *cp = bu_avs_get(&g_avs, "regionid_colortable");
	    if (!cp) {
		bu_vls_printf(gedp->ged_result_str, "%s: Can't get regionid_colortable from %s", commandName, i_fname);
		bu_avs_free(&g_avs);
		return BRLCAD_ERROR;
	    }
	    char *colorTab = bu_strdup(cp);
	    struct bu_attribute_value_set t_avs = BU_AVS_INIT_ZERO;
	    db5_get_attributes(t_dbip, &t_avs, tglobal_dp);
	    bu_avs_add(&t_avs, "regionid_colortable", colorTab);
	    db5_import_color_table(gedp->dbip, colorTab);
	    db5_update_attributes(tglobal_dp, &t_avs, gedp->dbip);
	    bu_free(colorTab, "colorTab");
	    bu_avs_free(&g_avs);
	    bu_avs_free(&t_avs);
	}
    }

    /* copy each directory pointer in the input database */
    FOR_ALL_DIRECTORY_START(dp, cc_data.incoming_dbip) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY)
	    continue;
	copy_object(gedp, dp, &cc_data);
    } FOR_ALL_DIRECTORY_END;

    rt_mempurge(&(cc_data.incoming_dbip->i->dbi_freep));

    /* Free all the directory entries, and close the incoming database */
    db_close(cc_data.incoming_dbip);

    if (cc_data.overwrite) {
	bu_vls_printf(gedp->ged_result_str, "    [%ld] objects overwritten", cc_data.overwritten);
    }

    /* force changes to disk */
    db_sync(cc_data.target_dbip);

    /* Update references. */
    db_update_nref(gedp->dbip);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_CONCAT_COMMANDS(X, XID) \
    X(concat, ged_concat_core, GED_CMD_DEFAULT, &concat_cmd_schema) \
    X(dbconcat, ged_concat_core, GED_CMD_DEFAULT, &dbconcat_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_CONCAT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_concat", 1, GED_CONCAT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
