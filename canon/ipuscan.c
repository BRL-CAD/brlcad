/*	
 *			 I P U S C A N
 *
 *  Obtain a BRL-CAD .pix file from the Canon CLC-500 scanner glass.
 *
 *	Options
 *	h	help
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canon.h"


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
	u_char *red, *green, *blue;
	struct dsreq *dsp;
	FILE *fd;
	int i;
	int bufpos;
	u_char buf[3*10240];
	int	canon_y;
	int	pix_y;
	int	buf_y;
	int	x;

	/* pick the LUN for the scanner this time */
	scsi_device = "/dev/scsi/sc0d6l0";

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) < ac) {
		if ((fd=fopen(av[arg_index], "w")) == (FILE *)NULL) {
			(void)fprintf(stderr, "%s: ", progname);
			perror(av[arg_index]);
			return(-1);
		}
	} else if (isatty(fileno(stdout))) {
		usage("Cannot scan to tty\n");
	} else
		fd = stdout;

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
		int	todo;	/* # scanlines to do */

		todo = 255*1024 / (3*width);	/* Limit 255 Kbytes */
		if( height - pix_y < todo )  todo = height - pix_y;

		canon_y = height - (pix_y+todo);

		red = ipu_get_image(dsp, 1, 0, canon_y, width, todo);

		green = &red[width*todo];
		blue = &red[width*todo*2];

		for( buf_y = todo-1; buf_y >= 0; buf_y-- )  {
			int	offset;
			register unsigned char	*cp;
			cp = buf;
			offset = buf_y * width;
			for( x=0; x < width; x++ )  {
				*cp++ = red[offset+x];
				*cp++ = green[offset+x];
				*cp++ = blue[offset+x];
			}
			if (fwrite(buf, width*3, 1, fd) != 1) {
				fprintf(stderr, "buffer write error, line %d\n", pix_y);
				return(-1);
			}
			pix_y++;	/* Record our progress */
		}
		(void)free(red);
	}

	(void)dsclose(dsp);
	(void)chmod(av[arg_index], 0444);
	return(0);
}
