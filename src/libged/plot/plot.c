/*                         P L O T . C
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
/** @file libged/plot.c
 *
 * The plot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "bn.h"
#include "bu/cmdschema.h"
#include "bv/plot3.h"
#include "bg/clip.h"

#include "../ged_private.h"

#if defined(HAVE_POPEN) && !defined(HAVE_DECL_POPEN) && !defined(popen)
extern FILE *popen(const char *command, const char *type);
#endif
#if defined(HAVE_POPEN) && !defined(HAVE_POPEN_DECL) && !defined(pclose)
extern int pclose(FILE *stream);
#endif

void
dl_plot(struct bu_list *hdlp, FILE *fp, mat_t model2view, int floating, mat_t center, fastf_t scale, int Three_D, int Z_clip)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    struct bv_vlist *vp;
    static vect_t clipmin, clipmax;
    static vect_t last;         /* last drawn point */
    static vect_t fin;
    static vect_t start;
    int Dashing;                        /* linetype is dashed */

    if (floating) {
        pd_3space(fp,
                  -center[MDX] - scale,
                  -center[MDY] - scale,
                  -center[MDZ] - scale,
                  -center[MDX] + scale,
                  -center[MDY] + scale,
                  -center[MDZ] + scale);
        Dashing = 0;
        pl_linmod(fp, "solid");

        gdlp = BU_LIST_NEXT(display_list, hdlp);
        while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
            next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

            for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
                /* Could check for differences from last color */
                pl_color(fp,
                         sp->s_color[0],
                         sp->s_color[1],
                         sp->s_color[2]);
                if (Dashing != sp->s_soldash) {
                    if (sp->s_soldash)
                        pl_linmod(fp, "dotdashed");
                    else
                        pl_linmod(fp, "solid");
                    Dashing = sp->s_soldash;
                }
                bv_vlist_to_uplot(fp, &(sp->s_vlist));
            }

            gdlp = next_gdlp;
        }

        return;
    }
    /*
     * Integer output version, either 2-D or 3-D.
     * Viewing region is from -1.0 to +1.0
     * which is mapped to integer space -2048 to +2048 for plotting.
     * Compute the clipping bounds of the screen in view space.
     */
    clipmin[X] = -1.0;
    clipmax[X] =  1.0;
    clipmin[Y] = -1.0;
    clipmax[Y] =  1.0;
    if (Z_clip) {
        clipmin[Z] = -1.0;
        clipmax[Z] =  1.0;
    } else {
        clipmin[Z] = -1.0e20;
        clipmax[Z] =  1.0e20;
    }

    if (Three_D)
        pl_3space(fp, (int)BV_MIN, (int)BV_MIN, (int)BV_MIN, (int)BV_MAX, (int)BV_MAX, (int)BV_MAX);
    else
        pl_space(fp, (int)BV_MIN, (int)BV_MIN, (int)BV_MAX, (int)BV_MAX);
    pl_erase(fp);
    Dashing = 0;
    pl_linmod(fp, "solid");

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
            if (Dashing != sp->s_soldash) {
                if (sp->s_soldash)
                    pl_linmod(fp, "dotdashed");
                else
                    pl_linmod(fp, "solid");
                Dashing = sp->s_soldash;
            }
            for (BU_LIST_FOR(vp, bv_vlist, &(sp->s_vlist))) {
                size_t i;
                size_t nused = vp->nused;
                int *cmd = vp->cmd;
                point_t *pt = vp->pt;
                for (i = 0; i < nused; i++, cmd++, pt++) {
                    switch (*cmd) {
                        case BV_VLIST_POLY_START:
                        case BV_VLIST_POLY_VERTNORM:
                        case BV_VLIST_TRI_START:
                        case BV_VLIST_TRI_VERTNORM:
                            continue;
                        case BV_VLIST_POLY_MOVE:
                        case BV_VLIST_LINE_MOVE:
                        case BV_VLIST_TRI_MOVE:
                            /* Move, not draw */
                            MAT4X3PNT(last, model2view, *pt);
                            continue;
                        case BV_VLIST_LINE_DRAW:
                        case BV_VLIST_POLY_DRAW:
                        case BV_VLIST_POLY_END:
                        case BV_VLIST_TRI_DRAW:
                        case BV_VLIST_TRI_END:
                            /* draw */
                            MAT4X3PNT(fin, model2view, *pt);
                            VMOVE(start, last);
                            VMOVE(last, fin);
                            break;
                    }
                    if (bg_ray_vclip(start, fin, clipmin, clipmax) == 0)
                        continue;

                    if (Three_D) {
                        /* Could check for differences from last color */
                        pl_color(fp,
                                 sp->s_color[0],
                                 sp->s_color[1],
                                 sp->s_color[2]);
                        pl_3line(fp,
                                 (int)(start[X] * BV_MAX),
                                 (int)(start[Y] * BV_MAX),
                                 (int)(start[Z] * BV_MAX),
                                 (int)(fin[X] * BV_MAX),
                                 (int)(fin[Y] * BV_MAX),
                                 (int)(fin[Z] * BV_MAX));
                    } else {
                        pl_line(fp,
                                (int)(start[0] * BV_MAX),
                                (int)(start[1] * BV_MAX),
                                (int)(fin[0] * BV_MAX),
                                (int)(fin[1] * BV_MAX));
                    }
                }
            }
        }

        gdlp = next_gdlp;
    }
}


struct plot_args {
    int two_d;
    int three_d;
    int floating;
    int grid;
    int z_clip;
};

static const struct bu_cmd_option plot_schema_options[] = {
    BU_CMD_FLAG("2", NULL, struct plot_args, two_d, "Write a 2D plot"),
    BU_CMD_FLAG("3", NULL, struct plot_args, three_d, "Write a 3D plot"),
    BU_CMD_FLAG("f", NULL, struct plot_args, floating,
	"Use floating-point plot output"),
    BU_CMD_FLAG("g", NULL, struct plot_args, grid, "Include a grid"),
    BU_CMD_FLAG("z", NULL, struct plot_args, z_clip, "Enable Z clipping"),
    BU_CMD_ALIAS_SHORT("Z", "z", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand plot_schema_operands[] = {
    BU_CMD_OPERAND("output", BU_CMD_VALUE_FILE, 1, 1,
	"Output file or |filter command", "ged.file_path"),
    BU_CMD_OPERAND("filter_arguments", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Arguments to a pipe filter", NULL),
    BU_CMD_OPERAND_NULL
};
static const char *plot_dimension_options[] = {"2", "3", NULL};
static const struct bu_cmd_constraint plot_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(plot_dimension_options, 0, 1,
	"-2 and -3 are mutually exclusive"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema plot_cmd_schema = {
    "plot", "Write the current display as a UNIX plot", plot_schema_options,
    plot_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, plot_schema_constraints)
};


/*
 * plot file [opts]
 * potential options might include:
 * grid, 3d w/color, |filter, infinite Z
 */
int
ged_plot_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    struct plot_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;
    int Three_D;			/* 0=2-D -vs- 1=3-D */
    int Z_clip;			/* Z clipping */
    int floating;			/* 3-D floating point plot */
    int is_pipe = 0;
    static const char *plot_usage = "file [2|3] [f] [g] [z]";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], plot_usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(&plot_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	return BRLCAD_ERROR;
    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    if (operand_count < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: no filename or filter specified\n", argv[0]);
	return BRLCAD_ERROR;
    }

    Three_D = args.two_d ? 0 : 1;
    Z_clip = args.z_clip;
    floating = args.floating;
    if (args.grid)
	bu_vls_printf(gedp->ged_result_str, "%s: grid unimplemented\n", argv[0]);
    if (Z_clip)
	bu_vls_printf(gedp->ged_result_str, "%s: Clipped in Z to viewing cube\n", argv[0]);

    if (operands[0][0] == '|') {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_strcpy(&str, &operands[0][1]);
	for (int oi = 1; oi < operand_count; oi++) {
	    bu_vls_strcat(&str, " ");
	    bu_vls_strcat(&str, operands[oi]);
	}
	fp = popen(bu_vls_addr(&str), "wb");
	if (fp == NULL) {
	    perror(bu_vls_addr(&str));
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "piped to %s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
	is_pipe = 1;
    } else {
	fp = fopen(operands[0], "wb");
	if (fp == NULL) {
	    perror(operands[0]);
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "plot stored in %s\n", operands[0]);
	is_pipe = 0;
    }

    dl_plot(gedp->i->ged_gdp->gd_headDisplay, fp, gedp->ged_gvp->gv_model2view, floating, gedp->ged_gvp->gv_center, gedp->ged_gvp->gv_scale, Three_D, Z_clip);

    if (is_pipe)
	(void)pclose(fp);
    else
	(void)fclose(fp);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_PLOT_COMMANDS(X, XID) \
    X(plot, ged_plot_core, GED_CMD_DEFAULT, &plot_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PLOT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_plot", 1, GED_PLOT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
