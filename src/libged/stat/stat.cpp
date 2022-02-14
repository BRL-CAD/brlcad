/*                       S T A T . C P P
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
#include "fort.h"
#define ALPHANUM_IMPL
#include "../alphanum.h"
}

#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/sort.h"
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
    const struct bn_tol arb_tol = BG_TOL_INIT;
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
	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0 || intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
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
	db5_standardize_avs(&avs1);
	struct bu_attribute_value_set avs2 = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(dbip, &avs2, dp2)) {
	    bu_avs_free(&avs1);
	    continue;
	}
	db5_standardize_avs(&avs2);

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
stat_output(ft_table_t *table, struct ged *gedp, struct directory *dp, const char *key, int raw)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (BU_STR_EQUAL(key, "name")) {
	ft_write(table, dp->d_namep);
	return;
    }

    if (BU_STR_EQUAL(key, "uses")) {
	bu_vls_sprintf(&str, "%ld", dp->d_uses);
	ft_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "refs")) {
	bu_vls_sprintf(&str, "%ld", dp->d_nref);
	ft_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "flags")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_flags);
	ft_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "major_type")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_major_type);
	ft_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }

    if (BU_STR_EQUAL(key, "minor_type")) {
	// TODO - some sort of human intuitive printing
	bu_vls_sprintf(&str, "%d", dp->d_minor_type);
	ft_write(table, bu_vls_cstr(&str));
	bu_vls_free(&str);
	return;
    }


    if (BU_STR_EQUAL(key, "type")) {
	struct bu_vls tstr = BU_VLS_INIT_ZERO;
	type_str(&tstr, dp, gedp->dbip);
	ft_write(table, bu_vls_cstr(&tstr));
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
	ft_write(table, bu_vls_cstr(&str));
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
	ft_write(table, bu_avs_get(&avs, bu_vls_cstr(&str)));
    } else {
	ft_write(table, " ");
    }
    bu_avs_free(&avs);
    bu_vls_free(&str);
}

/*********************************************
 * Command function and user option handling *
 *********************************************/

static void
stat_usage(struct bu_vls *str, const char *cmd, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
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

    int print_help = 0;;
    long verbosity = 1;
    long quiet = 0;
    int raw = 0;
    struct bu_ptbl sobjs = BU_PTBL_INIT_ZERO;
    struct bu_vls search_filter = BU_VLS_INIT_ZERO;
    struct bu_vls sort_str = BU_VLS_INIT_ZERO;
    struct bu_vls keys_str = BU_VLS_INIT_ZERO;
    struct bu_vls ofile = BU_VLS_INIT_ZERO;
    FILE *fp = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct db_i *dbip = gedp->dbip;
    const char *pname = argv[0];

    // Stashed command name, increment and continue
    argc--;argv++;

    // Clear result string
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h", "help",       "",            NULL,              &print_help,    "Print help and exit");
    BU_OPT(d[1], "?", "",           "",            NULL,              &print_help,    "");
    BU_OPT(d[2], "v", "verbosity",  "",            &bu_opt_incr_long, &verbosity,     "Increase output verbosity (multiple specifications of -v increase verbosity more)");
    BU_OPT(d[3], "q", "quiet",      "",            &bu_opt_incr_long, &quiet,         "Decrease output verbosity (multiple specifications of -q decrease verbosity more)");
    BU_OPT(d[3], "r", "raw",       "",             NULL ,             &raw,           "Print raw values instead of human friendly values");
    BU_OPT(d[4], "F", "filter",     "\"string\"",  &bu_opt_vls,       &search_filter, "Filter objects being reported (uses search style filter specifications)");
    BU_OPT(d[5], "C", "columns",       "\"type1[,type2]...\"",  &bu_opt_vls,       &keys_str,      "Comma separated list of data columns to print");
    BU_OPT(d[6], "S", "sort-order",       "\"type1[,type2]...\"",  &bu_opt_vls,       &sort_str,      "Comma separated list of cols to sort by (priority is left to right).  To reverse sorting order for an individual column, prefix the specifier with a '!' character.");
    BU_OPT(d[7], "o", "output-file",    "filename",  &bu_opt_vls,       &ofile,      "Write output to file");
    BU_OPT_NULL(d[8]);

    int ret_ac = bu_opt_parse(&msg, argc, argv, d);
    if (ret_ac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	bu_vls_free(&search_filter);
	bu_vls_free(&sort_str);
	bu_vls_free(&keys_str);
	bu_vls_free(&ofile);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&msg);

    if (print_help) {
	stat_usage(gedp->ged_result_str, pname, d);
	bu_vls_free(&search_filter);
	bu_vls_free(&sort_str);
	bu_vls_free(&keys_str);
	bu_vls_free(&ofile);
	return BRLCAD_HELP;
    }

    // If we have one or a series of patterns, process
    // them - otherwise, report usage.
    argc = ret_ac;
    if (!argc) {
	stat_usage(gedp->ged_result_str, pname, d);
	bu_vls_free(&search_filter);
	bu_vls_free(&sort_str);
	bu_vls_free(&keys_str);
	bu_vls_free(&ofile);
	return BRLCAD_ERROR;
    }

    // If we have an output file specified, make sure it's not already there
    if (bu_vls_strlen(&ofile)) {
	int oret = 0;
	if (bu_file_exists(bu_vls_cstr(&ofile), NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists, not overwriting", bu_vls_cstr(&ofile));
	    oret = 1;
	}
	if (!oret) {
	    fp = fopen(bu_vls_cstr(&ofile), "wb");
	    if (!fp) {
		bu_vls_printf(gedp->ged_result_str, "failed to open %s", bu_vls_cstr(&ofile));
		oret = 1;
	    }
	}
	if (oret) {
	    bu_vls_free(&search_filter);
	    bu_vls_free(&sort_str);
	    bu_vls_free(&keys_str);
	    bu_vls_free(&ofile);
	    return BRLCAD_ERROR;
	}
    }

    if (!bu_vls_strlen(&keys_str)) {
	// No key set supplied - set defaults based on verbosity
	bu_vls_sprintf(&keys_str, "name,uses,refs,flags,type,size");
    }

    // Make sure our info is current
#if 0
    if (db_dirbuild(dbip) < 0) {
	bu_vls_printf(gedp->ged_result_str, "db_dirbuild failed on geometry database file\n");
	return BRLCAD_ERROR;
    }
#endif
    db_update_nref(dbip, &rt_uniresource);

    // Combine verbosity and quiet flags
    verbosity = verbosity - quiet;

    /* Split the key string up into individual keys */
    std::vector<std::string> keys = _stat_keys_split(std::string(bu_vls_cstr(&keys_str)));

    // Create table
    ft_table_t *table = ft_create_table();
    ft_set_border_style(table, FT_SIMPLE_STYLE);

    // Set header (depends on verbosity)
    for (size_t i = 0; i < keys.size(); i++) {
	// TODO - better pretty-printing of header strings - i.e. "Object Name" instead of "name"
	ft_write(table, stat_key_prettyprint(keys[i].c_str()));
    }
    ft_ln(table);
    ft_add_separator(table);

    // If we're going to filter, build the set of "allowed" objects
    if (bu_vls_strlen(&search_filter)) {
	int s_flags = 0;
	s_flags |= DB_SEARCH_RETURN_UNIQ_DP;
	(void)db_search(&sobjs, s_flags, bu_vls_cstr(&search_filter), 0, NULL, dbip, NULL);

	// If we're not allowed *any* objects according to the filters, there's no point in
	// doing any more work - just print the header and exit.
	if (!BU_PTBL_LEN(&sobjs)) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", ft_to_string(table));
	    ft_destroy_table(table);
	    bu_ptbl_free(&sobjs);
	    return BRLCAD_OK;
	}
    }

    std::set<struct directory *> udp;

    for (int i = 0; i < argc; i++) {

	struct directory **paths;
	int path_cnt = db_ls(gedp->dbip, DB_LS_HIDDEN, argv[i], &paths);


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
    dpath_sort((void *)objs.buffer, BU_PTBL_LEN(&objs), bu_vls_cstr(&sort_str), gedp);

    for (size_t j = 0; j < BU_PTBL_LEN(&objs); j++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(&objs, j);
	for (size_t k = 0; k < keys.size(); k++) {
	    stat_output(table, gedp, dp, keys[k].c_str(), raw);
	}
	ft_ln(table);
    }
    bu_ptbl_free(&objs);

    if (!fp) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", ft_to_string(table));
    } else {
	fprintf(fp, "%s\n", ft_to_string(table));
	fclose(fp);
    }
    ft_destroy_table(table);
    bu_ptbl_free(&sobjs);

    return BRLCAD_OK;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
struct ged_cmd_impl stat_cmd_impl = {
    "stat",
    ged_stat_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd stat_cmd = { &stat_cmd_impl };
const struct ged_cmd *stat_cmds[] = { &stat_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  stat_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
}
#endif /* GED_PLUGIN */



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
