/*                        T C L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libtclcad */
/** @{ */
/** @file tclcad.h
 *
 * @brief
 *  Header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 *  This library contains convenience routines for preparing and
 *  initializing Tcl.
 *
 */

#ifndef __TCLCAD_H__
#define __TCLCAD_H__

#include "common.h"
#include "tcl.h"
#include "ged.h"

#include "fbserv_obj.h"

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


struct tclcad_obj {
    struct bu_list	l;
    struct ged_obj	*to_gop;
    Tcl_Interp		*to_interp;
};

#define TCLCAD_OBJ_NULL (struct tclcad_obj *)0

TCLCAD_EXPORT extern int tclcad_tk_setup(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_auto_path(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_tcl_library(Tcl_Interp *interp);
TCLCAD_EXPORT extern int Tclcad_Init(Tcl_Interp *interp);

/* defined in tclcad_obj.c */
TCLCAD_EXPORT extern int Go_Init(Tcl_Interp *interp);
TCLCAD_EXPORT extern int to_open_tcl(ClientData UNUSED(clientData),
				     Tcl_Interp *interp,
				     int argc,
				     const char **argv);
TCLCAD_EXPORT extern struct application *to_rt_gettrees_application(struct ged *gedp,
								    int argc,
								    const char *argv[]);



__END_DECLS

#endif /* __TCLCAD_H__ */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
