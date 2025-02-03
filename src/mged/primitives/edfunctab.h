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
/** @file mged/primitives/edfunctab.h
  */

#ifndef EDFUNCTAB_H
#define EDFUNCTAB_H

#include "common.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "../mged.h"

__BEGIN_DECLS

const char *
mged_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol);

/* scale the solid uniformly about its vertex point */
int
mged_generic_sscale(
	struct mged_solid_edit *s,
	struct rt_db_internal *ip
	);

/* translate solid */
void
mged_generic_strans(
	struct mged_solid_edit *s,
	struct rt_db_internal *ip
	);

/* rot solid about vertex */
void
mged_generic_srot(
	struct mged_solid_edit *s,
	struct rt_db_internal *ip
	);

void
mged_generic_sscale_xy(
	struct mged_solid_edit *s,
	const vect_t mousevec
	);
void
mged_generic_strans_xy(vect_t *pos_view,
	struct mged_solid_edit *s,
	const vect_t mousevec
	);

void
update_edit_absolute_tran(struct mged_solid_edit *s, vect_t view_pos);

int mged_generic_edit(struct mged_solid_edit *s, int edflag);

int
mged_generic_edit_xy(
	struct mged_solid_edit *s,
	int edflag,
	const vect_t mousevec
	);


void
mged_set_edflag(struct mged_solid_edit *s, int edflag);

int
mged_generic_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol);


struct mged_functab {
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
	    struct mged_solid_edit *s,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_E_AXES_POS_CAST(_func) ((void(*)(struct mged_solid_edit *s, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

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

    int(*ft_edit)(struct mged_solid_edit *s, int edflag);
#define EDFUNCTAB_FUNC_EDIT_CAST(_func) ((int(*)(struct mged_solid_edit *, int))((void (*)(void))_func))

    /* Translate mouse info into edit ready info.  mousevec [X] and [Y] are in
     * the range -1.0...+1.0, corresponding to viewspace.
     *
     * In order to allow command line commands to do the same things that a
     * mouse movements can, the preferred strategy is to store values and allow
     * ft_edit to actually do the work. */
    int(*ft_edit_xy)(struct mged_solid_edit *s, int edflag, const vect_t mousevec);
#define EDFUNCTAB_FUNC_EDITXY_CAST(_func) ((int(*)(struct mged_solid_edit *, int, const vect_t))((void (*)(void))_func))

    /* Create primitive specific editing struct */
    void *(*ft_prim_edit_create)(void);
#define EDFUNCTAB_FUNC_PRIMEDIT_CREATE_CAST(_func) ((void *(*)()((void (*)(void))_func))

    /* Destroy primitive specific editing struct */
    void (*ft_prim_edit_destroy)(void *);
#define EDFUNCTAB_FUNC_PRIMEDIT_DESTROY_CAST(_func) ((void(*)(void *)((void (*)(void))_func))

    int (*ft_menu_str)(struct bu_vls *m, const struct rt_db_internal *ip, const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_MENU_STR_CAST(_func) ((int(*)(struct bu_vls *, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    struct menu_item *(*ft_menu_item)(const struct bn_tol *tol);
#define EDFUNCTAB_FUNC_MENU_ITEM_CAST(_func) ((struct menu_item *(*)(const struct bn_tol *))((void (*)(void))_func))

};
extern const struct mged_functab EDOBJ[];

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
