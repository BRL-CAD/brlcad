/*                         Z O O M . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#include "common.h"

#include "ged.h"

HIDDEN int
zoom(struct ged *gedp, double sf)
{
    gedp->ged_gvp->gv_scale /= sf;
    if (gedp->ged_gvp->gv_scale < RT_MINVIEWSCALE)
	gedp->ged_gvp->gv_scale = RT_MINVIEWSCALE;
    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


int
ged_zoom(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    double sf = 1.0;

    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s scale_factor", argv[0]);
	return (argc == 1) ? GED_HELP : GED_ERROR;
    }

    /* get the scale factor */
    ret = sscanf(argv[1], "%lf", &sf);
    if (ret != 1 || sf < SMALL_FASTF || sf > INFINITY) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad scale factor [%s]", argv[1]);
	return GED_ERROR;
    }

    return zoom(gedp, sf);
}


HIDDEN int
zoom_load(struct ged *gedp)
{
    extern const struct ged_cmd *zoom_cmd(void);

    const struct ged_cmd *cmd = zoom_cmd();
    return gedp->add(cmd);
}


HIDDEN void
zoom_unload(struct ged *gedp)
{
    gedp->del("zoom");
}


const struct ged_cmd *
zoom_cmd(void)
{
    static struct ged_cmd cmd = {
	BU_LIST_INIT_ZERO,
	"zoom",
	"zoom view by specified scale factor",
	"zoom",
	&zoom_load,
	&zoom_unload,
	&ged_zoom
    };
    BU_LIST_MAGIC_SET(&(cmd.l), GED_CMD_MAGIC);
    return &cmd;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
