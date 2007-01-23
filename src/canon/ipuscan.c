/*                       I P U S C A N . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2007 United States Government as represented by
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
/** @file ipuscan.c
 *
 *  Obtain a BRL-CAD .pix file from the Canon CLC-500 scanner glass.
 *
 *	Options
 *	h	help
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"

#include "./canon.h"

#ifdef IPU_FULL_LIB

# define _SGI_SOURCE	1	/* IRIX 5.0.1 needs this to def M_BLKSZ */
# define _BSD_TYPES	1	/* IRIX 5.0.1 botch in sys/prctl.h */
# include <ulocks.h>

#include "./chore.h"

static 	struct dsreq *dsp;
static	int	fd;

struct chore	chores[3];

struct chore	*await1;
struct chore	*await2;
struct chore	*await3;

void step1(aa)
     void	*aa;
{
    struct chore	*chorep;
    int		pix_y;
    int		canon_y;
    static int	nstarted = 0;

    pix_y = 0;
    for(;;)  {
	if( nstarted < 3 )  {
	    chorep = &chores[nstarted++];
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

	canon_y = height - (pix_y+chorep->todo);

	chorep->cbuf = ipu_get_image(dsp, 1, 0, canon_y, width, chorep->todo);
	pix_y += chorep->todo;

	/* Pass this chore off to next process */
	PUT( await2, chorep );
    }
    exit(0);	/* exit this thread */
}

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
		    *cp++ = *rp++;
		    *cp++ = *gp++;
		    *cp++ = *bp++;
		}
		chorep->pix_y++;	/* Record our progress */
	    }
	} else {
	    /* Monochrome */
	    for( buf_y = chorep->todo-1; buf_y >= 0; buf_y-- )  {
		int	offset;
		register unsigned char	*rp;
		offset = buf_y * width;
		rp = &chorep->cbuf[offset];
		bcopy( rp, cp, width );
		cp += width;
		chorep->pix_y++;	/* Record our progress */
	    }
	}
	PUT( await3, chorep );
    }
    exit(0);
}

/*
 *  While this step looks innocuous, if the file is located on a slow
 *  or busy disk drive or (worse yet) is on an NFS partition,
 *  this can take a long time.
 */
void step3(aa)
     void	*aa;
{
    struct chore	*chorep;

    for(;;)  {
	GET( chorep, await3 );
	if( chorep->pix_y < 0 )  {
	    break;	/* "done" token */
	}

	if( write( fd, chorep->obuf, chorep->buflen ) != chorep->buflen )  {
	    perror("ipuscan write");
	    fprintf(stderr, "buffer write error, line %d\n", chorep->pix_y);
	    exit(2);
	}
	(void)free(chorep->cbuf);
	chorep->cbuf = NULL;
	PUT( await1, chorep );
    }
    exit(0);
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{
    int arg_index;
    int i;
    int	pid[3];

    /* pick the LUN for the scanner this time */
    (void)strncpy( scsi_device, "/dev/scsi/sc0d6l0", 1024 );

    /* parse command flags, and make sure there are arguments
     * left over for processing.
     */
    if ((arg_index = parse_args(ac, av)) < ac) {
	if ((fd=creat(av[arg_index], 0444)) == -1) {
	    perror(av[arg_index]);
	    (void)fprintf(stderr, "%s: ", progname);
	    return(-1);
	}
    } else if (isatty(fileno(stdout))) {
	usage("Cannot scan to tty\n");
    } else
	fd = fileno(stdout);

    if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
	perror(scsi_device);
	usage("Cannot open SCSI device\n");
    }

    if (ipu_debug)
	fprintf(stderr, "%s\n", ipu_inquire(dsp));

    ipu_remote(dsp);
    ipu_delete_file(dsp, 1);
    /* Don't bother clearing memory, it takes too long */
    ipu_create_file(dsp, 1, ipu_filetype, width, height, 0);
    ipu_scan_config(dsp,units,divisor,conv,0,0);

    if (conv == IPU_AUTOSCALE)
	ipu_scan_file(dsp,1/*id*/,
		      0/*wait*/,0,0,0,0,&param);
    else
	ipu_scan_file(dsp,1/*id*/,
		      0/*wait*/,scr_xoff,scr_yoff,
		      width,height,&param);

    ipu_acquire(dsp, 30);

    if (ipu_debug)
	fprintf(stderr, "%s\n", ipu_list_files(dsp));



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

    close(fd);
    (void)dsclose(dsp);
    (void)close(fd);
    (void)chmod(av[arg_index], 0444);
    return(0);
}

#else /* !IPU_FULL_LIB */

int
main(int ac, char *av[])
{
    fprintf(stderr,
	    "%s only works on SGI(tm) systems with dslib support\n", *av);
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
