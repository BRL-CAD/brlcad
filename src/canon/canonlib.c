/*                      C A N O N L I B . C
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
/** @file canonlib.c
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#include <ctype.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif


#include "machine.h"

#include "./canon.h"

#define BUFSIZE 262144	/* 256Kbytes size of image transfer buffer */


int ipu_debug = 0;

/* this is from some imaginary libds.so ?
 * was extern'd, but without that lib, it failed... so we punt.
 */
int dsdebug = 0;

static void
toshort(dest, src)
     u_char	*dest;
     int	src;
{
    dest[0] = (u_char)(src >> 8);
    dest[1] = (u_char)src;
}
static void
toint(dest, src)
     u_char	*dest;
     int	src;
{
    dest[0] = (u_char)(src >> 24);
    dest[1] = (u_char)(src >> 16);
    dest[2] = (u_char)(src >> 8);
    dest[3] = (u_char)src;
}
#if defined(IRIX) && (IRIX == 4 || IRIX == 5 || IRIX == 6)
static void scsi_perror(int val, struct dsreq *dsp)
{
    fprintf(stderr, "doscsicmd retuns: %d ds_ret: 0x%02x   ds_status: 0x%02x  ds_msg: 0x%02x\n",
	    val,
	    dsp->ds_ret,
	    dsp->ds_status,
	    dsp->ds_msg);
    switch(dsp->ds_status) {
    case  0:
	fprintf(stderr, "scsi cmd finished normally\n");
	break;
    case  0x02:
	fprintf(stderr, "scsi cmd aborted, check\n");
	break;
    case  0x08:
	fprintf(stderr, "scsi cmd aborted, unit busy\n");
	break;
    case  0x10:
	fprintf(stderr, "scsi cmd with link finished\n");
	break;
    case  0x18:
	fprintf(stderr, "scsi unit aborted, reserved\n");
	break;
    }
}
/*	I P U _ A C Q U I R E
 *
 *	Wait for the IPU to finish what it was doing.  Exit program if IPU
 *	is unavailable.
 */
void
ipu_acquire(struct dsreq *dsp,
	    int timeout)
{
    int i=0;
    u_char buf[48];
    char *p;

    if (ipu_debug) fprintf(stderr, "ipu_acquire(%d)\n", timeout);

    while (testunitready00(dsp) && i < timeout)
	if (RET(dsp)==DSRT_NOSEL) {
	    fprintf(stderr, "IPU not responding\n");
	    exit(-1);
	} else {
	    /* IPU busy */
	    if (ipu_debug)
		fprintf(stderr, "ipu_acquire() sleeping at %d\r", i);

	    sleep(5);
	    i += 5;
	}


    if (ipu_debug)
	fprintf(stderr, "\n");

    if (i >= timeout) {
	fprintf(stderr, "IPU timeout after %d seconds\n", i);
	exit(-1);
    }

    /* now let's make sure we're talking to a "CANON IPU-" somthing */

    bzero(p=CMDBUF(dsp), 16);
    bzero(buf, sizeof(buf));
    p[0] = 0x12;
    p[4] = (u_char)sizeof(buf);
    CMDLEN(dsp) = 6;
    filldsreq(dsp, buf, sizeof(buf), DSRQ_READ|DSRQ_SENSE);

    if (!doscsireq(getfd(dsp), dsp) &&
	bcmp(&buf[8], "CANON   IPU-", 12)) {
	fprintf(stderr,
		"ipu_inquire() device is not IPU-10/CLC500!\n%s\n",
		&buf[8]);
	exit(-1);
    }
}


/*	I P U _ I N Q U I R E
 *
 *	returns
 *		static string describing device
 */
char *
ipu_inquire(struct dsreq *dsp)
{
    static char response[64];
    u_char buf[36];
    char *p;

    if (ipu_debug) fprintf(stderr, "inquire()\n");


    /* up to 11 characters of status */
    if (testunitready00(dsp) != 0) {
	if (RET(dsp)==DSRT_NOSEL)
	    strcpy(response, "no response from device");
	else
	    strcpy(response, "device not ready");
	return response;
    }

    bzero(p=CMDBUF(dsp), 16);
    p[0] = 0x12;
    p[4] = (u_char)sizeof(buf);
    CMDLEN(dsp) = 6;
    filldsreq(dsp, buf, sizeof(buf), DSRQ_READ|DSRQ_SENSE);

    bzero(response, sizeof(response));
    if (!doscsireq(getfd(dsp), dsp)) {
	/* 32 characters */
	(void)sprintf(response,
		      "ISO Ver(%1u)  ECMA Ver(%1u) ANSI(%1u) ",
		      buf[2] >> 6,
		      (buf[2]>>3)&0x07,
		      buf[2] & 0x07);

	/* 20 characters of Vendor/Product ID */
	bcopy(&buf[8], &response[strlen(response)], 20);
	if (bcmp(&buf[8], "CANON   IPU-", 12)) {
	    fprintf(stderr,
		    "ipu_inquire() device is not IPU-10/CLC500!\n%s\n",
		    response);
	    exit(-1);
	}
    }

    return response;
}

/*	I P U _ R E M O T E
 *
 *	Puts copier in "remote" mode.  In this mode copier functions
 *	are fully controllable by the IPU.
 *
 *	Can be used to "poll" printer done status.
 */
int
ipu_remote(struct dsreq *dsp)
{
    register char *p;
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_remote()\n");

    bzero(p=CMDBUF(dsp), 16);
    p[0] = 0xc7;

    CMDLEN(dsp) = 12;

    filldsreq(dsp, (u_char *)NULL, 0, DSRQ_SENSE);

    if (i=doscsireq(getfd(dsp), dsp)) {
	fprintf(stderr, "ipu_remote failed\n");
	scsi_perror(i, dsp);
	return -1;
    }
    return 0;
}

int ipu_ready(struct dsreq *dsp)
{
    return(testunitready00(dsp));
}



/*	I P U _ C R E A T E _ F I L E
 *
 *	create a file in the IPU
 *
 *	Parameters
 *
 *	id	file id	(1-16 for palette/RGB files, 0 for bit-map)
 *	type	{ IPU_RGB_FILE | IPU_BITMAP_FILE | IPU_PALETTE_FILE }
 *	width	file image width
 *	height	file image height
 */
void
ipu_create_file(struct dsreq *dsp,
		u_char id, 		/* file identifier */
		u_char type, 		/* file type */
		int width,
		int height,
		char clear)		/* boolean: clear file memory */
{
    char *p;
    u_char file_params[8];
    int i;

    if (ipu_debug)
	fprintf(stderr, "ipu_create_file(%d, t=%d %dx%d, %d)\n",
		id, type, width, height, clear);

    bzero(p=CMDBUF(dsp), 16);
    p[0] = 0xc4;
    p[9] = 8;
    CMDLEN(dsp) = 12;

    bzero(file_params, sizeof(file_params));
    file_params[0] = (u_char)id;
    file_params[1] = type;
    if (!clear) file_params[2] = 0x80;
    toshort(&file_params[4], width);
    toshort(&file_params[6], height);

    filldsreq(dsp, file_params, sizeof(file_params),
	      DSRQ_WRITE|DSRQ_SENSE);

    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "create_file(%d, %d by %d) failed\n",
		(int)id, width, height);
	scsi_perror(i, dsp);
	exit(-1);
    }
}

/*	I P U _ D E L E T E _ F I L E
 *
 *	Parameters
 *
 *	id	id of file to delete
 */
void
ipu_delete_file(struct dsreq *dsp,
		u_char id)
{
    register char *p;
    static short ids;
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_delete_file(%d)\n", id);

    bzero(p=CMDBUF(dsp), 16);
    p[0] = 0xc5;
    p[9] = 2;
    CMDLEN(dsp) = 12;

    ids = (id << 8) + id;

    filldsreq(dsp, (u_char *)&ids, 2, DSRQ_WRITE|DSRQ_SENSE);

    if (i=doscsireq(getfd(dsp), dsp)) {
	fprintf(stderr, "delete_file(%d) failed\n",
		id);
	scsi_perror(i, dsp);
	exit(-1);;
    }
}

/*	I P U _ G E T _ I M A G E
 *
 *	Copy image fragment from IPU buffer to host computer.
 *	No format conversion is performed
 */
u_char *
ipu_get_image(struct dsreq *dsp,
	      char id, 		/* file  id */
	      int sx, int sy, 	/* upper left corner of image */
	      int w, int h)	/* width/height of image portion to retrieve */
{
    register u_char *p;
    u_char		*img;
    int size;
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_get_image()\n");

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0xc3;
    p[2] = id;
    CMDLEN(dsp) = 12;

    toshort(&p[3], sx);
    toshort(&p[5], sy);
    toshort(&p[7], w);
    toshort(&p[9], h);

    size = w * h * 3;

    if ((img = malloc( size )) == NULL) {
	fprintf(stderr, "malloc error\n");
	exit(-1);
    }

    filldsreq(dsp, img, size, DSRQ_READ|DSRQ_SENSE);
    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "get_image(%d, %d,%d, %d,%d) failed\n",
		(int)id, sx, sy, w, h);
	scsi_perror(i, dsp);
	exit(-1);
    }

    return img;
}


/*
 *			I P U _  P U T _ I M A G E _ F R A G
 *
 *  Write image fragment from host to IPU buffer, in IPU format.
 *  The amount to be sent must be small enough to fit in a single
 *  SCSI transfer of 256k bytes.
 *  No format conversion is performed.
 */
void
ipu_put_image_frag(struct dsreq *dsp,
		   char id, 		/* file  id */
		   int sx, int sy, 	/* upper left corner of image */
		   int w, int h,	/* width/height of image portion to retrieve */
		   u_char *img)
{
    u_char	*p;
    int	i;
    int	nbytes;

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0xc2;			/* put image */
    p[2] = id;			/* file id */
    toshort(&p[3], sx);
    toshort(&p[5], sy);
    toshort(&p[7], w);		/* ipu img width */
    toshort(&p[9], h);		/* ipu img height */
    CMDLEN(dsp) = 12;

    nbytes = w * ipu_bytes_per_pixel * h;
    if( nbytes > (256 * 1024 - 2) )  {
	fprintf(stderr, "ipu_put_image_frag() nbytes=%d exceeds SCSI maximum transfer\n",
		nbytes);
	return;
    }

    filldsreq(dsp, img, nbytes, DSRQ_WRITE|DSRQ_SENSE);

    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr,
		"\nipu_put_image_frag(%d, %d,%d, %d,%d) failed\n",
		(int)id, sx, sy, w, h);
	scsi_perror(i, dsp);
	exit(-1);
    } else if ( ipu_debug )  {
	fprintf(stderr, "buffer y=%d  \r", sy);
    }
}


/*	I P U _ P U T _ I M A G E
 *
 *	load a BRL-CAD PIX(5) image into an IPU file.
 *	The entire image must be memory resident.
 *	Format conversion is performed.
 *
 *	Parameters
 *
 *	id	file id
 *	w	image width
 *	h	image height
 *	img	pointer to image data
 */
void
ipu_put_image(struct dsreq *dsp,
	      char id,
	      int w, int h,
	      u_char *img)
{
    u_char *ipubuf;
    int saved_debug;

    int bytes_per_line, lines_per_buf, bytes_per_buf, img_line;
    int buf_no, buf_line, orphan_lines, fullbuffers, pixel, ip;
    u_char *scanline, *red, *grn, *blu, *r, *g, *b;
    struct timeval tp1, tp2;
    struct timezone tz;
    FILE *fd;

    saved_debug = dsdebug;
    dsdebug = 0;

    if (ipu_debug) {
	fprintf(stderr, "ipu_put_image(id:%d, w:%d  h:%d)\n",
		id, w, h);
	if (dsdebug)
	    fd = fopen("put_image", "w");
    }
    bytes_per_line = w * ipu_bytes_per_pixel;
    lines_per_buf = BUFSIZE / bytes_per_line;
    bytes_per_buf = bytes_per_line * lines_per_buf;

    if ((ipubuf = malloc(bytes_per_buf)) == NULL) {
	fprintf(stderr, "malloc error in ipu_put_image()\n");
	exit(-1);
    }

    fullbuffers = h / lines_per_buf;
    orphan_lines = h % lines_per_buf;

    red = &ipubuf[0];
    grn = &ipubuf[lines_per_buf*w];
    blu = &ipubuf[lines_per_buf*w*2];

    if (ipu_debug) {
	fprintf(stderr, "%d buffers of %d lines, 1 buffer of %d lines\n",
		fullbuffers, lines_per_buf, orphan_lines);
	bzero(&tp1, sizeof(struct timeval));
	bzero(&tp2, sizeof(struct timeval));
	if (gettimeofday(&tp1, &tz))
	    perror("gettimeofday()");
    }

    img_line = 0;
    for (buf_no=0 ; buf_no < fullbuffers ; buf_no++) {
	/* fill a full buffer */
	if( ipu_bytes_per_pixel == 3 )  {
	    for (buf_line = lines_per_buf ; buf_line-- > 0 ; img_line++) {
		/* move img_line to buf_line */
		scanline = &img[img_line*bytes_per_line];

		r = & red[buf_line*w];
		g = & grn[buf_line*w];
		b = & blu[buf_line*w];

		for (pixel=0,ip=0 ; pixel < w ; pixel++ ) {
		    r[pixel] = scanline[ip++];
		    g[pixel] = scanline[ip++];
		    b[pixel] = scanline[ip++];
		}
	    }
	} else {
	    /* Monochrome */
	    for (buf_line = lines_per_buf ; buf_line-- > 0 ; img_line++) {
		/* move img_line to buf_line */
		scanline = &img[img_line*bytes_per_line];
		r = & red[buf_line*w];

		memcpy( r, scanline, bytes_per_line );
	    }
	}

	if (dsdebug)
	    fwrite(ipubuf, bytes_per_buf, 1, fd);

	/* send buffer to IPU */
	ipu_put_image_frag(dsp, id, 0, h-img_line, w, lines_per_buf, ipubuf);
    }


    if (orphan_lines) {
	grn = & ipubuf[orphan_lines*w];
	blu = & ipubuf[orphan_lines*w*2];

	if (ipu_debug)
	    fprintf(stderr, "\nDoing %d orphans (img_line %d)\n",
		    orphan_lines, img_line);

	if( ipu_bytes_per_pixel == 3 )  {
	    for (buf_line = orphan_lines ; buf_line-- > 0 ; img_line++) {
		scanline = &img[img_line*bytes_per_line];
		r = & red[buf_line*w];
		g = & grn[buf_line*w];
		b = & blu[buf_line*w];

		for (pixel=0 ; pixel < w ; pixel++ ) {
		    r[pixel] = scanline[pixel*3];
		    g[pixel] = scanline[pixel*3+1];
		    b[pixel] = scanline[pixel*3+2];
		}
	    }
	}  else  {
	    /* Monochrome */
	    for (buf_line = orphan_lines ; buf_line-- > 0 ; img_line++) {
		scanline = &img[img_line*bytes_per_line];
		r = & red[buf_line*w];

		memcpy( r, scanline, bytes_per_line );
	    }
	}

	if (dsdebug)
	    fwrite(ipubuf, orphan_lines*bytes_per_line, 1, fd);

	/* send buffer to IPU
	 * y offset is implicitly 0
	 */
	ipu_put_image_frag(dsp, id, 0, 0, w, orphan_lines, ipubuf);
	if (ipu_debug)
	    fprintf(stderr, "\n");
    }

    if (tp1.tv_sec && ipu_debug) {
	if (gettimeofday(&tp2, &tz))
	    perror("gettimeofday()");
	else
	    fprintf(stderr, "image transferred in %ld sec.\n",
		    (long)(tp2.tv_sec - tp1.tv_sec));
    }

    if (dsdebug)
	fclose(fd);
    free(ipubuf);
    dsdebug = saved_debug;
}

static unsigned char ipu_units[] = {
    0x23,	/* Page Code "print measurement parameters" */
    6,	/* Parameter Length */
    IPU_UNITS_INCH,
    0,	/* Reserved */
    1,	/* divisor MSB */
    0x90,	/* divisor LSB */
    0,	/* Reserved */
    0	/* Reserved */
};

static unsigned char pr_mode[12] = {
    0x25,	/* Page Code "Print Mode Parameters" */
    0x0a,	/* Parameter Length */
    0,	/* Size Conv */
    0,	/* Repeat mode */
    0,	/* Gamma Compensation type */
    0,	/* Paper tray selection */
    0, 0,	/* Upper/Lower tray paper codes */
    0, 0, 0, 0	/* Reserved */
};

/*	I P U _ P R I N T _ C O N F I G
 *
 *	Set configuration parameters for printer
 *
 *	units	IPU_UNITS_INCH | IPU_UNITS_MM
 *	divisor	1 10 100 132 400
 *	conv	IPU_AUTOSCALE IPU_AUTOSCALE_IND IPU_MAG_FACTOR IPU_RESOLUTION
 *	mosaic	boolean: perform mosaic
 *	gamma	style { IPU_GAMMA_STANDARD | IPU_GAMMA_RGB | IPU_GAMMA_CG }
 *	tray	tray selection
 */
void
ipu_print_config(struct dsreq *dsp,
		 char units,
		 int divisor,
		 u_char conv, u_char mosaic, u_char ipu_gamma,
		 int tray)
{
    register u_char *p;
    u_char params[255];
    int bytes;
    int save;
    int i;

    if (ipu_debug)
	fprintf(stderr,
		"ipu_print_config(0x%0x, 0x%0x, 0x%0x, 0x%0x, 0x%0x, 0x%0x)\n",
		units, divisor, conv, mosaic, ipu_gamma, tray);

    save = dsdebug;

    pr_mode[2] = conv;
    if (mosaic) pr_mode[3] = 1;
    else pr_mode[3] = 0;
    pr_mode[4] = ipu_gamma;
    pr_mode[5] = (u_char)tray;

    ipu_units[2] = 0x23;	/* print mode parameters */
    ipu_units[2] = units;
    toshort(&ipu_units[4], divisor);

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0x15;		/* MODE SELECT */
    p[1] = 0x10;
    CMDLEN(dsp) = 6;

    bzero(params, sizeof(params));
    bytes = 4;	/* leave room for the mode parameter header */

    bcopy(ipu_units, &params[bytes], sizeof(ipu_units));
    bytes += sizeof(ipu_units);

    bcopy(pr_mode, &params[bytes], sizeof(pr_mode));
    bytes += sizeof(pr_mode);

    p[4] = (u_char)bytes;
    params[3] = (u_char)(bytes - 4);

    filldsreq(dsp, params, bytes, DSRQ_WRITE|DSRQ_SENSE);

    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "error doing print config.\n");
	scsi_perror(i, dsp);
	exit(-1);
    }
    dsdebug = save;

}

/*	I P U _ P R I N T _ F I L E
 *
 *	id	IPU file id of image to print
 *	copies	# of prints of file
 *	wait	sync/async printing
 */
void
ipu_print_file(struct dsreq *dsp,
	       char id,
	       int copies,
	       int wait,
	       int sx, int sy,
	       int sw, int sh,
	       union ipu_prsc_param *pr_param)
{
    register u_char *p;
    char buf[18];
    int i;

    if (ipu_debug) fprintf(stderr,
			   "ipu_print_file(id=%d copies=%d wait=%d sx=%d sy=%d sw=%d sh=%d\n\
	0x%x 0x%x 0x%x 0x%x)\n", id, copies, wait, sx, sy, sw, sh,
			   pr_param->c[0],pr_param->c[1],pr_param->c[2],pr_param->c[3]);


    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0xc1;
    toint(&p[6], (int)sizeof(buf));
    CMDLEN(dsp) = 12;

    bzero(buf, sizeof(buf));
    buf[0] = id;
    if (copies < 0) {
	fprintf(stderr, "Cannot print %d copies\n", copies);
	exit(-1);
    } else if (copies > 99) {
	fprintf(stderr, "Cannot print more than 99 copies at once\n");
	exit(-1);
    }
    toshort(&buf[1], copies);
    if (!wait) buf[3] = 0x080;	/* quick return */
    toshort(&buf[6], sx);
    toshort(&buf[8], sy);
    toshort(&buf[10], sw);
    toshort(&buf[12], sh);
    bcopy(pr_param, &buf[14], 4);

    filldsreq(dsp, (u_char *)buf, sizeof(buf), DSRQ_WRITE|DSRQ_SENSE);
    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "error printing file %d\n", id);
	scsi_perror(i, dsp);
	exit(-1);
    }
}
static unsigned char sc_mode[6] = {
    0x24,	/* Page Code "Scan Mode Parameters" */
    0x04,	/* Parameter Length */
    0,	/* Size Conv */
    0,	/* Scan */
    0, 0	/* Analog Input Rotation */
};
/*	I P U _ S C A N _ C O N F I G
 *
 *	Set configuration parameters for scanning
 *
 *	units	IPU_UNITS_INCH | IPU_UNITS_MM
 *	divisor	1 10 100 132 400
 *	conv	IPU_AUTOSCALE IPU_AUTOSCALE_IND IPU_MAG_FACTOR IPU_RESOLUTION
 *	field	boolean:  Frame scan / Field scan
 *	rotation	angle of image rotation (multiple of 90 degrees)
 */
void
ipu_scan_config(struct dsreq *dsp,
		char units,
		int divisor,
		char conv,
		char field,
		short rotation)
{
    register u_char *p;
    u_char params[255];
    int bytes;
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_scan_config()\n");

    ipu_units[2] = units;
    toshort(&ipu_units[4], divisor);

    sc_mode[2] = conv;
    if (field) sc_mode[3] = 1;
    else sc_mode[3] = 0;

    rotation = rotation % 360;
    if (rotation != 0 && rotation != 90 && rotation != 180 &&
	rotation != 270) {
	fprintf(stderr,
		"image rotation in 90 degree increments only\n");
	exit(-1);
    }
    toshort(&sc_mode[4], rotation);

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0x15;			/* MODE SELECT */
    p[1] = 0x10;
    CMDLEN(dsp) = 6;

    bzero(params, sizeof(params));
    bytes = 4;	/* leave room for the mode parameter header */

    bcopy(ipu_units, &params[bytes], sizeof(ipu_units));
    bytes += sizeof(ipu_units);

    bcopy(sc_mode, &params[bytes], sizeof(sc_mode));
    bytes += sizeof(sc_mode);

    p[4] = (u_char)bytes;
    params[3] = (u_char)(bytes - 4);

    filldsreq(dsp, params, bytes, DSRQ_WRITE|DSRQ_SENSE);

    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "error doing scan config.\n");
	scsi_perror(i, dsp);
	exit(-1);
    }
}

/*	I P U _ S C A N _ F I L E
 */
void
ipu_scan_file(struct dsreq *dsp,
	      char id,
	      char wait,
	      int sx, int sy,
	      int w, int h,
	      union ipu_prsc_param *sc_param)
{
    register u_char *p;
    char buf[18];
    int i;

    if (ipu_debug) fprintf(stderr,
			   "ipu_scan_file(id=%d wait=%d sx=%d sy=%d sw=%d sh=%d\n\
		0x%x 0x%x 0x%x 0x%x)\n", id, wait, sx, sy, w, h,
			   sc_param->c[0],sc_param->c[1],
			   sc_param->c[2],sc_param->c[3]);

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0xc0;
    toint(&p[6], (int)sizeof(buf));
    CMDLEN(dsp) = 12;

    bzero(buf, (int)sizeof(buf));
    buf[0] = id;

    if (!wait) buf[3] = 0x080;	/* quick return */
    toshort(&buf[6], sx);
    toshort(&buf[8], sy);
    toshort(&buf[10], w);
    toshort(&buf[12], h);
    bcopy(sc_param, &buf[14], 4);

    filldsreq(dsp, (u_char *)buf, sizeof(buf), DSRQ_WRITE|DSRQ_SENSE);
    if ( i=doscsireq(getfd(dsp), dsp) )  {
	fprintf(stderr, "error scanning file %d.\n", id);
	scsi_perror(i, dsp);
	exit(-1);
    }
}

/*	I P U _ L I S T _ F I L E S
 *
 *	list files in the IPU
 *
 *	returns a null terminated, newline separated list of files and
 *		associated file dimensions.
 */
char *
ipu_list_files(struct dsreq *dsp)
{
#define IPU_FILE_DESC_SIZE 16	/* # bytes for file description */
#define IPU_LFPH_LEN 8		/* List File Parameters Header Length */
    u_char *p;
    static char buf[IPU_LFPH_LEN + IPU_MAX_FILES*IPU_FILE_DESC_SIZE];
    int len;
    int i;
    int file_count;

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0xc6;
    toint(&p[6], (int)sizeof(buf));
    CMDLEN(dsp) = 12;

    filldsreq(dsp, (u_char *)buf, sizeof(buf), DSRQ_READ|DSRQ_SENSE);

    if (i=doscsireq(getfd(dsp), dsp)) {
	fprintf(stderr, "error in ipu_list_files().\n");
	scsi_perror(i, dsp);
	exit(-1);
    }

    len = ((int)buf[0]<<8) + buf[1] + 2;
    file_count = (len - 8) / buf[2];

    if ((p=malloc(file_count*19+1)) == NULL) {
	fprintf(stderr, "malloc error in ipu_list_files()\n");
	exit(-1);
    } else {
	bzero(p, file_count*19+1);
    }

    for (i = 8 ; i < len ; i += buf[2]) {
	register char t;
	if (buf[i+1] == 0) t = 'B';
	else if (buf[i+1] == 2) t = 'R';
	else if (buf[i+1] == 3) t = 'P';
	else t = '?';

	sprintf((char *)&p[strlen( (char *)p)], "%1x.%c\t%4dx%4d\n",
		buf[i], t,
		((int)buf[i+2]<<8)+(int)buf[i+3],
		((int)buf[i+4]<<8)+(int)buf[i+5]);
    }

    return (char *)p;
}

/*	I P U _ S T O P
 *
 * Parameters
 *	dsp	SCSI interface ptr;
 *	halt	boolean:  Halt printing (or just report status)
 *
 * returns number of pages which have been printed
 */
int
ipu_stop(struct dsreq *dsp,
	 int halt)
{
    register char *p;
    char buf[18];
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_stop(%d)\n", halt);

    bzero(p=CMDBUF(dsp), 16);
    p[0] = 0xcc;	/* scan file */
    if (halt)
	p[2] = 0x80;
    p[9] = 18;	/* param list len */
    CMDLEN(dsp) = 12;

    bzero(buf, sizeof(buf));
    filldsreq(dsp, (u_char *)buf, sizeof(buf), DSRQ_READ|DSRQ_SENSE);

    if ((i=doscsireq(getfd(dsp), dsp)) == -1) {
	fprintf(stderr, "Error doing ipu_stop().\n");
	scsi_perror(i, dsp);
	exit(-1);
    }

    return ((int)buf[1] << 8) + (int)buf[2];
}


int
ipu_get_conf(struct dsreq *dsp)
{
    register u_char *p;
    u_char params[255];
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_config_printer()\n");

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0x1a;			/* mode sense */
    p[2] = 0x23;
    p[4] = sizeof(params);
    CMDLEN(dsp) = 6;

    bzero(params, sizeof(params));
    filldsreq(dsp, params, sizeof(params), DSRQ_READ|DSRQ_SENSE);

    if ((i=doscsireq(getfd(dsp), dsp)) == -1) {
	fprintf(stderr, "Error reading IPU configuration.\n");
	scsi_perror(i, dsp);
	exit(-1);;
    }

    if (params[0] == 11 && params[4] == 0x23) {
	p = & params[4];

	switch (p[2]) {
	case IPU_UNITS_INCH	:fprintf(stderr, "Units=Inch  ");
	    break;
	case IPU_UNITS_MM	:fprintf(stderr, "Units=mm  ");
	    break;
	default			:fprintf(stderr, "Units=??  ");
	    break;
	}

	i = ((int)p[4] << 8) + (int)p[5];
	fprintf(stderr, "Divisor=%d(0x%x)\n", i, i);
    }

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0x1a;			/* mode sense */
    p[2] = 0x25;
    p[4] = sizeof(params);
    CMDLEN(dsp) = 6;

    bzero(params, sizeof(params));
    filldsreq(dsp, params, sizeof(params), DSRQ_READ|DSRQ_SENSE);

    if ((i=doscsireq(getfd(dsp), dsp)) == -1) {
	fprintf(stderr, "Error reading IPU configuration.\n");
	scsi_perror(i, dsp);
	exit(-1);;
    }

    if (params[0] == 15 && params[4] == 0x25) {
	p = & params[4];
	switch (p[2]) {
	case IPU_AUTOSCALE : fprintf(stderr, "conv=Autoscale  ");
	    break;
	case IPU_AUTOSCALE_IND : fprintf(stderr, "conv=Autoscale_ind  ");
	    break;
	case IPU_MAG_FACTOR : fprintf(stderr, "conv=Mag Factor  ");
	    break;
	case IPU_RESOLUTION : fprintf(stderr, "conv=Resolution  ");
	    break;
	default	: fprintf(stderr, "conv=Unknown conv.");
	    break;
	}

	fprintf(stderr, "Repeat=%d  ", p[3]);

	switch (p[4]) {
	case IPU_GAMMA_STANDARD: fprintf(stderr, "gamma=std  ");
	    break;
	case IPU_GAMMA_RGB: fprintf(stderr, "gamma=rgb  ");
	    break;
	case IPU_GAMMA_CG: fprintf(stderr, "gamma=cg  ");
	    break;
	default: fprintf(stderr, "Unknown gamma  ");
	    break;
	}

	switch (p[5]) {
	case IPU_UPPER_CASSETTE : fprintf(stderr, "tray=upper\n");
	    break;
	case IPU_LOWER_CASSETTE : fprintf(stderr, "tray=lower\n");
	    break;
	case IPU_MANUAL_FEED : fprintf(stderr, "tray=Manual_Feed\n");
	    break;
	default: fprintf(stderr, "tray=Unknown (%d)\n", p[5]);
	    break;
	}

	fprintf(stderr, "Upper cassette paper=");
	switch (p[6]) {
	case 0x6: fputs("A3  ", stderr); break;
	case 0x8: fputs("A4R  ", stderr); break;
	case 0x18: fputs("B4  ", stderr); break;
	case 0x1a: fputs("B5R  ", stderr); break;
	case 0x28: fputs("A4  ", stderr); break;
	case 0x46: fputs("11\"x17\"  ", stderr); break;
	case 0x48: fputs("Letter R  ", stderr); break;
	case 0x58: fputs("Legal  ", stderr); break;
	case 0x68: fputs("Letter  ", stderr); break;
	case 0x80: fputs("Manual Feed  ", stderr); break;
	case 0xff: fputs("No cassette  ", stderr); break;
	default : fputs("unknown  ", stderr); break;
	}
	fprintf(stderr, "Lower cassette paper=");
	switch (p[7]) {
	case 0x6: fputs("A3\n", stderr); break;
	case 0x8: fputs("A4R\n", stderr); break;
	case 0x18: fputs("B4\n", stderr); break;
	case 0x1a: fputs("B5R\n", stderr); break;
	case 0x28: fputs("A4\n", stderr); break;
	case 0x46: fputs("11\"x17\"\n", stderr); break;
	case 0x48: fputs("Letter R\n", stderr); break;
	case 0x58: fputs("Legal\n", stderr); break;
	case 0x68: fputs("Letter\n", stderr); break;
	case 0x80: fputs("Manual Feed\n", stderr); break;
	case 0xff: fputs("No cassette\n", stderr); break;
	default : fputs("unknown\n", stderr); break;
	}
    }
    return 0;
}

int
ipu_get_conf_long(struct dsreq *dsp)
{
    register u_char *p;
    static u_char params[65535];
    int i;

    if (ipu_debug) fprintf(stderr, "ipu_config_printer()\n");

    bzero(p=(u_char *)CMDBUF(dsp), 16);
    p[0] = 0x5a;			/* mode sense */
    p[2] = 0x23;
    toint(&p[7], (int)sizeof(params));
    CMDLEN(dsp) = 10;

    bzero(params, sizeof(params));
    filldsreq(dsp, params, sizeof(params), DSRQ_READ|DSRQ_SENSE);

    if ((i=doscsireq(getfd(dsp), dsp)) == -1) {
	fprintf(stderr, "Error reading IPU configuration.\n");
	scsi_perror(i, dsp);
	exit(-1);;
    }

    (void)fprintf(stderr, "Sense Data Length %u\n",
		  ((unsigned)params[0] << 8) + params[1]);

    (void)fprintf(stderr, "Block Descriptor Length %u\n",
		  ((unsigned)params[6] << 8) + params[7]);
    return 0;
}

/*
 *			I P U _ S E T _ P A L E T T E
 *
 *  For 8-bit mode, program the color palette.
 *  Works in the same manner as LIBFB color maps.
 *
 *  If 'cmap' is NULL, then linear ramp is used.
 *  If 'cmap' is non-NULL, then it points to a 768 byte array
 *  arranged as r0, g0, b0, r1, g1, b1, ...
 */
void
ipu_set_palette( dsp, cmap )
     struct dsreq	*dsp;
     unsigned char	*cmap;		/* NULL or [768] */
{
    register caddr_t p;
    unsigned char	linear[768];
    int		i;
    int		ret;

    if (ipu_debug) fprintf(stderr, "ipu_set_palette(cmap=x%lx)\n", (long)cmap);
    if( cmap == NULL )  {
	register int	j;
	register unsigned char *cp = linear;
	for( j=0; j < 256; j++ )  {
	    *cp++ = j;
	    *cp++ = j;
	    *cp++ = j;
	}
	cmap = linear;
    }

    /* The Palette has to be sent in 4 parts */
    for( i=0; i < 4; i++ )  {
	unsigned char	buf[4+2+192];

	bzero(p=CMDBUF(dsp), 16);
	p[0] = 0x15;		/* MODE SELECT, Group code 0 (6 byte) */
	/* Lun 0=scanner, Lun 3=printer */
	p[1] = (3<<5) | 0x10;	/* Lun, PF=1 (page format) */
	p[4] = sizeof(buf);
	CMDLEN(dsp) = 6;

	/* buf[0]:  Parameter header, 4 bytes long. */
	toint( buf, 192+2 );

	/* buf[4]: MODE PARAMETERS, block(s) of page codes. */
	buf[4] = 0x30 + i;	/* PAGE CODE "Color Palette Parameters" */
	buf[5] = 0xC0;		/* 192 bytes follow */
	bcopy( cmap+i*192, buf+4+2, 192 );

	filldsreq(dsp, buf, sizeof(buf), DSRQ_WRITE|DSRQ_SENSE);

	if ( ret=doscsireq(getfd(dsp), dsp) )  {
	    fprintf(stderr, "ipu_set_palette(%d) failed\n", i);
	    scsi_perror(ret, dsp);
	    exit(-1);
	}
    }
}

#endif /* __sgi__ */

char *options = "P:p:Q:q:acd:g:hmn:s:t:vw:zAC:M:R:D:N:S:W:X:Y:U:V#:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
char scsi_device[1024] = "/dev/scsi/sc0d6l3";
char ipu_gamma = IPU_GAMMA_CG;
int  ipu_filetype = IPU_RGB_FILE;
int  ipu_bytes_per_pixel = 3;
char tray = IPU_UPPER_CASSETTE;
char conv = IPU_AUTOSCALE;
char clear = 0;
int width = 512;
int height = 512;
int zoom = 0;
int scr_width;
int scr_height;
int scr_xoff;
int scr_yoff;
int copies = 1;
int autosize = 0;
int units = IPU_UNITS_INCH;
int divisor = 0x190;
int mosaic = 0;

union ipu_prsc_param param;

#define MAX_ARGS	64
static char arg_buf[10000] = {0};
static int  len;
char	*arg_v[MAX_ARGS];
int	arg_c;
char *print_queue = "canon";

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(s)
     char *s;
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [options] [pixfile]\nOptions:\n%s", progname,
		   "	[-h] [-n scanlines] [-w width] [-s squareimagesize]\n\
	[-N outputheight] [-W outputwidth] [-S outputsquaresize]\n\
	[-X PageXOffset] [-Y PageYOffset] [-# bytes_pixel]\n\
	[-a(utosize_input)] [-g(amma) { s | r | c }]\n\
	[-z(oom)] [-t { u | l | m }] [-C(opies) {1-99}] [-m(osaic)]\n\
	[ -A(utoscale_output) | -M xmag:ymag | -R dpi ]\n\
	[-u(nits) { i|m } ] [-D divisor]\n\
	[-c(lear)] [-d SCSI_device] [-v(erbose)] [-V(erboser)]\n\
	[-q queue] [-p printer]\n");

    exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(ac, av)
     int ac;
     char *av[];
{
    int  c;
    char *p;

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    strcpy(arg_buf, progname);
    len = strlen(arg_buf) + 1;
    arg_v[arg_c = 0] = arg_buf;
    arg_v[++arg_c] = (char *)NULL;

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line */
    while ((c=getopt(ac,av,options)) != EOF) {
	/* slup off a printer queue name */
	if (c == 'q' ||  c == 'p') {
	    print_queue = optarg;
	    continue;
	}

	/* add option & arg to arg_v */
	if (p=strchr(options, c)) {
	    arg_v[arg_c++] = &arg_buf[len];
	    arg_v[arg_c] = (char *)NULL;
	    (void)sprintf(&arg_buf[len], "-%c", c);
	    len += strlen(&arg_buf[len]) + 1;
	    if (p[1] == ':') {
		arg_v[arg_c++] = &arg_buf[len];
		arg_v[arg_c] = (char *)NULL;
		(void)sprintf(&arg_buf[len], "%s", optarg);
		len += strlen(&arg_buf[len]) + 1;
	    }
	}

	switch (c) {
	case 'a'	: autosize = !autosize; break;
	case 'c'	: clear = !clear; break;
	case 'd'	: if (isprint(*optarg)) {
	    memset(scsi_device, 0, sizeof(scsi_device));
	    strncpy(scsi_device,optarg,sizeof(scsi_device)-1);
	} else
	    usage("-d scsi_device_name\n");
	    break;
	case 'g'	: if (*optarg == 's')
	    ipu_gamma = IPU_GAMMA_STANDARD;
	else if (*optarg == 'r')
	    ipu_gamma = IPU_GAMMA_RGB;
	else if (*optarg == 'c')
	    ipu_gamma = IPU_GAMMA_CG;
	else
	    usage("-g {std|rgb|cg}\n");
	    break;
	case 'h'	: width = height = 1024; break;
	case 'm'	: mosaic = !mosaic; break;
	case 'n'	: if ((c=atoi(optarg)) > 0)
	    height = c;
	else
	    usage("-n scanlines\n");
	    break;
	case 's'	: if ((c=atoi(optarg)) > 0)
	    width = height = c;
	else
	    usage("-s squareimagesize\n");
	    break;
	case 'w'	: if ((c=atoi(optarg)) > 0)
	    width = c;
	else
	    usage("-w imagewidth\n");
	    break;
	case 't'	:switch (*optarg) {
	case 'u'	:
	case 'U'	: tray = IPU_UPPER_CASSETTE;
	    break;
	case 'l'	:
	case 'L'	: tray = IPU_LOWER_CASSETTE;
	    break;
	case 'm'	:
	case 'M'	: tray = IPU_MANUAL_FEED;
	    break;
	default:
	    usage("-t {u|l|m}\n");
	    break;
	}
	    break;
	case 'U'	: if (*optarg == 'i')
	    units = IPU_UNITS_INCH;
	else if (*optarg == 'm')
	    units = IPU_UNITS_MM;
	else
	    usage("invalid units\n");
	    break;
	case 'z'	: zoom = !zoom;
	    scr_width= scr_height= scr_xoff= scr_yoff= 0;
	    break;
	case 'A'	: conv = IPU_AUTOSCALE;
	    bzero( (char *)&param, sizeof(union ipu_prsc_param));
	    break;
	case 'M'	: conv = IPU_MAG_FACTOR;
	    if ((c=atoi(optarg)) < 100 || c > 2000)
		usage("X Mag factor out of range 100-2000\n");
	    param.s[0] = c & 0x0ffff;
	    while (*optarg && *optarg++ != ':')
		;
	    if ((c = atoi(optarg)) < 100 || c > 2000)
		usage("Y Mag factor out of range 100-2000\n");
	    param.s[1] = c & 0x0ffff;
	    break;
	case 'R'	: if ((c=atoi(optarg)) > 0) {
	    param.i = c;
	    conv = IPU_RESOLUTION;
	} else {
	    fprintf(stderr,
		    "Resolution error (%d)\n",c);
	    exit(-1);
	}
	    if (ipu_debug)
		fprintf(stderr,
			"Res: %d 0%02x 0%02x 0%02x 0%02x\n",
			c, param.c[0], param.c[1], param.c[2],
			param.c[3]);
	    break;
	case 'C'	: if ((c=atoi(optarg)) > 0 && c < 99)
	    copies = c;
	else
	    usage("-C [1-99]\n");
	    break;
	case 'D'	: if ((c=atoi(optarg)) > 0)
	    divisor = c;
	else
	    usage("-D divisor\n");
	    break;
	case 'N'	: if ((c=atoi(optarg)) > 0)
	    scr_height = c;
	else
	    usage("-N outputlines\n");
	    break;
	case 'S'	: if ((c=atoi(optarg)) > 0)
	    scr_width = scr_height = c;
	else
	    usage("-S outputsquaresize\n");
	    break;
	case 'W'	: if ((c=atoi(optarg)) > 0)
	    scr_width = c;
	else
	    usage("-W outputwidth\n");
	    break;
	case 'X'	: if ((c=atoi(optarg)) >= 0)
	    scr_xoff = c;
	else
	    usage("-X pageXoffset\n");
	    break;
	case 'Y'	: if ((c=atoi(optarg)) >= 0)
	    scr_yoff = c;
	else
	    usage("-X pageYoffset\n");
	    break;
	case 'V'	: dsdebug = ! dsdebug;
	case 'v'	: ipu_debug = !ipu_debug; break;
	case '#'	: c = atoi(optarg);
	    switch(c)  {
	    case 3:
		ipu_filetype = IPU_RGB_FILE;
		ipu_bytes_per_pixel = 3;
		break;
	    case 1:
		ipu_filetype = IPU_PALETTE_FILE;
		ipu_bytes_per_pixel = 1;
		break;
	    default:
		fprintf(stderr, "Bad value %d for bytes_per_pixel\n", c);
		break;
	    }
	    break;
	case '?'	:
	default		: fprintf(stderr, "Bad or help flag specified '%c'\n", c); break;
	}
    }
    return(optind);
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
