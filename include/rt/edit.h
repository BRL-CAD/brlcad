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

#define RT_SOLID_EDIT_DEFAULT   -1
#define RT_SOLID_EDIT_IDLE       0
#define RT_SOLID_EDIT_TRANS      1
#define RT_SOLID_EDIT_SCALE      2 // Scale whole solid by scalar
#define RT_SOLID_EDIT_ROT        3
#define RT_SOLID_EDIT_PSCALE     4 // Scale one solid parameter by scalar
#define RT_SOLID_EDIT_PICK       5
// The following are unique to matrix editing (done via objedit in MGED).
// Unlike other ops, they can ONLY be done reliably by updating a matrix rather
// than by altering solid parameters - some primitives do not support directly
// expressing the shapes that can be created with these opts (for example, a
// tor scaled in the X direction.)
#define RT_MATRIX_EDIT_SCALE_X   6
#define RT_MATRIX_EDIT_SCALE_Y   7
#define RT_MATRIX_EDIT_SCALE_Z   8

struct rt_solid_edit_map;

struct rt_solid_edit {

    struct rt_solid_edit_map *m;

    // Optional logging of messages from editing code
    struct bu_vls *log_str;

    // Container to hold the intermediate state
    // of the object being edited
    struct rt_db_internal es_int;

    // Tolerance for calculations
    const struct bn_tol *tol;
    struct bview *vp;

    // Current editing operation
    int edit_flag;

    // Knob based editing data
    struct bview_knobs k;

    // MGED wants to know if we're in solid rotate, translate or scale mode.
    // (TODO - why?) Rather than keying off of primitive specific edit op
    // types, have the ops set a flag.  Options are:
    //
    // RT_SOLID_EDIT_TRANS
    // RT_SOLID_EDIT_SCALE
    // RT_SOLID_EDIT_ROT
    // RT_SOLID_EDIT_PICK
    int solid_edit_mode;

    fastf_t es_scale;           /* scale factor */
    mat_t incr_change;          /* change(s) from last cycle */
    mat_t model_changes;        /* full changes this edit */
    mat_t model2objview;        /* Matrix for applying model_changes to view objects in vp (used in obj_edit) */

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

/** Create and initialize an rt_solid_edit struct */
RT_EXPORT extern struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);

/** Free a rt_solid_edit struct */
RT_EXPORT extern void
rt_solid_edit_destroy(struct rt_solid_edit *s);

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

RT_EXPORT extern struct rt_solid_edit_map *
rt_solid_edit_map_create(void);
RT_EXPORT extern void
rt_solid_edit_map_destroy(struct rt_solid_edit_map *);
RT_EXPORT extern int
rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int mode, bu_clbk_t f, void *d);
RT_EXPORT extern int
rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int mode);
RT_EXPORT extern int
rt_solid_edit_map_clear(struct rt_solid_edit_map *m);
RT_EXPORT extern int
rt_solid_edit_map_copy(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im);

/* Functions for manipulating rt_solid_edit data */
RT_EXPORT extern void
rt_get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, fastf_t *mat);

RT_EXPORT extern void
rt_update_edit_absolute_tran(struct rt_solid_edit *s, vect_t view_pos);

RT_EXPORT extern void
rt_solid_edit_set_edflag(struct rt_solid_edit *s, int edflag);

RT_EXPORT extern int
rt_edit_knob_cmd_process(
	struct rt_solid_edit *s,
	vect_t *rvec, int *do_rot, vect_t *tvec, int *do_tran, int *do_sca,
        struct bview *v, const char *cmd, fastf_t f,
        char origin, int incr_flag, void *u_data
	);

RT_EXPORT extern void
rt_knob_edit_rot(struct rt_solid_edit *s,
        char coords,
        char rotate_about,
        int matrix_edit,
        mat_t newrot
	);

RT_EXPORT extern void
rt_knob_edit_tran(struct rt_solid_edit *s,
        char coords,
        int matrix_edit,
        vect_t tvec);

RT_EXPORT extern void
rt_knob_edit_sca(
	struct rt_solid_edit *s,
	int matrix_edit);

/* Equivalent to sedit - run editing logic after input data is set in
 * rt_solid_edit container */
RT_EXPORT extern void
rt_solid_edit_process(struct rt_solid_edit *s);


/* Edit menu items encode information about specific edit operations, as well
 * as info documenting them.  Edit functab methods use this data type. */
struct rt_solid_edit_menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_solid_edit *, int, int, int, void *);
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
