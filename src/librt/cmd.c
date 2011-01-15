/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup ray */
/** @{ */
/** @file librt/cmd.c
 *
 * Read and parse a viewpoint-control command stream.
 *
 * This module is intended to be common to all programs which read
 * this type of command stream; the routines to handle the various
 * keywords should go in application-specific modules.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"

/*
 * R T _ R E A D _ C M D
 *
 * Read one semi-colon terminated string of arbitrary length from the
 * given file into a dynamicly allocated buffer.  Various commenting
 * and escaping conventions are implemented here.
 *
 * Returns:
 * NULL on EOF
 * char * on good read
 */
char *
rt_read_cmd(register FILE *fp)
{
    register int c;
    register char *buf;
    register int curpos;
    register int curlen;

    curpos = 0;
    curlen = 400;
    buf = bu_malloc(curlen, "rt_read_cmd command buffer");

    do {
	c = fgetc(fp);
	if (c == EOF) {
	    c = '\0';
	} else if (c == '#') {
	    /* All comments run to the end of the line */
	    while ((c = fgetc(fp)) != EOF && c != '\n')
		;
	    continue;
	} else if (c == '\n') {
	    c = ' ';
	} else if (c == ';') {
	    c = '\0';
	} else if (c == '\\') {
	    /* Backslash takes next character literally.
	     * EOF detection here is not a problem, next
	     * pass will detect it.
	     */
	    c = fgetc(fp);
	}
	if (c != '\0' && curpos == 0 && isspace(c)) {
	    /* Dispose of leading white space.
	     * Necessary to slurp up what newlines turn into.
	     */
	    continue;
	}
	if (curpos >= curlen) {
	    curlen *= 2;
	    buf = bu_realloc(buf, curlen, "rt_read_cmd command buffer");
	}
	buf[curpos++] = c;
    } while (c != '\0');
    if (curpos <= 1) {
	bu_free(buf, "rt_read_cmd command buffer (EOF)");
	return (char *)0;		/* EOF */
    }
    return buf;				/* OK */
}


#define MAXWORDS 4096	/* Max # of args per command */


/**
 * R T _ S P L I T _ C M D
 *
 * DEPRECATED: use bu_argv_from_string() instead
 */
int
rt_split_cmd(char **argv, int lim, char *lp)
{
    /* bu_argv_from_string doesn't count the NULL */
    return bu_argv_from_string(argv, lim-1, lp);
}


/*
 * R T _ D O _ C M D
 *
 * Slice up input buffer into whitespace separated "words", look up
 * the first word as a command, and if it has the correct number of
 * args, call that function.  Negative min/max values in the tp
 * command table effectively mean that they're not bounded.
 *
 * Expected to return -1 to halt command processing loop.
 *
 * Based heavily on mged/cmd.c by Chuck Kennedy.
 *
 * DEPRECATED: needs to migrate to libbu
 */
int
rt_do_cmd(struct rt_i *rtip, const char *ilp, register const struct command_tab *tp)
/* FUTURE:  for globbing */


{
    register int nwords;			/* number of words seen */
    char *cmd_args[MAXWORDS+1];	/* array of ptrs to args */
    char *lp;
    int retval;

    if (rtip)
	RT_CK_RTI(rtip);

    lp = bu_strdup(ilp);

    nwords = bu_argv_from_string(cmd_args, MAXWORDS, lp);
    if (nwords <= 0)
	return 0;	/* No command to process */


    for (; tp->ct_cmd != (char *)0; tp++) {
	if (cmd_args[0][0] != tp->ct_cmd[0] ||
	    /* the length of "n" is not significant, just needs to be big enough */
	    strncmp(cmd_args[0], tp->ct_cmd, MAXWORDS) != 0)
	    continue;
	if ((nwords >= tp->ct_min)
	    && ((tp->ct_max < 0) || (nwords <= tp->ct_max)))
	{
	    retval = tp->ct_func(nwords, cmd_args);
	    bu_free(lp, "rt_do_cmd lp");
	    return retval;
	}
	bu_log("rt_do_cmd Usage: %s %s\n\t%s\n",
	       tp->ct_cmd, tp->ct_parms, tp->ct_comment);
	bu_free(lp, "rt_do_cmd lp");
	return -1;		/* ERROR */
    }
    bu_log("rt_do_cmd(%s):  command not found\n", cmd_args[0]);
    bu_free(lp, "rt_do_cmd lp");
    return -1;			/* ERROR */
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
