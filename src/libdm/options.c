/*                       O P T I O N S . C
 * BRL-CAD
 *
 * Copyright (C) 1999-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file options.c
 *
 * Option processing routines.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */
#include "common.h"



#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

int
dm_processOptions(struct dm *dmp, struct bu_vls *init_proc_vls, int argc, char **argv)
{
  register int c;

  bu_optind = 0;	 /* re-init bu_getopt */
  bu_opterr = 0;
  while((c = bu_getopt(argc, argv, "N:S:W:s:d:i:n:t:")) != EOF){
    switch(c){
    case 'N':
      dmp->dm_height = atoi(bu_optarg);
      break;
    case 'S':
    case 's':
      dmp->dm_width = dmp->dm_height = atoi(bu_optarg);
      break;
    case 'W':
      dmp->dm_width = atoi(bu_optarg);
      break;
    case 'd':
      bu_vls_strcpy(&dmp->dm_dName, bu_optarg);
      break;
    case 'i':
      bu_vls_strcpy(init_proc_vls, bu_optarg);
      break;
    case 'n':
      if(*bu_optarg != '.')
	bu_vls_printf(&dmp->dm_pathName, ".%s", bu_optarg);
      else
	bu_vls_strcpy(&dmp->dm_pathName, bu_optarg);
      break;
    case 't':
      dmp->dm_top = atoi(bu_optarg);
      break;
    default:
      bu_log("dm_processOptions: option '%c' unknown\n", bu_optopt);
      break;
    }
  }

  return bu_optind;
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
