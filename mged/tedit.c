/*
 *  			T E D I T . C
 *
 * Functions -
 *	f_tedit		Run text editor on numerical parameters of solid
 *	writesolid	Write numerical parameters of solid into a file
 *	readsolid	Read numerical parameters of solid from file
 *	editit		Run $EDITOR on temp file
 *
 *  Author -
 *	Michael John Muuss
 *	(Inspired by 4.2 BSD program "vipw")
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <signal.h>
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"

#define	DEFEDITOR	"/bin/ed"

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

static char	tmpfil[] = "/tmp/GED.aXXXXX";

void writesolid(), readsolid();
int editit();

void
f_tedit()
{
	register int i;
	struct solidrec lsolid;		/* local copy of solid */

	/* Only do this if in solid edit state */
	if( not_state( ST_S_EDIT, "Solid Text Edit" ) )
		return;

	if( es_rec.u_id != ID_SOLID ) {
		(void)printf("tedit: not a solid\n");
		return;
	}

	(void)mktemp(tmpfil);
	i=creat(tmpfil, 0600);
	if( i < 0 )  {
		perror(tmpfil);
		return;
	}
	(void)close(i);

	if( es_rec.s.s_type == GENARB8 )  {
		/* convert to point notation in temporary buffer */
		VMOVE( &lsolid.s_values[0], &es_rec.s.s_values[0] );
		for( i = 3; i <= 21; i += 3 )  {  
			VADD2(&lsolid.s_values[i], &es_rec.s.s_values[i], &lsolid.s_values[0]);
		}
		writesolid( &lsolid );
	} else {
		writesolid( &es_rec.s );
	}

	if( editit( tmpfil ) )  {
		if( es_rec.s.s_type == GENARB8 )  {
			readsolid( &lsolid );

			/* Convert back to point&vector notation */
			VMOVE( &es_rec.s.s_values[0], &lsolid.s_values[0] );
			for( i = 3; i <= 21; i += 3 )  {  
				VSUB2( &es_rec.s.s_values[i], &lsolid.s_values[i], &lsolid.s_values[0]);
			}
		}  else  {
			readsolid( &es_rec.s );
		}

		/* Update the display */
		replot_editing_solid();
		pr_solid( &es_rec.s );
		dmaflag = 1;
		(void)printf("done\n");
	}
	(void)unlink(tmpfil);
}

/* Write numerical parameters of a solid into a file */
void
writesolid( sp )
register struct solidrec *sp;
{
	register int i;
	FILE *fp;

	fp = fopen(tmpfil, "w");

	/* Print solid parameters, 1 vector or point per line */
	/* TODO:  This should be type-specific, with labels */
	for( i = 0; i < 24; i+=3 )
		(void)fprintf(fp,"%.9f %.9f %.9f\n",
			sp->s_values[i]*base2local,
			sp->s_values[i+1]*base2local,
			sp->s_values[i+2]*base2local );
	(void)fclose(fp);
}

/* Read numerical parameters of solid from file */
void
readsolid( sp )
register struct solidrec *sp;
{
	register int i;
	char line[256];
	FILE *fp;

	fp = fopen(tmpfil, "r");
	if( fp == NULL )  {
		perror(tmpfil);
		return;
	}

	/* Read solid parameters, 1 vector or point per line */
	for( i = 0; i < 24; i+=3 )  {
		if (fgets(line, sizeof (line), fp) == NULL)
			break;
		
		(void)sscanf( line, "%e %e %e",
			&sp->s_values[i],
			&sp->s_values[i+1],
			&sp->s_values[i+2] );
		sp->s_values[i] *= local2base;
		sp->s_values[i+1] *= local2base;
		sp->s_values[i+2] *= local2base;
	}
	(void)fclose(fp);
}

/* Run $EDITOR on temp file */
editit( file )
char *file;
{
#ifdef BSD
	register pid, xpid;
	int stat, omask;

#define	mask(s)	(1<<((s)-1))
	omask = sigblock(mask(SIGINT)|mask(SIGQUIT)|mask(SIGHUP));

	if ((pid = fork()) < 0) {
		perror("fork");
		return (0);
	}
	if (pid == 0) {
		register char *ed;

		sigsetmask(omask);
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = DEFEDITOR;
		(void)printf("Invoking %s...\n", ed);
		(void)execlp(ed, ed, file, 0);
		perror(ed);
		exit(1);
	}
	while ((xpid = wait(&stat)) >= 0)
		if (xpid == pid)
			break;
	sigsetmask(omask);
	return (!stat);
#endif
#ifdef SYSV
	/* System V */
	register pid, xpid;
	int stat;
	void (*s2)(), (*s3)();

	s2 = signal( SIGINT, SIG_IGN );
	s3 = signal( SIGQUIT, SIG_IGN );
	if ((pid = fork()) < 0) {
		perror("fork");
		return (0);
	}
	if (pid == 0) {
		register char *ed;
		register int i;

		for( i=3; i < 20; i++ )
			(void)close(i);

		(void)signal( SIGINT, SIG_DFL );
		(void)signal( SIGQUIT, SIG_DFL );
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = DEFEDITOR;
		(void)printf("Invoking %s...\n", ed);
		(void)execlp(ed, ed, file, 0);
		perror(ed);
		exit(1);
	}
	while ((xpid = wait(&stat)) >= 0)
		if (xpid == pid)
			break;
	(void)signal(SIGINT, s2);
	(void)signal(SIGQUIT, s3);
	return (!stat);
#endif
}
