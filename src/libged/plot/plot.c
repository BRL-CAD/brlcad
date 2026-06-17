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
#include "bsg/export.h"
#include "bsg/field.h"
#include "bsg/plot3.h"
#include "bsg/render.h"
#include "bg/clip.h"

#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"

#if defined(HAVE_POPEN) && !defined(HAVE_DECL_POPEN) && !defined(popen)
extern FILE *popen(const char *command, const char *type);
#endif
#if defined(HAVE_POPEN) && !defined(HAVE_POPEN_DECL) && !defined(pclose)
extern int pclose(FILE *stream);
#endif

/* Callback data for plot operations */
struct plot_data {
    FILE *fp;
    mat_t model2view;
    int floating;
    mat_t center;
    fastf_t scale;
    int Three_D;
    int Z_clip;
    int Dashing;
    vect_t clipmin;
    vect_t clipmax;
};

struct plot_segment_data {
    const struct bsg_export_record *rec;
    struct plot_data *pd;
};

static int
plot_floating_segment_cb(const point_t a, const point_t b, void *data)
{
    struct plot_segment_data *sd = (struct plot_segment_data *)data;
    if (!sd || !sd->pd)
	return 0;

    pdv_3move(sd->pd->fp, a);
    pdv_3cont(sd->pd->fp, b);
    return 1;
}

static int
plot_integer_segment_cb(const point_t a, const point_t b, void *data)
{
    struct plot_segment_data *sd = (struct plot_segment_data *)data;
    vect_t start;
    vect_t fin;

    if (!sd || !sd->pd || !sd->rec)
	return 0;

    MAT4X3PNT(start, sd->pd->model2view, a);
    MAT4X3PNT(fin, sd->pd->model2view, b);
    if (bg_ray_vclip(start, fin, sd->pd->clipmin, sd->pd->clipmax) == 0)
	return 1;

    if (sd->pd->Three_D) {
	pl_color(sd->pd->fp, sd->rec->color[0], sd->rec->color[1], sd->rec->color[2]);
	pl_3line(sd->pd->fp,
		(int)(start[X] * BSG_VIEW_MAX),
		(int)(start[Y] * BSG_VIEW_MAX),
		(int)(start[Z] * BSG_VIEW_MAX),
		(int)(fin[X] * BSG_VIEW_MAX),
		(int)(fin[Y] * BSG_VIEW_MAX),
		(int)(fin[Z] * BSG_VIEW_MAX));
    } else {
	pl_line(sd->pd->fp,
		(int)(start[X] * BSG_VIEW_MAX),
		(int)(start[Y] * BSG_VIEW_MAX),
		(int)(fin[X] * BSG_VIEW_MAX),
		(int)(fin[Y] * BSG_VIEW_MAX));
    }

    return 1;
}

/* Callback for floating-point 3D plot */
static void
plot_floating_record(const struct bsg_export_record *rec, struct plot_data *pd)
{
    if (!rec || !pd)
	return;
    if (!bsg_export_record_has_segments(rec))
	return;

    pl_color(pd->fp, rec->color[0], rec->color[1], rec->color[2]);
    if (pd->Dashing != rec->line_style) {
	if (rec->line_style)
	    pl_linmod(pd->fp, "dotdashed");
	else
	    pl_linmod(pd->fp, "solid");
	pd->Dashing = rec->line_style;
    }
    struct plot_segment_data sd;
    sd.rec = rec;
    sd.pd = pd;
    (void)bsg_export_record_foreach_segment(rec, plot_floating_segment_cb, &sd);
}

/* Callback for integer plot (2D or 3D) */
static void
plot_integer_record(const struct bsg_export_record *rec, struct plot_data *pd)
{
    if (!rec || !pd)
	return;
    if (!bsg_export_record_has_segments(rec))
	return;

    if (pd->Dashing != rec->line_style) {
	if (rec->line_style)
	    pl_linmod(pd->fp, "dotdashed");
	else
	    pl_linmod(pd->fp, "solid");
	pd->Dashing = rec->line_style;
    }

    struct plot_segment_data sd;
    sd.rec = rec;
    sd.pd = pd;
    (void)bsg_export_record_foreach_segment(rec, plot_integer_segment_cb, &sd);
}

void
dl_plot(struct ged *gedp, FILE *fp, mat_t model2view, int floating, mat_t center, fastf_t scale, int Three_D, int Z_clip)
{
    struct plot_data pd;

    pd.fp = fp;
    MAT_COPY(pd.model2view, model2view);
    pd.floating = floating;
    MAT_COPY(pd.center, center);
    pd.scale = scale;
    pd.Three_D = Three_D;
    pd.Z_clip = Z_clip;
    pd.Dashing = 0;

    struct bsg_export_request request;
    bsg_export_request_init(&request, gedp->ged_gvp);
    request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY;
    request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result)
	return;

    if (floating) {
	pd_3space(fp,
		  -center[MDX] - scale,
		  -center[MDY] - scale,
		  -center[MDZ] - scale,
		  -center[MDX] + scale,
		  -center[MDY] + scale,
		  -center[MDZ] + scale);
	pl_linmod(fp, "solid");
	for (size_t i = 0; i < bsg_export_result_count(result); i++)
	    plot_floating_record(bsg_export_result_get(result, i), &pd);
	bsg_export_result_free(result);
	return;
    }

    /*
     * Integer output version, either 2-D or 3-D.
     * Viewing region is from -1.0 to +1.0
     * which is mapped to integer space -2048 to +2048 for plotting.
     * Compute the clipping bounds of the screen in view space.
     */
    pd.clipmin[X] = -1.0;
    pd.clipmax[X] =  1.0;
    pd.clipmin[Y] = -1.0;
    pd.clipmax[Y] =  1.0;
    if (Z_clip) {
	pd.clipmin[Z] = -1.0;
	pd.clipmax[Z] =  1.0;
    } else {
	pd.clipmin[Z] = -1.0e20;
	pd.clipmax[Z] =  1.0e20;
    }

    if (Three_D)
	pl_3space(fp, (int)BSG_VIEW_MIN, (int)BSG_VIEW_MIN, (int)BSG_VIEW_MIN, (int)BSG_VIEW_MAX, (int)BSG_VIEW_MAX, (int)BSG_VIEW_MAX);
    else
	pl_space(fp, (int)BSG_VIEW_MIN, (int)BSG_VIEW_MIN, (int)BSG_VIEW_MAX, (int)BSG_VIEW_MAX);
    pl_erase(fp);
    pl_linmod(fp, "solid");
    for (size_t i = 0; i < bsg_export_result_count(result); i++)
	plot_integer_record(bsg_export_result_get(result, i), &pd);
    bsg_export_result_free(result);
}


/*
 * plot file [opts]
 * potential options might include:
 * grid, 3d w/color, |filter, infinite Z
 */
int
ged_plot_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
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

    /* Process any options */
    Three_D = 1;				/* 3-D w/color, by default */
    Z_clip = 0;				/* NO Z clipping, by default*/
    floating = 0;
    while (argv[1] != (char *)0 && argv[1][0] == '-') {
	switch (argv[1][1]) {
	    case 'f':
		floating = 1;
		break;
	    case '3':
		Three_D = 1;
		break;
	    case '2':
		Three_D = 0;		/* 2-D, for portability */
		break;
	    case 'g':
		/* do grid */
		bu_vls_printf(gedp->ged_result_str, "%s: grid unimplemented\n", argv[0]);
		break;
	    case 'z':
	    case 'Z':
		/* Enable Z clipping */
		bu_vls_printf(gedp->ged_result_str, "%s: Clipped in Z to viewing cube\n", argv[0]);
		Z_clip = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: bad PLOT option %s\n", argv[0], argv[1]);
		break;
	}
	argv++;
    }
    if (argv[1] == (char *)0) {
	bu_vls_printf(gedp->ged_result_str, "%s: no filename or filter specified\n", argv[0]);
	return BRLCAD_ERROR;
    }
    if (argv[1][0] == '|') {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_strcpy(&str, &argv[1][1]);
	while ((++argv)[1] != (char *)0) {
	    bu_vls_strcat(&str, " ");
	    bu_vls_strcat(&str, argv[1]);
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
	fp = fopen(argv[1], "wb");
	if (fp == NULL) {
	    perror(argv[1]);
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "plot stored in %s\n", argv[1]);
	is_pipe = 0;
    }

    dl_plot(gedp, fp, gedp->ged_gvp->gv_model2view, floating, gedp->ged_gvp->gv_center, gedp->ged_gvp->gv_scale, Three_D, Z_clip);

    if (is_pipe)
	(void)pclose(fp);
    else
	(void)fclose(fp);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_PLOT_COMMANDS(X, XID) \
    X(plot, ged_plot_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_PLOT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_plot", 1, GED_PLOT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
