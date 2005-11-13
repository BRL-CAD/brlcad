/*                         B I N F O . C
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
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
/** @file binfo.c
 *		G 2 A S C . C
 *
 *
 *  Usage:  binfo
 *
 *  Author -
 *  	Charles M Kennedy
 *	Christopher Sean Morrison
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "tcl.h"

extern const char bu_version[];
extern const char bn_version[];
extern const char rt_version[];
extern const char fb_version[];

static char usage[] = "\
Usage: binfo \
 returns information about the BRL-CAD runtime environment characteristics\n\
";

int
main(int argc, char *argv[])
{
  if (argc > 0) {
    printf("%s", usage);
  }

  printf("binfo: bu_version=[%s]\n", bu_version);
  printf("binfo: bn_version=[%s]\n", bn_version);
#if 0
  printf("binfo: rt_version=[%s]\n", rt_version);
#endif
  printf("binfo: fb_version=[%s]\n", fb_version);

  exit(0);
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
