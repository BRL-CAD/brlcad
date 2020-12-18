/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
#include "dm/bview.h"
#include "rt/search.h"
#include "rt/solid.h"

__BEGIN_DECLS

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

/** all okay return code, not a maskable result */
#define GED_OK    0x0000

/**
 * possible maskable return codes from ged functions.  callers should
 * not rely on the actual values but should instead test via masking.
 */
#define GED_ERROR 0x0001 /**< something went wrong, the action was not performed */
#define GED_HELP  0x0002 /**< invalid specification, result contains usage */
#define GED_MORE  0x0004 /**< incomplete specification, can specify again interactively */
#define GED_QUIET 0x0008 /**< don't set or modify the result string */
#define GED_UNKNOWN 0x0010 /**< argv[0] was not a known command */

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

/* Forward declaration */
struct ged;

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
#define GED_FUNC_PTR_NULL ((ged_func_ptr)0)

/* Callback related definitions */
typedef void (*ged_io_func_t)(void *, int);
typedef void (*ged_refresh_func_t)(void *);
typedef void (*ged_create_vlist_solid_func_t)(struct solid *);
typedef void (*ged_create_vlist_display_list_func_t)(struct display_list *);
typedef void (*ged_destroy_vlist_func_t)(unsigned int, int);
struct ged_callback_state;

/**
 * Definition of global parallel-processing semaphores.
 *
 */
#define GED_SEM_WORKER ANALYZE_SEM_LAST
#define GED_SEM_STATS GED_SEM_WORKER+1
#define GED_SEM_LIST GED_SEM_STATS+1
#define GED_SEM_LAST GED_SEM_LIST+1

#define GED_INIT(_gedp, _wdbp) { \
	ged_init((_gedp)); \
	(_gedp)->ged_wdbp = (_wdbp); \
    }

#define GED_INITIALIZED(_gedp) ((_gedp)->ged_wdbp != RT_WDB_NULL)
#define GED_LOCAL2BASE(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_local2base)
#define GED_BASE2LOCAL(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_base2local)

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




struct ged_cmd;

/* struct details are private - use accessor functions to manipulate */
struct ged_results;

struct ged {
    struct bu_vls               go_name;
    struct rt_wdb		*ged_wdbp;

    // The full set of bviews associated with this ged object
    struct bu_ptbl              ged_views;

    void                        *u_data; /**< @brief User data associated with this ged instance */

    /** for catching log messages */
    struct bu_vls		*ged_log;

    struct solid                *freesolid;  /* For now this is a struct solid, but longer term that may not always be true */
    struct bu_ptbl              free_solids; /**< @brief  solid structures available for reuse */

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
    struct bview		*ged_gvp;
    struct bu_hash_tbl		*ged_selections; /**< @brief object name -> struct rt_object_selections */

    char			*ged_output_script;		/**< @brief  script for use by the outputHandler */


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

    /* Interface to LIBDM */
    void *ged_dmp;


    /* Callbacks */

    struct ged_callback_state    *ged_cbs;
    void			(*ged_refresh_handler)(void *);	/**< @brief  function for handling refresh requests */
    void			*ged_refresh_clientdata;	/**< @brief  client data passed to refresh handler */
    void			(*ged_output_handler)(struct ged *, char *);	/**< @brief  function for handling output */
    void			(*ged_create_vlist_solid_callback)(struct solid *);	/**< @brief  function to call after creating a vlist to create display list for solid */
    void			(*ged_create_vlist_display_list_callback)(struct display_list *);	/**< @brief  function to call after all vlist created that loops through creating display list for each solid  */
    void			(*ged_destroy_vlist_callback)(unsigned int, int);	/**< @brief  function to call after freeing a vlist */

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

    // Other callbacks...
    // Tcl command strings - these are libtclcad level callbacks that execute user supplied Tcl commands if set:
    // gdv_callback, gdv_edit_motion_delta_callback, go_more_args_callback, go_rt_end_callback
    //
    // fbserv_obj: fbs_callback
    // bview.h gv_callback (only used by MGED?)
    // db_search_callback_t

    void *ged_interp; /* Temporary - do not rely on when designing new functionality */
    db_search_callback_t ged_interp_eval; /* FIXME: broke the rule written on the previous line */

};

/* accessor functions for ged_results - calling
 * applications should not work directly with the
 * internals of ged_results, which are not guaranteed
 * to stay the same.
 * defined in ged_util.c */
GED_EXPORT extern size_t ged_results_count(struct ged_results *results);
GED_EXPORT extern const char *ged_results_get(struct ged_results *results, size_t index);
GED_EXPORT extern void ged_results_clear(struct ged_results *results);
GED_EXPORT extern void ged_results_free(struct ged_results *results);



GED_EXPORT extern void ged_close(struct ged *gedp);
GED_EXPORT extern void ged_free(struct ged *gedp);
GED_EXPORT extern void ged_init(struct ged *gedp);
/* Call BU_PUT to release returned ged structure */
GED_EXPORT extern struct ged *ged_open(const char *dbtype,
				       const char *filename,
				       int existing_only);


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

/* Report any messages from libged when plugins were initially loaded.
 * Can be important when diagnosing command errors. */
GED_EXPORT const char * ged_init_msgs();


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
