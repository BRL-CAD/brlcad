/*
 * vas_io.c - I/O routines to talk to the VAS IV
 */


#include <stdio.h>
#include <sgtty.h>

#define RW 2
struct sgttyb user, vtty;

#include "./vas4.h"

#define VAS_PORT	"/dev/vas"
#define BAUD		B300

int vas_fd;
int debug = TRUE;

/*
 * vas_open - attach to the VAS serial line
 *
 *	return a file descriptor of NULL on error
 */

vas_open()
{

	/* Open VAS Port */
	if((vas_fd=open(VAS_PORT,RW)) < 0){
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
}

/*
 *			V A S _ R A W P U T C
 *
 *  Defeat the (typically desirable) ack/rexmit behavior of the
 *  vas_putc() routine.  Useful mostly for the 'V' (get_vtr_status)
 *  command, and the 'A' (send current activity) commands.
 */
vas_rawputc(c)
char c;
{
	int	got;

	got = write(vas_fd, &c, 1);
	if (got != 1)  {
		perror("VAS Write");
		return(got);
		/* Error recovery?? */
	}
	if(debug) fprintf(stderr,"vas_rawputc 0%o '%c'\n",c,c);

}

/*
 *			V A S _ P U T C
 *
 *  Output the specified character to the VAS.
 *  Wait for a 006 (^F) ACK of this character, 
 *  or for an "activity state" character,
 *  else retransmit.
 */
vas_putc(c)
char c;
{
	int	got;
	int	reply;

retry:
	got = write(vas_fd, &c, 1);
	if (got != 1)  {
		perror("VAS Write");
		return(got);
		/* Error recovery?? */
	}
	if(debug) fprintf(stderr,"vas_putc 0%o '%c'\n",c,c);

	reply=vas_getc();
	if( reply == 006 )
		return(got);
	if( reply >= 0x60 && reply <= 0x78 )  {
		vas_response(reply);
		return(got);
	}
	fprintf(stderr,"VAS -- no ACK rcvd!!\n");
	vas_response(reply);
	sleep(1);
	goto retry;
}

/*
 *			V A S _ P U T S
 *
 *  Output a null terminated string to the VAS.
 */
vas_puts(s)
char *s;
{
	while (*s != NULL ) {
		vas_putc(*s++);
	}
}

/*
 *			V A S _ P U T N U M
 *
 *  Output a number in decimal to the VAS.
 */
vas_putnum(n)
int	n;
{
	char	buf[32];

	sprintf(buf,"%d",n);
	vas_puts(buf);
}


/* Read a single character from the VAS, return EOF on error */

vas_getc()
{
	char c;

	if (read(vas_fd, &c, 1) > 0)
		return(c & 0377);
	else
		return(EOF);
}


vas_close()
{
	close(vas_fd);
	vas_fd = -1;
}


/*
   vas_putc_wait - Like putc, but also wait for a specified response

   send		- the character to send to VAS
   expect 	- is the character expected
 *
 *  This routine has the dubious property that it will continue to
 *  reissue the command character until it hears the response
 *  character.  Considering the large number of "asynchronous"
 *  replies of an advisory nature, this seems unwise.
 */
vas_putc_wait(send, expect)
char send, expect;
{
	char c;

	vas_putc(send);

	while (1) {
		if (debug) {
			fprintf(stderr,"Sent '%c', Expect '%c'\n",send,expect);
		}
		c = vas_getc();
		if (debug)  vas_response(c);
		if (c == expect)
			break;
	}
}

/*
 *			V A S _ A W A I T
 *
 *  Slurp up input characters, until designated one comes along.
 *  If too much time has been spent waiting, we should consider
 *  some way of bailing out.  Error recovery needs more attention.
 *
 *  Returns -
 *	0	all is well
 *	-1	failure
 */
vas_await(c, sec)
int	c;
int	sec;
{
	int	reply;
	int	count;

	for(count=0; count<20; count++)  {
		reply = vas_getc();
		if(debug) vas_response(reply);
		if( reply == c )  return(0);	/* OK */
	}
	return(-1);			/* BAD:  too many bad chars */
}

/*
 *			V A S _ R E S P O N S E
 *
 *  Attempt to interpret a reply as something sensible.
 *  This may not work in all cases (such as a multi-character response),
 *  but certainly beats looking at single-character codes.
 */
vas_response(c)
char	c;
{
	fprintf(stderr,"---Got 0%o '%c' ", c, c);
	switch(c)  {
	case 6:
		fprintf(stderr,"last command accepted\n");
		break;
	case 7:
		fprintf(stderr,"***Command ignored at current activity level\n");
		break;
	case 'I':
		fprintf(stderr,"Initialized.  Controller is ready for operation\n");
		break;
	case 'P':
		fprintf(stderr,"Program cmd accepted\n");
		break;
	case 'F':
		fprintf(stderr,"Frame rate cmd accepted\n");
		break;
	case 'E':
		fprintf(stderr,"Update cmd accepted\n");
		break;
	case 'U':
		fprintf(stderr,"Update cmd accepted\n");
		break;
	case 'S':
		fprintf(stderr,"Search command accepted, ready for E/E\n");
		break;
	case 'W':
		fprintf(stderr,"After E/E, search began, scene is not correct\n");
		break;
	case 'B':
		fprintf(stderr,"After E/E, search began, frame code lost while checking scene\n");
		break;
	case 'N':
		fprintf(stderr,"After E/E, search for frame fails (preceding frame not found)\n");
		break;
	case 'R':
		fprintf(stderr,"Ready to accept Record command\n");
		break;
	case 'M':
		fprintf(stderr,"Preroll fails after Record cmd, backspace for retry begins\n");
		break;
	case 'X':
		fprintf(stderr,"Notice:  2 frames before cut-in\n");
		break;
	case 'Y':
		fprintf(stderr,"Notice:  2 frames before cut-out\n");
		break;
	case 'D':
		fprintf(stderr,"Recording done, starting backspacing for next preroll\n");
		break;
	case 'J':
		fprintf(stderr,"Jaunt:  standby timeout;  tape moving back to Title\n");
		break;
	case 'T':
		fprintf(stderr,"Trash:  recording interrupted by STOP\n");
		break;
	case 'Q':
		fprintf(stderr,"Quit:  Ending EDIT mode\n");
		break;
	case 'L':
		fprintf(stderr,"Located sought frame\n");
		break;
	case 'K':
		fprintf(stderr,"Knave:  Search-for-frame failed\n");
		break;

	/**** Current activity states ****/
	/**** These are all suspect, as they don't match the BRL manual ***/
	case '`':
		fprintf(stderr,"Idling:  Power-on condition or newly initialized\n");
		break;
	case 'a':
		fprintf(stderr,"Register function is active\n");
		break;
	case 'b':
		fprintf(stderr,"Accepting programming for a recording\n");
		break;
	case 'c':
		fprintf(stderr,"Accepting programming for an edit recording\n");
		break;
	case 'd':
		fprintf(stderr,"Flashing E/E switch; ready to search for frame\n");
		break;
	case 'e':
		fprintf(stderr,"Checking for position on correct scene\n");
		break;
	case 'f':
		fprintf(stderr,"Ready to record next recording or TITLE\n");
		break;
	case 'g':
		fprintf(stderr,"Prerolling, about to make recording\n");
		break;
	case 'h':
		fprintf(stderr,"Recording in progress\n");
		break;
	case 'i':
		fprintf(stderr,"Backspacing for next preroll and recording\n");
		break;
	case 'j':
		fprintf(stderr,"Searching for frame preceding next to record\n");
		break;
	case 'k':
		fprintf(stderr,"Accepting programming for Frame Change\n");
		break;
	case 'l':
		fprintf(stderr,"Accepting programming for HOLD\n");
		break;
	case 'm':
		fprintf(stderr,"Displaying a warning message\n");
		break;
	case 'n':
		fprintf(stderr,"Ready to record first recording on old scene\n");
		break;
	case 'o':
		fprintf(stderr,"Holding momentarily before allowing to RECORD\n");
		break;

	default:
		fprintf(stderr,"???unknown???\n");
		break;
	}
}
