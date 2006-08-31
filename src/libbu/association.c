/*                   A S S O C I A T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2006 United States Government as represented by
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

/** \addtogroup bu_log */
/*@{*/
/** @file association.c
 * Look up the association for a specified value.
 *
 *  @author -
 *	Paul Tanenbaum
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char libbu_association_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#if HAVE_STRING_H
#  include <string.h>
#endif

#include "machine.h"
#include "bu.h"


/**
 *			 B U _ A S S O C I A T I O N
 *
 *	    Look up the association for a specified value
 *
 *	This function reads the specified file, searches for the
 *	first line of the form
 *
@code
		    <value><field_sep>...
@endcode
 *
 *	and returns the rest of the line beyond the field separator.
 */
struct bu_vls *
bu_association (const char *fname,
		const char *value,
		int        field_sep)
{
    char		*cp;
    FILE		*fp;
    size_t		len;
    struct bu_vls	*vp = 0;
    struct bu_vls	buffer;

	/* XXX NONPARALLEL */
	/* I'd prefer using "bu_open_mapped_file()" here instead, I think  -Mike */
    if ((fp = fopen(fname, "r")) == NULL) {
	/*	XXX
	 *	Should we be exiting here?
	 *	I don't want to just return 0,
	 *	because the application probably needs to distinguish
	 *	between ERROR_PERFORMING_THE_LOOKUP
	 *	and VALUE_NOT_FOUND.
	 */
	bu_log("bu_association:  Cannot open association file '%s'\n", fname);
	exit (1);
    }

    bu_vls_init(&buffer);
    len = strlen(value);

    do {
	bu_vls_trunc(&buffer, 0);
	if (bu_vls_gets(&buffer, fp) == -1)
	    goto wrap_up;
	cp = bu_vls_addr(&buffer);

    } while ((*cp != *value) || (*(cp + len) != field_sep)
	     || (strncmp(cp, value, len) != 0));

    vp = (struct bu_vls *) bu_malloc(sizeof(struct bu_vls), "value of bu_association");
    bu_vls_init(vp);
    bu_vls_strcpy(vp, cp + len + 1);

wrap_up:
    bu_vls_trunc(&buffer, 0);
    fclose(fp);
    return (vp);
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
