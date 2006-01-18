/*                        F B _ L O G . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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

/** \addtogroup fb */
/*@{*/
/** @file fb_log.c
 * Log a framebuffer library event in the Standard way.
    Author -
    Gary S. Moss

    Source -
    SECAD/VLD Computing Consortium, Bldg 394
    The U. S. Army Ballistic Research Laboratory
    Aberdeen Proving Ground, Maryland  21005-5066


    $Header$ (BRL)
*/
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STDARG_H
#  include <stdarg.h>
#else
#  if HAVE_VARARGS_H
#    include <varargs.h>
#  endif
#endif

#include "machine.h"
#include "fb.h"

#if defined(HAVE_STDARG_H)

/*
 *  			F B _ L O G
 *
 *  Log a framebuffer library event in the Standard way.
 */
void
fb_log( char *fmt, ... )
{
    va_list ap;

    va_start( ap, fmt );
    (void)vfprintf( stderr, fmt, ap );
    va_end(ap);
}

#else  /* defined(HAVE_STDARG_H) */
#  if defined(HAVE_VARARGS_H)

/* VARARGS */
void
fb_log( char *fmt, va_dcl va_alist )
{
    va_list		ap;
    va_start( ap );
#    ifdef HAVE_VPRINTF
    (void) vfprintf( stderr, fmt, ap);
#     else
    (void) _doprnt( fmt, ap, stderr );
#     endif
    va_end( ap );
    return;
}

#  else /* defined(HAVE_VARARGS_H) */

void
fb_log( fmt, a,b,c,d,e,f,g,h,i )
     char	*fmt;
{
    fprintf( stderr, fmt, a,b,c,d,e,f,g,h,i );
}


#  endif	/* defined(HAVE_VARARGS_H) */
#endif  /* defined(HAVE_STDARG_H) */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
