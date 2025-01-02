/*                      E D R H C . H
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
/** @file mged/edrhc.h
 */

#ifndef EDRHC_H
#define EDRHC_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define MENU_RHC_B		46
#define MENU_RHC_H		47
#define MENU_RHC_R		48
#define MENU_RHC_C		49

void menu_rhc_b(struct mged_state *s);
void menu_rhc_h(struct mged_state *s);
void menu_rhc_r(struct mged_state *s);
void menu_rhc_c(struct mged_state *s);

#endif  /* EDRHC_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
