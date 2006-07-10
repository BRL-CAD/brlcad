/*                         F B _ I K . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file fb_ik.h
	SCCS id:	@(#) fb_ik.h	1.14
	Last edit: 	8/6/86 at 13:56:23
	Retrieved: 	8/6/86 at 13:56:57
	SCCS archive:	/vld/src/libfb/s.fb_ik.h

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/

#ifndef __FB_IK_H__
#define __FB_IK_H__

#ifdef vax
/* GSM : '' needed for Sys V emulation.					*/
#  ifdef BSD
#    define IKIOGETADDR	_IOR(I,13,caddr_t)/* addr of mapped UNIBUS regs */
#  else
#    define IKIOGETADDR	_IOR('I',13,caddr_t)/* addr of mapped UNIBUS regs */
#  endif
#else
#  define IKIOGETADDR	0	/**** Bogus for now ****/
#endif

/* useful macros for forming multibit values from smaller operands */

#define D1616(a,b) (((((long)a)&0177777)<<16)|(((long)b)&0177777))
#define D1410(a,b) (((((long)a)&037777)<<10)|(((long)b)&01777))
#define D101010(a,b,c) (((((((long)a)&01777)<<10)|(((long)b)&01777))<<10|(((long)c)&01777)))
#define D888(a,b,c) (((((((long)a)&0377)<<8)|(((long)b)&0377))<<8|(((long)c)&0377)))

#define RGB10(r,g,b) D101010(b,g,r)	/* Includes reversal!! */
#define RGB8(r,g,b) D888(b,g,r)

#define IK(h,l) D1410(h,l)
#define XY16(x,y) D1616(y,x)

/* definitions of important addresses in RDS-3000 system */

#define DR256	IK(0,0)		/* beginning of lo/hi res dr memory */
#define DR256W	IK(01000,0)	/* beginning of word res dr memory */
#define FBC	IK(030000,0)	/* beginning of FBC register block */
#define FBCVPL	IK(030000,0)	/* fbc viewport location (y,x) */
#define FBCVPS	IK(030000,1)	/* fbc viewport size (y,x) */
#define FBCWL	IK(030000,2)	/* fbc window location (y,x) */
#define FBCZOOM	IK(030000,3)	/* fbc zoom factor (y,x) */
#define FBCDRC	IK(030000,4)	/* fbc display rate control */
#define FBCVC	IK(030000,5)	/* fbc video control */
#define FBCCL	IK(030000,6)	/* cursor (y,x) location registers */
#define FBCCD	IK(030000,0400)	/* cursor bit map definition */
#define XBS	IK(030200,0)	/* beginning of 34 xbs registers */
#define LUVO	IK(020300,0)	/* beginning of luvo color tables */
#define LUVOXBS	IK(020301,0)	/* luvo channel crossbar switch */
#define BPS	IK(020500,0)	/* beginning of bps registers */
#define BPSPC	IK(020500,0)	/* bps program counter (read only) */
#define BPSSTAT	IK(020500,0)	/* bps status register (write only) */
#define SR	IK(020200,0)	/* beginnig of scratch pad registers */
#define MCM	IK(020000,0)	/* beginning of microcode memory */
#define MA	IK(020400,0)	/* beginning of multiplier/accumulator */
#define MACM	IK(020400,0)	/* ma coefficient memory */
#define MAMPM	IK(020401,0)	/* ma microprogram memory */
#define MAPA	IK(020402,0)	/* ma program address registers */
#define MAIOLIST IK(020402,1)	/* ma input/output list offset */
#define MALPCNT	IK(020402,2)	/* ma loop 0 counter */
#define MAZRANGE IK(020402,3)	/* ma zrange for clipping */
#define MARESULT IK(020403,0)	/* ma result registers */
#define MAXYREG	IK(020403,0)	/* ma x/y register */
#define MAZSHADE IK(020403,1)	/* ma z/shade register */
#define MAWREG	IK(020403,2)	/* ma w register */
#define MAPC	IK(020403,3)	/* pc readback register */
#define MASTART	IK(020402,7)	/* ma program start trigger */
#define MPC	IK(034000,0)	/* microprocessor controller */
#define MPCRAM	IK(034000,0)	/* microprocessor ram */
#define MPCIO	IK(034040,0)	/* microprocessor i/o space */
#define VI	IK(030100,0)	/* video input */
#define VIVPL	IK(030100,0)	/* vi viewport location (y,x) */
#define VIMF	IK(030100,1)	/* vi minification factor (y,x) */
#define VIWL	IK(030100,2)	/* vi window location (y,x) */
#define VIVPS	IK(030100,3)	/* vi viewport size (y,x) */
#define VICR	IK(030100,4)	/* vi control register */
#define VISFREQ	IK(030100,5)	/* vi sampling frequency */
#define CGM	IK(020100,0)	/* character generator module */
#define CGMFT	IK(020100,0)	/* cgm font table */
#define CGMS	IK(020200,0200)	/* cgm string */
#define CGMBCB	IK(020600,0)	/* cgm base control block */

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Definitions relating to the Ikonas FBC (frame buffer controller).
 * This group of registers is write-only, and resides at
 * Ikonas address 30,000$0 (FBC).
 *
 * Mike Muuss, BRL, 10/26/83.
 */
struct ik_fbc  {
	short	fbc_xviewport, fbc_yviewport;		/* Y<<16 | X */
	short	fbc_xsizeview, fbc_ysizeview;
	short	fbc_xwindow, fbc_ywindow;
	short	fbc_xzoom, fbc_yzoom;
	short	fbc_horiztime, fbc_nlines;
	short	fbc_Lcontrol, fbc_Hcontrol;
	short	fbc_xcursor, fbc_ycursor;
};

/* Definitions for features in fbc_Lcontrol */
#define FBC_CURSOR	(1<<2)		/* Set to turn cursor ON */
#define FBC_HIRES	(1<<3)		/* Set gives HIRES, else LORES */
#define FBC_AUTOCLEAR	(1<<5)		/* write only -- clear image mem */
#define FBC_EXTERNSYNC	(1<<6)		/* set=EXTERN, else horiztime+nlines */
#define FBC_CMAP2	(1<<7)		/* select 2nd color map */
#define FBC_RS343	(1<<9)		/* set for RS343, else RS170 */
#define FBC_NOINTERLACE	(1<<10)		/* set=repeat field, else interlace */
/* Definitions for features in fbc_Hcontrol */
#define FBCH_PIXELCLOCK(r) ((r)<<(16-16))  /* nanoseconds/pixel, 16-22 */
#define FBCH_EXTPIXELCLK	(1<<(22-16))		/* ?? */
#define FBCH_PROGSYN	(1<<(23-16))		/* ?? */
#define FBCH_DRIVEBPCK	(1<<(24-16))		/* ?? */

/* Seemingly, some small omissions from the interface command list */
#define IKWRITE 020
#define IKREAD	000

#ifdef vax
/* some CSR bits */
#  define IKPIX		2	/* Pixel transfer mode */
#  define IKINCR		0400	/* increment Ikonas address (Icsr) */
#  define IKHIRES		1	/* Image is in hi-res mode */
#  define IK_IO_INHIBIT	0200	/* Set invisible I/O.			*/
/* GSM : /usr/5include/sys/types.h uses 'ushort'.			*/
struct ikdevice {
	short  ubwcount;	/* unibus word count (two's complement) */
	unsigned short ubaddr;		/* unibus address register (must be even) */
	unsigned short ubcomreg;	/* unibus status & command register */
	unsigned short datareg;		/* data i/o register */
	unsigned short ikloaddr;	/* ikonas lower address register */
	unsigned short ikhiaddr;	/* ikonas upper address register */
	unsigned short ikcomreg;	/* ikonas status & command register */
};
#  define IKREADY 0000200
#  define IKERROR	0100000
#endif vax

#ifdef vax
#  define MAX_IK_DMA (64*1024L)		/* max # of bytes/dma */
#else
#  define MAX_IK_DMA (16*1024L)
#endif
#define MAX_IK_PIX_DMA (MAX_IK_DMA/4L)

#define X_CURSOR_OFFSET		(-15)
#define Y_CURSOR_OFFSET		15

#endif  /* __FB_IK_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
