static char rcsid[] = "$Id: error.c,v 1.13 1997/10/23 21:41:44 sauderd Exp $";

/************************************************************************
** Module:	Error
** Description:	This module implements the ERROR abstraction
************************************************************************/

/*
 * Development of this code was funded by the United States Government,
 * and is not subject to copyright.
 *
 * $Log: error.c,v $
 * Revision 1.13  1997/10/23 21:41:44  sauderd
 * I am backing out to version 1.10 before the changes related to va_list and
 * __STDC__ etc. I don't have time to finish fixing all this.
 *
 * Revision 1.12  1997/10/23 21:35:31  sauderd
 * Changed more mess with compiler directives for __STDC__ and HAVE_STDARG_H
 * but am getting deeper into problems because of how PROTO is defined for
 * dealing with prototypes based on standard C or not. PROTO is broken as well
 * so I'm not going down this road further. Next I will back out but am this
 * far ahead for later if we fix this.
 *
 * Revision 1.11  1997/10/22 16:10:26  sauderd
 * This would #include stdarg.h if __STDC__ was defined otherwise it would
 * #include vararg.h. I changed it to check the configure generated config file
 * named scl_cf.h (if HAVE_CONFIG_H is defined - it's also defined by
 * configure) to see if HAVE_STDARG_H is defined. If it is it #includes stdarg.h
 * otherwise it #includes vararg.h. If HAVE_CONFIG_H isn't defined then it works
 * like it used to.
 *
 * Revision 1.10  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.9  1994/05/11  19:51:46  libes
 * numerous fixes
 *
 * Revision 1.8  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/22  21:44:34  libes
 * ANSI compat fixes
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */

#include <stdlib.h>
#include "conf.h"
#include <setjmp.h>

#include "signal.h"
#include "express/error.h"
#include "string.h"
#include "express/linklist.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif


Boolean __ERROR_buffer_errors = False;
char *current_filename = "stdin";

/* flag to remember whether non-warning errors have occurred */
Boolean ERRORoccurred = False;


Error experrc = ERROR_none;
Error ERROR_subordinate_failed = ERROR_none;
Error ERROR_syntax_expecting = ERROR_none;

/* all of these are 1 if true, 0 if false switches */
/* for debugging fedex */
int ERRORdebugging = 0;
/* for debugging malloc during resolution */
int malloc_debug_resolve = 0;
/* for debugging yacc/lex */
int debug = 0;

struct Linked_List_ *ERRORwarnings;
struct freelist_head ERROR_OPT_fl;

void (*ERRORusage_function)(void);

#include "express/express.h"

#define ERROR_MAX_ERRORS	100	/* max line-numbered errors */
#define ERROR_MAX_SPACE		4000	/* max space for line-numbered errors */
#define ERROR_MAX_STRLEN	200	/* assuming all error messages are */
		/* less than this, if we have less than this much space */
		/* remaining in the error string buffer, call it a day and */
		/* dump the buffer */

static struct heap_element {
	int line;
	char *msg;
} heap[ERROR_MAX_ERRORS+1];	/* NOTE!  element 0 is purposely ignored, and */
		/* an additional element is at the end.  This allows the */
		/* later heap calculations to be much simpler */

static int ERROR_with_lines = 0;	/* number of warnings & errors */
				/* that have occurred with a line number */
static char *ERROR_string;
static char *ERROR_string_base;

static char ERROR_unsafe = False;
static jmp_buf ERROR_safe_env;

/* message buffer file */
#define error_file stderr

/*
** Procedure:	ERRORinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Error module
*/

void
ERRORinitialize(void)
{
    ERROR_subordinate_failed =
	ERRORcreate("A subordinate failed.", SEVERITY_ERROR);
    ERROR_syntax_expecting =
	ERRORcreate("%s, expecting %s in %s %s",SEVERITY_EXIT);

	ERROR_string_base = (char *)calloc(1, ERROR_MAX_SPACE);
	ERROR_start_message_buffer();


#ifdef SIGQUIT
	signal(SIGQUIT,ERRORabort);
#endif
#ifdef SIGBUS
	signal(SIGBUS, ERRORabort);
#endif
#ifdef SIGSEGV
	signal(SIGSEGV,ERRORabort);
#endif
#ifdef SIGABRT
	signal(SIGABRT,ERRORabort);
#endif
}

/* Need the LIST routines to complete ERROR initialization */
void
ERRORinitialize_after_LIST(void)
{
	ERRORwarnings = LISTcreate();

	MEMinitialize(&ERROR_OPT_fl,sizeof(struct Error_Warning_),5,5);	
}

/*VARARGS*/
void
ERRORcreate_warning(char *name,Error error)
{
	struct Error_Warning_ *o;

	/* first check if we know about this type of error */
	LISTdo(ERRORwarnings, opt, Error_Warning)
		if (streq(name,opt->name)) {
			LISTadd(opt->errors,(Generic)error);
			return;
		}
	LISTod

	/* new error */
	o = ERROR_OPT_new();
	o->name = name;
	o->errors = LISTcreate();
	LISTadd(o->errors,(Generic)error);
	LISTadd(ERRORwarnings,(Generic)o);
}

void
ERRORset_warning(char *name,int set)
{
	int found = False;

	if (streq(name,"all")) ERRORset_all_warnings(set);
	else if (streq(name,"none")) ERRORset_all_warnings(!set);
	else {
		LISTdo(ERRORwarnings, opt, Error_Warning)
			if (streq(opt->name,name)) {
				found = True;
				LISTdo(opt->errors, err, Error)
					err->enabled = set;
				LISTod
			}
		LISTod
		if (found) return;

		fprintf(stderr,"unknown warning: %s\n",name);
		if (ERRORusage_function) {
			(*ERRORusage_function)();
		}
	}
}

void
ERRORset_all_warnings(int set)
{
	LISTdo(ERRORwarnings, opts, Error_Warning)
		LISTdo(opts->errors, err, Error)
			err->enabled = set;
		LISTod
	LISTod
}

/*
** Procedure:	ERRORdisable
** Parameters:	Error error	- error to disable
** Returns:	void
** Description:	Disable an error (ERRORreport*() will ignore it)
*/

/* this function is inlined in error.h */

/*
** Procedure:	ERRORenable
** Parameters:	Error error	- error to enable
** Returns:	void
** Description:	Enable an error (ERRORreport*() will report it)
*/

/* this function is inlined in error.h */

/*
** Procedure:	ERRORis_enabled
** Parameters:	Error error	- error to test
** Returns:	Boolean		- is reporting of the error enabled?
** Description:	Check whether an error is enabled
*/

/* this function is inlined in error.h */

/*
** Procedure:	ERRORreport
** Parameters:	Error what	- error to report
**		...		- arguments for error string
** Returns:	void
** Description:	Print a report of an error
**
** Notes:	The second and subsequent arguments should match the
**		format fields of the message generated by 'what.'
*/

/*VARARGS*/
void
#ifdef __STDC__
ERRORreport(Error what, ...)
{
#else
ERRORreport(va_alist)
va_dcl
{
    Error what;
#endif
/*    extern void abort(void);*/
    va_list args;

#ifdef __STDC__
    va_start(args,what);
#else
    va_start(args);
    what = va_arg(args,Error);
#endif

    if ((what != ERROR_none) &&
	(what != ERROR_subordinate_failed) &&
	what->enabled) {
	    if (what->severity >= SEVERITY_ERROR) {
		fprintf(error_file, "ERROR: ");
		vfprintf(error_file, what->message, args);
		fputc('\n', error_file);
		ERRORoccurred = True;
	    } else if (what->severity >= SEVERITY_WARNING) {
		fprintf(error_file, "WARNING: ", what->severity);
		vfprintf(error_file, what->message, args);
		fputc('\n', error_file);
	    }
	    if (what->severity >= SEVERITY_EXIT) {
		ERROR_flush_message_buffer();
		if (what->severity >= SEVERITY_DUMP)
		    abort();
		else
		    exit(EXPRESS_fail((Express)0));
	    }
    }
    experrc = ERROR_none;
    va_end(args);
}

/*
** Procedure:	ERRORreport_with_line
** Parameters:	Error what	- error to report
**		int   line	- line number of error
**		...		- arguments for error string
** Returns:	void
** Description:	Print a report of an error, including a line number
**
** Notes:	The third and subsequent arguments should match the
**		format fields of the message generated by 'what.'
*/

/*VARARGS*/
void
#ifdef __STDC__
ERRORreport_with_line(Error what, int line, ...)
{
#else
ERRORreport_with_line(va_alist)
va_dcl
{
	Error what;
	int line;
#endif

	char buf[BUFSIZ];
	char *savemsg;	/* save what->message here while we fool */
			/* ERRORreport_with_line */
	Symbol sym;
	va_list args;
#ifdef __STDC__
	va_start(args,line);
#else
	va_start(args);
	what = va_arg(args,Error);
	line = va_arg(args,int);
#endif

	sym.filename = current_filename;
	sym.line = line;

	vsprintf(buf,what->message,args);

	/* gross, but there isn't any way to do this more directly */
	/* without writing yet another variant of ERRORreport_with_line */
	savemsg = what->message;
	what->message = "%s";
	ERRORreport_with_symbol(what,&sym,buf);
	what->message = savemsg;
}

/*VARARGS*/
void
#ifdef __STDC__
ERRORreport_with_symbol(Error what, Symbol *sym, ...)
{
#else
ERRORreport_with_symbol(va_alist)
va_dcl
{
    Error what;
    Symbol *sym;
#endif
/*    extern void abort(void);*/
    va_list args;

#ifdef __STDC__
    va_start(args,sym);
#else
    va_start(args);
    what = va_arg(args,Error);
    sym = va_arg(args,Symbol *);
#endif

    if ((what != ERROR_none) && (what != ERROR_subordinate_failed) && what->enabled) {
	if (__ERROR_buffer_errors) {
		int child, parent;

		/*
		 * add an element to the heap
		 * by (logically) storing the new value
		 * at the end of the array and bubbling
		 * it up as necessary
		 */

		child = ++ERROR_with_lines;
		parent = child/2;
		while (parent) {
			if (sym->line < heap[parent].line) {
				heap[child] = heap[parent];
			} else break;
			child = parent;
			parent = child/2;
		}
		heap[child].line = sym->line;
		heap[child].msg = ERROR_string;

	    if (what->severity >= SEVERITY_ERROR) {
		sprintf(ERROR_string, "%s:%d: --ERROR: ",sym->filename,sym->line);
		ERROR_string += strlen(ERROR_string);
		vsprintf(ERROR_string, what->message, args);
		ERROR_string += strlen(ERROR_string);
		*ERROR_string++ = '\n';
		*ERROR_string++ = '\0';
		ERRORoccurred = True;
	    } else if (what->severity >= SEVERITY_WARNING) {
		sprintf(ERROR_string, "%s:%d: WARNING: ",sym->filename,sym->line);
		ERROR_string += strlen(ERROR_string);
		vsprintf(ERROR_string, what->message, args);
		ERROR_string += strlen(ERROR_string);
		*ERROR_string++ = '\n';
		*ERROR_string++ = '\0';
	    }
	    if (what->severity >= SEVERITY_EXIT ||
		ERROR_string + ERROR_MAX_STRLEN > ERROR_string_base + ERROR_MAX_SPACE ||
		ERROR_with_lines == ERROR_MAX_ERRORS) {
		ERROR_flush_message_buffer();
		if (what->severity >= SEVERITY_DUMP)
		    abort();
		else
		    exit(EXPRESS_fail((Express)0));
	    }
	} else {
	    if (what->severity >= SEVERITY_ERROR) {
		fprintf(error_file, "%s:%d: --ERROR: ",sym->filename,sym->line);
		vfprintf(error_file, what->message, args);
		fprintf(error_file,"\n");
		ERRORoccurred = True;
	    } else if (what->severity >= SEVERITY_WARNING) {
		fprintf(error_file, "%s:%d: WARNING: ",sym->filename,sym->line);
		ERROR_string += strlen(ERROR_string) + 1;
		vfprintf(error_file, what->message, args);
		fprintf(error_file,"\n");
	    }
	    if (what->severity >= SEVERITY_EXIT) {
		if (what->severity >= SEVERITY_DUMP)
		    abort();
		else
		    exit(EXPRESS_fail((Express)0));
	    }
        }
    }
    experrc = ERROR_none;
    va_end(args);
}

void
ERRORnospace()
{
	fprintf(stderr,"%s: out of space\n",EXPRESSprogram_name);
	ERRORabort(0);
}

/*
** Procedure:	ERRORcreate
** Parameters:	String   message	- error message
**		Severity severity	- severity of error
** Returns:	Error			- newly created error
** Description:	Create a new error
*/

Error
ERRORcreate(char * message, Severity severity)
{
    Error n;

    n = (struct Error_ *)malloc(sizeof (struct Error_));
    n->message = message;
    n->severity = severity;
    n->enabled = True;
    return n;
}

/*
** Procedure:	ERRORbuffer_messages
** Parameters:	Boolean flag	- to buffer or not to buffer
** Returns:	void
** Description:	Selects buffering of error messages
*/

/* this function is inlined in error.h */

/*
** Procedure:	ERRORflush_messages
** Parameters:	void
** Returns:	void
** Description:	Flushes the error message buffer to standard output.
**
** Notes:	The error messages are sorted by line number (which appears
**		in the third column).
*/

/* this function is inlined in error.h */

/*
** Procedure:	ERROR_start_message_buffer
** Parameters:	-- none --
** Returns:	void
** Description:	
*/

void
ERROR_start_message_buffer(void)
{
	ERROR_string = ERROR_string_base;
	ERROR_with_lines = 0;
}

/*
** Procedure:	ERROR_flush_message_buffer
** Parameters:	-- none --
** Returns:	void
** Description:	
*/

void
ERROR_flush_message_buffer(void)
{
	if (__ERROR_buffer_errors == False) return;

	while (ERROR_with_lines) {
		struct heap_element *replace;
		int parent, child;

		/* pop off the top of the heap */
		fprintf(stderr,heap[1].msg);

		replace = &heap[ERROR_with_lines--];

		child = 1;
		while (1) {
			parent = child;
			child = 2*parent;
			if (child > ERROR_with_lines) break;
			if (child+1 <= ERROR_with_lines) {
				if (heap[child].line > heap[child+1].line) child++;
			}
			if (replace->line <= heap[child].line) break;
			heap[parent] = heap[child];
#if 0
			if (replace->line > heap[child].line) {
				heap[parent] = heap[child];
			}
#endif
		}
		heap[parent] = *replace;
	}
}

/*ARGSUSED*/
void
ERRORabort(int sig)
{
	ERRORflush_messages();
	if (!ERRORdebugging) {
		if (ERROR_unsafe) {
			longjmp(ERROR_safe_env,1);
		}
#ifdef SIGABRT
		signal(SIGABRT,SIG_DFL);
#endif
		abort();
	}

	fprintf(stderr,"pausing...press ^C now to enter debugger: ");
	pause();
}

void
ERRORsafe(jmp_buf env)
{
	memcpy(ERROR_safe_env,env,sizeof(jmp_buf));
}

void
ERRORunsafe()
{
	ERROR_unsafe = True;
}
