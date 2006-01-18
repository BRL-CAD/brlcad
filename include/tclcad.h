/*                        T C L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @file tclcad.h
 *
 *  Header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 *  This library contains convenience routines for preparing and
 *  initializing Tcl.
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */

#ifndef __TCLCAD_H__
#define __TCLCAD_H__

#include "common.h"

#include "tcl.h"
#include "machine.h"

__BEGIN_DECLS

#ifndef TCLCAD_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef TCLCAD_EXPORT_DLL
#      define TCLCAD_EXPORT __declspec(dllexport)
#    else
#      define TCLCAD_EXPORT __declspec(dllimport)
#    endif
#  else
#    define TCLCAD_EXPORT
#  endif
#endif

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 *  The setting of USE_PROTOTYPES is done in machine.h
 */
#ifdef USE_PROTOTYPES
#	define	TCLCAD_EXTERN(type_and_name,args)	extern type_and_name args
#	define	TCLCAD_ARGS(args)			args
#else
#	define	TCLCAD_EXTERN(type_and_name,args)	extern type_and_name()
#	define	TCLCAD_ARGS(args)			()
#endif


TCLCAD_EXPORT TCLCAD_EXTERN(int tclcad_tk_setup, (Tcl_Interp *interp));
TCLCAD_EXPORT TCLCAD_EXTERN(void tclcad_auto_path, (Tcl_Interp *interp));

__END_DECLS

#endif /* __TCLCAD_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
