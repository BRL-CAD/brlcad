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
 * rle.h - Global declarations for Utah Raster Toolkit RLE programs.
 * 
 * Author:	Todd W. Fuqua
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sun Jul 29 1984
 * Copyright (c) 1984 Todd W. Fuqua
 * 
 * $Id$
 */

#ifndef RLE_H
#define RLE_H

enum rle_dispatch {
    NO_DISPATCH = -1,
    RUN_DISPATCH = 0
};

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
 * Defines for traditional channel numbers.
 */
#define	RLE_RED		0	/* Red channel traditionally here. */
#define RLE_GREEN	1	/* Green channel traditionally here. */
#define	RLE_BLUE	2	/* Blue channel traditionally here. */
#define RLE_ALPHA      -1	/* Alpha channel here. */

/*
 * Return values from rle_get_setup.
 */
#define	RLE_SUCCESS	 0
#define	RLE_NOT_RLE	-1
#define	RLE_NO_SPACE	-2
#define	RLE_EMPTY	-3
#define	RLE_EOF		-4

/*
 * TAG( rle_hdr )
 *
 * Definition of header structure used by RLE routines.
 */

#ifndef c_plusplus
typedef struct rle_hdr rle_hdr;
#endif

struct rle_hdr {
    enum 	rle_dispatch dispatch;	/* Type of file to create. */
    int	    	ncolors,	/* Number of color channels. */
	       *bg_color,	/* Pointer to bg color vector. */
	    	alpha,		/* If !0, save alpha channel. */
	    	background,	/* 0->just save all pixels, */
				/* 1->overlay, 2->clear to bg first. */
	    	xmin,		/* Lower X bound (left.) */
	    	xmax,		/* Upper X bound (right.) */
	    	ymin,		/* Lower Y bound (bottom.) */
	    	ymax,		/* Upper Y bound (top.) */
	    	ncmap,		/* Number of color channels in color map. */
				/* Map only saved if != 0. */
	    	cmaplen;	/* Log2 of color map length. */
    rle_map    *cmap;		/* Pointer to color map array. */
    const char **comments;	/* Pointer to array of pointers to comments. */
    FILE       *rle_file;	/* Input or output file. */
    /* 
     * Bit map of channels to read/save.  Indexed by (channel mod 256).
     * Alpha channel sets bit 255.
     * 
     * Indexing (0 <= c <= 255):
     *	    bits[c/8] & (1 << (c%8))
     */
#define RLE_SET_BIT(glob,bit) \
     ((glob).bits[((bit)&0xff)/8] |= (1<<((bit)&0x7)))
#define RLE_CLR_BIT(glob,bit) \
	((glob).bits[((bit)&0xff)/8] &= ~(1<<((bit)&0x7)))
#define RLE_BIT(glob,bit) \
	((glob).bits[((bit)&0xff)/8] & (1<<((bit)&0x7)))
    char    bits[256/8];
    /* 
     * Local storage for rle_getrow & rle_putrow.
     * rle_getrow has
     *	    scan_y	int	    current Y scanline.
     *	    vert_skip	int	    number of lines to skip.
     * rle_putrow has
     *	    nblank	int	    number of blank lines.
     *	    brun	short(*)[2] Array of background runs.
     *	    fileptr	long	    Position in output file.
     */
     union {
	struct {
	    int	scan_y,
		vert_skip;
	    char is_eof,	/* Set when EOF or EofOp encountered. */
		is_seek;	/* If true, can seek input file. */
	} get;
	struct {
	    int	nblank;
	    short (*brun)[2];
	    long fileptr;
	} put;
     } priv;
};

/* 
 * TAG( rle_dflt_hdr )
 *
 * Global variable with possibly useful default values.
 */
extern rle_hdr rle_dflt_hdr;


/* Declare RLE library routines. */

#ifdef USE_PROTOTYPES
    /* From rle_getrow.c */

    /*****************************************************************
     * TAG( rle_debug )
     * 
     * Turn RLE debugging on or off.
     */
    extern void rle_debug( int on_off );

    /*****************************************************************
     * TAG( rle_get_error )
     *
     * Print an error message based on the error code returned by
     * rle_get_setup.
     */
    extern int rle_get_error( int code,
			      const char *pgmname,
			      const char *fname );
			  
    /*****************************************************************
     * TAG( rle_get_setup )
     */
    extern int rle_get_setup( rle_hdr *the_hdr );

    /*****************************************************************
     * TAG( rle_get_setup_ok )
     *
     * Call rle_get_setup.  If it returns an error code, call
     * rle_get_error to print the error message, then exit with the error
     * code. 
     */
    extern void rle_get_setup_ok( rle_hdr *the_hdr,
				  const char *prog_name,
				  const char *file_name);

    /*****************************************************************
     * TAG( rle_getrow )
     *
     * Read a scanline worth of data from an RLE file.
     */
    extern int rle_getrow( rle_hdr * the_hdr, 
			   rle_pixel * scanline[] );

    /* From rle_getskip.c */

    /*****************************************************************
     * TAG( rle_getskip )
     * Skip a scanline, return the number of the next one.
     */
    extern unsigned int rle_getskip( rle_hdr *the_hdr );

    /* From rle_putrow.c. */

    /*****************************************************************
     * TAG( rgb_to_bw )
     *
     * Converts RGB data to gray data via the NTSC Y transform.
     */
    extern void rgb_to_bw( rle_pixel *red_row,
			   rle_pixel *green_row,
			   rle_pixel *blue_row,
			   rle_pixel *bw_row,
			   int rowlen );

    /*****************************************************************
     * TAG( rle_puteof )
     *
     * Write an End-of-image opcode to the RLE file.
     */
    extern void rle_puteof( rle_hdr *the_hdr );

    /*****************************************************************
     * TAG( rle_putrow )
     *
     * Write a scanline of data to the RLE file.
     */
    extern void rle_putrow( rle_pixel *rows[], int rowlen, rle_hdr *the_hdr );

    /*****************************************************************
     * TAG( rle_put_init )
     *
     * Initialize header for output, but don't write it to the file.
     */
    extern void rle_put_init( rle_hdr * the_hdr );

    /*****************************************************************
     * TAG( rle_put_setup )
     *
     * Write header information to a new RLE image file.
     */
    extern void rle_put_setup( rle_hdr * the_hdr );

    /*****************************************************************
     * TAG( rle_skiprow )
     *
     * Skip nrow scanlines in the output file.
     */
    extern void rle_skiprow( rle_hdr *the_hdr, int nrow );

    /* From rle_cp.c */
    /*****************************************************************
     * TAG( rle_cp )
     * Copy image data from input to output with minimal interpretation.
     */
    extern void rle_cp( rle_hdr *in_hdr, rle_hdr *out_hdr );

    /* From rle_row_alc.c. */
    /*****************************************************************
     * TAG( rle_row_alloc )
     *
     * Allocate scanline memory for use by rle_getrow.
     */
    extern int rle_row_alloc( rle_hdr * the_hdr,
			      rle_pixel *** scanp );

    /*****************************************************************
     * TAG( rle_row_free )
     *
     * Free the above.
     */
    extern void rle_row_free( rle_hdr *the_hdr, rle_pixel **scanp );

    /* From buildmap.c. */
    /* 
     * buildmap - build a more usable colormap from data in the_hdr struct.
     */
    extern rle_pixel **buildmap( rle_hdr *the_hdr,
				 int minmap,
				 double orig_gamma,
				 double new_gamma );

    /* From rle_getcom.c. */
    /*****************************************************************
     * TAG( rle_getcom )
     *
     * Get a specific comment from the image comments.
     */
    extern char * rle_getcom( const char * name, rle_hdr * the_hdr );

    /* From rle_putcom.c. */
    /*****************************************************************
     * TAG( rle_delcom )
     *
     * Delete a specific comment from the image comments.
     */
    extern const char *
    rle_delcom( const char * name, rle_hdr * the_hdr );

    /*****************************************************************
     * TAG( rle_putcom )
     * 
     * Put (or replace) a comment into the image comments.
     */
    extern const char *
    rle_putcom( const char * value, rle_hdr * the_hdr );

    /* From dither.c. */
    /*****************************************************************
     * TAG( bwdithermap )
     * Create a color map for ordered dithering in grays.
     */
    extern void bwdithermap( int levels, double _gamma, int bwmap[],
			     int divN[256], int modN[256], int magic[16][16] );
    /*****************************************************************
     * TAG( ditherbw )
     * Dither a gray-scale value.
     */
    extern int ditherbw( int x, int y, int val, 
			 int divN[256], int modN[256], int magic[16][16] );
    /*****************************************************************
     * TAG( dithergb )
     * Dither a color value.
     */
    extern int dithergb( int x, int y, int r, int g, int b,
			 int divN[256], int modN[256], int magic[16][16] );
    /*****************************************************************
     * TAG( dithermap )
     * Create a color map for ordered dithering in color.
     */
    extern void dithermap( int levels, double _gamma, int rgbmap[][3],
			   int divN[256], int modN[256], int magic[16][16] );
    /*****************************************************************
     * TAG( make_square )
     * Make a 16x16 magic square for ordered dithering.
     */
    extern void make_square( double N, int divN[256], int modN[256],
			     int magic[16][16] );

    /* From float_to_exp.c. */
    /*****************************************************************
     * TAG( float_to_exp )
     * Convert a list of floating point numbers to "exp" format.
     */
    extern void float_to_exp( int count, float * floats, rle_pixel * pixels );

    /* From rle_open_f.c. */
    /*****************************************************************
     * TAG( rle_open_f )
     *
     * Open an input/output file with default.
     */
    extern FILE *
    rle_open_f( const char *prog_name,
		const char *f_name,
		const char *mode );

    /*****************************************************************
     * TAG( rle_open_f_noexit )
     *
     * Open an input/output file with default.
     */
    extern FILE *
    rle_open_f_noexit( const char *prog_name,
		       const char *f_name,
		       const char *mode );

    /* From colorquant.c. */
    /*****************************************************************
     * TAG( colorquant )
     * Compute a colormap for quantizing an image to a limited set of colors.
     */
    extern int colorquant( rle_pixel *red, rle_pixel *green, rle_pixel *blue,
			   unsigned long pixels, rle_pixel *colormap[3],
			   int colors, int bits,
                         rle_pixel *rgbmap, int fast, int otherimages );

    /* From rle_addhist.c. */
    /*****************************************************************
     * TAG( rle_addhist )
     * Append history information to the HISTORY comment.
     */
extern void rle_addhist( char *argv[],
			 rle_hdr *in_hdr,
			 rle_hdr *out_hdr );

    /* From cmd_name.c. */
    /*****************************************************************
     * TAG( cmd_name )
     * Extract command name from argv.
     */
    extern char *cmd_name( char **argv );

    /* From scanargs.c. */
    /*****************************************************************
     * TAG( scanargs )
     * Scan command argument list and parse arguments.
     */
    extern int scanargs( int argc,
			 char **argv,
			 const char *format,
			 ... );

#ifdef NEED_BSTRING
    /* From bstring.c. */
    /*****************************************************************
     * TAG( bstring bzero )
     * 'Byte string' functions.
     */
    extern void bzero( char *str, int count );
    extern void bcopy( char *from, char *to, int count );
#endif
#else /* USE_PROTOTYPES */
    /* Return value decls for "K&R" C.  See above for full descriptions. */

    /* From rle_getrow.c. */
    extern void rle_debug();
    extern int rle_get_error();
    extern int rle_get_setup();
    extern void rle_get_setup_ok();
    extern int rle_getrow();

    /* From rle_getskip.c */
    extern unsigned int rle_getskip();

    /* From rle_putrow.c. */
    extern void rgb_to_bw();
    extern void rle_puteof();
    extern void rle_putrow();
    extern void rle_put_init();
    extern void rle_put_setup();
    extern void rle_skiprow();

    /* From rle_cp.c */
    extern void rle_cp();

    /* From rle_row_alc.c. */
    extern int rle_row_alloc();
    extern void rle_row_free();

    /* From buildmap.c. */
    extern rle_pixel **buildmap();
    
    /* From rle_getcom.c. */
    extern char *rle_getcom();

    /* From rle_putcom.c. */
    extern char *rle_delcom();
    extern char *rle_putcom();

    /* From dither.c. */
    extern void bwdithermap();
    extern int ditherbw();
    extern int dithergb();
    extern void dithermap();
    extern void magic4x4();
    extern void make_square();

    /* From float_to_exp.c. */
    extern void float_to_exp();

    /* From rle_open_f.c. */
    extern FILE *rle_open_f();
    extern FILE *rle_open_f_noexit();

    /* From colorquant.c. */
    extern int colorquant();

    /* From rle_addhist.c. */
    extern void rle_addhist();

    /* From cmd_name.c. */
    extern char *cmd_name();

    /* From scanargs.c. */
    extern int scanargs();
#ifdef NEED_BSTRING
    /* From bstring.c. */
    extern void bzero();
    extern void bcopy();
#endif
#endif /* USE_PROTOTYPES */

#endif /* RLE_H */
