/*                           L O G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */
/** @file log.c
 *
 * @brief parallel safe version of fprintf for logging
 *
 *  BRL-CAD support library, error logging routine.
 *  Note that the user may provide his own logging routine,
 *  by replacing these functions.  That is why this is in file of it's own.
 *  For example, LGT and RTSRV take advantage of this.
 *
 * @par  Primary Functions (replacements MUST implement all these) -
 * @n	bu_log			Called to log library events.
 * @n	bu_log_indent_delta	Change global indentation level
 * @n	bu_log_indent_vls	Apply indentation level (used by librt/pr.c)
 *
 * @par Specialty Functions -
 *	bu_log_add_hook		Start catching log events (used by mged/cmd.c)
 * @n	bu_putchar
 *
 *  @author	Michael John Muuss
 *  @author	Glenn Durfee
 *
 * @par  Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSlog[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#if defined(HAVE_STDARG_H)
# include <stdarg.h>
#else
#  if defined(HAVE_VARARGS_H)
#    include <varargs.h>
#  endif
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#include "machine.h"
#include "bu.h"

#if defined(HAVE_VARARGS_H) || defined(HAVE_STDARG_H)
BU_EXTERN(void	bu_vls_vprintf, (struct bu_vls *vls, const char *fmt, va_list ap));
#endif

static int	bu_log_indent_cur_level = 0; /* formerly rt_g.rtg_logindent */
/**
 *			B U _ L O G _ I N D E N T _ D E L T A
 *
 *  Change indentation level by indicated number of characters.
 *  Call with a large negative number to cancel all indentation.
 */
void
bu_log_indent_delta(int delta)
{
	if( (bu_log_indent_cur_level += delta) < 0 )
		bu_log_indent_cur_level = 0;
}

/**
 *			B U _ L O G _ I N D E N T _ V L S
 *
 *  For multi-line vls generators, honor logindent level like bu_log() does,
 *  and prefix the proper number of spaces.
 *  Should be called at the front of each new line.
 */
void
bu_log_indent_vls(struct bu_vls *v)
{
	bu_vls_spaces( v, bu_log_indent_cur_level );
}

#if 1
struct bu_hook_list bu_log_hook_list = {
	{	BU_LIST_HEAD_MAGIC,
		&bu_log_hook_list.l,
		&bu_log_hook_list.l
	},
	BUHOOK_NULL,
	GENPTR_NULL
};
#else
struct bu_hook_list bu_log_hook_list;
#endif

static int bu_log_first_time = 1;
static int bu_log_hooks_called = 0;

/**
 *			B U _ L O G _ A D D _ H O O K
 *
 *  Adds a hook to the list of bu_log hooks.  The top (newest) one of these
 *  will be called with its associated client data and a string to be
 *  processed.  Typcially, these hook functions will display the output
 *  (possibly in an X window) or record it.
 *
 *  XXX The hook functions are all non-PARALLEL.
 */

void
bu_log_add_hook(bu_hook_t func, genptr_t clientdata)
{
#if 0
    struct bu_hook_list *toadd;

    /* Grab a hunk of memory for a new node, and put it at the head of the
       list */

    BU_GETSTRUCT(toadd, bu_hook_list);
    toadd->hookfunc = func;
    toadd->clientdata = clientdata;
    toadd->l.magic = BUHOOK_LIST_MAGIC;

    BU_LIST_APPEND( &(bu_log_hook_list.l), &(toadd->l) );
#else
    bu_add_hook(&bu_log_hook_list, func, clientdata);
#endif
}


/**
 *			B U _ L O G _ D E L E T E _ H O O K
 *
 *  Removes the hook matching the function and clientdata parameters from
 *  the hook list.  Note that it is not necessarily the active (top) hook.
 */
void
bu_log_delete_hook(bu_hook_t func, genptr_t clientdata)
{
#if 0
    struct bu_hook_list *cur = &bu_log_hook_list;

    for ( BU_LIST_FOR( cur, bu_hook_list, &(bu_log_hook_list.l) ) ) {
        if ( cur->hookfunc == func && cur->clientdata == clientdata) {
	    struct bu_hook_list *old = BU_LIST_PLAST(bu_hook_list, cur);
	    BU_LIST_DEQUEUE( &(cur->l) );
	    bu_free((genptr_t)cur, "bu_log hook");
	    cur = old;
	}
    }
#else
    bu_delete_hook(&bu_log_hook_list, func, clientdata);
#endif
}

#if 1
HIDDEN void
bu_log_call_hooks(genptr_t buf)
{
#if 0
    bu_hook_t hookfunc;		/* for clarity */
    genptr_t clientdata;
#endif

    bu_log_hooks_called = 1;

#if 0
    hookfunc = BU_LIST_FIRST(bu_hook_list, &(bu_log_hook_list.l))->hookfunc;
    clientdata = BU_LIST_FIRST(bu_hook_list, &(bu_log_hook_list.l))->clientdata;

    (hookfunc)( clientdata, buf);
#else
    bu_call_hook(&bu_log_hook_list, buf);
#endif

    bu_log_hooks_called = 0;
}
#endif

/**
 *			B U _ L O G _ D O _ I N D E N T _ L E V E L
 *
 *  This subroutine is used to append bu_log_indent_cur_level spaces
 *  into a printf() format specifier string, after each newline
 *  character is encountered.
 *  It exists primarily for bu_shootray() to affect the indentation
 *  level of all messages at that recursion level, even if the calls
 *  to bu_log come from non-librt routines.
 */

HIDDEN void
bu_log_do_indent_level(struct bu_vls *new, register char *old)
{
    register int i;

    while (*old) {
	bu_vls_putc(new, (int)(*old));
	if (*old == '\n') {
	    i = bu_log_indent_cur_level;
	    while (i-- > 0)
		bu_vls_putc(new, ' ');
	}
	++old;
    }
}

/**
 *			B U _ P U T C H A R
 *
 * Log a single character with no flushing.
 */

void
bu_putchar(int c)
{
    if ( BU_LIST_IS_EMPTY( &(bu_log_hook_list.l) ) ) {
	fputc(c, stderr);
    } else {
	char buf[2];
	buf[0] = (char)c;
	buf[1] = '\0';
#if 1
	bu_log_call_hooks(buf);
#else
	bu_call_hook(&bu_log_hook_list, (genptr_t)buf);
#endif
    }

    if (bu_log_indent_cur_level > 0 && c == '\n') {
	int i;

	i = bu_log_indent_cur_level;
	while (i-- > 0)
	    bu_putchar(' ');
    }
}

/**
 *  			B U _ L O G
 *
 *  Log a library event in the Standard way.
 */
void
#if defined(HAVE_STDARG_H)
bu_log(char *fmt, ...)                      /* ANSI C */
{
    va_list ap;
#else
#  if defined(HAVE_VARARGS_H)
bu_log(va_alist)                            /* VARARGS */
va_dcl
{
    va_list ap;
    char *fmt;
#  else
bu_log(fmt, a,b,c,d,e,f,g,h,i,j)            /* Cray XMP */
char *fmt;
{
#  endif
#endif

    struct bu_vls output;

    bu_vls_init(&output);

#if defined(HAVE_STDARG_H)                  /* ANSI C */
    va_start(ap, fmt);

    if (!fmt || strlen(fmt) == 0) {
	return;
    }

    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);   /* VARARGS */
    }
#else
#  if defined(HAVE_VARARGS_H)
    va_start(ap);
    fmt = va_arg(ap, char *);

    if (!fmt || strlen(fmt) == 0) {
	return;
    }

    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
#  else                                     /* Cray XMP */
    if (!fmt || strlen(fmt) == 0) {
	return;
    }

    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_printf(&output, bu_vls_addr(&newfmt), a,b,c,d,e,f,g,h,i,j);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_printf(&output, fmt, a,b,c,d,e,f,g,h,i,j);
    }
#  endif
#endif

    if ( BU_LIST_IS_EMPTY( &(bu_log_hook_list.l) )  || bu_log_hooks_called) {
    	int ret;
	size_t len;

	if (bu_log_first_time) {
	    bu_setlinebuf(stderr);
	    bu_log_first_time = 0;
	}

	len = bu_vls_strlen(&output);
	if(len){
	  bu_semaphore_acquire(BU_SEM_SYSCALL);
	  ret = fwrite( bu_vls_addr(&output), len, 1, stderr );
	  (void)fflush(stderr);
	  bu_semaphore_release(BU_SEM_SYSCALL);
	  if( ret != 1 )  bu_bomb("bu_log: write error");
	}

    } else {
#if 1
	    bu_log_call_hooks(bu_vls_addr(&output));
#else
	    bu_call_hook(&bu_log_hook_list, (genptr_t)bu_vls_addr(&output));
#endif
    }

#if defined(HAVE_STDARG_H) || defined(HAVE_VARARGS_H)
    va_end(ap);
#endif

    bu_vls_free(&output);
}

/**
 *  			B U _ F L O G
 *
 *  Log a library event in the Standard way, to a specified file.
 */
void
#if defined(HAVE_STDARG_H)
bu_flog(FILE *fp, char *fmt, ...)                      /* ANSI C */
{
    va_list ap;
#else
#  if defined(HAVE_VARARGS_H)
bu_flog(va_alist)                            /* VARARGS */
va_dcl
{
    va_list ap;
    FILE *fp;
    char *fmt;
#  else
bu_flog(fp, fmt, a,b,c,d,e,f,g,h,i,j)            /* Cray XMP */
FILE *fp;
char *fmt;
{
#  endif
#endif

    struct bu_vls output;

    bu_vls_init(&output);

#if defined(HAVE_STDARG_H)                  /* ANSI C */
    va_start(ap, fmt);
    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);   /* VARARGS */
    }
#else
#  if defined(HAVE_VARARGS_H)
    va_start(ap);
    fp = va_arg(ap, FILE *);
    fmt = va_arg(ap, char *);
    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
#  else                                     /* Cray XMP */
    if (bu_log_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_log_do_indent_level(&newfmt, fmt);
	bu_vls_printf(&output, bu_vls_addr(&newfmt), a,b,c,d,e,f,g,h,i,j);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_printf(&output, fmt, a,b,c,d,e,f,g,h,i,j);
    }
#  endif
#endif

    if ( BU_LIST_IS_EMPTY( &(bu_log_hook_list.l) ) || bu_log_hooks_called) {
    	int ret;
	size_t len;

	len = bu_vls_strlen(&output);
	if(len){
	  bu_semaphore_acquire(BU_SEM_SYSCALL);
	  ret = fwrite( bu_vls_addr(&output), len, 1, fp );
	  bu_semaphore_release(BU_SEM_SYSCALL);
	  if( ret != 1 )  bu_bomb("bu_flog: write error");
	}

    } else {
#if 1
	    bu_log_call_hooks(bu_vls_addr(&output));
#else
	    bu_call_hook(&bu_log_hook_list, (genptr_t)bu_vls_addr(&output));
#endif
    }

#if defined(HAVE_STDARG_H) || defined(HAVE_VARARGS_H)
    va_end(ap);
#endif

    bu_vls_free(&output);
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
