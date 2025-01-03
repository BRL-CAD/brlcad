/*                          E D H Y P . H
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
/** @file mged/edhyp.h
 */

#ifndef EDHYP_H
#define EDHYP_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_HYP_ROT_H		91
#define ECMD_HYP_ROT_A		92

#define MENU_HYP_H              127
#define MENU_HYP_SCALE_A        128
#define MENU_HYP_SCALE_B	129
#define MENU_HYP_C		130
#define MENU_HYP_ROT_H		131

void menu_hyp_h(struct mged_state *s);
void menu_hyp_scale_a(struct mged_state *s);
void menu_hyp_scale_b(struct mged_state *s);
void menu_hyp_c(struct mged_state *s);

#endif  /* EDHYP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
