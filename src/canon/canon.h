/*                         C A N O N . H
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
/** @file canon.h
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *
 *  $Header$
 */

#ifndef __CANON_H__
#define __CANON_H__

#include "common.h"

#define	IPU_LUN_SCANNER		0x0
#define	IPU_LUN_FILM_SCANNER	0x1
#define	IPU_LUN_ANALOG_INPUT	0x2
#define	IPU_LUN_PRINTER		0x3
#define	IPU_LUN_ANALOG_OUTPUT	0x4

#define IPU_RGB_FILE	2
#define IPU_BITMAP_FILE	0
#define IPU_PALETTE_FILE 3

#define IPU_UNITS_INCH	'\0'
#define IPU_UNITS_MM	'\1'

#define IPU_AUTOSCALE	0
#define IPU_AUTOSCALE_IND	1
#define IPU_MAG_FACTOR	2
#define IPU_RESOLUTION	3

#define IPU_GAMMA_STANDARD	0
#define IPU_GAMMA_RGB		1
#define IPU_GAMMA_CG		2

#define IPU_UPPER_CASSETTE	0
#define IPU_LOWER_CASSETTE	1
#define IPU_MANUAL_FEED		128

#define	IPU_MAX_FILES	17	/* 16 image/palette + 1 bit-mapped */

union ipu_prsc_param {
    char	c[4];
    int	i;
    short	s[2];
};

extern int	ipu_debug;

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if defined(HAVE_SYS_PRCTL_H) 
#  include <sys/prctl.h>
#endif
#if defined(HAVE_DSLIB_H) && defined(PR_SALL) && defined(PR_SFDS)
#    define IPU_FULL_LIB 1
#    include <dslib.h>
#endif


#ifdef USE_PROTOTYPES
#  define	FUNC_EXTERN(type_and_name,args)	extern type_and_name args
#else
#  define	FUNC_EXTERN(type_and_name,args)	extern type_and_name()
#endif

#ifdef IPU_FULL_LIB
FUNC_EXTERN(int ipu_not_ready, (struct dsreq *dsp));
FUNC_EXTERN(char *ipu_inquire, (struct dsreq *dsp));
FUNC_EXTERN(int ipu_remote, (struct dsreq *dsp));
FUNC_EXTERN(void ipu_create_file, (struct dsreq *dsp,u_char id,u_char type,int width,int height,char clear));
FUNC_EXTERN(void ipu_delete_file, (struct dsreq *dsp, u_char id));
FUNC_EXTERN(u_char *ipu_get_image, (struct dsreq *dsp,char id,int sx,int sy,int w,int h));
FUNC_EXTERN(void ipu_put_image, (struct dsreq *dsp,char id,int w,int h,u_char *img));
FUNC_EXTERN(void ipu_print_config, (struct dsreq *dsp,char units,int divisor,u_char conv,u_char mosaic,u_char gamma,int tray));
FUNC_EXTERN(void ipu_print_file, (struct dsreq *dsp,char id,int copies,int wait,int sx,int sy,int sw,int sh,union ipu_prsc_param *param));
FUNC_EXTERN(char *ipu_list_files, (struct dsreq *dsp));
FUNC_EXTERN(int ipu_stop, (struct dsreq *dsp,int halt));
FUNC_EXTERN(void ipu_scan_file, (struct dsreq *dsp,char id,char wait,int sx,int sy,int w,int h,union ipu_prsc_param *param));
FUNC_EXTERN(void ipu_scan_config, (struct dsreq *dsp,char units,int divisor,char conv,char field,short rotation));
#endif

FUNC_EXTERN(int parse_args, (int ac, char *av[]));
FUNC_EXTERN(void usage, (char *s));

extern char *progname;
extern char scsi_device[];
extern char ipu_gamma;
extern int  ipu_filetype;
extern int  ipu_bytes_per_pixel;
extern char tray;
extern char conv;
extern char clear;
extern int width;
extern int height;
extern int zoom;
extern int scr_width;
extern int scr_height;
extern int scr_xoff;
extern int scr_yoff;
extern int copies;
extern int autosize;
extern int units;
extern int divisor;
extern int mosaic;
extern union ipu_prsc_param param;
extern char *arg_v[];
extern int arg_c;
extern char *print_queue;

#endif  /* __CANON_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
