/*                           M R O . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2007 United States Government as represented by
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

/** \addtogroup mro */
/*@{*/

/** @file mro.c
 * @brief  The Multiply Represented Object package.
 *
 *
 *  @author	John R. Anderson
 *
 * @par  Source -
 *      The U. S. Army Research Laboratory
 * @n      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

static const char libbu_vls_RCSid[] = "@(#)$Header$ (BRL)";

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if defined(HAVE_STDARG_H)
/* ANSI C */
# include <stdarg.h>
#endif
#if !defined(HAVE_STDARG_H) && defined(HAVE_VARARGS_H)
/* VARARGS */
# include <varargs.h>
#endif

#include "machine.h"
#include "bu.h"

/**
 *
 */
void
bu_mro_init( struct bu_mro *mrop )
{
	mrop->magic = BU_MRO_MAGIC;
	bu_vls_init( &mrop->string_rep );
	BU_MRO_INVALIDATE( mrop );
}


/**
 *
 */
void
bu_mro_free( struct bu_mro *mrop )
{
	BU_CK_MRO( mrop );

	bu_vls_free( &mrop->string_rep );
	BU_MRO_INVALIDATE( mrop );
}


/**
 *
 */
void
bu_mro_set( struct bu_mro *mrop, const char *string )
{
	BU_CK_MRO( mrop );

	bu_vls_trunc( &mrop->string_rep, 0 );
	bu_vls_strcpy( &mrop->string_rep, string );
	BU_MRO_INVALIDATE( mrop );
}


/**
 *
 */
void
bu_mro_init_with_string( struct bu_mro *mrop, const char *string )
{
	mrop->magic = BU_MRO_MAGIC;
	bu_vls_init( &mrop->string_rep );
	bu_vls_strcpy( &mrop->string_rep, string );
	BU_MRO_INVALIDATE( mrop );
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
