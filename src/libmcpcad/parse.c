/*                        P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file parse.c
 *
 * Parsing of flat command strings into argc/argv form for execution.
 *
 * Tokenization is delegated to libbu's bu_argv_from_string(), which
 * handles whitespace splitting, double-quoted strings, and backslash
 * escapes.  This layer adds input validation (size caps, control
 * character rejection) and an ownership model: the returned command
 * owns a private copy of the input that its argv pointers reference,
 * so a single mcpcad_cmd_free() releases everything.
 */

#include "common.h"

#include <string.h>
#include <ctype.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "mcpcad.h"


struct mcpcad_cmd *
mcpcad_cmd_parse(const char *line)
{
    size_t len, i;
    struct mcpcad_cmd *c;

    if (UNLIKELY(!line))
	return NULL;

    /* MCPCAD_MAXLINE includes the terminating NUL, so the longest
     * accepted string is MAXLINE-1 characters */
    len = strlen(line);
    if (len == 0 || len >= MCPCAD_MAXLINE)
	return NULL;

    /* Reject control characters other than tab.  Newlines delimit
     * messages at the transport layer - one surviving into a command
     * payload means upstream framing is confused, and we want that
     * loud rather than silently executing half a message.  The cast
     * matters: passing a negative char to iscntrl() is undefined. */
    for (i = 0; i < len; i++) {
	unsigned char ch = (unsigned char)line[i];
	if (iscntrl(ch) && ch != '\t')
	    return NULL;
    }

    BU_GET(c, struct mcpcad_cmd);

    /* bu_argv_from_string() carves its input buffer in place (writes
     * NULs over separators) and the resulting argv points into it.
     * The caller's string is const - take a private copy that lives
     * exactly as long as the argv referencing it. */
    c->line = bu_strdup(line);
    c->argv = (char **)bu_calloc(MCPCAD_MAXARGS + 1, sizeof(char *),
				 "mcpcad_cmd argv");
    c->argc = (int)bu_argv_from_string(c->argv, MCPCAD_MAXARGS, c->line);

    /* argc == 0: whitespace-only input, nothing to execute.
     * argc >= MAXARGS: the tokenizer stops at the cap rather than
     * failing, so hitting it may mean arguments were dropped - reject
     * outright, never execute a possibly-partial command. */
    if (c->argc == 0 || c->argc >= MCPCAD_MAXARGS) {
	mcpcad_cmd_free(c);
	return NULL;
    }

    return c;
}


void
mcpcad_cmd_free(struct mcpcad_cmd *c)
{
    if (!c)
	return;

    /* argv[i] are pointers into c->line, never separate allocations -
     * freeing them individually would corrupt the heap.  Two frees
     * cover everything. */
    if (c->argv)
	bu_free(c->argv, "mcpcad_cmd argv");
    if (c->line)
	bu_free(c->line, "mcpcad_cmd line");

    BU_PUT(c, struct mcpcad_cmd);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
