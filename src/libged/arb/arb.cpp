/*                         A R B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file arb.cpp
 *
 * Top level command for logic specific to ARB8 objects.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/geom.h"
#include "bu/cmd.h"
#include "ged/event_txn.h"
#include "../ged_private.h"
#include "ged_arb.h"

extern "C" int
_arb_cmd_create(void *bs, int argc, const char *argv[])
{
    struct _ged_arb_info *gb = (struct _ged_arb_info *)bs;
    struct ged *gedp = gb->gedp;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_arb_internal *arb;
    int i, j;
    double rota, fb_a;
    vect_t norm1, norm2, norm3;
    static const char *usage = "name rot fb";

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    /* get rotation angle */
    if (sscanf(argv[2], "%lf", &rota) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad rotation angle - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    /* get fallback angle */
    if (sscanf(argv[3], "%lf", &fb_a) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad fallback angle - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    rota *= DEG2RAD;
    fb_a *= DEG2RAD;

    BU_ALLOC(arb, struct rt_arb_internal);
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &OBJ[ID_ARB8];
    internal.idb_ptr = (void *)arb;
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    VSET(arb->pt[0], 0.0, 0.0, 0.0);

    /* calculate normal vector defined by rot, fb_a */
    norm1[0] = cos(fb_a) * cos(rota);
    norm1[1] = cos(fb_a) * sin(rota);
    norm1[2] = sin(fb_a);

    /* find two perpendicular vectors which are perpendicular to norm */
    j = 0;
    for (i = 0; i < 3; i++) {
	if (fabs(norm1[i]) < fabs(norm1[j]))
	    j = i;
    }
    VSET(norm2, 0.0, 0.0, 0.0);
    norm2[j] = 1.0;
    VCROSS(norm3, norm2, norm1);
    VCROSS(norm2, norm3, norm1);

    /* create new rpp 20x20x2 */
    /* the 20x20 faces are in rot, fb plane */
    VUNITIZE(norm2);
    VUNITIZE(norm3);
    VJOIN1(arb->pt[1], arb->pt[0], 508.0, norm2);
    VJOIN1(arb->pt[3], arb->pt[0], -508.0, norm3);
    VJOIN2(arb->pt[2], arb->pt[0], 508.0, norm2, -508.0, norm3);
    for (i = 0; i < 4; i++)
	VJOIN1(arb->pt[i+4], arb->pt[i], -50.8, norm1);

    int event_batch_opened = (ged_event_batch_begin(gedp) > 0);
    dp = db_diradd(gedp->dbip, argv[1], RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_SOLID, (void *)&internal.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to add %s to the database.",
		argv[1]);
	if (event_batch_opened)
	    ged_event_batch_end(gedp, NULL);
	rt_db_free_internal(&internal);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(dp, gedp->dbip, &internal) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write failure.");
	if (event_batch_opened)
	    ged_event_batch_end(gedp, NULL);
	return BRLCAD_ERROR;
    }
    (void)ged_event_notify_object_added(gedp, argv[1], NULL);
    if (event_batch_opened)
	ged_event_batch_end(gedp, NULL);

    return BRLCAD_OK;
}

const struct bu_cmdtab _arb_cmds[] = {
    { "create", _arb_cmd_create },
    { "repair", _arb_cmd_repair },
    { NULL, NULL }
};

extern "C" int
ged_arb_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s <subcommand> [args]\n", argv[0]);
	bu_vls_printf(gedp->ged_result_str, "Subcommands: create, repair\n");
	bu_vls_printf(gedp->ged_result_str, "Legacy Usage: %s name rot fb\n", argv[0]);
	return GED_HELP;
    }

    if (bu_cmd_valid(_arb_cmds, argv[1]) == BRLCAD_OK) {
	int cmd_argc = argc - 1;
	const char **cmd_argv = &argv[1];

	struct _ged_arb_info gb;
	gb.gedp = gedp;
	gb.cmds = _arb_cmds;

	if (bu_cmd(_arb_cmds, cmd_argc, cmd_argv, 0, (void *)&gb, &ret) == BRLCAD_OK) {
	    return ret;
	}
    }

    if (argc == 4) {
	bu_log("WARNING: The 'arb name rot fb' syntax is deprecated and will be removed in a future release.\n");
	bu_log("         Please use 'arb create name rot fb' instead.\n");

	struct _ged_arb_info gb;
	gb.gedp = gedp;
	gb.cmds = _arb_cmds;

	return _arb_cmd_create((void *)&gb, argc, argv);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s <subcommand> [args]\n", argv[0]);
    bu_vls_printf(gedp->ged_result_str, "Subcommands: create, repair\n");
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

extern "C" int ged_rotate_arb_face_core(struct ged *gedp, int argc, const char *argv[]);

#define GED_ARB_COMMANDS(X, XID) \
    X(arb, ged_arb_core, GED_CMD_DEFAULT) \
    X(rotate_arb_face, ged_rotate_arb_face_core, GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_ARB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_arb", 1, GED_ARB_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
