/*	This is a public-domain reimplementation of strtok(3).
	Not derived from licensed software.
	This code may be used on any computer system for any
	purpose by anyone.

	Henry Spencer @ U of Toronto Zoology
	{allegra,ihnp4,decvax,pyramid}!utzoo!henry
	Wed Jun 25 19:21:34 EDT 1986
*/

#include "common.h"

/* quell empty-compilation unit warnings */
static const int unused = 0;

/*
 *  This public domain version of strtok() made available to systems that
 *  don't have their own.
 */
#ifndef HAVE_STRTOK


static char *scanpoint = 0;

/**
 * Get next token from string s (0 on 2nd, 3rd, etc. calls),
 * where tokens are nonempty strings separated by runs of
 * chars from delim.  Writes NULs into s to end tokens.  delim need not
 * remain constant from call to call.
 */
char *				/* 0 if no token left */
strtok(char *s, register const char *delim)
{
    register char *scan;
    char *tok;
    register const char *dscan;
    if (s == (char*)0 && scanpoint == (char*)0)
	return((char*)0);
    if (s != (char*)0)
	scan = s;
    else
	scan = scanpoint;

    /*
     * Scan leading delimiters.
     */
    for (; *scan != '\0'; scan++) {
	for (dscan = delim; *dscan != '\0'; dscan++)
	    if (*scan == *dscan)
		break;
	if (*dscan == '\0')
	    break;
    }
    if (*scan == '\0') {
	scanpoint = (char*)0;
	return((char*)0);
    }

    tok = scan;

    /*
     * Scan token.
     */
    for (; *scan != '\0'; scan++) {
	for (dscan = delim; *dscan != '\0';)	/* ++ moved down. */
	    if (*scan == *dscan++) {
		scanpoint = scan+1;
		*scan = '\0';
		return(tok);
	    }
    }

    /*
     * Reached end of string.
     */
    scanpoint = (char*)0;
    return(tok);
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
