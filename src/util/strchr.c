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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
