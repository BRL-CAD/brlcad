#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
extern int Debug;
extern double Beta;
/*	sharpen	an image.
 *
 * Entry:
 *	buf	- buffer to place bytes.
 *	size	- size of element
 *	num	- number of elements to read.
 *	file	- file to read from.
 *	Map	- tone scale mapping.
 *
 * Exit:
 *	returns pointer to buf or NULL
 *
 * Uses:
 *	Debug	- Current debug level.
 *	Beta	- sharpening value.
 *
 * Calls:
 *	Random	- return a random number between -0.5 and 0.5;
 *
 * Method:
 *	straight-forward.
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * 
 */
int
sharpen(buf,size,num,file,Map)
unsigned char *buf;
int  size;
int  num;
FILE *file;
unsigned char *Map;
{
	static unsigned char *last=0,*cur,*next;
	static int linelen;
	int result;

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
	if (!last) {
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
 *	if cur is NULL then we are at EOF.
 */
	if (!cur) return(0);

	if (!last) {
		i=0;
		value=next[i] + cur[i+1] - cur[i]*2;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*2);
		for (; i < linelen-1; i++) {
			value = next[i] + cur[i-1] + cur[i+1] - cur[i]*3;
			buf[i] = cur[i] - Beta*value*cur[i]/(255*3);
		}
		value=next[i] + cur[i-1] - cur[i]*2;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*2);
		last  = (unsigned char *)malloc(linelen);
	} else if (!next) {
		i=0;
		value=last[i] + cur[i+1] - cur[i]*2;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*2);
		for (; i < linelen-1; i++) {
			value = last[i] + cur[i-1] + cur[i+1] - cur[i]*3;
			buf[i] = cur[i] - Beta*value*cur[i]/(255*3);
		}
		value=last[i] + cur[i-1] - cur[i]*2;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*2);
	} else {
		i=0;
		value=last[i] +next[i] + cur[i+1] - cur[i]*3;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*3);
		for (; i < linelen-1; i++) {
			value = last[i] + next[i] + cur[i-1] + cur[i+1]
			     - cur[i]*4;
			buf[i] = cur[i] - Beta*value*cur[i]/(255*4);
		}
		value=last[i] + next[i] + cur[i-1] - cur[i]*3;
		buf[i] = cur[i] - Beta*value*cur[i]/(255*3);
	}
	return(linelen);
}
