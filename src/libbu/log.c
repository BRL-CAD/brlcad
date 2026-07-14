/*                           L O G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "bio.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "bu/snooze.h"


/**
 * list of callbacks to call during bu_log.
 *
 * FIXME: this would be better returned to the caller as a logging
 * context instead of as a library-stateful set of callback functions
 */
static struct bu_hook_list log_hook_list = BU_HOOK_LIST_INIT_ZERO;
int BU_SEM_LOG_HOOK;

static int log_first_time = 1;
static THREADLOCAL int log_hooks_called = 0;
static int log_indent_level = 0;


/* A log call has historically blocked until its output is accepted.  Preserve
 * that behavior if an application has made its standard stream non-blocking. */
static int
log_retryable_error(int error)
{
    if (error == EINTR || error == EAGAIN)
	return 1;
#ifdef EWOULDBLOCK
    if (error == EWOULDBLOCK)
	return 1;
#endif
    return 0;
}


/*
 * Write all of a log message through a standard-stream file descriptor.
 * stdio is unsuitable here: after a short non-blocking write it may accept
 * additional bytes into its private buffer, making the retry boundary
 * unknowable.  Raw writes have an unambiguous partial-write result on both
 * the POSIX and Microsoft CRT interfaces.  The caller holds BU_SEM_SYSCALL.
 *
 * Returns 1 on success, 0 when no part of this message was written, and -1
 * when an unrecoverable error follows a partial write.
 */
static int
log_std_stream_write(FILE *fp, const char *buf, size_t len)
{
    size_t written = 0;
    int fd;

    if (!fp || !buf || !len)
	return 0;

    fd = fileno(fp);
    if (fd < 0)
	return 0;

    while (written < len) {
	size_t to_write = len - written;
	int ret;

	if (to_write > BU_PAGE_SIZE)
	    to_write = BU_PAGE_SIZE;
	errno = 0;
#ifdef HAVE_WINDOWS_H
	ret = _write(fd, buf + written, (unsigned int)to_write);
#else
	ret = (int)write(fd, buf + written, to_write);
#endif
	if (ret > 0) {
	    written += (size_t)ret;
	    continue;
	}

	int error = errno;
	if (ret < 0 && log_retryable_error(error)) {
	    if (error != EINTR)
		(void)bu_snooze(1000);
	    continue;
	}

	return written ? -1 : 0;
    }
    return 1;
}


static int
log_output(const char *buf, size_t len, int set_line_buffer)
{
    int ret = 0;

    if (stderr != NULL) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	if (set_line_buffer && UNLIKELY(log_first_time)) {
	    bu_setlinebuf(stderr);
	    log_first_time = 0;
	}
	ret = log_std_stream_write(stderr, buf, len);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    /* Only fall back when stderr accepted none of this message. */
    if (ret == 0 && stdout != NULL) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ret = log_std_stream_write(stdout, buf, len);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    return ret;
}


void
bu_log_indent_delta(int delta)
{
    if ((log_indent_level += delta) < 0)
	log_indent_level = 0;
}


void
bu_log_indent_vls(struct bu_vls *v)
{
    bu_vls_spaces(v, log_indent_level);
}


void
bu_log_add_hook(bu_hook_t func, void *clientdata)
{
    bu_semaphore_acquire(BU_SEM_LOG_HOOK);
    bu_hook_add(&log_hook_list, func, clientdata);
    bu_semaphore_release(BU_SEM_LOG_HOOK);
}


void
bu_log_delete_hook(bu_hook_t func, void *clientdata)
{
    bu_semaphore_acquire(BU_SEM_LOG_HOOK);
    bu_hook_delete(&log_hook_list, func, clientdata);
    bu_semaphore_release(BU_SEM_LOG_HOOK);
}


static int
log_call_hooks(void *buf)
{
    int called = 0;

    /* A hook which logs must not recursively dispatch the same hook list. */
    if (log_hooks_called)
	return 0;

    bu_semaphore_acquire(BU_SEM_LOG_HOOK);

    if (log_hook_list.size) {
	log_hooks_called = 1;
	bu_hook_call(&log_hook_list, buf);
	log_hooks_called = 0;
	called = 1;
    }

    bu_semaphore_release(BU_SEM_LOG_HOOK);
    return called;
}


void
bu_log_hook_save_all(struct bu_hook_list *save_hlp)
{
    bu_semaphore_acquire(BU_SEM_LOG_HOOK);
    bu_hook_save_all(&log_hook_list, save_hlp);
    bu_semaphore_release(BU_SEM_LOG_HOOK);
}


void
bu_log_hook_delete_all(void)
{
    bu_semaphore_acquire(BU_SEM_LOG_HOOK);
    bu_hook_delete_all(&log_hook_list);
    bu_semaphore_release(BU_SEM_LOG_HOOK);
}


void
bu_log_hook_restore_all(struct bu_hook_list *restore_hlp)
{
    bu_semaphore_acquire(BU_SEM_LOG_HOOK);
    bu_hook_restore_all(&log_hook_list, restore_hlp);
    bu_semaphore_release(BU_SEM_LOG_HOOK);
}


/**
 * This subroutine is used to append log_indent_level spaces
 * into a printf() format specifier string, after each newline
 * character is encountered.
 *
 * It exists primarily for bu_shootray() to affect the indentation
 * level of all messages at that recursion level, even if the calls
 * to bu_log come from non-librt routines.
 */
static void
log_do_indent_level(struct bu_vls *new_vls, register const char *old_vls)
{
    register int i;

    while (*old_vls) {
	bu_vls_putc(new_vls, (int)(*old_vls));
	if (*old_vls == '\n') {
	    i = log_indent_level;
	    while (i-- > 0)
		bu_vls_putc(new_vls, ' ');
	}
	++old_vls;
    }
}


void
bu_putchar(int c)
{
    char buf[2];

    buf[0] = (char)c;
    buf[1] = '\0';

    if (!log_call_hooks(buf)) {
	if (UNLIKELY(log_output(buf, 1, 0) != 1)) {
	    bu_bomb("bu_putchar: write error");
	}
    }

    if (log_indent_level > 0 && c == '\n') {
	int i;

	i = log_indent_level;
	while (i-- > 0)
	    bu_putchar(' ');
    }
}


int
bu_log(const char *fmt, ...)
{
    size_t len;
    va_list ap;
    struct bu_vls output = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!fmt || strlen(fmt) == 0)) {
	bu_vls_free(&output);
	return 0;
    }

    va_start(ap, fmt);
    if (log_indent_level > 0) {
	struct bu_vls newfmt = BU_VLS_INIT_ZERO;

	log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
    va_end(ap);

    len = bu_vls_strlen(&output);

    if (!log_call_hooks(bu_vls_addr(&output))) {
	if (UNLIKELY(len <= 0)) {
	    bu_vls_free(&output);
	    return len;
	}

	if (UNLIKELY(log_output(bu_vls_addr(&output), len, 1) != 1)) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    perror("write failed");
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    bu_bomb("bu_log: write error");
	}
    }

    bu_vls_free(&output);

    return (int)len;
}


int
bu_flog(FILE *fp, const char *fmt, ...)
{
    size_t len;
    va_list ap;

    struct bu_vls output = BU_VLS_INIT_ZERO;

    va_start(ap, fmt);
    if (log_indent_level > 0) {
	struct bu_vls newfmt = BU_VLS_INIT_ZERO;

	log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
    va_end(ap);

    len = bu_vls_strlen(&output);

    if (!log_call_hooks(bu_vls_addr(&output))) {
	if (LIKELY(len)) {
	    size_t ret;
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    ret = fwrite(bu_vls_addr(&output), len, 1, fp);
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    if (UNLIKELY(ret != 1))
		bu_bomb("bu_flog: write error");
	}
    }

    bu_vls_free(&output);

    return (int)len;
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
