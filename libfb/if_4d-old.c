#undef _LOCAL_
#define _LOCAL_ /**/
/*
 *			I F _ 4 D . C
 *
 *  Authors -
 *	Paul R. Stay
 *	Michael John Muuss
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <gl.h>
#include <device.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <gl/immed.h>
#include <signal.h>
#undef RED

#include "fb.h"
#include "./fblocal.h"

#define bzero(p,cnt)	memset(p,'\0',cnt)

extern char *sbrk();
extern char *malloc();
extern int errno;
extern char *shmat();
extern int brk();

static Cursor	cursor =
	{
#include "./sgicursor.h"
	};

_LOCAL_ int	mips_dopen(),
		mips_dclose(),
		mips_dclear(),
		mips_bread(),
		mips_bwrite(),
		mips_cmread(),
		mips_cmwrite(),
		mips_viewport_set(),
		mips_window_set(),
		mips_zoom_set(),
		mips_curs_set(),
		mips_cmemory_addr(),
		mips_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO mips_interface =
		{
		mips_dopen,
		mips_dclose,
		fb_null,		/* reset? */
		mips_dclear,
		mips_bread,
		mips_bwrite,
		mips_cmread,
		mips_cmwrite,
		mips_viewport_set,
		mips_window_set,
		mips_zoom_set,
		mips_curs_set,
		mips_cmemory_addr,
		fb_null,		/* cscreen_addr */
		"Silicon Graphics 4D/60T",
		1280,			/* max width */
		1024,			/* max height */
		"/dev/mips",
		1024,			/* current/default width  */
		1024,			/* current/default height */
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


_LOCAL_ Colorindex get_Color_Index();
_LOCAL_ void	mpw_inqueue();
_LOCAL_ void	mips_rectf_pix();
static int	is_linear_cmap();

struct mips_cmap {
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
};

/*
 *  Per MIPS (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */
struct mipsinfo {
	short	mi_curs_on;
	short	mi_xzoom;
	short	mi_yzoom;
	short	mi_xcenter;
	short	mi_ycenter;
	int	mi_rgb_ct;
	short	mi_cmap_flag;
	int	mi_shmid;
};
#define	MIPS(ptr)	((struct mipsinfo *)((ptr)->u1.p))
#define	MIPSL(ptr)	((ptr)->u1.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct mips_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct mips_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct mips_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(1024-ifp->if_width-MARGIN)
#define WIN_R	(1024-1-MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height-1+MARGIN)

static int map_size;			/* # of color map slots available */

#define MODE_MALLOC	0		/* Use malloc memory */
#define MODE_SHARED	1		/* Use Shared memory */

static RGBpixel	rgb_table[4096];

/*
 *			M I P S _ G E T M E M
 *
 *  Because there is no hardware zoom or pan, we need to repaint the
 *  screen (with big pixels) to implement these operations.
 *  This means that the actual "contents" of the frame buffer need
 *  to be stored somewhere else.  If possible, we allocate a shared
 *  memory segment to contain that image.  This has several advantages,
 *  the most important being that when operating the display in 12-bit
 *  output mode, pixel-readbacks still give the full 24-bits of color.
 *  System V shared memory persists until explicitly killed, so this
 *  also means that in MEX mode, the previous contents of the frame
 *  buffer still exist, and can be again accessed, even though the
 *  MEX windows are transient, per-process.
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
mips_getmem( ifp )
FBIO	*ifp;
{
#define SHMEM_KEY	42
	int	pixsize;
	int	size;
	int	i;
	char	*old_brk;
	char	*new_brk;
	char	*sp;
	int	new = 0;


	errno = 0;
	pixsize = 1024 * 1280 * sizeof(RGBpixel);
	size = pixsize + sizeof(struct mips_cmap);
	size = (size + 4096-1) & ~(4096-1);

	if( ifp->if_mode == MODE_MALLOC )
	{
	
		sp = malloc( size );
		if( sp == 0 )
		{
			fb_log("if_sgi: frame buffer memory malloc failed\n");
			goto fail;
		}

		ifp->if_mem = sp;
		ifp->if_cmap = sp + pixsize;	/* cmap at end of area */

		mips_cmwrite( ifp, COLORMAP_NULL );

		return(0);
	}

	/* First try to attach to an existing one */
	if( (MIPS(ifp)->mi_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )  {
		/* No existing one, create a new one */
		if( (MIPS(ifp)->mi_shmid = shmget(
		    SHMEM_KEY, size, IPC_CREAT|0666 )) < 0 )  {
			fb_log("if_sgi: shmget failed, errno=%d\n", errno);
			goto fail;
		}
		new = 1;
	}

	/* Move up the existing break, to leave room for later malloc()s */
	old_brk = sbrk(0);
	new_brk = (char *)(6 * 1024 * 1024);
	if( new_brk <= old_brk )
		new_brk = old_brk + 1024 * 1024;
	new_brk = (char *)((((int)new_brk) + 4096-1) & ~(4096-1));
	if( brk( new_brk ) < 0 )  {
		fb_log("new brk(x%x) failure, errno=%d\n", new_brk, errno);
		goto fail;
	}

	/* Open the segment Read/Write, near the current break */
	if( (sp = shmat( MIPS(ifp)->mi_shmid, 0, 0 )) == (char *)(-1) )  {
		fb_log("shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}

	/* Restore the old break */
	if( brk( old_brk ) < 0 )  {
		fb_log("restore brk(x%x) failure, errno=%d\n", old_brk, errno);
		/* Take the memory and run */
	}

	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* cmap at end of area */

	/* Provide non-black colormap on creation of new shared mem */
	if(new)
		mips_cmwrite( ifp, COLORMAP_NULL );
	return(0);
fail:
	fb_log("mips_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_sgi.c for details\n");
	if( (sp = malloc( size )) == NULL )  {
		fb_log("mips_getmem:  malloc failure\n");
		return(-1);
	}
	ifp->if_mem = sp;
	return(0);
}

/*
 *			M I P S _ Z A P M E M
 */
void
mips_zapmem()
{
	int shmid;
	int i;

	if( (shmid = shmget( SHMEM_KEY, 0, 0 )) < 0 )  {
		fb_log("mips_zapmem shmget failed, errno=%d\n", errno);
		return;
	}

	i = shmctl( shmid, IPC_RMID, 0 );
	if( i < 0 )  {
		fb_log("mips_zapmem shmctl failed, errno=%d\n", errno);
		return;
	}
	fb_log("if_mips: shared memory released\n");
}

/*
 *			M I P S _ R E P A I N T
 *
 *  Given the current window center, and the current zoom,
 *  repaint the screen from the shared memory buffer,
 *  which stores RGB pixels.
 */
_LOCAL_ int
mips_repaint( ifp )
register FBIO	*ifp;
{
	short xmin, xmax;
	short ymin, ymax;
	register short i;
	register unsigned char *ip;
	short y;
	short xscroff, yscroff, xscrpad, yscrpad;
	short xwidth;
	static RGBpixel black = { 0, 0, 0 };
	im_setup;

	if( MIPS(ifp)->mi_curs_on )
		cursoff();		/* Cursor interferes with drawing */

	xscroff = yscroff = 0;
	xscrpad = yscrpad = 0;
	xwidth = ifp->if_width/MIPS(ifp)->mi_xzoom;
	i = xwidth/2;
	xmin = MIPS(ifp)->mi_xcenter - i;
	xmax = MIPS(ifp)->mi_xcenter + i - 1;
	i = (ifp->if_height/2)/MIPS(ifp)->mi_yzoom;
	ymin = MIPS(ifp)->mi_ycenter - i;
	ymax = MIPS(ifp)->mi_ycenter + i - 1;
	if( xmin < 0 )  {
		xscroff = -xmin * MIPS(ifp)->mi_xzoom;
		xmin = 0;
	}
	if( ymin < 0 )  {
		yscroff = -ymin * MIPS(ifp)->mi_yzoom;
		ymin = 0;
	}
	if( xmax > ifp->if_width-1 )  {
		xscrpad = (xmax-(ifp->if_width-1))*MIPS(ifp)->mi_xzoom;
		xmax = ifp->if_width-1;
	}
	if( ymax > ifp->if_height-1 )  {
		yscrpad = (ymax-(ifp->if_height-1))*MIPS(ifp)->mi_yzoom;
		ymax = ifp->if_height-1;
	}

	/* Blank out area left of image.			*/
	if( xscroff > 0 )
		mips_rectf_pix(	ifp,
				0, 0,
				(Scoord) xscroff-1, (Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	/* Blank out area below image.			*/
	if( yscroff > 0 )
		mips_rectf_pix(	ifp,
				0, 0,
				(Scoord) ifp->if_width-1, (Scoord) yscroff-1,
				(RGBpixel *) black
				);
	/* Blank out area right of image.			*/
	if( xscrpad > 0 )
		mips_rectf_pix(	ifp,
				(Scoord) ifp->if_width-xscrpad,
				0,
				(Scoord) ifp->if_width-1,
				(Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	/* Blank out area above image.			*/
	if( yscrpad > 0 )
		mips_rectf_pix(	ifp,
				0,
				(Scoord) ifp->if_height-yscrpad,
				(Scoord) ifp->if_width-1,
				(Scoord) ifp->if_height-1,
				(RGBpixel *) black
				);
	for( y = ymin; y <= ymax; y++ )  {

		ip = (unsigned char *)
			&ifp->if_mem[(y*1024+xmin)*sizeof(RGBpixel)];

		if( ifp->if_zoomflag )  {
			register Scoord l, b, r, t;

			l = xscroff;
			b = yscroff + (y-ymin)*MIPS(ifp)->mi_yzoom;
			t = b + MIPS(ifp)->mi_yzoom - 1;
			if ( MIPS(ifp)->mi_cmap_flag == FALSE )  {
				for( i=xwidth; i > 0; i--)  
				{
 					RGBcolor( ip[RED], ip[GRN], ip[BLU]);
					r = l + MIPS(ifp)->mi_xzoom - 1;
					rectfs( l, b, r, t );
					l = r + 1;
					ip += sizeof(RGBpixel);
				}
			} else {
				for( i=xwidth; i > 0; i--)  
				{
				    	RGBcolor( CMR(ifp)[ip[RED]], 
						CMG(ifp)[ip[GRN]], 
						CMB(ifp)[ip[BLU]] );

					r = l + MIPS(ifp)->mi_xzoom - 1;
					im_rectfs( l, b, r, t );
					l = r + 1;
					ip += sizeof(RGBpixel);
				}
			}
			continue;
		}

		/* Non-zoomed case */
		cmov2s(xscroff, yscroff +( y - ymin) );

		if ( MIPS(ifp)->mi_cmap_flag == FALSE )  {
			long amount, n;

			n = xwidth;

			while( n > 0 )
			{
				amount = n > 30 ? 30 : n;

				im_passthru( amount + amount + amount + 2);
				im_outshort( FBCRGBdrawpixels);
				im_outshort( amount );
				
				n -= amount;
				while( --amount != -1 )
				{
					im_outshort( *ip++ );
					im_outshort( *ip++ );
					im_outshort( *ip++ );
				}
				amount = amount;
			}
			im_freepipe;
			GEWAIT;
		} else {
			long amount, n;

			n = xwidth;

			while( n > 0 )
			{
				amount = n > 30 ? 30 : n;

				im_passthru( amount + amount + amount + 2);
				im_outshort( FBCRGBdrawpixels);
				im_outshort( amount );
				
				n -= amount;
				while( --amount != -1 )
				{
					im_outshort( CMR(ifp)[*ip++] );
					im_outshort( CMR(ifp)[*ip++] );
					im_outshort( CMR(ifp)[*ip++] );
				}
				amount = amount;
			}
			im_freepipe;
			GEWAIT;
		}
	}
	if( MIPS(ifp)->mi_curs_on )
		curson();		/* Cursor interferes with reading! */
}

int fb_parent;
sigkid()
{

	exit(0);
}

/*
 *			M I P S _ D O P E N
 */
_LOCAL_ int
mips_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	int f;

	/* the Silicon Graphics Library Window management routines
	 * use shared memory. This causes lots of problems when you
	 * want to pass a window structure to a child process.
	 * One hack to get around this is to immediately fork
	 * and create a child process and sleep until the child
	 * sends a kill signal to the parent process. (in FBCLOSE)
	 * This allows us to use the traditional fb utility programs 
	 * as well as allow the frame buffer window to remain around
	 * until killed by the menu subsystem.
    	 */
	fb_parent = getpid();		/* save parent pid */

	signal( SIGUSR1, sigkid);

	if(((f = fork()) != 0 ) && ( f != -1)) 
	{
		int k;
		for(k=0; k< 20; k++)
			(void) close(k);
		pause();		/* pause until killed */
		exit(0);
	}

	if( file != NULL )  {
		register char *cp;
		int mode;	

		/* "/dev/mips###" gives optional mode for shared memory*/
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;

		if( *cp && isdigit(*cp) )
			(void)sscanf( cp, "%d", &mode );

		ifp->if_mode = MODE_SHARED;

		if( mode == 1 ) 
			ifp->if_mode = MODE_MALLOC;

		if( mode > 99 )  {
			/* Attempt to release shared memory segment */
			mips_zapmem();
			return(-1);
		}
	} else
		ifp->if_mode = MODE_SHARED;
	

	if( (MIPSL(ifp) = (char *)calloc( 1, sizeof(struct mipsinfo) )) == NULL )  {
		fb_log("mips_dopen:  mipsinfo malloc failed\n");
		return(-1);
	}

	/* By default, pop up a 512x512 MEX window, rather than fullsize */
	if( width <= 0 )
		width = 512;
	if( height <= 0 )
		height = 512;
	if ( width > ifp->if_max_width)
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height - 2 * MARGIN - BANNER)
		height = ifp->if_max_height - 2 * MARGIN - BANNER;

	ifp->if_width = width;
	ifp->if_height = height;

	blanktime(0);

	prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
	foreground();		/* Direct focus here, don't detach */
	if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
	{
		fb_log( "No more graphics ports available.\n" );
		return	-1;
	}
	if (ifp->if_mode == MODE_MALLOC)
		wintitle( "BRL libfb Frame Buffer" );
	else
		wintitle( "BRL libfb Frame Buffer Shared Memory" );
	
	/* Free window of position constraint.		*/
	prefsize( (long)ifp->if_width, (long)ifp->if_height );
	winconstraints();
	singlebuffer();
	RGBmode();
	gconfig();	/* Must be called after singlebuffer().	*/
			/* Need to clean out images from windows below */
			/* This hack is necessary until windows persist from
			 * process to process */
	color(BLACK);
	clear();

	/* Must initialize these window state variables BEFORE calling
		"mips_getmem", because this function can indirectly trigger
		a call to "mips_repaint" (when initializing shared memory
		after a reboot).					*/
	ifp->if_zoomflag = 0;
	MIPS(ifp)->mi_xzoom = 1;	/* for zoom fakeout */
	MIPS(ifp)->mi_yzoom = 1;	/* for zoom fakeout */
	MIPS(ifp)->mi_xcenter = width/2;
	MIPS(ifp)->mi_ycenter = height/2;

	if( mips_getmem(ifp) < 0 )
		return(-1);

	/* Must call "is_linear_cmap" AFTER "mips_getmem" which allocates
		space for the color map.				*/
	MIPS(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );
	drawmode(CURSORDRAW);
	mapcolor(1, 255, 255, 255);
	drawmode(NORMALDRAW);
	setcursor(1, 1, 0);

	/* The screen has no useful state.  Restore it as it was before */
	/* SMART deferral logic needed */
	mips_repaint( ifp );
	return	0;
}

/*
 *			M I P S _ D C L O S E
 *
 *  Finishing with a gexit() is mandatory.
 *  If we do a tpon() greset(), the text port pops up right
 *  away, spoiling the image.  Just doing the greset() causes
 *  the region of the text port to be disturbed, although the
 *  text port does not become readable.
 *
 *  Unfortunately, this means that the user has to run the
 *  "gclear" program before the screen can be used again,
 *  which is certainly a nuisance.  On the other hand, this
 *  means that images can be created AND READ BACK from
 *  separate programs, just like we do on the real framebuffers.
 */
_LOCAL_ int
mips_dclose( ifp )
FBIO	*ifp;
{
	int menu, menuval, val, dev, f;

	kill(fb_parent, SIGUSR1);

	menu = defpup("close");
	qdevice(RIGHTMOUSE);
	qdevice(REDRAW);
	
	while(1)
	{
		while( (qtest()))
		{
			dev = qread( &val );
			if (dev == RIGHTMOUSE)
			{
				menuval = dopup( menu );
				if (menuval == 1 ) {
				/* 20 minute blanking when fb closed */
					blanktime( (long) 67 * 60 * 20L );	
					gexit();

					if( MIPSL(ifp) != NULL )
						(void)free( (char *)MIPS(ifp) );
					return(0);					
				}
			}
			if (dev == REDRAW) {
				reshapeviewport();
				mips_repaint(ifp);
			}
		}
	}
}

/*
 *			M I P S _ D C L E A R
 */
_LOCAL_ int
mips_dclear( ifp, pp )
FBIO	*ifp;
register RGBpixel	*pp;
{

	if( qtest() )
		mpw_inqueue(ifp);

	if ( pp != RGBPIXEL_NULL)  {
		register char *op = ifp->if_mem;
		register int cnt;

		/* Slightly simplistic -- runover to right border */
		for( cnt=1024*ifp->if_height-1; cnt > 0; cnt-- )  {
			*op++ = (*pp)[RED];
			*op++ = (*pp)[GRN];
			*op++ = (*pp)[BLU];
		}

		RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
	} else {
		RGBcolor( (short) 0, (short) 0, (short) 0);
		bzero( ifp->if_mem, 1024*ifp->if_height*sizeof(RGBpixel) );
	}
	clear();
	return(0);
}

/*
 *			M I P S _ W I N D O W _ S E T
 */
_LOCAL_ int
mips_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	if( qtest() )
		mpw_inqueue(ifp);

	if( MIPS(ifp)->mi_xcenter == x && MIPS(ifp)->mi_ycenter == y )
		return(0);
	if( x < 0 || x >= ifp->if_width )
		return(-1);
	if( y < 0 || y >= ifp->if_height )
		return(-1);
	MIPS(ifp)->mi_xcenter = x;
	MIPS(ifp)->mi_ycenter = y;
	mips_repaint( ifp );
	return(0);
}

/*
 *			M I P S _ Z O O M _ S E T
 */
_LOCAL_ int
mips_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	if( qtest() )
		mpw_inqueue(ifp);

	if( x < 1 ) x = 1;
	if( y < 1 ) y = 1;
	if( MIPS(ifp)->mi_xzoom == x && MIPS(ifp)->mi_yzoom == y )
		return(0);
	if( x >= ifp->if_width || y >= ifp->if_height )
		return(-1);

	MIPS(ifp)->mi_xzoom = x;
	MIPS(ifp)->mi_yzoom = y;

	if( MIPS(ifp)->mi_xzoom > 1 || MIPS(ifp)->mi_yzoom > 1 )
		ifp->if_zoomflag = 1;
	else	ifp->if_zoomflag = 0;

	mips_repaint( ifp );
	return(0);
}

/*
 *			M I P S _ B R E A D
 */
_LOCAL_ int
mips_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{
	register short scan_count;
	short xpos, ypos;
	register char *ip;
	int ret;

	if( qtest() )
		mpw_inqueue(ifp);

	if( x < 0 || x > ifp->if_width ||
	    y < 0 || y > ifp->if_height )
		return(-1);
	ret = 0;
	xpos = x;
	ypos = y;

	while( count > 0 )  {
		ip = &ifp->if_mem[(ypos*1024+xpos)*sizeof(RGBpixel)];

		if ( count >= ifp->if_width-xpos )  {
			scan_count = ifp->if_width-xpos;
		} else	{
			scan_count = count;
		}
		memcpy( *pixelp, ip, scan_count*sizeof(RGBpixel) );

		pixelp += scan_count;
		count -= scan_count;
		ret += scan_count;
		xpos = 0;
		/* Advance upwards */
		if( ++ypos >= ifp->if_height )
			break;
	}
	return(ret);
}

/*
 *			M I P S _ B W R I T E
 */
_LOCAL_ int
mips_bwrite( ifp, xmem, ymem, pixelp, count )
FBIO	*ifp;
int	xmem, ymem;
RGBpixel *pixelp;
int	count;
{
	register short scan_count;	/* # pixels on this scanline */
	int xscr, yscr;
	register int i;
	register unsigned char *cp;
	register unsigned char *op;
	int ret;
	int hfwidth = (ifp->if_width/MIPS(ifp)->mi_xzoom)/2;
	int hfheight = (ifp->if_height/MIPS(ifp)->mi_yzoom)/2;
	im_setup;

	if( qtest() )
		mpw_inqueue(ifp);
	if( xmem < 0 || xmem > ifp->if_width ||
	    ymem < 0 || ymem > ifp->if_height)
		return(-1);
	if( MIPS(ifp)->mi_curs_on )
		cursoff();		/* Cursor interferes with writing */

	ret = 0;
	xscr = (xmem - (MIPS(ifp)->mi_xcenter-hfwidth)) * MIPS(ifp)->mi_xzoom;
	yscr = (ymem - (MIPS(ifp)->mi_ycenter-hfheight)) * MIPS(ifp)->mi_yzoom;
	cp = (unsigned char *)(*pixelp);
	while( count > 0 )  {
		if( yscr >= ifp->if_height )
			break;
		if ( count >= ifp->if_width-xmem )
			scan_count = ifp->if_width-xmem;
		else
			scan_count = count;

		op = (unsigned char *)&ifp->if_mem[(ymem*1024+xmem)*sizeof(RGBpixel)];

		if( ifp->if_zoomflag )  {
			register Scoord l, b, r, t;
			int todraw = (ifp->if_width-xscr)/MIPS(ifp)->mi_xzoom;
			int tocopy;

			if( todraw > scan_count )  todraw = scan_count;
			tocopy = scan_count - todraw;

			l = xscr;
			b = yscr;
			t = b + MIPS(ifp)->mi_yzoom - 1;
			for( i = todraw; i > 0; i-- )  {
				if ( MIPS(ifp)->mi_cmap_flag == FALSE ) {
 					RGBcolor( cp[RED], cp[GRN], cp[BLU]);
				} else {
				    	RGBcolor( CMR(ifp)[cp[RED]], 
						CMG(ifp)[cp[GRN]], 
						CMB(ifp)[cp[BLU]] );
				}

				r = l + MIPS(ifp)->mi_xzoom - 1;

				im_rectfs( l, b, r, t ); 

				l = r + 1;

				*op++ = *cp++;
				*op++ = *cp++;
				*op++ = *cp++;
			}
			for( i = tocopy; i > 0; i-- )  {
				*op++ = *cp++;
				*op++ = *cp++;
				*op++ = *cp++;
			}				
			count -= scan_count;
			ret += scan_count;
			xmem = 0;
			ymem++;
			xscr = (hfwidth-MIPS(ifp)->mi_xcenter) *
				MIPS(ifp)->mi_xzoom;
			yscr += MIPS(ifp)->mi_yzoom;
			continue;
		}
		/* Non-zoomed case */
		cmov2s( xscr, yscr );

		if ( MIPS(ifp)->mi_cmap_flag == FALSE )  
		{
			long amount, n;
			n = scan_count;

			while( n > 0 )
			{
				amount = n > 30 ? 30 : n;

				im_passthru( amount + amount + amount + 2);
				im_outshort( FBCRGBdrawpixels);
				im_outshort( amount );
				
				n -= amount;
				while( --amount != -1 )
				{
					im_outshort(  *op++ = *cp++ );
					im_outshort(  *op++ = *cp++ );
					im_outshort(  *op++ = *cp++ );
				}
				amount = amount;
			}
			im_freepipe;
			GEWAIT;
		} else {
			long amount, n;
			n = scan_count;

			while( n > 0 )
			{
				amount = n > 30 ? 30 : n;

				im_passthru( amount + amount + amount + 2);
				im_outshort( FBCRGBdrawpixels);
				im_outshort( amount );
				
				n -= amount;
				while( --amount != -1 )
				{
					im_outshort(CMR(ifp)[*op++ = *cp++] );
					im_outshort(CMR(ifp)[*op++ = *cp++] );
					im_outshort(CMR(ifp)[*op++ = *cp++] );
				}
				amount = amount;
			}
			im_freepipe;
			GEWAIT;
		}

		count -= scan_count;
		ret += scan_count;
		xmem = 0;
		ymem++;
		xscr = MIPS(ifp)->mi_xcenter - hfwidth;
		yscr++;
	}

	if( MIPS(ifp)->mi_curs_on )
		curson();		/* Cursor interferes with writing */
	return(ret);
}

/*
 *			M I P S _ V I E W P O R T _ S E T
 */
_LOCAL_ int
mips_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	if( qtest() )
		mpw_inqueue(ifp);
#if 0
	viewport(	(Screencoord) left,
			(Screencoord) right,
			(Screencoord) top,
			(Screencoord) (bottom * fb2iris_scale)
			);
#endif
	return(0);
}

/*
 *			M I P S _ C M R E A D
 */
_LOCAL_ int
mips_cmread( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if( qtest() )
		mpw_inqueue(ifp);

	/* Just parrot back the stored colormap */
	for( i = 0; i < 256; i++)
	{
		cmp->cm_red[i] = CMR(ifp)[i]<<8;
		cmp->cm_green[i] = CMG(ifp)[i]<<8;
		cmp->cm_blue[i] = CMB(ifp)[i]<<8;
	}
	return(0);
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
{
	register int i;

	for( i=0; i<256; i++ )  {
		if( CMR(ifp)[i] != i )  return(0);
		if( CMG(ifp)[i] != i )  return(0);
		if( CMB(ifp)[i] != i )  return(0);
	}
	return(1);
}

/*
 *			 M I P S _ C M W R I T E
 */
_LOCAL_ int
mips_cmwrite( ifp, cmp )
register FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if( qtest() )
		mpw_inqueue(ifp);

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 256; i++)  {
			CMR(ifp)[i] = i;
			CMG(ifp)[i] = i;
			CMB(ifp)[i] = i;
		}
		if( MIPS(ifp)->mi_cmap_flag ) {
			MIPS(ifp)->mi_cmap_flag = FALSE;
			mips_repaint( ifp );
		}
		MIPS(ifp)->mi_cmap_flag = FALSE;
		return(0);
	}
	
	for(i = 0; i < 256; i++)  {
		CMR(ifp)[i] = cmp-> cm_red[i]>>8;
		CMG(ifp)[i] = cmp-> cm_green[i]>>8; 
		CMB(ifp)[i] = cmp-> cm_blue[i]>>8;

	}
	MIPS(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);
	mips_repaint( ifp );
	return(0);
}

/*
 *			M I P S _ C U R S _ S E T
 */
_LOCAL_ int
mips_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
{
	register int	y;
	register int	xbytes;
	Cursor		newcursor;

	if( qtest() )
		mpw_inqueue(ifp);

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
	for( y = 0; y < ybits; y++ )  {
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
	}
	defcursor( 1, newcursor );
	curorigin( 1, (short) xorig, (short) yorig );
	setcursor(1,0,0);
	return	0;
}

/*
 *			M I P S _ C M E M O R Y _ A D D R
 */
_LOCAL_ int
mips_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	short	xmin, ymin;
	register short	i;
	short	xwidth;
	long left, bottom, x_size, y_size;

	if( qtest() )
		mpw_inqueue(ifp);

	MIPS(ifp)->mi_curs_on = mode;
	if( ! mode )  {
		cursoff();
		return	0;
	}
	xwidth = ifp->if_width/MIPS(ifp)->mi_xzoom;
	i = xwidth/2;
	xmin = MIPS(ifp)->mi_xcenter - i;
	i = (ifp->if_height/2)/MIPS(ifp)->mi_yzoom;
	ymin = MIPS(ifp)->mi_ycenter - i;
	x -= xmin;
	y -= ymin;
	x *= MIPS(ifp)->mi_xzoom;
	y *= MIPS(ifp)->mi_yzoom;
	curson();
	getsize(&x_size, &y_size);
	getorigin( &left, &bottom );

/*	RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF ); */
	setvaluator( MOUSEX, x+left, 0, 1023 );
	setvaluator( MOUSEY, y+bottom, 0, 1023 );

	return	0;
}


/*
 *			S G W _ I N Q U E U E
 *
 *  Called when a qtest() indicates that there is a window event.
 *  Process all events, so that we don't loop on recursion to sgw_bwrite.
 */
_LOCAL_ void
mpw_inqueue(ifp)
register FBIO *ifp;
{
	short val;
	int redraw = 0;
	register int ev;

	while( qtest() )  {
		switch( ev = qread(&val) )  {
		case REDRAW:
			redraw = 1;
			break;
		case INPUTCHANGE:
			break;
		case MODECHANGE:
			/* This could be bad news.  Should we re-write
			 * the color map? */
			fb_log("mpw_inqueue:  modechange?\n");
			break;
		case MOUSEX :
		case MOUSEY :
		case KEYBD :
			break;
		default:
			fb_log("mpw_inqueue:  event %d unknown\n", ev);
			break;
		}
	}
	/*
	 * Now that all the events have been removed from the input
	 * queue, handle any actions that need to be done.
	 */
	if( redraw )  {
		mips_repaint( ifp );
		redraw = 0;
	}
}

_LOCAL_ void
mips_rectf_pix( ifp, l, b, r, t, pixelp )
FBIO		*ifp;
Scoord		l, b, r, t;
RGBpixel	*pixelp;
{
	im_setup;

	RGBcolor( (*pixelp)[RED], (*pixelp)[GRN], (*pixelp)[BLU] );

	im_rectfs( l, b, r, t ); 

	return;
}
