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
 */

#ifndef RT_EDIT_H
#define RT_EDIT_H

#include "common.h"
#include "vmath.h"
#include "bn/mat.h"
#include "bv/defines.h"
#include "rt/defines.h"
#include "rt/db_internal.h"
#include "rt/resource.h"

__BEGIN_DECLS

// Settings used for both solid and matrix edits
#define RT_EDIT_DEFAULT   -1
#define RT_EDIT_IDLE       0


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

// Note that the underlying solids do not always support directly expressing
// the shapes that can be created with these opts using comb instances (for
// example, a tor scaled in the X direction) and the edit will be rejected in
// those cases.  Just because a comb instance of a solid can be stretched, that
// does not mean the solid itself can support that shape.  Consequently, it is
// recommended that the _X, _Y and _Z scale operations be used sparingly if at
// all - even if a comb instance's tree is able to support rendering such an
// operation when made, a subsequent editing of that tree could result in an
// inability to apply the matrix successfully to all leaf solids.
#define RT_MATRIX_EDIT_ROT       6
#define RT_MATRIX_EDIT_TRANS     7
#define RT_MATRIX_EDIT_TRANS_X   8
#define RT_MATRIX_EDIT_TRANS_Y   9
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

    // MGED wants to know if we're in solid rotate, translate or scale mode,
    // even if edit_flag is set to a more specific mode. (TODO - why?)
    // Rather than keying off of primitive specific edit op types, have the ops
    // set a flag.  Options are:
    //
    // RT_PARAMS_EDIT_TRANS
    // RT_PARAMS_EDIT_SCALE
    // RT_PARAMS_EDIT_ROT
    // RT_PARAMS_EDIT_PICK
    //
    // (TODO - should we be setting this for matrix and pscale values as well?
    // The above were originally driven by MGED code, which IIRC was using it
    // for awareness of cases when primitive specific edits need specific
    // interaction modes...)
    //
    // NOTE - this is only active in the new librt editing code - MGED uses
    // primitive aware defines to do this instead (ew) so in the main branch
    // it is not used except in the state save/restore functions.
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

    int e_inpara;               /* parameter input from keyboard flag.  1 == e_para valid.  e_mvalid must = 0 */
    vect_t e_para;              /* keyboard input parameter changes */

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

    /* User pointer */
    void *u_ptr;
};

/** Create and initialize an rt_edit struct */
RT_EXPORT extern struct rt_edit *
rt_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);

/** Free a rt_edit struct */
RT_EXPORT extern void
rt_edit_destroy(struct rt_edit *s);

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
        vect_t tvec);

RT_EXPORT extern void
rt_knob_edit_sca(
	struct rt_edit *s,
	int matrix_edit);

/* Equivalent to sedit - run editing logic after input data is set in
 * rt_edit container */
RT_EXPORT extern void
rt_edit_process(struct rt_edit *s);


/* Edit menu items encode information about specific edit operations, as well
 * as info documenting them.  Edit functab methods use this data type. */
struct rt_edit_menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_edit *, int, int, int, void *);
    int menu_arg;
};


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
