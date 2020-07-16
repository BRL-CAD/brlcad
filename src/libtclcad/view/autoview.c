/*                        A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2020 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/view/autoview.c
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../tclcad_private.h"
#include "../view/view.h"

void
to_autoview_view(struct ged_dm_view *gdvp, const char *scale)
{
    int ret;
    const char *av[3];

    gdvp->gdv_gop->go_gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "autoview";
    av[1] = scale;
    av[2] = NULL;

    if (scale)
	ret = ged_autoview(gdvp->gdv_gop->go_gedp, 2, (const char **)av);
    else
	ret = ged_autoview(gdvp->gdv_gop->go_gedp, 1, (const char **)av);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	}

	to_refresh_view(gdvp);
    }
}

int
to_autoview(struct ged *gedp,
	    int argc,
	    const char *argv[],
	    ged_func_ptr UNUSED(func),
	    const char *usage,
	    int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s [scale]", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argc > 2)
	to_autoview_view(gdvp, argv[2]);
    else
	to_autoview_view(gdvp, NULL);

    return GED_OK;
}


void
to_autoview_all_views(struct tclcad_obj *top)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &top->to_gop->go_head_views.l)) {
	to_autoview_view(gdvp, NULL);
    }
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
