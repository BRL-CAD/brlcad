/*                      E D E T O . H
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
/** @file mged/edeto.h
 */

#ifndef EDETO_H
#define EDETO_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_ETO_ROT_C		16

#define MENU_ETO_R		57
#define MENU_ETO_RD		58
#define MENU_ETO_SCALE_C	59
#define MENU_ETO_ROT_C		60

struct menu_item *
mged_eto_menu_item(const struct bn_tol *tol);

void menu_eto_r(struct mged_state *s);
void menu_eto_rd(struct mged_state *s);
void menu_eto_scale_c(struct mged_state *s);
void ecmd_eto_rot_c(struct mged_state *s);

#endif  /* EDETO_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
