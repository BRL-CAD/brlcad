/*
 *	@(#) vproc.c			retrieved: 8/13/86 at 08:23:30,
 *	@(#) version 1.5		last edit: 10/11/83 at 09:31:36.
 *
 *	Written by Gary S. Moss.
 *	All rights reserved, Ballistic Research Laboratory.
 *
 *	Procedures for vproc.c
 *
 *	Section 1:  Commands
 *		2:  Object Directory Routines
 *		3:  List Processing Routines
 *		4:  String Processing Routines
 *		5:  Input/Output Routines
 *		6:  Interrupt Handlers
 *	  
 */
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include "./ged_types.h"
#include "./3d.h"
#include "./vdeck.h"
#include "./vextern.h"
char		*addname();
Directory	*lookup(), *diradd();

/*
 *	Section 1:	C O M M A N D S
 *
 *			deck()
 *			shell()
 */

/*	==== d e c k ( )
 *	make a COMGEOM deck for current list of objects
 *
 */
deck( prefix )
register
char *prefix;
{
	register int	i, j;
	
	nns = nnr = regflag = numrr = 0;
	
	/* rewind object file
	 */
	lseek( objfd, 0L, 0 );

	/* create file for solid table
	 */
	if( prefix != 0 ) {
		strncpy( st_file, prefix, 73 );
		strcat( st_file, ".st" );
	} else	strncpy( st_file, "solids", 7 );
	if( (solfd = creat( st_file, 0666 )) < 0 ) {
		perror( st_file );
		exit( 10 );
	}

	/* title
	 */
	write( solfd, objfile, strlen( objfile ) );
	write( solfd, LF, 1 );

	/* save space for number of solids and regions
	 */
	savsol = lseek( solfd, 0L, 1 );
	blank_fill( solfd, 30 );
	write( solfd, LF, 1 );

	/* create file for region table
	 */
	if( prefix != 0 ) {
		strncpy( rt_file, prefix, 73 );
		strcat( rt_file, ".rt" );
	} else	strncpy( rt_file, "regions", 8 );
	if( (regfd = creat( rt_file, 0666 )) < 0 ) {
		perror( rt_file );
		exit( 10 );
	}

	/* create file for region ident table
	 */
	if( prefix != 0 ) {
		strncpy( id_file, prefix, 73 );
		strcat( id_file, ".id" );
	} else	strncpy( id_file, "region_ids", 11 );
	if( (ridfd = creat( id_file, 0666 )) < 0 ) {
		perror( id_file );
		exit( 10 );
	}
	itoa( -1, buff, 5 );
	write( ridfd, buff, 5 );
	write( ridfd, LF, 1 );
	itoa( 0, buff, 10 );
	write( ridfd, buff, 10 );
	write( ridfd, LF, 1 );

	/* create /tmp file for discrimination of files
	 */
	strncpy( disc_file, mktemp( "/tmp/disXXXXXX" ), 15 );
	if( (idfd = creat( disc_file, 0666 )) < 0 ) {
		perror( disc_file );
		exit( 10 );
	}
	rd_idfd = open( disc_file, 2 );

	/* create /tmp file for storage of region names in the comgeom desc
	 */
	strncpy( reg_file, mktemp( "/tmp/regXXXXXX" ), 15 );
	if( (rrfd = creat( reg_file, 0666 )) < 0 ) {
		perror( reg_file );
		exit( 10 );
	}
	rd_rrfd = open( reg_file, 2 );

	/* initialize matrices
	 */
	mat_idn( identity );
	mat_idn( xform );

	/* Check integrity of list against directory and build
	 * card deck.
	 */
	for( i = 0; i < curr_ct; i++ )
		if( (dp = lookup( curr_list[i], NOISY )) != DIR_NULL )
			cgobj( dp, 0, identity );

	/* add number of solids and regions on second card
	 */
	lseek( solfd, savsol, 0 );
	itoa( nns, buff, 20 );
	write( solfd, buff, 20 );
	itoa( nnr, buff, 10 );
	write( solfd, buff, 10 );

	/* finish region id table
	 */
	write( ridfd, LF, 1 );

 	/* must go back and add regions as members of regions
	 */
	if( numrr > 0 ) {
		for( i = 1; i <= numrr; i++ ) {
			lseek( rd_rrfd, 0L, 0 );   /* rewind */
			for( j = 1; j <= nnr; j++ ) {
				/* next region name in desc
				 */
				readF( rd_rrfd, name, 16 );
				if(	strcmp( findrr[i].rr_name, name )
					== 0)
				{	/* region number in desc is j
					 * add to regfd at rrpos[i]
					 */
					lseek( regfd, findrr[i].rr_pos, 0);
					itoa( j+delreg, buff, 4 );
					write( regfd, buff, 4 );
					break;
				}
			}
			if( j > nnr ) {
				fprintf( stderr,
					"Region %s is member of a region ",
					findrr[i].rr_name );
				fprintf( stderr,
					"but not in description.\n" );
				exit( 10 );
 			}
		}
	}
	prompt( "\n====================================================" );
	prompt( "\nO U T P U T    F I L E S :\n\n" );
	prompt( "solid table = \"%s\"\n", st_file );
	prompt( "region table = \"%s\"\n", rt_file );
	prompt( "region identification table = \"%s\"\n", id_file );
	close( solfd );
	close( regfd );
	close( ridfd );
	unlink( disc_file );
	close( idfd );
	close( rd_idfd );
	unlink( reg_file );
	close( rrfd );
	close( rd_rrfd );

	/* reset starting numbers for solids and regions
	 */
	delsol = delreg = 0;
}

/*	==== s h e l l ( )
 *	Execute shell command.
 */
shell( args )
char  *args[];
{
	register char	*from, *to;
	char		*argv[4], cmdbuf[MAXLN];
	int		pid, ret, status;
	register int	i;

	signal( SIGINT, SIG_IGN );

	/* build arg vector
	 */
	argv[0] = "Shell( deck )";
	argv[1] = "-c";
	to = argv[2] = cmdbuf;
	for( i = 1; i < arg_ct; i++ ) {
		from = args[i];
		if( (to + strlen( args[i] )) - argv[2] > MAXLN - 1 ) {
			fprintf( stderr, "\ncommand line too long\n" );
			exit( 10 );
		}
		printf( "%s ", args[i] );
		while( *from )	*to++ = *from++;
		*to++ = ' ';
	}
	to[-1] = '\0';
	prompt( "\n" );
	argv[3] = 0;
	if( (pid = fork()) == -1 ) {
		perror( "shell()" );
		return( -1 );
	} else	if( pid == 0 ) { /*
				  * CHILD process - execs a shell command
				  */
		signal( SIGINT, SIG_DFL );
		execv(	"/bin/sh", argv );
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

/*
 *	Section 2:	O B J E C T   D I R E C T O R Y   R O U T I N E S
 *
 *			builddir()
 *			toc()
 *			diradd()
 *			addname()
 */

/*
 *			B U I L D D I R
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 */
builddir()
{ register Directory *dp;
	dp = &directory[0];
	while( 1 ) {
		dp->d_addr = lseek( objfd, 0L, 1 );
		if(	readF( objfd, &record, sizeof record )
			!= sizeof record
		)	break;
		if( ++ndir >= NDIR )  {
			fprintf( stderr, "Too many objects in input\n" );
			break;
		}
		switch( record.u_id ) {
		case SOLID:
			/* Check for a deleted record
			 */
			if( record.s.s_name[0] == 0 )  {
				ndir--;
				continue;
			}
			dp->d_namep = addname( record.s.s_name );
			break;
		case ARS_A:
			/* Check for a deleted record
			 */
			if( record.s.s_name[0] == 0 )  {
				ndir--;
				continue;
			}
			dp->d_namep = addname( record.s.s_name );

			/*  skip remaining B type recods
			 */
			lseek(	objfd,
				((long)record.a.a_totlen)
				* sizeof record,
				1 );
			break;
		case COMB:
			/* Check for a deleted record
			 */
			if( record.c.c_name[0] == 0 )  {
				ndir--;
				dp--;
			}  else	dp->d_namep = addname( record.c.c_name );

			/* Skip over remaining records
			 */
			lseek(	objfd,
				((long)record.c.c_length)
				* sizeof record,
				1 );
			break;
		default:
			fprintf( stderr,
				"Builddir:  unknown record %c (0%o).\n",
				record.u_id,
				record.u_id );
			ndir--;
			break;
		}
		dp++;
	}
	prompt( "\n%d objects tallied\n", ndir );
}

/*	==== t o c ( )
 *	Build a sorted list of names of all the objects accessable
 *	in the object file.
 */
toc() {
	static Directory	*dp, *ep;
	register int		i;

	dp = &directory[0];
	ep = &directory[ndir];

	for( i = 0; dp < ep; )	toc_list[i++] = (dp++)->d_namep;
	toc_ct = i;
}

/*	==== l o o k u p ( )
 *	This routine takes a name, and looks it up in the
 *	directory table.  If the name is present, a pointer to
 *	the directory struct element is returned, otherwise
 *	a -1 is returned.
 *
 * If the flag is NOISY, a print occurs, else only
 * the return code indicates failure.
 */
Directory *
lookup( str, flag )
register char *str;
{
	register Directory *dp;
	register char *np;
	static Directory *ep;
	static int	i;

	ep = &directory[ndir];
	for( dp = &directory[0]; dp < ep; dp++ )  {
		np = dp->d_namep;
		for( i = 0; i < 16; i++ ) {
			if( str[i] != *np )	break;
			if( *np++ == 0 || i == 15 )	return	dp;
		}
	}
	if( flag == NOISY )
		fprintf( stderr, "Lookup: could not find '%s'.\n", str );
	return	DIR_NULL;
}

/*	==== d i r a d d ( )
 *	Add an entry to the directory.
 */
Directory *
diradd( name, laddr )
register char *name;
long laddr;
{
	register Directory *dp;

	if( ndir >= NDIR )  {
		fprintf( stderr, "Diradd:  no more dir structs.\n");
		return	DIR_NULL;
	}

	dp = &directory[ndir++];
	dp->d_namep = addname( name );
	dp->d_addr = laddr;
	return( dp );
}

/*	==== a d d n a m e ( )
 *	Given a name, it puts the name in the name buffer, and
 *	returns a pointer to that string.
 */
char *
addname( cp )
register
char	*cp;
{
	static char	*holder;
	register int	i;

	if( dir_last >= &dir_names[NDIR*10-16] )  {
		fprintf( stderr, "Addname:  out of name space.\n" );
		exit( 1 );
	}
	holder = dir_last;
	i = 0;
	while( *cp != 0 && i++ < 16 )	*dir_last++ = *cp++;
	*dir_last++ = 0;
	return	holder;
}

/*
 *	Section 3:	L I S T   P R O C E S S I N G   R O U T I N E S
 *
 *			list_toc()
 *			col_prt()
 *			insert()
 *			delete()
 */

/*	==== l i s t _ t o c ( )
 *	List the table of contents.
 */
list_toc( args )
char	 *args[];
{
	register int	i, j;

	for( tmp_ct = 0, i = 1; args[i] != 0; i++ )
		for( j = 0; j < toc_ct; j++ )
			if( match( args[i], toc_list[j] ) )
				tmp_list[tmp_ct++] = toc_list[j];
	if( i > 1 )	col_prt( tmp_list, tmp_ct );
	else		col_prt( toc_list, toc_ct );
}

/*	==== c o l _ p r t ( )
 *	Print list of names in tabular columns.
 */
col_prt( list, ct )
char	*list[];
register
int	ct;
{
	char		buf[72];
	register char	*lbuf = buf;
	register int	i, column = 0;

	for( i = 0; i < ct; i++ ) {
		strcpy( lbuf, list[i] );
		column += strlen( list[i] );
		lbuf += strlen( list[i] );
		if( column > 56 ) {
			*lbuf++ = '\n';
			write( 1, buf, lbuf-buf );
			lbuf = buf;
			column = 0;
		} else	{
			*lbuf++ = '\t';
			column += 8 - (column % 8 );
		}
	}
	*lbuf++ = '\n';
	write( 1, buf, lbuf-buf );
	lbuf = buf;
	return( ct );
}

/*	==== i n s e r t ( )
 *	Insert each member of the table of contents 'toc_list' which
 *	matches one of the arguments into the current list 'curr_list'.
 */
insert(  args,	ct )
char	*args[];
register int	ct;
{
	char		*malloc();
	register int	i, j, nomatch;
	unsigned	bytect;
	
	/* for each argument (does not include args[0])
	 */
	for( i = 1; i < ct; i++ ) {
		/* if object is in table of contents,
		 * insert in current list
		 */
		nomatch = YES;
		for( j = 0; j < toc_ct; j++ ) {
			if( match( args[i], toc_list[j] ) ) {
				nomatch = NO;

				/* allocate storage for string
				 */
				bytect = strlen( toc_list[j] );
				if(	(curr_list[curr_ct] =
					malloc( ++bytect )) == 0 )
				{
					perror( "malloc" );
					exit( 1 );
				}

				/* insert string at end of list
				 */
				strcpy(	curr_list[curr_ct++],
					toc_list[j] );
			}
		}
		if( nomatch )
			fprintf( stderr,
				"Object \"%s\" not found.\n", args[i] );
	}
	return( curr_ct );
}

/*	==== d e l e t e ( )
 *	delete all members of current list 'curr_list' which match
 *	one of the arguments
 */
delete(  args )
char	*args[];
{
	register int	i;
	register int	nomatch;
	
	/* for each object in arg list
	 */
	for( i = 1; i < arg_ct; i++ ) { register int	j;
		nomatch = YES;

		/* traverse list to find string
		 */
		for( j = 0; j < curr_ct; )
			if( match( args[i], curr_list[j] ) )
			{	register int	k;			

				nomatch = NO;
				free( curr_list[j] );	--curr_ct;
				/* starting from bottom of list,
				 * pull all entries up to fill up space
				 * made by deletion
				 */
				for( k = j; k < curr_ct; k++ )
					curr_list[k] = curr_list[k+1];
			} else	++j;
		if( nomatch )
			fprintf( stderr,
				"Object \"%s\" not found.\n", args[i] );
	}
	return( curr_ct );
}

/*
 *	Section 4:	S T R I N G   P R O C E S S I N G   R O U T I N E S
 *
 *			match()
 *			itoa()
 *			ftoascii()
 *			check()
 */

/*	==== m a t c h ( )
 *	if string matches pattern, return 1, else return 0
 *	special characters:
 *		*	Matches any string including the null string.
 *		?	Matches any single character.
 *		[...]	Matches any one of the characters enclosed.
 *		-	May be used inside brackets to specify range
 *			(i.e. str[1-58] matches str1, str2, ... str5, str8)
 *		\	Escapes special characters.
 */
match(	 pattern,  string )
register
char	*pattern, *string;
{
	do {	switch( *pattern ) {
		case '*': /*
			   * match any string including null string
			   */
			++pattern;
			do	if( match( pattern, string ) ) return( 1 );
			while( *string++ != '\0' );
			return( 0 );
			break;
		case '?': /*
			   * match any character
			   */
			if( *string == '\0' )	return( 0 );
			break;
		case '[': /*
			   * try to match one of the characters in brackets
			   */
			++pattern;
			while( *pattern != *string ) {
				if(	pattern[ 0] == '-'
				    &&	pattern[-1] != '\\'
				)	if(	pattern[-1] <= *string
					    &&	pattern[-1] != '['
					    &&	pattern[ 1] >= *string
					    &&	pattern[ 1] != ']'
					)	break;
				if( *++pattern == ']' )	return( 0 );
			}

			/* skip to next character after closing bracket
			 */
			while( *++pattern != ']' );
			break;
		case '\\': /*
			    * escape special character
			    */
			++pattern;
			/* WARNING: falls through to default case */
		default:  /*
			   * compare characters
			   */
			if( *pattern != *string )	return( 0 );
			break;
		}
		++string;
	} while( *pattern++ != '\0' );
	return( 1 );
}

/*	==== i t o a ( )
 *	Convert integer to ascii  wd format.
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
	do	s[i++] = n % 10 + '0';	while( (n /= 10) > 0 );
	if( sign < 0 )	s[i++] = '-';

	/* blank fill array
	 */
	for( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w )
		fprintf( stderr, "Itoa: field length too small.\n" );
	s[w] = '\0';

	/* reverse the array
	 */
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*	==== f t o a s c i i ( )
 *	Convert float to ascii  w.df format.
 */
ftoascii( f, s, w, d )
register
char	    *s;
register int	w, d;
float	  f;
{
	int	c, i, j;
	long	n, sign;

	if( w <= d + 2 ) {
		fprintf( stderr,
			"ftoascii: incorrect format  need w.df  stop" );
		exit( 10 );
	}
	for( i = 1; i <= d; i++ )	f = f * 10.0;

	/* round up */
	if( f < 0.0 )	f -= 0.5;
	else		f += 0.5;
	n = f;
	if( (sign = n) < 0 )	n = -n;
	i = 0;
	do {
		s[i++] = n % 10 + '0';
		if( i == d )	s[i++] = '.';
	} while( (n /= 10) > 0 );

	/* zero fill the d field if necessary
	 */
	if( i < d ) {	
		for( j = i; j < d; j++ )	s[j] = '0';
		s[j++] = '.';
		i = j;
	}
	if( sign < 0 )	s[i++] = '-';
	
	/* blank fill rest of field
	 */
	for ( j = i; j < w; j++ )	s[j] = ' ';
	if( i > w )
		fprintf( stderr, "Ftoascii: field length too small\n" );
	s[w] = '\0';

	/* reverse the array
	 */
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*	==== c h e c k ( )
 *	Compares solids to see if have a new solid.
 */
check( a, b )
register
char	*a, *b;
{
	register int	c = sizeof( struct ident );

	while( c-- )	if( *a++ != *b++ ) return( 0 );   /* new solid */
	return( 1 );   /* match - old solid */
}

/*
 *	Section 5:	I / O   R O U T I N E S
 *
 *			getcmd()
 *			getarg()
 *			pars_arg()
 *			menu()
 *			blank_fill()
 *			bug()
 *			fbug()
 */

/*	==== g e t c m d ( )
 *	Return first character read from keyboard,
 *	copy command into args[0] and arguments into args[1]...args[n].
 *		
 */
char
getcmd(  args, ct )
char	*args[];
register
int		ct;
{
	/* Get arguments.						 */
	if( ct == 0 )	while( --arg_ct >= 0 )	free( args[arg_ct] );
	for( arg_ct = ct; arg_ct < MAXARG - 1; ++arg_ct ) {
		args[arg_ct] = malloc( MAXLN );
		if( !getarg( args[arg_ct] ) )	break;
	}
	++arg_ct;
	args[arg_ct] = 0;

	/* Before returning to command interpreter,
	 * set up interrupt handler for commands...
	 * trap interrupts such that command is aborted cleanly and
	 * command line is restored rather than terminating program
	 */
	signal( SIGINT, abort );
	return( (args[0])[0] );
}

/*	==== g e t a r g ( )
 *	Get a word of input into 'str',
 *	Return 0 if newline is encountered.
  *	Return 1 otherwise.
 */
char
getarg(		 str )
register char	*str;
{
	do {
		*str = getchar();
		if( *str == ' ' ) {
			*str = '\0';
			return( 1 );
		} else	++str;
	} while( str[-1] != EOF && str[-1] != '\n' );
	if( str[-1] == '\n' )	--str;
	*str = '\0';
	return( 0 );
}

/*	==== p a r s _ a r g ( )
 *	Seperate words into seperate arguments.
 */
pars_arg( argvec,	ct )
char	 *argvec[];
register int		ct;
{
	char		buf[MAXLN];
	register char	*from, *to = buf;
	register int	i;

	for( i = 1; i < ct; i++ ) {
		from = argvec[i];
		while( *from != '\0' ) {
			if( *from != ' ' )	*to++ = *from++;
			else {
				*to = '\0';
				if( (arg_list[arg_ct] =
					malloc( strlen( buf ) + 1 ))
					== 0 ) {
					perror( "malloc()" );
					exit( 10 );
				}
				strcpy( arg_list[arg_ct], buf );
				to = buf;
				from++;
				arg_ct++;
			}
		}
		*to = '\0';
		if( (arg_list[arg_ct] = malloc( strlen( buf ) + 1 )) == 0 )
		{
			perror( "malloc()" );
			exit( 10 );
		}
		strcpy( arg_list[arg_ct], buf );
		to = buf;
		++arg_ct;
	}
}

/*	==== m e n u ( )
 *	Display menu stored at address 'addr'.
 */
menu( addr )
register
char *addr;
{
	register char	**sbuf = addr;
	
	while( *sbuf )	prompt( "%s\n", *sbuf++ );
	fflush( stdout );
}

/*	==== b l a n k _ f i l l ( )
 *	Write count blanks to fildes.
 */
blank_fill(	fildes,	count )
register int	fildes,	count;
{
	register char	*blank_buf = BLANKS;

	return( write( fildes, blank_buf, count ) );
}

/*
 *	Section 6:	I N T E R R U P T   H A N D L E R S
 *
 *			abort()
 *			quit()
 */

/*	==== a b o r t ( )
 *	Abort command without terminating run (restore command prompt) and
 *	cleanup temporary files.
 */
abort( sig ) {
	signal( SIGINT, quit );	/* reset trap */
	if( access( disc_file, 0 ) == 0 ) {
		unlink( disc_file );
		if( idfd > 0 )		close( idfd );
		if( rd_idfd > 0 )	close( rd_idfd );
	}
	if( access( reg_file, 0 ) == 0 ) {
		unlink( reg_file );
		if( rrfd > 0 )		close( rrfd );
		if( rd_rrfd > 0 )	close( rd_rrfd );
	}

	/* goto command interpreter with environment restored
	 */
	longjmp( env, sig );
}

/*	==== q u i t ( )
 *	Terminate run.
 */
quit( sig ) {
	fprintf( stderr, "quitting...\n" );
	exit( 0 );
}
