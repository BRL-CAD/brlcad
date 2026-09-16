/*
 * Based on OpenBSD glob.c v 1.50 2020/10/13 04:42:28
 *
 * Copyright (c) 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE
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
#include <string.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bio.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/glob.h"

/* uce-dirent.h provides opendir/readdir/closedir on all platforms */
#include "uce-dirent.h"


/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/* Return non-zero if 'pat' contains any glob metacharacter. */
static int
_bu_glob_has_magic(const char *pat, int noescape)
{
    char c;
    while ((c = *pat++) != '\0') {
	if (c == '\\' && !noescape) {
	    if (*pat == '\0')
		break;
	    pat++;
	    continue;
	}
	if (c == '*' || c == '?' || c == '[')
	    return 1;
    }
    return 0;
}


/* Append result 'path' to the match list in ctx. */
static int
_bu_glob_add(struct bu_glob_context *ctx, const char *path)
{
    struct bu_vls *entry;
    struct bu_vls **newv;
    size_t newcnt = (size_t)(ctx->gl_pathc) + 1;

    newv = (struct bu_vls **)bu_realloc(ctx->gl_pathv,
	    (newcnt + 1) * sizeof(struct bu_vls *),
	    "bu_glob pathv");
    if (!newv)
	return 1;
    ctx->gl_pathv = newv;

    BU_ALLOC(entry, struct bu_vls);
    BU_VLS_INIT(entry);
    bu_vls_sprintf(entry, "%s", path);

    ctx->gl_pathv[ctx->gl_pathc] = entry;
    ctx->gl_pathc++;
    ctx->gl_pathv[ctx->gl_pathc] = NULL;  /* keep NULL-terminated */
    ctx->gl_matchc++;
    return 0;
}


/* Comparator for sorting the result array. */
static int
_bu_glob_cmp(const void *a, const void *b, void *UNUSED(u))
{
    const struct bu_vls *va = *(const struct bu_vls * const *)a;
    const struct bu_vls *vb = *(const struct bu_vls * const *)b;
    return bu_strcmp(bu_vls_cstr(va), bu_vls_cstr(vb));
}


/* --------------------------------------------------------------------------
 * Default filesystem callbacks
 * -------------------------------------------------------------------------- */

struct _bu_fs_dir_ctx {
    DIR *dirp;
};


static void *
_bu_glob_fs_opendir(const char *path, void *UNUSED(data))
{
    struct _bu_fs_dir_ctx *dctx;
    const char *p = (path && *path) ? path : ".";
    DIR *dirp = opendir(p);
    if (!dirp)
	return NULL;

    BU_ALLOC(dctx, struct _bu_fs_dir_ctx);
    dctx->dirp = dirp;
    return (void *)dctx;
}


static int
_bu_glob_fs_readdir(struct bu_dirent *de, void *handle)
{
    struct _bu_fs_dir_ctx *dctx = (struct _bu_fs_dir_ctx *)handle;
    struct dirent *dp;

    if (!dctx || !dctx->dirp)
	return 1;

    /* Skip . and .. automatically */
    do {
	dp = readdir(dctx->dirp);
	if (!dp)
	    return 1;
    } while (dp->d_name[0] == '.' &&
	    (dp->d_name[1] == '\0' ||
	     (dp->d_name[1] == '.' && dp->d_name[2] == '\0')));

    bu_vls_sprintf(de->name, "%s", dp->d_name);
    return 0;
}


static void
_bu_glob_fs_closedir(void *handle)
{
    struct _bu_fs_dir_ctx *dctx = (struct _bu_fs_dir_ctx *)handle;
    if (!dctx)
	return;
    if (dctx->dirp)
	closedir(dctx->dirp);
    bu_free(dctx, "_bu_fs_dir_ctx");
}


static int
_bu_glob_fs_lstat(const char *path, struct bu_stat *sb, void *UNUSED(data))
{
    struct stat s;
#ifdef HAVE_LSTAT
    if (lstat(path, &s) < 0) return -1;
#else
    if (stat(path, &s) < 0) return -1;
#endif
    sb->size = (b_off_t)s.st_size;
    sb->is_dir = ((s.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
    return 0;
}



/* --------------------------------------------------------------------------
 * Dispatch helpers (pick ctx callbacks or default filesystem ones)
 * -------------------------------------------------------------------------- */

static void *
_glob_opendir(const char *path, struct bu_glob_context *ctx)
{
    if (ctx->gl_opendir)
	return ctx->gl_opendir(path, ctx->data);
    return _bu_glob_fs_opendir(path, NULL);
}


static int
_glob_readdir(struct bu_dirent *de, void *handle, struct bu_glob_context *ctx)
{
    if (ctx->gl_readdir)
	return ctx->gl_readdir(de, handle);
    return _bu_glob_fs_readdir(de, handle);
}


static void
_glob_closedir(void *handle, struct bu_glob_context *ctx)
{
    if (ctx->gl_closedir)
	ctx->gl_closedir(handle);
    else
	_bu_glob_fs_closedir(handle);
}


/* Stat a path; zeroes *sb before calling the backend. */
static int
_glob_lstat(const char *path, struct bu_stat *sb, struct bu_glob_context *ctx)
{
    memset(sb, 0, sizeof(*sb));
    if (ctx->gl_lstat)
	return ctx->gl_lstat(path, sb, ctx->data);
    return _bu_glob_fs_lstat(path, sb, NULL);
}


/* --------------------------------------------------------------------------
 * Core recursive engine
 * -------------------------------------------------------------------------- */

/*
 * Append a path separator and segment to pathbuf, respecting existing
 * trailing slashes so we don't double them.
 */
static void
_glob_path_append(struct bu_vls *pathbuf, const char *seg)
{
    size_t plen = bu_vls_strlen(pathbuf);
    const char *pb = bu_vls_cstr(pathbuf);
    if (plen > 0 && pb[plen - 1] != '/')
	bu_vls_putc(pathbuf, '/');
    bu_vls_strcat(pathbuf, seg);
}


/*
 * Recursively expand the pattern against the hierarchy rooted at pathbuf.
 *
 * pathbuf   - Current path accumulator (modified in place, always
 *             restored to its original length before returning).
 * pattern   - Remaining pattern to match (starting at the current segment).
 * ctx       - Glob context.
 * flags     - Flags passed to bu_glob().
 */
static int
_glob_expand(struct bu_vls *pathbuf,
	const char *pattern,
	struct bu_glob_context *ctx,
	int flags)
{
    char seg[1024];
    const char *rest;
    size_t seglen;
    int noescape = (flags & BU_GLOB_NOESCAPE) ? 1 : 0;
    int has_magic;

    /* Find the end of the current segment (up to the next '/') */
    rest = pattern;
    while (*rest != '\0' && *rest != '/')
	rest++;
    seglen = (size_t)(rest - pattern);

    /* Advance past consecutive '/' separators to find the rest */
    while (*rest == '/')
	rest++;

    if (seglen == 0) {
	/* Consecutive separators or trailing slash - recurse */
	if (*rest == '\0')
	    return 0;
	return _glob_expand(pathbuf, rest, ctx, flags);
    }

    if (seglen >= sizeof(seg))
	seglen = sizeof(seg) - 1;
    memcpy(seg, pattern, seglen);
    seg[seglen] = '\0';

    has_magic = _bu_glob_has_magic(seg, noescape);

    if (!has_magic) {
	/* ---- Literal segment: append and continue ---- */
	size_t orig_len = bu_vls_strlen(pathbuf);
	struct bu_stat sb;

	_glob_path_append(pathbuf, seg);

	if (*rest == '\0') {
	    /* End of pattern: check if path actually exists */
	    if (_glob_lstat(bu_vls_cstr(pathbuf), &sb, ctx) == 0)
		_bu_glob_add(ctx, bu_vls_cstr(pathbuf));
	} else {
	    /* More pattern segments remain: must be a container */
	    if (_glob_lstat(bu_vls_cstr(pathbuf), &sb, ctx) == 0 && sb.is_dir)
		_glob_expand(pathbuf, rest, ctx, flags);
	}

	bu_vls_trunc(pathbuf, (int)orig_len);
	return 0;
    }

    /* ---- Wildcard segment: enumerate the current directory ---- */
    {
	void *dirhandle;
	struct bu_dirent de;
	struct bu_vls de_name = BU_VLS_INIT_ZERO;
	int match_flags = noescape ? BU_PATH_MATCH_NOESCAPE : 0;
	const char *dirpath = bu_vls_strlen(pathbuf) > 0 ? bu_vls_cstr(pathbuf) : "";

	de.name = &de_name;
	de.data = NULL;

	dirhandle = _glob_opendir(dirpath, ctx);
	if (!dirhandle) {
	    if (ctx->gl_errfunc) {
		int e = errno;
		if (ctx->gl_errfunc(dirpath, e, ctx->data)) {
		    bu_vls_free(&de_name);
		    return BU_GLOB_ABORTED;
		}
	    }
	    bu_vls_free(&de_name);
	    return 0;
	}

	while (_glob_readdir(&de, dirhandle, ctx) == 0) {
	    const char *name = bu_vls_cstr(&de_name);
	    size_t orig_len;

	    if (bu_path_match(seg, name, match_flags) != 0)
		continue;

	    orig_len = bu_vls_strlen(pathbuf);
	    _glob_path_append(pathbuf, name);

	    if (*rest == '\0') {
		_bu_glob_add(ctx, bu_vls_cstr(pathbuf));
	    } else {
		struct bu_stat sb;
		if (_glob_lstat(bu_vls_cstr(pathbuf), &sb, ctx) == 0 && sb.is_dir)
		    _glob_expand(pathbuf, rest, ctx, flags);
	    }

	    bu_vls_trunc(pathbuf, (int)orig_len);
	}

	_glob_closedir(dirhandle, ctx);
	bu_vls_free(&de_name);
    }

    return 0;
}


/* --------------------------------------------------------------------------
 * Pimpl implementation type
 * -------------------------------------------------------------------------- */

struct bu_glob_ctx_impl {
    int _reserved;  /**< placeholder; reserved for future internal engine state
		     *   (e.g. stack depth limit, per-call allocation arena).
		     *   Do not access directly. */
};


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

struct bu_glob_context *
bu_glob_ctx_create(void)
{
    struct bu_glob_context *gp;
    BU_ALLOC(gp, struct bu_glob_context);
    memset(gp, 0, sizeof(*gp));
    BU_GET(gp->i, struct bu_glob_ctx_impl);
    gp->i->_reserved = 0;
    return gp;
}


void
bu_glob_ctx_destroy(struct bu_glob_context *gp)
{
    int i;
    if (!gp)
	return;
    if (gp->gl_pathv) {
	for (i = 0; i < gp->gl_pathc; i++) {
	    if (gp->gl_pathv[i]) {
		bu_vls_free(gp->gl_pathv[i]);
		bu_free(gp->gl_pathv[i], "bu_glob path entry");
	    }
	}
	bu_free(gp->gl_pathv, "bu_glob pathv");
	gp->gl_pathv = NULL;
    }
    gp->gl_pathc = 0;
    gp->gl_matchc = 0;
    if (gp->i) {
	BU_PUT(gp->i, struct bu_glob_ctx_impl);
	gp->i = NULL;
    }
    bu_free(gp, "bu_glob_context");
}


int
bu_glob(const char *pattern, int flags, struct bu_glob_context *gp)
{
    struct bu_vls pathbuf = BU_VLS_INIT_ZERO;
    int ret;
    const char *pat;

    if (!pattern || !gp)
	return 1;

    /* Optionally clear results from a prior call */
    if (!(flags & BU_GLOB_APPEND)) {
	int i;
	if (gp->gl_pathv) {
	    for (i = 0; i < gp->gl_pathc; i++) {
		if (gp->gl_pathv[i]) {
		    bu_vls_free(gp->gl_pathv[i]);
		    bu_free(gp->gl_pathv[i], "bu_glob path entry");
		}
	    }
	    bu_free(gp->gl_pathv, "bu_glob pathv");
	    gp->gl_pathv = NULL;
	}
	gp->gl_pathc = 0;
	gp->gl_matchc = 0;
    }

    pat = pattern;

    /* Absolute patterns: seed pathbuf with "/" */
    if (*pat == '/') {
	bu_vls_strcat(&pathbuf, "/");
	while (*pat == '/')
	    pat++;
    }

    if (*pat == '\0') {
	/* Pattern was just "/" - return root if it exists */
	struct bu_stat sb;
	if (_glob_lstat(bu_vls_cstr(&pathbuf), &sb, gp) == 0)
	    _bu_glob_add(gp, bu_vls_cstr(&pathbuf));
	bu_vls_free(&pathbuf);
	return 0;
    }

    ret = _glob_expand(&pathbuf, pat, gp, flags);
    bu_vls_free(&pathbuf);
    if (ret)
	return ret;

    /* Sort results unless the caller opts out */
    if (!(flags & BU_GLOB_NOSORT) && gp->gl_pathc > 1)
	bu_sort(gp->gl_pathv, (size_t)gp->gl_pathc, sizeof(struct bu_vls *),
		_bu_glob_cmp, NULL);

    return 0;
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
