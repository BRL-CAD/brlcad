/*                         S H A R E . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file share.c
 *
 * Description -
 *	Routines for sharing resources among display managers.
 *
 * Functions -
 *	f_share				command to share/unshare resources
 *	f_rset				command to set resources
 *
 * Source -
 *      SLAD CAD Team
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *      Robert G. Parker
 */

#include "common.h"


#include <math.h>
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

#define RESOURCE_TYPE_ADC		0
#define RESOURCE_TYPE_AXES		1
#define RESOURCE_TYPE_COLOR_SCHEMES	2
#define RESOURCE_TYPE_GRID		3
#define RESOURCE_TYPE_MENU		4
#define RESOURCE_TYPE_MGED_VARIABLES	5
#define RESOURCE_TYPE_RUBBER_BAND	6
#define RESOURCE_TYPE_VIEW		7

#define SHARE_RESOURCE(uflag,str,resource,rc,dlp1,dlp2,vls,error_msg) { \
    if (uflag) { \
      struct str *strp; \
\
      if (dlp1->resource->rc > 1) {   /* must be sharing this resource */ \
	--dlp1->resource->rc; \
	strp = dlp1->resource; \
	BU_GETSTRUCT(dlp1->resource, str); \
	*dlp1->resource = *strp;        /* struct copy */ \
	dlp1->resource->rc = 1; \
      } \
    } else { \
      /* must not be sharing this resource */ \
      if (dlp1->resource != dlp2->resource) { \
        if (!--dlp2->resource->rc) \
          bu_free((genptr_t)dlp2->resource, error_msg); \
\
          dlp2->resource = dlp1->resource; \
          ++dlp1->resource->rc; \
      } \
    } \
}

extern void mged_vls_struct_parse(struct bu_vls *vls, char *title, struct bu_structparse *how_to_parse, const char *structp, int argc, char **argv); /* defined in vparse.c */
extern void view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2); /* defined in chgview.c */

extern struct bu_structparse axes_vparse[];
extern struct bu_structparse color_scheme_vparse[];
extern struct bu_structparse grid_vparse[];
extern struct bu_structparse rubber_band_vparse[];
extern struct bu_structparse mged_vparse[];

void free_all_resources(struct dm_list *dlp);

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
f_share(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  register int uflag = 0;		/* unshare flag */
  struct dm_list *dlp1 = (struct dm_list *)NULL;
  struct dm_list *dlp2 = (struct dm_list *)NULL;
  struct bu_vls vls;

  bu_vls_init(&vls);

  if (argc != 4) {
    bu_vls_printf(&vls, "helpdevel share");
    Tcl_Eval(interp, bu_vls_addr(&vls));

    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (argv[1][0] == '-' && argv[1][1] == 'u') {
    uflag = 1;
    --argc;
    ++argv;
  }

  FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l)
    if (!strcmp(argv[2], bu_vls_addr(&dlp1->dml_dmp->dm_pathName)))
      break;

  if (dlp1 == &head_dm_list) {
    Tcl_AppendResult(interp, "share: unrecognized pathName - ",
		     argv[2], "\n", (char *)NULL);

    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (!uflag) {
    FOR_ALL_DISPLAYS(dlp2, &head_dm_list.l)
      if (!strcmp(argv[3], bu_vls_addr(&dlp2->dml_dmp->dm_pathName)))
	break;

    if (dlp2 == &head_dm_list) {
      Tcl_AppendResult(interp, "share: unrecognized pathName - ",
		       argv[3], "\n", (char *)NULL);

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
      SHARE_RESOURCE(uflag,_adc_state,dml_adc_state,adc_rc,dlp1,dlp2,vls,"share: adc_state")
    else if (argv[1][1] == 'x' || argv[1][1] == 'X')
      SHARE_RESOURCE(uflag,_axes_state,dml_axes_state,ax_rc,dlp1,dlp2,vls,"share: axes_state")
    else {
      bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

      bu_vls_free(&vls);
      return TCL_ERROR;
    }
    break;
  case 'c':
  case 'C':
    SHARE_RESOURCE(uflag,_color_scheme,dml_color_scheme,cs_rc,dlp1,dlp2,vls,"share: color_scheme")
    break;
  case 'd':
  case 'D':
    {
      struct dm *dmp1;
      struct dm *dmp2 = (struct dm *)NULL;

      dmp1 = dlp1->dml_dmp;
      if (dlp2 != (struct dm_list *)NULL)
	dmp2 = dlp2->dml_dmp;

      if (dm_share_dlist(dmp1, dmp2) == TCL_OK) {
	SHARE_RESOURCE(uflag,_dlist_state,dml_dlist_state,dl_rc,dlp1,dlp2,vls,"share: dlist_state");
	if (uflag) {
	  dlp1->dml_dlist_state->dl_active = dlp1->dml_mged_variables->mv_dlist;

	  if (dlp1->dml_mged_variables->mv_dlist) {
	    struct dm_list *save_dlp;

	    save_dlp = curr_dm_list;

	    curr_dm_list = dlp1;
	    createDLists(&dgop->dgo_headSolid);

	    /* restore */
	    curr_dm_list = save_dlp;
	  }

	  dlp1->dml_dirty = 1;
	} else {
	  dlp1->dml_dirty = dlp2->dml_dirty = 1;
	}
      }
    }
    break;
  case 'g':
  case 'G':
    SHARE_RESOURCE(uflag,_grid_state,dml_grid_state,gr_rc,dlp1,dlp2,vls,"share: grid_state")
    break;
  case 'm':
  case 'M':
    SHARE_RESOURCE(uflag,_menu_state,dml_menu_state,ms_rc,dlp1,dlp2,vls,"share: menu_state")
    break;
  case 'r':
  case 'R':
    SHARE_RESOURCE(uflag,_rubber_band,dml_rubber_band,rb_rc,dlp1,dlp2,vls,"share: rubber_band")
    break;
  case 'v':
  case 'V':
    if ((argv[1][1] == 'a' || argv[1][1] == 'A') &&
	(argv[1][2] == 'r' || argv[1][2] == 'R'))
      SHARE_RESOURCE(uflag,_mged_variables,dml_mged_variables,mv_rc,dlp1,dlp2,vls,"share: mged_variables")
    else if (argv[1][1] == 'i' || argv[1][1] == 'I') {
      if (uflag) {
	struct _view_state *ovsp;

	ovsp = dlp1->dml_view_state;
	SHARE_RESOURCE(uflag,_view_state,dml_view_state,vs_rc,dlp1,dlp2,vls,"share: view_state")

	/* initialize dlp1's view_state */
	if (ovsp != dlp1->dml_view_state)
	  view_ring_init(dlp1->dml_view_state, ovsp);
      } else {
	/* free dlp2's view_state resources if currently not sharing */
	if (dlp2->dml_view_state->vs_rc == 1)
	  view_ring_destroy(dlp2);

	SHARE_RESOURCE(uflag,_view_state,dml_view_state,vs_rc,dlp1,dlp2,vls,"share: view_state")
      }
    }else {
      bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    break;
  default:
    bu_vls_printf(&vls, "share: resource type '%s' unknown\n", argv[1]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (!uflag) 
    dlp2->dml_dirty = 1;	/* need to redraw this guy */

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
f_rset (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct bu_vls vls;

  bu_vls_init(&vls);

  /* print values for all resources */
  if (argc == 1) {
    mged_vls_struct_parse(&vls, "Axes, res_type - ax", axes_vparse,
			  (const char *)axes_state, argc, argv);
    bu_vls_printf(&vls, "\n");
    mged_vls_struct_parse(&vls, "Color Schemes, res_type - c", color_scheme_vparse,
			  (const char *)color_scheme, argc, argv);
    bu_vls_printf(&vls, "\n");
    mged_vls_struct_parse(&vls, "Grid, res_type - g", grid_vparse,
			  (const char *)grid_state, argc, argv);
    bu_vls_printf(&vls, "\n");
    mged_vls_struct_parse(&vls, "Rubber Band, res_type - r", rubber_band_vparse,
			  (const char *)rubber_band, argc, argv);
    bu_vls_printf(&vls, "\n");
    mged_vls_struct_parse(&vls, "MGED Variables, res_type - var", mged_vparse,
			  (const char *)mged_variables, argc, argv);

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
  }

  switch (argv[1][0]) {
  case 'a':
  case 'A':
    if (argv[1][1] == 'd' || argv[1][1] == 'D')
      bu_vls_printf(&vls, "rset: use the adc command for the 'adc' resource");
    else if (argv[1][1] == 'x' || argv[1][1] == 'X')
      mged_vls_struct_parse(&vls, "Axes", axes_vparse,
			    (const char *)axes_state, argc-1, argv+1);
    else {
      bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

      bu_vls_free(&vls);
      return TCL_ERROR;
    }
    break;
  case 'c':
  case 'C':
    mged_vls_struct_parse(&vls, "Color Schemes", color_scheme_vparse,
			  (const char *)color_scheme, argc-1, argv+1);
    break;
  case 'g':
  case 'G':
    mged_vls_struct_parse(&vls, "Grid", grid_vparse,
			  (const char *)grid_state, argc-1, argv+1);
    break;
  case 'r':
  case 'R':
    mged_vls_struct_parse(&vls, "Rubber Band", rubber_band_vparse,
			  (const char *)rubber_band, argc-1, argv+1);
    break;
  case 'v':
  case 'V':
    if ((argv[1][1] == 'a' || argv[1][1] == 'A') &&
	(argv[1][2] == 'r' || argv[1][2] == 'R'))
      mged_vls_struct_parse(&vls, "mged variables", mged_vparse,
				(const char *)mged_variables, argc-1, argv+1);
    else if (argv[1][1] == 'i' || argv[1][1] == 'I')
      bu_vls_printf(&vls, "rset: no support available for the 'view' resource");
    else {
      bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    break;
  default:
    bu_vls_printf(&vls, "rset: resource type '%s' unknown\n", argv[1]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);

    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

/*
 * dlp1 takes control of dlp2's resources. dlp2 is
 * probably on its way out (i.e. being destroyed).
 */
void
usurp_all_resources(struct dm_list *dlp1, struct dm_list *dlp2)
{
  free_all_resources(dlp1);
  dlp1->dml_view_state = dlp2->dml_view_state;
  dlp1->dml_adc_state = dlp2->dml_adc_state;
  dlp1->dml_menu_state = dlp2->dml_menu_state;
  dlp1->dml_rubber_band = dlp2->dml_rubber_band;
  dlp1->dml_mged_variables = dlp2->dml_mged_variables;
  dlp1->dml_color_scheme = dlp2->dml_color_scheme;
  dlp1->dml_grid_state = dlp2->dml_grid_state;
  dlp1->dml_axes_state = dlp2->dml_axes_state;

  /* sanity */
  dlp2->dml_view_state = (struct _view_state *)NULL;
  dlp2->dml_adc_state = (struct _adc_state *)NULL;
  dlp2->dml_menu_state = (struct _menu_state *)NULL;
  dlp2->dml_rubber_band = (struct _rubber_band *)NULL;
  dlp2->dml_mged_variables = (struct _mged_variables *)NULL;
  dlp2->dml_color_scheme = (struct _color_scheme *)NULL;
  dlp2->dml_grid_state = (struct _grid_state *)NULL;
  dlp2->dml_axes_state = (struct _axes_state *)NULL;

  /* it doesn't make sense to save display list info */
  if(!--dlp2->dml_dlist_state->dl_rc)
    bu_free((genptr_t)curr_dm_list->dml_dlist_state, "usurp_all_resources: _dlist_state");
}

/*
 * - decrement the reference count of all resources
 * - free all resources that are not being used
 */
void
free_all_resources(struct dm_list *dlp)
{
  if(!--dlp->dml_view_state->vs_rc){
    view_ring_destroy(dlp);
    bu_free((genptr_t)dlp->dml_view_state, "free_all_resources: view_state");
  }

  if (!--dlp->dml_adc_state->adc_rc)
    bu_free((genptr_t)dlp->dml_adc_state, "free_all_resources: adc_state");

  if (!--dlp->dml_menu_state->ms_rc)
    bu_free((genptr_t)dlp->dml_menu_state, "free_all_resources: menu_state");

  if (!--dlp->dml_rubber_band->rb_rc)
    bu_free((genptr_t)dlp->dml_rubber_band, "free_all_resources: rubber_band");

  if (!--dlp->dml_mged_variables->mv_rc)
    bu_free((genptr_t)dlp->dml_mged_variables, "free_all_resources: mged_variables");

  if (!--dlp->dml_color_scheme->cs_rc)
    bu_free((genptr_t)dlp->dml_color_scheme, "free_all_resources: color_scheme");

  if (!--dlp->dml_grid_state->gr_rc)
    bu_free((genptr_t)dlp->dml_grid_state, "free_all_resources: grid_state");

  if (!--dlp->dml_axes_state->ax_rc)
    bu_free((genptr_t)dlp->dml_axes_state, "free_all_resources: axes_state");
}

void
share_dlist(struct dm_list *dlp2)
{
  struct dm_list *dlp1;

  if(!dlp2->dml_dmp->dm_displaylist)
    return;

  FOR_ALL_DISPLAYS(dlp1, &head_dm_list.l){
    if(dlp1 != dlp2 && 
       dlp1->dml_dmp->dm_type == dlp2->dml_dmp->dm_type &&
       !strcmp(bu_vls_addr(&dlp1->dml_dmp->dm_dName), bu_vls_addr(&dlp2->dml_dmp->dm_dName))){
      if (dm_share_dlist(dlp1->dml_dmp, dlp2->dml_dmp) == TCL_OK) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	SHARE_RESOURCE(0,_dlist_state,dml_dlist_state,dl_rc,dlp1,dlp2,vls,"share: dlist_state");
	dlp1->dml_dirty = dlp2->dml_dirty = 1;
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
