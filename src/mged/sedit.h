/*                         S E D I T . H
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

/* These EDIT_CLASS_ values go in es_edclass. */
#define EDIT_CLASS_NULL 0
#define EDIT_CLASS_TRAN 1
#define EDIT_CLASS_ROTATE 2
#define EDIT_CLASS_SCALE 3

/* These ECMD_ values go in es_edflag.  Some names not changed yet */
#define IDLE		0	/* edarb.c */
#define STRANS		1	/* buttons.c */
#define SSCALE		2	/* buttons.c */	/* Scale whole solid by scalar */
#define SROT		3	/* buttons.c */
#define PSCALE		4	/* Scale one solid parameter by scalar */

#define ECMD_VTRANS		17	/* vertex translate */
#include "primitives/edarb.h"
#include "primitives/edars.h"
#include "primitives/edbot.h"
#include "primitives/edbspline.h"
#include "primitives/edcline.h"
#include "primitives/eddsp.h"
#include "primitives/edebm.h"
#include "primitives/edehy.h"
#include "primitives/edell.h"
#include "primitives/edepa.h"
#include "primitives/edeto.h"
#include "primitives/edextrude.h"
#include "primitives/edhyp.h"
#include "primitives/edmetaball.h"
#include "primitives/ednmg.h"
#include "primitives/edpart.h"
#include "primitives/edpipe.h"
#include "primitives/edrhc.h"
#include "primitives/edrpc.h"
#include "primitives/edsuperell.h"
#include "primitives/edtgc.h"
#include "primitives/edtor.h"
#include "primitives/edvol.h"


#define SEDIT_ROTATE (GEOM_EDIT_STATE == ST_S_EDIT && \
		      (es_edflag == SROT || \
		       es_edflag == ECMD_TGC_ROT_H || \
		       es_edflag ==  ECMD_TGC_ROT_AB || \
		       es_edflag == ECMD_ARB_ROTATE_FACE || \
		       es_edflag == ECMD_EXTR_ROT_H || \
		       es_edflag == ECMD_ETO_ROT_C))
#define OEDIT_ROTATE (GEOM_EDIT_STATE == ST_O_EDIT && \
		      edobj == BE_O_ROTATE)
#define EDIT_ROTATE (SEDIT_ROTATE || OEDIT_ROTATE)

#define SEDIT_SCALE (GEOM_EDIT_STATE == ST_S_EDIT && \
		     (es_edflag == SSCALE || \
		      es_edflag == PSCALE || \
		      es_edflag == ECMD_VOL_THRESH_LO || \
		      es_edflag == ECMD_VOL_THRESH_HI || \
		      es_edflag == ECMD_VOL_CSIZE || \
		      es_edflag == ECMD_DSP_SCALE_X || \
		      es_edflag == ECMD_DSP_SCALE_Y || \
		      es_edflag == ECMD_DSP_SCALE_ALT || \
		      es_edflag == ECMD_EBM_HEIGHT || \
		      es_edflag == ECMD_CLINE_SCALE_H || \
		      es_edflag == ECMD_CLINE_SCALE_R || \
		      es_edflag == ECMD_CLINE_SCALE_T || \
		      es_edflag == ECMD_EXTR_SCALE_H))
#define OEDIT_SCALE (GEOM_EDIT_STATE == ST_O_EDIT && \
		     (edobj == BE_O_XSCALE || \
		      edobj == BE_O_YSCALE || \
		      edobj == BE_O_ZSCALE || \
		      edobj == BE_O_SCALE))
#define EDIT_SCALE (SEDIT_SCALE || OEDIT_SCALE)

#define SEDIT_TRAN (GEOM_EDIT_STATE == ST_S_EDIT && \
		    (es_edflag == STRANS || \
		     es_edflag == ECMD_TGC_MV_H || \
		     es_edflag == ECMD_TGC_MV_HH || \
		     es_edflag == EARB || \
		     es_edflag == PTARB || \
		     es_edflag == ECMD_ARB_MOVE_FACE || \
		     es_edflag == ECMD_VTRANS || \
		     es_edflag == ECMD_NMG_EMOVE || \
		     es_edflag == ECMD_NMG_ESPLIT || \
		     es_edflag == ECMD_NMG_LEXTRU || \
		     es_edflag == ECMD_PIPE_PT_MOVE || \
		     es_edflag == ECMD_PIPE_SPLIT || \
		     es_edflag == ECMD_PIPE_PT_ADD || \
		     es_edflag == ECMD_PIPE_PT_INS || \
		     es_edflag == ECMD_ARS_MOVE_PT || \
		     es_edflag == ECMD_ARS_MOVE_CRV || \
		     es_edflag == ECMD_ARS_MOVE_COL || \
		     es_edflag == ECMD_BOT_MOVEV || \
		     es_edflag == ECMD_BOT_MOVEE || \
		     es_edflag == ECMD_BOT_MOVET || \
		     es_edflag == ECMD_CLINE_MOVE_H || \
		     es_edflag == ECMD_EXTR_MOV_H))
#define OEDIT_TRAN (GEOM_EDIT_STATE == ST_O_EDIT && \
		    (edobj == BE_O_X || \
		     edobj == BE_O_Y || \
		     edobj == BE_O_XY))
#define EDIT_TRAN (SEDIT_TRAN || OEDIT_TRAN)

#define SEDIT_PICK (GEOM_EDIT_STATE == ST_S_EDIT && \
		    (es_edflag == ECMD_NMG_EPICK || \
		     es_edflag == ECMD_PIPE_PICK || \
		     es_edflag == ECMD_ARS_PICK || \
		     es_edflag == ECMD_BOT_PICKV || \
		     es_edflag == ECMD_BOT_PICKE || \
		     es_edflag == ECMD_BOT_PICKT || \
		     es_edflag == ECMD_METABALL_PT_PICK))


extern fastf_t es_para[3];	/* keyboard input parameter changes */
extern int es_menu;		/* item/edit_mode selected from menu */
extern int es_edflag;		/* type of editing for this solid */
extern int es_edclass;		/* type of editing class for this solid */
extern int es_type;		/* COMGEOM solid type */
extern int es_keyfixed;		/* keypoint specified by user */


extern mat_t es_mat;		/* accumulated matrix of path */
extern mat_t es_invmat;		/* inverse of es_mat KAA */

extern point_t es_keypoint;	/* center of editing xforms */
extern const char *es_keytag;		/* string identifying the keypoint */
extern point_t curr_e_axes_pos;	/* center of editing xforms */

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
