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
static char RCSid[] = "@(#)$Header$ (ARL)";
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

#define BU_DEBUG_AVS		0x00000400	/* 012 bu_avs_*() logging */

struct bu_attribute_value_pair {
	char	*name;
	char	*value;
};

/*
 *  Every one of the names and values is a local copy made with bu_strdup().
 *  They need to be freed automatically.
 */
struct bu_attribute_value_set {
	long				magic;
	int				count;	/* # valid entries in avp */
	int				max;	/* # allocated slots in avp */
	struct bu_attribute_value_pair	*avp;
};
#define BU_AVS_MAGIC		0x41765321	/* AvS! */
#define BU_CK_AVS(_avp)		BU_CKMAG(_avp, BU_AVS_MAGIC, "bu_attribute_value_set")

#define BU_AVS_FOR(_pp, _avp)	\
	(_pp) = &(_avp)->avp[(_avp)->count-1]; (_pp) >= (_avp)->avp; (_pp)--

/*
 *			B U _ A V S _ I N I T
 */
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
		bu_free( app->value, "app->value" );
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
		bu_free( app->name, "app->name" );
		bu_free( app->value, "app->value" );
		/* Move last one down to fit */
		epp = &avp->avp[avp->count--];
		if( app != epp )  {
			*app = *epp;		/* struct copy */
		}
		epp->name = 0;		/* sanity */
		epp->value = 0;
		return 0;
	}
	return -1;
}

/*
 *			B U _ A V S _ F R E E
 */
void
bu_avs_free( avp )
struct bu_attribute_value_set	*avp;
{
	struct bu_attribute_value_pair *app;

	BU_CK_AVS(avp);

	for( BU_AVS_FOR(app, avp) )  {
		bu_free( app->name, "app->name" );
		bu_free( app->value, "app->value" );
	}
	bu_free( avp->avp, "avp->avp" );
	avp->magic = -1L;
}
