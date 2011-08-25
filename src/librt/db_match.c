/*                      D B _ M A T C H . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_match.c
 *
 * Routines to dtermine if a string matches a given pattern.
 *
 */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "rtgeom.h"
#include "raytrace.h"


/**
 * DEPRECATED: Use bu_fnmatch() instead of this function.
 *
 *			D B _ R E G E X P _ M A T C H
 *
 *	If string matches pattern, return 1, else return 0
 *
 *	special characters:
 *		*	Matches any string including the null string.
 *		?	Matches any single character.
 *		[...]	Matches any one of the characters enclosed.
 *		-	May be used inside brackets to specify range
 *			(i.e. str[1-58] matches str1, str2, ... str5, str8)
 *		\	Escapes special characters.
 */
int
db_regexp_match(register const char *pattern, register const char *string)
{
    if (bu_fnmatch(pattern, string, 0) == 0)
	return 1;

    return 0;
}


/**
 *			D B _ R E G E X P _ M A T C H _ A L L
 *
 * Appends a list of all database matches to the given vls, or the pattern
 * itself if no matches are found.
 * Returns the number of matches.
 */

int
db_regexp_match_all(struct bu_vls *dest, struct db_i *dbip, const char *pattern)
{
    register int i, num;
    register struct directory *dp;

    for ( i = num = 0; i < RT_DBNHASH; i++ )  {
	for ( dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw ) {
	    if (bu_fnmatch(pattern, dp->d_namep, 0) != 0)
		continue;
	    if ( num == 0 )
		bu_vls_strcat( dest, dp->d_namep );
	    else {
		bu_vls_strcat( dest, " " );
		bu_vls_strcat( dest, dp->d_namep );
	    }
	    ++num;
	}
    }

    return num;
}


HIDDEN void
db_count_refs(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t dummy1, genptr_t dummy2, genptr_t dummy3)
{
    struct directory	*dp;

    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE( comb_leaf );
    BU_ASSERT(dummy1 == NULL);
    BU_ASSERT(dummy2 == NULL);
    BU_ASSERT(dummy3 == NULL);

    if ( (dp=db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) != RT_DIR_NULL )
	++dp->d_nref;
}


/**
 *			D B _ U P D A T E _ N R E F
 *
 * Updates the d_nref fields (which count the number of times a given entry
 * is referenced by a COMBination in the database).
 *
 */

void
db_update_nref( struct db_i *dbip, struct resource *resp )
{
    register int			i;
    register struct directory      *dp;
    struct rt_db_internal		intern;
    struct rt_comb_internal	       *comb;

    RT_CK_DBI( dbip );
    RT_CK_RESOURCE(resp);

    /* First, clear any existing counts */
    for ( i = 0; i < RT_DBNHASH; i++ )
	for ( dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw )
	    dp->d_nref = 0;

    /* Examine all COMB nodes */
    for ( i = 0; i < RT_DBNHASH; i++ )  {
	for ( dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw ) {

	    /* handle non-combination objects that reference other objects */
	    if ( dp->d_major_type == DB5_MAJORTYPE_BRLCAD ) {
		struct directory *dp2;

		if ( dp->d_minor_type == DB5_MINORTYPE_BRLCAD_EXTRUDE ) {
		    struct rt_extrude_internal *extr;

		    if ( rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, resp) < 0 )
			continue;
		    extr = (struct rt_extrude_internal *)intern.idb_ptr;
		    RT_EXTRUDE_CK_MAGIC( extr );
		    if ( extr->sketch_name ) {
			dp2 = db_lookup( dbip, extr->sketch_name, LOOKUP_QUIET );
			if ( dp2 != RT_DIR_NULL ) {
			    dp2->d_nref++;
			}
		    }
		    rt_db_free_internal(&intern);
		} else if ( dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_REVOLVE ) {
		    struct rt_revolve_internal *revolve;

		    if ( rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, resp) < 0 )
			continue;
		    revolve = (struct rt_revolve_internal *)intern.idb_ptr;
		    RT_REVOLVE_CK_MAGIC( revolve );
		    if ( bu_vls_strlen(&revolve->sketch_name) > 0 ) {
			dp2 = db_lookup( dbip, bu_vls_addr(&revolve->sketch_name), LOOKUP_QUIET );
			if ( dp2 != RT_DIR_NULL ) {
			    dp2->d_nref++;
			}
		    }
		    rt_db_free_internal(&intern);
		} else if ( dp->d_minor_type ==  DB5_MINORTYPE_BRLCAD_DSP ) {
		    struct rt_dsp_internal *dsp;

		    if ( rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, resp) < 0 )
			continue;
		    dsp = (struct rt_dsp_internal *)intern.idb_ptr;
		    RT_DSP_CK_MAGIC( dsp );
		    if ( dsp->dsp_datasrc == RT_DSP_SRC_OBJ && bu_vls_strlen( &dsp->dsp_name) > 0 ) {
			dp2 = db_lookup( dbip, bu_vls_addr( &dsp->dsp_name ), LOOKUP_QUIET );
			if ( dp2 != RT_DIR_NULL ) {
			    dp2->d_nref++;
			}
		    }
		    rt_db_free_internal(&intern);
		}
	    }
	    if ( !(dp->d_flags & RT_DIR_COMB) )
		continue;
	    if ( rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, resp) < 0 )
		continue;
	    if ( intern.idb_type != ID_COMBINATION )  {
		bu_log("NOTICE: %s was marked a combination, but isn't one?  Clearing flag\n",
		       dp->d_namep);
		dp->d_flags &= ~RT_DIR_COMB;
		rt_db_free_internal(&intern);
		continue;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    db_tree_funcleaf( dbip, comb, comb->tree,
			      db_count_refs, (genptr_t)NULL,
			      (genptr_t)NULL, (genptr_t)NULL );
	    rt_db_free_internal(&intern);
	}
    }
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
