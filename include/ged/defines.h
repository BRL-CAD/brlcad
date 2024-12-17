/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @addtogroup ged_defines
 *
 * Geometry EDiting Library specific definitions.
 *
 */
/** @{ */
/** @file ged/defines.h */

#ifndef GED_DEFINES_H
#define GED_DEFINES_H

#include "common.h"
#include "bu/hash.h"
#include "bu/list.h"
#include "bu/process.h"
#include "bu/vls.h"
#include "bv/defines.h"
#include "rt/search.h"
#include "bv/defines.h"
#include "bv/lod.h"
#include "dm/fbserv.h" // for fbserv_obj
#include "rt/wdb.h" // for struct rt_wdb


#ifndef GED_EXPORT
#  if defined(GED_DLL_EXPORTS) && defined(GED_DLL_IMPORTS)
#    error "Only GED_DLL_EXPORTS or GED_DLL_IMPORTS can be defined, not both."
#  elif defined(GED_DLL_EXPORTS)
#    define GED_EXPORT COMPILER_DLLEXPORT
#  elif defined(GED_DLL_IMPORTS)
#    define GED_EXPORT COMPILER_DLLIMPORT
#  else
#    define GED_EXPORT
#  endif
#endif

#define GED_VMIN -2048.0
#define GED_VMAX 2047.0
#define GED_VRANGE 4095.0
#define INV_GED_V 0.00048828125
#define INV_4096_V 0.000244140625

#define GED_NULL ((struct ged *)0)
#define GED_DISPLAY_LIST_NULL ((struct display_list *)0)
#define GED_DRAWABLE_NULL ((struct ged_drawable *)0)
#define GED_VIEW_NULL ((struct bview *)0)

#define GED_RESULT_NULL ((void *)0)

/* Sequence starts after BRLCAD_ERROR, to be compatible with
 * BU return codes */
#define GED_HELP     0x0002 /**< invalid specification, result contains usage */
#define GED_MORE     0x0004 /**< incomplete specification, can specify again interactively */
#define GED_QUIET    0x0008 /**< don't set or modify the result string */
#define GED_UNKNOWN  0x0010 /**< argv[0] was not a known command */
#define GED_EXIT     0x0020 /**< command is requesting a clean application shutdown */
#define GED_OVERRIDE 0x0040 /**< used to indicate settings have been overridden */

/* Check the internal GED magic */
#define GED_CK_MAGIC(_p) BU_CKMAG(_p->i, GED_MAGIC, "ged")

/* Forward declaration */
struct ged;
struct ged_selection_set;

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
#define GED_FUNC_PTR_NULL ((ged_func_ptr)0)

/* Callback related definitions */
typedef void (*ged_io_func_t)(void *, int);
typedef void (*ged_refresh_func_t)(void *);
typedef void (*ged_create_vlist_solid_func_t)(void *, struct bv_scene_obj *);
typedef void (*ged_create_vlist_display_list_func_t)(void *, struct display_list *);
typedef void (*ged_destroy_vlist_func_t)(void *, unsigned int, int);
struct ged_callback_state;

/**
 * Definition of global parallel-processing semaphores.
 *
 */
#define GED_SEM_WORKER ANALYZE_SEM_LAST
#define GED_SEM_STATS GED_SEM_WORKER+1
#define GED_SEM_LIST GED_SEM_STATS+1
#define GED_SEM_LAST GED_SEM_LIST+1

#define GED_INITIALIZED(_gedp) ((_gedp)->dbip != NULL)
#define GED_LOCAL2BASE(_gedp) ((_gedp)->dbip->dbi_local2base)
#define GED_BASE2LOCAL(_gedp) ((_gedp)->dbip->dbi_base2local)

/* From include/dm.h */
#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/* FIXME: leftovers from dg.h */
#define RT_VDRW_PREFIX          "_VDRW"
#define RT_VDRW_PREFIX_LEN      6
#define RT_VDRW_MAXNAME         31
#define RT_VDRW_DEF_COLOR       0xffff00


struct ged_subprocess {
    uint32_t magic;         /**< @brief magic number */
    struct bu_process *p;
    void *chan;
    int aborted;
    struct ged *gedp;
    bu_clbk_t end_clbk;	/**< @brief  function called when process completes */
    void *end_clbk_data;
    int stdin_active;
    int stdout_active;
    int stderr_active;
};

/* FIXME: should be private */
struct ged_qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

/* FIXME: should be private */
struct ged_qray_fmt {
    char type;
    struct bu_vls fmt;
};

struct vd_curve {
    struct bu_list      l;
    char                vdc_name[RT_VDRW_MAXNAME+1];    /**< @brief name array */
    long                vdc_rgb;        /**< @brief color */
    struct bu_list      vdc_vhd;        /**< @brief head of list of vertices */
};
#define VD_CURVE_NULL   ((struct vd_curve *)NULL)

/* FIXME: should be private */
struct ged_drawable {
    struct bu_list		*gd_headDisplay;	/**< @brief  head of display list */
    struct bu_list		*gd_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*gd_currVHead;		/**< @brief  current vdraw head */

    char			**gd_rt_cmd;    /* DEPRECATED - will be removed, do not use */
    int				gd_rt_cmd_len;  /* DEPRECATED - will be removed, do not use */

    void			(*gd_rtCmdNotify)(int aborted);	/**< @brief  function called when rt command completes */

    int				gd_uplotOutputMode;	/**< @brief  output mode for unix plots */

    /* qray state */
    struct bu_vls		gd_qray_basename;	/**< @brief  basename of query ray vlist */
    struct bu_vls		gd_qray_script;		/**< @brief  query ray script */
    char			gd_qray_effects;	/**< @brief  t for text, g for graphics or b for both */
    int				gd_qray_cmd_echo;	/**< @brief  0 - don't echo command, 1 - echo command */
    struct ged_qray_fmt		*gd_qray_fmts;
    struct ged_qray_color	gd_qray_odd_color;
    struct ged_qray_color	gd_qray_even_color;
    struct ged_qray_color	gd_qray_void_color;
    struct ged_qray_color	gd_qray_overlap_color;
    int				gd_shaded_mode;		/**< @brief  1 - draw bots shaded by default */
};

/* Experimental work on a high-performance in-memory representation of
 * database, view and selection states. We want this to be visible to C++ APIs
 * like libqtcad, so they can reflect the state of the .g hierarchy in their
 * own structures without us or them having to make copies of the data.
 */
#include "ged/dbi.h"

__BEGIN_DECLS

struct ged_cmd;

/* struct details are private - use accessor functions to manipulate */
struct ged_results;

/* Need to see if we can start hiding aspects of the GED state.
 * Define an impl container to hold internal details. */
struct ged_impl;

// TODO - in the above struct, we can hide a general callback mechanism where
// we can allow apps (via accessor functions) to set app level pre and post
// command execution callbacks on any command.  We should also be able to
// define the "generic" callbacks that aren't specific to just one command
// (view refresh being the canonical example), maybe via separate functions
// from the command specific callbacks.  Goal would be to eliminate the need
// for the command tables in libtclcad and mged in favor of setting callback
// functions.  Another tell that we're getting it right will be the ability to
// eliminate most of the current callback hooks explicitly spelled out in the
// ged struct.
//
// ged_exec will always check for a specific command's callbacks, but commands
// can also internally check for things like the view refresh callback if the
// command implementation knows it will need to trigger it (draw, for example,
// should check for a view refresh callback once its work is done.)  Probably
// need to track to make sure we don't have infinite loops of callbacks calling
// callbacks assigned at run time - it'll be a situation similar to the Qt
// signal/slots mechanism where you can accidentally set such things up.
// Probably want to allow more than one callback function on the same command,
// maybe checking to prevent multiple calls to the same function getting
// assigned by mistake.

struct ged {
    struct ged_impl             *i;
    struct bu_vls               go_name;
    struct db_i                 *dbip;
    DbiState			*dbi_state;

    /*************************************************************/
    /* Information pertaining to views and view objects .        */
    /*************************************************************/
    /* The current view */
    struct bview		*ged_gvp;
    /* The full set of views associated with this ged object */
    struct bview_set            ged_views;
    /* Sometimes applications will supply GED views, and sometimes GED commands
     * may create views.  In the latter case, ged_close will also need to free
     * the views.  We define a container to hold those views that libged is
     * managing, since ged_views views may belong to the application rather
     * than GED. */
    struct bu_ptbl              ged_free_views;

    /* Drawing data associated with this .g file */
    struct bv_mesh_lod_context  *ged_lod;


    void                        *u_data; /**< @brief User data associated with this ged instance */

    /** for catching log messages */
    struct bu_vls		*ged_log;


    /* @todo: add support for returning an array of objects, not just a
     * simple string.
     *
     * the calling application needs to be able to distinguish the
     * individual object names from the "ls" command without resorting
     * to quirky string encoding or format-specific quote wrapping.
     *
     * want to consider whether we need a json-style dictionary, but
     * probably a literal null-terminated array will suffice here.
     */
    struct bu_vls		*ged_result_str;
    struct ged_results          *ged_results;

    struct ged_drawable		*ged_gdp;
    struct bu_ptbl              free_solids;

    char			*ged_output_script;		/**< @brief  script for use by the outputHandler */

    /* Old selection data containers used by joint and brep*/
    struct bu_hash_tbl		*ged_selections; /**< @brief object name -> struct rt_object_selections */

    /* FIXME -- this ugly hack needs to die.  the result string should
     * be stored before the call.
     */
    int 			ged_internal_call;


    /* TODO: hide all callback related symbols, callback typedefs
     * (above), and eventually most if not all of the remaining fields
     * into an _impl structure so callers are not tightly coupled to
     * the ged structure.  access via public functions that have been
     * given design consideration.
     */

    /* FOR LIBGED INTERNAL USE */
    struct ged_cmd *cmds;
    int (*add)(struct ged *gedp, const struct ged_cmd *cmd);
    int (*del)(struct ged *gedp, const char *name);
    int (*run)(struct ged *gedp, int ac, char *av[]);

    struct bu_ptbl		ged_subp; /**< @brief  forked sub-processes */

    /* Callbacks */
    struct ged_callback_state *ged_cbs;
    void (*ged_refresh_handler)(void *);	/**< @brief  function for handling refresh requests */
    void *ged_refresh_clientdata;	/**< @brief  client data passed to refresh handler */
    void (*ged_output_handler)(struct ged *, char *);	/**< @brief  function for handling output */
    void (*ged_create_vlist_scene_obj_callback)(void *, struct bv_scene_obj *);	/**< @brief  function to call after creating a vlist to create display list for solid */
    void (*ged_create_vlist_display_list_callback)(void *, struct display_list *);	/**< @brief  function to call after all vlist created that loops through creating display list for each solid  */
    void (*ged_destroy_vlist_callback)(void *, unsigned int, int);	/**< @brief  function to call after freeing a vlist */
    void *vlist_ctx;

    /* Handler functions for I/O communication with asynchronous subprocess commands.  There
     * are two opaque data structures at play here, with different scopes.  One is the "data"
     * pointer passed to ged_create_io_handler, which is used to store command-specific
     * information internal to the library (the simplest thing to do is pass ged_subprocess
     * in as the data pointer, but if that's not enough - see for example rtcheck - this
     * mechanism allows for more elaborate measures.
     *
     * The second is ged_io_data, which is set in gedp by the calling application.  This is where
     * information specific to the parent's I/O environment (which by definition the library
     * can't know about as it is application specific) lives.  It should be assigned in the
     * applications gedp before any calls to ged_create_io_handler are made.
     * */
    void (*ged_create_io_handler)(struct ged_subprocess *gp, bu_process_io_t d, ged_io_func_t callback, void *data);
    void (*ged_delete_io_handler)(struct ged_subprocess *gp, bu_process_io_t fd);
    void *ged_io_data;  /**< brief caller supplied data */

    /* fbserv server and I/O callbacks.  These must hook into the application's event
     * loop, and so cannot be effectively supplied by low-level libraries - the
     * application must tell us how to tie in with the toplevel event
     * processing system it is using (typically toolkit specific). */
    struct fbserv_obj *ged_fbs;

    // Other callbacks...
    // Tcl command strings - these are libtclcad level callbacks that execute user supplied Tcl commands if set:
    // gdv_callback, gdv_edit_motion_delta_callback, go_more_args_callback, go_rt_end_callback
    //
    // fbserv_obj: fbs_callback
    // bv.h gv_callback (only used by MGED?)

    void *ged_interp; /* Temporary - do not rely on when designing new functionality */
};

// Create and destroy a ged container.  Handles all initialization - no
// need to call init or free using these routines.
GED_EXPORT struct ged *ged_create(void);
GED_EXPORT void ged_destroy(struct ged *);

// If you create a ged container on the stack or without using create/destroy,
// you need to call ged_init before it can be used and ged_free once you are
// done with it.
GED_EXPORT extern void ged_init(struct ged *gedp);
GED_EXPORT extern void ged_free(struct ged *gedp);



// Make some definitions so it's clear what callback slot we are addressing
#define GED_CLBK_PRE -1
#define GED_CLBK_DURING 0
#define GED_CLBK_POST 1

// In some cases (rt launches in particular) the client application may need
// to take action when the subprocess ends, not simply when the command
// returns.  Distinguished from GED_CLBK_POST with this setting.
#define GED_CLBK_LINGER 2

// Associate a callback function pointer for a command.  If mode is less than
// zero, function will be registered to run BEFORE actual cmd logic is run, and
// if greater than zero will be registered to run AFTER the cmd logic is run.
// If mode is zero, then it is intended to be used *during* command execution
// and it will be up to the command's implementation to incorporate the
// callback into its execution logic. A "GED_CLBK_DURING" will be a no-op
// unless the command implementation logic of the libged command in question
// utilizes it.  The "search" command is an example of a command that utilizes
// a "GED_CLBK_DURING" callback. PRE and POST callbacks are handled
// automatically by ged_exec, and command implementations should not invoke
// them directly. LINGER is a special case involving commands utilizing long
// running subprocesses.
//
// Only one function can be registered for each pre/post command slot - an
// assignment to a command slot that already has an assigned function will
// result in the previous function pointer being cleared and replaced.  If an
// existing pointer has been overridden GED_OVERRIDE will be set in return
// code.  An overriding assignment will override both the function and data
// values - a NULL data value in d will overwrite an existing non-NULL value.
//
// If f is NULL, any existing callback function pointer in the specified slot
// is cleared along with its data pointer.
//
// d optionally associates user supplied data with the callback.  The bu_clbk_t
// function signature has two data pointers - this value will be supplied as
// the second pointer argument.  (The first will usually be the ged pointer,
// but that is not guaranteed in the GED_CLBK_DURING and GED_CLBK_LINGER cases
// - check the command implementation to see what data it is returning via the
// first ptr.)
//
// To clear all command callbacks, iterate over the ged_cmd_list results
// and assign NULL f values.
GED_EXPORT extern int ged_clbk_set(struct ged *gedp, const char *cmd, int mode, bu_clbk_t f, void *d);

// Method calling code can use to get the current clbk info for a specific command.
GED_EXPORT extern int ged_clbk_get(bu_clbk_t *f, void **d, struct ged *gedp, const char *cmd, int mode);

// Wrapper that utilizes internal libged capabilities to detect recursive callbacks
// Using this is optional - caller can also use results of ged_clbk_get directly -
// but if they wish to detect and report recursion this functionality is helpful.
GED_EXPORT extern int ged_clbk_exec(
	struct bu_vls *log,
	struct ged *gedp,
	int limit,
	bu_clbk_t f,
	int ac,
	const char **av,
	void *u1,
	void *u2);

// Functions for associating and retrieving application context information for
// specific dm types.  Not really using this yet, but we will eventually need
// some way to provide application contents for the dm attach command to work.
//
// NOTE - this API is still experimental
GED_EXPORT extern void ged_dm_ctx_set(struct ged *gedp, const char *dm_type, void *ctx);
GED_EXPORT extern void *ged_dm_ctx_get(struct ged *gedp, const char *dm_type);


/* accessor functions for ged_results - calling
 * applications should not work directly with the
 * internals of ged_results, which are not guaranteed
 * to stay the same.
 * defined in ged_util.c */
GED_EXPORT extern size_t ged_results_count(struct ged_results *results);
GED_EXPORT extern const char *ged_results_get(struct ged_results *results, size_t index);
GED_EXPORT extern void ged_results_clear(struct ged_results *results);
GED_EXPORT extern void ged_results_free(struct ged_results *results);


// Call ged_close to release returned ged structure.  To open and close a
// database without creating and destroying ged structure pointers (i.e. to
// reuse a struct) call the ged_exec_open and ged_exec_close commands rather
// than using these functions.
GED_EXPORT extern struct ged *ged_open(const char *dbtype,
				       const char *filename,
				       int existing_only);
// Note that ged_close frees all memory and deletes the gedp
GED_EXPORT extern void ged_close(struct ged *gedp);


/**
 * make sure there is a command name given
 *
 * @todo - where should this go?
 */
#define GED_CHECK_ARGC_GT_0(_gedp, _argc, _flags) \
    if ((_argc) < 1) { \
	int ged_check_argc_gt_0_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_argc_gt_0_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "Command name not provided on (%s:%d).", __FILE__, __LINE__); \
	} \
	return (_flags); \
    }


struct ged_cmd_impl;
struct ged_cmd {
    struct ged_cmd_impl *i;
};

struct ged_plugin {
    uint32_t api_version; /* must be first in struct */
    const struct ged_cmd ** const cmds;
    int cmd_cnt;
};


struct ged_cmd_process_impl;
struct ged_cmd_process {
    struct ged_cmd_process_impl *i;
};

struct ged_process_plugin {
    uint32_t api_version; /* must be first in struct */
    const struct ged_cmd_process * const p;
};

typedef int (*ged_process_ptr)(int, const char *[]);

/* Report any messages from libged when plugins were initially loaded.
 * Can be important when diagnosing command errors. */
GED_EXPORT const char * ged_init_msgs(void);


__END_DECLS

#endif /* GED_DEFINES_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
