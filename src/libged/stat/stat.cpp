/*                       S T A T . C P P
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
/** @file stat.c
 *
 * Command for reporting information about geometry database
 * objects.
 *
 */

#include "common.h"

#include <stdio.h>

#include <set>
#include <sstream>

extern "C" {
#include "../alphanum.h"
}

#include "bu/cmdschema.h"
#include "bu/ptbl.h"
#include "bu/sort.h"
#include "bu/tbl.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "rt/directory.h"
#include "ged.h"

static size_t
_stat_find_first_unescaped(std::string &s, const char *keys, int offset)
{
    int off = offset;
    int done = 0;
    size_t candidate = std::string::npos;
    while (!done) {
	candidate = s.find_first_of(keys, off);
	if (!candidate || candidate == std::string::npos) return candidate;
	if (s.at(candidate - 1) == '\\') {
	    off = candidate + 1;
	} else {
	    done = 1;
	}
    }
    return candidate;
}

std::vector<std::string>
_stat_keys_split(std::string s)
{
    std::vector<std::string> substrs;
    std::string lstr = s;
    size_t pos = 0;
    while ((pos = _stat_find_first_unescaped(lstr, ",", 0)) != std::string::npos) {
	std::string ss = lstr.substr(0, pos);
	substrs.push_back(ss);
	lstr.erase(0, pos + 1);
    }
    substrs.push_back(lstr);
    return substrs;
}

static const char *
stat_key_prettyprint(const char *key)
{
    if (BU_STR_EQUAL(key, "flags")) {
	static const char *s = "Flags";
	return s;
    }
    if (BU_STR_EQUAL(key, "name")) {
	static const char *s = "Object Name";
	return s;
    }
    if (BU_STR_EQUAL(key, "refs")) {
	static const char *s = "References";
	return s;
    }
    if (BU_STR_EQUAL(key, "size")) {
	static const char *s = "Size";
	return s;
    }
    if (BU_STR_EQUAL(key, "type")) {
	static const char *s = "Type";
	return s;
    }

    return key;
}

struct cmp_dps_arg {
    struct db_i *dbip;
    std::vector<std::string> *keys;
};

static void
type_str(struct bu_vls *n, struct directory *dp, struct db_i *dbip)
{
    int type;
    struct rt_db_internal intern;
    const struct bn_tol arb_tol = BN_TOL_INIT_TOL;
    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	bu_vls_sprintf(n, " ");
	return;
    }

    if (dp->d_flags & RT_DIR_COMB) {
	if (dp->d_flags & RT_DIR_REGION) {
	    bu_vls_sprintf(n, "region");
	    return;
	} else {
	    bu_vls_sprintf(n, "comb");
	    return;
	}
    } else {
	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL) < 0 || intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	    rt_db_free_internal(&intern);
	    bu_vls_sprintf(n, " ");
	    return;
	}

	switch (intern.idb_minor_type) {
	    case DB5_MINORTYPE_BRLCAD_ARB8:
		type = rt_arb_std_type(&intern, &arb_tol);
		switch (type) {
		    case 4:
			bu_vls_sprintf(n, "arb4");
			break;
		    case 5:
			bu_vls_sprintf(n, "arb5");
			break;
		    case 6:
			bu_vls_sprintf(n, "arb6");
			break;
		    case 7:
			bu_vls_sprintf(n, "arb7");
			break;
		    case 8:
			bu_vls_sprintf(n, "arb8");
			break;
		    default:
			bu_vls_sprintf(n, "arb?");
			break;
		}
		break;
	    default:
		bu_vls_sprintf(n, "%s", intern.idb_meth->ft_label);
		break;
	}
    }

    rt_db_free_internal(&intern);
}

static int
cmp_dps(const void *a, const void *b, void *arg)
{
    int cmp = 0;
    struct directory *dp1 = *(struct directory **)a;
    struct directory *dp2 = *(struct directory **)b;
    struct cmp_dps_arg *sarg = (struct cmp_dps_arg *)arg;
    std::vector<std::string> *keys = sarg->keys;
    struct db_i *dbip = sarg->dbip;
    // Try each of the specified criteria until we get
    // a decision
    for (size_t i = 0; i < keys->size(); i++) {
	int flip = 1;
	struct bu_vls key = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&key, "%s", (*keys)[i].c_str());
	if (bu_vls_cstr(&key)[0] == '!') {
	    bu_vls_nibble(&key, 1);
	    flip = -1;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "name")) {
	    cmp = alphanum_impl((const char *)dp1->d_namep, (const char *)dp2->d_namep, arg);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "uses")) {
	    cmp = (dp1->d_uses > dp2->d_uses);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_uses < dp2->d_uses);
	    if (cmp) {
		bu_vls_free(&key);
		return -1 * flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "refs")) {
	    cmp = (dp1->d_nref > dp2->d_nref);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_nref < dp2->d_nref);
	    if (cmp) {
		bu_vls_free(&key);
		return -1 * flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "flags")) {
	    cmp = (dp1->d_flags > dp2->d_flags);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_flags < dp2->d_flags);
	    if (cmp) {
		bu_vls_free(&key);
		return -1 * flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "major_type")) {
	    cmp = (dp1->d_major_type > dp2->d_major_type);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_major_type < dp2->d_major_type);
	    if (cmp) {
		bu_vls_free(&key);
		return -1 * flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "minor_type")) {
	    cmp = (dp1->d_minor_type > dp2->d_minor_type);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_minor_type < dp2->d_minor_type);
	    if (cmp) {
		bu_vls_free(&key);
		return -1 * flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "type")) {
	    struct bu_vls n1 = BU_VLS_INIT_ZERO;
	    struct bu_vls n2 = BU_VLS_INIT_ZERO;
	    type_str(&n1, dp1, dbip);
	    type_str(&n2, dp2, dbip);
	    cmp = bu_strcmp(bu_vls_cstr(&n1), bu_vls_cstr(&n2));
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    continue;
	}
	if (BU_STR_EQUAL(bu_vls_cstr(&key), "size")) {
	    cmp = (dp1->d_len > dp2->d_len);
	    if (cmp) {
		bu_vls_free(&key);
		return flip * cmp;
	    }
	    cmp = (dp1->d_len < dp2->d_len);
	    if (cmp) {
		return -1 * flip * cmp;
	    }
	    continue;
	}

	// If we've gotten this far, we're after an attribute
	struct bu_attribute_value_set avs1 = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(dbip, &avs1, dp1)) {
	    continue;
	}
	struct bu_attribute_value_set avs2 = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(dbip, &avs2, dp2)) {
	    bu_avs_free(&avs1);
	    continue;
	}

	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&str, "%s", bu_vls_cstr(&key));
	/* attr: is a special key to allow a stat output to print arbitrary attribute values. */
	if (!bu_strncmp(bu_vls_cstr(&str), "attr:", 5)) {
	    bu_vls_nibble(&str, 5);
	}
	cmp = bu_strcmp(bu_avs_get(&avs1, bu_vls_cstr(&str)), bu_avs_get(&avs2, bu_vls_cstr(&str)));
	bu_avs_free(&avs1);
	bu_avs_free(&avs2);
	bu_vls_free(&str);
	if (cmp) {
	    bu_vls_free(&key);
	    return flip * cmp;
	}
    }

    return cmp;
}

void
dpath_sort(void *paths, int path_cnt, const char *col_order, struct ged *gedp)
{
    struct cmp_dps_arg sarg;
    std::vector<std::string> keys;
    sarg.dbip = gedp->dbip;
    sarg.keys = &keys;
    if (!strlen(col_order)) {
	keys.push_back(std::string("name"));
    } else {
	keys = _stat_keys_split(std::string(col_order));
    }
    bu_sort(paths, path_cnt, sizeof(struct directory *), cmp_dps, (void *)&sarg);
}

static void
stat_output(struct bu_tbl *table, struct ged *gedp, struct directory *dp, const char *key, int raw)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (BU_STR_EQUAL(key, "name")) {
	bu_tbl_write(table, dp->d_namep);
	return;
    }

    if (BU_STR_EQUAL(key, "uses")) {
	bu_vls_sprintf(&str, "%ld", dp->d_uses);
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "refs")) {
	bu_vls_sprintf(&str, "%ld", dp->d_nref);
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "flags")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_flags);
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "major_type")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_major_type);
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "minor_type")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_minor_type);
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }


    if (BU_STR_EQUAL(key, "type")) {
	struct bu_vls tstr = BU_VLS_INIT_ZERO;
	type_str(&tstr, dp, gedp->dbip);
	bu_tbl_write(table, bu_vls_cstr(&tstr));
	bu_vls_free(&tstr);
	return;
    }

    if (BU_STR_EQUAL(key, "size")) {
	if (raw) {
	    bu_vls_sprintf(&str, "%zd", dp->d_len);
	} else {
	    char hlen[6] = { '\0' };
	    (void)bu_humanize_number(hlen, 5, (int64_t)dp->d_len, "",
				     BU_HN_AUTOSCALE,
				     BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
	    bu_vls_printf(&str,  "%s", hlen);
	}
	bu_tbl_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }


    // If we've gotten this far, we're after an attribute
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_log("Error: cannot get attributes for object %s\n", dp->d_namep);
	return;
    }

    bu_vls_sprintf(&str, "%s", key);
    /* attr: is a prefix to allow a stat output to print arbitrary attribute values, even if
     * the attribute name matches one of the "defined" keys. */
    if (!bu_strncmp(key, "attr:", 5)) {
	bu_vls_nibble(&str, 5);
    }

    if (bu_avs_get(&avs, bu_vls_cstr(&str))) {
	bu_tbl_write(table, bu_avs_get(&avs, bu_vls_cstr(&str)));
    } else {
	bu_tbl_write(table, " ");
    }
    bu_avs_free(&avs);
    bu_vls_free(&str);
}

/*********************************************
 * Command function and user option handling *
 *********************************************/

struct stat_args {
    int print_help;
    long verbosity;
    long quiet;
    int raw;
    struct bu_vls search_filter;
    struct bu_vls sort_str;
    struct bu_vls keys_str;
    const char *output_file;
};


static const struct bu_cmd_arg_shape stat_filter_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_CUSTOM, 1, 1, "Search expression language");
static const struct bu_cmd_arg_shape stat_columns_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_COMMA_LIST, 1, 1, "Comma-separated column names");
static const struct bu_cmd_arg_shape stat_pattern_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_RANGE_PATTERN, 1, 1, "Database name or glob pattern");
static const char * const stat_column_keywords[] = {
    "name", "uses", "refs", "flags", "major_type", "minor_type", "type", "size", NULL
};
static const struct bu_cmd_option stat_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct stat_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbosity", struct stat_args, verbosity,
	"Increase output verbosity"),
    BU_CMD_COUNTING_LONG_FLAG("q", "quiet", struct stat_args, quiet,
	"Decrease output verbosity"),
    BU_CMD_FLAG("r", "raw", struct stat_args, raw, "Print raw values"),
    {"F", "filter", "filter", "expression", "Filter using a search expression",
	BU_CMD_VALUE_VLS, offsetof(struct stat_args, search_filter), NULL, NULL,
	"ged.search", NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, &stat_filter_shape, NULL, NULL},
    {"C", "columns", "columns", "columns", "Comma-separated output columns",
	BU_CMD_VALUE_VLS, offsetof(struct stat_args, keys_str), NULL, NULL, NULL,
	NULL, 0, 0, stat_column_keywords, BU_CMD_ARG_REQUIRED, &stat_columns_shape, NULL, NULL},
    {"S", "sort-order", "sort-order", "columns", "Comma-separated sort columns",
	BU_CMD_VALUE_VLS, offsetof(struct stat_args, sort_str), NULL, NULL, NULL,
	NULL, 0, 0, stat_column_keywords, BU_CMD_ARG_REQUIRED, &stat_columns_shape, NULL, NULL},
    BU_CMD_FILE("o", "output-file", struct stat_args, output_file, "file",
	"Write output to a new file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand stat_schema_operands[] = {
    BU_CMD_OPERAND_SHAPED("patterns", BU_CMD_VALUE_DB_PATH, 1,
	BU_CMD_COUNT_UNLIMITED, NULL, "Database names, paths, or glob patterns",
	"ged.db_path", &stat_pattern_shape),
    BU_CMD_OPERAND_NULL
};


static int
stat_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (!bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return 0;

    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_VALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_NONE;
    result->completion_type = BU_CMD_VALUE_FLAG;
    result->hint = "command help";
    return 0;
}


static const struct bu_cmd_schema stat_cmd_schema = {
    "stat", "Report tabular database object statistics", stat_schema_options,
    stat_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(stat_schema_validate, NULL)
};


static void
stat_usage(struct bu_vls *str, const char *cmd) {
    char *option_help = bu_cmd_schema_describe(&stat_cmd_schema);
    bu_vls_sprintf(str, "Usage: %s [options] pattern\n", cmd);
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
    bu_vls_printf(str, "Available column types are:\n");
    bu_vls_printf(str, "    name\n");
    bu_vls_printf(str, "    uses\n");
    bu_vls_printf(str, "    refs\n");
    bu_vls_printf(str, "    flags\n");
    bu_vls_printf(str, "    major_type\n");
    bu_vls_printf(str, "    minor_type\n");
    bu_vls_printf(str, "    type\n");
    bu_vls_printf(str, "    size\n");
    bu_vls_printf(str, "\nAttribute keys may also be specified.  If the user attribute matches one of the\ncolumn keys above, the user attribute may be accessed by prefixing it with the\nstring \"attr:\" (for example, \"attr:name\" would print the value of\n\"attr get objname name\", not \"objname\"\n");
}

int
ged_stat_core(struct ged *gedp, int argc, const char *argv[])
{
    if (!gedp || !argc || !argv)
	return BRLCAD_ERROR;

    struct stat_args args = {0, 1, 0, 0, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO,
	BU_VLS_INIT_ZERO, NULL};
    int operand_count;
    const char **operands;
    struct bu_ptbl sobjs = BU_PTBL_INIT_ZERO;
    FILE *fp = NULL;
    struct db_i *dbip = gedp->dbip;
    const char *pname = argv[0];

    // Clear result string
    bu_vls_trunc(gedp->ged_result_str, 0);

    int operand_index = bu_cmd_schema_parse_complete(&stat_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_free(&args.search_filter);
	bu_vls_free(&args.sort_str);
	bu_vls_free(&args.keys_str);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;
    operands = argv + operand_index + 1;

    if (args.print_help) {
	stat_usage(gedp->ged_result_str, pname);
	bu_vls_free(&args.search_filter);
	bu_vls_free(&args.sort_str);
	bu_vls_free(&args.keys_str);
	return GED_HELP;
    }

    // If we have one or a series of patterns, process
    // them - otherwise, report usage.
    if (!operand_count) {
	stat_usage(gedp->ged_result_str, pname);
	bu_vls_free(&args.search_filter);
	bu_vls_free(&args.sort_str);
	bu_vls_free(&args.keys_str);
	return BRLCAD_ERROR;
    }

    // If we have an output file specified, make sure it's not already there
    if (args.output_file) {
	int oret = 0;
	if (bu_file_exists(args.output_file, NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists, not overwriting", args.output_file);
	    oret = 1;
	}
	if (!oret) {
	    fp = fopen(args.output_file, "wb");
	    if (!fp) {
		bu_vls_printf(gedp->ged_result_str, "failed to open %s", args.output_file);
		oret = 1;
	    }
	}
	if (oret) {
	    bu_vls_free(&args.search_filter);
	    bu_vls_free(&args.sort_str);
	    bu_vls_free(&args.keys_str);
	    return BRLCAD_ERROR;
	}
    }

    if (!bu_vls_strlen(&args.keys_str)) {
	// No key set supplied - set defaults based on verbosity
	bu_vls_sprintf(&args.keys_str, "name,uses,refs,flags,type,size");
    }

    // Make sure our info is current
#if 0
    if (db_dirbuild(dbip) < 0) {
	bu_vls_printf(gedp->ged_result_str, "db_dirbuild failed on geometry database file\n");
	return BRLCAD_ERROR;
    }
#endif
    db_update_nref(dbip);

    // Combine verbosity and quiet flags
    args.verbosity = args.verbosity - args.quiet;

    /* Split the key string up into individual keys */
    std::vector<std::string> keys = _stat_keys_split(std::string(bu_vls_cstr(&args.keys_str)));

    // Create table
    struct bu_tbl *table = bu_tbl_create();
    bu_tbl_style(table, BU_TBL_STYLE_LIST);

    // Set header (depends on verbosity)
    for (size_t i = 0; i < keys.size(); i++) {
	// TODO - better pretty-printing of header strings - i.e. "Object Name" instead of "name"
	bu_tbl_write(table, stat_key_prettyprint(keys[i].c_str()));
    }
    bu_tbl_style(table, BU_TBL_ROW_END);
    bu_tbl_style(table, BU_TBL_ROW_SEPARATOR);

    // If we're going to filter, build the set of "allowed" objects
    if (bu_vls_strlen(&args.search_filter)) {
	int s_flags = 0;
	s_flags |= DB_SEARCH_RETURN_UNIQ_DP;
	(void)db_search(&sobjs, s_flags, bu_vls_cstr(&args.search_filter), 0, NULL, dbip, NULL, NULL, NULL);

	// If we're not allowed *any* objects according to the filters, there's no point in
	// doing any more work - just print the header and exit.
	if (!BU_PTBL_LEN(&sobjs)) {
	    struct bu_vls tstr = BU_VLS_INIT_ZERO;
	    bu_tbl_vls(&tstr, table);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&tstr));
	    bu_vls_free(&tstr);
	    bu_tbl_destroy(table);
	    bu_ptbl_free(&sobjs);
	    bu_vls_free(&args.keys_str);
	    bu_vls_free(&args.search_filter);
	    bu_vls_free(&args.sort_str);
	    return BRLCAD_OK;
	}
    }

    std::set<struct directory *> udp;

    for (int i = 0; i < operand_count; i++) {

	struct directory **paths = NULL;
	int path_cnt = db_ls(gedp->dbip, DB_LS_HIDDEN, operands[i], &paths);

	if (!paths) {
	    bu_log("WARNING:  path specifier %s does not match any geometry - skipping", operands[i]);
	    continue;
	}

	for (int j = 0; j < path_cnt; j++) {
	    struct directory *dp = paths[j];
	    if (BU_PTBL_LEN(&sobjs)) {
		// We have filtering enabled - check the object
		if (bu_ptbl_locate(&sobjs,  (long *)dp ) == -1) {
		    // skip
		    continue;
		}
	    }
	    udp.insert(dp);
	}

	bu_free(paths, "dp array");
    }

    // If we have any sorting enabled, do that
    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;
    std::set<struct directory *>::iterator o_it;
    for (o_it = udp.begin(); o_it != udp.end(); o_it++) {
	struct directory *dp = *o_it;
	bu_ptbl_ins(&objs,  (long *)dp);
    }
    dpath_sort((void *)objs.buffer, BU_PTBL_LEN(&objs), bu_vls_cstr(&args.sort_str), gedp);

    for (size_t j = 0; j < BU_PTBL_LEN(&objs); j++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(&objs, j);
	for (size_t k = 0; k < keys.size(); k++) {
	    stat_output(table, gedp, dp, keys[k].c_str(), args.raw);
	}
	bu_tbl_style(table, BU_TBL_ROW_END);
    }
    bu_ptbl_free(&objs);

    struct bu_vls tstr = BU_VLS_INIT_ZERO;
    bu_tbl_vls(&tstr, table);
    if (!fp) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&tstr));
    } else {
	fprintf(fp, "%s\n", bu_vls_cstr(&tstr));
	fclose(fp);
    }
    bu_vls_free(&tstr);
    bu_tbl_destroy(table);
    bu_ptbl_free(&sobjs);
    bu_vls_free(&args.keys_str);
    bu_vls_free(&args.search_filter);
    bu_vls_free(&args.sort_str);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_STAT_COMMANDS(X, XID) \
    XID(statcmd, "stat", ged_stat_core, GED_CMD_DEFAULT, &stat_cmd_schema)

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_STAT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_stat", 1, GED_STAT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
