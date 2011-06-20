/*                      R T S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file librtserver/rtserver.c
 *
 * library for BRL-CAD raytrace server
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "bin.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "rtserver.h"

#ifdef HAVE_JAVAVM_JNI_H
#  include <JavaVM/jni.h>
#elif defined(HAVE_JNI_H)
#  include <jni.h>
#else
#  include "ERROR: jni.h could not be found"
#endif
#include "RtServerImpl.h"

/* private structures not used outside this file */
struct rtserver_rti {
    struct rt_i *rtrti_rtip;		/* pointer to an rti structure */
    char *rtrti_name;			/* name of this "assembly" (bu_malloc'd storage) */
    size_t rtrti_num_trees;		/* number of trees in this rti structure */
    char **rtrti_trees;			/* array of pointers to tree-top names trees[num_trees] (bu_malloc'd storage) */
    matp_t rtrti_xform;			/* transformation matrix from global coords to this rt instance (NULL -> identity) */
    matp_t rtrti_inv_xform;		/* inverse of above xform (NULL -> identity) */
    Tcl_HashTable *rtrti_region_names;	/* A Tcl hash table containing region names as keys and index numbers as values.
					 * The indices are used to reference region names in the Java return byte array
					 * rather than using the full name.
					 */
    int region_count;			/* number of entries in above hash table */
};

struct rtserver_geometry {
    size_t rts_number_of_rtis;		/* number of rtserver_rti structures */
    struct rtserver_rti **rts_rtis;	/* array of pointers to rtserver_rti
					 * structures rts_rtis[rts_number_of_rtis] (bu_malloc'd storage )
					 */
    point_t		rts_mdl_min;	/* min corner of model bounding RPP */
    point_t		rts_mdl_max;	/* max corner of model bounding RPP */
    double		rts_radius;	/* radius of model bounding sphere */
    Tcl_HashTable	*rts_comp_names;/* A Tcl hash table containing ident numbers as keys
					 * and component names as values
					 */
};

static struct bu_ptbl apps; /* dynamic table of application structures, each incoming connection gets its own private struct */

static int app_count = 0; /* number of application structures created so far */

static struct rt_i *myrtip = NULL; /* rt_i pointer for the geometry */

/* mutex to protect the list of application structures */
static pthread_mutex_t apps_mutex = PTHREAD_MUTEX_INITIALIZER;

#define GET_APPLICATION(_p) {						\
	pthread_mutex_lock( &apps_mutex );				\
	if ( BU_PTBL_LEN(&apps) ) {					\
	    _p = (struct application *)BU_PTBL_GET( &apps, BU_PTBL_LEN( &apps )-1 ); \
	    bu_ptbl_trunc( &apps, BU_PTBL_LEN( &apps )-1 );		\
	    pthread_mutex_unlock( &apps_mutex );			\
	    bu_vlb_reset(_p->a_uptr);					\
	} else {							\
	    int app_no = app_count++;					\
	    pthread_mutex_unlock( &apps_mutex );			\
	    _p = (struct application *)bu_malloc( sizeof(struct application), "struct application"); \
	    RT_APPLICATION_INIT(_p);					\
	    _p->a_rt_i = myrtip;					\
	    _p->a_uptr = (struct bu_vlb *)bu_calloc( sizeof(struct bu_vlb), 1, "bu_vlb GET_APPLICATION"); \
	    bu_vlb_init(_p->a_uptr);					\
	    _p->a_resource = (struct resource *)bu_calloc( sizeof(struct resource), 1, "resource"); \
	    rt_init_resource( _p->a_resource, app_no, _p->a_rt_i );	\
	    _p->a_hit = rts_hit;					\
	    _p->a_miss = rts_miss;					\
	    _p->a_logoverlap = rt_silent_logoverlap;			\
	}								\
    }

#define FINISH_APPLICATION(_p) {		\
	pthread_mutex_lock( &apps_mutex );	\
	bu_ptbl_ins( &apps, (long *)(_p) );	\
	pthread_mutex_unlock( &apps_mutex );	\
	_p = (struct application *)NULL;	\
    }


/* the title of this BRL-CAD database */
static char *title=NULL;

/* use air flag (0 -> ignore air regions) */
static int use_air=0;

/* verbosity flag */
static int verbose=0;

/* total number of MUVES component names */
static CLIENTDATA_INT comp_count=0;

/* array of MUVES component names */
static char **names;

/* the geometry */
static struct rtserver_geometry **rts_geometry=NULL;	/* array of rtserver_geometry structures
							 * indexed by session id
							 * NULL entry -> unused slot
							 * index 0 must never be NULL
							 */
static size_t num_geometries=0;	/* the length of the rts_geometry array */
static int used_session_0=0;	/* flag indicating if initial session has been used */

/* hash tables for MUVES components */
static int hash_table_exists=0;
static Tcl_HashTable name_tbl;		/* all the MUVES component names (key is the MUVES component name,
					 * value = MUVES id number
					 */

/* wrapper for the GET_APPLICATION macro */
void
getApplication(struct application **ap)
{
    GET_APPLICATION((*ap));
}

/* wrapper for the FINISH_APPLICATION macro */
void
freeApplication(struct application *ap)
{
    FINISH_APPLICATION(ap);
}

/*	Routine to create a hash table of all the MUVES component names that appear in this BRL-CAD model
 *
 *  MUVES components are identified by an attribute named "MUVES_Comp", and its value is the MUVES
 *  component name. Ray trace results will employ indices into this list rather than using the name strings
 *  themselves. Note that index 0 will be used to indicate an object with no MUVES name.
 */
void
get_muves_components()
{
    Tcl_HashEntry *name_entry;
    Tcl_HashSearch search;
    size_t i, j;
    size_t sessionid;

    /* make sure we have some geometry */
    if ( !rts_geometry ) {
	return;
    }

    for ( sessionid=0; sessionid<num_geometries; sessionid++ ) {
	if ( rts_geometry[sessionid] ) {
	    break;
	}
    }

    if ( !rts_geometry[sessionid] ) {
	return;
    }

    /* initialize the hash tables */
    Tcl_InitHashTable( &name_tbl, TCL_STRING_KEYS ); /* MUVES Component name to index table */
    hash_table_exists = 1;

    /* visit each rt_i */
    for ( i=0; i<rts_geometry[sessionid]->rts_number_of_rtis; i++ ) {
	struct rtserver_rti *rts_rtip=rts_geometry[sessionid]->rts_rtis[i];
	struct rt_i *rtip=rts_rtip->rtrti_rtip;

	/* visit each region in this rt_i */
	for ( j=0; j<rtip->nregions; j++ ) {
	    struct region *rp=rtip->Regions[j];
	    int new;
	    const char *attrget;
	    CLIENTDATA_INT idx=0;

	    attrget = bu_avs_get(&(rp->attr_values), "MUVES_Component");
	    
	    if ( attrget == NULL ) {
		/* not a region, or does not have a MUVES_Component attribute */
		continue;
	    }

	    /* create an entry for this MUVES_Component name */
	    name_entry = Tcl_CreateHashEntry( &name_tbl, attrget, &new );
	    if ( verbose ) {
		fprintf( stderr, "region %s, name = %s\n",
			 rp->reg_name, attrget );
	    }
	    /* set value to next index */
	    if ( new ) {
		comp_count++;
		Tcl_SetHashValue( name_entry, (ClientData)comp_count );
		idx = comp_count;
	    } else {
		idx = (CLIENTDATA_INT)Tcl_GetHashValue( name_entry );
	    }
	}
    }

    /* create an array of MUVES_Component names.
     * this can be returned to a client
     */
    comp_count++;
    names = (char **)bu_calloc( comp_count + 1, sizeof( char *), "MUVES names" );
    names[0] = bu_strdup( "No MUVES Name" );
    name_entry = Tcl_FirstHashEntry( &name_tbl, &search );
    while ( name_entry ) {
	char *name;
	CLIENTDATA_INT idx;

	name = Tcl_GetHashKey( &name_tbl, name_entry );
	idx = (CLIENTDATA_INT)Tcl_GetHashValue( name_entry );
	names[idx] = bu_strdup( name );

	name_entry = Tcl_NextHashEntry( &search );
    }
}

/* set the verbosity level for this library (currently, only zero or non-zero is supported */
void rts_set_verbosity( int v )
{
    verbose = v;
}


/* check if the specified rt_i pointer is the last use of this rt_i */
int
isLastUseOfRti( struct rt_i *rtip, int sessionid )
{
    size_t i, j;

    for ( i=0; i<num_geometries; i++ ) {
	if ( i == (size_t)sessionid )
	    continue;
	if ( !rts_geometry[i] )
	    continue;
	for ( j=0; j<rts_geometry[i]->rts_number_of_rtis; j++ ) {
	    struct rtserver_rti *rtsrtip = rts_geometry[i]->rts_rtis[j];
	    if ( rtsrtip->rtrti_rtip == rtip )
		return 0;
	}
    }

    return 1;
}


/* routine to free memory associated with the rtserver_geometry */
void
rts_clean( int sessionid)
{
    struct application *ap;
    struct resource *resp;
    size_t i, j;

    if ( sessionid >= 0 && (size_t)sessionid < num_geometries && rts_geometry[sessionid] ) {
	/* free all old geometry */
	for ( i=0; i<rts_geometry[sessionid]->rts_number_of_rtis; i++ ) {
	    struct rtserver_rti *rtsrtip;

	    rtsrtip = rts_geometry[sessionid]->rts_rtis[i];
	    if ( rtsrtip->rtrti_name ) {
		bu_free( rtsrtip->rtrti_name, "rtserver assembly name" );
		rtsrtip->rtrti_name = NULL;
	    }
	    if ( rtsrtip->rtrti_xform ) {
		bu_free( rtsrtip->rtrti_xform, "rtserver xform matrix" );
		rtsrtip->rtrti_xform = NULL;
	    }
	    if ( rtsrtip->rtrti_inv_xform ) {
		bu_free( rtsrtip->rtrti_inv_xform, "rtserver inverse xform matrix" );
		rtsrtip->rtrti_inv_xform = NULL;
	    }

	    for ( j=0; j<rtsrtip->rtrti_num_trees; j++ ) {
		bu_free( rtsrtip->rtrti_trees[j], "rtserver tree name" );
		rtsrtip->rtrti_trees[j] = NULL;
	    }
	    rtsrtip->rtrti_num_trees =  0;
	    if ( rtsrtip->rtrti_trees ) {
		bu_free( rtsrtip->rtrti_trees, "rtserver tree names" );
		rtsrtip->rtrti_trees = NULL;
	    }

	    if ( rtsrtip->rtrti_rtip ) {
		if ( isLastUseOfRti( rtsrtip->rtrti_rtip, i ) ) {
		    rt_free_rti( rtsrtip->rtrti_rtip );
		    rtsrtip->rtrti_rtip = NULL;
		}
	    }
	    if ( rtsrtip->rtrti_region_names ) {
		Tcl_DeleteHashTable( rtsrtip->rtrti_region_names );
		bu_free(rtsrtip->rtrti_region_names, "region names hash table");
	    }

	    bu_free(rtsrtip, "rtserver_rti");
	}
	bu_free(rts_geometry[sessionid]->rts_rtis, "rtserver_rti *");
	if ( rts_geometry[sessionid]->rts_comp_names ) {
	    Tcl_DeleteHashTable( rts_geometry[sessionid]->rts_comp_names );
	    bu_free(rts_geometry[sessionid]->rts_comp_names, "component names hash table");
	}
	bu_free( rts_geometry[sessionid], "rts_geometry" );
	rts_geometry[sessionid] = NULL;
    }

    if ( hash_table_exists ) {
	Tcl_DeleteHashTable( &name_tbl );
	hash_table_exists = 0;
    }

    num_geometries = 0;
    bu_free( (char *)rts_geometry, "rts_geometry" );
    rts_geometry = NULL;

    if(title != NULL) {
	bu_free(title, "title");
	title = NULL;
    }

    for( i=0 ; i<BU_PTBL_LEN(&apps) ; i++ ) {
	struct bu_vlb *vlb;

	ap = (struct application *)BU_PTBL_GET( &apps, i);

	vlb = (struct bu_vlb *)ap->a_uptr;
	if(vlb != NULL) {
	    bu_vlb_free(vlb);
	    bu_free(vlb, "vlb");
	}

	if( ap->a_resource != NULL ) {
	    rt_clean_resource_complete(NULL, ap->a_resource);
	    bu_free(ap->a_resource, "resource");
	}

	bu_free(ap, "struct application");
    }
    bu_ptbl_free(&apps);
    memset(&apps, 0, sizeof( struct bu_ptbl));

    resp = &rt_uniresource;
    rt_clean_resource_complete(NULL, resp);
}

/* routine to close a session
 * session id 0 is just marked as closed, but not freed
 * any other session is deleted
 */
void
rts_close_session( int UNUSED(sessionid) )
{
    /* does nothing for now */
}


/* routine to create a new session id
 *
 * Uses sessionid 0 for everyone so far
 */
int
rts_open_session()
{
    /* make sure we have some geometry */
    if ( num_geometries == 0 || rts_geometry[0] == NULL ) {
	fprintf( stderr, "rtServer: ERROR: no geometry loaded!!\n" );
	return -1;
    }

    /* for now, just return the same session to everyone */
    used_session_0 = 1;
    return 0;
}

/* routine to load geometry from a BRL-CAD model
 *
 * Uses "rtserver_data" object in the BRL-CAD model to find object names (see rtserver.h)
 * eliminates any old data
 * creates a new session (sessionid = 0)
 *
 * Returns:
 *	sessionid - all is well
 *	negative number - we have a problem
 */
int
rts_load_geometry( char *filename, int num_trees, char **objects )
{
    struct rt_i *rtip;
    struct db_i *dbip;
    size_t i, j;
    int sessionid=0;
    const char *attrs[] = {(const char *)"muves_comp", (const char *)NULL };

    /* clean up any prior geometry data */
    if ( rts_geometry ) {

	dbip = rts_geometry[0]->rts_rtis[0]->rtrti_rtip->rti_dbip;
	for ( i=0; i<num_geometries; i++ ) {
	    if ( rts_geometry[i] ) {
		rts_clean( i );
	    }
	}
	num_geometries = 0;
	bu_free( (char *)rts_geometry, "rts_geometry" );
	rts_geometry = NULL;
    }

    if( !BU_PTBL_TEST(&apps) ) {
	bu_ptbl_init( &apps, 8, "application structure list" );
    }

    if ( hash_table_exists ) {
	Tcl_DeleteHashTable( &name_tbl );
	hash_table_exists = 0;
    }

    /* create the new session */
    if ( num_geometries < 1 ) {
	num_geometries = 5;	/* create five slots to start */
	sessionid = 0;
	rts_geometry = (struct rtserver_geometry **)bu_calloc( num_geometries,
							       sizeof( struct rts_geometry *),
							       "rts_geometry *" );
    }

    /* open the BRL-CAD model */
    rtip = rt_dirbuild( filename, (char *)NULL, 0 );
    if ( rtip == RTI_NULL ) {
	bu_log( "rt_dirbuild() failed for file %s\n", filename );
	return -1;
    }
    myrtip = rtip;

    /* grab the DB instance pointer (just for convenience) */
    dbip = rtip->rti_dbip;

    /* set the title */
    title = bu_strdup( dbip->dbi_title );

    /* set the use air flags */
    rtip->useair = use_air;


    /* load the specified objects */
    /* malloc some memory for the rtserver geometry structure (bu_calloc zeros the memory) */
    rts_geometry[sessionid] = (struct rtserver_geometry *)bu_calloc( 1,
								     sizeof( struct rtserver_geometry ),
								     "rtserver geometry" );

    /* just one RT instance */
    rts_geometry[sessionid]->rts_number_of_rtis = 1;
    rts_geometry[sessionid]->rts_rtis = (struct rtserver_rti **)bu_malloc( sizeof( struct rtserver_rti *),
									   "rtserver_rti *" );
    rts_geometry[sessionid]->rts_rtis[0] = (struct rtserver_rti *)bu_calloc( 1,
									     sizeof( struct rtserver_rti ),
									     "rtserver_rti" );
    rts_geometry[sessionid]->rts_rtis[0]->rtrti_rtip = rtip;
    rts_geometry[sessionid]->rts_rtis[0]->rtrti_num_trees = num_trees;
    if ( verbose ) {
	fprintf( stderr, "num_trees = %d\n", num_trees );
    }

    /* initialize our overall bounding box */
    VSETALL( rts_geometry[sessionid]->rts_mdl_min, MAX_FASTF );
    VREVERSE( rts_geometry[sessionid]->rts_mdl_max, rts_geometry[sessionid]->rts_mdl_min );

    /* for each RT instance, get its trees */
    for ( i=0; i<rts_geometry[sessionid]->rts_number_of_rtis; i++ ) {
	struct rtserver_rti *rts_rtip;
	size_t regno;

	/* cache the rtserver_rti pointer and its associated rt instance pointer */
	rts_rtip = rts_geometry[sessionid]->rts_rtis[i];
	rtip = rts_rtip->rtrti_rtip;
	rts_rtip->rtrti_num_trees = num_trees;
	rts_rtip->rtrti_trees = (char**)bu_calloc(rts_rtip->rtrti_num_trees, sizeof(char *), "rtrti_trees");

	for ( j=0; j<rts_rtip->rtrti_num_trees; j++ ) {
	    rts_rtip->rtrti_trees[j] = bu_strdupm(objects[j], "rtrti_tree");
	}
	/* get the BRL-CAD objects for this rt instance */
	if ( verbose ) {
	    fprintf( stderr, "Getting trees:\n" );
	    for ( j=0; j<rts_rtip->rtrti_num_trees; j++ ) {
		fprintf( stderr, "\t%s\n", rts_rtip->rtrti_trees[j] );
	    }
	}
	if ( rt_gettrees_and_attrs( rtip, attrs, rts_rtip->rtrti_num_trees,
				    (const char **)rts_rtip->rtrti_trees, 1 ) ) {
	    fprintf( stderr, "Failed to get BRL-CAD objects:\n" );
	    for ( j=0; j<rts_rtip->rtrti_num_trees; j++ ) {
		fprintf( stderr, "\t%s\n", rts_rtip->rtrti_trees[j] );
	    }
	    return -4;
	}

	/* prep the geometry for raytracing */
	rt_prep_parallel( rtip, 1 );

	/* create the hash table of region names */
	rts_rtip->rtrti_region_names = (Tcl_HashTable *)bu_calloc(1, sizeof(Tcl_HashTable), "region names hash table");
	Tcl_InitHashTable(rts_rtip->rtrti_region_names, TCL_STRING_KEYS);
	for( regno=0 ; regno<rtip->nregions ; regno++ ) {
	    int newPtr = 0;
	    Tcl_HashEntry *entry = Tcl_CreateHashEntry(rts_rtip->rtrti_region_names, rtip->Regions[regno]->reg_name, &newPtr);
	    if( !newPtr ) {
		if( verbose ) {
		    bu_log( "Already have an entry for region %s\n", rtip->Regions[regno]->reg_name);
		}
		continue;
	    }
	    if( verbose ) {
		bu_log( "Setting hash table for key %s to %d\n", rtip->Regions[regno]->reg_name, regno);
	    }
	    Tcl_SetHashValue(entry, (ClientData)regno );
	}
	rts_rtip->region_count = rtip->nregions;

	/* update our overall bounding box */
	VMINMAX( rts_geometry[sessionid]->rts_mdl_min, rts_geometry[sessionid]->rts_mdl_max, rtip->mdl_min );
	VMINMAX( rts_geometry[sessionid]->rts_mdl_min, rts_geometry[sessionid]->rts_mdl_max, rtip->mdl_max );
    }

    if ( verbose ) {
	fprintf( stderr, "Model extents: (%g %g %g) <-> (%g %g %g)\n",
		 V3ARGS( rts_geometry[sessionid]->rts_mdl_min ),
		 V3ARGS( rts_geometry[sessionid]->rts_mdl_max ) );
    }

    return sessionid;
}

/* ray missed routine for rt_shootray() */
int
rts_miss( struct application *ap )
{
    struct bu_vlb *vlb;
    int numPartitions = 0;

    /* get the results pointer from the application structure */
    vlb = (struct bu_vlb *)ap->a_uptr;
    if(vlb != NULL) {
	unsigned char buffer[SIZEOF_NETWORK_LONG];
	*(uint32_t *)buffer = htonl(numPartitions);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);
    }
    if ( verbose ) {
	fprintf( stderr, "Missed!!!\n" );
    }
    return 0;
}

/* ray hit routine for rt_shootray()
 *
 * this routine adds the list of ray_hit structures to the ray_result structure for this ray
 */
int
rts_hit( struct application *ap, struct partition *partHeadp, struct seg *UNUSED(segs))
{
    int sessionid = ap->a_user;
    struct partition *pp;
    struct bu_vlb *vlb;
    vect_t reverse_ray_dir;
    int numPartitions;
    unsigned char buffer[SIZEOF_NETWORK_DOUBLE*3];

    if ( verbose ) {
	fprintf( stderr, "Got a hit!!!\n" );
    }

    /* get the results pointer from the application structure */
    vlb = (struct bu_vlb *)ap->a_uptr;

    /* count the number of partitions */
    numPartitions = 0;
    for ( BU_LIST_FOR( pp, partition, (struct bu_list *)partHeadp ) ) {
	numPartitions++;
    };
    /* write the number of partitiions to the byte array */
    *(uint32_t *)buffer = htonl(numPartitions);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

    VREVERSE(reverse_ray_dir, ap->a_ray.r_dir);

    /* write hits to the bu_vlb structure */
    for ( BU_LIST_FOR( pp, partition, (struct bu_list *)partHeadp ) ) {
	struct region *rp;
	vect_t enterNormal;
	vect_t exitNormal;
	Tcl_HashEntry *entry;
	double los;
	double inObl, outObl;
	double dot;
	int regionIndex;

	/* fill in the data for this hit */
	los = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	RT_HIT_NORMAL( enterNormal, pp->pt_inhit,
		       pp->pt_inseg->seg_stp, 0, pp->pt_inflip );
	RT_HIT_NORMAL( exitNormal, pp->pt_outhit,
		       pp->pt_outseg->seg_stp, 0, pp->pt_outflip );

	rp = pp->pt_regionp;

	/* write partition info to the byte array */
	/* start with entrance point */
	htond(buffer, (unsigned char *)pp->pt_inhit->hit_point, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
	/* next exit point */
	htond(buffer, (unsigned char *)pp->pt_outhit->hit_point, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
	/* next entrance surface normal vector */
	htond(buffer, (unsigned char *)enterNormal, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
	/* next entrance surface normal vector */
	htond(buffer, (unsigned char *)exitNormal, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);

	/* calculate the entrance and exit obliquities */
	dot = VDOT( reverse_ray_dir, enterNormal );
	if( dot < -1.0 ) {
	    dot = -1.0;
	} else if( dot > 1.0 ) {
	    dot = 1.0;
	}
	inObl = acos(dot);
	if ( inObl < 0.0 ) {
	    inObl = -inObl;
	}
	if ( inObl > M_PI_2 ) {
	    inObl = M_PI_2;
	}

	dot = VDOT( ap->a_ray.r_dir, exitNormal );
	if( dot < -1.0 ) {
	    dot = -1.0;
	} else if( dot > 1.0 ) {
	    dot = 1.0;
	}
	outObl = acos(dot);
	if ( outObl < 0.0 ) {
	    outObl = -outObl;
	}
	if ( outObl > M_PI_2 ) {
	    outObl = M_PI_2;
	}

	/* write obliquities to the buffer */
	htond( buffer, (unsigned char *)&inObl, 1 );
	htond( &buffer[SIZEOF_NETWORK_DOUBLE], (unsigned char *)&outObl, 1);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*2);

	/* get the region index from the hash table */
	entry = Tcl_FindHashEntry( rts_geometry[sessionid]->rts_rtis[0]->rtrti_region_names, (ClientData)rp->reg_name );
	regionIndex = (CLIENTDATA_INT)Tcl_GetHashValue( entry );

	/* write region index to buffer */
	*(uint32_t *)buffer = htonl(regionIndex);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

	/* write the ident number to the buffer */
	*(uint32_t *)buffer = htonl(rp->reg_regionid);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

	/* write the aircode number to the buffer */
	*(uint32_t *)buffer = htonl(rp->reg_aircode);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

	if ( verbose ) {
	    fprintf( stderr, "\tentrance at dist=%g, hit region %s (id = %d)\n",
		     pp->pt_inhit->hit_dist, rp->reg_name, rp->reg_regionid );
	    fprintf( stderr, "\t\texit at dist = %g\n", pp->pt_outhit->hit_dist );
	}
    }

    return 1;
}

/* routine to print out the info that has been written to the bu_vlb structure */
void
printHits(struct bu_vlb *vlb)
{
    unsigned char *c;
    int numRays = 0;
    int rayNum;

    c = bu_vlb_addr(vlb);
    numRays = BU_GLONG(c);
    bu_log( "number of rays: %d\n", numRays);

    c += SIZEOF_NETWORK_LONG;

    for(rayNum=0 ; rayNum<numRays ; rayNum++) {
	int numPartitions = 0;
	int partNo;

	bu_log("ray #%d\n", rayNum);
	numPartitions = BU_GLONG(c);
	c += SIZEOF_NETWORK_LONG;
	bu_log("\tnumber of partitions: %d\n", numPartitions);

	for(partNo=0 ; partNo<numPartitions ; partNo++) {
	    point_t enterPt;
	    point_t exitPt;
	    vect_t enterNorm;
	    vect_t exitNorm;
	    double inObl;
	    double outObl;
	    int regionIndex;

	    ntohd((unsigned char *)enterPt, c, 3);
	    bu_log("\t\tenter hit at (%g %g %g)\n", V3ARGS(enterPt));
	    c += SIZEOF_NETWORK_DOUBLE * 3;

	    ntohd((unsigned char *)exitPt, c, 3);
	    bu_log("\t\texit hit at (%g %g %g)\n", V3ARGS(exitPt));
	    c += SIZEOF_NETWORK_DOUBLE * 3;

	    ntohd((unsigned char *)enterNorm, c, 3);
	    bu_log("\t\tenter normal: (%g %g %g)\n", V3ARGS(enterNorm));
	    c += SIZEOF_NETWORK_DOUBLE * 3;

	    ntohd((unsigned char *)exitNorm, c, 3);
	    bu_log("\t\texit normal; (%g %g %g)\n", V3ARGS(exitNorm));
	    c += SIZEOF_NETWORK_DOUBLE * 3;

	    ntohd((unsigned char*)&inObl, c, 1);
	    bu_log("\t\tenter obliquity: %g\n", inObl);
	    c += SIZEOF_NETWORK_DOUBLE;

	    ntohd((unsigned char*)&outObl, c, 1);
	    bu_log("\t\tenter obliquity: %g\n", outObl);
	    c += SIZEOF_NETWORK_DOUBLE;

	    regionIndex = BU_GLONG(c);
	    bu_log("\t\tregion index: %d, name = %s\n",
		   regionIndex, myrtip->Regions[regionIndex]->reg_name);
	    c += SIZEOF_NETWORK_LONG;
	}
    }
}

void
get_model_extents( int sessionid, point_t min, point_t max )
{
    VMOVE( min, rts_geometry[sessionid]->rts_mdl_min );
    VMOVE( max, rts_geometry[sessionid]->rts_mdl_max );
}

void
rts_shootray( struct application *ap )
{
    unsigned char buffer[SIZEOF_NETWORK_LONG];
    int i = 1;

    /* write the number of rays to the byte array */
    *(uint32_t *)buffer = htonl(i);
    bu_vlb_write(ap->a_uptr, buffer, SIZEOF_NETWORK_LONG);

    /* actually shoot the ray */
    rt_shootray( ap );
}

void fillItemTree( jobject parent_node,
		   struct db_i *dbip,
		   JNIEnv *env,
		   char *name,
		   jclass itemTree_class,
		   jmethodID itemTree_constructor_id,
		   jmethodID itemTree_addcomponent_id,
		   jmethodID itemTree_setMuvesName_id,
		   jmethodID itemTree_setMaterialName_id,
		   jmethodID itemTree_setIdentNumber_id,
		   jmethodID itemTree_setLos_id,
		   jmethodID itemTree_setUseCount_id );

/*
 *			F I L L I T E M M E M B E R S
 *
 * Routine to descend into a BRL-CAD tree structure and call fillItemTree() at each leaf
 */
void
fillItemMembers( jobject node,
		 struct db_i *dbip,
		 JNIEnv *env,
		 union tree *tp,
		 jclass itemTree_class,
		 jmethodID itemTree_constructor_id,
		 jmethodID itemTree_addcomponent_id,
		 jmethodID itemTree_setMuvesName_id,
		 jmethodID itemTree_setMaterialName_id,
		 jmethodID itemTree_setIdentNumber_id,
		 jmethodID itemTree_setLos_id,
		 jmethodID itemTree_setUseCount_id )
{
    switch ( tp->tr_op ) {
	case OP_SOLID:
	case OP_NOP:
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	case OP_FREE:
	    return;
	case OP_UNION:
	case OP_INTERSECT:
	    fillItemMembers( node, dbip, env, tp->tr_b.tb_left, itemTree_class,
			     itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			     itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id);
	    fillItemMembers( node, dbip, env, tp->tr_b.tb_right, itemTree_class,
			     itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			     itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id);
	    break;
	case OP_SUBTRACT:
	    fillItemMembers( node, dbip, env, tp->tr_b.tb_left, itemTree_class,
			     itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			     itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id);
	    break;
	case OP_DB_LEAF:
	    fillItemTree( node, dbip, env, tp->tr_l.tl_name, itemTree_class,
			  itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			  itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id);
	    break;
    }
}

/*				F I L L I T E M T R E E
 *
 * Routine to fill a MUVES3 ItemTree structure based on a leaf node of a BRL-CAD tree structure.
 */
void
fillItemTree( jobject parent_node,
	      struct db_i *dbip,
	      JNIEnv *env,
	      char *name,
	      jclass itemTree_class,
	      jmethodID itemTree_constructor_id,
	      jmethodID itemTree_addcomponent_id,
	      jmethodID itemTree_setMuvesName_id,
	      jmethodID itemTree_setMaterialName_id,
	      jmethodID itemTree_setIdentNumber_id,
	      jmethodID itemTree_setLos_id,
	      jmethodID itemTree_setUseCount_id)
{
    struct directory *dp;
    int id;
    struct rt_db_internal intern;
    jstring nodeName;
    jobject node;
    const char *muvesName;

    if ( (dp=db_lookup( dbip, name, LOOKUP_QUIET )) == RT_DIR_NULL ) {
	return;
    }

    /* create an ItemTree node for this object */
    nodeName = (*env)->NewStringUTF( env, name );
    node = (*env)->NewObject( env, itemTree_class, itemTree_constructor_id, nodeName );

    /* check for any exceptions */
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating ItemTree node\n" );
	(*env)->ExceptionDescribe(env);
	return;
    }

    /* add this node to parent */
    (*env)->CallVoidMethod( env, parent_node, itemTree_addcomponent_id, node );

    /* check for any exceptions */
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while adding an ItemTree node\n" );
	(*env)->ExceptionDescribe(env);
	return;
    }


    if ( dp->d_flags & RT_DIR_REGION ) {
	struct rt_comb_internal *comb;
	jint ident, los, uses;

	if ( (id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource ) ) < 0 ) {
	    fprintf( stderr, "Failed to get internal form of BRL-CAD object (%s)\n", name );
	    return;
	}

	/* check for MUVES_Component */
	if ( (muvesName=bu_avs_get( &intern.idb_avs, "MUVES_Component" )) != NULL ) {
	    /* add this attribute */
	    jstring jmuvesName;

	    jmuvesName = (*env)->NewStringUTF( env, muvesName );
	    (*env)->CallVoidMethod( env, node, itemTree_setMuvesName_id, jmuvesName );

	    /* check for any exceptions */
	    if ( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while setting the ItemTree MuvesName\n" );
		(*env)->ExceptionDescribe(env);
		rt_db_free_internal(&intern);
		return;
	    }
	}

	/* assign ident number */
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB( comb );
	ident = comb->region_id;
	(*env)->CallVoidMethod( env, node, itemTree_setIdentNumber_id, ident );
	/* check for any exceptions */
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while setting the ItemTree ident number\n" );
	    (*env)->ExceptionDescribe(env);
	    rt_db_free_internal(&intern);
	    return;
	}

	/* assign los number */
	los = comb->los;
	(*env)->CallVoidMethod( env, node, itemTree_setLos_id, los );
	/* check for any exceptions */
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while setting the ItemTree los number\n" );
	    (*env)->ExceptionDescribe(env);
	    rt_db_free_internal(&intern);
	    return;
	}

	/* assign use count */
	uses = dp->d_uses;
	(*env)->CallVoidMethod( env, node, itemTree_setUseCount_id, uses );
	/* check for any exceptions */
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while setting the ItemTree use count\n" );
	    (*env)->ExceptionDescribe(env);
	    rt_db_free_internal(&intern);
	    return;
	}

	rt_db_free_internal(&intern);

	/* do not recurse into regions */
	return;
    }

    if ( dp->d_flags & RT_DIR_COMB ) {
	/* recurse into this combination */
	struct rt_comb_internal *comb;

	if ( (id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource ) ) < 0 ) {
	    fprintf( stderr, "Failed to get internal form of BRL-CAD object (%s)\n", name );
	    return;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CHECK_COMB( comb );

	/* check for MUVES_Component */
	if ( (muvesName=bu_avs_get( &intern.idb_avs, "MUVES_Component" )) != NULL ) {
	    /* add this attribute */
	    jstring jmuvesName;

	    jmuvesName = (*env)->NewStringUTF( env, muvesName );
	    (*env)->CallVoidMethod( env, node, itemTree_setMuvesName_id, jmuvesName );

	    /* check for any exceptions */
	    if ( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while setting the ItemTree MuvesName\n" );
		(*env)->ExceptionDescribe(env);
		return;
	    }
	}


	/* add members of this combination to the ItemTree */
	fillItemMembers( node, dbip, env, comb->tree, itemTree_class,
			 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			 itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id );
	rt_db_free_internal(&intern);
    }
}

/*
 *
 * JAVA JNI BINDINGS
 *
 */


/* init routine called from java */
JNIEXPORT jint JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_rtsInit(JNIEnv *env, jobject UNUSED(obj), jobjectArray args)
{

    jsize len=(*env)->GetArrayLength(env, args);
    jstring jfile_name, *jobj_name;
    char *file_name;
    char **obj_list;
    int num_objects=(len - 3);
    jint ret=0;
    int rts_load_return=0;
    int i;

    if ( len < 2 ) {
	bu_log( "wrong number of args\n" );
	return (jint) 1;
    }

    /* get the aruments from the JAVA args object array */
    jfile_name = (jstring)(*env)->GetObjectArrayElement( env, args, 0 );
    file_name = (char *)(*env)->GetStringUTFChars(env, jfile_name, 0);

    obj_list = (char **)bu_calloc( num_objects, sizeof( char *), "obj_list" );
    jobj_name = (jstring *)bu_calloc( num_objects, sizeof( jstring ), "jobj_name" );
    for ( i=0; i<num_objects; i++ ) {
	jobj_name[i] = (jstring)(*env)->GetObjectArrayElement( env, args, i+1 );
	obj_list[i] = (char *)(*env)->GetStringUTFChars(env, jobj_name[i], 0);
    }

    /* load the geometry */
    if ( (rts_load_return=rts_load_geometry( file_name, num_objects, obj_list )) < 0 ) {
	bu_log( "Failed to load geometry, rts_load_geometry() returned %d\n", rts_load_return );
	ret = 2;
    }

    /* release the JAVA String objects that we created */
    (*env)->ReleaseStringChars( env, jfile_name, (const jchar *)file_name);
    for ( i=0; i<num_objects; i++ ) {
	(*env)->ReleaseStringChars( env, jobj_name[i], (const jchar *)obj_list[i]);
    }

    bu_free( (char *)obj_list, "obj_list" );
    bu_free( (char *)jobj_name, "jobj_name" );

    return ret;

}

JNIEXPORT jstring JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_getDbTitle(JNIEnv *env, jobject UNUSED(obj))
{
    return (*env)->NewStringUTF(env, title);
}

JNIEXPORT jstring JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_getLibraryVersion(JNIEnv *env, jobject UNUSED(obj))
{
    return (*env)->NewStringUTF(env, rt_version());
}

JNIEXPORT jint JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_openSession(JNIEnv *UNUSED(env), jobject UNUSED(obj))
{
    return 0;
}

JNIEXPORT void JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_closeSession(JNIEnv *UNUSED(env), jobject UNUSED(obj), jint UNUSED(sessionId))
{
    return;
}

JNIEXPORT jobject JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_getBoundingBox(JNIEnv *env, jobject UNUSED(obj), jint sessionId)
{
    jclass boundingBox_class, point_class;
    jmethodID boundingBox_constructor_id, point_constructor_id;
    jobject point1, point2, bb;
    pointp_t min_pt, max_pt;

    if ( sessionId < 0 || (size_t)sessionId >= num_geometries ) {
	fprintf( stderr, "Called getItemTree with invalid sessionId\n" );
	return (jobject)NULL;
    }

    min_pt = rts_geometry[sessionId]->rts_mdl_min;
    max_pt = rts_geometry[sessionId]->rts_mdl_max;

    /* get the BoundingBox class */
    if ( (boundingBox_class=(*env)->FindClass( env, "org/brlcad/numerics/BoundingBox" ) ) == NULL ) {
	fprintf( stderr, "Failed to find BoundingBox class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the BoundingBox constructor id */
    if ( (boundingBox_constructor_id=(*env)->GetMethodID( env, boundingBox_class, "<init>",
							  "(Lorg/brlcad/numerics/Point;Lorg/brlcad/numerics/Point;)V" ) ) == NULL ) {
	fprintf( stderr, "Failed to find BoundingBox constructor method id\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the Point class */
    if ( (point_class=(*env)->FindClass( env, "org/brlcad/numerics/Point" ) ) == NULL ) {
	fprintf( stderr, "Failed to find Point class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the Point constructor id */
    if ( (point_constructor_id=(*env)->GetMethodID( env, point_class, "<init>",
						    "(DDD)V" ) ) == NULL ) {
	fprintf( stderr, "Failed to find BoundingBox constructor method id\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* create the points of Bounding Box */
    point1 = (*env)->NewObject( env, point_class, point_constructor_id, min_pt[X], min_pt[Y], min_pt[Z] );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating minimum point for BoundingBox\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }
    point2 = (*env)->NewObject( env, point_class, point_constructor_id, max_pt[X], max_pt[Y], max_pt[Z] );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating maximum point for BoundingBox\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* create the BoundingBox */
    bb = (*env)->NewObject( env, boundingBox_class, boundingBox_constructor_id, point1, point2 );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating BoundingBox\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    return bb;
}

/* JAVA shootRay method
 *
 * env - the JNI environment
 * jobj - "This" object (the caller)
 * jstart_pt - the start point for this ray
 * jdir - the ray direction
 * sessionId - the identifier for this session (must have come from rts_load_geometry() or rts_open_session() )
 *
 * return - a byte array containing the hit information for the ray
 */
JNIEXPORT jbyteArray JNICALL
Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_shootRay( JNIEnv *env, jobject UNUSED(jobj),
								  jobject jstart_pt, jobject jdir, jint sessionId )
{
    jclass point_class, vect_class;
    jfieldID fid;
    jsize len; /* length of byte array */
    jbyteArray array;
    struct application *ap;
    struct bu_vlb *vlb;
    unsigned char buffer[SIZEOF_NETWORK_DOUBLE*3];
    int rayCount = 1;

    /* set up our own application structure */
    GET_APPLICATION( ap );

    /* extract start point */
    if ( (point_class = (*env)->GetObjectClass( env, jstart_pt ) ) == NULL ) {
	fprintf( stderr, "Failed to find Point class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, point_class, "x", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_pt[X] = (jdouble)(*env)->GetDoubleField( env, jstart_pt, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, point_class, "y", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_pt[Y] = (*env)->GetDoubleField( env, jstart_pt, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, point_class, "z", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_pt[Z] = (*env)->GetDoubleField( env, jstart_pt, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* extract direction vector */
    if ( (vect_class = (*env)->GetObjectClass( env, jdir ) ) == NULL ) {
	fprintf( stderr, "Failed to find Vector3 class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, vect_class, "x", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_dir[X] = (*env)->GetDoubleField( env, jdir, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, vect_class, "y", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_dir[Y] = (*env)->GetDoubleField( env, jdir, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fid = (*env)->GetFieldID( env, vect_class, "z", "D" );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ap->a_ray.r_dir[Z] = (*env)->GetDoubleField( env, jdir, fid );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* set the desired onehit flag */
    ap->a_onehit = 0;

    /* make session id available for the hit routine */
    ap->a_user = sessionId;

    vlb = ap->a_uptr;

    /* write the number of rays to the byte array (one in this case) */
    *(uint32_t *)buffer = htonl(rayCount);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

    /* write this ray info to the byte array */
    htond(buffer, (unsigned char *)ap->a_ray.r_pt, 3);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
    htond(buffer, (unsigned char *)ap->a_ray.r_dir, 3);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);

    /* shoot the ray */
    rt_shootray(ap);

    /* create the java byte array to be returned */
    len = bu_vlb_buflen(vlb);
    array = (*env)->NewByteArray( env, len );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating byte array\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }
    (*env)->SetByteArrayRegion(env, array, 0, len, (jbyte *)bu_vlb_addr(vlb) );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while setting byte array contents\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }

    FINISH_APPLICATION(ap);

    /* return JAVA result */
    return array;
}

/* routine to shoot a list of rays
 *
 * env - the JNI environment
 * jobj - "This" object (the caller)
 * aRays - a Java array of Ray objects
 * oneHit - the desired value for ap->a_onehit
 * sessionId - the identifier for this session (must have come from rts_load_geometry() or rts_open_session() )
 *
 * return - a byte array containing the hit information for all the rays in the list
 */
JNIEXPORT jbyteArray JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_shootList(JNIEnv *env, jobject UNUSED(obj), jobjectArray aRays, jint oneHit, jint sessionId)
{
    jsize rayCount;
    jsize len; /* length of byte array */
    jbyteArray array;
    jsize rayIndex;
    jclass rayClass;
    jclass pointClass;
    jclass vector3Class;
    jfieldID fidvx, fidvy, fidvz;
    jfieldID fidpx, fidpy, fidpz;
    jfieldID fidStart, fidDirection;
    struct application *ap;
    struct bu_vlb *vlb;
    unsigned char buffer[SIZEOF_NETWORK_DOUBLE*3];

    if ( (rayClass=(*env)->FindClass( env, "org/brlcad/numerics/Ray" ) ) == NULL ) {
	fprintf( stderr, "Failed to find Ray class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidStart = (*env)->GetFieldID( env, rayClass, "start", "Lorg/brlcad/numerics/Point;" );
    if ( fidStart == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidDirection = (*env)->GetFieldID( env, rayClass, "direction", "Lorg/brlcad/numerics/Vector3;" );
    if ( fidDirection == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting fid of ray direction vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    if ( (pointClass=(*env)->FindClass( env, "org/brlcad/numerics/Point" ) ) == NULL ) {
	fprintf( stderr, "Failed to find Point class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    if ( (vector3Class=(*env)->FindClass( env, "org/brlcad/numerics/Vector3" ) ) == NULL ) {
	fprintf( stderr, "Failed to find Vector3 class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpx = (*env)->GetFieldID( env, pointClass, "x", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpy = (*env)->GetFieldID( env, pointClass, "y", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpz = (*env)->GetFieldID( env, pointClass, "z", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvx = (*env)->GetFieldID( env, vector3Class, "x", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray start vector3\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvy = (*env)->GetFieldID( env, vector3Class, "y", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray start vector3\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvz = (*env)->GetFieldID( env, vector3Class, "z", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray start vector3\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* set up our own application structure */
    GET_APPLICATION( ap );

    /* set the desired onehit flag */
    ap->a_onehit = oneHit;

    /* make session id available for the hit routine */
    ap->a_user = sessionId;

    rayCount = (*env)->GetArrayLength(env, aRays);

    /* write the number of rays to the byte array */
    vlb = ap->a_uptr;
    *(uint32_t *)buffer = htonl(rayCount);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

    for(rayIndex=0 ; rayIndex<rayCount ; rayIndex++) {
	jobject ray, start, direction;

	ray = (*env)->GetObjectArrayElement(env, aRays, rayIndex);
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting ray #%d from array\n", (int)rayIndex );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	start = (*env)->GetObjectField(env, ray, fidStart);
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting ray start point\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	direction = (*env)->GetObjectField(env, ray, fidDirection);
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting ray direction vector\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_pt[X] = (jdouble)(*env)->GetDoubleField( env, start, fidpx );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting x coord of ray start point\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_pt[Y] = (jdouble)(*env)->GetDoubleField( env, start, fidpy );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting y coord of ray start point\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_pt[Z] = (jdouble)(*env)->GetDoubleField( env, start, fidpz );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting z coord of ray start point\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_dir[X] = (jdouble)(*env)->GetDoubleField( env, direction, fidvx );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting x coord of ray direction\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_dir[Y] = (jdouble)(*env)->GetDoubleField( env, direction, fidvy );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting y coord of ray direction\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	ap->a_ray.r_dir[Z] = (jdouble)(*env)->GetDoubleField( env, direction, fidvz );
	if ( (*env)->ExceptionOccurred(env) ) {
	    fprintf( stderr, "Exception thrown while getting z coord of ray direction\n" );
	    (*env)->ExceptionDescribe(env);
	    return (jobject)NULL;
	}

	/* write this ray info to the byte array */
	htond(buffer, (unsigned char *)ap->a_ray.r_pt, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
	htond(buffer, (unsigned char *)ap->a_ray.r_dir, 3);
	bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);

	/* shoot the ray */
	rt_shootray(ap);
    }

    /* create the java byte array to be returned */
    len = bu_vlb_buflen(vlb);
    array = (*env)->NewByteArray( env, len );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating byte array\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }
    (*env)->SetByteArrayRegion(env, array, 0, len, (jbyte *)bu_vlb_addr(vlb) );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while setting byte array contents\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }

    FINISH_APPLICATION(ap);

    /* return JAVA result */
    return array;
}

/* routine to shoot a grid of rays
 *
 * env - the JNI environment
 * jobj - "This" object (the caller)
 * jstart_pt - the initial ray start point
 * jdir - the common ray direction
 * jrow_diff - vector difference between each row in the grid
 * jcol_diff - vector difference between each column in the grid
 * num_rows - the number of rows in the grid
 * num_cols - the number of columns in the grid
 * oneHit - the desired value for ap->a_onehit
 * sessionId - the identifier for this session (must have come from rts_load_geometry() or rts_open_session() )
 *
 * return - a byte array containing the hit information for all the rays in the grid
 */
JNIEXPORT jbyteArray JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_shootArray(JNIEnv *env, jobject UNUSED(jobj),
												jobject jstart_pt, jobject jdir, jobject jrow_diff, jobject jcol_diff, jint num_rows, jint num_cols, jint oneHit, jint sessionId )
{
    jclass point_class, vect_class;
    jfieldID fidvx, fidvy, fidvz;
    jfieldID fidpx, fidpy, fidpz;
    jsize len; /* length of byte array */
    jbyteArray array;
    point_t base_pt;
    vect_t row_dir, col_dir, ray_dir;
    struct application *ap;
    struct bu_vlb *vlb;
    unsigned char buffer[SIZEOF_NETWORK_DOUBLE*3];
    int row, col;
    int rayCount = num_rows * num_cols;

    /* extract base point for array of rays */
    if ( (point_class = (*env)->GetObjectClass( env, jstart_pt ) ) == NULL ) {
	fprintf( stderr, "Failed to find Point class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpx = (*env)->GetFieldID( env, point_class, "x", "D" );
    if ( fidpx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    base_pt[X] = (jdouble)(*env)->GetDoubleField( env, jstart_pt, fidpx );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpy = (*env)->GetFieldID( env, point_class, "y", "D" );
    if ( fidpy == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    base_pt[Y] = (*env)->GetDoubleField( env, jstart_pt, fidpy );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidpz = (*env)->GetFieldID( env, point_class, "z", "D" );
    if ( fidpz == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    base_pt[Z] = (*env)->GetDoubleField( env, jstart_pt, fidpz );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of ray start point\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* extract direction vector for the rays */
    if ( (vect_class = (*env)->GetObjectClass( env, jdir ) ) == NULL ) {
	fprintf( stderr, "Failed to find Vector3 class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvx = (*env)->GetFieldID( env, vect_class, "x", "D" );
    if ( fidvx == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ray_dir[X] = (*env)->GetDoubleField( env, jdir, fidvx );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvy = (*env)->GetFieldID( env, vect_class, "y", "D" );
    if ( fidvy == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ray_dir[Y] = (*env)->GetDoubleField( env, jdir, fidvy );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    fidvz = (*env)->GetFieldID( env, vect_class, "z", "D" );
    if ( fidvz == 0 && (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z-fid of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    ray_dir[Z] = (*env)->GetDoubleField( env, jdir, fidvz );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of ray direction\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* extract row difference vector (the vector distance from one row to the next) */
    row_dir[X] = (*env)->GetDoubleField( env, jrow_diff, fidvx );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of row difference vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    row_dir[Y] = (*env)->GetDoubleField( env, jrow_diff, fidvy );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of row difference vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    row_dir[Z] = (*env)->GetDoubleField( env, jrow_diff, fidvz );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of row difference vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* extract the column difference vector (the vector distance from column to the next) */
    col_dir[X] = (*env)->GetDoubleField( env, jcol_diff, fidvx );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting x coord of column difference vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    col_dir[Y] = (*env)->GetDoubleField( env, jcol_diff, fidvy );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting y coord of column difference vector\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    col_dir[Z] = (*env)->GetDoubleField( env, jcol_diff, fidvz );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while getting z coord of column difference vector\n" );
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	return (jobject)NULL;
    }

    /* throw an exception if we are asked to build an impossible array */
    if ( num_rows < 1 || num_cols < 1 ) {
	jclass rtServerUsageException = (*env)->FindClass( env, "mil/army/muves/geometryservice/GeometryServiceException" );
	if ( rtServerUsageException == 0 ) {
	    return (jobject)NULL;
	}
	(*env)->ThrowNew( env, rtServerUsageException, "neither rows nor columns can be less than 1" );
	return (jobject)NULL;
    }

    /* set up our own application structure */
    GET_APPLICATION( ap );

    /* set the desired onehit flag */
    ap->a_onehit = oneHit;

    /* make session id available for the hit routine */
    ap->a_user = sessionId;

    VMOVE( ap->a_ray.r_dir, ray_dir );

    vlb = (struct bu_vlb *)ap->a_uptr;

    /* write the number of rays to the byte array */
    vlb = ap->a_uptr;
    *(uint32_t *)buffer = htonl(rayCount);
    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_LONG);

    for ( row=0; row < num_rows; row++ ) {
	for ( col=0; col < num_cols; col++ ) {
	    ap->a_ray.index = row * num_cols + col;
	    VJOIN2( ap->a_ray.r_pt, base_pt, (double)row, row_dir, (double)col, col_dir );

	    /* write this ray info to the byte array */
	    htond(buffer, (unsigned char *)ap->a_ray.r_pt, 3);
	    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);
	    htond(buffer, (unsigned char *)ap->a_ray.r_dir, 3);
	    bu_vlb_write(vlb, buffer, SIZEOF_NETWORK_DOUBLE*3);

	    /* finally, shoot this ray */
	    rt_shootray(ap);
	}
    }

    /* create the java byte array to be returned */
    len = bu_vlb_buflen(vlb);
    array = (*env)->NewByteArray( env, len );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating byte array\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }
    (*env)->SetByteArrayRegion(env, array, 0, len, (jbyte *)bu_vlb_addr(vlb) );
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while setting byte array contents\n" );
	(*env)->ExceptionDescribe(env);
	FINISH_APPLICATION(ap);
	return (jobject)NULL;
    }

    FINISH_APPLICATION(ap);

    /* return JAVA result */
    return array;
}


/*
 *				G E T I T E M T R E E
 *
 * This is the implementation of the MUVES3 Platform.getItemTree() method for BRL-CAD geometry.
 * Basically a tree walker that gathers information about the BRL-CAD objects that are prepped
 * and ready for ray-tracing. The returned tree contains leaf nodes for each BRL-CAD region in the
 * tree, and non-leaf nodes for BRL-CAD combinations above the regions. The leaf nodes are populated with the
 * region name, ident, los, material name, number of uses (dp->d_uses), and MUVES Component name (if the
 * region has a "MUVES_Component" attribute. The only information currently stored
 * in non-leaf nodes is the combination name and MUVES Component name.
 *
 * inputs:
 *	sessionId - the session identifier
 *
 * outputs:
 *	A Java ItemTree object containing data about the prepped objects in the BRL-CAD model
 */

JNIEXPORT jobject JNICALL
Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_getItemTree(JNIEnv *env, jobject UNUSED(obj), jint sessionId )
{
    jclass itemTree_class;
    jmethodID itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id, itemTree_setIdentNumber_id,
	itemTree_setMaterialName_id, itemTree_setLos_id, itemTree_setUseCount_id;
    jobject rootNode;
    jstring nodeName;
    struct db_i *dbip;
    struct rtserver_geometry *rtsg;
    size_t i;

    if ( sessionId < 0 || (size_t)sessionId >= num_geometries ) {
	fprintf( stderr, "Called getItemTree with invalid sessionId\n" );
	return (jobject)NULL;
    }

    /* get the JAVA ItemTree class */
    if ( (itemTree_class=(*env)->FindClass( env, "mil/army/muves/geometryservice/datatypes/ItemTree" )) == NULL ) {
	fprintf( stderr, "Failed to find ItemTree class\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree class constructor */
    if ( (itemTree_constructor_id=(*env)->GetMethodID( env, itemTree_class, "<init>",
						       "(Ljava/lang/String;)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree constructor\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree addSubcomponent method */
    if ( (itemTree_addcomponent_id=(*env)->GetMethodID( env, itemTree_class, "addSubComponent",
							"(Lmil/army/muves/geometryservice/datatypes/ItemTree;)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree addSubComponent method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree setMuvesComponentName method */
    if ( (itemTree_setMuvesName_id=(*env)->GetMethodID( env, itemTree_class, "setMuvesComponentName",
							"(Ljava/lang/String;)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree setMuvesComponentName method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree setIdentNumber method */
    if ( (itemTree_setIdentNumber_id=(*env)->GetMethodID( env, itemTree_class, "setIdentNumber",
							  "(I)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree setIdentNumber method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree setLos method */
    if ( (itemTree_setLos_id=(*env)->GetMethodID( env, itemTree_class, "setLos",
						  "(I)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree setLos method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree setMaterialName method */
    if ( (itemTree_setMaterialName_id=(*env)->GetMethodID( env, itemTree_class, "setMaterialName",
							   "(Ljava/lang/String;)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree setMaterialName method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* get the JAVA method id for the ItemTree setUseCount method */
    if ( (itemTree_setUseCount_id=(*env)->GetMethodID( env, itemTree_class, "setUseCount",
						       "(I)V" )) == NULL ) {
	fprintf( stderr, "Failed to get method id for ItemTree setUseCount method\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* create root node for ItemTree return */
    nodeName = (*env)->NewStringUTF(env, "root" );
    rootNode = (*env)->NewObject( env, itemTree_class, itemTree_constructor_id, nodeName );

    /* check for any exceptions */
    if ( (*env)->ExceptionOccurred(env) ) {
	fprintf( stderr, "Exception thrown while creating the ItemTree root node\n" );
	(*env)->ExceptionDescribe(env);
	return (jobject)NULL;
    }

    /* traverse the model trees for this sessionid */
    rtsg = rts_geometry[sessionId];
    dbip = rtsg->rts_rtis[0]->rtrti_rtip->rti_dbip;
    for ( i=0; i<rtsg->rts_number_of_rtis; i++ ) {
	struct rtserver_rti *rts_rti=rtsg->rts_rtis[i];
	size_t j;

	for ( j=0; j<rts_rti->rtrti_num_trees; j++ ) {
	    fillItemTree( rootNode, dbip, env, rts_rti->rtrti_trees[j], itemTree_class,
			  itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id,
			  itemTree_setMaterialName_id, itemTree_setIdentNumber_id, itemTree_setLos_id, itemTree_setUseCount_id );
	}

    }


    return rootNode;
}

/* Get the list of region names in this geometry
 *
 * The caller will use this to decode the region information returned in a byte array from the shoot methods */
JNIEXPORT jobjectArray JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_getRegionNames(JNIEnv *env, jobject UNUSED(obj), jint sessionId)
{
    Tcl_HashTable *hashTbl;
    Tcl_HashEntry *entry;
    Tcl_HashSearch searchTbl;
    int region_count;
    jobject jNameArray;
    jstring region_name;
    CLIENTDATA_INT region_number;

    hashTbl = rts_geometry[sessionId]->rts_rtis[0]->rtrti_region_names;
    region_count = rts_geometry[sessionId]->rts_rtis[0]->region_count;

    jNameArray = (*env)->NewObjectArray( env, region_count, (*env)->FindClass(env, "java/lang/String"), (jobject)NULL);
    entry = Tcl_FirstHashEntry(hashTbl, &searchTbl);

    while( entry != NULL ) {
	region_number = (CLIENTDATA_INT)Tcl_GetHashValue(entry);
	region_name = (*env)->NewStringUTF(env, Tcl_GetHashKey(hashTbl, entry));
	(*env)->SetObjectArrayElement(env, jNameArray, region_number, region_name);
	entry = Tcl_NextHashEntry(&searchTbl);
    }

    return jNameArray;
}

JNIEXPORT void JNICALL Java_mil_army_muves_brlcadservice_impl_BrlcadJNIWrapper_shutdownNative(JNIEnv *UNUSED(env), jobject UNUSED(obj))
{
    bu_log( "Shutting down...");
    rts_clean(0);
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
