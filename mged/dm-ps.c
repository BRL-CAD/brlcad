/*
 *			D M - P S . C
 *
 * A useful hack to allow GED to generate
 * PostScript files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, used for making viewgraphs and photographs
 * of an editing session.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/time.h>		/* for struct timeval */
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "dm-ps.h"

extern void dm_var_init();

int
PS_dm_init(o_dm_list, argc, argv)
struct dm_list *o_dm_list;
int argc;
char *argv[];
{
  dm_var_init(o_dm_list);

  if((dmp = dm_open(DM_TYPE_PS, argc, argv)) == DM_NULL)
    return TCL_ERROR;

  zclip_ptr = &((struct ps_vars *)dmp->dm_vars)->zclip;
  curr_dm_list->s_info->opp = &pathName;
  return TCL_OK;
}
