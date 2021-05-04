/*                        B V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup bview
 *
 * @brief
 * Types and definitions related to display lists, angle distance cursor,
 * and other generic view constructs.
 *
 * This header is intended to be independent of any one BRL-CAD library and is
 * specifically intended to allow the easy definition of common display list
 * types between otherwise independent libraries (libdm and libged, for
 * example).
 *
 *
 * NEXT STEPS:  get a selection set for the view organized, add commands that
 * allow the view2 command to create(view) and select (view or solid) objects,
 * and figure out how to allow per-object xy handling callbacks.  (The latter
 * will probably in the end be how we implement primitive editing as well...)
 *
 * Test case will be the polygon circle - create and resize.  May want to
 * switch the container being used from a raw array to a bu_ptbl...  eventually
 * would probably be better to have polygons be first class view scene
 * objects...
 *
 */
#ifndef DM_BVIEW_H
#define DM_BVIEW_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/ptbl.h"
#include "bg/polygon_types.h"
#include "vmath.h"

/** @{ */
/** @file bview.h */

#ifndef BVIEW_EXPORT
#  if defined(BVIEW_DLL_EXPORTS) && defined(BVIEW_DLL_IMPORTS)
#    error "Only BVIEW_DLL_EXPORTS or BVIEW_DLL_IMPORTS can be defined, not both."
#  elif defined(BVIEW_DLL_EXPORTS)
#    define BVIEW_EXPORT COMPILER_DLLEXPORT
#  elif defined(BVIEW_DLL_IMPORTS)
#    define BVIEW_EXPORT COMPILER_DLLIMPORT
#  else
#    define BVIEW_EXPORT
#  endif
#endif

#define BVIEW_POLY_CIRCLE_MODE 15
#define BVIEW_POLY_CONTOUR_MODE 16

#define BVIEW_MINVIEWSIZE 0.0001
#define BVIEW_MINVIEWSCALE 0.00005

#ifndef UP
#  define UP 0
#endif
#ifndef DOWN
#  define DOWN 1
#endif

#define BVIEW_ANCHOR_AUTO          0
#define BVIEW_ANCHOR_BOTTOM_LEFT   1
#define BVIEW_ANCHOR_BOTTOM_CENTER 2
#define BVIEW_ANCHOR_BOTTOM_RIGHT  3
#define BVIEW_ANCHOR_MIDDLE_LEFT   4
#define BVIEW_ANCHOR_MIDDLE_CENTER 5
#define BVIEW_ANCHOR_MIDDLE_RIGHT  6
#define BVIEW_ANCHOR_TOP_LEFT      7
#define BVIEW_ANCHOR_TOP_CENTER    8
#define BVIEW_ANCHOR_TOP_RIGHT     9
struct bview_label {
    int           size;
    struct bu_vls label;
    point_t       p;         // 3D base of label text
    int           line_flag; // If 1, draw a line from label anchor to target
    point_t       target;
    int           anchor;    // Either closest candidate to target (AUTO), or fixed
    int           arrow;     // If 1, use an arrow indicating direction from label to target
};


/* Note - this container holds information both for data axes and for
 * the more elaborate visuals associated with the Archer style model axes.
 * The latter is a superset of the former, so there should be no need for
 * a separate data type. */
struct bview_axes {
    int       draw;
    point_t   axes_pos;             /* in model coordinates */
    fastf_t   axes_size;            /* in view coordinates for HUD drawing-mode axes */
    int       line_width;           /* in pixels */
    int       axes_color[3];

    /* The following are (currently) only used when drawing
     * the faceplace HUD axes */
    int       pos_only;
    int       label_flag;
    int       label_color[3];
    int       triple_color;
    int       tick_enabled;
    int       tick_length;          /* in pixels */
    int       tick_major_length;    /* in pixels */
    fastf_t   tick_interval;        /* in mm */
    int       ticks_per_major;
    int       tick_threshold;
    int       tick_color[3];
    int       tick_major_color[3];
};


// Many settings have defaults at the view level, and may be overridden for
// individual scene objects.
//
// TODO - once this settles down, it will probably warrant a bu_structparse
// for value setting
struct bview_settings {

    // scene_obj settings
    int s_line_width;		/**< @brief  current line width */
    fastf_t s_arrow_tip_length; /**< @brief  arrow tip length */
    fastf_t s_arrow_tip_width;  /**< @brief  arrow tip width */
    int s_hiddenLine;         	/**< @brief  1 - hidden line - TODO - this is really a drawing mode */

    fastf_t transparency;	/**< @brief  holds a transparency value in the range [0.0, 1.0] */
    int s_dmode;         	/**< @brief  draw mode: 0 - wireframe
				 *	      1 - shaded bots and polysolids only (booleans NOT evaluated)
				 *	      2 - shaded (booleans NOT evaluated)
				 *	      3 - shaded (booleans evaluated)
				 */

    // draw command opts in _ged_client_data
    int color_override;
    unsigned char color[3];	/**< @brief  color to draw as */

    int shaded_mode_override;

    int draw_wireframes;
    int draw_solid_lines_only;
    int draw_non_subtract_only;

    // Adaptive plotting related settings - these are used when the wireframe
    // generated by primitives is based on the view information.
    int           adaptive_plot;
    int           redraw_on_zoom;
    int           x_samples;
    int           y_samples;
    fastf_t       point_scale;
    fastf_t       curve_scale;
    size_t        bot_threshold;

};


/* Note that it is possible for a view object to be view-only (not
 * corresponding directly to the wireframe of a database shape) but also based
 * off of database data.  Evaluated shaded objects would be an example, as
 * would NIRT solid shotline visualizations or overlap visualizations.  The
 * categorizations for the various types of bview_scene_obj objects would be:
 *
 * solid wireframe/triangles (obj.s):  BVIEW_DBOBJ_BASED
 * rtcheck overlap visual:             BVIEW_DBOBJ_BASED & BVIEW_VIEWONLY
 * polygon/line/label:                 BVIEW_VIEWONLY
 *
 * The distinction between objects (lines, labels, etc.) defined as
 * bview_scene_obj VIEW ONLY objects and the faceplate elements is objects
 * defined as bview_scene_obj objects DO exist in the 3D scene, and will move
 * as 3D elements when the view is manipulated (although label text is drawn
 * parallel to the view plane.)  Faceplate elements exist ONLY in the HUD and
 * are not managed as bview_scene_obj objects - they will not move with view
 * manipulation.
 */
#define BVIEW_DBOBJ_BASED    0x01
#define BVIEW_VIEWONLY       0x02
#define BVIEW_LINES          0x04
#define BVIEW_LABELS         0x08
#define BVIEW_AXES           0x10
#define BVIEW_POLYGONS       0x20

struct bview;

// Separate these out, as we'll try not to use them in the new display work
struct bview_scene_obj_old_settings {
    char s_wflag;		/**< @brief  work flag - used by various libged and Tcl functions */
    char s_dflag;		/**< @brief  1 - s_basecolor is derived from the default */
    unsigned char s_basecolor[3];	/**< @brief  color from containing region */
    char s_uflag;		/**< @brief  1 - the user specified the color */
    char s_cflag;		/**< @brief  1 - use the default color */

   /* Database object related info */
    char s_Eflag;		/**< @brief  flag - not a solid but an "E'd" region (MGED ONLY)*/
    short s_regionid;		/**< @brief  region ID (MGED ONLY)*/
};

struct bview_scene_obj  {
    struct bu_list l;

    /* View object name and type id */
    unsigned long long s_type_flags;
    struct bu_vls s_name;       /**< @brief object name (may not be unique, used for activities like path lookup) */
    struct bu_vls s_uuid;       /**< @brief object name (unique, may be less immediately clear to user) */
    mat_t s_mat;		/**< @brief mat to use for internal lookup */

    /* Associated bview.  Note that scene objects are not assigned uniquely to
     * one view.  This value may be changed by the application in a multi-view
     * scenario as an object is edited from multiple different views, to supply
     * the necessary view context for editing. If the object needs to retain
     * knowledge of its original/creation view, it should save that info
     * internally in its s_i_data container. */
    struct bview *s_v;

    /* Knowledge of how to create/update s_vlist and the other 3D geometry data, as well as
     * manage any custom data specific to this object */
    void *s_i_data;  /**< @brief custom view data (bview_line_seg, bview_label, bview_polyon, etc) */
    int (*s_update_callback)(struct bview_scene_obj *);  /**< @brief custom update/generator for s_vlist */
    void (*s_free_callback)(struct bview_scene_obj *);  /**< @brief free any info stored in s_i_data */

    /* Actual 3D geometry data and information */
    struct bu_list s_vlist;	/**< @brief  Pointer to unclipped vector list */
    int s_vlen;			/**< @brief  Number of actual cmd[] entries in vlist */
    unsigned int s_dlist;	/**< @brief  display list index */
    fastf_t s_size;		/**< @brief  Distance across solid, in model space */
    fastf_t s_csize;		/**< @brief  Dist across clipped solid (model space) */
    vect_t s_center;		/**< @brief  Center point of solid, in model space */

    /* Display properties */
    char s_flag;		/**< @brief  UP = object visible, DOWN = obj invis */
    char s_iflag;	        /**< @brief  UP = illuminated, DOWN = regular */
    unsigned char s_color[3];	/**< @brief  color to draw as */
    int s_soldash;		/**< @brief  solid/dashed line flag: 0 = solid, 1 = dashed*/
    int s_arrow;		/**< @brief  arrow flag for view object drawing routines */
    int s_changed;		/**< @brief  changed flag - set by s_update_callback if a change occurred */

    /* Scene object settings which also (potentially) have global defaults but
     * may be overridden locally */
    struct bview_settings s_os;

    /* Settings that may be less necessary... */
    struct bview_scene_obj_old_settings s_old;

    /* Child objects of this object */
    struct bu_ptbl children;

    /* Container for reusing bview_scene_obj allocations */
    struct bview_scene_obj *free_scene_obj;

    /* User data to associate with this view object */
    void *s_u_data;
};

/* bview_scene_groups (one level above scene objects, conceptually equivalent
 * to display_list) are used to capture the intent of drawing commands.  For
 * example, in the scenario where a draw command is used to visualize a comb
 * with sub-combs a and b:
 *
 * ged> draw comb
 *
 * The drawing code will check the proposed group against existing groups,
 * adding and removing accordingly.  It will then walk the hierarchy and create
 * bview_scene_obj instances for all solids below comb/a and comb/b as children
 * of the scene group.  Note that since we specified "comb" as the drawn
 * object, if comb/b is removed from comb and comb/c is added, we would expect
 * comb's displayed view to be updated to reflect its current structure.  If,
 * however, we instead did the original visualization with the commands:
 *
 * ged> draw comb/a
 * ged> draw comb/b
 *
 * The same solids would be drawn, but conceptually the comb itself is not
 * drawn - the two instances are.  If comb/b is removed and comb/c added, we
 * would not expect comb/c to be drawn since we never drew either that instance
 * or its parent comb.
 *
 * However, if comb/a and comb/b are drawn and then comb is drawn, the new comb
 * scene group will replace both the comb/a and comb/b groups since they are now
 * part of a higher level object being drawn.  If comb is drawn and comb/a is
 * subsequently drawn, it will be a no-op since "comb" is already covering that
 * case.
 *
 * The rule with bview_scene_group instances is their children must specify a
 * fully realized entity - if the s_name is "/comb/a" then everything below
 * /comb/a is drawn.  If /comb/a/obj1.s is erased, new bview_scene_group
 * entities will be needed to reflect the partial nature of /comb/a in the
 * visualization.  That requirement also propagates back up the tree. If a has
 * obj1.s and obj2.s below it, and /comb/a/obj1.s is erased, an original
 * "/comb" scene group will be replaced by new scene groups: /comb/a/obj2.s and
 * /comb/b.  Note that if /comb/a/obj1.s is subsequently drawn in isolation,
 * the scene groups will not collapse back to a single comb group - the user
 * will not at that point have explicitly issued instructions to draw comb as a
 * whole, even though all the individual elements have been drawn.  A "view
 * simplify" command should probably be added to support collapsing to the
 * simplest available option automatically in that situation.
 *
 * Note that the above rule is for explicit erasure from the drawn scene group
 * - if the structure of /comb/a is changed the drawn object is still "comb"
 * and the solid children of the existing group are updated to reflect the
 * current state of comb, rather than introducing new scene groups.
 *
 * Much like point_t and vect_t, the distinction between a group and an
 * individual object is largely semantic rather than a question of different
 * data storage.  A group just uses the bview_scene_obj container to store
 * group-wide default settings, and g.children holds the individual
 * bview_scene_obj entries corresponding to the solids.  A bview_scene_obj
 * should always map to a solid - a group may specify a solid but more
 * typically will reference the root of a CSG tree and have solids below it.
 * We define them to have different types only to help keep straight in the
 * code what is a conceptually a group and what is an individual scene object.
 */
struct bview_scene_group {
    struct bview_scene_obj *g;
};

/* A display list corresponds (typically) to a database object.  It is composed of one
 * or more scene objects, which can be manipulated independently but collectively make
 * up the displayed representation of an object. */
struct display_list {
    struct bu_list      l;
    void               *dl_dp;                 /* Normally this will be a struct directory pointer */
    struct bu_vls       dl_path;
    struct bu_list      dl_head_scene_obj;          /**< @brief  head of scene obj list for this display list */
    int                 dl_wflag;
};

struct bview_adc_state {
    int         draw;
    int         dv_x;
    int         dv_y;
    int         dv_a1;
    int         dv_a2;
    int         dv_dist;
    fastf_t     pos_model[3];
    fastf_t     pos_view[3];
    fastf_t     pos_grid[3];
    fastf_t     a1;
    fastf_t     a2;
    fastf_t     dst;
    int         anchor_pos;
    int         anchor_a1;
    int         anchor_a2;
    int         anchor_dst;
    fastf_t     anchor_pt_a1[3];
    fastf_t     anchor_pt_a2[3];
    fastf_t     anchor_pt_dst[3];
    int         line_color[3];
    int         tick_color[3];
    int         line_width;
};

struct bview_data_axes_state {
    int       draw;
    int       color[3];
    int       line_width;          /* in pixels */
    fastf_t   size;                /* in view coordinates */
    int       num_points;
    point_t   *points;             /* in model coordinates */
};

struct bview_grid_state {
    int       rc;
    int       draw;               /* draw grid */
    int       snap;               /* snap to grid */
    fastf_t   anchor[3];
    fastf_t   res_h;              /* grid resolution in h */
    fastf_t   res_v;              /* grid resolution in v */
    int       res_major_h;        /* major grid resolution in h */
    int       res_major_v;        /* major grid resolution in v */
    int       color[3];
};

struct bview_interactive_rect_state {
    int        active;     /* 1 - actively drawing a rectangle */
    int        draw;       /* draw rubber band rectangle */
    int        line_width;
    int        line_style;  /* 0 - solid, 1 - dashed */
    int        pos[2];     /* Position in image coordinates */
    int        dim[2];     /* Rectangle dimension in image coordinates */
    fastf_t    x;          /* Corner of rectangle in normalized     */
    fastf_t    y;          /* ------ view coordinates (i.e. +-1.0). */
    fastf_t    width;      /* Width and height of rectangle in      */
    fastf_t    height;     /* ------ normalized view coordinates.   */
    int        bg[3];      /* Background color */
    int        color[3];   /* Rectangle color */
    int        cdim[2];    /* Canvas dimension in pixels */
    fastf_t    aspect;     /* Canvas aspect ratio */
};


struct bview_data_arrow_state {
    int       gdas_draw;
    int       gdas_color[3];
    int       gdas_line_width;          /* in pixels */
    int       gdas_tip_length;
    int       gdas_tip_width;
    int       gdas_num_points;
    point_t   *gdas_points;             /* in model coordinates */
};

struct bview_data_label_state {
    int         gdls_draw;
    int         gdls_color[3];
    int         gdls_num_labels;
    int         gdls_size;
    char        **gdls_labels;
    point_t     *gdls_points;
};

struct bview_data_line_state {
    int       gdls_draw;
    int       gdls_color[3];
    int       gdls_line_width;          /* in pixels */
    int       gdls_num_points;
    point_t   *gdls_points;             /* in model coordinates */
};

typedef struct {
    int                 gdps_draw;
    int                 gdps_moveAll;
    int                 gdps_color[3];
    int                 gdps_line_width;        /* in pixels */
    int                 gdps_line_style;
    int                 gdps_cflag;             /* contour flag */
    size_t              gdps_target_polygon_i;
    size_t              gdps_curr_polygon_i;
    size_t              gdps_curr_point_i;
    point_t             gdps_prev_point;
    bg_clip_t           gdps_clip_type;
    fastf_t             gdps_scale;
    point_t             gdps_origin;
    mat_t               gdps_rotation;
    mat_t               gdps_view2model;
    mat_t               gdps_model2view;
    struct bg_polygons  gdps_polygons;
    fastf_t             gdps_data_vZ;
} bview_data_polygon_state;

struct bview_other_state {
    int gos_draw;
    int gos_line_color[3];
    int gos_text_color[3];
};

struct bview_obj_internal;
struct bview_obj_cache {
    struct bview_obj_internal *i;
};

struct bview {
    uint32_t	  magic;             /**< @brief magic number */
    struct bu_vls gv_name;

    /* Size info */
    fastf_t       gv_i_scale;
    fastf_t       gv_a_scale;        /**< @brief absolute scale */
    fastf_t       gv_scale;
    fastf_t       gv_size;           /**< @brief  2.0 * scale */
    fastf_t       gv_isize;          /**< @brief  1.0 / size */
    int		  gv_width;
    int		  gv_height;
    fastf_t       gv_base2local;
    fastf_t       gv_rscale;
    fastf_t       gv_sscale;

    /* Camera info */
    fastf_t       gv_perspective;    /**< @brief  perspective angle */
    vect_t        gv_aet;
    vect_t        gv_eye_pos;        /**< @brief  eye position */
    vect_t        gv_keypoint;
    char          gv_coord;          /**< @brief  coordinate system */
    char          gv_rotate_about;   /**< @brief  indicates what point rotations are about */
    mat_t         gv_rotation;
    mat_t         gv_center;
    mat_t         gv_model2view;
    mat_t         gv_pmodel2view;
    mat_t         gv_view2model;
    mat_t         gv_pmat;           /**< @brief  perspective matrix */

    /* Keyboard/mouse info */
    fastf_t       gv_prevMouseX;
    fastf_t       gv_prevMouseY;
    int           gv_mouse_x;
    int           gv_mouse_y;
    char          gv_key;
    unsigned long gv_mod_flags;
    fastf_t       gv_minMouseDelta;
    fastf_t       gv_maxMouseDelta;

    /* Settings */
    struct bview_settings gvs;
    int           gv_polygon_mode;  // libtclcad polygon modes
    int           gv_snap_lines;
    double 	  gv_snap_tol_factor;
    int           gv_cleared;
    int           gv_zclip;
    fastf_t       gv_data_vZ;
    int           gv_autoview;
    int		  gv_hide;          // libtclcad setting for hiding view - unused?

    // Faceplate elements fall into two general categories: those which are
    // interactively adjusted (in a geometric sense) and those which are not.
    // The non-interactive are generally just enabled or disabled:
    struct bview_axes           gv_model_axes;
    struct bview_axes           gv_view_axes;
    struct bview_grid_state     gv_grid;
    struct bview_other_state    gv_center_dot;
    struct bview_other_state    gv_view_params;
    struct bview_other_state    gv_view_scale;
    int                         gv_fps; // Display Frames-Per-Second metric
    double                      gv_frametime;

    // Container for db object groups (may come from GED)
    struct bu_ptbl                      *gv_db_grps;
    // Container for storing bview_scene_obj elements unique to this
    // view (labels, polygons, etc.)
    struct bu_ptbl                      *gv_view_objs;

    // Available bn_vlist entities to recycle before allocating new.
    struct bu_list      gv_vlfree;     /**< @brief  head of bn_vlist freelist */
    struct bview_obj_cache gv_cache;

    // More complex are the view elements not corresponding to geometry objects
    // but editable by the user.  These are selectable, but because they are
    // not view objects which elements are part of the current selection set
    // must be handled differently.
    struct bu_ptbl                      *gv_selected;
    struct bview_adc_state              gv_adc;
    struct bview_data_arrow_state       gv_data_arrows;
    struct bview_data_axes_state        gv_data_axes;
    struct bview_data_label_state       gv_data_labels;
    struct bview_data_line_state        gv_data_lines;
    bview_data_polygon_state            gv_data_polygons;
    struct bview_data_arrow_state       gv_sdata_arrows;
    struct bview_data_axes_state        gv_sdata_axes;
    struct bview_data_label_state       gv_sdata_labels;
    struct bview_data_line_state        gv_sdata_lines;
    bview_data_polygon_state            gv_sdata_polygons;
    struct bview_other_state            gv_prim_labels;
    struct bview_interactive_rect_state gv_rect;

    /* Callback, external data */
    void          (*gv_callback)();  /**< @brief  called in ged_view_update with gvp and gv_clientData */
    void           *gv_clientData;   /**< @brief  passed to gv_callback */
    struct bu_ptbl *callbacks;
    void           *dmp;             /* Display manager pointer, if one is associated with this view */
    void           *u_data;          /* Caller data associated with this view */
};

#endif /* DM_BVIEW_H */

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
