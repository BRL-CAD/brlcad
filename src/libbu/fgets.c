/*                         F G E T S . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2011 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"


char *
bu_fgets(char *s, int size, FILE *stream)
{
    int totBytesRead = 0;
    int isEOF = 0;

    /* if we are not asked to read anything, just return */
    if (UNLIKELY(size < 1)) {
	return s;
    }

    /* if the buffer size is one, we have no space (we add a null)
     * so just return
     */
    if (UNLIKELY(size == 1)) {
	*s = '\0';
	return s;
    }

    /* check for EOF or error */
    if (feof(stream) || ferror(stream)) {
	*s = '\0';
	return (char *)NULL;
    }

    /* actually do some reading */
    while (totBytesRead < size - 1) {
	int c;

	c = fgetc(stream);
	if (c == EOF) {
	    isEOF = 1;
	    break;
	}

	s[totBytesRead++] = c;

	/* check for newline */
	if (c == '\n') {
	    break;
	}

	/* chech for CR */
	if (c == '\r') {

	    /* check for CR/LF combination */
	    c = fgetc(stream);
	    if (c != '\n') {
		/* not a CR/LF, so unget the last char */
		ungetc(c, stream);
	    }

	    break;
	}
    }

    /* add our null */
    s[totBytesRead] = '\0';

    if (isEOF && totBytesRead == 0)
	return (char *)NULL;
    else
	return s;
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
