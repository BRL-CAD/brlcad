/*                      E D V O L . H
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
/** @file mged/edvol.h
 */

#ifndef EDVOL_H
#define EDVOL_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_VOL_CSIZE		48	/* set voxel size */
#define ECMD_VOL_FSIZE		49	/* set VOL file dimensions */
#define ECMD_VOL_THRESH_LO	50	/* set VOL threshold (lo) */
#define ECMD_VOL_THRESH_HI	51	/* set VOL threshold (hi) */
#define ECMD_VOL_FNAME		52	/* set VOL file name */

#define MENU_VOL_FNAME		75
#define MENU_VOL_FSIZE		76
#define MENU_VOL_CSIZE		77
#define MENU_VOL_THRESH_LO	78
#define MENU_VOL_THRESH_HI	79

extern struct menu_item vol_menu[];

void menu_vol_csize(struct mged_state *s);
void ecmd_vol_csize(struct mged_state *s);
void ecmd_vol_fsize(struct mged_state *s);
void ecmd_vol_thresh_lo(struct mged_state *s);
void ecmd_vol_thresh_hi(struct mged_state *s);
void ecmd_vol_fname(struct mged_state *s);

#endif  /* EDVOL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
