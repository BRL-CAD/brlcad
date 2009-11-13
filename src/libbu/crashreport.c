/*                   C R A S H R E P O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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

/* system headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* common headers */
#include "bu.h"
#include "brlcad_version.h"


#define CR_BUFSIZE 2048
static char buffer[CR_BUFSIZE] = {0};
static FILE *fp = NULL;
static FILE *popenfp = NULL;
static time_t now;
static const char *path = NULL;


int
bu_crashreport(const char *filename)
{
    if (!filename) {
	return 0;
    }

    fp = fopen(filename, "ab");
    if (!fp) {
	perror("unable to open crash report file");
	bu_log("ERROR: Unable to open crash report file [%s]\n", filename);
	return 0;
    }

    /* vat time ist? */
    (void)time(&now);

    path = bu_which(bu_argv0());

    /* do our own expansion to avoid heap allocation */
    snprintf(buffer, CR_BUFSIZE, "******************************************\n\n"
	     "%s\n"		/* version info */
	     "Command: %s\n"	/* argv0 */
	     "Process: %d\n"	/* pid */
	     "Path: %s\n"	/* which binary */
	     "Date: %s\n",	/* date/time */
	     brlcad_ident("Crash Report"),
	     bu_argv0(),
	     bu_process_id(),
	     path ? path : "Unknown",
	     ctime(&now));

    /* print the report header */
    if (fprintf(fp, (const char *)buffer) <= 0) {
	/* cannot bomb */
	bu_log("ERROR: Unable to write to crash report file [%s]\n", filename);
	(void)fclose(fp);
	fp = NULL;
	return 0;
    }

    /* write out the backtrace */
    fprintf(fp, "Call stack backtrace:\n");
    fflush(fp);
    if (bu_backtrace(fp) == 0) {
	bu_log("WARNING: Unable to obtain a call stack backtrace\n");
    }

    /* write out operating system information */
    path = bu_which("uname");
    if (path) {
	snprintf(buffer, CR_BUFSIZE, "%s -a 2>&1", path);
	popenfp = popen(buffer, "r");
	if (!popenfp) {
	    perror("unable to popen uname");
	    bu_log("WARNING: Unable to obtain uname information\n");
	} else {
	    fprintf(fp, "\nSystem characteristics:\n");
	    while (bu_fgets(buffer, CR_BUFSIZE, popenfp)) {
		fprintf(fp, "%s", buffer);
	    }
	}
	(void)pclose(popenfp);
	popenfp = NULL;
	path = NULL;
    }

    /* write out kernel and hardware information */
    path = bu_which("sysctl");
    if (path) {
	/* need 2>&1 to capture stderr junk from sysctl on Mac OS X for kern.exec */
	snprintf(buffer, CR_BUFSIZE, "%s -a 2>&1", path);
	popenfp = popen(buffer, "r");
	if (popenfp == (FILE *)NULL) {
	    perror("unable to popen sysctl");
	    bu_log("WARNING: Unable to obtain sysctl information\n");
	} else {
	    fprintf(fp, "\nSystem information:\n");
	    while (bu_fgets(buffer, CR_BUFSIZE, popenfp)) {
		if ((strlen(buffer) == 0) || ((strlen(buffer) == 1) && (buffer[0] == '\n'))) {
		    continue;
		}
		fprintf(fp, "%s", buffer);
	    }
	}
	(void)pclose(popenfp);
	popenfp = NULL;
	path = NULL;
    }

    memset(buffer, 0, CR_BUFSIZE);
    fprintf(fp, "\n");
    fflush(fp);
    (void)fclose(fp);
    fp = NULL;

    /* success */
    return 1;
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
