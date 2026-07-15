/*                         V O X E L I Z E . C
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
/** @file libged/voxelize.c
 *
 * The voxelize command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "analyze.h"
#include "wdb.h"


struct voxelizeData
{
    fastf_t threshold;
    fastf_t *bbMin;
    fastf_t sizeVoxel[3];
    struct rt_wdb *wdbp;
    char *newname;
    struct wmember content;
};


struct voxelize_args {
    vect_t size;
    int detail;
    fastf_t threshold;
};


static int
voxelize_size_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    vect_t size = VINIT_ZERO;
    int consumed = bu_cmd_vector3_from_argv(size, argc, (const char * const *)argv);

    if (consumed == 0 || (size_t)consumed != argc) {
	if (msg)
	    bu_vls_printf(msg, "voxel size must be x/y/z, x,y,z, x;y;z, quoted x y z, or three finite numbers\n");
	return -1;
    }
    if (size[X] <= 0.0 || size[Y] <= 0.0 || size[Z] <= 0.0) {
	if (msg)
	    bu_vls_printf(msg, "voxel dimensions must all be positive\n");
	return -1;
    }
    if (storage)
	VMOVE((fastf_t *)storage, size);
    return 0;
}


static int
voxelize_threshold_validate(struct bu_vls *msg, const char *arg)
{
    fastf_t threshold;

    if (bu_cmd_number_from_str(&threshold, arg) && threshold >= 0.0 && threshold <= 1.0)
	return 0;
    if (msg)
	bu_vls_printf(msg, "fill threshold must be a finite number from 0 through 1\n");
    return -1;
}


static const struct bu_cmd_option voxelize_schema_options[] = {
    BU_CMD_OPTION_SHAPED("s", NULL, "s", struct voxelize_args, size,
	BU_CMD_VALUE_VECTOR, "dx/dy/dz", "Positive voxel dimensions",
	BU_CMD_ARG_REQUIRED, &bu_cmd_vector3_arg_shape, voxelize_size_consume),
    BU_CMD_POSITIVE_INTEGER("d", NULL, struct voxelize_args, detail, "level",
	"Positive ray-sampling detail level"),
    BU_CMD_NUMBER_VALIDATE("t", NULL, struct voxelize_args, threshold,
	voxelize_threshold_validate, "fraction", "Fill threshold from 0 through 1"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand voxelize_schema_operands[] = {
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the output voxel group", NULL),
    BU_CMD_OPERAND("input_objects", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Existing geometry objects", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema voxelize_cmd_schema = {
    "voxelize", "Voxelize geometry objects", voxelize_schema_options,
    voxelize_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


static void
voxelize_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&voxelize_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-s dx/dy/dz] [-d level] [-t fraction] new_obj old_obj [old_obj ...]",
	command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "voxelize native option help");
    }
}

static void
create_boxes(void *callBackData, int x, int y, int z, const char *a, fastf_t fill)
{
    if (a != NULL) {
	fastf_t min[3], max[3];

	struct bu_vls *vp;
	char bufx[50], bufy[50], bufz[50];
	char *nameDestination;

	struct voxelizeData *dataValues = (struct voxelizeData *)callBackData;

	sprintf(bufx, "%d", x);
	sprintf(bufy, "%d", y);
	sprintf(bufz, "%d", z);

	if (dataValues->threshold <= fill) {
	    vp = bu_vls_vlsinit();
	    bu_vls_strcat(vp, dataValues->newname);
	    bu_vls_strcat(vp, ".x");
	    bu_vls_strcat(vp, bufx);
	    bu_vls_strcat(vp, "y");
	    bu_vls_strcat(vp, bufy);
	    bu_vls_strcat(vp, "z");
	    bu_vls_strcat(vp, bufz);
	    bu_vls_strcat(vp, ".s");

	    min[0] = (dataValues->bbMin)[0] + (x * (dataValues->sizeVoxel)[0]);
	    min[1] = (dataValues->bbMin)[1] + (y * (dataValues->sizeVoxel)[1]);
	    min[2] = (dataValues->bbMin)[2] + (z * (dataValues->sizeVoxel)[2]);
	    max[0] = (dataValues->bbMin)[0] + ( (x + 1.0) * (dataValues->sizeVoxel)[0]);
	    max[1] = (dataValues->bbMin)[1] + ( (y + 1.0) * (dataValues->sizeVoxel)[1]);
	    max[2] = (dataValues->bbMin)[2] + ( (z + 1.0) * (dataValues->sizeVoxel)[2]);

	    nameDestination = bu_vls_strgrab(vp);

	    /* guard against duplicate rpp's
	     *	voxelize() calls this once per region - NOT once per voxel. So overlapping
	     *	regions can/will create duplicate solids in the tree
	     */
	    if (db_lookup(dataValues->wdbp->dbip, nameDestination, LOOKUP_QUIET) == RT_DIR_NULL) {
		mk_rpp(dataValues->wdbp, nameDestination, min, max);
		mk_addmember(nameDestination, &dataValues->content.l, 0, WMOP_UNION);
	    }

	    bu_free(nameDestination, "free nameDestination strgrab");
	}
    }
    /* else this voxel is air */
}

int
ged_voxelize_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_i *rtip;
    fastf_t sizeVoxel[3];
    int levelOfDetail;
    void *callBackData;
    struct voxelizeData voxDat;
    struct voxelize_args args = {{1.0, 1.0, 1.0}, 1, 0.5};
    int operand_index;
    int object_count;
    const char **objects;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	voxelize_show_help(gedp, argv[0]);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(&voxelize_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	voxelize_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    object_count = argc - 1 - operand_index;
    objects = argv + operand_index + 1;

    sizeVoxel[0] = args.size[0] * gedp->dbip->dbi_local2base;
    sizeVoxel[1] = args.size[1] * gedp->dbip->dbi_local2base;
    sizeVoxel[2] = args.size[2] * gedp->dbip->dbi_local2base;
    levelOfDetail = args.detail;

    voxDat.newname = (char *)objects[0];

    if (db_lookup(gedp->dbip, voxDat.newname, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "error: solid '%s' already exists, aborting\n", voxDat.newname);
	return BRLCAD_ERROR;
    }

    rtip = rt_i_create(gedp->dbip);
    rtip->useair = 1;

    /* Walk trees.  Here we identify any object trees in the database
     * that the user wants included in the ray trace.
     */
	for (int i = 1; i < object_count; i++) {
	if (rt_gettree(rtip, objects[i]) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "error: object '%s' does not exist, aborting\n", objects[i]);
	    rt_i_destroy(rtip);
	    return BRLCAD_ERROR;
	}
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    voxDat.sizeVoxel[0] = sizeVoxel[0];
    voxDat.sizeVoxel[1] = sizeVoxel[1];
    voxDat.sizeVoxel[2] = sizeVoxel[2];
    voxDat.threshold = args.threshold;
    voxDat.wdbp = wdbp;
    voxDat.bbMin = rtip->mdl_min;
    BU_LIST_INIT(&voxDat.content.l);

    callBackData = (void*)(&voxDat);

   /* voxelize function is called here with rtip(ray trace instance), userParameter and create_boxes function */
    voxelize(rtip, sizeVoxel, levelOfDetail, create_boxes, callBackData);

    mk_comb(wdbp, voxDat.newname, &voxDat.content.l, 1, "plastic", "sh=4 sp=0.5 di=0.5 re=0.1", 0, 1000, 0, 0, 100, 0, 0, 0);

    mk_freemembers(&voxDat.content.l);
    rt_i_destroy(rtip);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_VOXELIZE_COMMANDS(X, XID) \
    X(voxelize, ged_voxelize_core, GED_CMD_DEFAULT, &voxelize_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_VOXELIZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_voxelize", 1, GED_VOXELIZE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
