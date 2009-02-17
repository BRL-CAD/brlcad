/*                      M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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

/** @file metaball.c
 *
 * He slimed me!
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#define FMTSIZ 64

static const char usage[] = "\
Usage:\n\
\t%s [-h] [-n number of frames] [-m method] [-o outfile]\n\
\n\
\t-h\tShow help\n\
\t-n #\tNumber of frames to (objects) to create\n\
\t-m #\tMetaball render method (1 for iso, 2 for Blinn)\n\
\t-t title\tTitle of geometry\n\
\t-o file\tFile to write to\n\
\t-g\tGravotronic animation\n\
\t-c #\tNumber of control points (require Gravotronic mode)\n\
\n";

/******************************************************************/

/* hideous hack to generate a hideous dynamic format string for snprintf. *sob*/
int
mkfmt(char *fmt, char *prefix, char *postfix, int nframes)
{
    return snprintf(fmt, FMTSIZ, "%s%%0%dd%s", prefix, (int)ceil(log10((double)nframes)), postfix);
}

int
bitronic(struct rt_wdb *outfp, int method, fastf_t threshold, int nframes)
{
    /*
    char buf[BUFSIZ];
    */
    char fmt[FMTSIZ];
    fastf_t step;
    struct rt_metaball_internal *mb;

    BU_GETSTRUCT( mb, rt_metaball_internal );
    mb->magic = RT_METABALL_INTERNAL_MAGIC;
    mb->threshold = threshold > 0 ? threshold : 1.0;
    mb->method = method >= 0 ? method : 0;	/* default to Blinn blob */

    step = 2.0 / (double) nframes;

    mkfmt(fmt, "mball", ".s", nframes);

#if 0
    bleh[0].l->next = &bleh[1];
    bleh[1].l->next = NULL;

    while(nframes--) {
	struct wdb_metaballpt *mbpt;
	snprintf(buf, BUFSIZ, fmt, nframes);
#define PT(B, X,Y,Z,FLDSTR,GOO)	B.type = WDB_METABALLPT_TYPE_POINT; B.fldstr = FLDSTR; B.sweat = GOO; VSET(B.coord, X, Y, Z);
	BU_GETSTRUCT( mbpt, wdb_metaballpt );
	PT(bleh[0], - ((double)nframes) * step, 0, 0, 1, 1);
	PT(bleh[1], ((double)nframes) * step, 0, 0, 1, 1);
#undef PT
	BU_LIST_INSERT( &mb->metaball_ctrl_head, &mbpt->l );
	/*
	mk_metaball(outfp, buf, 2, method, threshold, bleh);
	*/
	printf("%s\t%f %f\n", buf, - ((double)nframes) * step, ((double)nframes) * step);
    }
#endif
    return EXIT_FAILURE;
}

int
gravotronic(struct rt_wdb *outfp, int method, fastf_t threshold, int nframes, int count)
{
    char buf[BUFSIZ], fmt[FMTSIZ];

    mkfmt(fmt, "mball", ".s", nframes);
    snprintf(buf, BUFSIZ, fmt, nframes);
    /* probably need a list or something
    BU_LIST_INIT( &head.l );
    */
    
    /* do something
    mk_lfcomb( outfp, "mball.g", &head, 0 );
    */

    return EXIT_FAILURE;
}

/******************************************************************/

int
main(int argc, char **argv)
{
    char title[BUFSIZ] = "metaball", outfile[MAXPATHLEN] = "metaball.g";
    int nframes = 1, method = 1, optc, gravotron = 0, count = 0, retval = EXIT_SUCCESS;
    struct rt_wdb *outfp;

    while ( (optc = bu_getopt( argc, argv, "Hhm:n:o:t:" )) != -1 )
	switch (optc) {
	    case 'n' :
		nframes = atoi(bu_optarg);
		break;
	    case 'm' :
		method = atoi(bu_optarg);
		break;
	    case 't':
		snprintf(title, BUFSIZ, "%s", bu_optarg);;
		break;
	    case 'o':
		snprintf(outfile, MAXPATHLEN, "%s", bu_optarg);;
		break;
	    case 'g':
		++gravotron;
		break;
	    case 'c':
		count = atoi(bu_optarg);
		break;
	    case 'h' :
	    case 'H' :
	    case '?' :
		printf(usage, *argv);
		return optc == '?' ? EXIT_FAILURE : EXIT_SUCCESS;
	}

    /* sanity checking on the various parameters */

    if(count && !gravotron) {
	fprintf(stderr, "Count only works in gravotronic mode. Turning on the gravitrons.\n");
	++gravotron;
    }
	
    /* prepare for DB generation */

    outfp = wdb_fopen( outfile );
    if( outfp == RT_WDB_NULL ) {
	perror("Failed to open file for writing. Aborting.\n");
	return EXIT_FAILURE;
    }
    mk_id( outfp, title );

    /* here we go! */

    if(gravotron)
	retval = gravotronic(outfp, method, 1.0, nframes, count);
    else
	retval = bitronic(outfp, method, 1.0, nframes);

    /* and clean up  */

    wdb_close(outfp);

    if(retval != EXIT_SUCCESS) {
	fprintf(stderr, "Removing \"%s\".\n", outfile);
	if(unlink(outfile) == -1)
	    perror("Failure removing: ");
    }

    return retval;
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
