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
 *  FIGURATION NOTE below) and increase SHMPOOL appropriately.  If you
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
#include <pixrect/pixrect_hs.h>
#include <sunwindow/window_hs.h>

#include "fb.h"
#include "./fblocal.h"

extern char	*sbrk();
extern char	*calloc(), *malloc();
extern int	errno;
extern char	*shmat();
extern int	brk();

/* CONFIGURATION NOTE:  If you have 4 Megabytes of memory, best limit the
	window sizes to 512-by-512 (whether or not your using shared memory)
	because a 24-bit image is stored in memory and we don't want our
	systems to thrash do we). */
#define XMAXSCREEN	1152	/* Physical screen width in pixels. */
#define YMAXSCREEN	896	/* Physical screen height in pixels. */
#define XMAXWINDOW	512	/* Max. width of frame buffer in pixels. */
#define YMAXWINDOW	512	/* Max. height of frame buffer in pixels. */
#define BITSDEEP	1	/* No. of bit planes, same as su_depth. */
#define BANNER		18	/* Height of title bar at top of window. */
#define BORDER		2	/* Border thickness of window. */
#define PARENTBORDER	4	/* Border thickness of parent window. */
#define TITLEXOFFSET	20	/* Indentation for text within title bar. */
#define TITLEYOFFSET	4	/* Vertical padding of text within title bar. */
#define RESTXMOUSE	BORDER	/* Out of way position for mouse while */
#define RESTYMOUSE	BORDER	/*	doing pixel writes. */

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
sun_curs_set(),
sun_cmemory_addr(),
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
				 sun_curs_set,
				 sun_cmemory_addr,
				 sun_cscreen_addr,
				 "SUN Pixwin",
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

/*
 *  Per SUN (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct suninfo
	{
	short	su_curs_on;
	short	su_cmap_flag;
	short	su_xzoom;
	short	su_yzoom;
	short	su_xcenter;
	short	su_ycenter;
	short	su_xcursor;
	short	su_ycursor;
	short	su_depth;
	};
#define if_mem		u2.p	/* shared memory pointer */
#define if_cmap		u3.p	/* color map in shared memory */
#define if_windowfd	u4.l	/* window file descriptor under SUNTOOLS */
#define if_shmid	u6.l	/* shared memory ID */
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

#define SUNPW(ptr)	((Pixwin *)((ptr)->u5.p))
#define SUNPWL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define SUNPR(ptr)	((Pixrect *)((ptr)->u5.p))
#define SUNPRL(ptr)	((ptr)->u5.p)	/* left hand side version. */
#define	SUN(ptr)	((struct suninfo *)((ptr)->u1.p))
#define SUNL(ptr)	((ptr)->u1.p)	/* left hand side version. */
#define CMR(x)		((struct sun_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct sun_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct sun_cmap *)((x)->if_cmap))->cmb

static int	sun_redraw = FALSE;  /* Doing a sun_repaint. */
static int	sun_damaged = FALSE; /* SIGWINCH fired, need sun_repair. */
static int	sun_pixwin = FALSE;  /* Running under SUNTOOLS. */

struct sun_cmap
	{
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
	};

static int	dither_flg = FALSE;   /* dither output */

/* One scanline wide buffer */
extern struct pixrectops mem_ops;
char		sun_mpr_buf[1024];
mpr_static( sun_mpr, 1024, 1, BITSDEEP, (short *)sun_mpr_buf );

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

unsigned char   others[256];	/* unused colors */
unsigned char   red8Amap[256];	/* red 8bit dither color map */
unsigned char   grn8Amap[256];	/* green 8bit dither color map */
unsigned char   blu8Amap[256];	/* blue 8bit dither color map */

static unsigned char redmap[256], grnmap[256], blumap[256];

#define RR	6
#define GR	7
#define BR	6

static int      biggest = RR * GR * BR - 1;

#define COLOR_APPROX(p) \
	(((*p)[RED] * RR ) / 256) * GR*BR + \
	(((*p)[GRN] * GR ) / 256) * BR  + \
	(((*p)[BLU] * BR ) / 256) + 1

_LOCAL_ int
sun_sigwinch( sig )
	{
	sun_damaged = TRUE;
	return	sig;
	}

sun_repair( pw )
Pixwin	*pw;
	{
	pw_damaged( pw );
	pw_repairretained( pw );
	pw_donedamaged( pw );
	sun_damaged = FALSE;
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
		(*memp)[RED] = (*p)[RED];
		(*memp)[GRN] = (*p)[GRN];
		(*memp)[BLU] = (*p)[BLU];
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
		(*memp)[RED] = (*p)[RED];
		(*memp)[GRN] = (*p)[GRN];
		(*memp)[BLU] = (*p)[BLU];
		}
	return;
	}

_LOCAL_ void
sun_repaint( ifp )
register FBIO	*ifp;
	{	register int	y;
		register int	i;
		register int	ymin, ymax;
		int		xmin, xmax;
		int		xwidth;
		int		xscroff, yscroff;
		int		xscrpad, yscrpad;
	/*fb_log( "sun_repaint: xzoom=%d yzoom=%d xcenter=%d ycenter=%d\n",
		SUN(ifp)->su_xzoom, SUN(ifp)->su_yzoom, SUN(ifp)->su_xcenter, SUN(ifp)->su_ycenter );*/
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
		sunrop(	0, 0,
			xscroff,
			ifp->if_height,
			PIX_SET,
			(Pixrect *) NULL, 0, 0
			);
	/* Blank out area below image.			*/
	if( yscroff > 0 )
		sunrop(	0, ifp->if_height-yscroff,
			ifp->if_width, yscroff,
			PIX_SET,
			(Pixrect *) NULL, 0, 0
			);
	/* Blank out area right of image.			*/
	if( xscrpad > 0 )
		sunrop(	ifp->if_width-xscrpad, 0,
			xscrpad, ifp->if_height,
			PIX_SET,
			(Pixrect *) NULL, 0, 0
			);
	/* Blank out area above image.			*/
	if( yscrpad > 0 )
		sunrop(	0, 0,
			ifp->if_width, yscrpad,
			PIX_SET,
			(Pixrect *) NULL, 0, 0
			);
	sun_redraw = TRUE;
	for( y = ymin; y <= ymax; y++ )
		if( sun_bwrite(	ifp,
				xmin, y,
				(RGBpixel *)(ifp->if_mem+(y*ifp->if_width+xmin)*sizeof(RGBpixel)),
				xwidth
				)
		     <	xwidth
			)
			fb_log( "sun_repaint: Write of %d pixels to <%d,%d> failed.\n", xwidth, xmin, y );
	sun_redraw = FALSE;
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
 * 
 *  There are a few oddities, however.  The worst is that System V will
 *  not allow the break (see sbrk(2)) to be set above a shared memory
 *  segment, and shmat(2) does not seem to allow the selection of any
 *  reasonable memory address (like 6 Mbytes up) for the shared memory.
 *  In the initial version of this routine, that prevented subsequent
 *  calls to malloc() from succeeding, quite a drawback.  The work-around
 *  used here is to increase the current break to a large value,
 *  attach to the shared memory, and then return the break to it's
 *  original value.  This should allow most reasonable requests for
 *  memory to be satisfied.  In special cases, the values used here
 *  might need to be increased.
 */
_LOCAL_ int
sun_getmem( ifp )
FBIO	*ifp;
	{
		int	pixsize;
		int	size;
		int	i;
		int	new = 0;
		char	*sp;

	errno = 0;
	pixsize = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
	size = pixsize + sizeof(struct sun_cmap);
	size = (size + 4096-1) & ~(4096-1);

#define SHMEM_KEY	42
	/* First try to attach to an existing one */
	if( (ifp->if_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )
		{ /* No existing one, create a new one */
		if( (ifp->if_shmid = shmget(
		    SHMEM_KEY, size, IPC_CREAT|0666 )) < 0 )
			{
			fb_log( "if_sun: shmget failed, errno=%d\n", errno );
			goto fail;
			}
		new = 1;
		}

	/* Open the segment Read/Write, near the current break */
	if( (sp = shmat( ifp->if_shmid, 0, 0 )) == (char *)(-1) )
		{
		fb_log("shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
		}
	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* Color map at end of area */

	/* Provide non-black colormap on creation of new shared mem */
	if( new )
		{	static RGBpixel black = { 0, 0, 0 };
		sun_cmwrite( ifp, COLORMAP_NULL );
		sun_storebackground( ifp, 0, 0, black, ifp->if_max_width*ifp->if_max_height );
		}
	return	0;
fail:
	fb_log("sun_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_sun.c for details\n");
	if(	ifp->if_mem == (char *) NULL
	    ||	(ifp->if_mem = malloc( size )) == NULL )
		{
		fb_log( "sun_getmem:  malloc failure, couldn't allocate %d bytes\n", size );
		return	-1;
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
		int		i;
		int		x;
		struct pr_prpos	where;
		struct pixfont	*myfont;
	if( file != NULL )
		{	register char *cp;
			int mode;
		/* "/dev/sun###" gives optional mode */
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
		mode = 0;
		if( *cp && isdigit(*cp) )
			(void)sscanf( cp, "%d", &mode );
		if( mode >= 99 )
			{ /* Attempt to release shared memory segment */
			sun_zapmem();
			return	-1;
			}
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

	myfont = pf_open( "/usr/lib/fonts/fixedwidthfonts/screen.b.14" );

	/* Create window. */
        if( sun_pixwin = (we_getgfxwindow(sun_parentwinname) == 0) )
		{	int	parentno;
			int	parentfd;
			Rect	parentrect;
			int	sun_windowfd;
			Pixwin	*sun_windowpw;
		if( (parentfd = open( sun_parentwinname, 2 )) < 0 )
			{
			fb_log( "sun_dopen, couldn't open parent window.\n" );
			return	-1;	/* FAIL */
			}
		win_getrect( parentfd, &parentrect );

        	/* Running under SunView, with windows */
		if( (sun_windowfd = win_getnewwindow()) == -1 )
			{
			fb_log("sun_dopen:  win_getnewwindow failed\n");
			return	-1;     /* FAIL */
			}
		winrect.r_left = rect_right(&parentrect) -
				(width+BORDER*2+PARENTBORDER);
		winrect.r_top = rect_bottom(&parentrect) -
				(height+BANNER+BORDER*3+PARENTBORDER+13);
		winrect.r_width = width+BORDER*2;
		winrect.r_height = height+BANNER+BORDER*3;
		win_setrect( sun_windowfd, &winrect );
		parentno = win_nametonumber( sun_parentwinname );
		win_setlink( sun_windowfd, WL_PARENT, parentno );
		win_insert( sun_windowfd );
		sun_windowpw = pw_open(sun_windowfd);
		SUNPWL(ifp) = (char *)
			pw_region(	sun_windowpw,
					BORDER, BANNER+BORDER*2,
					width, height
					);
		if( width > winrect.r_width )
			width = winrect.r_width;
		if( height > winrect.r_height )
			height = winrect.r_height;

		ifp->if_windowfd = sun_windowfd;
		SUN(ifp)->su_depth = SUNPW(ifp)->pw_pixrect->pr_depth;
		SUNPW(ifp)->pw_prretained = mem_create( width, height, SUN(ifp)->su_depth );
		sun_mpr.pr_depth = SUN(ifp)->su_depth;
		if( SUN(ifp)->su_depth == 8 )
			{
			if( dither_flg )
				{
#ifdef DIT
				draw8Ainit();
				dither8Ainit();
				pw_set8Amap(SUN(ifp), &sun_cmap);
#endif DIT
				}
			else
				{
				/* r | g | b, values = RR, GR, BR */
				/* set a new cms name; initialize it */
				x = pw_setcmsname(SUN(ifp), "libfb");
				for (x = 0; x < (RR * GR * BR); x++)
					{	int             new;
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
				x = pw_putcolormap(SUN(ifp), 0, 256, redmap, grnmap, blumap);
				}
			}
		/* Outer border is black. */
		pw_rop( sun_windowpw, 0, 0,
			winrect.r_width, winrect.r_height,
			PIX_SET, (Pixrect *) NULL, 0, 0 );
		/* Inner border is white. */
		pw_rop( sun_windowpw, 1, 1,
			winrect.r_width-2, winrect.r_height-2,
			PIX_CLR, (Pixrect *) NULL, 0, 0 );
		/* Black out title bar. */
		pw_rop( sun_windowpw, BORDER, BORDER,
			width, BANNER,
			PIX_SET, (Pixrect *) NULL, 0, 0 );
		/* Draw title in title bar (banner). */
		pw_ttext( sun_windowpw, TITLEXOFFSET, BANNER - TITLEYOFFSET,
			  PIX_CLR,
			  myfont, "BRL libfb Frame Buffer" );
		}
	else
		{	static Pixrect	*myscreen = NULL;
			static Pixrect	*mywindow;
		if( myscreen == (Pixrect *) NULL )
			{
			myscreen = pr_open( "/dev/fb" );
			mywindow = pr_region(	myscreen,
					XMAXSCREEN-width-BORDER*2,
					YMAXSCREEN-(height+BANNER+BORDER*3),
					width+BORDER*2, height+BANNER+BORDER*3
					);
			}
		SUNPRL(ifp) = (char *)
			pr_region(	mywindow,
					BORDER, BANNER+BORDER*2,
					width, height
					);
		/* Outer border is black. */
		pr_rop( mywindow, 0, 0,
			width+BORDER*2, height+BANNER+BORDER*3,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Inner border is white. */
		pr_rop( mywindow, 1, 1,
			width+2, height+BANNER+BORDER*2,
			PIX_DONTCLIP | PIX_CLR,
			(Pixrect *) NULL, 0, 0 );
		/* Black out title bar. */
		pr_rop( mywindow, BORDER, BORDER,
			width, BANNER,
			PIX_DONTCLIP | PIX_SET,
			(Pixrect *) NULL, 0, 0 );
		/* Draw title in title bar (banner). */
		where.pr = mywindow;
		where.pos.x = TITLEXOFFSET;
		where.pos.y = BANNER - TITLEYOFFSET;
		pf_ttext(	where,
				PIX_CLR,
				myfont, "BRL libfb Frame Buffer" );
		SUN(ifp)->su_depth = SUNPR(ifp)->pr_depth;
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

	(void) signal( SIGWINCH, sun_sigwinch );
	return	0;		/* "Success" */
	}

/*
 *			S U N _ D C L O S E 
 */
_LOCAL_ int
sun_dclose(ifp)
FBIO	*ifp;
	{
	if( SUNL(ifp) == (struct suninfo *) NULL )
		{
		fb_log( "sun_dclose: frame buffer not open.\n" );
		return	-1;
		}
	if( sun_pixwin )
		{
		pw_close( SUNPW(ifp) );
		win_remove( ifp->if_windowfd );
		}
	else
		{
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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );

	sun_storebackground( ifp, 0, 0, pp, ifp->if_width*ifp->if_height );
	sun_repaint( ifp );
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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );
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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );
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
	{	register int	y;
		register int	xbytes;
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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );
	for(	memp =	(RGBpixel *)
			(&ifp->if_mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]);
		count > 0;
		memp++, p++, count--
		)
		{
		(*p)[RED] = (*memp)[RED];
		(*p)[GRN] = (*memp)[GRN];
		(*p)[BLU] = (*memp)[BLU];
		}
	return	count;
	}

_LOCAL_ RGBpixel *
sun_cmapval( ifp, p )
register FBIO		*ifp;
register RGBpixel	*p;
	{	static RGBpixel	cmval;
	if( SUN(ifp)->su_cmap_flag )
		{
		cmval[RED] = CMR(ifp)[(*p)[RED]];
		cmval[GRN] = CMG(ifp)[(*p)[GRN]];
		cmval[BLU] = CMB(ifp)[(*p)[BLU]];
		return	(RGBpixel *) cmval;
		}
	else
		return	p;
	}

/*
 *			S U N _ B W R I T E
 */
_LOCAL_ int
sun_bwrite(ifp, x, y, p, count)
register FBIO		*ifp;
int			x, y;
register RGBpixel	*p;
register int		count;
	{	register	scrx, scry;
		register	value;
		register	i, j;
		register	cnt;
		register	cury;
		RGBpixel	*base = p;
	/*fb_log( "sun_bwrite(0x%x,%d,%d,0x%x,%d)\n", ifp, x, y, p, count );*/
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );
	scrx =	(x*SUN(ifp)->su_xzoom -
		(SUN(ifp)->su_xcenter*SUN(ifp)->su_xzoom - ifp->if_width/2));
	scry =	(ifp->if_height-1) -
		(y*SUN(ifp)->su_yzoom -
		(SUN(ifp)->su_ycenter*SUN(ifp)->su_yzoom - ifp->if_height/2));
	if( scrx < 0 || scrx >= ifp->if_width )
		return	0;
	if( scry < 0 || scry >= ifp->if_height )
		return	0;

	/* Store pixels in memory. */
	if( ! sun_redraw )
		sun_storepixel( ifp, x, y, p, count );

	if( sun_pixwin )
		{ /* Lock the display and get the cursor out of the way. */
		pw_lock( SUNPW(ifp), SUNPW(ifp)->pw_pixrect );
		win_setmouseposition(	ifp->if_windowfd,
					RESTXMOUSE,
					RESTYMOUSE
					);
		}

	/* Take care of single-pixel case. */
	if( count == 1 )
		{	register RGBpixel *vp = sun_cmapval( ifp, p );
		if( SUN(ifp)->su_depth < 8 )
			value = ((*vp)[RED] + (*vp)[GRN] + (*vp)[BLU]);
		else
			value = COLOR_APPROX(vp);
		for(	i = 0, cury = scry;
			i < SUN(ifp)->su_yzoom && cury > 0;
			i++, cury-- )
			{	register curx;
			for(	j = 0, curx = scrx+j;
				j < SUN(ifp)->su_xzoom
			     &&	curx < ifp->if_width;
				j++, curx++ )
				{	register int	val;
				/* 0 gives white, 1 gives black */
				val = (value < dither[curx&07][cury&07]*3);
				sunput( curx, cury, val );
				}
			}
		if( sun_pixwin )
			{ /* Release display lock and restore cursor. */
			pw_unlock( SUNPW(ifp) );
			win_setmouseposition(	ifp->if_windowfd,
						SUN(ifp)->su_xcursor,
						SUN(ifp)->su_ycursor
						);
			}
		return	count;
		}

	/* Multiple pixels. */
	bzero( sun_mpr_buf, ifp->if_width * SUN(ifp)->su_depth / sizeof(char) );

	/* Need to write out each scanline YZOOM times. */
	for(	i = 0, cury = scry;
		i < SUN(ifp)->su_yzoom
	    &&	cury >= 0
	    &&	cury < ifp->if_height;
		i++, cury--, p = base )
		{	register int	curx;
		curx = 0;
		for( cnt = 0; cnt < count; cnt++, p++ )
			{	register RGBpixel *vp = sun_cmapval( ifp, p );
			/* Write pixel to memory pixrect. */
			if( SUN(ifp)->su_depth < 8 )
				{ /* Must build pixel from dither pattern,
					whose dimensions are based on the
					zoom factors. */
				value = ((*vp)[RED] + (*vp)[GRN] + (*vp)[BLU]);
				for( j = 0; j < SUN(ifp)->su_xzoom; j++ )
					{
					if( SUN(ifp)->su_depth < 8 )
						{ /* 0 gives white, 1 gives black */
						if( value < dither[curx+j&07][cury&07]*3 )
							sun_mpr_buf[curx+j>>3] |= (0x80>>(curx+j&07));
						}
					}
				}
			else
				{
				value = COLOR_APPROX(vp);
				for( j = 0; j < SUN(ifp)->su_xzoom; j++ )
					sun_mpr_buf[curx+j] = value;
				}
			curx += SUN(ifp)->su_xzoom;
	
			/* Output full scan-line. */
			if (curx >= ifp->if_width)
				{
				sunrop( scrx, cury,
					ifp->if_width, 1,
					PIX_SRC, &sun_mpr, 0, 0	);
				bzero( (char *) sun_mpr_buf,
					ifp->if_width * SUN(ifp)->su_depth
					/ sizeof(char) );
				}
			} /* all pixels done */
		}
	if( sun_pixwin )
		{ /* Release display lock and restore cursor. */
		pw_unlock( SUNPW(ifp) );
		win_setmouseposition(	ifp->if_windowfd,
					SUN(ifp)->su_xcursor,
					SUN(ifp)->su_ycursor
					);
		}
	return	count;
	}

/*
 *			S U N _ V I E W P O R T _ S E T 
 */
_LOCAL_ int
sun_viewport_set( ifp )
FBIO	*ifp;
	{
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );
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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );

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
	if( sun_damaged )
		sun_repair( SUNPW(ifp) );

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
