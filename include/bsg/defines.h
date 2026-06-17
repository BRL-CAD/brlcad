/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * Canonical scene-graph and view type definitions.
 *
 * This header is the authoritative home for all scene-graph and view
 * type definitions previously in bv/defines.h.  bv/defines.h is now a
 * backward-compatibility bridge that will be removed once all callers
 * have migrated to bsg/defines.h.
 */
/** @{ */
/* @file bsg/defines.h */

#ifndef BSG_DEFINES_H
#define BSG_DEFINES_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bg/polygon_types.h"
#include "bsg/tcl_data.h"
#include "bsg/faceplate.h"

__BEGIN_DECLS

#ifndef BSG_EXPORT
#  if defined(BSG_DLL_EXPORTS) && defined(BSG_DLL_IMPORTS)
#    error "Only BSG_DLL_EXPORTS or BSG_DLL_IMPORTS can be defined, not both."
#  elif defined(BSG_DLL_EXPORTS)
#    define BSG_EXPORT COMPILER_DLLEXPORT
#  elif defined(BSG_DLL_IMPORTS)
#    define BSG_EXPORT COMPILER_DLLIMPORT
#  else
#    define BSG_EXPORT
#  endif
#endif

/* Define view ranges.  The numbers -2048 and 2047 go all the way back to the
 * original angle-distance cursor code that predates even BRL-CAD itself, but
 * (at least right now) there doesn't seem to be any documentation of why those
 * specific values were chosen. */
#define BSG_VIEW_MAX 2047.0
#define BSG_VIEW_MIN -2048.0
#define BSG_VIEW_RANGE 4095.0
/* Map +/-2048 BV space into -1.0..+1.0 :: x/2048*/
#define BSG_INV_VIEW 0.00048828125
#define INV_BV BSG_INV_VIEW
#define BSG_INV_4096 0.000244140625
#define INV_4096 0.000244140625


#define BSG_MINVIEWSIZE 0.0001
#define BSG_MINVIEWSCALE 0.00005

#ifndef UP
#  define UP 0
#endif
#ifndef DOWN
#  define DOWN 1
#endif

#define BSG_ANCHOR_AUTO          0
#define BSG_ANCHOR_BOTTOM_LEFT   1
#define BSG_ANCHOR_BOTTOM_CENTER 2
#define BSG_ANCHOR_BOTTOM_RIGHT  3
#define BSG_ANCHOR_MIDDLE_LEFT   4
#define BSG_ANCHOR_MIDDLE_CENTER 5
#define BSG_ANCHOR_MIDDLE_RIGHT  6
#define BSG_ANCHOR_TOP_LEFT      7
#define BSG_ANCHOR_TOP_CENTER    8
#define BSG_ANCHOR_TOP_RIGHT     9
struct bsg_label {
    int           size;
    struct bu_vls label;
    point_t       p;         // 3D base of label text
    int           line_flag; // If 1, draw a line from label anchor to target
    point_t       target;
    int           anchor;    // Either closest candidate to target (AUTO), or fixed
    int           arrow;     // If 1, use an arrow indicating direction from label to target
};


/* Note - this container holds information both for data axes and for the more
 * elaborate visuals associated with the Archer style model axes.  The latter
 * is a superset of the former, so there should be no need for a separate data
 * type. */
struct bsg_axes {
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

/* Many settings have application level defaults that can be overridden for
 * individual shape nodes.  Render traversal resolves these fields together
 * with source material, command overrides, and selection/edit state into a
 * BSG appearance action. */
struct bsg_appearance_settings {

    int draw_mode;         	/**< @brief  draw mode (legacy integer encoding; prefer the BSG_DRAW_MODE_* names):
				 *            0 - wireframe
				 *	      1 - shaded bots and polysolids only (booleans NOT evaluated)
				 *	      2 - shaded (booleans NOT evaluated)
				 *	      3 - wireframe with evaluated booleans
				 *	      4 - hidden line
				 *	      5 - sampled evaluated triangle points
				 */
    int mixed_modes;            /**< @brief  preserve other draw modes for the same source path */
    fastf_t transparency;	/**< @brief  holds a transparency value in the range [0.0, 1.0] - 1 is opaque */

    int color_override;
    unsigned char color[3];	/**< @brief  color to draw as */

    int s_line_width;		/**< @brief  current line width */
    fastf_t s_arrow_tip_length; /**< @brief  arrow tip length */
    fastf_t s_arrow_tip_width;  /**< @brief  arrow tip width */
    int draw_solid_lines_only;   /**< @brief do not use dashed lines for subtraction solids */
    int draw_non_subtract_only;  /**< @brief do not visualize subtraction solids */
    int strict_fallback;         /**< @brief fail shaded/hidden-line realization instead of falling back to wireframe */
};
#define BSG_APPEARANCE_SETTINGS_INIT {0, 0, 1.0, 0, {255, 0, 0}, 1, 0.0, 0.0, 0, 0, 0}


typedef enum bsg_geometry_role {
    BSG_GEOMETRY_DB_OBJECT      = 0x01, /**< @brief geometry realized from a database path */
    BSG_GEOMETRY_VIEW_OVERLAY   = 0x02, /**< @brief model-space overlay geometry */
    BSG_GEOMETRY_LINE_SET       = 0x04, /**< @brief line-segment set geometry */
    BSG_GEOMETRY_TEXT_LABELS    = 0x08, /**< @brief text-label geometry */
    BSG_GEOMETRY_AXES_WIDGET    = 0x10, /**< @brief axes-widget geometry */
    BSG_GEOMETRY_POLYGON_REGION = 0x20  /**< @brief polygon-region geometry */
} bsg_geometry_role;

struct bsg_view;

typedef enum bsg_source_scope {
    BSG_SOURCE_DB    = 0x01, /**< @brief database-backed source */
    BSG_SOURCE_VIEW  = 0x02, /**< @brief view-scoped non-database source */
    BSG_SOURCE_LOCAL = 0x04, /**< @brief source owned by one view/session */
    BSG_SOURCE_CHILD = 0x08  /**< @brief internal child/group source */
} bsg_source_scope;

struct bsg_draw_intent;  /* draw-intent metadata — see bsg/draw_intent.h */

/* Backend type tags used by display-manager-owned resource caches. */
#define BSG_BACKEND_NONE 0u   /* no backend state attached */
#define BSG_BACKEND_GL   1u   /* OpenGL/GL-via-software-rasterizer (dm-gl, dm-swrast, dm-qtgl, dm-glx, dm-wgl) */

/* The primary "working" data for mesh Level-of-Detail (LoD) drawing is stored
 * in a bsg_mesh_lod container.
 *
 * Most LoD information is deliberately hidden in the internal, but the key
 * data needed for drawing routines and view setup are exposed. Although this
 * data structure is primarily managed in libbg, the public data in this struct
 * is needed at many levels of the software stack, including libbv. */
struct bsg_mesh_lod {

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

    // Opaque scene object handle using this LoD structure
    void *s;

    // Pointer to the higher level LoD context associated with this LoD data
    void *c;

    // Pointer to internal LoD implementation information specific to this object
    void *i;
};

/* Flags to identify categories of objects to snap */
#define BSG_SNAP_SHARED 0x1
#define BSG_SNAP_LOCAL  0x2
#define BSG_SNAP_DB     0x4
#define BSG_SNAP_VIEW   0x8
#define BSG_SNAP_TCL    0x10

struct bsg_view_settings;

/* A view needs to know what objects are active within it, but this is a
 * function not just of adding and removing objects via commands like
 * "draw" and "erase" but also what settings are active.  Shared objects
 * are common to multiple views, but if adaptive plotting is enabled the
 * scene objects cannot also be common - the representations of the objects
 * may be different in each view, even though the object list is shared.
 */
struct bsg_view_obj_pool {
    void *i; /**< @brief opaque private view scene-store handle */
};

// Data for managing "knob" manipulation of views.  One historical hardware
// example of this "knob" concept of view manipulation would be Dial boxes such
// as the Silicon Graphics SN-921, used with 3D workstations in the early days.
// Although we've not heard of Dial boxes being used with BRL-CAD in many
// years, the mathematics of view manipulation used to support them still
// underpins interactions driven with inputs from modern peripherals such as
// the mouse.
struct bsg_view_knobs {

    /* Rate data */
    vect_t      rot_m;      // rotation - model coords
    int         rot_m_flag;
    char        origin_m;
    void	*rot_m_udata;

    vect_t	rot_o;      // rotation - object coords
    int		rot_o_flag;
    char	origin_o;
    void	*rot_o_udata;

    vect_t      rot_v;      // rotation - view coords
    int         rot_v_flag;
    char        origin_v;
    void	*rot_v_udata;

    fastf_t     sca;        // scale
    int         sca_flag;
    void	*sca_udata;

    vect_t      tra_m;      // translation - model coords
    int         tra_m_flag;
    void	*tra_m_udata;

    vect_t      tra_v;      // translation - view coords
    int         tra_v_flag;
    void	*tra_v_udata;

    /* Absolute data */
    vect_t      rot_m_abs;       // rotation - model coords
    vect_t      rot_m_abs_last;

    vect_t      rot_o_abs;       // rotation - object coords
    vect_t      rot_o_abs_last;

    vect_t      rot_v_abs;       // rotation - view coords
    vect_t      rot_v_abs_last;

    fastf_t     sca_abs;

    vect_t      tra_m_abs;       // translation - model coords
    vect_t      tra_m_abs_last;

    vect_t      tra_v_abs;       // translation - view coords
    vect_t      tra_v_abs_last;

};

struct bsg_view_set;
struct bsg_camera;       /* forward decl for camera-node binding */

struct bsg_view {
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
     * definitions are needed in bsg_view to support "view aware" algorithms that
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
    struct bsg_view_state *gv_state;    /**< @brief canonical command/UI view state record */
    struct bsg_view_settings *settings_active;
    struct bsg_view_settings *settings_local;

    /* Set containing this view.  Also holds pointers to resources shared
     * across multiple views */
    struct bsg_view_set *vset;

    /* Scene objects active in a view.  Managing these is a relatively complex
     * topic and depends on whether a view is shared, independent or adaptive.
     * Shared objects are common across views to make more efficient use of
     * system memory. */
    struct bsg_view_obj_pool gv_objs;

    /* We sometimes need to define the volume in space that is "active" for the
     * view.  For an orthogonal camera this is the oriented bounding box
     * extruded to contain active scene objects visible in the view  The app
     * must set the gv_bounds_update callback to bg_view_bound so a bsg_update
     * call can update these values.*/
    point_t obb_center;
    vect_t obb_extent1;
    vect_t obb_extent2;
    vect_t obb_extent3;
    void (*gv_bounds_update)(struct bsg_view *);

    /* "Backed out" point, lookat direction, scene radius. Used for geometric
     * view based interrogation. */
    point_t gv_vc_backout;
    vect_t gv_lookat;
    double radius;

    /* Knob-based view manipulation data */
    struct bsg_view_knobs k;

    /* Virtual trackball position */
    point_t     orig_pos;

    // libtclcad data (optional: NULL for non-Tcl views, allocated and owned by
    // libtclcad tclcad_view_data when a Tcl-backed view is created)
    struct bsg_data_tclcad *gv_tcl;

    /* Callback, external data */
    void          (*gv_callback)(struct bsg_view *, void *);  /**< @brief  called in ged_view_update with gvp and gv_clientData */
    void           *gv_clientData;   /**< @brief  passed to gv_callback */
    struct bu_ptbl *callbacks;
    void           *dmp;             /* Display manager pointer, if one is associated with this view */
    void           *u_data;          /* Caller data associated with this view */

    /* Opaque scene attachment for this view.  The implementation owns the
     * retained draw scene and the view only carries a handle to it.  The
     * handle is intentionally opaque so application code cannot depend on
     * implementation node identity. */
    void           *gv_scene;

    /* per-frame edit-mode matrix
     * override.  When non-NULL, draw_scene_obj() renders edit-highlighted
     * objects with this matrix instead of gv_model2view.  MGED sets this to
     * view_state->vs_model2objview while the frame is being painted and
     * clears it to NULL immediately afterward.  Never heap-allocated; always
     * points into caller-owned storage. */
    matp_t          gv_edit_mat;

    /* Per-frame generation counter.  Bumped at the top of dm_draw_objs() so that
     * shapes painted in the current frame can be identified by
     * bsg_appearance_drawn_rev(shape) == gv_frame_rev without any per-frame full-tree
     * retained-state reset sweep.  Starts at 0; first dm_draw_objs()
     * call observes 1.  Wraps cleanly on uint64_t overflow (would take
     * billions of years at 60 fps, but the comparison still produces
     * the right answer for any pair of consecutive frames). */
    uint64_t        gv_frame_rev;

    /* per-view HUD scene-graph root.
     * Stored as void * to avoid a circular include dependency.  Cast to
     * NULL until the HUD root is created.  Contains one child per faceplate
     * feature; bsg_hud_sync() updates each child's visibility to reflect the
     * current bsg_view_settings before each render pass. */
    void           *gv_hud_root;

    /* bound camera snapshot for this
     * view.  When non-NULL, bsg_view_apply_camera_node() restores the
     * camera state from this snapshot.  Stored as a borrowed pointer
     * (the view does NOT own the struct bsg_camera); callers must keep
     * the camera alive for the lifetime of the binding.  NULL by default. */
    struct bsg_camera *gv_camera_node;

};

// Because bsg_view instances frequently share objects in applications, they are
// not always fully independent - we define a container and some basic
// operations to manage this.
struct bsg_view_set_internal;
struct bsg_view_set {
    struct bsg_view_set_internal   *i;
    struct bsg_view_settings       *settings;
};

/* Export macro */
#ifndef BSG_EXPORT
#  if defined(BSG_DLL_EXPORTS) && defined(BSG_DLL_IMPORTS)
#    error "Only BSG_DLL_EXPORTS or BSG_DLL_IMPORTS can be defined, not both."
#  elif defined(BSG_DLL_EXPORTS)
#    define BSG_EXPORT COMPILER_DLLEXPORT
#  elif defined(BSG_DLL_IMPORTS)
#    define BSG_EXPORT COMPILER_DLLIMPORT
#  else
#    define BSG_EXPORT
#  endif
#endif

/* BSG view magic.  Numeric value matches the historical bv view magic so
 * existing binary checks continue to work without requiring historical
 * namespace names in BSG code. */
#define BSG_VIEW_MAGIC 0x62766965

__END_DECLS

#endif /* BSG_DEFINES_H */

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
