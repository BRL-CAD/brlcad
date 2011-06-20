/*                  R T S E R V E R T E S T . C
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
/** @file librtserver/rtserverTest.c
 *
 * library for BRL-CAD raytrace server
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "rtserver.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

/* number of seconds to wait for geometry to load */
#define SLEEPYTIME 5

#define RTS_ADD_RAY_TO_JOB( _ajob, _aray ) bu_ptbl_ins( &(_ajob)->rtjob_rays, (long *)(_aray) )

/* usage statement */
static char *usage="Usage:\n\t%s [-a] [-s grid_size] [-p] [-v] [-o object] model.g\n";

int
loadGeometry( char *fileName, struct bu_ptbl *objs) {
    int sess_id = -1;
    
    if ( BU_PTBL_LEN( objs ) > 0 ) {
	char **objects;
        int i;

	objects = (char **)bu_malloc( BU_PTBL_LEN( objs ) * sizeof( char *), "objects" );
	for ( i=0; i<(int)BU_PTBL_LEN( objs ); i++ ) {
	    objects[i] = (char *)BU_PTBL_GET( objs, i );
	}
	sess_id = rts_load_geometry(fileName, BU_PTBL_LEN( objs ), objects);
        bu_free( objects, "objects");
    } else {
        fprintf( stderr, "No BRL-CAD model objects specified\n" );
        bu_exit(1, usage, "rtserverTest");
    }
    return sess_id;
}

int
countHits(struct bu_vlb *vlb)
{
    unsigned char *c;
    int numRays = 0;
    int rayNum;
    int hitCount = 0;
    
    c = bu_vlb_addr(vlb);
    numRays = BU_GLONG(c);
    
    c += SIZEOF_NETWORK_LONG;
    
    for(rayNum=0 ; rayNum<numRays ; rayNum++) {
        int numPartitions = 0;
        int partNo;
        
        numPartitions = BU_GLONG(c);
        c += SIZEOF_NETWORK_LONG;
        
        for(partNo=0 ; partNo<numPartitions ; partNo++) {
            point_t enterPt;
            point_t exitPt;
            vect_t enterNorm;
            vect_t exitNorm;
            double inObl;
            double outObl;
            int regionIndex;
            
            ntohd((unsigned char *)enterPt, c, 3);
            c += SIZEOF_NETWORK_DOUBLE * 3;
            
            ntohd((unsigned char *)exitPt, c, 3);
            c += SIZEOF_NETWORK_DOUBLE * 3;
            
            ntohd((unsigned char *)enterNorm, c, 3);
            c += SIZEOF_NETWORK_DOUBLE * 3;
            
            ntohd((unsigned char *)exitNorm, c, 3);
            c += SIZEOF_NETWORK_DOUBLE * 3;
            
            ntohd((unsigned char*)&inObl, c, 1);
            c += SIZEOF_NETWORK_DOUBLE;
            
            ntohd((unsigned char*)&outObl, c, 1);
            c += SIZEOF_NETWORK_DOUBLE;
            
            regionIndex = BU_GLONG(c);
            c += SIZEOF_NETWORK_LONG;
            
            hitCount++;
        }
    }
    return hitCount;
}

int
main( int argc, char *argv[] )
{
    struct application *ap;
    int c;
    int verbose = 0;
    int i, j;
    int grid_size = 64;
    fastf_t cell_size;
    vect_t model_size;
    vect_t xdir, zdir;
    int job_count=0;
    char **result_map = NULL;
    struct bu_ptbl objs;
    int my_session_id;
    int do_plot=0;
    int use_air=0;
    struct timeval startTime;
    struct timeval endTime;
    double diff;
    point_t mdl_min;
    point_t mdl_max;
    struct bu_vlb *vlb;
    
    /* Things like bu_malloc() must have these initialized for use with parallel processing */
    bu_semaphore_init( RT_SEM_LAST );

    /* initialize the list of BRL-CAD objects to be raytraced (this is used for the "-o" option) */
    bu_ptbl_init( &objs, 64, "objects" );

    /* process command line args */
    while ( (c=bu_getopt( argc, argv, "vps:ao:" ) ) != -1 ) {
	switch ( c ) {
	    case 'p': /* do print plot */
		do_plot = 1;
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
    
    if (bu_debug & BU_DEBUG_MEM_CHECK) {
        bu_prmem("initial memory map");
        bu_mem_barriercheck();
    }


    /* shoot a ray ten times, cleaning and loading geometry each time */
    for(i=0 ; i<10 ; i++) {
        /* load geometry */
        my_session_id = loadGeometry( argv[bu_optind], &objs );


        if ( my_session_id < 0 ) {
            bu_exit(2, "Failed to load geometry from file (%s)\n", argv[bu_optind] );
        }

        get_model_extents( my_session_id, mdl_min, mdl_max );
        VSET( xdir, 1, 0, 0 );
        VSET( zdir, 0, 0, 1 );
        VSUB2( model_size, mdl_max, mdl_min );
        
        ap = NULL;
        getApplication(&ap);
        VJOIN2( ap->a_ray.r_pt, mdl_min,
                model_size[Z]/2.0, zdir,
                model_size[X]/2.0, xdir );
        VSET( ap->a_ray.r_dir, 0, 1, 0 );
        rts_shootray(ap);

        vlb = (struct bu_vlb*)ap->a_uptr;
        printHits(vlb);

        freeApplication(ap);
        /*rts_clean( my_session_id );*/
        bu_log( "\n\n********* %d\n", i);
        if (bu_debug & BU_DEBUG_MEM_CHECK) {
            bu_prmem("memory after shutdown");
        }
    }

    my_session_id = loadGeometry( argv[bu_optind], &objs );
    
    /* submit some jobs */
    fprintf( stderr, "\nfiring a grid (%dx%d) of rays at",
	     grid_size, grid_size );
    for ( i=0; i<(int)BU_PTBL_LEN( &objs ); i++ ) {
	fprintf( stderr, " %s", (char *)BU_PTBL_GET( &objs, i ) );
    }
    fprintf( stderr, "...\n" );

    if( do_plot ) {
	result_map = (char **)bu_calloc( grid_size, sizeof( char *), "result_map" );
	for ( i=0; i<grid_size; i++ ) {
	    result_map[i] = (char *)bu_calloc( (grid_size+1), sizeof( char ), "result_map[i]" );
	}
    }

    cell_size = model_size[X] / grid_size;
    gettimeofday( &startTime, NULL );
    for ( i=0; i<grid_size; i++ ) {
	if( verbose ) {
	    fprintf( stderr, "shooting row %d\n", i );
	}
	for ( j=0; j<grid_size; j++ ) {
            int hitCount;
            
            getApplication(&ap);
	    ap->a_user = my_session_id;
	    VJOIN2( ap->a_ray.r_pt,
		    mdl_min,
		    i*cell_size,
		    zdir,
		    j*cell_size,
		    xdir );
	    ap->a_ray.index = ap->a_user;
	    VSET( ap->a_ray.r_dir, 0, 1, 0 );
            rts_shootray(ap);
            if( do_plot ) {
                hitCount = countHits(ap->a_uptr);
                if ( hitCount == 0 ) {
                    result_map[i][j] = ' ';
                } else if ( hitCount <= 9 ) {
                    result_map[i][j] = '0' + hitCount;
                } else {
                    result_map[i][j] = '*';
                }
            }
            freeApplication(ap);
	    job_count++;
	}
    }
    gettimeofday( &endTime, NULL );
    diff = endTime.tv_sec - startTime.tv_sec + (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
    fprintf( stderr, "time for %d individual rays: %g second\n", job_count, diff );

    if(do_plot) {
	for ( i=grid_size-1; i>=0; i-- ) {
	    fprintf( stderr, "%s\n", result_map[i] );
	}
    }
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
