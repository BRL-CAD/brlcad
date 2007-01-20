/*                    R E A D - R T L O G . C
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
/** @file read-rtlog.c
 *
 *  This is a program will read an RT log file.  It is meant to be
 *  used by any other program that needs to read an RT log file to
 *  extract the model size, orientation, eye position, azimuth, and
 *  elevation from the log file.
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
static const char RCSreadfile[] = "@(#)$Header$";
#endif

#include "common.h"

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

#define BUFF_LEN 256
#define FALSE 0
#define TRUE 1


extern int	verbose;


/*		R E A D _ R T _ F I L E
 *
 * Read an RT program's log file line by line until it either finds
 * view, orientation, eye_postion, and size of the model, or it hits the
 * end of file.  When a colon is found, sscanf() retrieves the
 * necessary information.  It takes a file pointer, file name, and a matrix
 * pointer as parameters.  It returns 0 okay or < 0 failure.
 */

int
read_rt_file(FILE *infp, char *name, fastf_t *model2view)
{
	FILE		*fp;
	fastf_t		azimuth;		/* part of the view */
	fastf_t		elevation;		/* part of the view */
	quat_t		orientation;		/* orientation */
	point_t		eye_pos;
	fastf_t		m_size;			/* size of model in mm */
	char		*ret;			/* return code for fgets */
	char		string[BUFF_LEN];	/* temporary buffer */
	char		*arg_ptr;		/* place holder */
	char		forget_it[9];		/* "azimuth" catcher, then forget */
	int		i;			/* reusable counter */
	int		num;			/* return code for sscanf */
	int		seen_view;		/* these are flags.  */
	int		seen_orientation;
	int		seen_eye_pos;
	int		seen_size;

	mat_t		rotate, xlate;


	/* Open the incoming file for reading */

	fp = fopen(name, "r");
	if( fp == NULL )  {
		perror(name);
		bu_bomb("unable to open file for reading");
	}

	/* Set all flags to ready state.  */

	seen_view = FALSE;
	seen_orientation = FALSE;
	seen_eye_pos = FALSE;
	seen_size = FALSE;

	if(verbose)  {
		fprintf(stderr, "set flags: view:%d, orient:%d, eye_pos:%d, size:%d\n",
		seen_view, seen_orientation, seen_eye_pos, seen_size);
	}

	/* feof returns 1 on failure */

	while( feof(infp) == 0 )  {

		/* clear the buffer */
		for( i = 0; i < BUFF_LEN; i++ )  {
			string[i] = '\0';
		}
		ret = fgets(string, BUFF_LEN, infp);

		if( ret == NULL )  {
			/* There are two times when NULL might be seen:
			 * at the end of the file (handled above) and
			 * when the process dies horriblely and unexpectedly.
			 */

			if( feof(infp) )
				break;

			/* This needs to be seen only if there is an
			 * unexpected end.
			 */
			fprintf(stderr, "read_rt_file: read failure on file %s\n",
 				name);
 			return(-1);
		}

		/* Check the first for a colon in the buffer.  If there is
		 * one, replace it with a NULL, and set a pointer to the
		 * next space.  Then feed the buffer to
		 * strcmp see whether it is the view, the orientation,
		 * the eye_position, or the size.  If it is, then sscanf()
		 * the needed information into the appropriate variables.
		 * If the keyword is not found, go back for another line.
		 *
		 * Set arg_ptr to NULL so it can be used as a flag to verify
		 * finding a colon in the input buffer.
		 */

		arg_ptr = NULL;

		for( i = 0; i < BUFF_LEN; i++ )  {
			/* Check to make sure the first char. is not a NULL;
			 * if it is, go back for a new line.
			 */
			if( string[i] == '\0' )  {
				break;
			}
			if( string[i] == ':')  {
				/* If a colon is found, set arg_ptr to the
				 * address of the colon, and break: no need to
				 * look for more colons on this line.
				 */

				if(verbose)  {
					fprintf(stderr, "found colon\n");
				}

				string[i] = '\0';
				arg_ptr = &string[++i];		/* increment before using */
				break;
			}
		}

		/* Check to see if a colon has been found.  If not, get another
		 * input line.
		 */

		if( arg_ptr == NULL )  {
			continue;
		}

		/* Now compare the first word in the buffer with the
		 * key words wanted.  If there is a match, read the
		 * information that follows into the appropriate
		 * variable, and set a flag to indicate that the
		 * magic thing has been seen.
		 *
		 * Note two points of interest: scanf() does not like %g;
		 * use %lf.  Likewise, don't use %g for printing out info:
		 * it might get rounded to the nearest integer.  Use %.6f
		 * instead.
		 * Also, if loading a whole array of characters
		 * with %s, then the name of the array can be used for the
		 * destination.  However, if the characters are loaded
		 * individually into the subsripted spots with %c (or equiv),
		 * the address of the location must be provided: &eye_pos[0].
		 */

		if(strcmp(string, "View") == 0)  {
			num = sscanf(arg_ptr, "%lf %s %lf", &azimuth, forget_it, &elevation);
			if( num != 3)  {
				fprintf(stderr, "View= %.6f %s %.6f elevation\n", azimuth, forget_it, elevation);
				return(-1);
			}
			seen_view = TRUE;
		} else if(strcmp(string, "Orientation") == 0)  {
			num = sscanf(arg_ptr, "%lf, %lf, %lf, %lf",
				&orientation[0], &orientation[1], &orientation[2],
				&orientation[3]);

 			if(num != 4)  {
				fprintf(stderr, "Orientation= %.6f, %.6f, %.6f, %.6f\n",
				 	V4ARGS(orientation) );
				return(-1);
			}
			seen_orientation = TRUE;
		} else if(strcmp(string, "Eye_pos") == 0)  {
			num = sscanf(arg_ptr, "%lf, %lf, %lf", &eye_pos[0],
				&eye_pos[1], &eye_pos[2]);
			if( num != 3)  {
				fprintf(stderr, "Eye_pos= %.6f, %.6f, %.6f\n",
					V3ARGS(eye_pos) );
				return(-1);
			}
			seen_eye_pos = TRUE;
		} else if(strcmp(string, "Size") == 0)  {
			num = sscanf(arg_ptr, "%lf", &m_size);
			if(num != 1)  {
				fprintf(stderr, "Size=%.6f\n", m_size);
				return(-1);
			}
			seen_size = TRUE;
		}
	}

	/* Check that all the information to proceed is available */

	if( seen_view != TRUE )  {
		fprintf(stderr, "View not read for %s!\n", name);
		return(-1);
	}

	if( seen_orientation != TRUE )  {
		fprintf(stderr, "Orientation not read for %s!\n", name);
		return(-1);
	}

	if( seen_eye_pos != TRUE )  {
		fprintf(stderr, "Eye_pos not read for %s!\n", name);
		return(-1);
	}

	if ( seen_size != TRUE )  {
		fprintf(stderr, "Size not read for %s!\n", name);
		return(-1);
	}

	if( verbose )  {
		/* Take your chances on the %g with the orientation: it is difficult
		 * to say how many figures it will take to print the orientation back,
		 * and it is disconcerting to have it come back as 0.
		 */

		fprintf(stderr, "logfile: %s\n", name);
		fprintf(stderr, "view: azimuth %.6f; elevation: %.6f\n", azimuth, elevation);
		fprintf(stderr, "orientation: %g, %g, %g, %g\n", V4ARGS(orientation) );
		fprintf(stderr, "eye_pos: %.6f, %.6f, %.6f\n", V3ARGS(eye_pos) );
		fprintf(stderr, "size: %.6fmm\n", m_size);
	}

	/* Build the view2model matrix. */

	quat_quat2mat( rotate, orientation );
	rotate[15] = 0.5 * m_size;
	MAT_IDN( xlate );
	MAT_DELTAS( xlate, -eye_pos[0], -eye_pos[1], -eye_pos[2] );
	bn_mat_mul( model2view, rotate, xlate );

	if(verbose)  {
		 bn_mat_print("model2view", model2view);
	}

	fclose(fp);		/* clean up */
	return(0);
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
