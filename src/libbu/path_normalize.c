/* bu_path_normalize is a subset of NetBSD's realpath function:
 *
 * Copyright (c) 1989, 1991, 1993, 1995
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

#include <string.h>
#include <ctype.h>

#include "bu/path.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"

const char *
bu_path_normalize(const char *path)
{
    static char resolved[MAXPATHLEN] = {0};
    const char *q;
    char *p;
    if (!path) return (NULL);

    /*
     * `p' is where we'll put a new component with prepending
     * a delimiter.
     */
    p = resolved;

    if (*path == 0) return (NULL);

loop:
    /* Skip any slash. */
    while (*path == '/')
	path++;

    if (*path == 0) {
	if (p == resolved)
	    *p++ = '/';
	*p = 0;
	return (resolved);
    }

    /* Find the end of this component. */
    q = path;
    do
	q++;
    while (*q != '/' && *q != 0);

    /* Test . or .. */
    if (path[0] == '.') {
	if (q - path == 1) {
	    path = q;
	    goto loop;
	}
	if (path[1] == '.' && q - path == 2) {
	    /* Trim the last component. */
	    if (p != resolved)
		while (*--p != '/')
		    ;
	    path = q;
	    goto loop;
	}
    }

    /* Append this component. */
    if (p - resolved + 1 + q - path + 1 > MAXPATHLEN) {
	if (p == resolved)
	    *p++ = '/';
	*p = 0;
	return (NULL);
    }
    p[0] = '/';
    memcpy(&p[1], path,
	   /* LINTED We know q > path. */
	   q - path);
    p[1 + q - path] = 0;

    /* Advance both resolved and unresolved path. */
    p += 1 + q - path;
    path = q;
    goto loop;
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
