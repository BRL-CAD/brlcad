/*
 *			T A B S U B . C
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
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#ifdef unix
# include <sys/types.h>
# include <sys/stat.h>
#endif


char	*prototype;		/* Contains full text of prototype document */


/*
 *			M A I N
 *
 */
main( argc, argv )
int	argc;
char	**argv;
{
	FILE	*table;

	if( argc < 1 || argc > 3 )  {
		fprintf(stderr,"Usage:  tabsub prototype_file [table_file]\n");
		exit(12);
	}

	/* Acquire in-core copy of prototype file */
	get_proto( argv[1] );

	if( argc < 3 )  {
		table = stdin;
	} else {
		if( (table = fopen( argv[2], "r" )) == NULL )  {
			perror( argv[2] );
			exit(3);
		}
	}
	do_lines( table );
}

get_proto( file )
char	*file;
{
	struct stat	sb;
	int	fd;
	char	*proto;
	register char	*ip, *op;

	if( (fd = open( file, 0 )) < 0 || stat( file, &sb ) < 0 )  {
		perror(file);
		exit(1);
	}
	if( sb.st_size == 0 )  {
		fprintf(stderr,"tabsub:  %s is empty\n", file );
		exit(1);
	}
	prototype = rt_malloc( sb.st_size, "prototype document");
	proto = rt_malloc( sb.st_size, "temporary prototype document");
	if( read( fd, proto, sb.st_size ) != sb.st_size )  {
		perror(file);
		exit(2);
	}

	/* Eliminate comments from input */
	ip = proto;
	op = prototype;
	while( *ip )  {
		if( *ip != '#' )  {
			*op++ = *ip++;
			continue;
		}
		/* Start of comment seen, gobble until newline or EOF */
		ip++;
		while( *ip && *ip != '\n' )
			ip++;
	}
	rt_free( proto, "temporary prototype document" );
}

#define	NCHANS	1024
char	linebuf[NCHANS*10];
int	line;				/* input line number */

char	*chanwords[NCHANS+1];
int	nwords;				/* # words in chanwords[] */

#define NTOKENWORDS	16
char	*tokenwords[NTOKENWORDS+1];

do_lines( fp )
FILE	*fp;
{
	char	token[128];
	int	ntokenwords;
	register char	*cp;
	register char	*tp;
	int	chan;

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

		/* Here, there is no way to check for too many words */
		nwords = rt_split_cmd( chanwords, NCHANS+1, linebuf );

		for( cp=prototype; *cp != '\0'; )  {
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
				while( *cp && *cp != ')' )
					*tp++ = *cp++;
				*tp++ = '\0';
				cp++;		/* skip ')' */
			} else if( isdigit( *cp ) )  {
				tp = token;
				while( isdigit( *cp ) )
					*tp++ = *cp++;
				*tp++ = '\0';
			} else {
				fprintf( stderr,"Line %d:  Bad sequence '@%c'\n", line, *cp);
				fprintf( stdout, "@%c", *cp++ );
				continue;
			}

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
			ntokenwords = rt_split_cmd( tokenwords, NTOKENWORDS+1, token );

			if( strcmp( tokenwords[0], "rot" ) == 0 )  {
				mat_t	mat;

				mat_idn( mat );
				mat_angles( mat, 
				    atof( chanwords[str2chan_index(tokenwords[1])] ),
				    atof( chanwords[str2chan_index(tokenwords[2])] ),
				    atof( chanwords[str2chan_index(tokenwords[3])] ) );
				out_mat( mat, stdout );
				continue;
			}
			if( strcmp( tokenwords[0], "xlate" ) == 0 )  {
				mat_t	mat;

				mat_idn( mat );
				MAT_DELTAS( mat, 
				    atof( chanwords[str2chan_index(tokenwords[1])] ),
				    atof( chanwords[str2chan_index(tokenwords[2])] ),
				    atof( chanwords[str2chan_index(tokenwords[3])] ) );
				out_mat( mat, stdout );
				continue;
			}

			fprintf(stderr,"Line %d: keyword @(%s) unknown\n", line, token);
			fprintf(stdout, "@(%s)", token );
		}
	}
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
str2chan_index( s )
char	*s;
{
	int	chan;

	chan = atoi( s );
	if( chan < 0 || chan > nwords-2 )  {
		fprintf(stderr,"Line %d:  chan %d out of range 0..%d\n", line, chan, nwords-2 );
		return(0);		/* Flag [0]:  time channel */
	}
	return(chan+1);
}

out_mat( m, fp )
matp_t	m;
FILE	*fp;
{
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[0], m[1], m[2], m[3] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[4], m[5], m[6], m[7] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e\n", m[8], m[9], m[10], m[11] );
	fprintf( fp, "\t%.9e %.9e %.9e %.9e", m[12], m[13], m[14], m[15] );
}
