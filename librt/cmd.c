/*
 *			C M D . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCScloud[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>

/*
 *			R E A D _ C M D
 *
 *  Returns -1 on EOF, 0 on good read.
 */
read_cmd( fp, buf, len )
register FILE	*fp;
char	*buf;
int	len;
{
	register int	c;
	register char	*pos;
	register char	*epos;

	pos = buf;
	epos = buf+len;
	*pos = '\0';

	while( (c = fgetc(fp)) != EOF )  {
		/* All comments run to the end of the line */
		if( c == '#' )  {
			while( (c = fgetc(fp)) != EOF && c != '\n' )  ;
			continue;
		}
		if( c == '\n' )  {
			*pos++ = ' ';
			if( pos >= epos )  {
				fprintf(stderr, "read_cmd:  buffer overrun\n");
				return(-1);	/* EOF */
			}
			continue;
		}
		if( c == ';' )  {
			*pos++ = '\0';
			return(0);	/* OK */
		}
		if( !isascii(c) )  {
			fprintf(stderr, "read_cmd:  non-ASCII char read\n");
			return(-1);	/* EOF */
		}
		*pos++ = c;
		if( pos >= epos )  {
			fprintf(stderr, "read_cmd:  buffer overrun\n");
			return(-1);	/* EOF */
		}
	}
	if( pos > buf )  {
		*pos++ = '\0';
		return(0);		/* OK */
	}
	return(-1);			/* EOF */
}

extern int	cm_start();
extern int	cm_vsize();
extern int	cm_eyept();
extern int	cm_vrot();
extern int	cm_end();
extern int	cm_multiview();
extern int	cm_anim();
extern int	cm_tree();
extern int	cm_clean();
extern int	cm_set();

#define MAXWORDS		32	/* Maximum number of args per command */

struct cmd_tab {
	char	*ct_cmd;
	char	*ct_parms;
	int	(*ct_func)();
	int	ct_min;		/* min number of words in cmd */
	int	ct_max;		/* max number of words in cmd */
};
static struct cmd_tab cmdtab[] = {
	"start",	"frame number",	cm_start,	2, 2,
	"viewsize",	"size in mm",	cm_vsize,	2, 2,
	"eye_pt",	"xyz of eye",	cm_eyept,	4, 4,
	"viewrot",	"4x4 matrix",	cm_vrot,	17,17,
	"end",		"",		cm_end,		1, 1,
	"multiview",	"",		cm_multiview,	1, 1,
	"anim",		"path type args", cm_anim,	4, MAXWORDS,
	"tree",		"treetop(s)",	cm_tree,	1, MAXWORDS,
	"clean",	"",		cm_clean,	1, 1,
	"set",		"",		cm_set,		1, MAXWORDS,
	(char *)0,	(char *)0,	0,		0, 0	/* END */
};

/*
 *			D O _ C M D
 *
 *  Slice up input buffer into whitespace separated "words",
 *  look up the first word as a command, and if it has the
 *  correct number of args, call that function.
 *  The input buffer is altered in the process.
 *
 *  Expected to return -1 to halt command processing loop.
 *
 *  Based heavily on mged/cmd.c by Chuck Kennedy.
 */
int
do_cmd( lp )
register char *lp;
{
	register int	nwords;			/* number of words seen */
	register struct cmd_tab *tp;
	char		*cmd_args[MAXWORDS+1];	/* array of ptrs to args */

	nwords = 0;
	cmd_args[0] = lp;

	if( *lp == '\n' )
		return(0);		/* No command */

	/* Handle "!" shell escape char so the shell can parse the line */
	if( *lp == '!' )  {
		(void)system( lp+1 );
		return(0);		/* No command */
	}

	if( *lp != '\0' && !isspace( *lp ) )
		nwords++;		/* some arg will be seen, [0] set */

	for( ; *lp != '\0'; lp++ )  {
		register char	*lp1;

		if( !isspace( *lp ) )
			continue;	/* skip over current word */

		*lp = '\0';		/* terminate current word */
		lp1 = lp + 1;
		if( *lp1 != '\0' && !isspace( *lp1 ) )  {
			/* Begin next word */
			if( nwords >= MAXWORDS )  {
				(void)fprintf(stderr,
					"rt: More than %d arguments, excess flushed\n",
					MAXWORDS);
				cmd_args[MAXWORDS] = (char *)0;
				return(-1);	/* ERROR */
			}
			cmd_args[nwords++] = lp1;
		}
	}
	cmd_args[nwords] = (char *)0;	/* safety */

	if( nwords <= 0 )
		return(0);	/* No command to process */

	for( tp = cmdtab; tp->ct_cmd != (char *)0; tp++ )  {
		if( cmd_args[0][0] != tp->ct_cmd[0] ||
		    strcmp( cmd_args[0], tp->ct_cmd ) != 0 )
			continue;
		if( (nwords >= tp->ct_min) &&
		    (nwords <= tp->ct_max) )  {
			return( tp->ct_func( nwords, cmd_args ) );
		}
		(void)fprintf(stderr, "rt cmd Usage: %s %s\n",
			tp->ct_cmd, tp->ct_parms);
		return(-1);		/* ERROR */
	}
	fprintf(stderr,"do_cmd(%s):  unknown command\n", cmd_args[0]);
	return(-1);			/* ERROR */
}
