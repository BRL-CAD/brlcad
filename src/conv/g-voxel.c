/*                     G - V O X E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "analyze.h"


struct voxelizeData
{
    fastf_t voxelSize[3];
    fastf_t threshold;
    fastf_t bbMin[3];
};


struct g_voxel_args
{
    int help;
    fastf_t voxel_size[3];
    int detail;
    fastf_t threshold;
};


static const struct bu_arg_option g_voxel_options[] = {
    BU_ARG_FLAG("h", "help", struct g_voxel_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_POSITIVE_VECTOR3("s", "size", struct g_voxel_args, voxel_size,
	"dx/dy/dz",
	"Set positive XYZ voxel dimensions in millimeters as a packed or quoted triple, or three separate values (default 1/1/1)"),
    BU_ARG_INTEGER_RANGE("d", "detail", struct g_voxel_args, detail,
	1, 1, 1, ANALYZE_VOXELIZE_MAX_LEVEL_DETAIL, "rays",
	"Set positive rays per voxel edge for fill sampling (default 4)"),
    BU_ARG_NUMBER_RANGE("t", "threshold", struct g_voxel_args, threshold,
	1, 0.0, 1, 1.0, "fraction",
	"Write voxels meeting this 0 to 1 fill fraction (default 0.5)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand g_voxel_operands[] = {
    BU_ARG_OPERAND("model.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Object to voxelize", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema g_voxel_schema = BU_ARG_SCHEMA(
    "g-voxel", "Voxelize BRL-CAD geometry into occupied-cell samples",
    g_voxel_options, g_voxel_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar g_voxel_grammar = {
    &g_voxel_schema, NULL, NULL
};


/**
 * Function to print values to File
 */
static void
printToFile(void *callBackData, int x, int y, int z, const char *a, fastf_t fill) {
    struct voxelizeData *dataValues = (struct voxelizeData *)callBackData;
    FILE *fp;

    if ((a != NULL) && (dataValues->threshold <= fill)) {
	fp = fopen("voxels1.txt", "a");

	if (fp != NULL) {
	    fastf_t voxel[3];

	    voxel[0] = dataValues->bbMin[0] + (x + 0.5) * dataValues->voxelSize[0];
	    voxel[1] = dataValues->bbMin[1] + (y + 0.5) * dataValues->voxelSize[1];
	    voxel[2] = dataValues->bbMin[2] + (z + 0.5) * dataValues->voxelSize[2];

	    fprintf(fp, "(%f, %f, %f)\t%s\t%f\n", voxel[0], voxel[1], voxel[2], a, fill);
	    fclose(fp);
	}
    }
    /* else this voxel is air */
}


int
main(int argc, char **argv)
{
    static struct rt_i *rtip;
    struct g_voxel_args args = {0, {1.0, 1.0, 1.0}, 4, 0.5};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct voxelizeData dataValues;
    int introspection;
    int operand_index;
    int gottree = 0;

    char title[1024] = {0};

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &g_voxel_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&g_voxel_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&g_voxel_schema, "g-voxel");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "g-voxel help");
	return 0;
    }
    VMOVE(dataValues.voxelSize, args.voxel_size);
    dataValues.threshold = args.threshold;
    argc -= operand_index + 1;
    argv += operand_index + 1;

    /* Load the specified geometry database (i.e., a ".g" file).
     * rt_dirbuild() returns an "instance" pointer which describes the
     * database to be raytraced.  It also gives you back the title
     * string if you provide a buffer.  This builds a directory of the
     * geometry (i.e., a table of contents) in the file.
     */
    rtip = rt_dirbuild(argv[0], title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", argv[0]);
    }
    argc--;
    argv++;

    /* Load geometry database objects that were specified on the
     * command line.
     */
    while (argc > 0) {
	if (rt_gettree(rtip, argv[0]) < 0)
	    bu_log("Loading the geometry for [%s] FAILED\n", argv[0]);
	else
	    gottree = 1;

	argc--;
	argv++;
    }

    if (gottree != 0) {
	VMOVE(dataValues.bbMin, rtip->mdl_min);

	/* work horse */
	voxelize(rtip, dataValues.voxelSize, args.detail, printToFile,
		(void *)&dataValues);
    }

    rt_i_destroy(rtip);

    return 0;
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
