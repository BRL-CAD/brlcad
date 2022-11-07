/*                        A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
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
to_autoview_view(struct bview *gdvp, const char *scale)
{
    int ret;
    const char *av[3];

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    tvd->gedp->ged_gvp = gdvp;
    av[0] = "autoview";
    av[1] = scale;
    av[2] = NULL;

    if (scale)
	ret = ged_autoview(tvd->gedp, 2, (const char **)av);
    else
	ret = ged_autoview(tvd->gedp, 1, (const char **)av);

    if (ret == BRLCAD_OK) {
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
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
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s [scale]", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (argc > 2)
	to_autoview_view(gdvp, argv[2]);
    else
	to_autoview_view(gdvp, NULL);

    return BRLCAD_OK;
}


void
to_autoview_all_views(struct tclcad_obj *top)
{
    struct bview *gdvp;

    for (size_t i = 0; i < BU_PTBL_LEN(&top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&top->to_gedp->ged_views, i);
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
