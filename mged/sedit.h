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

/* data for solid editing */
extern union record es_rec;		/* current solid record */

#define es_type	es_rec.s.s_cgtype	/* COMGEOM solid type */

extern int     es_edflag;		/* type of editing for this solid */
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

extern fastf_t	es_scale;		/* scale factor */
extern fastf_t 	es_para[3];		/* keyboard input parameter changes */
extern fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
extern int	es_menu;		/* item/edit_mode selected from menu */

extern mat_t	es_mat;			/* accumulated matrix of path */ 
extern mat_t 	es_invmat;		/* inverse of es_mat   KAA */

extern point_t	es_keypoint;		/* center of editing xforms */
extern char	*es_keytag;		/* string identifying the keypoint */

extern int arb_faces[5][24];	/* from edarb.c */
extern int arb_planes[5][24];	/* from edarb.c */
