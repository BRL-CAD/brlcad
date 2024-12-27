/*                      E D D S P . H
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
/** @file mged/eddsp.h
 */

#ifndef EDDSP_H
#define EDDSP_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_DSP_FNAME		56	/* set DSP file name */
#define ECMD_DSP_FSIZE		57	/* set DSP file size */
#define ECMD_DSP_SCALE_X        58	/* Scale DSP x size */
#define ECMD_DSP_SCALE_Y        59	/* Scale DSP y size */
#define ECMD_DSP_SCALE_ALT      60	/* Scale DSP Altitude size */

#define MENU_DSP_FNAME		83
#define MENU_DSP_FSIZE		84	/* Not implemented yet */
#define MENU_DSP_SCALE_X	85
#define MENU_DSP_SCALE_Y	86
#define MENU_DSP_SCALE_ALT	87

extern struct menu_item dsp_menu[];

void ecmd_dsp_scale_x(struct mged_state *s);
void ecmd_dsp_scale_y(struct mged_state *s);
void ecmd_dsp_scale_alt(struct mged_state *s);
int ecmd_dsp_fname(struct mged_state *s);

#endif  /* EDDSP_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
