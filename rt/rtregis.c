/*                    R E G I S T E R
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
*	2) possibley use plrot to rotate/transform the UNIX_Plot file
*	   or do this here
*			and
*	3) involve pix-fb -o to do the overlaying of the actual files.
*
*
*  Authors -
*	Susanne L. Muuss, J.D.
*	
*
*  Source -
*	SECAD/VLD Computing Consortium, Bldg. 394
*	The U. S. Army Ballistic Reasearch Laboratory
*	Aberdeen Proving Ground, Maryland  21005
*
*  Copyright Notice -
*	This software is Copyright (C) 1991 by the United States Army.
*	All rights reserved.
*/
#ifndef lint
static char RCSregis[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"

#define NAMELEN 40
#define BUFF_LEN 256
#define FALSE 0
#define TRUE 1

char usage[] = "\
Usage:  regis plot.log pix.log\n";


int	mat_build();
int	read_rt_file();
int	overlay();

static FILE	*fp;

/*
 *
 *                     M A I N
 *
 *  Main exists to coordinate the actions of the parts of this program.
 *  It also processes its own arguments (argc and argv).
 */

main(argc, argv)
int	argc;
char	**argv;

{

	mat_t		mod1;			/* matrix from first log */
	mat_t		mod2;			/* matrix from second log */
	mat_t		regismat;		/* registration matrix */
	mat_t		view2model;		/* matrix for converting from view to model space */
	int		ret;			/* function return code */

	mat_idn(mod1);				/* makes an identity matrix */
	mat_idn(mod2);
	mat_idn(regismat);

	/* Check to see that the correct format is given, else print
	 * usage message.
	 */

	if(argc != 3)  {
		fputs(usage, stderr);
		exit(-1);
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
		exit(-1);
	}

	ret = read_rt_file(fp, argv[1], mod1);
	if(ret < 0)  {
		exit(-1);
	}
	fclose(fp);		/* clean up */

	fp = fopen(argv[2], "r");
	if( fp == NULL )  {
		perror(argv[2]);
		exit(-1);
	}

	ret = read_rt_file(fp, argv[2], mod2);
	if(ret < 0)  {
		exit(-1);
	}

	fclose(fp);

/*	mat_inv(view2model, model2view);
 */
mat_print("mod1-plot.log", mod1);
mat_print("mod2-pix.log", mod2);
/* fprintf(stderr, "mod1[0, 1, 2, 3, 15]: %g, %g, %g, %g, %g\n",
 *	mod1[0], mod1[1], mod1[2], mod1[3], mod1[15]);
 */

	/* Now build the registration matrix for the two files. */

	ret = mat_build(mod1, mod2, regismat);
	if(ret == FALSE)  {
		fprintf(stderr, "regis: can't build registration matrix!\n");
		exit(-1);
	}
	
	exit(0);

}


/*
 *		 M A T _ B U I L D
 *
 * This routine takes pointers to two matices corresponding to the two
 * files to be registered and a registration matrix.
 * It builds the registration matrix.  It returns success or failure.
 */

int
mat_build(mat1, mat2, regismat)
mat_t	mat1;
mat_t	mat2;
mat_t	regismat;
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
	 * of the model (scale).
	 */

	VSUB2(delta, adelta, bdelta);
	scale = mat1[15]/mat2[15];

	VPRINT("delta", delta);
	fprintf(stderr, "scale: %g\n", scale);	
}


/*		R E A D _ R T _ F I L E
 *
 * This routine reads an rt_log file line by line until it either finds
 * view, orientation, eye_postion, and size of the model, or it hits the
 * end of file.  When a colon is found, sscanf() retrieves the
 * necessary information.  It takes a file pointer, file name, and a matrix
 * pointer as parameters.  It returns 0 okay or < 0 failure.
 */

int
read_rt_file(infp, name, model2view)
FILE	*infp;
char	*name;
mat_t 	model2view;
{

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
	mat_t		tmp_mat;

	/* Set all flags to ready state.  */

	seen_view = FALSE;
	seen_orientation = FALSE;
	seen_eye_pos = FALSE;
	seen_size = FALSE;

/* fprintf(stderr, "set flags: view=%d, orient.=%d, eye_pos=%d, size=%d\n",
 *	seen_view, seen_orientation, seen_eye_pos, seen_size);
 */

	/* feof returns 1 on failure */

	while( feof(infp) == 0 )  {

		/* clear the buffer */	
		for( i = 0; i < BUFF_LEN; i++ )  {
			string[i] = '\0';
		}
		ret = fgets(string, BUFF_LEN, infp);
#ifdef
		if( ret == NULL )  {
			fprintf(stderr, "read_rt_log: read failure on file %s\n",
 				name);
 			return(-1);
			break;
		}
#endif

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

/* fprintf(stderr, "found colon\n");
 */
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
		 * use %lf.  Also, if loading a whole array of characters
		 * with %s, then the name of the array can be used for the
		 * destination.  However, if the characters are loaded 
		 * individually into the subsripted spots with %c (or equiv),
		 * the address of the location must be provided: &eye_pos[0].
		 */

		if(strcmp(string, "View") == 0)  {
			num = sscanf(arg_ptr, "%lf %s %lf", &azimuth, forget_it, &elevation);
			if( num != 3)  {
				fprintf(stderr, "View= %g %s %g elevation\n", azimuth, forget_it, elevation);
				return(-1);
			}
			seen_view = TRUE;
		} else if(strcmp(string, "Orientation") == 0)  {
			num = sscanf(arg_ptr, "%lf, %lf, %lf, %lf",
				&orientation[0], &orientation[1], &orientation[2],
				&orientation[3]);

 			if(num != 4)  {
				fprintf(stderr, "Orientation= %g, %g, %g, %g\n",
				 	V4ARGS(orientation) );
				return(-1);
			}
			seen_orientation = TRUE;
		} else if(strcmp(string, "Eye_pos") == 0)  {
			num = sscanf(arg_ptr, "%lf, %lf, %lf", &eye_pos[0],
				&eye_pos[1], &eye_pos[2]);
			if( num != 3)  {
				fprintf(stderr, "Eye_pos= %g, %g, %g\n",
					V3ARGS(eye_pos) );
				return(-1);
			}
			seen_eye_pos = TRUE;
		} else if(strcmp(string, "Size") == 0)  {
			num = sscanf(arg_ptr, "%lf", &m_size);
			if(num != 1)  {
				fprintf(stderr, "Size=%g\n", m_size);
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

	/* For now, just print the stuff */

	fprintf(stderr, "logfile: %s\n", name);
	fprintf(stderr, "view= %g azimuth, %g elevation\n", azimuth, elevation);
	fprintf(stderr, "orientation= %g, %g, %g, %g\n", V4ARGS(orientation) );
	fprintf(stderr, "eye_pos= %g, %g, %g\n", V3ARGS(eye_pos) );
	fprintf(stderr, "size= %gmm\n", m_size);

	/* Build the view2model matrix. */

	quat_quat2mat( rotate, orientation );
	rotate[15] = 0.5 * m_size;
	mat_idn( xlate );
	MAT_DELTAS( xlate, -eye_pos[0], -eye_pos[1], -eye_pos[2] );
	mat_mul( model2view, rotate, xlate );

 mat_print("model2view", model2view);

	return(0);
}


	

/*		D R A W S C A L E
 *
 * 
 * 
 * 
 * 
 * 
 */

int
drawscale(outfp, startpt, len, hgt, lenv, hgtv, inv_hgtv)
FILE		*outfp;
point_t		startpt;
fastf_t		len;
fastf_t		hgt;
vect_t		lenv;
vect_t		hgtv;
vect_t		inv_hgtv;
{
}


/*		D R A W T I C K S
 *
 *
 *
 *
 *
 */

int
drawticks(outfp, centerpt, hgtv, hgt, inv_hgtv)
FILE		*outfp;
point_t		centerpt;
vect_t		hgtv;
fastf_t		hgt;
vect_t		inv_hgtv;
{
}
