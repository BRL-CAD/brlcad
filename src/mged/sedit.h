/*                         S E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/sedit.h
 *
 * This header file contains the esolid structure definition,
 * which holds all the information necessary for solid editing.
 * Storage is actually allocated in edsol.c
 *
 */

#ifndef MGED_SEDIT_H
#define MGED_SEDIT_H

#include "mged.h"

#define MGED_SMALL_SCALE 1.0e-10

/* These EDIT_CLASS_ values go in s->edit_state.e_edclass. */
#define EDIT_CLASS_NULL 0
#define EDIT_CLASS_TRAN 1
#define EDIT_CLASS_ROTATE 2
#define EDIT_CLASS_SCALE 3

/* These values go in edit_flag .  Some names not changed yet */
#define IDLE		0	/* edarb.c */
#define STRANS		1	/* buttons.c */
#define SSCALE		2	/* buttons.c */	/* Scale whole solid by scalar */
#define SROT		3	/* buttons.c */
#define PSCALE		4	/* Scale one solid parameter by scalar */

#define SEDIT_ROTATE (s->edit_state.global_editing_state == ST_S_EDIT && \
	s->s_edit.solid_edit_rotate)
#define SEDIT_TRAN (s->edit_state.global_editing_state == ST_S_EDIT && \
	s->s_edit.solid_edit_translate)
#define SEDIT_SCALE (s->edit_state.global_editing_state == ST_S_EDIT && \
	s->s_edit.solid_edit_scale)
#define SEDIT_PICK (s->edit_state.global_editing_state == ST_S_EDIT && \
	s->s_edit.solid_edit_pick)

#define OEDIT_ROTATE (s->edit_state.global_editing_state == ST_O_EDIT && \
		      edobj == BE_O_ROTATE)
#define OEDIT_TRAN (s->edit_state.global_editing_state == ST_O_EDIT && \
		    (edobj == BE_O_X || \
		     edobj == BE_O_Y || \
		     edobj == BE_O_XY))
#define OEDIT_SCALE (s->edit_state.global_editing_state == ST_O_EDIT && \
		     (edobj == BE_O_XSCALE || \
		      edobj == BE_O_YSCALE || \
		      edobj == BE_O_ZSCALE || \
		      edobj == BE_O_SCALE))

#define EDIT_ROTATE (SEDIT_ROTATE || OEDIT_ROTATE)
#define EDIT_SCALE (SEDIT_SCALE || OEDIT_SCALE)
#define EDIT_TRAN (SEDIT_TRAN || OEDIT_TRAN)

extern void get_solid_keypoint(struct mged_state *s, point_t *pt,
			       const char **strp,
			       struct rt_db_internal *ip,
			       fastf_t *mat);

extern void set_e_axes_pos(struct mged_state *s, int both);


#endif /* MGED_SEDIT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
