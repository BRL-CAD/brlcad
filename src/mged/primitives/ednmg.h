/*                      E D N M G . H
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
/** @file mged/ednmg.h
 */

#ifndef EDNMG_H
#define EDNMG_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

// NMG editing vars
extern struct edgeuse *es_eu;
extern struct loopuse *lu_copy;
extern point_t lu_keypoint;
extern plane_t lu_pl;
extern struct shell *es_s;

#define ECMD_NMG_EPICK		19	/* edge pick */
#define ECMD_NMG_EMOVE		20	/* edge move */
#define ECMD_NMG_EDEBUG		21	/* edge debug */
#define ECMD_NMG_FORW		22	/* next eu */
#define ECMD_NMG_BACK		23	/* prev eu */
#define ECMD_NMG_RADIAL		24	/* radial+mate eu */
#define ECMD_NMG_ESPLIT		25	/* split current edge */
#define ECMD_NMG_EKILL		26	/* kill current edge */
#define ECMD_NMG_LEXTRU		27	/* Extrude loop */

extern const char *
mged_nmg_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol);

extern void
mged_nmg_labels(
	int *num_lines,
	point_t *lines,
	struct rt_point_labels *pl,
	int max_pl,
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *tol);

struct menu_item *
mged_nmg_menu_item(const struct bn_tol *tol);

extern void ecmd_nmg_emove(struct mged_state *s);
extern void ecmd_nmg_ekill(struct mged_state *s);
extern void ecmd_nmg_esplit(struct mged_state *s);
extern void ecmd_nmg_lextru(struct mged_state *s);
extern void ecmd_nmg_epick(struct mged_state *s, const vect_t mousevec);

#endif  /* EDNMG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
