/*                      E D T G C . H
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
/** @file mged/edtgc.h
 */

#ifndef EDTGC_H
#define EDTGC_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_TGC_MV_H	5
#define ECMD_TGC_MV_HH	6
#define ECMD_TGC_ROT_H	7
#define ECMD_TGC_ROT_AB	8
#define ECMD_TGC_MV_H_CD	81	/* move end of tgc, while scaling CD */
#define ECMD_TGC_MV_H_V_AB	82	/* move vertex end of tgc, while scaling AB */

#define MENU_TGC_ROT_H		23
#define MENU_TGC_ROT_AB 	24
#define MENU_TGC_MV_H		25
#define MENU_TGC_MV_HH		26
#define MENU_TGC_SCALE_H	27
#define MENU_TGC_SCALE_H_V	28
#define MENU_TGC_SCALE_A	29
#define MENU_TGC_SCALE_B	30
#define MENU_TGC_SCALE_C	31
#define MENU_TGC_SCALE_D	32
#define MENU_TGC_SCALE_AB	33
#define MENU_TGC_SCALE_CD	34
#define MENU_TGC_SCALE_ABCD	35

#define MENU_TGC_SCALE_H_CD	111
#define MENU_TGC_SCALE_H_V_AB	112

extern struct menu_item tgc_menu[];

#endif  /* EDTGC_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
