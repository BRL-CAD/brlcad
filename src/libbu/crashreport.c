/*                   C R A S H R E P O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
    if (UNLIKELY(!filename || strlen(filename) == 0)) {
	return 0;
    }

    /* vat time ist? */
    (void)time(&now);

    path = bu_argv0_full_path();

    /* do our own expansion to avoid heap allocation */
    snprintf(buffer, CR_BUFSIZE, "******************************************\n\n"
	     "%s\n"		/* version info */
	     "Command: %s\n"	/* program name */
	     "Process: %d\n"	/* pid */
	     "Path: %s\n"	/* which binary */
	     "Date: %s\n",	/* date/time */
	     brlcad_ident("Crash Report"),
	     bu_getprogname(),
	     bu_process_id(),
	     path ? path : "Unknown",
	     ctime(&now));

    fp = fopen(filename, "ab");
    if (UNLIKELY(!fp || ferror(fp))) {
	perror("unable to open crash report file");
	bu_log("ERROR: Unable to open crash report file [%s]\n", filename);
	return 0;
    }

    /* make the file stream unbuffered */
    if (setvbuf(fp, (char *)NULL, _IONBF, 0) != 0) {
	perror("unable to make stream unbuffered");
    }

    /* print the report header */
    if (fwrite(buffer, 1, strlen(buffer), fp) != strlen(buffer)) {
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
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	popenfp = popen(buffer, "r");
	if (!popenfp) {
	    perror("unable to popen uname");
	    bu_log("WARNING: Unable to obtain uname information\n");
	}
#endif
	if (popenfp) {
	    fprintf(fp, "\nSystem characteristics:\n");
	    fflush(fp);
	    while (bu_fgets(buffer, CR_BUFSIZE, popenfp)) {
		size_t ret;
		size_t len;

		len = strlen(buffer);
		ret = fwrite(buffer, 1, len, fp);
		if (ret != len)
		    perror("fwrite failed");
	    }
	}
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	(void)pclose(popenfp);
#endif
	popenfp = NULL;
	path = NULL;
    }

    /* write out kernel and hardware information */
    path = bu_which("sysctl");
    if (path) {
	/* need 2>&1 to capture stderr junk from sysctl on Mac OS X for kern.exec */
	snprintf(buffer, CR_BUFSIZE, "%s -a 2>&1", path);
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	popenfp = popen(buffer, "r");
	if (popenfp == (FILE *)NULL) {
	    perror("unable to popen sysctl");
	    bu_log("WARNING: Unable to obtain sysctl information\n");
	}
#endif
	if (popenfp != (FILE *)NULL) {
	    fprintf(fp, "\nSystem information:\n");
	    fflush(fp);
	    while (bu_fgets(buffer, CR_BUFSIZE, popenfp)) {
		size_t ret;
		size_t len;
		
		len = strlen(buffer);
		if ((len == 0) || (len == 1) && (buffer[0] == '\n')) {
		    continue;
		}

		ret = fwrite(buffer, 1, len, fp);
		if (ret != len)
		    perror("fwrite failed");
	    }
	}
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	(void)pclose(popenfp);
#endif
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
