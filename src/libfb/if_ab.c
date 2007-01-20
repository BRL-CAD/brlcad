/*                         I F _ A B . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup if */
/*@{*/
/** @file if_ab.c
 *  Communicate with an Abekas A60 digital videodisk as if it was
 *  a framebuffer, to ease the task of loading and storing images.
 *
 *  Authors -
 *	Michael John Muuss
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"


HIDDEN void	ab_log(FBIO *ifp, char *str);
HIDDEN int	ab_get_reply(int fd);
HIDDEN void	ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, int len);
HIDDEN void	ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, int len);
HIDDEN int	ab_yuvio(int, char *, char *, int, int, int);

HIDDEN int	ab_open(register FBIO *ifp, register char *file, int width, int height),
		ab_close(FBIO *ifp),
		ab_clear(FBIO *ifp, unsigned char *bgpp),
		ab_read(register FBIO *ifp, int x, register int y, unsigned char *pixelp, int count),
		ab_write(register FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		ab_rmap(register FBIO *ifp, register ColorMap *cmap),
		ab_wmap(register FBIO *ifp, const ColorMap *cmap),
		ab_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		ab_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		ab_cursor(FBIO *ifp, int mode, int x, int y),
		ab_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		ab_help(FBIO *ifp);

FBIO abekas_interface = {
	0,
	ab_open,
	ab_close,
	ab_clear,
	ab_read,
	ab_write,
	ab_rmap,
	ab_wmap,
	ab_view,
	ab_getview,
	fb_null_setcursor,		/* setcursor */
	ab_cursor,
	ab_getcursor,
	fb_sim_readrect,
	fb_sim_writerect,
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	fb_null,			/* poll */
	fb_null,			/* flush */
	ab_close,			/* free */
	ab_help,
	"Abekas A60 Videodisk, via Ethernet",
	720,				/* max width */
	486,				/* max height */
	"/dev/ab",
	720,				/* current/default width */
	486,				/* current/default height */
	-1,				/* select fd */
	-1,				/* file descriptor */
	1, 1,				/* zoom */
	360, 243,			/* window center */
	0, 0, 0,			/* cursor */
	PIXEL_NULL,			/* page_base */
	PIXEL_NULL,			/* page_curp */
	PIXEL_NULL,			/* page_endp */
	-1,				/* page_no */
	0,				/* page_ref */
	0L,				/* page_curpos */
	0L,				/* page_pixels */
	0				/* debug */
};

#define if_frame	u1.l		/* frame number on A60 disk */
#define if_mode		u2.l		/* see MODE_ and STATE_ defines */
#define if_yuv		u3.p		/* YUV image, 4th quadrant */
#define if_rgb		u4.p		/* RGB image, 1st quadrant */
#define if_host		u5.p		/* Hostname */
#define if_xyoff	u6.l		/* (x<<16) | y pixel offsets */

/*
 *  The mode has several independent bits:
 *	Center -vs- lower-left
 *	Output-only (or well-behaved read-write) -vs- conservative read-write
 *
 *  Also, the state bits ride in some of the upper bits.
 */
#define MODE_1MASK	(1<<0)
#define MODE_1CENTER	(0<<0)
#define MODE_1LOWERLEFT	(1<<0)

#define MODE_2MASK	(1<<1)
#define MODE_2READFIRST	(0<<1)
#define MODE_2OUTONLY	(1<<1)

#define MODE_3MASK	(1<<2)
#define MODE_3QUIET	(0<<2)
#define MODE_3VERBOSE	(1<<2)

#define MODE_4MASK	(1<<3)
#define MODE_4NETWORK	(0<<3)
#define MODE_4DISK	(1<<3)

#define STATE_FRAME_WAS_READ	(1<<8)
#define STATE_USER_HAS_READ	(1<<9)
#define STATE_USER_HAS_WRITTEN	(1<<10)

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'l',	MODE_1MASK, MODE_1LOWERLEFT,
		"Lower left;  default=center" },
	{ 'o',	MODE_2MASK, MODE_2OUTONLY,
		"Output only (in before out); default=always read first" },
	{ 'v',	MODE_3MASK, MODE_3VERBOSE,
		"Verbose logging; default=quiet" },
	{ 'f',	MODE_4MASK, MODE_4DISK,
		"Read/write YUV to disk file given by @#; default=use EtherNet" },
	{ '\0', 0, 0, "" }
};


/*
 *			A B _ O P E N
 *
 *  The device name is expected to have a fairly rigid format:
 *
 *	/dev/abco@host#300
 *
 *  ie, options follow first (if any),
 *  "@host" gives target host (else use environment variable),
 *  "#300" gives frame number (else use default frame).
 *
 *  It is intentional that the frame number is last in the sequence;
 *  this should make changing it easier with tcsh, etc.
 *
 */
HIDDEN int
ab_open(register FBIO *ifp, register char *file, int width, int height)
{
	register char	*cp;
	register int	i;
	char	message[128];
	int	mode;

	FB_CK_FBIO(ifp);
	mode = 0;

	if( file == NULL )  {
		fb_log( "ab_open: NULL device string\n" );
		return(-1);
	}

	if( strncmp( file, "/dev/ab", 7 ) != 0 )  {
		fb_log("ab_open: bad device '%s'\n", file );
		return(-1);
	}

	/* Process any options */
	for( cp = &file[7]; *cp != '\0'; cp++)  {
		register struct	modeflags *mfp;

		if( *cp == '@' || *cp == '#' )  break;
		for( mfp = modeflags; mfp->c != '\0'; mfp++ )  {
			if( mfp->c != *cp )  continue;
			mode = (mode & ~(mfp->mask)) | mfp->value;
			break;
		}
		if( mfp->c == '\0' )  {
			fb_log("ab_open: unknown option '%c' ignored\n", *cp);
		}
	}
	ifp->if_mode = mode;

	/* Process host name */
	if( *cp == '@' )  {
		register char	*ep;

		cp++;			/* advance over '@' */

		/* Measure length, allocate memory for string */
		for( ep=cp; *ep != '\0' && *ep != '#'; ep++ ) /* NULL */ ;
		ifp->if_host = (char *)malloc(ep-cp+2);

		ep = ifp->if_host;
		for( ; *cp != '\0' && *cp != '#'; )
			*ep++ = *cp++;
		*ep++ = '\0';
	} else if( *cp != '#' && *cp != '\0' )  {
		fb_log("ab_open: error in file spec '%s'\n", cp);
		return(-1);
	} else {
		/* Get hostname from environment variable */
		if( (ifp->if_host = getenv("ABEKAS")) == NULL )  {
			if( (ifp->if_mode & MODE_4MASK) == MODE_4NETWORK )  {
				fb_log("ab_open: hostname not given and ABEKAS environment variable not set\n");
				return(-1);
			} else {
				/* Here, "host" is a filename prefix. */
				ifp->if_host = "";
			}
		}
	}

	/* Process frame number */
	ifp->if_frame = -1;		/* default to frame store */
	if( *cp == '#' )  {
		register int	i;

		i = atoi(cp+1);
		if( (ifp->if_mode & MODE_4MASK) == MODE_4NETWORK )  {
			/* Perform validity checking on frame numbers */
			if( i >= 50*30 )  {
				fb_log("ab_open: frame %d out of range\n", i);
				return(-1);
			}
			if( i < 0 ) {
				fb_log("ab_open: frame < 0, using frame store\n");
			}
		}
		ifp->if_frame = i;
	} else if( *cp != '\0' )  {
		fb_log("ab_open: error in file spec '%s'\n", cp);
		return(-1);
	}

	/* Allocate memory for YUV and RGB buffers */
	if( (ifp->if_yuv = malloc(720*486*2)) == NULL ||
	    (ifp->if_rgb = malloc(720*486*3)) == NULL )  {
		fb_log("ab_open: unable to malloc buffer\n");
		return(-1);
	}

	/* Handle size defaulting and checking */
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

	/* X and Y offsets if centering & non-full size */
	ifp->if_xyoff = 0;
	if( (ifp->if_mode & MODE_1MASK) == MODE_1CENTER )  {
		if( width < ifp->if_max_width )  {
			i = (ifp->if_max_width - width)/2;
			ifp->if_xyoff = i<<16;
		}
		if( height < ifp->if_max_height )  {
			i = (ifp->if_max_height - height)/2;
			i &= ~1;	/* preserve field alignment */
			ifp->if_xyoff |= i;
		}
	}


	/*
	 *  If "output-only" mode was set, clear the frame to black.
	 */
	if( (ifp->if_mode & MODE_2MASK) == MODE_2OUTONLY )  {
		(void)ab_clear( ifp, PIXEL_NULL );
		/* This sets STATE_USER_HAS_WRITTEN */
	}

	if( ifp->if_xyoff )  {
		sprintf(message,"ab_open %d*%d xoff=%ld yoff=%ld",
			ifp->if_width, ifp->if_height,
			ifp->if_xyoff>>16, ifp->if_xyoff&0xFFFF );
	} else {
		sprintf(message,"ab_open %d*%d",
			ifp->if_width, ifp->if_height);
	}
	ab_log(ifp, message);

	return( 0 );			/* OK */
}

/*
 *			A B _ L O G
 *
 *  If verbose mode is enabled, print the time and a message
 */
HIDDEN void ab_log(FBIO *ifp, char *str)
{
	time_t		now;
	struct tm	*tmp;

	if( (ifp->if_mode & MODE_3MASK) != MODE_3VERBOSE )  return;

	(void)time( &now );
	tmp = localtime( &now );
	if( ifp->if_frame < 0 ) {
		fb_log("%2.2d:%2.2d:%2.2d %s frame store: %s\n",
			tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
			ifp->if_host, str );
	} else {
		fb_log("%2.2d:%2.2d:%2.2d %s frame %d: %s\n",
			tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
			ifp->if_host, ifp->if_frame, str );
	}
}

/*
 *			A B _ R E A D F R A M E
 */
HIDDEN int
ab_readframe(FBIO *ifp)
{
	register int	y;

	ab_log(ifp, "Reading frame");
	if( ab_yuvio( 0, ifp->if_host, ifp->if_yuv,
	    720*486*2, ifp->if_frame,
	    (ifp->if_mode & MODE_4MASK) == MODE_4NETWORK
	    ) != 720*486*2 )  {
		fb_log("ab_readframe(%d): unable to get frame from %s!\n",
			ifp->if_frame, ifp->if_host);
		return(-1);
	}

	/* convert YUV to RGB */
	ab_log(ifp, "Converting YUV to RGB");
	for( y=0; y < 486; y++ )  {
		ab_yuv_to_rgb(
		    &ifp->if_rgb[(486-1-y)*720*3],
		    &ifp->if_yuv[y*720*2],
		    720 );
	}
	ab_log(ifp, "Conversion done");

	ifp->if_mode |= STATE_FRAME_WAS_READ;
	return(0);			/* OK */
}

/*
 *			A B _ C L O S E
 */
HIDDEN int
ab_close(FBIO *ifp)
{
	int	ret = 0;

	if( ifp->if_mode & STATE_USER_HAS_WRITTEN )  {
		register int y;		/* in Abekas coordinates */

		/* Convert RGB to YUV */
		ab_log(ifp, "Converting RGB to YUV");
		for( y=0; y < 486; y++ )  {
			ab_rgb_to_yuv(
			    &ifp->if_yuv[y*720*2],
			    &ifp->if_rgb[(486-1-y)*720*3],
			    720 );
		}
		ab_log(ifp, "Writing frame");

		if( ab_yuvio( 1, ifp->if_host, ifp->if_yuv,
		    720*486*2, ifp->if_frame,
		    (ifp->if_mode & MODE_4MASK) == MODE_4NETWORK
		    ) != 720*486*2 )  {
			fb_log("ab_close: unable to send frame %d to A60 %s!\n",
				ifp->if_frame, ifp->if_host);
		    	ret = -1;
		}
		ab_log(ifp, "Transmission done");
	}

	/* Free dynamic memory */
	free( ifp->if_yuv );
	ifp->if_yuv = NULL;
	free( ifp->if_rgb );
	ifp->if_rgb = NULL;

	ab_log(ifp, "ab_close");
	return(ret);
}

/*
 *			A B _ C L E A R
 */
HIDDEN int
ab_clear(FBIO *ifp, unsigned char *bgpp)
{
	register int	r,g,b;
	register int	count;
	register char	*cp;

	if( bgpp == PIXEL_NULL )  {
		/* Clear to black */
		bzero( ifp->if_rgb, 720*486*3 );
	} else {
		r = (bgpp)[RED];
		g = (bgpp)[GRN];
		b = (bgpp)[BLU];

		cp = ifp->if_rgb;
		for( count = 720*486-1; count >= 0; count-- )  {
			*cp++ = r;
			*cp++ = g;
			*cp++ = b;
		}
	}

	ifp->if_mode |= STATE_USER_HAS_WRITTEN;
	return(0);
}

/*
 *			A B _ R E A D
 */
HIDDEN int
ab_read(register FBIO *ifp, int x, register int y, unsigned char *pixelp, int count)
{
	register short		scan_count;	/* # pix on this scanline */
	register char		*cp;
	int			ret;
	int			xoff, yoff;

	if( count <= 0 )
		return(0);

	if( x < 0 || x > ifp->if_width ||
	    y < 0 || y > ifp->if_height)
		return(-1);

	if( (ifp->if_mode & STATE_FRAME_WAS_READ) == 0 )  {
		if( (ifp->if_mode & STATE_USER_HAS_WRITTEN) != 0 )  {
			fb_log("ab_read:  WARNING out-only mode set & pixels were written.  Subsequent read operation is unsafe\n");
			/* Give him whatever is in the buffer */
		} else {
			/* Read in the frame */
			if( ab_readframe(ifp) < 0 )  return(-1);
		}
	}

	xoff = ifp->if_xyoff>>16;
	yoff = ifp->if_xyoff & 0xFFFF;

	/* Copy from if_rgb[] */
	ret = 0;
	cp = (char *)(pixelp);

	while( count )  {
		if( y >= ifp->if_height )
			break;

		if ( count >= ifp->if_width-x )
			scan_count = ifp->if_width-x;
		else
			scan_count = count;

		bcopy( &ifp->if_rgb[((y+yoff)*720+(x+xoff))*3], cp,
			scan_count*3 );
		cp += scan_count * 3;
		ret += scan_count;
		count -= scan_count;
		/* Advance upwards */
		x = 0;
		y++;
	}
	ifp->if_mode |= STATE_USER_HAS_READ;
	return(ret);
}

/*
 *			A B _ W R I T E
 */
HIDDEN int
ab_write(register FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	register short		scan_count;	/* # pix on this scanline */
	register const unsigned char	*cp;
	int			ret;
	int			xoff, yoff;

	if( count <= 0 )
		return(0);

	if( x < 0 || x > ifp->if_width ||
	    y < 0 || y > ifp->if_height)
		return(-1);

	/*
	 *  If this is the first write, and the frame has not
	 *  yet been read, then read it.
	 */
	if( (ifp->if_mode & STATE_FRAME_WAS_READ) == 0 &&
	    (ifp->if_mode & STATE_USER_HAS_WRITTEN) == 0 )  {
		/* Read in the frame first */
		(void)ab_readframe(ifp);
	}

	xoff = ifp->if_xyoff>>16;
	yoff = ifp->if_xyoff & 0xFFFF;

	/* Copy from if_rgb[] */
	ret = 0;
	cp = (pixelp);

	while( count )  {
		if( y >= ifp->if_height )
			break;

		if ( count >= ifp->if_width-x )
			scan_count = ifp->if_width-x;
		else
			scan_count = count;

		bcopy( (char *)cp, &ifp->if_rgb[((y+yoff)*720+(x+xoff))*3],
			scan_count*sizeof(RGBpixel) );
		cp += scan_count * sizeof(RGBpixel);
		ret += scan_count;
		count -= scan_count;
		/* Advance upwards */
		x = 0;
		y++;
	}
	ifp->if_mode |= STATE_USER_HAS_WRITTEN;
	return(ret);
}

/*
 */
HIDDEN int
ab_cursor(FBIO *ifp, int mode, int x, int y)
{
	fb_sim_cursor(ifp, mode, x, y);

	return(-1);
}

/*
 */
HIDDEN int
ab_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	fb_sim_getcursor(ifp, mode, x, y);

	return(-1);
}

/*
 */
HIDDEN int
ab_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);

	return(-1);
}

/*
 */
HIDDEN int
ab_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	fb_sim_getview(ifp, xcenter, ycenter, xzoom, yzoom);

	return(-1);
}

HIDDEN int
ab_rmap(register FBIO *ifp, register ColorMap *cmap)
{
	register int	i;

	for( i = 0; i < 256; i++ ) {
		cmap->cm_red[i] = i<<8;
		cmap->cm_green[i] = i<<8;
		cmap->cm_blue[i] = i<<8;
	}
	return(0);
}

HIDDEN int
ab_wmap(register FBIO *ifp, const ColorMap *cmap)
{
	/* Just pretend it worked OK */
	return(0);
}

/*
 *			A B _ H E L P
 */
HIDDEN int
ab_help(FBIO *ifp)
{
	struct	modeflags *mfp;

	fb_log( "Abekas A60 Ethernet Interface\n" );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Maximum width height: %d %d\n",
		ifp->if_max_width,
		ifp->if_max_height );
	fb_log( "Default width height: %d %d\n",
		ifp->if_width,
		ifp->if_height );
	fb_log( "A60 Host: %s\n", ifp->if_host );
	if( ifp->if_frame < 0 ) {
		fb_log( "Frame: store\n" );
	} else {
		fb_log( "Frame: %d\n", ifp->if_frame );
	}
	fb_log( "Usage: /dev/ab[options][@host][#framenumber]\n" );
	fb_log( "       If no framenumber is given, the frame store is used\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}

	return(0);
}

/*
 *			A B _ Y U V I O
 *
 *  Input or output a full frame image from the Abekas using the RCP
 *  protocol.
 *
 *  Returns -
 *	-1	error
 *	len	successful count
 */
int
ab_yuvio(int output, char *host, char *buf, int len, int frame, int to_network)
   	       		/* 0=read(input), 1=write(output) */
   	      		/* frame number */
{
	struct sockaddr_in	sinme;		/* Client */
	struct sockaddr_in	sinhim;		/* Server */
	struct servent		*rlogin_service;
	struct hostent		*hp;
	char			xmit_buf[128] = {0};
	int			n;
	int			netfd;
	int			got;

	if( !to_network )  {
		if( frame >= 0 )
			sprintf( xmit_buf, "%s%d.yuv", host, frame );
		else
			sprintf( xmit_buf, "%s.yuv", host);
		if( output )
			netfd = creat( xmit_buf, 0444 );
		else
			netfd = open( xmit_buf, 0 );
		if( netfd < 0 )  {
			perror( xmit_buf );
			return -1;
		}
		if( output )
			got = write( netfd, buf, len );
		else
			got = bu_mread( netfd, buf, len );
		if( got != len )  {
			perror("read/write");
			fb_log("ab_yuvio file read/write error, len=%d, got=%d\n", len, got );
			goto err;
		}
		(void)close(netfd);
		return got;		/* OK */
	}

	bzero((char *)&sinhim, sizeof(sinhim));
	bzero((char *)&sinme, sizeof(sinme));

	if( (rlogin_service = getservbyname("shell", "tcp")) == NULL )  {
		fb_log("getservbyname(shell,tcp) fail\n");
		return(-1);
	}
	sinhim.sin_port = rlogin_service->s_port;

	if( atoi( host ) > 0 )  {
		/* Numeric */
		sinhim.sin_family = AF_INET;
		sinhim.sin_addr.s_addr = inet_addr(host);
	} else {
		if( (hp = gethostbyname(host)) == NULL )  {
			fb_log("gethostbyname(%s) fail\n", host);
			return(-1);
		}
		sinhim.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&sinhim.sin_addr, hp->h_length);
	}

	if( (netfd = socket(sinhim.sin_family, SOCK_STREAM, 0)) < 0 )  {
		perror("socket()");
		return(-1);
	}
	sinme.sin_port = 0;		/* let kernel pick it */

	if( bind(netfd, (struct sockaddr *)&sinme, sizeof(sinme)) < 0 )  {
		perror("bind()");
		goto err;
	}

	if( connect(netfd, (struct sockaddr *)&sinhim, sizeof(sinhim)) < 0 ) {
		perror("connect()");
		goto err;
	}

	/*
	 *  Connection established.  Now, speak the RCP protocol.
	 */

	/* Indicate that standard error is not connected */
	if( write( netfd, "\0", 1 ) != 1 )  {
		perror("write()");
		goto err;
	}

	/* Write local and remote user names, null terminated */
	if( write( netfd, "a60libfb\0", 9 ) != 9 )  {
		perror("write()");
		goto err;
	}
	if( write( netfd, "a60libfb\0", 9 ) != 9 )  {
		perror("write()");
		goto err;
	}
	if( ab_get_reply(netfd) < 0 )  goto err;

	if( output )  {
		/* Output from buffer to A60 */
		/* Send command, null-terminated */
		if( frame < 0 )
			sprintf( xmit_buf, "rcp -t store.yuv" );
		else
			sprintf( xmit_buf, "rcp -t %d.yuv", frame );
		n = strlen(xmit_buf)+1;		/* include null */
		if( write( netfd, xmit_buf, n ) != n )  {
			perror("write()");
			goto err;
		}
		if( ab_get_reply(netfd) < 0 )  goto err;

		/* Send Access flags, length, old name */
		if( frame < 0 )
			sprintf( xmit_buf, "C0664 %d store.yuv\n", len );
		else
			sprintf( xmit_buf, "C0664 %d %d.yuv\n", len, frame );
		n = strlen(xmit_buf);
		if( write( netfd, xmit_buf, n ) != n )  {
			perror("write()");
			goto err;
		}
		if( ab_get_reply(netfd) < 0 )  goto err;

		if( (got = write( netfd, buf, len )) != len )  {
			perror("write()");
			goto err;
		}

		/* Send final go-ahead */
		if( (got = write( netfd, "\0", 1 )) != 1 )  {
			fb_log("go-ahead write got %d\n", got);
			perror("write()");
			goto err;
		}
		if( (got = ab_get_reply(netfd)) < 0 )  {
			fb_log("get_reply got %d\n", got);
			goto err;
		}

		(void)close(netfd);
		return(len);		/* OK */
	} else {
		register char	*cp;
		int		perm;
		int		src_size;
		/* Input from A60 into buffer */
		/* Send command, null-terminated */
		if( frame < 0 )
			sprintf( xmit_buf, "rcp -f store.yuv" );
		else
			sprintf( xmit_buf, "rcp -f %d.yuv", frame );
		n = strlen(xmit_buf)+1;		/* include null */
		if( write( netfd, xmit_buf, n ) != n )  {
			perror("write()");
			goto err;
		}

		/* Send go-ahead */
		if( write( netfd, "\0", 1 ) != 1 )  {
			perror("write()");
			goto err;
		}

		/* Read up to a newline */
		cp = xmit_buf;
		for(;;)  {
			if( read( netfd, cp, 1 ) != 1 )  {
				perror("read()");
				goto err;
			}
			if( *cp == '\n' )  break;
			cp++;
			if( (cp - xmit_buf) >= sizeof(xmit_buf) )  {
				fb_log("cmd buffer overrun\n");
				goto err;
			}
		}
		*cp++ = '\0';
		/* buffer will contain old permission, size, old name */
		src_size = 0;
		if( sscanf( xmit_buf, "C%o %d", (unsigned int *)&perm, &src_size ) != 2 )  {
			fb_log("sscanf error\n");
			goto err;
		}

		/* Send go-ahead */
		if( write( netfd, "\0", 1 ) != 1 )  {
			perror("write()");
			goto err;
		}

		/* Read data */
		if( (got = bu_mread( netfd, buf, len )) != len )  {
			fb_log("bu_mread len=%d, got %d\n", len, got );
			goto err;
		}

		/* Send go-ahead */
		if( write( netfd, "\0", 1 ) != 1 )  {
			perror("write()");
			goto err;
		}
		(void)close(netfd);
		return(len);		/* OK */
	}

err:
	(void)close(netfd);
	return(-1);
}

HIDDEN int
ab_get_reply(int fd)
{
	char	rep_buf[128];
	int	got;

	if( (got = read( fd, rep_buf, sizeof(rep_buf) )) < 0 )  {
		perror("ab_get_reply()/read()");
		fb_log("ab_get_reply() read error\n");
		return(-1);
	}

	if( got == 0 )  {
		fb_log("ab_get_reply() unexpected EOF\n");
		return(-2);		/* EOF seen */
	}

	/* got >= 1 */
	if( rep_buf[0] == 0 )
		return(0);		/* OK */

	if( got == 1 )  {
		fb_log("ab_get_reply() error reply code, no attached message\n");
		return(-3);
	}

	/* Print error code received from other end */
	fb_log("ab_get_reply() error='%s'\n", &rep_buf[1] );
	return(-4);
}


/*************************************************************************
 *************************************************************************
 *  Herein lies the conversion between YUV and RGB
 *************************************************************************
 *************************************************************************
 */
/*  A 4:2:2 framestore uses 2 bytes per pixel.  The even pixels (from 0)
 *  hold Cb and Y, the odd pixels Cr and Y.  Thus a scan line has:
 *      Cb Y Cr Y Cb Y Cr Y ...
 *  If we are at an even pixel, we use the Cr value following it.  If
 *  we are at an odd pixel, we use the Cb value following it.
 *
 *  Y:       0 .. 219 range, offset by 16   [16 .. 235]
 *  U, V: -112 .. +112 range, offset by 128 [16 .. 240]
 */

#define	VDOT(a,b)	(a[0]*b[0]+a[1]*b[1]+a[2]*b[2])
#define	V5DOT(a,b)	(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3]+a[4]*b[4])
#define	CLIP(out,in)		{ register int t; \
		if( (t = (in)) < 0 )  (out) = 0; \
		else if( t >= 255 )  (out) = 255; \
		else (out) = t; }

#define	LINE_LENGTH	720
#define	FRAME_LENGTH	486

static double	y_weights[] = {  0.299,   0.587,   0.114 };
static double	u_weights[] = { -0.1686, -0.3311,  0.4997 };
static double	v_weights[] = {  0.4998, -0.4185, -0.0813 };

static double	y_filter[] = { -0.05674, 0.01883, 1.07582, 0.01883, -0.05674 };
static double	u_filter[] = {  0.14963, 0.22010, 0.26054, 0.22010,  0.14963 };
static double	v_filter[] = {  0.14963, 0.22010, 0.26054, 0.22010,  0.14963 };

static double	ybuf[724];
static double	ubuf[724];
static double	vbuf[724];

/* RGB to YUV */
HIDDEN void
ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, int len)
{
	register unsigned char *cp;
	register double	*yp, *up, *vp;
	register int	i;
	static int	first=1;

	if(first)  {
		/* SETUP */
		for( i = 0; i < 5; i++ ) {
			y_filter[i] *= 219.0/255.0;
			u_filter[i] *= 224.0/255.0;
			v_filter[i] *= 224.0/255.0;
		}
		first = 0;
	}

	/* Matrix RGB's into separate Y, U, and V arrays */
	yp = &ybuf[2];
	up = &ubuf[2];
	vp = &vbuf[2];
	cp = rgb_buf;
	for( i = len; i; i-- ) {
		*yp++ = VDOT( y_weights, cp );
		*up++ = VDOT( u_weights, cp );
		*vp++ = VDOT( v_weights, cp );
		cp += 3;
	}

	/* filter, scale, and sample YUV arrays */
	yp = ybuf;
	up = ubuf;
	vp = vbuf;
	cp = yuv_buf;
	for( i = len/2; i; i-- ) {
		*cp++ = V5DOT(u_filter,up) + 128.0;	/* u */
		*cp++ = V5DOT(y_filter,yp) + 16.0;	/* y */
		*cp++ = V5DOT(v_filter,vp) + 128.0;	/* v */
		yp++;
		*cp++ = V5DOT(y_filter,yp) + 16.0;	/* y */
		yp++;
		up += 2;
		vp += 2;
	}
}

/* YUV to RGB */
HIDDEN void
ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, int len)
{
	register unsigned char *rgbp;
	register unsigned char *yuvp;
	register double	y;
	register double	u = 0.0;
	register double	v;
	register int	pixel;
	int		last;

	/* Input stream looks like:  uy  vy  uy  vy  */

	rgbp = rgb_buf;
	yuvp = yuv_buf;
	last = len/2;
	for( pixel = last; pixel; pixel-- ) {
		/* even pixel, get y and next v */
		if( pixel == last ) {
			u = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);
		}
		y = ((double)(((int)*yuvp++) - 16)) * (255.0/219.0);
		v = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);

		CLIP( *rgbp++, y + 1.4026 * v);			/* R */
		CLIP( *rgbp++, y - 0.3444 * u - 0.7144 * v);	/* G */
		CLIP( *rgbp++, y + 1.7730 * u);			/* B */

		/* odd pixel, got v already, get y and next u */
		y = ((double)(((int)*yuvp++) - 16)) * (255.0/219.0);
		if( pixel != 1 ) {
			u = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);
		}

		CLIP( *rgbp++, y + 1.4026 * v);			/* R */
		CLIP( *rgbp++, y - 0.3444 * u - 0.7144 * v);	/* G */
		CLIP( *rgbp++, y + 1.7730 * u);			/* B */
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
