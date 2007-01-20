/*                       R T R E G I S . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2007 United States Government as represented by
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
 */
/** @file rtregis.c
 *
 *  This is a program will register a Unix-Plot file with its companion
 *  pix file.  It is assumed that both images were ray-traced at the same
 *  rotation.  A homogeneous transformation matrix is constructed from the
 *  RT log files of the two images.  This matrix will permit the translation
 *  and scaling of the plot file so that it can be readily overlaid onto its
 *  pixel image mate.
 *
 *  It is expected that the first log file given corresponds to the image
 *  file to be overlaid onto the image that corresponds to the second log
 *  file.  Also for the moment it is expected that the first log will
 *  correspond to a Unix-Plot file, whereas the second will correspond to a
 *  pixel file.  If both images where Unix-Plot files, they can be overlaid
 *  by simply concatentating them: "cat file.pl file.pl >> out.pl"
 *
 *  The program conisists of three parts:
 *	1) take view, orientation, eye_position, and size from two rt log
 *          files, and use this information to build up the registration matrix;
 *	2) puts out a registration matrix and a new space command to be
 *	   used by plrot in lieu of -a#, -e#, -g to rotate/transform the
 *	   UNIX_Plot file
 *			and
 *	3) involve pix-fb -o to do the overlaying of the actual files.
 *	4) Note: two pixel files (one lo-res, one hi-res) will be registered
 *	   later in a slightly different way.
 *
 *  Authors -
 *	Susanne L. Muuss, J.D.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg. 394
 *	The U. S. Army Ballistic Reasearch Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSregis[] = "@(#)$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"


#define NAMELEN 40
#define BUFF_LEN 256
#define FALSE 0
#define TRUE 1

char usage[] = "\
Usage:  regis plot.log pix.log\n";


int	mat_build(fastf_t *mat1, fastf_t *mat2, fastf_t *regismat);
int	read_rt_file(FILE *infp, char *name, fastf_t *model2view);
void	print_info(fastf_t *mat);

static FILE	*fp;
int		verbose;		/* to be used for debugging */

/*
 *
 *                     M A I N
 *
 *  Main exists to coordinate the actions of the parts of this program.
 *  It also processes its own arguments (argc and argv).
 */
int
main(int argc, char **argv)
{

	mat_t		mod2view1;		/* first log matrix its view */
	mat_t		mod2view2;		/* second log matrix to its view*/
	mat_t		regismat;		/* registration matrix */
	mat_t		view2model;		/* matrix for converting from view to model space */
	int		ret;			/* function return code */

	MAT_IDN(mod2view1);				/* makes an identity matrix */
	MAT_IDN(mod2view2);
	MAT_IDN(regismat);

	/* Check to see that the correct format is given, else print
	 * usage message.
	 */

	if(argc != 3)  {
		fputs(usage, stderr);
		return 1;
	}

	/* Now process the arguments from main: i.e. open the log files
	 * for reading and send to read_rt_file().
	 * Send read_rt_file() a pointer to local model matrix
	 * and to the appropriate log file.
	 * ( Note &view2model[0] can be used, but is not elegant.)
	 */

	fp = fopen(argv[1], "r");
	if( fp == NULL )  {
		perror(argv[1]);
		return 1;
	}

	ret = read_rt_file(fp, argv[1], mod2view1);
	if(ret < 0)  {
	    return 2;
	}
	fclose(fp);		/* clean up */

	fp = fopen(argv[2], "r");
	if( fp == NULL )  {
		perror(argv[2]);
		return 2;
	}

	ret = read_rt_file(fp, argv[2], mod2view2);
	if(ret < 0)  {
	    return 2;
	}

	fclose(fp);

	if(verbose)  {
		bn_mat_inv(view2model, mod2view1);
		bn_mat_print("mod2view1-plot.log", mod2view1);
		bn_mat_print("mod2view2-pix.log", mod2view2);
		fprintf(stderr, "mod2view1[0, 1, 2, 3, 15]: %.6f, %.6f, %.6f, %.6f, %.6f\n",
		mod2view1[0], mod2view1[1], mod2view1[2], mod2view1[3], mod2view1[15]);
	}

	/* Now build the registration matrix for the two files. */

	ret = mat_build(mod2view1, mod2view2, regismat);
	if(ret == FALSE)  {
		fprintf(stderr, "regis: can't build registration matrix!\n");
		return 3;
	}
	print_info(regismat);

	return 0;
}


/*
 *		 M A T _ B U I L D
 *
 * This routine takes pointers to two matices corresponding to the two
 * files to be registered and a registration matrix.
 * It builds the registration matrix.  It returns success or failure.
 */

int
mat_build(fastf_t *mat1, fastf_t *mat2, fastf_t *regismat)
{

	vect_t	adelta, bdelta;		/* deltas for mod1 and mod2 */
	vect_t	delta;			/* difference bet. mod1 and mod2 deltas */
	fastf_t	scale;

	/* At this point it is important to check that the rotation part
	 * of the matices is within a certain tolerance: ie. that the
	 * two images were raytraced from the same angle.  No overlays will
	 * be possible if they are not at the same rotation.
	 */

	/* Now record the deltas: the translation part of the matrix. */
	VSET(adelta, mat1[MDX], mat1[MDY], mat1[MDZ]);
	VSET(bdelta, mat2[MDX], mat2[MDY], mat2[MDZ]);

	/* Take the difference between the deltas. Also scale the size
	 * of the model (scale).  These will be used to register two
	 * pixel files later on.
	 */

	VSUB2(delta, adelta, bdelta);
	scale = mat1[15]/mat2[15];

	VPRINT("delta", delta);
	fprintf(stderr, "scale: %.6f\n", scale);

	/* If the first log corresponds to a UNIX-Plot file, following
	 * applies.  Since UNIX-Plot files are in model coordinates, the
	 * mod2view2 ("model2pix") is also the registration matrix.  In
	 * this case, pl-fb needs to learn that the UNIX-Plot file's space
	 * runs from -1 -> 1 in x and y.  This can be done by adding an
	 * alternate space command in that program. Therefore the below
	 * applies.
	 * What if the first log corresponds to a hi-res pixel file to be
	 * registered with a lo-res pixel file?  Then the above calculated
	 * deltas are used.   This will be implemented later.
	 */

	MAT_COPY( regismat, mat2);
	bn_mat_print("regismat", regismat);
	return(1);				/* OK */
}


/*		P R I N T _ I N F O
 *
 *  This routine takes as its input parameter a registration matrix.  Its
 *  sole task is to print this matrix out in a form usable by plrot.  It
 *  also prints out the parameters for the new space command for plrot.
 */

void
print_info(fastf_t *mat)
{

	int	i;

	fprintf(stdout, "plrot -m\"");
	for( i = 0; i < 15; i++ )  {
		fprintf(stdout, "%.6f ", mat[i]);
	}
	fprintf(stdout, "%g\" -S\"-1 -1 -1 1 1 1\"\n", mat[15]);
	return;
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
