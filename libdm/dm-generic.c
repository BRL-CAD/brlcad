/*
 *				D M - G E N E R I C . C
 *
 * Generic display manager routines.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 *
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1999 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#include "conf.h"
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

extern struct dm *Nu_open(Tcl_Interp *interp, int argc, char **argv);
extern struct dm *plot_open(Tcl_Interp *interp, int argc, char **argv);
extern struct dm *ps_open(Tcl_Interp *interp, int argc, char **argv);

#ifdef DM_X
extern struct dm *X_open();

#ifdef DM_OGL
extern struct dm *ogl_open();
extern void ogl_fogHint();
extern int ogl_share_dlist();
#endif /* DM_OGL */
#endif /* DM_X */

struct dm *
dm_open(Tcl_Interp *interp, int type, int argc, char **argv)
{
	switch (type) {
	case DM_TYPE_NULL:
		return Nu_open(interp, argc, argv);
	case DM_TYPE_PLOT:
		return plot_open(interp, argc, argv);
	case DM_TYPE_PS:
		return ps_open(interp, argc, argv);
#ifdef DM_X
	case DM_TYPE_X:
		return X_open(interp, argc, argv);
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
		return ogl_open(interp, argc, argv);
#endif
  default:
    break;
  }

  return DM_NULL;
}

/*
 * Provides a way to (un)share display lists. If dmp2 is
 * NULL, then dmp1 will no longer share its display lists.
 */
int
dm_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
  if(dmp1 == DM_NULL)
    return TCL_ERROR;

  /*
   * Only display managers of the same type and using the 
   * same OGL server are allowed to share display lists.
   *
   * XXX - need a better way to check if using the same OGL server.
   */
  if(dmp2 != DM_NULL)
    if(dmp1->dm_type != dmp2->dm_type ||
       strcmp(bu_vls_addr(&dmp1->dm_dName), bu_vls_addr(&dmp2->dm_dName)))
      return TCL_ERROR;

  switch(dmp1->dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return ogl_share_dlist(dmp1, dmp2);
#endif
  default:
    return TCL_ERROR;
  }
}

fastf_t
dm_Xx2Normal(struct dm *dmp, register int x)
{
  return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(struct dm *dmp, register fastf_t f)
{
  return (f * 0.5 + 0.5) * dmp->dm_width;
}

fastf_t
dm_Xy2Normal(struct dm *dmp, register int y, int use_aspect)
{
  if(use_aspect)
    return ((0.5 - y / (fastf_t)dmp->dm_height) / dmp->dm_aspect * 2.0);
  else
    return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}

int
dm_Normal2Xy(struct dm *dmp, register fastf_t f, int use_aspect)
{
  if(use_aspect)
    return (0.5 - f * 0.5 * dmp->dm_aspect) * dmp->dm_height;
  else
    return (0.5 - f * 0.5) * dmp->dm_height;
}

void
dm_fogHint(struct dm *dmp, int fastfog)
{
  switch(dmp->dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    ogl_fogHint(dmp, fastfog);
    return;
#endif
  default:
    return;
  }
}
