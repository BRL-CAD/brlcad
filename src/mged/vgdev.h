/*                         V G D E V . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file vgdev.h
 *
 */

/*	SCCSID	%W%	%E%	VAX version	*/
/*
 *			V G D E V . H
 *
 * This file contains definitions for building displaylists for the VG,
 * and will ordinarily be included in all user programs which use the VG.
 *
 * There is a companion file vgreg.h which contains defnitions for
 * operating the UNIBUS interface to the VG.
 *
 *		REVISION HISTORY:
 *
 *	06-Sep-83 DAG	Created from /sys/vaxuba/vgdev.h with shorter
 *			symbols, etc. to aid portability
 */

/*
 *	IOCTL definitions
 */
#define	VGIOHALT	0x20007600		/* Halt display processor */
#define	VGIOSTART	0x80047601		/* Start display processor */

/*
 *  DISPLAY REGISTER DESIGNATIONS
 */
#define VGR_FS		000	/* Function switches unit 1 */
#define VGR_LTH		000	/* Function sw lamps 0-7 */
#define VGR_KBR		001	/* Keyboard character */
#define VGR_LTL		001	/* Function sw lamps 8-15 */
#define VGR_TIX		002	/* Tablet X input */
#define 	VGR_T_PRESS	01	/* PRESS is detected */
#define 	VGR_T_NEAR	02	/* NEAR is detected */
#define VGR_TIY		003	/* Tablet Y input */
#define VGR_PIR		004	/* Priority interrupt requests & name */
#define VGR_MCR		005	/* Mode & control (incl. intr enables) */
#define VGR_IR		006	/* Display instruction */
#define VGR_WCR		007	/* Word count */
#define VGR_XR		010	/* X coordinate */
#define VGR_YR		011	/* Y coordinate */
#define VGR_ZR		012	/* Z coordinate */
#define VGR_AIR		013	/* Autoincrement */
#define VGR_IOR		014	/* Intensity offset (dimming) */
#define VGR_ISR		015	/* Intensity scale (depth cueing) */
#define VGR_MAR		016	/* Memory fetch address */
#define VGR_SPR		017	/* Stack pointer */
#define VGR_TGR		020	/* Temp. general purpose */
#define VGR_PSR		021	/* Picture scale */
#define VGR_NMR		022	/* Name byte */
#define VGR_CSR		023	/* Coordinate scale */
#define VGR_DXR		024	/* Coordinate X displacement */
#define VGR_DYR		025	/* Coordinate Y displacement */
#define VGR_DZR		026	/* Coordinate Z displacement */
#define VGR_R11R	027
#define VGR_R12R	030
#define VGR_R13R	031
#define VGR_R21R	032
#define VGR_R22R	033
#define VGR_R23R	034
#define VGR_R31R	035
#define VGR_R32R	036
#define VGR_R33R	037
#define VGR_WMCR	040	/* Window mode control */
#define VGR_PDXR	047	/* Picture X dislplacement */
#define VGR_PDYR	050	/* Picture Y displacement */
#define VGR_PIRX	056	/* Extended devices priority intr requests */
#define VGR_MCRX	057	/* Extended devices interrupt enables */
#define VGR_PENR	060	/* Pen hit resolution count */
#define VGR_FS2		064	/* Function switches unit 2 */
#define VGR_LT2H	064	/* 2nd lights unit */

#define VGR_PX		0100	/* Picture X coordinate */
#define VGR_PY		0101	/* Picture Y coordinate */
#define VGR_PZ		0102	/* Picture Z coordinate */
#define VGR_JX		0103	/* Joystick X input */
#define VGR_JY		0104	/* Joystick Y input */
#define VGR_JZ		0105	/* Joystick Z input */
#define VGR_DIAL	0106	/* Start of 1st dial box: (dial n = DIAL+n) */
#define VGR_CXR		0120	/* Window acquisition X coordinate */
#define VGR_CYR		0121	/* Window acquisition Y coordinate */
#define VGR_CZR		0122	/* Window acquisition Z coordinate */
#define VGR_JX2		0123	/* Joystick #2 X input */
#define VGR_JY2		0124	/* Joystick #2 Y input */
#define VGR_JZ2		0125	/* Joystick #2 Z input */
#define VGR_CD11	0126	/* Start of second dial box */
#define VGR_TBX		0140	/* Track ball X coordinate */
#define VGR_TBY		0141	/* Track ball Y coordinate */

/*
 *  VG Instructions
 */
#define VGI_NOP		0
#define VGI_SPC		020000
#define VGI_CH		010017
#define VGI_CHI		010016
#define VGI_CH_BSWAP	004000	/* swap bytes in char data */
#define VGI_CHS0	0x0040	/* character size 0:  120x60 */
#define VGI_CHS1	0x0050	/* character size 1:   81x41 */
#define VGI_CHS2	0x0060	/* character size 2:   60x30 */
#define VGI_CHS3	0x0070	/* character size 3:   32x16 */
#define VGI_CH_END	000024	/* Draw characters end marker (character) */

#define VGI_VA		010004	/* Vector Absolute */
#define VGI_VR		010000	/* Vector relative display */
#define VA_DOTDASH	000120	/* dash-dot line */
#define VA_DOTTED	000040	/* dotted line */
#define VA_DASH		000020	/* dashed line */

#define VGI_SHOW(pt,coord,op)	((pt)<<4|(coord)|(op))	/* Compose a vector */
#define VGI_V_X		1	/* Vector Drawing:  X coordinate */
#define VGI_V_Y		2
#define VGI_V_Z		3
#define VGI_V_LOAD	00	/* Vector Drawing: Operation */
#define VGI_V_DRAW	04
#define VGI_V_MOVE	010
#define VGI_V_DT	014	/* (originally VGI_V_DRAWT) */
#define VGI_V_OPMASK	~014

/* Convert a float from [-1.0..+1.0] to a VG displaylist (<<4) coord. */
#define VGI_V_CVT(x)	( (displaylist_t)((float)(x) * 32767.0) & 0xFFF0 )

#define VGI_LD		040000
#define VGI_TERMINATE	000001		/* "Terminate" bit */
#define VGI_LDS		044000
#define VGI_STS		074000
#define VGI_STSM	074200
#define VGI_RTS		044016	/* Return from subroutine */

#define VGI_HALT	030000	/* Regular halt */
#define VGI_RHALT	034000	/* Special refresh halt */

/*
 *  The following macro converts a byte address into
 *  the word address of the format required to load the MAR
 *  or stack pointer from inside the display list.
 */
#define BATODLA(b)	( (b)<<3 & 0xFFF0  |  (b)>>12 & 0x000E )
			/* 3, not 4, to convert byte address to word addr */

/*
 * Format of /dev/vgrs0 (buffered and signalled register device)
 */
struct vgregb  {
	short	vg_fs1;		/* Function Switch Unit 1 */
	short	vg_fs2;		/* Function Switch Unit 1 */
	short	vg_tix;		/* Tablet X input */
	short	vg_tiy;		/* Tablet Y input */
	short	vg_jx;		/* Joystick X input */
	short	vg_jy;		/* Joystick Y input */
	short	vg_jz;		/* Joystick Z input */
	short	vg_d0;		/* Dial 0 */
	short	vg_d1;		/* Dial 1 */
	short	vg_d2;		/* Dial 2 */
	short	vg_d3;		/* Dial 3 */
	short	vg_d4;		/* Dial 4 */
	short	vg_d5;		/* Dial 5 */
	short	vg_d6;		/* Dial 6 */
	short	vg_d7;		/* Dial 7 */
	short	vg_d8;		/* Dial 8 */
	short	vg_d9;		/* Dial 9 */
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
