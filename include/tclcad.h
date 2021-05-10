/*                        T C L C A D . H
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
/** @file tclcad.h
 *
 * @brief
 *  Header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 *  This library contains convenience routines for preparing and
 *  initializing Tcl.
 *
 */

#ifndef TCLCAD_H
#define TCLCAD_H

#include "common.h"
#include "bu/cmd.h"
#include "bu/process.h"
#include "tcl.h"
#include "dm.h"
#include "ged.h"

__BEGIN_DECLS

#ifndef TCLCAD_EXPORT
#  if defined(TCLCAD_DLL_EXPORTS) && defined(TCLCAD_DLL_IMPORTS)
#    error "Only TCLCAD_DLL_EXPORTS or TCLCAD_DLL_IMPORTS can be defined, not both."
#  elif defined(TCLCAD_DLL_EXPORTS)
#    define TCLCAD_EXPORT COMPILER_DLLEXPORT
#  elif defined(TCLCAD_DLL_IMPORTS)
#    define TCLCAD_EXPORT COMPILER_DLLIMPORT
#  else
#    define TCLCAD_EXPORT
#  endif
#endif

#define TCLCAD_IDLE_MODE 0
#define TCLCAD_ROTATE_MODE 1
#define TCLCAD_TRANSLATE_MODE 2
#define TCLCAD_SCALE_MODE 3
#define TCLCAD_CONSTRAINED_ROTATE_MODE 4
#define TCLCAD_CONSTRAINED_TRANSLATE_MODE 5
#define TCLCAD_OROTATE_MODE 6
#define TCLCAD_OSCALE_MODE 7
#define TCLCAD_OTRANSLATE_MODE 8
#define TCLCAD_MOVE_ARB_EDGE_MODE 9
#define TCLCAD_MOVE_ARB_FACE_MODE 10
#define TCLCAD_ROTATE_ARB_FACE_MODE 11
#define TCLCAD_PROTATE_MODE 12
#define TCLCAD_PSCALE_MODE 13
#define TCLCAD_PTRANSLATE_MODE 14
// TCLCAD_POLY_CIRCLE_MODE replaced by BV_POLY_CIRCLE_MODE
// TCLCAD_POLY_CONTOUR_MODE replaced by BV_POLY_CONTOUR_MODE
#define TCLCAD_POLY_ELLIPSE_MODE 17
#define TCLCAD_POLY_RECTANGLE_MODE 18
#define TCLCAD_POLY_SQUARE_MODE 19
#define TCLCAD_RECTANGLE_MODE 20
#define TCLCAD_MOVE_METABALL_POINT_MODE 21
#define TCLCAD_MOVE_PIPE_POINT_MODE 22
#define TCLCAD_MOVE_BOT_POINT_MODE 23
#define TCLCAD_MOVE_BOT_POINTS_MODE 24
#define TCLCAD_DATA_MOVE_OBJECT_MODE 25
#define TCLCAD_DATA_MOVE_POINT_MODE 26
#define TCLCAD_DATA_SCALE_MODE 27

#define TCLCAD_OBJ_FB_MODE_OFF 0
#define TCLCAD_OBJ_FB_MODE_UNDERLAY 1
#define TCLCAD_OBJ_FB_MODE_INTERLAY 2
#define TCLCAD_OBJ_FB_MODE_OVERLAY  3

/* Use fbserv */
#define USE_FBSERV 1

/* Framebuffer server object */

#define NET_LONG_LEN 4 /**< @brief # bytes to network long */
#define MAX_CLIENTS 32
#define MAX_PORT_TRIES 100
#define FBS_CALLBACK_NULL (void (*)())NULL
#define FBSERV_OBJ_NULL (struct fbserv_obj *)NULL

struct fbserv_listener {
    int fbsl_fd;                        /**< @brief socket to listen for connections */
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsl_chan;
#endif
    int fbsl_port;                      /**< @brief port number to listen on */
    int fbsl_listen;                    /**< @brief !0 means listen for connections */
    struct fbserv_obj *fbsl_fbsp;       /**< @brief points to its fbserv object */
};


struct fbserv_client {
    int fbsc_fd;
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsc_chan;
    Tcl_FileProc *fbsc_handler;
#endif
    struct pkg_conn *fbsc_pkg;
    struct fbserv_obj *fbsc_fbsp;       /**< @brief points to its fbserv object */
};


struct fbserv_obj {
    struct fb *fbs_fbp;                        /**< @brief framebuffer pointer */
    void *fbs_interp;             /**< @brief tcl interpreter */
    struct fbserv_listener fbs_listener;                /**< @brief data for listening */
    struct fbserv_client fbs_clients[MAX_CLIENTS];      /**< @brief connected clients */
    void (*fbs_callback)(void *clientData);             /**< @brief callback function */
    void *fbs_clientData;
    int fbs_mode;                       /**< @brief 0-off, 1-underlay, 2-interlay, 3-overlay */
};

DM_EXPORT extern int fbs_open(struct fbserv_obj *fbsp, int port);
DM_EXPORT extern int fbs_close(struct fbserv_obj *fbsp);

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

// Data specific to an individual view rather than the geometry database
// instance.
struct tclcad_view_data {
    struct ged		*gedp;
    struct bu_vls	gdv_edit_motion_delta_callback;
    int                 gdv_edit_motion_delta_callback_cnt;
    struct bu_vls	gdv_callback;
    int			gdv_callback_cnt;
    struct fbserv_obj	gdv_fbs;
};

struct tclcad_obj {
    struct bu_list	l;
    struct ged		*to_gedp;
    Tcl_Interp		*to_interp;
};

#define TCLCAD_OBJ_NULL (struct tclcad_obj *)0

TCLCAD_EXPORT extern int tclcad_tk_setup(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_auto_path(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_tcl_library(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_bn_setup(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_bn_mat_print(Tcl_Interp *interp, const char *title, const mat_t m);


/**
 * Allow specification of a ray to trace, in two convenient formats.
 *
 * Examples -
 * {0 0 0} dir {0 0 -1}
 * {20 -13.5 20} at {10 .5 3}
 */
TCLCAD_EXPORT extern int tclcad_rt_parse_ray(Tcl_Interp *interp,
					     struct xray *rp,
					     const char *const*argv);


/**
 * Format a "union cutter" structure in a TCL-friendly format.  Useful
 * for debugging space partitioning
 *
 * Examples -
 * type cutnode
 * type boxnode
 */
TCLCAD_EXPORT extern void tclcad_rt_pr_cutter(Tcl_Interp *interp,
					      const union cutter *cutp);
TCLCAD_EXPORT extern int tclcad_rt_cutter(ClientData clientData,
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
TCLCAD_EXPORT extern void tclcad_rt_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag);


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


/**
 * Take a db_full_path and append it to the TCL result string.
 *
 * NOT moving to db_fullpath.h because it is evil Tcl_Interp api
 */
TCLCAD_EXPORT extern void db_full_path_appendresult(Tcl_Interp *interp,
						    const struct db_full_path *pp);


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
TCLCAD_EXPORT extern void go_refresh(struct ged *gedp,
				     struct bview *gdvp);
TCLCAD_EXPORT extern void go_refresh_draw(struct ged *gedp,
					  struct bview *gdvp,
					  int restore_zbuffer);
TCLCAD_EXPORT extern int go_view_axes(struct ged *gedp,
				      struct bview *gdvp,
				      int argc,
				      const char *argv[],
				      const char *usage);
TCLCAD_EXPORT extern int go_data_labels(Tcl_Interp *interp,
					struct ged *gedp,
					struct bview *gdvp,
					int argc,
					const char *argv[],
					const char *usage);
TCLCAD_EXPORT extern int go_data_arrows(Tcl_Interp *interp,
					struct ged *gedp,
					struct bview *gdvp,
					int argc,
					const char *argv[],
					const char *usage);
TCLCAD_EXPORT extern int go_data_pick(struct ged *gedp,
				      struct bview *gdvp,
				      int argc,
				      const char *argv[],
				      const char *usage);
TCLCAD_EXPORT extern int go_data_axes(Tcl_Interp *interp,
				      struct ged *gedp,
				      struct bview *gdvp,
				      int argc,
				      const char *argv[],
				      const char *usage);
TCLCAD_EXPORT extern int go_data_lines(Tcl_Interp *interp,
				       struct ged *gedp,
				       struct bview *gdvp,
				       int argc,
				       const char *argv[],
				       const char *usage);
TCLCAD_EXPORT extern int go_data_move(Tcl_Interp *interp,
				      struct ged *gedp,
				      struct bview *gdvp,
				      int argc,
				      const char *argv[],
				      const char *usage);
TCLCAD_EXPORT extern int go_data_move_object_mode(Tcl_Interp *interp,
						  struct ged *gedp,
						  struct bview *gdvp,
						  int argc,
						  const char *argv[],
						  const char *usage);
TCLCAD_EXPORT extern int go_data_move_point_mode(Tcl_Interp *interp,
						 struct ged *gedp,
						 struct bview *gdvp,
						 int argc,
						 const char *argv[],
						 const char *usage);
TCLCAD_EXPORT extern int go_data_polygons(Tcl_Interp *interp,
					  struct ged *gedp,
					  struct bview *gdvp,
					  int argc,
					  const char *argv[],
					  const char *usage);
TCLCAD_EXPORT extern int go_mouse_poly_circ(Tcl_Interp *interp,
					    struct ged *gedp,
					    struct bview *gdvp,
					    int argc,
					    const char *argv[],
					    const char *usage);
TCLCAD_EXPORT extern int go_mouse_poly_cont(Tcl_Interp *interp,
					    struct ged *gedp,
					    struct bview *gdvp,
					    int argc,
					    const char *argv[],
					    const char *usage);
TCLCAD_EXPORT extern int go_mouse_poly_ell(Tcl_Interp *interp,
					   struct ged *gedp,
					   struct bview *gdvp,
					   int argc,
					   const char *argv[],
					   const char *usage);
TCLCAD_EXPORT extern int go_mouse_poly_rect(Tcl_Interp *interp,
					    struct ged *gedp,
					    struct bview *gdvp,
					    int argc,
					    const char *argv[],
					    const char *usage);
TCLCAD_EXPORT extern int go_poly_circ_mode(Tcl_Interp *interp,
					   struct ged *gedp,
					   struct bview *gdvp,
					   int argc,
					   const char *argv[],
					   const char *usage);
TCLCAD_EXPORT extern int go_poly_ell_mode(Tcl_Interp *interp,
					  struct ged *gedp,
					  struct bview *gdvp,
					  int argc,
					  const char *argv[],
					  const char *usage);
TCLCAD_EXPORT extern int go_poly_rect_mode(Tcl_Interp *interp,
					   struct ged *gedp,
					   struct bview *gdvp,
					   int argc,
					   const char *argv[],
					   const char *usage);
TCLCAD_EXPORT extern int go_run_tclscript(Tcl_Interp *interp,
					  const char *tclscript,
					  struct bu_vls *result_str);
TCLCAD_EXPORT extern int go_poly_cont_build(Tcl_Interp *interp,
					    struct ged *gedp,
					    struct bview *gdvp,
					    int argc,
					    const char *argv[],
					    const char *usage);
TCLCAD_EXPORT extern int go_poly_cont_build_end(Tcl_Interp *UNUSED(interp),
						struct ged *gedp,
						struct bview *gdvp,
						int argc,
						const char *argv[],
						const char *usage);

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

/**
 * Tcl specific I/O handlers
 */
struct tclcad_io_data {
    Tcl_Interp *interp;
    int io_mode;
    void *state;
};
TCLCAD_EXPORT struct tclcad_io_data *
tclcad_create_io_data();
TCLCAD_EXPORT void
tclcad_destroy_io_data(struct tclcad_io_data *d);

TCLCAD_EXPORT void
tclcad_create_io_handler(struct ged_subprocess *p, bu_process_io_t d, ged_io_func_t callback, void *data);
TCLCAD_EXPORT void
tclcad_delete_io_handler(struct ged_subprocess *p, bu_process_io_t d);


/* dm_tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
TCLCAD_EXPORT extern void fb_tcl_setup(void);

__END_DECLS

#endif /* TCLCAD_H */
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
