/*	@(#)strchr.c	1.2	*/
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#define NULL	(char *)0
/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */
char *
strchr(register char *sp, register char c)
{
	do {
		if(*sp == c)
			return(sp);
	} while(*sp++);
	return(NULL);
}
