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
#include "bu.h"
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
f_red(clientData, interp, argc , argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{

	union record record;
	struct directory *dp;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (dp=db_lookup( dbip , argv[1] , LOOKUP_NOISY )) == DIR_NULL )
	{
	  Tcl_AppendResult(interp, "Cannot edit: ", argv[1], "\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( db_get( dbip , dp , &record , 0 , 1 ) < 0 ) {
	  TCL_READ_ERR_return;
	}
	if( record.u_id != ID_COMB )	/* Not a combination */
	{
	  Tcl_AppendResult(interp, argv[1],
			   " is not a combination, so cannot be edited this way\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Save for later use in rebuilding */
	ident = record.c.c_regionid;
	air = record.c.c_aircode;


	/* Make a file for the text editor */
	(void)mktemp( red_tmpfil );

	/* Write the combination components to the file */
	if( writecomb( dp ) )
	{
	  Tcl_AppendResult(interp, "Unable to edit ", argv[1], "\n", (char *)NULL);
	  unlink( red_tmpfil );
	  return TCL_ERROR;
	}

	/* Edit the file */
	if( editit( red_tmpfil ) )
	{
	  if( checkcomb() ){ /* Do some quick checking on the edited file */
	    Tcl_AppendResult(interp, "Error in edited region, no changes made\n", (char *)NULL);
	    (void)unlink( red_tmpfil );
	    return TCL_ERROR;
	  }
	  if( save_comb( dp ) ){ /* Save combination to a temp name */
	    Tcl_AppendResult(interp, "No changes made\n", (char *)NULL);
	    (void)unlink( red_tmpfil );
	    return TCL_OK;
	  }
	  if( clear_comb( dp ) ){ /* Empty this combination */
	    Tcl_AppendResult(interp, "Unable to empty ", dp->d_namep,
			     ", original restored\n", (char *)NULL);
	    restore_comb( dp );
	    (void)unlink( red_tmpfil );
	    return TCL_ERROR;
	  }
	  if( build_comb( dp ) ){ /* Use comb_add() to rebuild combination */
	    Tcl_AppendResult(interp, "Unable to construct new ", dp->d_namep,
			     ", original restored\n", (char *)NULL);
	    restore_comb( dp );
	    (void)unlink( red_tmpfil );
	    return TCL_ERROR;
	  }else{ /* eliminate the temporary combination */
	    char *av[3];

	    av[0] = "kill";
	    av[1] = red_tmpcomb;
	    av[2] = NULL;
	    (void)f_kill(clientData, interp, 2, av);
	  }
	}

	(void)unlink( red_tmpfil );
	return TCL_OK;
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
	  Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
	  perror( "MGED" );
	  return(1);
	}

	/* Get combo info and write it to file */
	for( offset=1 ; offset< dp->d_len ; offset++ )
	{
		if( db_get( dbip , dp , &record , offset , 1 ) )
		{
		  fclose( fp );
		  Tcl_AppendResult(interp, "Cannot get combination information\n", (char *)NULL);
		  return( 1 );
		}

		if( record.u_id != ID_MEMB )
		{
		  Tcl_AppendResult(interp, "This combination appears to be corrupted\n",
				   (char *)NULL);
		  fclose( fp );
		  return( 1 );
		}

		for( i=0 ; i<ELEMENTS_PER_MAT ; i++ ){
		  if( record.M.m_mat[i] != identity[i] ){
		    Tcl_AppendResult(interp, "Member `", record.M.m_instname,
				     "` has been object edited\n",
				     "\tCombination must be `pushed` before editing\n",
				     (char *)NULL);
		    fclose( fp );
		    return( 1 );
		  }
		}

		if( fprintf( fp , " %c %s\n" , record.M.m_relation , record.M.m_instname ) <= 0 )
		{
			Tcl_AppendResult(interp, "Cannot write to temp file (", red_tmpfil,
				"). Aborting edit\n", (char *)NULL );
			fclose( fp );
			return( 1 );
		}
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
	  Tcl_AppendResult(interp, "Cannot open create file for editing\n", (char *)NULL);
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
		  Tcl_AppendResult(interp, "Line too long in edited file\n", (char *)NULL);
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
		      Tcl_AppendResult(interp, "Error in format of edited file\n",
				       "Must be just one operator and object per line\n",
				       (char *)NULL);
		      fclose( fp );
		      return( 1 );
		    }
		}

		if( relation != '+' && relation != 'u' & relation != '-' )
		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, " %c is not a legal operator\n" , relation );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		  fclose( fp );
		  return( 1 );
		}
		if( relation != '-' )
			nonsubs++;

		if( name[0] == '\0' )
		{
		  Tcl_AppendResult(interp, " operand name missing\n", (char *)NULL);
		  fclose( fp );
		  return( 1 );
		}

		if( db_lookup( dbip , name , LOOKUP_NOISY ) == DIR_NULL )
		{
		  Tcl_AppendResult(interp, " ", name, " does not exist\n", (char *)NULL);
		  fclose( fp );
		  return( 1 );
		}
	}

	fclose( fp );

	if( nonsubs == 0 )
	{
	  Tcl_AppendResult(interp, "Cannot create a combination with all subtraction operators\n",
			   (char *)NULL);
	  return( 1 );
	}
	return( 0 );
}

/*
 *  Returns -
 *	0 if OK
 *	1 on failure
 */
int clear_comb( dp )
struct directory *dp;
{
	register int i;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	if( dp == DIR_NULL )
		return( 1 );

	if( (dp->d_flags & DIR_COMB) == 0 )  {
		Tcl_AppendResult(interp, "clear_comb: ", dp->d_namep,
			" is not a combination\n", (char *)NULL );
		return 1;
	}

	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )  {
		Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
			") failure", (char *)NULL );
		return 1;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if( comb->tree )  {
		db_free_tree( comb->tree );
		comb->tree = NULL;
	}

	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )  {
		Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
		return 1;
	}
	return 0;
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
	  Tcl_AppendResult(interp, " Cannot open edited file: ",
			   red_tmpfil, "\n", (char *)NULL);
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
		  Tcl_AppendResult(interp, " ", name, " does not exist\n", (char *)NULL);
		  return( 1 );
		}

		/* Add it to the combination */
		if( combadd( dp1 , dp->d_namep , region , relation , ident , air ) == DIR_NULL )
		{
		  Tcl_AppendResult(interp, " Error in rebuilding combination\n", (char *)NULL);
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
	  Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			   ", no changes made\n", (char *)NULL);
	  return( 1 );
	}

	if( (dp=db_diradd( dbip, red_tmpcomb, -1, dpold->d_len, dpold->d_flags)) == DIR_NULL ||
	    db_alloc( dbip, dp, dpold->d_len ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot save copy of ", dpold->d_namep,
			   ", no changes made\n", (char *)NULL);
	  return( 1 );
	}

	/* All objects have name in the same place */
	NAMEMOVE( red_tmpcomb, rp->c.c_name );
	if( db_put( dbip, dp, rp, 0, dpold->d_len ) < 0 )
	{
	  Tcl_AppendResult(interp, "Cannot save copy of ", dp->d_namep,
			   ", no changes made\n", (char *)NULL);
	  return( 1 );
	}

	bu_free( (genptr_t)rp, "record" );
	return( 0 );
}

/* restore a combination that was saved in "red_tmpcomb" */
void
restore_comb( dp )
struct directory *dp;
{
  char *av[4];
  char name[NAMESIZE];

  /* Save name of original combo */
  strcpy( name , dp->d_namep );

  av[0] = "kill";
  av[1] = name;
  av[2] = NULL;
  av[3] = NULL;
  (void)f_kill((ClientData)NULL, interp, 2, av);

  av[0] = "mv";
  av[1] = red_tmpcomb;
  av[2] = name;

  (void)f_name((ClientData)NULL, interp, 3, av);
}
