/*                      I N T E R A C T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file interact.c
 *
 */

/*      INTERACT.C      */
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/ 
#include "common.h"



#include <stdlib.h>

#include <stdio.h>
#include <ctype.h>
#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "./nirt.h"
#include "./usrfmt.h"

extern int		nirt_debug;
extern com_table	ComTab[];
extern int		silent_flag;

extern void		bu_log(char *, ...);

static int sgetc (char *string)
{
    static char	*prev_string = 0;
    static char	*sp;
    static int	reported_EOS = 0;

    if (nirt_debug & DEBUG_INTERACT)
	bu_log("sgetc(%s) '%s' '%s'... ", string, prev_string, sp);

    if ((string == 0) || (string != prev_string))
    {
	sp = prev_string = string;
	reported_EOS = 0;
	if (nirt_debug & DEBUG_INTERACT)
	    bu_log("initializing\n");
	if (string == 0)
	    return (EOF);
    }
    if (*sp == '\0')
	if (reported_EOS)
	{
	    if (nirt_debug & DEBUG_INTERACT)
		bu_log("returning EOF\n");
	    return (EOF);
	}
	else
	{
	    reported_EOS = 1;
	    if (nirt_debug & DEBUG_INTERACT)
		bu_log("returning EOS\n");
	    return ('\0');
	}
    else
	if (nirt_debug & DEBUG_INTERACT)
	    bu_log("returning '%c' (o%o)\n", *sp, *sp);
	return ((int) *sp++);
}

/*	               I N T E R A C T ( )
 *
 *	Handle user interaction.  Interact() prompts on stdin
 *	for a key word, looks the key word up in the command table
 *	and, if it finds the key word, the command is executed.
 *
 */
void interact(int input_source, void *sPtr)
{
    int		Ch;		/* individual characters of the input line */
    int		Prev_ch=0;	/* previous character */
    char    	line_buffer[256];	/* line of text the user types   */
    int		i;		/* position on the line_buffer[]           */
    com_table	*ctp;		/* command table pointer		   */
    int		key_len;	/* the length of the key word             */
    int		in_cmt;		/* are we now within a comment? */
    int		more_on_line = 0;/* are we withing a multi-command line? */

#define	next_char(s)	(input_source == READING_FILE)		?	\
			    fgetc((FILE *) s)			:       \
			(input_source == READING_STRING)	?	\
			    sgetc((char *) s)			:	\
			(bu_log("next_char(%d) error.  Shouldn't happen\n", \
			    input_source), EOF)

    if (nirt_debug & DEBUG_INTERACT)
	bu_log("interact(%s, %x)...\n",
	    (input_source == READING_FILE) ? "READING_FILE" :
	    (input_source == READING_STRING) ? "READING_STRING" : "???",
	    sPtr);

    /*
     *	Prime the pump when reading from a string
     */
    if (input_source == READING_STRING)
	sgetc((char *)0);

    for (;;)
    {
	in_cmt = 0;
	key_len = 0;
	if ((input_source == READING_FILE) && (sPtr == stdin)
	 && (silent_flag != SILENT_YES) && (! more_on_line))
	    (void) fputs(NIRT_PROMPT, stdout);     

	more_on_line = 0;
	while (((Ch = next_char(sPtr)) == ' ') || (Ch == '\t'))
	    if (nirt_debug & DEBUG_INTERACT)
		bu_log("Skipping '%c'\n", Ch);
	if (Ch == '\n')
	    continue;
	for (i = 0; (Ch != '\n') && (i < 255); ++i)
	{
	    if (Ch == CMT_CHAR)
	    {
		if( Prev_ch == '\\' )
		    i--;
		else
		{
		    in_cmt = 1;
		    while (((Ch = next_char(sPtr)) != EOF) && (Ch != '\n'))
			;
		}
	    }
	    if (Ch == SEP_CHAR)
	    {
		more_on_line = 1;
		break;
	    }
	    else if (Ch == '\n')
		break;
	    if ((input_source == READING_STRING) && (Ch == '\0'))
		break;
	    if (Ch == EOF)
	    {
		if ((input_source == READING_FILE) && (sPtr == stdin))
		{
		    bu_log( "Unexpected EOF in input!!\n");
		    exit(1);
		}
		else
		    return;
	    }
	    if (key_len == 0 && (Ch == ' ' || Ch == '\t'))
		key_len = i;      /* length of key word */
	    line_buffer[i] = Ch;
	    Prev_ch = Ch;
	    if (nirt_debug & DEBUG_INTERACT)
		bu_log("line_buffer[%d] = '%c' (o%o)\n", i, Ch, Ch);
	    Ch = next_char(sPtr);
	}
	if (key_len == 0)      /* length of key word */ 
	{
	    if (in_cmt)
		continue;
	    key_len = i;
	}
	line_buffer[i] = '\0';

	if (nirt_debug & DEBUG_INTERACT)
	    bu_log("Line buffer contains '%s'\n", line_buffer);

	if ((ctp = get_comtab_ent(line_buffer, key_len)) == CT_NULL)
	{
	    line_buffer[key_len] = '\0';
	    fprintf(stderr, 
		"Invalid command name '%s'.  Enter '?' for help\n",
		line_buffer);
	}
	else
	    (*(ctp -> com_func)) (&line_buffer[key_len], ctp);
    }
}

com_table *get_comtab_ent (char *pattern, int pat_len)
{
    com_table	*ctp;
    int		len;

    for (ctp = ComTab; ctp -> com_name; ++ctp)
    {
	len = max(pat_len, (int)strlen(ctp -> com_name));
	if ((strncmp (pattern, ctp -> com_name, len)) == 0)
	    break;
    }
    return ((ctp -> com_name) ? ctp : CT_NULL);
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
