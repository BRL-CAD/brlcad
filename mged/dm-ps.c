/*
 *			D M - P S . C
 *
 *  Routines specific to MGED's use of LIBDM's Postscript display manager.
 *
 *  Author -
 *	Robert G. Parker
 *  
 *  Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifndef WIN32
#include <sys/time.h>		/* for struct timeval */
#endif
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "dm-ps.h"

extern void dm_var_init(struct dm_list *initial_dm_list);

int
PS_dm_init(struct dm_list *o_dm_list, int argc, char **argv)
{
  dm_var_init(o_dm_list);

  if ((dmp = dm_open(interp, DM_TYPE_PS, argc, argv)) == DM_NULL)
    return TCL_ERROR;

  return TCL_OK;
}
