/*
 *			M R O . C
 *
 *  The Multiply Represented Object package.
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 2001-2004 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */
static const char libbu_vls_RCSid[] = "@(#)$Header$ (BRL)";

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#ifdef USE_STRING_H
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
#include "externs.h"
#include "bu.h"

void
bu_mro_init( struct bu_mro *mrop )
{
	mrop->magic = BU_MRO_MAGIC;
	bu_vls_init( &mrop->string_rep );
	BU_MRO_INVALIDATE( mrop );
}

void
bu_mro_free( struct bu_mro *mrop )
{
	BU_CK_MRO( mrop );

	bu_vls_free( &mrop->string_rep );
	BU_MRO_INVALIDATE( mrop );
}

void
bu_mro_set( struct bu_mro *mrop, const char *string )
{
	BU_CK_MRO( mrop );

	bu_vls_trunc( &mrop->string_rep, 0 );
	bu_vls_strcpy( &mrop->string_rep, string );
	BU_MRO_INVALIDATE( mrop );
}

void
bu_mro_init_with_string( struct bu_mro *mrop, const char *string )
{
	mrop->magic = BU_MRO_MAGIC;
	bu_vls_init( &mrop->string_rep );
	bu_vls_strcpy( &mrop->string_rep, string );
	BU_MRO_INVALIDATE( mrop );
}
