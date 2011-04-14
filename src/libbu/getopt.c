/*                        G E T O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include <stdio.h>
#include <string.h>

#include "bu.h"

/* globals available: bu_opterr, bu_optind, bu_optopt, bu_optarg
 * see globals.c for details
 */

#define BADCH (int)'?'
#define EMSG ""
#define tell(s)	if (bu_opterr) {		\
	fputs(*nargv, stderr);			\
	fputs(s, stderr);			\
	fputc(bu_optopt, stderr);		\
	fputc('\n', stderr);			\
    } return BADCH;


int
bu_getopt(int nargc, char * const nargv[], const char *ostr)
{
    static char *place = EMSG;	/* option letter processing */
    register char *oli;		/* option letter list index */

    if (*place=='\0') {
	/* update scanning pointer */
	if (bu_optind >= nargc || *(place = nargv[bu_optind]) != '-' ||
	    !*++place) {
	    place = EMSG;
	    return -1;
	}
	if (*place == '-') {
	    /* found "--" */
	    place = EMSG;
	    ++bu_optind;
	    return -1;
	}
    } /* option letter okay? */

    bu_optopt = (int)*place++;
    oli = strchr(ostr, bu_optopt);
    if (bu_optopt == (int)':' || !oli) {
	++bu_optind;
	place = EMSG;
	tell(": illegal option -- ");
    }
    if (*++oli != ':') {
	/* don't need argument */
	bu_optarg = NULL;
	if (*place == '\0') {
	    ++bu_optind;
	    place = EMSG;
	}
    }
    else {
	/* need an argument */
	if (*place) bu_optarg = place;	/* no white space */
	else if (nargc <= ++bu_optind) {
	    /* no arg */
	    place = EMSG;
	    tell(": option requires an argument -- ");
	}
	else bu_optarg = nargv[bu_optind];	/* white space */
	place = EMSG;
	++bu_optind;
    }
    return bu_optopt;			/* dump back option letter */
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
