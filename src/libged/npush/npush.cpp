/*                         P U S H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/push.c
 *
 * The push command.
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"

/* When it comes to push operations, the notion of what constitutes a unique
 * instance is defined as the combination of the object and its associated
 * parent matrix. Because the dp may be used under multiple matrices, for some
 * push operations it will be necessary to generate a new unique name to refer
 * to instances - we store those names with the instances for convenience. */
class dp_i {
    public:
	struct directory *dp;
	mat_t mat;
	std::string iname;
	const struct bn_tol *ts_tol;

	bool operator<(const dp_i &o) const {
	    if (dp < o.dp) return true;
	    if (o.dp < dp) return false;
	    /* If the dp doesn't tell us, check the matrix. */
	    if (!bn_mat_is_equal(mat, o.mat, ts_tol)) {
		for (int i = 0; i < 16; i++) {
		    if (mat[i] < o.mat[i]) {
			return true;
		    }
		}
	    }
	    return false;
	}
};

/* Container to hold information during tree walk.  To generate names (or
 * recognize when a push would create a conflict) we keep track of how many
 * times we have encountered each dp during processing. */
struct push_state {
    std::set<dp_i> s_i;
    std::map<struct directory *, int> s_c;
};


void comb_cnt(struct db_i *UNUSED(dbip), struct directory *dp, void *data)
{
    struct push_state *s = (struct push_state *)data;
    s->s_c[dp]++;
    bu_log("Visiting comb: %s (%d)\n", dp->d_namep, s->s_c[dp]);
}

void solid_cnt(struct db_i *UNUSED(dbip), struct directory *dp, void *data)
{
    struct push_state *s = (struct push_state *)data;
    s->s_c[dp]++;
    bu_log("Visiting solid: %s (%d)\n", dp->d_namep, s->s_c[dp]);
}



static void
npush_usage(struct bu_vls *str, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: npush [options] obj\n");
    bu_vls_printf(str, "\nPushes position/rotation matrices 'down' the tree hierarchy, altering existing geometry as needed.  Default behavior clears all matrices from tree, unless push requires creation of new geometry objects.\n\n");
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}


extern "C" int
ged_npush_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int xpush = 0;
    int to_regions = 0;
    int to_solids = 0;
    int max_depth = 0;
    struct bu_opt_desc d[8];
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,  "Print help and exit");
    BU_OPT(d[1], "?", "",          "",   NULL,         &print_help,  "");
    BU_OPT(d[2], "f", "force",     "",   NULL,         &xpush,       "Create new objects if needed to push matrices (xpush)");
    BU_OPT(d[3], "x", "xpush",     "",   NULL,         &xpush,       "");
    BU_OPT(d[4], "r", "regions",   "",   NULL,         &to_regions,  "Halt push at regions (matrix will be above region reference)");
    BU_OPT(d[5], "s", "solids",    "",   NULL,         &to_solids,   "Halt push at solids (matrix will be above solid reference)");
    BU_OPT(d[6], "d", "max-depth", "",   &bu_opt_int,  &max_depth,   "Halt at depth # from tree root (matrix will be above item # layers deep)");

    BU_OPT_NULL(d[7]);

    /* Skip command name */
    argc--; argv++;

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (!argc || print_help) {
	struct bu_vls npush_help = BU_VLS_INIT_ZERO;
	npush_usage(&npush_help, d);
	bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&npush_help));
	bu_vls_free(&npush_help);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct push_state s;
    for (int i = 0; i < argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_NOISY);
	if (dp != RT_DIR_NULL) {
	    db_functree(dbip, dp, comb_cnt, solid_cnt, &rt_uniresource, &s);
	}
    }

    return GED_OK;
}

extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl npush_cmd_impl = {
    "npush",
    ged_npush_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd npush_cmd = { &npush_cmd_impl };
const struct ged_cmd *npush_cmds[] = { &npush_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  npush_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
