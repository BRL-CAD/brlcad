/*
 *			L O G . C
 *
 *  BRL-CAD support library, error logging routine.
 *  Note that the user may provide his own logging routine,
 *  by replacing this function.  That is why it is in file of it's own.
 *  For example, LGT and RTSRV take advantage of this.
 *
 *  Functions -
 *	bu_log		Called to log library events.
 *
 *  Authors -
 *	Michael John Muuss
 *	Glenn Durfee
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSlog[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if defined(HAVE_STDARG_H)
# include <stdarg.h>
#else
#  if defined(HAVE_VARARGS_H)
#    include <varargs.h>
#  endif
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"

static int	bu_indent_cur_level = 0; /* formerly rt_g.rtg_logindent */
/*
 *			B U _ L O G _ I N D E N T _ D E L T A
 *
 *  Change indentation level by indicated number of characters.
 *  Call with a large negative number to cancel all indentation.
 */
void
bu_log_indent_delta( delta )
int	delta;
{
	if( (bu_indent_cur_level += delta) < 0 )
		bu_indent_cur_level = 0;
}

/*
 *			B U _ L O G _ I N D E N T _ V L S
 *
 *  For multi-line vls generators, honor logindent level like bu_log() does,
 *  and prefix the proper number of spaces.
 *  Should be called at the front of each new line.
 */
void
bu_log_indent_vls( v )
struct bu_vls	*v;
{
	bu_vls_spaces( v, bu_indent_cur_level );
}


static struct bu_hook_list {
	struct rt_list	l;
	bu_hook_t	hookfunc;
	genptr_t 	clientdata;
} bu_log_hook_list = {
	{	RT_LIST_HEAD_MAGIC, 
		&bu_log_hook_list.l, 
		&bu_log_hook_list.l
	}, 
	BUHOOK_NULL,
	GENPTR_NULL
};

#define BUHOOK_LIST_MAGIC	0x90d5dead	/* Nietzsche? */
#define BUHOOK_LIST_NULL	((struct bu_hook_list *) 0)

static int bu_log_first_time = 1;
static int bu_log_hooks_called = 0;

/*
 *			B U _ A D D _ H O O K
 *
 *  Adds a hook to the list of bu_log hooks.  The top (newest) one of these
 *  will be called with its associated client data and a string to be
 *  processed.  Typcially, these hook functions will display the output
 *  (possibly in an X window) or record it.
 *
 *  XXX The hook functions are all non-PARALLEL.
 */

void
bu_add_hook( func, clientdata )
bu_hook_t func;
genptr_t clientdata;
{
    struct bu_hook_list *toadd;

    /* Grab a hunk of memory for a new node, and put it at the head of the
       list */

    toadd = (struct bu_hook_list *)bu_malloc(sizeof(struct bu_hook_list),
					    "bu_log hook");
    toadd->hookfunc = func;
    toadd->clientdata = clientdata;
    toadd->l.magic = BUHOOK_LIST_MAGIC;

    RT_LIST_APPEND( &(bu_log_hook_list.l), &(toadd->l) );
}


/*
 *			B U _ D E L E T E _ H O O K
 *
 *  Removes the hook matching the function and clientdata parameters from
 *  the hook list.  Note that it is not necessarily the active (top) hook.
 */
void
bu_delete_hook( func, clientdata )
bu_hook_t func;
genptr_t clientdata;
{
    struct bu_hook_list *cur = &bu_log_hook_list;

    for ( RT_LIST_FOR( cur, bu_hook_list, &(bu_log_hook_list.l) ) ) {
        if ( cur->hookfunc == func && cur->clientdata == clientdata) {
	    struct bu_hook_list *old = RT_LIST_PLAST(bu_hook_list, cur);
	    RT_LIST_DEQUEUE( &(cur->l) );
	    bu_free((genptr_t)cur, "bu_log hook");
	    cur = old;
	}
    }    
}

HIDDEN void
bu_call_hooks( buf )
genptr_t	buf;
{
    bu_hook_t hookfunc;		/* for clarity */
    genptr_t clientdata;

    bu_log_hooks_called = 1;

    hookfunc = RT_LIST_FIRST(bu_hook_list, &(bu_log_hook_list.l))->hookfunc;
    clientdata = RT_LIST_FIRST(bu_hook_list, &(bu_log_hook_list.l))->clientdata;

    (hookfunc)( clientdata, buf);

    bu_log_hooks_called = 0;
}

/*
 *			B U _ I N D E N T _ L E V E L
 *
 *  This subroutine is used to append bu_indent_cur_level spaces
 *  into a printf() format specifier string, after each newline
 *  character is encountered.
 *  It exists primarily for bu_shootray() to affect the indentation
 *  level of all messages at that recursion level, even if the calls
 *  to bu_log come from non-librt routines.
 */

HIDDEN void
bu_indent_level( new, old )
struct bu_vls *new;
register char *old;
{
    register int i;

    while (*old) {
	bu_vls_putc(new, (int)(*old));
	if (*old == '\n') {
	    i = bu_indent_cur_level;
	    while (i-- > 0)
		bu_vls_putc(new, ' ');
	}
	++old;
    }
}

/*
 *			B U _ P U T C H A R
 *
 * Log a single character with no flushing.
 */

void
bu_putchar( c )
int c;
{
    if ( RT_LIST_IS_EMPTY( &(bu_log_hook_list.l) ) ) {
	fputc(c, stderr);
    } else {
	char buf[2];
	buf[0] = (char)c;
	buf[1] = '\0';
	bu_call_hooks(buf);
    }

    if (bu_indent_cur_level > 0 && c == '\n') {
	int i;

	i = bu_indent_cur_level;
	while (i-- > 0)
	    bu_putchar(' ');
    }
}

/*
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
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);   /* VARARGS */
    }
#else
#  if defined(HAVE_VARARGS_H)
    va_start(ap);
    fmt = va_arg(ap, char *);
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
#  else                                     /* Cray XMP */
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
	bu_vls_printf(&output, bu_vls_addr(&newfmt), a,b,c,d,e,f,g,h,i,j);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_printf(&output, fmt, a,b,c,d,e,f,g,h,i,j);
    }
#  endif
#endif
    
    if ( RT_LIST_IS_EMPTY( &(bu_log_hook_list.l) )  || bu_log_hooks_called) {
    	int ret;
	int len;

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
	bu_call_hooks(bu_vls_addr(&output));
    }

#if defined(HAVE_STDARG_H) || defined(HAVE_VARARGS_H)
    va_end(ap);
#endif

    bu_vls_free(&output);
}

/*
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
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
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
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
	bu_vls_vprintf(&output, bu_vls_addr(&newfmt), ap);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_vprintf(&output, fmt, ap);
    }
#  else                                     /* Cray XMP */
    if (bu_indent_cur_level > 0) {
	struct bu_vls newfmt;

	bu_vls_init(&newfmt);
	bu_indent_level(&newfmt, fmt);
	bu_vls_printf(&output, bu_vls_addr(&newfmt), a,b,c,d,e,f,g,h,i,j);
	bu_vls_free(&newfmt);
    } else {
	bu_vls_printf(&output, fmt, a,b,c,d,e,f,g,h,i,j);
    }
#  endif
#endif
    
    if ( RT_LIST_IS_EMPTY( &(bu_log_hook_list.l) ) || bu_log_hooks_called) {
    	int ret;
	int len;

	len = bu_vls_strlen(&output);
	if(len){
	  bu_semaphore_acquire(BU_SEM_SYSCALL);
	  ret = fwrite( bu_vls_addr(&output), len, 1, fp );
	  bu_semaphore_release(BU_SEM_SYSCALL);
	  if( ret != 1 )  bu_bomb("bu_flog: write error");
	}

    } else {
	bu_call_hooks(bu_vls_addr(&output));
    }

#if defined(HAVE_STDARG_H) || defined(HAVE_VARARGS_H)
    va_end(ap);
#endif

    bu_vls_free(&output);
}
