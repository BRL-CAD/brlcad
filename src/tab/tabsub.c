/*                        T A B S U B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
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
/** @file tabsub.c
 *
 *  This program is a simple macro processor for taking
 *  a big table of input values, and a prototype output document,
 *  and generating an instantiation of the output document
 *  for each line of input values.
 *
 *  This program follows "tabinterp", and is the last step in creating
 *  RT animation scripts.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


int	debug = 0;

void	get_proto(char **buffer, char *file);
void	do_lines(FILE *fp, char *buffer);
void	out_mat(matp_t m, FILE *fp);
int	str2chan_index( char *s );
int	multi_words( char *words[], int	nwords );


/*
 *			M A I N
 *
 */
int
main(int argc, char **argv)
{
    char	*prototype;		/* Contains full text of prototype document */
    char	proto_file[256] = {0};

    FILE	*table;
    char	table_file[256] = {0};

    if( argc < 2 || argc > 3 )  {
	fprintf(stderr,"Usage:  tabsub prototype_file [table_file]\n");
	exit(12);
    }
    strncpy(proto_file, argv[1], 255);

    /* Acquire in-core copy of prototype file */
    get_proto( &prototype, proto_file );

    if( argc < 3 )  {
	table = stdin;
    } else {
	strncpy(table_file, argv[2], 255);
	if( (table = fopen( table_file, "r" )) == NULL )  {
	    perror( table_file );
	    exit(3);
	}
    }
    do_lines( table, prototype );
    return 0;
}

void
get_proto(char **buffer, char *file)
{
    struct stat	sb;
    int	fd;
    int bytes_read = 0;

    if (file == NULL) {
	bu_log("tabsub: get_proto given null file name\n");
	return;
    }

    if ( (fd = open( file, 0 )) < 0) {
	perror(file);
	exit(1);
    }

    if (fstat( fd, &sb ) != 0 )  {
	perror(file);
	exit(1);
    }

    if( sb.st_size == 0 )  {
	fprintf(stderr,"tabsub:  %s is empty\n", file );
	exit(1);
    }
    *buffer = bu_malloc( (size_t)(sb.st_size+4), "prototype document");
    bytes_read = read( fd, *buffer, (size_t)sb.st_size );
    if ( bytes_read != sb.st_size )  {
	printf("only read %d bytes (expecting %ld)\n", bytes_read, (long)sb.st_size);
	perror(file);
	exit(2);
    }
    (*buffer)[sb.st_size] = '\0';
    return;
}

#define	NCHANS	1024
char	linebuf[NCHANS*10];
int	line;				/* input line number */

char	*chanwords[NCHANS+1];
int	nwords;				/* # words in chanwords[] */

#define NTOKENWORDS	16
char	*tokenwords[NTOKENWORDS+1];

void
do_lines(FILE *fp, char *buffer)
{
#define TOKLEN	128
	char	token[TOKLEN];
	int	ntokenwords;
	register char	*cp;
	register char	*tp;
	int	i;

	for( line=0; /*NIL*/; line++ )  {
		linebuf[0] = '\0';
		(void)fgets( linebuf, sizeof(linebuf), fp );
		if( feof(fp) )
			break;

		/* Skip blank or commented out lines */
		if( linebuf[0] == '\0' ||
		    linebuf[0] == '#' ||
		    linebuf[0] == '\n' )
			continue;

		if(debug)  {
			fprintf(stderr, "Prototype=\n%s", buffer);
			fprintf(stderr, "Line %d='%s'\n", line, linebuf);
		}

		/* Here, there is no way to check for too many words */
		nwords = rt_split_cmd( chanwords, NCHANS+1, linebuf );

		for( cp=buffer; *cp != '\0'; )  {
			if(debug) fputc( *cp, stderr );
			/* Copy all plain text, verbatim */
			if( *cp != '@' )  {
				putc( *cp++, stdout );
				continue;
			}

			/* An '@' sign has been seen, slurp up a token */
			cp++;			/* skip '@' */
			if( *cp == '@' )  {
				/* Double '@' is escape for single one
				 * (just like ARPANET TACs)
				 */
				putc( '@', stdout );
				cp++;		/* skip '@' */
				continue;
			}
			if( *cp == '(' )  {
				cp++;		/* skip '(' */
				tp = token;
				while( *cp && *cp != ')' && tp<&token[TOKLEN-1])  {
					*tp++ = *cp++;
				}
				*tp++ = '\0';
				cp++;		/* skip ')' */
			} else if( isdigit( *cp ) )  {
				tp = token;
				while( isdigit( *cp ) && tp<&token[TOKLEN-1] )  {
					*tp++ = *cp++;
				}
				*tp++ = '\0';
			} else {
				fprintf( stderr,"Line %d:  Bad sequence '@%c'\n", line, *cp);
				fprintf( stdout, "@%c", *cp++ );
				continue;
			}
			if(debug) fprintf(stderr,"token='%s'\n", token);

			if( isdigit( token[0] ) )  {
				fputs( chanwords[str2chan_index(token)],
					stdout );
				continue;
			}
			if( strcmp( token, "line" ) == 0 )  {
				fprintf(stdout, "%d", line );
				continue;
			}
			if( strcmp( token, "time" ) == 0 )  {
				fputs( chanwords[0], stdout );
				continue;
			}

			/* Check here for multi-word tokens */
			ntokenwords = rt_split_cmd( tokenwords, NTOKENWORDS+1,
				token );

			/*  If first character of a word is '@' or '%', that
			 *  signifies substituting the value of the
			 *  indicated channel.  Otherwise the word is literal.
			 */
			for( i=1; i<ntokenwords; i++ )  {
				char	c;
				int	chan;
				c = tokenwords[i][0];
				if( c != '@' && c != '%' )  continue;
				chan = str2chan_index( &tokenwords[i][1] );
				tokenwords[i] = chanwords[chan];
			}

			if( (i=multi_words( tokenwords, ntokenwords )) >= 0 )
				continue;

			if( i == -1 )  {
				fprintf(stderr,
					"Line %d: keyword @(%s) encountered error\n",
					line, token);
				fprintf(stdout,
					"@(%s)", token );
			} else {
				fprintf(stderr,
					"Line %d: keyword @(%s) unknown\n",
					line, token);
				fprintf(stdout,
					"@(%s)", token );
			}
			for( i=0; i<ntokenwords; i++ )  {
				fprintf( stderr,
					"word[%2d] = '%s'\n",
					i, tokenwords[i] );
			}
		}
	}
}

/*
 *  Returns -
 *	-2	unknown keyword
 *	-1	error in processing keyword
 *	 0	OK
 */
int
multi_words( char *words[], int	nwords )
{

	if( strcmp( words[0], "rot" ) == 0 )  {
		mat_t	mat;

		/* Expects rotations rx, ry, rz, in degrees */
		if( nwords < 4 )  return(-1);
		MAT_IDN( mat );
		bn_mat_angles( mat,
		    atof( words[1] ),
		    atof( words[2] ),
		    atof( words[3] ) );
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "xlate" ) == 0 )  {
		mat_t	mat;

		if( nwords < 4 )  return(-1);
		/* Expects translations tx, ty, tz */
		MAT_IDN( mat );
		MAT_DELTAS( mat,
		    atof( words[1] ),
		    atof( words[2] ),
		    atof( words[3] ) );
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "rot_at" ) == 0 )  {
		mat_t	mat;
		mat_t	mat1;
		mat_t	mat2;
		mat_t	mat3;

		/* JG - Expects x, y, z, rx, ry, rz               */
		/* Translation back to the origin by (-x,-y,-z)   */
		/* is done first, then the rotation, and finally  */
		/* back into the original position by (+x,+y,+z). */

		if( nwords < 7 )  return(-1);

		MAT_IDN( mat1 );
		MAT_IDN( mat2 );
		MAT_IDN( mat3 );

		MAT_DELTAS( mat1,
		    -atof( words[1] ),
		    -atof( words[2] ),
		    -atof( words[3] ) );

		bn_mat_angles( mat2,
		    atof( words[4] ),
		    atof( words[5] ),
		    atof( words[6] ) );

		MAT_DELTAS( mat3,
		    atof( words[1] ),
		    atof( words[2] ),
		    atof( words[3] ) );

		bn_mat_mul( mat, mat2, mat1 );
		bn_mat_mul2( mat3, mat );

		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "orient" ) == 0 )  {
		register int i;
		mat_t	mat;
		double	args[8];

		/* Expects tx, ty, tz, rx, ry, rz, [scale]. */
		/* All rotation is done first, then translation */
		/* Note: word[0] and args[0] are the keyword */
		if( nwords < 6+1 )  return(-1);
		for( i=1; i<6+1; i++ )
			args[i] = 0;
		args[7] = 1.0;	/* optional arg, default to 1 */
		for( i=1; i<nwords; i++ )
			args[i] = atof( words[i] );
		MAT_IDN( mat );
		bn_mat_angles( mat, args[4], args[5], args[6] );
		MAT_DELTAS( mat, args[1], args[2], args[3] );
		if( NEAR_ZERO( args[7], VDIVIDE_TOL ) )  {
			/* Nearly zero, signal error */
			fprintf(stderr,"Orient scale arg is near zero ('%s')\n",
				words[7] );
			return(-1);
		} else {
			mat[15] = 1 / args[7];
		}
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "ae" ) == 0 )  {
		mat_t	mat;
		fastf_t	az, el;

		if( nwords < 3 )  return(-1);
		/* Expects azimuth, elev, optional twist */
		az = atof(words[1]);
		el = atof(words[2]);
#if 0
		if( nwords == 3 )
			twist = 0.0;
		else
			twist = atof(words[3]);
#endif
		MAT_IDN( mat );
		/* XXX does not take twist, for now XXX */
		bn_mat_ae( mat, az, el );
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "arb_rot_pt" ) == 0 )  {
		mat_t	mat;
		point_t	pt1, pt2;
		vect_t	dir;
		fastf_t	ang;

		if( nwords < 1+3+3+1 )  return(-1);
		/* Expects point1, point2, angle */
		VSET( pt1, atof(words[1]), atof(words[2]), atof(words[3]) );
		VSET( pt2, atof(words[4]), atof(words[5]), atof(words[6]) );
		ang = atof(words[7]) * bn_degtorad;
		VSUB2( dir, pt2, pt2 );
		VUNITIZE(dir);
		MAT_IDN( mat );
		bn_mat_arb_rot( mat, pt1, dir, ang );
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "arb_rot_dir" ) == 0 )  {
		mat_t	mat;
		point_t	pt1;
		vect_t	dir;
		fastf_t	ang;

		if( nwords < 1+3+3+1 )  return(-1);
		/* Expects point1, dir, angle */
		VSET( pt1, atof(words[1]), atof(words[2]), atof(words[3]) );
		VSET( dir, atof(words[4]), atof(words[5]), atof(words[6]) );
		ang = atof(words[7]) * bn_degtorad;
		VUNITIZE(dir);
		MAT_IDN( mat );
		bn_mat_arb_rot( mat, pt1, dir, ang );
		out_mat( mat, stdout );
		return(0);
	}
	if( strcmp( words[0], "quat" ) == 0 )  {
		mat_t	mat;
		quat_t	quat;

		/* Usage: quat x,y,z,w */
		if( nwords < 5 ) return -1;
		QSET( quat, atof(words[1]), atof(words[2]),
			atof(words[3]), atof(words[4]) );

		quat_quat2mat( mat, quat );
		out_mat( mat, stdout);
		return 0;
	}
	if( strcmp( words[0], "fromto" ) == 0 )  {
		mat_t	mat;
		point_t	cur;
		point_t	next;
		vect_t	from;
		vect_t	to;

		/* Usage: fromto +Z cur_xyz next_xyz */
		if( nwords < 8 )  return -1;
		if( strcmp( words[1], "+X" ) == 0 )  {
			VSET( from, 1, 0, 0 );
		} else if( strcmp( words[1], "-X" ) == 0 )  {
			VSET( from, -1, 0, 0 );
		} else if( strcmp( words[1], "+Y" ) == 0 )  {
			VSET( from, 0, 1, 0 );
		} else if( strcmp( words[1], "-Y" ) == 0 )  {
			VSET( from, 0, -1, 0 );
		} else if( strcmp( words[1], "+Z" ) == 0 )  {
			VSET( from, 0, 0, 1 );
		} else if( strcmp( words[1], "-Z" ) == 0 )  {
			VSET( from, 0, 0, -1 );
		} else {
			fprintf(stderr,"fromto '%s' is not +/-XYZ\n", words[1]);
			return -1;
		}
		VSET( cur, atof(words[2]), atof(words[3]), atof(words[4]) );
		VSET( next, atof(words[5]), atof(words[6]), atof(words[7]) );
		VSUB2( to, next, cur );
		VUNITIZE(to);
		bn_mat_fromto( mat, from, to );
		/* Check to see if it worked. */
		{
			vect_t	got;

			MAT4X3VEC( got, mat, from );
			if( VDOT( got, to ) < 0.9 )  {
				bu_log("\ntabsub ERROR: At t=%s, bn_mat_fromto failed!\n", chanwords[0] );
				VPRINT("\tfrom", from);
				VPRINT("\tto", to);
				VPRINT("\tgot", got);
			}
		}
		out_mat( mat, stdout );
		return 0;
	}
	return(-2);		/* Unknown keyword */
}

/*
 *			S T R 2 C H A N _ I N D E X
 *
 *  Convert an ascii string to a channel index.
 *  Specifying channel 0 selects column (and thus subscript) 1,
 *  because column 0 contains the current time.
 *  Thus, valid channel values are 0 through nwords-2,
 *  resulting in column numbers 1 through nwords-1.
 *
 *  To signal an error, 0 is returned;  this will index the time column.
 */
int
str2chan_index( char *s )
{
	int	chan;

	chan = atoi( s );
	if( chan < 0 || chan > nwords-2 )  {
		fprintf(stderr,"Line %d:  chan %d out of range 0..%d\n", line, chan, nwords-2 );
		return(0);		/* Flag [0]:  time channel */
	}
	return(chan+1);
}

void
out_mat(matp_t m, FILE *fp)
{
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[0], m[1], m[2], m[3] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[4], m[5], m[6], m[7] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[8], m[9], m[10], m[11] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e", m[12], m[13], m[14], m[15] );
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
