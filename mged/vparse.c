/*
 *				V P A R S E . C
 *
 * Description -
 *	Routines for interfacing with the LIBBU struct parsing
 *	utilities.
 *	
 * Source -
 *      SLAD CAD Team
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005
 *
 * Authors -
 *      Robert G. Parker
 *	Lee A. Butler
 *      Glenn Durfee
 *
 * Copyright Notice -
 *      This software is Copyright (C) 1998 by the United States Army.
 *      All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"

void
mged_vls_struct_parse(vls, title, how_to_parse, structp, argc, argv)
struct bu_vls *vls;
char *title;
struct bu_structparse *how_to_parse;
char *structp;
int argc;
char *argv[];
{
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);
  start_catching_output(vls);

  if (argc < 2) {
    /* Bare set command, print out current settings */
    bu_struct_print(title, how_to_parse, structp);
  } else if (argc == 2) {
    bu_vls_struct_item_named(&tmp_vls, how_to_parse, argv[1], structp, ' ');
    bu_log("%s", bu_vls_addr(&tmp_vls));
  } else {
    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
    bu_vls_from_argv(&tmp_vls, argc-2, argv+2);
    bu_vls_putc(&tmp_vls, '\"');
    bu_struct_parse(&tmp_vls, how_to_parse, structp);
  }

  stop_catching_output(vls);
  bu_vls_free(&tmp_vls);
}

void
mged_vls_struct_parse_old(vls, title, how_to_parse, structp, argc, argv)
struct bu_vls *vls;
char *title;
struct bu_structparse *how_to_parse;
char *structp;
int argc;
char *argv[];
{
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);
  start_catching_output(vls);

  if (argc < 2) {
    /* Bare set command, print out current settings */
    bu_struct_print(title, how_to_parse, structp);
  } else if (argc == 2) {
    bu_vls_strcpy(&tmp_vls, argv[1]);
    bu_struct_parse(&tmp_vls, how_to_parse, structp);
  }

  stop_catching_output(vls);
  bu_vls_free(&tmp_vls);
}
