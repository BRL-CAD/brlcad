/*
 *	@(#) vdeck.c		retrieved 8/13/86 at 08:05:20,
 *	@(#) version 1.6		  created 3/29/83 at 10:53:50.
 *
 *	Written by Gary S. Moss.
 *	All rights reserved, Ballistic Research Laboratory.
 *
 *	Generates interactively, a COMGEOM deck of objects from GED data
 *	base file.
 *
 *	Written by	Gary S. Moss
 *
 *	Derived from KARDS, written by Keith Applin.
 *
 *	To compile	make all
 */
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include "ged_types.h"
#include "3d.h"
#include "deck.h"
#include "deck_glob.h"

char	*cmd[] = {
"",
"C O M M A N D                  D E S C R I P T I O N",
"",
"deck [output file prefix]      Produce COM GEOM card deck.",
"erase                          Erase current list of objects.",
"insert [object[s]]             Add an object to current list.",
"list [object[s]]               Display current list of selected objects.",
"number [solid] [region]        Specify starting numbers for objects.",
"quit                           Terminate run.",
"remove [object[s]]             Remove an object from current list.",
"sort                           Sort table of contents alphabetically.",
"toc [object[s]]                Table of contents of solids database.",
"! [shell command]              Execute a UNIX shell command.",
"",
"NOTE:",
"First letter of command is sufficient, and all arguments are optional.",
"Objects may be specified with string matching operators (*, [], -, ? or \\)",
"as in the UNIX shell.",
0 };

char	*usage[] = {
"",
"d e c k ====",
"Make COMGEOM decks of objects from a GED data base file",
"Usage:  deck {file}.g [{obj1} {obj2} ... {objn}]",
"",
0 };

/*	==== m a i n ( )
 */
main( argc, argv )	char	*argv[];
{
	if( argc < 2 ) { menu( usage );	exit( 1 ); }
	setbuf( stdout, malloc( BUFSIZ ) );

	/* open  G E D  data base object file
	 */
	if( (objfd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		menu( usage );
		exit( 10 );
	} else	objfile = argv[1];
	builddir();	/* Build directory from object file.	 	*/
	toc();		/* Build table of contents from directory.	*/

	if( argc > 1 )
	{	/* Add objects from command line to current list.	*/
		pars_arg( argv, argc );
		insert( arg_list, arg_ct );
	}

	/* C o m m a n d   I n t e r p r e t e r
	 */
	setjmp( env );	/* Point of re-entry from aborted command.	*/
	prompt( "%s", CMD_PROMPT );	
	while( 1 ) {
		/* Return to default interrupt handler after every command,
		 * allows exit from program only while command interpreter
		 * is waiting for input from keyboard.
		 */
		signal( SIGINT, quit );

		switch( getcmd( arg_list, 0 ) ) {
		case DECK:
			deck( arg_list[1] );
			break;
		case ERASE:
			while( curr_ct > 0 ) free( curr_list[--curr_ct] );
			break;
		case INSERT:
			if( arg_list[1] == 0 ) {
				prompt( "enter object[s] to insert: " );
				getcmd( arg_list, arg_ct );
			}
			insert( arg_list, arg_ct );
			break;
		case LIST:
		{	register int	i;
			if( arg_list[1] == 0 ) {
				col_prt( curr_list, curr_ct );
				break;
			}
			for( tmp_ct = 0, i = 0; i < curr_ct; i++ )
				if( match( arg_list[1], curr_list[i] ) )
					tmp_list[tmp_ct++] = curr_list[i];
			col_prt( tmp_list, tmp_ct );
			break;
		}
		case MENU:
			menu( cmd );
			prompt( "%s", PROMPT );
			continue;
		case NUMBER:
			if( arg_list[1] == 0 ) {
				prompt( "enter number of 1st solid: " );
				getcmd( arg_list, arg_ct );
				prompt( "enter number of 1st region: " );
				getcmd( arg_list, arg_ct );
			}
			if( arg_list[1] ) delsol = atoi( arg_list[1] ) - 1;
			if( arg_list[2] ) delreg = atoi( arg_list[2] ) - 1;
			break;
		case REMOVE:
			if( arg_list[1] == 0 ) {
				prompt( "enter object[s] to remove: " );
				getcmd( arg_list, arg_ct );
			}
			delete( arg_list );
			break;
		case RETURN:
			prompt( "%s", PROMPT );
			continue;
		case SHELL:
			if( arg_list[1] == 0 ) {
				prompt( "enter shell command: " );
				getcmd( arg_list, arg_ct );
			}
			shell( arg_list );
			break;
		case SORT_TOC:
			sort( toc_list, toc_ct );
			break;
		case TOC:
			list_toc( arg_list );
			break;
		case EOF:
		case QUIT:
			prompt( "quitting...\n" );
			exit( 0 );
		UNKNOWN:
			prompt( "invalid command\n%s", PROMPT );
			continue;
		}
		prompt( "%s", CMD_PROMPT );		
	}
}

/*	==== c g o b j ( )
 *	Build deck for object pointed to by 'dp'.
 */
cgobj(	   dp, pathpos, old_xlate )
register
Directory *dp;
int	pathpos;
matp_t	old_xlate;
{
	register struct members	*mp;
	Record		rec;
	Directory	*nextdp, *tdp;
	mat_t			new_xlate;
	long	savepos;
	int	nparts;
	int	i, j;
	int	length;
	int	dchar = 0;
	int	nnt;
	char	buf[80], *bp;
	char	ars_name[16];

	/* Limit tree hierarchy to 8 levels.				*/
	if( pathpos >= 8 ) {
		fprintf( stderr, "Nesting exceeds 8 levels\n" );
		for( i = 0; i < 8; i++ )
			fprintf( stderr, "/%s", path[i]->d_namep );
		fprintf( stderr, "\n" );
		return;
	}
	savepos = lseek( objfd, 0L, 1 );
	lseek( objfd, dp->d_addr, 0 );
	readF( objfd, &rec, sizeof rec );

	if( rec.u_id == COMB )  { /* We have a group.			*/
		if( regflag > 0 ) {
			/* record is part of a region
			 */
			if( operate == UNION ) {
				fprintf( stderr,
					"Region: %s is member of ",
					rec.c.c_name );
				fprintf( stderr,
					"region %s with OR operation.\n",
					buff );
				exit( 10 );
			}

			/* check for end of line in region table
			 */
			if(	(isave % 9 ==  1 && isave >  1)
			    ||	(isave % 9 == -1 && isave < -1) )
			{
				write( regfd, buff, strlen( buff ) );
				write( regfd, LF, 1 );
				blank_fill( regfd, 6 );
			}
			buf[0] = 'r';	buf[1] = 'g';	buf[2] = operate;
			write( regfd, buf, 3 );

			/* check if this region is in desc yet
			 */
			lseek( rd_rrfd, 0L, 0 );
			for( j = 1; j <= nnr; j++ ) {
				readF( rd_rrfd, name, 16 );
				if( strcmp( name, rec.c.c_name ) == 0 ) {
					/* region is #j
					 */
					itoa( j+delreg, buf, 4 );
					write( regfd, buf, 4 );
					break;
				}
			}
			if( j > nnr ) {   /* region not in desc yet */
				numrr++;
				if( numrr > MAXRR ) {
					fprintf( stderr,
						"More than %d regions.\n",
						MAXRR );
					exit( 10 );
				}

				/* add to list of regions to look up later
				 */
				findrr[numrr].rr_pos = lseek(	regfd,
								0L,
								1 );
				for( i = 0; i < 16; i++ )
					findrr[numrr].rr_name[i] =
						   rec.c.c_name[i];
				blank_fill( regfd, 4 );
			}

			/* check for end of this region
			 */
			if( isave < 0 ) {
				isave = -isave;
				regflag = 0;
				if( (j = isave % 9) > 0 )
					blank_fill( regfd, 7*(9-j) );
				write( regfd, buff, strlen( buff ) );
				write( regfd, LF, 1 );
			}
			lseek( objfd, savepos, 0);
			return;
		}

		regflag = 0;
		nparts = rec.c.c_length;
		if( rec.c.c_flags == 'R') {
			/* record is a region but not member of a region
			 */
			regflag = 1;
			nnr++;

			/* dummy region
			 */
			if( nparts == 0 )	regflag = 0;

			/* save the region name
			 */
			strncpy( buff, rec.c.c_name, 16 );

			/* start new region
			 */
			itoa( nnr+delreg, buf, 5 );
			write( regfd, buf, 5 );
			blank_fill( regfd, 1 );

			/* check for dummy region
			 */
			if( nparts == 0 ) {
				write( regfd, LF, 1 );
				regflag = 0;
			}

			/* add region to list of regions in desc
			 */
			lseek( rrfd, 0L, 2 );
			write( rrfd, rec.c.c_name, 16 );

			/* check for any OR
			 */
			orflag = 0;
			if( nparts > 1 ) {
				/* first OR doesn't count, throw away
				 * first member
				 */
				readF( objfd, &rec, sizeof rec );
				for( i = 2; i <= nparts; i++ ) {
					readF( objfd, &rec, sizeof rec );
					if( rec.M.m_relation == UNION ) {
						orflag = 1;
						break;
					}
				}
				lseek( objfd, dp->d_addr, 0 );
				readF( objfd, &rec, sizeof rec );
			}

			/* write region ident table
			 */
			itoa(	nnr+delreg,	buf,	  10 );
			itoa(	rec.c.c_regionid,	&buf[10], 10 );
			itoa(	rec.c.c_aircode,	&buf[20], 10 );
			write(	ridfd,		buf,	  30 );
			blank_fill( ridfd,		  10 );
			bp = buf;
			length = strlen( rec.c.c_name );
			for( j = 0; j < pathpos; j++ ) {
				strncpy(	bp,
						path[j]->d_namep,
						strlen( path[j]->d_namep )
				);
				bp += strlen( path[j]->d_namep );
				*bp++ = '/';
			}
			length += bp - buf;
			if( length > 34 ) {
				bp = buf + (length - 34);
				*bp = '*';
				write(	ridfd,
					bp,
					34 - strlen( rec.c.c_name )
				);
			} else	write( ridfd, buf, bp - buf );
			write(	ridfd,
				rec.c.c_name,
				strlen( rec.c.c_name )
			);
			write( ridfd, LF, 1 );
			printf( "\nREGION %4d    ", nnr+delreg );
			for( j = 0; j < pathpos; j++ )
				printf( "/%s", path[j]->d_namep );
			prompt( "/%s", rec.c.c_name );
		}
		isave = 0;
		for( i = 1; i <= nparts; i++ )  {
			if( ++isave == nparts )	isave = -isave;
			readF( objfd, &rec, sizeof rec );
			mp = &rec.M;
 
			/* save this operation
			 */
			operate = mp->m_relation;
 
			path[pathpos] = dp;
			if(	(nextdp =
				lookup( mp->m_instname, NOISY )) == -1 )
				continue;
			if( mp->m_brname[0] != 0 )  {
				/* Create an alias.
				 * First step towards full branch naming.
				 * User is responsible for his branch names
				 * being unique.
				 */
				if(	(tdp =
					lookup( mp->m_brname, QUIET )) !=
					-1 )
					/* use existing alias
					 */
					nextdp = tdp;
				else	nextdp = diradd(mp->m_brname,
							nextdp->d_addr );
			}
			mat_mul( new_xlate, mp->m_mat, old_xlate );

			/* Recursive call
			 */
			cgobj( nextdp, pathpos+1, new_xlate );
		}
		lseek( objfd, savepos, 0 );
		return;
	}

	/* N O T  a  C O M B I N A T I O N  record
	 */
	if( rec.u_id != SOLID && rec.u_id != ARS_A ) {
		fprintf( stderr,
			"Bad input: should have a 'S' or 'A' record, " );
		fprintf( stderr,
			"but have '%c'.\n", rec.u_id );
		exit( 10 );
	}

	/* now have proceeded down branch to a solid
	 *
	 * if regflag = 1  add this solid to present region
         * if regflag = 0  solid not defined as part of a region
	 *		   make new region if scale != 0 
	 *
	 * if orflag = 1   this region has or's
	 * if orflag = 0   none
	 */
	if( old_xlate[15] < .0001 ) {     /* do not add solid */
		lseek( objfd, savepos, 0 );
		return;
	}

	/* fill ident struct
	 */
	mat_copy( ident.i_mat, old_xlate );
	strncpy( ident.i_name, rec.s.s_name, 16 );
	strncpy(     ars_name, rec.s.s_name, 16 );
	
	/* calculate first look discriminator for this solid
	 */
	dchar = 0;
	for( i = 0; i < 16; i++ ) {
		if( rec.s.s_name[i] == 0 )	break;
		dchar += (rec.s.s_name[i] << (i&7));
	}

	/* quick check if solid already in solid table
	 */
	nnt = 0;
	for( i = 0; i < nns; i++ ) {
		if( dchar == discr[i] ) {
			/* quick look match - check further
			 */
			lseek( rd_idfd, ((long)i) * sizeof ident, 0);
			readF( rd_idfd, &idbuf, sizeof ident );
			ident.i_index = i + 1;
			if( check( &ident, &idbuf ) == 1 ) {
				/*really is an old solid
				 */
				nnt = i + 1;
				goto notnew;
			}
			/* false alarm - keep looking for
			 * quick look matches */
		}
	}

	/* new solid
	 */
	discr[nns] = dchar;
	nns++;
	ident.i_index = nns;

	if( nns > MAXSOL ) {
		fprintf( stderr,
			"\nNumber of solids (%d) greater than max (%d).\n",
			nns, MAXSOL );
		exit( 10 );
	}

	/* write ident struct at end of idfd file
	 */
	lseek( idfd, 0L, 2 );
	write( idfd, &ident, sizeof ident );
	nnt = nns;

	/* process this solid
	 */
	mat_copy( xform, old_xlate );
	mat_copy( notrans, xform );

	/* notrans = homogeneous matrix with a zero translation vector
	 */
	notrans[3]  = notrans[7]  = notrans[11] = 0.0;

	/* write solid #
	 */
	itoa( nnt+delsol, buf, 3 );
	write( solfd, buf, 3 );

	/* process appropriate solid type
	 */
	switch( rec.s.s_type ) {
	case TOR :
		addtor( &rec );
		break;
	case GENARB8 :
		addarb( &rec );
		break;
	case GENELL :
		addell( &rec );
		break;
	case GENTGC :
		addtgc( &rec );
		break;
	case ARS :
		addars( &rec );
		break;
	default:
		fprintf( stderr,
			"Solid type (%d) unknown.\n", rec.s.s_type );
		exit( 10 );
	}

notnew:	/* sent here if solid already in solid table
	 */
	/* finished with solid
	 */
	/* put solid in present region if regflag == 1
	 */
	if( regflag == 1 ) {
		/* isave = number of this solid in this region
		 * if negative then is the last solid in this region */
		if(	(isave % 9 ==  1 && isave >  1)
		    ||	(isave % 9 == -1 && isave < -1) ) {
			/* new line
			 */
			write( regfd, buff, strlen( buff ) );
			write( regfd, LF, 1 );
			blank_fill( regfd, 6 );
		}
		buf[0] = ' ';	buf[1] = ' ';
		nnt += delsol;
		if( operate == '-' )	nnt = -nnt;
		if( orflag == 1 ) {
			if( operate == UNION || isave == 1 ) {
				buf[0] = 'o';
				buf[1] = 'r';
			}
		}
		write( regfd, buf, 2 );
		itoa( nnt, buf, 5 );
		write( regfd, buf, 5 );
		if( nnt < 0 )	nnt = -nnt;
		nnt -= delsol;
		if( isave < 0 ) {   /* end this region */
			isave = -isave;
			regflag = 0;
			if( (j = isave % 9) > 0 )
				blank_fill( regfd, 7*(9-j) );
			write( regfd, buff, strlen( buff ) );
			write( regfd, LF, 1 );
		}
	} else if( old_xlate[15] > 0.0001 ) {
		/* solid not part of a region
		 * make solid into region if scale > 0
		 */
		++nnr;
		itoa(	nnr+delreg, buf,     5 );
		itoa(	nnt+delsol, &buf[5], 8 );
		write(	regfd,	    buf,    13 );
		blank_fill( regfd, 56 );
		write( regfd, rec.s.s_name, strlen( rec.s.s_name ) );
		itoa( nnr+delreg, buf,	  10 );
		itoa(	item,	&buf[10], 10 );
		itoa(	space,	&buf[20], 10 );
		write(	ridfd,	buf,	  30 );
		blank_fill( ridfd, 10 );
		printf( "\nREGION %4d    ", nnr+delreg );
		j = strlen( rec.s.s_name );
		for( i = 0; i < pathpos; i++ ) {
			if( j += strlen( path[i]->d_namep ) < 38 )
				write(	ridfd,
					path[i]->d_namep,
					strlen( path[i]->d_namep ) );
			write( ridfd, "/", 1 );
			printf( "/%s", path[i]->d_namep );
			j++;
		}
		
		if( rec.u_id == ARS_B ) {	/* ars extension record
						 */
			prompt( "/%s", ars_name );
			write( ridfd, ars_name, strlen( ars_name ) );
		} else	{
			prompt( "/%s", rec.s.s_name );
			write(	ridfd,
				rec.s.s_name,
				strlen( rec.s.s_name )
			);
		}
		write( ridfd, LF, 1 );
		write( regfd, LF, 1 );
	}
	if( isave < 0 )	regflag = 0;
	lseek( objfd, savepos, 0 );
	return;
}

/*	==== p s p ( )
 *	Print solid parameters  -  npts points or vectors.
 */
psp(	npts,  rec )
register
int	npts;
register
Record *rec;
{
	register int	i, j, k, jk;
	char		buf[60];

	j = jk = 0;
	for( i = 0; i < npts*3; i += 3 )  {
		/* write 3 points
		 */
		for( k = i; k <= i+2; k++ ) {
			ftoascii( rec->s.s_values[k], &buf[jk*10], 10, 4 );
			++jk;
		}

		if( (++j & 01) == 0 ) {
			/* end of line
			 */
			write( solfd, buf, 60 );
			jk = 0;
			write(	solfd,
				rec->s.s_name,
				strlen( rec->s.s_name )
			);
			write( solfd, LF, 1 );
			if( i != (npts-1)*3 ) {   /* new line */
				itoa( nns+delsol, buf, 3 );
				write( solfd, buf, 3 );
				blank_fill( solfd, 7 );
			}
		}
	}	
	if( (j & 01) == 1 ) {   /* finish off rest of line */
		for( k = 30; k <= 60; k++ )	buf[k] = ' ';
		write( solfd, buf, 60 );
		write( solfd, rec->s.s_name, strlen( rec->s.s_name ) );
		write( solfd, LF, 1 );
	}
	return;
}

/*	==== a d d t o r ( )
 *	Process torus.
 */
addtor( rec )
register
Record *rec;
{
	int	i;
	float	work[3];
	float	rr1,rr2;
	vect_t	v_work;

	write( solfd, "tor    ", 7 );

	/* operate on vertex
	 */
	matXvec( v_work, xform, &(rec->s.s_values[0]) );
	VMOVE( &(rec->s.s_values[0]) , v_work );

	/* rest of vectors
	 */
	for( i = 3; i <= 21; i += 3 ) {
		matXvec( v_work, notrans, &(rec->s.s_values[i]) );
		VMOVE( &(rec->s.s_values[i]), v_work );
	}
	rr1 = MAGNITUDE( SV2 );	/* r1 */
	rr2 = MAGNITUDE( SV1 );	/* r2 */    

	/*  print solid parameters
	 */
	work[0] = rr1;
	work[1] = rr2;
	work[2] = 0.0;
	VMOVE( SV2, work );
	psp( 3, rec );
	return;
}

/*	==== a d d a r b ( )
 *	Process generalized arb.
 */
addarb( rec )
register
Record *rec;
{
	register int	i;
	float	work[3], worc[3];
	vect_t	v_work, v_workk;
	float	xmin, xmax, ymin, ymax, zmin, zmax;
	int	univecs[8], samevecs[11];
	
	/* Operate on vertex.						*/
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk );
	htov_move( &(rec->s.s_values[0]), v_work );

	/* Rest of vectors.						*/
	for( i = 3; i <= 21; i += 3 ) {
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( v_workk, v_work );

		/* Point notation.					*/
		VADD2(	&(rec->s.s_values[i]),
			&(rec->s.s_values[0]),
			v_workk
		);
	}

	/* Enter new arb code.						*/
	if( (i = cgarbs( rec, univecs, samevecs )) == 0 ) return;
	redoarb( rec, univecs, samevecs, i );
	rec->s.s_num *= -1;
	
	/* Print the solid parameters.					*/
	switch( rec->s.s_num ) {
	case ARB8:
		write( solfd, "arb8   ", 7 );
		psp( 8, rec );
		break;
	case ARB7:
		write( solfd, "arb7   ", 7 );
		psp( 7, rec );
		break;
	case ARB6:
		write( solfd, "arb6   ", 7 );
		VMOVE( SV5, SV6 );
		psp( 6, rec );
		break;
	case ARB5:
		write( solfd, "arb5   ", 7 );
		psp( 5, rec );
		break;
	case ARB4:
		write( solfd, "arb4   ", 7 );
		VMOVE( SV3, SV4 );
		psp( 4, rec );
		break;
	case RAW:
		write( solfd, "raw    ", 7 );
		VSUB2( work, SV1, SV0 );
		VSUB2( SV1, SV3, SV0);		/* H */
		VMOVE( SV2, work);		/* W */
		VSUB2( SV3, SV4, SV0);		/* D */
		psp( 4, rec );
		break;
	case BOX:
		write( solfd, "box    ", 7 );
		VSUB2( work, SV1, SV0 );
		VSUB2( SV1, SV3, SV0);		/* H */
		VMOVE( SV2, work);		/* W */
		VSUB2( SV3, SV4, SV0);		/* D */
		psp( 4, rec );
		break;
	case RPP:
		write( solfd, "rpp    ", 7 );
		xmin = ymin = zmin = 100000000.0;
		xmax = ymax = zmax = -100000000.0;
		for(i=0; i<=21; i+=3) {
			MINMAX(xmin, xmax, rec->s.s_values[i]);
			MINMAX(ymin, ymax, rec->s.s_values[i+1]);
			MINMAX(zmin, zmax, rec->s.s_values[i+2]);
		}
		work[0] = xmin;
		work[1] = xmax;
		work[2] = ymin;
		worc[0] = ymax;
		worc[1] = zmin;
		worc[2] = zmax;
		VMOVE( SV0, work );
		VMOVE( SV1, worc );
		psp( 2, rec );
		break;
	default:
		fprintf( stderr, "Unknown arb (%d).\n", rec->s.s_num );
		exit( 10 );
	}
	return;
}

/*	==== a d d e l l ( )
 *	Process the general ellipsoid.
 */
addell( rec )
register
Record *rec;
{
	int	i;
	float	work[3];
	vect_t	v_work, v_workk;

	/* operate on vertex
	 */
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk);
	htov_move( &(rec->s.s_values[0]), v_work );

	/* rest of vectors
	 */
	for( i = 3; i <= 9; i += 3 ) {
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( &(rec->s.s_values[i]), v_work );
	}

	/* check for ell1 or sph
	 */
	if( fabs( MAGNITUDE(SV1) - MAGNITUDE(SV2) ) < .0001 ) {
		/* vector A == vector B */
		rec->s.s_num = ELL1;
		/* SPH if vector B == vector C also */
		if(fabs(MAGNITUDE(SV2) - MAGNITUDE(SV3)) < .0001)
			rec->s.s_num = SPH;
		if(rec->s.s_num != SPH) {
			/* switch A and C */
			VMOVE(work, SV1);
			VMOVE(SV1, SV3);
			VMOVE(SV3, work);
		}
	}

	else
	if(fabs(MAGNITUDE(SV1) - MAGNITUDE(SV3)) < .0001) {
		/* vector A == vector C */
		rec->s.s_num = ELL1;
		/* switch vector A and vector B */
		VMOVE(work, SV1);
		VMOVE(SV1, SV2);
		VMOVE(SV2, work);
	}

	else
	if(fabs(MAGNITUDE(SV2) - MAGNITUDE(SV3)) < .0001) 
		rec->s.s_num = ELL1;

	else
		rec->s.s_num = GENELL;

	/* print the solid parameters
	 */
	switch( rec->s.s_num ) {
	case GENELL:
		write( solfd, "ellg   ", 7 );
		psp( 4, rec );
		break;
	case ELL1:
		write( solfd, "ell1   ", 7 );
		work[0] = MAGNITUDE( SV2 );
		work[1] = work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case SPH:
		write( solfd, "sph    ", 7 );
		work[0] = MAGNITUDE( SV1 );
		work[1] = work[2] = 0.0;
		VMOVE( SV1, work );
		psp( 2, rec );
		break;
	default:
		fprintf( stderr,
			"Error in type of ellipse (%d).\n", rec->s.s_num );
		exit( 10 );
	}
	return;
}

/*	==== a d d t g c ( )
 *	Process generalized truncated cone.
 */
addtgc( rec )
register
Record *rec;
{
	register int	i;
	float	work[3], axb[3], cxd[3];
	vect_t	v_work, v_workk;
	float	ma, mb, mc, md, maxb, mcxd, mh;

	/* operate on vertex
	 */
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk );
	htov_move( &(rec->s.s_values[0]), v_work );

	for( i = 3; i <= 15; i += 3 ) {
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( &(rec->s.s_values[i]), v_work );
	}

	/* check for tec rec trc rcc
	 */
	rec->s.s_num = TGC;
	VCROSS( axb, SV2, SV3 );
	VCROSS( cxd, SV4, SV5 );
	ma = MAGNITUDE( SV2 );
	mb = MAGNITUDE( SV3 );
	mc = MAGNITUDE( SV4 );
	md = MAGNITUDE( SV5 );
	maxb = MAGNITUDE( axb );
	mcxd = MAGNITUDE( cxd );
	mh = MAGNITUDE( SV1 );

	/* tec if ratio top and bot vectors equal and base parallel to top
	 */
	if(	fabs( (mb/md)-(ma/mc) ) < .0001
	    &&  fabs( fabs(DOT(axb,cxd)) - (maxb*mcxd) ) < .0001
	)	rec->s.s_num = TEC;

	/* check for right cylinder
	 */
	if( fabs( fabs(DOT(SV1,axb)) - (mh*maxb) ) < .0001 ) {
		if( fabs( ma-mb ) < .0001 ) {
			if( fabs( ma-mc ) < .0001 )	rec->s.s_num = RCC;
			else				rec->s.s_num = TRC;
		} else    /* elliptical */
		if( fabs( ma-mc ) < .0001 )		rec->s.s_num = REC;
	}

	/* print the solid parameters
	 */
	switch( rec->s.s_num ) {
	case TGC :
		write( solfd, "tgc    ", 7 );
		work[0] = MAGNITUDE( SV4 );
		work[1] = MAGNITUDE( SV5 );
		work[2] = 0.0;
		VMOVE( SV4, work );
		psp( 5, rec );
		break;
	case RCC :
		write( solfd, "rcc    ", 7 );
		work[0] = MAGNITUDE( SV2 );
		work[1] = work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case TRC :
		write( solfd, "trc    ", 7 );
		work[0] = MAGNITUDE( SV2 );
		work[1] = MAGNITUDE( SV4 );
		work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case TEC :
		write( solfd, "tec    ", 7 );
		work[0] = MAGNITUDE( SV2) / MAGNITUDE( SV4 );
		work[1] = work[2] = 0.0;
		VMOVE( SV4, work );
		psp( 5, rec );
		break;
	case REC :
		write( solfd, "rec    ", 7 );
		psp( 4, rec );
		break;
	default:
		fprintf( stderr,
			"Error in tgc type (%d).\n", rec->s.s_num );
		exit( 10 );
	}
	return;
}

/*	==== a d d a r s ( )
 *	Process triangular surfaced polyhedron - ars.
 */
addars( rec )
register
Record *rec;
{
	char	buf[10];
	int	i, vec;
	int	npt, npts, ncurves, ngrans, granule, totlen;
	float	work[3], vertex[3];
	vect_t	v_work, v_workk;

	ngrans = rec->a.a_curlen;
	totlen = rec->a.a_totlen;
	npts = rec->a.a_n;
	ncurves = rec->a.a_m;

	/* write ars header line in solid table
	 */
	write( solfd, "ars    ", 7 );
	itoa( ncurves, buf, 10 );
	write( solfd, buf, 10 );
	itoa( npts, buf, 10 );
	write( solfd, buf, 10 );
	blank_fill( solfd, 40 );
	write( solfd, rec->a.a_name, strlen( rec->a.a_name ) );
	write( solfd, LF, 1 );

	/* process the data one granule at a time
	 */
	for( granule = 1; granule <= totlen; granule++ ) {
		/* read a granule (ars extension record 'B')
		 */
		readF( objfd, rec, sizeof record );

		/* find number of points in this granule
		 */
		if( rec->b.b_ngranule == ngrans && (npt = npts % 8) != 0 );
		else				  npt = 8;

		/* operate on vertex
		 */
		if( granule == 1 ) {
			vtoh_move( v_workk, &(rec->b.b_values[0]) );
			matXvec( v_work, xform, v_workk );
			htov_move( &(rec->b.b_values[0]), v_work );
			VMOVE( vertex, &(rec->b.b_values[0]) );
			vec = 1;
		} else	vec = 0;

		/* rest of vectors
		 */
		for( i = vec; i < npt; i++, vec++ ) {
			vtoh_move( v_workk, &(rec->b.b_values[vec*3]) );
			matXvec( v_work, notrans, v_workk );
			htov_move( work, v_work );
			VADD2( &(rec->b.b_values[vec*3]), vertex, work );
		}

		/* print the solid parameters
		 */
		parsp( npt, rec );
	}
	return;
}

/*	==== p a r s p ( )
 *	Print npts points of an ars.
 */
parsp(	npts,	 rec )
register
int	npts;
register
Record	*rec;
{
	register int	i, j, k, jk;
	char		bufout[80];

	j = jk = 0;

	itoa( nns+delsol, &bufout[0], 3 );
	for( i = 3; i < 10; i++ )	bufout[i] = ' ';
	strncpy( &bufout[70], "curve ", 6 );
	itoa( rec->b.b_n, &bufout[76], 3 );
	bufout[79] = '\n';

	for( i = 0; i < npts*3; i += 3 ) {
		/* write 3 points
		 */
		for( k = i; k <= i+2; k++ ) {
			++jk;
			ftoascii(	rec->b.b_values[k],
					&bufout[jk*10],
					10,
					4 );
		}
		if( (++j & 01) == 0 ) {
			/* end of line
			 */
			bufout[70] = 'c';
			write( solfd, bufout, 80 );
			jk = 0;
		}
	}

	if( (j & 01) == 1 ) {
		/* finish off line
		 */
		for( k = 40; k < 70; k++ )	bufout[k] = ' ';
		write( solfd, bufout, 80 );
	}
	return;
}

/*	==== m a t _ z e r o ( )
 *	Fill in the matrix "m" with zeros.
 */
mat_zero(	m )
register matp_t m;
{
	register int	i;

	/* Clear everything */
	for( i = 0; i < 16; i++ )	*m++ = 0;
}

/*	m a t _ i d n ( )
 *	Fill in the matrix "m" with an identity matrix.
 */
mat_idn(	m )
register matp_t m;
{
	mat_zero( m );
	m[0] = m[5] = m[10] = m[15] = 1;
}

/*	==== m a t _ c o p y ( )
 *	Copy the matrix "im" into the matrix "om".
 */
mat_copy(	om, im )
register matp_t om, im;
{
	register int	i;

	/* Copy all elements.				*/
	for( i = 0; i< 16; i++ )	*om++ = *im++;
}


/*	==== m a t _ m u l ( )
 *	Multiply matrix "im1" by "im2" and store the result in "om".
 *	NOTE:  This is different from multiplying "im2" by "im1" (most
 *	of the time!)
 */
mat_mul(	om, im1, im2 )
register matp_t om, im1, im2;
{
	register int em1;	/* Element subscript for im1.	*/
	register int em2;	/* Element subscript for im2.	*/
	register int el = 0;	/* Element subscript for om.	*/

	/* For each element in the output matrix... */
	for( ; el < 16; el++ ) { register int i;
		om[el] = 0;		/* Start with zero in output.	*/
		em1 = (el / 4) * 4;	/* Element at rt of row in im1.	*/
		em2 = el % 4;		/* Element at top of col in im2.*/
		for( i = 0; i < 4; i++ ) {
			om[el] += im1[em1] * im2[em2];
			em1++;		/* Next row element in m1.	*/
			em2 += 4;	/* Next column element in m2.	*/
		}
	}
}

/*	==== m a t X v e c ( )
 *	Multiply the vector "iv" by the matrix "im" and store the result
 *	in the vector "ov".
 */
matXvec( op, mp, vp )
register vectp_t op;
register matp_t  mp;
register vectp_t vp;
{
	register int io;	/* Position in output vector.	*/
	register int im = 0;	/* Position in input matrix.	*/
	register int iv;	/* Position in input vector.	*/

	/* fill each element of output vector with
	 */
	for( io = 0; io < 4; io++ ) {
		/* dot product of each row with each element of input vec
		 */
		op[io] = 0.;
		for( iv = 0; iv < 4; iv++ ) op[io] += mp[im++] * vp[iv];
	}
}

/*	==== v t o h _ m o v e ( )
 */
vtoh_move(	h, v)
register float *h,*v;
{
	*h++ = *v++;
	*h++ = *v++;
	*h++ = *v;
	*h++ = 1.;
}

/*	==== h t o v _ m o v e ( )
 */
htov_move(	v, h )
register float *v,*h;
{	static float inv;

	if( h[3] == 1. ) {
		*v++ = *h++;
		*v++ = *h++;
		*v   = *h;
	} else {
		if( h[3] == 0. )	inv = 1.;
		else			inv = 1. / h[3];
		*v++ = *h++ * inv;
		*v++ = *h++ * inv;
		*v = *h * inv;
	}
}

/*	==== p r o m p t ( )
 *	Flush stdout after the printf.
 */
#ifndef prompt
prompt( a0, a1, a2, a3, a4, a5, a6, a7, a8, a9 )
{
	printf( a0, a1, a2, a3, a4, a5, a6, a7, a8, a9 );
	fflush( stdout );
}
#endif

/*	==== r e a d F ( )
 *	Read with error checking and debugging.
 */
readF(	fd, buf, bytes )
int	fd,	 bytes;
char	   *buf;
{ register int	bytesRead;
	if( (bytesRead = read( fd, buf, bytes )) != bytes ) {
		;
#ifdef debug
	fprintf( stderr, "Read failed! Only %d bytes read.\n",
				bytesRead );
#endif
	}
	return( bytesRead );
}
