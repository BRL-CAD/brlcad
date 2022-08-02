/*                           U - A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
 */
/** @file u-a.c
 *
 */

#include "common.h"
#include "bio.h"

#include "bu/app.h"
#include "bu/snooze.h"


int
main(int UNUSED(argc), const char **argv)
{
#define MAXBUF 16*1024
    unsigned short ibuf[MAXBUF];

    int n, i;

    bu_setprogname(argv[0]);

    fprintf(stderr,"DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    while ((n=fread(ibuf, sizeof(*ibuf), MAXBUF, stdin)) > 0) {
	for (i=0; i < n; ++i)
	    printf("%hd\n", ibuf[i]);
    }

    return 0;
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
