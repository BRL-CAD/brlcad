/*                   T C L C A D / M I S C . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file tclcad/misc.h
 *
 * @brief
 *  Miscellaneous header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 */

#ifndef TCLCAD_MISC_H
#define TCLCAD_MISC_H

#include "common.h"
#include "bu/process.h"
#include "tcl.h"
#include "dm.h"
#include "ged.h"
#include "tclcad/defines.h"

__BEGIN_DECLS

struct tclcad_ged_data {
    struct ged		*gedp;
    struct bu_vls	go_more_args_callback;
    int			go_more_args_callback_cnt;

    // These are view related, but appear to be intended as global across all
    // views associated with the gedp - that is why they are here and not in
    // tclcad_view_data.
    struct bu_vls	go_rt_end_callback;
    int                 go_rt_end_callback_cnt;
    struct dm_view_data go_dmv;
};

struct tclcad_obj {
    struct bu_list	l;
    struct ged		*to_gedp;
    Tcl_Interp		*to_interp;
};

#define TCLCAD_OBJ_NULL (struct tclcad_obj *)0

TCLCAD_EXPORT extern int tclcad_rt_cutter(ClientData clientData,
					  Tcl_Interp *interp,
					  int argc,
					  const char *const*argv);

/**
 * Generic interface for the LIBRT_class manipulation routines.
 *
 * Usage:
 * procname dbCmdName ?args?
 * Returns: result of cmdName LIBRT operation.
 *
 * Objects of type 'procname' must have been previously created by the
 * 'rt_gettrees' operation performed on a database object.
 *
 * Example -
 *      .inmem rt_gettrees .rt all.g
 *      .rt shootray {0 0 0} dir {0 0 -1}
 */
TCLCAD_EXPORT extern int tclcad_rt(ClientData clientData,
				   Tcl_Interp *interp,
				   int argc,
				   const char **argv);

/************************************************************************************************
 *                                                                                              *
 *                      Tcl interface to the Database                                           *
 *                                                                                              *
 ************************************************************************************************/

/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * TCL_OK
 * TCL_ERROR
 */
TCLCAD_EXPORT extern int tclcad_rt_import_from_path(Tcl_Interp *interp,
						    struct rt_db_internal *ip,
						    const char *path,
						    struct rt_wdb *wdb);

/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to int, and stores them in the passed
 * in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
TCLCAD_EXPORT extern int tcl_obj_to_int_array(Tcl_Interp *interp,
					      Tcl_Obj *list,
					      int **array,
					      int *array_len);

/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to fastf_t, and stores them in the
 * passed in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
TCLCAD_EXPORT extern int tcl_obj_to_fastf_array(Tcl_Interp *interp,
						Tcl_Obj *list,
						fastf_t **array,
						int *array_len);


/**
 * interface to above tcl_obj_to_int_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * Returns the number of elements converted.
 */
TCLCAD_EXPORT extern int tcl_list_to_int_array(Tcl_Interp *interp,
					       char *char_list,
					       int **array,
					       int *array_len);


/**
 * interface to above tcl_obj_to_fastf_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * returns the number of elements converted.
 */
TCLCAD_EXPORT extern int tcl_list_to_fastf_array(Tcl_Interp *interp,
						 const char *char_list,
						 fastf_t **array,
						 int *array_len);


/* defined in tclcad_obj.c */
TCLCAD_EXPORT extern int to_open_tcl(ClientData UNUSED(clientData),
				     Tcl_Interp *interp,
				     int argc,
				     const char **argv);
TCLCAD_EXPORT extern struct application *to_rt_gettrees_application(struct ged *gedp,
								    int argc,
								    const char *argv[]);

/**
 * Tcl specific I/O handlers
 */
struct tclcad_io_data {
    Tcl_Interp *interp;
    int io_mode;
    void *state;
};
TCLCAD_EXPORT struct tclcad_io_data *
tclcad_create_io_data(void);
TCLCAD_EXPORT void
tclcad_destroy_io_data(struct tclcad_io_data *d);

TCLCAD_EXPORT void
tclcad_create_io_handler(struct ged_subprocess *p, bu_process_io_t d, ged_io_func_t callback, void *data);
TCLCAD_EXPORT void
tclcad_delete_io_handler(struct ged_subprocess *p, bu_process_io_t d);

__END_DECLS

#endif /* TCLCAD_MISC_H */

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
