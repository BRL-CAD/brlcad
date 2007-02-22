/*                          B R E P . C P P 
 * BRL-CAD
 *
 * Copyright (c) 1987-2007 United States Government as represented by
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
/** @file brep.c
 *
 *  Library for writing BREP objects into
 *  MGED databases from arbitrary procedures.
 *
 *  Authors -
 *	Jason Owens
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "wdb.h"


/*
 *                        M K _ B R E P
 *
 *  Create a brep in the geometry file. 
 */
int
mk_brep( struct rt_wdb* file, const char* name, ON_Brep* brep )
{
  struct rt_brep_internal* bi;

  BU_ASSERT(brep != NULL);
  BU_GETSTRUCT(bi, rt_brep_internal);
  bi->magic = RT_BREP_INTERNAL_MAGIC;
  bi->brep = brep;
  return wdb_export(file, name, (genptr_t)bi, ID_BREP, mk_conv2mm);
}
