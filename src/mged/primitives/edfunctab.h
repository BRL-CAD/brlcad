/*                    M G E D _ F U N C T A B . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file primitives/edfunctab.h
  */

#ifndef EDFUNCTAB_H
#define EDFUNCTAB_H

#include "common.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rt_ecmds.h"



#ifdef __cplusplus

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

#endif

#ifdef __cplusplus

class RT_Edit_Map_Internal {
    public:
	// Key is ECMD_ type, populated from MGED_Internal map
	std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> cmd_prerun_clbk;
	std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> cmd_during_clbk;
	std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> cmd_postrun_clbk;
	std::map<std::pair<int, int>, std::pair<bu_clbk_t, void *>> cmd_linger_clbk;

	std::map<bu_clbk_t, int> clbk_recursion_depth_cnt;
	std::map<int, int> cmd_recursion_depth_cnt;
};

#else

#define RT_Edit_Map_Internal void

#endif

struct rt_solid_edit_map {
    RT_Edit_Map_Internal *i;
};

__BEGIN_DECLS

#define RT_SOLID_EDIT_IDLE		0	/* edarb.c */
#define RT_SOLID_EDIT_TRANS		1	/* buttons.c */
#define RT_SOLID_EDIT_SCALE		2	/* buttons.c */	/* Scale whole solid by scalar */
#define RT_SOLID_EDIT_ROT		3	/* buttons.c */
#define RT_SOLID_EDIT_PSCALE		4	/* Scale one solid parameter by scalar */

#define CMD_OK 919
#define CMD_BAD 920
#define CMD_MORE 921

struct rt_solid_edit {

    struct rt_solid_edit_map *m;

    // Optional logging of messages from editing code
    struct bu_vls *log_str;

    // Container to hold the intermediate state
    // of the object being edited (I think?)
    struct rt_db_internal es_int;

    // Tolerance for calculations
    const struct bn_tol *tol;
    struct bview *vp;

    // Primary variable used to identify specific editing operations
    int edit_flag;
    /* item/edit_mode selected from menu.  TODO - it seems like this
     * may be used to "specialize" edit_flag to narrow its scope to
     * specific operations - in which case we might be able to rename
     * it to something more general than "menu"... */
    int edit_menu;

    // Translate values used in XY mouse vector manipulation
    vect_t edit_absolute_model_tran;
    vect_t last_edit_absolute_model_tran;
    vect_t edit_absolute_view_tran;
    vect_t last_edit_absolute_view_tran;

    // Scale values used in XY mouse vector manipulation
    fastf_t edit_absolute_scale;

    // MGED wants to know if we're in solid rotate, translate or scale mode.
    // (TODO - why?) Rather than keying off of primitive specific edit op
    // types, have the ops set flags:
    int solid_edit_rotate;
    int solid_edit_translate;
    int solid_edit_scale;
    int solid_edit_pick;

    fastf_t es_scale;		/* scale factor */
    mat_t incr_change;		/* change(s) from last cycle */
    mat_t model_changes;	/* full changes this edit */
    fastf_t acc_sc_sol;		/* accumulate solid scale factor */
    mat_t acc_rot_sol;		/* accumulate solid rotations */

    int e_keyfixed;		/* keypoint specified by user? */
    point_t e_keypoint;		/* center of editing xforms */
    const char *e_keytag;	/* string identifying the keypoint */

    int e_mvalid;		/* e_mparam valid.  e_inpara must = 0 */
    vect_t e_mparam;		/* mouse input param.  Only when es_mvalid set */

    int e_inpara;		/* parameter input from keyboard flag.  1 == e_para valid.  e_mvalid must = 0 */
    vect_t e_para;		/* keyboard input parameter changes */

    mat_t e_invmat;		/* inverse of e_mat KAA */
    mat_t e_mat;		/* accumulated matrix of path */

    point_t curr_e_axes_pos;	/* center of editing xforms */
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

struct rt_solid_edit_map *rt_solid_edit_map_create();
void rt_solid_edit_map_destroy(struct rt_solid_edit_map *);
int rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d);
int rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode);
int rt_solid_edit_map_sync(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im);
int rt_solid_edit_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit *s, int ed_cmd, int menu_cmd, int mode);


struct rt_solid_edit_menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_solid_edit *, int, int, int, void *);
    int menu_arg;
};

void rt_solid_edit_process(struct rt_solid_edit *s);


void rt_get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, struct rt_db_internal *ip, fastf_t *mat);

extern struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);
extern void
rt_solid_edit_destroy(struct rt_solid_edit *ssed);


const char *
rt_solid_edit_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol);

/* scale the solid uniformly about its vertex point */
int
rt_solid_edit_generic_sscale(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

/* translate solid */
void
rt_solid_edit_generic_strans(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

/* rot solid about vertex */
void
rt_solid_edit_generic_srot(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

void
rt_solid_edit_generic_sscale_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	);
void
rt_solid_edit_generic_strans_xy(vect_t *pos_view,
	struct rt_solid_edit *s,
	const vect_t mousevec
	);

void
update_edit_absolute_tran(struct rt_solid_edit *s, vect_t view_pos);

int rt_solid_edit_generic_edit(struct rt_solid_edit *s, int edflag);

int
rt_solid_edit_generic_edit_xy(
	struct rt_solid_edit *s,
	int edflag,
	const vect_t mousevec
	);


void
rt_solid_edit_set_edflag(struct rt_solid_edit *s, int edflag);

int
rt_solid_edit_generic_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol);


struct rt_solid_edit_functab {
    uint32_t magic;
    char ft_name[17]; /* current longest name is 16 chars, need one element for terminating NULL */
    char ft_label[9]; /* current longest label is 8 chars, need one element for terminating NULL */

    void (*ft_labels)(int *num_lines,
	    point_t *lines,
	    struct rt_point_labels *pl,
	    int max_pl,
	    const mat_t xform,
	    struct rt_db_internal *ip,
	    struct bn_tol *tol);
#define EDFUNCTAB_FUNC_LABELS_CAST(_func) ((void (*)(int *, point_t *, struct rt_point_labels *, int, const mat_t, struct rt_db_internal *, struct bn_tol *))((void (*)(void))_func))

    const char *(*ft_keypoint)(point_t *pt,
	    const char *keystr,
	    const mat_t mat,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_KEYPOINT_CAST(_func) ((const char *(*)(point_t *, const char *, const mat_t, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    void(*ft_e_axes_pos)(
	    struct rt_solid_edit *s,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_E_AXES_POS_CAST(_func) ((void(*)(struct rt_solid_edit *s, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    // Written format is intended to be human editable text that will be parsed
    // by ft_read_params.  There are no guarantees of formatting consistency by
    // this API, so external apps should not rely on this format being
    // consistent release-to-release.  The only API guarantee is that
    // ft_write_params output for a given BRL-CAD version is readable by
    // ft_read_params.
    void(*ft_write_params)(
	    struct bu_vls *p,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol,
	    fastf_t base2local);
#define EDFUNCTAB_FUNC_WRITE_PARAMS_CAST(_func) ((void(*)(struct bu_vls *, const struct rt_db_internal *, const struct bn_tol *, fastf_t))((void (*)(void))_func))

    // Parse ft_write_params output and assign numerical values to ip.
    int(*ft_read_params)(
	    struct rt_db_internal *ip,
	    const char *fc,
	    const struct bn_tol *tol,
	    fastf_t local2base);
#define EDFUNCTAB_FUNC_READ_PARAMS_CAST(_func) ((int(*)(struct rt_db_internal *, const char *, const struct bn_tol *, fastf_t))((void (*)(void))_func))

    int(*ft_edit)(struct rt_solid_edit *s, int edflag);
#define EDFUNCTAB_FUNC_EDIT_CAST(_func) ((int(*)(struct rt_solid_edit *, int))((void (*)(void))_func))

    /* Translate mouse info into edit ready info.  mousevec [X] and [Y] are in
     * the range -1.0...+1.0, corresponding to viewspace.
     *
     * In order to allow command line commands to do the same things that a
     * mouse movements can, the preferred strategy is to store values and allow
     * ft_edit to actually do the work. */
    int(*ft_edit_xy)(struct rt_solid_edit *s, int edflag, const vect_t mousevec);
#define EDFUNCTAB_FUNC_EDITXY_CAST(_func) ((int(*)(struct rt_solid_edit *, int, const vect_t))((void (*)(void))_func))

    /* Create primitive specific editing struct */
    void *(*ft_prim_edit_create)(void);
#define EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(_func) ((void *(*)()((void (*)(void))_func))

    /* Destroy primitive specific editing struct */
    void (*ft_prim_edit_destroy)(void *);
#define EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(_func) ((void(*)(void *)((void (*)(void))_func))

    int (*ft_menu_str)(struct bu_vls *m, const struct rt_db_internal *ip, const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_MENU_STR_CAST(_func) ((int(*)(struct bu_vls *, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    struct rt_solid_edit_menu_item *(*ft_menu_item)(const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_MENU_ITEM_CAST(_func) ((struct rt_solid_edit_menu_item *(*)(const struct bn_tol *))((void (*)(void))_func))

};
extern const struct rt_solid_edit_functab EDOBJ[];

__END_DECLS

#endif  /* EDFUNCTAB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
