/*
 *		I F _ S U N . C 
 *
 *
 *  Authors -
 *	Bill Lindemann
 *	Michael John Muuss
 *	Phil Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pixrect/pixrect_hs.h>
#include <sunwindow/window_hs.h>

#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int 
sun_dopen(),
sun_dclose(),
sun_dclear(),
sun_bread(),
sun_bwrite(),
sun_cmread(),
sun_cmwrite(),
sun_viewport_set(),
sun_window_set(),
sun_zoom_set(),
sun_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO            sun_interface = {
				 sun_dopen,
				 sun_dclose,
				 fb_null,	/* reset? */
				 sun_dclear,
				 sun_bread,
				 sun_bwrite,
				 sun_cmread,
				 sun_cmwrite,
				 sun_viewport_set,
				 sun_window_set,
				 sun_zoom_set,
				 fb_null,	/* sun_curs_set, */
				 fb_null,	/* sun_cmemory_addr */
				 fb_null,	/* cscreen_addr */
				 "SUN Pixwin",
				 1152,	/* max width */
				 896,	/* max height */
				 "/dev/sun",
				 512,	/* current/default width  */
				 512,	/* current/default height */
				 -1,	/* file descriptor */
				 PIXEL_NULL,	/* page_base */
				 PIXEL_NULL,	/* page_curp */
				 PIXEL_NULL,	/* page_endp */
				 -1,	/* page_no */
				 0,	/* page_ref */
				 0L,	/* page_curpos */
				 0L,	/* page_pixels */
				 0	/* debug */
};


Pixwin		*sun_pw;
int             sun_win_fd;
int             sun_xzoom = 1;
int             sun_yzoom = 1;
int             sun_damaged;	/* window has incurred damage */
int             dither_flg = 0;	/* dither output */
Rect            master_rect;
int		sun_depth;

/* One scanline wide buffer */
extern struct pixrectops mem_ops;
char		sun_mpr_buf[1024];
mpr_static( sun_mpr, 1024, 1, 16, (short *)sun_mpr_buf );

/* dither pattern (threshold level) */
static short	dither[8][8] =
{
	6,  51,  14,  78,   8,  57,  16,  86,
	118,  22, 178,  34, 130,  25, 197,  37,
	18,  96,  10,  63,  20, 106,  12,  70,
	219,  42, 145,  27, 243,  46, 160,  30,
	9,  60,  17,  91,   7,  54,  15,  82,
	137,  26, 208,  40, 124,  23, 187,  36,
	21, 112,  13,  74,  19, 101,  11,  66,
	254,  49, 169,  32, 230,  44, 152,  29
};

#define NDITHER   8
#define LNDITHER  3
#define EXTERN extern

unsigned char   red8Amat[NDITHER][NDITHER];	/* red dither matrix */
unsigned char   grn8Amat[NDITHER][NDITHER];	/* green matrix */
unsigned char   blu8Amat[NDITHER][NDITHER];	/* blue  matrix */

typedef unsigned char uchar;

#ifdef DIT
EXTERN struct pixrect *redscr;	/* red screen */
EXTERN struct pixrect *grnscr;	/* green screen */
EXTERN struct pixrect *bluscr;	/* blue screen */
#endif DIT

EXTERN struct pixrect *redmem;	/* red memory pixrect */
EXTERN struct pixrect *grnmem;	/* grn memory pixrect */
EXTERN struct pixrect *blumem;	/* blu memory pixrect */
EXTERN unsigned char others[256];	/* unused colors */
EXTERN unsigned char redmap8[256];	/* red 8bit color map */
EXTERN unsigned char grnmap8[256];	/* green 8bit color map */
EXTERN unsigned char blumap8[256];	/* blue 8bit color map */

EXTERN int      fb_type;	/* 8 = 8 bit, 24 = 24 bit */
EXTERN struct pixfont *myfont;	/* font to label with */

EXTERN int      zbuf;		/* 1 = yes, 0 = no */
EXTERN int      antialias;	/* 1 = yes, 0 = no */
unsigned char   others[256];	/* unused colors */
unsigned char   red8Amap[256];	/* red 8bit dither color map */
unsigned char   grn8Amap[256];	/* green 8bit dither color map */
unsigned char   blu8Amap[256];	/* blue 8bit dither color map */

#define RR	6
#define GR	7
#define BR	6

static unsigned char redmap[256], grnmap[256], blumap[256];

static int      biggest = RR * GR * BR - 1;

#define COLOR_APPROX(p) \
	(((*p)[RED] * RR ) / 256) * GR*BR + \
	(((*p)[GRN] * GR ) / 256) * BR  + \
	(((*p)[BLU] * BR ) / 256) + 1

sun_sigwinch()
{
	sun_damaged = 1;
}

sun_repair()
{
	if (sun_damaged) {
		pw_damaged(sun_pw);
		pw_repairretained(sun_pw);
		pw_donedamaged(sun_pw);
		sun_damaged = 0;
	}
}

/*
 *			S U N _ D O P E N 
 */
_LOCAL_ int
sun_dopen(ifp, file, width, height)
	FBIO           *ifp;
	char           *file;
	int             width, height;
{
	char            gfxwinname[128];	/* Name of window to use */
	int             i;
	int             x;

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	if (sun_pw != (Pixwin *) 0) {
		fb_log("sun_open, already open\n");
		return(-1);	/* FAIL */
	}

	if( we_getgfxwindow(gfxwinname) == 0 )  {
		int             gfxfd;

		/* Running under SunView, with windows */
		if( (sun_win_fd = win_getnewwindow()) < 0 )  {
			fb_log("sun_dopen:  win_getnewwindow returned %d\n", sun_win_fd);
			return(-1);	/* FAIL */
		}
		
		/* The blanket window covers current GFX window */
		gfxfd = open(gfxwinname, 2);
		win_insertblanket(sun_win_fd, gfxfd);
	} else {
		struct screen sun_screen;
		Pixrect *screen_pr;

		/* Create root window on raw screen */
		bzero( (char *)&sun_screen, sizeof(sun_screen) );
		strcpy( sun_screen.scr_kbdname, "NONE" );
		strcpy( sun_screen.scr_msname, "NONE" );
		if( (sun_win_fd = win_screennew( &sun_screen )) < 0 )  {
			fb_log("sun_dopen: win_screennew returned %d\n", sun_win_fd);
			return(-1);	/* FAIL */
		}
	}
	sun_pw = pw_open(sun_win_fd);
	win_getsize(sun_win_fd, &master_rect);

	if( width > master_rect.r_width )
		width = master_rect.r_width;
	if( height > master_rect.r_height )
		height = master_rect.r_height;

	sun_depth = sun_pw->pw_pixrect->pr_depth;
	sun_pw->pw_prretained = mem_create(width, height, sun_depth);
	sun_mpr.pr_depth = sun_depth;
	if( sun_depth < 8 )  {
/**		pr_whiteonblack( sun_pw->pw_pixrect, 0, 1 );**/
	} else {
		if (dither_flg) {
#ifdef DIT
			draw8Ainit();
			dither8Ainit();
			pw_set8Amap(sun_pw, &sun_cmap);
#endif DIT
		} else {
			/* r | g | b, values = RR, GR, BR */
			/* set a new cms name; initialize it */
			x = pw_setcmsname(sun_pw, "libfb");
			fb_log("setcmsname ret %d\n", x);
			for (x = 0; x < (RR * GR * BR); x++) {
				int             new;
				RGBpixel        q;
				RGBpixel       *qq = (RGBpixel *) q;

				blumap[x + 1] = ((x % BR)) * (255 / (BR - 1));
				grnmap[x + 1] = (((x / BR) % GR)) * (255 / (GR - 1));
				redmap[x + 1] = ((x / (BR * GR))) * (255 / (RR - 1));
				q[RED] = redmap[x + 1];
				q[GRN] = grnmap[x + 1];
				q[BLU] = blumap[x + 1];
				new = COLOR_APPROX(qq);
			}
			x = pw_putcolormap(sun_pw, 0, 256, redmap, grnmap, blumap);
			fb_log("colormap ret %d\n", x);
		}
	}

	/* Set entire area to background color */
	/* SUN reserves [0] for white (background), [1] for black (fg) */
	pw_rop(sun_pw, 0, 0, master_rect.r_width, master_rect.r_height,
	       PIX_SRC | PIX_COLOR(1), (Pixrect *) 0, 0, 0);

	signal(SIGWINCH, sun_sigwinch);
	return (0);		/* "Success" */
}

/*
 * S U N _ D C L O S E 
 */
_LOCAL_ int
sun_dclose(ifp)
	FBIO           *ifp;
{
	char            ch;
	Pixrect        *pr_out;
	FILE           *ofp;

	sun_repair();
	pw_close(sun_pw);
	sun_pw = (Pixwin *) 0;
}

/*
 * S U N _ D C L E A R 
 */
_LOCAL_ int
sun_dclear(ifp, pp)
	FBIO           *ifp;
	register RGBpixel *pp;
{
	sun_repair();
}

/*
 * S U N _ W I N D O W _ S E T 
 */
_LOCAL_ int
sun_window_set(ifp, x, y)
	FBIO           *ifp;
	int             x, y;
{
	sun_repair();
}

/*
 * S U N _ Z O O M _ S E T 
 */
_LOCAL_ int
sun_zoom_set(ifp, xpts, ypts)
	FBIO           *ifp;
	int             xpts, ypts;
{
	sun_repair();
	sun_xzoom = xpts;
	sun_yzoom = ypts;
}

/*
 *			S U N _ B R E A D 
 */
_LOCAL_ int
sun_bread(ifp, x, y, p, count)
	FBIO           *ifp;
	int             x, y;
	register RGBpixel *p;
	int             count;
{
	return(count);
}

/*
 *			S U N _ B W R I T E
 */
_LOCAL_ int
sun_bwrite(ifp, x, y, p, count)
	FBIO           *ifp;
	int             x, y;
	register RGBpixel *p;
	int             count;
{
	register int    cx, cy;
	register int    value;
	register int	i;
	register int    cnt;
	int		needflush = 0;

	sun_repair();
	cx = x;
	cy = master_rect.r_height - 1 - y;
	if (cy < 0 || cy >= master_rect.r_height)
		return;

	if( count == 1 )  {
		if( sun_depth < 8 )  {
			/* 0 gives white, 1 gives black */
			value = ((*p)[RED] + (*p)[GRN] + (*p)[BLU]);
			if( value < dither[(cx&07)][cy&07]*3 )
				pw_put( sun_pw, cx, cy, 1 );
		} else {
			value = COLOR_APPROX(p);
			pw_put( sun_pw, cx, cy, value );
		}
		return(1);
	}
	/* This code has problems if only part of a line is being written */

	bzero( sun_mpr_buf, ifp->if_width * sun_depth / sizeof(char) );
	pw_lock( sun_pw, sun_pw->pw_pixrect );
	for (cnt = count; --cnt >= 0; p++) {
		for( i=sun_xzoom; i-- > 0; cx++)  {
			if( sun_depth < 8 )  {
				register short xxx;
				/* 0 gives white, 1 gives black */
				value = ((*p)[RED] + (*p)[GRN] + (*p)[BLU]);
				if( value < dither[xxx=(cx&07)][cy&07]*3 )
					sun_mpr_buf[cx>>3] |= 0x80 >> xxx;
			} else {
				value = COLOR_APPROX(p);
				sun_mpr_buf[cx] = value;
			}
		}
		needflush = 1;
		if (cx >= ifp->if_width) {
			for( i=sun_yzoom; i-- > 0; cy-- )  {
				pw_rop( sun_pw, x, cy,
					cx-x, 1,
					PIX_SRC, &sun_mpr, x, 0 );
				x = cx = 0;
			}
			bzero( sun_mpr_buf, ifp->if_width * sun_depth / sizeof(char) );
			needflush = 0;
		}
	}
	if(needflush)  {
		for( i=sun_yzoom; i-- > 0; cy-- )  {
			pw_rop( sun_pw, x, cy,
				cx-x, 1,
				PIX_SRC, &sun_mpr, x, 0 );
			x = cx = 0;
		}
	}
	pw_unlock( sun_pw );
	return(count);
}

sun_put(pw, vx, vy, p)
	Pixwin         *pw;
	int             vx, vy;
	register RGBpixel *p;
{
	int             cx, cy;
	int             fx, fy;	/* framebuffer x,y */
	int             xcnt, ycnt;
	int             value;

	fx = vx * sun_xzoom;
	fy = vy * sun_yzoom;
	if (dither_flg) {
#ifdef DIT
		for (ycnt = sun_yzoom; --ycnt >= 0;) {
			for (xcnt = sun_xzoom; --xcnt >= 0;) {
				cx = fx + xcnt;
				cy = fy + ycnt;
				value = get_dither8Abit(cx, cy, (*p)[RED], (*p)[GRN], (*p)[BLU]);
				pw_put(pw, cx, cy, value);
			}
		}
#endif DIT
	} else {
		value = ((((((*p)[RED] * (RR)) >> 8) * GR) +
			  (((*p)[GRN] * (GR)) >> 8)) * BR
			 + (((*p)[BLU] * (BR)) >> 8));
		/**	value = ((*p)[RED] + (*p)[GRN] + (*p)[BLU]) / 3; **/
		pw_rop(pw, fx, fy, sun_xzoom, sun_yzoom,
		       PIX_SRC | PIX_COLOR(value), (Pixrect *) 0, 0, 0);
	}
}

/*
 * S U N _ V I E W P O R T _ S E T 
 */
_LOCAL_ int
sun_viewport_set()
{
}

/*
 * S U N _ C M R E A D 
 */
_LOCAL_ int
sun_cmread()
{
}

/*
 * S U N _ C M W R I T E 
 */
_LOCAL_ int
sun_cmwrite(ifp, cmp)
	register FBIO  *ifp;
	register ColorMap *cmp;
{
	sun_repair();
}

/* --------------------------------------------------------------- */

#ifdef DIT
/*
 * draw8Abit.c 
 *
 * By: David H. Elrod;  Sun Microsystems; September 1986 
 *
 * Draw a pixel in 8 bit color space using a color cube that is 6values red, 
 * 7values green and 6values blue. 
 *
 * External Variables Used: redscr, grnscr, bluscr	- red, green and blue
 * pixrects red8Amap, grn8Amap, blue8Amap	- software color map 
 *
 * Bugs: 
 *
 */

draw8Abit(x, y, r, g, b)
	int             x, y;	/* pixel location */
	unsigned char   r, g, b;/* red, green, blue pixel values */
{
	int             red, green, blue;	/* return values */
	int             v;	/* 8 bit value */

	v = biggest - (((((r * (RR)) >> 8) * GR) + ((g * (GR)) >> 8)) * BR
		       + ((b * (BR)) >> 8));

	/* deal with 24 bit frame buffer */
	red = pr_put(redscr, x, y, red8Amap[v]);
	green = pr_put(grnscr, x, y, grn8Amap[v]);
	blue = pr_put(bluscr, x, y, blu8Amap[v]);

	if ((red == PIX_ERR) || (green == PIX_ERR) || (blue == PIX_ERR))
		return (PIX_ERR);
	return (0);
}
#endif DIT

draw8Ainit()
{
	int             i, r, g, b;	/* loop counters */

	/* ordered dither matrix (6 reds, 7 greens and 6 blues) */
	i = 0;
	for (r = 0; r < RR; r++)
		for (g = 0; g < GR; g++)
			for (b = 0; b < BR; b++) {
				red8Amap[i] = 255 - (r * 255 / (RR - 1));
				grn8Amap[i] = 255 - (g * 255 / (GR - 1));
				blu8Amap[i] = 255 - (b * 255 / (BR - 1));
				i++;
			}
}

/*
 * pw_dither8Abit.c 
 *
 * Modified:	Bill Lindemann;	Sun Microsystems; September 1986 From
 * original by: David H. Elrod;  Sun Microsystems; September 1986 
 *
 * Display a pixel using an ordered dither algoritm to approximate the 24 bit
 * rgb value supplied.  Convert this value to an 8 bit system, and display in
 * the given pixwin.  Assume the colormap is already set. 
 *
 * External Variables Used: red8Amat, grn8Amat, blu8Amat	- dither matricies; 
 *
 * Bugs: 
 *
 */
#ifdef DIT
pw_dither8Abit(pw, x, y, r, g, b)
	Pixwin         *pw;
	int             x, y;	/* pixel location */
	unsigned char   r, g, b;/* red, green, blue pixel values */
{
	int             red, green, blue;	/* return values */
	int             v;	/* 8 bit value */

	v = biggest - ((dit8A(r, red8Amat, RR - 1, x, y) * GR +
			dit8A(g, grn8Amat, GR - 1, x, y)) * BR +
		       dit8A(b, blu8Amat, BR - 1, x, y));

	pw_put(pw, x, y, v);

	if ((red == PIX_ERR) || (green == PIX_ERR) || (blue == PIX_ERR))
		return (PIX_ERR);
	return (0);
}

get_dither8Abit(x, y, r, g, b)
	int             x, y;	/* pixel location */
	unsigned char   r, g, b;/* red, green, blue pixel values */
{
	int             v;	/* 8 bit value */

	v = biggest - ((dit8A(r, red8Amat, RR - 1, x, y) * GR +
			dit8A(g, grn8Amat, GR - 1, x, y)) * BR +
		       dit8A(b, blu8Amat, BR - 1, x, y));

	return (v);
}

pw_set8Amap(pw, cmap)
	Pixwin         *pw;
	colormap_t     *cmap;
{
	pw_setcmsname(pw, "dith8Amap");
	pw_putcolormap(pw, 0, biggest + 1, red8Amap, grn8Amap, blu8Amap);
	if (cmap != (colormap_t *) 0) {
		cmap->type = RMT_EQUAL_RGB;
		cmap->length = biggest + 1;
		cmap->map[0] = red8Amap;
		cmap->map[1] = grn8Amap;
		cmap->map[2] = blu8Amap;
	}
}

pw_dither8Abit_rop(pw, pr_red, pr_grn, pr_blu, size)
	Pixwin         *pw;
	Pixrect        *pr_red, *pr_grn, *pr_blu;
	int             size;
{
	register unsigned char *redP, *grnP, *bluP, *compP;
	register int    x, y;
	Pixrect        *pr_comp;
	struct mpr_data *mpr_red, *mpr_grn, *mpr_blu, *mpr_comp;
	unsigned char  *red_base, *grn_base, *blu_base, *comp_base;

	pr_comp = mem_create(size, size, 8);
	if (pr_comp == (Pixrect *) 0) {
		(void) printf(stderr, "mem_create failed\n");
		exit(1);
	}
	mpr_red = mpr_d(pr_red);
	mpr_grn = mpr_d(pr_grn);
	mpr_blu = mpr_d(pr_blu);
	mpr_comp = mpr_d(pr_comp);
	red_base = (unsigned char *) mpr_red->md_image;
	grn_base = (unsigned char *) mpr_grn->md_image;
	blu_base = (unsigned char *) mpr_blu->md_image;
	comp_base = (unsigned char *) mpr_comp->md_image;

	for (y = size; --y >= 0;) {
		redP = red_base + (y * mpr_red->md_linebytes);
		grnP = grn_base + (y * mpr_grn->md_linebytes);
		bluP = blu_base + (y * mpr_blu->md_linebytes);
		compP = comp_base + (y * mpr_comp->md_linebytes);
		for (x = 0; x < size; x++) {
			*compP++ = biggest - ((dit8A(*redP++, red8Amat, RR - 1, x, y) * GR +
			      dit8A(*grnP++, grn8Amat, GR - 1, x, y)) * BR +
				    dit8A(*bluP++, blu8Amat, BR - 1, x, y));
		}
	}
	pw_rop(pw, 0, 0, size, size, PIX_SRC, pr_comp, 0, 0);
	pr_destroy(pr_comp);
}

pw_24dither8Abit_rop(pw, pr_24, size)
	Pixwin         *pw;
	Pixrect        *pr_24;
	int             size;
{
	register unsigned char *pr24P, *compP;
	register int    red, grn, blu;
	register int    x, y;
	Pixrect        *pr_comp;
	struct mpr_data *mpr_24, *mpr_comp;
	unsigned char  *pr24_base, *comp_base;

	pr_comp = mem_create(size, size, 8);
	if (pr_comp == (Pixrect *) 0) {
		(void) printf(stderr, "mem_create failed\n");
		exit(1);
	}
	mpr_24 = mpr_d(pr_24);
	mpr_comp = mpr_d(pr_comp);
	pr24_base = (unsigned char *) mpr_24->md_image;
	comp_base = (unsigned char *) mpr_comp->md_image;

	for (y = size; --y >= 0;) {
		pr24P = pr24_base + (y * mpr_24->md_linebytes);
		compP = comp_base + (y * mpr_comp->md_linebytes);
		for (x = 0; x < size; x++) {
			red = *pr24P++;
			grn = *pr24P++;
			blu = *pr24P++;
			*compP++ = biggest - ((dit8A(red, red8Amat, RR - 1, x, y) * GR +
				  dit8A(grn, grn8Amat, GR - 1, x, y)) * BR +
					dit8A(blu, blu8Amat, BR - 1, x, y));
		}
	}
	pw_rop(pw, 0, 0, size, size, PIX_SRC, pr_comp, 0, 0);
	pr_destroy(pr_comp);
}
#endif DIT
