/*
 *			A T T A C H . C
 *
 * Functions -
 *	attach		attach to a given display processor
 *	release		detach from current display processor
 *  
 *  Author -
 *	Michael John Muuss
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
#include "ged_types.h"
#include "../h/db.h"
#include "ged.h"
#include "solid.h"
#include "dm.h"

static int	Nu_input();	/* Quite necessary */
static void	Nu_void();
static int	Nu_int0();
static unsigned Nu_unsign();

struct dm dm_Null = {
	Nu_int0, Nu_void,
	Nu_input,
	Nu_void, Nu_void,
	Nu_void, Nu_void,
	Nu_void,
	Nu_void, Nu_void,
	Nu_void,
	Nu_int0,
	Nu_unsign, Nu_unsign,
	Nu_void,
	Nu_void,
	Nu_void,
	Nu_void, Nu_void,
	0,
	0.0,
	"nu", "Null Display"
};
extern struct dm dm_Tek, dm_Ir;
#ifdef BSD42
extern struct dm dm_Mg, dm_Vg, dm_Tek, dm_Rat, dm_Mer;
#endif
#ifdef PS300
extern struct dm_Ps;
#endif

struct dm *dmp = &dm_Null;	/* Ptr to current Display Manager package */

/* The [0] entry will be the startup default */
static struct dm *which_dm[] = {
	&dm_Null,		/* This should go first */
	&dm_Tek,
	&dm_Ir,
#ifdef BSD42
	&dm_Mg,
	&dm_Vg,
	&dm_Rat,
	&dm_Mer,
#endif
#ifdef PS300
	&dm_Ps,
#endif PS300
	0
};

release()
{
	register struct solid *sp;

	/* Delete all references to display processor memory */
	FOR_ALL_SOLIDS( sp )  {
		memfree( &(dmp->dmr_map), sp->s_bytes, (unsigned long)sp->s_addr );
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

		no_memory = 0;
		if( dmp->dmr_open() )
			break;

		(void)printf("ATTACHING %s (%s)\n",
			dmp->dmr_name, dmp->dmr_lname);

		FOR_ALL_SOLIDS( sp )  {
			/* Write vector subs into new display processor */
			if( (sp->s_bytes = dmp->dmr_cvtvecs( sp )) != 0 )  {
				sp->s_addr = memalloc( &(dmp->dmr_map), sp->s_bytes );
				if( sp->s_addr == 0 )  break;
				sp->s_bytes = dmp->dmr_load(sp->s_addr, sp->s_bytes);
			} else {
				sp->s_addr = 0;
				sp->s_bytes = 0;
			}
		}
		dmp->dmr_colorchange();
		color_soltab();
		dmp->dmr_viewchange( DM_CHGV_REDO, SOLID_NULL );
		dmaflag++;
		return;
	}
	(void)printf("attach(%s): BAD\n", name);
	dmp = &dm_Null;
}

static int Nu_int0() { return(0); }
static void Nu_void() { ; }
static unsigned Nu_unsign() { return(0); }

/* ARGSUSED */
static
Nu_input( fd, noblock )
{
	long readfds = (1<<fd);
	(void)select( 32, &readfds, 0L, 0L, (char *)0 );
	return(1);
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

	/* If non-interactive, don't attach a device and don't ask */
	if( !isatty(0) )  {
		attach( "nu" );
		return;
	}

	while(1)  {
		(void)printf("attach (");
		for( dp = &which_dm[0]; *dp != (struct dm *)0; dp++ )
			(void)printf("%s|", (*dp)->dmr_name);
		(void)printf(")[%s]? ", which_dm[0]->dmr_name);
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
