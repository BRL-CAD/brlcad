/*
 *			D M - P L O T . C
 *
 * An unsatisfying (but useful) hack to allow GED to generate
 * UNIX-plot files that not only contain the drawn objects, but
 * also contain the faceplate display as well.
 * Mostly, a useful hack for making viewgraphs and photographs
 * of an editing session.
 * We assume that the UNIX-plot filter used can at least discard
 * the non-standard extention to specify color (a Gwyn@BRL addition).
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
#include "dm-plot.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./solid.h"

extern void color_soltab();

int Plot_dm_init();

int
Plot_dm_init(argc, argv)
int argc;
char *argv[];
{
  if(dmp->dmr_init(dmp, color_soltab, argc, argv) == TCL_ERROR)
    return TCL_ERROR;

  return dmp->dmr_open(dmp);
}

#if 0
static int
Plot_close(p)
genptr_t *p;
{
  bu_free(p, "mged_plot_vars");
  return TCL_OK;
}
#endif
