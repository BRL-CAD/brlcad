/*                         V P R O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file vproc.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Procedures for vproc.c

	Section 1:  Commands
		3:  List Processing Routines
		4:  String Processing Routines
		5:  Input/Output Routines
		6:  Interrupt Handlers

 */
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <setjmp.h>

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./vextern.h"

#include "./std.h"

extern void	ewrite();


char		getarg();
void		quit(), abort_sig();
void		blank_fill();

/*
	Section 1:	C O M M A N D S

			deck()
			shell()
 */

/*	d e c k ( )
	make a COMGEOM deck for current list of objects

 */
deck( prefix )
register char *prefix;
{
	register int	i;

	nns = nnr = 0;

	/* Create file for solid table.					*/
	if( prefix != 0 )
	{
		(void) strncpy( st_file, prefix, 73 );
		(void) strcat( st_file, ".st" );
	}
	else
		(void) strncpy( st_file, "solids", 7 );
	if( (solfd = creat( st_file, 0644 )) < 0 )
	{
		perror( st_file );
		exit( 10 );
	}


	/* Target units (a2,3x)						*/
	ewrite( solfd, rt_units_string(dbip->dbi_local2base), 2 );
	blank_fill( solfd, 3 );

	/* Title							*/
	if( dbip->dbi_title == NULL )
		ewrite( solfd, objfile, (unsigned) strlen( objfile ) );
	else
		ewrite( solfd, dbip->dbi_title, (unsigned) strlen( dbip->dbi_title ) );
	ewrite( solfd, LF, 1 );

	/* Save space for number of solids and regions.			*/
	savsol = lseek( solfd, 0L, 1 );
	blank_fill( solfd, 10 );
	ewrite( solfd, LF, 1 );

	/* Create file for region table.				*/
	if( prefix != 0 )
	{
		(void) strncpy( rt_file, prefix, 73 );
		(void) strcat( rt_file, ".rt" );
	}
	else
		(void) strncpy( rt_file, "regions", 8 );
	if( (regfd = creat( rt_file, 0644 )) < 0 )
	{
		perror( rt_file );
		exit( 10 );
	}

	/* create file for region ident table
	 */
	if( prefix != 0 )
	{
		(void) strncpy( id_file, prefix, 73 );
		(void) strcat( id_file, ".id" );
	}
	else
		(void) strncpy( id_file, "region_ids", 11 );
	if( (ridfd = creat( id_file, 0644 )) < 0 )
	{
		perror( id_file );
		exit( 10 );
	}
	itoa( -1, buff, 5 );
	ewrite( ridfd, buff, 5 );
	ewrite( ridfd, LF, 1 );

	/* Initialize matrices.						*/
	MAT_IDN( identity );

	/* Check integrity of list against directory and build card deck.	*/
	for( i = 0; i < curr_ct; i++ )
	{
		struct directory	*dirp;
		if( (dirp = db_lookup( dbip, curr_list[i], LOOKUP_NOISY )) != DIR_NULL )  {
#if 1
			treewalk( curr_list[i] );
#else
			cgobj( dirp, 0, identity );
#endif
		}
	}
	/* Add number of solids and regions on second card.		*/
	(void) lseek( solfd, savsol, 0 );
	itoa( nns, buff, 5 );
	ewrite( solfd, buff, 5 );
	itoa( nnr, buff, 5 );
	ewrite( solfd, buff, 5 );

	/* Finish region id table.					*/
	ewrite( ridfd, LF, 1 );

	(void) printf( "====================================================\n" );
	(void) printf( "O U T P U T    F I L E S :\n\n" );
	(void) printf( "solid table = \"%s\"\n", st_file );
	(void) printf( "region table = \"%s\"\n", rt_file );
	(void) printf( "region identification table = \"%s\"\n", id_file );
	(void) close( solfd );
	(void) close( regfd );
	(void) close( ridfd );

	/* reset starting numbers for solids and regions
	 */
	delsol = delreg = 0;
	/* XXX should free soltab list */
}

/*	s h e l l ( )
	Execute shell command.
 */
int
shell( args )
char  *args[];
{
	register char	*from, *to;
	char		*argv[4], cmdbuf[MAXLN];
	int		pid, ret, status;
	register int	i;

	(void) signal( SIGINT, SIG_IGN );

	/* Build arg vector.						*/
	argv[0] = "Shell( deck )";
	argv[1] = "-c";
	to = argv[2] = cmdbuf;
	for( i = 1; i < arg_ct; i++ ) {
		from = args[i];
		if( (to + strlen( args[i] )) - argv[2] > MAXLN - 1 ) {
			(void) fprintf( stderr, "\ncommand line too long\n" );
			exit( 10 );
		}
		(void) printf( "%s ", args[i] );
		while( *from )
			*to++ = *from++;
		*to++ = ' ';
	}
	to[-1] = '\0';
	(void) printf( "\n" );
	argv[3] = 0;
	if( (pid = fork()) == -1 ) {
		perror( "shell()" );
		return( -1 );
	} else	if( pid == 0 ) { /*
				  * CHILD process - execs a shell command
					  */
		(void) signal( SIGINT, SIG_DFL );
		(void) execv( "/bin/sh", argv );
		perror( "/bin/sh -c" );
		exit( 99 );
	} else	/*
		 * PARENT process - waits for shell command
		 * to finish.
			 */
		do {
			if( (ret = wait( &status )) == -1 ) {
				perror( "wait( /bin/sh -c )" );
				break;
			}
		} while( ret != pid );
	return( 0 );
}

/*	t o c ( )
	Build a sorted list of names of all the objects accessable
	in the object file.
 */
void
toc()
{
	register struct directory *dp;
	register int		i;

	(void) printf( "Making the Table of Contents.\n" );
	(void) fflush( stdout );

	FOR_ALL_DIRECTORY_START(dp, dbip) {
	    toc_list[ndir++] = dp->d_namep;
	} FOR_ALL_DIRECTORY_END;
}

/*
	Section 3:	L I S T   P R O C E S S I N G   R O U T I N E S

			list_toc()
			col_prt()
			insert()
			delete()
*/

/*	l i s t _ t o c ( )
	List the table of contents.
 */
void
list_toc( args )
char	 *args[];
{
	register int	i, j;
	(void) fflush( stdout );
	for( tmp_ct = 0, i = 1; args[i] != NULL; i++ )
	{
		for( j = 0; j < ndir; j++ )
		{
			if( match( args[i], toc_list[j] ) )
				tmp_list[tmp_ct++] = toc_list[j];
		}
	}
	if( i > 1 )
		(void) col_prt( tmp_list, tmp_ct );
	else
		(void) col_prt( toc_list, ndir );
	return;
}

#define NAMESIZE	16
#define MAX_COL	(NAMESIZE*5)
#define SEND_LN()	{\
			buf[column++] = '\n';\
			ewrite( 1, buf, (unsigned) column );\
			column = 0;\
			}

/*	c o l _ p r t ( )
	Print list of names in tabular columns.
 */
col_prt( list, ct )
register char	*list[];
register int	ct;
{
	char		buf[MAX_COL+2];
	register int	i, column, spaces;

	for( i = 0, column = 0; i < ct; i++ )
	{
		if( column + strlen( list[i] ) > MAX_COL )
		{
			SEND_LN();
			i--;
		}
		else
		{
			(void) strcpy( &buf[column], list[i] );
			column += strlen( list[i] );
			spaces = NAMESIZE - (column % NAMESIZE );
			if( column + spaces < MAX_COL )
				for( ; spaces > 0; spaces-- )
					buf[column++] = ' ';
			else
				SEND_LN();
		}
	}
	SEND_LN();
	return	ct;
}

/*	i n s e r t ( )
	Insert each member of the table of contents 'toc_list' which
	matches one of the arguments into the current list 'curr_list'.
 */
int
insert(  args,	ct )
char		*args[];
register int	ct;
{
	register int	i, j, nomatch;

	/* For each argument (does not include args[0]).			*/
	for( i = 1; i < ct; i++ )
	{ /* If object is in table of contents, insert in current list.	*/
		nomatch = YES;
		for( j = 0; j < ndir; j++ )
		{
			if( match( args[i], toc_list[j] ) )
			{
				nomatch = NO;
				/* Allocate storage for string.			*/
				curr_list[curr_ct++] = rt_strdup(toc_list[j]);
			}
		}
		if( nomatch )
			(void) fprintf( stderr,
			    "Object \"%s\" not found.\n", args[i] );
	}
	return	curr_ct;
}

/*	d e l e t e ( )
	delete all members of current list 'curr_list' which match
	one of the arguments
 */
int
delete(  args )
char	*args[];
{
	register int	i;
	register int	nomatch;

	/* for each object in arg list
	 */
	for( i = 1; i < arg_ct; i++ ) {
		register int	j;
		nomatch = YES;

		/* traverse list to find string
		 */
		for( j = 0; j < curr_ct; )
			if( match( args[i], curr_list[j] ) )
			{
				register int	k;

				nomatch = NO;
				rt_free( curr_list[j], "curr_list" );
				--curr_ct;
				/* starting from bottom of list,
				 * pull all entries up to fill up space
				 made by deletion
				 */
				for( k = j; k < curr_ct; k++ )
					curr_list[k] = curr_list[k+1];
			}
			else	++j;
		if( nomatch )
			(void) fprintf( stderr,
			    "Object \"%s\" not found.\n",
			    args[i]
			    );
	}
	return( curr_ct );
}

/*
	Section 4:	S T R I N G   P R O C E S S I N G   R O U T I N E S

			itoa()
			check()
 */

/*	i t o a ( )
	Convert integer to ascii  wd format.
 */
itoa( n, s, w )
register
char	*s;
register
int   n,    w;
{
	int	 c, i, j, sign;

	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do
		s[i++] = n % 10 + '0';
	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* Blank fill array.					*/
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w ) {
		s[w-1] = (s[w]-1-'0')*10 + (s[w-1]-'0')  + 'A';
	}
	s[w] = '\0';

	/* Reverse the array.					*/
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

vls_blanks( v, n )
struct rt_vls	*v;
int		n;
{
	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "                                                                                                                                ",
	    n);
}

/*
 *			V L S _ I T O A
 *
 *	Convert integer to ascii  wd format.
 */
vls_itoa( v, n, w )
struct rt_vls	*v;
register int	n;
register int	w;
{
	int	 c, i, j, sign;
	register char	*s;

	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", w);
	s = rt_vls_addr(v)+rt_vls_strlen(v)-w;

	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do
		s[i++] = n % 10 + '0';
	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* Blank fill array.					*/
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w ) {
		s[w-1] = (s[w]-1-'0')*10 + (s[w-1]-'0')  + 'A';
	}
	s[w] = '\0';

	/* Reverse the array.					*/
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

vls_ftoa_vec_cvt( v, vec, w, d )
struct rt_vls	*v;
vect_t		vec;
int		w;
int		d;
{
	vls_ftoa( v, vec[X]*dbip->dbi_base2local, w, d );
	vls_ftoa( v, vec[Y]*dbip->dbi_base2local, w, d );
	vls_ftoa( v, vec[Z]*dbip->dbi_base2local, w, d );
}

vls_ftoa_vec( v, vec, w, d )
struct rt_vls	*v;
vect_t		vec;
int		w;
int		d;
{
	vls_ftoa( v, vec[X], w, d );
	vls_ftoa( v, vec[Y], w, d );
	vls_ftoa( v, vec[Z], w, d );
}

vls_ftoa_cvt( v, f, w, d )
struct rt_vls	*v;
register double	f;
register int	w, d;
{
	vls_ftoa( v, f*dbip->dbi_base2local, w, d );
}

/*
 *			V L S _ F T O A
 *
 *	Convert float to ascii  w.df format.
 */
vls_ftoa( v, f, w, d )
struct rt_vls	*v;
register double	f;
register int	w, d;
{
	register char	*s;
	register int	c, i, j;
	long	n, sign;

	RT_VLS_CHECK(v);
	rt_vls_strncat( v, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", w);
	s = rt_vls_addr(v)+rt_vls_strlen(v)-w;

	if( w <= d + 2 )
	{
		(void) fprintf( stderr,
		    "ftoascii: incorrect format  need w.df  stop"
		    );
		exit( 10 );
	}
	for( i = 1; i <= d; i++ )
		f = f * 10.0;

	/* round up */
	if( f < 0.0 )
		f -= 0.5;
	else
		f += 0.5;
	n = f;
	if( (sign = n) < 0 )
		n = -n;
	i = 0;
	do	{
		s[i++] = n % 10 + '0';
		if( i == d )
			s[i++] = '.';
	}	while( (n /= 10) > 0 );

	/* Zero fill the d field if necessary.				*/
	if( i < d )
	{
		for( j = i; j < d; j++ )
			s[j] = '0';
		s[j++] = '.';
		i = j;
	}
	if( sign < 0 )
		s[i++] = '-';

	/* Blank fill rest of field.					*/
	for ( j = i; j < w; j++ )
		s[j] = ' ';
	if( i > w )
		(void) fprintf( stderr, "Ftoascii: field length too small\n" );
	s[w] = '\0';

	/* Reverse the array.						*/
	for( i = 0, j = w - 1; i < j; i++, j-- )
	{
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*
	Section 5:	I / O   R O U T I N E S
 *
			getcmd()
			getarg()
			menu()
			blank_fill()
			bug()
			fbug()
 */

/*	g e t c m d ( )
	Return first character read from keyboard,
	copy command into args[0] and arguments into args[1]...args[n].

 */
char
getcmd( args, ct )
char		*args[];
register int	ct;
{
	/* Get arguments.						 */
	if( ct == 0 )
		while( --arg_ct >= 0 )
			rt_free( args[arg_ct], "args[arg_ct]" );
	for( arg_ct = ct; arg_ct < MAXARG - 1; ++arg_ct )
	{
		args[arg_ct] = rt_malloc( MAXLN, "getcmd buffer" );
		if( ! getarg( args[arg_ct] ) )
			break;
	}
	++arg_ct;
	args[arg_ct] = 0;

	/* Before returning to command interpreter,
	 * set up interrupt handler for commands...
	 * trap interrupts such that command is aborted cleanly and
	 * command line is restored rather than terminating program
	 */
	(void) signal( SIGINT, abort_sig );
	return	(args[0])[0];
}

/*	g e t a r g ( )
	Get a word of input into 'str',
	Return 0 if newline is encountered.
 	Return 1 otherwise.
 */
char
getarg( str )
register char	*str;
{
	do
	{
		*str = getchar();
		if( (int)(*str) == ' ' )
		{
			*str = '\0';
			return( 1 );
		}
		else
			++str;
	}	while( (int)(str[-1]) != EOF && (int)(str[-1]) != '\n' );
	if( (int)(str[-1]) == '\n' )
		--str;
	*str = '\0';
	return	0;
}

/*	m e n u ( )
	Display menu stored at address 'addr'.
 */
void
menu( addr )
char **addr;
{
	register char	**sbuf = addr;
	while( *sbuf )
		(void) printf( "%s\n", *sbuf++ );
	(void) fflush( stdout );
	return;
}

/*	b l a n k _ f i l l ( )
	Write count blanks to fildes.
 */
void
blank_fill( fildes, count )
register int	fildes,	count;
{
	register char	*blank_buf = BLANKS;
	ewrite( fildes, blank_buf, (unsigned) count );
	return;
}

/*
	Section 6:	I N T E R R U P T   H A N D L E R S
 *
			abort_sig()
			quit()
 */

/*	a b o r t ( )
	Abort command without terminating run (restore command prompt) and
	cleanup temporary files.
 */
/*ARGSUSED*/
void
abort_sig( sig )
{
	(void) signal( SIGINT, quit );	/* reset trap */

	/* goto command interpreter with environment restored.		*/
	longjmp( env, sig );
}

/*	q u i t ( )
	Terminate run.
 */
/*ARGSUSED*/
void
quit( sig )
{
	(void) fprintf( stdout, "quitting...\n" );
	exit( 0 );
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
