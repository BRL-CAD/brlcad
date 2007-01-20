/*                       P I X - I P U . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file pix-ipu.c
 *
 *  Print a BRL-CAD .pix or .bw file on the Canon CLC-500 scanner.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *
 *
 *	Options
 *	a	autosize image file
 *	c	clear framebuffer first
 *	d	SCSI device
 *	g	gamma
 *	h	1Kx1K
 *	m	mosaic
 *	n	scanlines (image)
 *	s	squaresize (image)
 *	w	width (image)
 *	x	file_xoffset
 *	y	file_yoffset
 *	z	zoom image display
 *	A	Autoscale
 *	M	Mag_factor
 *	R	Resolution
 *	C	# copies
 *	D	Divisor
 *	N	scr_height
 *	S	scr_height
 *	U	units ( i | m )
 *	W	scr_width
 *	X	scr_xoffset
 *	Y	scr_yoffset
 *	v	verbose;
 *	V	verbose;
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"

#include "./canon.h"

#ifdef IPU_FULL_LIB

#define _SGI_SOURCE	1	/* IRIX 5.0.1 needs this to def M_BLKSZ */
#define _BSD_TYPES	1	/* IRIX 5.0.1 botch in sys/prctl.h */
#include <ulocks.h>
/* ulocks.h #include's <limits.h> and <malloc.h> */
/* ulocks.h #include's <task.h> for getpid stuff */
/* task.h #include's <sys/prctl.h> */
#include <malloc.h>
/* <malloc.h> #include's <stddef.h> */
#include <fcntl.h>
#include <stdlib.h>

#include "./chore.h"


static 	struct dsreq *dsp;
static	int	fd;

struct chore	chores[3];

struct chore	*await1;
struct chore	*await2;
struct chore	*await3;


/*
 *  While this step looks innocuous, if the file is located on a slow
 *  or busy disk drive or (worse yet) is on an NFS partition,
 *  this can take a long time.
 */
void step1(aa)
     void *aa;
{
    struct chore	*chorep;
    int		pix_y;
    static int	nstarted = 0;

    pix_y = 0;
    for(;;)  {
	if( nstarted < 3 )  {
	    chorep = &chores[nstarted++];
	    chorep->cbuf = malloc( 255*1024 );
	} else {
	    GET( chorep, await1 );
	}

	if( pix_y >= height )  {
	    /* Send through a "done" chore and exit */
	    chorep->pix_y = -1;
	    PUT( await2, chorep );
	    /* Wait for them to percolate through */
	    GET( chorep, await1 );
	    GET( chorep, await1 );
	    break;
	}

	chorep->pix_y = pix_y;
	chorep->todo = 255*1024 / (ipu_bytes_per_pixel*width);	/* Limit 255 Kbytes */
	if( height - pix_y < chorep->todo )  chorep->todo = height - pix_y;
	chorep->buflen = chorep->todo * ipu_bytes_per_pixel * width;

	if( bu_mread( fd, chorep->obuf, chorep->buflen ) != chorep->buflen )  {
	    perror("pix-ipu READ ERROR");
	    fprintf(stderr, "buffer read error, line %d\n", chorep->pix_y);
	    exit(2);
	}
	pix_y += chorep->todo;

	/* Pass this chore off to next process */
	PUT( await2, chorep );
    }
    exit(0);
}

/* format conversion */
void step2(aa)
     void	*aa;
{
    struct chore	*chorep;
    register unsigned char	*cp;
    unsigned char *green, *blue;
    int	buf_y;

    for(;;)  {
	GET(chorep, await2);
	if( chorep->pix_y < 0 )  {
	    /* Pass on "done" token and exit */
	    PUT( await3, chorep );
	    break;
	}

	cp = chorep->obuf;

	if( ipu_bytes_per_pixel == 3 )  {
	    green = &chorep->cbuf[width*chorep->todo];
	    blue = &chorep->cbuf[width*chorep->todo*2];

	    for( buf_y = chorep->todo-1; buf_y >= 0; buf_y-- )  {
		int	offset;
		register unsigned char	*rp, *gp, *bp;
		register int		x;

		offset = buf_y * width;
		rp = &chorep->cbuf[offset];
		gp = &green[offset];
		bp = &blue[offset];
		for( x = width-1; x >= 0; x-- )  {
		    *rp++ = *cp++;
		    *gp++ = *cp++;
		    *bp++ = *cp++;
		}
	    }
	} else {
	    /* Monochrome */
	    for( buf_y = chorep->todo-1; buf_y >= 0; buf_y-- )  {
		int	offset;
		register unsigned char	*rp;
		offset = buf_y * width;
		rp = &chorep->cbuf[offset];
		bcopy( cp, rp, width );
		cp += width;
	    }
	}
	PUT( await3, chorep );
    }
    exit(0);
}

/* output via SCSI bus to IPU.  This is the time consuming step. */
void step3(aa)
     void	*aa;
{
    struct chore	*chorep;
    int		canon_y;

    for(;;)  {
	GET( chorep, await3 );
	if( chorep->pix_y < 0 )  {
	    break;	/* "done" token */
	}

	canon_y = height - chorep->pix_y - chorep->todo;

	ipu_put_image_frag(dsp, 1, 0, canon_y, width, chorep->todo, chorep->cbuf);

	/* Pass this chore off to next process for recycling */
	PUT( await1, chorep );
    }
    exit(0);	/* exit this thread */
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int
main(int ac, char *av[])
{
    int arg_index;
    int i;
    int	pid[3];

    if ((arg_index = parse_args(ac, av)) >= ac) {
	if (isatty(fileno(stdin)))
	    usage("Specify image on cmd line or redirect from standard input\n");

	if (autosize) fprintf(stderr, "Cannot autosize stdin\n");

    } else if (arg_index+1 < ac)
	(void)fprintf(stderr,
		      "%s: Excess command line arguments ignored\n", *av);
    else if (freopen(av[arg_index], "r", stdin) == NULL) {
	perror(av[arg_index]);
	return(-1);
    } else if (autosize &&
	       !fb_common_file_size( &width, &height, av[arg_index], ipu_bytes_per_pixel)) {
	fprintf(stderr, "unable to autosize\n");
    }

    /* open the printer SCSI device */
    if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
	perror(scsi_device);
	usage("Cannot open SCSI device\n");
    }

    if (ipu_debug)
	fprintf(stderr, "Image is %dx%d (%d)\n", width, height, width*height*ipu_bytes_per_pixel);

    if (conv == IPU_RESOLUTION) {
	if (scr_width)
	    scr_width *= 400.0 / (double)param.i;
	else
	    scr_width = width * 400.0 / (double)param.i;
	if (scr_height)
	    scr_height *= 400.0 / (double)param.i;
	else
	    scr_height = height * 400.0 / (double)param.i;
    } else if (conv == IPU_MAG_FACTOR) {
	if (scr_width)
	    scr_width *= 400.0 / (double)param.i;
	else
	    scr_width = width * 400.0 / (double)param.i;
	if (scr_height)
	    scr_height *= 400.0 / (double)param.i;
	else
	    scr_height = height * 400.0 / (double)param.i;
    }

    /* Wait for printer to finish what it was doing */
    ipu_acquire(dsp, 120);

    ipu_delete_file(dsp, 1);
    ipu_create_file(dsp, (char)1, ipu_filetype, width, height, 0);

    /* Stream file into the IPU */
    /* Start three threads, then wait for them to finish */
    pid[0] = sproc( step1, PR_SALL|PR_SFDS );
    pid[1] = sproc( step2, PR_SALL|PR_SFDS );
    pid[2] = sproc( step3, PR_SALL|PR_SFDS );

    for( i=0; i<3; i++ )  {
	int	this_pid;
	int	pstat;
	int	j;

	pstat = 0;
	if( (this_pid = wait(&pstat)) <= 0  )  {
	    perror("wait");
	    fprintf(stderr, "wait returned %d\n", this_pid);
	    for( j=0; j<3; j++) kill(pid[j], 9);
	    exit(3);
	}
	if( (pstat & 0xFF) != 0 )  {
	    fprintf(stderr, "*** child pid %d blew out with error x%x\n", this_pid, pstat);
	    for( j=0; j<3; j++) kill(pid[j], 9);
	    exit(4);
	}
    }
    /* All children are finished */

    ipu_print_config(dsp, units, divisor, conv,
		     mosaic, ipu_gamma, tray);

    if( ipu_filetype == IPU_PALETTE_FILE )
	ipu_set_palette(dsp, NULL);

    if (strcmp(progname, "pix-ipu")==0)
	ipu_print_file(dsp, (char)1, copies, 0/*wait*/,
		       scr_xoff, scr_yoff, scr_width, scr_height, &param);

    /* Wait for print operation to complete */
    ipu_acquire(dsp, 30 * copies);

    dsclose(dsp);
    return(0);
}

#else /* !IPU_FULL_LIB */

int
main(int ac, char *av[])
{
    fprintf(stderr,
	    "%s only works on SGI(tm) systems with dslib (direct SCSI library) support\n", *av);
    return(-1);
}

#endif /* IPU_FULL_LIB */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
