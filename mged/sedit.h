/*
 *  			S E D I T . H
 *  
 *  This header file contains the esolid structure definition,
 *  which holds all the information necessary for solid editing.
 *  Storage is actually allocated in edsol.c
 *  
 *  Author -
 *	Kieth A Applin
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#define MGED_SMALL_SCALE 1.0e-10

/* These EDIT_CLASS_ values go in es_edclass. */
#define EDIT_CLASS_NULL 0
#define EDIT_CLASS_TRAN 1
#define EDIT_CLASS_ROTATE 2
#define EDIT_CLASS_SCALE 3

/* These ECMD_ values go in es_edflag.  Some names not changed yet */
#define IDLE		0	/* edarb.c */
#define STRANS		1	/* buttons.c */
#define SSCALE		2	/* buttons.c */	/* Scale whole solid by scalor */
#define SROT		3	/* buttons.c */
#define PSCALE		4	/* Scale one solid parameter by scalor */

#define	ECMD_TGC_MV_H	5
#define ECMD_TGC_MV_HH	6
#define ECMD_TGC_ROT_H	7
#define ECMD_TGC_ROT_AB	8

#define EARB		9	/* chgmodel.c, edarb.c */
#define PTARB		10	/* edarb.c */
#define ECMD_ARB_MAIN_MENU	11
#define ECMD_ARB_SPECIFIC_MENU	12
#define ECMD_ARB_MOVE_FACE	13
#define ECMD_ARB_SETUP_ROTFACE	14
#define ECMD_ARB_ROTATE_FACE	15

#define ECMD_ETO_ROT_C		16

#define ECMD_VTRANS		17	/* vertex translate */
#define ECMD_NMG_EPICK		19	/* edge pick */
#define ECMD_NMG_EMOVE		20	/* edge move */
#define ECMD_NMG_EDEBUG		21	/* edge debug */
#define ECMD_NMG_FORW		22	/* next eu */
#define ECMD_NMG_BACK		23	/* prev eu */
#define ECMD_NMG_RADIAL		24	/* radial+mate eu */
#define	ECMD_NMG_ESPLIT		25	/* split current edge */
#define	ECMD_NMG_EKILL		26	/* kill current edge */
#define	ECMD_NMG_LEXTRU		27	/* Extrude loop */

#define ECMD_PIPE_PICK		28	/* Pick pipe point */
#define	ECMD_PIPE_SPLIT		29	/* Split a pipe segment into two */
#define	ECMD_PIPE_PT_ADD	30	/* Add a pipe point to end of pipe */
#define	ECMD_PIPE_PT_INS	31	/* Add a pipe point to start of pipe */
#define	ECMD_PIPE_PT_DEL	32	/* Delete a pipe point */
#define	ECMD_PIPE_PT_MOVE	33	/* Move a pipe point */

#define	ECMD_ARS_PICK		34	/* select an ARS point */
#define	ECMD_ARS_NEXT_PT	35	/* select next ARS point in same curve */
#define	ECMD_ARS_PREV_PT	36	/* select previous ARS point in same curve */
#define	ECMD_ARS_NEXT_CRV	37	/* select corresponding ARS point in next curve */
#define	ECMD_ARS_PREV_CRV	38	/* select corresponding ARS point in previous curve */
#define	ECMD_ARS_MOVE_PT	39	/* translate an ARS point */
#define	ECMD_ARS_DEL_CRV	40	/* delete an ARS curve */
#define	ECMD_ARS_DEL_COL	41	/* delete all corresponding points in each curve (a column) */
#define	ECMD_ARS_DUP_CRV	42	/* duplicate an ARS curve */
#define	ECMD_ARS_DUP_COL	43	/* duplicate an ARS column */
#define	ECMD_ARS_MOVE_CRV	44	/* translate an ARS curve */
#define	ECMD_ARS_MOVE_COL	45	/* translate an ARS column */
#define ECMD_ARS_PICK_MENU	46	/* display the ARS pick menu */
#define ECMD_ARS_EDIT_MENU	47	/* display the ARS edit menu */

#define	ECMD_VOL_CSIZE		48	/* set voxel size */
#define	ECMD_VOL_FSIZE		49	/* set VOL file dimensions */
#define	ECMD_VOL_THRESH_LO	50	/* set VOL threshold (lo) */
#define	ECMD_VOL_THRESH_HI	51	/* set VOL threshold (hi) */
#define	ECMD_VOL_FNAME		52	/* set VOL file name */

#define	ECMD_EBM_FNAME		53	/* set EBM file name */
#define	ECMD_EBM_FSIZE		54	/* set EBM file size */
#define	ECMD_EBM_HEIGHT		55	/* set EBM extrusion depth */

#define	ECMD_DSP_FNAME		56	/* set DSP file name */
#define	ECMD_DSP_FSIZE		57	/* set DSP file size */
#define ECMD_DSP_SCALE_X        58	/* Scale DSP x size */
#define ECMD_DSP_SCALE_Y        59	/* Scale DSP y size */
#define ECMD_DSP_SCALE_ALT      60	/* Scale DSP Altitude size */


#define SEDIT_ROTATE (state == ST_S_EDIT && \
		      (es_edflag == SROT || \
		       es_edflag == ECMD_TGC_ROT_H || \
		       es_edflag ==  ECMD_TGC_ROT_AB || \
		       es_edflag == ECMD_ARB_ROTATE_FACE || \
		       es_edflag == ECMD_ETO_ROT_C))
#define OEDIT_ROTATE (state == ST_O_EDIT && \
		      edobj == BE_O_ROTATE)
#define EDIT_ROTATE (SEDIT_ROTATE || OEDIT_ROTATE)

#define SEDIT_SCALE (state == ST_S_EDIT && \
		     (es_edflag == SSCALE || \
		      es_edflag == PSCALE || \
		      es_edflag == ECMD_VOL_THRESH_LO || \
		      es_edflag == ECMD_VOL_THRESH_HI || \
		      es_edflag == ECMD_VOL_CSIZE || \
		      es_edflag == ECMD_DSP_SCALE_X || \
		      es_edflag == ECMD_DSP_SCALE_Y || \
		      es_edflag == ECMD_DSP_SCALE_ALT || \
		      es_edflag == ECMD_EBM_HEIGHT ))
#define OEDIT_SCALE (state == ST_O_EDIT && \
		     (edobj == BE_O_XSCALE || \
		      edobj == BE_O_YSCALE || \
		      edobj == BE_O_ZSCALE || \
		      edobj == BE_O_SCALE))
#define EDIT_SCALE (SEDIT_SCALE || OEDIT_SCALE)

#define SEDIT_TRAN (state == ST_S_EDIT && \
		    (es_edflag == STRANS || \
		     es_edflag == ECMD_TGC_MV_H || \
		     es_edflag == ECMD_TGC_MV_HH || \
		     es_edflag == EARB || \
		     es_edflag == PTARB || \
		     es_edflag == ECMD_ARB_MOVE_FACE || \
		     es_edflag == ECMD_VTRANS || \
		     es_edflag == ECMD_NMG_EMOVE || \
		     es_edflag == ECMD_PIPE_PT_MOVE) || \
		     es_edflag == ECMD_ARS_MOVE_PT || \
		     es_edflag == ECMD_ARS_MOVE_CRV || \
		     es_edflag == ECMD_ARS_MOVE_COL)
#define OEDIT_TRAN (state == ST_O_EDIT && \
		    (edobj == BE_O_X || \
		     edobj == BE_O_Y || \
		     edobj == BE_O_XY))
#define EDIT_TRAN (SEDIT_TRAN || OEDIT_TRAN)

#define SEDIT_PICK (state == ST_S_EDIT && \
		    (es_edflag == ECMD_NMG_EPICK || \
		     es_edflag == ECMD_NMG_ESPLIT || \
		     es_edflag == ECMD_PIPE_PICK || \
		     es_edflag == ECMD_PIPE_SPLIT || \
		     es_edflag == ECMD_PIPE_PT_ADD || \
		     es_edflag == ECMD_PIPE_PT_INS || \
		     es_edflag == ECMD_ARS_PICK))

extern vect_t edit_absolute_model_rotate; 
extern vect_t edit_absolute_object_rotate;
extern vect_t edit_absolute_view_rotate;
extern vect_t last_edit_absolute_model_rotate;
extern vect_t last_edit_absolute_object_rotate;
extern vect_t last_edit_absolute_view_rotate;
extern vect_t edit_rate_model_rotate;
extern vect_t edit_rate_object_rotate;
extern vect_t edit_rate_view_rotate;
extern int edit_rateflag_model_rotate;
extern int edit_rateflag_object_rotate;
extern int edit_rateflag_view_rotate;

extern vect_t edit_absolute_model_tran;
extern vect_t edit_absolute_view_tran;
extern vect_t last_edit_absolute_model_tran;
extern vect_t last_edit_absolute_view_tran;
extern vect_t edit_rate_model_tran;
extern vect_t edit_rate_view_tran;
extern int edit_rateflag_model_tran;
extern int edit_rateflag_view_tran;

extern fastf_t edit_absolute_scale;
extern fastf_t edit_rate_scale;
extern int edit_rateflag_scale;

extern char edit_rate_model_origin;
extern char edit_rate_object_origin;
extern char edit_rate_view_origin;
extern char edit_rate_coords;
extern struct dm_list *edit_rate_mr_dm_list;
extern struct dm_list *edit_rate_or_dm_list;
extern struct dm_list *edit_rate_vr_dm_list;
extern struct dm_list *edit_rate_mt_dm_list;
extern struct dm_list *edit_rate_vt_dm_list;

extern struct bu_vls edit_info_vls;
extern struct bu_vls edit_rate_model_tran_vls[3];
extern struct bu_vls edit_rate_view_tran_vls[3];
extern struct bu_vls edit_rate_model_rotate_vls[3];
extern struct bu_vls edit_rate_object_rotate_vls[3];
extern struct bu_vls edit_rate_view_rotate_vls[3];
extern struct bu_vls edit_rate_scale_vls;
extern struct bu_vls edit_absolute_model_tran_vls[3];
extern struct bu_vls edit_absolute_view_tran_vls[3];
extern struct bu_vls edit_absolute_model_rotate_vls[3];
extern struct bu_vls edit_absolute_object_rotate_vls[3];
extern struct bu_vls edit_absolute_view_rotate_vls[3];
extern struct bu_vls edit_absolute_scale_vls;

extern fastf_t	es_scale;		/* scale factor */
extern fastf_t 	es_para[3];		/* keyboard input parameter changes */
extern fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
extern int	es_menu;		/* item/edit_mode selected from menu */
extern int	es_edflag;		/* type of editing for this solid */
extern int	es_edclass;		/* type of editing class for this solid */
extern int	es_type;		/* COMGEOM solid type */

extern mat_t	es_mat;			/* accumulated matrix of path */ 
extern mat_t 	es_invmat;		/* inverse of es_mat   KAA */

extern point_t	es_keypoint;		/* center of editing xforms */
extern char	*es_keytag;		/* string identifying the keypoint */

extern int arb_faces[5][24];	/* from edarb.c */
extern int arb_planes[5][24];	/* from edarb.c */
