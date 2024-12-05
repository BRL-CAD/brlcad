/*                         S H A R E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2024 United States Government as represented by
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
/** @file mged/share.c
 *
 * Description -
 * Routines for sharing resources among display managers.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"

#include "./mged.h"
#include "./mged_dm.h"

#define RESOURCE_TYPE_ADC		0
#define RESOURCE_TYPE_AXES		1
#define RESOURCE_TYPE_COLOR_SCHEMES	2
#define RESOURCE_TYPE_GRID		3
#define RESOURCE_TYPE_MENU		4
#define RESOURCE_TYPE_MGED_VARIABLES	5
#define RESOURCE_TYPE_RUBBER_BAND	6
#define RESOURCE_TYPE_VIEW		7

#define SHARE_RESOURCE(uflag, str, resource, rc, dlp1, dlp2, vls, error_msg) \
    do { \
	if (uflag) { \
	    struct str *strp; \
\
	    if (dlp1->resource->rc > 1) {   /* must be sharing this resource */ \
		--dlp1->resource->rc; \
		strp = dlp1->resource; \
		BU_ALLOC(dlp1->resource, struct str); \
		*dlp1->resource = *strp;        /* struct copy */ \
		dlp1->resource->rc = 1; \
	    } \
	} else { \
	    /* must not be sharing this resource */ \
	    if (dlp1->resource != dlp2->resource) { \
		if (!--dlp2->resource->rc) \
		    bu_free((void *)dlp2->resource, error_msg); \
\
		dlp2->resource = dlp1->resource; \
		++dlp1->resource->rc; \
	    } \
	} \
    } while (0)


extern struct bu_structparse axes_vparse[];
extern struct bu_structparse color_scheme_vparse[];
extern struct bu_structparse grid_vparse[];
extern struct bu_structparse rubber_band_vparse[];
extern struct bu_structparse mged_vparse[];

void free_all_resources(struct mged_dm *dlp);

/*
 * SYNOPSIS
 *	share [-u] res p1 [p2]
 *
 * DESCRIPTION
 *	Provides a mechanism to (un)share resources among display managers.
 *	Currently, there are nine different resources that can be shared.
 *	They are:
 *		ADC AXES COLOR_SCHEMES DISPLAY_LISTS GRID MENU MGED_VARIABLES RUBBER_BAND VIEW
 *
 * EXAMPLES
 *	share res_type p1 p2	--->	causes 'p1' to share its resource of type 'res_type' with 'p2'
 *	share -u res_type p	--->	causes 'p' to no longer share resource of type 'res_type'
 */
int
f_share(ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    int uflag = 0;		/* unshare flag */
    struct mged_dm *dlp1 = MGED_DM_NULL;
    struct mged_dm *dlp2 = MGED_DM_NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_vls_printf(&vls, "helpdevel share");
	Tcl_Eval(interpreter, bu_vls_addr(&vls));

	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argv[1][0] == '-' && argv[1][1] == 'u') {
	uflag = 1;
	--argc;
	++argv;
    }

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	struct bu_vls *pname = dm_get_pathname(m_dmp->dm_dmp);
	if (BU_STR_EQUAL(argv[2], bu_vls_cstr(pname))) {
	    dlp1 = m_dmp;
	    break;
	}
    }

    if (dlp1 == MGED_DM_NULL) {
	Tcl_AppendResult(interpreter, "share: unrecognized path name - ", argv[2], "\n", (char *)NULL);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (!uflag) {
	for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	    struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	    struct bu_vls *pname = dm_get_pathname(m_dmp->dm_dmp);
	    if (BU_STR_EQUAL(argv[3], bu_vls_cstr(pname))) {
		dlp2 = m_dmp;
		break;
	    }
	}

	if (dlp2 == MGED_DM_NULL) {
	    Tcl_AppendResult(interpreter, "share: unrecognized path name - ", argv[3], "\n", (char *)NULL);
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	}

	/* same display manager */
	if (dlp1 == dlp2) {
	    bu_vls_free(&vls);
	    return TCL_OK;
	}
    }

    switch (argv[1][0]) {
	case 'a':
	case 'A':
	    if (argv[1][1] == 'd' || argv[1][1] == 'D')
		SHARE_RESOURCE(uflag, _adc_state, dm_adc_state, adc_rc, dlp1, dlp2, vls, "share: adc_state");
	    else if (argv[1][1] == 'x' || argv[1][1] == 'X')
		SHARE_RESOURCE(uflag, _axes_state, dm_axes_state, ax_rc, dlp1, dlp2, vls, "share: axes_state");
	    else {
		bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
		Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

		bu_vls_free(&vls);
		return TCL_ERROR;
	    }
	    break;
	case 'c':
	case 'C':
	    SHARE_RESOURCE(uflag, _color_scheme, dm_color_scheme, cs_rc, dlp1, dlp2, vls, "share: color_scheme");
	    break;
	case 'd':
	case 'D':
	    {
		struct dm *dmp1;
		struct dm *dmp2 = (struct dm *)NULL;

		dmp1 = dlp1->dm_dmp;
		if (dlp2 != (struct mged_dm *)NULL)
		    dmp2 = dlp2->dm_dmp;

		if (dm_share_dlist(dmp1, dmp2) == TCL_OK) {
		    SHARE_RESOURCE(uflag, _dlist_state, dm_dlist_state, dl_rc, dlp1, dlp2, vls, "share: dlist_state");
		    if (uflag) {
			dlp1->dm_dlist_state->dl_active = dlp1->dm_mged_variables->mv_dlist;

			if (dlp1->dm_mged_variables->mv_dlist) {
			    struct mged_dm *save_dlp;

			    save_dlp = mged_curr_dm;

			    set_curr_dm(s, dlp1);
			    createDLists(s->GEDP->ged_gdp->gd_headDisplay);

			    /* restore */
			    set_curr_dm(s, save_dlp);
			}

			dlp1->dm_dirty = 1;
			dm_set_dirty(dlp1->dm_dmp, 1);
		    } else {
			dlp1->dm_dirty = dlp2->dm_dirty = 1;
			dm_set_dirty(dlp1->dm_dmp, 1);
			dm_set_dirty(dlp2->dm_dmp, 1);
		    }
		}
	    }
	    break;
	case 'g':
	case 'G':
	    SHARE_RESOURCE(uflag, bv_grid_state, dm_grid_state, rc, dlp1, dlp2, vls, "share: grid_state");
	    break;
	case 'm':
	case 'M':
	    SHARE_RESOURCE(uflag, _menu_state, dm_menu_state, ms_rc, dlp1, dlp2, vls, "share: menu_state");
	    break;
	case 'r':
	case 'R':
	    SHARE_RESOURCE(uflag, _rubber_band, dm_rubber_band, rb_rc, dlp1, dlp2, vls, "share: rubber_band");
	    break;
	case 'v':
	case 'V':
	    if ((argv[1][1] == 'a' || argv[1][1] == 'A') &&
		(argv[1][2] == 'r' || argv[1][2] == 'R'))
		SHARE_RESOURCE(uflag, _mged_variables, dm_mged_variables, mv_rc, dlp1, dlp2, vls, "share: mged_variables");
	    else if (argv[1][1] == 'i' || argv[1][1] == 'I') {
		if (!uflag) {
		    /* free dlp2's view_state resources if currently not sharing */
		    if (dlp2->dm_view_state->vs_rc == 1)
			view_ring_destroy(dlp2);
		}

		SHARE_RESOURCE(uflag, _view_state, dm_view_state, vs_rc, dlp1, dlp2, vls, "share: view_state");

		if (uflag) {
		    struct _view_state *ovsp;
		    ovsp = dlp1->dm_view_state;

		    /* initialize dlp1's view_state */
		    if (ovsp != dlp1->dm_view_state)
			view_ring_init(dlp1->dm_view_state, ovsp);
		}
	    } else {
		bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
		Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

		bu_vls_free(&vls);
		return TCL_ERROR;
	    }

	    break;
	default:
	    bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
	    Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

	    bu_vls_free(&vls);
	    return TCL_ERROR;
    }

    if (!uflag) {
	dlp2->dm_dirty = 1;	/* need to redraw this guy */
	dm_set_dirty(dlp2->dm_dmp, 1);
    }

    bu_vls_free(&vls);
    return TCL_OK;
}


/*
 * SYNOPSIS
 *	rset [res_type [res [vals]]]
 *
 * DESCRIPTION
 *	Provides a mechanism to set resource values for some resource.
 *
 * EXAMPLES
 *	rset c bg 0 0 50	--->	sets the background color to dark blue
 */
int
f_rset (ClientData clientData, Tcl_Interp *interpreter, int argc, const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* print values for all resources */
    if (argc == 1) {
	mged_vls_struct_parse(s, &vls, "Axes, res_type - ax", axes_vparse,
			      (const char *)axes_state, argc, argv);
	bu_vls_printf(&vls, "\n");
	mged_vls_struct_parse(s, &vls, "Color Schemes, res_type - c", color_scheme_vparse,
			      (const char *)color_scheme, argc, argv);
	bu_vls_printf(&vls, "\n");
	mged_vls_struct_parse(s, &vls, "Grid, res_type - g", grid_vparse,
			      (const char *)grid_state, argc, argv);
	bu_vls_printf(&vls, "\n");
	mged_vls_struct_parse(s, &vls, "Rubber Band, res_type - r", rubber_band_vparse,
			      (const char *)rubber_band, argc, argv);
	bu_vls_printf(&vls, "\n");
	mged_vls_struct_parse(s, &vls, "MGED Variables, res_type - var", mged_vparse,
			      (const char *)mged_variables, argc, argv);

	Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    switch (argv[1][0]) {
	case 'a':
	case 'A':
	    if (argv[1][1] == 'd' || argv[1][1] == 'D')
		bu_vls_printf(&vls, "rset: use the adc command for the 'adc' resource");
	    else if (argv[1][1] == 'x' || argv[1][1] == 'X')
		mged_vls_struct_parse(s, &vls, "Axes", axes_vparse,
				      (const char *)axes_state, argc-1, argv+1);
	    else {
		bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
		Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

		bu_vls_free(&vls);
		return TCL_ERROR;
	    }
	    break;
	case 'c':
	case 'C':
	    mged_vls_struct_parse(s, &vls, "Color Schemes", color_scheme_vparse,
				  (const char *)color_scheme, argc-1, argv+1);
	    break;
	case 'g':
	case 'G':
	    mged_vls_struct_parse(s, &vls, "Grid", grid_vparse,
				  (const char *)grid_state, argc-1, argv+1);
	    break;
	case 'r':
	case 'R':
	    mged_vls_struct_parse(s, &vls, "Rubber Band", rubber_band_vparse,
				  (const char *)rubber_band, argc-1, argv+1);
	    break;
	case 'v':
	case 'V':
	    if ((argv[1][1] == 'a' || argv[1][1] == 'A') &&
		(argv[1][2] == 'r' || argv[1][2] == 'R'))
		mged_vls_struct_parse(s, &vls, "mged variables", mged_vparse,
				      (const char *)mged_variables, argc-1, argv+1);
	    else if (argv[1][1] == 'i' || argv[1][1] == 'I')
		bu_vls_printf(&vls, "rset: no support available for the 'view' resource");
	    else {
		bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
		Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

		bu_vls_free(&vls);
		return TCL_ERROR;
	    }

	    break;
	default:
	    bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
	    Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);

	    bu_vls_free(&vls);
	    return TCL_ERROR;
    }

    Tcl_AppendResult(interpreter, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
}


/*
 * dlp1 takes control of dlp2's resources. dlp2 is
 * probably on its way out (i.e. being destroyed).
 */
void
usurp_all_resources(struct mged_dm *dlp1, struct mged_dm *dlp2)
{
    free_all_resources(dlp1);
    dlp1->dm_view_state = dlp2->dm_view_state;
    dlp1->dm_adc_state = dlp2->dm_adc_state;
    dlp1->dm_menu_state = dlp2->dm_menu_state;
    dlp1->dm_rubber_band = dlp2->dm_rubber_band;
    dlp1->dm_mged_variables = dlp2->dm_mged_variables;
    dlp1->dm_color_scheme = dlp2->dm_color_scheme;
    dlp1->dm_grid_state = dlp2->dm_grid_state;
    dlp1->dm_axes_state = dlp2->dm_axes_state;

    /* sanity */
    dlp2->dm_view_state = (struct _view_state *)NULL;
    dlp2->dm_adc_state = (struct _adc_state *)NULL;
    dlp2->dm_menu_state = (struct _menu_state *)NULL;
    dlp2->dm_rubber_band = (struct _rubber_band *)NULL;
    dlp2->dm_mged_variables = (struct _mged_variables *)NULL;
    dlp2->dm_color_scheme = (struct _color_scheme *)NULL;
    dlp2->dm_grid_state = (struct bv_grid_state *)NULL;
    dlp2->dm_axes_state = (struct _axes_state *)NULL;

    /* it doesn't make sense to save display list info */
    if (!--dlp2->dm_dlist_state->dl_rc)
	bu_free((void *)mged_curr_dm->dm_dlist_state, "usurp_all_resources: _dlist_state");
}


/*
 * - decrement the reference count of all resources
 * - free all resources that are not being used
 */
void
free_all_resources(struct mged_dm *dlp)
{
    if (!--dlp->dm_view_state->vs_rc) {
	view_ring_destroy(dlp);
	bu_free((void *)dlp->dm_view_state, "free_all_resources: view_state");
    }

    if (!--dlp->dm_adc_state->adc_rc)
	bu_free((void *)dlp->dm_adc_state, "free_all_resources: adc_state");

    if (!--dlp->dm_menu_state->ms_rc)
	bu_free((void *)dlp->dm_menu_state, "free_all_resources: menu_state");

    if (!--dlp->dm_rubber_band->rb_rc)
	bu_free((void *)dlp->dm_rubber_band, "free_all_resources: rubber_band");

    if (!--dlp->dm_mged_variables->mv_rc)
	bu_free((void *)dlp->dm_mged_variables, "free_all_resources: mged_variables");

    if (!--dlp->dm_color_scheme->cs_rc)
	bu_free((void *)dlp->dm_color_scheme, "free_all_resources: color_scheme");

    if (!--dlp->dm_grid_state->rc)
	bu_free((void *)dlp->dm_grid_state, "free_all_resources: grid_state");

    if (!--dlp->dm_axes_state->ax_rc)
	bu_free((void *)dlp->dm_axes_state, "free_all_resources: axes_state");
}


void
share_dlist(struct mged_dm *dlp2)
{
    if (!dm_get_displaylist(dlp2->dm_dmp))
	return;

    for (size_t di = 0; di < BU_PTBL_LEN(&active_dm_set); di++) {
	struct mged_dm *dlp1 = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, di);
	if (dlp1 != dlp2 &&
	    dm_get_type(dlp1->dm_dmp) == dm_get_type(dlp2->dm_dmp) && dm_get_dname(dlp1->dm_dmp) && dm_get_dname(dlp2->dm_dmp) &&
	    !bu_vls_strcmp(dm_get_dname(dlp1->dm_dmp), dm_get_dname(dlp2->dm_dmp))) {
	    if (dm_share_dlist(dlp1->dm_dmp, dlp2->dm_dmp) == TCL_OK) {
		struct bu_vls vls = BU_VLS_INIT_ZERO;

		SHARE_RESOURCE(0, _dlist_state, dm_dlist_state, dl_rc, dlp1, dlp2, vls, "share: dlist_state");
		dlp1->dm_dirty = dlp2->dm_dirty = 1;
		dm_set_dirty(dlp1->dm_dmp, 1);
		dm_set_dirty(dlp2->dm_dmp, 1);
		bu_vls_free(&vls);
	    }

	    break;
	}
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
