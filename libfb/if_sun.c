/*
 *			I F _ S U N . C
 *
 *  Based on original code from Bill Lindemann of SUN.
 *
 */
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/file.h>
#include <pixrect/pixrect_hs.h>
#include <sunwindow/window_hs.h>

#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	sun_dopen(),
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
FBIO sun_interface =  {
		sun_dopen,
		sun_dclose,
		fb_null,		/* reset? */
		sun_dclear,
		sun_bread,
		sun_bwrite,
		sun_cmread,
		sun_cmwrite,
		sun_viewport_set,
		sun_window_set,
		sun_zoom_set,
		fb_null,		/* sun_curs_set, */
		fb_null,		/* sun_cmemory_addr */
		fb_null,		/* cscreen_addr */
		"SUN Pixwin",
		1152,			/* max width */
		896,			/* max height */
		"/dev/sun",
		512,			/* current/default width  */
		512,			/* current/default height */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* page_ref */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
};


Pixwin         *sun_pw, *sun_master_pw;
Rect            sun_rect;
int             sun_win_fd;
int             sun_xzoom=1;
int		sun_yzoom=1;
int             sun_damaged;		/* window has incurred damage */
int             dither_flg = 0;	/* dither output */
int             batch_flg;
Rect            master_rect;

#define NDITHER   8
#define LNDITHER  3
#define EXTERN extern

unsigned char red8Amat[NDITHER][NDITHER];	/* red dither matrix */
unsigned char grn8Amat[NDITHER][NDITHER];	/* green matrix */
unsigned char blu8Amat[NDITHER][NDITHER];	/* blue  matrix */

typedef unsigned char uchar;

#ifdef DIT
EXTERN struct pixrect *redscr;		/* red screen */
EXTERN struct pixrect *grnscr;		/* green screen */
EXTERN struct pixrect *bluscr;		/* blue screen */
#endif DIT

EXTERN struct pixrect *redmem;		/* red memory pixrect */
EXTERN struct pixrect *grnmem;		/* grn memory pixrect */
EXTERN struct pixrect *blumem;		/* blu memory pixrect */
EXTERN unsigned char others[256];	/* unused colors */
EXTERN unsigned char redmap8[256];	/* red 8bit color map */
EXTERN unsigned char grnmap8[256];	/* green 8bit color map */
EXTERN unsigned char blumap8[256];	/* blue 8bit color map */

EXTERN int fb_type;			/* 8 = 8 bit, 24 = 24 bit */
EXTERN struct pixfont *myfont;		/* font to label with */

EXTERN int zbuf;			/* 1 = yes, 0 = no */
EXTERN int antialias;			/* 1 = yes, 0 = no */
unsigned char others[256];	/* unused colors */
unsigned char red8Amap[256];	/* red 8bit dither color map */
unsigned char grn8Amap[256];	/* green 8bit dither color map */
unsigned char blu8Amap[256];	/* blue 8bit dither color map */

#define RR	6
#define GR	7
#define BR	6

static unsigned char   redmap[256], grnmap[256], blumap[256];

static int biggest = RR*GR*BR-1;

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
    if (sun_damaged)
    {
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
sun_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int	i;
    int             x;
    int             gfxfd;
    char            gfxwinname[128];

    if (sun_pw != (Pixwin *) 0)
    {
	fprintf(stderr,"sun_open, already open\n");
/**	fbclose(); */
	sun_pw = 0;
    }

    sun_win_fd = win_getnewwindow();
    we_getgfxwindow(gfxwinname);
    gfxfd = open(gfxwinname, 2);
    win_insertblanket(sun_win_fd, gfxfd);
    sun_master_pw = pw_open(sun_win_fd);
    win_getsize(sun_win_fd, &master_rect);
    sun_rect = master_rect;
    sun_pw = sun_master_pw;
    sun_pw->pw_prretained = mem_create(512, 512, 8);

    if (dither_flg)
    {
#ifdef DIT
	draw8Ainit();
	dither8Ainit();
	pw_set8Amap(sun_pw, &sun_cmap);
#endif DIT
    }
    else
    {
	/* r | g | b, values = RR, GR, BR */
	/* set a new cms name; initialize it */
	x = pw_setcmsname(sun_pw, "libfb");
	fb_log("setcmsname ret %d\n", x);
	for (x = 0; x < (RR*GR*BR); x++)
	{
		int new;
		RGBpixel q;
		RGBpixel *qq = (RGBpixel *)q;

		blumap[x+1] = ((x % BR) ) * (255/(BR-1));
		grnmap[x+1] = (((x / BR) % GR)) * (255/(GR-1));
		redmap[x+1] = ((x / (BR*GR))) * (255/(RR-1));
		q[RED] = redmap[x+1];
		q[GRN] = grnmap[x+1];
		q[BLU] = blumap[x+1];
		new = COLOR_APPROX( qq );
#ifdef never
/**		if( new != x+1 )**/
fb_log(" %3d  %3d, %3d %3d %3d\n", x, new, q[RED], q[GRN], q[BLU] );
#endif
	}
	x = pw_putcolormap(sun_pw, 0, 256, redmap, grnmap, blumap);
	fb_log("colormap ret %d\n", x);
    }
#ifdef never
	{
		static unsigned char   redmp[256], grnmp[256], blump[256];
		pw_getcolormap(sun_pw, 0, 256, redmp, grnmp, blump);
		for( x=0; x< (RR*GR*BR); x++ )  {
fb_log("%3d %3d %3d =? %3d %3d %3d\n",
redmp[x], grnmp[x], blump[x],
redmap[x], grnmap[x], blumap[x] );
		}
	}
#endif
	/* Set entire area to background color (black) */
	/* SUN reserves [0] for white, [1] for black ?? */
	pw_rop(sun_pw, 0, 0, master_rect.r_width, master_rect.r_height,
		PIX_SRC | PIX_COLOR(0), (Pixrect *) 0, 0, 0);

	signal(SIGWINCH, sun_sigwinch);
	return (0);			/* "Success" */
}

/*
 *			S U N _ D C L O S E
 */
_LOCAL_ int
sun_dclose( ifp )
FBIO	*ifp;
{
    char            ch;
    Pixrect        *pr_out;
    FILE           *ofp;

    sun_repair();
    pw_close(sun_pw);
    sun_pw = (Pixwin *) 0;
}

/*
 *			S U N _ D C L E A R
 */
_LOCAL_ int
sun_dclear( ifp, pp )
FBIO	*ifp;
register RGBpixel	*pp;
{
    sun_repair();
}

/*
 *			S U N _ W I N D O W _ S E T
 */
_LOCAL_ int
sun_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
    sun_repair();
}

/*
 *			S U N _ Z O O M _ S E T
 */
_LOCAL_ int
sun_zoom_set( ifp, xpts, ypts)
FBIO	*ifp;
int	xpts, ypts;
{
    sun_repair();
    if (batch_flg)
    {
	sun_xzoom = 1;
	sun_yzoom = 1;
    }
    else
    {
	sun_xzoom = xpts;
	sun_yzoom = ypts;
    }
}

extern Pixwin  *sun_pw;

/*
 *			S U N _ B R E A D
 */
_LOCAL_ int
sun_bread()
{
}

/*
 *			S U N _ B W R I T E
 */
_LOCAL_ int
sun_bwrite( ifp, x, y, p, count)
FBIO	*ifp;
int	x, y;
register RGBpixel *p;
int	count;
{
    register int    cnt;
    register int    cx, cy;
    register int	value;

    sun_repair();
    cx = x;
    cy = master_rect.r_height-1-y;
	if( cy < 0 || cy >= master_rect.r_height) return;
    for( cnt = count; --cnt >= 0; p++ )  {
	value = COLOR_APPROX(p);
#ifdef never
fb_log("%3d: %3d %3d %3d --> %3d %3d %3d\n", value,
(*p)[RED], (*p)[GRN], (*p)[BLU],
redmap[value], grnmap[value], blumap[value] );
#endif
	pw_rop(sun_pw, cx, cy, sun_xzoom, sun_yzoom,
	       PIX_DONTCLIP | PIX_SRC | PIX_COLOR(value), (Pixrect *) 0, 0, 0);

	cx += sun_xzoom;
	if (cx >= ifp->if_width)  {
	    cx = 0;
	    cy -= sun_yzoom;
	}
    }
}

sun_put(pw, vx, vy, p)
Pixwin         *pw;
int		vx, vy;
register RGBpixel *p;
{
    int             cx, cy;
    int             fx, fy;	/* framebuffer x,y */
    int             xcnt, ycnt;
    int             value;

    fx = vx * sun_xzoom;
    fy = vy * sun_yzoom;
    if (dither_flg)
    {
#ifdef DIT
	for (ycnt = sun_yzoom; --ycnt >= 0;)
	{
	    for (xcnt = sun_xzoom; --xcnt >= 0;)
	    {
		cx = fx + xcnt;
		cy = fy + ycnt;
		value = get_dither8Abit(cx, cy, (*p)[RED], (*p)[GRN], (*p)[BLU]);
		pw_put(pw, cx, cy, value);
	    }
	}
#endif DIT
    }
    else
    {
	value = ((((((*p)[RED] * (RR))>>8) * GR ) +
		(((*p)[GRN]*(GR))>>8)) * BR
		+ (((*p)[BLU] * (BR))>>8));
/**	value = ((*p)[RED] + (*p)[GRN] + (*p)[BLU]) / 3; **/
	pw_rop(pw, fx, fy, sun_xzoom, sun_yzoom,
	       PIX_SRC | PIX_COLOR(value), (Pixrect *) 0, 0, 0);
    }
}

/*
 *			S U N _ V I E W P O R T _ S E T
 */
_LOCAL_ int
sun_viewport_set()
{
}

/*
 *			S U N _ C M R E A D
 */
_LOCAL_ int
sun_cmread()
{
}

/*
 *			S U N _ C M W R I T E
 */
_LOCAL_ int
sun_cmwrite( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
    sun_repair();
}

/* --------------------------------------------------------------- */

#ifdef DIT
/* draw8Abit.c
 *
 * By: David H. Elrod;  Sun Microsystems; September 1986
 *
 * Draw a pixel in 8 bit color space using a color cube that
 * is 6values red,  7values green and 6values blue.
 *
 * External Variables Used:
 *	redscr, grnscr, bluscr	- red, green and blue pixrects
 *	red8Amap, grn8Amap, blue8Amap	- software color map
 *
 * Bugs:
 *
 */

draw8Abit(x, y, r, g, b)
int x, y;			/* pixel location */
unsigned char r, g, b;		/* red, green, blue pixel values */
{
    int red, green, blue;	/* return values */
    int v;			/* 8 bit value */

    v = biggest - (((((r*(RR))>>8) * GR ) + ((g*(GR))>>8)) * BR
	+ ((b*(BR))>>8));

     /* deal with 24 bit frame buffer */
    red =   pr_put(redscr, x, y, red8Amap[v]);
    green = pr_put(grnscr, x, y, grn8Amap[v]);
    blue =  pr_put(bluscr, x, y, blu8Amap[v]);

    if((red == PIX_ERR) || (green == PIX_ERR) || (blue == PIX_ERR))
	return(PIX_ERR);
    return(0);
}
#endif DIT

draw8Ainit()
{   
    int i, r, g, b;		/* loop counters */
    
     /* ordered dither matrix (6 reds, 7 greens and 6 blues) */
    i = 0;
    for(r=0; r<RR; r++)
	for(g=0; g<GR; g++)
	    for(b=0; b<BR; b++) {
		red8Amap[i] = 255 - (r*255 / (RR - 1));
		grn8Amap[i] = 255 - (g*255 / (GR - 1));
		blu8Amap[i] = 255 - (b*255 / (BR - 1));
		i++;
	    }
}
    
/* pw_dither8Abit.c
 *
 * Modified:	Bill Lindemann;	Sun Microsystems; September 1986
 * From original by: David H. Elrod;  Sun Microsystems; September 1986
 *
 * Display a pixel using an ordered dither algoritm to approximate the
 * 24 bit rgb value supplied.  Convert this value to an 8 bit system,
 * and display in the given pixwin.  Assume the colormap is already set.
 *
 * External Variables Used:
 *	red8Amat, grn8Amat, blu8Amat	- dither matricies;
 *
 * Bugs:
 *
 */
#ifdef DIT
pw_dither8Abit(pw, x, y, r, g, b)
Pixwin	*pw;
int x, y;			/* pixel location */
unsigned char r, g, b;		/* red, green, blue pixel values */
{
    int red, green, blue;	/* return values */
    int v;			/* 8 bit value */

    v = biggest - (( dit8A(r, red8Amat, RR-1, x, y)*GR +
	dit8A(g, grn8Amat, GR-1, x, y)  )*BR +
	dit8A(b, blu8Amat, BR-1, x, y));

    pw_put(pw, x, y, v);

    if((red == PIX_ERR) || (green == PIX_ERR) || (blue == PIX_ERR))
	return(PIX_ERR);
    return(0);
}

get_dither8Abit(x, y, r, g, b)
int x, y;			/* pixel location */
unsigned char r, g, b;		/* red, green, blue pixel values */
{
    int v;			/* 8 bit value */

    v = biggest - (( dit8A(r, red8Amat, RR-1, x, y)*GR +
	dit8A(g, grn8Amat, GR-1, x, y)  )*BR +
	dit8A(b, blu8Amat, BR-1, x, y));

    return(v);
}

pw_set8Amap(pw, cmap)
Pixwin	*pw;
colormap_t	*cmap;
{
    pw_setcmsname(pw, "dith8Amap");
    pw_putcolormap(pw, 0, biggest+1, red8Amap, grn8Amap, blu8Amap);
    if (cmap != (colormap_t *) 0)
    {
	cmap->type = RMT_EQUAL_RGB;
	cmap->length = biggest+1;
	cmap->map[0] = red8Amap;
	cmap->map[1] = grn8Amap;
	cmap->map[2] = blu8Amap;
    }
}

pw_dither8Abit_rop(pw, pr_red, pr_grn, pr_blu, size)
Pixwin		*pw;
Pixrect		*pr_red, *pr_grn, *pr_blu;
int		size;
{
    register unsigned char *redP, *grnP, *bluP, *compP;
    register int    x, y;
    Pixrect        *pr_comp;
    struct mpr_data *mpr_red, *mpr_grn, *mpr_blu, *mpr_comp;
    unsigned char  *red_base, *grn_base, *blu_base, *comp_base;

    pr_comp = mem_create(size, size, 8);
    if (pr_comp == (Pixrect *) 0)
    {
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

    for (y = size; --y >= 0;)
    {
	redP = red_base + (y * mpr_red->md_linebytes);
	grnP = grn_base + (y * mpr_grn->md_linebytes);
	bluP = blu_base + (y * mpr_blu->md_linebytes);
	compP = comp_base + (y * mpr_comp->md_linebytes);
	for (x = 0; x < size; x++)
	{
	    *compP++ = biggest - (( dit8A(*redP++, red8Amat, RR-1, x, y)*GR +
		dit8A(*grnP++, grn8Amat, GR-1, x, y)  )*BR +
		dit8A(*bluP++, blu8Amat, BR-1, x, y));
	}
    }
    pw_rop(pw, 0, 0, size, size, PIX_SRC, pr_comp, 0, 0);
    pr_destroy(pr_comp);
}

pw_24dither8Abit_rop(pw, pr_24, size)
Pixwin		*pw;
Pixrect		*pr_24;
int		size;
{
    register unsigned char *pr24P, *compP;
    register int    red, grn, blu;
    register int    x, y;
    Pixrect        *pr_comp;
    struct mpr_data *mpr_24, *mpr_comp;
    unsigned char  *pr24_base, *comp_base;

    pr_comp = mem_create(size, size, 8);
    if (pr_comp == (Pixrect *) 0)
    {
	(void) printf(stderr, "mem_create failed\n");
	exit(1);
    }
    mpr_24 = mpr_d(pr_24);
    mpr_comp = mpr_d(pr_comp);
    pr24_base = (unsigned char *) mpr_24->md_image;
    comp_base = (unsigned char *) mpr_comp->md_image;

    for (y = size; --y >= 0;)
    {
	pr24P = pr24_base + (y * mpr_24->md_linebytes);
	compP = comp_base + (y * mpr_comp->md_linebytes);
	for (x = 0; x < size; x++)
	{
	    red = *pr24P++;
	    grn = *pr24P++;
	    blu = *pr24P++;
	    *compP++ = biggest - (( dit8A(red, red8Amat, RR-1, x, y)*GR +
		dit8A(grn, grn8Amat, GR-1, x, y)  )*BR +
		dit8A(blu, blu8Amat, BR-1, x, y));
	}
    }
    pw_rop(pw, 0, 0, size, size, PIX_SRC, pr_comp, 0, 0);
    pr_destroy(pr_comp);
}
#endif DIT
