/*
 *			D U N N C O L O R . C				
 *
 *	Sets the AUX exposure values in the Dunn camera to the
 *	specified arguments.
 *
 *	dunncolor baseval redval greenval blueval
 *
 *	Don Merritt
 *	August 1985
 *
 */


#include <sys/time.h>
#include <sgtty.h>
#include <sys/file.h>
struct	sgttyb	tty;
int fd;
char cmd;
unsigned char status[4], values[21];
int readfds, xcptfd;

main(argc, argv)
int argc;
char **argv;
{
	struct timeval waittime, *timeout;
	timeout = &waittime;
	timeout->tv_sec = 20;
	timeout->tv_usec = 0;


	/* open the camera device */

	if( (fd = open("/dev/camera", O_RDWR | O_NDELAY)) < 0 
	     || ioctl(fd, TIOCGETP, &tty) < 0) {
	     	printf("\007dunncolor: can't open /dev/camera\n");
		close(fd);
		exit(10);
	}
	
	/* set up the camera device */

	tty.sg_ispeed = tty.sg_ospeed = B9600;
	tty.sg_flags = RAW | EVENP | ODDP | XTABS;
	if( ioctl(fd,TIOCSETP,&tty) < 0 ) {
		printf("\007dunncolor: error on /dev/camera setup\n");
		exit(20);
	}

	/* check argument */

	if ( argc != 5) {
		printf("usage: dunncolor baseval redval greenval blueval\n"); 
		exit(25);
	}


	if (!ready(1)) {
		printf("dunncolor: 30 camera not ready\n");
		exit(30);
	}
		
	cmd = '=';	/* request AUX exposure values command */
	write(fd, &cmd, 1);
	readfds = 1<<fd;
	select(fd+1, &readfds, (int *)0, &xcptfd, timeout);
	if( (readfds&(1<<fd)) != 0) {
		mread(fd, values, 20);
		values[20] = '\0';
		printf("dunncolor: current values= %s\n",values);
	}
	else {
		printf("dunncolor:\007request values cmd timed out\n");
		exit(40);
	}


	if(!ready(20)) {
		printf("dunncolor: 50 camera not ready\n");
		exit(50);
	}

	send('A',atoi(*++argv));
	send('R',atoi(*++argv));
	send('G',atoi(*++argv));
	send('B',atoi(*++argv));

	if(!ready(20)) {
		printf("dunncolor: 60 camera not ready\n");
		exit(60);
	}

	cmd = '=';	/* request AUX exposure values command */
	write(fd, &cmd, 1);
	readfds = 1<<fd;
	select(fd+1, &readfds, (int *)0, &xcptfd, timeout);
	if( (readfds&(1<<fd)) != 0) {
		mread(fd, values, 20);
		values[20] = '\0';
		printf("dunncolor: new values= %s\n",values);
	}
	else {
		printf("dunncolor:\007request values cmd timed out\n");
		exit(70);
	}
}

/*
 *			S E N D
 *
 */
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
			printf("\007dunncolor: status error from camera\n");
			printf("status[0]= 0x%x [1]= 0x%x [2]= 0x%x [3]= 0x%x\n",status[0]&0xf,status[1]&0xf,
				status[2]&0x3,status[3]&0x7f);
		}
	} else
		printf("\007dunncolor: status request timed out\n");
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
