/*                        V P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file mged/vparse.c
 *
 * Routines for interfacing with the LIBBU struct parsing utilities.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdio.h>
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./mged.h"
#include "./mged_dm.h"


void
mged_vls_struct_parse(struct bu_vls *vls,
		      const char *title,
		      struct bu_structparse *how_to_parse,
		      const char *structp,
		      int argc,
		      const char *argv[])
{
    if (argc < 2) {
	/* Bare set command, print out current settings */
	bu_vls_struct_print2(vls, title, how_to_parse, structp);
    } else if (argc == 2) {
	bu_vls_struct_item_named(vls, how_to_parse, argv[1], structp, ' ');
    } else {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	bu_vls_putc(&tmp_vls, '\"');
	bu_struct_parse(&tmp_vls, how_to_parse, structp);
	bu_vls_free(&tmp_vls);
    }
}


void
mged_vls_struct_parse_old(
    struct bu_vls *vls,
    const char *title,
    struct bu_structparse *how_to_parse,
    char *structp,
    int argc,
    const char *argv[])
{
    if (argc < 2) {
	/* Bare set command, print out current settings */
	bu_vls_struct_print2(vls, title, how_to_parse, structp);
    } else if (argc == 2) {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_strcpy(&tmp_vls, argv[1]);
	bu_struct_parse(&tmp_vls, how_to_parse, structp);
	bu_vls_free(&tmp_vls);
    }
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
