/*                      C A N O N L I B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "bu.h"

#include "./canon.h"

#define BUFSIZE 262144	/* 256Kbytes size of image transfer buffer */


int ipu_debug = 0;

/* this is from some imaginary libds.so ?
 * was extern'd, but without that lib, it failed... so we punt.
 */
int dsdebug = 0;

char *options = "P:p:Q:q:acd:g:hmn:s:t:vw:zAC:M:R:D:N:S:W:X:Y:U:V#:";

char *progname = "(noname)";
char scsi_device[1024] = "/dev/scsi/sc0d6l3";
char ipu_gamma = IPU_GAMMA_CG;
int  ipu_filetype = IPU_RGB_FILE;
int  ipu_bytes_per_pixel = 3;
int tray = IPU_UPPER_CASSETTE;
char conv = IPU_AUTOSCALE;
char clear = 0;
size_t width = 512;
size_t height = 512;
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

    bu_exit(1, NULL);
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

    if ( ! (progname=strrchr(*av, '/')) )
	progname = *av;
    else
	++progname;

    bu_strlcpy(arg_buf, progname, sizeof(arg_buf));
    len = strlen(arg_buf) + 1;
    arg_v[arg_c = 0] = arg_buf;
    arg_v[++arg_c] = (char *)NULL;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	/* slup off a printer queue name */
	if (c == 'q' ||  c == 'p') {
	    print_queue = bu_optarg;
	    continue;
	}

	/* add option & arg to arg_v */
	if ((p=strchr(options, c))) {
	    arg_v[arg_c++] = &arg_buf[len];
	    arg_v[arg_c] = (char *)NULL;
	    (void)sprintf(&arg_buf[len], "-%c", c);
	    len += strlen(&arg_buf[len]) + 1;
	    if (p[1] == ':') {
		arg_v[arg_c++] = &arg_buf[len];
		arg_v[arg_c] = (char *)NULL;
		(void)snprintf(&arg_buf[len], 10000, "%s", bu_optarg);
		len += strlen(&arg_buf[len]) + 1;
	    }
	}

	switch (c) {
	    case 'a'	: autosize = !autosize; break;
	    case 'c'	: clear = !clear; break;
	    case 'd'	: if (isprint(*bu_optarg)) {
		memset(scsi_device, 0, sizeof(scsi_device));
		bu_strlcpy(scsi_device, bu_optarg, sizeof(scsi_device));
	    } else
		usage("-d scsi_device_name\n");
		break;
	    case 'g'	: if (*bu_optarg == 's')
		ipu_gamma = IPU_GAMMA_STANDARD;
	    else if (*bu_optarg == 'r')
		ipu_gamma = IPU_GAMMA_RGB;
	    else if (*bu_optarg == 'c')
		ipu_gamma = IPU_GAMMA_CG;
	    else
		usage("-g {std|rgb|cg}\n");
		break;
	    case 'h'	: width = height = 1024; break;
	    case 'm'	: mosaic = !mosaic; break;
	    case 'n'	: if ((c=atoi(bu_optarg)) > 0)
		height = c;
	    else
		usage("-n scanlines\n");
		break;
	    case 's'	: if ((c=atoi(bu_optarg)) > 0)
		width = height = c;
	    else
		usage("-s squareimagesize\n");
		break;
	    case 'w'	: if ((c=atoi(bu_optarg)) > 0)
		width = c;
	    else
		usage("-w imagewidth\n");
		break;
	    case 't'	:switch (*bu_optarg) {
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
	    case 'U'	: if (*bu_optarg == 'i')
		units = IPU_UNITS_INCH;
	    else if (*bu_optarg == 'm')
		units = IPU_UNITS_MM;
	    else
		usage("invalid units\n");
		break;
	    case 'z'	: zoom = !zoom;
		scr_width= scr_height= scr_xoff= scr_yoff= 0;
		break;
	    case 'A'	: conv = IPU_AUTOSCALE;
		memset((char *)&param, 0, sizeof(union ipu_prsc_param));
		break;
	    case 'M'	: conv = IPU_MAG_FACTOR;
		if ((c=atoi(bu_optarg)) < 100 || c > 2000)
		    usage("X Mag factor out of range 100-2000\n");
		param.s[0] = c & 0x0ffff;
		while (*bu_optarg && *bu_optarg++ != ':')
		    ;
		if ((c = atoi(bu_optarg)) < 100 || c > 2000)
		    usage("Y Mag factor out of range 100-2000\n");
		param.s[1] = c & 0x0ffff;
		break;
	    case 'R'	: if ((c=atoi(bu_optarg)) > 0) {
		param.i = c;
		conv = IPU_RESOLUTION;
	    } else {
		fprintf(stderr,
			"Resolution error (%d)\n", c);
		bu_exit(-1, NULL);
	    }
		if (ipu_debug)
		    fprintf(stderr,
			    "Res: %d 0%02x 0%02x 0%02x 0%02x\n",
			    c, param.c[0], param.c[1], param.c[2],
			    param.c[3]);
		break;
	    case 'C'	: if ((c=atoi(bu_optarg)) > 0 && c < 99)
		copies = c;
	    else
		usage("-C [1-99]\n");
		break;
	    case 'D'	: if ((c=atoi(bu_optarg)) > 0)
		divisor = c;
	    else
		usage("-D divisor\n");
		break;
	    case 'N'	: if ((c=atoi(bu_optarg)) > 0)
		scr_height = c;
	    else
		usage("-N outputlines\n");
		break;
	    case 'S'	: if ((c=atoi(bu_optarg)) > 0)
		scr_width = scr_height = c;
	    else
		usage("-S outputsquaresize\n");
		break;
	    case 'W'	: if ((c=atoi(bu_optarg)) > 0)
		scr_width = c;
	    else
		usage("-W outputwidth\n");
		break;
	    case 'X'	: if ((c=atoi(bu_optarg)) >= 0)
		scr_xoff = c;
	    else
		usage("-X pageXoffset\n");
		break;
	    case 'Y'	: if ((c=atoi(bu_optarg)) >= 0)
		scr_yoff = c;
	    else
		usage("-X pageYoffset\n");
		break;
	    case 'V'	: dsdebug = ! dsdebug;
	    case 'v'	: ipu_debug = !ipu_debug; break;
	    case '#'	: c = atoi(bu_optarg);
		switch (c) {
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
    return bu_optind;
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
