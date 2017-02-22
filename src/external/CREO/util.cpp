/*                    U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @file util.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"

extern "C" void
set_identity( ProMatrix xform )
{
    int i, j;

    for ( i=0; i<4; i++ ) {
	for ( j=0; j<4; j++ ) {
	    if ( i == j ) {
		xform[i][j] = 1.0;
	    } else {
		xform[i][j] = 0.0;
	    }
	}
    }
}

/* routine to check if xform is an identity */
extern "C" int
is_non_identity( ProMatrix xform )
{
    int i, j;

    for ( i=0; i<4; i++ ) {
	for ( j=0; j<4; j++ ) {
	    if ( i == j ) {
		if ( xform[i][j] != 1.0 )
		    return 1;
	    } else {
		if ( xform[i][j] != 0.0 )
		    return 1;
	    }
	}
    }

    return 0;
}

extern "C" ProError
dimension_filter( ProDimension *dim, ProAppData data ) {
    return PRO_TK_NO_ERROR;
}


extern "C" ProError
check_dimension( ProDimension *dim, ProError status, ProAppData data )
{
    ProDimensiontype dim_type;
    ProError ret;
    double tmp;

    if ( (ret=ProDimensionTypeGet( dim, &dim_type ) ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "ProDimensionTypeGet Failed for %s\n", curr_part_name );
	return ret;
    }

    switch ( dim_type ) {
	case PRODIMTYPE_RADIUS:
	    if ( (ret=ProDimensionValueGet( dim, &radius ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
		return ret;
	    }
	    diameter = 2.0 * radius;
	    got_diameter = 1;
	    break;
	case PRODIMTYPE_DIAMETER:
	    if ( (ret=ProDimensionValueGet( dim, &diameter ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
		return ret;
	    }
	    radius = diameter / 2.0;
	    got_diameter = 1;
	    break;
	case PRODIMTYPE_LINEAR:
	    if ( (ret=ProDimensionValueGet( dim, &tmp ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
		return ret;
	    }
	    if ( got_distance1 ) {
		distance2 = tmp;
	    } else {
		got_distance1 = 1;
		distance1 = tmp;
	    }
	    break;
    }

    return PRO_TK_NO_ERROR;
}

extern "C" void
lower_case( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	(*c) = tolower( *c );
	c++;
    }
}

extern "C" void
make_legal( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	if ( *c <= ' ' || *c == '/' || *c == '[' || *c == ']' ) {
	    *c = '_';
	} else if ( *c > '~' ) {
	    *c = '_';
	}
	c++;
    }
}

/* create a unique BRL-CAD object name from a possibly illegal name */
extern "C" char *
create_unique_name( char *name )
{
    struct bu_vls *tmp_name;
    std::pair<std::set<struct bu_vls *, StrCmp>::iterator, bool> ret;
    long count=0;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "create_unique_name( %s )\n", name );
    }

    /* create a unique name */
    BU_GET(tmp_name, struct bu_vls);
    bu_vls_init(tmp_name);
    bu_vls_strcpy(tmp_name, name);
    lower_case(bu_vls_addr(tmp_name));
    make_legal(bu_vls_addr(tmp_name));
    ret = brlcad_names.insert(tmp_name);
    while (ret.second == false) {
	(void)bu_namegen(tmp_name, NULL, NULL);
	count++;
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\tTrying %s\n", bu_vls_addr(tmp_name) );
	}
	if (count == LONG_MAX) {
	    bu_vls_free(tmp_name);
	    BU_PUT(tmp_name, struct bu_vls);
	    return NULL;
	}
	ret = brlcad_names.insert(tmp_name);
    }

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "\tnew name for %s is %s\n", name, bu_vls_addr(tmp_name) );
    }
    return bu_vls_addr(tmp_name);
}

extern "C" char *
get_brlcad_name( char *part_name )
{
    char *brlcad_name=NULL;
    struct bu_hash_entry *entry=NULL, *prev=NULL;
    int new_entry=0;
    unsigned long index=0;
    char *name_copy;

    name_copy = bu_strdup( part_name );
    lower_case( name_copy );

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "get_brlcad_name( %s )\n", name_copy );
    }

    /* find name for this part in hash table */
    entry = bu_hash_tbl_find( name_hash, (unsigned char *)name_copy, strlen( name_copy ), &prev, &index );

    if ( entry ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\treturning %s\n", (char *)bu_get_hash_value( entry ) );
	}
	bu_free( name_copy, "name_copy" );
	return (char *)bu_get_hash_value( entry );
    } else {

	/* must create a new name */
	brlcad_name = create_unique_name( name_copy );
	entry = bu_hash_tbl_add( name_hash, (unsigned char *)name_copy, strlen( name_copy ), &new_entry );
	bu_set_hash_value( entry, (unsigned char *)brlcad_name );
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\tCreating new brlcad name (%s) for part (%s)\n", brlcad_name, name_copy );
	}
	bu_free( name_copy, "name_copy" );
	return brlcad_name;
    }
}

extern "C" void
model_units( ProMdl model )
{
    // TODO - the following is completely untested - fix that!!
    ProUnitsystem us;
    ProUnititem lmu;
    char *lname = NULL;
    ProUnititem mmu;
    ProUnitConversion conv;
    double creo_conv;

    ProMdlPrincipalunitsystemGet(model, &us);
    ProUnitsystemUnitGet(&us, PRO_UNITTYPE_LENGTH, &lmu);
    ProUnitInit(model, L"mm", &mmu);
    ProWstringToString(lname, lmu.name);
    ProUnitConversionGet(&lmu, &conv, &mmu);
    creo_conv = conv.scale;

    /* adjust tolerance for Pro/E units */
    local_tol = tol_dist / creo_conv;
    local_tol_sq = local_tol * local_tol;
}


extern "C" void
add_to_empty_list( char *name )
{
    struct empty_parts *ptr;
    int found=0;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Adding %s to list of empty parts\n", name );
    }

    if ( empty_parts_root == NULL ) {
	BU_ALLOC(empty_parts_root, struct empty_parts);
	ptr = empty_parts_root;
    } else {
	ptr = empty_parts_root;
	while ( !found && ptr->next ) {
	    if ( BU_STR_EQUAL( name, ptr->name ) ) {
		found = 1;
		break;
	    }
	    ptr = ptr->next;
	}
	if ( !found ) {
	    BU_ALLOC(ptr->next, struct empty_parts);
	    ptr = ptr->next;
	}
    }

    ptr->next = NULL;
    ptr->name = (char *)bu_strdup( name );
}

extern "C" int
create_temp_directory()
{
    ProError status;
    int ret_status;

    empty_parts_root = NULL;

    /* use UI dialog */
    status = ProUIDialogCreate( "creo_brl", "creo_brl" );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to create dialog box for creo-brl, error = %d\n", status );
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "doit", doit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to set action for 'Go' button\n" );
	ProUIDialogDestroy( "creo_brl" );
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "quit", do_quit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to set action for 'Go' button\n" );
	ProUIDialogDestroy( "creo_brl" );
	return 0;
    }

    status = ProUIDialogActivate( "creo_brl", &ret_status );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Error in creo-brl Dialog, error = %d\n",
		status );
	fprintf( stderr, "\t dialog returned %d\n", ret_status );
    }

    return 0;
}

extern "C" void
free_hash_values( struct bu_hash_tbl *htbl )
{
    struct bu_hash_entry *entry;
    struct bu_hash_record rec;

    entry = bu_hash_tbl_first( htbl, &rec );

    while ( entry ) {
	bu_free( bu_get_hash_value( entry ), "hash entry" );
	entry = bu_hash_tbl_next( &rec );
    }
}

extern "C" struct bu_hash_tbl *
create_name_hash( FILE *name_fd )
{
    struct bu_hash_tbl *htbl;
    char line[MAX_LINE_LEN];
    struct bu_hash_entry *entry=NULL;
    long line_no=0;

    htbl = bu_hash_create( NUM_HASH_TABLE_BINS );

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "name hash created, now filling it:\n" );
    }
    while ( bu_fgets( line, MAX_LINE_LEN, name_fd ) ) {
	char *part_no, *part_name, *ptr;
	line_no++;

	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "line %ld: %s", line_no, line );
	}

	ptr = strtok( line, " \t\n" );
	if ( !ptr ) {
	    bu_log( "Warning: unrecognizable line in part name file (bad part number):\n\t%s\n", line );
	    bu_log( "\tIgnoring\n" );
	    continue;
	}
	part_no = bu_strdup( ptr );
	lower_case( part_no );

	/* match up to the EOL, everything up to it minus surrounding ws is the name */
	ptr = strtok( (char *)NULL, "\n" );
	if ( !ptr ) {
	    bu_log( "Warning: unrecognizable line in part name file (bad part name):\n\t%s\n", line );
	    bu_log( "\tIgnoring\n" );
	    continue;
	}
	if (bu_hash_get( htbl, (unsigned char *)part_no, strlen( part_no )) ) {
	    if ( logger_type == LOGGER_TYPE_ALL ) {
		fprintf( logger, "\t\t\tHash table entry already exists for part number (%s)\n", part_no );
	    }
	    bu_free( part_no, "part_no" );
	    continue;
	}

	/* trim any left whitespace */
	while (isspace(*ptr) && (*ptr != '\n')) {
	    ptr++;
	}
	part_name = ptr;

	/* trim any right whitespace */
	if (strlen(ptr) > 0) {
	    ptr += strlen(ptr) - 1;
	    while ((ptr != part_name) && (isspace(*ptr))) {
		*ptr = '\0';
		ptr--;
	    }
	}

	/* generate the name sans spaces, lowercase */
	lower_case( part_name );
	part_name = create_unique_name( part_name );

	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\t\tpart_no = %s, part name = %s\n", part_no, part_name );
	}


	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\t\t\tCreating new hash tabel entry for above names\n" );
	}
	bu_hash_set( htbl, (unsigned char *)part_no, strlen( part_no ), part_name );
    }

    return htbl;
}

/* Test function */
ProError ShowMsg()
{
    ProUIMessageButton* button;
    ProUIMessageButton bresult;
    ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, L"Hello World", L"Hello world!", button, PRO_UI_MESSAGE_OK, &bresult);
    ProArrayFree((ProArray*)&button);
    return PRO_TK_NO_ERROR;
}



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
