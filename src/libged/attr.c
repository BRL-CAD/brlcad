/*                         A T T R . C
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
/** @file attr.c
 *
 * The attr command.
 *
 */

#include <string.h>

#include "ged.h"

/* comparison function for the avs->name qsort below -- based on example in libc: man qsort */
static int
cmpstringp(const void *p1, const void *p2)
{
    return strcmp((char * const)((struct bu_attribute_value_pair *)p1)->name,
                  (char * const)((struct bu_attribute_value_pair *)p2)->name);
}

int
ged_attr(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    struct directory	*dp;
    struct bu_attribute_value_set avs;
    struct bu_attribute_value_pair	*avpp;
    static const char *usage = "{set|get|show|rm|append} object [key [value] ... ]";

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

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* this is only valid for v5 databases */
    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
	bu_vls_printf(&gedp->ged_result_str, "Attributes are not available for this database format.\nPlease upgrade your database format using \"dbupgrade\" to enable attributes.");
	return GED_ERROR;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    GED_DB_LOOKUP(gedp, dp, argv[2], LOOKUP_QUIET, GED_ERROR);

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(&gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return GED_ERROR;
    }

    /* sort attribute-value set array by attribute name */
    qsort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), cmpstringp);

    if (BU_STR_EQUAL(argv[1], "get")) {
	if (argc == 3) {
	    /* just list all the attributes */
	    avpp = avs.avp;
	    for (i=0; i < avs.count; i++, avpp++) {
		bu_vls_printf(&gedp->ged_result_str, "%s {%s} ", avpp->name, avpp->value);
	    }
	} else {
	    const char *val;
	    int do_separators=argc-4; /* if more than one attribute */

	    for (i=3; i<(size_t)argc; i++) {
		val = bu_avs_get(&avs, argv[i]);
		if ( !val ) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Object %s does not have a %s attribute\n",
				  dp->d_namep,
				  argv[i]);
		    bu_avs_free(&avs);
		    return GED_ERROR;
		}
		if (do_separators) {
		    bu_vls_printf(&gedp->ged_result_str, "{%s} ", val);
		} else {
		    bu_vls_printf(&gedp->ged_result_str, "%s", val);
		}
	    }
	}

	bu_avs_free(&avs);

    } else if ( BU_STR_EQUAL( argv[1], "set" ) ) {
	/* setting attribute/value pairs */
	if ((argc - 3) % 2) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	i = 3;
	while (i < (size_t)argc) {
	    if(BU_STR_EQUAL( argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
 		dp->d_flags |= DIR_REGION;
 	    }
	    (void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    i += 2;
	}
	if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_update_attributes() */

    } else if (BU_STR_EQUAL(argv[1], "rm")) {
	i = 3;
	while (i < (size_t)argc) {
    	    if(BU_STR_EQUAL( argv[i], "region")) {
 		dp->d_flags = dp->d_flags & ~(DIR_REGION);
 	    }
	    (void)bu_avs_remove(&avs, argv[i]);
	    i++;
	}
	if (db5_replace_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */

    } else if ( BU_STR_EQUAL( argv[1], "append" ) ) {
	if ((argc-3)%2) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Error: attribute names and values must be in pairs!!!\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}
	i = 3;
	while (i < (size_t)argc) {
	    const char *old_val;
	    if(BU_STR_EQUAL( argv[i], "region") && BU_STR_EQUAL(argv[i+1], "R")) {
 		dp->d_flags |= DIR_REGION;
 	    }
	    old_val = bu_avs_get(&avs, argv[i]);
	    if ( !old_val ) {
		(void)bu_avs_add(&avs, argv[i], argv[i+1]);
	    } else {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_strcat(&vls, old_val);
		bu_vls_strcat(&vls, argv[i+1]);
		bu_avs_add_vls(&avs, argv[i], &vls);
		bu_vls_free(&vls);
	    }

	    i += 2;
	}
	if (db5_replace_attributes( dp, &avs, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Error: failed to update attributes\n");
	    bu_avs_free(&avs);
	    return GED_ERROR;
	}

	/* avs is freed by db5_replace_attributes() */

    } else if ( BU_STR_EQUAL( argv[1], "show" ) ) {
	int max_attr_name_len=0;
	int tabs1=0;

	/* pretty print */
	if ( dp->d_flags & DIR_COMB ) {
	    if ( dp->d_flags & DIR_REGION ) {
		bu_vls_printf( &gedp->ged_result_str, "%s region:\n", argv[2] );
	    } else {
		bu_vls_printf( &gedp->ged_result_str, "%s combination:\n", argv[2] );
	    }
	} else if ( dp->d_flags & DIR_SOLID ) {
	    bu_vls_printf( &gedp->ged_result_str, "%s %s:\n", argv[2],
			   rt_functab[dp->d_minor_type].ft_label );
	} else {
	    switch ( dp->d_major_type ) {
		case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
		    bu_vls_printf( &gedp->ged_result_str, "%s global:\n", argv[2] );
		    break;
		case DB5_MAJORTYPE_BINARY_MIME:
		    bu_vls_printf( &gedp->ged_result_str, "%s binary(mime):\n", argv[2] );
		    break;
		case DB5_MAJORTYPE_BINARY_UNIF:
		    bu_vls_printf( &gedp->ged_result_str, "%s %s:\n", argv[2],
				   binu_types[dp->d_minor_type] );
		    break;
	    }
	}
	if ( argc == 3 ) {
	    /* just display all attributes */
	    avpp = avs.avp;
	    for ( i=0; i < avs.count; i++, avpp++ ) {
		int len;

		len = (int)strlen( avpp->name );
		if ( len > max_attr_name_len ) {
		    max_attr_name_len = len;
		}
	    }
	    tabs1 = 2 + max_attr_name_len/8;
	    avpp = avs.avp;
	    for ( i=0; i < avs.count; i++, avpp++ ) {
		const char *c;
		int tabs2;
		int k;
		int len;

		bu_vls_printf( &gedp->ged_result_str, "\t%s", avpp->name );
		len = (int)strlen( avpp->name );
		tabs2 = tabs1 - 1 - len/8;
		for ( k=0; k<tabs2; k++ ) {
		    bu_vls_putc( &gedp->ged_result_str, '\t' );
		}
		c = avpp->value;
		while ( *c ) {
		    bu_vls_putc( &gedp->ged_result_str, *c );
		    if ( *c == '\n' ) {
			for ( k=0; k<tabs1; k++ ) {
			    bu_vls_putc( &gedp->ged_result_str, '\t' );
			}
		    }
		    c++;
		}
		bu_vls_putc( &gedp->ged_result_str, '\n' );
	    }
	} else {
	    const char *val;
	    int len;

	    /* show just the specified attributes */
	    for ( i=0; i<(size_t)argc; i++ ) {
		len = (int)strlen( argv[i] );
		if ( len > max_attr_name_len ) {
		    max_attr_name_len = len;
		}
	    }
	    tabs1 = 2 + max_attr_name_len/8;
	    for (i=3; i<(size_t)argc; i++) {
		int tabs2;
		int k;
		const char *c;

		val = bu_avs_get(&avs, argv[i]);
		if (!val) {
		    bu_vls_printf(&gedp->ged_result_str,
				  "Object %s does not have a %s attribute\n",
				  dp->d_namep,
				  argv[i]);
		    bu_avs_free(&avs);
		    return GED_ERROR;
		}
		bu_vls_printf(&gedp->ged_result_str, "\t%s", argv[i]);
		len = (int)strlen(val);
		tabs2 = tabs1 - 1 - len/8;
		for (k=0; k<tabs2; k++) {
		    bu_vls_putc(&gedp->ged_result_str, '\t');
		}
		c = val;
		while (*c) {
		    bu_vls_putc(&gedp->ged_result_str, *c);
		    if (*c == '\n') {
			for (k=0; k<tabs1; k++) {
			    bu_vls_putc(&gedp->ged_result_str, '\t');
			}
		    }
		    c++;
		}
		bu_vls_putc(&gedp->ged_result_str, '\n');
	    }
	}

    } else {
	bu_vls_printf(&gedp->ged_result_str, "ERROR: unrecognized attr subcommand %s\n", argv[1]);
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

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
