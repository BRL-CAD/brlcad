/*
 *		R T S E R V E R . C
 *
 *	library for BRL-CAD raytrace server
 *
 *  Author -
 *      John R. Anderson
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 2004 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <math.h>
#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <pthread.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "rtserver.h"

#include "RtServerImpl.h"

/* verbosity flag */
static int verbose=0;

/* total number of MUVES component names */
static int comp_count=0;

/* use air flag (0 -> ignore air regions) */
static int use_air=0;

/* attribute name for MUVES components */
/* static char *muves_comp="MUVES_Comp"; */

/* array of MUVES component names */
static char **names;

/* hash tables for MUVES components */
static int hash_table_exists=0;
static Tcl_HashTable name_tbl;		/* all the MUVES component names (key is the MUVES component name,
					   value = MUVES id number */
static Tcl_HashTable ident_tbl;		/* all the non-air regions (key = ident, value = MUVES id number) */
static Tcl_HashTable air_tbl;		/* all the air regions (key = aircode, value = MUVES id number) */

/* number of threads */
static int num_threads=1;

/* the threads */
static pthread_t *threads;

/* mutex to protect the resources */
static pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;

/* mutexes to protect the input and output queues */
static pthread_mutex_t *input_queue_mutex, *output_queue_mutex;

/* input queues condition and mutex */
static pthread_mutex_t input_queue_ready_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t input_queue_ready = PTHREAD_COND_INITIALIZER;

/* output queues condition and mutex */
static pthread_mutex_t output_queue_ready_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t output_queue_ready = PTHREAD_COND_INITIALIZER;

/* the queues */
static struct rtserver_job *input_queue;
static struct rtserver_result *output_queue;
static int num_queues=3;

/* the geometry */
static struct rtserver_geometry **rts_geometry=NULL;	/* array of rtserver_geometry structures
							 * indexed by session id
							 * NULL entry -> unused slot
							 * index 0 must never be NULL
							 */
static int num_geometries=0;	/* the length of the rts_geometry array */
static int used_session_0=0;	/* flag indicating if initial session has been used */
static int needs_initialization=1;	/* flag indicating if init has already been done */
#define GEOMETRIES_BLOCK_SIZE	5	/* the number of slots to allocate at one time */

/* number of cpus (only used for call to rt_prep_parallel) */
int ncpus=1;

/* the title of this BRL-CAD database */
static char *title=NULL;


/* resources for librtserver */
struct rts_resources {
	struct bu_list rtserver_results;
	struct bu_list ray_results;
	struct bu_list ray_hits;
	struct bu_list rtserver_jobs;
	struct bu_ptbl xrays;
};

static struct rts_resources rts_resource;

void
fillItemTree( jobject parent_node,
	      struct db_i *dbip,
	      JNIEnv *env,
	      char *name,
	      jclass itemTree_class,
	      jmethodID itemTree_constructor_id,
	      jmethodID itemTree_addcomponent_id,
	      jmethodID itemTree_setMuvesName_id );


/* MACRO to add a ray to a job */
#define RTS_ADD_RAY_TO_JOB( _ajob, _aray ) bu_ptbl_ins( &(_ajob)->rtjob_rays, (long *)(_aray) );

/* MACROS for getting and releasing resources */
#define RTS_GET_XRAY( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        if( BU_PTBL_LEN( &rts_resource.xrays ) ) { \
                _p = (struct xray *)BU_PTBL_GET( &rts_resource.xrays, BU_PTBL_LEN( &rts_resource.xrays )-1 );\
                bu_ptbl_trunc( &rts_resource.xrays, BU_PTBL_LEN( &rts_resource.xrays )-1 );\
                pthread_mutex_unlock( &resource_mutex ); \
                bzero( (_p), sizeof( struct xray ) ); \
        } else { \
                pthread_mutex_unlock( &resource_mutex ); \
                _p = (struct xray *)bu_calloc( 1, sizeof( struct xray ), "xray" ); \
        } \
        (_p)->magic = RT_RAY_MAGIC;

#define RTS_FREE_XRAY( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        bu_ptbl_ins( &rts_resource.xrays, (long *)(_p) ); \
        pthread_mutex_unlock( &resource_mutex ); \
        _p = (struct xray *)NULL;


#define RTS_GET_RTSERVER_JOB( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        if( BU_LIST_NON_EMPTY( &rts_resource.rtserver_jobs ) ) { \
	       _p = BU_LIST_FIRST( rtserver_job, &rts_resource.rtserver_jobs ); \
               BU_LIST_DEQUEUE( &(_p)->l ); \
               pthread_mutex_unlock( &resource_mutex ); \
               bzero( (_p), sizeof( struct rtserver_job ) ); \
        } else { \
                pthread_mutex_unlock( &resource_mutex ); \
               _p = (struct rtserver_job *)bu_calloc( 1, sizeof( struct rtserver_job ), "rtserver_job" ); \
        } \
        BU_LIST_INIT( &(_p)->l ); \
        bu_ptbl_init( &(_p)->rtjob_rays, 128, "rtjob_rays list" );

#define RTS_FREE_RTSERVER_JOB( _p ) \
        { \
                int _i; \
                if( (_p)->l.forw ) { \
                       BU_LIST_DEQUEUE( &((_p)->l) ); \
                } \
                for( _i=0 ; _i<BU_PTBL_LEN( &(_p)->rtjob_rays) ; _i++ ) { \
                        struct xray *_xray; \
                        _xray = (struct xray *)BU_PTBL_GET( &(_p)->rtjob_rays, _i ); \
                        RTS_FREE_XRAY( _xray ); \
                } \
                bu_ptbl_free( &(_p)->rtjob_rays ); \
                pthread_mutex_lock( &resource_mutex ); \
                BU_LIST_INSERT( &rts_resource.rtserver_jobs, &((_p)->l) ); \
                pthread_mutex_unlock( &resource_mutex ); \
                _p = (struct rtserver_job *)NULL; \
        }

#define RTS_GET_RAY_HIT( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        if( BU_LIST_NON_EMPTY( &rts_resource.ray_hits ) ) { \
	       _p = BU_LIST_FIRST( ray_hit, &rts_resource.ray_hits ); \
               BU_LIST_DEQUEUE( &(_p)->l ); \
               pthread_mutex_unlock( &resource_mutex ); \
               bzero( (_p), sizeof( struct ray_hit ) ); \
               BU_LIST_INIT( &(_p)->l ); \
        } else { \
                pthread_mutex_unlock( &resource_mutex ); \
               _p = (struct ray_hit *)bu_calloc( 1, sizeof( struct ray_hit ), "ray_hit" ); \
                BU_LIST_INIT( &(_p)->l );\
        }

#define RTS_FREE_RAY_HIT( _p ) \
        BU_LIST_DEQUEUE( &(_p)->l ); \
        pthread_mutex_lock( &resource_mutex ); \
        BU_LIST_APPEND( &rts_resource.ray_hits, &(_p)->l ); \
        pthread_mutex_unlock( &resource_mutex ); \
        _p = (struct ray_hit *)NULL;

#define RTS_FREE_RAY_RESULT( _p ) \
        { \
                struct ray_hit *_rhp; \
                while( BU_LIST_WHILE( _rhp, ray_hit, &(_p)->hitHead.l ) ) { \
                        RTS_FREE_RAY_HIT( _rhp ); \
                } \
                BU_LIST_DEQUEUE( &((_p)->l) ); \
                pthread_mutex_lock( &resource_mutex ); \
                BU_LIST_INSERT( &rts_resource.ray_results, &((_p)->l) ); \
                pthread_mutex_unlock( &resource_mutex ); \
                _p = (struct ray_result *)NULL; \
        }

#define RTS_GET_RAY_RESULT( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        if( BU_LIST_NON_EMPTY( &rts_resource.ray_results ) ) { \
	       _p = BU_LIST_FIRST( ray_result, &rts_resource.ray_results ); \
               BU_LIST_DEQUEUE( &(_p)->l ); \
               pthread_mutex_unlock( &resource_mutex ); \
               bzero( (_p), sizeof( struct ray_result ) ); \
        } else { \
                pthread_mutex_unlock( &resource_mutex ); \
               _p = (struct ray_result *)bu_calloc( 1, sizeof( struct ray_result ), "ray_result" ); \
        } \
        BU_LIST_INIT( &((_p)->l) );\
        BU_LIST_INIT( &(_p)->hitHead.l );


#define RTS_GET_RTSERVER_RESULT( _p ) \
        pthread_mutex_lock( &resource_mutex ); \
        if( BU_LIST_NON_EMPTY( &rts_resource.rtserver_results ) ) { \
	       _p = BU_LIST_FIRST( rtserver_result, &rts_resource.rtserver_results ); \
               BU_LIST_DEQUEUE( &(_p)->l ); \
               pthread_mutex_unlock( &resource_mutex ); \
               bzero( (_p), sizeof( struct rtserver_result ) ); \
        } else { \
                pthread_mutex_unlock( &resource_mutex ); \
               _p = (struct rtserver_result *)bu_calloc( 1, sizeof( struct rtserver_result ), "rtserver_result" ); \
        } \
        BU_LIST_INIT( &(_p)->l ); \
        BU_LIST_INIT( &(_p)->resultHead.l );

#define RTS_FREE_RTSERVER_RESULT( _p ) \
        { \
                struct ray_result *_rrp; \
                while( BU_LIST_WHILE( _rrp, ray_result, &(_p)->resultHead.l ) ) { \
                        RTS_FREE_RAY_RESULT( _rrp ); \
                } \
                if( (_p)->the_job ) { \
                     RTS_FREE_RTSERVER_JOB( (_p)->the_job ); \
                } \
                if( (_p)->l.forw ) { \
                        BU_LIST_DEQUEUE( &((_p)->l) ); \
                } \
                pthread_mutex_lock( &resource_mutex ); \
                BU_LIST_INSERT( &rts_resource.rtserver_results, &((_p)->l) ); \
                pthread_mutex_unlock( &resource_mutex ); \
        }
                


/* count the number of members in a bu_list structure */
int
count_list_members( struct bu_list *listhead )
{
	int count=0;
	struct bu_list *l;
	for( BU_LIST_FOR( l, bu_list, listhead ) ) {
		count++;
	}

	return( count );
}


/* initialize the librtserver resources */
void
rts_resource_init()
{
        pthread_mutex_lock( &resource_mutex ); \
	BU_LIST_INIT( &rts_resource.rtserver_results );
	BU_LIST_INIT( &rts_resource.ray_results );
	BU_LIST_INIT( &rts_resource.ray_hits );
	BU_LIST_INIT( &rts_resource.rtserver_jobs );
	bu_ptbl_init( &rts_resource.xrays, 128, "xrays" );
        pthread_mutex_unlock( &resource_mutex ); \
}


/* print a summary of librt resources */
void
rts_pr_resource_summary()
{
	fprintf( stderr, "Resource Summary:\n" );

        pthread_mutex_lock( &resource_mutex ); \
	fprintf( stderr, "\t %d rtserver_job structures\n",
		 count_list_members( &rts_resource.rtserver_jobs ) );
	fprintf( stderr, "\t %d rtserver_result structures\n",
		 count_list_members( &rts_resource.rtserver_results ) );
	fprintf( stderr, "\t %d ray_result structures\n",
		 count_list_members( &rts_resource.ray_results ) );
	fprintf( stderr, "\t %d ray_hit structures\n",
		 count_list_members( &rts_resource.ray_hits ) );
	fprintf( stderr, "\t %d xray structures\n",
		 BU_PTBL_LEN( &rts_resource.xrays ) );
        pthread_mutex_unlock( &resource_mutex ); \
}


/* create a new copy of the rtserver_geometry (typically for a new session)
 * based on an existing session (usually zero)
 */
void
copy_geometry( int dest, int src )
{
	int i, j;

	/* allocate some memory */
	rts_geometry[dest] = (struct rtserver_geometry *)bu_calloc( 1,
				 sizeof( struct rtserver_geometry ),
				 "rts_geometry[]" );

	/* allocate memory for the rtserver_rti structures */
	rts_geometry[dest]->rts_number_of_rtis =
		rts_geometry[src]->rts_number_of_rtis;
	rts_geometry[dest]->rts_rtis =
		(struct rtserver_rti **)bu_calloc( rts_geometry[dest]->rts_number_of_rtis,
									  sizeof( struct rtserver_rti *),
									  "rtserver_rti *" );
	/* initialize our overall bounding box */
	VSETALL( rts_geometry[dest]->rts_mdl_min, MAX_FASTF );
	VREVERSE( rts_geometry[dest]->rts_mdl_max, rts_geometry[dest]->rts_mdl_min );

	/* fill out each rtsserver_rti structure */
	for( i=0 ; i < rts_geometry[dest]->rts_number_of_rtis ; i++ ) {
		struct rt_i *rtip;

		/* the "calloc" call initializes the xform pointers to NULL */
		rts_geometry[dest]->rts_rtis[i] = (struct rtserver_rti *)bu_calloc( 1,
						    sizeof( struct rtserver_rti ),
						    "rtserver_rti" );

		/* copy the rt_i pointer (the same ones are used by all the sessions */
		rtip = rts_geometry[src]->rts_rtis[i]->rtrti_rtip;
		rts_geometry[dest]->rts_rtis[i]->rtrti_rtip = rtip;
		if( rts_geometry[src]->rts_rtis[i]->rtrti_name ) {
			rts_geometry[dest]->rts_rtis[i]->rtrti_name =
				bu_strdup( rts_geometry[src]->rts_rtis[i]->rtrti_name );
		}
		rts_geometry[dest]->rts_rtis[i]->rtrti_num_trees =
			rts_geometry[src]->rts_rtis[i]->rtrti_num_trees;
		rts_geometry[dest]->rts_rtis[i]->rtrti_trees =
			(char **) bu_calloc( rts_geometry[dest]->rts_rtis[i]->rtrti_num_trees,
					    sizeof( char *),
					    "rtrti_trees" );
		for( j=0 ; j<rts_geometry[dest]->rts_rtis[i]->rtrti_num_trees ; j++ ) {
			rts_geometry[dest]->rts_rtis[i]->rtrti_trees[j] =
				bu_strdup( rts_geometry[src]->rts_rtis[i]->rtrti_trees[j] );
		}

		/* update our overall bounding box */
		VMINMAX( rts_geometry[dest]->rts_mdl_min, rts_geometry[dest]->rts_mdl_max, rtip->mdl_min );
		VMINMAX( rts_geometry[dest]->rts_mdl_min, rts_geometry[dest]->rts_mdl_max, rtip->mdl_max );
	}


	/* hash table of component names is shared */
	rts_geometry[dest]->rts_comp_names =
		rts_geometry[src]->rts_comp_names;
}


/* routine to free memory associated with the rtserver_geometry */
void
rts_clean( int sessionid)
{
	int i, j;

	if( sessionid >= 0 && sessionid < num_geometries && rts_geometry[sessionid] ) {
		/* free all old geometry */
		for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
			struct rtserver_rti *rtsrtip;

			rtsrtip = rts_geometry[sessionid]->rts_rtis[i];
			if( rtsrtip->rtrti_name ) {
				bu_free( rtsrtip->rtrti_name, "rtserver assembly name" );
			}
			if( rtsrtip->rtrti_xform ) {
				bu_free( rtsrtip->rtrti_xform, "rtserver xform matrix" );
			}
			if( rtsrtip->rtrti_inv_xform ) {
				bu_free( rtsrtip->rtrti_inv_xform, "rtserver inverse xform matrix" );
			}

			for( j=0 ; j<rtsrtip->rtrti_num_trees ; j++ ) {
				bu_free( rtsrtip->rtrti_trees[j], "rtserver tree name" );
			}
			if( rtsrtip->rtrti_trees ) {
				bu_free( rtsrtip->rtrti_trees, "rtserver tree names" );
			}

			if( rtsrtip->rtrti_rtip ) {
				rt_clean( rtsrtip->rtrti_rtip );
			}
		}
		if( rts_geometry[sessionid]->rts_comp_names ) {
			Tcl_DeleteHashTable( rts_geometry[sessionid]->rts_comp_names );
		}
		bu_free( rts_geometry[sessionid], "rts_geometry" );
		rts_geometry[sessionid] = NULL;
	}
}

/* routine to initialize anything that needs initializing */
void
rts_init()
{
	if( !needs_initialization ) {
		return;
	}
	/* Things like bu_malloc() must have these initialized for use with parallel processing */
	bu_semaphore_init( RT_SEM_LAST );

	/* initialize the rtserver resources (cached structures) */
	rts_resource_init();

	needs_initialization = 0;
}

/* routine to create a new sesion id
 *
 * Uses sessionid 0 if no sessions have been requesdted yet
 * Otherwise, make a copy of the session
 */
int
rts_open_session()
{
	int i;

	/* make sure we have some geometry */
	if( num_geometries == 0 || rts_geometry[0] == NULL ) {
		fprintf( stderr, "rtServer: ERROR: no geometry loaded!!\n" );
		return -1;
	}

	/* if the initial session is not yet used, just return it */
	if( !used_session_0 ) {
		used_session_0 = 1;
		return 0;
	}

	/* create a new session by making a new copy of session #0 */
	/* look for an empty slot */
	for( i=1 ; i<num_geometries ; i++ ) {
		if( !rts_geometry[i] ) {
			break;
		}
	}

	if( i >= num_geometries ) {
		/* need more slots */
		num_geometries += GEOMETRIES_BLOCK_SIZE;
		rts_geometry = (struct rtserver_geometry **)bu_realloc( rts_geometry,
				    num_geometries * sizeof( struct rtserver_geometry **),
				    "realloc rtserver_geometry" );
	}

	/* copy into slot #i from session 0 */
	copy_geometry( i, 0 );

	return( i );
}


/* routine to set all the xforms to NULL for the given session id */
void
reset_xforms( int sessionid )
{
	int i;

	for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
		struct rtserver_rti *rts_rtip = rts_geometry[sessionid]->rts_rtis[i];

		if( rts_rtip->rtrti_xform ) {
			bu_free( (char *)rts_rtip->rtrti_xform, "xform" );
			rts_rtip->rtrti_xform = NULL;
		}
		if( rts_rtip->rtrti_inv_xform ) {
			bu_free( (char *)rts_rtip->rtrti_inv_xform, "inv_xform" );
			rts_rtip->rtrti_inv_xform = NULL;
		}
	}
}


/* routine to close a session
 * session id 0 is just marked as closed, but not freed
 * any other session is deleted
 */
void
rts_close_session( int sessionid )
{
	int i,j;

	if( sessionid == 0 ) {
		/* never eliminate sessionid 0 */
		used_session_0 = 0;

		/* reset any xforms */
		reset_xforms( sessionid );

		return;
	}
	if( sessionid >= num_geometries ) {
		/* no such session */
		return;
	}
	if( !rts_geometry[sessionid] ) {
		/* this session has already been closed (or never opened) */
		return;
	}

	/* free the xforms */
	reset_xforms( sessionid );

	/* free everything else */
	for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
		struct rtserver_rti *rts_rtip = rts_geometry[sessionid]->rts_rtis[i];

		rts_rtip->rtrti_rtip = NULL;
		if( rts_rtip->rtrti_name ) {
			bu_free( rts_rtip->rtrti_name, "name" );
			rts_rtip->rtrti_name = NULL;
		}
		for( j=0 ; j<rts_rtip->rtrti_num_trees ; j++ ) {
			bu_free( rts_rtip->rtrti_trees[j], "tree" );
		}
		bu_free( (char *)rts_rtip->rtrti_trees, "rtrti_trees" );
		rts_rtip->rtrti_trees = NULL;

		bu_free( (char *)rts_rtip, "rts_rtip" );
		rts_geometry[sessionid]->rts_rtis[i] = NULL;
	}

	bu_free( (char *)rts_geometry[sessionid]->rts_rtis, "rtrtis" );

	bu_free( (char*)rts_geometry[sessionid], "session" );

	/* mark this slot as unused */
	rts_geometry[sessionid] = NULL;
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
rts_load_geometry( char *filename, int use_articulation, int num_objs, char **objects )
{
	struct rt_i *rtip;
	struct db_i *dbip;
	Tcl_Interp *interp;
	Tcl_Obj *rtserver_data;
	int i, j;
	int sessionid;
	const char *attrs[] = {(const char *)"muves_comp", (const char *)NULL };

	/* clean up any prior geometry data */
	if( rts_geometry ) {
		struct db_i *dbip;

		dbip = rts_geometry[0]->rts_rtis[0]->rtrti_rtip->rti_dbip;
		for( i=0 ; i<num_geometries ; i++ ) {
			if( rts_geometry[i] ) {
				rts_clean( i );
			}
		}
		num_geometries = 0;
		bu_free( (char *)rts_geometry, "rts_geometry" );
		rts_geometry = NULL;
	}

	if( hash_table_exists ) {
		Tcl_DeleteHashTable( &name_tbl );
		Tcl_DeleteHashTable( &ident_tbl);
		Tcl_DeleteHashTable( &air_tbl );
		hash_table_exists = 0;
	}

	/* create the new session */
	if( num_geometries < 1 ) {
		num_geometries = 5;	/* create five slots to start */
		sessionid = 0;
		rts_geometry = (struct rtserver_geometry **)bu_calloc( num_geometries,
						 sizeof( struct rts_geometry *),
						 "rts_geometry" );
	}

	/* open the BRL-CAD model */
	rtip = rt_dirbuild( filename, (char *)NULL, 0 );
	if( rtip == RTI_NULL ) {
		return -1;
	}

	/* grab the DB instance pointer (just for convenience) */
	dbip = rtip->rti_dbip;

	/* set the title */
	title = bu_strdup( dbip->dbi_title );

	/* set the use air flags */
	rtip->useair = use_air;

	if( use_articulation && objects ) {
		fprintf( stderr, "Cannot use articulation when specifiying object names on the comand line for rtserver\n" );
		return -2;
	} else if( objects ) {
		int num_trees;

		/* load the specified objects */
		/* malloc some memory for the rtserver geometry structure (bu_calloc zeros the memory) */
		rts_geometry[sessionid] = (struct rtserver_geometry *)bu_calloc( 1,
							 sizeof( struct rtserver_geometry ),
							 "rtserver geometry" );

		/* just one RT instance */
		rts_geometry[sessionid]->rts_number_of_rtis = 1;
		rts_geometry[sessionid]->rts_rtis = (struct rtserver_rti **)bu_malloc( sizeof( struct rtserver_rti *),
									    "rtserver_rti" );
		rts_geometry[sessionid]->rts_rtis[0] = (struct rtserver_rti *)bu_calloc( 1,
							     sizeof( struct rtserver_rti ),
							     "rtserver_rti" );
		rts_geometry[sessionid]->rts_rtis[0]->rtrti_rtip = rtip;
		num_trees = num_objs;
		rts_geometry[sessionid]->rts_rtis[0]->rtrti_num_trees = num_trees;
		if( verbose ) {
			fprintf( stderr, "num_trees = %d\n", num_trees );
		}

		/* malloc some memory for pointers to the object names */
		rts_geometry[sessionid]->rts_rtis[0]->rtrti_trees = (char **)bu_calloc( num_trees,
						     sizeof( char *),
						     "rtrti_trees" );

		for( i=0 ; i<num_trees ; i++ ) {
			rts_geometry[sessionid]->rts_rtis[0]->rtrti_trees[i] = bu_strdup( objects[i] );
		}
	} else if( use_articulation ) {
		/* XXX get articulation data */
	} else {
		/* user just wants a single RT instance, with no articulation */
		struct directory *dp;
		struct rt_db_internal intern;
		struct rt_binunif_internal *bip;
		int index1, index2;
		int id;
		int data_len;

		/* find the "rtserver_data" binary object */
		dp = db_lookup( dbip, "rtserver_data", LOOKUP_QUIET );
		if( dp == DIR_NULL ) {
			/* not found, cannot continue */
			return -2;
		}

		/* fetch the "rtserver_data" object */
		if( (id=rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource )) !=  ID_BINUNIF ) {
			/* either we did not get the object, or it is not a binary object */
			return -3;
		}

		/* malloc some memory for the rtserver geometry structure (bu_calloc zeros the memory) */
		rts_geometry[sessionid] = (struct rtserver_geometry *)bu_calloc( 1,
							sizeof( struct rtserver_geometry ),
							"rtserver geometry" );

		/* just one RT instance */
		rts_geometry[sessionid]->rts_number_of_rtis = 1;
		rts_geometry[sessionid]->rts_rtis = (struct rtserver_rti **)bu_malloc( sizeof( struct rtserver_rti *),
									    "rtserver_rti" );
		rts_geometry[sessionid]->rts_rtis[0] = (struct rtserver_rti *)bu_calloc( 1,
						       sizeof( struct rtserver_rti ),
						       "rtserver_rti" );
		rts_geometry[sessionid]->rts_rtis[0]->rtrti_rtip = rtip;

		/* use Tcl to interpret the rtserver_data */
		interp = Tcl_CreateInterp();

		/* get the internal form of the binary object from the above fetch */
		bip = (struct rt_binunif_internal *)intern.idb_ptr;

		/* make a list object to hold the rtserver data, one line per list element */
		rtserver_data = Tcl_NewObj();
		index1 = 0;
		index2 = index1;

		/* step through the rtserver_data buffer, appending each line as an element */
		while( index2 < bip->count ) {
			Tcl_Obj *tmp;
			int length;

			/* find end of line */
			while( index2 < bip->count &&
			       bip->u.int8[index2] != '\n' &&
			       bip->u.int8[index2] != '\0' ) {
				index2++;
			}
			length = index2 - index1;
			if( length > 0 ) {
				/* make a new object and append it to the list */
				tmp = Tcl_NewStringObj( &bip->u.int8[index1], index2 - index1 );
				Tcl_ListObjAppendElement( interp, rtserver_data, tmp );
			}
			index2++;
			index1 = index2;
		}

		/* free the binary object */
		rt_db_free_internal( &intern, &rt_uniresource );

		/* get the number of lines in the final object */
		Tcl_ListObjLength( interp, rtserver_data, &data_len );

		/* look for top level object (check each line in the list) */
		for( i=0 ; i<data_len ; i++ ) {
			Tcl_Obj *aline;
			Tcl_Obj *key;
			int found=0;

			/* get the next line from the list */
			Tcl_ListObjIndex( interp, rtserver_data, i, &aline );

			/* get the first element from this line */
			Tcl_ListObjIndex( interp, aline, 0, &key );

			/* is this the "rtserver_tops" key?? */
			if( !strcmp( "rtserver_tops", Tcl_GetStringFromObj( key, NULL ) ) ) {
				/* found top level object */
				Tcl_Obj *value;
				int num_trees=0;

				/* get the value for this key (the next list element) */
				Tcl_ListObjIndex( interp, aline, 1, &value );

				/* how long is the valu list (number of top level object) */
				Tcl_ListObjLength( interp, value, &num_trees );

				/* save this number in the rts_geometry structure */
				rts_geometry[sessionid]->rts_rtis[0]->rtrti_num_trees = num_trees;
				if( verbose ) {
					fprintf( stderr, "num_trees = %d\n", num_trees );
				}

				/* malloc some memory for pointers to the object names */
				rts_geometry[sessionid]->rts_rtis[0]->rtrti_trees = (char **)bu_calloc( num_trees,
						     sizeof( char *),
						     "rtrti_trees" );

				/* get the names of the top-level BRL-CAD objects */
				for( j=0 ; j < num_trees ; j++ ) {
					Tcl_Obj *tree;

					Tcl_ListObjIndex( interp, value, j, &tree );

					/* copy the names */
					rts_geometry[sessionid]->rts_rtis[0]->rtrti_trees[j] =
						bu_strdup( Tcl_GetString( tree ) );
					if( verbose ) {
						fprintf( stderr, "\t%s\n", rts_geometry[sessionid]->rts_rtis[0]->rtrti_trees[j] );
					}
				}
				found = 1;
				break;
			}
			if( found ) {
				break;
			}
		}
		Tcl_DeleteInterp( interp );
	}


	/* initialize our overall bounding box */
	VSETALL( rts_geometry[sessionid]->rts_mdl_min, MAX_FASTF );
	VREVERSE( rts_geometry[sessionid]->rts_mdl_max, rts_geometry[sessionid]->rts_mdl_min );

	/* for each RT instance, get its trees */
	for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
		struct rtserver_rti *rts_rtip;
		struct rt_i *rtip;

		/* cache the rtserver_rti pointer and its associated rt instance pointer */
		rts_rtip = rts_geometry[sessionid]->rts_rtis[i];
		rtip = rts_rtip->rtrti_rtip;

		/* create resource structures for each thread */
		for( j=0 ; j<num_threads ; j++ ) {
			struct resource *resp;

			resp = (struct resource *)bu_calloc( 1, sizeof( struct resource ), "resource" );
			rt_init_resource( resp, j, rtip );
		}

		/* get the BRL-CAD objects for this rt instance */
		if( verbose ) {
			fprintf( stderr, "Getting trees:\n" );
			for( j=0 ; j<rts_rtip->rtrti_num_trees ; j++ ) {
				fprintf( stderr, "\t%s\n", rts_rtip->rtrti_trees[j] );
			}
		}
		if( rt_gettrees_and_attrs( rtip, attrs, rts_rtip->rtrti_num_trees,
				 (const char **)rts_rtip->rtrti_trees, ncpus ) ) {
			fprintf( stderr, "Failed to get BRL-CAD objects:\n" );
			for( j=0 ; j<rts_rtip->rtrti_num_trees ; j++ ) {
				fprintf( stderr, "\t%s\n", rts_rtip->rtrti_trees[j] );
			}
			return -4;
		}

		/* prep the geometry for raytracing */
		rt_prep_parallel( rtip, ncpus );

		/* update our overall bounding box */
		VMINMAX( rts_geometry[sessionid]->rts_mdl_min, rts_geometry[sessionid]->rts_mdl_max, rtip->mdl_min );
		VMINMAX( rts_geometry[sessionid]->rts_mdl_min, rts_geometry[sessionid]->rts_mdl_max, rtip->mdl_max );
	}

	if( verbose ) {
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
	if( verbose ) {
		fprintf( stderr, "Missed!!!\n" );
	}
	return 0;
}

/* ray hit routine for rt_shootray()
 *
 * this routine adds the list of ray_hit structures to the ray_result structure for this ray
 */
int
rts_hit( struct application *ap, struct partition *partHeadp, struct seg *segs )
{
	struct partition *pp;
	struct ray_result *ray_res;

	if( verbose ) {
		fprintf( stderr, "Got a hit!!!\n" );
	}

	/* get the results pointer from the application structure */
	ray_res = (struct ray_result *)ap->a_uptr;

	/* save a pointer to the fired ray */
	ray_res->the_ray = &ap->a_ray;

	/* build a list of hits */
	for( BU_LIST_FOR( pp, partition, (struct bu_list *)partHeadp ) ) {
		struct ray_hit *ahit;
		struct region *rp;
		Tcl_HashEntry *entry;

		/* get one hit structure */
		RTS_GET_RAY_HIT( ahit );

		/* fill in the data for this hit */
		ahit->hit_dist = pp->pt_inhit->hit_dist;
		ahit->los = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		RT_HIT_NORMAL( ahit->enter_normal, pp->pt_inhit,
			       pp->pt_inseg->seg_stp, 0, pp->pt_inflip );
		RT_HIT_NORMAL( ahit->exit_normal, pp->pt_outhit,
			       pp->pt_outseg->seg_stp, 0, pp->pt_outflip );

		rp = pp->pt_regionp;

		/* find has table entry, if we have a table (to get MUVES_Component index) */
		if( hash_table_exists ) {
			if( rp->reg_aircode ) {
				entry = Tcl_FindHashEntry( &air_tbl, (ClientData)rp->reg_aircode );
			} else {
				entry = Tcl_FindHashEntry( &ident_tbl, (ClientData)rp->reg_regionid );
			}
		} else {
			entry = NULL;
		}

		/* assign comp_id based on hash table results */
		if( entry == NULL ) {
			ahit->comp_id = 0;
		} else {
			ahit->comp_id = (int)Tcl_GetHashValue( entry );
		}

		/* stash a pointer to the region structure (used by JAVA code to get region name) */
		ahit->regp = rp;

		/* add this to our list of hits */
		BU_LIST_INSERT( &ray_res->hitHead.l, &ahit->l );

		if( verbose ) {
			fprintf( stderr, "\tentrance at dist=%g, hit region %s (id = %d)\n",
				 pp->pt_inhit->hit_dist, rp->reg_name, ahit->comp_id );
			fprintf( stderr, "\t\texit at dist = %g\n", pp->pt_outhit->hit_dist );
		}
	}

	return 1;
}

/* Routine to weave a list of results that are from multiple shots.
 * These are typically from articulated geometry where each "piece" is
 * raytraced separately. This routine must order them and handle any overlaps
 */
void
rts_uber_boolweave( struct ray_result *ray_res )
{
	/* XXX */
}


/* this is the code for a thread in the rtserver */
void *
rtserver_thread( void *num )
{

	int queues_are_empty=1;		/* initially, all the queues will be empty */
	struct application ap;
	int thread_no=(int)(num);

	if( verbose ) {
		fprintf( stderr, "starting thread x%lx (%d)\n", (long unsigned int)pthread_self(), thread_no );
	}

	/* set up our own application structure */
	bzero( &ap, sizeof( struct application ) );
	ap.a_hit = rts_hit;
	ap.a_miss = rts_miss;
	ap.a_logoverlap = rt_silent_logoverlap;

	/* run forever */
	while( 1 ) {
		int queue;
		int count_empty;

		if( queues_are_empty ) {
			/* wait until someone says there is stuff in an input queue */
			pthread_mutex_lock( &input_queue_ready_mutex );
			pthread_cond_wait( &input_queue_ready, &input_queue_ready_mutex );
			pthread_mutex_unlock( &input_queue_ready_mutex );
			queues_are_empty = 0;
		}

		/* check all the queues in order (priority) */
		count_empty = 0;
		for( queue=0 ; queue<num_queues ; queue++ ) {
			pthread_mutex_lock( &input_queue_mutex[queue] );
			if( BU_LIST_NON_EMPTY( &input_queue[queue].l ) ) {
				struct rtserver_job *ajob;
				struct rtserver_result *aresult;
				struct xray *aray;
				int sessionid;
				int i, j;

				/* get next job from this queue */
				ajob = BU_LIST_FIRST( rtserver_job, &input_queue[queue].l );
				BU_LIST_DEQUEUE( &ajob->l );
				pthread_mutex_unlock( &input_queue_mutex[queue] );

				/* if this job is an exit signal, just exit */
				if( ajob->exit_flag ) {
					pthread_exit( (void *)0 );
				}

				/* grab the session id */
				sessionid = ajob->sessionid;

				/* get a result structure for this job */
				RTS_GET_RTSERVER_RESULT( aresult );

				/* remember which job we are from */
				aresult->the_job = ajob;

				/* initialize hit count */
				aresult->got_some_hits = 0;

				/* do some work
				 * we may have a bunch of rays for this job
				 */
				for( j=0 ; j<BU_PTBL_LEN( &ajob->rtjob_rays ) ; j++ ) {
					struct ray_result *ray_res;

					aray = (struct xray *)BU_PTBL_GET( &ajob->rtjob_rays, j );

					/* get a ray result structure for this ray */
					RTS_GET_RAY_RESULT( ray_res );

					/* add this ray result structure to the overall result structure */
					BU_LIST_INSERT( &aresult->resultHead.l, &ray_res->l );

					/* stash a pointer to the ray result structure in the application
					 * structure (so the hit routine can find it)
					 */
					ap.a_uptr = (genptr_t)ray_res;

					if( verbose ) {
						fprintf( stderr, "thread x%lx (%d) got job %d\n",
							 (unsigned long int)pthread_self(), thread_no, ajob->rtjob_id );
					}

					/* shoot this ray at each rt_i structure for this session */
					for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
						struct rtserver_rti *rts_rtip;
						struct rt_i *rtip;

						rts_rtip = rts_geometry[sessionid]->rts_rtis[i];
						rtip = rts_rtip->rtrti_rtip;
						ap.a_rt_i = rtip;
						ap.a_resource =
							(struct resource *)BU_PTBL_GET( &rtip->rti_resources,
								    thread_no );
						if( rts_rtip->rtrti_xform ) {
							MAT4X3PNT( ap.a_ray.r_pt,
								   rts_rtip->rtrti_xform,
								   aray->r_pt );
							MAT4X3VEC( ap.a_ray.r_dir,
								   rts_rtip->rtrti_xform,
								   aray->r_dir );
						} else {
							VMOVE( ap.a_ray.r_pt, aray->r_pt );
							VMOVE( ap.a_ray.r_dir, aray->r_dir );
						}
						if( verbose ) {
							fprintf( stderr, "shooting ray (%g %g %g) -> (%g %g %g)\n",
								 V3ARGS( ap.a_ray.r_pt ), V3ARGS( ap.a_ray.r_dir ) );
						}
						if( rt_shootray( &ap ) ) {
							aresult->got_some_hits = 1;
						}
					}

					/* weave together results of all the rays
					 * nothing to do if we only have one rt_i
					 */
					rts_uber_boolweave( ray_res );

					/* put results on output queue */
					pthread_mutex_lock( &output_queue_mutex[queue] );
					BU_LIST_INSERT( &output_queue[queue].l, &aresult->l );
					pthread_mutex_unlock( &output_queue_mutex[queue] );

					/* let everyone know that results are available */
					pthread_mutex_lock( &output_queue_ready_mutex );
					pthread_cond_broadcast( &output_queue_ready );
					pthread_mutex_unlock( &output_queue_ready_mutex );
				}

				break;
			} else {
				/* nothing in this queue */
				pthread_mutex_unlock( &input_queue_mutex[queue] );
				count_empty++;
			}
		}
		if( count_empty == num_queues ) {
			queues_are_empty = 1;
		}
	}
	return 0;
}

/* Routine to submit a job to a specified input queue */
void
rts_submit_job( struct rtserver_job *ajob, int queue )
{
	/* insert this job into the input queue (at the end) */
	pthread_mutex_lock( &input_queue_mutex[queue] );
	BU_LIST_INSERT( &input_queue[queue].l, &ajob->l );
	pthread_mutex_unlock( &input_queue_mutex[queue] );

	/* let everyone know that jobs are waiting */
	pthread_mutex_lock( &input_queue_ready_mutex );
	pthread_cond_broadcast( &input_queue_ready );
	pthread_mutex_unlock( &input_queue_ready_mutex );

}


/* Routine to start the server threads.
 * the number of threads and queues are global
 */
void
rts_start_server_threads()
{
	int i;

	/* create the queues */
	input_queue = (struct rtserver_job *)calloc( num_queues, sizeof( struct rtserver_job ) );
	output_queue = (struct rtserver_result *)calloc( num_queues,
							 sizeof( struct rtserver_result ) );

	/* create the mutexes */
	input_queue_mutex = (pthread_mutex_t *)calloc( num_queues, sizeof( pthread_mutex_t ) );
	output_queue_mutex = (pthread_mutex_t *)calloc( num_queues, sizeof( pthread_mutex_t ) );

	/* initialize the mutexes and the queues */
	for( i=0 ; i<num_queues ; i++ ) {
		BU_LIST_INIT( &input_queue[i].l );
		BU_LIST_INIT( &output_queue[i].l );
		pthread_mutex_init( &input_queue_mutex[i], NULL );
		pthread_mutex_init( &output_queue_mutex[i], NULL );
	}

	/* create the thread variables */
	threads = (pthread_t *)calloc( num_threads, sizeof( pthread_t ) );

	/* start the threads */
	for( i=0 ; i<num_threads ; i++ ) {
		(void)pthread_create( &threads[i], NULL, rtserver_thread, (void *)i );
	}
}

/* Routine to get any available result for the specified session.
 * This routine does not wait, just checks and returns
 *
 * return:
 *	NULL - no results availabel right now for this session is
 *	struct rtserver_result - the highest priority result available for this session
 */
struct rtserver_result *
rts_get_any_waiting_result( int sessionid )
{
	struct rtserver_result *aresult=NULL;
	int queue;

	/* check all the queues in priority order */
	for( queue=0 ; queue<num_queues ; queue++ ) {
		/* lock the queue */
		pthread_mutex_lock( &output_queue_mutex[queue] );

		/* check for a result */
		
		if( BU_LIST_NON_EMPTY( &output_queue[queue].l ) ) {
			for( BU_LIST_FOR( aresult, rtserver_result, &output_queue[queue].l ) ) {
				aresult = BU_LIST_FIRST( rtserver_result, &output_queue[queue].l );
				if( aresult->the_job->sessionid == sessionid ) {

					/* remove the result from the queue */
					BU_LIST_DEQUEUE( &aresult->l );

					/* unlock the queue */
					pthread_mutex_unlock( &output_queue_mutex[queue] );

					/* return the result */
					return( aresult );
				}
			}
		}

		/* unlock the queue */
		pthread_mutex_unlock( &output_queue_mutex[queue] );
	}

	/* no results available */
	return( NULL );
}

/* Routine to submit a job and get the results.
 * This routine submits a job to the highest priority queue and waits for the result
 */
struct rtserver_result *
rts_submit_job_and_wait( struct rtserver_job *ajob )
{
	int id;
	int queue=0;
	int queue_is_empty=0;
	int sessionid;

	/* grab some identifying info */
	id = ajob->rtjob_id;
	sessionid = ajob->sessionid;

	/* submit the job */
	rts_submit_job( ajob, queue );

	/* run until we get our result from this queue */
	while( 1 ) {
		struct rtserver_result *aresult;

		if( queue_is_empty ) {
			/* wait until someone says there is stuff in the queue */
			pthread_mutex_lock( &output_queue_ready_mutex );
			pthread_cond_wait( &output_queue_ready, &output_queue_ready_mutex );
			pthread_mutex_unlock( &output_queue_ready_mutex );
			queue_is_empty = 0;
		}

		/* lock the queue */
		pthread_mutex_lock( &output_queue_mutex[queue] );

		/* check for a result */
		if( BU_LIST_NON_EMPTY( &output_queue[queue].l ) ) {
			/* look for our result */
			for( BU_LIST_FOR( aresult, rtserver_result, &output_queue[queue].l ) ) {
				if( aresult->the_job->rtjob_id == id &&
				    aresult->the_job->sessionid == sessionid ) {
					BU_LIST_DEQUEUE( &aresult->l );
					break;
				}
			}

			if( aresult->the_job->rtjob_id == id &&
			    aresult->the_job->sessionid == sessionid ) {

				/* unlock the queue */
				pthread_mutex_unlock( &output_queue_mutex[queue] );

				/* return the result */
				return( aresult );
			} else {
				/* unlock the queue */
				pthread_mutex_unlock( &output_queue_mutex[queue] );

				queue_is_empty = 1;
			}
		} else {
			/* unlock the queue */
			pthread_mutex_unlock( &output_queue_mutex[queue] );

			/* nothing available */
			queue_is_empty = 1;
		}
	}
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
	Tcl_HashEntry *name_entry, *ident_entry, *air_entry;
	Tcl_HashSearch search;
	int i, j;
	int sessionid;

	/* make sure we have some geometry */
	if( !rts_geometry ) {
		return;
	}

	for( sessionid=0 ; sessionid<num_geometries ; sessionid++ ) {
		if( rts_geometry[sessionid] ) {
			break;
		}
	}

	if( !rts_geometry[sessionid] ) {
		return;
	}

	/* initialize the hash tables */
	Tcl_InitHashTable( &name_tbl, TCL_STRING_KEYS ); /* MUVES Component name to index table */
	Tcl_InitHashTable( &ident_tbl, TCL_ONE_WORD_KEYS ); /* ident to MUVES_Component index table */
	Tcl_InitHashTable( &air_tbl, TCL_ONE_WORD_KEYS ); /* aircode to MUVES_Componnet index table */
	hash_table_exists = 1;

	/* visit each rt_i */
	for( i=0 ; i<rts_geometry[sessionid]->rts_number_of_rtis ; i++ ) {
		struct rtserver_rti *rts_rtip=rts_geometry[sessionid]->rts_rtis[i];
		struct rt_i *rtip=rts_rtip->rtrti_rtip;

		/* visit each region in this rt_i */
		for( j=0 ; j<rtip->nregions ; j++ ) {
			struct region *rp=rtip->Regions[j];
			struct bu_mro *attrs=rp->attr_values[0];
			int new;
			int index=0;

			if( !rp || BU_MRO_STRLEN(attrs) < 1 ) {
				/* not a region, or does not have a MUVES_Component attribute */
				continue;
			}

			/* create an entry for this MUVES_Component name */
			name_entry = Tcl_CreateHashEntry( &name_tbl, BU_MRO_GETSTRING( attrs ), &new );
			if( verbose ) {
				fprintf( stderr, "region %s, name = %s\n",
					 rp->reg_name, BU_MRO_GETSTRING( attrs ) );
			}
			/* set value to next index */
			if( new ) {
				comp_count++;
				Tcl_SetHashValue( name_entry, (ClientData)comp_count );
				index = comp_count;
			} else {
				index = (int )Tcl_GetHashValue( name_entry );
			}

			if( rp->reg_aircode > 0 ) {
				/* this is an air region, create an air table entry */
				air_entry = Tcl_CreateHashEntry( &air_tbl, (char *)rp->reg_aircode, &new );
				if( new ) {
					Tcl_SetHashValue( air_entry, (ClientData)index );
				}
			} else {
				/* this is a solid region, create an ident table entry */
				ident_entry = Tcl_CreateHashEntry( &ident_tbl, (char *)rp->reg_regionid, &new );
				if( new ) {
					Tcl_SetHashValue( ident_entry, (ClientData)index );
				}
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
	while( name_entry ) {
		char *name;
		int index;

		name = Tcl_GetHashKey( &name_tbl, name_entry );
		index = (int)Tcl_GetHashValue( name_entry );
		names[index] = bu_strdup( name );

		name_entry = Tcl_NextHashEntry( &search );
	}
}


/* shutdown the server */
void
rts_shutdown()
{
	int i;
	struct db_i *dbip;

	dbip = rts_geometry[0]->rts_rtis[0]->rtrti_rtip->rti_dbip;

	/* send a shutdown job to each thread */
	for( i=0 ; i<num_threads ; i++ ) {
		struct rtserver_job *ajob;

		RTS_GET_RTSERVER_JOB( ajob );
		ajob->exit_flag = 1;

		rts_submit_job( ajob, 0 );
	}

	/* wait for each thread to exit */
	for( i=0 ; i<num_threads ; i++ ) {
		pthread_t thread = threads[i];
		pthread_join( thread, NULL );
	}

	/* clean up */
	bu_free( (char *)threads, "threads" );
	threads = NULL;

	for( i=0 ; i<num_geometries ; i++ ) {
		rts_clean( i );
	}

	if( dbip ) {
		db_close( dbip );
	}
	num_geometries = 0;
	bu_free( (char *)rts_geometry, "rts_geometry" );
	rts_geometry = NULL;

	if( hash_table_exists ) {
		Tcl_DeleteHashTable( &name_tbl );
		Tcl_DeleteHashTable( &ident_tbl);
		Tcl_DeleteHashTable( &air_tbl );
		hash_table_exists = 0;
	}

	/* free resources */
	while( BU_LIST_NON_EMPTY( &rts_resource.rtserver_results ) ) {
		struct rtserver_result *p;
		p = (struct rtserver_result *)BU_LIST_FIRST( rtserver_result, &rts_resource.rtserver_results );
		BU_LIST_DEQUEUE( &p->l );
		bu_free( (char *)p, "rtserver_result" );
	}

	while( BU_LIST_NON_EMPTY( &rts_resource.ray_results ) ) {
		struct ray_result *p;
		p = (struct ray_result *)BU_LIST_FIRST( ray_result, &rts_resource.ray_results );
		BU_LIST_DEQUEUE( &p->l );
		bu_free( (char *)p, "ray_result" );
	}

	while( BU_LIST_NON_EMPTY( &rts_resource.ray_hits ) ) {
		struct ray_hit *p;
		p = (struct ray_hit *)BU_LIST_FIRST( ray_hit, &rts_resource.ray_hits );
		BU_LIST_DEQUEUE( &p->l );
		bu_free( (char *)p, "ray_hit" );
	}

	while( BU_LIST_NON_EMPTY( &rts_resource.rtserver_jobs ) ) {
		struct rtserver_job *p;
		p = (struct rtserver_job *)BU_LIST_FIRST( rtserver_job, &rts_resource.rtserver_jobs );
		BU_LIST_DEQUEUE( &p->l );
		bu_free( (char *)p, "rtserver_job" );
	}

	for( i=0 ; i<BU_PTBL_LEN( &rts_resource.xrays ) ; i++ ) {
		struct xray *p = (struct xray *)BU_PTBL_GET( &rts_resource.xrays, i );
		bu_free( (char *)p, "xray" );
	}
	bu_ptbl_free( &rts_resource.xrays );

	needs_initialization = 1;
}

/*				b u i l d _ J a v a _ R a y R e s u l t
 *
 *	Routine to build a JAVA RayResult object from an rtserver_result.
 *
 * inputs:
 *	JNIEnv *env - The JAVA env object (must come from a JNI call)
 *	struct rtserver_result *aresult - The result structure produced by the rtserver
 *	jobject jstart_pt - A JAVA "point" object (the ray start point)
 *	jobject jdir - A JAVA "direction" object (the ray direction)
 *	jclass point_class - A JAVA class (point)
 *	jclass vect_class - A JAVA class (point)
 *
 * returns:
 *	A JAVA RayResult object containing the results from the rtserver_result structure or
 *	NULL - something went wrong
 */
jobject
build_Java_RayResult( JNIEnv *env, struct rtserver_result *aresult, jobject jstart_pt, jobject jdir, jclass point_class, jclass vect_class )
{
	jclass ray_class, rayResult_class, partition_class;
	jmethodID ray_constructor_id, rayResult_constructor_id, point_constructor_id, partition_constructor_id, add_partition_id;
	jobject jrayResult, jray, jpartition, jinhitPoint, jouthitPoint;
	struct ray_result *ray_res;
	struct ray_hit *ahit;

	/* get the JAVA Ray class */
	if( (ray_class=(*env)->FindClass( env, "mil/army/arl/muves/math/Ray" )) == NULL ) {
		fprintf( stderr, "Failed to find Ray class\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get the JAVA method id for the Ray class constructor */
	if( (ray_constructor_id=(*env)->GetMethodID( env, ray_class, "<init>",
	    "(Lmil/army/arl/muves/math/Point;Lmil/army/arl/muves/math/Vector3;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for ray constructor\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* create a JAVA ray object from the passed in ray start point and direction */
	jray = (*env)->NewObject( env, ray_class, ray_constructor_id, jstart_pt, jdir );

	/* check for any exceptions */
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown creating a ray\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get the JAVA RayResult class */
	if( (rayResult_class=(*env)->FindClass( env,
	    "mil/army/arl/muves/rtserver/RayResult" )) == NULL ) {
		fprintf( stderr, "Failed to get class for RayResult\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get the JAVA method id for the RayResult constructor */
	if( (rayResult_constructor_id=(*env)->GetMethodID( env, rayResult_class, "<init>",
					   "(Lmil/army/arl/muves/math/Ray;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for rayResult constructor\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* create a RayResult object using the newly created Ray */
	jrayResult = (*env)->NewObject( env, rayResult_class, rayResult_constructor_id, jray );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while creating a rayResult\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* If we have no hits, we are done, just return the empty RayResult object */
	if( !aresult->got_some_hits ) {
		RTS_FREE_RTSERVER_RESULT( aresult );
		return( jrayResult );
	}

	/* Get the JAVA methodid for the Point constructor */
	if( (point_constructor_id=(*env)->GetMethodID( env, point_class, "<init>",
							   "(DDD)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for Point constructor\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* Get the JAVA class for Partition */
	if( (partition_class=(*env)->FindClass( env,
	    "mil/army/arl/muves/rtserver/Partition" )) == NULL ) {
		fprintf( stderr, "Failed to get class for Partition\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* Get the JAVA constructor for a Partition */
	if( (partition_constructor_id=(*env)->GetMethodID( env, partition_class, "<init>",
	     "(DFFLmil/army/arl/muves/math/Point;Lmil/army/arl/muves/math/Point;Ljava/lang/String;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for Partition constructor\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* Get the JAVA method id for the RayResult's "addPartition" method */
	if( (add_partition_id=(*env)->GetMethodID( env, rayResult_class, "addPartition",
				"(Lmil/army/arl/muves/rtserver/Partition;)Z" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for rayResult addPartition method\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* loop through all the hits in the results, and add them to the JAVA RayResult object */
	ray_res = BU_LIST_FIRST( ray_result, &aresult->resultHead.l );
	for( BU_LIST_FOR( ahit, ray_hit, &ray_res->hitHead.l ) ) {
		jdouble in_hit[3], out_hit[3];
		jfloat inObl, outObl;
		jstring regionName;
		vect_t reverse_ray_dir;

		/* get reverse ray direction for obliquity calculation */
		VREVERSE( reverse_ray_dir, ray_res->the_ray->r_dir );

		/* calculate the entrance and exit hit coordinates */
		VJOIN1( in_hit, ray_res->the_ray->r_pt, ahit->hit_dist, ray_res->the_ray->r_dir );
		VJOIN1( out_hit, ray_res->the_ray->r_pt, ahit->hit_dist + ahit->los, ray_res->the_ray->r_dir );

		/* Create an entrance hit point (JAVA Point) */
		jinhitPoint = (*env)->NewObject( env, point_class, point_constructor_id,
						 in_hit[X], in_hit[Y], in_hit[Z] );

		/* check for any exceptions */
		if( (*env)->ExceptionOccurred(env) ) {
			fprintf( stderr, "Exception thrown while creating inhit point\n" );
			(*env)->ExceptionDescribe(env);
			return( (jobject)NULL );
		}

		/* Create an exit hit point (JAVA Point) */
		jouthitPoint = (*env)->NewObject( env, point_class, point_constructor_id,
						 out_hit[X], out_hit[Y], out_hit[Z] );

		/* check for any exceptions */
		if( (*env)->ExceptionOccurred(env) ) {
			fprintf( stderr, "Exception thrown while creating outhit point\n" );
			(*env)->ExceptionDescribe(env);
			return( (jobject)NULL );
		}

		/* calculate the entrance and exit obliquities */
		inObl = acos( VDOT( reverse_ray_dir, ahit->enter_normal ) );
		if( inObl < 0.0 ) {
			inObl = -inObl;
		}
		if( inObl > M_PI_2 ) {
			inObl = M_PI_2;
		}

		outObl = acos( VDOT( ray_res->the_ray->r_dir, ahit->exit_normal ) );
		if( outObl < 0.0 ) {
			outObl = -outObl;
		}
		if( outObl > M_PI_2 ) {
			outObl = M_PI_2;
		}

		/* Create a JAVA String version of the hit region name from ahit->regp */
		regionName = (*env)->NewStringUTF(env, ahit->regp->reg_name );

		/* check for any exceptions */
		if( (*env)->ExceptionOccurred(env) ) {
			fprintf( stderr, "Exception thrown while getting x coord of ray start point\n" );
			(*env)->ExceptionDescribe(env);
			return( (jobject)NULL );
		}

		/* Create a JAVA Partition with all the needed info */
		jpartition = (*env)->NewObject( env, partition_class, partition_constructor_id,
						ahit->los, inObl, outObl,
						jinhitPoint, jouthitPoint,
						regionName );

		/* check for any exceptions */
		if( (*env)->ExceptionOccurred(env) ) {
			fprintf( stderr, "Exception thrown while creating a partition\n" );
			(*env)->ExceptionDescribe(env);
			return( (jobject)NULL );
		}

		/* add this partition to the linked list of partitions */
		if( (*env)->CallBooleanMethod( env, jrayResult, add_partition_id, jpartition ) != JNI_TRUE ) {
			fprintf( stderr, "Failed to add a partition to rayResult!!!\n" );
			(*env)->ExceptionDescribe(env);
			return( (jobject)NULL );
		}
	}

	/* return the RayResult object */
	return( jrayResult );
}

void
fillItemMembers( jobject node,
		 struct db_i *dbip,
		 JNIEnv *env,
		 union tree *tp,
		 jclass itemTree_class,
		 jmethodID itemTree_constructor_id,
		 jmethodID itemTree_addcomponent_id,
		 jmethodID itemTree_setMuvesName_id )
{
	switch( tp->tr_op ) {
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
				 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		fillItemMembers( node, dbip, env, tp->tr_b.tb_right, itemTree_class,
				 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		break;
	case OP_SUBTRACT:
		fillItemMembers( node, dbip, env, tp->tr_b.tb_left, itemTree_class,
				 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		break;
	case OP_DB_LEAF:
		fillItemTree( node, dbip, env, tp->tr_l.tl_name, itemTree_class,
				 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		break;
	}
}

void
fillItemTree( jobject parent_node,
	      struct db_i *dbip,
	      JNIEnv *env,
	      char *name,
	      jclass itemTree_class,
	      jmethodID itemTree_constructor_id,
	      jmethodID itemTree_addcomponent_id,
	      jmethodID itemTree_setMuvesName_id )
{
	struct directory *dp;
	int id;
	struct rt_db_internal intern;
	jstring nodeName;
	jobject node;

	if( (dp=db_lookup( dbip, name, LOOKUP_QUIET )) == DIR_NULL ) {
		return;
	}

	/* create an ItemTree node for this object */
	nodeName = (*env)->NewStringUTF( env, name );
	node = (*env)->NewObject( env, itemTree_class, itemTree_constructor_id, nodeName );

	/* check for any exceptions */
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while creating ItemTree node\n" );
		(*env)->ExceptionDescribe(env);
		return;
	}

	/* add this node to parent */
	(*env)->CallVoidMethod( env, parent_node, itemTree_addcomponent_id, node );

	/* check for any exceptions */
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while adding an ItemTree node\n" );
		(*env)->ExceptionDescribe(env);
		return;
	}


	if( dp->d_flags & DIR_REGION ) {
		/* do not recurse into regions */
		return;
	}

	if( dp->d_flags & DIR_COMB ) {
		/* recurse into this combination */
		struct rt_comb_internal *comb;
		const char *muvesName;

		if( (id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource ) ) < 0 ) {
			fprintf( stderr, "Failed to get internal form of BRL-CAD object (%s)\n", name );
			return;
		}

		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CHECK_COMB( comb );

		/* check for MUVES_Component */
		if( (muvesName=bu_avs_get( &intern.idb_avs, "MUVES_Component" )) != NULL ) {
			/* add this attribute */
			jstring jmuvesName;

			jmuvesName = (*env)->NewStringUTF( env, muvesName );
			(*env)->CallVoidMethod( env, node, itemTree_setMuvesName_id, jmuvesName );

			/* check for any exceptions */
			if( (*env)->ExceptionOccurred(env) ) {
				fprintf( stderr, "Exception thrown while setting the ItemTree MuvesName\n" );
				(*env)->ExceptionDescribe(env);
				return;
			}
		}


		/* add members of this combination to the ItemTree */
		fillItemMembers( node, dbip, env, comb->tree, itemTree_class,
				 itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		rt_db_free_internal( &intern, &rt_uniresource );
	}
}


/* JAVA JNI bindings */

JNIEXPORT jobject JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_getItemTree(JNIEnv *env, jobject obj, jint sessionId )
{
	jclass itemTree_class;
	jmethodID itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id;
	jobject rootNode;
	jstring nodeName;
	struct db_i *dbip;
	struct rtserver_geometry *rtsg;
	int i;

	if( sessionId < 0 || sessionId >= num_geometries ) {
		fprintf( stderr, "Called getItemTree with invalid sessionId\n" );
		return( (jobject)NULL );
	}

	/* get the JAVA ItemTree class */
	if( (itemTree_class=(*env)->FindClass( env, "mil/army/arl/muves/rtserver/ItemTree" )) == NULL ) {
		fprintf( stderr, "Failed to find ItemTree class\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get the JAVA method id for the ItemTree class constructor */
	if( (itemTree_constructor_id=(*env)->GetMethodID( env, itemTree_class, "<init>",
	    "(Ljava/lang/String;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for ItemTree constructor\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}
	
	/* get the JAVA method id for the ItemTree addSubcomponent method */
	if( (itemTree_addcomponent_id=(*env)->GetMethodID( env, itemTree_class, "addSubComponent",
	    "(Lmil/army/arl/muves/rtserver/ItemTree;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for ItemTree addSubComponent method\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get the JAVA method id for the ItemTree setMuvesComponentName method */
	if( (itemTree_setMuvesName_id=(*env)->GetMethodID( env, itemTree_class, "setMuvesComponentName",
	     "(Ljava/lang/String;)V" )) == NULL ) {
		fprintf( stderr, "Failed to get method id for ItemTree setMuvesComponentName method\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}
	
	/* create root node for ItemTree return */
	nodeName = (*env)->NewStringUTF(env, "root" );
	rootNode = (*env)->NewObject( env, itemTree_class, itemTree_constructor_id, nodeName );

	/* check for any exceptions */
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while creating the ItemTree root node\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* traverse the model trees for this sessionid */
	rtsg = rts_geometry[sessionId];
	dbip = rtsg->rts_rtis[0]->rtrti_rtip->rti_dbip;
	for( i=0 ; i<rtsg->rts_number_of_rtis ; i++ ) {
		struct rtserver_rti *rts_rti=rtsg->rts_rtis[i];
		int j;

		for( j=0 ; j<rts_rti->rtrti_num_trees ; j++ ) {
			fillItemTree( rootNode, dbip, env, rts_rti->rtrti_trees[j], itemTree_class,
				      itemTree_constructor_id, itemTree_addcomponent_id, itemTree_setMuvesName_id );
		}

	}


	return( rootNode );			      
}

/*				R t S e r v e r I m p l _ r t s I n i t
 *
 *	Implements the "rtsInit" method called by the RtServerImpl constructor
 *
 * inputs:
 *	JNIENV *env - Environment object passed in by the JNI structure
 *	jobject obj - JAVA object ("this")
 *	jobjectArray args - Array of args passed in by the call from the constructor
 *
 * return:
 *	JNI_FALSE - all is well
 *	JNI_TRUE - something went wrong
 */
JNIEXPORT jint JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_rtsInit(JNIEnv *env, jobject obj, jobjectArray args) 
{
	jsize len=(*env)->GetArrayLength(env, args);
	jstring jfile_name, *jobj_name;
	char *file_name;
	char **obj_list;
	int num_objects=(len - 3);
	jint ret=0;
	int i;

	rts_init();

	if( len < 4 ) {
		return( (jint) 1 );
	}

	/* get the aruments from the JAVA args object array */
	jfile_name = (jstring)(*env)->GetObjectArrayElement( env, args, 2 );
	file_name = (char *)(*env)->GetStringUTFChars(env, jfile_name, 0);

	obj_list = (char **)bu_calloc( num_objects, sizeof( char *), "obj_list" );
	jobj_name = (jstring *)bu_calloc( num_objects, sizeof( jstring ), "jobj_name" );
	for( i=0 ; i<num_objects ; i++ ) {
		jobj_name[i] = (jstring)(*env)->GetObjectArrayElement( env, args, i+3 );
		obj_list[i] = (char *)(*env)->GetStringUTFChars(env, jobj_name[i], 0);
	}

	/* load the geometry */
	if( rts_load_geometry( file_name, 0, num_objects, obj_list ) < 0 ) {
		ret = 2;
	} else {
		/* get number of queues specified by command line */
		jstring jobj=(jstring)(*env)->GetObjectArrayElement( env, args, 0 );
		char *str=(char *)(*env)->GetStringUTFChars(env, jobj, 0);
		num_queues = atoi( str );
		(*env)->ReleaseStringChars( env, jobj, (const jchar *)str);

		/* do not use less than two queues */
		if( num_queues < 2 ) {
			num_queues = 2;
		}

		/* get number of threads from comamnd line */
		jobj=(jstring)(*env)->GetObjectArrayElement( env, args, 1 );
		str=(char *)(*env)->GetStringUTFChars(env, jobj, 0);
		num_threads = atoi( str );
		(*env)->ReleaseStringChars( env, jobj, (const jchar *)str);

		/* do not use less than one thread */
		if( num_threads < 1 ) {
			num_threads = 1;
		}

		/* start raytrace threads */
		rts_start_server_threads();
	}

	/* release the JAVA String objects that we created */
	(*env)->ReleaseStringChars( env, jfile_name, (const jchar *)file_name);
	for( i=0 ; i<num_objects ; i++ ) {
		(*env)->ReleaseStringChars( env, jobj_name[i], (const jchar *)obj_list[i]);
	}

	bu_free( (char *)obj_list, "obj_list" );
	bu_free( (char *)jobj_name, "jobj_name" );

	return( ret );

}


/* JAVA openSession method */
JNIEXPORT jint JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_openSession(JNIEnv *env, jobject jobj)
{
	return( (jint)rts_open_session() );
}

/* JAVA closeSession method */
JNIEXPORT void JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_closeSession(JNIEnv *env, jobject jobj,
							   jint sessionId)
{
	rts_close_session( (int)sessionId );
}

/* JAVA getDbTitle method */
JNIEXPORT jstring JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_getDbTitle(JNIEnv *env, jobject jobj )
{
	return( (*env)->NewStringUTF(env, title) );
}

/* JAVA shootRay method */
JNIEXPORT jobject JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_shootRay( JNIEnv *env, jobject jobj,
	jobject jstart_pt, jobject jdir, jint sessionId )
{
	jclass point_class, vect_class;
	jfieldID fid;
	jobject jrayResult;
	struct rtserver_job *ajob;
	struct xray *aray;
	struct rtserver_result *aresult;

	/* get a ray structure */
	RTS_GET_XRAY( aray );
	aray->index = 1;

	/* extract start point */
	if( (point_class = (*env)->GetObjectClass( env, jstart_pt ) ) == NULL ) {
		fprintf( stderr, "Failed to find Point class\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, point_class, "x", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting x-fid of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_pt[X] = (jdouble)(*env)->GetDoubleField( env, jstart_pt, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting x coord of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, point_class, "y", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting y-fid of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_pt[Y] = (*env)->GetDoubleField( env, jstart_pt, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting y coord of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, point_class, "z", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting z-fid of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_pt[Z] = (*env)->GetDoubleField( env, jstart_pt, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting z coord of ray start point\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* extract direction vector */
	if( (vect_class = (*env)->GetObjectClass( env, jdir ) ) == NULL ) {
		fprintf( stderr, "Failed to find Vector3 class\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, vect_class, "x", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting x-fid of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_dir[X] = (*env)->GetDoubleField( env, jdir, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting x coord of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, vect_class, "y", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting y-fid of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_dir[Y] = (*env)->GetDoubleField( env, jdir, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting y coord of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	fid = (*env)->GetFieldID( env, vect_class, "z", "D" );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting z-fid of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	aray->r_dir[Z] = (*env)->GetDoubleField( env, jdir, fid );
	if( (*env)->ExceptionOccurred(env) ) {
		fprintf( stderr, "Exception thrown while getting z coord of ray direction\n" );
		(*env)->ExceptionDescribe(env);
		return( (jobject)NULL );
	}

	/* get a job structure */
	RTS_GET_RTSERVER_JOB( ajob );
	ajob->rtjob_id = 1;
	ajob->sessionid = sessionId;

	/* add the requested ray to this job */
	RTS_ADD_RAY_TO_JOB( ajob, aray );

	/* run this job */
	aresult = rts_submit_job_and_wait( ajob );

	/* build result to return */
	jrayResult = build_Java_RayResult( env, aresult, jstart_pt, jdir, point_class, vect_class );

	RTS_FREE_RTSERVER_RESULT( aresult );

	/* return JAVA result */
	return( jrayResult );
}

JNIEXPORT void JNICALL
Java_mil_army_arl_muves_rtserver_RtServerImpl_shutdownNative(JNIEnv *env, jobject obj )
{
	rts_shutdown();
}

#ifdef TESTING
/* usage statement */
static char *usage="Usage:\n\t%s [-n num_cpus] [-t num_threads] [-q num_queues] [-a] [-s grid_size] [-v] [-o object] model.g\n";

int
main( int argc, char *argv[] )
{
	int ret;
	int nthreads=1;
	int c;
	extern char *optarg;
	extern int optind, opterr, optopt;
	struct rtserver_job *ajob;
	struct rtserver_result *aresult;
	struct xray *aray;
	char *name;
	int i, j;
	int grid_size = 64;
	fastf_t cell_size;
	vect_t model_size;
	vect_t xdir, zdir;
	int job_count=0;
	char **result_map;
	struct bu_ptbl objs;
	int my_session_id;

	/* Things like bu_malloc() must have these initialized for use with parallel processing */
	bu_semaphore_init( RT_SEM_LAST );

	/* initialize the list of BRL-CAD objects to be raytraced (this is used for the "-o" option) */
	bu_ptbl_init( &objs, 64, "objects" );

	/* initialize the rtserver resources (cached structures) */
	rts_resource_init();

	/* process command line args */
	while( (c=getopt( argc, argv, "vs:n:t:q:ao:" ) ) != -1 ) {
		switch( c ) {
		case 'n':	/* number of cpus to use for prepping */
			ncpus = atoi( optarg );
			break;
		case 't':	/* number of server threads to start */
			num_threads = atoi( optarg );
			break;
		case 'q':	/* number of request queues to create */
			num_queues = atoi( optarg );
			break;
		case 'a':	/* set flag to use air regions in the BRL-CAD model */
			use_air = 1;
			break;
		case 's':	/* set the grid size (default is 64x64) */
			grid_size = atoi( optarg );
			break;
		case 'v':	/* turn on verbose logging */
			verbose = 1;
			break;
		case 'o':	/* add an object name to the list of BRL-CAD objects to raytrace */
			bu_ptbl_ins( &objs, (long *)optarg );
			break;
		default:	/* ERROR */
			fprintf( stderr, usage, argv[0] );
			exit( 1 );
		}
	}
	
	/* load geometry */
	if( BU_PTBL_LEN( &objs ) > 0 ) {
		char **objects;

		objects = (char **)bu_malloc( BU_PTBL_LEN( &objs ) * sizeof( char *), "objects" );
		for( i=0 ; i<BU_PTBL_LEN( &objs ) ; i++ ) {
			objects[i] = (char *)BU_PTBL_GET( &objs, i );
		}
		my_session_id = rts_load_geometry( argv[optind], 0, BU_PTBL_LEN( &objs ), objects );
	} else {
		if( optind >= argc ) {
			fprintf( stderr, "No BRL-CAD model specified\n" );
			fprintf( stderr, usage, argv[0] );
			exit( 1 );
		}
		my_session_id = rts_load_geometry( argv[optind], 0, 0, (char **)NULL );
	}

	if( my_session_id < 0 ) {
		fprintf( stderr, "Failed to load geometry from file (%s)\n", argv[optind] );
		exit( 2 );
	}

	/* exercise the open session capability */
	my_session_id = rts_open_session();
	my_session_id = rts_open_session();
	rts_close_session( my_session_id );
	my_session_id = rts_open_session();
	if( my_session_id < 0 ) {
		fprintf( stderr, "Failed to open session\n" );
		exit( 2 );
	} else {
		fprintf( stderr, "Using session id %d\n", my_session_id );
	}
#if 0
	get_muves_components();

	if( verbose ) {
		fprintf( stderr, "MUVES Component List: (%d components)\n", comp_count );
		i = 0;
		while( names[i] ) {
			fprintf( stderr, "\t%d - %s\n", i, names[i] );
			i++;
		}	
	}
#endif
	/* start the server threads */
	rts_start_server_threads( nthreads, num_queues );

	/* submit and wait for a job */
	RTS_GET_RTSERVER_JOB( ajob );
	RTS_GET_XRAY( aray );
	RTS_ADD_RAY_TO_JOB( ajob, aray );

	VSET( aray->r_pt, -2059.812865, -6750.847220, -323.551389 );
	VSET( aray->r_dir, 0, 1, 0 );
	ajob->sessionid = my_session_id;
	aresult = rts_submit_job_and_wait( ajob );
	/* list results */
	fprintf( stderr, "shot from (%g %g %g) in direction (%g %g %g):\n",
		 V3ARGS( aray->r_pt ),
		 V3ARGS( aray->r_dir ) );
	if( !aresult->got_some_hits ) {
		fprintf( stderr, "\tMissed\n" );
	} else {
		struct ray_result *ray_res;
		struct ray_hit *ahit;

		ray_res = BU_LIST_FIRST( ray_result, &aresult->resultHead.l );
		for( BU_LIST_FOR( ahit, ray_hit, &ray_res->hitHead.l ) ) {
			fprintf( stderr, "\thit on comp %d at dist = %g los = %g\n",
				 ahit->comp_id, ahit->hit_dist, ahit->los );
		}
	}

	RTS_FREE_RTSERVER_RESULT( aresult );
	rts_pr_resource_summary();

	/* submit some jobs */
	VSET( xdir, 1, 0, 0 );
	VSET( zdir, 0, 0, 1 );
	VSUB2( model_size, rts_geometry[my_session_id]->rts_mdl_max, rts_geometry[my_session_id]->rts_mdl_min );
	cell_size = model_size[X] / grid_size;
	for( i=0 ; i<grid_size ; i++ ) {
		for( j=0 ; j<grid_size ; j++ ) {
			RTS_GET_RTSERVER_JOB( ajob );
			ajob->rtjob_id = (grid_size - i - 1)*1000 + j;
			ajob->sessionid = my_session_id;
			RTS_GET_XRAY( aray );
			VJOIN2( aray->r_pt,
				rts_geometry[my_session_id]->rts_mdl_min,
				i*cell_size,
				zdir,
				j*cell_size,
				xdir );
			aray->index = ajob->rtjob_id;
			VSET( aray->r_dir, 0, 1, 0 );

			RTS_ADD_RAY_TO_JOB( ajob, aray );

			rts_submit_job( ajob, j%num_queues );
			job_count++;
		}
	}


	result_map = (char **)bu_calloc( grid_size, sizeof( char *), "result_map" );
	for( i=0 ; i<grid_size ; i++ ) {
		result_map[i] = (char *)bu_calloc( (grid_size+1), sizeof( char ), "result_map[i]" );
	}
	/* get all the results */
	while( job_count ) {
		aresult = rts_get_any_waiting_result( my_session_id );
		if( aresult ) {
			i = aresult->the_job->rtjob_id/1000;
			j = aresult->the_job->rtjob_id%1000;
			if( aresult->got_some_hits ) {
				/* count hits */
				struct ray_hit *ahit;
				struct ray_result *ray_res;
				int hit_count=0;

				ray_res = BU_LIST_FIRST( ray_result, &aresult->resultHead.l );
				for( BU_LIST_FOR( ahit, ray_hit, &ray_res->hitHead.l ) ) {
					hit_count++;
				}
				if( hit_count <= 9 ) {
					result_map[i][j] = '0' + hit_count;
				} else {
					result_map[i][j] = '*';
				}
			} else {
				result_map[i][j] = ' ';
			}
			job_count--;

			RTS_FREE_RTSERVER_RESULT( aresult );
		}
	}

	for( i=0 ; i<grid_size ; i++ ) {
		fprintf( stderr,"%s\n", result_map[i] );
	}


	rts_pr_resource_summary();

	return 0;
}
#endif
