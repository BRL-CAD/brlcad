/*                    E D F U N C T A B . H
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
#include "rt/functab.h"
#include "rt/db_internal.h"
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

struct rt_solid_edit_map *rt_solid_edit_map_create(void);
void rt_solid_edit_map_destroy(struct rt_solid_edit_map *);
int rt_solid_edit_map_clbk_set(struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d);
int rt_solid_edit_map_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit_map *em, int ed_cmd, int menu_cmd, int mode);
int rt_solid_edit_map_sync(struct rt_solid_edit_map *om, struct rt_solid_edit_map *im);
int rt_solid_edit_clbk_get(bu_clbk_t *f, void **d, struct rt_solid_edit *s, int ed_cmd, int menu_cmd, int mode);


void rt_solid_edit_process(struct rt_solid_edit *s);


void rt_get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, fastf_t *mat);

extern struct rt_solid_edit *
rt_solid_edit_create(struct db_full_path *dfp, struct db_i *dbip, struct bn_tol *, struct bview *v);
extern void
rt_solid_edit_destroy(struct rt_solid_edit *ssed);


const char *
rt_solid_edit_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_solid_edit *s,
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
