/*                   T C L C A D / S E T U P . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file tclcad/setup.h
 *
 * @brief
 *  Setup header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 */

#ifndef TCLCAD_SETUP_H
#define TCLCAD_SETUP_H

#include "common.h"
#include "bu/cmd.h"
#include "bu/process.h"
#include "tcl.h"
#include "dm.h"
#include "tclcad/defines.h"

__BEGIN_DECLS

TCLCAD_EXPORT extern int tclcad_tk_setup(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_auto_path(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_tcl_library(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_bn_setup(Tcl_Interp *interp);

/**
 * Add all the supported Tcl interfaces to LIBRT routines to the list
 * of commands known by the given interpreter.
 *
 * wdb_open creates database "objects" which appear as Tcl procs,
 * which respond to many operations.
 *
 * The "db rt_gettrees" operation in turn creates a ray-traceable
 * object as yet another Tcl proc, which responds to additional
 * operations.
 *
 * Note that the MGED mainline C code automatically runs "wdb_open db"
 * and "wdb_open .inmem" on behalf of the MGED user, which exposes all
 * of this power.
 */
TCLCAD_EXPORT extern void tclcad_rt_setup(Tcl_Interp *interp);


/**
 * Allows LIBRT to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 * "load /usr/brlcad/lib/libbn.so"
 * "load /usr/brlcad/lib/librt.so"
 */
TCLCAD_EXPORT extern int Rt_Init(Tcl_Interp *interp);


/* defined in cmdhist_obj.c */
TCLCAD_EXPORT extern int Cho_Init(Tcl_Interp *interp);

/**
 * Open a command history object.
 *
 * USAGE:
 * ch_open name
 */
TCLCAD_EXPORT extern int cho_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv);


/**
 * This is a convenience routine for registering an array of commands
 * with a Tcl interpreter. Note - this is not intended for use by
 * commands with associated state (i.e. ClientData).  The interp is
 * passed to the bu_cmdtab function as clientdata instead of the
 * bu_cmdtab entry.
 *
 * @param interp - Tcl interpreter wherein to register the commands
 * @param cmds	 - commands and related function pointers
 */
TCLCAD_EXPORT extern void tclcad_register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds);


/**
 * Set the variables "argc" and "argv" in interp.
 */
TCLCAD_EXPORT extern void tclcad_set_argv(Tcl_Interp *interp, int argc, const char **argv);

/**
 * This is the "all-in-one" initialization intended for use by
 * applications that are providing a Tcl_Interp and want to initialize
 * all of the BRL-CAD Tcl/Tk interfaces.
 *
 * libbu, libbn, librt, libged, and Itcl are always initialized.
 *
 * To initialize graphical elements (Tk/Itk), set init_gui to 1.
 */
TCLCAD_EXPORT extern int tclcad_init(Tcl_Interp *interp, int init_gui, struct bu_vls *tlog);

__END_DECLS

#endif /* TCLCAD_SETUP_H */

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
