/*                  R T S E R V E R T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
 *
 * library for BRL-CAD raytrace server
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "rtserver.h"

#include <sys/time.h>
#include <time.h>

/* number of seconds to wait for geometry to load */
#define SLEEPYTIME 5

#define RTS_ADD_RAY_TO_JOB( _ajob, _aray ) bu_ptbl_ins( &(_ajob)->rtjob_rays, (long *)(_aray) )

/* usage statement */
static char *usage="Usage:\n\t%s [-t num_threads] [-q num_queues] [-a] [-s grid_size] [-p] [-v] [-o object] model.g\n";


int
main( int argc, char *argv[] )
{
    int ret;
    int c;
    extern char *bu_optarg;
    extern int bu_optind, bu_opterr, optopt;
    struct rtserver_job *ajob;
    struct rtserver_result *aresult;
    struct xray *aray;
    int verbose = 0;
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
    int queue_count=3;
    int thread_count=2;
    int do_plot=0;
    int use_air=0;
    struct timeval startTime;
    struct timeval endTime;
    double diff;
    point_t mdl_min;
    point_t mdl_max;

    /* Things like bu_malloc() must have these initialized for use with parallel processing */
    bu_semaphore_init( RT_SEM_LAST );

    /* initialize the list of BRL-CAD objects to be raytraced (this is used for the "-o" option) */
    bu_ptbl_init( &objs, 64, "objects" );

    /* initialize the rtserver resources (cached structures) */
    rts_resource_init();

    /* process command line args */
    while ( (c=bu_getopt( argc, argv, "vps:t:q:ao:" ) ) != -1 ) {
	switch ( c ) {
	    case 'p': /* do print plot */
		do_plot = 1;
		break;
	    case 't':	/* number of server threads to start */
		thread_count = atoi( bu_optarg );
		break;
	    case 'q':	/* number of request queues to create */
		queue_count = atoi( bu_optarg );
		break;
	    case 'a':	/* set flag to use air regions in the BRL-CAD model */
		use_air = 1;
		break;
	    case 's':	/* set the grid size (default is 64x64) */
		grid_size = atoi( bu_optarg );
		break;
	    case 'v':	/* turn on verbose logging */
		verbose = 1;
		rts_set_verbosity( 1 );
		break;
	    case 'o':	/* add an object name to the list of BRL-CAD objects to raytrace */
		bu_ptbl_ins( &objs, (long *)bu_optarg );
		break;
	    default:	/* ERROR */
		bu_exit(1, usage, argv[0]);
	}
    }

    /* load geometry */
    if ( BU_PTBL_LEN( &objs ) > 0 ) {
	char **objects;

	objects = (char **)bu_malloc( BU_PTBL_LEN( &objs ) * sizeof( char *), "objects" );
	for ( i=0; i<BU_PTBL_LEN( &objs ); i++ ) {
	    objects[i] = (char *)BU_PTBL_GET( &objs, i );
	}
	my_session_id = rts_load_geometry( argv[bu_optind], 0, BU_PTBL_LEN( &objs ), objects, thread_count );
    } else {
	if ( bu_optind >= argc ) {
	    fprintf( stderr, "No BRL-CAD model specified\n" );
	    bu_exit(1, usage, argv[0]);
	}
	my_session_id = rts_load_geometry( argv[bu_optind], 0, 0, (char **)NULL, thread_count );
    }

    if ( my_session_id < 0 ) {
	bu_exit(2, "Failed to load geometry from file (%s)\n", argv[bu_optind] );
    }

    /* exercise the open session capability */
    my_session_id = rts_open_session();
    my_session_id = rts_open_session();
    rts_close_session( my_session_id );
    my_session_id = rts_open_session();
    if ( my_session_id < 0 ) {
	bu_exit(2, "Failed to open session\n" );
    } else {
	fprintf( stderr, "Using session id %d\n", my_session_id );
    }
#if 0
    get_muves_components();

    if ( verbose ) {
	fprintf( stderr, "MUVES Component List: (%d components)\n", comp_count );
	i = 0;
	while ( names[i] ) {
	    fprintf( stderr, "\t%d - %s\n", i, names[i] );
	    i++;
	}
    }
#endif
    /* start the server threads */
    rts_start_server_threads( thread_count, queue_count );

    fprintf( stderr, "sleeping for %d seconds while geometry loads\n", SLEEPYTIME );
    sleep( SLEEPYTIME );

    get_model_extents( my_session_id, mdl_min, mdl_max );
    VSET( xdir, 1, 0, 0 );
    VSET( zdir, 0, 0, 1 );
    VSUB2( model_size, mdl_max, mdl_min );
    aray = rts_get_xray();
    VJOIN2( aray->r_pt, mdl_min,
	    model_size[Z]/2.0, zdir,
	    model_size[X]/2.0, xdir );
    VSET( aray->r_dir, 0, 1, 0 );

    /* submit and wait for a job */
    ajob = rts_get_rtserver_job();
    RTS_ADD_RAY_TO_JOB( ajob, aray );

    ajob->sessionid = my_session_id;
    if( verbose ) {
	fprintf( stderr, "submitting job and waaiting\n" );
    }
    aresult = rts_submit_job_and_wait( ajob );
    /* list results */
    fprintf( stderr, "shot from (%g %g %g) in direction (%g %g %g):\n",
	     V3ARGS( aray->r_pt ),
	     V3ARGS( aray->r_dir ) );
    if ( !aresult->got_some_hits ) {
	fprintf( stderr, "\tMissed\n" );
    } else {
	struct ray_result *ray_res;
	struct ray_hit *ahit;

	ray_res = BU_LIST_FIRST( ray_result, &aresult->resultHead.l );
	for ( BU_LIST_FOR( ahit, ray_hit, &ray_res->hitHead.l ) ) {
	    fprintf( stderr, "\thit on region %s at dist = %g los = %g\n",
		     ahit->regp->reg_name, ahit->hit_dist, ahit->los );
	}
    }

    rts_free_rtserver_result( aresult );
    fprintf( stderr, "resources after firing one ray:\n" );
    rts_pr_resource_summary();

    /* submit some jobs */
    fprintf( stderr, "\nfiring a grid (%dx%d) of rays at",
	     grid_size, grid_size );
    for ( i=0; i<BU_PTBL_LEN( &objs ); i++ ) {
	fprintf( stderr, " %s", (char *)BU_PTBL_GET( &objs, i ) );
    }
    fprintf( stderr, "...\n" );

    cell_size = model_size[X] / grid_size;
    gettimeofday( &startTime, NULL );
    for ( i=0; i<grid_size; i++ ) {
	if( verbose ) {
	    fprintf( stderr, "shooting row %d\n", i );
	}
	for ( j=0; j<grid_size; j++ ) {
	    ajob = rts_get_rtserver_job();
	    ajob->rtjob_id = (grid_size - i - 1)*grid_size*10 + j;
	    ajob->sessionid = my_session_id;
	    aray = rts_get_xray();
	    VJOIN2( aray->r_pt,
		    mdl_min,
		    i*cell_size,
		    zdir,
		    j*cell_size,
		    xdir );
	    aray->index = ajob->rtjob_id;
	    VSET( aray->r_dir, 0, 1, 0 );

	    RTS_ADD_RAY_TO_JOB( ajob, aray );
	    if( do_plot ) {
		rts_submit_job( ajob, j%queue_count );
	    } else {
		aresult = rts_submit_job_to_queue_and_wait( ajob, j%queue_count );
		rts_free_rtserver_result( aresult );
	    }
	    job_count++;

	}
    }
    gettimeofday( &endTime, NULL );
    diff = endTime.tv_sec - startTime.tv_sec + (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
    fprintf( stderr, "time for %d individual rays: %g second\n", job_count, diff );

    if( do_plot ) {
	result_map = (char **)bu_calloc( grid_size, sizeof( char *), "result_map" );
	for ( i=0; i<grid_size; i++ ) {
	    result_map[i] = (char *)bu_calloc( (grid_size+1), sizeof( char ), "result_map[i]" );
	}
	while ( job_count ) {
	    aresult = rts_get_any_waiting_result( my_session_id );
	    if ( aresult ) {
		i = aresult->the_job->rtjob_id/(grid_size*10);
		j = aresult->the_job->rtjob_id%(grid_size*10);
		if ( aresult->got_some_hits ) {
		    struct ray_hit *ahit;
		    struct ray_result *ray_res;
		    int hit_count=0;

		    ray_res = BU_LIST_FIRST( ray_result, &aresult->resultHead.l );
		    for ( BU_LIST_FOR( ahit, ray_hit, &ray_res->hitHead.l ) ) {
			hit_count++;
		    }
		    if ( hit_count <= 9 ) {
			result_map[i][j] = '0' + hit_count;
		    } else {
			result_map[i][j] = '*';
		    }
		} else {
		    result_map[i][j] = ' ';
		}
		job_count--;

		rts_free_rtserver_result( aresult );
	    }
	}


	for ( i=0; i<grid_size; i++ ) {
	    fprintf( stderr, "%s\n", result_map[i] );
	}
    }

    fprintf( stderr, "resources after firing %d rays:\n", grid_size*grid_size + 1 );
    rts_pr_resource_summary();

    return 0;
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
