/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * cmd_name.c - Extract command name from argv[0].
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Wed Jun 27 1990
 * Copyright (c) 1990, University of Michigan
 */

static char no_name[] = "(no-name)";

char *
cmd_name( argv )
char **argv;
{
    register char *cp, *a;

    /* Be paranoid. */
    if ( !argv || !(a = *argv) )
	return no_name;

    /* Find end of file name. */
    for ( cp = a; *cp; cp++ )
	;

    /* Find last / or beginning of command name. */
    for ( cp--; *cp != '/' && cp > a; cp-- )
	;
    
    /* If it's a /, skip it. */
    if ( *cp == '/' )
	cp++;

    return cp;
}
