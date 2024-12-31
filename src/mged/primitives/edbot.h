/*                      E D B O T . H
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
/** @file mged/edbot.h
 */

#ifndef EDBOT_H
#define EDBOT_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_BOT_PICKV		61	/* pick a BOT vertex */
#define ECMD_BOT_PICKE		62	/* pick a BOT edge */
#define ECMD_BOT_PICKT		63	/* pick a BOT triangle */
#define ECMD_BOT_MOVEV		64	/* move a BOT vertex */
#define ECMD_BOT_MOVEE		65	/* move a BOT edge */
#define ECMD_BOT_MOVET		66	/* move a BOT triangle */
#define ECMD_BOT_MODE		67	/* set BOT mode */
#define ECMD_BOT_ORIENT		68	/* set BOT face orientation */
#define ECMD_BOT_THICK		69	/* set face thickness (one or all) */
#define ECMD_BOT_FMODE		70	/* set face mode (one or all) */
#define ECMD_BOT_FDEL		71	/* delete current face */
#define ECMD_BOT_FLAGS		72	/* set BOT flags */

#define MENU_BOT_PICKV		91
#define MENU_BOT_PICKE		92
#define MENU_BOT_PICKT		93
#define MENU_BOT_MOVEV		94
#define MENU_BOT_MOVEE		95
#define MENU_BOT_MOVET		96
#define MENU_BOT_MODE		97
#define MENU_BOT_ORIENT		98
#define MENU_BOT_THICK		99
#define MENU_BOT_FMODE		100
#define MENU_BOT_DELETE_TRI	101
#define MENU_BOT_FLAGS		102

extern int bot_verts[3];

void
mged_bot_labels(
    int *num_lines,
    point_t *lines,
    struct rt_point_labels *pl,
    int max_pl,
    const mat_t xform,
    struct rt_db_internal *ip,
    struct bn_tol *tol);


const char *
mged_bot_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol);

struct menu_item *
mged_bot_menu_item(const struct bn_tol *tol);

void ecmd_bot_mode(struct mged_state *s);
void ecmd_bot_orient(struct mged_state *s);
void ecmd_bot_thick(struct mged_state *s);
void ecmd_bot_flags(struct mged_state *s);
void ecmd_bot_fmode(struct mged_state *s);
int ecmd_bot_fdel(struct mged_state *s);
void ecmd_bot_movev(struct mged_state *s);
void ecmd_bot_movee(struct mged_state *s);
void ecmd_bot_movet(struct mged_state *s);
int ecmd_bot_pickv(struct mged_state *s, const vect_t mousevec);
int ecmd_bot_picke(struct mged_state *s, const vect_t mousevec);
void ecmd_bot_pickt(struct mged_state *s, const vect_t mousevec);

#endif  /* EDBOT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
