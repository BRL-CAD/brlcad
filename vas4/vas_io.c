/*
 * vas_io.c - I/O routines to talk to the VAS IV
 */


#include <stdio.h>
#include <sgtty.h>

#define RW 2
struct sgttyb user, vtty;

#include "vas4.h"

#define VAS_PORT	"/dev/vas"
#define BAUD		B300

int vas_fd;
int debug = TRUE;


/* #define DEBUG 1 /* Define to simulate VAS with stdio */

/*
 * vas_open - attach to the VAS serial line
 *
 *	return a file descriptor of NULL on error
 */

vas_open()
{

#if DEBUG
#else
	/* Open VAS Port */
	if((vas_fd=open(VAS_PORT,RW)) == EOF){
		fprintf(stderr,"Can not open VAS port");
		fprintf(stderr,VAS_PORT);
		fprintf(stderr,"\n");
		exit(1);
	}

	/* Setup VAS line */
	vtty.sg_ispeed = BAUD;
	vtty.sg_ospeed = BAUD;
	vtty.sg_flags = RAW+EVENP+ODDP;
	ioctl(vas_fd,TIOCSETP,&vtty);
	ioctl(vas_fd,TIOCEXCL,&vtty);	/* exclusive use */
#endif
}


/* Output the specified character to the VAS */

vas_putc(c)
char c;
{
	int n_written;

#ifdef DEBUG
	vas_fd = 1;
#endif
	n_written = write(vas_fd, &c, 1);
	if (n_written != 1)
		fprintf(stderr,"VAS write error\n");
fprintf(stderr,"<%c>",c);
	return(n_written);

}

/* Output a string to the VAS. (The string must be NULL terminated) */

vas_puts(s)
char *s;
{
	while (*s != NULL ) {
		vas_putc(*s++);
	}
}


/* Read a single character from the VAS, return EOF on error */

vas_getc()
{
	char c;

#ifdef DEBUG
	vas_fd = 2;
#endif

	if (read(vas_fd, &c, 1) > 0)
		return(c & 0377);
	else
		return(EOF);
}


vas_close()
{
#ifdef DEBUG
#else
	close(vas_fd);
#endif

}


/*
   vas_putc_wait - Like putc, but also wait for a specified response

   send		- the character to send to VAS
   expect 	- is the character expected
*/

vas_putc_wait(send, expect)
char send, expect;
{

	char c;

	vas_putc(send);

	while (1) {
		if (debug) {
			fprintf(stderr,"Send '%c', Expect '%c' ",send,expect);
		}
		c = vas_getc();
		if (debug)
			fprintf(stderr,"Got '%c'\n",c);
		if (c == expect)
			break;
	}
}


#ifdef MAIN

main()
{
	int c;

	vas_puts("hello\n");

	vas_open();

	vas_putc('I');

	fprintf(stderr,"Waiting for VAS response\n");
	c = vas_getc();

	printf("octal: '%o'  char: '%c'\n",c,c);
}
#endif

	
