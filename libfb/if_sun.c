/*
 *			I F _ S U N . C 
 *
 *  SUN display interface.  In order to simulate the behavior of a real
 *  framebuffer, an entire image worth of memory is saved using SysV
 *  shared memory.  This image exists across invocations of frame buffer
 *  utilities, and allows the full resolution of an image to be retained,
 *  and captured, even with the 1-bit deep 3/50 displays.
 *
 *  In order to use this large a chunk of memory with the shared memory
 *  system, it is necessary to reconfigure your kernel to authorize this.
 *  If you have already reconfigured your kernel and your hostname is
 *  "PICKLE", you should have a "/sys/conf/PICKLE" file, otherwise you should
 *  read up on kernel reconfiguration in your owners manual, and then
 *  copy /sys/conf/GENERIC to /sys/conf/PICKLE and make the necessary
 *  deletions.  Then add the following line to the other "options" lines
 *  near the top of "PICKLE".
 *
 *  options	SHMPOOL=1024	# Increase for BRL CAD libfb
 *
 *  Once you have modified "PICKLE" finish the procedure by typing:
 *
 *  # /etc/config PICKLE
 *  # cd ../PICKLE
 *  # make && mv /vmunix /vmunix.bak && mv vmunix /vmunix
 *
 *  Then reboot your system and you should be all set.
 *
 *  Note that this has been tested on release 3.2 of the 3/50 kernel.
 *  If you have more than 4 Megs of memory on your system, you may want
 *  to increase [XY]MAXWINDOW to be as big as [XY]MAXSCREEN (see CON-
 *  FIGURATION NOTES below) and increase SHMPOOL appropriately.  If you
 *  do not reconfigure your kernel, the attempt to acquire shared memory
 *  will fail, and the image will be stored in the process's address space
 *  instead, with the only penalty being lack of persistance of the image
 *  across processes.
 *
 *  Authors -
 *	Bill Lindemann
 *	Michael John Muuss
 *	Phil Dykstra
 *	Gary S. Moss
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
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <suntool/sunview.h>
#include <suntool/canvas.h>

#include "fb.h"
#include "./fblocal.h"
extern char	*calloc(), *malloc();
extern int	errno;
extern char	*shmat();
static int	linger();

/* CONFIGURATION NOTES:

	If you have 4 Megabytes of memory, best limit the window sizes
	to 512-by-512 (whether or not your using shared memory) because a
	24-bit image is stored in memory and we don't want our systems to
	thrash do we).
 */
#define XMAXSCREEN	1152	/* Physical screen width in pixels. */
#define YMAXSCREEN	896	/* Physical screen height in pixels. */
#define XMAXWINDOW	512	/* Max. width of frame buffer in pixels. */
#define YMAXWINDOW	512	/* Max. height of frame buffer in pixels. */
#define BANNER		18	/* Height of title bar at top of window. */
#define BORDER		2	/* Border thickness of window. */
#define PARENTBORDER	4	/* Border thickness of parent window. */
#define TITLEXOFFSET	20	/* Indentation for text within title bar. */
#define TITLEYOFFSET	4	/* Vertical padding of text within title bar. */
#define TIMEOUT		15	/* Seconds between check for window repaint. */
#define MAXDITHERSZ	8	/* Dimensions of dithering threshold matrix. */
#define DITHERSZ	8	/* Size of dither pattern used. */
#define DITHMASK(ii) ((ii)&07)	/* Masks of DITHERSZ bits. */

#define EQUALRGB(aa,bb) \
	((aa)[RED]==(bb)[RED]&&(aa)[GRN]==(bb)[GRN]&&(aa)[BLU]==(bb)[BLU])
#define XIMAGE2SCR( x ) \
 ((x)*SUN(ifp)->su_xzoom-(SUN(ifp)->su_xcenter*SUN(ifp)->su_xzoom-ifp->if_width/2))
#define YIMAGE2SCR( y ) \
 ((ifp->if_height-1)-((y)*SUN(ifp)->su_yzoom-(SUN(ifp)->su_ycenter*SUN(ifp)->su_yzoom - ifp->if_height/2)))
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
		sun_curs_set(),
		sun_cmemory_addr(),
		sun_cscreen_addr(),
		sun_help();

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
				 sun_curs_set,
				 sun_cmemory_addr,
				 sun_cscreen_addr,
				 sun_help,
				 "SUN SunView or raw Pixwin",
				 XMAXWINDOW,	/* max width */
				 YMAXWINDOW,	/* max height */
				 "/dev/sun",
				 XMAXWINDOW,	/* current/default width  */
				 YMAXWINDOW,	/* current/default height */
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

static int	is_linear_cmap();

/*
 * Our image (window) pixwin
 * WARNING: this should be in suninfo but isn't there yet
 * because of signal routines the need to find it.
 */
static Pixwin	*imagepw;

/*
 *  Per SUN (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct suninfo
	{
	Window	frame;
	Window	canvas;
	short	su_curs_on;
	short	su_cmap_flag;
	short	su_xzoom;
	short	su_yzoom;
	short	su_xcenter;
	short	su_ycenter;
	short	su_xcursor;
	short	su_ycursor;
	short	su_depth;
	int	su_shmid;	/* shared memory ID */
	int	su_mode;
	};
#define if_mem		u2.p	/* shared memory pointer */
#define if_cmap		u3.p	/* color map in shared memory */
#define if_windowfd	u4.l	/* f. b. window file descriptor under SUNTOOLS */
#define sunrop(dx,dy,w,h,op,pix,sx,sy) \
		if( sun_pixwin ) \
			pw_rop(SUNPW(ifp),dx,dy,w,h,op,pix,sx,sy); \
		else \
			pr_rop(SUNPR(ifp),dx,dy,w,h,PIX_DONTCLIP|op,pix,sx,sy)

#define sunput(dx,dy,v) \
		if( sun_pixwin ) \
			pw_put(SUNPW(ifp),dx,dy,v); \
		else \
			pr_put(SUNPR(ifp),dx,dy,v);

#define sunreplrop(dx,dy,dw,dh,op,spr,sx,sy) \
		if( sun_pixwin ) \
			pw_replrop(SUNPW(ifp),dx,dy,dw,dh,op,spr,sx,sy); \
		else \
			pr_replrop(SUNPR(ifp),dx,dy,dw,dh,op,spr,sx,sy);

#define SUNPW(ptr)	((Pixwin *)((ptr)->u5.p))
#define SUNPWL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define SUNPR(ptr)	((Pixrect *)((ptr)->u5.p))
#define SUNPRL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define	SUN(ptr)	((struct suninfo *)((ptr)->u1.p))
#define SUNL(ptr)	((ptr)->u1.p)	/* left hand side version. */
#define CMR(x)		((struct sun_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct sun_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct sun_cmap *)((x)->if_cmap))->cmb

static int	sun_damaged = FALSE; /* SIGWINCH fired, need to repair damage. */
static int	sun_pixwin = FALSE;  /* Running under SUNTOOLS. */
static RGBpixel black = { 0, 0, 0 };
struct sun_cmap
	{
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
	};

static int	dither_flg = FALSE;   /* dither output */

/* dither pattern (threshold level) */
static short	dither[MAXDITHERSZ][MAXDITHERSZ] =
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
#endif

unsigned char   red8Amap[256];	/* red 8bit dither color map */
unsigned char   grn8Amap[256];	/* green 8bit dither color map */
unsigned char   blu8Amap[256];	/* blue 8bit dither color map */

/* Our copy of the *hardware* colormap */
static unsigned char redmap[256], grnmap[256], blumap[256];

#define RR	6
#define GR	7
#define BR	6

#ifdef DIT
static int      biggest = RR * GR * BR - 1;
#endif

#define COLOR_APPROX(p) \
	(((p)[RED] * RR ) / 256) * GR*BR + \
	(((p)[GRN] * GR ) / 256) * BR  + \
	(((p)[BLU] * BR ) / 256) + 1

#define SUN_CMAPVAL( p, o )\
	if( SUN(ifp)->su_cmap_flag )\
		{\
		(o)[RED] = CMR(ifp)[(*p)[RED]];\
		(o)[GRN] = CMG(ifp)[(*p)[GRN]];\
		(o)[BLU] = CMB(ifp)[(*p)[BLU]];\
		}\
	else	COPYRGB( o, *p );

/*
 *  The mode has several independent bits:
 *	SHARED -vs- MALLOC'ed memory for the image
 *	TRANSIENT -vs- LINGERING windows
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)		/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)		/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)

#define MODE_15MASK	(1<<14)
#define MODE_15NORMAL	(0<<14)
#define MODE_15ZAP	(1<<14)

struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'p',	MODE_1MASK, MODE_1MALLOC,
		"Private memory - else shared" },
	{ 'l',	MODE_2MASK, MODE_2LINGERING,
		"Lingering window - else transient" },
	{ 'z',	MODE_15MASK, MODE_15ZAP,
		"Zap (free) shared memory" },
	{ '\0', 0, 0, "" }
};

_LOCAL_ int
sun_sigwinch( sig )
	{
	sun_damaged = TRUE;
	return	sig;
	}

_LOCAL_ int
sun_sigalarm( sig )
	{
	if( imagepw == (Pixwin *) NULL )
		return	sig;
	(void) signal( SIGALRM, sun_sigalarm );
	if( sun_damaged )
		{
		pw_damaged( imagepw );
		pw_repairretained( imagepw );
		pw_donedamaged( imagepw );
		sun_damaged = FALSE;
		}
	alarm( TIMEOUT );
	return	sig;
	}

_LOCAL_ void
sun_storepixel( ifp, x, y, p, count )
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
	{	register RGBpixel	*memp;
	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--
		)
		{
		COPYRGB( *memp, *p );
		}
	return;
	}

_LOCAL_ void
sun_storebackground( ifp, x, y, p, count )
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
	{	register RGBpixel	*memp;
	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, count--
		)
		{
		COPYRGB( *memp, *p );
		}
	return;
	}

/* These lock routines are unused.  They do not seem to provide any speedup when
bracketing raster op routines.  This may pan out differently when other processes
are actively competing for raster ops. */
_LOCAL_ void
sun_lock( ifp )
FBIO	*ifp;
	{
	if( sun_pixwin )
		{ /* Lock the display and get the cursor out of the way. */
		pw_lock( SUNPW(ifp), SUNPW(ifp)->pw_pixrect );
		}
	return;
	}

_LOCAL_ void
sun_unlock( ifp )
FBIO	*ifp;
	{
	if( sun_pixwin )
		{ /* Release display lock and restore cursor. */
		pw_unlock( SUNPW(ifp) );
		}
	return;
	}

/* Dither pattern pixrect for fast raster ops.  (1bit displays) */
static char	dither_mpr_buf[8];
mpr_static( dither_mpr, DITHERSZ, DITHERSZ, 1, (short *)dither_mpr_buf );

/* Straight pixrect for grey-scale and color devices. (8bit) */
static char	pixel_mpr_buf[1];
mpr_static( pixel_mpr, 1, 1, 8, (short *)pixel_mpr_buf );

_LOCAL_ void
sun_rectclear( ifp, xlft, ytop, width, height, pp )
FBIO		*ifp;
int		xlft, ytop, width, height;
RGBpixel	*pp;
	{	static int		lastvalue = 0;
		register int		value;
		RGBpixel		v;

	/*fb_log( "sun_rectclear(%d,%d,%d,%d)\n", xlft, ytop, width, height ); /* XXX-debug */
	/* Get color map value for pixel. */
	SUN_CMAPVAL( pp, v );

 	if( SUN(ifp)->su_depth == 1 )
		{ /* Construct dither pattern in memory pixrect. */
		value = (v[RED] + v[GRN] + v[BLU]);
		if( value == 0 )
			{
			sunrop( xlft, ytop, width, height,
				PIX_SET, (Pixrect *) 0, 0, 0
				);
			return;
			}
		if( value != lastvalue )
			{	register int	i, j;
			for( i = 0; i < DITHERSZ; i++ )
				for( j = 0; j < DITHERSZ; j++ )
					{	register int	op;
					op = (value < dither[j][i]*3) ?
						PIX_SET : PIX_CLR;
					pr_rop( &dither_mpr, j, i, 1, 1, op,
						(Pixrect *) NULL, 0, 0 );
					}
			lastvalue = value;
			}
		/* Blat out dither pattern. */
		sunreplrop( xlft, ytop, width, height,
			PIX_SRC, &dither_mpr, DITHMASK(xlft), DITHMASK(ytop) );
		}
	else
		{ /* Grey-scale or color image. */
		pixel_mpr_buf[0] = COLOR_APPROX(v);
		sunreplrop( xlft, ytop, width, height,
			PIX_SRC, &pixel_mpr, 0, 0
			);
		}
	return;
	}

/*
 * Scanline dither pattern pixrect.  One each for 1bit, and 8bit
 * deep displays.  If DITHERSZ or BITSDEEP > sizeof(char) you must
 * extend the length of scan_mpr_buf accordingly.
 */
static char	scan_mpr_buf[XMAXWINDOW];
mpr_static( scan1_mpr, XMAXWINDOW, DITHERSZ, 1, (short *)scan_mpr_buf );
mpr_static( scan8_mpr, XMAXWINDOW, DITHERSZ, 8, (short *)scan_mpr_buf );

_LOCAL_ void
sun_scanwrite( ifp, xlft, ybtm, xrgt, pp )
FBIO		*ifp;
int		xlft;
int		ybtm;
register int	xrgt;
RGBpixel	*pp;
	{	register int	sy = YIMAGE2SCR( ybtm+1 ) + 1;
		register int	xzoom = SUN(ifp)->su_xzoom;
		int		xl = XIMAGE2SCR( xlft );

	/*fb_log( "sun_scanwrite(%d,%d,%d,0x%x)\n", xlft, ybtm, xrgt, pp );
		/* XXX-debug */
 	if( SUN(ifp)->su_depth == 1 )
		{	register int	x;
			register int	sx = xl;
			int		yzoom = SUN(ifp)->su_yzoom;
		/* Clear buffer to black. */
		(void) memset( scan_mpr_buf, 0xff, XMAXWINDOW );
		for( x = xlft; x <= xrgt; x++, sx += xzoom, pp++ )
			{	register int		value;
				RGBpixel		v;
			/* Get color map value for pixel. */
			SUN_CMAPVAL( pp, v );
			value = v[RED] + v[GRN] + v[BLU];
			if( value != 0 )
				{ register int	maxdy = yzoom < DITHERSZ ?
							yzoom : DITHERSZ;
				  register int	dy;
				  register int	yoffset = sx;
				/* Construct dither pattern. */
				value /= 3;
				for( dy = 0; dy < maxdy; dy++, yoffset += XMAXWINDOW )
					{ register int dx;
					  register int ydit = DITHMASK(sy+dy);
					for( dx = 0; dx < xzoom; dx++ )
						{ register int xdit =
							DITHMASK(sx+dx);
						if( value >= dither[xdit][ydit] )
							scan_mpr_buf[(yoffset+dx)>>3] &= ~(0x80>>xdit);
						}
					}
				}
			}
		sunreplrop( xl, sy,
			(xrgt-xlft+1)*xzoom, yzoom,
			PIX_SRC, &scan1_mpr,
			xl, 0
			);
		}
	else /* Grey-scale or color image. */
		{	register int	x;
			register int	sx = xl;
		for( x = xlft; x <= xrgt; x++, pp++, sx += xzoom )
			{	register int		dx;
				register int		value;
				RGBpixel		v;
			/* Get color map value for pixel. */
			SUN_CMAPVAL( pp, v );
			value = COLOR_APPROX(v);
			for( dx = 0; dx < xzoom; dx++ )
				scan_mpr_buf[sx+dx] = value; 
			}
		sunreplrop( xl, sy,
			(xrgt-xlft+1)*xzoom, SUN(ifp)->su_yzoom,
			PIX_SRC, &scan8_mpr,
			xl, 0
			);
		}
	return;
	}

_LOCAL_ void
sun_rectwrite( ifp, xmin, ymin, xmax, ymax, buf, offset )
register FBIO		*ifp;
int			xmin, ymin;
int			xmax;
register int		ymax;
RGBpixel		*buf;
register int		offset;
	{	register int		y;
		register RGBpixel	*p;
	/*fb_log( "sun_rectwrite(xmin=%d,ymin=%d,xmax=%d,ymax=%d,buf=0x%x,offset=%d)\n",
		xmin, ymin, xmax, ymax, buf, offset ); /* XXX--debug */
	p = buf-offset+ymin*ifp->if_width+xmin;
	for( y = ymin; y <= ymax; y++, p += ifp->if_width )
		sun_scanwrite(	ifp, xmin, y, xmax, p );
	return;
	}

_LOCAL_ void
sun_repaint( ifp )
register FBIO	*ifp;
	{	register int	i;
		register int	ymin, ymax;
		int		xmin, xmax;
		int		xwidth;
		int		xscroff, yscroff;
		int		xscrpad, yscrpad;
	/*fb_log( "sun_repaint: xzoom=%d yzoom=%d xcenter=%d ycenter=%d\n",
		SUN(ifp)->su_xzoom, SUN(ifp)->su_yzoom,
		SUN(ifp)->su_xcenter, SUN(ifp)->su_ycenter ); /* XXX-debug */
	xscroff = yscroff = 0;
	xscrpad = yscrpad = 0;
	xwidth = ifp->if_width/SUN(ifp)->su_xzoom;
	i = xwidth/2;
	xmin = SUN(ifp)->su_xcenter - i;
	xmax = SUN(ifp)->su_xcenter + i - 1;
	i = (ifp->if_height/2)/SUN(ifp)->su_yzoom;
	ymin = SUN(ifp)->su_ycenter - i;
	ymax = SUN(ifp)->su_ycenter + i - 1;
	if( xmin < 0 )
		{
		xscroff = -xmin * SUN(ifp)->su_xzoom;
		xmin = 0;
		}
	if( ymin < 0 )
		{
		yscroff = -ymin * SUN(ifp)->su_yzoom;
		ymin = 0;
		}
	if( xmax > ifp->if_width-1 )
		{
		xscrpad = (xmax-(ifp->if_width-1))*SUN(ifp)->su_xzoom;
		xmax = ifp->if_width-1;
		}
	if( ymax > ifp->if_height-1 )
		{
		yscrpad = (ymax-(ifp->if_height-1))*SUN(ifp)->su_yzoom;
		ymax = ifp->if_height-1;
		}
	/* Blank out area left of image.			*/
	if( xscroff > 0 )
		sun_rectclear(	ifp, 0, 0, xscroff, ifp->if_height,
				(RGBpixel *) black );
	/* Blank out area below image.			*/
	if( yscroff > 0 )
		sun_rectclear(	ifp, 0, ifp->if_height-yscroff,
				ifp->if_width, yscroff,
				(RGBpixel *) black );
	/* Blank out area right of image.			*/
	if( xscrpad > 0 )
		sun_rectclear(	ifp, ifp->if_width-xscrpad, 0,
				xscrpad, ifp->if_height,
				(RGBpixel *) black );
	/* Blank out area above image.			*/
	if( yscrpad > 0 )
		sun_rectclear(	ifp, 0, 0,
				ifp->if_width, yscrpad,
				(RGBpixel *) black );
	sun_rectwrite( ifp, xmin, ymin, xmax, ymax, (RGBpixel *)ifp->if_mem, 0 );
	return;
	}

/*
 *			S U N _ G E T M E M
 *
 *  Because there is no hardware zoom or pan, we need to repaint the
 *  screen (with big pixels) to implement these operations.
 *  This means that the actual "contents" of the frame buffer need
 *  to be stored somewhere else.  If possible, we allocate a shared
 *  memory segment to contain that image.  This has several advantages,
 *  the most important being that although output mode requires monochrome,
 *  dithering, or up to 8-bit color on the Suns, pixel-readbacks still
 *  give the full 24-bits of color.  System V shared memory persists
 *  until explicitly killed, so this also means that under SUNTOOLS, the
 *  previous contents of the frame buffer still exist, and can be again
 *  accessed, even though the windows are transient, per-process.
 */
_LOCAL_ int
sun_getmem( ifp )
FBIO	*ifp;
	{	int	pixsize;
		int	size;
		int	new = 0;
		static char	*sp = NULL;
		extern caddr_t	sbrk();

	errno = 0;
	pixsize = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
	size = pixsize + sizeof(struct sun_cmap);
	size = (size + 4096-1) & ~(4096-1);
#define SHMEM_KEY	42
	if( sp == (char *) NULL ) /* Do once per process. */
		{
		if( (SUN(ifp)->su_mode & MODE_1MASK) == MODE_1MALLOC )
			goto localmem;

		/* First try to attach to an existing one */
		if( (SUN(ifp)->su_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )
			{ /* No existing one, create a new one */
			if(	(SUN(ifp)->su_shmid =
				shmget( SHMEM_KEY, size, IPC_CREAT|0666 ))
				< 0 )
				{
				fb_log( "if_sun: shmget failed, errno=%d\n", errno );
				goto fail;
				}
			new = 1;
			}
		/* Open the segment Read/Write, near the current break */
		if( (sp = shmat( SUN(ifp)->su_shmid, 0, 0 )) < 0 )
			{
			fb_log("shmat returned x%x, errno=%d\n", sp, errno );
			goto fail;
			}
		goto	common;
fail:
		fb_log("sun_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_sun.c for details\n");
		/* Change it to local */
		SUN(ifp)->su_mode = (SUN(ifp)->su_mode & ~MODE_1MASK)
			| MODE_1MALLOC;
localmem:
		if( (sp = malloc( size )) == NULL )
			{
			fb_log( "sun_getmem:  malloc failure, couldn't allocate %d bytes\n", size );
			return	-1;
			}
		new = 1;
		}
common:
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* Color map at end of area */
	
	/* Provide non-black colormap on creation of new shared mem */
	if( new )
		{
		sun_cmwrite( ifp, COLORMAP_NULL );
		sun_storebackground( ifp, 0, 0, black, ifp->if_max_width*ifp->if_max_height );
		}
	return	0;
	}

/*
 *			S U N _ Z A P M E M
 */
_LOCAL_ void
sun_zapmem()
	{ 	int	shmid;
		int	i;
	if( (shmid = shmget( SHMEM_KEY, 0, 0 )) < 0 )
		{
		fb_log( "sun_zapmem shmget failed, errno=%d\n", errno );
		return;
		}

	i = shmctl( shmid, IPC_RMID, 0 );
	if( i < 0 )
		{
		fb_log( "sun_zapmem shmctl failed, errno=%d\n", errno );
		return;
		}
	fb_log( "if_sun: shared memory released\n" );
	return;
	}

#ifdef SUN_USE_AGENT
_LOCAL_ Notify_value
event_func( client, event, arg, when )
Notify_client		client;
Event			*event;
Notify_arg		arg;
Notify_event_type	when;
	{
	fb_log( "event_func(%s)\n", client );
	switch( event_id( event ) )
		{
	case WIN_REPAINT :
		sun_repair( win_get_pixwin( client ) );
		break;
	default :
		break;
		}
	return	NOTIFY_DONE;
	}

_LOCAL_ Notify_value
destroy_func( client, status )
Notify_client	client;
Destroy_status	status;
	{
	fb_log( "destroy_func(%s)\n", client );
	return	NOTIFY_DONE;
	}
#endif

/*
 *			S U N _ D O P E N 
 */
_LOCAL_ int
sun_dopen(ifp, file, width, height)
FBIO	*ifp;
char	*file;
int	width, height;
	{	char		sun_parentwinname[WIN_NAMESIZE];
		Rect		winrect;
		int		x;
		struct pr_prpos	where;
		struct pixfont	*myfont;
		int	mode;

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/sun###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/sun", 8) ) {
			/* How did this happen?? */
			mode = 0;
		} else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[8];
			while( *cp != '\0' && !isspace(*cp) ) {
				*mp++ = *cp;	/* copy it to buffer */
				if( isdigit(*cp) ) {
					cp++;
					continue;
				}
				alpha++;
				for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
					if( mfp->c == *cp ) {
						mode = (mode&~mfp->mask)|mfp->value;
						break;
					}
				}
				if( mfp->c == '\0' && *cp != '-' ) {
					fb_log( "if_sun: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
		}

		if( (mode & MODE_15MASK) == MODE_15ZAP ) {
			/* Only task: Attempt to release shared memory segment */
			sun_zapmem();
			return(-1);
		}

		/* Pick off just the mode bits of interest here */
		mode &= (MODE_1MASK | MODE_2MASK);
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	if( SUN(ifp) != (struct suninfo *) NULL )
		{
		fb_log( "sun_dopen, already open\n" );
		return	-1;	/* FAIL */
		}
	if( (SUNL(ifp) = calloc( 1, sizeof(struct suninfo) )) == NULL )
		{
		fb_log( "sun_dopen:  suninfo calloc failed\n" );
		return	-1;
		}
	SUN(ifp)->su_mode = mode;
	myfont = pf_open( "/usr/lib/fonts/fixedwidthfonts/screen.b.14" );

	/*
	 * Initialize what we want an 8bit *hardware* colormap to
	 * look like.  Note that our software colormap is totally
	 * separate from this.
	 * r | g | b, values = RR, GR, BR
	 */
	for (x = 0; x < (RR * GR * BR); x++) {
		blumap[x + 1] = ((x % BR)) * 255 / (BR - 1);
		grnmap[x + 1] = (((x / BR) % GR)) * 255 / (GR - 1);
		redmap[x + 1] = ((x / (BR * GR))) * 255 / (RR - 1);
	}

	/* Create window. */
        if( sun_pixwin = (we_getgfxwindow(sun_parentwinname) == 0) ) {
        	/************** SunView Open **************/
		Window	frame;
		Canvas	canvas;

		/* Make a frame and a canvas to go in it */
		frame = window_create(NULL, FRAME,
			      FRAME_LABEL, "Frame Buffer", 0);
		/* XXX - "command line" args? pg.51 */
		canvas = window_create(frame, CANVAS,
			      WIN_WIDTH, width,
			      WIN_HEIGHT, height, 0);
		/* Fit window to canvas (width+10, height+20) */
		window_fit(frame);

		/* get the canvas pixwin to draw in */
		imagepw = canvas_pixwin(canvas);
		SUNPWL(ifp) = (char *) imagepw;

		winrect = *((Rect *)window_get(canvas, WIN_RECT));
		if( width > winrect.r_width )
			width = winrect.r_width;
		if( height > winrect.r_height )
			height = winrect.r_height;

		ifp->if_windowfd = imagepw->pw_clipdata->pwcd_windowfd;
		SUN(ifp)->su_depth = SUNPW(ifp)->pw_pixrect->pr_depth;
		SUN(ifp)->frame = frame;
		SUN(ifp)->canvas = canvas;

		/* set up window input */
		/*window_set(canvas, WIN_EVENT_PROC, input_eater, 0);*/

		window_set(frame, WIN_SHOW, TRUE, 0);	/* display it! */
		(void) notify_dispatch();		/* process event */

		if( SUN(ifp)->su_depth == 8 )
			{
			if( dither_flg )
				{
#ifdef DIT
				draw8Ainit();
				dither8Ainit();
				pw_set8Amap(imagepw, &sun_cmap);
#endif DIT
				}
			else
				{
				/* set a new cms name; initialize it */
				x = pw_setcmsname(imagepw, "libfb");
				/*
				 * set first (background) and last (foreground)
				 * entries the same so suntools will fill them
				 * in for us.
				 */
				redmap[0] = redmap[255] = 0;
				grnmap[0] = grnmap[255] = 0;
				blumap[0] = blumap[255] = 0;
				x = pw_putcolormap(imagepw, 0, 256, redmap, grnmap, blumap);
				x = pw_getcolormap(imagepw, 0, 256, redmap, grnmap, blumap);
				/*
				 * save the filled in foreground color in
				 * last-1 since this is where the usual
				 * monochrome colormap segment lives.
				 */
				redmap[254] = redmap[0];
				grnmap[254] = grnmap[0];
				blumap[254] = blumap[0];
				x = pw_putcolormap(imagepw, 0, 256, redmap, grnmap, blumap);
				}
			}
#ifdef SHOULDNOTNEEDTHIS
		(void) signal( SIGWINCH, sun_sigwinch );
		(void) signal( SIGALRM, sun_sigalarm );
		alarm( TIMEOUT );
#endif
#ifdef SUN_USE_AGENT
		/* Register pixwin with Agent so it can manage repaints. */
		win_register(	(Notify_client) ifp->if_type,
				SUNPW(ifp),
				event_func, destroy_func,
				PW_RETAIN | PW_REPAINT_ALL
				);
#endif SUN_USE_AGENT
	} else {
        	/************ Raw Screen Open ************/
		static Pixrect	*screenpr = NULL;
		static Pixrect	*windowpr;

		if( screenpr == (Pixrect *) NULL )
			{
			screenpr = pr_open( "/dev/fb" );
			windowpr = pr_region(	screenpr,
					XMAXSCREEN-width-BORDER*2,
					YMAXSCREEN-(height+BANNER+BORDER*3),
					width+BORDER*2, height+BANNER+BORDER*3
					);
			}
		SUNPRL(ifp) = (char *)
			pr_region(	windowpr,
					BORDER, BANNER+BORDER*2,
					width, height
					);
		/* Outer border is black. */
		pr_rop( windowpr, 0, 0,
			width+BORDER*2, height+BANNER+BORDER*3,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Inner border is white. */
		pr_rop( windowpr, 1, 1,
			width+2, height+BANNER+BORDER*2,
			PIX_DONTCLIP | PIX_CLR,
			(Pixrect *) NULL, 0, 0 );
		/* Black out title bar. */
		pr_rop( windowpr, BORDER, BORDER,
			width, BANNER,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Draw title in title bar (banner). */
		where.pr = windowpr;
		where.pos.x = TITLEXOFFSET;
		where.pos.y = BANNER - TITLEYOFFSET;
		pf_ttext(	where,
				PIX_CLR,
				myfont, "BRL libfb Frame Buffer" );
		SUN(ifp)->su_depth = SUNPR(ifp)->pr_depth;
		if( SUN(ifp)->su_depth == 8 ) {
			pr_putcolormap(windowpr, 1, 253,
				&redmap[1], &grnmap[1], &blumap[1] );
		}
	}

	pf_close( myfont );
	ifp->if_width = width;
	ifp->if_height = height;
	SUN(ifp)->su_xzoom = 1;
	SUN(ifp)->su_yzoom = 1;
	SUN(ifp)->su_xcenter = width/2;
	SUN(ifp)->su_ycenter = height/2;
	sun_getmem( ifp );

	/* Must call "is_linear_cmap" AFTER "sun_getmem" which allocates
		space for the color map.				*/
	SUN(ifp)->su_cmap_flag = !is_linear_cmap(ifp);

	if( (SUN(ifp)->su_mode & MODE_1MASK) == MODE_1SHARED ) {
		/* Redraw 24-bit image from shared memory */
		sun_repaint( ifp );
	}

	if( (SUN(ifp)->su_depth != 1) && (SUN(ifp)->su_depth != 8) ) {
		fb_log( "if_sun: Can only handle 1bit and 8bit deep displays (not %d)\n",
			SUN(ifp)->su_depth );
		return	-1;
	}

	return	0;		/* "Success" */
	}

/*
 *			S U N _ D C L O S E 
 */
_LOCAL_ int
sun_dclose(ifp)
FBIO	*ifp;
{
	if( SUNL(ifp) == (char *) NULL ) {
		fb_log( "sun_dclose: frame buffer not open.\n" );
		return	-1;
	}
	if( sun_pixwin ) {
		if( (SUN(ifp)->su_mode & MODE_2MASK) == MODE_2LINGERING ) {
			if( linger(ifp) )
				return	0;  /* parent leaves the display */
		}
		/* child or single process */
#ifdef SUN_USE_AGENT
		win_unregister( ifp->if_type );
#endif
		alarm( 0 ); /* Turn off window redraw daemon. */
		window_set(SUN(ifp)->frame, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(SUN(ifp)->frame);
		imagepw = NULL;
	} else {
		pr_close( SUNPR(ifp) );
	}
	(void) free( (char *) SUNL(ifp) );
	SUNL(ifp) = NULL;
	return	0;
}

/*
 *			S U N _ D C L E A R 
 */
_LOCAL_ int
sun_dclear(ifp, pp)
FBIO			*ifp;
register RGBpixel	*pp;
	{
	if( pp == (RGBpixel *) NULL )
		pp = (RGBpixel *) black;

	/* Clear 24-bit image in shared memory. */
	sun_storebackground( ifp, 0, 0, pp, ifp->if_width*ifp->if_height );

	/* Clear entire screen. */
	sun_rectclear( ifp, 0, 0, ifp->if_width, ifp->if_height, pp );
	return	0;
	}

/*
 *			S U N _ W I N D O W _ S E T 
 */
_LOCAL_ int
sun_window_set(ifp, xcenter, ycenter)
FBIO	*ifp;
int     xcenter, ycenter;
	{
	/*fb_log( "sun_window_set(0x%x,%d,%d)\n", ifp, xcenter , ycenter );*/
	if( SUN(ifp)->su_xcenter == xcenter && SUN(ifp)->su_ycenter == ycenter )
		return	0;
	if( xcenter < 0 || xcenter >= ifp->if_width )
		return	-1;
	if( ycenter < 0 || ycenter >= ifp->if_height )
		return	-1;
	SUN(ifp)->su_xcenter = xcenter;
	SUN(ifp)->su_ycenter = ycenter;

	/* Redraw 24-bit image from memory. */
	sun_repaint(ifp);
	return	0;
	}

/*
 *			S U N _ Z O O M _ S E T 
 */
_LOCAL_ int
sun_zoom_set(ifp, xzoom, yzoom)
FBIO	*ifp;
int	xzoom, yzoom;
	{
	/*fb_log( "sun_zoom_set(0x%x,%d,%d)\n", ifp, xzoom, yzoom );*/
	if( SUN(ifp)->su_xzoom == xzoom && SUN(ifp)->su_yzoom == yzoom )
		return	0;
	if( xzoom >= ifp->if_width || yzoom >= ifp->if_height )
		return	-1;
	SUN(ifp)->su_xzoom = xzoom;
	SUN(ifp)->su_yzoom = yzoom;

	/* Redraw 24-bit image from memory. */
	sun_repaint( ifp );
	return	0;
	}

/*
 *			S U N _ C U R S _ S E T
 */
_LOCAL_ int
sun_curs_set(ifp, bits, xbits, ybits, xorig, yorig )
FBIO		*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
	{	register int	xbytes;
		register int	y;
		Cursor		newcursor;
	/* Check size of cursor.					*/
	if( xbits < 0 )
		return	-1;
	if( xbits > 16 )
		xbits = 16;
	if( ybits < 0 )
		return	-1;
	if( ybits > 16 )
		ybits = 16;
	if( (xbytes = xbits / 8) * 8 != xbits )
		xbytes++;
#if 0
	for( y = 0; y < ybits; y++ )
		{
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
		}
#endif
	return	0;
	}

/*
 *			S U N _ C M E M O R Y _ A D D R
 */
_LOCAL_ int
sun_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{	short	xmin, ymin;
		register short	i;
		short	xwidth;
	if( ! sun_pixwin )
		return	0; /* No cursor outside of suntools yet. */
	SUN(ifp)->su_curs_on = mode;
	if( ! mode )
		{
		/* XXX turn off cursor. */
		return	0;
		}
	xwidth = ifp->if_width/SUN(ifp)->su_xzoom;
	i = xwidth/2;
	xmin = SUN(ifp)->su_xcenter - i;
	i = (ifp->if_height/2)/SUN(ifp)->su_yzoom;
	ymin = SUN(ifp)->su_ycenter - i;
	x -= xmin;
	y -= ymin;
	x *= SUN(ifp)->su_xzoom;
	y *= SUN(ifp)->su_yzoom;
	y = ifp->if_height - y;
	/* Move cursor/mouse to <x,y>. */	
	if(	x < 1 || x > ifp->if_width
	    ||	y < 1 || y > ifp->if_height
		)
		return	-1;
	/* Translate address from window to tile space. */
	x += BORDER;
	y += BORDER*2 + BANNER;
	SUN(ifp)->su_xcursor = x;
	SUN(ifp)->su_ycursor = y;
	win_setmouseposition( ifp->if_windowfd, x, y );
	return	0;
	}

/*
 *			S U N _ C S C R E E N _ A D D R
 */
_LOCAL_ int
sun_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{
	if( ! sun_pixwin )
		return	0; /* No cursor outside of suntools yet. */
	SUN(ifp)->su_curs_on = mode;
	if( ! mode )
		{
		/* XXX turn off cursor. */
		return	0;
		}
	y = ifp->if_height - y;
	/* Move cursor/mouse to <x,y>. */	
	if(	x < 1 || x > ifp->if_width
	    ||	y < 1 || y > ifp->if_height
		)
		return	-1;
	/* Translate address from window to tile space. */
	x += BORDER;
	y += BORDER*2 + BANNER;
	SUN(ifp)->su_xcursor = x;
	SUN(ifp)->su_ycursor = y;
	win_setmouseposition( ifp->if_windowfd, x, y );
	return	0;
	}

/*
 *			S U N _ B R E A D 
 */
_LOCAL_ int
sun_bread(ifp, x, y, p, count)
FBIO			*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
	{	register RGBpixel	*memp;
	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--
		)
		{
		COPYRGB( *p, *memp );
		}
	return	count;
	}

/*
 *			S U N _ B W R I T E
 */
_LOCAL_ int
sun_bwrite(ifp, x, y, p, count)
register FBIO	*ifp;
int		x, y;
RGBpixel	*p;
register int	count;
	{	int		xmax, ymax;
		register int	xwidth;
	/*fb_log( "sun_bwrite(0x%x,%d,%d,0x%x,%d)\n", ifp, x, y, p, count );
		/* XXX--debug */
	/* Store pixels in memory. */
	sun_storepixel( ifp, x, y, p, count );

	xwidth = ifp->if_width/SUN(ifp)->su_xzoom;
	xmax = count >= xwidth-x ? xwidth-1 : x+count-1;
	ymax = y + (count-1)/ ifp->if_width;
	sun_rectwrite( ifp, x, y, xmax, ymax, p, y*ifp->if_width+x );
	return	count;
	}

/*
 *			S U N _ V I E W P O R T _ S E T 
 */
_LOCAL_ int
sun_viewport_set( ifp )
FBIO	*ifp;
	{
	return	0;
	}

/*
 *			S U N _ C M R E A D 
 */
_LOCAL_ int
sun_cmread( ifp, cmp )
register FBIO		*ifp;
register ColorMap	*cmp;
	{	register int i;
	/* Just parrot back the stored colormap */
	for( i = 0; i < 256; i++)
		{
		cmp->cm_red[i] = CMR(ifp)[i]<<8;
		cmp->cm_green[i] = CMG(ifp)[i]<<8;
		cmp->cm_blue[i] = CMB(ifp)[i]<<8;
		}
	return	0;
	}

/*
 *			I S _ L I N E A R _ C M A P
 *
 *  Check for a color map being linear in R, G, and B.
 *  Returns 1 for linear map, 0 for non-linear map
 *  (ie, non-identity map).
 */
static int
is_linear_cmap(ifp)
register FBIO	*ifp;
	{ 	register int i;
	for( i=0; i<256; i++ )
		{
		if( CMR(ifp)[i] != i )  return(0);
		if( CMG(ifp)[i] != i )  return(0);
		if( CMB(ifp)[i] != i )  return(0);
		}
	return	1;
	}

/*
 *			S U N _ C M W R I T E 
 */
_LOCAL_ int
sun_cmwrite(ifp, cmp)
register FBIO		*ifp;
register ColorMap	*cmp;
	{	register int i;
	if( cmp == COLORMAP_NULL )
		{
		for( i = 0; i < 256; i++)
			{
			CMR(ifp)[i] = i;
			CMG(ifp)[i] = i;
			CMB(ifp)[i] = i;
			}
		if( SUN(ifp)->su_cmap_flag )
			{
			SUN(ifp)->su_cmap_flag = FALSE;
			sun_repaint( ifp );
			}
		SUN(ifp)->su_cmap_flag = FALSE;
		return	0;
		}
	
	for( i = 0; i < 256; i++ )
		{
		CMR(ifp)[i] = cmp-> cm_red[i]>>8;
		CMG(ifp)[i] = cmp-> cm_green[i]>>8; 
		CMB(ifp)[i] = cmp-> cm_blue[i]>>8;
		}
	SUN(ifp)->su_cmap_flag = !is_linear_cmap(ifp);
	sun_repaint( ifp );
	return	0;
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

pw_dither8Abit_rop(ifp, pr_red, pr_grn, pr_blu, size)
	FBIO		*ifp;
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
	sunrop( 0, 0, size, size, PIX_SRC, pr_comp, 0, 0 );
	pr_destroy(pr_comp);
}

pw_24dither8Abit_rop(ifp, pr_24, size)
	FBIO		*ifp;
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
	sunrop( 0, 0, size, size, PIX_SRC, pr_comp, 0, 0 );
	pr_destroy(pr_comp);
}
#endif DIT

_LOCAL_ int
sun_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", sun_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		sun_interface.if_max_width,
		sun_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		sun_interface.if_width,
		sun_interface.if_height );
	fb_log( "Usage: /dev/sun[options]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}

	return(0);
}

static int window_not_destroyed = 1;

static Notify_value
my_real_destroy(frame, status)
Frame	frame;
Destroy_status	status;
{
	if( status != DESTROY_CHECKING ) {
		window_not_destroyed = 0;
	}
	/* Let frame get destroy event */
	return( notify_next_destroy_func(frame, status) );
}

/*
 *  Lingering Window.
 *  Wait around until a "quit" is selected.  Ideally this would
 *  happen in a child process with a fork done to free the original
 *  program and return control to the shell.  However, something
 *  in Suntools seems to stop notifying this window of repaints, etc.
 *  as soon as the PID changes.  Until we figure out how to get around
 *  this the parent will have to keep hanging on.
 */
static int
linger( ifp )
FBIO	*ifp;
{
	struct timeval  timeout;

#ifdef DOFORK
	/* allow child to inherit input events */
	window_release_event_lock(SUN(ifp)->frame);
	window_release_event_lock(SUN(ifp)->canvas);
	pw_unlock(SUNPW(ifp));

	notify_dispatch();

	printf("forking\n");
	if( fork() )
		return 1;	/* release the parent */
#endif

	notify_interpose_destroy_func(SUN(ifp)->frame,my_real_destroy);

	timeout.tv_sec = 0;
	timeout.tv_usec = 250000;

	while( window_not_destroyed ) {
		(void) select(0, (long *)0, (long *)0, (long *)0, &timeout);
		notify_dispatch();
	}
	return	0;
}
