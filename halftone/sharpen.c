#ifndef lint
static const char RCSid[] = "$Header$";
#endif
#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For malloc */

extern int Debug;
extern double Beta;
/*	sharpen	- return a sharpened tone mapped buffer.
 *
 * Entry:
 *	buf	- buffer to place bytes.
 *	size	- size of element
 *	num	- number of elements to read.
 *	file	- file to read from.
 *	Map	- tone scale mapping.
 *
 * Exit:
 *	returns 0 on EOF
 *		number of byes read otherwise.
 *
 * Uses:
 *	Debug	- Current debug level.
 *	Beta	- sharpening value.
 *
 * Calls:
 *	None.
 *
 * Method:
 *	if no sharpening just read a line tone scale and return.
 *	if first time called get space for processing and do setup.
 *	if first line then
 *		only use cur and next lines
 *	else if last line then
 *		only use cur and last lines
 *	else
 *		use last cur and next lines
 *	endif
 *
 * Author:
 *	Christopher T. Johnson
 *
 * $Log$
 * Revision 11.5.2.1  2002/09/19 18:00:58  morrison
 * Initial ANSIfication
 *
 * Revision 11.5  2002/08/20 17:07:30  jra
 * Restoration of entire source tree to Pre-Hartley state
 *
 * Revision 11.3  2000/08/24 23:09:00  mike
 *
 * lint
 *
 * Revision 11.2  2000/08/24 23:07:42  mike
 *
 * lint
 *
 * Revision 11.1  1995/01/04  10:21:52  mike
 * Release_4.4
 *
 * Revision 10.2  94/08/23  17:59:09  gdurf
 * Added includes of conf.h, machine.h, externs.h for malloc
 * 
 * Revision 10.1  1991/10/12  06:53:19  mike
 * Release_4.0
 *
 * Revision 2.4  91/08/30  00:26:43  mike
 * Stardent ANSI C
 * 
 * Revision 2.3  91/01/03  23:01:09  cjohnson
 * Added range checking, to limit values to 0..255
 * 
 * Revision 2.2  90/04/13  03:09:03  cjohnson
 * Back off code to reduce number of floating point multiblies.
 * 
 * Revision 2.1  90/04/13  01:23:12  cjohnson
 * First Relese.
 * 
 * Revision 1.2  90/04/13  00:46:54  cjohnson
 * Clean up comments.
 * Fix error on second line.
 * 
 * Revision 1.1  90/04/13  00:31:49  cjohnson
 * Initial revision
 * 
 * 
 */
int
sharpen(unsigned char *buf, int size, int num, FILE *file, unsigned char *Map)
{
	static unsigned char *last,*cur=0,*next;
	static int linelen;
	int result;
	register int newvalue;
	register  int i,value;

/*
 *	if no sharpening going on then just read from the file and exit.
 */
	if (Beta == 0.0) {
		result = fread(buf, size, num, file);
		if (!result) return(result);
		for (i=0; i<size*num; i++) {
			buf[i] = Map[buf[i]];
		}
		return(result);
	}

/*
 *	if we are sharpening we depend on the pixel size being one char.
 */
	if (size != 1) {
		fprintf(stderr, "sharpen: size != 1.\n");
		exit(2);
	}

/*
 *	if this is the first time we have been called then get some
 *	space to load pixels into and read first and second line of
 *	the file.
 */
	if (!cur) {
		linelen=num;
		last = 0;
		cur  = (unsigned char *)malloc(linelen);
		next = (unsigned char *)malloc(linelen);
		result = fread(cur, 1, linelen, file);
		for (i=0; i<linelen;i++) cur[i] = Map[cur[i]];
		if (!result) return(result);	/* nothing there! */
		result = fread(next, 1, linelen, file);
		if (!result) {
			free(next);
			next = 0;
		} else {
			for (i=0; i<linelen;i++) cur[i] = Map[cur[i]];
		}
	} else {
		unsigned char *tmp;

		if (linelen != num) {
			fprintf(stderr,"sharpen: buffer size changed!\n");
			exit(2);
		}
/*
 *		rotate the buffers.
 */
		tmp=last;
		last=cur;
		cur=next;
		next=tmp;
		result=fread(next, 1, linelen, file);
/*
 *		if at EOF then free up a buffer and set next to NULL
 *		to flag EOF for the next time we are called.
 */
		if (!result) {
			free(next);
			next = 0;
		} else {
			for (i=0; i<linelen;i++) cur[i] = Map[cur[i]];
		}
	}
/*
 *	if cur is NULL then we are at EOF.  Memory leak here as
 *	we don't free last.
 */
	if (!cur) return(0);

/*
 * Value is the following Laplacian filter kernel:
 *		0.25
 *	0.25	-1.0	0.25
 *		0.25
 *
 * Thus value is zero in constant value areas and none zero on
 * edges.
 *
 * Page 335 of Digital Halftoning defines
 *	J     [n] = J[n] - Beta*Laplacian_filter[n]*J[n]
 *	 sharp
 *
 * J, J     , Laplacian_filter[n] are all in the range 0.0 to 1.0
 *     sharp
 *
 * The folowing is done in mostly interger mode, there is only one
 * floating point multiply done.
 */

/*
 *	if first line
 */
	if (!last) {
		i=0;
		value=next[i] + cur[i+1] - cur[i]*2;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;
		for (; i < linelen-1; i++) {
			value = next[i] + cur[i-1] + cur[i+1] - cur[i]*3;
			newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
			buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
			    255: newvalue;
		}
		value=next[i] + cur[i-1] - cur[i]*2;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;

/*
 *		first time through so we will need this buffer space
 *		the next time through.
 */
		last  = (unsigned char *)malloc(linelen);
/*
 *	if last line
 */
	} else if (!next) {
		i=0;
		value=last[i] + cur[i+1] - cur[i]*2;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;
		for (; i < linelen-1; i++) {
			value = last[i] + cur[i-1] + cur[i+1] - cur[i]*3;
			newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
			buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
			    255: newvalue;
		}
		value=last[i] + cur[i-1] - cur[i]*2;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*2);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;
/*
 *	all other lines.
 */
	} else {
		i=0;
		value=last[i] + next[i] + cur[i+1] - cur[i]*3;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;
		for (; i < linelen-1; i++) {
			value = last[i] + next[i] + cur[i-1] + cur[i+1]
			     - cur[i]*4;
			newvalue = cur[i] - Beta*value*((int)cur[i])/(255*4);
			buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
			    255: newvalue;
		}
		value=last[i] + next[i] + cur[i-1] - cur[i]*3;
		newvalue = cur[i] - Beta*value*((int)cur[i])/(255*3);
		buf[i] = (newvalue < 0) ? 0 : (newvalue > 255) ? 
		    255: newvalue;
	}
	return(linelen);
}
