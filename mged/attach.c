/*
 *			A T T A C H . C
 *
 * Functions -
 *	attach		attach to a given display processor
 *	release		detach from current display processor
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "ged_types.h"
#include "db.h"
#include "ged.h"
#include "solid.h"
#include "dm.h"

static int	Nu_input();	/* Quite necessary */
static void	Nu_void();
static int	Nu_int();
static unsigned Nu_unsign();

struct dm dm_Null = {
	Nu_void, Nu_void, Nu_void,
	Nu_input,
	Nu_void, Nu_void,
	Nu_void, Nu_void,
	Nu_void,
	Nu_void, Nu_void,
	Nu_void,
	Nu_int,
	Nu_unsign, Nu_unsign,
	0.0,
	"nu", "Null Display"
};
extern struct dm dm_Mg, dm_Vg, dm_Tek, dm_Rat;

struct dm *dmp = &dm_Null;	/* Ptr to current Display Manager package */

/* The [0] entry will be the startup default */
static struct dm *which_dm[] = {
	&dm_Mg,
	&dm_Vg,
	&dm_Tek,
	&dm_Rat,
	&dm_Null,
	0
};

release()
{
	register struct solid *sp;

	/* Delete all references to display processor memory */
	FOR_ALL_SOLIDS( sp )  {
		memfree( &(dmp->dmr_map), sp->s_bytes, sp->s_addr );
		sp->s_bytes = 0;
		sp->s_addr = 0;
	}
	mempurge( &(dmp->dmr_map) );

	dmp->dmr_close();
	dmp = &dm_Null;
}

attach(name)
char *name;
{
	register struct dm **dp;
	register struct solid *sp;

	if( dmp != &dm_Null )
		release();
	for( dp=which_dm; *dp != (struct dm *)0; dp++ )  {
		if( strcmp( (*dp)->dmr_name, name ) != 0 )
			continue;
		dmp = *dp;
		printf("ATTACHING %s (%s)\n",
			dmp->dmr_name, dmp->dmr_lname);

		dmp->dmr_open();

		FOR_ALL_SOLIDS( sp )  {
			/* Write vector subs into new display processor */
			sp->s_bytes = dmp->dmr_cvtvecs( sp );
			sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
			if( sp->s_addr == 0 )  break;
			sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes);
		}
		dmaflag++;
		return;
	}
	printf("attach(%s): BAD\n", name);
	dmp = &dm_Null;
}

static int Nu_int() { return(0); }
static void Nu_void() { ; }
static unsigned Nu_unsign() { return(0); }

static
Nu_input( fd, noblock )
{
	long readfds = (1<<fd);
	(void)_select( 32, &readfds, 0L, 0L, (char *)0 );
}

/*
 *  			G E T _ A T T A C H E D
 *
 *  Prompt the user with his options, and loop until a valid choice is made.
 */
get_attached()
{
	char line[80];
	register struct dm **dp;

	while(1)  {
		printf("attach (");
		for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
			printf("%s|", (*dp)->dmr_name);
		printf(")[%s]? ", which_dm[0]->dmr_name);
		(void)gets( line );		/* Null terminated */
		if( feof(stdin) )  quit();
		if( line[0] == '\0' )  {
			dp = &which_dm[0];	/* default */
			break;
		}
		for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
			if( strcmp( line, (*dp)->dmr_name ) == 0 )
				break;
		if( *dp != (struct dm *)0 )
			break;
		/* Not a valid choice, loop. */
	}
	/* Valid choice made, attach to it */
	attach( (*dp)->dmr_name );
}
