/*
	SCCS id:	@(#) rle.h	1.1
	Last edit: 	3/21/85 at 13:59:50	G S M
	Retrieved: 	8/13/86 at 10:30:50
	SCCS archive:	/m/cad/librle/RCS/s.rle.h

	Modified for by :	Gary S. Moss
				U. S. Army Ballistic Research Laboratory
				Aberdeen Proving Ground
				Maryland 21005-5066
				(301)278-6647 or AV-283-6647
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

/* Opcode definitions.							*/
#define	    RSkipLinesOp	1
#define	    RSetColorOp		2
#define	    RSkipPixelsOp	3
#define	    RByteDataOp		5
#define	    RRunDataOp		6

/* If this bit is set in inst.opcode, we have a long instruction.	*/
#define     LONG                0x40

typedef struct	/* Extended format RLE header.				*/
	{
	short	h_xpos, h_ypos;	/* Lower-left corner of image.		*/
        short	h_xlen, h_ylen;	/* Size of saved box.			*/
	char	h_flags;	/* Some flags.				*/
#define	H_CLEARFIRST	0x1	/* Clear the fb to background first.	*/
	char	h_ncolors;	/* Number of color channels.		*/
#define H_LD_CMAP_ONLY	0	/* Load only the color map (if present).*/
#define H_B_W		1	/* We have a black and white image.	*/
#define H_RGB		3	/* We have a normal color image.	*/
	char	h_pixelbits;	/* Bits per pixel per channel.		*/
	char	h_ncmap;	/* Channels in color map (map present).	*/
	char	h_cmaplen;	/* Log base 2 of entries in color map.	*/
	u_char  h_background[3];/* Background color.			*/
} Xtnd_Rle_Header;

typedef struct	/* Old format RLE header.				 */
	{
	short	magic;
	short	xpos, ypos;	/* Lower-left corner of image.		*/
	short	xsize, ysize;	/* Size of saved box.			*/
	u_char  bg_r;		/* Background colors.			*/
	u_char	bg_g;
	u_char	bg_b;
	char	map;		/* Flag for map presence.		*/
	} Rle_Header;

typedef struct /* Old RLE format instruction.				*/
	{
	unsigned short datum: 12, opcode:4;
	} Rle_Word;

/* Old RLE format magic numbers.					*/
#define	    RMAGIC	('R' << 8)	/* Top half of magic number.	*/
#define	    WMAGIC	('W' << 8)	/* Black&white rle image.	*/

#define	    XtndRMAGIC	((short)0xcc52) /* Extended RLE magic number.	*/

#define STRIDE (sizeof(Pixel))	/* Distance (bytes) to next pixel.	*/
#define _Get_Inst( _fp, _inst ) \
	(fread( (char *)_inst, sizeof(Rle_Word), 1, _fp ) != 1 ? EOF : 1)

/* Global data intended mainly for internal (library) use.		*/
extern int	_bg_flag;
extern int	_bw_flag;
extern int	_cm_flag;
extern Pixel	_bg_pixel;
