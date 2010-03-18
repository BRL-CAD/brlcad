/*                       O B J _ P A R S E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
 *
 */
/** @file obj_parser.c
 *
 *  Handling routines for obj parsing
 *
 */

/* interface header */
#include "./obj_parser.h"

#include <stdio.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#else
#  ifdef HAVE_INTTYPES_H
#    include <inttypes.h>
#  endif
#endif

#include "bu.h"
#include "vmath.h"

extern FILE *yyin;
extern int yyparse (void);

int main(int argc, char *argv[]) 
{
  if (argc > 0) {
     printf("Reading from %s\n", argv[1]);
     yyin = fopen(argv[1], "r");
     if (!yyin)
     {
         perror("Unable to open file");
         return -1;
     }
     while (!feof(yyin)) {
    	 yyparse();
     }
     if (yyin) {
     	 fclose(yyin);
     }
 } else {
     printf("Reading from stdin\n");
     yyin = stdin;
     yyparse();
 }

 return 0;
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
