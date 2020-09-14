/*                  R E A L P A T H _ B S D . C
 *
 * Based off of OpenBSD's realpath.c,v 1.22 2017/12/24
 *
 * Copyright (c) 2003 Constantin S. Svintsoff <kostik@iclub.nsu.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "bio.h"
#include "bu/malloc.h"
#include "bu/str.h"

#ifndef HAVE_WORKING_REALPATH

#ifndef SYMLOOP_MAX
#  define SYMLOOP_MAX 8
#endif

/*
 * char *realpath(const char *path, char resolved[PATH_MAX]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
char *
bsd_realpath(const char *path, char *resolved)
{
    const char *p;
    char *q;
    size_t left_len, resolved_len, next_token_len;
    unsigned symlinks;
    int serrno, mem_allocated;
    ssize_t slen;
    char left[MAXPATHLEN], next_token[MAXPATHLEN], symlink[MAXPATHLEN];

    if (path == NULL) {
	errno = EINVAL;
	return (NULL);
    }

    if (path[0] == '\0') {
	errno = ENOENT;
	return (NULL);
    }

    serrno = errno;

    if (resolved == NULL) {
	resolved = (char *)bu_malloc(MAXPATHLEN, "alloc MAXPATHLEN");
	if (resolved == NULL)
	    return (NULL);
	mem_allocated = 1;
    } else
	mem_allocated = 0;

    symlinks = 0;
    if (path[0] == '/') {
	resolved[0] = '/';
	resolved[1] = '\0';
	if (path[1] == '\0')
	    return (resolved);
	resolved_len = 1;
	left_len = bu_strlcpy(left, path + 1, sizeof(left));
    } else {
	if (getcwd(resolved, MAXPATHLEN) == NULL) {
	    if (mem_allocated)
		free(resolved);
	    else
		bu_strlcpy(resolved, ".", MAXPATHLEN);
	    return (NULL);
	}
	resolved_len = strlen(resolved);
	left_len = bu_strlcpy(left, path, sizeof(left));
    }
    if (left_len >= sizeof(left)) {
	errno = ENAMETOOLONG;
	goto err;
    }

    /*
     * Iterate over path components in `left'.
     */
    while (left_len != 0) {
	/*
	 * Extract the next path component and adjust `left'
	 * and its length.
	 */
	p = strchr(left, '/');

	next_token_len = p ? (size_t) (p - left) : left_len;
	memcpy(next_token, left, next_token_len);
	next_token[next_token_len] = '\0';

	if (p != NULL) {
	    left_len -= next_token_len + 1;
	    memmove(left, p + 1, left_len + 1);
	} else {
	    left[0] = '\0';
	    left_len = 0;
	}

	if (resolved[resolved_len - 1] != '/') {
	    if (resolved_len + 1 >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		goto err;
	    }
	    resolved[resolved_len++] = '/';
	    resolved[resolved_len] = '\0';
	}
	if (next_token[0] == '\0')
	    continue;
	else if (bu_strcmp(next_token, ".") == 0)
	    continue;
	else if (bu_strcmp(next_token, "..") == 0) {
	    /*
	     * Strip the last path component except when we have
	     * single "/"
	     */
	    if (resolved_len > 1) {
		resolved[resolved_len - 1] = '\0';
		q = strrchr(resolved, '/') + 1;
		*q = '\0';
		resolved_len = q - resolved;
	    }
	    continue;
	}

	/*
	 * Append the next path component and readlink() it. If
	 * readlink() fails we still can return successfully if
	 * it exists but isn't a symlink, or if there are no more
	 * path components left.
	 */
	resolved_len = bu_strlcat(resolved, next_token, MAXPATHLEN);
	if (resolved_len >= MAXPATHLEN) {
	    errno = ENAMETOOLONG;
	    goto err;
	}
	slen = readlink(resolved, symlink, sizeof(symlink) - 1);
	/* POS30-C: ensure NULL termination of readlink */
	if (slen >= 0) {
	    symlink[slen] = '\0';
	}
	if (slen < 0) {
	    switch (errno) {
		case EINVAL:
		    /* not a symlink, continue to next component */
		    continue;
		case ENOENT:
		    if (p == NULL) {
			errno = serrno;
			return (resolved);
		    }
		    /* FALLTHROUGH */
		default:
		    goto err;
	    }
	} else if (slen == 0) {
	    errno = EINVAL;
	    goto err;
	} else if (slen == sizeof(symlink)) {
	    errno = ENAMETOOLONG;
	    goto err;
	} else {
	    if (symlinks++ > SYMLOOP_MAX) {
		errno = ELOOP;
		goto err;
	    }

	    symlink[slen] = '\0';
	    if (symlink[0] == '/') {
		resolved[1] = 0;
		resolved_len = 1;
	    } else {
		/* Strip the last path component. */
		q = strrchr(resolved, '/') + 1;
		*q = '\0';
		resolved_len = q - resolved;
	    }

	    /*
	     * If there are any path components left, then
	     * append them to symlink. The result is placed
	     * in `left'.
	     */
	    if (p != NULL) {
		if (symlink[slen - 1] != '/') {
		    if (slen + 1 >= (ssize_t)sizeof(symlink)) {
			errno = ENAMETOOLONG;
			goto err;
		    }
		    symlink[slen] = '/';
		    symlink[slen + 1] = 0;
		}
		left_len = bu_strlcat(symlink, left, sizeof(symlink));
		if (left_len >= sizeof(symlink)) {
		    errno = ENAMETOOLONG;
		    goto err;
		}
	    }
	    left_len = bu_strlcpy(left, symlink, sizeof(left));
	}
    }

    /*
     * Remove trailing slash except when the resolved pathname
     * is a single "/".
     */
    if (resolved_len > 1 && resolved[resolved_len - 1] == '/')
	resolved[resolved_len - 1] = '\0';
    return (resolved);

err:
    if (mem_allocated)
	free(resolved);
    return (NULL);
}

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
