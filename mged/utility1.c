/*
 *			U T I L I T Y 1 . C
 *
 *  Functions -
 *	f_tables()	control routine for building ascii tables
 *	tables()	builds ascii summary tables
 *	f_edcodes()	control routine for editing region ident codes
 *	edcodes()	allows for easy editing of region ident codes
 *	f_which_id()	lists all regions with given ident number
 *
 *  Author -
 *	Keith A. Applin
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

int readcodes(), writecodes();
int loadcodes(), printcodes();
void		tables(), edcodes(), changes(), prfield();

#define LINELEN 256
#define MAX_LEVELS 12
struct directory *path[MAX_LEVELS];

/* structure to distinguish new solids from existing (old) solids */
struct identt {
	int	i_index;
	char	i_name[NAMESIZE];
	mat_t	i_mat;
};
struct identt identt, idbuf;

static union record record;

#define ABORTED		-99
#define OLDSOLID	0
#define NEWSOLID	1
#define SOL_TABLE	1
#define REG_TABLE	2
#define ID_TABLE	3

/*
 *
 *	F _ T A B L E S :	control routine to build ascii tables
 *
 *
 */

char operate;
int regflag, numreg, lastmemb, numsol, old_or_new, oper_ok;
int discr[MAXARGS], idfd, rd_idfd;
int flag;	/* which type of table to make */
FILE	*tabptr;

int
f_tables(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{

	register struct directory *dp;
	register int i;
	char *timep;
	time_t now;
	static CONST char sortcmd[] = "sort -n +1 -2 -o /tmp/ord_id ";
	static CONST char catcmd[] = "cat /tmp/ord_id >> ";

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
	else
	  return TCL_OK;

	/* find out which ascii table is desired */
	if( strcmp(argv[0], "solids") == 0 ) {
		/* complete summary - down to solids/paremeters */
		flag = SOL_TABLE;
	}
	else if( strcmp(argv[0], "regions") == 0 ) {
		/* summary down to solids as members of regions */
		flag = REG_TABLE;
	}
	else if( strcmp(argv[0], "idents") == 0 ) {
		/* summary down to regions */
		flag = ID_TABLE;
	}
	else {
		/* should never reach here */
	  Tcl_AppendResult(interp, "tables:  input error\n", (char *)NULL);
	  return TCL_ERROR;
	}

	regflag = numreg = lastmemb = numsol = 0;
	tabptr = NULL;

	/* open the file */
	if( (tabptr=fopen(argv[1], "w+")) == NULL ) {
	  Tcl_AppendResult(interp, "Can't open ", argv[1], "\n", (char *)NULL);
	  return TCL_ERROR;
	}

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		/* temp file for discrimination of solids */
		if( (idfd = creat("/tmp/mged_discr", 0600)) < 0 ) {
			perror( "/tmp/mged_discr" );
			return TCL_ERROR;
		}
		rd_idfd = open( "/tmp/mged_discr", 2 );
	}

	(void)time( &now );
	timep = ctime( &now );
	timep[24] = '\0';
	(void)fprintf(tabptr,"1 -8    Summary Table {%s}  (written: %s)\n",argv[0],timep);
	(void)fprintf(tabptr,"2 -7         file name    : %s\n",dbip->dbi_filename);    
	(void)fprintf(tabptr,"3 -6         \n");
	(void)fprintf(tabptr,"4 -5         \n");
	(void)fprintf(tabptr,"5 -4         user         : %s\n",getpwuid(getuid())->pw_gecos);
	(void)fprintf(tabptr,"6 -3         target title : %s\n",cur_title);
	(void)fprintf(tabptr,"7 -2         target units : %s\n",
		rt_units_string(dbip->dbi_local2base) );
	(void)fprintf(tabptr,"8 -1         objects      :");
	for(i=2; i<argc; i++) {
		if( (i%8) == 0 )
			(void)fprintf(tabptr,"\n                           ");
		(void)fprintf(tabptr," %s",argv[i]);
	}
	(void)fprintf(tabptr,"\n\n");

	/* make table of the objects */
#if 0
	mat_idn( identity );
#endif
	for(i=2; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i],LOOKUP_NOISY)) != DIR_NULL )
			tables(dp, 0, identity, flag);
		else
		  Tcl_AppendResult(interp, " skip this object\n", (char *)NULL);
	}

	Tcl_AppendResult(interp, "Summary written in: ", argv[1], "\n", (char *)NULL);

	if( flag == SOL_TABLE || flag == REG_TABLE ) {
		/* remove the temp file */
		(void)unlink( "/tmp/mged_discr\0" );
		(void)fprintf(tabptr,"\n\nNumber Solids = %d  Number Regions = %d\n",
				numsol,numreg);
		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "Processed %d Solids and %d Regions\n",
				numsol,numreg);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		(void)fclose( tabptr );
	}

	else {
		struct bu_vls	cmd;

		(void)fprintf(tabptr,"* 9999999\n* 9999999\n* 9999999\n* 9999999\n* 9999999\n");
		(void)fclose( tabptr );

		{
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "Processed %d Regions\n",numreg);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		/* make ordered idents */
		bu_vls_init( &cmd );
		bu_vls_strcpy( &cmd, sortcmd );
		bu_vls_strcat( &cmd, argv[1] );
		Tcl_AppendResult(interp, bu_vls_addr(&cmd), "\n", (char *)NULL);
		(void)system( bu_vls_addr(&cmd) );

		bu_vls_trunc( &cmd, 0 );
		bu_vls_strcpy( &cmd, catcmd );
		bu_vls_strcat( &cmd, argv[1] );
		Tcl_AppendResult(interp, bu_vls_addr(&cmd), "\n", (char *)NULL);
		(void)system( bu_vls_addr(&cmd) );
		bu_vls_free( &cmd );

		(void)unlink( "/tmp/ord_id\0" );
	}

	return TCL_OK;
}




char ctemp[7];
/*
 *
 *	F _ E D C O D E S ( )
 *
 *		control routine for editing region ident codes
 *
 *
 */
int
f_edcodes(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  int i;
  int status;
  char *tmpfil = "/tmp/GED.aXXXXX";
  char **av;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  (void)mktemp(tmpfil);
  i=creat(tmpfil, 0600);
  if( i < 0 ){
    perror(tmpfil);
    return TCL_ERROR;
  }

  (void)close(i);

  av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "f_edcodes: av");
  av[0] = "wcodes";
  av[1] = tmpfil;
  for(i = 2; i < argc + 1; ++i)
    av[i] = argv[i-1];

  av[i] = NULL;

  if( f_wcodes(clientData, interp, argc + 1, av) == TCL_ERROR ){
    (void)unlink(tmpfil);
    bu_free((char *)av, "f_edcodes: av");
    return TCL_ERROR;
  }

  if( editit(tmpfil) ){
    regflag = lastmemb = 0;
    av[0] = "rcodes";
    av[2] = NULL;
    status = f_rcodes(clientData, interp, 2, av);
  }else
    status = TCL_ERROR;

  (void)unlink(tmpfil);
  bu_free((char *)av, "f_edcodes: av");
  return status;
}


/* write codes to a file */
int
f_wcodes(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
  register int i;
  int status;
  FILE *fp;
  register struct directory *dp;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if((fp = fopen(argv[1], "w")) == NULL){
    Tcl_AppendResult(interp, "f_wcodes: Failed to open file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  regflag = lastmemb = 0;
  for(i = 2; i < argc; ++i){
    if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL ){
      status = printcodes(fp, dp, 0);

      if(status == TCL_ERROR){
	(void)fclose(fp);
	return TCL_ERROR;
      }
    }
  }

  (void)fclose(fp);
  return TCL_OK;
}

/* read codes from a file and load them into the database */
int
f_rcodes(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    *argv[];
{
  int item, air, mat, los;
  char name[MAX_LEVELS * NAMESIZE];
  char line[LINELEN];
  char *cp;
  FILE *fp;
  register struct directory *dp;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
        return TCL_ERROR;

  if((fp = fopen(argv[1], "r")) == NULL){
    Tcl_AppendResult(interp, "f_rcodes: Failed to read file - ", argv[1], (char *)NULL);
    return TCL_ERROR;
  }

  while(fgets( line , LINELEN, fp ) != NULL){
    if(sscanf(line, "%d%d%d%d%s", &item, &air, &mat, &los, name) != 5)
      continue; /* not useful */

    /* skip over the path */
    if((cp = strrchr(name, (int)'/')) == NULL)
      cp = name;
    else
      ++cp;

    if(*cp == '\0')
      continue;

    if((dp = db_lookup( dbip, cp, LOOKUP_NOISY )) == DIR_NULL){
      Tcl_AppendResult(interp, "f_rcodes: Warning - ", cp, " not found in database.\n",
		       (char *)NULL);
      continue;
    }

    if( db_get( dbip, dp, &record, 0, 1) < 0 )
      TCL_READ_ERR_return;

    /* make the changes */
    record.c.c_regionid = item;
    record.c.c_aircode = air;
    record.c.c_material = mat;
    record.c.c_los = los;

    /* write out all changes */
    if( db_put( dbip, dp, &record, 0, 1 ) < 0 ){
      Tcl_AppendResult(interp, "Database write error, aborting.\n",
		       (char *)NULL);
      TCL_ERROR_RECOVERY_SUGGESTION;
      return TCL_ERROR;
    }
  }

  return TCL_OK;
}


int
printcodes(fp, dp, pathpos)
FILE *fp;
struct directory *dp;
int pathpos;
{
  int i;
  int status;
  int nparts;
  struct directory *nextdp;

  if(pathpos >= MAX_LEVELS){
    regflag = ABORTED;
    return TCL_ERROR;
  }

  if( db_get( dbip, dp, &record, 0, 1) < 0 )
    return TCL_ERROR;

  if( record.u_id == ID_COMB ){
    if(regflag > 0){
      if(record.c.c_flags != 'R'){
	fprintf(fp, "**WARNING** group= %s is member of region= ",record.c.c_name);

	for(i=0; i < pathpos; i++)
	  fprintf(fp, "/%s",path[i]->d_namep);

	fprintf(fp, "\n");
      }

      if(lastmemb)
	regflag = lastmemb = 0;

      return TCL_OK;
    }

    regflag = 0;
    nparts = dp->d_len-1;
    if(record.c.c_flags == 'R'){
      /* first region in this path */
      regflag = 1;

      if( nparts == 0 )	/* dummy region */
	regflag = 0;

      fprintf(fp, "%-6d%-3d%-3d%-4d  ",record.c.c_regionid,record.c.c_aircode,
	      record.c.c_material,record.c.c_los);

      for(i=0; i < pathpos; i++)
	fprintf(fp, "/%s",path[i]->d_namep);

      fprintf(fp, "/%s%s\n", record.c.c_name, nparts==0 ? " **DUMMY REGION**" : "");
    }

    lastmemb = 0;
    for(i=1; i<=nparts; i++) {
      if(i == nparts)
	lastmemb = 1;

      if( db_get( dbip, dp, &record, i, 1) < 0 ){
	TCL_READ_ERR_return;
      }

      path[pathpos] = dp;
      if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
	continue;

      /* Recursive call */
      status = printcodes(fp, nextdp, pathpos+1);
      if(status == TCL_ERROR)
	return TCL_ERROR;
    }

    return TCL_OK;
  }      

  /* not a combination  -  should have a solid */

  /* last (bottom) position */
  path[pathpos] = dp;

  if( lastmemb ){
    regflag = lastmemb = 0;
  }

  return TCL_OK;
}

/*
 *
 *		T A B L E S ( ) :    builds ascii tables (summary)
 *
 *
 */
void
tables( dp, pathpos, old_xlate, flag)
register struct directory *dp;
int pathpos;
mat_t old_xlate;
int flag;
{

	struct directory *nextdp;
	mat_t new_xlate;
	int nparts, i, k;
	int	nsoltemp;
	int dchar = 0;
	struct bu_vls str;

	bu_vls_init( &str );

	if( pathpos >= MAX_LEVELS ) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "nesting exceeds %d levels\n",MAX_LEVELS);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);

	  for(i=0; i<MAX_LEVELS; i++)
	    Tcl_AppendResult(interp, "/", path[i]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	  return;
	}

	if( db_get( dbip, dp, &record, 0, 1) < 0 )  READ_ERR_return;
	if( record.u_id == ID_COMB ) {
		if(regflag > 0) {
			/* this comb record is part of a region */
			if(record.c.c_flags == 'R') {
				oper_ok++;
				if(flag == SOL_TABLE || flag == REG_TABLE)
					(void)fprintf(tabptr,"   RG %c %s\n",operate,record.c.c_name);
			}
			else {
			  Tcl_AppendResult(interp, "**WARNING** group= ", record.c.c_name,
					   " is member of region= ", (char *)NULL);

			  for(k=0;k<pathpos;k++)
			    Tcl_AppendResult(interp, "/", path[k]->d_namep, (char *)NULL);

			  Tcl_AppendResult(interp, "\n", (char *)NULL);
			}
			if(lastmemb)
				regflag = lastmemb = 0;
			return;
		}
		regflag = 0;
		nparts = dp->d_len-1;
		if(record.c.c_flags == 'R') {
			/* first region in this path */
			numreg++;
			regflag = 1;

			if( nparts <= 0 )	/* dummy region */
				regflag = 0;

			oper_ok = 0;

			/*
			 * This format depends on %4d getting wider
			 * when the numbers exceeds 9999.
			 * MUVES *depends* on the fields being white-space
			 * separated, but doesn't care about field width.
			 * Lack of spaces also confuses the IDENT command's sort.
			 */
			(void)fprintf(tabptr,
				" %-4d %4d %4d %4d %4d  ",
				numreg,record.c.c_regionid,
				record.c.c_aircode,record.c.c_material,
				record.c.c_los);
			for(k=0;k<pathpos;k++) {
				(void)fprintf(tabptr,"/%s",path[k]->d_namep);
			}
			(void)fprintf(tabptr,"/%s:\n",record.c.c_name);
		}
		lastmemb = 0;
		for(i=1; i<=nparts; i++) {
			mat_t	xmat;

			if(i == nparts)
				lastmemb = 1;
			if( db_get( dbip, dp, &record, i, 1) < 0 )  READ_ERR_return;
			operate = record.M.m_relation;

			if(regflag && operate != SUBTRACT)
				oper_ok++;

			path[pathpos] = dp;
			if( (nextdp = db_lookup( dbip, record.M.m_instname, LOOKUP_NOISY)) == DIR_NULL )
				continue;

			rt_mat_dbmat( xmat, record.M.m_mat );
			mat_mul(new_xlate, old_xlate, xmat);

			/* Recursive call */
			tables(nextdp, pathpos+1, new_xlate, flag);
		}
		return;
	}

	/* not a combination  -  should have a solid */

	/* last (bottom) position */
	path[pathpos] = dp;

	if(regflag == 0) {
	  /* have a solid that's not part of a region */
	  Tcl_AppendResult(interp, "**WARNING** following path (solid) has no region:\n",
			   (char *)NULL);

	  for(k=0;k<=pathpos;k++)
	    Tcl_AppendResult(interp, "/", path[k]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	}

	if( regflag && lastmemb && oper_ok == 0 ) {
	  Tcl_AppendResult(interp, "**WARNING** following region has only '-' oprations:\n",
			   (char *)NULL);

	  for(k=0; k<pathpos; k++)
	    Tcl_AppendResult(interp, "/", path[k]->d_namep, (char *)NULL);

	  Tcl_AppendResult(interp, "\n", (char *)NULL);
	}

	if(flag == ID_TABLE) {
		if(lastmemb)
			regflag = lastmemb = 0;
		return;
	}

	/* check if this is a new or old solid */
	if(record.u_id == ID_SOLID) {
		(void)strncpy(identt.i_name, record.s.s_name, NAMESIZE);
	}
	else {
		(void)strncpy(identt.i_name, record.a.a_name, NAMESIZE);
	}
	mat_copy(identt.i_mat, old_xlate);

	/* first (quick) look discriminator is based on name alone */
	dchar = 0;
	for(i=0; i<NAMESIZE; i++) {
		if(identt.i_name[i] == 0) 
			break;
		dchar += (identt.i_name[i] << (i&7));
	}

	/* now check if solid is an old one -- method:
		1.  check quick look method
		2.  if quick look match check long method
	 */
	nsoltemp = 0;
	for(i=0; i<numsol; i++) {
		if(dchar == discr[i]) {
			/* have a quick look match - check further */
			(void)lseek(rd_idfd, i*(long)sizeof identt, 0);
			(void)read(rd_idfd, &idbuf, sizeof identt);
			identt.i_index = i + 1;
			if( check(&identt, &idbuf) == 1 ) {
				/* really is old...number = i+1 */
				nsoltemp = i + 1;
				old_or_new = OLDSOLID;
				goto notnew;
			}
			/* false alarm - quick look match but the
			      	long check indicates is not really old
			 */
		}
	}

	/* Have a NEW solid */
	old_or_new = NEWSOLID;
	discr[numsol++] = dchar;
	identt.i_index = numsol;
	if(numsol > MAXARGS) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "tables: number of solids > max (%d)\n",MAXARGS);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);

	  exit(10);
	}
	(void)lseek(idfd, (off_t)0L, 2);
	(void)write(idfd, &identt, sizeof identt);
	nsoltemp = numsol;

	notnew:		/* sent here if solid is an old one */

	if( regflag == 0 ) {
		(void)fprintf(tabptr,"\n\n***** NO REGION for the following solid:\n");
	}

	(void)fprintf(tabptr,"   %c [%d] ",
		operate,nsoltemp);
	if(lastmemb) {
		regflag = 0;
		lastmemb = 0;
	}
	if(flag == REG_TABLE || old_or_new == OLDSOLID) {
		(void)fprintf(tabptr,"%s\n", dp->d_namep);
		return;
	}

	/* Pretty-print the solid */
	do_list( &str, dp, 1 );		/* verbose */
	fprintf(tabptr, bu_vls_addr(&str));
}


/*    C H E C K      -     compares solids       returns 1 if they match
							 0 otherwise
 */

check( a, b )
register char *a, *b;
{

	register int	c= sizeof( struct identt );

	while( c-- )	if( *a++ != *b++ ) return( 0 );	/* no match */
	return( 1 );	/* match */

}




/*   E D I T L I N E :   allows the user to edit a line of region codes
 */

#define TAB		9
#define BLANK		32
#define BACKSPACE	8
#define CONTROLC	3
#define CRETURN		13
#define LINEFEED	10
#define RESTORE		82	/* R */
#define QUIT_q		113	/* q */

int item, air, mat, los;	/* temp values */

int
editline(dp)
struct directory	*dp;
{

	int	c;
	int field, lpos, eflag, i;
	int elflag, maxpos[4];
 	lpos = eflag = field = elflag = 0;

	/* field lengths */
	maxpos[0] = 6;
	maxpos[1] = maxpos[2] = 3;
	maxpos[3] = 4;

	item = record.c.c_regionid;
	air = record.c.c_aircode;
	mat = record.c.c_material;
	los = record.c.c_los;

	while( c = getchar() ) {
		switch( c )  {

			case TAB:
			case BLANK:
				/* record any changes to this field */
				if( elflag && lpos > 0 ) {
					ctemp[lpos] = c;
					changes( field );
					elflag = 0;
				}

				/* move cursor to beginning of next field */
				if(lpos == 0) 
					prfield(field);
				else {
				  for(i=0;i<(maxpos[field] - lpos); i++)
				    Tcl_AppendResult(interp, " ", (char *)NULL);
				}

				lpos = 0;
				if(++field > 3) {
				  Tcl_AppendResult(interp, "\r", (char *)NULL);
				  field = 0;
				}
			break;

			case BACKSPACE:
				if(lpos > 0) {
				  Tcl_AppendResult(interp, "\b", (char *)NULL);
				  lpos--;
				}
				else {
					/* go to beginning of previous field */
					if(elflag) {
						/* this field changed on screen
						 *   but change was not recorded.
						 *   Print out latest recorded
						 *   value.
						 */
						prfield(field);
						elflag = 0;
						for(i=0;i<maxpos[field];i++)
						  Tcl_AppendResult(interp, "\b", (char *)NULL);
					}
					if(field == 0)
						break;
					field--;
					for(i=0; i<maxpos[field]; i++)
					  Tcl_AppendResult(interp, "\b", (char *)NULL);
				}
			break;

			case CRETURN:
			  Tcl_AppendResult(interp, "\r\n", (char *)NULL);
			  if(eflag) {
			    if(elflag) {
			      /* change present field */
			      ctemp[lpos] = ' ';
			      elflag = 0;
			      changes( field );
			    }
			    /* update the record with changes */
			    record.c.c_regionid = item;
			    record.c.c_aircode = air;
			    record.c.c_material = mat;
			    record.c.c_los = los;
			    /* write out all changes */
			    if( db_put( dbip, dp, &record, 0, 1 ) < 0 )  {
			      Tcl_AppendResult(interp, "Database write error, aborting.\n",
					       (char *)NULL);
			      TCL_ERROR_RECOVERY_SUGGESTION;
			      return(1);
			    }
			  }
			  /* get out of loop */
			  return(0);

			case QUIT_q:
				/* 'q' entered ==> quit the editing */
			case CONTROLC:
			  Tcl_AppendResult(interp, "\r\n", (char *)NULL);
			  return(1);

			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
			  /* integer was input */
			  eflag++;
			  if(lpos < maxpos[field]) {
			    struct bu_vls tmp_vls;

			    elflag++;
			    bu_vls_init(&tmp_vls);
			    bu_vls_printf(&tmp_vls, "%c", c );
			    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			    bu_vls_free(&tmp_vls);
			    ctemp[lpos++] = c;
			  }
			  else {
			    Tcl_AppendResult(interp, "\a\a", (char *)NULL);
			  }
			  break;

			case RESTORE:
			  /* 'R' was input - restore the line */
			  item = record.c.c_regionid;
			  air = record.c.c_aircode;
			  mat = record.c.c_material;
			  los = record.c.c_los;
			  lpos = field = eflag = elflag = 0;

			  {
			    struct bu_vls tmp_vls;

			    bu_vls_init(&tmp_vls);
			    bu_vls_printf(&tmp_vls, "\r%-6d%-3d%-3d%-4d\r",item,air,mat,los);
			    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			    bu_vls_free(&tmp_vls);
			  }

			  break;

			default:
			  Tcl_AppendResult(interp, "\a", (char *)NULL);

			  break;
		}	/* end of switch */
	}	/* end of while loop */
	return(1);	/* We should never get here */
}

/*
 *			C H A N G E S 
 */
void
changes( num )
int num;
{

	switch( num ) {

		case 0:
			item = atoi( ctemp );
		break;

		case 1:
			air = atoi( ctemp );
		break;

		case 2:
			mat = atoi( ctemp );
		break;

		case 3:
			los = atoi( ctemp );
		break;

	}
	return;
}

/*
 *			P R F I E L D
 */
void
prfield( num )
int num;
{
  struct bu_vls tmp_vls;

  bu_vls_init(&tmp_vls);
  switch( num ) {
  case 0:
    bu_vls_printf(&tmp_vls, "%-6d",item);
    break;
  case 1:
    bu_vls_printf(&tmp_vls, "%-3d",air);
    break;
  case 2:
    bu_vls_printf(&tmp_vls, "%-3d",mat);
    break;
  case 3:
    bu_vls_printf(&tmp_vls, "%-4d",los);
    break;
  }

  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);
  return;
}

/*
 *	F _ W H I C H _ I D ( ) :	finds all regions with given idents
 */
int
f_which_id(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	union record	rec;
	register int	i,j;
	register struct directory *dp;
	int		item;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		Tcl_AppendResult(interp, "Region[s] with ident ", argv[j],
				 ":\n", (char *)NULL);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( (dp->d_flags & DIR_COMB|DIR_REGION) !=
				    (DIR_COMB|DIR_REGION) )
					continue;
				if( db_get( dbip, dp, &rec, 0, 1 ) < 0 ) {
				  TCL_READ_ERR_return;
				}
				if( rec.c.c_regionid != item )
					continue;

				Tcl_AppendResult(interp, "   ", rec.c.c_name,
						 "\n", (char *)NULL);
			}
		}
	}
	return TCL_OK;
}
/*
 *	F _ W H I C H _ A I R ( ) :	finds all regions with given air codes
 */
int
f_which_air(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	union record	rec;
	register int	i,j;
	register struct directory *dp;
	int		item;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for( j=1; j<argc; j++) {
		item = atoi( argv[j] );
		Tcl_AppendResult(interp, "Region[s] with air code ", argv[j],
				 ":\n", (char *)NULL);

		/* Examine all COMB nodes */
		for( i = 0; i < RT_DBNHASH; i++ )  {
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
				if( (dp->d_flags & DIR_COMB|DIR_REGION) !=
				    (DIR_COMB|DIR_REGION) )
					continue;
				if( db_get( dbip, dp, &rec, 0, 1 ) < 0 ) {
				  TCL_READ_ERR_return;
				}
				if( rec.c.c_regionid != 0 || rec.c.c_aircode != item )
					continue;

				Tcl_AppendResult(interp, "   ", rec.c.c_name,
						 "\n", (char *)NULL);
			}
		}
	}
	return TCL_OK;
}
