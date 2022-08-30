/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

/* Forward declaration */
struct ged;
struct ged_selection_set;

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
#define GED_FUNC_PTR_NULL ((ged_func_ptr)0)

/* Callback related definitions */
typedef void (*ged_io_func_t)(void *, int);
typedef void (*ged_refresh_func_t)(void *);
typedef void (*ged_create_vlist_solid_func_t)(struct bv_scene_obj *);
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
        if ((_gedp)->ged_wdbp) { \
	    (_gedp)->dbip = (_gedp)->ged_wdbp->dbip; \
        } \
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
    void (*end_clbk)(int, void *);	/**< @brief  function called when process completes */
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


#ifdef __cplusplus

/* Experimental */
// We want this to be visible to C++ APIs like libqtcad, so they can reflect
// the state of the .g hierarchy in their own structures without us or them
// having to make copies of the data.  This is similar to what we need to do
// to handle ON_Brep
#include <unordered_map>
#include <unordered_set>

class GED_EXPORT DbiState;

class GED_EXPORT BViewState {
    public:
	BViewState(DbiState *);

	unsigned long long path_hash(std::vector<unsigned long long> &path, size_t max_len);

	void redraw();

	// Sets defining all drawn solid paths (including invalid paths).  The
	// s_keys holds the ordered individual keys of each drawn solid path - it
	// is the latter that allows for the collapse operation to populate
	// drawn_paths.  s_map uses the same key as s_keys to map instances to
	// actual scene objects.
	std::unordered_map<unsigned long long, struct bv_scene_obj *> s_map;
	std::unordered_map<unsigned long long, std::vector<unsigned long long>> s_keys;

	// Set of hashes of all drawn paths and subpaths, constructed during the collapse
	// operation from the set of drawn solid paths.  This allows calling codes to
	// spot check any path to see if it is active, without having to interrogate
	// other data structures or walk down the tree.
	std::unordered_set<unsigned long long> drawn_paths;


	// Called when the parent Db context is getting ready to update the data
	// structures - we may need to redraw, so we save any necessary information
	// ahead of the changes.  Although this is a public function of the BViewState,
	// it is practically speaking an implementation detail
	void cache_collapsed();

    private:
	DbiState *dbis;

	int check_status(std::unordered_set<unsigned long long> *invalid_objects, std::vector<unsigned long long> &cpath);

	void collapse(std::vector<std::vector<unsigned long long>> &collapsed);


	std::unordered_set<unsigned long long> all_fully_drawn;

	// The collapsed drawn paths from the previous db state
	std::vector<std::vector<unsigned long long>> prev_collapsed;

	std::vector<unsigned long long> active_paths;
};

class GED_EXPORT DbiState {
    public:
	DbiState(struct db_i *);
	~DbiState();

	void update();

	bool path_color(struct bu_color *c, std::vector<unsigned long long> &elements);

	bool get_matrix(matp_t m, unsigned long long p_key, unsigned long long i_key);
	bool get_path_matrix(matp_t m, std::vector<unsigned long long> &elements);

	bool get_bbox(point_t *bbmin, point_t *bbmax, matp_t curr_mat, unsigned long long hash);
	bool get_path_bbox(point_t *bbmin, point_t *bbmax, std::vector<unsigned long long> &elements);

	void print_path(struct bu_vls *opath, std::vector<unsigned long long> &path);

	std::vector<unsigned long long> digest_path(const char *path);

	unsigned long long path_hash(std::vector<unsigned long long> &path, size_t max_len);

	// These maps are the ".g ground truth" of the comb structures - the set
	// associated with each has contains all the child hashes from the comb
	// definition in the database for quick lookup, and the vector preserves
	// the comb ordering for listing.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> p_c;
	// Note: to match MGED's 'l' printing you need to use a reverse_iterator
	std::unordered_map<unsigned long long, std::vector<unsigned long long>> p_v;

	// Translate individual object hashes to their directory names.  This map must
	// be updated any time a database object changes to remain valid.
	std::unordered_map<unsigned long long, struct directory *> d_map;

	// For invalid comb entry strings, we can't point to a directory pointer.  This
	// map must also be updated after every db change - if a directory pointer hash
	// maps to an entry in this map it needs to be removed, and newly invalid entries
	// need to be added.
	std::unordered_map<unsigned long long, std::string> invalid_entry_map;

	// This is a map of non-uniquely named child instances (i.e. instances that must be
	// numbered) to the .g database name associated with those instances.  Allows for
	// one unique entry in p_c rather than requiring per-instance duplication
	std::unordered_map<unsigned long long, unsigned long long> i_map;
	std::unordered_map<unsigned long long, std::string> i_str;

	// Matrices above comb instances are critical to geometry placement.  For non-identity
	// matrices, we store them locally so they may be accessed without having to unpack
	// the comb from disk.
	std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>> matrices;

	// Similar to matrices, store non-union bool ops for instances
	std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>> i_bool;


	// Bounding boxes for each solid.  To calculate the bbox for a comb, the
	// children are walked combining the bboxes.  The idea is to be able to
	// retrieve solid bboxes and calculate comb bboxes without having to touch
	// the disk beyond the initial per-solid calculations, which may be done
	// once per load and/or dimensional change.
	std::unordered_map<unsigned long long, std::vector<fastf_t>> bboxes;


	// We also have a number of standard attributes that can impact drawing,
	// which are normally only accessible by loading in the attributes of
	// the object.  We stash them in maps to have the information available
	// without having to interrogate the disk
	std::unordered_map<unsigned long long, int> c_inherit; // color inheritance flag
	std::unordered_map<unsigned long long, unsigned int> rgb; // color RGB value  (r + (g << 8) + (b << 16))
	std::unordered_map<unsigned long long, int> region_id; // region_id


	// Data to be used by callbacks
	std::unordered_set<struct directory *> added;
	std::unordered_set<struct directory *> changed;
	std::unordered_set<unsigned long long> changed_hashes;
	std::unordered_set<unsigned long long> removed;
	std::unordered_map<unsigned long long, std::string> old_names;

	// The shared view is common to multiple views, so we always update it.
	// For other associated views (if any), we track their drawn states
	// separately, but they too need to update in response to database
	// changes (as well as draw/erase commands).
	BViewState *shared_vs;
	std::unordered_map<struct bview *, BViewState *> view_states;

	// Database Instance associated with this container
	struct db_i *dbip;

	bool need_update_nref = true;

    private:
	void populate_maps(struct directory *dp, unsigned long long phash, int reset);
	unsigned long long update_dp(struct directory *dp, int reset);
	unsigned int color_int(struct bu_color *);
	int int_color(struct bu_color *c, unsigned int);
	struct resource *res = NULL;
};


#else

/* Placeholders to allow ged.h to compile when we're compiling with a C compiler */
typedef struct _dbi_state {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} DbiState;
typedef struct _bview_state {
    int dummy; /* MS Visual C hack which can be removed if the struct contains something meaningful */
} BViewState;

#endif

__BEGIN_DECLS

struct ged_cmd;

/* struct details are private - use accessor functions to manipulate */
struct ged_results;

struct ged {
    struct bu_vls               go_name;
    struct rt_wdb		*ged_wdbp;
    struct db_i                 *dbip;
    DbiState			*dbi_state;

    /*************************************************************/
    /* Information pertaining to views and view objects .        */
    /*************************************************************/
    /* The current view */
    struct bview		*ged_gvp;
    /* The full set of views associated with this ged object */
    struct bview_set            ged_views;
    /* Drawing data associated with this .g file */
    struct bg_mesh_lod_context  *ged_lod;


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

    /* Selection data */
    struct ged_selection_sets	*ged_selection_sets;
    struct ged_selection_set    *ged_cset;


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

    struct ged_callback_state    *ged_cbs;
    void			(*ged_refresh_handler)(void *);	/**< @brief  function for handling refresh requests */
    void			*ged_refresh_clientdata;	/**< @brief  client data passed to refresh handler */
    void			(*ged_output_handler)(struct ged *, char *);	/**< @brief  function for handling output */
    void			(*ged_create_vlist_scene_obj_callback)(struct bv_scene_obj *);	/**< @brief  function to call after creating a vlist to create display list for solid */
    void			(*ged_create_vlist_display_list_callback)(struct display_list *);	/**< @brief  function to call after all vlist created that loops through creating display list for each solid  */
    void			(*ged_destroy_vlist_callback)(unsigned int, int);	/**< @brief  function to call after freeing a vlist */


    /* Functions assigned to ged_subprocess init_clbk and end_clbk
     * slots when the ged_subprocess is created.  TODO - eventually
     * this should be command-specific callback registrations, but
     * first we'll get the basic mechanism working, then introduce
     * that extra complication... */
    void			(*ged_subprocess_init_callback)(int, void *);	/**< @brief  function called when process starts */
    void			(*ged_subprocess_end_callback)(int, void *);	/**< @brief  function called when process completes */
    void			*ged_subprocess_clbk_context;

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
    int (*fbs_is_listening)(struct fbserv_obj *);          /**< @brief return 1 if listening, else 0 */
    int (*fbs_listen_on_port)(struct fbserv_obj *, int);  /**< @brief return 1 on success, 0 on failure */
    void (*fbs_open_server_handler)(struct fbserv_obj *);   /**< @brief platform/toolkit method to open listener handler */
    void (*fbs_close_server_handler)(struct fbserv_obj *);   /**< @brief platform/toolkit method to close handler listener */
    void (*fbs_open_client_handler)(struct fbserv_obj *, int, void *);   /**< @brief platform/toolkit specific client handler setup (called by fbs_new_client) */
    void (*fbs_close_client_handler)(struct fbserv_obj *, int);   /**< @brief platform/toolkit method to close handler for client at index client_id */

    // Other callbacks...
    // Tcl command strings - these are libtclcad level callbacks that execute user supplied Tcl commands if set:
    // gdv_callback, gdv_edit_motion_delta_callback, go_more_args_callback, go_rt_end_callback
    //
    // fbserv_obj: fbs_callback
    // bv.h gv_callback (only used by MGED?)
    // db_search_callback_t

    // TODO - this probably should be handled with a registration function of some kind
    // that assigns contexts based on dm type - right now the dms more or less have to
    // assume that whatever is in here is intended for their initialization, and if a
    // program wants to attach multiple types of DMs a single pointer is not a great way
    // to manage things.  A map would be better, but that's a C++ construct so we can't
    // expose it directly here - need some sort of means to manage the setting and getting,
    // maybe with functions like:
    // ged_ctx_set(const char *dm_type, void *ctx)
    // ged_ctx_get(const char *dm_type)
    void *ged_ctx; /* Temporary - do not rely on when designing new functionality */

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
	int ged_check_argc_gt_0_quiet = (_flags) & BRLCAD_QUIET; \
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
