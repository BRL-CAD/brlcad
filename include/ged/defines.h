/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
#include "bu/vls.h"

__BEGIN_DECLS

#ifndef GED_EXPORT
#  if defined(GED_DLL_EXPORTS) && defined(GED_DLL_IMPORTS)
#    error "Only GED_DLL_EXPORTS or GED_DLL_IMPORTS can be defined, not both."
#  elif defined(GED_DLL_EXPORTS)
#    define GED_EXPORT __declspec(dllexport)
#  elif defined(GED_DLL_IMPORTS)
#    define GED_EXPORT __declspec(dllimport)
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

#define GED_VMIN -2048.0
#define GED_VMAX 2047.0
#define GED_VRANGE 4095.0
#define INV_GED_V 0.00048828125
#define INV_4096_V 0.000244140625

#define GED_NULL ((struct ged *)0)
#define GED_DISPLAY_LIST_NULL ((struct display_list *)0)
#define GED_DRAWABLE_NULL ((struct ged_drawable *)0)
#define GED_VIEW_NULL ((struct bview *)0)
#define GED_DM_VIEW_NULL ((struct ged_dm_view *)0)

#define GED_RESULT_NULL ((void *)0)

#define GED_FUNC_PTR_NULL ((ged_func_ptr)0)
#define GED_REFRESH_CALLBACK_PTR_NULL ((ged_refresh_callback_ptr)0)
#define GED_CREATE_VLIST_SOLID_CALLBACK_PTR_NULL ((ged_create_vlist_solid_callback_ptr)0)
#define GED_CREATE_VLIST_CALLBACK_PTR_NULL ((ged_create_vlist_callback_ptr)0)
#define GED_FREE_VLIST_CALLBACK_PTR_NULL ((ged_free_vlist_callback_ptr)0)

/**
 * Definition of global parallel-processing semaphores.
 *
 */
#define GED_SEM_WORKER RT_SEM_LAST
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


struct ged_run_rt {
    struct bu_list l;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE fd;
    HANDLE hProcess;
    DWORD pid;

#  ifdef TCL_OK
    Tcl_Channel chan;
#  else
    void *chan;
#  endif
#else
    int fd;
    int pid;
#endif
    int aborted;
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
    struct bu_list		l;
    struct bu_list		*gd_headDisplay;		/**< @brief  head of display list */
    struct bu_list		*gd_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*gd_currVHead;		/**< @brief  current vdraw head */
    struct solid                *gd_freeSolids;         /**< @brief  ptr to head of free solid list */

    char			**gd_rt_cmd;
    int				gd_rt_cmd_len;
    struct ged_run_rt		gd_headRunRt;		/**< @brief  head of forked rt processes */

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
    struct bu_list		l;
    struct rt_wdb		*ged_wdbp;

    /** for catching log messages */
    struct bu_vls		*ged_log;

    struct solid                *freesolid;  /* For now this is a struct solid, but longer term that may not always be true */

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
    struct fbserv_obj		*ged_fbsp; /* FIXME: this shouldn't be here */
    struct bu_hash_tbl		*ged_selections; /**< @brief object name -> struct rt_object_selections */

    void			*ged_dmp;
    void			*ged_refresh_clientdata;	/**< @brief  client data passed to refresh handler */
    void			(*ged_refresh_handler)(void *);	/**< @brief  function for handling refresh requests */
    void			(*ged_output_handler)(struct ged *, char *);	/**< @brief  function for handling output */
    char			*ged_output_script;		/**< @brief  script for use by the outputHandler */
    void			(*ged_create_vlist_solid_callback)(struct solid *);	/**< @brief  function to call after creating a vlist to create display list for solid */
    void			(*ged_create_vlist_callback)(struct display_list *);	/**< @brief  function to call after all vlist created that loops through creating display list for each solid  */
    void			(*ged_free_vlist_callback)(unsigned int, int);	/**< @brief  function to call after freeing a vlist */

    /* FIXME -- this ugly hack needs to die.  the result string should be stored before the call. */
    int 			ged_internal_call;

    /* FOR LIBGED INTERNAL USE */
    struct ged_cmd *cmds;
    int (*add)(const struct ged_cmd *cmd);
    int (*del)(const char *name);
    int (*run)(int ac, char *av[]);
    void *ged_interp; /* Temporary - do not rely on when designing new functionality */

    /* Interface to LIBDM */
    int ged_dm_width;
    int ged_dm_height;
    int ged_dmp_is_null;
    void (*ged_dm_get_display_image)(struct ged *, unsigned char **);

};

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
typedef void (*ged_refresh_callback_ptr)(void *);
typedef void (*ged_create_vlist_solid_callback_ptr)(struct solid *);
typedef void (*ged_create_vlist_callback_ptr)(struct display_list *);
typedef void (*ged_free_vlist_callback_ptr)(unsigned int, int);


/**
 * describes a command plugin
 */
struct ged_cmd {
    struct bu_list l;

    const char *name;
    const char description[80];
    const char *manpage;

    int (*load)(struct ged *);
    void (*unload)(struct ged *);
    int (*exec)(struct ged *, int, const char *[]);
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
