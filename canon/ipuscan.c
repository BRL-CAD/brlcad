/*	
 *			 I P U S C A N
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"

#if defined(IRIX) && (IRIX == 4 || IRIX == 5)
#include "./canon.h"

static 	struct dsreq *dsp;
static	int	fd;

struct chore {
	int	todo;
	int	buflen;
	int	pix_y;
	int	canon_y;
	unsigned char	*cbuf;			/* ptr to canon buffer */
	unsigned char	obuf[255*1024];
};

struct chore	chore1;

int step1(chorep, pix_y)
struct chore	*chorep;
int		pix_y;
{
	int	canon_y;

	chorep->pix_y = pix_y;
	chorep->todo = 255*1024 / (ipu_bytes_per_pixel*width);	/* Limit 255 Kbytes */
	if( height - pix_y < chorep->todo )  chorep->todo = height - pix_y;
	chorep->buflen = chorep->todo * ipu_bytes_per_pixel * width;

	canon_y = height - (pix_y+chorep->todo);

	chorep->cbuf = ipu_get_image(dsp, 1, 0, canon_y, width, chorep->todo);

	return pix_y + chorep->todo;
}

void step2(chorep)
struct chore	*chorep;
{
	register unsigned char	*cp;
	unsigned char *red, *green, *blue;
	int	buf_y;

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
			register int		x;
				offset = buf_y * width;
			rp = &chorep->cbuf[offset];
			bcopy( rp, cp, width );
			cp += width;
			chorep->pix_y++;	/* Record our progress */
		}
	}
}

/*
 *  While this step looks innocuous, if the file is located on a slow
 *  or busy disk drive or (worse yet) is on an NFS partition,
 *  this can take a long time.
 */
void step3(chorep)
struct chore	*chorep;
{
	if( write( fd, chorep->obuf, chorep->buflen ) != chorep->buflen )  {
		perror("ipuscan write");
		fprintf(stderr, "buffer write error, line %d\n", chorep->pix_y);
		exit(2);
	}
	(void)free(chorep->cbuf);
	chorep->cbuf = NULL;
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	int arg_index;
	int i;
	int	pix_y;

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

	sleep(15);
	ipu_acquire(dsp, 120);

	if (ipu_debug)
		fprintf(stderr, "%s\n", ipu_list_files(dsp));

	/* SCSI Bus can't do large ones all at once.
	 * Take it in small chunks.
	 * Also, note that the scanner is quadrant IV,
	 * while .pix files are quadrant I.
	 */
	for( pix_y=0; pix_y < height; )  {
		struct chore	*chorep = &chore1;

		pix_y = step1( chorep, pix_y );
		step2( chorep );
		step3( chorep );
	}

	close(fd);
	(void)dsclose(dsp);
	(void)close(fd);
	(void)chmod(av[arg_index], 0444);
	return(0);
}
#else
int
main(ac, av)
int ac;
char *av[];
{
	fprintf(stderr,
		"%s only works on SGI(tm) systems with dslib support\n", *av);
	return(-1);
}
#endif
