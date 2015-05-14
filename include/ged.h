/*                           G E D . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file ged.h
 *
 * Functions provided by the LIBGED geometry editing library.  These
 * routines are a procedural basis for the geometric editing
 * capabilities available in BRL-CAD.  The library is tightly coupled
 * to the LIBRT library for geometric representation and analysis.
 *
 */

#ifndef GED_H
#define GED_H

#include "common.h"

#include "solid.h"
#include "dm/bview.h"
#include "raytrace.h"
#include "fbserv_obj.h"


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

/** Check if the object is a combination */
#define	GED_CHECK_COMB(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_COMB) == 0) { \
	int ged_check_comb_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_comb_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a combination", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_gedp, _flags) \
    if ((_gedp) == GED_NULL || (_gedp)->ged_wdbp == RT_WDB_NULL || (_gedp)->ged_wdbp->dbip == DBI_NULL) { \
	int ged_check_database_open_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_database_open_quiet) { \
	    if ((_gedp) != GED_NULL) { \
		bu_vls_trunc((_gedp)->ged_result_str, 0); \
		bu_vls_printf((_gedp)->ged_result_str, "A database is not open!"); \
	    } else {\
		bu_log("A database is not open!\n"); \
	    } \
	} \
	return (_flags); \
    }

/** Check if a drawable exists */
#define GED_CHECK_DRAWABLE(_gedp, _flags) \
    if (_gedp->ged_gdp == GED_DRAWABLE_NULL) { \
	int ged_check_drawable_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_drawable_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A drawable does not exist."); \
	} \
	return (_flags); \
    }

/** Check if a view exists */
#define GED_CHECK_VIEW(_gedp, _flags) \
    if (_gedp->ged_gvp == GED_VIEW_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A view does not exist."); \
	} \
	return (_flags); \
    }

#define GED_CHECK_FBSERV(_gedp, _flags) \
    if (_gedp->ged_fbsp == FBSERV_OBJ_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A framebuffer server object does not exist."); \
	} \
	return (_flags); \
    }

#define GED_CHECK_FBSERV_FBP(_gedp, _flags) \
    if (_gedp->ged_fbsp->fbs_fbp == FB_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A framebuffer IO structure does not exist."); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_CHECK_EXISTS(_gedp, _name, _noisy, _flags) \
    if (db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy)) != RT_DIR_NULL) { \
	int ged_check_exists_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_exists_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s already exists.", (_name)); \
	} \
	return (_flags); \
    }

/** Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_gedp, _flags) \
    if ((_gedp)->ged_wdbp->dbip->dbi_read_only) { \
	int ged_check_read_only_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_read_only_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "Sorry, this database is READ-ONLY."); \
	} \
	return (_flags); \
    }

/** Check if the object is a region */
#define	GED_CHECK_REGION(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_REGION) == 0) { \
	int ged_check_region_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_region_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s is not a region.", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** make sure there is a command name given */
#define GED_CHECK_ARGC_GT_0(_gedp, _argc, _flags) \
    if ((_argc) < 1) { \
	int ged_check_argc_gt_0_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_argc_gt_0_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "Command name not provided on (%s:%d).", __FILE__, __LINE__); \
	} \
	return (_flags); \
    }

/** add a new directory entry to the currently open database */
#define GED_DB_DIRADD(_gedp, _dp, _name, _laddr, _len, _dirflags, _ptr, _flags) \
    if (((_dp) = db_diradd((_gedp)->ged_wdbp->dbip, (_name), (_laddr), (_len), (_dirflags), (_ptr))) == RT_DIR_NULL) { \
	int ged_db_diradd_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_diradd_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to add %s to the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_DB_LOOKUP(_gedp, _dp, _name, _noisy, _flags) \
    if (((_dp) = db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy))) == RT_DIR_NULL) { \
	int ged_db_lookup_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_lookup_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Unable to find %s in the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Get internal representation */
#define GED_DB_GET_INTERNAL(_gedp, _intern, _dp, _mat, _resource, _flags) \
    if (rt_db_get_internal((_intern), (_dp), (_gedp)->ged_wdbp->dbip, (_mat), (_resource)) < 0) { \
	int ged_db_get_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_get_internal_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read failure."); \
	} \
	return (_flags); \
    }

/** Put internal representation */
#define GED_DB_PUT_INTERNAL(_gedp, _dp, _intern, _resource, _flags) \
    if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, (_intern), (_resource)) < 0) { \
	int ged_db_put_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_put_internal_quiet) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database write failure."); \
	} \
	return (_flags); \
    }

/* From include/dm.h */
#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

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

/* FIXME: leftovers from dg.h */
#define RT_VDRW_PREFIX          "_VDRW"
#define RT_VDRW_PREFIX_LEN      6
#define RT_VDRW_MAXNAME         31
#define RT_VDRW_DEF_COLOR       0xffff00
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

    /* TODO: add support for returning an array of objects, not just a
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
    void			(*ged_create_vlist_callback)(struct display_list *);	/**< @brief  function to call after creating a vlist */
    void			(*ged_free_vlist_callback)();	/**< @brief  function to call after freeing a vlist */

    /* FIXME -- this ugly hack needs to die.  the result string should be stored before the call. */
    int 			ged_internal_call;

    /* FOR LIBGED INTERNAL USE */
    struct ged_cmd *cmds;
    int (*add)(const struct ged_cmd *cmd);
    int (*del)(const char *name);
    int (*run)(int ac, char *av[]);

    /* Interface to LIBDM */
    int ged_dm_width;
    int ged_dm_height;
    int ged_dmp_is_null;
    void (*ged_dm_get_display_image)(struct ged *, unsigned char **);

};

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
typedef void (*ged_refresh_callback_ptr)(void *);
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


/* defined in adc.c */
GED_EXPORT extern void ged_calc_adc_pos(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_a1(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_a2(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_dst(struct bview *gvp);

/* defined in clip.c */
GED_EXPORT extern int ged_clip(fastf_t *xp1,
			       fastf_t *yp1,
			       fastf_t *xp2,
			       fastf_t *yp2);
GED_EXPORT extern int ged_vclip(vect_t a,
				vect_t b,
				fastf_t *min,
				fastf_t *max);

/* defined in copy.c */
GED_EXPORT extern int ged_dbcopy(struct ged *from_gedp,
				 struct ged *to_gedp,
				 const char *from,
				 const char *to,
				 int fflag);

/* defined in display_list.c */
GED_EXPORT void dl_set_iflag(struct bu_list *hdlp, int iflag);
GED_EXPORT extern void dl_color_soltab(struct bu_list *hdlp);
GED_EXPORT extern void dl_erasePathFromDisplay(struct bu_list *hdlp,
	                                       struct db_i *dbip,
					       void (*callback)(unsigned int, int),
					       const char *path,
					       int allow_split,
					       struct solid *freesolid);
GED_EXPORT extern struct display_list *dl_addToDisplay(struct bu_list *hdlp, struct db_i *dbip, const char *name);
/* When finalized, this stuff belongs in a header file of its own */
struct polygon_header {
    uint32_t magic;             /* magic number */
    int ident;                  /* identification number */
    int interior;               /* >0 => interior loop, gives ident # of exterior loop */
    vect_t normal;                      /* surface normal */
    unsigned char color[3];     /* Color from containing region */
    int npts;                   /* number of points */
};
#define POLYGON_HEADER_MAGIC 0x8623bad2
GED_EXPORT extern void dl_polybinout(struct bu_list *hdlp, struct polygon_header *ph, FILE *fp);

GED_EXPORT extern int invent_solid(struct bu_list *hdlp, struct db_i *dbip, void (*callback_create)(struct display_list *), void (*callback_free)(unsigned int, int), char *name, struct bu_list *vhead, long int rgb, int copy, fastf_t transparency, int dmode, struct solid *freesolid, int csoltab);


/* defined in ged.c */
GED_EXPORT extern void ged_close(struct ged *gedp);
GED_EXPORT extern void ged_free(struct ged *gedp);
GED_EXPORT extern void ged_init(struct ged *gedp);
/* Call BU_PUT to release returned ged structure */
GED_EXPORT extern struct ged *ged_open(const char *dbtype,
				       const char *filename,
				       int existing_only);
GED_EXPORT extern void ged_view_init(struct bview *gvp);

/* defined in grid.c */
GED_EXPORT extern void ged_snap_to_grid(struct ged *gedp, fastf_t *vx, fastf_t *vy);

/* defined in inside.c */
GED_EXPORT extern int ged_inside_internal(struct ged *gedp,
					  struct rt_db_internal *ip,
					  int argc,
					  const char *argv[],
					  int arg,
					  char *o_name);

/* defined in rt.c */
GED_EXPORT extern int ged_build_tops(struct ged	*gedp,
				     char		**start,
				     char		**end);
GED_EXPORT extern size_t ged_count_tops(struct ged *gedp);


/* Defined in vutil.c */
GED_EXPORT extern void ged_persp_mat(fastf_t *m,
				     fastf_t fovy,
				     fastf_t aspect,
				     fastf_t near1,
				     fastf_t far1,
				     fastf_t backoff);
GED_EXPORT extern void ged_mike_persp_mat(fastf_t *pmat,
					  const fastf_t *eye);
GED_EXPORT extern void ged_deering_persp_mat(fastf_t *m,
					     const fastf_t *l,
					     const fastf_t *h,
					     const fastf_t *eye);
GED_EXPORT extern void ged_view_update(struct bview *gvp);


/**
 * Creates an arb8 given the following:
 *   1)   3 points of one face
 *   2)   coord x, y or z and 2 coordinates of the 4th point in that face
 *   3)   thickness
 */
GED_EXPORT extern int ged_3ptarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Angle distance cursor.
 */
GED_EXPORT extern int ged_adc(struct ged *gedp, int argc, const char *argv[]);

/**
 * Adjust object's attribute(s)
 */
GED_EXPORT extern int ged_adjust(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates an arb8 given rotation and fallback angles
 */
GED_EXPORT extern int ged_arb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert az/el to a direction vector.
 */
GED_EXPORT extern int ged_ae2dir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the azimuth, elevation and twist.
 */
GED_EXPORT extern int ged_aet(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns lots of information about the specified object(s)
 */
GED_EXPORT extern int ged_analyze(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates an annotation.
 */
GED_EXPORT extern int ged_annotate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Append a pipe point.
 */
GED_EXPORT extern int ged_append_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Allow editing of the matrix, etc., along an arc.
 */
GED_EXPORT extern int ged_arced(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set, get, show, remove or append to attribute values for the specified object.
 * The arguments for "set" and "append" subcommands are attribute name/value pairs.
 * The arguments for "get", "rm", and "show" subcommands are attribute names.
 * The "set" subcommand sets the specified attributes for the object.
 * The "append" subcommand appends the provided value to an existing attribute,
 * or creates a new attribute if it does not already exist.
 * The "get" subcommand retrieves and displays the specified attributes.
 * The "rm" subcommand deletes the specified attributes.
 * The "show" subcommand does a "get" and displays the results in a user readable format.
 */
GED_EXPORT extern int ged_attr(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate angle degrees about the specified axis
 */
GED_EXPORT extern int ged_arot_args(struct ged *gedp, int argc, const char *argv[], mat_t rmat);
GED_EXPORT extern int ged_arot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Auto-adjust the view so that all displayed geometry is in view
 */
GED_EXPORT extern int ged_autoview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Report the size of the bounding box (rpp) around the specified object
 */
GED_EXPORT extern int ged_bb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Tessellates each operand object, then performs the
 * boolean evaluation, storing result in 'new_obj'
 */
GED_EXPORT extern int ged_bev(struct ged *gedp, int argc, const char *argv[]);

/**
 * Manipulate opaque binary objects.
 * Must specify one of -i (for creating or adjusting objects (input))
 * or -o for extracting objects (output).
 * If the major type is "u" the minor type must be one of:
 *   "f" -> float
 *   "d" -> double
 *   "c" -> char (8 bit)
 *   "s" -> short (16 bit)
 *   "i" -> int (32 bit)
 *   "l" -> long (64 bit)
 *   "C" -> unsigned char (8 bit)
 *   "S" -> unsigned short (16 bit)
 *   "I" -> unsigned int (32 bit)
 *   "L" -> unsigned long (64 bit)
 * For input, source is a file name and dest is an object name.
 * For output source is an object name and dest is a file name.
 * Only uniform array binary objects (major_type=u) are currently supported}}
 */
GED_EXPORT extern int ged_bo(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase all currently displayed geometry and draw the specified object(s)
 */
GED_EXPORT extern int ged_blast(struct ged *gedp, int argc, const char *argv[]);

/**
 * Query or manipulate properties of bot
 */
GED_EXPORT extern int ged_bot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create new_bot by condensing old_bot
 */
GED_EXPORT extern int ged_bot_condense(struct ged *gedp, int argc, const char *argv[]);

/**
 * Uses edge decimation to reduce the number of triangles in the
 * specified BOT while keeping within the specified constraints.
 */
GED_EXPORT extern int ged_bot_decimate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump bots to the specified format.
 */
GED_EXPORT extern int ged_bot_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump displayed bots to the specified format.
 */
GED_EXPORT extern int ged_dbot_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot edge. This splits the triangles that share the edge.
 */
GED_EXPORT extern int ged_bot_edge_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create new_bot by fusing faces in old_bot
 */
GED_EXPORT extern int ged_bot_face_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sort the facelist of BOT solids to optimize ray trace performance
 * for a particular number of triangles per raytrace piece.
 */
GED_EXPORT extern int ged_bot_face_sort(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot face into three parts (i.e. by adding a point to the center)
 */
GED_EXPORT extern int ged_bot_face_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Flip/reverse the specified bot's orientation.
 */
GED_EXPORT extern int ged_bot_flip(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fuse bot
 */
GED_EXPORT extern int ged_bot_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create bot_dest by merging the bot sources.
 */
GED_EXPORT extern int ged_bot_merge(struct ged *gedp, int argc, const char *argv[]);

/**
 * Calculate vertex normals for the BOT primitive
 */
GED_EXPORT extern int ged_bot_smooth(struct ged *gedp, int argc, const char *argv[]);

/**
 * Split the specified bot
 */
GED_EXPORT extern int ged_bot_split(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sync the specified bot's orientation (i.e. make sure all neighboring triangles have same orientation).
 */
GED_EXPORT extern int ged_bot_sync(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fuse bot vertices
 */
GED_EXPORT extern int ged_bot_vertex_fuse(struct ged *gedp, int argc, const char *argv[]);

/**
 * BREP utility command
 */
GED_EXPORT extern int ged_brep(struct ged *gedp, int argc, const char *argv[]);

/**
 * List attributes (brief).
 */
GED_EXPORT extern int ged_cat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create constraint object
 */
GED_EXPORT extern int ged_cc(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the view center.
 */
GED_EXPORT extern int ged_center(struct ged *gedp, int argc, const char *argv[]);

/**
 * Performs a deep copy of object.
 */
GED_EXPORT extern int ged_clone(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make coil shapes.
 */
GED_EXPORT extern int ged_coil(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make color entry.
 */
GED_EXPORT extern int ged_color(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set combination color.
 */
GED_EXPORT extern int ged_comb_color(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or extend combination w/booleans.
 */
GED_EXPORT extern int ged_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or extend a combination using standard notation.
 */
GED_EXPORT extern int ged_comb_std(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get comb's members.
 */
GED_EXPORT extern int ged_combmem(struct ged *gedp, int argc, const char *argv[]);

/**
 * create, update, remove, and list geometric and dimensional constraints.
 */
GED_EXPORT extern int ged_constraint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Import a database into the current database using an auto-incrementing or custom affix
 */
GED_EXPORT extern int ged_concat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy a database object
 */
GED_EXPORT extern int ged_copy(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy an 'evaluated' path solid
 */
GED_EXPORT extern int ged_copyeval(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy the matrix from one combination's arc to another.
 */
GED_EXPORT extern int ged_copymat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Copy cylinder and position at end of original cylinder
 */
GED_EXPORT extern int ged_cpi(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get dbip
 */
GED_EXPORT extern int ged_dbip(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get libbu's debug bit vector
 */
GED_EXPORT extern int ged_debugbu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump of the database's directory
 */
GED_EXPORT extern int ged_debugdir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's debug bit vector
 */
GED_EXPORT extern int ged_debuglib(struct ged *gedp, int argc, const char *argv[]);

/**
 * Provides user-level access to LIBBU's bu_prmem()
 */
GED_EXPORT extern int ged_debugmem(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_memprint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get librt's NMG debug bit vector
 */
GED_EXPORT extern int ged_debugnmg(struct ged *gedp, int argc, const char *argv[]);

/**
 * Decompose nmg_solid into maximally connected shells
 */
GED_EXPORT extern int ged_decompose(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delay the specified amount of time
 */
GED_EXPORT extern int ged_delay(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delete the specified pipe point.
 */
GED_EXPORT extern int ged_delete_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert a direction vector to az/el.
 */
GED_EXPORT extern int ged_dir2ae(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_draw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Dump a full copy of the database into file.g
 */
GED_EXPORT extern int ged_dump(struct ged *gedp, int argc, const char *argv[]);

/**
 * Check for duplicate names in file
 */
GED_EXPORT extern int ged_dup(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_E(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare all regions with the given air code(s) for display
 */
GED_EXPORT extern int ged_eac(struct ged *gedp, int argc, const char *argv[]);

/**
 * Echo the specified arguments.
 */
GED_EXPORT extern int ged_echo(struct ged *gedp, int argc, const char *argv[]);

/**
 * Arb specific edits.
 */
GED_EXPORT extern int ged_edarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Text edit the color table
 */
GED_EXPORT extern int ged_edcolor(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit region ident codes.
 */
GED_EXPORT extern int ged_edcodes(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit combination.
 */
GED_EXPORT extern int ged_edcomb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit objects, by using subcommands.
 */
GED_EXPORT extern int ged_edit(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit file.
 */
GED_EXPORT extern int ged_editit(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit combination materials.
 *
 * Command relies on rmater, editit, and wmater commands.
 */
GED_EXPORT extern int ged_edmater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase objects from the display.
 */
GED_EXPORT extern int ged_erase(struct ged *gedp, int argc, const char *argv[]);

/**
 * Evaluate objects via NMG tessellation
 */
GED_EXPORT extern int ged_ev(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the eye point
 */
GED_EXPORT extern int ged_eye(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the eye position
 */
GED_EXPORT extern int ged_eye_pos(struct ged *gedp, int argc, const char *argv[]);

/**
 * Checks to see if the specified database object exists.
 */
GED_EXPORT extern int ged_exists(struct ged *gedp, int argc, const char *argv[]);

/**
 * Globs expression against database objects
 */
GED_EXPORT extern int ged_expand(struct ged *gedp, int argc, const char *argv[]);

/**
 * Facetize the specified objects
 */
GED_EXPORT extern int ged_facetize(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fb2pix writes a framebuffer image to a .pix file.
 */
GED_EXPORT extern int ged_fb2pix(struct ged *gedp, int argc, const char *argv[]);

/**
 * Fclear clears a framebuffer.
 */
GED_EXPORT extern int ged_fbclear(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find combinations that reference object
 */
GED_EXPORT extern int ged_find(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the arb edge nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_arb_edge_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the bot edge nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_bot_edge_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the bot point nearest the specified point in view coordinates.
 */
GED_EXPORT extern int ged_find_botpt_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Add a metaball point.
 */
GED_EXPORT extern int ged_add_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Delete a metaball point.
 */
GED_EXPORT extern int ged_delete_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the metaball point nearest the specified point in model coordinates.
 */
GED_EXPORT extern int ged_find_metaballpt_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move a metaball point.
 */
GED_EXPORT extern int ged_move_metaballpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find the pipe point nearest the specified point in model coordinates.
 */
GED_EXPORT extern int ged_find_pipept_nearest_pt(struct ged *gedp, int argc, const char *argv[]);

/**
 * returns form for objects of type "type"
 */
GED_EXPORT extern int ged_form(struct ged *gedp, int argc, const char *argv[]);

/**
 * Given an NMG solid, break it up into several NMG solids, each
 * containing a single shell with a single sub-element.
 */
GED_EXPORT extern int ged_fracture(struct ged *gedp, int argc, const char *argv[]);

/**
 * Calculate a geometry diff
 */
GED_EXPORT extern int ged_gdiff(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get object attributes
 */
GED_EXPORT extern int ged_get(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get view size and center such that all displayed solids would be in view
 */
GED_EXPORT extern int ged_get_autoview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get a bot's edges
 */
GED_EXPORT extern int ged_get_bot_edges(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get combination information
 */
GED_EXPORT extern int ged_get_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the viewsize, orientation and eye point.
 */
GED_EXPORT extern int ged_get_eyemodel(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the object's type
 */
GED_EXPORT extern int ged_get_type(struct ged *gedp, int argc, const char *argv[]);

/**
 * Globs expression against the database
 */
GED_EXPORT extern int ged_glob(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_gqa(struct ged *gedp, int argc, const char *argv[]);

/**
 * Query or manipulate properties of a graph.
 */
GED_EXPORT extern int ged_graph(struct ged *gedp, int argc, const char *argv[]);

/**
 * Grid utility command.
 */
GED_EXPORT extern int ged_grid(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert grid coordinates to model coordinates.
 */
GED_EXPORT extern int ged_grid2model_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert grid coordinates to view coordinates.
 */
GED_EXPORT extern int ged_grid2view_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a group
 */
GED_EXPORT extern int ged_group(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the "hidden" flag for the specified objects so they do not
 * appear in an "ls" command output
 */
GED_EXPORT extern int ged_hide(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns how an object is being displayed
 */
GED_EXPORT extern int ged_how(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a human
 */
GED_EXPORT extern int ged_human(struct ged *gedp, int argc, const char *argv[]);

/**
 * Illuminate/highlight database object.
 */
GED_EXPORT extern int ged_illum(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a primitive via keyboard.
 */
GED_EXPORT extern int ged_in(struct ged *gedp, int argc, const char *argv[]);

/**
 * Finds the inside primitive per the specified thickness.
 */
GED_EXPORT extern int ged_inside(struct ged *gedp, int argc, const char *argv[]);

/**
 * Add instance of obj to comb
 */
GED_EXPORT extern int ged_instance(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes a bot object out of the specified section.
 */
GED_EXPORT extern int ged_importFg4Section(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the inverse view size.
 */
GED_EXPORT extern int ged_isize(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set region ident codes.
 */
GED_EXPORT extern int ged_item(struct ged *gedp, int argc, const char *argv[]);

/**
  * Joint command ported to the libged library.
  */
GED_EXPORT extern int ged_joint(struct ged *gedp, int argc, const char *argv[]);

/**
  * New joint command.
  */
GED_EXPORT extern int ged_joint2(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save/keep the specified objects in the specified file
 */
GED_EXPORT extern int ged_keep(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the keypoint
 */
GED_EXPORT extern int ged_keypoint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill/delete the specified objects from the database
 */
GED_EXPORT extern int ged_kill(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill/delete the specified objects from the database, removing all references
 */
GED_EXPORT extern int ged_killall(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill all references to the specified object(s).
 */
GED_EXPORT extern int ged_killrefs(struct ged *gedp, int argc, const char *argv[]);

/**
 * Kill all paths belonging to objects
 */
GED_EXPORT extern int ged_killtree(struct ged *gedp, int argc, const char *argv[]);

/**
 * List object information, verbose.
 */
GED_EXPORT extern int ged_list(struct ged *gedp, int argc, const char *argv[]);

/**
 * Load the view
 */
GED_EXPORT extern int ged_loadview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Configure Level of Detail drawing.
 */
GED_EXPORT extern int ged_lod(struct ged *gedp, int argc, const char *argv[]);

/**
 * Used to control logging.
 */
GED_EXPORT extern int ged_log(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the look-at point
 */
GED_EXPORT extern int ged_lookat(struct ged *gedp, int argc, const char *argv[]);

/**
 * List the objects in this database
 */
GED_EXPORT extern int ged_ls(struct ged *gedp, int argc, const char *argv[]);

/**
 * List object's tree as a tcl list of {operator object} pairs
 */
GED_EXPORT extern int ged_lt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert the specified model point to a view point.
 */
GED_EXPORT extern int ged_m2v_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make a new primitive.
 */
GED_EXPORT extern int ged_make(struct ged *gedp, int argc, const char *argv[]);

/**
 * Creates a point-cloud (pnts) given the following:
 *   1)   object name
 *   2)   path and filename to point data file
 *   3)   point data file format (xyzrgbsijk?)
 *   4)   point data file units or conversion factor to mm
 *   5)   default diameter of each point
 */
GED_EXPORT extern int ged_make_pnts(struct ged *gedp, int argc, const char *argv[]);

/**
 * Make a unique object name.
 */
GED_EXPORT extern int ged_make_name(struct ged *gedp, int argc, const char *argv[]);

/**
 * Globs expression against database objects, does not return tokens that match nothing
 */
GED_EXPORT extern int ged_match(struct ged *gedp, int argc, const char *argv[]);

/**
 * Modify material information.
 */
GED_EXPORT extern int ged_mater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Mirror the primitive or combination along the specified axis.
 */
GED_EXPORT extern int ged_mirror(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert model coordinates to grid coordinates.
 */
GED_EXPORT extern int ged_model2grid_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the model to view matrix
 */
GED_EXPORT extern int ged_model2view(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert model coordinates to view coordinates.
 */
GED_EXPORT extern int ged_model2view_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move an arb's edge through point
 */
GED_EXPORT extern int ged_move_arb_edge(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move/rename a database object
 */
GED_EXPORT extern int ged_move(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move/rename all occurrences object
 */
GED_EXPORT extern int ged_move_all(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move an arb's face through point
 */
GED_EXPORT extern int ged_move_arb_face(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified bot point. This can be relative or absolute.
 */
GED_EXPORT extern int ged_move_botpt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified bot points. This movement is always relative.
 */
GED_EXPORT extern int ged_move_botpts(struct ged *gedp, int argc, const char *argv[]);

/**
 * Move the specified pipe point.
 */
GED_EXPORT extern int ged_move_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view. Note - x, y and z are rotations in model coordinates.
 */
GED_EXPORT extern int ged_mrot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Trace a single ray from the current view.
 */
GED_EXPORT extern int ged_nirt(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_vnirt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Decimate NMG primitive via edge collapse
 */
GED_EXPORT extern int ged_nmg_collapse(struct ged *gedp, int argc, const char *argv[]);

/**
 * Attempt to fix an NMG primitive's normals.
 */
GED_EXPORT extern int ged_nmg_fix_normals(struct ged *gedp, int argc, const char *argv[]);

/**
 * Simplify the NMG primitive, if possible
 */
GED_EXPORT extern int ged_nmg_simplify(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get object center.
 */
GED_EXPORT extern int ged_ocenter(struct ged *gedp, int argc, const char *argv[]);

/**
 * Open a database
 */
GED_EXPORT extern int ged_reopen(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view orientation using a quaternion.
 */
GED_EXPORT extern int ged_orient(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate obj about the keypoint by
 */
GED_EXPORT extern int ged_orotate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale obj about the keypoint by sf.
 */
GED_EXPORT extern int ged_oscale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Translate obj by dx dy dz.
 */
GED_EXPORT extern int ged_otranslate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Overlay the specified 2D/3D UNIX plot file
 */
GED_EXPORT extern int ged_overlay(struct ged *gedp, int argc, const char *argv[]);

/**
 * List all paths from name(s) to leaves
 */
GED_EXPORT extern int ged_pathlist(struct ged *gedp, int argc, const char *argv[]);

/**
 * Lists all paths matching the input path
 */
GED_EXPORT extern int ged_pathsum(struct ged *gedp, int argc, const char *argv[]);

/**
 * Checks that each directory in the supplied path actually has the subdirectories
 * that are implied by the path.
 */
GED_EXPORT extern int ged_path_validate(struct ged *gedp, const struct db_full_path * const path);

/**
 * Set/get the perspective angle.
 */
GED_EXPORT extern int ged_perspective(struct ged *gedp, int argc, const char *argv[]);

/**
 * Pix2fb reads a pix file into a framebuffer.
 */
GED_EXPORT extern int ged_pix2fb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a unix plot file of the currently displayed objects.
 */
GED_EXPORT extern int ged_plot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the perspective matrix.
 */
GED_EXPORT extern int ged_pmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the pmodel2view matrix.
 */
GED_EXPORT extern int ged_pmodel2view(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a png file of the view.
 */
GED_EXPORT extern int ged_png(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_screen_grab(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write out polygons (binary) of the currently displayed geometry.
 */
GED_EXPORT extern int ged_polybinout(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set point of view
 */
GED_EXPORT extern int ged_pov(struct ged *gedp, int argc, const char *argv[]);

/**
 * Print color table
 */
GED_EXPORT extern int ged_prcolor(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prefix the specified objects with the specified prefix
 */
GED_EXPORT extern int ged_prefix(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepend a pipe point.
 */
GED_EXPORT extern int ged_prepend_pipept(struct ged *gedp, int argc, const char *argv[]);

/**
 * Preview a new style RT animation script.
 */
GED_EXPORT extern int ged_preview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a postscript file of the view.
 */
GED_EXPORT extern int ged_ps(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate obj's attributes by rvec.
 */
GED_EXPORT extern int ged_protate(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale obj's attributes by sf.
 */
GED_EXPORT extern int ged_pscale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set an obj's attribute to the specified value.
 */
GED_EXPORT extern int ged_pset(struct ged *gedp, int argc, const char *argv[]);

/**
 * Translate obj's attributes by tvec.
 */
GED_EXPORT extern int ged_ptranslate(struct ged *gedp, int argc, const char *argv[]);

/**
 *Pull objects' path transformations from primitives
 */
GED_EXPORT extern int ged_pull(struct ged *gedp, int argc, const char *argv[]);

/**
 * Push objects' path transformations to primitives
 */
GED_EXPORT extern int ged_push(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a database object
 */
GED_EXPORT extern int ged_put(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set combination attributes
 */
GED_EXPORT extern int ged_put_comb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Replace the matrix on an arc
 */
GED_EXPORT extern int ged_putmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set query_ray attributes
 */
GED_EXPORT extern int ged_qray(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the view orientation using a quaternion
 */
GED_EXPORT extern int ged_quat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view from a direction vector and twist.
 */
GED_EXPORT extern int ged_qvrot(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern void ged_init_qray(struct ged_drawable *gdp);
GED_EXPORT extern void ged_free_qray(struct ged_drawable *gdp);

/**
 * Read region ident codes from filename.
 */
GED_EXPORT extern int ged_rcodes(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rubber band rectangle utility.
 */
GED_EXPORT extern int ged_rect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Edit region/comb
 */
GED_EXPORT extern int ged_red(struct ged *gedp, int argc, const char *argv[]);

/**
 * Change the default region ident codes: item air los mat
 */
GED_EXPORT extern int ged_regdef(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a region
 */
GED_EXPORT extern int ged_region(struct ged *gedp, int argc, const char *argv[]);

/**
 * Remove members from a combination
 */
GED_EXPORT extern int ged_remove(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the solid table & vector list as a string
 */
GED_EXPORT extern int ged_report(struct ged *gedp, int argc, const char *argv[]);

/**
 * Makes and arb given a point, 2 coord of 3 pts, rot, fb and thickness.
 */
GED_EXPORT extern int ged_rfarb(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of id to region name mappings for the entire database.
 */
GED_EXPORT extern int ged_rmap(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the rotation matrix.
 */
GED_EXPORT extern int ged_rmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Read material properties from a file.
 */
GED_EXPORT extern int ged_rmater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view.
 */
GED_EXPORT extern int ged_rot_args(struct ged *gedp, int argc, const char *argv[], char *coord, mat_t rmat);
GED_EXPORT extern int ged_rot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the rotate_about point.
 */
GED_EXPORT extern int ged_rotate_about(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate an arb's face through point
 */
GED_EXPORT extern int ged_rotate_arb_face(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the point.
 */
GED_EXPORT extern int ged_rot_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Invoke any raytracing application with the current view.
 */
GED_EXPORT extern int ged_rrt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the previously defined rectangle.
 */
GED_EXPORT extern int ged_rselect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the raytracing application.
 */
GED_EXPORT extern int ged_rt(struct ged *gedp, int argc, const char *argv[]);

/**
 * Abort the current raytracing processes.
 */
GED_EXPORT extern int ged_rtabort(struct ged *gedp, int argc, const char *argv[]);

/**
 * Check for overlaps in the current view.
 */
GED_EXPORT extern int ged_rtcheck(struct ged *gedp, int argc, const char *argv[]);

/**
 * Run the rtwizard application.
 */
GED_EXPORT extern int ged_rtwizard(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save keyframe in file (experimental)
 */
GED_EXPORT extern int ged_savekey(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save the view
 */
GED_EXPORT extern int ged_saveview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale the view.
 */
GED_EXPORT extern int ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3);
GED_EXPORT extern int ged_scale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Interface to search functionality (i.e. Unix find for geometry)
 */
GED_EXPORT extern int ged_search(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the specified rectangle or circle.
 */
GED_EXPORT extern int ged_select(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return ged selections for specified object. Created if it doesn't
 * exist.
 */
GED_EXPORT struct rt_object_selections *ged_get_object_selections(struct ged *gedp,
								  const char *object_name);

/**
 * Return ged selections of specified kind for specified object.
 * Created if it doesn't exist.
 */
GED_EXPORT struct rt_selection_set *ged_get_selection_set(struct ged *gedp,
							  const char *object_name,
							  const char *selection_name);

/**
 * Get/set the output handler script
 */
GED_EXPORT extern int ged_set_output_script(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the unix plot output mode
 */
GED_EXPORT extern int ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the transparency of the specified object
 */
GED_EXPORT extern int ged_set_transparency(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view orientation given the angles x, y and z in degrees.
 */
GED_EXPORT extern int ged_setview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the shaded mode.
 */
GED_EXPORT extern int ged_shaded_mode(struct ged *gedp, int argc, const char *argv[]);

/**
 * Simpler, command-line version of 'mater' command.
 */
GED_EXPORT extern int ged_shader(struct ged *gedp, int argc, const char *argv[]);

/**
 * Breaks the NMG model into separate shells
 */
GED_EXPORT extern int ged_shells(struct ged *gedp, int argc, const char *argv[]);

/**
 * Show the matrix transformations along path
 */
GED_EXPORT extern int ged_showmats(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the view size.
 */
GED_EXPORT extern int ged_size(struct ged *gedp, int argc, const char *argv[]);

/**
 * Performs simulations.
 */
GED_EXPORT extern int ged_simulate(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_solids_on_ray(struct ged *gedp, int argc, const char *argv[]);

/**
 * Slew the view
 */
GED_EXPORT extern int ged_slew(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create or append objects to a group using a sphere
 */
GED_EXPORT extern int ged_sphgroup(struct ged *gedp, int argc, const char *argv[]);

/**
 * Count/list primitives/regions/groups
 */
GED_EXPORT extern int ged_summary(struct ged *gedp, int argc, const char *argv[]);

/**
 * Sync up the in-memory database with the on-disk database.
 */
GED_EXPORT extern int ged_sync(struct ged *gedp, int argc, const char *argv[]);

/**
 * The ged_tables() function serves idents, regions and solids.
 *
 * Make ascii summary of region idents.
 *
 */
GED_EXPORT extern int ged_tables(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a tire
 */
GED_EXPORT extern int ged_tire(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the database title
 */
GED_EXPORT extern int ged_title(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get tessellation and calculation tolerances
 */
GED_EXPORT extern int ged_tol(struct ged *gedp, int argc, const char *argv[]);

/**
 * Find all top level objects
 */
GED_EXPORT extern int ged_tops(struct ged *gedp, int argc, const char *argv[]);

/**
 * Translate the view.
 */
GED_EXPORT extern int ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec);
GED_EXPORT extern int ged_tra(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a track
 */
GED_EXPORT extern int ged_track(struct ged *gedp, int argc, const char *argv[]);

/**
 *
 *
 * Usage:
 *     tracker [-fh] [# links] [increment] [spline.iges] [link...]
 */
GED_EXPORT extern int ged_tracker(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 */
GED_EXPORT extern int ged_tree(struct ged *gedp, int argc, const char *argv[]);

/**
 * Unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output
 */
GED_EXPORT extern int ged_unhide(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the database units
 */
GED_EXPORT extern int ged_units(struct ged *gedp, int argc, const char *argv[]);

/**
 * Recalculate plots for displayed objects.
 */
GED_EXPORT extern int ged_redraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert the specified view point to a model point.
 */
GED_EXPORT extern int ged_v2m_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Vector drawing utility.
 */
GED_EXPORT extern int ged_vdraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the database version.
 */
GED_EXPORT extern int ged_version(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set view attributes
 */
GED_EXPORT extern int ged_view_func(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert view coordinates to grid coordinates.
 */
GED_EXPORT extern int ged_view2grid_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the view2model matrix.
 */
GED_EXPORT extern int ged_view2model(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert view coordinates to model coordinates.
 */
GED_EXPORT extern int ged_view2model_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert a view vector to a model vector.
 */
GED_EXPORT extern int ged_view2model_vec(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view. Note - x, y and z are rotations in view coordinates.
 */
GED_EXPORT extern int ged_vrot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the view direction.
 */
GED_EXPORT extern int ged_viewdir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write region ident codes to filename.
 */
GED_EXPORT extern int ged_wcodes(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the specified region's id.
 */
GED_EXPORT extern int ged_whatid(struct ged *gedp, int argc, const char *argv[]);

/**
 * The ged_which() function serves both whichair and whichid.
 *
 * Find the regions with the specified air codes.  Find the regions
 * with the specified region ids.
 */
GED_EXPORT extern int ged_which(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return all combinations with the specified shaders.
 */
GED_EXPORT extern int ged_which_shader(struct ged *gedp, int argc, const char *argv[]);

/**
 * List the objects currently prepped for drawing
 */
GED_EXPORT extern int ged_who(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write material properties to a file for specified combination(s).
 */
GED_EXPORT extern int ged_wmater(struct ged *gedp, int argc, const char *argv[]);

/**
 * Push object path transformations to solids, creating primitives if necessary
 */
GED_EXPORT extern int ged_xpush(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the view orientation using yaw, pitch and roll
 */
GED_EXPORT extern int ged_ypr(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase all currently displayed geometry
 */
GED_EXPORT extern int ged_zap(struct ged *gedp, int argc, const char *argv[]);

/**
 * Zoom the view in or out.
 */
GED_EXPORT extern int ged_zoom(struct ged *gedp, int argc, const char *argv[]);
/**
 * Voxelize the specified objects
 */
GED_EXPORT extern int ged_voxelize(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern bview_polygon *ged_clip_polygon(ClipType op, bview_polygon *subj, bview_polygon *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern bview_polygon *ged_clip_polygons(ClipType op, bview_polygons *subj, bview_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern int ged_export_polygon(struct ged *gedp, bview_data_polygon_state *gdpsp, size_t polygon_i, const char *sname);
GED_EXPORT extern bview_polygon *ged_import_polygon(struct ged *gedp, const char *sname);
GED_EXPORT extern fastf_t ged_find_polygon_area(bview_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size);
GED_EXPORT extern int ged_polygons_overlap(struct ged *gedp, bview_polygon *polyA, bview_polygon *polyB);
GED_EXPORT extern void ged_polygon_fill_segments(struct ged *gedp, bview_polygon *poly, vect2d_t vfilldir, fastf_t vfilldelta);



/* defined in trace.c */

#define _GED_MAX_LEVELS 12
struct _ged_trace_data {
    struct ged *gtd_gedp;
    struct directory *gtd_path[_GED_MAX_LEVELS];
    struct directory *gtd_obj[_GED_MAX_LEVELS];
    mat_t gtd_xform;
    int gtd_objpos;
    int gtd_prflag;
    int gtd_flag;
};


GED_EXPORT extern void ged_trace(struct directory *dp,
		       int pathpos,
		       const mat_t old_xlate,
		       struct _ged_trace_data *gtdp,
		       int verbose);


/* defined in get_obj_bounds.c */
GED_EXPORT extern int ged_get_obj_bounds(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       int use_air,
			       point_t rpp_min,
			       point_t rpp_max);


/* defined in track.c */
GED_EXPORT extern int ged_track2(struct bu_vls *log_str, struct rt_wdb *wdbp, const char *argv[]);

/* defined in wdb_importFg4Section.c */
GED_EXPORT int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);


/***************************************
 * Conceptual Documentation for LIBGED *
 ***************************************
 *
 * Below are developer notes for a data structure layout that this
 * library is being migrated towards.  This is not necessarily the
 * current status of the library, but rather a high-level concept for
 * how the data might be organized down the road for the core data
 * structures available for application and extension management.
 *
 * struct ged {
 *   dbip
 *   views * >-----.
 *   result()      |
 * }               |
 *                 |
 * struct view { <-'
 *   geometry * >------.
 *   update()          |
 * }                   |
 *                     |
 * struct geometry { <-'
 *   display lists
 *   directory *
 *   update()
 * }
 *
 */

__END_DECLS

#endif /* GED_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
