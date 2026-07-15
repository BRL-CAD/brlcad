/*                      S P H G R O U P . C
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
/** @file libged/group.c
 *
 * The group command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "wdb.h"

#include "../ged_private.h"


static const struct bu_cmd_operand sphgroup_schema_operands[] = {
    BU_CMD_OPERAND("group_name", BU_CMD_VALUE_STRING, 1, 1,
	"Output group name", NULL),
    BU_CMD_OPERAND("target_sphere", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Existing bounding sphere", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema sphgroup_cmd_schema = {
    "sphgroup", "Create a group around a target sphere", NULL,
    sphgroup_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_sphgroup_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp, *sphdp;
    point_t obj_min, obj_max;
    point_t rpp_min, rpp_max;
    point_t centerpt;
    int inside_flag = 0;
    struct rt_db_internal sph_intern;
    struct rt_ell_internal *bsph;
    static const char *usage = "gname target_sphere.s";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    operand_index = bu_cmd_schema_parse_complete(&sphgroup_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    if ((sphdp = db_lookup(gedp->dbip, argv[argc-1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Specified bounding sphere %s not found\n", argv[argc-1]);
	return BRLCAD_ERROR;
    } else {
	if (rt_db_get_internal(&sph_intern, sphdp, gedp->dbip, (fastf_t *)NULL) < 0)
	  return BRLCAD_ERROR;
	if ((sph_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ELL) && (sph_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_SPH)) {
	    bu_vls_printf(gedp->ged_result_str, "Specified bounding object %s not a sphere\n", argv[argc-1]);
	    return BRLCAD_ERROR;
	}
	bsph = (struct rt_ell_internal *)sph_intern.idb_ptr;
    }

    /* get objects to add to group - at the moment, only gets regions*/
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	if (dp->d_nref == 0 && !(dp->d_flags & RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) continue;
	if (BU_STR_EQUAL(dp->d_namep, sphdp->d_namep)) continue;
	if (!(dp->d_flags & RT_DIR_REGION)) continue;
	inside_flag = 0;
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&(dp->d_namep), 0, obj_min, obj_max) != BRLCAD_ERROR) {
	    VSETALL(rpp_min, INFINITY);
	    VSETALL(rpp_max, -INFINITY);
	    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	    /*
	      VMOVE(testpts[0], rpp_min);
	      VSET(testpts[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
	      VSET(testpts[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
	      VSET(testpts[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
	      VSET(testpts[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
	      VSET(testpts[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
	      VMOVE(testpts[6], rpp_max);
	      VSET(testpts[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
	      for (j = 0; j < 8; j++) {
	      if (DIST_PNT_PNT(testpts[j], bsph->v) <= MAGNITUDE(bsph->a)) inside_flag = 1;
	      }*/
	    VSET(centerpt, (rpp_min[0] + rpp_max[0])*0.5, (rpp_min[1] + rpp_max[1])*0.5, (rpp_min[2] + rpp_max[2])*0.5);
	    if (DIST_PNT_PNT(centerpt, bsph->v) <= MAGNITUDE(bsph->a)) inside_flag = 1;
	    if (inside_flag == 1) {
		if (_ged_combadd(gedp, dp, (char *)argv[1], 0, WMOP_UNION, 0, 0) == RT_DIR_NULL) return BRLCAD_ERROR;
	    }
	}
    FOR_ALL_DIRECTORY_END;
    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SPHGROUP_COMMANDS(X, XID) \
    X(sphgroup, ged_sphgroup_core, GED_CMD_DEFAULT, &sphgroup_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SPHGROUP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_sphgroup", 1, GED_SPHGROUP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
