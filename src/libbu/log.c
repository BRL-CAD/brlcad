/*                           L O G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "bu/log.h"
#include "bu/parallel.h"


/**
 * list of callbacks to call during bu_log.
 *
 * FIXME: this would be better returned to the caller as a logging
 * context instead of as a library-stateful set of callback functions
 */
static struct bu_hook_list log_hook_list = BU_HOOK_LIST_INIT_ZERO;
static int log_call_hooks_semaphore = -1;

static int log_first_time = 1;
static int log_hooks_called = 0;
static int log_indent_level = 0;


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
    bu_hook_add(&log_hook_list, func, clientdata);

    if(LIKELY(log_call_hooks_semaphore == -1))
    {
        /* initialize log_call_hooks_semaphore */
        log_call_hooks_semaphore = bu_semaphore_register("log_call_hooks_semaphore");
    }
}


void
bu_log_delete_hook(bu_hook_t func, void *clientdata)
{
    bu_hook_delete(&log_hook_list, func, clientdata);
}


static void
log_call_hooks(void *buf)
{
    if(LIKELY(log_call_hooks_semaphore != -1))
        bu_semaphore_acquire(log_call_hooks_semaphore);

    bu_hook_call(&log_hook_list, buf);

    if(LIKELY(log_call_hooks_semaphore != -1))
        bu_semaphore_release(log_call_hooks_semaphore);
}


void
bu_log_hook_save_all(struct bu_hook_list *save_hlp)
{
    bu_hook_save_all(&log_hook_list, save_hlp);
}


void
bu_log_hook_delete_all(void)
{
    bu_hook_delete_all(&log_hook_list);
}


void
bu_log_hook_restore_all(struct bu_hook_list *restore_hlp)
{
    bu_hook_restore_all(&log_hook_list, restore_hlp);
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
    int ret = EOF;

    if (log_hook_list.size == 0) {

	if (LIKELY(stderr != NULL)) {
	    ret = fputc(c, stderr);
	}

	if (UNLIKELY(ret == EOF && stdout)) {
	    ret = fputc(c, stdout);
	}

	if (UNLIKELY(ret == EOF)) {
	    bu_bomb("bu_putchar: write error");
	}

    } else {
	char buf[2];
	buf[0] = (char)c;
	buf[1] = '\0';

	log_call_hooks(buf);
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

    if (log_hook_list.size == 0 || log_hooks_called) {
	size_t ret = 0;

	if (UNLIKELY(log_first_time)) {
	    bu_setlinebuf(stderr);
	    log_first_time = 0;
	}

	if (UNLIKELY(len <= 0)) {
	    bu_vls_free(&output);
	    return len;
	}

	if (LIKELY(stderr != NULL)) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    ret = fwrite(bu_vls_addr(&output), len, 1, stderr);
	    fflush(stderr);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}

	if (UNLIKELY(ret == 0 && stdout)) {
	    /* if stderr fails, try stdout instead */
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    ret = fwrite(bu_vls_addr(&output), len, 1, stdout);
	    fflush(stdout);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}

	if (UNLIKELY(ret == 0)) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    perror("fwrite failed");
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    bu_bomb("bu_log: write error");
	}

    } else {
	log_call_hooks(bu_vls_addr(&output));
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

    if (log_hook_list.size == 0 || log_hooks_called) {
	size_t ret;

	if (LIKELY(len)) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    ret = fwrite(bu_vls_addr(&output), len, 1, fp);
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    if (UNLIKELY(ret != 1))
		bu_bomb("bu_flog: write error");
	}

    } else {
	log_call_hooks(bu_vls_addr(&output));
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
