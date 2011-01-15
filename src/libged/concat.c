/*                         C O N C A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file concat.c
 *
 * The concat command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"

#include "./ged_private.h"


/**
 * structure used by the dbconcat command for keeping tract of where
 * objects are being copied to and from, and what type of affix to
 * use.
 */
struct ged_concat_data {
    int copy_mode;
    struct db_i *old_dbip;
    struct db_i *new_dbip;
    struct bu_vls affix;
};

#define NO_AFFIX	1<<0
#define AUTO_PREFIX	1<<1
#define AUTO_SUFFIX	1<<2
#define CUSTOM_PREFIX	1<<3
#define CUSTOM_SUFFIX	1<<4

static int
ged_copy_object(struct ged		*gedp,
		struct directory	*input_dp,
		struct db_i		*input_dbip,
		struct db_i		*curr_dbip,
		Tcl_HashTable		*name_tbl,
		Tcl_HashTable		*used_names_tbl,
		struct ged_concat_data	*cc_data);

int
ged_concat(struct ged *gedp, int argc, const char *argv[])
{
    struct db_i		*newdbp;
    int			bad = 0;
    struct directory	*dp;
    Tcl_HashTable	name_tbl;
    Tcl_HashTable	used_names_tbl;
    Tcl_HashEntry	*ptr;
    Tcl_HashSearch	search;
    struct ged_concat_data	cc_data;
    const char *oldfile;
    static const char *usage = "[-s|-p] file.g [suffix|prefix]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if ((argc < 2) ||
	(argc > 4) ||
	(argv[1][0] != '-' && argc > 3) ||
	(argv[1][0] == '-' && (argc < 3 || argc > 4))) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_init( &cc_data.affix );
    cc_data.copy_mode = 0;

    if ( argv[1][0] == '-' ) {
	/* specified suffix or prefix explicitly */

	oldfile = argv[2];

	if ( argv[1][1] == 'p' ) {

	    cc_data.copy_mode |= AUTO_PREFIX;

	    if (argc == 3 || BU_STR_EQUAL(argv[3], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[3]);
		cc_data.copy_mode |= CUSTOM_PREFIX;
	    }

	} else if ( argv[1][1] == 's' ) {

	    cc_data.copy_mode |= AUTO_SUFFIX;

	    if (argc == 3 || BU_STR_EQUAL(argv[3], "/")) {
		cc_data.copy_mode = NO_AFFIX | CUSTOM_SUFFIX;
	    } else {
		(void)bu_vls_strcpy(&cc_data.affix, argv[3]);
		cc_data.copy_mode |= CUSTOM_SUFFIX;
	    }

	} else {
	    bu_vls_free( &cc_data.affix );
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

    } else {
	/* no prefix/suffix preference, use prefix */

	oldfile = argv[1];

	cc_data.copy_mode |= AUTO_PREFIX;

	if (argc == 2 || BU_STR_EQUAL(argv[2], "/")) {
	    cc_data.copy_mode = NO_AFFIX | CUSTOM_PREFIX;
	} else {
	    (void)bu_vls_strcpy(&cc_data.affix, argv[2]);
	    cc_data.copy_mode |= CUSTOM_PREFIX;
	}

    }

    if ( gedp->ged_wdbp->dbip->dbi_version < 5 ) {
	if ( bu_vls_strlen(&cc_data.affix) > _GED_V4_MAXNAME-1) {
	    bu_log("ERROR: affix [%s] is too long for v%d\n", bu_vls_addr(&cc_data.affix), gedp->ged_wdbp->dbip->dbi_version);
	    bu_vls_free( &cc_data.affix );
	    return GED_ERROR;
	}
    }

    /* open the input file */
    if ((newdbp = db_open(oldfile, "r")) == DBI_NULL) {
	bu_vls_free( &cc_data.affix );
	perror(oldfile);
	bu_vls_printf(&gedp->ged_result_str, "%s: Can't open %s", argv[0], oldfile);
	return GED_ERROR;
    }

    if ( newdbp->dbi_version > 4 && gedp->ged_wdbp->dbip->dbi_version < 5 ) {
	bu_vls_free( &cc_data.affix );
	bu_vls_printf(&gedp->ged_result_str, "%s: databases are incompatible, use dbupgrade on %s first",
		      argv[0], gedp->ged_wdbp->dbip->dbi_filename);
	return GED_ERROR;
    }

    db_dirbuild( newdbp );

    cc_data.new_dbip = newdbp;
    cc_data.old_dbip = gedp->ged_wdbp->dbip;

    /* visit each directory pointer in the input database */
    Tcl_InitHashTable( &name_tbl, TCL_STRING_KEYS );
    Tcl_InitHashTable( &used_names_tbl, TCL_STRING_KEYS );

    FOR_ALL_DIRECTORY_START( dp, newdbp ) {
	if ( dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY ) {
	    /* skip GLOBAL object */
	    continue;
	}
	ged_copy_object(gedp, dp, newdbp, gedp->ged_wdbp->dbip, &name_tbl, &used_names_tbl, &cc_data);
    } FOR_ALL_DIRECTORY_END;

    bu_vls_free( &cc_data.affix );
    rt_mempurge(&(newdbp->dbi_freep));

    /* Free all the directory entries, and close the input database */
    db_close(newdbp);

    db_sync(gedp->ged_wdbp->dbip);	/* force changes to disk */

    /* Free the Hash tables */
    ptr = Tcl_FirstHashEntry( &name_tbl, &search );
    while ( ptr ) {
	bu_free( (char *)Tcl_GetHashValue( ptr ), "new name" );
	ptr = Tcl_NextHashEntry( &search );
    }
    Tcl_DeleteHashTable( &name_tbl );
    Tcl_DeleteHashTable( &used_names_tbl );

    return bad ? GED_ERROR : GED_OK;
}

/**
 * find a new unique name given a list of previously used names, and
 * the type of naming mode that described what type of affix to use.
 */
static char *
ged_get_new_name(const char		*name,
		 struct db_i		*dbip,
		 Tcl_HashTable		*name_tbl,
		 Tcl_HashTable		*used_names_tbl,
		 struct ged_concat_data	*cc_data)
{
    struct bu_vls new_name;
    Tcl_HashEntry *ptr = NULL;
    char *aname = NULL;
    char *ret_name = NULL;
    int new=0;
    long num=0;

    RT_CK_DBI(dbip);
    BU_ASSERT(name_tbl);
    BU_ASSERT(used_names_tbl);
    BU_ASSERT(cc_data);

    if (!name) {
	bu_log("WARNING: encountered NULL name, renaming to \"UNKNOWN\"\n");
	name = "UNKNOWN";
    }

    ptr = Tcl_CreateHashEntry( name_tbl, name, &new );

    if ( !new ) {
	return (char *)Tcl_GetHashValue( ptr );
    }

    bu_vls_init( &new_name );

    do {
	/* iterate until we find an object name that is not in
	 * use, trying to accommodate the user's requested affix
	 * naming mode.
	 */

	bu_vls_trunc( &new_name, 0 );

	if (cc_data->copy_mode & NO_AFFIX) {
	    if (num > 0 && cc_data->copy_mode & CUSTOM_PREFIX) {
		/* auto-increment prefix */
		bu_vls_printf( &new_name, "%ld_", num );
	    }
	    bu_vls_strcat( &new_name, name );
	    if (num > 0 && cc_data->copy_mode & CUSTOM_SUFFIX) {
		/* auto-increment suffix */
		bu_vls_printf( &new_name, "_%ld", num );
	    }
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    /* use custom suffix */
	    bu_vls_strcpy( &new_name, name );
	    if (num > 0) {
		bu_vls_printf( &new_name, "_%ld_", num );
	    }
	    bu_vls_vlscat( &new_name, &cc_data->affix );
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    /* use custom prefix */
	    bu_vls_vlscat( &new_name, &cc_data->affix );
	    if (num > 0) {
		bu_vls_printf( &new_name, "_%ld_", num );
	    }
	    bu_vls_strcat( &new_name, name );
	} else if (cc_data->copy_mode & AUTO_SUFFIX) {
	    /* use auto-incrementing suffix */
	    bu_vls_strcat( &new_name, name );
	    bu_vls_printf( &new_name, "_%ld", num );
	} else if (cc_data->copy_mode & AUTO_PREFIX) {
	    /* use auto-incrementing prefix */
	    bu_vls_printf( &new_name, "%ld_", num );
	    bu_vls_strcat( &new_name, name );
	} else {
	    /* no custom suffix/prefix specified, use prefix */
	    if (num > 0) {
		bu_vls_printf( &new_name, "_%ld", num );
	    }
	    bu_vls_strcpy( &new_name, name );
	}

	/* make sure it fits for v4 */
	if ( cc_data->old_dbip->dbi_version < 5 ) {
	    if (bu_vls_strlen(&new_name) > _GED_V4_MAXNAME) {
		bu_log("ERROR: generated new name [%s] is too long (%d > %d)\n", bu_vls_addr(&new_name), bu_vls_strlen(&new_name), _GED_V4_MAXNAME);
	    }
	    return NULL;
	}
	aname = bu_vls_addr( &new_name );

	num++;

    } while (db_lookup( dbip, aname, LOOKUP_QUIET ) != DIR_NULL ||
	     Tcl_FindHashEntry( used_names_tbl, aname ) != NULL);

    /* if they didn't get what they asked for, warn them */
    if (num > 1) {
	if (cc_data->copy_mode & NO_AFFIX) {
	    bu_log("WARNING: unable to import [%s] without an affix, imported as [%s]\n", name, bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_SUFFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, name, bu_vls_addr(&cc_data->affix), bu_vls_addr(&new_name));
	} else if (cc_data->copy_mode & CUSTOM_PREFIX) {
	    bu_log("WARNING: unable to import [%s] as [%s%s], imported as [%s]\n", name, bu_vls_addr(&cc_data->affix), name, bu_vls_addr(&new_name));
	}
    }

    /* we should now have a unique name.  store it in the hash */
    ret_name = bu_vls_strgrab( &new_name );
    Tcl_SetHashValue( ptr, (ClientData)ret_name );
    (void)Tcl_CreateHashEntry( used_names_tbl, ret_name, &new );
    bu_vls_free( &new_name );

    return ret_name;
}

/**
 *
 *
 */
static void
ged_adjust_names(union tree		*trp,
		 struct db_i		*dbip,
		 Tcl_HashTable		*name_tbl,
		 Tcl_HashTable		*used_names_tbl,
		 struct ged_concat_data	*cc_data)
{
    char *new_name;

    if ( trp == NULL ) {
	return;
    }

    switch ( trp->tr_op ) {
	case OP_DB_LEAF:
	    new_name = ged_get_new_name(trp->tr_l.tl_name, dbip,
					name_tbl, used_names_tbl, cc_data);
	    if ( new_name ) {
		bu_free( trp->tr_l.tl_name, "leaf name" );
		trp->tr_l.tl_name = bu_strdup( new_name );
	    }
	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    ged_adjust_names(trp->tr_b.tb_left, dbip,
			     name_tbl, used_names_tbl, cc_data);
	    ged_adjust_names(trp->tr_b.tb_right, dbip,
			     name_tbl, used_names_tbl, cc_data);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    ged_adjust_names(trp->tr_b.tb_left, dbip,
			     name_tbl, used_names_tbl, cc_data);
	    break;
    }
}

static int
ged_copy_object(struct ged		*gedp,
		struct directory	*input_dp,
		struct db_i		*input_dbip,
		struct db_i		*curr_dbip,
		Tcl_HashTable		*name_tbl,
		Tcl_HashTable		*used_names_tbl,
		struct ged_concat_data	*cc_data)
{
    struct rt_db_internal ip;
    struct rt_extrude_internal *extr;
    struct rt_dsp_internal *dsp;
    struct rt_comb_internal *comb;
    struct directory *new_dp;
    char *new_name;

    if ( rt_db_get_internal( &ip, input_dp, input_dbip, NULL, &rt_uniresource) < 0 ) {
	bu_vls_printf(&gedp->ged_result_str,
		      "Failed to get internal form of object (%s) - aborting!!!\n",
		      input_dp->d_namep);
	return GED_ERROR;
    }

    if ( ip.idb_major_type == DB5_MAJORTYPE_BRLCAD ) {
	/* adjust names of referenced object in any object that reference other objects */
	switch ( ip.idb_minor_type ) {
	    case DB5_MINORTYPE_BRLCAD_COMBINATION:
		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		ged_adjust_names(comb->tree, curr_dbip, name_tbl, used_names_tbl, cc_data);
		break;
	    case DB5_MINORTYPE_BRLCAD_EXTRUDE:
		extr = (struct rt_extrude_internal *)ip.idb_ptr;
		RT_EXTRUDE_CK_MAGIC( extr );

		new_name = ged_get_new_name( extr->sketch_name, curr_dbip, name_tbl, used_names_tbl, cc_data );
		if ( new_name ) {
		    bu_free( extr->sketch_name, "sketch name" );
		    extr->sketch_name = bu_strdup( new_name );
		}
		break;
	    case DB5_MINORTYPE_BRLCAD_DSP:
		dsp = (struct rt_dsp_internal *)ip.idb_ptr;
		RT_DSP_CK_MAGIC( dsp );

		if ( dsp->dsp_datasrc == RT_DSP_SRC_OBJ ) {
		    /* This dsp references a database object, may need to change its name */
		    new_name = ged_get_new_name( bu_vls_addr( &dsp->dsp_name ), curr_dbip,
						 name_tbl, used_names_tbl, cc_data );
		    if ( new_name ) {
			bu_vls_free( &dsp->dsp_name );
			bu_vls_strcpy( &dsp->dsp_name, new_name );
		    }
		}
		break;
	}
    }

    new_name = ged_get_new_name(input_dp->d_namep, curr_dbip, name_tbl, used_names_tbl, cc_data );
    if ( !new_name ) {
	new_name = input_dp->d_namep;
    }
    if ( (new_dp = db_diradd( curr_dbip, new_name, RT_DIR_PHONY_ADDR, 0, input_dp->d_flags,
			      (genptr_t)&input_dp->d_minor_type ) ) == DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str,
		      "Failed to add new object name (%s) to directory - aborting!!\n",
		      new_name);
	return GED_ERROR;
    }

    if ( rt_db_put_internal( new_dp, curr_dbip, &ip, &rt_uniresource ) < 0 )  {
	bu_vls_printf(&gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      new_name);
	return GED_ERROR;
    }

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
