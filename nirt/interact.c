/*      INTERACT.C      */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/ 
#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "./nirt.h"
#include "./usrfmt.h"

extern com_table	ComTab[];
extern int		silent_flag;

/*	               I N T E R A C T ( )
 *
 *	Handle user interaction.  Interact() prompts on stdin
 *	for a key word, looks the key word up in the command table
 *	and, if it finds the key word, the command is executed.
 *
 */
void interact(fPtr)

FILE	*fPtr;

{
	int		Ch;	/* individual characters of the input line */
	int		Prev_ch=0; /* previous character */
	char    	line_buffer[256]; /* line of text the user types   */
	int 		i;      /* position on the line_buffer[]           */
	com_table       *ctp;   /* command table pointer		   */
	int		key_len; /* the length of the key word             */
	int		n;	 /* how many chars to compare */
	int		in_cmt;	/* are we now within a comment? */

	for (EVER)
	{
		in_cmt = 0;
		key_len = 0;
		if ((fPtr == stdin) && (silent_flag != SILENT_YES))
		    (void) fputs(NIRT_PROMPT, stdout);     
		while (((Ch = fgetc(fPtr)) == ' ') OR (Ch == '\t'));
		if (Ch == '\n')
			continue;
		for (i = 0; (Ch != '\n') AND (i < 255); ++i)
		{
			if (Ch == CMT_CHAR)
			{
			    if( Prev_ch == '\\' )
				i--;
			    else
			    {
				    in_cmt = 1;
				    while (((Ch = fgetc(fPtr)) != EOF) && (Ch != '\n'))
					;
			    }
			}
			if (Ch == '\n')
			    break;
			if (Ch == EOF)
			    if (fPtr == stdin)
				exit(1);
			    else
				return;
		        if (key_len == 0 AND (Ch == ' ' OR Ch == '\t'))
				key_len = i;      /* length of key word */
			line_buffer[i] = Ch;
			Prev_ch = Ch;
			Ch = fgetc(fPtr);
		}
		if (key_len == 0)      /* length of key word */ 
		{
		    if (in_cmt)
			continue;
		    key_len = i;
		}
		line_buffer[i] = '\0';

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

com_table *get_comtab_ent (pattern, pat_len)

char	*pattern;
int	pat_len;

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
