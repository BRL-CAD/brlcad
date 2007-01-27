/*                     S T R _ M A N I P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file str_manip.c
 *
 */

/*	STR_MANIP.C	*/
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "common.h"


#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "./nirt.h"

int str_dbl(char *buf, double *Result)
{
	int 	status = 0; 		/* 0, function failed - 1, success */
	int	sign = POS;		/* POS or NEG sign		   */
	double	Frac = .1;              /* used in the case of a fraction  */
	double  Value = 0.0;		/* current value of the double	   */
	int	i = 0;			/* current position of *buf        */

	if (*buf == '-')      /* check for a minus sign */
	{
		sign = NEG;
		++i;
		++buf;
	}
	while (isdigit(*buf)) /* update Value while there is a digit */
	{
		status = 1;
		Value *= 10.0;
		Value += *buf++ - '0';
		++i;
	}
	if (*buf == '.')      /* check for a decimal point */
	{
		++i;
		++buf;
		while (isdigit(*buf))  /* update Value while there is a digit */
		{
			++i;
			status = 1;
			Value += (*buf++ - '0') * Frac;
			Frac *= .1;
		}
	}
	if (status == 0)
		return(0);    /* if function failed return a 0 */
	else
	{
		if (sign == NEG)
			Value *= -1;
		*Result = Value;
		return(i);    /* if function succeeds return position of *buf */
	}

}

#ifndef HAVE_BASENAME
char *basename(char *string)
{
    char	*sp;
    char        *sp2;

    for (sp = string + strlen(string); (sp > string) && (*sp != '/'); --sp)
	;
    if (*sp == '/')
	++sp;
    sp2 = sp;
    while (*sp2 != '{' && *sp2 != '\0')
	++sp2;
    *sp2 = '\0';
    return (sp);
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
