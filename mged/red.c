/*
 *			R E D . C
 *
 *	These routines allow editing of a combination using the text editor
 *	of the users choice.
 *
 *  Author -
 *	John Anderson
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#	include <string.h>
#else
#	include <strings.h>
#endif
#include <errno.h>

#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include <ctype.h>

static char	red_tmpfil[] = "/tmp/GED.aXXXXX";
static char	red_tmpcomb[] = "red_tmp.aXXXXX";

void restore_comb();
int editit();
int clear_comb(),build_comb(),save_comb();

static int ident;
static int air;

int
f_red( argc , argv )
int argc;
char **argv;
{

	union record record;
	struct directory *dp;

	if( argc != 2 )
	{
		rt_log( "Usage:\n\tred object_name\n" );
		return CMD_BAD;
	}

	if( (dp=db_lookup( dbip , argv[1] , LOOKUP_NOISY )) == DIR_NULL )
	{
		rt_log( " Cannot edit: %s\n" , argv[1] );
		return CMD_BAD;
	}

	if( db_get( dbip , dp , &record , 0 , 1 ) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}
	if( record.u_id != ID_COMB )	/* Not a combination */
	{
		rt_log( " %s is not a combination, so cannot be edited this way\n", argv[1] );
		return CMD_BAD;
	}

	/* Save for later use in rebuilding */
	ident = record.c.c_regionid;
	air = record.c.c_aircode;


	/* Make a file for the text editor */
	(void)mktemp( red_tmpfil );

	/* Write the combination components to the file */
	if( writecomb( dp ) )
	{
		rt_log( "Unable to edit %s\n" , argv[1] );
		unlink( red_tmpfil );
		return CMD_BAD;
	}

	/* Edit the file */
	if( editit( red_tmpfil ) )
	{
		if( checkcomb() ) /* Do some quick checking on the edited file */
		{
			rt_log( "Error in edited region, no changes made\n" );
			(void)unlink( red_tmpfil );
			return CMD_BAD;
		}
		if( save_comb( dp ) )	/* Save combination to a temp name */
		{
			rt_log( "No changes made\n" );
			(void)unlink( red_tmpfil );
			return CMD_OK;
		}
		if( clear_comb( dp ) )	/* Empty this combination */
		{
			rt_log( "Unable to empty %s, original restored\n" , dp->d_namep );
			restore_comb( dp );
			(void)unlink( red_tmpfil );
			return CMD_BAD;
		}
		if( build_comb( dp ) )	/* Use comb_add() to rebuild combination */
		{
			rt_log( "Unable to construct new %s, original restored\n" , dp->d_namep );
			restore_comb( dp );
			(void)unlink( red_tmpfil );
			return CMD_BAD;
		}
		else			/* eliminate the temporary combination */
		{
			struct rt_vls	v;

			rt_vls_init( &v );
			rt_vls_strcat( &v, "kill " );
			rt_vls_strcat( &v , red_tmpcomb );
			rt_vls_strcat( &v , "\n" );

			cmdline( &v, FALSE );

			rt_vls_free( &v );
		}
	}

	(void)unlink( red_tmpfil );
	return CMD_OK;
}

int
writecomb( dp )
struct directory *dp;
{
/*	Writes the file for later editing */
	union record record;
	FILE *fp;
	int offset,i;

	/* open the file */
	if( (fp=fopen( red_tmpfil , "w" )) == NULL )
	{
		rt_log( "Cannot open create file for editing\n" );
		perror( "MGED" );
		return(1);
	}

	/* Get combo info and write it to file */
	for( offset=1 ; offset< dp->d_len ; offset++ )
	{
		if( db_get( dbip , dp , &record , offset , 1 ) )
		{
			fclose( fp );
			rt_log( "Cannot get combination information\n" );
			return( 1 );
		}

		if( record.u_id != ID_MEMB )
		{
			rt_log( "This combination appears to be corrupted\n" );
			return( 1 );
		}

		for( i=0 ; i<ELEMENTS_PER_MAT ; i++ )
		{
			if( record.M.m_mat[i] != identity[i] )
			{
				rt_log( "Member `%s` has been object edited\n" , record.M.m_instname );
				rt_log( "\tCombination must be `pushed` before editing\n" );
				return( 1 );
			}
		}

		fprintf( fp , " %c %s\n" , record.M.m_relation , record.M.m_instname );
	}
	fclose( fp );
	return( 0 );
}

int
checkcomb()
{
/*	Do some minor checking of the edited file */

	FILE *fp;
	int nonsubs=0;
	int i,j,done,ch;
	char relation;
	char name[NAMESIZE+1];
	char line[MAXLINE];

	if( (fp=fopen( red_tmpfil , "r" )) == NULL )
	{
		rt_log( "Cannot open create file for editing\n" );
		perror( "MGED" );
		return(1);
	}

	/* Read a line at a time */
	done = 0;
	while( !done )
	{
		/* Read a line */
		i = (-1);

		while( (ch=getc( fp )) != EOF && ch != '\n' && i<MAXLINE )
			line[++i] = ch;

		if( ch == EOF )	/* We must be done */
		{
			done = 1;
			break;
		}
		if( i == MAXLINE )
		{
			rt_log( "Line too long in edited file\n" );
			return( 1 );
		}

		line[++i] = '\0';

		/* skip leading white space */
		i = (-1);
		while( isspace( line[++i] ));

		/* First non-white is the relation operator */
		relation = line[i];
		if( relation == '\0' )
		{
			done = 1;
			break;
		}

		/* Skip more white space */
		while( isspace( line[++i] ));

		/* Next must be the member name */
		strncpy( name , &line[i] , NAMESIZE );
		name[NAMESIZE] = '\0';

		/* Eliminate trailing white space from name */
		j = NAMESIZE;
		while( isspace( name[--j] ) )
			name[j] = '\0';

		/* Skip over name */
		while( !isspace( line[++i] ) && line[i] != '\0' );

		if( line[i] != '\0' )
		{
			/* Look for junk on the tail end of the line */
			while( isspace( line[++i] ) );
			if( line[i] != '\0' )
			{
				/* found some junk */
				rt_log( "Error in format of edited file\n" );
				rt_log( "Must be just one operator and object per line\n" );
				fclose( fp );
				return( 1 );
			}
		}

		if( relation != '+' && relation != 'u' & relation != '-' )
		{
			rt_log( " %c is not a legal operator\n" , relation );
			fclose( fp );
			return( 1 );
		}
		if( relation != '-' )
			nonsubs++;

		if( name[0] == '\0' )
		{
			rt_log( " operand name missing\n" );
			fclose( fp );
			return( 1 );
		}

		if( db_lookup( dbip , name , LOOKUP_NOISY ) == DIR_NULL )
		{
			rt_log( " %s does not exist\n" , name );
			fclose( fp );
			return( 1 );
		}
	}

	fclose( fp );

	if( nonsubs == 0 )
	{
		rt_log( "Cannot create a combination with all subtraction operators\n" );
		return( 1 );
	}
	return( 0 );
}

int clear_comb( dp )
struct directory *dp;
{

	register int i, rec;
	union record record;

	if( dp == DIR_NULL )
		return( 1 );

	/* Delete all the Member records, one at a time */

	rec = dp->d_len;
	for( i = 1; i < rec; i++ )
	{
		if( db_get( dbip,  dp, &record, 1 , 1) < 0 )
		{
			rt_log( "Unable to clear %s\n" , dp->d_namep );
			return( 1 );
		}

		if( db_delrec( dbip, dp, 1 ) < 0 )
		{
			rt_log("Error in deleting member.\n");
			return( 1 );
		}
	}
	return( 0 );
}

int build_comb( dp )
struct directory *dp;
{
/*	Build the new combination by adding to the recently emptied combination
	This keeps combo info associated with this combo intact */

	FILE *fp;
	char relation;
	char name[NAMESIZE+1];
	char line[MAXLINE];
	int ch;
	int i;
	int done=0;
	int region;
	struct directory *dp1;

	if( (fp=fopen( red_tmpfil , "r" )) == NULL )
	{
		rt_log( " Cannot open edited file: %s\n" , red_tmpfil );
		return( 1 );
	}

	/* Will need to know whether this is a region later */
	if( dp->d_flags == DIR_REGION )
		region = 1;
	else
		region = 0;

	/* Read edited file */
	while( !done )
	{
		/* Read a line */
		i = (-1);

		while( (ch=getc( fp )) != EOF && ch != '\n' && i<MAXLINE )
			line[++i] = ch;

		if( ch == EOF )	/* We must be done */
		{
			done = 1;
			break;
		}

		line[++i] = '\0';

		/* skip leading white space */
		i = (-1);
		while( isspace( line[++i] ));

		/* First non-white is the relation operator */
		relation = line[i];
		if( relation == '\0' )
		{
			done = 1;
			break;
		}

		/* Skip more white space */
		while( isspace( line[++i] ));

		/* Next must be the member name */
		strncpy( name , &line[i] , NAMESIZE );
		name[NAMESIZE] = '\0';

		/* Eliminate trailing white space from name */
		i = NAMESIZE;
		while( isspace( name[--i] ) )
			name[i] = '\0';

		/* Check for existence of member */
		if( (dp1=db_lookup( dbip , name , LOOKUP_NOISY )) == DIR_NULL )
		{
			rt_log( " %s does not exist\n" , name );
			return( 1 );
		}

		/* Add it to the combination */
		if( combadd( dp1 , dp->d_namep , region , relation , ident , air ) == DIR_NULL )
		{
			rt_log( " Error in rebuilding combination\n" );
			return( 1 );
		}
	}
	return( 0 );
}

void
mktemp_comb( str )
char *str;
{
/* Make a temporary name for a combination
	a template name is expected as in "mk_temp()" with 
	5 trailing X's */

	int counter,done;
	char *ptr;


	/* Set "ptr" to start of X's */

	ptr = str;
	while( *ptr != '\0' )
		ptr++;

	while( *(--ptr) == 'X' );
	ptr++;


	counter = 1;
	done = 0;
	while( !done && counter < 99999 )
	{
		sprintf( ptr , "%d" , counter );
		if( db_lookup( dbip , str , LOOKUP_QUIET ) == DIR_NULL )
			done = 1;
		else
			counter++;
	}
}

int save_comb( dpold )
struct directory *dpold;
{
/* Save a combination under a temporory name */

	register struct directory *dp;
	union record		*rp;

	/* Make a new name */
	mktemp_comb( red_tmpcomb );

	/* Following code is lifted from "f_copy()" and slightly modified */
	if( (rp = db_getmrec( dbip, dpold )) == (union record *)0 )
	{
		rt_log( "Cannot save copy of %s, no changes made\n" , dpold->d_namep );
		return( 1 );
	}

	if( (dp=db_diradd( dbip, red_tmpcomb, -1, dpold->d_len, dpold->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, dpold->d_len ) < 0 )
	{
		rt_log( "Cannot save copy of %s, no changes made\n" , dp->d_namep );
		return( 1 );
	}

	/* All objects have name in the same place */
	NAMEMOVE( red_tmpcomb, rp->c.c_name );
	if( db_put( dbip, dp, rp, 0, dpold->d_len ) < 0 )
	{
		rt_log( "Cannot save copy of %s, no changes made\n" , dp->d_namep );
		return( 1 );
	}

	rt_free( (char *)rp, "record" );
	return( 0 );
}

void
restore_comb( dp )
struct directory *dp;
{
/* restore a combination that was saved in "red_tmpcomb" */
	struct rt_vls	v;
	char name[NAMESIZE];

	/* Save name of original combo */
	strcpy( name , dp->d_namep );

	/* Kill original combination */
	rt_vls_init(&v);
	rt_vls_strcat( &v , "kill " );
	rt_vls_strcat( &v , dp->d_namep );
	rt_vls_strcat( &v , "\n" );

	cmdline( &v, FALSE );

	rt_vls_free( &v );

	/* Move temp to original */
	rt_vls_strcat( &v , "mv " );
	rt_vls_strcat( &v , red_tmpcomb );
	rt_vls_strcat( &v , " " );
	rt_vls_strcat( &v , name );
	rt_vls_strcat( &v , "\n" );

	cmdline( &v, FALSE );

	rt_vls_free( &v );
}
