/*                           A V S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup avs */
/*@{*/
/** @file avs.c
 *  @brief
 *  Routines to manage attribute/value sets.
 *
 *  @Author Michael John Muuss
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory			@n
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"


/**
 *			B U _ A V S _ I N I T _ E M P T Y
 *
 * @brief 
 *	initialize an empty avs
 */
void
bu_avs_init_empty( struct bu_attribute_value_set *avsp )
{
	avsp->magic = BU_AVS_MAGIC;
	avsp->count = 0;
	avsp->max = 0;
	avsp->avp = (struct bu_attribute_value_pair *)NULL;
	avsp->readonly_min = avsp->readonly_max = NULL;
}

/**
 *			B U _ A V S _ I N I T
 *
 * @brief 
 *	initialize avs with storage for len entries
 */
void
bu_avs_init(struct bu_attribute_value_set *avsp, int len, const char *str)
{
	if (bu_debug & BU_DEBUG_AVS)
		bu_log("bu_avs_init(%8x, len=%d, %s)\n", avsp, len, str);

	avsp->magic = BU_AVS_MAGIC;
	if( len <= 0 )  len = 32;
	avsp->count = 0;
	avsp->max = len;
	avsp->avp = (struct bu_attribute_value_pair *)bu_calloc(avsp->max,
		sizeof(struct bu_attribute_value_pair), str);
	avsp->readonly_min = avsp->readonly_max = NULL;
}

/**			B U _ A V S _ N E W
 *
 *  @brief 
 *	Allocate storage for a new attribute/value set, with at least
 *	'len' slots pre-allocated.
 */
struct bu_attribute_value_set	*
bu_avs_new(int len, const char *str)
{
	struct bu_attribute_value_set	*avsp;

	BU_GETSTRUCT( avsp, bu_attribute_value_set );
	bu_avs_init( avsp, len, "bu_avs_new" );

	if (bu_debug & BU_DEBUG_AVS)
		bu_log("bu_avs_new(len=%d, %s) = x%x\n", len, str, avsp);

	return avsp;
}

/**
 *			B U _ A V S _ A D D
 *
 *  If the given attribute exists it will recieve the new value,
 *  othwise the set will be extended to have a new attribute/value pair.
 *
 *  Returns -
 *	1	existing attribute updated with new value
 *	2	set extended with new attribute/value pair
 */
int
bu_avs_add(struct bu_attribute_value_set *avsp, const char *attribute, const char *value)
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avsp);

	if( avsp->count ) {
		for( BU_AVS_FOR(app, avsp) )  {
			if( strcmp( app->name, attribute ) != 0 )  continue;
			if( app->value && AVS_IS_FREEABLE(avsp, app->value) )
				bu_free( (genptr_t)app->value, "app->value" );
			if( value )
				app->value = bu_strdup( value );
			else
				app->value = (char *)NULL;
			return 1;
		}
	}

	if( avsp->count >= avsp->max )  {
		/* Allocate more space first */
		avsp->max += 4;
		if( avsp->avp ) {
			avsp->avp = (struct bu_attribute_value_pair *)bu_realloc(
			  avsp->avp,  avsp->max * sizeof(struct bu_attribute_value_pair),
				"attribute_value_pair.avp[] (add)" );
		} else {
			avsp->avp = (struct bu_attribute_value_pair *)bu_calloc(
				avsp->max, sizeof(struct bu_attribute_value_pair ),
			       "attribute_value_pair.avp[] (add)" );
		}
	}

	app = &avsp->avp[avsp->count++];
	app->name = bu_strdup(attribute);
	if( value )
		app->value = bu_strdup(value);
	else
		app->value = (char *)NULL;
	return 2;
}

/*
 *			B U _ A V S _ A D D _ V L S
 */
int
bu_avs_add_vls(struct bu_attribute_value_set *avsp, const char *attribute, const struct bu_vls *value_vls)
{
	BU_CK_AVS(avsp);
	BU_CK_VLS(value_vls);

	return bu_avs_add( avsp, attribute, bu_vls_addr(value_vls) );
}

/**
 *			B U _ A V S _ M E R G E
 *
 *  @brief 
 *	Take all the attributes from 'src' and merge them into 'dest'.
 */
void
bu_avs_merge( struct bu_attribute_value_set *dest, const struct bu_attribute_value_set *src )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(dest);
	BU_CK_AVS(src);

	if( src->count ) {
		for( BU_AVS_FOR( app, src ) )  {
			(void)bu_avs_add( dest, app->name, app->value );
		}
	}
}

/*
 *			B U _ A V S _ G E T
 */
const char *
bu_avs_get( const struct bu_attribute_value_set *avsp, const char *attribute )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avsp);

	if( avsp->count < 1 )
		return NULL;

	for( BU_AVS_FOR(app, avsp) )  {
		if( strcmp( app->name, attribute ) != 0 )  continue;
		return app->value;
	}
	return NULL;
}

/**
 *			B U _ A V S _ R E M O V E
 *
 * @brief
 *	Remove the given attribute from the set
 *
 * @Return
 *	-1	attribute not found in set
 * @Return
 *	 0	OK
 */
int
bu_avs_remove(struct bu_attribute_value_set *avsp, const char *attribute)
{
	struct bu_attribute_value_pair *app, *epp;

	BU_CK_AVS(avsp);

	if( avsp->count ) {
		for( BU_AVS_FOR(app, avsp) )  {
			if( strcmp( app->name, attribute ) != 0 )  continue;
			if( app->name && AVS_IS_FREEABLE( avsp, app->name ) )
				bu_free( (genptr_t)app->name, "app->name" );
			app->name = NULL;	/* sanity */
			if( app->value && AVS_IS_FREEABLE( avsp, app->value ) )
				bu_free( (genptr_t)app->value, "app->value" );
			app->value = NULL;	/* sanity */

			/* Move last one down to replace it */
			epp = &avsp->avp[--avsp->count];
			if( app != epp )  {
				*app = *epp;		/* struct copy */
			}
			epp->name = NULL;			/* sanity */
			epp->value = NULL;
			return 0;
		}
	}
	return -1;
}

/*
 *			B U _ A V S _ F R E E
 */
void
bu_avs_free( struct bu_attribute_value_set *avsp )
{
    struct bu_attribute_value_pair *app;

    BU_CK_AVS(avsp);

    if( avsp->max < 1 )
	return;

    if( avsp->count ) {
	for( BU_AVS_FOR(app, avsp) )  {
	    if( app->name && AVS_IS_FREEABLE( avsp, app->name ) ) {
		bu_free( (genptr_t)app->name, "app->name" );
	    }
	    app->name = NULL;	/* sanity */
	    if( app->value && AVS_IS_FREEABLE( avsp, app->value ) ) {
		bu_free( (genptr_t)app->value, "app->value" );
	    }
	    app->value = NULL;	/* sanity */
	}
	avsp->count = 0;
    }
    if ( avsp->avp ) {
	bu_free( (genptr_t)avsp->avp, "bu_avs_free avsp->avp" );
	avsp->avp = NULL; /* sanity */
	avsp->max = 0;
    }
}


/*
 *			B U _ A V S _ P R I N T
 */
void
bu_avs_print( const struct bu_attribute_value_set *avsp, const char *title )
{
	struct bu_attribute_value_pair	*avpp;
	unsigned int i;

	BU_CK_AVS(avsp);

	bu_log("bu_avs_print: %s\n", title);

	avpp = avsp->avp;
	for( i = 0; i < avsp->count; i++, avpp++ )  {
		bu_log(" %s = %s\n", avpp->name, avpp->value );
	}
}

/**
 *			B U _ A V S _ A D D _ N O N U N I Q U E
 *
 * @brief
 *	Add a name/value pair even if the name already exists in this AVS
 */
void
bu_avs_add_nonunique( struct bu_attribute_value_set *avsp, char *attribute, char *value )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avsp);

	if( avsp->count >= avsp->max )  {
		/* Allocate more space first */
		avsp->max += 4;
		if( avsp->avp ) {
			avsp->avp = (struct bu_attribute_value_pair *)bu_realloc(
			  avsp->avp,  avsp->max * sizeof(struct bu_attribute_value_pair),
				"attribute_value_pair.avp[] (add)" );
		} else {
			avsp->avp = (struct bu_attribute_value_pair *)bu_malloc(
				avsp->max * sizeof(struct bu_attribute_value_pair ),
			       "attribute_value_pair.avp[] (add)" );
		}
	}

	app = &avsp->avp[avsp->count++];
	app->name = bu_strdup(attribute);
	if( value )
		app->value = bu_strdup(value);
	else
		app->value = (char *)NULL;
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
