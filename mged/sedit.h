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

#define es_name	es_rec.s.s_name
#define es_type	es_rec.s.s_cgtype	/* COMGEOM solid type */
#define es_gentype es_rec.s.s_type	/* GED general solid type */

extern int     es_edflag;		/* type of editing for this solid */
#define IDLE	0
#define STRANS	1
#define SSCALE	2
#define SROT	3
#define MENU	4
#define PSCALE	5
#define PROT	6
#define EARB	7
#define	MOVEH	8
#define MOVEHH	9
#define PTARB	10
#define CONTROL 11
#define MVFACE  12
#define ROTFACE 13
#define CHGMENU 14
#define SETUP_ROTFACE 15

extern fastf_t	es_scale;		/* scale factor */
extern fastf_t 	es_para[3];		/* keyboard input parameter changes */
extern fastf_t	es_peqn[7][4];		/* ARBs defining plane equations */
extern int	es_menu;		/* item selected from menu */

#define MENUH		1
#define MENUR1		2
#define MENUR2		3
#define MENUA		4
#define MENUB		5
#define MENUC		6
#define MENURH		7
#define MENURAB 	8
#define	MENUMH		9
#define MENUMHH		10
#define MENUP1		11
#define MENUP2		12
#define MENUAB		13
#define MENUCD		14
#define MENUABCD	15
#define MENUABC		16
#define EDGEMENU	17
#define MOVEMENU	18
#define ROTMENU		19
extern mat_t	es_mat;			/* accumulated matrix of path */ 
extern mat_t 	es_invmat;		/* inverse of es_mat   KAA */

extern int arb_faces[5][24];	/* from edarb.c */
extern int arb_planes[5][24];	/* from edarb.c */
