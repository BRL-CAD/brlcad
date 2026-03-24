/*                         E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @file edit.h
 *
 * NOTE!!! This API is a work in progress as we migrate and consolidate
 * editing code from MGED, libged and other codes to a unified common
 * implementation in LIBRT.  Until this notice is removed, there are NO
 * guarantees of API stability with this code!
 *
 * Design notes:
 *
 * Aside from the X/Y/Z-only scale operations, the "generic" operations for
 * both sed and oed seem to differ primarily in that the former update the
 * wireframe and primitive parameters immediately, and the latter manipulate
 * only the matrix until the final step.  In the oed case, the altered
 * primitive wireframes are handled by using the working matrix to distort the
 * existing draw solids.
 *
 * It feels like there should be some sort of consolidation possible here - in
 * the case where an operation (sed or oed) doesn't need a new wireframe,
 * reusing an existing one is a sensible approach.  For operations on large
 * combs, which may involve thousands of large wireframes, reuse is key.
 * Rather than have two "modes", what we should do instead is have the edit
 * operations themselves determine if a new wireframe is needed.  If not, then
 * no matter what the op is we should try to reuse an existing wireframe (if
 * one is available from the app) and only generate one if the app does not
 * have the one we need available.  That may allow us to completely eliminate
 * the distinction between sed and oed, and treat comb edits like any other
 * primitive edits by using functab methods (semi-related is the attempt to
 * make a comb tess method in d1dc6a4fae8).
 *
 * Such changes would involve a significant update to the MGED drawing logic,
 * which uses a very simple UP/DOWN flag trick to turn existing solid
 * wireframes into the edit wireframes (and comes with some significant
 * limitations as well).  What we really need is a way for a scene obj to
 * reference another scene obj and override specific values, so we can point to
 * an existing scene obj and use it for edit drawing without having to do
 * anything to the original obj except flagging it as involved (so the main
 * update pass knows to skip drawing it.) Commit de2c0da2d4a has a bit of what
 * would be needed for that to work, but there's a lot more to think about both
 * in the original and new draw cycles.  To properly have the edit drawing
 * independent of other uses of geometry in a scene while still enabling vlist
 * reuse (which the new drawing path isn't really doing properly) we may need
 * to have all comb instances define themselves in a scene as an object that
 * references another scene object which isn't drawn but holds the data
 * defining the vlist.  Then the instance obj would just hold the matrix and
 * any override info for color, etc.  Need to check how MGED is handling comb
 * instances now, the new drawing layer I think is just creating a new
 * bv_scene_obj for each instance with its own vlist...
 */

#ifndef RT_EDIT_H
#define RT_EDIT_H

#include "common.h"
#include <float.h>
#include "vmath.h"
#include "bn/mat.h"
#include "bu/parse.h"
#include "bv/defines.h"
#include "rt/defines.h"
#include "rt/db_internal.h"
#include "rt/resource.h"

__BEGIN_DECLS

// Settings used for both solid and matrix edits
#define RT_EDIT_DEFAULT   -1
#define RT_EDIT_IDLE       0

/**
 * Maximum number of parameters that can be supplied via e_para in a single
 * rt_edit operation.  This must be large enough to hold all parameters for
 * the most complex single command (currently ECMD_SKETCH_APPEND_BEZIER, which
 * needs one slot per control-point index; degree 15 → 16 indices).
 */
/* Maximum number of keyboard-input parameters for a single edit command.
 * SET_MATRIX requires 1 index + 16 matrix elements = 17 values; use 20
 * to give headroom for future operations. */
#define RT_EDIT_MAXPARA   20

/* Maximum number of simultaneous string parameters (companion to e_para for
 * RT_EDIT_PARAM_STRING commands) and the maximum byte length of each. */
#define RT_EDIT_MAXSTR      5
#define RT_EDIT_MAXSTR_LEN  512


// Solid editing (done via sed in MGED) alters primitive parameters to produce
// new shapes.  Parameters are updated immediately, allowing for new wireframe
// generation from primitive plot commands.  Not supported for combs.
//
// Translate, rotate and scale can typically be applied to any geometry object,
// without needing awareness of the details of the object's definition.  The
// primitives themselves define more specific per-data editing flags.
#define RT_PARAMS_EDIT_TRANS      1
#define RT_PARAMS_EDIT_SCALE      2 // Scale whole solid by scalar
#define RT_PARAMS_EDIT_ROT        3

// This isn't an edit operation as such in that it doesn't (and won't) change
// the geometry itself - rather it is used in edit_mode to indicate a
// particular selection state.
//
// Eventually it might very well end up being used for an edit flag - it is
// quite reasonable to think that ft_edit_xy might take this flag and a mouse
// argument to select the geometrically closest editing feature from the solid
// and set the appropriate edit flag and/or per-prim data structure info - but
// right now we don't have anything like that.
#define RT_PARAMS_EDIT_PICK       5

// Matrix editing (done via oed in MGED) alters a matrix rather than solid
// parameters in its intermediate stages (at the end when changes are written
// to disk, and if a solid is being edited rather than a comb the matrix
// changes are translated by the per-primitive routines to new solid parameters
// at that time.)
//
// Matrix rotation: absolute Euler angles (X,Y,Z in degrees) supplied via
// e_para[0..2].  The existing rotation stored in model_changes is replaced
// by the new absolute rotation while the keypoint's world position and the
// accumulated scale factor are preserved.  This mirrors MGED's "orot X Y Z"
// (f_rot_obj / mged_rot_obj) behaviour.  Incremental rotation via knob or
// mouse input uses rt_knob_edit_rot() with matrix_edit=1 instead.
#define RT_MATRIX_EDIT_ROT       6

// Matrix translate operations specified relative to VIEW XY are view-dependent.
// They project the object keypoint into view space, replace the requested
// XY component(s) with the supplied mouse/parameter value, and project back
// to model space.  This matches MGED's oed mouse-drag (RARROW/UARROW) behaviour
// and is the correct mapping for interactive viewport dragging.
//
// For non-interactive (command-line) absolute placement in model coordinates,
// use RT_MATRIX_EDIT_TRANS_MODEL_XYZ which places the keypoint at the given
// model-space X,Y,Z position directly.  This mirrors MGED's "translate X Y Z"
// (f_tr_obj) behaviour when in object-edit mode.
#define RT_MATRIX_EDIT_TRANS_VIEW_XY  7
#define RT_MATRIX_EDIT_TRANS_VIEW_X   8
#define RT_MATRIX_EDIT_TRANS_VIEW_Y   9

// Absolute model-space translation: move object so that e_keypoint lands at
// the specified model-space position.  e_para[0..2] supply the target X,Y,Z
// coordinates in local (display) units; the edit function converts to base
// units internally.  Equivalent to MGED's "translate X Y Z" command when in
// object-edit (oed) mode.  Unlike the VIEW variants above this operation is
// view-independent and maps directly to model coordinates.
#define RT_MATRIX_EDIT_TRANS_MODEL_XYZ 14

// Non-uniform scale operations scale relative to the MODEL coordinate system,
// NOT the view plane.
//
// Note that the underlying solids do not always support directly expressing
// the shapes that can be created with these opts using comb instances (for
// example, a TOR scaled in the X direction) and the edit will be rejected in
// those cases.  Just because a comb instance of a solid can be stretched, that
// does not mean the solid itself can support that shape.  Consequently, it is
// recommended that the _X, _Y and _Z versions of the scale operations be used
// sparingly if at all.  Even if a comb instance's tree is able to support
// rendering such an operation at the time the original edit is made, a
// subsequent editing of that tree could add new geometry and ultimately result
// in an inability to apply the matrix successfully to all leaf solids.
#define RT_MATRIX_EDIT_SCALE    10
#define RT_MATRIX_EDIT_SCALE_X  11
#define RT_MATRIX_EDIT_SCALE_Y  12
#define RT_MATRIX_EDIT_SCALE_Z  13


struct rt_edit_map;

struct rt_edit {

    struct rt_edit_map *m;

    // Optional logging of messages from editing code
    struct bu_vls *log_str;

    // Container to hold the intermediate state
    // of the object being edited
    struct rt_db_internal es_int;

    // When we're editing a comb instance, we need to record which specific
    // instance(s) we're working with.  db_find_named_leaf is what finds the
    // named instance that the editing matrix will ultimately be applied to.
    //
    // Although normally we're either transforming the whole comb (i.e. the
    // matrix gets applied to ALL instances in the comb tree) or operating on a
    // single instance, there is nothing conceptually that would prevent the
    // editing of multiple instances.
    //
    // In the interest of making the API flexible, we use a bu_ptbl to hold
    // pointers to tr_l.tl_name strings in the es_int comb tree to identify all
    // active instances in the comb that are actually being edited.  (An empty
    // bu_ptbl means edit all of them.) For any object type other than combs
    // this tbl is ignored.
    //
    // An edit operation on a comb will populate this with the active instance(s).
    // which in turn means the edit structure will have enough information for
    // application level drawing code to figure out which solids in the comb need
    // to be visualized as part of the active editing geometry.
    struct bu_ptbl comb_insts;

    // Tolerance for calculations
    const struct bn_tol *tol;

    // Main view associated with the edit.  This may not be the only view in
    // which the edit is *visible*, but this should hold the pointer to the
    // view which will be used to drive any view dependent edit ops.
    struct bview *vp;
    // Knob based editing data
    struct bview_knobs k;

    // Current editing operation.  This holds the exact operation being
    // performed (for example, ECMD_TGC_SCALE_A to scale a tgc primitive's
    // A vector).  The RT_PARAM_* and RT_MATRIX_* values may also be set
    // here, if the operations being performed are the more generic/general
    // cases.
    int edit_flag;

    // MGED uses es_edclass (in mged_edit_state) to track whether the active
    // edit is a rotate, translate, or scale.  That tracking is driven by the
    // primitive-specific EDIT_ROTATE / EDIT_TRAN / EDIT_SCALE macros in
    // sedit.h.  edit_mode serves the same purpose in the librt editing layer:
    // it tells mouse-input handlers and knob drivers which interaction mode
    // is active, without having to enumerate every primitive-specific ECMD.
    //
    // Options:
    // RT_PARAMS_EDIT_TRANS  – free translate
    // RT_PARAMS_EDIT_SCALE  – uniform or axis scale
    // RT_PARAMS_EDIT_ROT    – rotation
    // RT_PARAMS_EDIT_PICK   – geometric pick (e.g. click to select a vertex)
    //
    // For matrix editing (RT_MATRIX_EDIT_*) and primitive-scale (pscale),
    // edit_mode is also set so that edit_generic_xy() and the knob rate loop
    // can decide how to apply the incremental delta.
    //
    // NOTE - this is only active in the librt editing code; MGED uses its
    // own primitive-aware defines (SEDIT_ROTATE, etc.) for the same purpose.
    int edit_mode;

    fastf_t es_scale;           /* scale factor */
    mat_t incr_change;          /* change(s) from last cycle */

    /* Matrix-based editing visualizations are a different beast from edits
     * that change primitive parameters.  In the latter case, we must
     * regenerate vlists based on new object parameters.  Applications
     * supporting solid editing need to be able to support visualizing an
     * entity whose source geometry visualization data is constantly changing.
     *
     * Combs, on the other hand, have a wireframe representation that consists
     * of one or more individual primitive wireframes from OTHER solids -
     * potentially *thousands* of them for large models.  Nor to matrix edits
     * typically call for a wireframe regeneration - tra and rot operations
     * definitely don't, and while scaled primitives might be more smoothly
     * represented by refreshed wireframes not all matrix edits can be
     * successfully translated to primitive param updates in all cases.  As
     * a result, for matrix-based editing wireframes are treated as static
     * data to be manipulated.
     *
     * Consequently, rather than generating copies of all those wireframes and
     * modifying them all every time an incremental edit is made, a more
     * efficient option is to re-use any existing wireframes the app already
     * has loaded for the comb tree in question and distort the *view space* to
     * reflect the editing operation while doing the draw.  Because the source
     * wireframe data isn't changing, view distortion drawing is feasible.
     *
     * Combs are *only* editable via matrix manipulations, so rather than
     * trying to manage the comb's editing visualization data in this struct
     * the problem is "punted" to the application, which can decide for itself
     * whether and how to reuse any existing wireframe information when drawing
     * editing objects.
     *
     * To use model_changes to generate appropriate matrices for this purpose,
     * the pattern looks like the following:
     *
     * mat_t model2objview;
     * struct bview *vp = <current view pointer>;
     * bn_mat_mul(model2objview, vp->gv_model2view, s->model_changes);
     * ## A second bn_mat_mul might be needed if a perspective matrix is in use
     * dm_loadmatrix(DMP, model2objview, which_eye);
     * ## Do the drawing
     */
    mat_t model_changes;        /* full changes this edit */

    mat_t model2objview;        /* Matrix for applying model_changes to view objects (used for matrix xy translation) */

    fastf_t acc_sc[3];          /* accumulate local object scale factors */
    fastf_t acc_sc_obj;         /* accumulate global object scale factor */
    fastf_t acc_sc_sol;         /* accumulate solid scale factor */
    mat_t acc_rot_sol;          /* accumulate solid rotations */

    int e_keyfixed;             /* keypoint specified by user? */
    point_t e_keypoint;         /* center of editing xforms */
    const char *e_keytag;       /* string identifying the keypoint */

    int e_mvalid;               /* e_mparam valid.  e_inpara must = 0 */
    vect_t e_mparam;            /* mouse input param.  Only when es_mvalid set */

    int e_inpara;               /* number of valid entries in e_para (set by caller before rt_edit_process) */
    fastf_t e_para[RT_EDIT_MAXPARA]; /* keyboard input parameters; e_para[0..e_inpara-1] are valid */

    /* Parallel string-parameter array for RT_EDIT_PARAM_STRING commands.
     * Use rt_edit_set_str() to write; primitive edit handlers retrieve the
     * value via the ECMD_GET_FILENAME callback mechanism. */
    int  e_nstr;                                     /* number of valid entries in e_str */
    char e_str[RT_EDIT_MAXSTR][RT_EDIT_MAXSTR_LEN];  /* string params; e_str[0..e_nstr-1] are valid */

    mat_t e_invmat;             /* inverse of e_mat KAA */
    mat_t e_mat;                /* accumulated matrix of path */

    point_t curr_e_axes_pos;    /* center of editing xforms */
    point_t e_axes_pos;

    // Conversion factors
    double base2local;
    double local2base;

    // Trigger for view updating
    int update_views;

    // vlfree list
    struct bu_list *vlfree;

    /* Flag to trigger some primitive edit opts to use keypoint (and maybe other behaviors?) */
    int mv_context;

    /* Internal primitive editing information specific to primitive types. */
    void *ipe_ptr;

    /* Snap-to-grid: when snap.enabled is non-zero, ft_edit_xy implementations
     * should call rt_edit_snap_point() on the computed UV/model position before
     * applying it.  spacing is in model (base) units. */
    struct {
	int     enabled;   /**< non-zero → snap active */
	fastf_t spacing;   /**< grid spacing in mm (base units) */
    } snap;

    /* Single-level checkpoint/revert.  rt_edit_checkpoint() serialises
     * es_int here; rt_edit_revert() restores it.  bu_external.ext_buf is
     * NULL when no snapshot has been saved. */
    struct bu_external es_ckpt;

    /* User pointer */
    void *u_ptr;
};

/** Create and initialize an rt_edit struct */
RT_EXPORT extern struct rt_edit *
rt_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);

/** Free a rt_edit struct */
RT_EXPORT extern void
rt_edit_destroy(struct rt_edit *s);

/**
 * Reset an rt_edit back to an idle/empty state without freeing the struct
 * itself.  This frees any loaded primitive data (es_int, ipe_ptr, es_ckpt,
 * comb_insts) and resets all edit-state fields to their initial values, but
 * leaves the rt_edit struct, its edit_map, and its log_str allocated and
 * usable.  The pointer returned by rt_edit_create() remains valid after this
 * call and can be reloaded with a new solid by calling rt_edit_reinit().
 *
 * Use this instead of rt_edit_destroy()+rt_edit_create() when a single
 * persistent rt_edit should be kept alive across multiple editing sessions
 * (e.g. in MGED where MEDIT(s) must never be NULL).
 */
RT_EXPORT extern void
rt_edit_reset(struct rt_edit *s);

/**
 * Reload an existing (reset or newly created) rt_edit with solid data from
 * @a dfp inside database @a dbip.  Equivalent to rt_edit_create() but reuses
 * the already-allocated struct rather than allocating a new one.
 *
 * Calls rt_edit_reset() first to discard any previous solid data, then
 * imports the solid, sets up the primitive-specific private state, and
 * computes the path matrix and initial keypoint.
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR if the solid import fails.
 */
RT_EXPORT extern int
rt_edit_reinit(struct rt_edit *s, struct db_full_path *dfp, struct db_i *dbip,
               struct bn_tol *tol, struct bview *v);

/**
 * Set a string parameter in the edit struct's e_str[] array.
 *
 * @param s      The edit struct to update.
 * @param index  Slot index (0 .. RT_EDIT_MAXSTR-1); must equal
 *               rt_edit_param_desc::index for the STRING parameter.
 * @param str    NUL-terminated string to copy (truncated to
 *               RT_EDIT_MAXSTR_LEN-1 bytes).
 */
RT_EXPORT extern void
rt_edit_set_str(struct rt_edit *s, int index, const char *str);

/* Logic for working with editing callback maps.
 *
 * Editing callback maps allow applications to register logic to be executed for particular
 * commands or the pre-defined standard registration points which are general to all commands,
 * per the defines below. */
#define ECMD_CLEAR_CLBKS           0
#define ECMD_PRINT_STR            10
#define ECMD_PRINT_RESULTS        20
#define ECMD_EAXES_POS            30
#define ECMD_REPLOT_EDITING_SOLID 40
#define ECMD_VIEW_UPDATE          50
#define ECMD_VIEW_SET_FLAG        60
#define ECMD_MENU_SET             70
#define ECMD_MENU_REFRESH         71
#define ECMD_GET_FILENAME         80

RT_EXPORT extern struct rt_edit_map *
rt_edit_map_create(void);
RT_EXPORT extern void
rt_edit_map_destroy(struct rt_edit_map *);
RT_EXPORT extern int
rt_edit_map_clbk_set(struct rt_edit_map *em, int ed_cmd, int mode, bu_clbk_t f, void *d);
RT_EXPORT extern int
rt_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_edit_map *em, int ed_cmd, int mode);
RT_EXPORT extern int
rt_edit_map_clear(struct rt_edit_map *m);
RT_EXPORT extern int
rt_edit_map_copy(struct rt_edit_map *om, struct rt_edit_map *im);

/* Functions for manipulating rt_edit data */
RT_EXPORT extern void
rt_get_solid_keypoint(struct rt_edit *s, point_t *pt, const char **strp, fastf_t *mat);

RT_EXPORT extern void
rt_edit_set_edflag(struct rt_edit *s, int edflag);

RT_EXPORT extern int
rt_edit_knob_cmd_process(
	struct rt_edit *s,
	vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran, int *do_sca,
        struct bview *v, const char *cmd, fastf_t f,
        char origin, int incr_flag, void *u_data
	);

RT_EXPORT extern void
rt_knob_edit_rot(struct rt_edit *s,
        char coords,
        char rotate_about,
        int matrix_edit,
        mat_t newrot
	);

RT_EXPORT extern void
rt_knob_edit_tran(struct rt_edit *s,
        char coords,
        int matrix_edit,
        const vect_t tvec);

RT_EXPORT extern void
rt_knob_edit_sca(
	struct rt_edit *s,
	int matrix_edit);

/* Equivalent to sedit - run editing logic after input data is set in
 * rt_edit container */
RT_EXPORT extern void
rt_edit_process(struct rt_edit *s);

/**
 * Snap a 2-D UV point to the grid defined in s->snap.
 *
 * If s->snap.enabled is zero the point is returned unchanged.
 * Otherwise each component is rounded to the nearest multiple of
 * s->snap.spacing.
 *
 * @param[in,out] pt  2-D UV coordinate to snap (in model/base units).
 * @param[in]     s   rt_edit struct carrying snap configuration.
 */
RT_EXPORT extern void
rt_edit_snap_point(point2d_t pt, const struct rt_edit *s);

/**
 * Save a snapshot of the current primitive parameters so they can be
 * restored later with rt_edit_revert().
 *
 * The snapshot is stored inside the rt_edit struct.  Calling this
 * function again overwrites any previous snapshot (single-level undo).
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR if the export failed.
 */
RT_EXPORT extern int
rt_edit_checkpoint(struct rt_edit *s);

/**
 * Restore primitive parameters from the snapshot saved by
 * rt_edit_checkpoint().
 *
 * If no snapshot has been saved (or the last snapshot was already
 * consumed) this function logs a message and returns BRLCAD_ERROR.
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR otherwise.
 */
RT_EXPORT extern int
rt_edit_revert(struct rt_edit *s);


/* Edit menu items encode information about specific edit operations, as well
 * as info documenting them.  Edit functab methods use this data type. */
struct rt_edit_menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_edit *, int, int, int, void *);
    int menu_arg;
};


/*
 * ============================================================
 * ft_edit_desc() parameter-descriptor API
 *
 * These types allow a primitive's ft_edit_desc() slot to return
 * machine-readable metadata describing every edit operation it
 * supports.  A GUI (e.g. qged) can use this metadata to
 * auto-generate appropriate edit widgets without needing any
 * primitive-specific code.
 * ============================================================
 */

/** Parameter type codes for struct rt_edit_param_desc */
#define RT_EDIT_PARAM_SCALAR   1  /**< single fastf_t; QDoubleSpinBox / QSlider     */
#define RT_EDIT_PARAM_INTEGER  2  /**< truncated fastf_t; QSpinBox                  */
#define RT_EDIT_PARAM_BOOLEAN  3  /**< !NEAR_ZERO(val); QCheckBox                   */
#define RT_EDIT_PARAM_POINT    4  /**< point_t (3 fastf_t); 3x QDoubleSpinBox       */
#define RT_EDIT_PARAM_VECTOR   5  /**< vect_t  (3 fastf_t); 3x QDoubleSpinBox       */
#define RT_EDIT_PARAM_STRING   6  /**< NUL-terminated; QLineEdit                    */
#define RT_EDIT_PARAM_ENUM     7  /**< integer choice; QComboBox                    */
#define RT_EDIT_PARAM_COLOR    8  /**< RGB triple as three fastf_t (0-255) in
                                   *   e_para[index..index+2]; QColorDialog button  */
#define RT_EDIT_PARAM_MATRIX   9  /**< 4x4 row-major in e_para[0..15]; matrix widget*/

/** Sentinel for "no range constraint" on a parameter. */
#define RT_EDIT_PARAM_NO_LIMIT  (-DBL_MAX)

/**
 * Describes a single input parameter for one edit command.
 *
 * For scalar/integer/boolean/enum parameters the value is stored in
 * s->e_para[index].  For POINT/VECTOR, three consecutive slots starting
 * at e_para[index] are used.  For MATRIX, e_para[0..15] are used.
 * For STRING the value is in the primitive-specific edit struct; prim_field
 * documents which field that is.
 * For COLOR three fastf_t integer values (0-255) are stored in
 * e_para[index], e_para[index+1], e_para[index+2].
 */
struct rt_edit_param_desc {
    const char  *name;         /**< machine-readable id, e.g. "r1"                  */
    const char  *label;        /**< human-readable widget label, e.g. "Major Radius" */
    int          type;         /**< RT_EDIT_PARAM_* type code                        */
    int          index;        /**< offset into s->e_para[] (unused for STRING)      */
    fastf_t      range_min;    /**< RT_EDIT_PARAM_NO_LIMIT = no lower bound          */
    fastf_t      range_max;    /**< RT_EDIT_PARAM_NO_LIMIT = no upper bound          */
    const char  *units;        /**< "length", "angle_deg", "angle_rad",
                                *   "fraction", "count", "none", or NULL             */
    /* RT_EDIT_PARAM_ENUM only */
    int          nenum;                   /**< number of choices                     */
    const char * const *enum_labels;      /**< human-readable option strings         */
    const int   *enum_ids;                /**< integer value stored in e_para[index] */
    /* RT_EDIT_PARAM_STRING only */
    const char  *prim_field;   /**< name of the primitive struct field, e.g.
                                *   "es_shader" or "dsp_name"                        */
};

/**
 * Describes a single edit command (one ECMD_* constant) together with
 * the parameters it requires.
 */
struct rt_edit_cmd_desc {
    int          cmd_id;        /**< ECMD_* constant                                 */
    const char  *label;         /**< human-readable operation label                  */
    const char  *category;      /**< grouping hint: "radius", "geometry",
                                 *   "rotation", "material", "tree", "misc"          */
    int          nparam;        /**< number of entries in params[]                   */
    const struct rt_edit_param_desc *params; /**< NULL when nparam == 0              */
    /** Non-zero: GUI should re-call rt_edit_process() on every widget change
     *  (live wireframe update).  Zero: only apply on explicit Apply button. */
    int          interactive;
    /** Suggested display order within the category group.  Lower values
     *  appear first.  Ties are broken by array order. */
    int          display_order;
};

/**
 * Top-level descriptor for a single primitive type.
 * Returned by ft_edit_desc().
 */
struct rt_edit_prim_desc {
    const char                    *prim_type;  /**< "tor", "ell", "tgc", ...         */
    const char                    *prim_label; /**< "Torus", "Ellipsoid", ...         */
    int                            ncmd;
    const struct rt_edit_cmd_desc *cmds;       /**< array of ncmd entries             */
};

/**
 * Serialise a primitive edit descriptor to a JSON string appended to @p out.
 * The caller is responsible for bu_vls_init / bu_vls_free.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on error.
 */
RT_EXPORT extern int
rt_edit_prim_desc_to_json(struct bu_vls *out,
                          const struct rt_edit_prim_desc *desc);

/**
 * Convenience wrapper: look up the EDOBJ entry for @p prim_type_id and
 * call rt_edit_prim_desc_to_json() on its ft_edit_desc() result.
 * Returns BRLCAD_OK on success, BRLCAD_ERROR if the primitive has no
 * descriptor or the type id is out of range.
 */
RT_EXPORT extern int
rt_edit_type_to_json(struct bu_vls *out, int prim_type_id);


__END_DECLS

#endif /* RT_EDIT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
