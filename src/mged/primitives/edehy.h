/*                      E D E H Y . H
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
/** @file mged/edehy.h
 */

#ifndef EDEHY_H
#define EDEHY_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define MENU_EHY_H		53
#define MENU_EHY_R1		54
#define MENU_EHY_R2		55
#define MENU_EHY_C		56

void menu_ehy_h(struct mged_state *s);
void menu_ehy_r1(struct mged_state *s);
void menu_ehy_r2(struct mged_state *s);
void menu_ehy_c(struct mged_state *s);

#endif  /* EDEHY_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
