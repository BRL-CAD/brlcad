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
	unsigned char *red, *green, *blue;
	struct dsreq *dsp;
	int	fd;
	int i;
	int bufpos;
	unsigned char	*buf = (unsigned char *)NULL;
	int	canon_y;
	int	pix_y;
	int	buf_y;
	int	x;

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) < ac) {
		if ((fd=open(av[arg_index], 0)) < 0) {
			(void)fprintf(stderr, "%s: ", progname);
			perror(av[arg_index]);
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
	ipu_create_file(dsp, 1, IPU_RGB_FILE, width, height, 0);
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
		register unsigned char	*cp;
		int	todo;	/* # scanlines to do */

		todo = 255*1024 / (3*width);	/* Limit 255 Kbytes */
		if( !buf )  buf = (unsigned char *)malloc(todo*3*width);
		if( height - pix_y < todo )  todo = height - pix_y;

		canon_y = height - (pix_y+todo);

		red = ipu_get_image(dsp, 1, 0, canon_y, width, todo);

		green = &red[width*todo];
		blue = &red[width*todo*2];
		cp = buf;

		for( buf_y = todo-1; buf_y >= 0; buf_y-- )  {
			int	offset;

			offset = buf_y * width;
			for( x=0; x < width; x++ )  {
				*cp++ = red[offset+x];
				*cp++ = green[offset+x];
				*cp++ = blue[offset+x];
			}
			pix_y++;	/* Record our progress */
		}
		(void)free(red);

		/* Large buffer write */
		if (write(fd, buf, todo*width*3) != todo*width*3) {
			fprintf(stderr, "buffer write error, line %d\n", pix_y);
			return(-1);
		}
	}

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
