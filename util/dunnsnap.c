/*
 *			D U N N S N A P . C
 *
 *	Checks status of the Dunn camera and exposes the number of frames
 *	of film specified in the argument (default is 1 frame).
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
#include <sgtty.h>

#ifdef BSD
#include <sys/time.h>
#include <sys/file.h>
#endif

struct	sgttyb	tty;
int fd;
char cmd;
unsigned char status[4];
int readfds, xcptfd;

main(argc, argv)
int argc;
char **argv;
{
	int nframes;


	/* open the camera device */

	if( (fd = open("/dev/camera", O_RDWR | O_NDELAY)) < 0 
	     || ioctl(fd, TIOCGETP, &tty) < 0) {
	     	printf("\007dunnsnap: can't open /dev/camera\n");
		close(fd);
		exit(10);
	}
	
	/* set up the camera device */

	tty.sg_ispeed = tty.sg_ospeed = B9600;
	tty.sg_flags = RAW | EVENP | ODDP | XTABS;
	if( ioctl(fd,TIOCSETP,&tty) < 0 ) {
		printf("\007dunnsnap: error on /dev/camera setup\n");
		exit(20);
	}

	/* check argument */

	if ( argc > 1) 
		nframes = atoi(*++argv);
	else
		nframes = 1;

	if (!ready(1)) {
		printf("camera not ready\n");
		exit(30);
	}
		
	/* loop until number of frames specified have been exposed */

	while (nframes) {

		while (!ready(20)) {
			printf("camera not ready\n");
			exit(40);
		}

		if (!goodstatus()) {
			printf("badstatus\n");
			exit(50);
		}
		
		/* send expose command to camera */

			cmd = 'I';	/* expose command */
			write(fd, &cmd, 1);
			hangten();
			if (!ready(20)) {
				printf("camera not ready\n");
				exit(60);
			}
		--nframes;
	}
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
	select(fd+1, &readfds, (int *)0, &xcptfd, timeout);
	if( (readfds & (1<<fd)) !=0) {
		mread(fd, status, 4);
		if ((status[0]&0xf) == 0 &&
		    (status[1]&0xf) == 0x8 &&
		    (status[2]&0x3) == 0x3)
			return 1;	/* status is ok */
		else {
			printf("\007dunnsnap: status error from camera\n");
			printf("status[0]= 0x%x [1]= 0x%x [2]= 0x%x [3]= 0x%x\n",status[0]&0xf,status[1]&0xf,
				status[2]&0x3,status[3]&0x7f);
		}
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
	select(fd+1, &readfds, (int *)0, &xcptfd, timeout);
	if ((readfds & (1<<fd)) != 0) {
		mread(fd, status, 2);
		if((status[0]&0x7f) == 'R')
			return 1;	/* camera is ready */
	}
	return 0;	/* camera is not ready or timeout after n secs */
}
