/*
 *	d e c k _ p r o c . c
 *	Proceedures for deck.c
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
#include "ged_types.h"
#include "3d.h"
#include "deck.h"
#include "deck_proc.h"
#include "tty.h"

/*
 *	Section 1:	C O M M A N D S
 *
 *			shell()
 */

/*	==== D E C K ( )
 *	make a COMGEOM deck for current list of objects
 *
 */
deck( prefix )
char *prefix;
{
	int	i, j;

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

	/* check integrity of list against directory and build
	 * card deck
	 */
	for( i = 0; i < curr_ct; i++ )
		if( (dp = lookup( curr_list[i], NOISY )) != -1 )
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
				read( rd_rrfd, name, 16 );
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
				printf( "region %s is member of a region ",
					findrr[i].rr_name );
				printf( ",but not in description\n" );
				exit( 10 );
 			}
		}
	}
	printf( "\n====================================================" );
	printf( "\nO U T P U T    F I L E S :\n\n" );
	printf( "solid table = \"%s\"\n", st_file );
	printf( "region table = \"%s\"\n", rt_file );
	printf( "region identification table = \"%s\"\n", id_file );
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

/*	==== S H E L L ( )
 *	execute shell command
 */
shell( args )
char  *args[];
{
	char	*argv[4], cmdbuf[MAXLN], *from, *to;
	int	pid, ret, status;
	int	i;

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
	printf( "\n" );
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
builddir() {
	register Directory *dp;

	dp = &directory[0];
	while( 1 ) {
		dp->d_addr = lseek( objfd, 0L, 1 );
		if(	read( objfd, &record, sizeof record )
			!= sizeof record
		)	break;
		if( ++ndir >= NDIR )  {
			printf( "Too many objects in input\n" );
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
			printf(	"builddir:  unknown record %c (0%o)\n",
				record.u_id,
				record.u_id );
			ndir--;
			break;
		}
		dp++;
	}
	printf( "\n%d objects tallied\n", ndir );
}

/*	==== T O C ( )
 *	Build a sorted list of names of all the objects accessable
 *	in the object file.
 */
toc() {
	extern char	*toc_list[];
	extern int	toc_ct;
	
	static Directory	*dp, *ep;
	register int		i;

	dp = &directory[0];
	ep = &directory[ndir];

	for( i = 0; dp < ep; )	toc_list[i++] = (dp++)->d_namep;
	toc_ct = i;
	sort( toc_list, toc_ct );
}

/*	==== L O O K U P ( )
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
			if( *np++ == 0 || i == 15 )	return( dp );
		}
	}
	if( flag == NOISY ) printf( "lookup: could not find '%s'\n", str );
	return( -1 );
}

/*	==== D I R A D D ( )
 *	Add an entry to the directory
 */
Directory *
diradd( name, laddr )
register char *name;
long laddr;
{
	register Directory *dp;

	if( ndir >= NDIR )  {
		printf("diradd:  no more dir structs\n");
		return( -1 );
	}

	dp = &directory[ndir++];
	dp->d_namep = addname( name );
	dp->d_addr = laddr;
	return( dp );
}

/*	==== A D D N A M E ( )
 *	Given a name, it puts the name in the name buffer, and
 *	returns a pointer to that string.
 */
addname( cp )
register char *cp;
{
	static char *holder;
	register int i;

	if( dir_last >= &dir_names[NDIR*10-16] )  {
		fprintf( stderr, "addname:  out of name space\n" );
		exit( 1 );
	}
	holder = dir_last;
	i = 0;
	while( *cp != 0 && i++ < 16 )	*dir_last++ = *cp++;
	*dir_last++ = 0;
	return( holder );
}

/*
 *	Section 3:	L I S T   P R O C E S S I N G   R O U T I N E S
 *
 *			list_toc()
 *			col_prt()
 *			insert()
 *			delete()
 *			sort()
 */

/*	==== L I S T _ T O C ( )
 *	list the table of contents
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

/*	==== C O L _ P R T
 *	print list of names in tabular columns
 */
col_prt( list, ct )
char	*list[];
int	ct;
{
	char		buf[72];
	char		*lbuf = buf;
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

/*	==== I N S E R T ( )
 *	insert each member of the table of contents 'toc_list' which
 *	matches one of the arguments into the current list 'curr_list'
 */
insert(  args,	ct )
char	*args[];
int		ct;
{
	char	*malloc();
	int	i, j, nomatch;
	unsigned bytect;
	
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
			printf( "object \"%s\" not found\n", args[i] );
	}
	return( curr_ct );
}

/*	==== D E L E T E ( )
 *	delete all members of current list 'curr_list' which match
 *	one of the arguments
 */
delete(  args )
char	*args[];
{
	int	i, j, k;
	int	nomatch;
	
	/* for each object in arg list
	 */
	for( i = 1; i < arg_ct; i++ ) {
		nomatch = YES;

		/* traverse list to find string
		 */
		for( j = 0; j < curr_ct; )
			if( match( args[i], curr_list[j] ) ) {
				nomatch = NO;
				free( curr_list[j] );
				--curr_ct;
				/* starting from bottom of list,
				 * pull all entries up to fill up space
				 * made by deletion
				 */
				for( k = j; k < curr_ct; k++ )
					curr_list[k] = curr_list[k+1];
			} else	++j;
		if( nomatch )	printf( "object \"%s\" not found\n",
					args[i] );
	}
	return( curr_ct );
}

/*	==== S O R T ( )
 *	sort a list of strings alphabetically stored at 'linebuf'
 */
sort( linebuf,	items )
char *linebuf[];
int		items;
{
	char	*p;
	int	i, j;
	int	change = YES;

	for( i = 0; i < items-1 && change == YES; i++ ) {
		change = NO;
		for( j = items-1; j > i; --j ) {
			if( strcmp( linebuf[j], linebuf[j-1] ) < 0 ) {
				p = linebuf[j];
				linebuf[j] = linebuf[j-1];
				linebuf[j-1] = p;
				change = YES;
			}
		}
	}
}

/*
 *	Section 4:	S T R I N G   P R O C E S S I N G   R O U T I N E S
 *
 *			match()
 *			itoa()
 *			ftoascii()
 *			check()
 */

/*	==== M A T C H ( )
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

/*	==== I T O A ( )
 *	convert integer to ascii  wd format
 */
itoa( n, s, w )
char	 s[];
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
	if( i > w )	printf( "itoa: field length too small\n" );
	s[w] = '\0';
	/* reverse the array
	 */
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*	==== F T O A S C I I ( )
 *	convert float to ascii  w.df format
 */
ftoascii( f, s, w, d )
char	     s[];
int		w, d;
float	  f;
{
	int	c, i, j;
	long	n, sign;

	if( w <= d + 2 ) {
		printf( "ftoascii: incorrect format  need w.df  stop" );
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
	if( i > w )	printf("ftoascii: field length too small\n");
	s[w] = '\0';

	/* reverse the array
	 */
	for( i = 0, j = w - 1; i < j; i++, j-- ) {
		c    = s[i];
		s[i] = s[j];
		s[j] =    c;
	}
}

/*	==== C H E C K ( )
 *	compares solids to see if have a new solid
 */
check( a, b )
register char	*a, *b;
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

/*	==== G E T C M D ( )
 *	return first character read from keyboard
 *	copy command into args[0] and arguments into args[1]...args[n]
 *		
 */
char
getcmd(  args, ct )
char	*args[];
int	ct;
{
	/* get arguments
	 */
	if( ct == 0 )	while( --arg_ct >= 0 )	free( args[arg_ct] );
	for( arg_ct = ct; arg_ct < MAXARG - 1; ++arg_ct ) {
		args[arg_ct] = malloc( MAXLN );
		if( getarg( args[arg_ct] ) )	break;
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

/*	==== G E T A R G ( )
 *	get a word of input into 'str', return first character
 */
char
getarg(		 str )
register char	*str;
{
	while( (*str = getchar()) != EOF && *str != '\n' ) {
		if( *str == ' ' ) {
			*str = '\0';
			return( 0 );
		} else	++str;
	}
	*str = '\0';
	return( 1 );
}

/*	==== P A R S _ A R G ( )
 *	seperate words into seperate arguments
 */
pars_arg( argvec,	ct )
char	 *argvec[];
int			ct;
{
	char	buf[MAXLN];
	char	*from, *to = buf;
	int	i;

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

/*	==== M E N U ( )
 *	display menu stored at address 'addr';
 */
menu( addr )
char *addr;
{
	char	**sbuf = addr;
	
	while( *sbuf )	printf( "%s\n", *sbuf++ );
}

/*	==== B L A N K _ F I L L ( )
 *	write count blanks to fildes
 */
blank_fill(	fildes,	count )
int		fildes,	count;
{
	char	*blank_buf = BLANKS;

	return( write( fildes, blank_buf, count ) );
}

/*	==== B U G ( )
 *	debug statements
 */
bug(	p0, p1, p2, p3, p4, p5, p6, p7, p8, p9 )
int	p0, p1, p2, p3, p4, p5, p6, p7, p8, p9;
{
	fprintf( stderr, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9 );
}

/*	==== F B U G ( )
 *	debug statements
 */
fbug(	control, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9 )
int	control;
float	    f0, f1, f2, f3, f4, f5, f6, f7, f8, f9;
{
	fprintf( stderr, control, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9 );
}

deadbug( control, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9 )
int	 control;
float	    f0, f1, f2, f3, f4, f5, f6, f7, f8, f9;
{}

/*
 *	Section 6:	I N T E R R U P T   H A N D L E R S
 *
 *			abort()
 *			quit()
 */

/*	==== A B O R T ( )
 *	abort command without terminating run (restore command prompt)
 *	cleanup /tmp files
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

/*	==== Q U I T ( )
 *	terminate run
 */
quit( sig ) {
	fprintf( stderr, "quitting...\n" );
	exit( 0 );
}
