/*
 * tclPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in platform specific
 *	portability files.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef _TCLPORT
#define _TCLPORT

#include "tcl.h"

#if defined(__WIN32__)
#   include "../win/tclWinPort.h"
#else
#   if defined(MAC_TCL)
#      include "tclMacPort.h"
#   else
#      include "../unix/tclUnixPort.h"
#   endif
#endif

#if !defined(TCL_WIDE_INT_IS_LONG) && !defined(LLONG_MIN)
#   ifdef LLONG_BIT
#      define LLONG_MIN ((Tcl_WideInt)(Tcl_LongAsWide(1)<<(LLONG_BIT-1)))
#   else
/* Assume we're on a system with a 64-bit 'long long' type */
#      define LLONG_MIN ((Tcl_WideInt)(Tcl_LongAsWide(1)<<63))
#   endif
/* Assume that if LLONG_MIN is undefined, then so is LLONG_MAX */
#   define LLONG_MAX (~LLONG_MIN)
#endif


#endif /* _TCLPORT */
