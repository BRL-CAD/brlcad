/*                         M G D E V . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file mged/mgdev.h
 *
 * This file contains definitions for building displaylists for the Megatek,
 * and will ordinarily be included in all user programs which use the Megatek.
 *
 * The Megatek provides a 12-bit, 4096 x 4096 address space.
 * Megatek addresses are mapped from byte addresses
 * by shifting LEFT two bits (multiplying by 4) since the Megatek
 * words are 32 bits long (4 bytes).  For example, the color-lookup
 * table addresses start at FEC0.  To access this with lseek(),
 * it looks like:
 * err = lseek(rfd, ((long) 0xFEC0)<<2, 0);
 *
 */

/*
 * Special Addresses in MEGATEK display list
 */
#define MDL_CONTROL	0x0000FFF1L	/* Graphics Engine control */
#define MDL_COLORMAP	0x0000FEC0L	/* Color Lookup Table */
#define MDL_LIST_START	0x00000001L	/* display list START address */
#define MDL_FORG_START	0x00000003L	/* foreground START address */
#define MDL_FSCD	0x000000C0L	/* first FSCD control block */
#define MDL_ASEGTAB	0x00000200L	/* actual displaylist (01000) */

/*
 * Miscellaneous defines
 */
#define MEG_WORD(x, y)		(((long)(x)<<16)|((long)(y)&0xFFFFL))
#define MAKE(x, y, z, color) MEG_WORD(0177000, 0010000|((z)&0xFFF)), MEG_WORD(0100000|((x)&0xFFF), (color<<12)|(y&0xFFF))

/*
 * MEGATEK Instructions
 */
#define MGI_NO_XLATE MEG_WORD(0xFE80, 0x0000)	/* Reset translation */
#define MGI_RET MEG_WORD(0xFE40, 0x0000)	/* Return from subroutine */
#define MGI_END MEG_WORD(0xFE00, 0x4000)	/* End of display list */
#define MGI_NOP MEG_WORD(0xFE00, 0x0000)	/* No-Op */
/*#define MGI_ENABLE MEG_WORD(0xFE80, 0x0024)	/* enable display 1 */
#define MGI_ENABLE MEG_WORD(0xFE80, 0x0044)	/* enable display 1+2 */

#define MGI_SETORIGIN(x, y) MEG_WORD(0xD000|(x&0xFFF), 0x7000|(y&0xFFF))
#define MGI_SETCOLOR(c) MEG_WORD(0xFE10|(c&0x000F), 0x0000)
#define MGI_SETDASH(p) MEG_WORD(0xE008, (p&0xFFFF))

#define MGI_PA(x, y) MEG_WORD(0xC000|(x&0xFFF), 0x8000|(y&0xFFF))
#define MGI_PA_BLINK(x, y) MEG_WORD(0xD000|(x&0xFFF), 0x8000|(y&0xFFF))

/* Absolute vector instructions, set color and blink */
#define MGI_A_MOVE(x, y) MEG_WORD(0xC000|(x&0xFFF), 0x4000|(y&0xFFF))
#define MGI_A_DRAW(x, y, c) MEG_WORD(0x0000|(x&0xFFF), (c<<12)|(y&0xFFF))
#define MGI_A_DASH(x, y, c) MEG_WORD(0x2000|(x&0xFFF), (c<<12)|(y&0xFFF))

/* Absolute vector instruction, existing color and blink */
#define MGI_DRAW(x, y) MEG_WORD(0xC000|(x&0xFFF), 0xC000|(y&0xFFF))

#define MGI_Z_DRAW(z) MEG_WORD(0xFE00, 0xD000 | (z&0xFFF)) /* PREV+DRAW */
#define MGI_Z_MOVE(z) MEG_WORD(0xFE00, 0x5000 | (z&0xFFF)) /* PREV+BLANK*/
#define MGI_Z_NEXT(z) MEG_WORD(0xFE00, 0x9000 | (z&0xFFF)) /* NEXT+DRAW */

#define MGI_JMP(w_addr) MEG_WORD(0xFC00, w_addr)	/* absolute jump */
#define MGI_JMS(w_addr) MEG_WORD(0xFC80, w_addr)	/* subroutine call */

/*
 * Character display
 * NOTE that 2 characters go into the right halfword of the instruction.
 * The character list is terminated by a NULL byte.
 */
#define MGCH(size, color)	(short)(0xF600 | size | color)
#define MGCH_S0 0x00		/* 341x227 */
#define MGCH_S1 0x10		/* 170x113 */
#define MGCH_S2 0x20		/* 113x 75 */
#define MGCH_S3 0x30		/*  85x 56 */
#define MGCH_S4 0x40		/*  68x 45 */
#define MGCH_S5 0x50		/*  56x 37 */
#define MGCH_S6 0x60		/*  48x 32 */
#define MGCH_S7 0x70		/*  42x 28 */

/*
 * MEGATEK partial instructions (instructions are 32 bits long)
 */
#define DASH 0xF0F0		/* blink pattern for change attribute instr. */
#define DCHA 0xFB44		/* front half of change attributes instr. */

/*
 * Megatek Peripheral Bus Addresses
 */
#define MGP_FSCD 0x18		/* Function Switch & Control Dials */
#define MGP_IPCU 0x34		/* Intel. Peripheral Control Unit */
#define MGP_PICK 0x10		/* Pick Module */
#define MGP_PRINT 0x08		/* Rasterizor Print Function */
#define MGP_PLOT 0x09		/*   "  Plot */
#define MGP_SPEC 0x0A		/*   "  Special Function */
#define MGP_COPY 0x0B		/*   "  Screen Copy Function */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
