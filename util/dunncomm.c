/*
 *			D U N N C O M M . C
 *
 *	Common routines needed for both dunncolor and dunnsnap
 *
 *  Author -
 *	Don Merritt
 *	August 1985
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <sys/time.h>
#ifdef __NetBSD__
#	define USE_OLD_TTY
#	include <sys/ioctl_compat.h>
#endif
#if defined(__bsdi__)
#	include <sys/ioctl_compat.h>
#	define OCRNL   0000010
#endif

#include "machine.h"
#include "externs.h"

/*
 *  This file will work IFF one of these three flags is set:
 *	HAVE_TERMIOS_H	use POSIX termios and tcsetattr() call with XOPEN flags
 *	SYSV		use SysV Rel3 termio and TCSETA ioctl
 *	BSD		use Version 7 / BSD sgttyb and TIOCSETP ioctl
 */

#if defined(HAVE_TERMIOS_H)
#  undef SYSV
#  undef BSD
#  include <termios.h>

	static struct termios	tty;

#else	/* !defined(HAVE_TERMIOS_H) */

#  ifdef BSD
#    include <sys/ioctl.h>
     struct	sgttyb	tty;
#    define TCSETA	TIOCSETP
#    define TCGETA	TIOCGETP
#  endif	/* BSD */

#  ifdef SYSV
#if 0
#	if !defined(__sparc) && !defined(CRAY)
		struct timeval {
			int	tv_sec;
			int	tv_usec;
	};
#	endif
#endif
#   include <termio.h>
    struct	termio	tty;
#  endif /* SYSV */

#endif /* _POSIX_SOURCE */

#ifndef OCRNL
#define OCRNL 0x00000008
#endif

#ifndef TAB1
#define TAB1 0x00000400
#endif

#ifndef TAB2
#define TAB2 0x00000800
#endif

#ifndef	XTABS
#	define	XTABS	(TAB1 | TAB2)
#endif /* XTABS */

int fd;
char cmd;
unsigned char	status[4];
unsigned char	values[21];
fd_set	readfds;
int	polaroid = 0;		/* 0 = aux camera, 1 = Polaroid 8x10 */

void
unsnooze(x)
int	x;
{
	printf("\007dunnsnap: request timed out, aborting\n");
	exit(1);
}

/*
 *			D U N N O P E N
 */
void
dunnopen()
{

	/* open the camera device */

#ifdef HAVE_TERMIOS_H
	if( (fd = open("/dev/camera", O_RDWR | O_NONBLOCK)) < 0 )
#else
	if( (fd = open("/dev/camera", O_RDWR | O_NDELAY)) < 0 )
#endif
	{
	     	printf("\007dunnopen: can't open /dev/camera\n");
		close(fd);
		exit(10);
	}
#ifdef HAVE_TERMIOS_H
	if( tcgetattr( fd, &tty ) < 0 )
#else
	if( ioctl(fd, TCGETA, &tty) < 0)
#endif
	{
	     	printf("\007dunnopen: can't open /dev/camera\n");
		close(fd);
		exit(10);
	}
	
	/* set up the camera device */
	
#ifdef BSD
	tty.sg_ispeed = tty.sg_ospeed = B9600;
	tty.sg_flags = RAW | EVENP | ODDP | XTABS;
#endif
#if defined(SYSV) || defined(HAVE_TERMIOS_H)
	tty.c_cflag = B9600 | CS8;	/* Character size = 8 bits */
	tty.c_cflag &= ~CSTOPB;		/* One stop bit */
	tty.c_cflag |= CREAD;		/* Enable the reader */
	tty.c_cflag &= ~PARENB;		/* Parity disable */
	tty.c_cflag &= ~HUPCL;		/* No hangup on close */
	tty.c_cflag |= CLOCAL;		/* Line has no modem control */

	tty.c_iflag &= ~(BRKINT|ICRNL|INLCR|IXON|IXANY|IXOFF);
	tty.c_iflag |= IGNBRK|IGNPAR;

	tty.c_oflag &= ~(OPOST|ONLCR|OCRNL);	/* Turn off all post-processing */
#if defined(XTABS)
	tty.c_oflag |= XTABS;		/* output tab expansion ON */
#endif
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;

	tty.c_lflag &= ~ICANON;		/* Raw mode */
	tty.c_lflag &= ~ISIG;		/* Signals OFF */
	tty.c_lflag &= ~(ECHO|ECHOE|ECHOK);	/* Echo mode OFF */
#endif

#if HAVE_TERMIOS_H
	if( tcsetattr( fd, TCSAFLUSH, &tty ) < 0 )
#else
	if( ioctl(fd, TCSETA, &tty) < 0 )
#endif
	{
		perror("/dev/camera");
		exit(20);
	}

	/* Be certain the FNDELAY is off */
	if( fcntl(fd, F_SETFL, 0) < 0 )  {
		perror("/dev/camera");
		exit(21);
	}

	/* Set up alarm clock catcher */
	(void)signal( SIGALRM, unsnooze );
}

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.
 */
static int
mread(fd, bufp, n)
int fd;
register unsigned char	*bufp;
unsigned	n;
{
	register unsigned	count = 0;
	register int		nread;

	do {
		nread = read(fd, bufp, n-count);
		if(nread == -1)
			return(nread);
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}

/*
 *			G O O D S T A T U S
 *
 *	Checks the status of the Dunn camera and returns 1 for good status
 *	and 0 for bad status.
 *
 */
int
goodstatus()
{
	struct timeval waittime, *timeout;

	timeout = &waittime;
	timeout->tv_sec = 10;
	timeout->tv_usec = 0;
	
	cmd = ';';	/* status request cmd */
	write(fd, &cmd, 1);	
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	select(fd+1, &readfds, (fd_set *)0, (fd_set *)0, timeout);
	if( FD_ISSET(fd, &readfds) ==0 ) {
		printf("\007dunnsnap: status request timed out\n");
		return(0);
	}

	(void)mread(fd, status, 4);
	alarm(0);

	if (status[0]&0x1)  printf("No vertical sync\n");
	if (status[0]&0x2)  printf("8x10 not ready\n");
	if (status[0]&0x4)  printf("Expose in wrong mode\n");
	if (status[0]&0x8)  printf("Aux camera out of film\n");
	if (status[1]&0x1)  printf("B/W mode\n");
	if (status[1]&0x2)  printf("Separate mode\n");
	if (status[2]&0x1)  printf("Y-smoothing off\n");

	if ((status[0]&0xf) == 0x0 &&
	    (status[1]&0x3) == 0x0 &&
	    (status[3]&0x7f)== '\r')
		return 1;	/* status is ok */

	printf("\007dunnsnap: status error from camera\n");
	printf("status[0]= 0x%x [1]= 0x%x [2]= 0x%x [3]= 0x%x\n",
		status[0]&0x7f,status[1]&0x7f,
		status[2]&0x7f,status[3]&0x7f);
	return 0;	/* status is bad or request timed out */
}

/*
 *			H A N G T E N 
 *
 *	Provides a 10 millisecond delay when called
 *
 */
void
hangten()
{
	static struct timeval delaytime = { 0, 10000}; /* set timeout to 10mS*/

	select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &delaytime);
}

/*
 *			R E A D Y
 *
 *	Sends a ready test command to the Dunn camera and returns 1 if the
 *	camera is ready or 0 if the camera is not ready after waiting the
 *	number of seconds specified by the argument.
 *
 */
int
ready(nsecs)
int nsecs;
{
	register int i;

	struct timeval waittime, *timeout;
	timeout = &waittime;
	timeout->tv_sec = nsecs;
	timeout->tv_usec = 0;

	cmd = ':';	/* ready test command */
	write(fd, &cmd, 1);

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	select(fd+1, &readfds, (fd_set *)0, (fd_set *)0, timeout);
	if ( FD_ISSET(fd, &readfds) ) {
		return 0;	/* timeout after n secs */
	}
	status[0] = status[1] = '\0';
	/* This loop is needed to skip leading nulls in input stream */
	do {
		i = read(fd, &status[0], 1);
		if( i != 1 )  {
			printf("dunnsnap:  unexpected EOF %d\n", i);
			return(0);
		}
	} while( status[0] == '\0' );
	(void)read(fd, &status[1], 1);

	if((status[0]&0x7f) == 'R' && (status[1]&0x7f) == '\r')
		return 1;	/* camera is ready */

	printf("dunnsnap/ready():  unexpected camera status 0%o 0%o\n",
		status[0]&0x7f, status[1]&0x7f);
	return 0;	/* camera is not ready */
}

/*
 *			G E T E X P O S U R E
 *
 *  Get and print the current exposure
 */
void
getexposure(title)
char *title;
{
	struct timeval waittime;

	waittime.tv_sec = 20;
	waittime.tv_usec = 0;

	if(!ready(20)) {
		printf("dunncolor: (getexposure) camera not ready\n");
		exit(60);
	}

	if(polaroid)
		cmd = '<';	/* req 8x10 exposure values */
	else
		cmd = '=';	/* request AUX exposure values */
	write(fd, &cmd, 1);
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	select(fd+1, &readfds, (fd_set *)0, (fd_set *)0, &waittime);
	if( FD_ISSET(fd, &readfds) ) {
		printf("dunncolor:\007 %s request exposure value cmd: timed out\n", title);
		exit(40);
	}

	(void)mread(fd, values, 20);

	values[20] = '\0';
	printf("dunncolor: %s = %s\n", title, values);
}

/*
 *			D U N N S E N D
 *
 */
int
dunnsend(color,val)
char color;
int val;
{
	char digit;

	if(val < 0 || val > 255) {
		printf("dunncolor: bad value %d\n",val);
		return(-1);
	}

	if(!ready(5)) {
		printf("dunncolor: dunnsend(), camera not ready\n");
		return(-1);
	}

	if( polaroid )
		cmd = 'K';	/* set 8x10 exposure values */
	else
		cmd = 'L';	/* set AUX exposure values */
	write(fd, &cmd, 1);
	hangten();
	write(fd, &color, 1);
	hangten();
	digit = (val/100 + 0x30)&0x7f;
	write(fd, &digit, 1);
	hangten();
	val = val%100;
	digit = (val/10 + 0x30)&0x7f;
	write(fd, &digit, 1);
	hangten();
	digit = (val%10 + 0x30)&0x7f;
	write(fd, &digit,1);
	hangten();
	return(0);		/* OK */
}
