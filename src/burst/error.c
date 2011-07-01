/*                         E R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/error.c
 *
 * Ray Tracing library and Framebuffer library, error handling routines.
 *
 * Functions -
 * brst_log Called to log RT library events.
 * fb_log Called to log FB library events.
 *
 */

#include "common.h"

#include <stdio.h>
#include <signal.h>

#include "./Sc.h"
#include "./extern.h"

/*
 * B R S T _ L O G
 *
 * Log an RT library event
 */
void
brst_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (tty && (errfile[0] == '\0' || BU_STR_EQUAL(errfile, "/dev/tty"))) {
	clr_Tabs(HmTtyFd);
	if (ScDL != NULL) {
	    (void) ScMvCursor(1, SCROLL_TOP);
	    (void) ScDeleteLn();
	    (void) ScMvCursor(1, SCROLL_BTM);
	    (void) ScClrEOL();
	    (void) vprintf(fmt, ap);
	} else {
	    if (ScSetScrlReg(SCROLL_TOP, SCROLL_BTM+1)) {
		char buf[LNBUFSZ];
		char *p;
		(void) ScMvCursor(1, SCROLL_BTM+1);
		(void) ScClrEOL();
		/* Work around for problem with vprintf(): it doesn't
		   cause the screen to scroll, don't know why. */
		(void) vsprintf(buf, fmt, ap);
		/* Newline will cause double scroll. */
		p = buf+strlen(buf)-1;
		if (*p == '\n')
		    *p = '\0'; /* clobber newline */
		(void) puts(buf);
		/*(void) vprintf(fmt, ap);*/
		(void) ScMvCursor(1, SCROLL_BTM+1);
		(void) ScClrScrlReg();
	    } else {
		(void) fprintf(stderr,
			       "%s %s %s %s!\n",
			       "This terminal has no delete line",
			       "or scrolling region capability, ",
			       "please dump it somewhere and get",
			       "a real terminal"
		    );
		bu_exit(1, NULL);
	    }
	}
	(void) fflush(stdout);
    } else {
	(void) vfprintf(stderr, fmt, ap);
    }
    va_end(ap);
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
