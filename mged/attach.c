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

#include "ged_types.h"
#include "3d.h"
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
extern struct dm dm_Mg, dm_Vg;
extern struct dm *dmp;		/* Ptr to current Display Manager package */

static struct dm *which_dm[] = {
	&dm_Null,
	&dm_Mg,
	&dm_Vg,
	0
};

release()
{
	register struct solid *sp;

	printf("releasing %s\n", dmp->dmr_name);

	/* Delete all references to display processor memory */
	FOR_ALL_SOLIDS( sp )  {
		memfree( &(dmp->dmr_map), sp->s_bytes, sp->s_addr );
		sp->s_bytes = 0;
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
		printf("attach(%s) %s\n",
			dmp->dmr_name, dmp->dmr_lname);

		dmp->dmr_open();

		FOR_ALL_SOLIDS( sp )  {
			/* Write vector subs into new display processor */
			sp->s_bytes = dmp->dmr_cvtvecs( sp->s_vlist,
				sp->s_center, sp->s_size,
				sp->s_soldash, sp->s_vlen );

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
