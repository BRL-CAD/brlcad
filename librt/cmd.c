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

struct cmd_tab {
	char	*ct_cmd;
	int	(*ct_func)();
};
static struct cmd_tab cmdtab[] = {
	"start",	cm_start,
	"viewsize",	cm_vsize,
	"eye_pt",	cm_eyept,
	"viewrot",	cm_vrot,
	"end",		cm_end,
	"multiview",	cm_multiview,
	(char *)0,	0		/* END */
};

/*
 *			D O _ C M D
 */
int
do_cmd( buf )
register char *buf;
{
	register struct cmd_tab *tp;
	register char *cmd;

	/* Strip leading spaces */
	while( *buf && isspace(*buf) )  buf++;
	cmd = buf;

	/* Find end of first keyword */
	while( *buf && !isspace(*buf) ) buf++;
	if( *buf != '\0' )  {
		*buf++ = '\0';
		/* Skip any leading spaces on arg string */
		while( *buf && isspace(*buf) )  buf++;
	}
	if( *cmd == '\0' )
		return(0);		/* null command, OK */

	for( tp = cmdtab; tp->ct_cmd != (char *)0; tp++ )  {
		if( cmd[0] != tp->ct_cmd[0] ||
		    strcmp( cmd, tp->ct_cmd ) != 0 )
			continue;
		return( tp->ct_func( cmd, buf ) );
	}
	fprintf(stderr,"do_cmd(%s):  unknown command\n", cmd);
}
