/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Procedures for vproc.c

	Section 1:  Commands
		2:  Object Directory Routines
		3:  List Processing Routines
		4:  String Processing Routines
		5:  Input/Output Routines
		6:  Interrupt Handlers
	  
 */
#include <stdio.h>
#include <signal.h>

#include "./vextern.h"

extern void	eread(), ewrite();
extern void	mat_idn();

#include "./std.h"

Directory	directory[NDIR];
static char	*db_title = NULL, *db_units = "  ";

char		*addname(), getarg();
Directory	*lookup(), *diradd();
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
	{	register int	i, j;
	
	nns = nnr = regflag = numrr = 0;
	
	/* Rewind object file.						*/
	(void) lseek( objfd, 0L, 0 );

	/* Create file for solid table.					*/
	if( prefix != 0 )
		{
		(void) strncpy( st_file, prefix, 73 );
		(void) strcat( st_file, ".st" );
		}
	else
		(void) strncpy( st_file, "solids", 7 );
	if( (solfd = creat( st_file, 0666 )) < 0 )
		{
		perror( st_file );
		exit( 10 );
		}

	/* Target units (a2,3x)						*/
	ewrite( solfd, db_units, 2 );
	blank_fill( solfd, 3 );

	/* Title							*/
	if( db_title == NULL )
		ewrite( solfd, objfile, (unsigned) strlen( objfile ) );
	else
		ewrite( solfd, db_title, (unsigned) strlen( db_title ) );
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
	if( (regfd = creat( rt_file, 0666 )) < 0 )
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
	if( (ridfd = creat( id_file, 0666 )) < 0 )
		{
		perror( id_file );
		exit( 10 );
		}
	itoa( -1, buff, 5 );
	ewrite( ridfd, buff, 5 );
	ewrite( ridfd, LF, 1 );

	/* Create /tmp file for discrimination of files.		*/
	(void) strncpy( disc_file, mktemp( "/tmp/disXXXXXX" ), 15 );
	if( (idfd = creat( disc_file, 0666 )) < 0 )
		{
		perror( disc_file );
		exit( 10 );
		}
	rd_idfd = open( disc_file, 2 );

	/* Create /tmp file for storage of region names in the comgeom desc.	*/
	(void) strncpy( reg_file, mktemp( "/tmp/regXXXXXX" ), 15 );
	if( (rrfd = creat( reg_file, 0666 )) < 0 )
		{
		perror( reg_file );
		exit( 10 );
		}
	rd_rrfd = open( reg_file, 2 );

	/* Initialize matrices.						*/
	mat_idn( identity );
	mat_idn( xform );

	/* Check integrity of list against directory and build card deck.	*/
	for( i = 0; i < curr_ct; i++ )
		{	Directory	*dirp;
		if( (dirp = lookup( curr_list[i], NOISY )) != DIR_NULL )
			cgobj( dirp, 0, identity );
		}
	/* Add number of solids and regions on second card.		*/
	(void) lseek( solfd, savsol, 0 );
	itoa( nns, buff, 5 );
	ewrite( solfd, buff, 5 );
	itoa( nnr, buff, 5 );
	ewrite( solfd, buff, 5 );

	/* Finish region id table.					*/
	ewrite( ridfd, LF, 1 );

 	/* Must go back and add regions as members of regions.		*/
	if( numrr > 0 )
		{
		for( i = 1; i <= numrr; i++ )
			{
			(void) lseek( rd_rrfd, 0L, 0 );   /* rewind */
			for( j = 1; j <= nnr; j++ )
				{
				/* Next region name in desc.		 */
				eread( rd_rrfd, name, NAMESIZE );
				if( strcmp( findrr[i].rr_name, name ) == 0 )
					{ /* Region number in desc is j add
						to regfd at rrpos[i]	*/
					(void) lseek( regfd, findrr[i].rr_pos, 0);
					itoa( j+delreg, buff, 4 );
					ewrite( regfd, buff, 4 );
					break;
					}
				}
			if( j > nnr )
				{
				(void) fprintf( stderr,
					"Region %s is member of a region ",
					findrr[i].rr_name );
				(void) fprintf( stderr,
					"but not in description.\n" );
				exit( 10 );
 				}
			}
		}
	(void) printf( "====================================================\n" );
	(void) printf( "O U T P U T    F I L E S :\n\n" );
	(void) printf( "solid table = \"%s\"\n", st_file );
	(void) printf( "region table = \"%s\"\n", rt_file );
	(void) printf( "region identification table = \"%s\"\n", id_file );
	(void) close( solfd );
	(void) close( regfd );
	(void) close( ridfd );
	(void) unlink( disc_file );
	(void) close( idfd );
	(void) close( rd_idfd );
	(void) unlink( reg_file );
	(void) close( rrfd );
	(void) close( rd_rrfd );

	/* reset starting numbers for solids and regions
	 */
	delsol = delreg = 0;
	}

/*	s h e l l ( )
	Execute shell command.
 */
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

/*
	Section 2:	O B J E C T   D I R E C T O R Y   R O U T I N E S

			builddir()
			toc()
			diradd()
			addname()
 */

/*
			B U I L D D I R

 This routine reads through the 3d object file and
 builds a directory of the object names, to allow rapid
 named access to objects.
 */
builddir()
	{	register Directory *dirp;
	(void) printf( "Building the directory.\n" );
	(void) fflush( stdout );
	for(	dirp = directory, ndir = 0;
		ndir < NDIR
	    &&	(dirp->d_addr = lseek( objfd, 0L, 1 )) != -1
	    &&	read( objfd, (char *) &record, sizeof(record) ) == sizeof(record);
		dirp++, ndir++
		)
		{
		switch( record.u_id )
			{
		case ID_IDENT : /* Identification record.		*/
			{	static int	units_set_flag = false;
			ndir--;	/* Don't include in directory.		*/
			dirp--;
			if( db_title == NULL )
				{
				/* This must be the first ident record.	*/
				db_title = emalloc( strlen(record.i.i_title)+1 );
				(void) strcpy( db_title, record.i.i_title );
				}
			(void) fprintf( stdout, "%s\n", record.i.i_title );
			(void) fprintf( stdout,
					"GED database version (%s)\n",
					record.i.i_version
					);
			/* Ignore second ident records' units, unless
				previous were bogus or unspecified.
			 */
			if( ! units_set_flag )
				{
				/* NOTE : Default unit conversion factor (1.0)
					is set in 'vglobal.c'.
				 */
				switch( record.i.i_units )
					{
				case ID_NO_UNIT : /* unspecified	*/
					(void) printf( "No units specified.\n" );
					break;
				case ID_MM_UNIT	: /* milimeters		*/
					(void) printf( "Units = milimeters.\n" );
					unit_conversion = 1.0;
					units_set_flag = true;
					(void) strcpy( db_units, "mm" );
					break;
				case ID_CM_UNIT	: /* centimeters	*/
					(void) printf( "Units = centimeters.\n" );
					unit_conversion = 0.1;
					units_set_flag = true;
					(void) strcpy( db_units, "cm" );
					break;
				case ID_M_UNIT  : /* meters		*/
					(void) printf( "Units = meters.\n" );
					unit_conversion = 0.001;
					units_set_flag = true;
					(void) strcpy( db_units, "m " );
					break;
				case ID_IN_UNIT	: /* inches		*/
					(void) printf( "Units = inches.\n" );
					unit_conversion = 0.03937008;
					units_set_flag = true;
					(void) strcpy( db_units, "in" );
					break;
				case ID_FT_UNIT	: /* feet		*/
					(void) printf( "Units = feet.\n" );
					unit_conversion = 0.00328084;
					units_set_flag = true;
					(void) strcpy( db_units, "ft" );
					break;
				default :
					(void) fprintf( stderr,
							"Unknown units (%d)!\n",
							record.i.i_units
							);
					break;
					}
				break;
				}
			}
		case ID_FREE :  /* Free record -- ignore.		*/
		case ID_MATERIAL : /* Material database record -- ignore.	*/
			ndir--;
			dirp--;
			break;
		case ID_SOLID : /* Check for a deleted record.	 	*/
			if( record.s.s_name[0] == 0 )
				{
				ndir--;
				dirp--;
				}
			else
				dirp->d_namep = addname( record.s.s_name );
			break;
		case ID_ARS_A :  /* Check for a deleted record.		 */
			if( record.s.s_name[0] == 0 )
				{
				ndir--;
				dirp--;
				}
			else
				dirp->d_namep = addname( record.s.s_name );
			/*  Skip remaining B type records.		*/
			(void) lseek(	objfd,
					(long)(record.a.a_totlen * sizeof record),
					1
					);
			break;
		case ID_COMB :  /* Check for a deleted record.		 */
			if( record.c.c_name[0] == 0 )
				{
				ndir--;
				dirp--;
				}
			else
				dirp->d_namep = addname( record.c.c_name );
			/* Skip over remaining records.			 */
			(void) lseek(	objfd,
					(long)(record.c.c_length * sizeof(record)),
					1
					);
			break;
		default :
			(void) fprintf( stderr,
					"Builddir:  unknown record %c (0%o).\n",
					record.u_id,
					record.u_id
					);
			ndir--;
			dirp--;
			break;
			}
		}
	if( ndir == NDIR )
		(void) fprintf( stderr, "Too many objects in input\n" );
	(void) printf( "%d objects tallied\n", ndir );
	return;
	}

/*	t o c ( )
	Build a sorted list of names of all the objects accessable
	in the object file.
 */
void
toc()
	{	register int		i;
	(void) printf( "Making the Table of Contents.\n" );
	(void) fflush( stdout );

	for( i = 0; i < ndir; i++ )
		toc_list[i] = directory[i].d_namep;
	return;
	}

/*	l o o k u p ( )
	This routine takes a name, and looks it up in the
	directory table.  If the name is present, a pointer to
	the directory struct element is returned, otherwise
	a -1 is returned.

	If the flag is NOISY, a print occurs, else only
	the return code indicates failure.
 */
Directory *
lookup( str, flag )
register char *str;
	{	register Directory	*dirp;
		register char		*np;
		static Directory	*ep;
		static int		i;
	ep = &directory[ndir];
	for( dirp = &directory[0]; dirp < ep; dirp++ )
		{
		np = dirp->d_namep;
		for( i = 0; i < NAMESIZE; i++ )
			{
			if( str[i] != *np )
				break;
			if( *np++ == 0 || i == 15 )
				return	dirp;
			}
		}
	if( flag == NOISY )
		(void) fprintf( stderr, "Lookup: could not find '%s'.\n", str );
	return	DIR_NULL;
	}

/*	d i r a d d ( )
	Add an entry to the directory.
 */
Directory *
diradd( namep, laddr )
register char	*namep;
long		laddr;
	{	register Directory *dirp;

	if( ndir >= NDIR )
		{
		(void) fprintf( stderr, "Diradd:  no more dir structs.\n" );
		return	DIR_NULL;
		}
	dirp = &directory[ndir++];
	dirp->d_namep = addname( namep );
	dirp->d_addr = laddr;
	return	dirp;
	}

/*	a d d n a m e ( )
	Given a name, it puts the name in the name buffer, and
	returns a pointer to that string.
 */
char *
addname( cp )
register char	*cp;
	{	static char	*holder;
		register int	i;
	if( dir_last >= &dir_names[NDIR*10-NAMESIZE] )
		{
		(void) fprintf( stderr, "Addname:  out of name space.\n" );
		exit( 1 );
		}
	holder = dir_last;
	i = 0;
	while( *cp != 0 && i++ < NAMESIZE )
		*dir_last++ = *cp++;
	*dir_last++ = 0;
	return	holder;
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
	{	register int	i, j;
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
	{	char		buf[MAX_COL+2];
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
insert(  args,	ct )
char		*args[];
register int	ct;
	{	register int	i, j, nomatch;
		unsigned	bytect;

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
				bytect = strlen( toc_list[j] );
				curr_list[curr_ct] = emalloc( (int) ++bytect );
				/* Insert string at end of list.		*/
				(void) strcpy(	curr_list[curr_ct++],
						toc_list[j]
						);
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
				 made by deletion
				 */
				for( k = j; k < curr_ct; k++ )
					curr_list[k] = curr_list[k+1];
			} else	++j;
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
			ftoascii()
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
	do	s[i++] = n % 10 + '0';	while( (n /= 10) > 0 );
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

/*	f t o a s c i i ( )
	Convert float to ascii  w.df format.
 */
ftoascii( f, s, w, d )
register char	   *s;
register int	w, d;
float	  	f;
	{	register int	c, i, j;
		long	n, sign;

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
		}
	while( (n /= 10) > 0 );

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

/*	c h e c k ( )
	Compares solids to see if have a new solid.
 */
check( a, b )
register char	*a, *b;
	{ 	register int	c = sizeof( struct deck_ident );
	while( c-- )
		if( *a++ != *b++ )
			return	0;   /* new solid */
	return	1;   /* match - old solid */
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
			free( args[arg_ct] );
	for( arg_ct = ct; arg_ct < MAXARG - 1; ++arg_ct )
		{
		args[arg_ct] = emalloc( MAXLN );
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
		}
	while( (int)(str[-1]) != EOF && (int)(str[-1]) != '\n' );
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
	{	register char	**sbuf = addr;
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
	{	register char	*blank_buf = BLANKS;
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
	if( access( disc_file, 0 ) == 0 )
		{
		(void) unlink( disc_file );
		if( idfd > 0 )
			(void) close( idfd );
		if( rd_idfd > 0 )
			(void) close( rd_idfd );
		}
	if( access( reg_file, 0 ) == 0 )
		{
		(void) unlink( reg_file );
		if( rrfd > 0 )
			(void) close( rrfd );
		if( rd_rrfd > 0 )
			(void) close( rd_rrfd );
		}
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
