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

	if( (fd = open( file, 0 )) < 0 || stat( file, &sb ) < 0 )  {
		perror(file);
		exit(1);
	}
	if( sb.st_size == 0 )  {
		fprintf(stderr,"tabsub:  %s is empty\n", file );
		exit(1);
	}
	prototype = rt_malloc( sb.st_size, "prototype document");
	if( read( fd, prototype, sb.st_size ) != sb.st_size )  {
		perror(file);
		exit(2);
	}
}

#define	NCHANS	1024
char	linebuf[NCHANS*10];
char	**chanwords[NCHANS+1];

do_lines( fp )
FILE	*fp;
{
	int	line;
	char	token[128];
	register char	*cp;
	register char	*tp;
	int	nwords;
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

		/* Here, there is not way to check for too many words */
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
				chan = atoi(token);
				if( chan < 0 || chan > nwords-2 )  {
					fprintf(stderr,"Line %d:  chan %d out of range 0..%d\n", line, chan, nwords-2 );
					fprintf(stdout,"@(%d)", chan);
					continue;
				}
				/* [0] has the time */
				fputs( chanwords[chan+1], stdout );
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
			fprintf(stderr,"Line %d: keyword @(%s) unknown\n", line, token);
			fprintf(stdout, "@(%s)", token );
		}
	}
}
