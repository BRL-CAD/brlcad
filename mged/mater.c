/*
 *  			M A T E R . C
 *  
 *  Code to deal with establishing and maintaining the tables which
 *  map region ID codes into worthwhile material information
 *  (colors and outboard database "handles").
 *
 *  Functions -
 *	f_prcolor	Print color & material table
 *	f_color		Add a color & material table entry
 *	f_edcolor	Invoke text editor on color table
 *	color_addrec	Called by dir_build on startup
 *	color_soltab	Apply colors to the solid table
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
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "./ged.h"
#include "./objdir.h"
#include "./solid.h"
#include "./dm.h"

extern char	*mktemp();

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */
extern int	read_only;	/* database is read-only */

/*
 *  It is expected that entries on this mater list will be sorted
 *  in strictly ascending order, with no overlaps (ie, monotonicly
 * increasing).
 */
extern struct mater *MaterHead;	/* now defined in librt/mater.c */

void color_addrec(), color_putrec(), color_zaprec();

static void
pr_mater( mp )
register struct mater *mp;
{
	char buf[128];

	(void)sprintf( buf, "%5d..%d", mp->mt_low, mp->mt_high );
	col_item( buf );
	(void)sprintf( buf, "%3d,%3d,%3d", mp->mt_r, mp->mt_g, mp->mt_b);
	col_item( buf );
	(void)sprintf( buf, "dm%d", mp->mt_dm_int );
	col_item( buf );
	col_eol();
}

/*
 *  			F _ P R C O L O R
 */
void
f_prcolor()
{
	register struct mater *mp;

	if( MaterHead == MATER_NULL )  {
		(void)printf("none\n");
		return;
	}
	for( mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw )
		pr_mater( mp );
}

/*
 *  			I N S E R T _ C O L O R
 *
 *  While any additional database records are created and written here,
 *  it is the responsibility of the caller to color_putrec(newp) if needed.
 */
static void
insert_color( newp )
register struct mater *newp;
{
	register struct mater *mp;
	register struct mater *zot;

	if( MaterHead == MATER_NULL || newp->mt_high < MaterHead->mt_low )  {
		/* Insert at head of list */
		newp->mt_forw = MaterHead;
		MaterHead = newp;
		return;
	}
	if( newp->mt_low < MaterHead->mt_low )  {
		/* Insert at head of list, check for redefinition */
		newp->mt_forw = MaterHead;
		MaterHead = newp;
		goto check_overlap;
	}
	for( mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw )  {
		if( mp->mt_low == newp->mt_low  &&
		    mp->mt_high <= newp->mt_high )  {
			(void)printf("dropping overwritten entry:\n");
			newp->mt_forw = mp->mt_forw;
			pr_mater( mp );
			color_zaprec( mp );
			*mp = *newp;		/* struct copy */
			free( newp );
			newp = mp;
			goto check_overlap;
		}
		if( mp->mt_low  < newp->mt_low  &&
		    mp->mt_high > newp->mt_high )  {
			/* New range entirely contained in old range; split */
			(void)printf("Splitting into 3 ranges\n");
			GETSTRUCT( zot, mater );
			*zot = *mp;		/* struct copy */
			zot->mt_daddr = MATER_NO_ADDR;
			/* zot->mt_high = mp->mt_high; */
			zot->mt_low = newp->mt_high+1;
			mp->mt_high = newp->mt_low-1;
			/* mp, newp, zot */
			/* zot->mt_forw = mp->mt_forw; */
			newp->mt_forw = zot;
			mp->mt_forw = newp;
			color_putrec( mp );
			color_putrec( zot );	/* newp put by caller */
			pr_mater( mp );
			pr_mater( newp );
			pr_mater( zot );
			return;
		}
		if( mp->mt_high > newp->mt_low )  {
			/* Overlap to the left: Shorten preceeding entry */
			(void)printf("Shortening lhs range, from:\n");
			pr_mater( mp );
			(void)printf("to:\n");
			mp->mt_high = newp->mt_low-1;
			color_putrec( mp );		/* it was changed */
			pr_mater( mp );
			/* Now append */
			newp->mt_forw = mp->mt_forw;
			mp->mt_forw = newp;
			goto check_overlap;
		}
		if( mp->mt_forw == MATER_NULL ||
		    newp->mt_low < mp->mt_forw->mt_low )  {
			/* Append */
			newp->mt_forw = mp->mt_forw;
			mp->mt_forw = newp;
			goto check_overlap;
		}
	}
	(void)printf("fell out of insert_color loop, append to end\n");
	/* Append at end */
	newp->mt_forw = MATER_NULL;
	mp->mt_forw = newp;
	f_prcolor();
	return;

	/* Check for overlap, ie, redefinition of following colors */
check_overlap:
	while( newp->mt_forw != MATER_NULL &&
	       newp->mt_high >= newp->mt_forw->mt_low )  {
		if( newp->mt_high >= newp->mt_forw->mt_high )  {
			/* Drop this mater struct */
			zot = newp->mt_forw;
			newp->mt_forw = zot->mt_forw;
			(void)printf("dropping overlaping entry:\n");
			pr_mater( zot );
			color_zaprec( zot );
			free( zot );
			continue;
		}
		if( newp->mt_high >= newp->mt_forw->mt_low )  {
			/* Shorten this mater struct, then done */
			(void)printf("Shortening rhs range, from:\n");
			pr_mater( newp->mt_forw );
			(void)printf("to:\n");
			newp->mt_forw->mt_low = newp->mt_high+1;
			color_putrec( newp->mt_forw );
			pr_mater( newp->mt_forw );
			continue;	/* more conservative than returning */
		}
	}
}

/*
 *  			F _ C O L O R
 *  
 *  Add a color table entry.
 */
void
f_color()
{
	register struct mater *newp;

	GETSTRUCT( newp, mater );
	newp->mt_low = atoi( cmd_args[1] );
	newp->mt_high = atoi( cmd_args[2] );
	newp->mt_r = atoi( cmd_args[3] );
	newp->mt_g = atoi( cmd_args[4] );
	newp->mt_b = atoi( cmd_args[5] );
	newp->mt_daddr = MATER_NO_ADDR;		/* not in database yet */
	insert_color( newp );
	color_putrec( newp );			/* write to database */
	dmp->dmr_colorchange();
}

/*
 *  			F _ E D C O L O R
 *  
 *  Print color table in easy to scanf() format,
 *  invoke favorite text editor to allow user to
 *  fiddle, then reload incore structures from file,
 *  and update database.
 */
void
f_edcolor()
{
	static char tempfile[] = "/tmp/MGED.aXXXXX";
	register struct mater *mp;
	register struct mater *zot;
	register FILE *fp;
	char line[128];
	static char hdr[] = "LOW\tHIGH\tRed\tGreen\tBlue\n";

	(void)mktemp(tempfile);
	if( (fp = fopen( tempfile, "w" )) == NULL )  {
		perror(tempfile);
		return;
	}

	(void)fprintf( fp, hdr );
	for( mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw )  {
		(void)fprintf( fp, "%d\t%d\t%3d\t%3d\t%3d",
			mp->mt_low, mp->mt_high,
			mp->mt_r, mp->mt_g, mp->mt_b );
		(void)fprintf( fp, "\n" );
	}
	(void)fclose(fp);

	if( !editit( tempfile ) )  {
		(void)printf("Editor returned bad status.  Aborted\n");
		return;
	}

	/* Read file and process it */
	if( (fp = fopen( tempfile, "r")) == NULL )  {
		perror( tempfile );
		return;
	}
	if( fgets(line, sizeof (line), fp) == NULL  ||
	    line[0] != hdr[0] )  {
		(void)printf("Header line damaged, aborting\n");
		return;
	}

	/* Zap all the current records, both in core and on disk */
	while( MaterHead != MATER_NULL )  {
		zot = MaterHead;
		MaterHead = MaterHead->mt_forw;
		color_zaprec( zot );
		free( zot );
	}

	while( fgets(line, sizeof (line), fp) != NULL )  {
		int cnt;
		int low, hi, r, g, b;

		cnt = sscanf( line, "%d %d %d %d %d",
			&low, &hi, &r, &g, &b );
		if( cnt != 5 )  {
			(void)printf("Discarding %s\n", line );
			continue;
		}
		GETSTRUCT( mp, mater );
		mp->mt_low = low;
		mp->mt_high = hi;
		mp->mt_r = r;
		mp->mt_g = g;
		mp->mt_b = b;
		mp->mt_daddr = MATER_NO_ADDR;
		insert_color( mp );
		color_putrec( mp );
	}
	(void)fclose(fp);
	(void)unlink( tempfile );
	dmp->dmr_colorchange();
}

/*
 *  			C O L O R _ A D D R E C
 *  
 *  Called from dir_build() when initially scanning database.
 */
void
color_addrec( recp, addr )
union record *recp;
long addr;
{
	register struct mater *mp;

	GETSTRUCT( mp, mater );
	mp->mt_low = recp->md.md_low;
	mp->mt_high = recp->md.md_hi;
	mp->mt_r = recp->md.md_r;
	mp->mt_g = recp->md.md_g;
	mp->mt_b = recp->md.md_b;
	mp->mt_daddr = addr;
	insert_color( mp );
}

/*
 *  			C O L O R _ P U T R E C
 *  
 *  Used to create a database record and get it written out to a granule.
 *  In some cases, storage will need to be allocated.
 */
void
color_putrec( mp )
register struct mater *mp;
{
	struct directory dir;
	union record rec;

	if( read_only )
		return;
	rec.md.md_id = ID_MATERIAL;
	rec.md.md_low = mp->mt_low;
	rec.md.md_hi = mp->mt_high;
	rec.md.md_r = mp->mt_r;
	rec.md.md_g = mp->mt_g;
	rec.md.md_b = mp->mt_b;

	/* Fake up a directory entry for db_* routines */
	dir.d_namep = "color_putrec";
	if( mp->mt_daddr == MATER_NO_ADDR )  {
		/* Need to allocate new database space */
		db_alloc( &dir, 1 );
		mp->mt_daddr = dir.d_addr;
	} else {
		dir.d_addr = mp->mt_daddr;
		dir.d_len = 1;
	}
	db_putrec( &dir, &rec, 0 );
}

/*
 *  			C O L O R _ Z A P R E C
 *  
 *  Used to release database resources occupied by a material record.
 */
void
color_zaprec( mp )
register struct mater *mp;
{
	struct directory dir;

	if( read_only || mp->mt_daddr == MATER_NO_ADDR )
		return;
	dir.d_namep = "color_zaprec";
	dir.d_len = 1;
	dir.d_addr = mp->mt_daddr;
	db_delete( &dir );
	mp->mt_daddr = MATER_NO_ADDR;
}

static struct mater default_mater = {
	0, 32767,
	DM_RED,
	255, 0, 0,
	MATER_NO_ADDR, 0
};

/*
 *  			C O L O R _ S O L T A B
 *
 *  Pass through the solid table and set pointer to appropriate
 *  mater structure.
 *  Called by the display manager anytime the color mappings change.
 */
void
color_soltab()
{
	register struct solid *sp;
	register struct mater *mp;

	FOR_ALL_SOLIDS( sp )  {
		for( mp = MaterHead; mp != MATER_NULL; mp = mp->mt_forw )  {
			if( sp->s_regionid <= mp->mt_high &&
			    sp->s_regionid >= mp->mt_low ) {
				sp->s_materp = (char *)mp;
				goto done;
			}
		}
		sp->s_materp = (char *)&default_mater;
done: ;
	}
	dmaflag = 1;		/* re-write control list with new colors */
}
