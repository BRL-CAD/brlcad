/*                          E D E B M . H
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
/** @file mged/edebm.h
 */

#ifndef EDEBM_H
#define EDEBM_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_EBM_FNAME		53	/* set EBM file name */
#define ECMD_EBM_FSIZE		54	/* set EBM file size */
#define ECMD_EBM_HEIGHT		55	/* set EBM extrusion depth */

#define MENU_EBM_FNAME		80
#define MENU_EBM_FSIZE		81
#define MENU_EBM_HEIGHT		82

int ecmd_ebm_fsize(struct mged_state *s);
int ecmd_ebm_fname(struct mged_state *s);
int ecmd_ebm_height(struct mged_state *s);

#endif  /* EDEBM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
