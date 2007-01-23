/*                        U S R F M T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file usrfmt.h
*/
#ifndef SEEN_USRFMT_H
#define SEEN_USRFMT_H

#define			OUTITEM_NULL		((outitem *) 0)
#define			OVERLAP_NULL		((overlap *) 0)
#define			DEF_RCF_NAME		".nirtrc"
#define			DEF_SF_NAME		"nirt_state"
#define			CMT_CHAR		'#'
#define			SEP_CHAR		';'

typedef struct outitem_tag
{
    char		*format;
    int			code_nm;
    struct outitem_tag	*next;
} 					outitem;

typedef struct outval_tag
{
    char		*name;
    int			code_nm;
    int			type;
    union ovv {
	fastf_t	fval;
	const char	*sval;
	int	ival;
	}		value;
}					outval;

typedef struct ovlp_tag
{
    struct application	*ap;
    struct partition	*pp;
    struct region	*reg1;
    struct region	*reg2;
    fastf_t		in_dist;
    fastf_t		out_dist;
    point_t		in_point;
    point_t		out_point;
    struct ovlp_tag	*forw;
    struct ovlp_tag	*backw;
}					overlap;

extern FILE	*fopenrc(void);

/* Codes for output-item data types */
#define		OIT_INT		0
#define		OIT_FLOAT	1
#define		OIT_FNOUNIT	2	/* Local units don't apply */
#define		OIT_STRING	3
#define		oit_name(t)	(((t)==OIT_INT) ? "int" :\
				 ((t)==OIT_FLOAT) ? "float" :\
				 ((t)==OIT_FNOUNIT) ? "float" :\
				 ((t)==OIT_STRING) ? "string" : "unknown")

/* Codes for output-statement types */
#define		FMT_RAY 	0
#define		FMT_HEAD	1	/* Don't change this w/o */
#define		FMT_PART	2	/* changing fmt_char[]	 */
#define		FMT_FOOT	3	/* in dump_state()	 */
#define		FMT_MISS	4
#define		FMT_OVLP	5
#define		FMT_NONE	6

/* Indices into the table of output values
 *
 *	WARNING - Don't change these #defines without
 *			changing the initialization of
 *			ValTab[]!
 */
#define		VTI_LITERAL	0
#define		VTI_X_ORIG	1
#define		VTI_Y_ORIG	2
#define		VTI_Z_ORIG	3
#define		VTI_H		4
#define		VTI_V		5
#define		VTI_D_ORIG	6
#define		VTI_X_DIR	7
#define		VTI_Y_DIR	8
#define		VTI_Z_DIR	9
#define		VTI_A		10
#define		VTI_E		11
#define		VTI_X_IN	12
#define		VTI_Y_IN	13
#define		VTI_Z_IN	14
#define		VTI_D_IN	15
#define		VTI_X_OUT	16
#define		VTI_Y_OUT	17
#define		VTI_Z_OUT	18
#define		VTI_D_OUT	19
#define		VTI_LOS		20
#define		VTI_SLOS	21
#define		VTI_PATH_NAME	22
#define		VTI_REG_NAME	23
#define		VTI_REG_ID	24
#define		VTI_OBLIQ_IN	25
#define		VTI_OBLIQ_OUT	26
#define		VTI_NM_X_IN	27
#define		VTI_NM_Y_IN	28
#define		VTI_NM_Z_IN	29
#define		VTI_NM_D_IN	30
#define		VTI_NM_H_IN	31
#define		VTI_NM_V_IN	32
#define		VTI_NM_X_OUT	33
#define		VTI_NM_Y_OUT	34
#define		VTI_NM_Z_OUT	35
#define		VTI_NM_D_OUT	36
#define		VTI_NM_H_OUT	37
#define		VTI_NM_V_OUT	38
#define		VTI_OV_REG1_NAME 39
#define		VTI_OV_REG1_ID	40
#define		VTI_OV_REG2_NAME 41
#define		VTI_OV_REG2_ID	42
#define		VTI_OV_SOL_IN	43
#define		VTI_OV_SOL_OUT	44
#define		VTI_OV_LOS	45
#define		VTI_OV_X_IN	46
#define		VTI_OV_Y_IN	47
#define		VTI_OV_Z_IN	48
#define		VTI_OV_D_IN	49
#define		VTI_OV_X_OUT	50
#define		VTI_OV_Y_OUT	51
#define		VTI_OV_Z_OUT	52
#define		VTI_OV_D_OUT	53
#define		VTI_SURF_NUM_IN	54
#define		VTI_SURF_NUM_OUT 55
#define		VTI_CLAIMANT_COUNT 56
#define		VTI_CLAIMANT_LIST 57
#define		VTI_CLAIMANT_LISTN 58
#define		VTI_ATTRIBUTES 59

#define		direct(i)	(ValTab[VTI_X_DIR + i].value.fval)
#define		target(i)	(ValTab[VTI_X_ORIG + i].value.fval)
#define		r_entry(i)	(ValTab[VTI_X_IN + i].value.fval)
#define		r_exit(i)	(ValTab[VTI_X_OUT + i].value.fval)
#define		n_entry(i)	(ValTab[VTI_NM_X_IN + i].value.fval)
#define		n_exit(i)	(ValTab[VTI_NM_X_OUT + i].value.fval)
#define		ov_entry(i)	(ValTab[VTI_OV_X_IN + i].value.fval)
#define		ov_exit(i)	(ValTab[VTI_OV_X_OUT + i].value.fval)
#define		grid(i)		(ValTab[VTI_H + i].value.fval)
#define		azimuth()	(ValTab[VTI_A].value.fval)
#define		elevation()	(ValTab[VTI_E].value.fval)

/* Define D, H, and V for use with the preceeding macros.
 *
 *	NOTE! --- H was defined in vmath.h as the index of
 *		  the fourth homogeneous coordinate.  In
 *		  NIRT, we are not using H that way!
 */
#define		D		3
#ifdef H
#	undef H
#endif
#define		H		4
#define		V		5

#endif /* SEEN_USRFMT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
