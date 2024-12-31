/*                    M G E D _ F U N C T A B . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/primitives/mged_functab.h
  */

#ifndef MGED_FUNCTAB_H
#define MGED_FUNCTAB_H

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
#define MGEDFUNCTAB_FUNC_LABELS_CAST(_func) ((void (*)(int *, point_t *, struct rt_point_labels *, int, const mat_t, struct rt_db_internal *, struct bn_tol *))((void (*)(void))_func))

    const char *(*ft_keypoint)(point_t *pt,
	    const char *keystr,
	    const mat_t mat,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol);
#define MGEDFUNCTAB_FUNC_KEYPOINT_CAST(_func) ((const char *(*)(point_t *, const char *, const mat_t, const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    void(*ft_e_axes_pos)(
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol);
#define MGEDFUNCTAB_FUNC_E_AXES_POS_CAST(_func) ((void(*)(const struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    void(*ft_write_params)(
	    struct bu_vls *p,
	    const struct rt_db_internal *ip,
	    const struct bn_tol *tol,
	    fastf_t base2local);
#define MGEDFUNCTAB_FUNC_WRITE_PARAMS_CAST(_func) ((void(*)(struct bu_vls *, const struct rt_db_internal *, const struct bn_tol *, fastf_t))((void (*)(void))_func))

    int(*ft_read_params)(
	    struct rt_db_internal *ip,
	    const char *fc,
	    fastf_t local2base);
#define MGEDFUNCTAB_FUNC_READ_PARAMS_CAST(_func) ((int(*)(struct rt_db_internal *, const char *, fastf_t))((void (*)(void))_func))

    struct menu_item *(*ft_menu_item)(const struct bn_tol *tol);
#define MGEDFUNCTAB_FUNC_MENU_ITEM_CAST(_func) ((struct menu_item *(*)(const struct bn_tol *))((void (*)(void))_func))

};
extern const struct mged_functab MGED_OBJ[];

__END_DECLS

#endif  /* MGED_FUNCTAB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
