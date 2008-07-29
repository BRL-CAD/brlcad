/*                      R T S E R V E R . H
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
/** @addtogroup rtserver */
/** @{ */
/** @file rtserver.h
 * @brief
 *	header file for the rtserver
 *
 *  @author: John R. Anderson
 *
 *  In order to use the rtserver, the BRL-CAD model must include an opaque binary object named "rtserver_data"
 *  This object must contain ASCII data consisting of lines of the form:
 *	assembly_name1 { object1 object2 object3 ...} key1 value1 key2 value2 ...
 *  Where assembly names are names of assemblies to be used for raytracing or articulation and
 *  the list of objects for each assembly specifies the BRL-CAD objects in that assembly. The
 *  assembly names do not need to be names of objects that already exist in the BRL-CAD model.
 *  At least one assembly named "rtserver_tops" must exist (this will be used for raytracing when
 *  no articulation is to be done. Each assembly must appear on its own line and must have at least
 *  one object in its list of objects.  If the assembly name has embedded spaces, it must be surrounded
 *  by "{" and "}". Each line is a series of key/value pairs with the first key being the assembly name.
 *  optional keys are:
 *	key_pt - value is the center of rotation for this assembly
 *	xrotate - values are the rotation limits (max min initial) about the x-axis (degrees). Default is no rotation allowed
 *	yrotate - values are the rotation limits (max min initial) about the y-axis (degrees). Default is no rotation allowed
 *	zrotate - values are the rotation limits (max min initial) about the z-axis (degrees). Default is no rotation allowed
 *	xtranslate - values are the limits (max min) of translation along the x-axis (mm). Default is no translation allowed.
 *	ytranslate - values are the limits (max min) of translation along the y-axis (mm). Default is no translation allowed.
 *	ztranslate - values are the limits (max min) of translation along the z-axis (mm). Default is no translation allowed.
 *	children - values are other assemblies that are rigidly attached to this assembly (The children will move
 *		with their parent).
 *
 *  An empty value for the translation keys implies unlimited translation is allowed.
 *
 */

#if SIZEOF_VOID_P == SIZEOF_INT
typedef int CLIENTDATA_INT;
#elif SIZEOF_VOID_P == SIZEOF_LONG
typedef long CLIENTDATA_INT;
#elif SIZEOF_VOID_P == SIZEOF_LONG_LONG
typedef long long CLIENTDATA_INT;
#else
#define CLIENTDATA_INT "ERROR: could not determine size of void*";
#endif

struct rtserver_job {
    struct bu_list l;		/* for linking */
    int exit_flag;			/* flag, non-zero means the running thread should exit */
    int sessionid;			/* index into sessions (rts_geometry array) */
    int rtjob_id;			/* identifying number, assigned by the rt server */
    int maxHits;			/* Max number of hits to consider along each ray (zero means take all of them) */
    int useByteArray;                   /* flag, non-zero means create byte array version of the results */
    struct bu_ptbl rtjob_rays;	/* list of pointers to rays to be fired */
};

struct ray_hit {
    struct bu_list l;
    struct region *regp;		/* pointer to containing region */
    int comp_id;			/* index into component list */
    fastf_t hit_dist;		/* distance along ray to hit point */
    fastf_t los;			/* line of sight distance through this component */
    vect_t enter_normal;		/* normal vector at entrance hit */
    vect_t exit_normal;		/* normal vector at exit hit */
};

struct ray_result {
    struct bu_list l;
    struct xray the_ray;		/* the originating ray */
    struct ray_hit hitHead;		/* the list of components hit along this ray */
    struct bu_vlb *vlb;                  /* pointer to the byte array in the owning rtserver_result */
};

struct rtserver_result {
    struct bu_list l;		/* for linked list */
    int got_some_hits;		/* flag 0-> no hits in results */
    struct rtserver_job *the_job;	/* the originating job */
    struct ray_result resultHead;	/* the list of results, one for each ray */
    struct bu_vlb *vlb;                  /* variable length byte array to contain the byte array
                                          * return for Java (null if rtserver_job->useByteArray == 0) */
};

struct rtserver_rti {
    struct rt_i *rtrti_rtip;	/* pointer to an rti structure */
    char *rtrti_name;		/* name of this "assembly" (bu_malloc'd storage) */
    int rtrti_num_trees;		/* number of trees in this rti structure */
    char **rtrti_trees;		/* array of pointers to tree-top names trees[num_trees] (bu_malloc'd storage) */
    matp_t rtrti_xform;		/* transformation matrix from global coords to this rt instance (NULL -> identity) */
    matp_t rtrti_inv_xform;		/* inverse of above xform (NULL -> identity) */
    Tcl_HashTable *rtrti_region_names; /* A Tcl hash table containing region names as keys and index numbers as values.
                                           * The indices are used to reference region names in the Java return byte array
                                           * rather than using the full name. */
    int region_count;                  /* number of entries in above hash table */
};

struct rtserver_geometry {
    int rts_number_of_rtis;		/* number of rtserver_rti structures */
    struct rtserver_rti **rts_rtis;	/* array of pointers to rtserver_rti
					   structures rts_rtis[rts_number_of_rtis] (bu_malloc'd storage ) */
    point_t		rts_mdl_min;	/* min corner of model bounding RPP */
    point_t		rts_mdl_max;	/* max corner of model bounding RPP */
    double		rts_radius;	/* radius of model bounding sphere */
    Tcl_HashTable	*rts_comp_names;	/* A Tcl hash table containing ident numbers as keys
						   and component names as values */
};

/* MACRO to add a ray to a job */
#define RTS_ADD_RAY_TO_JOB( _ajob, _aray ) bu_ptbl_ins( &(_ajob)->rtjob_rays, (long *)(_aray) )

extern int get_max_working_threads();

extern struct rtserver_job *rts_get_rtserver_job();

extern struct rtserver_result *rts_get_any_waiting_result( int sessionid );

extern struct rtserver_result *rts_submit_job_and_wait( struct rtserver_job *ajob );

extern struct rtserver_result *rts_submit_job_to_queue_and_wait( struct rtserver_job *ajob, int queue_count );

extern struct xray *rts_get_xray();

extern void get_model_extents( int sessionid, point_t min, point_t max );

extern void rts_free_rtserver_job( struct rtserver_job *job );

extern void rts_free_xray( struct xray *ray );

extern void rts_set_verbosity( int v );

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
