/*                         M A K E . C
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
/** @file libged/make.c
 *
 * The make command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "bu/opt.h"
#include "bu/interrupt.h"
#include "rt/func.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"


static void
print_usage(struct ged *gedp, const char *cmd, struct bu_opt_desc* dtable)
{
    struct bu_vls types = BU_VLS_INIT_ZERO;
    char* opts = bu_opt_describe(dtable, NULL);

    rt_obj_make_labels(&types, "|");
    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] name <%s>\n", cmd, bu_vls_cstr(&types));

    if (opts) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", opts);
	bu_free(opts, "opt help");
    }

    bu_vls_free(&types);
}


int
ged_make_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const char *cmd = argv[0];
    const char *name;
    const char *type;

    int uac = 0;
    int print_help = 0;
    int list_types = 0;
    fastf_t scale = 1.0;
    point_t origin = VINIT_ZERO;

    struct rt_db_internal internal;
    struct bu_vls omsg = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[9];
    BU_OPT(d[0], "o", "",       "x y z", &bu_opt_vect_t,  &origin,     "Origin point for the new object");
    BU_OPT(d[1], "O", "",       "x y z", &bu_opt_vect_t,  &origin,     "");
    BU_OPT(d[2], "s", "",       "sf",    &bu_opt_fastf_t, &scale,      "Scale factor for the new object");
    BU_OPT(d[3], "S", "",       "sf",    &bu_opt_fastf_t, &scale,      "");
    BU_OPT(d[4], "t", "",       "",      NULL,            &list_types, "List the primitive types that can be made");
    BU_OPT(d[5], "T", "",       "",      NULL,            &list_types, "");
    BU_OPT(d[6], "h", "help",   "",      NULL,            &print_help, "Print help and exit");
    BU_OPT(d[7], "H", "",       "",      NULL,            &print_help, "");
    BU_OPT_NULL(d[8]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	print_usage(gedp, cmd, d);
	return GED_HELP;
    }

    /* Process arguments (skip the command name) */
    argc--; argv++;
    uac = bu_opt_parse(&omsg, argc, argv, d);
    if (uac < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&omsg);

    if (print_help) {
	print_usage(gedp, cmd, d);
	return GED_HELP;
    }

    if (list_types) {
	rt_obj_make_labels(gedp->ged_result_str, " ");
	return GED_HELP;
    }

    /* what remains must be exactly: name type */
    if (uac != 2) {
	print_usage(gedp, cmd, d);
	return BRLCAD_ERROR;
    }
    name = argv[0];
    type = argv[1];

    GED_CHECK_EXISTS(gedp, name, LOOKUP_QUIET, BRLCAD_ERROR);
    RT_DB_INTERNAL_INIT(&internal);

    if (BU_STR_EQUAL(type, "hf")) {
	bu_vls_printf(gedp->ged_result_str, "make: the height field is deprecated and not supported by this command.\nUse the dsp primitive.\n");
	return BRLCAD_ERROR;
    } else if (BU_STR_EQUAL(type, "pg") ||
	       BU_STR_EQUAL(type, "poly")) {
	bu_vls_printf(gedp->ged_result_str, "make: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.");
	return BRLCAD_ERROR;
    } else if (BU_STR_EQUAL(type, "cline") ||
	       BU_STR_EQUAL(type, "dsp") ||
	       BU_STR_EQUAL(type, "ebm") ||
	       BU_STR_EQUAL(type, "nurb") ||
	       BU_STR_EQUAL(type, "spline") ||
	       BU_STR_EQUAL(type, "submodel") ||
	       BU_STR_EQUAL(type, "vol")) {
	bu_vls_printf(gedp->ged_result_str, "make: the %s primitive is not supported by this command", type);
	return BRLCAD_ERROR;
    } else if (rt_obj_make(type, origin, scale, &internal) == BRLCAD_OK) {
	bu_log("SUCCESS for type %s\n", type);
    } else {
	print_usage(gedp, cmd, d);
	return BRLCAD_ERROR;
    }

    /* TODO/FIXME: extrude relies on a sketch. Create said sketch as a post-process check.
     * is this how we want to handle this, should extrude's ft_make create it's own sketch,
     * or should an extrude without a sketch be legal?
     */
    if (BU_STR_EQUAL(type, "extrude")) {
	char *av[8];
	char center_str[512];
	char scale_str[128];
	struct rt_extrude_internal* extrude_ip = (struct rt_extrude_internal *)internal.idb_ptr;

	/* sanity */
	if (!extrude_ip)
	    return BRLCAD_ERROR;

	/* attach a sketch name to the extrude */
	av[0] = "make_name";
	av[1] = "skt_";
	ged_exec_make_name(gedp, 2, (const char **)av);
	if (extrude_ip->sketch_name)
	    bu_free(extrude_ip->sketch_name, "empty sketch_name");
	extrude_ip->sketch_name = bu_strdup(bu_vls_addr(gedp->ged_result_str));

	sprintf(center_str, "%f %f %f", V3ARGS(origin));
	sprintf(scale_str, "%f", scale);
	av[0] = "make";
	av[1] = "-o";
	av[2] = center_str;
	av[3] = "-s";
	av[4] = scale_str;
	av[5] = extrude_ip->sketch_name;
	av[6] = "sketch";
	av[7] = (char *)0;
	/* TODO: should probably validate the sketch was made successfully */
	ged_make_core(gedp, 7, (const char **)av);
    }

    /* no interrupts */
    (void)signal(SIGINT, SIG_IGN);

    GED_DB_DIRADD(gedp, dp, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_MAKE_COMMANDS(X, XID) \
    X(make, ged_make_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_MAKE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_make", 1, GED_MAKE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
