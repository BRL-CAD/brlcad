/*                        T C L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#if __STDC__ || USE_PROTOTYPES
#	define	TCLCAD_EXTERN(type_and_name, args)	extern type_and_name args
#	define	TCLCAD_ARGS(args)			args
#else
#	define	TCLCAD_EXTERN(type_and_name, args)	extern type_and_name()
#	define	TCLCAD_ARGS(args)			()
#endif

#define GED_OBJ_NUM_VIEWS 4
#define GED_OBJ_FB_MODE_OFF 0
#define GED_OBJ_FB_MODE_UNDERLAY 1
#define GED_OBJ_FB_MODE_INTERLAY 2
#define GED_OBJ_FB_MODE_OVERLAY  3

struct ged_dm_view {
    struct bu_list	l;
    struct bu_vls	gdv_name;
    struct ged_view	*gdv_view;
    struct dm		*gdv_dmp;
    struct fbserv_obj	gdv_fbs;
    struct ged_obj	*gdv_gop; /* Pointer back to its ged object */
};

struct ged_obj {
    struct bu_list	l;
    struct ged		*go_gedp;
#if 1
    struct ged_dm_view	go_head_views;
#else
    struct ged_view	*go_views[GED_OBJ_NUM_VIEWS];
    struct dm		*go_dmp;
#endif
    struct bu_vls	go_name;
    struct bu_observer	go_observers;
    Tcl_Interp		*go_interp;
};

#define GED_OBJ_NULL (struct ged_obj *)0


TCLCAD_EXPORT TCLCAD_EXTERN(int tclcad_tk_setup, (Tcl_Interp *interp));
TCLCAD_EXPORT TCLCAD_EXTERN(void tclcad_auto_path, (Tcl_Interp *interp));
TCLCAD_EXPORT TCLCAD_EXTERN(void tclcad_tcl_library, (Tcl_Interp *interp));
TCLCAD_EXPORT TCLCAD_EXTERN(int Tclcad_Init, (Tcl_Interp *interp));

/* defined in tcl.c */
TCLCAD_EXPORT BU_EXTERN(int Go_Init,
			(Tcl_Interp *interp));


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
