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

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

extern void color_soltab();
/*
 *  It is expected that entries on this mater list will be sorted
 *  in strictly ascending order, with no overlaps (ie, monotonicly
 * increasing).
 */
extern struct mater *rt_material_head;	/* now defined in librt/mater.c */

void color_putrec(), color_zaprec();

static void
pr_mater( mp )
register struct mater *mp;
{
  char buf[128];
  struct bu_vls vls;

  bu_vls_init(&vls);

  (void)sprintf(buf, "%5d..%d", mp->mt_low, mp->mt_high );
  vls_col_item(&vls, buf);
  (void)sprintf( buf, "%3d,%3d,%3d", mp->mt_r, mp->mt_g, mp->mt_b);
  vls_col_item(&vls, buf);
  vls_col_eol(&vls);

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
}

/*
 *  			F _ P R C O L O R
 */
int
f_prcolor(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register struct mater *mp;

  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help prcolor");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( rt_material_head == MATER_NULL )  {
    Tcl_AppendResult(interp, "none\n", (char *)NULL);
    return TCL_OK;
  }

  for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )
    pr_mater( mp );

  return TCL_OK;
}

/*
 *  			F _ C O L O R
 *  
 *  Add a color table entry.
 */
int
f_color(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct mater *newp,*next_mater;

	CHECK_READ_ONLY;

	if(argc < 7 || 7 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help color");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Delete all color records from the database */
	newp = rt_material_head;
	while( newp != MATER_NULL )
	{
		next_mater = newp->mt_forw;
		color_zaprec( newp );
		newp = next_mater;
	}

	/* construct the new color record */
	BU_GETSTRUCT( newp, mater );
	newp->mt_low = atoi( argv[1] );
	newp->mt_high = atoi( argv[2] );
	newp->mt_r = atoi( argv[3] );
	newp->mt_g = atoi( argv[4] );
	newp->mt_b = atoi( argv[5] );
	newp->mt_daddr = MATER_NO_ADDR;		/* not in database yet */

	/* Insert new color record in the in-memory list */
	rt_insert_color( newp );

	/* Write new color records for all colors in the list */
	newp = rt_material_head;
	while( newp != MATER_NULL )
	{
		next_mater = newp->mt_forw;
		color_putrec( newp );
		newp = next_mater;
	}

	color_soltab();
#if 0
	dmp->dm_colorchange(dmp);
#endif

	return TCL_OK;
}

/*
 *  			F _ E D C O L O R
 *  
 *  Print color table in easy to scanf() format,
 *  invoke favorite text editor to allow user to
 *  fiddle, then reload incore structures from file,
 *  and update database.
 */
int
f_edcolor(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	static char tempfile[] = "/tmp/MGED.aXXXXX";
	register struct mater *mp;
	register struct mater *zot;
	register FILE *fp;
	char line[128];
	static char hdr[] = "LOW\tHIGH\tRed\tGreen\tBlue\n";

	CHECK_READ_ONLY;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help edcolor");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	(void)mktemp(tempfile);
	if( (fp = fopen( tempfile, "w" )) == NULL )  {
		perror(tempfile);
		return TCL_ERROR;;
	}

	(void)fprintf( fp, hdr );
	for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
		(void)fprintf( fp, "%d\t%d\t%3d\t%3d\t%3d",
			mp->mt_low, mp->mt_high,
			mp->mt_r, mp->mt_g, mp->mt_b );
		(void)fprintf( fp, "\n" );
	}
	(void)fclose(fp);

	if( !editit( tempfile ) )  {
	  Tcl_AppendResult(interp, "Editor returned bad status.  Aborted\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Read file and process it */
	if( (fp = fopen( tempfile, "r")) == NULL )  {
	  perror( tempfile );
	  return TCL_ERROR;
	}

	if( fgets(line, sizeof (line), fp) == NULL  ||
	    line[0] != hdr[0] )  {
	  Tcl_AppendResult(interp, "Header line damaged, aborting\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Zap all the current records, both in core and on disk */
	while( rt_material_head != MATER_NULL )  {
		zot = rt_material_head;
		rt_material_head = rt_material_head->mt_forw;
		color_zaprec( zot );
		bu_free( (genptr_t)zot, "mater rec" );
	}

	while( fgets(line, sizeof (line), fp) != NULL )  {
		int cnt;
		int low, hi, r, g, b;

		cnt = sscanf( line, "%d %d %d %d %d",
			&low, &hi, &r, &g, &b );
		if( cnt != 5 )  {
		  Tcl_AppendResult(interp, "Discarding ",
				   line, "\n", (char *)NULL);
			continue;
		}
		BU_GETSTRUCT( mp, mater );
		mp->mt_low = low;
		mp->mt_high = hi;
		mp->mt_r = r;
		mp->mt_g = g;
		mp->mt_b = b;
		mp->mt_daddr = MATER_NO_ADDR;
		rt_insert_color( mp );
		color_putrec( mp );
	}
	(void)fclose(fp);
	(void)unlink( tempfile );

	color_soltab();
#if 0
	dmp->dm_colorchange(dmp);
#endif

	return TCL_OK;
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

	if( dbip->dbi_read_only )
		return;
	rec.md.md_id = ID_MATERIAL;
	rec.md.md_low = mp->mt_low;
	rec.md.md_hi = mp->mt_high;
	rec.md.md_r = mp->mt_r;
	rec.md.md_g = mp->mt_g;
	rec.md.md_b = mp->mt_b;

	/* Fake up a directory entry for db_* routines */
	dir.d_namep = "color_putrec";
	dir.d_magic = RT_DIR_MAGIC;
	if( mp->mt_daddr == MATER_NO_ADDR )  {
		/* Need to allocate new database space */
		if( db_alloc( dbip, &dir, 1 ) < 0 )  ALLOC_ERR_return;
		mp->mt_daddr = dir.d_addr;
	} else {
		dir.d_addr = mp->mt_daddr;
		dir.d_len = 1;
	}
	if( db_put( dbip, &dir, &rec, 0, 1 ) < 0 )  WRITE_ERR_return;
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

	if( dbip->dbi_read_only || mp->mt_daddr == MATER_NO_ADDR )
		return;
	dir.d_magic = RT_DIR_MAGIC;
	dir.d_namep = "color_zaprec";
	dir.d_len = 1;
	dir.d_addr = mp->mt_daddr;
	if( db_delete( dbip, &dir ) < 0 )  DELETE_ERR_return("color_zaprec");
	mp->mt_daddr = MATER_NO_ADDR;
}

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

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
			if( sp->s_regionid <= mp->mt_high &&
			    sp->s_regionid >= mp->mt_low ) {
			    	sp->s_color[0] = mp->mt_r;
			    	sp->s_color[1] = mp->mt_g;
			    	sp->s_color[2] = mp->mt_b;
				goto done;
			}
		}
		/*
		 *  There is no region-id-based coloring entry in the
		 *  table, so use the combination-record ("matter"
		 *  command) based color instead.
		 *  This is the "new way" of coloring things.
		 */
		sp->s_color[0] = sp->s_basecolor[0];
		sp->s_color[1] = sp->s_basecolor[1];
		sp->s_color[2] = sp->s_basecolor[2];
done: ;
	}
	dmaflag = 1;		/* re-write control list with new colors */
}
