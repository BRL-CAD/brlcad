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
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <fcntl.h>

#ifdef BSD
# include <sys/time.h>
# include <sys/ioctl.h>
struct	sgttyb	tty;
#define TCSETA	TIOCSETP
#define TCGETA	TIOCGETP
#ifndef	XTABS
#define	XTABS	(TAB1 | TAB2)
#endif XTABS
#endif

#ifdef SYSV
struct timeval {
	int	tv_sec;
	int	tv_usec;
};
# include <termio.h>
struct	termio	tty;
#endif SYSV

int fd;
char cmd;
unsigned char	status[4];
unsigned char	values[21];
int	readfds;
int	polaroid = 0;		/* 0 = aux camera, 1 = Polaroid 8x10 */

/*
 *			D U N N O P E N
 */
void
dunnopen()
{

	/* open the camera device */

	if( (fd = open("/dev/camera", O_RDWR | O_NDELAY)) < 0 
	     || ioctl(fd, TCGETA, &tty) < 0) {
	     	printf("\007dunnopen: can't open /dev/camera\n");
		close(fd);
		exit(10);
	}
	
	/* set up the camera device */

#ifdef BSD
	tty.sg_ispeed = tty.sg_ospeed = B9600;
	tty.sg_flags = RAW | EVENP | ODDP | XTABS;
#endif
#ifdef SYSV
	tty.c_cflag = B9600 | CS7 | TAB3;
	tty.c_lflag &= ~ICANON;		/* Raw mode */
	tty.c_lflag &= ~ISIG;		/* Signals OFF */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
#endif
	if( ioctl(fd, TCSETA, &tty) < 0 ) {
		printf("\007dunnopen: error on /dev/camera setup\n");
		exit(20);
	}
}

#ifdef SYSV
select()
{
	sleep(1);
}
#endif SYSV

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
register char	*bufp;
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

goodstatus()
{
	struct timeval waittime, *timeout;
	
	timeout = &waittime;
	timeout->tv_sec = 10;
	timeout->tv_usec = 0;
	
	cmd = ';';	/* status request cmd */
	write(fd, &cmd, 1);	
	readfds = 1<<fd;
	select(fd+1, &readfds, (int *)0, (int *)0, timeout);
	if( (readfds & (1<<fd)) !=0) {
		mread(fd, status, 4);
		if (status[0]&0x1)  printf("No vertical sync\n");
		if (status[0]&0x2)  printf("8x10 not ready\n");
		if (status[0]&0x4)  printf("Expose in wrong mode\n");
		if (status[0]&0x8)  printf("Aux camera out of film\n");
		if (status[1]&0x1)  printf("B/W mode\n");
		if (status[1]&0x2)  printf("Separate mode\n");
		if (status[2]&0x1)  printf("Y-smoothing off\n");

		if ((status[0]&0xf) == 0x0 &&
		    (status[1]&0x3) == 0x0 )
			return 1;	/* status is ok */
		printf("\007dunnsnap: status error from camera\n");
		printf("status[0]= 0x%x [1]= 0x%x [2]= 0x%x [3]= 0x%x\n",
			status[0]&0xf,status[1]&0xf,
			status[2]&0x3,status[3]&0x7f);
	} else
		printf("\007dunnsnap: status request timed out\n");
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

	select(0, (int *)0, (int *)0, (int *)0, &delaytime);
}

/*
 *			R E A D Y
 *
 *	Sends a ready test command to the Dunn camera and returns 1 if the
 *	camera is ready or 0 if the camera is not ready after waiting the
 *	number of seconds specified by the argument.
 *
 */
ready(nsecs)
int nsecs;
{
	struct timeval waittime, *timeout;
	timeout = &waittime;
	timeout->tv_sec = nsecs;
	timeout->tv_usec = 0;
	
	cmd = ':';	/* ready test command */
	write(fd, &cmd, 1);
	readfds = 1<<fd;
	select(fd+1, &readfds, (int *)0, (int *)0, timeout);
	if ((readfds & (1<<fd)) != 0) {
		mread(fd, status, 2);
		if((status[0]&0x7f) == 'R')
			return 1;	/* camera is ready */
	}
	return 0;	/* camera is not ready or timeout after n secs */
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
	readfds = 1<<fd;
	select(fd+1, &readfds, (int *)0, (int *)0, &waittime);
	if( (readfds&(1<<fd)) != 0) {
		mread(fd, values, 20);
		values[20] = '\0';
		printf("dunncolor: %s = %s\n", title, values);
	} else {
		printf("dunncolor:\007 %s request exposure value cmd: timed out\n", title);
		exit(40);
	}
}

/*
 *			S E N D
 *
 */
void
send(color,val)
char color;
int val;
{
	int digit;

	if(val < 0 || val > 255) {
		printf("dunncolor: bad value %d\n",val);
		exit(75);
	}

	if(!ready(20)) {
		printf("dunncolor: 80 camera not ready\n");
		exit(80);
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
}
