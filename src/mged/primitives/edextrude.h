/*                      E D E X T R U D E . H
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
/** @file mged/edextrude.h
 */

#ifndef EDEXTRUDE_H
#define EDEXTRUDE_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_EXTR_SCALE_H	73	/* scale extrusion vector */
#define ECMD_EXTR_MOV_H		74	/* move end of extrusion vector */
#define ECMD_EXTR_ROT_H		75	/* rotate extrusion vector */
#define ECMD_EXTR_SKT_NAME	76	/* set sketch that the extrusion uses */

#define MENU_EXTR_SCALE_H	103
#define MENU_EXTR_MOV_H		104
#define MENU_EXTR_ROT_H		105
#define MENU_EXTR_SKT_NAME	106

extern struct menu_item extr_menu[];

#endif  /* EDEXTRUDE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
