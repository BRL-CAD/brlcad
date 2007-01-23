/*                  R T S E R V E R T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file rtserverTest.c
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
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <math.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
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

#ifdef HAVE_JAVAVM_JNI_H
#  include <JavaVM/jni.h>
#elif defined(HAVE_JNI_H)
#  include <jni.h>
#else
#  error ERROR: jni.h could not be found
#endif
#include "RtServerImpl.h"

void *workerThread( void *gsize );

#define RTS_ADD_RAY_TO_JOB( _ajob, _aray ) bu_ptbl_ins( &(_ajob)->rtjob_rays, (long *)(_aray) )

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
	int i, j, k;
	int grid_size = 64;
	fastf_t cell_size;
	vect_t model_size;
	vect_t xdir, zdir;
	int job_count=0;
	char **result_map;
	struct bu_ptbl objs;
	int my_session_id;
	int queue_count=3;
	int thread_count=2;
	int verbose=0;
	point_t min, max;
	int worker_count = 1;
	pthread_t *worker_thread=NULL;

	/* Things like bu_malloc() must have these initialized for use with parallel processing */
	bu_semaphore_init( RT_SEM_LAST );

	/* initialize the list of BRL-CAD objects to be raytraced (this is used for the "-o" option) */
	bu_ptbl_init( &objs, 64, "objects" );

	/* initialize the rtserver resources (cached structures) */
	rts_resource_init();

	/* process command line args */
	while( (c=getopt( argc, argv, "w:vs:t:q:o:" ) ) != -1 ) {
		switch( c ) {
		case 'w':	/* number of worker threads */
			worker_count = atoi( optarg );
			break;
		case 't':	/* number of server threads to start */
			thread_count = atoi( optarg );
			break;
		case 'q':	/* number of request queues to create */
			queue_count = atoi( optarg );
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
		my_session_id = rts_load_geometry( argv[optind], 0, BU_PTBL_LEN( &objs ), objects, thread_count );
	} else {
		if( optind >= argc ) {
			fprintf( stderr, "No BRL-CAD model specified\n" );
			fprintf( stderr, usage, argv[0] );
			exit( 1 );
		}
		my_session_id = rts_load_geometry( argv[optind], 0, 0, (char **)NULL, thread_count );
	}

	if( my_session_id < 0 ) {
		fprintf( stderr, "Failed to load geometry from file (%s)\n", argv[optind] );
		exit( 2 );
	}

	/* exercise the open session capability */
	my_session_id = rts_open_session();
	rts_close_session( my_session_id );
	my_session_id = rts_open_session();
	if( my_session_id < 0 ) {
		fprintf( stderr, "Failed to open session\n" );
		exit( 2 );
	} else {
		fprintf( stderr, "Using session id %d\n", my_session_id );
	}

	/* start the server threads */
	rts_start_server_threads( thread_count, queue_count );

	worker_thread = (pthread_t *)realloc( worker_thread, worker_count * sizeof( pthread_t ) );
	for( k=0 ; k<worker_count ; k++ ) {
		pthread_create( &worker_thread[k], NULL, workerThread, (void *)grid_size );
	}

	for( k=0 ; k<worker_count ; k++ ) {
		pthread_join( worker_thread[k], NULL );
	}
}

void *
workerThread( void *gsize )
{
	int my_session_id;
	vect_t xdir, ydir, zdir, model_size;
	point_t min, max;
	double cell_size;
	int i, j, k;
	struct rtserver_job *ajob;
	struct xray *aray;
	struct rtserver_result *aresult;
	int grid_size = (int)gsize;

	my_session_id = rts_open_session();
	fprintf( stderr, "worker thread using sesion id = %d\n", my_session_id );
	/* submit some jobs */
	VSET( xdir, 1, 0, 0 );
	VSET( zdir, 0, 0, 1 );
	get_model_extents( my_session_id, min, max );
	VSUB2( model_size, max, min );
	cell_size = model_size[X] / grid_size;

	while( 1 ) {
	for( i=0 ; i<grid_size ; i++ ) {
		for( j=0 ; j<grid_size ; j++ ) {
			/* RTS_GET_RTSERVER_JOB( ajob ); */
			ajob = rts_get_rtserver_job();
			ajob->rtjob_id = (grid_size - i - 1)*1000 + j;
			ajob->sessionid = my_session_id;
			/* RTS_GET_XRAY( aray ); */
			aray = rts_get_xray();
			VJOIN2( aray->r_pt,
				min,
				i*cell_size,
				zdir,
				j*cell_size,
				xdir );
			aray->index = ajob->rtjob_id;
			VSET( aray->r_dir, 0, 1, 0 );

			RTS_ADD_RAY_TO_JOB( ajob, aray );

			aresult = rts_submit_job_and_wait( ajob );
			rts_free_rtserver_result( aresult );

		}
	}

	rts_pr_resource_summary();

	fprintf( stderr, "max working threads = %d\n", get_max_working_threads() );

	}

	return 0;
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
