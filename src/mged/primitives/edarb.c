/*                         E D A R B . C
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
/** @file mged/primitives/edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "rt/geom.h"
#include "rt/arb_edit.h"
#include "ged.h"
#include "rt/db4.h"

#include "../sedit.h"
#include "../mged.h"
#include "../mged_dm.h"
#include "../cmd.h"
#include "../menu.h"
#include "./edfunctab.h"
#include "./edarb.h"

#define EARB			4009
#define PTARB			4010
#define ECMD_ARB_MAIN_MENU	4011
#define ECMD_ARB_SPECIFIC_MENU	4012
#define ECMD_ARB_MOVE_FACE	4013
#define ECMD_ARB_SETUP_ROTFACE	4014
#define ECMD_ARB_ROTATE_FACE	4015

#define MENU_ARB_MV_EDGE	4036
#define MENU_ARB_MV_FACE	4037
#define MENU_ARB_ROT_FACE	4038

fastf_t es_peqn[7][4]; /* ARBs defining plane equations */

int newedge;

short int fixv;		/* used in ECMD_ARB_ROTATE_FACE, f_eqn(): fixed vertex */

static void
arb8_edge(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 12) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item edge8_menu[] = {
    { "ARB8 EDGES", NULL, 0 },
    { "Move Edge 12", arb8_edge, 0 },
    { "Move Edge 23", arb8_edge, 1 },
    { "Move Edge 34", arb8_edge, 2 },
    { "Move Edge 14", arb8_edge, 3 },
    { "Move Edge 15", arb8_edge, 4 },
    { "Move Edge 26", arb8_edge, 5 },
    { "Move Edge 56", arb8_edge, 6 },
    { "Move Edge 67", arb8_edge, 7 },
    { "Move Edge 78", arb8_edge, 8 },
    { "Move Edge 58", arb8_edge, 9 },
    { "Move Edge 37", arb8_edge, 10 },
    { "Move Edge 48", arb8_edge, 11 },
    { "RETURN",       arb8_edge, 12 },
    { "", NULL, 0 }
};


static void
arb7_edge(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 11) {
	/* move point 5 */
	s->edit_flag = PTARB;
	s->edit_menu = 4;	/* location of point */
    }
    if (arg == 12) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item edge7_menu[] = {
    { "ARB7 EDGES", NULL, 0 },
    { "Move Edge 12", arb7_edge, 0 },
    { "Move Edge 23", arb7_edge, 1 },
    { "Move Edge 34", arb7_edge, 2 },
    { "Move Edge 14", arb7_edge, 3 },
    { "Move Edge 15", arb7_edge, 4 },
    { "Move Edge 26", arb7_edge, 5 },
    { "Move Edge 56", arb7_edge, 6 },
    { "Move Edge 67", arb7_edge, 7 },
    { "Move Edge 37", arb7_edge, 8 },
    { "Move Edge 57", arb7_edge, 9 },
    { "Move Edge 45", arb7_edge, 10 },
    { "Move Point 5", arb7_edge, 11 },
    { "RETURN",       arb7_edge, 12 },
    { "", NULL, 0 }
};

static void
arb6_edge(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
     if (arg == 8) {
	/* move point 5, location = 4 */
	s->edit_flag = PTARB;
	s->edit_menu = 4;
    }
    if (arg == 9) {
	/* move point 6, location = 6 */
	s->edit_flag = PTARB;
	s->edit_menu = 6;
    }
    if (arg == 10) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item edge6_menu[] = {
    { "ARB6 EDGES", NULL, 0 },
    { "Move Edge 12", arb6_edge, 0 },
    { "Move Edge 23", arb6_edge, 1 },
    { "Move Edge 34", arb6_edge, 2 },
    { "Move Edge 14", arb6_edge, 3 },
    { "Move Edge 15", arb6_edge, 4 },
    { "Move Edge 25", arb6_edge, 5 },
    { "Move Edge 36", arb6_edge, 6 },
    { "Move Edge 46", arb6_edge, 7 },
    { "Move Point 5", arb6_edge, 8 },
    { "Move Point 6", arb6_edge, 9 },
    { "RETURN",       arb6_edge, 10 },
    { "", NULL, 0 }
};

static void
arb5_edge(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 8) {
	/* move point 5 at location 4 */
	s->edit_flag = PTARB;
	s->edit_menu = 4;
    }
    if (arg == 9) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item edge5_menu[] = {
    { "ARB5 EDGES", NULL, 0 },
    { "Move Edge 12", arb5_edge, 0 },
    { "Move Edge 23", arb5_edge, 1 },
    { "Move Edge 34", arb5_edge, 2 },
    { "Move Edge 14", arb5_edge, 3 },
    { "Move Edge 15", arb5_edge, 4 },
    { "Move Edge 25", arb5_edge, 5 },
    { "Move Edge 35", arb5_edge, 6 },
    { "Move Edge 45", arb5_edge, 7 },
    { "Move Point 5", arb5_edge, 8 },
    { "RETURN",       arb5_edge, 9 },
    { "", NULL, 0 }
};

static void
arb4_point(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    s->edit_flag = PTARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 5) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item point4_menu[] = {
    { "ARB4 POINTS", NULL, 0 },
    { "Move Point 1", arb4_point, 0 },
    { "Move Point 2", arb4_point, 1 },
    { "Move Point 3", arb4_point, 2 },
    { "Move Point 4", arb4_point, 4 },
    { "RETURN",       arb4_point, 5 },
    { "", NULL, 0 }
};

static void
arb8_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 7) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item mv8_menu[] = {
    { "ARB8 FACES", NULL, 0 },
    { "Move Face 1234", arb8_mv_face, 1 },
    { "Move Face 5678", arb8_mv_face, 2 },
    { "Move Face 1584", arb8_mv_face, 3 },
    { "Move Face 2376", arb8_mv_face, 4 },
    { "Move Face 1265", arb8_mv_face, 5 },
    { "Move Face 4378", arb8_mv_face, 6 },
    { "RETURN",         arb8_mv_face, 7 },
    { "", NULL, 0 }
};

static void
arb7_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 7) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item mv7_menu[] = {
    { "ARB7 FACES", NULL, 0 },
    { "Move Face 1234", arb7_mv_face, 1 },
    { "Move Face 2376", arb7_mv_face, 4 },
    { "RETURN",         arb7_mv_face, 7 },
    { "", NULL, 0 }
};

static void
arb6_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 6) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item mv6_menu[] = {
    { "ARB6 FACES", NULL, 0 },
    { "Move Face 1234", arb6_mv_face, 1 },
    { "Move Face 2365", arb6_mv_face, 2 },
    { "Move Face 1564", arb6_mv_face, 3 },
    { "Move Face 125", arb6_mv_face, 4 },
    { "Move Face 346", arb6_mv_face, 5 },
    { "RETURN",         arb6_mv_face, 6 },
    { "", NULL, 0 }
};

static void
arb5_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 6) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item mv5_menu[] = {
    { "ARB5 FACES", NULL, 0 },
    { "Move Face 1234", arb5_mv_face, 1 },
    { "Move Face 125", arb5_mv_face, 2 },
    { "Move Face 235", arb5_mv_face, 3 },
    { "Move Face 345", arb5_mv_face, 4 },
    { "Move Face 145", arb5_mv_face, 5 },
    { "RETURN",         arb5_mv_face, 6 },
    { "", NULL, 0 }
};

static void
arb4_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 5) {
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);
	sedit(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct menu_item mv4_menu[] = {
    { "ARB4 FACES", NULL, 0 },
    { "Move Face 123", arb4_mv_face, 1 },
    { "Move Face 124", arb4_mv_face, 2 },
    { "Move Face 234", arb4_mv_face, 3 },
    { "Move Face 134", arb4_mv_face, 4 },
    { "RETURN",         arb4_mv_face, 5 },
    { "", NULL, 0 }
};

static void
arb8_rot_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    mged_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 7)
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);

    sedit(s);
}
struct menu_item rot8_menu[] = {
    { "ARB8 FACES", NULL, 0 },
    { "Rotate Face 1234", arb8_rot_face, 1 },
    { "Rotate Face 5678", arb8_rot_face, 2 },
    { "Rotate Face 1584", arb8_rot_face, 3 },
    { "Rotate Face 2376", arb8_rot_face, 4 },
    { "Rotate Face 1265", arb8_rot_face, 5 },
    { "Rotate Face 4378", arb8_rot_face, 6 },
    { "RETURN",         arb8_rot_face, 7 },
    { "", NULL, 0 }
};

static void
arb7_rot_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    mged_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 7)
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);

    sedit(s);
}
struct menu_item rot7_menu[] = {
    { "ARB7 FACES", NULL, 0 },
    { "Rotate Face 1234", arb7_rot_face, 1 },
    { "Rotate Face 567", arb7_rot_face, 2 },
    { "Rotate Face 145", arb7_rot_face, 3 },
    { "Rotate Face 2376", arb7_rot_face, 4 },
    { "Rotate Face 1265", arb7_rot_face, 5 },
    { "Rotate Face 4375", arb7_rot_face, 6 },
    { "RETURN",         arb7_rot_face, 7 },
    { "", NULL, 0 }
};

static void
arb6_rot_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    mged_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 6)
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);

    sedit(s);
}
struct menu_item rot6_menu[] = {
    { "ARB6 FACES", NULL, 0 },
    { "Rotate Face 1234", arb6_rot_face, 1 },
    { "Rotate Face 2365", arb6_rot_face, 2 },
    { "Rotate Face 1564", arb6_rot_face, 3 },
    { "Rotate Face 125", arb6_rot_face, 4 },
    { "Rotate Face 346", arb6_rot_face, 5 },
    { "RETURN",         arb6_rot_face, 6 },
    { "", NULL, 0 }
};

static void
arb5_rot_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    mged_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 6)
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);

    sedit(s);
}

struct menu_item rot5_menu[] = {
    { "ARB5 FACES", NULL, 0 },
    { "Rotate Face 1234", arb5_rot_face, 1 },
    { "Rotate Face 125", arb5_rot_face, 2 },
    { "Rotate Face 235", arb5_rot_face, 3 },
    { "Rotate Face 345", arb5_rot_face, 4 },
    { "Rotate Face 145", arb5_rot_face, 5 },
    { "RETURN",         arb5_rot_face, 6 },
    { "", NULL, 0 }
};

static void
arb4_rot_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg - 1;
    mged_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 5)
	mged_set_edflag(s, ECMD_ARB_MAIN_MENU);

    sedit(s);
}
struct menu_item rot4_menu[] = {
    { "ARB4 FACES", NULL, 0 },
    { "Rotate Face 123", arb4_rot_face, 1 },
    { "Rotate Face 124", arb4_rot_face, 2 },
    { "Rotate Face 234", arb4_rot_face, 3 },
    { "Rotate Face 134", arb4_rot_face, 4 },
    { "RETURN",         arb4_rot_face, 5 },
    { "", NULL, 0 }
};

static void
arb_control(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    mged_set_edflag(s, ECMD_ARB_SPECIFIC_MENU);
    sedit(s);
}
struct menu_item cntrl_menu[] = {
    { "ARB MENU", NULL, 0 },
    { "Move Edges", arb_control, MENU_ARB_MV_EDGE },
    { "Move Faces", arb_control, MENU_ARB_MV_FACE },
    { "Rotate Faces", arb_control, MENU_ARB_ROT_FACE },
    { "", NULL, 0 }
};

struct menu_item *which_menu[] = {
    point4_menu,
    edge5_menu,
    edge6_menu,
    edge7_menu,
    edge8_menu,
    mv4_menu,
    mv5_menu,
    mv6_menu,
    mv7_menu,
    mv8_menu,
    rot4_menu,
    rot5_menu,
    rot6_menu,
    rot7_menu,
    rot8_menu
};

struct menu_item *
mged_arb_menu_item(const struct bn_tol *UNUSED(tol))
{
    return cntrl_menu;
}

int
mged_arb_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct menu_item *mip = NULL;
    struct bu_vls vls2 = BU_VLS_INIT_ZERO;
    int arb_type = rt_arb_std_type(ip, tol);

    /* title */
    bu_vls_printf(mstr, "{{ARB MENU} {}}");

    /* build "move edge" menu */
    mip = which_menu[arb_type-4];
    /* submenu title */
    bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

    bu_vls_printf(mstr, " {{%s} {%s}}", cntrl_menu[1].menu_string, bu_vls_addr(&vls2));
    bu_vls_trunc(&vls2, 0);

    /* build "move face" menu */
    mip = which_menu[arb_type+1];
    /* submenu title */
    bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

    bu_vls_printf(mstr, " {{%s} {%s}}", cntrl_menu[2].menu_string, bu_vls_addr(&vls2));
    bu_vls_trunc(&vls2, 0);

    /* build "rotate face" menu */
    mip = which_menu[arb_type+6];
    /* submenu title */
    bu_vls_printf(&vls2, "{{%s} {}}", mip->menu_string);
    for (++mip; mip->menu_func != NULL; ++mip)
	bu_vls_printf(&vls2, " {{%s} {}}", mip->menu_string);

    bu_vls_printf(mstr, " {{%s} {%s}}", cntrl_menu[3].menu_string, bu_vls_addr(&vls2));
    bu_vls_free(&vls2);

    return BRLCAD_OK;
}

const char *
mged_arb_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol)
{
    if (*keystr == 'V') {
	const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
	return strp;
    }
    static const char *vstr = "V1";
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, tol);
    return strp;
}

void
mged_arb_e_axes_pos(
	struct rt_solid_edit *s,
	const struct rt_db_internal *ip,
       	const struct bn_tol *tol
       	)
{
    int i = 0;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const int local_arb_faces[5][24] = rt_arb_faces;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    int arb_type = rt_arb_std_type(ip, tol);

    switch (s->edit_flag) {
	case EARB:
	    switch (arb_type) {
		case ARB5:
		    i = earb5[s->edit_menu][0];
		    break;
		case ARB6:
		    i = earb6[s->edit_menu][0];
		    break;
		case ARB7:
		    i = earb7[s->edit_menu][0];
		    break;
		case ARB8:
		    i = earb8[s->edit_menu][0];
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case PTARB:
	    switch (arb_type) {
		case ARB4:
		    i = s->edit_menu;	/* index for point 1, 2, 3 or 4 */
		    break;
		case ARB5:
		case ARB7:
		    i = 4;	/* index for point 5 */
		    break;
		case ARB6:
		    i = s->edit_menu;	/* index for point 5 or 6 */
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case ECMD_ARB_MOVE_FACE:
	    switch (arb_type) {
		case ARB4:
		    i = local_arb_faces[0][s->edit_menu * 4];
		    break;
		case ARB5:
		    i = local_arb_faces[1][s->edit_menu * 4];
		    break;
		case ARB6:
		    i = local_arb_faces[2][s->edit_menu * 4];
		    break;
		case ARB7:
		    i = local_arb_faces[3][s->edit_menu * 4];
		    break;
		case ARB8:
		    i = local_arb_faces[4][s->edit_menu * 4];
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case ECMD_ARB_ROTATE_FACE:
	    i = fixv;
	    break;
	default:
	    i = 0;
	    break;
    }
    MAT4X3PNT(s->curr_e_axes_pos, s->e_mat, arb->pt[i]);
}

/*
 * given the index of a vertex of the arb currently being edited,
 * return 1 if this vertex should appear in the editor
 * return 0 if this vertex is a duplicate of one of the above
 */
static int
useThisVertex(int idx, int *uvec, int *svec)
{
    int i;

    for (i=0; i<8 && uvec[i] != -1; i++)
	if (uvec[i] == idx)
	    return 1;

    if (svec[0] != 0 && idx == svec[2])
       	return 1;

    if (svec[1] != 0 && idx == svec[2+svec[0]])
       	return 1;

    return 0;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_arb_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *tol,
	fastf_t base2local)
{
    // TODO - these really should be stashed in a private arb editing struct so
    // they can persist until the read_params call...
    static int uvec[8];
    static int svec[11];
    static int cgtype = 8;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    for (int i=0; i<8; i++) uvec[i] = -1;
    rt_arb_get_cgtype(&cgtype, arb, tol, uvec, svec);
    int j = 0;
    for (int i=0; i<8; i++) {
	if (useThisVertex(i, uvec, svec)) {
	    j++;
	    bu_vls_printf(p, "pt[%d]: %.9f %.9f %.9f\n", j, V3BASE2LOCAL(arb->pt[i]));
	}
    }
}

#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
mged_arb_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *tol,
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    static int uvec[8];
    static int svec[11];
    static int cgtype = 8;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    RT_ARB_CK_MAGIC(arb);
    rt_arb_get_cgtype(&cgtype, arb, tol, uvec, svec);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (pt[0])
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;


    for (int i=0; i<8; i++) {
	/* only read vertices that we wrote */
	if (useThisVertex(i, uvec, svec)) {
	    if (i != 0) {
		// Above sets up initial line - otherwise,
		// we need to stage the next one.
		read_params_line_incr;
	    }
	    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
	    VSET(arb->pt[i], a, b, c);
	    VSCALE(arb->pt[i], arb->pt[i], local2base);
	}
    }

    /* fill in the duplicate vertices (based on rt_arb_get_cgtype call) */
    if (svec[0] != -1) {
	for (int i=1; i<svec[0]; i++) {
	    int start = 2;
	    VMOVE(arb->pt[svec[start+i]], arb->pt[svec[start]]);
	}
    }
    if (svec[1] != -1) {
	int start = 2 + svec[0];
	for (int i=1; i<svec[1]; i++) {
	    VMOVE(arb->pt[svec[start+i]], arb->pt[svec[start]]);
	}
    }

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/*
 * An ARB edge is moved by finding the direction of the line
 * containing the edge and the 2 "bounding" planes.  The new edge is
 * found by intersecting the new line location with the bounding
 * planes.  The two "new" planes thus defined are calculated and the
 * affected points are calculated by intersecting planes.  This keeps
 * ALL faces planar.
 *
 */
int
editarb(struct rt_solid_edit *s, vect_t pos_model)
{
    int ret = 0;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, es_peqn, s->tol)) {
	bu_vls_printf(s->log_str, "\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);

    ret = arb_edit(arb, es_peqn, s->edit_menu, newedge, pos_model, s->tol);

    // arb_edit doesn't zero out our global any more as a library call, so
    // reset once operation is complete.
    newedge = 0;

    if (ret) {
	mged_set_edflag(s, IDLE);
    }

    return ret;
}

void
ecmd_arb_main_menu(struct rt_solid_edit *s)
{
    /* put up control (main) menu for GENARB8s */
    mged_set_edflag(s, IDLE);
    bu_clbk_t f = NULL;
    void *d = NULL;
    mged_sedit_clbk_get(&f, &d, s, ECMD_MENU_SET, 0, GED_CLBK_DURING);
    if (!f)
	return;
    (*f)(0, NULL, d, cntrl_menu);
}

int
ecmd_arb_specific_menu(struct rt_solid_edit *s)
{
    /* put up specific arb edit menus */
    bu_clbk_t f = NULL;
    void *d = NULL;
    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    mged_set_edflag(s, IDLE);
    switch (s->edit_menu) {
	case MENU_ARB_MV_EDGE:
	    mged_sedit_clbk_get(&f, &d, s, ECMD_MENU_SET, 0, GED_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type-4]);
	    return BRLCAD_OK;
	case MENU_ARB_MV_FACE:
	    mged_sedit_clbk_get(&f, &d, s, ECMD_MENU_SET, 0, GED_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type+1]);
	    return BRLCAD_OK;
	case MENU_ARB_ROT_FACE:
	    mged_sedit_clbk_get(&f, &d, s, ECMD_MENU_SET, 0, GED_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type+6]);
	    return BRLCAD_OK;
	default:
	    bu_vls_printf(s->log_str, "Bad menu item.\n");
	    mged_sedit_clbk_get(&f, &d, s, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }
}

int
ecmd_arb_move_face(struct rt_solid_edit *s)
{
    /* move face through definite point */
    if (s->e_inpara) {

	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return TCL_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	vect_t work;
	struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
	RT_ARB_CK_MAGIC(arb);

	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(work, s->e_invmat, s->e_para);
	} else {
	    VMOVE(work, s->e_para);
	}
	/* change D of planar equation */
	es_peqn[s->edit_menu][W]=VDOT(&es_peqn[s->edit_menu][0], work);
	/* find new vertices, put in record in vector notation */

	int arb_type = rt_arb_std_type(&s->es_int, s->tol);

	(void)rt_arb_calc_points(arb, arb_type, (const plane_t *)es_peqn, s->tol);
    }

    return 0;
}

void
ecmd_arb_setup_rotface(struct rt_solid_edit *s)
{
    bu_clbk_t f = NULL;
    void *d = NULL;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    mged_sedit_clbk_get(&f, &d, s, ECMD_ARB_SETUP_ROTFACE, 0, GED_CLBK_DURING);
    if (f)
	fixv = (*f)(0, NULL, d, s);

    fixv--;
    s->edit_flag = ECMD_ARB_ROTATE_FACE;
    s->solid_edit_rotate = 1;
    s->solid_edit_translate = 0;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;

    /* draw arrow, etc. */
    int vs_flag = 1;
    f = NULL; d = NULL;
    mged_sedit_clbk_get(&f, &d, s, ECMD_VIEW_SET_FLAG, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &vs_flag);

    /* eaxes */
    int e_flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &e_flag);
}

int
ecmd_arb_rotate_face(struct rt_solid_edit *s)
{
    /* rotate a GENARB8 defining plane through a fixed vertex */
    fastf_t *eqp;
    bu_clbk_t f = NULL;
    void *d = NULL;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (s->e_inpara) {

	vect_t work;
	static mat_t invsolr;
	static vect_t tempvec;
	static float rota, fb_a;

	/*
	 * Keyboard parameters in degrees.
	 * First, cancel any existing rotations,
	 * then perform new rotation
	 */
	bn_mat_inv(invsolr, s->acc_rot_sol);
	eqp = &es_peqn[s->edit_menu][0];	/* s->edit_menu==plane of interest */
	VMOVE(work, eqp);
	MAT4X3VEC(eqp, invsolr, work);

	if (s->e_inpara == 3) {
	    /* 3 params:  absolute X, Y, Z rotations */
	    /* Build completely new rotation change */
	    MAT_IDN(s->model_changes);
	    bn_mat_angles(s->model_changes,
		    s->e_para[0],
		    s->e_para[1],
		    s->e_para[2]);
	    MAT_COPY(s->acc_rot_sol, s->model_changes);

	    /* Borrow s->incr_change matrix here */
	    bn_mat_mul(s->incr_change, s->model_changes, invsolr);
	    if (s->mv_context) {
		/* calculate rotations about keypoint */
		mat_t edit;
		bn_mat_xform_about_pnt(edit, s->incr_change, s->e_keypoint);

		/* We want our final matrix (mat) to xform the original solid
		 * to the position of this instance of the solid, perform the
		 * current edit operations, then xform back.
		 * mat = s->e_invmat * edit * s->e_mat
		 */
		mat_t mat, mat1;
		bn_mat_mul(mat1, edit, s->e_mat);
		bn_mat_mul(mat, s->e_invmat, mat1);
		MAT_IDN(s->incr_change);
		/* work contains original es_peqn[s->edit_menu][0] */
		MAT4X3VEC(eqp, mat, work);
	    } else {
		VMOVE(work, eqp);
		MAT4X3VEC(eqp, s->model_changes, work);
	    }
	} else if (s->e_inpara == 2) {
	    /* 2 parameters:  rot, fb were given */
	    rota= s->e_para[0] * DEG2RAD;
	    fb_a  = s->e_para[1] * DEG2RAD;

	    /* calculate normal vector (length = 1) from rot, struct fb */
	    es_peqn[s->edit_menu][0] = cos(fb_a) * cos(rota);
	    es_peqn[s->edit_menu][1] = cos(fb_a) * sin(rota);
	    es_peqn[s->edit_menu][2] = sin(fb_a);
	} else {
	    bu_vls_printf(s->log_str, "Must be < rot fb | xdeg ydeg zdeg >\n");
	    mged_sedit_clbk_get(&f, &d, s, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return TCL_ERROR;
	}

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* s->edit_menu == plane of interest */
	es_peqn[s->edit_menu][W]=VDOT(eqp, tempvec);

	/* Clear out solid rotation */
	MAT_IDN(s->model_changes);

    } else {
	/* Apply incremental changes */
	static vect_t tempvec;
	vect_t work;

	eqp = &es_peqn[s->edit_menu][0];
	VMOVE(work, eqp);
	MAT4X3VEC(eqp, s->incr_change, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* s->edit_menu == plane of interest */
	es_peqn[s->edit_menu][W]=VDOT(eqp, tempvec);
    }

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)es_peqn, s->tol);
    MAT_IDN(s->incr_change);

    /* no need to calc_planes again */
    replot_editing_solid(0, NULL, s, NULL);

    s->e_inpara = 0;

    return 0;
}

int
edit_arb_element(struct rt_solid_edit *s)
{

    if (s->e_inpara) {

	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return TCL_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;

	vect_t work;
	if (s->mv_context) {
	    /* apply s->e_invmat to convert to real model space */
	    MAT4X3PNT(work, s->e_invmat, s->e_para);
	} else {
	    VMOVE(work, s->e_para);
	}
	editarb(s, work);
    }

    return 0;
}

void
arb_mv_pnt_to(struct rt_solid_edit *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    /* move an arb point to indicated point */
    /* point is located at es_values[s->edit_menu*3] */
    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, s->e_invmat, temp);
    editarb(s, pos_model);
}

void
edarb_mousevec(struct rt_solid_edit *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, s->e_invmat, temp);
    editarb(s, pos_model);
}

void
edarb_move_face_mousevec(struct rt_solid_edit *s, const vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, s->e_invmat, temp);
    /* change D of planar equation */
    es_peqn[s->edit_menu][W]=VDOT(&es_peqn[s->edit_menu][0], pos_model);
    /* calculate new vertices, put in record as vectors */
    {
	struct rt_arb_internal *arb=
	    (struct rt_arb_internal *)s->es_int.idb_ptr;

	RT_ARB_CK_MAGIC(arb);

	int arb_type = rt_arb_std_type(&s->es_int, s->tol);

	(void)rt_arb_calc_points(arb, arb_type, (const plane_t *)es_peqn, s->tol);
    }
}

int
mged_arb_edit(struct rt_solid_edit *s, int edflag)
{
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);
    int ret = 0;

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, es_peqn, s->tol)) {
	bu_vls_printf(s->log_str, "\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return TCL_ERROR;
    }
    bu_vls_free(&error_msg);


    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    ret = mged_generic_sscale(s, &s->es_int);
	    goto arb_planecalc;
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->es_int);
	    break;
	case ECMD_ARB_MAIN_MENU:
	    ecmd_arb_main_menu(s);
	    break;
	case ECMD_ARB_SPECIFIC_MENU:
	    if (ecmd_arb_specific_menu(s) != BRLCAD_OK)
		return -1;
	    break;
	case ECMD_ARB_MOVE_FACE:
	    return ecmd_arb_move_face(s);
	case ECMD_ARB_SETUP_ROTFACE:
	    ecmd_arb_setup_rotface(s);
	    break;
	case ECMD_ARB_ROTATE_FACE:
	    ret = ecmd_arb_rotate_face(s);
	    if (ret)
		return ret;
	    return 1; // TODO - why is this a return rather than a break (skips sedit finalization)
	case PTARB:     /* move an ARB point */
	case EARB:      /* edit an ARB edge */
	    return edit_arb_element(s);
    }

arb_planecalc:

    /* must re-calculate the face plane equations for arbs */
    arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, es_peqn, s->tol) < 0)
	bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&error_msg));
    bu_vls_free(&error_msg);

    return ret;
}

int
mged_arb_edit_xy(
	struct rt_solid_edit *s,
	int edflag,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (edflag) {
	case SSCALE:
	case PSCALE:
	    mged_generic_sscale_xy(s, mousevec);
	    mged_arb_edit(s, edflag);
	    return 0;
	case STRANS:
	    mged_generic_strans_xy(&pos_view, s, mousevec);
	    break;
	case PTARB:
	    arb_mv_pnt_to(s, mousevec);
	    break;
	case EARB:
	    edarb_mousevec(s, mousevec);
	    break;
	case ECMD_ARB_MOVE_FACE:
	    edarb_move_face_mousevec(s, mousevec);
	    break;
	default:
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, edflag);
	    mged_sedit_clbk_get(&f, &d, s, ECMD_PRINT_RESULTS, 0, GED_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return TCL_ERROR;
    }

    update_edit_absolute_tran(s, pos_view);
    mged_arb_edit(s, edflag);

    return 0;
}

int
arb_f_eqn(struct rt_solid_edit *s, int argc, const char **argv)
{
    short int i;
    vect_t tempvec;
    struct rt_arb_internal *arb;

    if (argc < 4 || 4 < argc)
	return TCL_ERROR;

    if (s->es_int.idb_type != ID_ARB8) {
	bu_vls_printf(s->log_str, "Eqn: type must be GENARB8\n");
	return TCL_ERROR;
    }

    if (s->edit_flag != ECMD_ARB_ROTATE_FACE) {
	bu_vls_printf(s->log_str, "Eqn: must be rotating a face\n");
	return TCL_ERROR;
    }

    arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* get the A, B, C from the command line */
    for (i=0; i<3; i++)
	es_peqn[s->edit_menu][i]= atof(argv[i+1]);
    VUNITIZE(&es_peqn[s->edit_menu][0]);

    VMOVE(tempvec, arb->pt[fixv]);
    es_peqn[s->edit_menu][W]=VDOT(es_peqn[s->edit_menu], tempvec);

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_points(arb, arb_type, (const plane_t *)es_peqn, s->tol))
	return CMD_BAD;

    return TCL_OK;
}

int
arb_edgedir(struct rt_solid_edit *s, int argc, const char **argv)
{
    vect_t slope;
    fastf_t rot, fb_a;

    if (!s || !argc || !argv)
	return TCL_ERROR;

    if (s->edit_flag != EARB) {
	bu_vls_printf(s->log_str, "Not moving an ARB edge\n");
	return TCL_ERROR;
    }

    if (s->es_int.idb_type != ID_ARB8) {
	bu_vls_printf(s->log_str, "Edgedir: solid type must be an ARB\n");
	return TCL_ERROR;
    }

    /* set up slope -
     * if 2 values input assume rot, fb used
     * else assume delta_x, delta_y, delta_z
     */
    if (argc == 3) {
	rot = atof(argv[1]) * DEG2RAD;
	fb_a = atof(argv[2]) * DEG2RAD;
	slope[0] = cos(fb_a) * cos(rot);
	slope[1] = cos(fb_a) * sin(rot);
	slope[2] = sin(fb_a);
    } else {
	for (int i=0; i<3; i++) {
	    /* put edge slope in slope[] array */
	    slope[i] = atof(argv[i+1]);
	}
    }

    if (ZERO(MAGNITUDE(slope))) {
	bu_vls_printf(s->log_str, "BAD slope\n");
	return TCL_ERROR;
    }

    /* get it done */
    newedge = 1;
    editarb(s, slope);
    sedit(s);

    return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
