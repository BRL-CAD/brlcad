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
	Derived from KARDS, written by Keith Applin.

	Generate a COM-GEOM card images suitable for input to gift5
	(also gift(1V)) from a vged(1V) target description.

	There are 3 files generated at a time, the Solid table, Region table,
	and Ident table, which when concatenated in that order, make a
	COM-GEOM deck.  The record formats in the order that they appear, are
	described below, and are strictly column oriented.

	Note that the Solid table begins with a Title and a Control card, the
	rest of the record types appear once for each object, that is, one
	Solid record for each Solid, one Region and one
  	Ident record for each Region as totaled on the Control card, however,
  	the Solid and Region records may span more than 1 card.

----------------------------------------------------------------------------
|File|Record  :             Contents              :       Format           |
|----|---------------------------------------------------------------------|
| 1  |Title   : target_units, title               : a2,3x,a60              |
|    |Control : #_of_solids, #_of_regions         : i5,i5                  |
|    |Solid   : sol_#,sol_type,params.,comment    : i5,a5,6f10.0,a10       |
|    | cont'  : sol_#,parameters,comment          : i5,5x,6f10.0,a10       |
|----|---------------------------------------------------------------------|
| 2  |Region  : reg_#,(op,[sol/reg]_#)*,comment   : i5,1x,9(a2,i5),1x,a10  |
|    | cont'  : reg_#,(op,[sol/reg]_#)*,comment   : i5,1x,9(a2,i5),1x,a10  |
|----|---------------------------------------------------------------------|
| 3  |Flag    : a -1 marks end of region table    : i5                     |
|    |Idents  : reg_#,ident,space,mat,%,descriptn : 5i5,5x,a40             |
|--------------------------------------------------------------------------|

	To compile,
	               $ make 
		   or  $ make install clobber

 */
#include <stdio.h>
#include <signal.h>
#include "externs.h"
#include "./vextern.h"

extern Directory	*diradd();
extern void		blank_fill(), menu();
extern void		quit();
char			regBuffer[BUFSIZ], *regBufPtr;

char			getcmd();
void			prompt();
void			mat_copy(), mat_mul(), matXvec();
void			vtoh_move(), htov_move();
void			eread(), ewrite();

#define endRegion( buff ) \
	     	(void) sprintf( regBufPtr, "%s\n", buff ); \
	    	ewrite( regfd, regBuffer, (unsigned) strlen( regBuffer ) ); \
	    	regBufPtr = regBuffer;

#define putSpaces( s, xx ) \
		{	register int ii; \
		for( ii = 0; ii < (xx); ii++ ) \
			*s++ = ' '; \
		}

/*	m a i n ( )							*/
main( argc, argv )
char	*argv[];
	{
	setbuf( stdout, emalloc( BUFSIZ ) );

	if( ! parsArg( argc, argv ) )
		{
		menu( usage );
		exit( 1 );
		}

	builddir();	/* Build directory from object file.	 	*/
	toc();		/* Build table of contents from directory.	*/

	/*      C o m m a n d   I n t e r p r e t e r			*/
	(void) setjmp( env );/* Point of re-entry from aborted command.	*/
	prompt( CMD_PROMPT );
	while( 1 )
		{
		/* Return to default interrupt handler after every command,
		 allows exit from program only while command interpreter
		 is waiting for input from keyboard.
		 */
		(void) signal( SIGINT, quit );

		switch( getcmd( arg_list, 0 ) )
			{
		case DECK :
			regBufPtr = regBuffer;
			deck( arg_list[1] );
			break;
		case ERASE :
			while( curr_ct > 0 )
				free( curr_list[--curr_ct] );
			break;
		case INSERT :
			if( arg_list[1] == 0 )
				{
				prompt( "enter object[s] to insert: " );
				(void) getcmd( arg_list, arg_ct );
				}
			(void) insert( arg_list, arg_ct );
			break;
		case LIST :
			{	register int	i;
			if( arg_list[1] == 0 )
				{
				(void) col_prt( curr_list, curr_ct );
				break;
				}
			for( tmp_ct = 0, i = 0; i < curr_ct; i++ )
				if( match( arg_list[1], curr_list[i] ) )
					tmp_list[tmp_ct++] = curr_list[i];
			(void) col_prt( tmp_list, tmp_ct );
			break;
			}
		case MENU :
			menu( cmd );
			prompt( PROMPT );
			continue;
		case NUMBER :
			if( arg_list[1] == 0 )
				{
				prompt( "enter number of 1st solid: " );
				(void) getcmd( arg_list, arg_ct );
				prompt( "enter number of 1st region: " );
				(void) getcmd( arg_list, arg_ct );
				}
			if( arg_list[1] )
				delsol = atoi( arg_list[1] ) - 1;
			if( arg_list[2] )
				delreg = atoi( arg_list[2] ) - 1;
			break;
		case REMOVE :
			if( arg_list[1] == 0 )
				{
				prompt( "enter object[s] to remove: " );
				(void) getcmd( arg_list, arg_ct );
				}
			(void) delete( arg_list );
			break;
		case RETURN :
			prompt( PROMPT );
			continue;
		case SHELL :
			if( arg_list[1] == 0 )
				{
				prompt( "enter shell command: " );
				(void) getcmd( arg_list, arg_ct );
				}
			(void) shell( arg_list );
			break;
		case SORT_TOC :
			sort( toc_list, ndir );
			break;
		case TOC :
			list_toc( arg_list );
			break;
		case EOF :
		case QUIT :
			(void) printf( "quitting...\n" );
			exit( 0 );
		UNKNOWN :
			(void) printf( "Invalid command\n" );
			prompt( PROMPT );
			continue;
			}
		prompt( CMD_PROMPT );		
		}
	}

/*	c g o b j ( )
	Build deck for object pointed to by 'dirp'.
 */
cgobj( dirp, pathpos, old_xlate )
register Directory	*dirp;
int			pathpos;
matp_t			old_xlate;
	{
	register struct member	*mp;
	register char		*bp;
	Record			rec;
	Directory		*nextdirp, *tdirp;
	static Directory	*path[MAXPATH];
	mat_t			new_xlate;
	long			savepos, RgOffset;
	int			nparts;
	int			i, j;
	int			length;
	int			dchar = 0;
	int			nnt;
	static char		buf[MAXPATH*NAMESIZE];
	char			ars_name[NAMESIZE];

	/* Limit tree hierarchy to MAXPATH levels.			*/
	if( pathpos >= MAXPATH )
		{
		(void) fprintf( stderr, "Nesting exceeds %d levels\n", MAXPATH );
		for( i = 0; i < MAXPATH; i++ )
			(void) fprintf( stderr, "/%s", path[i]->d_namep );
		(void) fprintf( stderr, "\n" );
		return;
		}
	savepos = lseek( objfd, 0L, 1 );
	(void) lseek( objfd, dirp->d_addr, 0 );
	eread( objfd, (char *) &rec, sizeof rec );

	if( rec.u_id == ID_COMB )
		{ /* We have a group.					*/
		if( regflag > 0 )
			{
			/* Record is part of a region.			*/
			if( rec.c.c_flags != 'R' )
				{
				(void) fprintf( stderr,
	"Illegal combination, group '%s' is a member of a region (%s)",
						rec.c.c_name,
						buff
						);
				return;
				}
			if( operate == UNION )
				{
				(void) fprintf( stderr,
					"Region: %s is member of ",
					rec.c.c_name );
				(void) fprintf( stderr,
					"region %s with OR operation.\n",
					buff );
				return;
				}

			/* Check for end of line in region table.	*/
			if(	(isave % 9 ==  1 && isave >  1)
			    ||	(isave % 9 == -1 && isave < -1)
				)
				{
				endRegion( buff );
				putSpaces( regBufPtr, 6 );
				}
			(void) sprintf( regBufPtr, "rg%c", operate );
			regBufPtr += 3;
			RgOffset = (long)(regBufPtr - regBuffer);

			/* Check if this region is in desc yet		*/
			(void) lseek( rd_rrfd, 0L, 0 );
			for( j = 1; j <= nnr; j++ )
				{
				eread( rd_rrfd, name, (unsigned) NAMESIZE );
				if( strcmp( name, rec.c.c_name ) == 0 )
					{ /* Region is #j.		*/
					(void) sprintf( regBufPtr, "%4d", j+delreg );
					regBufPtr += 4;
					break;
					}
				}
			if( j > nnr )
				{   /* region not in desc yet */
				numrr++;
				if( numrr > MAXRR )
					{
					(void) fprintf( stderr,
						"More than %d regions.\n",
						MAXRR
						);
					exit( 10 );
					}

				/* Add to list of regions to look up.	*/
				findrr[numrr].rr_pos = lseek(	regfd,
								0L,
								1
								) + RgOffset;
				for( i = 0; i < NAMESIZE; i++ )
					findrr[numrr].rr_name[i] =
						   rec.c.c_name[i];
				putSpaces( regBufPtr, 4 );
				}
			/* Check for end of this region.		*/
			if( isave < 0 )
				{	int	n;
				isave = -isave;
				regflag = 0;
				n = 69 - (regBufPtr - regBuffer);
				putSpaces( regBufPtr, n );
				endRegion( buff );
				}
			(void) lseek( objfd, savepos, 0);
			return;
			}

		regflag = 0;
		nparts = rec.c.c_length;
		if( rec.c.c_flags == 'R')
			{ /* Record is region but not member of a region.	*/
			regflag = 1;
			nnr++;
			/* Dummy region.				*/
			if( nparts == 0 )
				regflag = 0;
			/* Save the region name.			*/
			(void) strncpy( buff, rec.c.c_name, NAMESIZE );
			/* Start new region.				*/
			(void) sprintf( regBufPtr, "%5d ", nnr+delreg );
			regBufPtr += 6;
			/* Check for dummy region.			*/
			if( nparts == 0 )
				{	int	n;
				n = 69 - (regBufPtr - regBuffer);
				putSpaces( regBufPtr, n );
				endRegion( "" );
				regflag = 0;
				}

			/* Add region to list of regions in desc.	*/
			(void) lseek( rrfd, 0L, 2 );
			ewrite( rrfd, rec.c.c_name, NAMESIZE );
			/* Check for any OR.				*/
			orflag = 0;
			if( nparts > 1 )
				{ /* First OR doesn't count, throw away
					first member.		 	*/
				eread( objfd, (char *) &rec, sizeof rec );
				for( i = 2; i <= nparts; i++ )
					{
					eread( objfd, (char *) &rec, sizeof rec );
					if( rec.M.m_relation == UNION )
						{
						orflag = 1;
						break;
						}
					}
				(void) lseek( objfd, dirp->d_addr, 0 );
				eread( objfd, (char *) &rec, sizeof rec );
				}
			/* Write region ident table.			*/
			itoa( nnr+delreg, buf, 5 );
			itoa( rec.c.c_regionid,	&buf[5], 5 );
			itoa( rec.c.c_aircode, &buf[10], 5 );
/* + */			itoa( rec.c.c_material, &buf[15], 5 );
/* + */			itoa( rec.c.c_los, &buf[20], 5 );
			ewrite( ridfd, buf, 25 );
			blank_fill( ridfd, 5 );
			bp = buf;
			length = strlen( rec.c.c_name );
			for( j = 0; j < pathpos; j++ )
				{
				(void) strncpy(	bp,
						path[j]->d_namep,
						strlen( path[j]->d_namep )
						);
				bp += strlen( path[j]->d_namep );
				*bp++ = '/';
				*bp = '\0';
				}
			length += bp - buf;
			if( length > 50 )
				{
				bp = buf + (length - 50);
				*bp = '*';
				ewrite(	ridfd,
					bp,
					(unsigned)(50 - strlen( rec.c.c_name ))
					);
				}
			else
				ewrite( ridfd, buf, (unsigned)(bp - buf) );
			ewrite(	ridfd,
				rec.c.c_name,
				(unsigned) strlen( rec.c.c_name )
				);
			ewrite( ridfd, LF, 1 );
			(void) printf( "%4d:", nnr+delreg );
			for( j = 0; j < pathpos; j++ )
				(void) printf( "/%s", path[j]->d_namep );
			(void) printf( "/%s\n", rec.c.c_name );
			}
		isave = 0;
		for( i = 1; i <= nparts; i++ )
			{
			if( ++isave == nparts )
				isave = -isave;
			eread( objfd, (char *) &rec, sizeof rec );
			mp = &rec.M;
 
			/* Save this operation.				*/
			operate = mp->m_relation;
 			path[pathpos] = dirp;
			if(	(nextdirp =
				lookup( mp->m_instname, NOISY )) == DIR_NULL
				)
				continue;
#if defined( BRANCH_NAMING )
			if( mp->m_brname[0] != 0 )
				{
				/* Create an alias.  First step towards
					full branch naming.  User is
					responsible for his branch names
				 	being unique.
				 */
				if(	(tdirp =
					lookup( mp->m_brname, NOISY ))
				     !=	DIR_NULL
					)
					/* Use existing alias.		*/
					nextdirp = tdirp;
				else
					nextdirp = diradd(mp->m_brname,
							nextdirp->d_addr );
				}
#endif
			mat_mul( new_xlate, old_xlate, mp->m_mat );

			/* Recursive call.				*/
			cgobj( nextdirp, pathpos+1, new_xlate );
			}
		(void) lseek( objfd, savepos, 0 );
		return;
		}

	/* N O T  a  C O M B I N A T I O N  record.			*/
	if( rec.u_id != ID_SOLID && rec.u_id != ID_ARS_A )
		{
		(void) fprintf( stderr,
				"Bad input: should have a 'S' or 'A' record, " );
		(void) fprintf( stderr,
				"but have '%c'.\n", rec.u_id );
		exit( 10 );
		}

	/* Now have proceeded down branch to a solid

		if regflag = 1  add this solid to present region
		if regflag = 0  solid not defined as part of a region
				make new region if scale != 0 
	
		if orflag = 1   this region has or's
		if orflag = 0   no
	*/
	if( old_xlate[15] < EPSILON )
		{ /* Do not add solid.		*/
		(void) lseek( objfd, savepos, 0 );
		return;
		}

	/* Fill ident struct.						*/
	mat_copy( d_ident.i_mat, old_xlate );
	(void) strncpy( d_ident.i_name, rec.s.s_name, NAMESIZE );
	(void) strncpy( ars_name, rec.s.s_name, NAMESIZE );
	
	/* Calculate first look discriminator for this solid.		*/
	dchar = 0;
	for( i = 0; i < NAMESIZE; i++ )
		{
		if( rec.s.s_name[i] == 0 )
			break;
		dchar += (rec.s.s_name[i] << (i&7));
		}

	/* Quick check if solid already in solid table.			*/
	nnt = 0;
	for( i = 0; i < nns; i++ )
		{
		if( dchar == discr[i] )
			{ /* Quick look match - check further.		*/
			(void) lseek( rd_idfd, (long)(i * sizeof d_ident), 0 );
			eread( rd_idfd, (char *) &idbuf, sizeof d_ident );
			d_ident.i_index = i + 1;
			if( check( (char *) &d_ident, (char *) &idbuf ) == 1 )
				{
				/* Really is an old solid.		*/
				nnt = i + 1;
				goto notnew;
				}
			/* False alarm - keep looking for quick look
				matches.
			 */
			}
		}

	/* New solid.							*/
	discr[nns] = dchar;
	nns++;
	d_ident.i_index = nns;

	if( nns > MAXSOL )
		{
		(void) fprintf( stderr,
				"\nNumber of solids (%d) greater than max (%d).\n",
				nns, MAXSOL
				);
		exit( 10 );
		}

	/* Write ident struct at end of idfd file.			*/
	(void) lseek( idfd, 0L, 2 );
	ewrite( idfd, (char *) &d_ident, sizeof d_ident );
	nnt = nns;

	/* Process this solid.						*/
	mat_copy( xform, old_xlate );
	mat_copy( notrans, xform );

	/* Notrans = homogeneous matrix with a zero translation vector.	*/
	notrans[3] = notrans[7] = notrans[11] = 0.0;

	/* Write solid #.						*/
	itoa( nnt+delsol, buf, 5 );
	ewrite( solfd, buf, 5 );

	/* Process appropriate solid type.				*/
	switch( rec.s.s_type )
		{
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
		(void) fprintf( stderr,
				"Solid type (%d) unknown.\n", rec.s.s_type
				);
		exit( 10 );
		}

notnew:	/* Sent here if solid already in solid table.			*/
	/* Finished with solid.						*/
	/* Put solid in present region if regflag == 1.			*/
	if( regflag == 1 )
		{
		/* isave = number of this solid in this region, if
			negative then is the last solid in this region.
		 */
		if(	(isave % 9 ==  1 && isave >  1)
		    ||	(isave % 9 == -1 && isave < -1)
			)
			{	int	n;
			/* New line.					*/
		    	n = 69 - (regBufPtr - regBuffer);
		    	putSpaces( regBufPtr, n );
		    	endRegion( buff );
			putSpaces( regBufPtr, 6 );
			}
		nnt += delsol;
		if( operate == '-' )	nnt = -nnt;
		if( orflag == 1 )
			{
			if( operate == UNION || isave == 1 )
				{
				(void) sprintf( regBufPtr, "or" );
				regBufPtr += 2;
				}
			else
				putSpaces( regBufPtr, 2 );
			}
		else
			putSpaces( regBufPtr, 2 );
		(void) sprintf( regBufPtr, "%5d", nnt );
		regBufPtr += 5;
		if( nnt < 0 )	nnt = -nnt;
		nnt -= delsol;
		if( isave < 0 )
			{	int	n; /* end this region */
			isave = -isave;
			regflag = 0;
			n = 69 - (regBufPtr - regBuffer);
			putSpaces( regBufPtr, n );
			endRegion( buff );
			}
		}
	else
	if( old_xlate[15] > EPSILON )
		{
		/* Solid not part of a region, make solid into region
			if scale > 0
		 */
		++nnr;
		(void) sprintf( regBufPtr, "%5d%8d", nnr+delreg, nnt+delsol );
		regBufPtr += 13;
		putSpaces( regBufPtr, 56 );
		(void) sprintf( regBufPtr, rec.s.s_name );
		regBufPtr += strlen( rec.s.s_name );
		itoa( nnr+delreg, buf, 5 );
		ewrite( ridfd, buf, 5 );

		/* Values for item, space, material and percentage are
			meaningless at this point.
		 */
		ewrite( ridfd, "    0", 5 );
		ewrite( ridfd, "    0", 5 );
		ewrite( ridfd, "    0", 5 );
		ewrite( ridfd, "    0", 5 );
		blank_fill( ridfd, 5 );
		(void) printf( "%4d:", nnr+delreg );
		bp = buf;
		length = strlen( rec.s.s_name );
		for( i = 0; i < pathpos; i++ )
			{
			(void) strncpy(	bp,
					path[i]->d_namep,
					strlen( path[i]->d_namep )
					);
			bp += strlen( path[i]->d_namep );
			*bp++ = '/';
			}
		length += bp - buf;
		if( length > 50 )
			{
			bp = buf + (length - 50);
			*bp = '*';
			ewrite(	ridfd,
				bp,
				(unsigned) (50 - strlen( rec.s.s_name ))
				);
			}
		else
			ewrite( ridfd, buf, (unsigned)(bp - buf) );

		if( rec.u_id == ID_ARS_B )
			{ /* Ars extension record.	*/
			(void) printf( "/%s\n", ars_name );
			ewrite(	ridfd,
				ars_name,
				(unsigned) strlen( ars_name )
				);
			}
		else
			{
			(void) printf( "/%s\n", rec.s.s_name );
			ewrite(	ridfd,
				rec.s.s_name,
				(unsigned) strlen( rec.s.s_name )
				);
			}
		ewrite( ridfd, LF, 1 );
		{	int	n;
		n = 69 - (regBufPtr - regBuffer);
		putSpaces( regBufPtr, n );
		endRegion( "" );
		}
		}
	if( isave < 0 )
		regflag = 0;
	(void) lseek( objfd, savepos, 0 );
	return;
	}


void
swap_vec( v1, v2, work )
register float	*v1, *v2, *work;
	{
	VMOVE( work, v1 );
	VMOVE( v1, v2 );
	VMOVE( v2, work );
	return;
	}

void
swap_dbl( d1, d2 )
register double	*d1, *d2;
	{	double	t;
	t = *d1;
	*d1 = *d2;
	*d2 = t;
	return;
	}

/*	a d d t o r ( )
	Process torus.
 */
addtor( rec )
register Record	*rec;
	{	register int	i;
		float	work[3];
		double	rr1, rr2;
		hvect_t	v_work, v_workk;

	ewrite( solfd, "tor  ", 5 );

	/* Operate on vertex.						*/
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk );
	htov_move( &(rec->s.s_values[0]), v_work );
	VMOVE( &(rec->s.s_values[0]) , v_work );

	/* Rest of vectors.						*/
	for( i = 3; i <= 21; i += 3 )
		{
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( &(rec->s.s_values[i]), v_work );
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

/*	a d d a r b ( )
	Process generalized arb.
 */
addarb( rec )
register Record	*rec;
	{	register int	i;
		float	work[3], worc[3];
		hvect_t	v_work, v_workk;
		double	xmin, xmax, ymin, ymax, zmin, zmax;
		int	univecs[8], samevecs[11];
	
	/* Operate on vertex.						*/
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk );
	htov_move( &(rec->s.s_values[0]), v_work );

	/* Rest of vectors.						*/
	for( i = 3; i <= 21; i += 3 )
		{
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
	(void) redoarb( rec, univecs, samevecs, i );
	rec->s.s_cgtype *= -1;
	
	/* Print the solid parameters.					*/
	switch( rec->s.s_cgtype )
		{
	case ARB8 :
		ewrite( solfd, "arb8 ", 5 );
		psp( 8, rec );
		break;
	case ARB7 :
		ewrite( solfd, "arb7 ", 5 );
		psp( 7, rec );
		break;
	case ARB6 :
		ewrite( solfd, "arb6 ", 5 );
		VMOVE( SV5, SV6 );
		psp( 6, rec );
		break;
	case ARB5 :
		ewrite( solfd, "arb5 ", 5 );
		psp( 5, rec );
		break;
	case ARB4 :
		ewrite( solfd, "arb4 ", 5 );
		VMOVE( SV3, SV4 );
		psp( 4, rec );
		break;
	case RAW :
		ewrite( solfd, "raw  ", 5 );
		VSUB2( work, SV1, SV0 );
		VSUB2( SV1, SV3, SV0);		/* H */
		VMOVE( SV2, work);		/* W */
		VSUB2( SV3, SV4, SV0);		/* D */
		psp( 4, rec );
		break;
	case BOX :
		ewrite( solfd, "box  ", 5 );
		VSUB2( work, SV1, SV0 );
		VSUB2( SV1, SV3, SV0);		/* H */
		VMOVE( SV2, work);		/* W */
		VSUB2( SV3, SV4, SV0);		/* D */
		psp( 4, rec );
		break;
	case RPP :
		ewrite( solfd, "rpp  ", 5 );
		xmin = ymin = zmin = 100000000.0;
		xmax = ymax = zmax = -100000000.0;
		for( i = 0; i <= 21; i += 3 )
			{
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
	default :
		(void) fprintf( stderr, "Unknown arb (%d).\n", rec->s.s_cgtype );
		exit( 10 );
		}
	return;
	}

/*	a d d e l l ( )
	Process the general ellipsoid.
 */
addell( rec )
register Record	*rec;
	{	register int	i;
		float	work[3];
		hvect_t	v_work, v_workk;
		double	ma, mb, mc;
	
	/* Operate on vertex.						*/
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk);
	htov_move( &(rec->s.s_values[0]), v_work );

	/* Rest of vectors.						*/
	for( i = 3; i <= 9; i += 3 )
		{
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( &(rec->s.s_values[i]), v_work );
		}

	/* Check for ell1 or sph.					*/
	ma = MAGNITUDE( SV1 );
	mb = MAGNITUDE( SV2 );
	mc = MAGNITUDE( SV3 );
	if( fabs( ma-mb ) < CONV_EPSILON )
		{ /* vector A == vector B */
		rec->s.s_cgtype = ELL1;
		/* SPH if vector B == vector C also */
		if( fabs( mb-mc ) < CONV_EPSILON )
			rec->s.s_cgtype = SPH;
		else	/* switch A and C */
			{
			swap_vec( SV1, SV3, work );
			swap_dbl( &ma, &mc );
			}
		}
	else
	if( fabs( ma-mc ) < CONV_EPSILON )
		{ /* vector A == vector C */
		rec->s.s_cgtype = ELL1;
		/* switch vector A and vector B */
		swap_vec( SV1, SV2, work );
		swap_dbl( &ma, &mb );
		}
	else
	if( fabs( mb-mc ) < CONV_EPSILON ) 
		rec->s.s_cgtype = ELL1;
	else
		rec->s.s_cgtype = GENELL;

	/* Print the solid parameters.					*/
	switch( rec->s.s_cgtype )
		{
	case GENELL :
		ewrite( solfd, "ellg ", 5 );
		psp( 4, rec );
		break;
	case ELL1 :
		ewrite( solfd, "ell1 ", 5 );
		work[0] = mb;
		work[1] = work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case SPH :
		ewrite( solfd, "sph  ", 5 );
		work[0] = ma;
		work[1] = work[2] = 0.0;
		VMOVE( SV1, work );
		psp( 2, rec );
		break;
	default :
		(void) fprintf( stderr,
				"Error in type of ellipse (%d).\n",
				rec->s.s_cgtype
				);
		exit( 10 );
		}
	return;
	}

/*	a d d t g c ( )
	Process generalized truncated cone.
 */
addtgc( rec )
register Record *rec;
	{	register int	i;
		float	work[3], axb[3], cxd[3];
		hvect_t	v_work, v_workk;
		double	ma, mb, mc, md, maxb, mcxd, mh;

	/* Operate on vertex.						*/
	vtoh_move( v_workk, &(rec->s.s_values[0]) );
	matXvec( v_work, xform, v_workk );
	htov_move( &(rec->s.s_values[0]), v_work );

	for( i = 3; i <= 15; i += 3 )
		{
		vtoh_move( v_workk, &(rec->s.s_values[i]) );
		matXvec( v_work, notrans, v_workk );
		htov_move( &(rec->s.s_values[i]), v_work );
		}

	/* Check for tec rec trc rcc.					*/
	rec->s.s_cgtype = TGC;
	VCROSS( axb, SV2, SV3 );
	VCROSS( cxd, SV4, SV5 );
	ma = MAGNITUDE( SV2 );
	mb = MAGNITUDE( SV3 );
	mc = MAGNITUDE( SV4 );
	md = MAGNITUDE( SV5 );
	maxb = MAGNITUDE( axb );
	mcxd = MAGNITUDE( cxd );
	mh = MAGNITUDE( SV1 );

	/* Tec if ratio top and bot vectors equal and base parallel to top.
	 */
	if( mc == 0.0 )
		{
		(void) fprintf( stderr,
				"Error in TGC, C vector has zero magnitude!\n"
				);
		return;
		}
	if( md == 0.0 )
		{
		(void) fprintf( stderr,
				"Error in TGC, D vector has zero magnitude!\n"
				);
		return;
		}
	if(	fabs( (mb/md)-(ma/mc) ) < CONV_EPSILON
	    &&  fabs( fabs(VDOT(axb,cxd)) - (maxb*mcxd) ) < CONV_EPSILON
		)
		rec->s.s_cgtype = TEC;

	/* Check for right cylinder.					*/
	if( fabs( fabs(VDOT(SV1,axb)) - (mh*maxb) ) < CONV_EPSILON )
		{
		if( fabs( ma-mb ) < CONV_EPSILON )
			{
			if( fabs( ma-mc ) < CONV_EPSILON )
				rec->s.s_cgtype = RCC;
			else
				rec->s.s_cgtype = TRC;
			}
		else    /* elliptical */
		if( fabs( ma-mc ) < CONV_EPSILON )
			rec->s.s_cgtype = REC;
		}

	/* Insure that magnitude of A is greater than B, and magnitude of 
		C is greater than D for the GIFT code (boy, is THIS a shame).
		This need only be done for the elliptical REC and TEC types.
	 */
	if( rec->s.s_cgtype == REC || rec->s.s_cgtype == TEC )
		{
		if( ma < mb )
			{
			swap_vec( SV2, SV3, work );
			swap_dbl( &ma, &mb );
			swap_vec( SV4, SV5, work );
			swap_dbl( &mc, &md );
			}
		}

	/* Print the solid parameters.					*/
	switch( rec->s.s_cgtype )
		{
	case TGC :
		ewrite( solfd, "tgc  ", 5 );
		work[0] = mc;
		work[1] = md;
		work[2] = 0.0;
		VMOVE( SV4, work );
		psp( 5, rec );
		break;
	case RCC :
		ewrite( solfd, "rcc  ", 5 );
		work[0] = ma;
		work[1] = work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case TRC :
		ewrite( solfd, "trc  ", 5 );
		work[0] = ma;
		work[1] = mc;
		work[2] = 0.0;
		VMOVE( SV2, work );
		psp( 3, rec );
		break;
	case TEC :
		ewrite( solfd, "tec  ", 5 );
		/* This backwards conversion is to counteract an unnecessary
			conversion of this ratio in 'psp()'.  Sorry about
			that.
		 */
		work[0] = (ma / mc) / unit_conversion;
		work[1] = work[2] = 0.0;
		VMOVE( SV4, work );
		psp( 5, rec );
		break;
	case REC :
		ewrite( solfd, "rec  ", 5 );
		psp( 4, rec );
		break;
	default :
		(void) fprintf( stderr,
				"Error in tgc type (%d).\n",
				rec->s.s_cgtype
				);
		exit( 10 );
		}
	return;
}

/*	a d d a r s ( )
	Process triangular surfaced polyhedron - ars.
 */
addars( rec )
register Record *rec;
	{	char	buf[10];
		register int	i, vec;
		int	npt, npts, ncurves, ngrans, granule, totlen;
		float	work[3], vertex[3];
		hvect_t	v_work, v_workk;

	ngrans = rec->a.a_curlen;
	totlen = rec->a.a_totlen;
	npts = rec->a.a_n;
	ncurves = rec->a.a_m;

	/* Write ars header line in solid table.			*/
	ewrite( solfd, "ars  ", 5 );
	itoa( ncurves, buf, 10 );
	ewrite( solfd, buf, 10 );
	itoa( npts, buf, 10 );
	ewrite( solfd, buf, 10 );
	blank_fill( solfd, 40 );
	ewrite( solfd, rec->a.a_name, (unsigned) strlen( rec->a.a_name ) );
	ewrite( solfd, LF, 1 );

	/* Process the data one granule at a time.			*/
	for( granule = 1; granule <= totlen; granule++ )
		{
		/* Read a granule (ars extension record 'B').		*/
		eread( objfd, (char *) rec, sizeof record );
		/* Find number of points in this granule.		*/
		if( rec->b.b_ngranule == ngrans && (npt = npts % 8) != 0 )
			;
		else
			npt = 8;
		/* Operate on vertex.					*/
		if( granule == 1 )
			{
			vtoh_move( v_workk, &(rec->b.b_values[0]) );
			matXvec( v_work, xform, v_workk );
			htov_move( &(rec->b.b_values[0]), v_work );
			VMOVE( vertex, &(rec->b.b_values[0]) );
			vec = 1;
			}
		else
			vec = 0;

		/* Rest of vectors.					*/
		for( i = vec; i < npt; i++, vec++ )
			{
			vtoh_move( v_workk, &(rec->b.b_values[vec*3]) );
			matXvec( v_work, notrans, v_workk );
			htov_move( work, v_work );
			VADD2( &(rec->b.b_values[vec*3]), vertex, work );
			}

		/* Print the solid parameters.				*/
		parsp( npt, rec );
		}
	return;
	}

#define MAX_PSP	60
/*	p s p ( )
	Print solid parameters  -  npts points or vectors.
 */
psp( npts, rec )
register int	npts;
register Record *rec;
	{	register int	i, j, k, jk;
		char		buf[MAX_PSP+1];
	j = jk = 0;
	for( i = 0; i < npts*3; i += 3 )
		{ /* Write 3 points.					*/
		for( k = i; k <= i+2; k++ )
			{
			rec->s.s_values[k] *= unit_conversion;
			ftoascii( rec->s.s_values[k], &buf[jk*10], 10, 4 );
			++jk;
			}

		if( (++j & 01) == 0 )
			{ /* End of line.				*/
			ewrite( solfd, buf, MAX_PSP );
			jk = 0;
			ewrite(	solfd,
				rec->s.s_name,
				(unsigned) strlen( rec->s.s_name )
				);
			ewrite( solfd, LF, 1 );
			if( i != (npts-1)*3 )
				{   /* new line */
				itoa( nns+delsol, buf, 5 );
				ewrite( solfd, buf, 5 );
				blank_fill( solfd, 5 );
				}
			}
		}	
	if( (j & 01) == 1 )
		{ /* Finish off rest of line.				*/
		for( k = 30; k <= MAX_PSP; k++ )
			buf[k] = ' ';
		ewrite( solfd, buf, MAX_PSP );
		ewrite(	solfd,
			rec->s.s_name,
			(unsigned) strlen( rec->s.s_name )
			);
		ewrite( solfd, LF, 1 );
		}
	return;
	}

/*	p a r s p ( )
	Print npts points of an ars.
 */
parsp( npts, rec )
register int	npts;
register Record	*rec;
	{ 	register int	i, j, k, jk;
		char		bufout[80];

	j = jk = 0;

	itoa( nns+delsol, &bufout[0], 5 );
	for( i = 5; i < 10; i++ )
		bufout[i] = ' ';
	(void) strncpy( &bufout[70], "curve ", 6 );
	itoa( rec->b.b_n, &bufout[76], 3 );
	bufout[79] = '\n';

	for( i = 0; i < npts*3; i += 3 )
		{ /* Write 3 points.  */
		for( k = i; k <= i+2; k++ )
			{
			++jk;
			rec->b.b_values[k] *= unit_conversion;
			ftoascii(	rec->b.b_values[k],
					&bufout[jk*10],
					10,
					4
					);
			}
		if( (++j & 01) == 0 )
			{ /* End of line. */
			bufout[70] = 'c';
			ewrite( solfd, bufout, 80 );
			jk = 0;
			}
		}
	if( (j & 01) == 1 )
		{ /* Finish off line. */
		for( k = 40; k < 70; k++ )
			bufout[k] = ' ';
		ewrite( solfd, bufout, 80 );
		}
	return;
	}

/*	m a t _ z e r o ( )
	Fill in the matrix "m" with zeros.
 */
void
mat_zero( m )
register matp_t	m;
	{	register int	i;
	for( i = 0; i < 16; i++ )
		*m++ = 0;
	return;
	}

/*	m a t _ i d n ( )
	Fill in the matrix "m" with an identity matrix.
 */
void
mat_idn( m )
register matp_t m;
	{
	mat_zero( m );
	m[0] = m[5] = m[10] = m[15] = 1;
	return;
	}

/*	m a t _ c o p y ( )
	Copy the matrix "im" into the matrix "om".
 */
void
mat_copy( om, im )
register matp_t om, im;
	{	register int	i;
	for( i = 0; i < 16; i++ )
		*om++ = *im++;
	return;
	}

/*	m a t _ m u l ( )
	Multiply matrix "im1" by "im2" and store the result in "om".
	NOTE:  This is different from multiplying "im2" by "im1" (most
	of the time!)
 */
void
mat_mul( om, im1, im2 )
register matp_t	om, im1, im2;
	{ 	register int em1;	/* Element subscript for im1.	*/
		register int em2;	/* Element subscript for im2.	*/
		register int el = 0;	/* Element subscript for om.	*/
	/* For each element in the output matrix... */
	for( ; el < 16; el++ )
		{	register int i;
		om[el] = 0;		/* Start with zero in output.	*/
		em1 = (el / 4) * 4;	/* Element at rt of row in im1.	*/
		em2 = el % 4;		/* Element at top of col in im2.*/
		for( i = 0; i < 4; i++ )
			{
			om[el] += im1[em1] * im2[em2];
			em1++;		/* Next row element in m1.	*/
			em2 += 4;	/* Next column element in m2.	*/
			}
		}
	return;
	}
/*	m a t X v e c ( )
	Multiply the vector "iv" by the matrix "im" and store the result
	in the vector "ov".
 */
void
matXvec( op, mp, vp )
register vectp_t	op;
register matp_t		mp;
register vectp_t	vp;
	{	register int io;	/* Position in output vector.	*/
		register int im = 0;	/* Position in input matrix.	*/
		register int iv;	/* Position in input vector.	*/
	/* Fill each element of output vector with.			*/
	for( io = 0; io < 4; io++ )
		{ /* Dot prod. of each row w/ each element of input vec.*/
		op[io] = 0.;
		for( iv = 0; iv < 4; iv++ )
			op[io] += mp[im++] * vp[iv];
		}
	return;
	}

/*	v t o h _ m o v e ( )						*/
void
vtoh_move( h, v)
register float	*h, *v;
	{
	*h++ = *v++;
	*h++ = *v++;
	*h++ = *v;
	*h++ = 1.0;
	return;
	}

/*	h t o v _ m o v e ( )						*/
void
htov_move( v, h )
register float	*v, *h;
	{	static double	inv;
	if( h[3] == 1.0 )
		{
		*v++ = *h++;
		*v++ = *h++;
		*v   = *h;
		}
	else
		{
		if( h[3] == 0.0 )
			inv = 1.0;
		else
			inv = 1.0 / h[3];
		*v++ = *h++ * inv;
		*v++ = *h++ * inv;
		*v = *h * inv;
		}
	return;
	}

#include <varargs.h>
/*	p r o m p t ( )							*/
/*VARARGS*/
void
prompt( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
	(void) _doprnt( fmt, ap, stdout );
	(void) fflush( stdout );
	va_end( ap );
	return;
	}

/*	e r e a d ( )
	Read with error checking.
 */
void
eread( fd, buf, bytes )
int		fd;
char		*buf;
unsigned	bytes;
	{	int	bytes_read;
	if(	(bytes_read = read( fd, buf, bytes )) != (int) bytes
	    &&	bytes_read != 0
		)
		(void) fprintf( stderr,
				"ERROR: Read of %d bytes returned %d\n",
				bytes,
				bytes_read
				);
	return;
	}

/*	e w r i t e
	Write with error checking.
 */
void
ewrite( fd, buf, bytes )
int		fd;
char		*buf;
unsigned	bytes;
	{	int	bytes_written;
	if( (bytes_written = write( fd, buf, bytes )) != (int) bytes )
		(void) fprintf( stderr,
				"ERROR: Write of %d bytes returned %d\n",
				bytes,
				bytes_written
				);
	return;
	}

pr_dir( dirp )
Directory	*dirp;
	{
	(void) printf(	"dirp(0x%x)->d_namep=%s d_addr=%ld\n",
			dirp, dirp->d_namep, dirp->d_addr
			);
	(void) fflush( stdout );
	return	1;
	}

char *
emalloc( size )
int	size;
	{
		char		*ptr;
	if( (ptr = (char *)malloc( (unsigned) size )) == NULL )
		{
		(void) fprintf( stderr, "Malloc() failed!\n" );
		exit( 1 );
		}
	return	ptr;
	}
