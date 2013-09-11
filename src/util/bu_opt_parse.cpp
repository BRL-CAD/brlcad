/*                 B U _ O P T _ P A R S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"


#include "tclap/CmdLine.h"
#include "bu_opt_parse_private.h"
#include "bu_opt_parse.h"

/* using ideas from Cliff and Sean... */
/**
 * construct all for TCLAP handling, ensure input args get proper values for the caller
 */
extern "C"
int
bu_opt_parse(bu_arg_vars *args[], int argc, char **argv)
{
  int i = 0;
  if (argc < 2)
    bu_exit(EXIT_FAILURE, "ERROR: too few args in %s\n", argv[0]);

  while (args[i]) {
    // handle this arg and fill in the values
    ++i;
  }

/*
  // map inputs to TCLAP:
  string
  TCLAP::ValueArg<int> intArg(short,long,description,required,default,"int");
  cmd->add(intArg);
*/

  return 0; // tmp return
}
