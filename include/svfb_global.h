/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * svfb_global.h - externally visible variables for svfb.
 * 
 * Author:	Todd W. Fuqua
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sun Jul 29 1984
 * Copyright (c) 1984 Todd W. Fuqua
 * 
 * $Header$
 */

enum sv_dispatch {
    RUN_DISPATCH
};

/* On BIGENDIAN machines swap the bytes. Everything but vax's and
 * pdp-11's (not sure if it's pdp11 or PDP11 ??)
 * are considered BIGENDIAN machines.
 */
#if !defined(vax) && !defined(pdp11)
#define SWAB(shrt)  (shrt = ((shrt >> 8) & 0xff) | ((shrt << 8) & 0xff00))
#else
#define	SWAB(shrt)
#endif

/* ****************************************************************
 * TAG( rle_pixel rle_map )
 *
 * Typedef for 8-bit (or less) pixel data.
 *
 * Typedef for 16-bit color map data.
 */
typedef unsigned char rle_pixel;
typedef unsigned short rle_map;

/*
 * Defines for traditional channel numbers
 */
#define	SV_RED	    0		/* red channel traditionally here */
#define SV_GREEN    1		/* green channel traditionally here */
#define	SV_BLUE	    2		/* blue channel traditionally here */
#define SV_ALPHA    -1		/* Alpha channel here */

/*
 * Return values from rle_get_setup
 */
#define	RLE_SUCCESS	0
#define	RLE_NOT_RLE	-1
#define	RLE_NO_SPACE	-2
#define	RLE_EMPTY	-3
#define	RLE_EOF		-4

/*
 * TAG( sv_globals )
 *
 * Definition of "globals" structure used by RLE routines
 */

extern struct sv_globals {
    enum sv_dispatch sv_dispatch; /* type of file to create */
    int	    sv_ncolors,		/* number of color channels */
	  * sv_bg_color,	/* pointer to bg color vector */
	    sv_alpha,		/* if !0, save alpha channel */
	    sv_background,	/* (background) 0->just save pixels, */
				/* 1->overlay, 2->clear to bg first */
	    sv_xmin,		/* lower X bound (left) */
	    sv_xmax,		/* upper X bound (right) */
	    sv_ymin,		/* lower Y bound (bottom) */
	    sv_ymax,		/* upper Y bound (top) */
	    sv_ncmap,		/* number of color channels in color map */
				/* map only saved if != 0 */
	    sv_cmaplen;		/* log2 of color map length */
    rle_map * sv_cmap;	/* pointer to color map array */
    char    ** sv_comments;	/* pointer to array of pointers to comments */
    FILE  * svfb_fd;		/* output file */
    /* 
     * Bit map of channels to read/save.  Indexed by (channel mod 256).
     * Alpha channel sets bit 255.
     * 
     * Indexing (0 <= c <= 255):
     *	    sv_bits[c/8] & (1 << (c%8))
     */
#define SV_SET_BIT(glob,bit) \
     ((glob).sv_bits[((bit)&0xff)/8] |= (1<<((bit)&0x7)))
#define SV_CLR_BIT(glob,bit) \
	((glob).sv_bits[((bit)&0xff)/8] &= ~(1<<((bit)&0x7)))
#define SV_BIT(glob,bit) \
	((glob).sv_bits[((bit)&0xff)/8] & (1<<((bit)&0x7)))
    char    sv_bits[256/8];
    /* 
     * Local storage for rle_getrow & sv_putrow.
     * rle_getrow has
     *	    scan_y	int	    current Y scanline
     *	    vert_skip	int	    number of lines to skip
     * sv_putrow has
     *	    nblank	int	    number of blank lines
     *	    brun	short(*)[2] Array of background runs.
     *	    fileptr	long	    Position in output file.
     */
     union {
	struct {
	    int	scan_y,
		vert_skip;
	    char is_eof,	/* Set when EOF or EofOp encountered */
		is_seek;	/* If true, can seek input file */
	} get;
	struct {
	    int	nblank;
	    short (*brun)[2];
	    long fileptr;
	} put;
     } sv_private;
} sv_globals;


/* 
 * buildmap - build a more usable colormap from data in globals struct.
 */
extern rle_pixel **
buildmap();
/* ( globals, minmap, gamma )
 * struct sv_globals * globals;
 * int minmap;
 * double gamma;
 */

/*
 * rle_getcom - get a specific comment from the image comments.
 */
extern char *
rle_getcom();
/* ( name, globals )
 * char * name;
 * struct sv_globals * globals;
 */

/*
 * rle_putcom - put (or replace) a comment into the image comments.
 */
extern char *
rle_putcom();
/* ( value, globals )
 * char * value;
 * struct sv_globals * globals;
 */

/*
 * rle_delcom - delete a specific comment from the image comments.
 */
extern char *
rle_delcom();
/* ( name, globals )
 * char * name;
 * struct sv_globals * globals;
 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
