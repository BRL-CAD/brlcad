/*
 *			A V S . C
 *
 *  Routines to manage attribute/value sets.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"

/*
 *			B U _ A V S _ I N I T
 */
void
bu_avs_init( avp, len, str )
struct bu_attribute_value_set	*avp;
int		len;
CONST char	*str;
{
	if (bu_debug & BU_DEBUG_AVS)
		bu_log("bu_avs_init(%8x, len=%d, %s)\n", avp, len, str);

	avp->magic = BU_AVS_MAGIC;
	if( len <= 0 )  len = 32;
	avp->count = 0;
	avp->max = len;
	avp->avp = (struct bu_attribute_value_pair *)bu_calloc(avp->max,
		sizeof(struct bu_attribute_value_pair), str);
	avp->readonly_min = avp->readonly_max = NULL;
}

/*
 *			B U _ A V S _ N E W
 *
 *  Allocate storage for a new attribute/value set, with at least
 *  'len' slots pre-allocated.
 */
struct bu_attribute_value_set	*
bu_avs_new( len, str )
int		len;
CONST char	*str;
{
	struct bu_attribute_value_set	*avp;

	BU_GETSTRUCT( avp, bu_attribute_value_set );
	bu_avs_init( avp, len, "bu_avs_new" );

	if (bu_debug & BU_DEBUG_AVS)
		bu_log("bu_avs_new(len=%d, %s) = x%x\n", len, str, avp);

	return avp;
}

/*
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
bu_avs_add( avp, attribute, value )
struct bu_attribute_value_set	*avp;
CONST char	*attribute;
CONST char	*value;
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avp);

	for( BU_AVS_FOR(app, avp) )  {
		if( strcmp( app->name, attribute ) != 0 )  continue;
		if( avp->readonly_max &&
		    (app->value < avp->readonly_min || app->value > avp->readonly_max) )
			bu_free( (genptr_t)app->value, "app->value" );
		app->value = bu_strdup( value );
		return 1;
	}

	if( avp->count >= avp->max )  {
		/* Allocate more space first */
		avp->max *= 4;
		avp->avp = (struct bu_attribute_value_pair *)bu_realloc(
			avp->avp,
			avp->max *
			sizeof(struct bu_attribute_value_pair),
			"attribute_value_pair.avp[] (add)" );
	}

	app = &avp->avp[avp->count++];
	app->name = bu_strdup(attribute);
	app->value = bu_strdup(value);
	return 2;
}

/*
 *			B U _ A V S _ A D D _ V L S
 */
int
bu_avs_add_vls( avp, attribute, value_vls )
struct bu_attribute_value_set	*avp;
const char		*attribute;
const struct bu_vls	*value_vls;
{
	BU_CK_AVS(avp);
	BU_CK_VLS(value_vls);

	return bu_avs_add( avp, attribute, bu_vls_addr(value_vls) );
}

/*
 *			B U _ A V S _ M E R G E
 *
 *  Take all the attributes from 'src' and merge them into 'dest'.
 */
void
bu_avs_merge( struct bu_attribute_value_set *dest, struct bu_attribute_value_set *src )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(dest);
	BU_CK_AVS(src);

	for( BU_AVS_FOR( app, src ) )  {
		(void)bu_avs_add( dest, app->name, app->value );
	}
}

/*
 *			B U _ A V S _ G E T
 */
const char *
bu_avs_get( const struct bu_attribute_value_set *avp, const char *attribute )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avp);

	for( BU_AVS_FOR(app, avp) )  {
		if( strcmp( app->name, attribute ) != 0 )  continue;
		return app->value;
	}
	return NULL;
}

/*
 *			B U _ A V S _ R E M O V E
 *
 *  Returns -
 *	-1	attribute not found in set
 *	 0	OK
 */
int
bu_avs_remove( avp, attribute )
struct bu_attribute_value_set	*avp;
CONST char	*attribute;
{
	struct bu_attribute_value_pair *app, *epp;

	BU_CK_AVS(avp);

	for( BU_AVS_FOR(app, avp) )  {
		if( strcmp( app->name, attribute ) != 0 )  continue;
		if( avp->readonly_max &&
		    (app->name < avp->readonly_min || app->name > avp->readonly_max) )
			bu_free( (genptr_t)app->name, "app->name" );
		app->name = NULL;	/* sanity */
		if( avp->readonly_max &&
		    (app->value < avp->readonly_min || app->value > avp->readonly_max) )
			bu_free( (genptr_t)app->value, "app->value" );
		app->value = NULL;	/* sanity */

		/* Move last one down to fit */
		epp = &avp->avp[avp->count--];
		if( app != epp )  {
			*app = *epp;		/* struct copy */
		}
		epp->name = NULL;			/* sanity */
		epp->value = NULL;
		return 0;
	}
	return -1;
}

/*
 *			B U _ A V S _ F R E E
 */
void
bu_avs_free( struct bu_attribute_value_set *avp )
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avp);

	for( BU_AVS_FOR(app, avp) )  {
		if( avp->readonly_max &&
		    (app->name < avp->readonly_min || app->name > avp->readonly_max) )
			bu_free( (genptr_t)app->name, "app->name" );
		app->name = NULL;	/* sanity */
		if( avp->readonly_max &&
		    (app->value < avp->readonly_min || app->value > avp->readonly_max) )
			bu_free( (genptr_t)app->value, "app->value" );
		app->value = NULL;	/* sanity */
	}
	bu_free( (genptr_t)avp->avp, "avp->avp" );
	avp->magic = -1L;
}

/*
 *			B U _ A V S _ P R I N T
 */
void
bu_avs_print( const struct bu_attribute_value_set *avp, const char *title )
{
	struct bu_attribute_value_pair	*avpp;
	int i;

	BU_CK_AVS(avp);

	bu_log("bu_avs_print: %s\n", title);

	avpp = avp->avp;
	for( i = 0; i < avp->count; i++, avpp++ )  {
		bu_log(" %s = %s\n", avpp->name, avpp->value );
	}
}
