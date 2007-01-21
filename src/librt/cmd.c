/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup ray */
/** @{ */
/** @file librt/cmd.c
 *
 *  Read and parse a viewpoint-control command stream.
 *  This module is intended to be common to all programs which
 *  read this type of command stream;  the routines to handle
 *  the various keywords should go in application-specific modules.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/** @} */

#ifndef lint
static const char RCScmd[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

/*
 *			R T _ R E A D _ C M D
 *
 *  Read one semi-colon terminated string of arbitrary length from
 *  the given file into a dynamicly allocated buffer.
 *  Various commenting and escaping conventions are implemented here.
 *
 *  Returns:
 *	NULL	On EOF
 *	char *	On good read
 */
char *
rt_read_cmd(register FILE *fp)
{
	register int	c;
	register char	*buf;
	register int	curpos;
	register int	curlen;

	curpos = 0;
	curlen = 400;
	buf = bu_malloc( curlen, "rt_read_cmd command buffer" );

	do  {
		c = fgetc(fp);
		if( c == EOF )  {
			c = '\0';
		} else if( c == '#' )  {
			/* All comments run to the end of the line */
			while( (c = fgetc(fp)) != EOF && c != '\n' )  ;
			continue;
		} else if( c == '\n' )  {
			c = ' ';
		} else if( c == ';' )  {
			c = '\0';
		} else if( c == '\\' )  {
			/*  Backslash takes next character literally.
			 *  EOF detection here is not a problem, next
			 *  pass will detect it.
			 */
			c = fgetc(fp);
		} else if( !isascii(c) )  {
			c = '?';
		}
		if( c != '\0' && curpos == 0 && isspace(c) )  {
			/*  Dispose of leading white space.
			 *  Necessary to slurp up what newlines turn into.
			 */
			continue;
		}
		if( curpos >= curlen )  {
			curlen *= 2;
			buf = bu_realloc( buf, curlen, "rt_read_cmd command buffer" );
		}
		buf[curpos++] = c;
	} while( c != '\0' );
	if( curpos <= 1 )  {
		bu_free( buf, "rt_read_cmd command buffer (EOF)" );
		return( (char *)0 );		/* EOF */
	}
	return( buf );				/* OK */
}

#define MAXWORDS	4096	/* Max # of args per command */

/*
 *			R T _ S P L I T _ C M D
 *
 *  Build argv[] array from input buffer, by splitting whitespace
 *  separated "words" into null terminated strings.
 *  The input buffer is altered by this process.
 *
 *  Returns -
 *	 0	no words in input
 *	nwords	number of words of input, now in argv[]
 */
int
rt_split_cmd(char **argv, int lim, register char *lp)
{
	register int	nwords;			/* number of words seen */
	register char	*lp1;

	argv[0] = "_NIL_";		/* sanity */

	while( *lp != '\0' && isspace( *lp ) )
		lp++;

	if( *lp == '\0' )
		return(0);		/* No words */

#ifdef HAVE_SHELL_ESCAPE
	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		int	ret;
		ret = system( lp+1 );
		if( ret != 0 )  {
			perror("system(3)");
			bu_log("rt_split_cmd() FAILED: !%s\n", lp);
		}
		return(0);		/* No words */
	}
#endif

	/* some non-space string has been seen, argv[0] is set */
	nwords = 1;
	argv[0] = lp;

	for( ; *lp != '\0'; lp++ )  {
		if( !isspace( *lp ) )
			continue;	/* skip over current word */

		*lp = '\0';		/* terminate current word */
		lp1 = lp + 1;
		if( *lp1 != '\0' && !isspace( *lp1 ) )  {
			/* Begin next word */
			if( nwords >= lim-1 )
				break;	/* argv[] full */

			argv[nwords++] = lp1;
		}
	}
	argv[nwords] = (char *)0;	/* safety */
	return( nwords );
}

/*
 *			R T _ D O _ C M D
 *
 *  Slice up input buffer into whitespace separated "words",
 *  look up the first word as a command, and if it has the
 *  correct number of args, call that function.
 *
 *  Expected to return -1 to halt command processing loop.
 *
 *  Based heavily on mged/cmd.c by Chuck Kennedy.
 */
int
rt_do_cmd(struct rt_i *rtip, const char *ilp, register const struct command_tab *tp)
           		      			/* FUTURE:  for globbing */


{
	register int	nwords;			/* number of words seen */
	char		*cmd_args[MAXWORDS+1];	/* array of ptrs to args */
	char 		*lp;
	int		retval;

	lp = bu_malloc(strlen(ilp)+1, "rt_do_cmd lp");
	strcpy(lp, ilp);

	nwords = rt_split_cmd( cmd_args, MAXWORDS, lp );
	if( nwords <= 0 )
		return(0);	/* No command to process */


	for( ; tp->ct_cmd != (char *)0; tp++ )  {
		if( cmd_args[0][0] != tp->ct_cmd[0] ||
				/* the length of "n" is not significant, just needs to be big enough */
		    strncmp( cmd_args[0], tp->ct_cmd, MAXWORDS ) != 0 )
			continue;
		if( (nwords >= tp->ct_min) && (nwords <= tp->ct_max) ) {
		    retval = tp->ct_func( nwords, cmd_args );
		    bu_free(lp, "rt_do_cmd lp");
		    return retval;
		}
		bu_log("rt_do_cmd Usage: %s %s\n\t%s\n",
			tp->ct_cmd, tp->ct_parms, tp->ct_comment );
		bu_free(lp, "rt_do_cmd lp");
		return(-1);		/* ERROR */
	}
	bu_log("rt_do_cmd(%s):  command not found\n", cmd_args[0]);
	bu_free(lp, "rt_do_cmd lp");
	return(-1);			/* ERROR */
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
