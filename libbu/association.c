/*
 *			A S S O C I A T I O N . C
 *
 *  Author -
 *	Paul Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char libbu_association_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#include "machine.h"
#include "externs.h"
#include "bu.h"

/*
 *			 B U _ A S S O C I A T I O N
 *
 *	    Look up the association for a specified value
 *
 *	This function reads the specified file, searches for the
 *	first line of the form
 *
 *		    <value><field_sep>...
 *
 *	and returns the rest of the line beyond the field separator.
 */
struct bu_vls *bu_association (fname, value, field_sep)

const char	*fname;
const char	*value;
int	field_sep;

{
    char		*cp;
    FILE		*fp;
    int			len;
    struct bu_vls	*vp = 0;
    struct bu_vls	buffer;

	/* XXX NONPARALLEL */
	/* I'd prefer using "bu_open_mapped_file()" here instead, I think  -Mike */
    if ((fp = fopen(fname, "r")) == NULL)
    {
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

    do
    {
	bu_vls_trunc(&buffer, 0);
	if (bu_vls_gets(&buffer, fp) == -1)
	    goto wrap_up;
	cp = bu_vls_addr(&buffer);

    } while ((*cp != *value) || (*(cp + len) != field_sep)
	  || (strncmp(cp, value, len) != 0));

    vp = (struct bu_vls *)
	    bu_malloc(sizeof(struct bu_vls), "value of bu_association");
    bu_vls_init(vp);
    bu_vls_strcpy(vp, cp + len + 1);

wrap_up:
    bu_vls_trunc(&buffer, 0);
    fclose(fp);
    return (vp);
}
