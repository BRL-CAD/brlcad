/*                          R T _ T C L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @addtogroup librt
 *
 * @brief
 * Tcl interfaces to LIBRT routines.
 *
 * LIBRT routines are not for casual command-line use; as a result,
 * the Tcl name for the function should be exactly the same as the C
 * name for the underlying function.  Typing "rt_" is no hardship when
 * writing Tcl procs, and clarifies the origin of the routine.
 *
 */
/** @{ */
/** @file rt_tcl.h */

#ifndef RT_RT_TCL_H
#define RT_RT_TCL_H

#include "common.h"
#include "vmath.h"
#include "bu/bu_tcl.h" /* for Tcl_Interp */
#include "rt/defines.h"

__BEGIN_DECLS


/**
 * Allow specification of a ray to trace, in two convenient formats.
 *
 * Examples -
 * {0 0 0} dir {0 0 -1}
 * {20 -13.5 20} at {10 .5 3}
 */
RT_EXPORT extern int rt_tcl_parse_ray(Tcl_Interp *interp,
                                      struct xray *rp,
                                      const char *const*argv);


/**
 * Format a "union cutter" structure in a TCL-friendly format.  Useful
 * for debugging space partitioning
 *
 * Examples -
 * type cutnode
 * type boxnode
 * type nugridnode
 */
RT_EXPORT extern void rt_tcl_pr_cutter(Tcl_Interp *interp,
                                       const union cutter *cutp);
RT_EXPORT extern int rt_tcl_cutter(ClientData clientData,
                                   Tcl_Interp *interp,
                                   int argc,
                                   const char *const*argv);


/**
 * Format a hit in a TCL-friendly format.
 *
 * It is possible that a solid may have been removed from the
 * directory after this database was prepped, so check pointers
 * carefully.
 *
 * It might be beneficial to use some format other than %g to give the
 * user more precision.
 */
RT_EXPORT extern void rt_tcl_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag);


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
RT_EXPORT extern int rt_tcl_rt(ClientData clientData,
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
RT_EXPORT extern int rt_tcl_import_from_path(Tcl_Interp *interp,
                                             struct rt_db_internal *ip,
                                             const char *path,
                                             struct rt_wdb *wdb);
RT_EXPORT extern void rt_generic_make(const struct rt_functab *ftp, struct rt_db_internal *intern);


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
RT_EXPORT extern void rt_tcl_setup(Tcl_Interp *interp);
RT_EXPORT extern int Sysv_Init(Tcl_Interp *interp);


/**
 * Allows LIBRT to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 * "load /usr/brlcad/lib/libbn.so"
 * "load /usr/brlcad/lib/librt.so"
 */
RT_EXPORT extern int Rt_Init(Tcl_Interp *interp);


/**
 * Take a db_full_path and append it to the TCL result string.
 *
 * NOT moving to db_fullpath.h because it is evil Tcl_Interp api
 */
RT_EXPORT extern void db_full_path_appendresult(Tcl_Interp *interp,
                                                const struct db_full_path *pp);


/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to int, and stores them in the passed
 * in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
RT_EXPORT extern int tcl_obj_to_int_array(Tcl_Interp *interp,
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
RT_EXPORT extern int tcl_obj_to_fastf_array(Tcl_Interp *interp,
                                            Tcl_Obj *list,
                                            fastf_t **array,
                                            int *array_len);


/**
 * interface to above tcl_obj_to_int_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * Returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_int_array(Tcl_Interp *interp,
                                           char *char_list,
                                           int **array,
                                           int *array_len);


/**
 * interface to above tcl_obj_to_fastf_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_fastf_array(Tcl_Interp *interp,
                                             const char *char_list,
                                             fastf_t **array,
                                             int *array_len);


__END_DECLS

#endif /* RT_RT_TCL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
