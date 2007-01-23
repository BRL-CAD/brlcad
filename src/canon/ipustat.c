/*                       I P U S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file ipustat.c
 *
 *	print the status of a Canon CLC500 IPU-10.
 *
 *	Options
 *	h	help
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "./canon.h"

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{

#ifdef IPU_FULL_LIB

    struct dsreq *dsp;
    char *p;

    if ((parse_args(ac, av)) < ac)
	fprintf(stderr,
		"%s: Excess command line arguments ignored\n",
		progname);


    if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
	perror(scsi_device);
	fprintf(stderr, "%s: Cannot open device \"%s\"\n", progname, scsi_device);
	usage(NULL);
    }


    printf("%s\n", ipu_inquire(dsp));
    ipu_get_conf(dsp);

    ipu_remote(dsp);

    printf("%s\n", p=ipu_list_files(dsp));

    free(p);

    dsclose(dsp);

    return(0);

#else /* !IPU_FULL_LIB */

    fprintf(stderr,
	    "%s only works on SGI(tm) systems with dslib support\n",
	    *av);
    return(-1);

#endif /* IPU_FULL_LIB */

}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
