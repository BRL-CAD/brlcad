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
/** @file primitives/edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/str.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "rt/db4.h"
#include "../edit_private.h"

#define EARB			4009
#define PTARB			4010
#define ECMD_ARB_MAIN_MENU	4011
#define ECMD_ARB_SPECIFIC_MENU	4012
#define ECMD_ARB_MOVE_FACE	4013
#define ECMD_ARB_SETUP_ROTFACE	4014
#define ECMD_ARB_ROTATE_FACE	4015

#define ECMD_ARB_MV_EDGE	4036
#define ECMD_ARB_MV_FACE	4037
#define ECMD_ARB_ROT_FACE	4038

void *
rt_solid_edit_arb_prim_edit_create(struct rt_solid_edit *UNUSED(s))
{
    struct rt_arb8_edit *a;
    BU_GET(a, struct rt_arb8_edit);
    a->newedge = 0;
    return (void *)a;
}

void
rt_solid_edit_arb_prim_edit_destroy(struct rt_arb8_edit *a)
{
    if (!a)
	return;
    BU_PUT(a, struct rt_arb8_edit);
}


static void
arb8_edge(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 12) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item edge8_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 11) {
	/* move point 5 */
	s->edit_flag = PTARB;
	aint->edit_menu = 4;	/* location of point */
    }
    if (arg == 12) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item edge7_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
     if (arg == 8) {
	/* move point 5, location = 4 */
	s->edit_flag = PTARB;
	aint->edit_menu = 4;
    }
    if (arg == 9) {
	/* move point 6, location = 6 */
	s->edit_flag = PTARB;
	aint->edit_menu = 6;
    }
    if (arg == 10) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }
    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item edge6_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    s->edit_flag = EARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 8) {
	/* move point 5 at location 4 */
	s->edit_flag = PTARB;
	aint->edit_menu = 4;
    }
    if (arg == 9) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item edge5_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    s->edit_flag = PTARB;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 5) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item point4_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 7) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item mv8_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 7) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item mv7_menu[] = {
    { "ARB7 FACES", NULL, 0 },
    { "Move Face 1234", arb7_mv_face, 1 },
    { "Move Face 2376", arb7_mv_face, 4 },
    { "RETURN",         arb7_mv_face, 7 },
    { "", NULL, 0 }
};

static void
arb6_mv_face(struct rt_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 6) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item mv6_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 6) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item mv5_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    s->edit_flag = ECMD_ARB_MOVE_FACE;
    s->solid_edit_rotate = 0;
    s->solid_edit_translate = 1;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;
    if (arg == 5) {
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);
	rt_solid_edit_process(s);
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}
struct rt_solid_edit_menu_item mv4_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 7)
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item rot8_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 7)
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item rot7_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 6)
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item rot6_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 6)
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);

    rt_solid_edit_process(s);
}

struct rt_solid_edit_menu_item rot5_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg - 1;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SETUP_ROTFACE);
    if (arg == 5)
	rt_solid_edit_set_edflag(s, ECMD_ARB_MAIN_MENU);

    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item rot4_menu[] = {
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
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;
    aint->edit_menu = arg;
    rt_solid_edit_set_edflag(s, ECMD_ARB_SPECIFIC_MENU);
    rt_solid_edit_process(s);
}
struct rt_solid_edit_menu_item cntrl_menu[] = {
    { "ARB MENU", NULL, 0 },
    { "Move Edges", arb_control, ECMD_ARB_MV_EDGE },
    { "Move Faces", arb_control, ECMD_ARB_MV_FACE },
    { "Rotate Faces", arb_control, ECMD_ARB_ROT_FACE },
    { "", NULL, 0 }
};

struct rt_solid_edit_menu_item *which_menu[] = {
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

struct rt_solid_edit_menu_item *
rt_solid_edit_arb_menu_item(const struct bn_tol *UNUSED(tol))
{
    return cntrl_menu;
}

int
rt_solid_edit_arb_menu_str(struct bu_vls *mstr, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    if (!mstr || !ip)
	return BRLCAD_ERROR;

    struct rt_solid_edit_menu_item *mip = NULL;
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
rt_solid_edit_arb_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_solid_edit *s,
	const struct bn_tol *tol)
{
    struct rt_db_internal *ip = &s->es_int;
    if (*keystr == 'V') {
	const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, keystr, mat, ip, tol);
	return strp;
    }
    static const char *vstr = "V1";
    const char *strp = OBJ[ip->idb_type].ft_keypoint(pt, vstr, mat, ip, tol);
    return strp;
}

void
rt_solid_edit_arb_e_axes_pos(
	struct rt_solid_edit *s,
	const struct rt_db_internal *ip,
       	const struct bn_tol *tol
       	)
{
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
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
		    i = earb5[a->edit_menu][0];
		    break;
		case ARB6:
		    i = earb6[a->edit_menu][0];
		    break;
		case ARB7:
		    i = earb7[a->edit_menu][0];
		    break;
		case ARB8:
		    i = earb8[a->edit_menu][0];
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case PTARB:
	    switch (arb_type) {
		case ARB4:
		    i = a->edit_menu;	/* index for point 1, 2, 3 or 4 */
		    break;
		case ARB5:
		case ARB7:
		    i = 4;	/* index for point 5 */
		    break;
		case ARB6:
		    i = a->edit_menu;	/* index for point 5 or 6 */
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case ECMD_ARB_MOVE_FACE:
	    switch (arb_type) {
		case ARB4:
		    i = local_arb_faces[0][a->edit_menu * 4];
		    break;
		case ARB5:
		    i = local_arb_faces[1][a->edit_menu * 4];
		    break;
		case ARB6:
		    i = local_arb_faces[2][a->edit_menu * 4];
		    break;
		case ARB7:
		    i = local_arb_faces[3][a->edit_menu * 4];
		    break;
		case ARB8:
		    i = local_arb_faces[4][a->edit_menu * 4];
		    break;
		default:
		    i = 0;
		    break;
	    }
	    break;
	case ECMD_ARB_ROTATE_FACE:
	    i = a->fixv;
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
rt_solid_edit_arb_write_params(
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
rt_solid_edit_arb_read_params(
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
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, a->es_peqn, s->tol)) {
	bu_vls_printf(s->log_str, "\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&error_msg);

    ret = arb_edit(arb, a->es_peqn, a->edit_menu, a->newedge, pos_model, s->tol);

    // arb_edit doesn't zero out our global any more as a library call, so
    // reset once operation is complete.
    a->newedge = 0;

    if (ret) {
	rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_IDLE);
    }

    return ret;
}

void
ecmd_arb_main_menu(struct rt_solid_edit *s)
{
    /* put up control (main) menu for GENARB8s */
    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_IDLE);
    bu_clbk_t f = NULL;
    void *d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_MENU_SET, 0, BU_CLBK_DURING);
    if (!f)
	return;
    (*f)(0, NULL, d, cntrl_menu);
}

int
ecmd_arb_specific_menu(struct rt_solid_edit *s)
{
    struct rt_arb8_edit *aint = (struct rt_arb8_edit *)s->ipe_ptr;

    /* put up specific arb edit menus */
    bu_clbk_t f = NULL;
    void *d = NULL;
    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    rt_solid_edit_set_edflag(s, RT_SOLID_EDIT_IDLE);
    switch (aint->edit_menu) {
	case ECMD_ARB_MV_EDGE:
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_MENU_SET, 0, BU_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type-4]);
	    return BRLCAD_OK;
	case ECMD_ARB_MV_FACE:
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_MENU_SET, 0, BU_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type+1]);
	    return BRLCAD_OK;
	case ECMD_ARB_ROT_FACE:
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_MENU_SET, 0, BU_CLBK_DURING);
	    if (!f)
		return BRLCAD_ERROR;
	    (*f)(0, NULL, d, which_menu[arb_type+6]);
	    return BRLCAD_OK;
	default:
	    bu_vls_printf(s->log_str, "Bad menu item.\n");
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }
}

int
ecmd_arb_move_face(struct rt_solid_edit *s)
{
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;

    /* move face through definite point */
    if (s->e_inpara) {

	if (s->e_inpara != 3) {
	    bu_vls_printf(s->log_str, "ERROR: three arguments needed\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
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
	a->es_peqn[a->edit_menu][W]=VDOT(&a->es_peqn[a->edit_menu][0], work);
	/* find new vertices, put in record in vector notation */

	int arb_type = rt_arb_std_type(&s->es_int, s->tol);

	(void)rt_arb_calc_points(arb, arb_type, (const plane_t *)a->es_peqn, s->tol);
    }

    return 0;
}

void
ecmd_arb_setup_rotface(struct rt_solid_edit *s)
{
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_ARB_SETUP_ROTFACE, 0, BU_CLBK_DURING);
    if (f)
	a->fixv = (*f)(0, NULL, d, s);

    a->fixv--;
    s->edit_flag = ECMD_ARB_ROTATE_FACE;
    s->solid_edit_rotate = 1;
    s->solid_edit_translate = 0;
    s->solid_edit_scale = 0;
    s->solid_edit_pick = 0;

    /* draw arrow, etc. */
    int vs_flag = 1;
    f = NULL; d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_VIEW_SET_FLAG, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &vs_flag);

    /* eaxes */
    int e_flag = 1;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &e_flag);
}

int
ecmd_arb_rotate_face(struct rt_solid_edit *s)
{
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;

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
	eqp = &a->es_peqn[a->edit_menu][0];	/* a->edit_menu==plane of interest */
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
		/* work contains original a->es_peqn[a->edit_menu][0] */
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
	    a->es_peqn[a->edit_menu][0] = cos(fb_a) * cos(rota);
	    a->es_peqn[a->edit_menu][1] = cos(fb_a) * sin(rota);
	    a->es_peqn[a->edit_menu][2] = sin(fb_a);
	} else {
	    bu_vls_printf(s->log_str, "Must be < rot fb | xdeg ydeg zdeg >\n");
	    f = NULL; d = NULL;
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
	}

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[a->fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* a->edit_menu == plane of interest */
	a->es_peqn[a->edit_menu][W]=VDOT(eqp, tempvec);

	/* Clear out solid rotation */
	MAT_IDN(s->model_changes);

    } else {
	/* Apply incremental changes */
	static vect_t tempvec;
	vect_t work;

	eqp = &a->es_peqn[a->edit_menu][0];
	VMOVE(work, eqp);
	MAT4X3VEC(eqp, s->incr_change, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[a->fixv]);

	/* set D of planar equation to anchor at fixed vertex */
	/* a->edit_menu == plane of interest */
	a->es_peqn[a->edit_menu][W]=VDOT(eqp, tempvec);
    }

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);

    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)a->es_peqn, s->tol);
    MAT_IDN(s->incr_change);

    /* no need to calc_planes again */
    f = NULL; d = NULL;
    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_REPLOT_EDITING_SOLID, 0, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, NULL);

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
	    return BRLCAD_ERROR;
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
    /* point is located at es_values[a->edit_menu*3] */
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
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
    vect_t pos_view = VINIT_ZERO;	/* Unrotated view space pos */
    vect_t temp = VINIT_ZERO;
    vect_t pos_model = VINIT_ZERO;	/* Rotated screen space pos */
    MAT4X3PNT(pos_view, s->vp->gv_model2view, s->curr_e_axes_pos);
    pos_view[X] = mousevec[X];
    pos_view[Y] = mousevec[Y];
    MAT4X3PNT(temp, s->vp->gv_view2model, pos_view);
    MAT4X3PNT(pos_model, s->e_invmat, temp);
    /* change D of planar equation */
    a->es_peqn[a->edit_menu][W]=VDOT(&a->es_peqn[a->edit_menu][0], pos_model);
    /* calculate new vertices, put in record as vectors */
    {
	struct rt_arb_internal *arb=
	    (struct rt_arb_internal *)s->es_int.idb_ptr;

	RT_ARB_CK_MAGIC(arb);

	int arb_type = rt_arb_std_type(&s->es_int, s->tol);

	(void)rt_arb_calc_points(arb, arb_type, (const plane_t *)a->es_peqn, s->tol);
    }
}

int
rt_solid_edit_arb_edit(struct rt_solid_edit *s)
{
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;
    RT_ARB_CK_MAGIC(arb);
    int ret = 0;

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, a->es_peqn, s->tol)) {
	bu_vls_printf(s->log_str, "\nCannot calculate plane equations for ARB8\n");
	bu_vls_free(&error_msg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&error_msg);


    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    ret = rt_solid_edit_generic_sscale(s, &s->es_int);
	    goto arb_planecalc;
	case RT_SOLID_EDIT_TRANS:
	    /* translate solid */
	    rt_solid_edit_generic_strans(s, &s->es_int);
	    break;
	case RT_SOLID_EDIT_ROT:
	    /* rot solid about vertex */
	    rt_solid_edit_generic_srot(s, &s->es_int);
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
	    return 1; // TODO - why is this a return rather than a break (skips rt_solid_edit_process finalization)
	case PTARB:     /* move an ARB point */
	case EARB:      /* edit an ARB edge */
	    return edit_arb_element(s);
    }

arb_planecalc:

    /* must re-calculate the face plane equations for arbs */
    arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_planes(&error_msg, arb, arb_type, a->es_peqn, s->tol) < 0)
	bu_vls_printf(s->log_str, "%s", bu_vls_cstr(&error_msg));
    bu_vls_free(&error_msg);

    return ret;
}

int
rt_solid_edit_arb_edit_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
	case RT_SOLID_EDIT_SCALE:
	case RT_SOLID_EDIT_PSCALE:
	    rt_solid_edit_generic_sscale_xy(s, mousevec);
	    rt_solid_edit_arb_edit(s);
	    return 0;
	case RT_SOLID_EDIT_TRANS:
	    rt_solid_edit_generic_strans_xy(&pos_view, s, mousevec);
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
	    bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
	    rt_solid_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, 0, BU_CLBK_DURING);
	    if (f)
		(*f)(0, NULL, d, NULL);
	    return BRLCAD_ERROR;
    }

    rt_update_edit_absolute_tran(s, pos_view);
    rt_solid_edit_arb_edit(s);

    return 0;
}

int
rt_arb_f_eqn(struct rt_solid_edit *s, int argc, const char **argv)
{
    short int i;
    vect_t tempvec;
    struct rt_arb_internal *arb;
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;

    if (argc < 4 || 4 < argc)
	return BRLCAD_ERROR;

    if (s->es_int.idb_type != ID_ARB8) {
	bu_vls_printf(s->log_str, "Eqn: type must be GENARB8\n");
	return BRLCAD_ERROR;
    }

    if (s->edit_flag != ECMD_ARB_ROTATE_FACE) {
	bu_vls_printf(s->log_str, "Eqn: must be rotating a face\n");
	return BRLCAD_ERROR;
    }

    arb = (struct rt_arb_internal *)s->es_int.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* get the A, B, C from the command line */
    for (i=0; i<3; i++)
	a->es_peqn[a->edit_menu][i]= atof(argv[i+1]);
    VUNITIZE(&a->es_peqn[a->edit_menu][0]);

    VMOVE(tempvec, arb->pt[a->fixv]);
    a->es_peqn[a->edit_menu][W]=VDOT(a->es_peqn[a->edit_menu], tempvec);

    int arb_type = rt_arb_std_type(&s->es_int, s->tol);
    if (rt_arb_calc_points(arb, arb_type, (const plane_t *)a->es_peqn, s->tol))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
rt_arb_edgedir(struct rt_solid_edit *s, int argc, const char **argv)
{
    vect_t slope;
    fastf_t rot, fb_a;
    struct rt_arb8_edit *a = (struct rt_arb8_edit *)s->ipe_ptr;

    if (!s || !argc || !argv)
	return BRLCAD_ERROR;

    if (s->edit_flag != EARB) {
	bu_vls_printf(s->log_str, "Not moving an ARB edge\n");
	return BRLCAD_ERROR;
    }

    if (s->es_int.idb_type != ID_ARB8) {
	bu_vls_printf(s->log_str, "Edgedir: solid type must be an ARB\n");
	return BRLCAD_ERROR;
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
	return BRLCAD_ERROR;
    }

    /* get it done */
    a->newedge = 1;
    editarb(s, slope);
    rt_solid_edit_process(s);

    return BRLCAD_OK;
}

void
ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb, fastf_t peqn[7][4])
{
    point_t pts[8];
    int i;

    VMOVE(pts[0], arb->pt[pt1]);
    VMOVE(pts[1], arb->pt[pt2]);
    VMOVE(pts[4], arb->pt[pt3]);
    VMOVE(pts[5], arb->pt[pt3]);

    /* extrude "distance" to get remaining points */
    VADD2(pts[2], pts[1], &peqn[6][0]);
    VADD2(pts[3], pts[0], &peqn[6][0]);
    VADD2(pts[6], pts[4], &peqn[6][0]);
    VMOVE(pts[7], pts[6]);

    /* copy to the original record */
    for (i=0; i<8; i++)
	VMOVE(arb->pt[i], pts[i]);
}

int
mv_edge(struct rt_arb_internal *arb,
	const vect_t thru,
	const int bp1, const int bp2,
	const int end1, const int end2,
	const vect_t dir,
	const struct bn_tol *tol,
	fastf_t peqn[7][4])
{
    fastf_t t1, t2;

    if (bg_isect_line3_plane(&t1, thru, dir, peqn[bp1], tol) < 0 ||
	bg_isect_line3_plane(&t2, thru, dir, peqn[bp2], tol) < 0) {
	return 1;
    }

    VJOIN1(arb->pt[end1], thru, t1, dir);
    VJOIN1(arb->pt[end2], thru, t2, dir);

    return 0;
}

int
arb_extrude(struct rt_arb_internal *arb,
	int face, fastf_t dist,
	const struct bn_tol *tol,
	fastf_t peqn[7][4])
{
    int type, prod, i, j;
    int uvec[8], svec[11];
    int pt[4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    struct rt_arb_internal larb;	/* local copy of arb for new way */
    RT_ARB_CK_MAGIC(arb);

    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;
    if (type != 8 && type != 6 && type != 4) {return 1;}

    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    if ((type == ARB6 || type == ARB4) && face < 1000) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    } else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	     10	         125 for ARB6
     *	     72	         346 for ARB6
     * --- special case to make ARB6 from ARB4
     * ---   provides easy way to build ARB6's
     *        6	         123 for ARB4
     *	      8	         124 for ARB4
     *	     12	         134 for ARB4
     *	     24	         234 for ARB4
     */
    prod = 1;
    for (i = 0; i <= 3; i++) {
	prod *= pt[i];
	if (type == ARB6 && pt[i] == 6)
	    pt[i]++;
	if (type == ARB4 && pt[i] == 4)
	    pt[i]++;
	pt[i]--;
	if (pt[i] > 7) {
	    return 1;
	}
    }

    /* find plane containing this face */
    if (bg_make_plane_3pnts(peqn[6], larb.pt[pt[0]], larb.pt[pt[1]],
			 larb.pt[pt[2]], tol)) {
	return 1;
    }

    /* get normal vector of length == dist */
    for (i = 0; i < 3; i++)
	peqn[6][i] *= dist;

    /* protrude the selected face */
    switch (prod) {

	case 24:   /* protrude face 1234 */
	    if (type == ARB6) {
		return 1;
	    }
	    if (type == ARB4)
		goto a4toa6;	/* extrude face 234 of ARB4 to make ARB6 */

	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VADD2(larb.pt[j], larb.pt[i], peqn[6]);
	    }
	    break;

	case 6:		/* extrude ARB4 face 123 to make ARB6 */
	case 8:		/* extrude ARB4 face 124 to make ARB6 */
	case 12:	/* extrude ARB4 face 134 to Make ARB6 */
    a4toa6:
	    ext4to6(pt[0], pt[1], pt[2], &larb, peqn);
	    type = ARB6;
	    /* TODO - solid edit menu was called here in MGED - why? */
	    break;

	case 1680:   /* protrude face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VADD2(larb.pt[i], larb.pt[j], peqn[6]);
	    }
	    break;

	case 60:   /* protrude face 1256 */
	case 10:   /* extrude face 125 of ARB6 */
	    VADD2(larb.pt[3], larb.pt[0], peqn[6]);
	    VADD2(larb.pt[2], larb.pt[1], peqn[6]);
	    VADD2(larb.pt[7], larb.pt[4], peqn[6]);
	    VADD2(larb.pt[6], larb.pt[5], peqn[6]);
	    break;

	case 672:	/* protrude face 4378 */
	case 72:	/* extrude face 346 of ARB6 */
	    VADD2(larb.pt[0], larb.pt[3], peqn[6]);
	    VADD2(larb.pt[1], larb.pt[2], peqn[6]);
	    VADD2(larb.pt[5], larb.pt[6], peqn[6]);
	    VADD2(larb.pt[4], larb.pt[7], peqn[6]);
	    break;

	case 252:   /* protrude face 2367 */
	    VADD2(larb.pt[0], larb.pt[1], peqn[6]);
	    VADD2(larb.pt[3], larb.pt[2], peqn[6]);
	    VADD2(larb.pt[4], larb.pt[5], peqn[6]);
	    VADD2(larb.pt[7], larb.pt[6], peqn[6]);
	    break;

	case 160:   /* protrude face 1548 */
	    VADD2(larb.pt[1], larb.pt[0], peqn[6]);
	    VADD2(larb.pt[5], larb.pt[4], peqn[6]);
	    VADD2(larb.pt[2], larb.pt[3], peqn[6]);
	    VADD2(larb.pt[6], larb.pt[7], peqn[6]);
	    break;

	case 120:
	case 180:
	    return 1;

	default:
	    return 1;
    }

    /* redo the plane equations */
    if (rt_arb_calc_planes(&error_msg, &larb, type, peqn, tol)) {
	bu_vls_free(&error_msg);
	return 1;
    }
    bu_vls_free(&error_msg);

    /* copy local copy back to original */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}


int
arb_permute(struct rt_arb_internal *arb, const char *encoded_permutation, const struct bn_tol *tol)
{
    struct rt_arb_internal larb;		/* local copy of solid */
    struct rt_arb_internal tarb;		/* temporary copy of solid */
    static size_t min_tuple_size[9] = {0, 0, 0, 0, 3, 2, 2, 1, 3};
    int vertex, i, k;
    size_t arglen;
    size_t face_size;	/* # vertices in THE face */
    int uvec[8], svec[11];
    int type;
    char **p;

    /*
     * The Permutations
     *
     * Each permutation is encoded as an 8-character string,
     * where the ith character specifies which of the current vertices
     * (1 through n for an ARBn) should assume the role of vertex i.
     * Wherever the internal representation of the ARB as an ARB8
     * stores a redundant copy of a vertex, the string contains a '*'.
     */
    static char *perm4[4][7] = {
	{"123*4***", "124*3***", "132*4***", "134*2***", "142*3***",
	    "143*2***", 0},
	{"213*4***", "214*3***", "231*4***", "234*1***", "241*3***",
	    "243*1***", 0},
	{"312*4***", "314*2***", "321*4***", "324*1***", "341*2***",
	    "342*1***", 0},
	{"412*3***", "413*2***", "421*3***", "423*1***", "431*2***",
	    "432*1***", 0}
    };
    static char *perm5[5][3] = {
	{"12345***", "14325***", 0},
	{"21435***", "23415***", 0},
	{"32145***", "34125***", 0},
	{"41235***", "43215***", 0},
	{0, 0, 0}
    };
    static char *perm6[6][3] = {
	{"12345*6*", "15642*3*", 0},
	{"21435*6*", "25631*4*", 0},
	{"34126*5*", "36524*1*", 0},
	{"43216*5*", "46513*2*", 0},
	{"51462*3*", "52361*4*", 0},
	{"63254*1*", "64153*2*", 0}
    };
    static char *perm7[7][2] = {
	{"1234567*", 0},
	{0, 0},
	{0, 0},
	{"4321576*", 0},
	{0, 0},
	{"6237514*", 0},
	{"7326541*", 0}
    };
    static char *perm8[8][7] = {
	{"12345678", "12654378", "14325876", "14852376",
	    "15624873", "15842673", 0},
	{"21436587", "21563487", "23416785", "23761485",
	    "26513784", "26731584", 0},
	{"32147658", "32674158", "34127856", "34872156",
	    "37624851", "37842651", 0},
	{"41238567", "41583267", "43218765", "43781265",
	    "48513762", "48731562", 0},
	{"51268437", "51486237", "56218734", "56781234",
	    "58416732", "58761432", 0},
	{"62157348", "62375148", "65127843", "65872143",
	    "67325841", "67852341", 0},
	{"73268415", "73486215", "76238514", "76583214",
	    "78436512", "78563412", 0},
	{"84157326", "84375126", "85147623", "85674123",
	    "87345621", "87654321", 0}
    };
    static int vert_loc[] = {
	/*		-----------------------------
	 *		   Array locations in which
	 *		   the vertices are stored
	 *		-----------------------------
	 *		1   2   3   4   5   6   7   8
	 *		-----------------------------
	 * ARB4 */	0,  1,  2,  4, -1, -1, -1, -1,
	/* ARB5 */	0,  1,  2,  3,  4, -1, -1, -1,
	/* ARB6 */	0,  1,  2,  3,  4,  6, -1, -1,
	/* ARB7 */	0,  1,  2,  3,  4,  5,  6, -1,
	/* ARB8 */	0,  1,  2,  3,  4,  5,  6,  7
    };
#define ARB_VERT_LOC(n, v) vert_loc[((n) - 4) * 8 + (v) - 1]

    RT_ARB_CK_MAGIC(arb);


    /* make a local copy of the solid */
    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    /*
     * Find the encoded form of the specified permutation, if it
     * exists.
     */
    arglen = strlen(encoded_permutation);
    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;
    if (type < 4 || type > 8) {return 1;}
    if (arglen < min_tuple_size[type]) {return 1;}
    face_size = (type == 4) ? 3 : 4;
    if (arglen > face_size) {return 1;}
    vertex = encoded_permutation[0] - '1';
    if ((vertex < 0) || (vertex >= type)) {return 1;}
    p = (type == 4) ? perm4[vertex] :
	(type == 5) ? perm5[vertex] :
	(type == 6) ? perm6[vertex] :
	(type == 7) ? perm7[vertex] : perm8[vertex];
    for (;; ++p) {
	if (*p == 0) {
	    return 1;
	}
	if (bu_strncmp(*p, encoded_permutation, arglen) == 0)
	    break;
    }

    /*
     * Collect the vertices in the specified order
     */
    for (i = 0; i < 8; ++i) {
	char buf[2];

	if ((*p)[i] == '*') {
	    VSETALL(tarb.pt[i], 0);
	} else {
	    sprintf(buf, "%c", (*p)[i]);
	    k = atoi(buf);
	    VMOVE(tarb.pt[i], larb.pt[ARB_VERT_LOC(type, k)]);
	}
    }

    /*
     * Reinstall the permuted vertices back into the temporary buffer,
     * copying redundant vertices as necessary
     *
     *		-------+-------------------------
     *		 Solid |    Redundant storage
     *		  Type | of some of the vertices
     *		-------+-------------------------
     *		 ARB4  |    3=0, 5=6=7=4
     *		 ARB5  |    5=6=7=4
     *		 ARB6  |    5=4, 7=6
     *		 ARB7  |    7=4
     *		 ARB8  |
     *		-------+-------------------------
     */
    for (i = 0; i < 8; i++) {
	VMOVE(larb.pt[i], tarb.pt[i]);
    }
    switch (type) {
	case ARB4:
	    VMOVE(larb.pt[3], larb.pt[0]);
	    /* fall through */
	case ARB5:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[6], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB6:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[6]);
	    break;
	case ARB7:
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB8:
	    break;
	default:
	    {
		return 1;
	    }
    }

    /* copy back to original arb */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}

int
arb_mirror_face_axis(struct rt_arb_internal *arb, fastf_t peqn[7][4], const int face, const char *axis, const struct bn_tol *tol)
{
    int i, j, k;
    int type;
    int uvec[8], svec[11];
    static int pt[4];
    static int prod;
    static vect_t work;
    struct rt_arb_internal larb;	/* local copy of solid */

    /* check which axis */
    k = -1;
    if (BU_STR_EQUAL(axis, "x"))
	k = 0;
    if (BU_STR_EQUAL(axis, "y"))
	k = 1;
    if (BU_STR_EQUAL(axis, "z"))
	k = 2;
    if (k < 0) {
	return 1;
    }

    work[0] = work[1] = work[2] = 1.0;
    work[k] = -1.0;

    /* make local copy of arb */
    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;

    if (type != ARB8 && type != ARB6) {return 1;}

    if (face > 9999 || (face < 1000 && type != ARB6)) {return 1;}

    if (type == ARB6 && face < 1000) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    } else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	     10	         125 for ARB6
     *	     72	         346 for ARB6
     */
    prod = 1;
    for (i = 0; i <= 3; i++) {
	prod *= pt[i];
	pt[i]--;
	if (pt[i] > 7) {
	    return 1;
	}
    }

    /* mirror the selected face */
    switch (prod) {

	case 24:   /* mirror face 1234 */
	    if (type == ARB6) {
		return 1;
	    }
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[j], larb.pt[i], work);
	    }
	    break;

	case 1680:   /* mirror face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[i], larb.pt[j], work);
	    }
	    break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
	    VELMUL(larb.pt[3], larb.pt[0], work);
	    VELMUL(larb.pt[2], larb.pt[1], work);
	    VELMUL(larb.pt[7], larb.pt[4], work);
	    VELMUL(larb.pt[6], larb.pt[5], work);
	    break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
	    VELMUL(larb.pt[0], larb.pt[3], work);
	    VELMUL(larb.pt[1], larb.pt[2], work);
	    VELMUL(larb.pt[5], larb.pt[6], work);
	    VELMUL(larb.pt[4], larb.pt[7], work);
	    break;

	case 252:   /* mirror face 2367 */
	    VELMUL(larb.pt[0], larb.pt[1], work);
	    VELMUL(larb.pt[3], larb.pt[2], work);
	    VELMUL(larb.pt[4], larb.pt[5], work);
	    VELMUL(larb.pt[7], larb.pt[6], work);
	    break;

	case 160:   /* mirror face 1548 */
	    VELMUL(larb.pt[1], larb.pt[0], work);
	    VELMUL(larb.pt[5], larb.pt[4], work);
	    VELMUL(larb.pt[2], larb.pt[3], work);
	    VELMUL(larb.pt[6], larb.pt[7], work);
	    break;

	case 120:
	case 180:
	    return 1;
	default:
	    return 1;
    }

    /* redo the plane equations */
    {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	if (rt_arb_calc_planes(&error_msg, &larb, type, peqn, tol)) {
	    return 1;
	}
	bu_vls_free(&error_msg);
    }

    /* copy to original */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}

int
arb_edit(struct rt_arb_internal *arb, fastf_t peqn[7][4], int edge, int newedge, vect_t pos_model, const struct bn_tol *tol)
{
    int type;
    int pflag = 0;
    int uvec[8], svec[11];
    static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    const short *edptr;		/* pointer to arb edit array */
    const short *final;		/* location of points to redo */
    static int i;
    const int *iptr;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const short earb4[5][18] = earb4_edit_array;

    RT_ARB_CK_MAGIC(arb);

    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) return 1;

    /* set the pointer */
    switch (type) {
	case ARB4:
	    edptr = &earb4[edge][0];
	    final = &earb4[edge][16];
	    pflag = 1;
	    break;
	case ARB5:
	    edptr = &earb5[edge][0];
	    final = &earb5[edge][16];
	    if (edge == 8)
		pflag = 1;
	    break;
	case ARB6:
	    edptr = &earb6[edge][0];
	    final = &earb6[edge][16];
	    if (edge > 7)
		pflag = 1;
	    break;
	case ARB7:
	    edptr = &earb7[edge][0];
	    final = &earb7[edge][16];
	    if (edge == 11)
		pflag = 1;
	    break;
	case ARB8:
	    edptr = &earb8[edge][0];
	    final = &earb8[edge][16];
	    break;
	default:
	    return 1;
    }


    /* do the arb editing */

    if (pflag) {
	/* moving a point - not an edge */
	VMOVE(arb->pt[edge], pos_model);
	edptr += 4;
    } else {
	vect_t edge_dir;

	/* moving an edge */
	pt1 = *edptr++;
	pt2 = *edptr++;
	/* direction of this edge */
	if (newedge) {
	    /* edge direction comes from edgedir() in pos_model */
	    VMOVE(edge_dir, pos_model);
	    VMOVE(pos_model, arb->pt[pt1]);
	} else {
	    /* must calculate edge direction */
	    VSUB2(edge_dir, arb->pt[pt2], arb->pt[pt1]);
	}
	if (ZERO(MAGNITUDE(edge_dir)))
	    goto err;
	/* bounding planes bp1, bp2 */
	bp1 = *edptr++;
	bp2 = *edptr++;

	/* move the edge */
	if (mv_edge(arb, pos_model, bp1, bp2, pt1, pt2, edge_dir, tol, peqn)){
	    goto err;
	}
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9) {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	int arb_calc_ret = 0;
	arb_calc_ret = rt_arb_calc_planes(&error_msg, arb, type, peqn, tol);
	bu_vls_free(&error_msg);
	if (arb_calc_ret) goto err;
    }

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1, p2, p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;
	    if (bg_make_plane_3pnts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;

	    /* next plane */
	    if ((newp = *edptr++) == -1 || newp == 8)
		break;
	}
    }
    if (newp == 8) {
	/* special...redo next planes using pts defined in faces */
	const int local_arb_faces[5][24] = rt_arb_faces;
	for (i=0; i<3; i++) {
	    if ((newp = *edptr++) == -1)
		break;
	    iptr = &local_arb_faces[type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;
	    if (bg_make_plane_3pnts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;
	}
    }

    /* the changed planes are all redone push necessary points back
     * into the planes.
     */
    edptr = final;	/* point to the correct location */
    for (i=0; i<2; i++) {
	if ((p1 = *edptr++) == -1)
	    break;
	/* intersect proper planes to define vertex p1 */
	if (rt_arb_3face_intersect(arb->pt[p1], (const plane_t *)peqn, type, p1*3))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must recalculate plane
     * 2 = 456
     */
    if (type == ARB7 && pflag) {
	if (bg_make_plane_3pnts(peqn[2], arb->pt[4], arb->pt[5], arb->pt[6], tol))
	    goto err;
    }

    /* carry along any like points */
    switch (type) {
	case ARB8:
	    break;

	case ARB7:
	    VMOVE(arb->pt[7], arb->pt[4]);
	    break;

	case ARB6:
	    VMOVE(arb->pt[5], arb->pt[4]);
	    VMOVE(arb->pt[7], arb->pt[6]);
	    break;

	case ARB5:
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;

	case ARB4:
	    VMOVE(arb->pt[3], arb->pt[0]);
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;
    }

    return 0;		/* OK */

 err:
    return 1;		/* BAD */

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
