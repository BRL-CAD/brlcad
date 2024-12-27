/*                      E D E L L . H
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
/** @file mged/edell.h
 */

#ifndef EDELL_H
#define EDELL_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define MENU_ELL_SCALE_A	39
#define MENU_ELL_SCALE_B	40
#define MENU_ELL_SCALE_C	41
#define MENU_ELL_SCALE_ABC	42

extern struct menu_item ell_menu[];

void menu_ell_scale_a(struct mged_state *s);
void menu_ell_scale_b(struct mged_state *s);
void menu_ell_scale_c(struct mged_state *s);
void menu_ell_scale_abc(struct mged_state *s);

#endif  /* EDELL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
