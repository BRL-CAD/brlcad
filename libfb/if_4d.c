#undef _LOCAL_
#define _LOCAL_ /**/
/*
 *			I F _ G T . C
 *
 *  BRL Frame Buffer Library interface for SGI Iris-4D with Graphics Turbo.
 *  Support for the 3030/2400 series ("Iris-3D") is in if_sgi.c
 *  Support for the 4D's  without the Graphics turbos is in if_4d.c
 *  However, all are called /dev/sgi
 *
 *  In order to use a large chunck of memory with the shared memory 
 *  system it is necessary to increase the shmmax and shmall paramaters
 *  of the system. You can do this by changing the defaults in the
 *  /usr/sysgen/master.d/shm to
 *
 * 	#define SHMMAX	5131072
 *	#define SHMALL	4000
 *
 *  refer to the Users Manuals to reconfigure your kernel..
 *
 *  There are 4 different Frame Buffer types supported on the 4D/60T or 4D/70
 *  set your environment FB_FILE to the appropriate Frame buffer type
 *	/dev/sgi0 	Transient window private memory
 *	/dev/sgi1	Transient window shared memory ( default )
 *	/dev/sgi2	Lingering window private memory
 *	/dev/sgi3	Lingering window shared memory
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
#include <gl/addrs.h>
#include <gl/cg2vme.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <psio.h>
#undef RED
#include "fb.h"
#include "./fblocal.h"

#define BZERO(p,cnt)	memset(p,'\0',cnt)

extern char *sbrk();
extern char *malloc();
extern int errno;
extern char *shmat();
extern int brk();

static Cursor	nilcursor;	/* to make it go away -- all bits off */
static Cursor	cursor =
	{
#include "./sgicursor.h"
	};

_LOCAL_ int	gt_dopen(),
		gt_dclose(),
		gt_dclear(),
		gt_bread(),
		gt_bwrite(),
		gt_cmread(),
		gt_cmwrite(),
		gt_viewport_set(),
		gt_window_set(),
		gt_zoom_set(),
		gt_curs_set(),
		gt_cmemory_addr(),
		gt_cscreen_addr(),
		gt_help();

/* This is the ONLY thing that we "export" */
FBIO gt_interface =
		{
		gt_dopen,
		gt_dclose,
		fb_null,		/* reset? */
		gt_dclear,
		gt_bread,
		gt_bwrite,
		gt_cmread,
		gt_cmwrite,
		gt_viewport_set,
		gt_window_set,
		gt_zoom_set,
		gt_curs_set,
		gt_cmemory_addr,
		fb_null,		/* cscreen_addr */
		gt_help,
		"SGI 4D/GT",
		XMAXSCREEN+1,		/* max width */
		YMAXSCREEN+1,		/* max height */
		"/dev/sgi",
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


_LOCAL_ Colorindex get_Color_Index();
_LOCAL_ void	mpw_inqueue();
static int	is_linear_cmap();

struct gt_cmap {
	unsigned char	cmr[256];
	unsigned char	cmg[256];
	unsigned char	cmb[256];
};


/*
 *  Per MIPS (window or device) state information
 *  Too much for just the if_u[1-6] area now.
 */

struct gtinfo {
	short	mi_curs_on;
	short	mi_xzoom;
	short	mi_yzoom;
	short	mi_xcenter;
	short	mi_ycenter;
	int	mi_rgb_ct;
	short	mi_cmap_flag;
	int	mi_shmid;
	int	mi_memwidth;		/* width of scanline in if_mem */
	long	mi_der1;		/* Saved DE_R1 */
};
#define	GT(ptr)	((struct gtinfo *)((ptr)->u1.p))
#define	GTL(ptr)	((ptr)->u1.p)		/* left hand side version */
#define if_mem		u2.p			/* shared memory pointer */
#define if_cmap		u3.p			/* color map in shared memory */
#define CMR(x)		((struct gt_cmap *)((x)->if_cmap))->cmr
#define CMG(x)		((struct gt_cmap *)((x)->if_cmap))->cmg
#define CMB(x)		((struct gt_cmap *)((x)->if_cmap))->cmb
#define if_zoomflag	u4.l			/* zoom > 1 */
#define if_mode		u5.l			/* see MODE_* defines */

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(ifp->if_max_width - ifp->if_width-MARGIN)
#define WIN_R	(ifp->if_max_width - 1 - MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height - 1 + MARGIN)

static int map_size;			/* # of color map slots available */

/*
 *  The mode has several independent bits:
 *	SHARED -vs- MALLOC'ed memory for the image
 *	TRANSIENT -vs- LINGERING windows
 *	Windowed vs FullScreen
 *	Default Hz -vs- 30hz monitor mode
 *	Genlock NTSC -vs- normal monitor mode
 */
#define MODE_1MASK	(1<<0)
#define MODE_1SHARED	(0<<0)		/* Use Shared memory */
#define MODE_1MALLOC	(1<<0)		/* Use malloc memory */

#define MODE_2MASK	(1<<1)
#define MODE_2TRANSIENT	(0<<1)
#define MODE_2LINGERING (1<<1)

#define MODE_3MASK	(1<<2)
#define MODE_3WINDOW	(0<<2)
#define MODE_3FULLSCR	(1<<2)

#define MODE_4MASK	(1<<3)
#define MODE_4HZDEF	(0<<3)
#define MODE_4HZ30	(1<<3)

#define MODE_5MASK	(1<<4)
#define MODE_5NORMAL	(0<<4)
#define MODE_5GENLOCK	(1<<4)

#define MODE_6MASK	(1<<5)
#define MODE_6NORMAL	(0<<5)
#define MODE_6EXTSYNC	(1<<5)

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
	{ 'f',	MODE_3MASK, MODE_3FULLSCR, 
		"Full centered screen - else windowed" },
	{ 't',	MODE_4MASK, MODE_4HZ30,
		"Thirty Hz (e.g. Dunn) - else 60 Hz" },
	{ 'n',	MODE_5MASK, MODE_5GENLOCK,
		"NTSC+GENLOCK - else normal video" },
	{ 'e',	MODE_6MASK, MODE_6EXTSYNC,
		"External sync - else internal" },
	{ 'z',	MODE_15MASK, MODE_15ZAP,
		"Zap (free) shared memory" },
	{ '\0', 0, 0, "" }
};

static RGBpixel	rgb_table[4096];

struct gt_pix {
	unsigned char alpha;	/* this will always be zero */
	unsigned char blue;
	unsigned char green;
	unsigned char red;
};

struct gt_pix gt_scan[1280];	/* Maximum length of a scan line */

static int fb_parent;

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
gt_getmem( ifp )
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

	if( (ifp->if_mode & MODE_1MASK) == MODE_1MALLOC )  {
		/*
		 *  In this mode, only malloc as much memory as is needed.
		 */
		GT(ifp)->mi_memwidth = ifp->if_width;
		pixsize = ifp->if_height * ifp->if_width * sizeof(RGBpixel);
		size = pixsize + sizeof(struct gt_cmap);

		sp = malloc( size );
		if( sp == 0 )  {
			fb_log("gt_getmem: frame buffer memory malloc failed\n");
			goto fail;
		}

		ifp->if_mem = sp;
		ifp->if_cmap = sp + pixsize;	/* cmap at end of area */

		gt_cmwrite( ifp, COLORMAP_NULL );
		return(0);
	}

	/* The shared memory section never changes size */
	GT(ifp)->mi_memwidth = ifp->if_max_width;
	pixsize = ifp->if_max_height * ifp->if_max_width * sizeof(RGBpixel);
	size = pixsize + sizeof(struct gt_cmap);
	size = (size + 4096-1) & ~(4096-1);

	/* First try to attach to an existing one */
	if( (GT(ifp)->mi_shmid = shmget( SHMEM_KEY, size, 0 )) < 0 )  {
		/* No existing one, create a new one */
		if( (GT(ifp)->mi_shmid = shmget(
		    SHMEM_KEY, size, IPC_CREAT|0666 )) < 0 )  {
			fb_log("gt_getmem: shmget failed, errno=%d\n", errno);
			goto fail;
		}
		new = 1;
	}

	/* Move up the existing break, to leave room for later malloc()s */
	old_brk = sbrk(0);
	new_brk = (char *)(6 * (XMAXSCREEN+1) * 1024);
	if( new_brk <= old_brk )
		new_brk = old_brk + (XMAXSCREEN+1) * 1024;
	new_brk = (char *)((((int)new_brk) + 4096-1) & ~(4096-1));
	if( brk( new_brk ) < 0 )  {
		fb_log("gt_getmem: new brk(x%x) failure, errno=%d\n", new_brk, errno);
		goto fail;
	}

	/* Open the segment Read/Write, near the current break */
	if( (sp = shmat( GT(ifp)->mi_shmid, 0, 0 )) == (char *)(-1) )  {
		fb_log("gt_getmem: shmat returned x%x, errno=%d\n", sp, errno );
		goto fail;
	}

	/* Restore the old break */
	if( brk( old_brk ) < 0 )  {
		fb_log("gt_getmem: restore brk(x%x) failure, errno=%d\n", old_brk, errno);
		/* Take the memory and run */
	}

	ifp->if_mem = sp;
	ifp->if_cmap = sp + pixsize;	/* cmap at end of area */

	/* Provide non-black colormap on creation of new shared mem */
	if(new)
		gt_cmwrite( ifp, COLORMAP_NULL );
	return(0);
fail:
	fb_log("gt_getmem:  Unable to attach to shared memory.\nConsult comment in cad/libfb/if_4d.c for details\n");
	if( (sp = malloc( size )) == NULL )  {
		fb_log("gt_getmem:  malloc failure\n");
		return(-1);
	}
	ifp->if_mem = sp;
	return(0);
}

/*
 *			M I P S _ Z A P M E M
 */
void
gt_zapmem()
{
	int shmid;
	int i;

	if( (shmid = shmget( SHMEM_KEY, 0, 0 )) < 0 )  {
		fb_log("gt_zapmem shmget failed, errno=%d\n", errno);
		return;
	}

	i = shmctl( shmid, IPC_RMID, 0 );
	if( i < 0 )  {
		fb_log("gt_zapmem shmctl failed, errno=%d\n", errno);
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
gt_repaint( ifp )
register FBIO	*ifp;
{
	register short xmin, xmax;
	register short ymin, ymax;
	register short i;
	short xscroff, yscroff;
	register unsigned char *ip;
	short y;
	short xwidth;


	readsource(SRC_ZBUFFER);	/* source for rectcopy() */
	backbuffer(TRUE);		/* dest for rectcopy() */
	frontbuffer(FALSE);

	xscroff = yscroff = 0;

	i = (ifp->if_width/2)/GT(ifp)->mi_yzoom;

	xmin = GT(ifp)->mi_xcenter - i;
	xmax = GT(ifp)->mi_xcenter + i - 1;

	i = (ifp->if_height/2)/GT(ifp)->mi_yzoom;

	ymin = GT(ifp)->mi_ycenter - i;
	ymax = GT(ifp)->mi_ycenter + i - 1;

	if( xmin < 0 )  {
		xscroff = -xmin * GT(ifp)->mi_xzoom;
		xmin = 0;
	}
	if( ymin < 0 )  {
		yscroff = -ymin * GT(ifp)->mi_yzoom;
		ymin = 0;
	}

	if( xmax > ifp->if_width-1 )  {
		xmax = ifp->if_width-1;
	}

	if( ymax > ifp->if_height-1 )  {
		ymax = ifp->if_height-1;
	}

	if( ifp->if_zoomflag )  {
		rectzoom( (double) GT(ifp)->mi_xzoom, 
			  (double) GT(ifp)->mi_yzoom);
	} 

	cpack(0x00000000);	/* clear to black first */
	clear();

	rectcopy( xmin, ymin, xmax, ymax, xscroff, yscroff);

 	swapbuffers();	 

	if( ifp->if_zoomflag ) 	/* Must set this back to 1.0 for zbuffer */
		rectzoom(1.0, 1.0);
}

/*
 *			S I G K I D
 */
static int sigkid()
{
	exit(0);
}

/*
 *			M I P S _ D O P E N
 */
_LOCAL_ int
gt_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	int f;
	int	status;
	int 	g_status;
	static char	title[128];
	int	mode;

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/sgi###"
	 *  The default mode is zero.
	 */

	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char modebuf[80];
		char *mp;
		int alpha;
		struct modeflags *mfp;

		
		if( strncmp(file, "/dev/sgi", 8) ) {
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
					fb_log( "if_4d: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
		}

		if( (mode & MODE_15MASK) == MODE_15ZAP ) {
			/* Only task: Attempt to release shared memory segment */
			gt_zapmem();
			return(-1);
		}

		/* Pick off just the mode bits of interest here */
		mode &= (MODE_1MASK | MODE_2MASK | MODE_3MASK | MODE_4MASK | 
			MODE_5MASK | MODE_6MASK);
	}
	ifp->if_mode = mode;

	/*
	 *  Now that the mode has been determined,
	 *  ensure that the graphics system is running.
	 */
	if( !(g_status = ps_open_PostScript()) )  {
		char * grcond = "/etc/gl/grcond";
		char * newshome = "/usr/brlcad/etc";		/* XXX */

		f = fork();
		if( f < 0 )  {
			perror("fork");
			return(-1);		/* error */
		}
		if( f == 0 )  {
			/* Child */
			chdir( newshome );
			execl( grcond, (char *) 0 );
			perror( grcond );
			_exit(1);
			/* NOTREACHED */
		}
		/* Parent */
		while( !(g_status = ps_open_PostScript()) )  {
			sleep(1);
		}
	}

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

	if( (ifp->if_mode & MODE_2MASK) == MODE_2LINGERING )  {

		fb_parent = getpid();		/* save parent pid */

		signal( SIGUSR1, sigkid);

		if(((f = fork()) != 0 ) && ( f != -1))   {
			int k;
			for(k=0; k< 20; k++)  {
				(void) close(k);
			}

			/*
			 *  Wait until the child dies, of whatever cause,
			 *  or until the child kills us.
			 *  Pretty vicious, this computer society.
			 */
			while( (k = wait(&status)) != -1 && k != f )
				/* NULL */ ;

			exit(0);
		}
	}

	if( (GTL(ifp) = (char *)calloc( 1, sizeof(struct gtinfo) )) == NULL )  {
		fb_log("gt_dopen:  gtinfo malloc failed\n");
		return(-1);
	}

	if( (ifp->if_mode & MODE_5MASK) == MODE_5GENLOCK )  {
		/* NTSC, see below */
		ifp->if_width = ifp->if_max_width = XMAX170+1;	/* 646 */
		ifp->if_height = ifp->if_max_height = YMAX170+1; /* 485 */
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width )
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height)
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

	blanktime(0);

	if( (ifp->if_mode & MODE_5MASK) == MODE_5GENLOCK )  {
		prefposition( 0, XMAX170, 0, YMAX170 );
		GT(ifp)->mi_curs_on = 0;	/* cursoff() happens below */
	} else if( (ifp->if_mode & MODE_3MASK) == MODE_3WINDOW )  {
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		GT(ifp)->mi_curs_on = 1;	/* Mex usually has it on */
	}  else  {
		prefposition( 0, XMAXSCREEN, 0, YMAXSCREEN );
		GT(ifp)->mi_curs_on = 0;	/* cursoff() happens below */
	}

	foreground();		/* Direct focus here, don't detach */
	if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
	{
		fb_log( "No more graphics ports available.\n" );
		return	-1;
	}

	/*  Establish operating mode (Hz, GENLOCK).
	 *  The assumption is that the device is always in the
	 *  "normal" mode to start with.  The mode will only
	 *  be saved and restored when 30Hz operation is specified;
	 *  on GENLOCK operation, valid NTSC sync pulses must be present
	 *  for downstream equipment;  user should run "Set60" when done.
	 */
	if( (ifp->if_mode & MODE_4MASK) == MODE_4HZ30 )  {
		GT(ifp)->mi_der1 = getvideo(DE_R1);
		setvideo( DE_R1, DER1_30HZ|DER1_UNBLANK);	/* 4-wire RS-343 */
	} else if( (ifp->if_mode & MODE_5MASK) == MODE_5GENLOCK )  {
		GT(ifp)->mi_der1 = getvideo(DE_R1);
		if( (GT(ifp)->mi_der1 & DER1_VMASK) == DER1_170 )  {
			/* 
			 *  Current mode is DE3 board internal NTSC sync.
			 *  Doing a setmonitor(NTSC) again will cause the
			 *  sync generator to drop out for a moment.
			 *  So, in this case, do nothing.
			 */
		} else if( getvideo(CG_MODE) != -1 )  {
		    	/*
			 *  Optional CG2 GENLOCK boark is installed.
			 *
			 *  Mode 2:  Internal sync generator is used.
			 *
			 *  Note that the stability of the sync generator
			 *  on the GENLOCK board is *worse* than the sync
			 *  generator on the regular DE3 board.  The GENLOCK
			 *  version "twitches" every second or so.
			 *
			 *  Mode 3:  Output is locked to incoming
			 *  NTSC composite video picture
		    	 *  for sync and chroma (on "REM IN" connector).
		    	 *  Color subcarrier is phase and amplitude locked to
		    	 *  incomming color burst.
		    	 *  The blue LSB has no effect on video overlay.
			 *
			 *  Note that the generated composite NTSC output
			 *  (on "VID OUT" connector) is often a problem,
			 *  since it has a DC offset of +0.3V to the base
			 *  of the sync pulse, while other studio eqiupment
			 *  often expects the blanking level to be at
			 *  exactly 0.0V, with sync at -0.3V.
			 *  In this case, the black leves are ruined.
			 *  Also, the inboard encoder chip isn't very good.
			 *  Therefore, it is necessary to use an outboard
			 *  RS-170 to NTSC encoder to get useful results.
		    	 */
			if( (ifp->if_mode & MODE_6MASK) == MODE_6EXTSYNC )  {
				/* external sync via GENLOCK board REM IN */
			    	setvideo(CG_MODE, CG2_M_MODE3);
			    	setvideo(DE_R1, DER1_G_170|DER1_UNBLANK );
			} else {
				/* internal sync */
#ifdef GENLOCK_SYNC
				/* GENLOCK sync, found to be highly unstable */
			    	setvideo(CG_MODE, CG2_M_MODE2);
			    	setvideo(DE_R1, DER1_G_170|DER1_UNBLANK );
#else
				/* Just use DE3 sync generator.
				 * For this case, GENLOCK board does nothing!
				 * Equiv to setmonitor(NTSC);
				 */
				setvideo(DE_R1, DER1_170|DER1_UNBLANK);
#endif
			}
		} else {
			/*
			 *  No genlock board is installed, produce RS-170
			 *  video at NTSC rates with separate sync,
			 *  and hope that they have an outboard NTSC
			 *  encoder device.  Equiv to setmonitor(NTSC);
			 */
			setvideo(DE_R1, DER1_170|DER1_UNBLANK);
		}
	}

	/* Build a descriptive window title bar */
	(void)sprintf( title, "BRL libfb /dev/sgi%d%s %s, %s",
		ifp->if_mode,
		((ifp->if_mode & MODE_4MASK) == MODE_4HZ30) ?
			" 30Hz" :
			"",
		((ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT) ?
			"Transient Win" :
			"Lingering Win",
		((ifp->if_mode & MODE_1MASK) == MODE_1MALLOC) ?
			"Private Mem" :
			"Shared Mem" );

	wintitle( title );
	
	/* Free window of position constraint.		*/
	prefsize( (long)ifp->if_width, (long)ifp->if_height );
	winconstraints();

	RGBmode();
	doublebuffer();
	gconfig();	/* Must be called after doublebuffer().	*/
			/* Need to clean out images from windows below */
			/* This hack is necessary until windows persist from
			 * process to process */

	/* In full screen mode, center the image on the
	 * usable part of the screen, either high-res, or NTSC
	 */
	if( (ifp->if_mode & MODE_3MASK) == MODE_3FULLSCR )  {
		int	xleft, ybot;
		xleft = (ifp->if_max_width)/2 - ifp->if_width/2;
		ybot = (ifp->if_max_height)/2 - ifp->if_height/2;
		viewport( xleft, xleft + ifp->if_width,
			  ybot, ybot + ifp->if_height );
		ortho2( (Coord)0, (Coord)ifp->if_width,
			(Coord)0, (Coord)ifp->if_height );
		/* set input focus to current window, so that
		 * we can manipulate the cursor icon */
		winattach();
	}

	/* Must initialize these window state variables BEFORE calling
		"gt_getmem", because this function can indirectly trigger
		a call to "gt_repaint" (when initializing shared memory
		after a reboot).					*/
	ifp->if_zoomflag = 0;
	GT(ifp)->mi_xzoom = 1;	/* for zoom fakeout */
	GT(ifp)->mi_yzoom = 1;	/* for zoom fakeout */
	GT(ifp)->mi_xcenter = width/2;
	GT(ifp)->mi_ycenter = height/2;

	if( gt_getmem(ifp) < 0 )
		return(-1);

	/* Must call "is_linear_cmap" AFTER "gt_getmem" which allocates
		space for the color map.				*/
	GT(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);

	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	defcursor( 2, nilcursor );
	curorigin( 1, 0, 0 );
	drawmode( CURSORDRAW );
	mapcolor( 1, 255, 0, 0 );
	drawmode( NORMALDRAW );
	setcursor(1, 1, 0);

	if( GT(ifp)->mi_curs_on == 0 )  {
		setcursor( 2, 1, 0 );		/* nilcursor */
		cursoff();
	}

	/* The screen has no useful state.  Restore it as it was before */
	/* Smarter deferral logic needed */
	if( (ifp->if_mode & MODE_1MASK) == MODE_1SHARED )
	{
		gt_zbuffer(ifp);	/* Write into the z buffer first */
		gt_repaint( ifp );	/* repaint the screen */
	}

	readsource(SRC_ZBUFFER);
	backbuffer(TRUE);
	frontbuffer(FALSE);

	return	0;
}

/*
 *			M I P S _ D C L O S E
 *
 */
_LOCAL_ int
gt_dclose( ifp )
FBIO	*ifp;
{
	int menu, menuval, val, dev, f;
	int k;
	FILE *fp = NULL;

	if( (ifp->if_mode & MODE_2MASK) == MODE_2TRANSIENT )
		goto out;

	/*
	 *  LINGER mode.  Don't return to caller until user mouses "close"
	 *  menu item.  This may delay final processing in the calling
	 *  function for some time, but the assumption is that the user
	 *  wishes to compare this image with others.
	 *
	 *  Since we plan to linger here, long after our invoker
	 *  expected us to be gone, be certain that no file descriptors
	 *  remain open to associate us with pipelines, network
	 *  connections, etc., that were ALREADY ESTABLISHED before
	 *  the point that fb_open() was called.
	 *
	 *  The simple for i=0..20 loop will not work, because that
	 *  smashes some window-manager files.  Therefore, we content
	 *  ourselves with eliminating stdin, stdout, and stderr,
	 *  (fd 0,1,2), in the hopes that this will successfully
	 *  terminate any pipes or network connections.  In the case
	 *  of calls from rfbd, in normal (non -d) mode, it gets the
	 *  network connection on stdin/stdout, so this is adequate.
	 */

	fclose( stdin );
	fclose( stdout );
	fclose( stderr );

	/* Line up at the "complaints window", just in case... */
	fp = fopen("/dev/console", "w");

	kill(fb_parent, SIGUSR1);	/* zap the lurking parent */

	menu = defpup("close");
	qdevice(RIGHTMOUSE);
	qdevice(REDRAW);
	
	while(1)  {
		val = 0;
		dev = qread( &val );
		switch( dev )  {

		case RIGHTMOUSE:
			menuval = dopup( menu );
			if (menuval == 1 )
				goto out;
			break;

		case REDRAW:
			reshapeviewport();
			gt_zbuffer(ifp);
			gt_repaint(ifp);
			break;

		case INPUTCHANGE:
		case CURSORX:
		case CURSORY:
			/* We don't need to do anything about these */
			break;

		case QREADERROR:
			/* These are fatal errors, bail out */
			if( fp ) fprintf(fp,"libfb/gt_dclose: qreaderror, aborting\n");
			goto out;

		default:
			/*
			 *  There is a tendency for infinite loops
			 *  here.  With only a few qdevice() attachments
			 *  done above, there shouldn't be too many
			 *  unexpected things.  But, lots show up.
			 *  At least this gives visibility.
			 */
			if( fp ) fprintf(fp,"libfb/gt_dclose: qread %d, val %d\r\n", dev, val );
			break;
		}
	}
out:
	/*
	 *  User is finally done with the frame buffer,
	 *  return control to our caller (who may have more to do).
	 *  set a 20 minute screensave blanking when fb is closed.
	 *  We have no way of knowing if there are other libfb windows
	 *  still open.
	 */
#if 0
	blanktime( (long) 67 * 60 * 20L );
#else
	/*
	 *  Set an 8 minute screensaver blanking, which will light up
	 *  the screen again if it was dark, and will protect it otherwise.
	 *  The 4D has a hardware botch limiting the time to 2**15 frames.
	 */
	blanktime( (long) 32767L );
#endif

	/* Restore initial operation mode, if this was 30Hz */
	if( (ifp->if_mode & MODE_4MASK) == MODE_4HZ30 )
		setvideo( DE_R1, GT(ifp)->mi_der1 );

	/* Always leave cursor on when done */
	if( GT(ifp)->mi_curs_on == 0 )  {
		setcursor( 0, 1, 0 );		/* system default cursor */
		curson();
	}

	gexit();			/* mandatory finish */
	if( fp )  fclose(fp);

	if( GTL(ifp) != NULL )
		(void)free( (char *)GT(ifp) );
	return(0);
}

/*
 *			M I P S _ D C L E A R
 */
_LOCAL_ int
gt_dclear( ifp, pp )
FBIO	*ifp;
register RGBpixel	*pp;
{

	if( qtest() )
		mpw_inqueue(ifp);

	if ( pp != RGBPIXEL_NULL)  {
		register char *op = ifp->if_mem;
		register int cnt;

		/* Slightly simplistic -- runover to right border */
		for( cnt=GT(ifp)->mi_memwidth*ifp->if_height-1; cnt > 0; cnt-- )  {
			*op++ = (*pp)[RED];
			*op++ = (*pp)[GRN];
			*op++ = (*pp)[BLU];
		}

		RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
	} else {
		RGBcolor( (short) 0, (short) 0, (short) 0);
		BZERO( ifp->if_mem, GT(ifp)->mi_memwidth*ifp->if_height*sizeof(RGBpixel) );
	}
	gt_zbuffer(ifp);
	gt_repaint(ifp);
	clear();
	return(0);
}

/*
 *			M I P S _ W I N D O W _ S E T
 */
_LOCAL_ int
gt_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	if( qtest() )
		mpw_inqueue(ifp);

	if( GT(ifp)->mi_xcenter == x && GT(ifp)->mi_ycenter == y )
		return(0);
	if( x < 0 || x >= ifp->if_width )
		return(-1);
	if( y < 0 || y >= ifp->if_height )
		return(-1);
	GT(ifp)->mi_xcenter = x;
	GT(ifp)->mi_ycenter = y;
	gt_repaint( ifp );
	return(0);
}

/*
 *			M I P S _ Z O O M _ S E T
 */
_LOCAL_ int
gt_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	if( qtest() )
		mpw_inqueue(ifp);

	if( x < 1 ) x = 1;
	if( y < 1 ) y = 1;
	if( GT(ifp)->mi_xzoom == x && GT(ifp)->mi_yzoom == y )
		return(0);
	if( x >= ifp->if_width || y >= ifp->if_height )
		return(-1);

	GT(ifp)->mi_xzoom = x;
	GT(ifp)->mi_yzoom = y;

	if( GT(ifp)->mi_xzoom > 1 || GT(ifp)->mi_yzoom > 1 )
		ifp->if_zoomflag = 1;
	else	ifp->if_zoomflag = 0;

	gt_repaint( ifp );
	return(0);
}

/*
 *			M I P S _ B R E A D
 */
_LOCAL_ int
gt_bread( ifp, x, y, pixelp, count )
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
		ip = &ifp->if_mem[(ypos*GT(ifp)->mi_memwidth+xpos)*sizeof(RGBpixel)];

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
gt_bwrite( ifp, xmem, ymem, pixelp, count )
register FBIO	*ifp;
register int	xmem, ymem;
RGBpixel *pixelp;
register int	count;
{
	register short scan_count;	/* # pixels on this scanline */
	register unsigned char *cp;
	int xscr, yscr;
	int ret;

	if( qtest() )
		mpw_inqueue(ifp);

	if( xmem < 0 || xmem > ifp->if_width ||
	    ymem < 0 || ymem > ifp->if_height)
		return(-1);

	ret = 0;

	cp = (unsigned char *)(pixelp);

	zdraw(TRUE);


	if ( !ifp->if_zoomflag )
	{
		backbuffer(TRUE);
		frontbuffer(TRUE);
	} else
	{
		backbuffer(FALSE);
		frontbuffer(FALSE);
	}

	while( count )  
	{
		register unsigned int n;

		if( ymem >= ifp->if_height )
			break;

		if ( count >= ifp->if_width-xmem )
			scan_count = ifp->if_width-xmem;
		else
			scan_count = count;

		/* Move original pixels to shared memory buffer */
		memcpy( (char *)&ifp->if_mem[
		    (ymem*GT(ifp)->mi_memwidth+xmem)*sizeof(RGBpixel)],
		    cp, scan_count*sizeof(RGBpixel) );

		if ( GT(ifp)->mi_cmap_flag == FALSE )  {
			register struct gt_pix * ip = &gt_scan[0];

			n = scan_count;
			if( (n & 3) != 0 )  {
				/* This code uses 60% of all CPU time */
				while( n )  {
					/* alpha channel is always zero */
					ip->red   = cp[RED];
					ip->green = cp[GRN];
					ip->blue  = cp[BLU];
					ip++;
					cp += 3;
					n--;
				}
			} else {
				while( n )  {
					/* alpha channel is always zero */
					ip[0].red   = cp[RED+0*3];
					ip[0].green = cp[GRN+0*3];
					ip[0].blue  = cp[BLU+0*3];
					ip[1].red   = cp[RED+1*3];
					ip[1].green = cp[GRN+1*3];
					ip[1].blue  = cp[BLU+1*3];
					ip[2].red   = cp[RED+2*3];
					ip[2].green = cp[GRN+2*3];
					ip[2].blue  = cp[BLU+2*3];
					ip[3].red   = cp[RED+3*3];
					ip[3].green = cp[GRN+3*3];
					ip[3].blue  = cp[BLU+3*3];
					ip += 4;
					cp += 3*4;
					n -= 4;
				}
			}
		} else {
			register struct gt_pix * ip = &gt_scan[0];

			n = scan_count;
			while( n )  {
				/* alpha channel is always zero */
				ip->red = CMR(ifp)[cp[0]];
				ip->green = CMG(ifp)[cp[1]];
				ip->blue = CMB(ifp)[cp[2]];
				cp += 3;
				ip++;
				n--;
			}
		}

		lrectwrite(xmem,ymem, xmem+scan_count, ymem, gt_scan);
		ret += scan_count;
		count -= scan_count;
		xmem = 0;
		ymem++;
	}

	zdraw(FALSE);
	frontbuffer(FALSE);
	backbuffer(TRUE);

	if ( ifp->if_zoomflag )
	{
		gt_repaint( ifp );
	}

	return(ret);
}

/*
 *			M I P S _ V I E W P O R T _ S E T
 */
_LOCAL_ int
gt_viewport_set( ifp, left, top, right, bottom )
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
gt_cmread( ifp, cmp )
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
gt_cmwrite( ifp, cmp )
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
		if( GT(ifp)->mi_cmap_flag ) {
			GT(ifp)->mi_cmap_flag = FALSE;
			gt_repaint( ifp );
		}
		GT(ifp)->mi_cmap_flag = FALSE;
		return(0);
	}
	
	for(i = 0; i < 256; i++)  {
		CMR(ifp)[i] = cmp-> cm_red[i]>>8;
		CMG(ifp)[i] = cmp-> cm_green[i]>>8; 
		CMB(ifp)[i] = cmp-> cm_blue[i]>>8;

	}
	GT(ifp)->mi_cmap_flag = !is_linear_cmap(ifp);

	gt_zbuffer(ifp);

	gt_repaint( ifp );

	return(0);
}

/*
 *			M I P S _ C U R S _ S E T
 */
_LOCAL_ int
gt_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
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
	setcursor( 1, 0, 0 );
	return	0;
}

/*
 *			M I P S _ C M E M O R Y _ A D D R
 */
_LOCAL_ int
gt_cmemory_addr( ifp, mode, x, y )
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

	GT(ifp)->mi_curs_on = mode;
	if( ! mode )  {
		setcursor( 2, 1, 0 );		/* nilcursor */
		cursoff();
		return	0;
	}
	xwidth = ifp->if_width/GT(ifp)->mi_xzoom;
	i = xwidth/2;
	xmin = GT(ifp)->mi_xcenter - i;
	i = (ifp->if_height/2)/GT(ifp)->mi_yzoom;
	ymin = GT(ifp)->mi_ycenter - i;
	x -= xmin;
	y -= ymin;
	x *= GT(ifp)->mi_xzoom;
	y *= GT(ifp)->mi_yzoom;
	setcursor( 1, 1, 0 );			/* our cursor */
	curson();
	getsize(&x_size, &y_size);
	getorigin( &left, &bottom );

/*	RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF ); */
	setvaluator( MOUSEX, x+left, 0, XMAXSCREEN );
	setvaluator( MOUSEY, y+bottom, 0, YMAXSCREEN );

	return	0;
}


/*
 *			M P W _ I N Q U E U E
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
		reshapeviewport();
		gt_zbuffer(ifp);
		gt_repaint( ifp );
		redraw = 0;
	}
}

gt_zbuffer(ifp)		/* update the Z_buffer from memory */
FBIO * ifp;
{
	register unsigned char * op;
	register struct gt_pix * mp;
	register int i,j;
	register unsigned int s_offset;

	s_offset = GT(ifp)->mi_memwidth * sizeof( RGBpixel );

	zbuffer(TRUE);
	zclear();
	zbuffer(FALSE);
	
	zdraw(TRUE);
	backbuffer(FALSE);

	for( i = 0; i < ifp->if_height; i++)
	{
		op = (unsigned char *) &ifp->if_mem[ i * s_offset];

		mp = &gt_scan[0];

		for( j = 0; j < ifp->if_width; j++)
		{
			if( GT(ifp)->mi_cmap_flag == FALSE)
			{
				mp->red =  *op++;
				mp->green = *op++;
				mp->blue = *op++;
				mp++;
			} else
			{
				mp->red =  CMR(ifp)[*op++];
				mp->green = CMG(ifp)[*op++];
				mp->blue = CMB(ifp)[*op++];
				mp++;
			}
		}
		lrectwrite(0,i, ifp->if_width-1, i, gt_scan);
	}
	zdraw(FALSE);
}

_LOCAL_ int
gt_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", gt_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Maximum width height: %d %d\n",
		ifp->if_max_width,
		ifp->if_max_height );
	fb_log( "Default width height: %d %d\n",
		ifp->if_width,
		ifp->if_height );
	fb_log( "Usage: /dev/sgi[options]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}

	return(0);
}
