/*		R T A S S O C . C
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
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"

/*
 *			 R T _ A S S O C ( )
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
struct rt_vls *rt_assoc (fname, value, field_sep)

char	*fname;
char	*value;
int	field_sep;

{
    char		*cp;
    FILE		*fp;
    int			len;
    struct rt_vls	*vp = 0;
    struct rt_vls	buffer;

    if ((fp = fopen(fname, "r")) == NULL)
    {
	/*	XXX
	 *	Should we be exiting here?
	 *	I don't want to just return 0,
	 *	because the application probably needs to distinguish
	 *	between ERROR_PERFORMING_THE_LOOKUP
	 *	and VALUE_NOT_FOUND.
	 */
	rt_log("rt_assoc:  Cannot open association file '%s'\n", fname);
	exit (1);
    }

    rt_vls_init(&buffer);
    len = strlen(value);

    do
    {
	rt_vls_trunc(&buffer, 0);
	if (rt_vls_gets(&buffer, fp) == -1)
	    goto wrap_up;
	cp = rt_vls_addr(&buffer);

    } while ((*cp != *value) || (*(cp + len) != field_sep)
	  || (strncmp(cp, value, len) != 0));

    vp = (struct rt_vls *)
	    rt_malloc(sizeof(struct rt_vls), "value of rt_assoc");
    rt_vls_init(vp);
    rt_vls_strcpy(vp, cp + len + 1);

wrap_up:
    rt_vls_trunc(&buffer, 0);
    return (vp);
}
