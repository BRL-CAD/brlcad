/*

	Modified by :		Gary S. Moss
				U. S. Army Ballistic Research Laboratory
				Aberdeen Proving Ground
				Maryland 21005-5066
				(301)278-6647 or AV-283-6647
 *
 *  Note:  Users of this file will need to include "fb.h" first.
 */
/*
 * Runsv.h - Definitions for Run Length Encoding.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 */

#include "machine.h"		/* for BIG_ENDIAN definition */

#ifdef BIG_ENDIAN
#define SWAB(shrt)  (shrt = ((shrt >> 8) & 0xff) | ((shrt << 8) & 0xff00))
#else
#define	SWAB(shrt)
#endif

/* Opcode definitions.							*/
#define	RSkipLinesOp	1
#define	RSetColorOp	2
#define	RSkipPixelsOp	3
#define	RByteDataOp	5
#define	RRunDataOp	6

/* If this bit is set in inst.opcode, we have a long instruction.	*/
#define LONG            0x40

/* Flags returned by 'rle_rhdr()'.					*/
#define NO_BOX_SAVE	(1<<0)	/* Saved with backgr.== H_CLEARFIRST	*/
#define NO_COLORMAP	(1<<1)	/* Color map not saved.			*/
#define NO_IMAGE	(1<<2)	/* Only color map saved.		*/

typedef struct	/* Extended format RLE header.				*/
	{
	short	h_xpos, h_ypos;	/* Lower-left corner of image.		*/
        short	h_xlen, h_ylen;	/* Size of saved box.			*/
	char	h_flags;	/* Some flags.				*/
#define	H_CLEARFIRST	0x1	/* Clear the fb to background first.	*/
#define H_BOXSAVE	0x0	/* Straight box save.			*/
	char	h_ncolors;	/* Number of color channels.		*/
#define H_LD_CMAP_ONLY	0	/* Load only the color map (if present).*/
#define H_B_W		1	/* We have a black and white image.	*/
#define H_RGB		3	/* We have a normal color image.	*/
	char	h_pixelbits;	/* Bits per pixel per channel.		*/
	char	h_ncmap;	/* Channels in color map (map present).	*/
	char	h_cmaplen;	/* Log base 2 of entries in color map.	*/
	unsigned char  h_background[3]; /* Background color.		*/
} Xtnd_Rle_Header;

typedef struct	/* Old format RLE header minus magic number field.	*/
	{
/***	short	magic;  Read seperately ***/
	short	xpos, ypos;	/* Lower-left corner of image.		*/
	short	xsize, ysize;	/* Size of saved box.			*/
	unsigned char 	bg_r;	/* Background colors.			*/
	unsigned char	bg_g;
	unsigned char	bg_b;
	char	map;		/* Flag for map presence.		*/
	} Old_Rle_Header;

typedef struct /* Old RLE format instruction.				*/
	{
#if __STDC__ || defined(__convexc__)
	/* XXX This won't match the file format, but will at least compile */
	/* ANSI insists that bit-field must be of type signed int, unsigned int or int */
	unsigned int datum:12, opcode:4;
#else
	unsigned short datum:12, opcode:4;
#endif
	} Old_Inst;

typedef struct /* Old RLE format instruction.				*/
	{
#if __STDC__ || defined(__convexc__)
	/* XXX This won't match the file format, but will at least compile */
	/* ANSI insists that bit-field must be of type signed int, unsigned int or int */
	int	opcode:8, datum:8;
#else
	short	opcode:8, datum:8;
#endif
	} Xtnd_Inst;

#define OPCODE(inst) (inst.opcode & ~LONG)
#define LONGP(inst) (inst.opcode & LONG)
#define DATUM(inst) (0x00ff & inst.datum)

/* Old RLE format magic numbers.					*/
#define	    RMAGIC	('R' << 8)	/* Top half of magic number.	*/
#define	    WMAGIC	('W' << 8)	/* Black&white rle image.	*/

#define	    XtndRMAGIC	((short)0xcc52) /* Extended RLE magic number.	*/

#define STRIDE (sizeof(RGBpixel))	/* Distance (bytes) to next pixel.	*/

/* Global data intended mainly for internal (library) use.		*/
extern int	_bg_flag;
extern int	_bw_flag;
extern int	_cm_flag;
extern RGBpixel	_bg_pixel;

/* Global flags for general use.					*/
extern int	rle_debug;
extern int	rle_verbose;

/* Entry points.							*/
void	rle_rlen(), rle_wlen(), rle_rpos(), rle_wpos();
int	rle_rhdr(), rle_whdr(), rle_rmap(), rle_wmap();
int	rle_decode_ln(), rle_encode_ln();

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
