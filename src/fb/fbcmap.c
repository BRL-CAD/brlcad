/*                        F B C M A P . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file fbcmap.c
 *
 *	Write a built-in colormap to a framebuffer.
 *	When invoked with no arguments, or with a flavor of 0,
 *	the standard 1:1 ramp color-map is written.
 *	Other flavors provide interesting alternatives.
 *
 *  Author -
 *	Mike Muuss, 7/17/82
 *	VAX version 10/18/83
 *
 *	Conversion to generic frame buffer utility using libfb(3).
 *	In the process, the name has been changed to fbcmap from ikcmap.
 *	Gary S. Moss, BRL. 03/12/85
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "fb.h"

void		usage(char **argv);
int		pars_Argv(int argc, register char **argv);

static char	*framebuffer = NULL;
static int	scr_width = 0;
static int	scr_height = 0;


static ColorMap cmap;
static int	flavor = 0;
extern unsigned char	utah8[], utah9[];	/* defined at end of file */
static unsigned char	utah_cmap[256] = {
	  0,  4,  9, 13, 17, 21, 25, 29, 32, 36, 39, 42, 45, 48, 51, 54,
	 57, 59, 62, 64, 67, 69, 72, 74, 76, 78, 81, 83, 85, 87, 89, 91,
	 92, 94, 96, 98,100,101,103,105,106,108,110,111,113,114,116,117,
	119,120,121,123,124,125,127,128,129,131,132,133,134,136,137,138,
	139,140,141,143,144,145,146,147,148,149,150,151,152,153,154,155,
	156,157,158,159,160,161,162,163,164,164,165,166,167,168,169,170,
	171,171,172,173,174,175,175,176,177,178,179,179,180,181,182,182,
	183,184,185,185,186,187,187,188,189,190,190,191,192,192,193,194,
	194,194,195,196,196,197,197,198,199,199,200,201,201,202,202,203,
	204,204,205,205,206,207,207,208,208,209,209,210,211,211,212,212,
	213,213,214,214,215,215,216,216,217,217,218,219,219,220,220,221,
	221,222,222,223,223,224,224,224,225,225,226,226,227,227,228,228,
	229,229,230,230,231,231,231,232,232,233,233,234,234,235,235,235,
	236,236,237,237,238,238,238,239,239,240,240,240,241,241,242,242,
	242,243,243,244,244,244,245,245,246,246,246,247,247,248,248,248,
	249,249,249,250,250,251,251,251,252,252,252,253,253,254,254,255
};

int
main(int argc, char **argv)
{
	register int		i;
	register int		fudge;
	register ColorMap	*cp = &cmap;
	FBIO *fbp;

	if( ! pars_Argv( argc, argv ) ) {
		usage( NULL );
		return	1;
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		return	1;

	switch( flavor )  {

	case 0 : /* Standard - Linear color map */
		(void) fprintf( stderr, "Color map #0, linear (standard).\n" );
		cp = (ColorMap *) NULL;
		break;

	case 1 : /* Reverse linear color map */
		(void) fprintf( stderr, "Color map #1, reverse-linear (negative).\n" );
		for( i = 0; i < 256; i++ ) {
			cp->cm_red[255-i] =
			cp->cm_green[255-i] =
			cp->cm_blue[255-i] = i << 8;
		}
		break;

	case 2 :
		/* Experimental correction, for POLAROID 8x10 print film */
		(void) fprintf( stderr,
			"Color map #2, corrected for POLAROID 809/891 film.\n" );
		/* First entry black */
#define BOOST(point, bias) \
	((int)((bias)+((float)(point)/256.*(255-(bias)))))
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST(i, 70);
			cp->cm_red[i] = fudge << 8;		/* B */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = i;
			cp->cm_green[i] = fudge << 8;	/* G */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST( i, 30 );
			cp->cm_blue[i] = fudge << 8;	/* R */
		}
		break;

	case 3 : /* Standard, with low intensities set to black */
		(void) fprintf( stderr, "Color map #3, low 100 entries black.\n" );
		for( i = 100; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = i << 8;
		}
		break;

	case 4 : /* Amplify middle of the range, for Moss's dim pictures */
#define UPSHIFT	64
		(void) fprintf( stderr,
			"Color map #4, amplify middle range to boost dim pictures.\n" );
		/* First entry black */
		for( i = 1; i< 256-UPSHIFT; i++ )  {
			register int j = i + UPSHIFT;
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = j << 8;
		}
		for( i = 256-UPSHIFT; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = 255 << 8;	/* Full Scale */
		}
		break;

	case 5 : /* University of Utah's color map */
		(void) fprintf( stderr,
			"Color map #5, University of Utah's gamma correcting map.\n" );
		for( i = 0; i < 256; i++ )
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = utah_cmap[i] << 8;
		break;

	case 6 :	/* Delta's */
		(void) fprintf( stderr, "Color map #6, color deltas.\n" );
		/* white at zero */
		cp->cm_red[0] = 65535;
		cp->cm_green[0] = 65535;
		cp->cm_blue[0] = 65535;
		/* magenta at 32 */
		cp->cm_red[32] = 65535;
		cp->cm_blue[32] = 65535;
		/* Red at 64 */
		cp->cm_red[32*2] = 65535;
		/* Yellow ... */
		cp->cm_red[32*3] = 65535;
		cp->cm_green[32*3] = 65535;
		/* Green */
		cp->cm_green[32*4] = 65535;
		/* Cyan */
		cp->cm_green[32*5] = 65535;
		cp->cm_blue[32*5] = 65535;
		/* Blue */
		cp->cm_blue[32*6] = 65535;
		break;

	case 8:
		(void) fprintf( stderr, "Color map #8, Ikcmap 8.\n" );
		for( i = 0; i < 256; i++ ) {
			cp->cm_red[i] = utah8[3*i] << 8;
			cp->cm_green[i] = utah8[3*i+1] << 8;
			cp->cm_blue[i] = utah8[3*i+2] << 8;
		}
		break;

	case 9:
		(void) fprintf( stderr, "Color map #9, Ikcmap 9.\n" );
		for( i = 0; i < 256; i++ ) {
			cp->cm_red[i] = utah9[3*i] << 8;
			cp->cm_green[i] = utah9[3*i+1] << 8;
			cp->cm_blue[i] = utah9[3*i+2] << 8;
		}
		break;

	case 10:	/* Black */
		(void) fprintf( stderr, "Color map #10, solid black.\n" );
		break;

	case 11:	/* White */
		(void) fprintf( stderr, "Color map #11, solid white.\n" );
		for( i = 0; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = 255 << 8;
		}
		break;

	case 12:	/* 18% Grey */
		(void) fprintf( stderr, "Color map #12, 18%% neutral grey.\n" );
		for( i = 0; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = 46 << 8;
		}
		break;

	default:
		(void) fprintf(	stderr,
				"Color map #%d, flavor not implemented!\n",
				flavor );
		usage( NULL );
		return	1;
	}
	fb_wmap( fbp, cp );
	return fb_close( fbp );
}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv(int argc, register char **argv)
{
	register int	c;
	extern int	bu_optind;

	while( (c = bu_getopt( argc, argv, "hF:s:S:w:W:n:N:" )) != EOF ) {
		switch( c ) {
		case 'h' :
			scr_width = scr_height = 1024;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'S':
		case 's':
			/* square file size */
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(bu_optarg);
			break;
		case '?' :
			return	0;
		}
	}
	if( argv[bu_optind] != NULL )
		flavor = atoi( argv[bu_optind] );
	return	1;
}

void
usage(char **argv)
{
	(void) fprintf(stderr,"Usage : fbcmap [-h] [-F framebuffer]\n");
	(void) fprintf(stderr,"	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height]\n");
	(void) fprintf(stderr,"	[map_number]\n" );
	(void) fprintf( stderr,
			"Color map #0, linear (standard).\n" );
	(void) fprintf( stderr,
			"Color map #1, reverse-linear (negative).\n" );
	(void) fprintf( stderr,
		"Color map #2, corrected for POLAROID 809/891 film.\n" );
	(void) fprintf( stderr,
			"Color map #3, low 100 entries black.\n" );
	(void) fprintf( stderr,
		"Color map #4, amplify middle range to boost dim pictures.\n" );
	(void) fprintf( stderr,
		"Color map #5, University of Utah's gamma correcting map.\n" );
	(void) fprintf( stderr, "Color map #6, color deltas.\n" );
	(void) fprintf( stderr, "Color map #8, ikcmap 8.\n" );
	(void) fprintf( stderr, "Color map #9, ikcmap 9.\n" );
	(void) fprintf( stderr, "Color map #10, solid black.\n" );
	(void) fprintf( stderr, "Color map #11, solid white.\n" );
	(void) fprintf( stderr, "Color map #12, 18%% neutral grey.\n" );
}

/* Ikcmap 8 & 9 colormaps */
unsigned char utah8[256*3] = {
	/* from 27.rle */
	249, 58, 8,
	250, 50, 7,
	250, 43, 6,
	250, 36, 5,
	250, 29, 4,
	251, 21, 3,
	251, 14, 2,
	251, 7, 1,
	252, 0, 0,
	244, 0, 7,
	236, 0, 15,
	228, 0, 23,
	220, 0, 31,
	212, 0, 39,
	204, 0, 47,
	196, 0, 55,
	189, 0, 63,
	181, 0, 71,
	173, 0, 79,
	165, 0, 87,
	157, 0, 95,
	149, 0, 103,
	141, 0, 111,
	133, 0, 119,
	126, 0, 127,
	118, 0, 134,
	110, 0, 142,
	102, 0, 150,
	94, 0, 158,
	86, 0, 166,
	78, 0, 174,
	70, 0, 182,
	63, 0, 190,
	55, 0, 198,
	47, 0, 206,
	39, 0, 214,
	31, 0, 222,
	23, 0, 230,
	15, 0, 238,
	7, 0, 246,
	0, 0, 254,
	7, 7, 253,
	14, 15, 252,
	21, 22, 252,
	28, 30, 251,
	35, 37, 251,
	42, 45, 250,
	49, 52, 250,
	56, 60, 249,
	63, 68, 249,
	70, 75, 248,
	77, 83, 248,
	84, 90, 247,
	91, 98, 247,
	98, 105, 246,
	105, 113, 246,
	112, 121, 245,
	119, 128, 244,
	126, 136, 244,
	133, 143, 243,
	140, 151, 243,
	147, 158, 242,
	154, 166, 242,
	161, 173, 241,
	168, 181, 241,
	175, 189, 240,
	182, 196, 240,
	189, 204, 239,
	196, 211, 239,
	203, 219, 238,
	210, 226, 238,
	217, 234, 237,
	225, 242, 237,
	224, 234, 235,
	223, 227, 233,
	222, 220, 232,
	221, 212, 230,
	220, 205, 229,
	219, 198, 227,
	219, 191, 225,
	218, 183, 224,
	217, 176, 222,
	216, 169, 221,
	215, 161, 219,
	214, 154, 217,
	214, 147, 216,
	213, 140, 214,
	212, 132, 213,
	211, 125, 211,
	210, 118, 209,
	209, 110, 208,
	208, 103, 206,
	208, 96, 205,
	207, 89, 203,
	206, 81, 201,
	205, 74, 200,
	204, 67, 198,
	203, 59, 197,
	203, 52, 195,
	202, 45, 193,
	201, 38, 192,
	200, 30, 190,
	199, 23, 189,
	198, 16, 187,
	198, 9, 186,
	191, 15, 185,
	185, 22, 184,
	179, 28, 183,
	173, 35, 183,
	167, 41, 182,
	160, 48, 181,
	154, 54, 180,
	148, 61, 180,
	142, 67, 179,
	136, 74, 178,
	129, 80, 177,
	123, 87, 177,
	117, 93, 176,
	111, 100, 175,
	105, 106, 174,
	99, 113, 174,
	92, 119, 173,
	86, 126, 172,
	80, 132, 171,
	74, 139, 171,
	68, 145, 170,
	61, 152, 169,
	55, 158, 168,
	49, 165, 168,
	43, 171, 167,
	37, 178, 166,
	30, 184, 165,
	24, 191, 165,
	18, 197, 164,
	12, 204, 163,
	6, 210, 162,
	0, 217, 162,
	0, 211, 159,
	0, 206, 156,
	0, 201, 153,
	0, 196, 151,
	0, 191, 148,
	0, 186, 145,
	0, 181, 142,
	0, 176, 140,
	0, 171, 137,
	0, 166, 134,
	0, 160, 131,
	0, 155, 129,
	0, 150, 126,
	0, 145, 123,
	0, 140, 120,
	0, 135, 118,
	0, 130, 115,
	0, 125, 112,
	0, 120, 109,
	0, 115, 107,
	0, 110, 104,
	0, 104, 101,
	0, 99, 98,
	0, 94, 96,
	0, 89, 93,
	0, 84, 90,
	0, 79, 87,
	0, 74, 85,
	0, 69, 82,
	0, 64, 79,
	0, 59, 76,
	0, 54, 74,
	0, 52, 71,
	0, 50, 69,
	0, 48, 67,
	0, 47, 64,
	0, 45, 62,
	0, 43, 60,
	0, 42, 57,
	0, 40, 55,
	0, 38, 53,
	0, 37, 50,
	0, 35, 48,
	0, 33, 46,
	0, 32, 43,
	0, 30, 41,
	0, 28, 39,
	0, 27, 37,
	0, 25, 34,
	0, 23, 32,
	0, 21, 30,
	0, 20, 27,
	0, 18, 25,
	0, 16, 23,
	0, 15, 20,
	0, 13, 18,
	0, 11, 16,
	0, 10, 13,
	0, 8, 11,
	0, 6, 9,
	7, 7, 1,
	15, 14, 2,
	22, 21, 3,
	30, 29, 4,
	37, 36, 5,
	45, 43, 6,
	53, 50, 7,
	60, 58, 8,
	68, 65, 9,
	75, 72, 10,
	83, 80, 11,
	91, 87, 12,
	98, 94, 13,
	106, 101, 14,
	113, 109, 15,
	121, 116, 16,
	129, 123, 17,
	136, 131, 18,
	144, 138, 19,
	151, 145, 20,
	159, 152, 21,
	167, 160, 22,
	174, 167, 23,
	182, 174, 24,
	189, 182, 25,
	197, 189, 26,
	205, 196, 27,
	212, 203, 28,
	220, 211, 29,
	227, 218, 30,
	235, 225, 31,
	243, 233, 32,
	243, 225, 31,
	243, 218, 30,
	243, 211, 29,
	244, 203, 28,
	244, 196, 27,
	244, 189, 26,
	244, 182, 25,
	245, 174, 24,
	245, 167, 23,
	245, 160, 22,
	246, 152, 21,
	246, 145, 20,
	246, 138, 19,
	246, 131, 18,
	247, 123, 17,
	247, 116, 16,
	247, 109, 15,
	248, 101, 14,
	248, 94, 13,
	248, 87, 12,
	248, 80, 11,
	249, 72, 10,
	249, 65, 9,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	255, 255, 255,
};
unsigned char utah9[256*3] = {
	/* from 31.rle */
	42, 89, 105,
	37, 85, 101,
	32, 80, 97,
	26, 76, 93,
	21, 71, 89,
	16, 67, 85,
	10, 62, 81,
	5, 58, 77,
	0, 54, 74,
	0, 52, 72,
	0, 51, 70,
	0, 50, 68,
	0, 48, 66,
	0, 47, 65,
	0, 46, 63,
	0, 44, 61,
	0, 43, 59,
	0, 42, 58,
	0, 41, 56,
	0, 39, 54,
	0, 38, 52,
	0, 37, 51,
	0, 35, 49,
	0, 34, 47,
	0, 33, 45,
	0, 32, 44,
	0, 30, 42,
	0, 29, 40,
	0, 28, 38,
	0, 27, 37,
	0, 25, 35,
	0, 24, 33,
	0, 23, 31,
	0, 21, 29,
	0, 20, 28,
	0, 19, 26,
	0, 17, 24,
	0, 16, 22,
	0, 15, 21,
	0, 14, 19,
	0, 12, 17,
	0, 11, 15,
	0, 10, 14,
	0, 9, 12,
	0, 7, 10,
	0, 6, 8,
	0, 5, 7,
	0, 3, 5,
	0, 2, 3,
	0, 1, 1,
	0, 0, 0,
	5, 5, 0,
	11, 11, 1,
	17, 16, 2,
	23, 22, 3,
	28, 27, 3,
	34, 33, 4,
	40, 38, 5,
	46, 44, 6,
	52, 49, 6,
	57, 55, 7,
	63, 61, 8,
	69, 66, 9,
	75, 72, 9,
	81, 77, 10,
	86, 83, 11,
	92, 88, 12,
	98, 94, 12,
	104, 99, 13,
	109, 105, 14,
	115, 110, 15,
	121, 116, 16,
	127, 122, 16,
	133, 127, 17,
	138, 133, 18,
	144, 138, 19,
	150, 144, 19,
	156, 149, 20,
	162, 155, 21,
	167, 160, 22,
	173, 166, 22,
	179, 171, 23,
	185, 177, 24,
	190, 183, 25,
	196, 188, 25,
	202, 194, 26,
	208, 199, 27,
	214, 205, 28,
	219, 210, 28,
	225, 216, 29,
	231, 221, 30,
	237, 227, 31,
	243, 233, 32,
	243, 227, 31,
	243, 221, 30,
	243, 216, 29,
	243, 210, 28,
	244, 205, 28,
	244, 199, 27,
	244, 194, 26,
	244, 188, 25,
	244, 183, 25,
	245, 177, 24,
	245, 171, 23,
	245, 166, 22,
	245, 160, 22,
	246, 155, 21,
	246, 149, 20,
	246, 144, 19,
	246, 138, 19,
	246, 133, 18,
	247, 127, 17,
	247, 122, 16,
	247, 116, 16,
	247, 110, 15,
	247, 105, 14,
	248, 99, 13,
	248, 94, 12,
	248, 88, 12,
	248, 83, 11,
	249, 77, 10,
	249, 72, 9,
	249, 66, 9,
	249, 61, 8,
	249, 55, 7,
	250, 49, 6,
	250, 44, 6,
	250, 38, 5,
	250, 33, 4,
	250, 27, 3,
	251, 22, 3,
	251, 16, 2,
	251, 11, 1,
	251, 5, 0,
	252, 0, 0,
	245, 0, 6,
	239, 0, 12,
	233, 0, 18,
	227, 0, 24,
	221, 0, 30,
	215, 0, 36,
	209, 0, 42,
	203, 0, 48,
	197, 0, 54,
	191, 0, 60,
	185, 0, 66,
	179, 0, 72,
	173, 0, 78,
	167, 0, 84,
	161, 0, 90,
	155, 0, 96,
	149, 0, 102,
	143, 0, 108,
	137, 0, 114,
	131, 0, 120,
	126, 0, 127,
	119, 0, 133,
	114, 0, 139,
	107, 0, 145,
	102, 0, 151,
	95, 0, 157,
	90, 0, 163,
	83, 0, 169,
	78, 0, 175,
	71, 0, 181,
	66, 0, 187,
	59, 0, 193,
	54, 0, 199,
	47, 0, 205,
	42, 0, 211,
	35, 0, 217,
	30, 0, 223,
	23, 0, 229,
	18, 0, 235,
	11, 0, 241,
	6, 0, 247,
	0, 0, 254,
	5, 5, 253,
	10, 11, 253,
	16, 17, 252,
	21, 23, 252,
	26, 28, 251,
	32, 34, 251,
	37, 40, 251,
	42, 46, 250,
	48, 51, 250,
	53, 57, 249,
	58, 63, 249,
	64, 69, 249,
	69, 74, 248,
	75, 80, 248,
	80, 86, 247,
	85, 92, 247,
	91, 97, 247,
	96, 103, 246,
	101, 109, 246,
	107, 115, 245,
	112, 121, 245,
	117, 126, 245,
	123, 132, 244,
	128, 138, 244,
	133, 144, 243,
	139, 149, 243,
	144, 155, 243,
	150, 161, 242,
	155, 167, 242,
	160, 172, 241,
	166, 178, 241,
	171, 184, 241,
	176, 190, 240,
	182, 195, 240,
	187, 201, 239,
	192, 207, 239,
	198, 213, 239,
	203, 218, 238,
	208, 224, 238,
	214, 230, 237,
	219, 236, 237,
	225, 242, 237,
	219, 237, 233,
	214, 233, 229,
	208, 228, 225,
	203, 224, 221,
	198, 219, 217,
	192, 215, 213,
	187, 210, 209,
	182, 206, 205,
	176, 201, 202,
	171, 197, 198,
	166, 192, 194,
	160, 188, 190,
	155, 183, 186,
	149, 179, 182,
	144, 174, 178,
	139, 170, 174,
	133, 165, 171,
	128, 161, 167,
	123, 156, 163,
	117, 152, 159,
	112, 148, 155,
	107, 143, 151,
	101, 139, 147,
	96, 134, 143,
	91, 130, 139,
	85, 125, 136,
	80, 121, 132,
	74, 116, 128,
	69, 112, 124,
	64, 107, 120,
	58, 103, 116,
	53, 98, 112,
	48, 94, 108,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	255, 255, 255,
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
