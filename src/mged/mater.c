/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mater.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

/*
 *  It is expected that entries on this mater list will be sorted
 *  in strictly ascending order, with no overlaps (ie, monotonicly
 * increasing).
 */

void color_soltab(void);
void color_putrec(register struct mater *mp), color_zaprec(register struct mater *mp);

static char	tmpfil[17];
#ifndef _WIN32
static char	*tmpfil_init = "/tmp/GED.aXXXXXX";
#else
static char	*tmpfil_init = "c:\\GED.aXXXXXX";
#endif

/*
 *  			F _ E D C O L O R
 *
 *  Print color table in easy to scanf() format,
 *  invoke favorite text editor to allow user to
 *  fiddle, then reload incore structures from file,
 *  and update database.
 */
int
f_edcolor(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register struct mater *mp;
	register struct mater *zot;
	register FILE *fp;
	register int fd;
	char line[128];
	static char hdr[] = "LOW\tHIGH\tRed\tGreen\tBlue\n";
	int ret = TCL_OK;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help edcolor");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	strcpy(tmpfil, tmpfil_init);
#ifdef _WIN32
	(void)mktemp(tmpfil);
	if ((fp = fopen(tmpfil, "w")) == NULL) {
		perror(tmpfil);
		return TCL_ERROR;;
	}
#else
	if ((fd = mkstemp(tmpfil)) < 0) {
		perror(tmpfil);
		return TCL_ERROR;
	}
	if ((fp = fdopen(fd, "w+")) == NULL) {
		close(fd);
		perror(tmpfil);
		return TCL_ERROR;;
	}
#endif

	(void)fprintf( fp, hdr );
	for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
		(void)fprintf( fp, "%d\t%d\t%3d\t%3d\t%3d",
			mp->mt_low, mp->mt_high,
			mp->mt_r, mp->mt_g, mp->mt_b );
		(void)fprintf( fp, "\n" );
	}
	(void)fclose(fp);

	if( !editit( tmpfil ) )  {
	  Tcl_AppendResult(interp, "Editor returned bad status.  Aborted\n", (char *)NULL);
	  return TCL_ERROR;
	}

	/* Read file and process it */
	if( (fp = fopen( tmpfil, "r")) == NULL )  {
	  perror( tmpfil );
	  return TCL_ERROR;
	}

	if (fgets(line, sizeof (line), fp) == NULL ||
	    line[0] != hdr[0]) {
		Tcl_AppendResult(interp, "Header line damaged, aborting\n", (char *)NULL);
		return TCL_ERROR;
	}

	if (dbip->dbi_version < 5) {
		/* Zap all the current records, both in core and on disk */
		while (rt_material_head != MATER_NULL) {
			zot = rt_material_head;
			rt_material_head = rt_material_head->mt_forw;
			color_zaprec(zot);
			bu_free((genptr_t)zot, "mater rec");
		}

		while (fgets(line, sizeof (line), fp) != NULL) {
			int cnt;
			int low, hi, r, g, b;

			cnt = sscanf(line, "%d %d %d %d %d",
				     &low, &hi, &r, &g, &b);
			if (cnt != 5) {
				Tcl_AppendResult(interp, "Discarding ",
						 line, "\n", (char *)NULL);
				continue;
			}
			BU_GETSTRUCT(mp, mater);
			mp->mt_low = low;
			mp->mt_high = hi;
			mp->mt_r = r;
			mp->mt_g = g;
			mp->mt_b = b;
			mp->mt_daddr = MATER_NO_ADDR;
			rt_insert_color(mp);
			color_putrec(mp);
		}
	} else {
		struct bu_vls vls;

		/* free colors in rt_material_head */
		rt_color_free();

		bu_vls_init(&vls);

		while (fgets(line, sizeof (line), fp) != NULL) {
			int cnt;
			int low, hi, r, g, b;

			/* check to see if line is reasonable */
			cnt = sscanf(line, "%d %d %d %d %d",
				     &low, &hi, &r, &g, &b);
			if (cnt != 5) {
				Tcl_AppendResult(interp, "Discarding ",
						 line, "\n", (char *)NULL);
				continue;
			}
			bu_vls_printf(&vls, "{%d %d %d %d %d} ", low, hi, r, g, b);
		}

		db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&vls), dbip);
		db5_import_color_table(bu_vls_addr(&vls));
		bu_vls_free(&vls);
	}

	(void)fclose(fp);
	(void)unlink(tmpfil);

	color_soltab();

	return ret;
}

/*
 *  			C O L O R _ P U T R E C
 *
 *  Used to create a database record and get it written out to a granule.
 *  In some cases, storage will need to be allocated. v4 db only.
 */
void
color_putrec(register struct mater *mp)
{
	struct directory dir;
	union record rec;

	if(dbip == DBI_NULL)
	  return;

	if( dbip->dbi_read_only )
		return;

	if (dbip->dbi_version >= 5) {
	    bu_log("color_putrec does not work on db5 or later databases");
	    return;
	}

	rec.md.md_id = ID_MATERIAL;
	rec.md.md_low = mp->mt_low;
	rec.md.md_hi = mp->mt_high;
	rec.md.md_r = mp->mt_r;
	rec.md.md_g = mp->mt_g;
	rec.md.md_b = mp->mt_b;

	/* Fake up a directory entry for db_* routines */
	dir.d_namep = "color_putrec";
	dir.d_magic = RT_DIR_MAGIC;
	dir.d_flags = 0;
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
color_zaprec(register struct mater *mp)
{
	struct directory dir;

	if(dbip == DBI_NULL)
	  return;

	if( dbip->dbi_read_only || mp->mt_daddr == MATER_NO_ADDR )
		return;
	dir.d_magic = RT_DIR_MAGIC;
	dir.d_namep = "color_zaprec";
	dir.d_len = 1;
	dir.d_addr = mp->mt_daddr;
	dir.d_flags = 0;
	if( db_delete( dbip, &dir ) < 0 )  DELETE_ERR_return("color_zaprec");
	mp->mt_daddr = MATER_NO_ADDR;
}

/*
 *  			C O L O R _ S O L T A B
 *
 *  Pass through the solid table and set pointer to appropriate
 *  mater structure.
 */
void
color_soltab(void)
{
	register struct solid *sp;
	register struct mater *mp;

	FOR_ALL_SOLIDS( sp, &dgop->dgo_headSolid )  {
		sp->s_cflag = 0;

	        /* the user specified the color, so use it */
		if( sp->s_uflag ) {
			sp->s_color[0] = sp->s_basecolor[0];
			sp->s_color[1] = sp->s_basecolor[1];
			sp->s_color[2] = sp->s_basecolor[2];
			goto done;
		}

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
		 *  table, so use the combination-record ("mater"
		 *  command) based color if one was provided. Otherwise,
		 *  use the default wireframe color.
		 *  This is the "new way" of coloring things.
		 */

		/* use wireframe_default_color */
		if (sp->s_dflag)
		  sp->s_cflag = 1;
		/* Be conservative and copy color anyway, to avoid black */
		sp->s_color[0] = sp->s_basecolor[0];
		sp->s_color[1] = sp->s_basecolor[1];
		sp->s_color[2] = sp->s_basecolor[2];
done: ;
	}
	update_views = 1;		/* re-write control list with new colors */
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
