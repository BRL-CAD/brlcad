/*                        B V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2024 United States Government as represented by
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
/** @addtogroup bv_defines
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
#ifndef BV_DEFINES_H
#define BV_DEFINES_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

/** @{ */
/** @file bv.h */

#ifndef BV_EXPORT
#  if defined(BV_DLL_EXPORTS) && defined(BV_DLL_IMPORTS)
#    error "Only BV_DLL_EXPORTS or BV_DLL_IMPORTS can be defined, not both."
#  elif defined(BV_DLL_EXPORTS)
#    define BV_EXPORT COMPILER_DLLEXPORT
#  elif defined(BV_DLL_IMPORTS)
#    define BV_EXPORT COMPILER_DLLIMPORT
#  else
#    define BV_EXPORT
#  endif
#endif

#include "bg/polygon_types.h"
#include "bv/tcl_data.h"
#include "bv/faceplate.h"

__BEGIN_DECLS

#define BV_MINVIEWSIZE 0.0001
#define BV_MINVIEWSCALE 0.00005

#ifndef UP
#  define UP 0
#endif
#ifndef DOWN
#  define DOWN 1
#endif

#define BV_ANCHOR_AUTO          0
#define BV_ANCHOR_BOTTOM_LEFT   1
#define BV_ANCHOR_BOTTOM_CENTER 2
#define BV_ANCHOR_BOTTOM_RIGHT  3
#define BV_ANCHOR_MIDDLE_LEFT   4
#define BV_ANCHOR_MIDDLE_CENTER 5
#define BV_ANCHOR_MIDDLE_RIGHT  6
#define BV_ANCHOR_TOP_LEFT      7
#define BV_ANCHOR_TOP_CENTER    8
#define BV_ANCHOR_TOP_RIGHT     9
struct bv_label {
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
struct bv_axes {
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
struct bv_obj_settings {

    int s_dmode;         	/**< @brief  draw mode: 0 - wireframe
				 *	      1 - shaded bots and polysolids only (booleans NOT evaluated)
				 *	      2 - shaded (booleans NOT evaluated)
				 *	      3 - shaded (booleans evaluated)
				 *	      4 - hidden line
				 */
    int mixed_modes;            /**< @brief  when drawing, don't remove an objects view objects for other modes */
    fastf_t transparency;	/**< @brief  holds a transparency value in the range [0.0, 1.0] - 1 is opaque */

    int color_override;
    unsigned char color[3];	/**< @brief  color to draw as */

    int s_line_width;		/**< @brief  current line width */
    fastf_t s_arrow_tip_length; /**< @brief  arrow tip length */
    fastf_t s_arrow_tip_width;  /**< @brief  arrow tip width */
    int draw_solid_lines_only;   /**< @brief do not use dashed lines for subtraction solids */
    int draw_non_subtract_only;  /**< @brief do not visualize subtraction solids */
};
#define BV_OBJ_SETTINGS_INIT {0, 0, 1.0, 0, {255, 0, 0}, 1, 0.0, 0.0, 0, 0}


/* Note that it is possible for a view object to be view-only (not
 * corresponding directly to the wireframe of a database shape) but also based
 * off of database data.  Evaluated shaded objects would be an example, as
 * would NIRT solid shotline visualizations or overlap visualizations.  The
 * categorizations for the various types of bv_scene_obj objects would be:
 *
 * solid wireframe/triangles (obj.s):  BV_DBOBJ_BASED
 * rtcheck overlap visual:             BV_DBOBJ_BASED & BV_VIEWONLY
 * polygon/line/label:                 BV_VIEWONLY
 *
 * The distinction between objects (lines, labels, etc.) defined as
 * bv_scene_obj VIEW ONLY objects and the faceplate elements is objects
 * defined as bv_scene_obj objects DO exist in the 3D scene, and will move
 * as 3D elements when the view is manipulated (although label text is drawn
 * parallel to the view plane.)  Faceplate elements exist ONLY in the HUD and
 * are not managed as bv_scene_obj objects - they will not move with view
 * manipulation.
 */
#define BV_DBOBJ_BASED    0x01
#define BV_VIEWONLY       0x02
#define BV_LINES          0x04
#define BV_LABELS         0x08
#define BV_AXES           0x10
#define BV_POLYGONS       0x20
#define BV_MESH_LOD       0x40
#define BV_CSG_LOD        0x80

struct bview;

#define BV_DB_OBJS 0x01
#define BV_VIEW_OBJS 0x02
#define BV_LOCAL_OBJS 0x04
#define BV_CHILD_OBJS 0x08

struct bv_scene_obj_internal;

struct bv_scene_obj  {
    struct bu_list l;

    /* Internal implementation storage */
    struct bv_scene_obj_internal *i;

    /* View object name and type id */
    unsigned long long s_type_flags;
    struct bu_vls s_name;       /**< @brief object name (should be unique if view objects are to be addressed by name) */
    void *s_path;       	/**< @brief alternative (app specific) encoding of s_name */
    void *dp;       		/**< @brief app obj data */
    mat_t s_mat;		/**< @brief mat to use for internal lookup and mesh LoD drawing */

    /* Associated bv.  Note that scene objects are not assigned uniquely to
     * one view.  This value may be changed by the application in a multi-view
     * scenario as an object is edited from multiple different views, to supply
     * the necessary view context for editing. If the object needs to retain
     * knowledge of its original/creation view, it should save that info
     * internally in its s_i_data container. */
    struct bview *s_v;

    /* Knowledge of how to create/update s_vlist and the other 3D geometry data, as well as
     * manage any custom data specific to this object */
    void *s_i_data;  /**< @brief custom view data (bv_line_seg, bv_label, bv_polyon, etc) */
    int (*s_update_callback)(struct bv_scene_obj *, struct bview *, int);  /**< @brief custom update/generator for s_vlist */
    void (*s_free_callback)(struct bv_scene_obj *);  /**< @brief free any info stored in s_i_data, s_path and draw_data */

    /* 3D vector list geometry data */
    struct bu_list s_vlist;	/**< @brief  Pointer to unclipped vector list */
    size_t s_vlen;			/**< @brief  Number of actual cmd[] entries in vlist */

    /* Display lists accelerate drawing when we can use them */
    unsigned int s_dlist;	/**< @brief  display list index */
    int s_dlist_mode;		/**< @brief  drawing mode in which display list was generated (if it doesn't match s_os.s_dmode, dlist is out of date.) */
    int s_dlist_stale;		/**< @brief  set by client codes when dlist is out of date - dm must update. */
    void (*s_dlist_free_callback)(struct bv_scene_obj *);  /**< @brief free any dlist specific data */

    /* 3D geometry metadata */
    fastf_t s_size;		/**< @brief  Distance across solid, in model space */
    fastf_t s_csize;		/**< @brief  Dist across clipped solid (model space) */
    vect_t s_center;		/**< @brief  Center point of solid, in model space */
    int s_displayobj;		/**< @brief  Vector list contains vertices in display context flag */
    point_t bmin;
    point_t bmax;
    int have_bbox;

    /* Display properties */
    char s_flag;		/**< @brief  UP = object visible, DOWN = obj invis */
    char s_iflag;	        /**< @brief  UP = illuminated, DOWN = regular */
    unsigned char s_color[3];	/**< @brief  color to draw as */
    int s_soldash;		/**< @brief  solid/dashed line flag: 0 = solid, 1 = dashed*/
    int s_arrow;		/**< @brief  arrow flag for view object drawing routines */
    int s_changed;		/**< @brief  changed flag - set by s_update_callback if a change occurred */
    int current;

    /* Adaptive plotting info.
     *
     * The adaptive wireframe flag is set if the wireframe was created while
     * adaptive mode is on - this is to allow reversion to non-adaptive
     * wireframes if the mode is switched off without the view scale changing.
     *
     * NOTE: We store the following NOT for controlling the drawing, but so we
     * can determine if the vlist is current with respect to the parent view
     * settings.  These values SHOULD NOT be directly manipulated by any user
     * facing commands (such as view obj). */
    int     adaptive_wireframe;
    int     csg_obj;
    int     mesh_obj;
    fastf_t view_scale;
    size_t  bot_threshold;
    fastf_t curve_scale;
    fastf_t point_scale;

    /* Scene object settings which also (potentially) have global defaults but
     * may be overridden locally */
    struct bv_obj_settings *s_os;
    struct bv_obj_settings s_local_os;

    /* Settings that may be less necessary... */
    struct bv_scene_obj_old_settings s_old;

    /* Child objects of this object */
    struct bu_ptbl children;

    /* Parent object of this object */
    struct bv_scene_ob *parent;

    /* Object level pointers to parent containers.  These are stored so
     * that the object itself knows everything needed for data manipulation
     * and it is unnecessary to explicitly pass other parameters. */

    /* Reusable vlists */
    struct bu_list *vlfree;

    /* Container for reusing bv_scene_obj allocations */
    struct bv_scene_obj *free_scene_obj;

    /* View container containing this object */
    struct bu_ptbl *otbl;

    /* For more specialized routines not using vlists, we may need
     * additional drawing data associated with a scene object */
    void *draw_data;

    /* User data to associate with this view object */
    void *s_u_data;
};



/* bv_scene_groups (one level above scene objects, conceptually equivalent
 * to display_list) are used to capture the intent of drawing commands.  For
 * example, in the scenario where a draw command is used to visualize a comb
 * with sub-combs a and b:
 *
 * ged> draw comb
 *
 * The drawing code will check the proposed group against existing groups,
 * adding and removing accordingly.  It will then walk the hierarchy and create
 * bv_scene_obj instances for all solids below comb/a and comb/b as children
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
 * The rule with bv_scene_group instances is their children must specify a
 * fully realized entity - if the s_name is "/comb/a" then everything below
 * /comb/a is drawn.  If /comb/a/obj1.s is erased, new bv_scene_group
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
 * data storage.  A group just uses the bv_scene_obj container to store
 * group-wide default settings, and g.children holds the individual
 * bv_scene_obj entries corresponding to the solids.  A bv_scene_obj
 * should always map to a solid - a group may specify a solid but more
 * typically will reference the root of a CSG tree and have solids below it.
 * We define them to have different types only to help keep straight in the
 * code what is a conceptually a group and what is an individual scene object.
 */
#define bv_scene_group bv_scene_obj


/* The primary "working" data for mesh Level-of-Detail (LoD) drawing is stored
 * in a bv_mesh_lod container.
 *
 * Most LoD information is deliberately hidden in the internal, but the key
 * data needed for drawing routines and view setup are exposed. Although this
 * data structure is primarily managed in libbg, the public data in this struct
 * is needed at many levels of the software stack, including libbv. */
struct bv_mesh_lod {

    // The set of triangle faces to be used when drawing
    int fcnt;
    const int *faces;

    // The vertices used by the faces array
    int pcnt;
    const point_t *points;      // If using snapped points, that's this array.  Else, points == points_orig.
    int porig_cnt;
    const point_t *points_orig;

    // Optional: per-face-vertex normals (one normal per triangle vertex - NOT
    // one normal per vertex.  I.e., a given point from points_orig may have
    // multiple normals associated with it in different faces.)
    const vect_t *normals;

    // Bounding box of the original full-detail data
    point_t bmin;
    point_t bmax;

    // The scene object using this LoD structure
    struct bv_scene_obj *s;

    // Pointer to the higher level LoD context associated with this LoD data
    void *c;

    // Pointer to internal LoD implementation information specific to this object
    void *i;
};

/* Flags to identify categories of objects to snap */
#define BV_SNAP_SHARED 0x1
#define BV_SNAP_LOCAL  0x2
#define BV_SNAP_DB  0x4
#define BV_SNAP_VIEW    0x8
#define BV_SNAP_TCL    0x10

/* We encapsulate non-camera settings into a container mainly to allow for
 * easier re-use of the same settings between different views - if a common
 * setting set is maintained between different views, this container allows
 * us to just point to the common set from all views using it. */
struct bview_settings {
    struct bv_obj_settings obj_s;
    int            gv_snap_lines;
    double 	   gv_snap_tol_factor;
    struct bu_ptbl gv_snap_objs;
    int		   gv_snap_flags;
    int            gv_cleared;
    int            gv_zclip;
    int            gv_autoview;

    // Adaptive plotting related settings - these are used when the wireframe
    // generated by primitives is based on the view information.
    int           adaptive_plot_mesh;
    int           adaptive_plot_csg;
    size_t        bot_threshold;
    fastf_t       curve_scale;
    fastf_t       point_scale;
    int           redraw_on_zoom;
    fastf_t 	  lod_scale;

    // Faceplate elements fall into two general categories: those which are
    // interactively adjusted (in a geometric sense) and those which are not.
    // The non-interactive are generally just enabled or disabled:
    struct bv_axes           gv_model_axes;
    struct bv_axes           gv_view_axes;
    struct bv_grid_state     gv_grid;
    struct bv_other_state    gv_center_dot;
    struct bv_params_state   gv_view_params;
    struct bv_other_state    gv_view_scale;
    double                   gv_frametime;

    // Framebuffer visualization is possible if there is an attached dm and
    // that dm has an associated framebuffer.  If those conditions are met,
    // this variable is used to control how the fb is visualized.
    int                      gv_fb_mode; // 0 = off, 1 = overlay, 2 = underlay

    // More complex are the faceplate view elements not corresponding to
    // geometry objects but editable by the user.  These aren't managed as
    // gv_view_objs (they are HUD visuals and thus not part of the scene) so
    // they have some unique requirements.
    struct bv_adc_state              gv_adc;
    struct bv_interactive_rect_state gv_rect;


    // Not yet implemented - mechanism for defining a set of selected view
    // objects
    struct bu_ptbl                      *gv_selected;
};

/* A view needs to know what objects are active within it, but this is a
 * function not just of adding and removing objects via commands like
 * "draw" and "erase" but also what settings are active.  Shared objects
 * are common to multiple views, but if adaptive plotting is enabled the
 * scene objects cannot also be common - the representations of the objects
 * may be different in each view, even though the object list is shared.
 */
struct bview_objs {

    // Container for db object groups unique to this view (typical use case is
    // adaptive plotting, where geometry wireframes may differ from view to
    // view and thus need unique vlists.)
    struct bu_ptbl  *db_objs;
    // Container for storing bv_scene_obj elements unique to this view.
    struct bu_ptbl  *view_objs;

    // Available bv_vlist entities to recycle before allocating new for local
    // view objects. This is used only if the app doesn't supply a vlfree -
    // normally the app should do so, so memory from one view can be reused for
    // other views.
    struct bu_list  gv_vlfree;

    /* Container for reusing bv_scene_obj allocations */
    struct bv_scene_obj *free_scene_obj;
};

struct bview_set;

struct bview {
    uint32_t	  magic;             /**< @brief magic number */
    struct bu_vls gv_name;

    /* Size info */
    fastf_t       gv_i_scale;
    fastf_t       gv_a_scale;        /**< @brief absolute scale */
    fastf_t       gv_scale;
    fastf_t       gv_size;           /**< @brief  2.0 * scale */
    fastf_t       gv_isize;          /**< @brief  1.0 / size */
    fastf_t       gv_base2local;
    fastf_t       gv_local2base;
    fastf_t       gv_rscale;
    fastf_t       gv_sscale;

    /* Information about current "window" into view.  This view may not be
     * displayed (that's up to the display managers) and it is up to the
     * calling code to set gv_width and gv_height to the current correct values
     * for such a display, if it is associated with this view.  These
     * definitions are needed in bview to support "view aware" algorithms that
     * require information defining an active pixel "window" into the view. */
    int		  gv_width;
    int		  gv_height;
    point2d_t	  gv_wmin; // view space bbox minimum of gv_width/gv_height window
    point2d_t	  gv_wmax; // view space bbox maximum of gv_width/gv_height window

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
    point_t       gv_prev_point;
    point_t       gv_point;
    char          gv_key;
    unsigned long gv_mod_flags;
    fastf_t       gv_minMouseDelta;
    fastf_t       gv_maxMouseDelta;

    /* Settings */
    struct bview_settings *gv_s;     /**< @brief shared settings supplied by user */
    struct bview_settings gv_ls;     /**< @brief locally maintained settings specific to view (used if gv_s is null) */

    /* If a view is marked as independent, its local containers are used even
     * if pointers to shared tables are set. This allows for fully independent
     * views with the same GED instance, at the cost of increased memory usage
     * if multiple views draw the same objects. */
    int independent;

    /* Set containing this view.  Also holds pointers to resources shared
     * across multiple views */
    struct bview_set *vset;

    /* Scene objects active in a view.  Managing these is a relatively complex
     * topic and depends on whether a view is shared, independent or adaptive.
     * Shared objects are common across views to make more efficient use of
     * system memory. */
    struct bview_objs gv_objs;

    /* We sometimes need to define the volume in space that is "active" for the
     * view.  For an orthogonal camera this is the oriented bounding box
     * extruded to contain active scene objects visible in the view  The app
     * must set the gv_bounds_update callback to bg_view_bound so a bv_update
     * call can update these values.*/
    point_t obb_center;
    vect_t obb_extent1;
    vect_t obb_extent2;
    vect_t obb_extent3;
    void (*gv_bounds_update)(struct bview *);

    /* "Backed out" point, lookat direction, scene radius. Used for geometric
     * view based interrogation. */
    point_t gv_vc_backout;
    vect_t gv_lookat;
    double radius;

    // libtclcad data
    struct bv_data_tclcad gv_tcl;

    /* Callback, external data */
    void          (*gv_callback)(struct bview *, void *);  /**< @brief  called in ged_view_update with gvp and gv_clientData */
    void           *gv_clientData;   /**< @brief  passed to gv_callback */
    struct bu_ptbl *callbacks;
    void           *dmp;             /* Display manager pointer, if one is associated with this view */
    void           *u_data;          /* Caller data associated with this view */
};

// Because bview instances frequently share objects in applications, they are
// not always fully independent - we define a container and some basic
// operations to manage this.
struct bview_set_internal;
struct bview_set {
    struct bview_set_internal   *i;
    struct bview_settings       settings;
};

__END_DECLS

#endif /* BV_DEFINES_H */

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
