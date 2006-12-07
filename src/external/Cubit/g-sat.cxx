/*                         G - S A T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file g-sat.cxx
 *
 *      C++ code for converting BRL-CAD models to the ACIS SAT format for importing into CUBIT.
 *      This code assumes that your receiving format can handle CSG primitives
 *      and Boolean trees with transformation matrices
 *
 *  Author -
 *	Michael J. Gillich
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
#include <stack>

#include "common.h"

// system headers
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

// CGM headers
#include "NoAcisQueryEngine.hpp"
#include "NoAcisModifyEngine.hpp"
#include "GeometryQueryTool.hpp"
#include "GeometryModifyTool.hpp"
#include "AppUtil.hpp"
#include "CGMApp.hpp"
#include "CastTo.hpp"
#include "Body.hpp"
#include "RefEntity.hpp"
#include "RefEntityName.hpp"
#include "CubitAttrib.hpp"
#include "CubitAttribManager.hpp"

// interface headers
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
//#include "../librt/debug.h"

using namespace std;

/*
extern char *optarg;
extern int optind, opterr, getopt();
*/

const int NUM_OF_CPUS_TO_USE = 1;

const int DEBUG_NAMES = 1;
const int DEBUG_STATS = 2;

const long debug = 1;
int verbose = 0;

static  db_i		*dbip;

static  rt_tess_tol     ttol;   /* tesselation tolerance in mm */
static  bn_tol          tol;    /* calculation tolerance */

static char	usage[] = "Usage: %s [-v] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";

// Global map for bodies names in the CGM global list
map<string, int> g_body_id_map;
map<string, int>::iterator g_itr;

int g_body_cnt = 0;

vector <string> g_CsgBoolExp;

// Function prototypes for access of the global map above.
void set_body_id( string body_name, int body_id );
int get_body_id( string body_name );

int region_start ( db_tree_state *tsp,  db_full_path *pathp, const  rt_comb_internal * combp, genptr_t client_data );
tree *region_end ( db_tree_state *tsp,  db_full_path *pathp, tree *curtree, genptr_t client_data );
tree *primitive_func( db_tree_state *tsp,  db_full_path *pathp, rt_db_internal *ip, genptr_t client_data);
void describe_tree( tree *tree,  bu_vls *str);
void output_triangles( nmgregion *r, model *m, shell *s);
bool make_bot( nmgregion *r, model *m, shell *s);
tree *booltree_evaluate(tree *tp, resource *resp);

string infix_to_postfix(string str);
void tokenize(const string& str, vector<string>& tokens, const string& delimiters);


/*
 *			M A I N
 */
int
main(int argc, char *argv[])
{
        struct user_data {
           int info;
        } user_data;

	int		i;
	register int	c;
        char idbuf[132];

        char *output_file = NULL;

	bu_setlinebuf( stderr );

	rt_init_resource(&rt_uniresource, 0, NULL);
/*
         rt_db_internal intern;
         directory *dp;
*/
         rt_i *rtip;
         db_tree_state init_state;


	/* calculational tolerances
	 * mostly used by NMG routines
	 */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp =1e-6;
	tol.para = 1 - tol.perp;

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "t:a:n:o:r:vx:X:")) != EOF) {
		switch (c) {
		case 't':		/* calculational tolerance */
			tol.dist = atof( optarg );
			tol.dist_sq = tol.dist * tol.dist;
		case 'o':		/* Output file name */
			/* grab output file name */
                        output_file = optarg;
			break;
		case 'v':		/* verbosity */
			verbose++;
			break;
		case 'x':		/* librt debug flag (see librt/debug.h) */
			sscanf( optarg, "%x", &rt_g.debug );
			bu_printb( "librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT );
			bu_log("\n");
			break;
		case 'X':		/* NMG debug flag (see h/nmg.h) */
			sscanf( optarg, "%x", &rt_g.NMG_debug );
			bu_printb( "librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT );
			bu_log("\n");
			break;
		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
			break;
		}
	}

	if (optind+1 >= argc) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

        // ***********************************************************************************
        // Setup Cubit Routines
        // ***********************************************************************************

        // CGM Initialization
        const char* ACIS_SAT = "ACIS_SAT";
        const char* OUTPUT_FILE = "test.sat";

        int dummy_argc = 0;
        char **dummy_argv =NULL;

        // Initialize the application
        AppUtil::instance()->startup(dummy_argc, dummy_argv);
        CGMApp::instance()->startup(dummy_argc, dummy_argv);

        CGMApp::instance()->attrib_manager()->auto_flag(true);

        // Create the geometry modify and query tool
        GeometryModifyTool *gmt = GeometryModifyTool::instance();
        GeometryQueryTool *gqt = GeometryQueryTool::instance();

        // Create the  ACIS engines
        AcisQueryEngine::instance();
        AcisModifyEngine::instance();

        // Get version number of the geometry engine.
        CubitString version = gqt->get_engine_version_string();
        cout << "ACIS Engine: " << version << endl;

        CubitStatus status;

	/* Open BRL-CAD database */
	/* Scan all the records in the database and build a directory */
        /* rtip=rt_dirbuild(argv[optind], idbuf, sizeof(idbuf)); */
        rtip=rt_dirbuild(argv[optind], idbuf, sizeof(idbuf));
        if ( rtip == RTI_NULL) {
           fprintf(stderr,"g-xxx: rt_dirbuild failure\n");
           exit(1);
        }

        init_state = rt_initial_tree_state;
        init_state.ts_dbip = rtip->rti_dbip;
        init_state.ts_rtip = rtip;
        init_state.ts_resp = NULL;
        init_state.ts_tol = &tol;
        init_state.ts_ttol = &ttol;
        bu_avs_init(&init_state.ts_attrs, 1, "avs in tree_state");
    
        /* Set up tesselation tolerance defaults */
        ttol.magic = RT_TESS_TOL_MAGIC;

        /* Defaults, updated by command line options. */
        ttol.abs = 0.0;
        ttol.rel = 0.01;
        ttol.norm = 0.0;


	optind++;

	/* Walk the trees named on the command line
	 * outputting combinations and primitives
	 */
	for( i=optind ; i<argc ; i++ )
	{
            db_walk_tree(rtip->rti_dbip, argc - i, (const char **)&argv[i], NUM_OF_CPUS_TO_USE,
                         &init_state ,region_start, region_end, primitive_func, (genptr_t) &user_data);
	}

        // *************************************************************************************
        // Write SAT file
        // *************************************************************************************

        // Make bodies list.
        DLIList<Body*> all_bodies, old_bodies, new_bodies, tools_bodies;
        Body* tool_body;

        // Get the global list
        //gqt->bodies(all_bodies);

/*
        tools_bodies.append(gqt->get_body(get_body_id("cone.s")));

        gmt->unite(all_bodies, new_bodies);
*/

/*
        cout << "*** STL MAP DEBUG BEGIN ***" << endl;
        for(g_itr = g_body_id_map.begin(); g_itr != g_body_id_map.end(); g_itr++) {
           cout << g_itr->first << '\t' << g_itr->second << endl;
        }
        cout << "*** STL MAP DEBUG END ***" << endl;
*/
 
        cout << "*** CSG DEBUG BEGIN ***" << endl;
        DLIList<Body*> all_region_bodies, region_bodies, from_bodies;
        Body* region_body;

        for (int i=0; i < g_CsgBoolExp.size(); i++) {
           cout << " R" << i << " = " << g_CsgBoolExp[i] << ": " << get_body_id(g_CsgBoolExp[i]) << endl;
           //if (i == 4) continue;

           int body_id = get_body_id(g_CsgBoolExp[i]);

           if (body_id  >= 0) {
               //region_bodies.append(gqt->get_body(get_body_id(g_CsgBoolExp[i])));
               all_region_bodies.append(gqt->get_body(body_id));
           }
           else if (body_id == -1) { // {empty}
               cout << "DEBUG: {empty}" << endl;
           }
           else {
               //tokenize
               vector <string> csgTokens;
               char csgOp;

               tokenize(g_CsgBoolExp[i],csgTokens," ");
               
               cout << "DEBUG " << csgTokens.size() << endl;
               for (int j = 0; j < csgTokens.size(); j++) {
                  cout <<"    T" << j << " = " << csgTokens[j] << ": " << get_body_id(csgTokens[j]) << endl;

                  if (get_body_id(csgTokens[j]) >= 0) {
                     region_bodies.append(gqt->get_body(get_body_id(csgTokens[j])));
                  }
                  else {
                     csgOp = csgTokens[j].at(0);
                     cout << "*DEBUG*  csgOp = " << csgOp << endl;
                     switch (csgOp) {
                        case '+':
                           cout << "*** DEBUG INTERSECT ***" << endl;
                           tool_body = region_bodies.pop();
                           from_bodies.append(region_bodies.pop());
                           gmt->intersect(tool_body, from_bodies, region_bodies, CUBIT_TRUE);
                           break;
                        case '-':
                           cout << "*** DEBUG SUBTRACT ***" << endl;
                           tool_body = region_bodies.pop();
                           from_bodies.append(region_bodies.pop());
                           gmt->subtract(tool_body, from_bodies, region_bodies, CUBIT_TRUE);
                           break;
                        case 'u':
                           cout << "*** DEBUG UNION ***" << endl;
                           if (region_bodies.size() >= 2) {
                              from_bodies.append(region_bodies.pop());
                              from_bodies.append(region_bodies.pop());
                              if (!gmt->unite(from_bodies, region_bodies, CUBIT_TRUE)) {
                                 cout << "GOT HERE!" << endl;
                                 region_bodies+=from_bodies;
                              }
                           }
                           else {
                              region_bodies+=from_bodies;
                           } 
                           break;
                        default:
                           // do nothing -- should get here
                           break;
                     }

                     from_bodies.clean_out();
                  }

               }               

               csgTokens.clear();

               if (region_bodies.size() != 0) {
                  all_region_bodies+=region_bodies;
                  region_bodies.clean_out();
               }

           }

        }
/*
        from_bodies.append(gqt->get_body(get_body_id("narb8.s")));
        gmt->subtract(gqt->get_body(get_body_id("sph.s")),from_bodies, region_bodies, CUBIT_TRUE);
*/

        //gmt->imprint(all_bodies, region_bodies);


/*
        vector<string>::const_iterator ci;
        for (ci=g_CsgBoolExp.begin(); ci!=g_CsgBoolExp.end(); ci++) {
           cout << *ci << endl;
           region_bodies.append(gqt->get_body(get_body_id(*ci)));
        }
*/
        cout << "*** CSG DEBUG END ***" << endl;

        //gmt->imprint(old_bodies, all_bodies);

        /*
        if (status == CUBIT_FAILURE) {
           cout << "Failure" << endl;
           exit(1);
        }
        */

        // Make entities list.
        DLIList<RefEntity*> parent_entities;

        // Cast boddies list to entities list
        //CAST_LIST_TO_PARENT(new_bodies, parent_entities);
        
        //gqt->bodies(all_bodies);
        //cout << "The size is " << all_bodies.size() << endl;
        //CAST_LIST_TO_PARENT(all_bodies, parent_entities);
        CAST_LIST_TO_PARENT(all_region_bodies, parent_entities);

        int size = parent_entities.size();
        cout << "The size is " << size << endl;

         // Export geometry
         if (size != 0) {
            status = gqt->export_solid_model(parent_entities, output_file, ACIS_SAT, size, version);
         }
         else {
           cout << "Failure: No geometry to convert!" << endl;
           exit(1);
        }

        CGMApp::instance()->shutdown();

        cout << "PRINT BODY COUNT: " << g_body_cnt << endl;
        cout << "GOT HERE!" << endl;
        abort();

	return 0;
}


/**
 *      R E G I O N _ S T A R T
 *
 * \brief This routine is called when a region is first encountered in the
 * heirarchy when processing a tree
 *
 *      \param tsp tree state (for parsing the tree)
 *      \param pathp A listing of all the nodes traversed to get to this node in the database
 *      \param combp the combination record for this region
 *      \param client_data pointer that was passed as last argument to db_walk_tree()
 *
 */
int
region_start ( db_tree_state *tsp,
               db_full_path *pathp,
              const  rt_comb_internal *combp,
              genptr_t client_data )
{
     rt_comb_internal *comb;
     directory *dp;
     bu_vls str;
     ostringstream ostr;
     string infix, postfix;

    if (debug&DEBUG_NAMES) {
        char *name = db_path_to_string(pathp);
        bu_log("region_start %s\n", name);
        bu_free(name, "reg_start name");
    }

    dp = DB_FULL_PATH_CUR_DIR(pathp);

    /* here is where the conversion should be done */
    cout << "* Here is where the conversion should be done *" << endl;
    printf( "Write this region (name=%s) as a part in your format:\n", dp->d_namep );

    bu_vls_init( &str );

    describe_tree( combp->tree, &str );

    printf( "\t%s\n\n", bu_vls_addr( &str ) );
    ostr <<  bu_vls_addr( &str );

    bu_vls_free( &str );

    infix = ostr.str();
    postfix = infix_to_postfix(infix);
    cout << "\tIn postfix: "<< postfix << endl << endl;
    g_CsgBoolExp.push_back(postfix);

    return 0;
}


/**
 *      R E G I O N _ E N D
 *
 *
 * \brief This is called when all sub-elements of a region have been processed by leaf_func.
 *
 *      \param tsp
 *      \param pathp
 *      \param curtree
 *      \param client_data
 *
 *      \return TREE_NULL if data in curtree was "stolen", otherwise db_walk_tree will
 *      clean up the data in the  tree * that is returned
 *
 * If it wants to retain the data in curtree it can by returning TREE_NULL.  Otherwise
 * db_walk_tree will clean up the data in the  tree * that is returned.
 *
 */
tree *
region_end ( db_tree_state *tsp,
             db_full_path *pathp,
             tree *curtree,
            genptr_t client_data )
{
    if (debug&DEBUG_NAMES) {
        char *name = db_path_to_string(pathp);
        bu_log("region_end   %s\n", name);
        bu_free(name, "region_end name");
    }

    return curtree;
}


/* This routine just produces an ascii description of the Boolean tree.
 * In a real converter, this would output the tree in the desired format.
 */
void
describe_tree(  tree *tree,
                bu_vls *str)
{
	 bu_vls left, right;
	char *union_op = " u ";
	char *subtract_op = " - ";
	char *intersect_op = " + ";
	char *xor_op = " ^ ";
	char *op = NULL;

	BU_CK_VLS(str);

	if( !tree )
	{
		/* this tree has no members */
		bu_vls_strcat( str, "{empty}" );
                set_body_id("{empty}", -1);
		return;
	}

	RT_CK_TREE(tree);

	/* Handle all the possible node types.
	 * the first four are the most common types, and are typically
	 * the only ones found in a BRL-CAD database.
	 */
	switch( tree->tr_op )
	{
		case OP_DB_LEAF:	/* leaf node, this is a member */
			/* Note: tree->tr_l.tl_mat is a pointer to a
			 * transformation matrix to apply to this member
			 */
			bu_vls_strcat( str,  tree->tr_l.tl_name );
			break;
		case OP_UNION:		/*  operator node */
			op = union_op;
			goto binary;
		case OP_INTERSECT:	/* intersection operator node */
			op = intersect_op;
			goto binary;
		case OP_SUBTRACT:	/* subtraction operator node */
			op = subtract_op;
			goto binary;
		case OP_XOR:		/* exclusive "or" operator node */
			op = xor_op;
binary:				/* common for all binary nodes */
			bu_vls_init( &left );
			bu_vls_init( &right );
			describe_tree( tree->tr_b.tb_left, &left );
			describe_tree( tree->tr_b.tb_right, &right );
			bu_vls_putc( str, '(' );
			bu_vls_vlscatzap( str, &left );
			bu_vls_strcat( str, op );
			bu_vls_vlscatzap( str, &right );
			bu_vls_putc( str, ')' );
			break;
		case OP_NOT:
			bu_vls_strcat( str, "(!" );
			describe_tree( tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_GUARD:
			bu_vls_strcat( str, "(G" );
			describe_tree( tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_XNOP:
			bu_vls_strcat( str, "(X" );
			describe_tree( tree->tr_b.tb_left, str );
			bu_vls_putc( str, ')' );
			break;
		case OP_NOP:
			bu_vls_strcat( str, "NOP" );
			break;
		default:
			bu_log( "ERROR: describe_tree() got unrecognized op (%d)\n", tree->tr_op );
			bu_bomb( "ERROR: bad op\n" );
	}
}



/* This routine is called by the tree walker (db_walk_tree)
 * for every primitive encountered in the trees specified on the command line */
tree *
primitive_func( db_tree_state *tsp,
                db_full_path *pathp,
                rt_db_internal *ip,
                genptr_t client_data)
{
    const double NEARZERO = 0.0001;

        int i;
        ostringstream ostr;
        string name;

        directory *dp;

        // Create the geometry modify and query tool
        GeometryModifyTool *gmt = GeometryModifyTool::instance();
        GeometryQueryTool *gqt = GeometryQueryTool::instance();

        dp = DB_FULL_PATH_CUR_DIR(pathp);

        if (debug&DEBUG_NAMES) {
            char *name = db_path_to_string(pathp);
            bu_log("leaf_func    %s\n", name);
            bu_free(name, "region_end name");
        }

	/* handle each type of primitive (see h/rtgeom.h) */
	if( ip->idb_major_type == DB5_MAJORTYPE_BRLCAD ) {
		switch( ip->idb_minor_type )
			{
			/* most commonly used primitives */
			case ID_TOR:	/* torus */
				{
                                        CubitVector x_axis(1.0, 0.0, 0.0);
                                        CubitVector y_axis(0.0, 1.0, 0.0);
                                        CubitVector z_axis(0.0, 0.0, 1.0);

					rt_tor_internal *tor = ( rt_tor_internal *)ip->idb_ptr;

                                        if (debug&DEBUG_NAMES) {
					    printf( "Write this torus (name=%s) in your format:\n", dp->d_namep );
					    printf( "\tV=(%g %g %g)\n", V3ARGS( tor->v ) );
					    printf( "\tnormal=(%g %g %g)\n", V3ARGS( tor->h ) );
					    printf( "\tradius1 = %g\n", tor->r_a );
					    printf( "\tradius2 = %g\n", tor->r_h );
                                        }

                                        CubitVector tor_v( V3ARGS( tor->v ) );
                                        CubitVector tor_h( V3ARGS( tor->h ) );

                                        double tor_ra = tor->r_a;
                                        double tor_rh = tor->r_h;

                                        if (gmt->torus(tor_ra, tor_rh)) {
                                            g_body_cnt++;
                                            ostr << dp->d_namep;
                                            name = ostr.str();
                                            set_body_id(name, g_body_cnt);
                                            ostr.flush();

                                            CubitVector raxis = z_axis * tor_h;
                                            double rangle = z_axis.interior_angle(tor_h);

                                            gqt->rotate(gqt->get_last_body(), raxis, rangle);
                                            gqt->translate(gqt->get_last_body(), tor_v);
                                        }

					break;
				}
			case ID_TGC: /* truncated general cone frustum */
			case ID_REC: /* right elliptical cylinder */
				{
					/* This primitive includes circular cross-section
					 * cones and cylinders
					 */
                                        CubitVector x_axis(1.0, 0.0, 0.0);
                                        CubitVector y_axis(0.0, 1.0, 0.0);
                                        CubitVector z_axis(0.0, 0.0, 1.0);

                                        bool direct_convert = false;

                                        fastf_t maxb, ma, mb, mc, md, mh;
                                        vect_t axb;

                                        CubitVector bbc[8];  // bounding box corners
                                        CubitVector center_of_base;

					rt_tgc_internal *tgc = ( rt_tgc_internal *)ip->idb_ptr;

                                        if (debug&DEBUG_NAMES) {
					    printf( "Write this TGC (name=%s) in your format:\n", dp->d_namep );
					    printf( "\tV=(%g %g %g)\n", V3ARGS( tgc->v ) );
					    printf( "\tH=(%g %g %g)\n", V3ARGS( tgc->h ) );
					    printf( "\tA=(%g %g %g)\n", V3ARGS( tgc->a ) );
					    printf( "\tB=(%g %g %g)\n", V3ARGS( tgc->b ) );
					    printf( "\tC=(%g %g %g)\n", V3ARGS( tgc->c ) );
					    printf( "\tD=(%g %g %g)\n", V3ARGS( tgc->d ) );
                                        }

                                        CubitVector tgc_v( V3ARGS( tgc->v ) );
                                        CubitVector tgc_h( V3ARGS( tgc->h ) );
                                        CubitVector tgc_a( V3ARGS( tgc->a ) );
                                        CubitVector tgc_b( V3ARGS( tgc->b ) );
                                        CubitVector tgc_c( V3ARGS( tgc->c ) );
                                        CubitVector tgc_d( V3ARGS( tgc->d ) );

                                        VCROSS(axb, tgc->a, tgc->b);
                                        maxb = MAGNITUDE(axb);
                                        ma = MAGNITUDE( tgc->a );
                                        mb = MAGNITUDE( tgc->b );
                                        mc = MAGNITUDE( tgc->c );
                                        md = MAGNITUDE( tgc->d );
                                        mh = MAGNITUDE( tgc->h );

                                        // check for right cone or cylinder
                                        if ( fabs(fabs(VDOT(tgc->h,axb)) - (mh*maxb)) < NEARZERO ) {
                                           // have a right cylinder or cone
                                           if ( fabs((ma - mc) - (mb - md))  < NEARZERO ) {
                                              // have similar and aligned base and top
                                              direct_convert = true;
                                           }
                                        }

                                        if (direct_convert) {
                                           if (gmt->cylinder(mh, ma, mb, mc)) {
                                              g_body_cnt++;
                                              ostr << dp->d_namep;
                                              name = ostr.str();
                                              set_body_id(name, g_body_cnt);
                                              ostr.flush();

                                              if ( (fabs(ma - mb) > NEARZERO )) {
                                                 double axbangle = x_axis.interior_angle(tgc_a);
                                                 gqt->rotate(gqt->get_last_body(), z_axis, axbangle);
                                              }

                                              CubitVector raxis = z_axis * tgc_h;
                                              double hangle = z_axis.interior_angle(tgc_h);
                                              gqt->rotate(gqt->get_last_body(), raxis, hangle);

                                              CubitVector tgc_cp = tgc_v + (tgc_h / 2);
                                              gqt->translate(gqt->get_last_body(), tgc_cp);
                                              }
                                        }
                                        else {
                                           goto TESS_CASE;
                                        }

                                break;
				}
			case ID_ELL:
			case ID_SPH:
				{
                                        CubitVector x_axis(1.0, 0.0, 0.0);
                                        CubitVector y_axis(0.0, 1.0, 0.0);
                                        CubitVector z_axis(0.0, 0.0, 1.0);

					/* spheres and ellipsoids */
					rt_ell_internal *ell = ( rt_ell_internal *)ip->idb_ptr;

                                        if (debug&DEBUG_NAMES) {
					   printf( "Write this ellipsoid (name=%s) in your format:\n", dp->d_namep );
					   printf( "\tV=(%g %g %g)\n", V3ARGS( ell->v ) );
					   printf( "\tA=(%g %g %g)\n", V3ARGS( ell->a ) );
					   printf( "\tB=(%g %g %g)\n", V3ARGS( ell->b ) );
					   printf( "\tC=(%g %g %g)\n", V3ARGS( ell->c ) );
                                        }

                                        CubitVector ell_v( V3ARGS( ell->v ) );

                                        double magsq_a = MAGSQ ( ell->a );
                                        double magsq_b = MAGSQ ( ell->b );
                                        double magsq_c = MAGSQ ( ell->c );
                                            
                                        if ( ( fabs(magsq_a - magsq_b) < NEARZERO ) && ( fabs(magsq_a - magsq_c) < NEARZERO ) ) {
                                           if (gmt->sphere( sqrt( magsq_a ) )) {
                                              g_body_cnt++;
                                              ostr << dp->d_namep;
                                              name = ostr.str();
                                              set_body_id(name, g_body_cnt);
                                              ostr.flush();

                                              gqt->translate(gqt->get_last_body(), ell_v);
                                           }
                                        }
                                        else {
                                           vect_t unitv;
                                           double angles[5];
                                           CubitVector rot_axis;

                                           CubitVector ell_a( V3ARGS( ell->a ) );
                                           CubitVector ell_b( V3ARGS( ell->b ) );
                                           CubitVector ell_c( V3ARGS( ell->c ) );

                                           double mag_a = sqrt( magsq_a );
                                           double mag_b = sqrt( magsq_b );
                                           double mag_c = sqrt( magsq_c );

                                           if (gmt->sphere(1.0)) {
                                              g_body_cnt++;
                                              ostr << dp->d_namep;
                                              name = ostr.str();
                                              set_body_id(name, g_body_cnt);
                                              ostr.flush();

                                              CubitVector scale_vector(mag_a, mag_b, mag_c);
                                              gqt->scale(gqt->get_last_body(), scale_vector, true);

                                              if ( (fabs(mag_a - mag_b) > NEARZERO )) {
                                                 double axbangle = x_axis.interior_angle(ell_a);
                                                 gqt->rotate(gqt->get_last_body(), z_axis, axbangle);
                                              }

                                              CubitVector raxis = z_axis * ell_c;
                                              double hangle = z_axis.interior_angle(ell_c);
                                              gqt->rotate(gqt->get_last_body(), raxis, hangle);
                                              gqt->translate(gqt->get_last_body(), ell_v);
                                           }

                                        }

					break;
				}
			case ID_ARB8:	/* convex primitive with from four to six faces */
				{
					/* this primitive may have degenerate faces
					 * faces are: 0123, 7654, 0347, 1562, 0451, 3267
					 * (points listed above in counter-clockwise order)
					 */
					rt_arb_internal *arb = ( rt_arb_internal *)ip->idb_ptr;

                                        if (debug&DEBUG_NAMES) {
					    printf( "Write this ARB (name=%s) in your format:\n", dp->d_namep );
					    for( i=0 ; i<8 ; i++ ) {
					        printf( "\tpoint #%d: (%g %g %g)\n", i, V3ARGS( arb->pt[i] ) );
                                            }
                                        }

                                        goto TESS_CASE;
				}

			case ID_BOT:	/* Bag O' Triangles */
				{
                                        rt_bot_internal *bot = (rt_bot_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}

				/* less commonly used primitives */
			case ID_ARS:
				{
					/* series of curves
					 * each with the same number of points
					 */
					rt_ars_internal *ars = ( rt_ars_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_HALF:
				{
					/* half universe defined by a plane */
					 rt_half_internal *half = ( rt_half_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_POLY:
				{
					/* polygons (up to 5 vertices per) */
					rt_pg_internal *pg = ( rt_pg_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_BSPLINE:
				{
					/* NURB surfaces */
					rt_nurb_internal *nurb = ( rt_nurb_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_NMG:
				{
                                        nmgregion *r;
                                        shell *s;

					/* N-manifold geometry */
					model *m = ( model *)ip->idb_ptr;

                                        NMG_CK_MODEL(m);

                                        /* walk the nmg to convert it to triangular facets */
                                        nmg_triangulate_model(m, tsp->ts_tol);

                                        if (make_bot(r, m, s)) {
                                           g_body_cnt++;
                                           ostr << dp->d_namep;
                                           name = ostr.str();
                                           set_body_id(name, g_body_cnt);
                                           ostr.flush();
                                        }

					break;
				}
			case ID_ARBN:
				{
					rt_arbn_internal *arbn = ( rt_arbn_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}

			case ID_DSP:
				{
					/* Displacement map (terrain primitive) */
					/* normally used for terrain only */
					/* the DSP primitive may reference an external file */
					rt_dsp_internal *dsp = ( rt_dsp_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_HF:
				{
					/* height field (terrain primitive) */
					/* the HF primitive references an external file */
					rt_hf_internal *hf = ( rt_hf_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}

				/* rarely used primitives */
			case ID_EBM:
				{
					/* extruded bit-map */
					/* the EBM primitive references an external file */
					rt_ebm_internal *ebm = ( rt_ebm_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_VOL:
				{
					/* the VOL primitive references an external file */
					rt_vol_internal *vol = ( rt_vol_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_PIPE:
				{
					rt_pipe_internal *pipe = ( rt_pipe_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_PARTICLE:
				{
					rt_part_internal *part = ( rt_part_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_RPC:
				{
					rt_rpc_internal *rpc = ( rt_rpc_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_RHC:
				{
					rt_rhc_internal *rhc = ( rt_rhc_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_EPA:
				{
					rt_epa_internal *epa = ( rt_epa_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_EHY:
				{
					rt_ehy_internal *ehy = ( rt_ehy_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_ETO:
				{
					rt_eto_internal *eto = ( rt_eto_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_GRIP:
				{
					rt_grip_internal *grip = ( rt_grip_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}

			case ID_SKETCH:
				{
					rt_sketch_internal *sketch = ( rt_sketch_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}
			case ID_EXTRUDE:
				{
					/* note that an extrusion references a sketch, make sure you convert
					 * the sketch also
					 */
					rt_extrude_internal *extrude = ( rt_extrude_internal *)ip->idb_ptr;

                                        goto TESS_CASE;

					break;
				}

			default:    TESS_CASE:
			        // bu_log( "Primitive %s is unrecognized type (%d)\n", dp->d_namep, ip->idb_type );
                                // break;
        {
            /* This section is for primitives which cannot be directly represented in the
             * format we are going to.  So we convert it to triangles
             */
            nmgregion *r;
            model *m = nmg_mm();
            shell *s;

            NMG_CK_MODEL(m);

            if (rt_functab[ip->idb_type].ft_tessellate(&r, m, ip, tsp->ts_ttol, tsp->ts_tol) != 0) {
               exit(-1);
            }

            //bu_log("triangulate %d\n", ip->idb_minor_type);
            /* walk the nmg to convert it to triangular facets */
            nmg_triangulate_model(m, tsp->ts_tol);

            if (make_bot(r, m, s)) {
               g_body_cnt++;
               ostr << dp->d_namep;
               name = ostr.str();
               set_body_id(name, g_body_cnt);
               ostr.flush();
            }


            break;
        }


	         }
	}
        else {
		switch( ip->idb_major_type ) {
			case DB5_MAJORTYPE_BINARY_UNIF:
				{
					/* not actually a primitive, just a block of storage for data
					 * a uniform array of chars, ints, floats, doubles, ...
					 */
					rt_binunif_internal *bin = ( rt_binunif_internal *)ip->idb_ptr;

					printf( "Found a binary object (%s)\n\n", dp->d_namep );
					break;
				}
			default:
				bu_log( "Major type of %s is unrecognized type (%d)\n", dp->d_namep, ip->idb_major_type );
				break;
		}
	}

        return ( tree *) NULL;
}


/* routine to output the facetted NMG representation of a BRL-CAD region */
void
output_triangles( nmgregion *r,
                  model *m,
                  shell *s )
{
	vertex *v;

 	for( BU_LIST_FOR( s, shell, &r->s_hd ) )
	{
		faceuse *fu;

		NMG_CK_SHELL( s );

		for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
		{
			loopuse *lu;
			vect_t facet_normal;

			NMG_CK_FACEUSE( fu );

			if( fu->orientation != OT_SAME )
				continue;

			/* Grab the face normal if needed */
			NMG_GET_FU_NORMAL( facet_normal, fu);

			for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
			{
				edgeuse *eu;

				NMG_CK_LOOPUSE( lu );

				if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
					continue;

				/* loop through the edges in this loop (facet) */
				printf( "\tfacet:\n" );
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );

					v = eu->vu_p->v_p;
					NMG_CK_VERTEX( v );
					printf( "\t\t(%g %g %g)\n", V3ARGS( v->vg_p->coord ) );
				}
			}
		}
	}
}


/* routine to output the facetted NMG representation of a BRL-CAD region */
bool
make_bot( nmgregion *r,
          model *m,
          shell *s )
{
        // Create the geometry modify and query tool
        GeometryModifyTool *gmt = GeometryModifyTool::instance();
        GeometryQueryTool *gqt = GeometryQueryTool::instance();

        vertex *v;

        CubitVector cv;
        DLIList <RefVertex*> VertexList;
        DLIList <RefEdge*> EdgeList;
        DLIList <RefFace*> FaceList;

        for( BU_LIST_FOR( s, shell, &r->s_hd ) )
        {
                faceuse *fu;

                NMG_CK_SHELL( s );

                for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
                {
                        loopuse *lu;
                        vect_t facet_normal;

                        NMG_CK_FACEUSE( fu );

                        if( fu->orientation != OT_SAME )
                                continue;

                        /* Grab the face normal if needed */
                        NMG_GET_FU_NORMAL( facet_normal, fu);

                        for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
                        {
                                edgeuse *eu;

                                NMG_CK_LOOPUSE( lu );

                                if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
                                        continue;

                                /* loop through the edges in this loop (facet) */
                                // printf( "\tfacet:\n" );
                                for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
                                {
                                        NMG_CK_EDGEUSE( eu );

                                        v = eu->vu_p->v_p;
                                        NMG_CK_VERTEX( v );
                                        // printf( "\t\t(%g %g %g)\n", V3ARGS( v->vg_p->coord ) );
                                        cv.set(V3ARGS( v->vg_p->coord ));
                                        VertexList.append(gmt->make_RefVertex(cv));
                                }
                                const int MAX_VERTICES = 3;
                                for (int i=0; i <= MAX_VERTICES-1; i++) {
                                    EdgeList.append(gmt->make_RefEdge(STRAIGHT_CURVE_TYPE,
                                                    VertexList[i], VertexList[(i+1)%MAX_VERTICES]));
                                }

                                RefFace *face = gmt->make_RefFace(PLANE_SURFACE_TYPE, EdgeList);

                                EdgeList.clean_out();
                                VertexList.clean_out();

                                FaceList.append(face);
                        }

                }

        }
        Body *BotBody, *RegBotBody;

        CubitStatus status;

        status = gmt->create_body_from_surfs(FaceList, BotBody);

        if (status != CUBIT_FAILURE) {
           cout << "make_bot made a Body!" << endl;
           gmt->regularize_body(BotBody, RegBotBody); 
        } 
        else {
           cout << "make_bot did not made a Body!" << endl;
        }

        FaceList.clean_out();

        return status;
}

// set_body_id function
void set_body_id(string body_name, int body_id)
{
   g_body_id_map[body_name] = body_id;
}

// get_body_id function
int get_body_id(string body_name)
{
   const int ERR_FLAG = -99;
   int rVal;

   map<string, int>::iterator iter;
   iter = g_body_id_map.find(body_name);

   if (iter != g_body_id_map.end()) {
      rVal = iter->second;
   }
   else {
      rVal = ERR_FLAG;
   }

   return rVal;
}

tree *
booltree_evaluate( tree *tp, resource *resp )
{
        union tree              *tl;
        union tree              *tr;
        int                     op;
        const char              *op_str;
        char                    *name;

        enum BOOL_ENUM_TYPE { ADD, ISECT, SUBTR };

        RT_CK_TREE(tp);

        switch(tp->tr_op) {
        case OP_NOP:
                return(0);
        case OP_DB_LEAF:
                /* Hit a tree leaf */
                return tp;
        case OP_UNION:
                op = ADD;
                op_str = " u ";
                break;
        case OP_INTERSECT:
                op = ISECT;
                op_str = " + ";
                break;
        case OP_SUBTRACT:
                op = SUBTR;
                op_str = " - ";
                break;
        default:
                bu_log("booltree_evaluate: bad op %d\n", tp->tr_op);
                return(0);
        }
        /* Handle a boolean operation node.  First get it's leaves. */
        tl = booltree_evaluate(tp->tr_b.tb_left, resp);
        tr = booltree_evaluate(tp->tr_b.tb_right, resp);

        if (tl == 0 || !tl->tr_d.td_r) {
                if (tr == 0 || !tr->tr_d.td_r)
                        return 0;
                if( op == ADD )
                        return tr;
                /* For sub and intersect, if lhs is 0, result is null */
                //db_free_tree(tr);
                tp->tr_b.tb_right = TREE_NULL;
                tp->tr_op = OP_NOP;
                return 0;
        }
        if (tr == 0 || !tr->tr_d.td_r) {
                if (tl == 0 || !tl->tr_d.td_r)
                        return 0;
                if( op == ISECT )  {
                        db_free_tree(tl, resp);
                        tp->tr_b.tb_left = TREE_NULL;
                        tp->tr_op = OP_NOP;
                        return 0;
                }
                /* For sub and add, if rhs is 0, result is lhs */
                return tl;
        }
        if( tl->tr_op != OP_DB_LEAF )  rt_bomb("booltree_evaluate() bad left tree\n");
        if( tr->tr_op != OP_DB_LEAF )  rt_bomb("booltree_evaluate() bad right tree\n");

        bu_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name );
        cout << "******" << tl->tr_d.td_name << op_str << tr->tr_d.td_name << "***********" << endl;

        /* Build string of result name */
        name = (char *)bu_malloc( strlen(tl->tr_d.td_name)+3+strlen(tr->tr_d.td_name)+2+1,
                "booltree_evaluate name");
        name[0] = '(';
        strcpy( name+1, tl->tr_d.td_name );
        strcat( name+1, op_str );
        strcat( name+1, tr->tr_d.td_name );
        strcat( name+1, ")" );

        /* Clean up child tree nodes (and their names) */
        db_free_tree(tl, resp);
        db_free_tree(tr, resp);

        /* Convert argument binary node into a result node */
        tp->tr_op = OP_DB_LEAF;
        tp->tr_d.td_name = name;
        return tp;
}

string infix_to_postfix(string str)
{

   stack <char> s;
   ostringstream ostr;
   char c;

   for (int i = 0; i < strlen(str.c_str()); i++) {
      c = str[i];
      if ( c == '(' ) {
         s.push(c);
      }
      else if ( c == ')' ) {
         while ( (c = s.top()) != '(' ) {
            ostr << ' ' << c;
            s.pop();
         }
         s.pop();
      }
      else if (((c == 'u') || (c == '+') || (c == '-')) && (str[i+1] == ' ')) {
         s.push(c);
         i++;
      }
      else {
         ostr << c ;
      }
   }

   if (!s.empty()) {
      ostr << s.top();
      s.pop();
   }

   return ostr.str();
}

void tokenize(const string& str, vector<string>& tokens, const string& delimiters)
{
   // Skip delimiters at beginning.
   string::size_type lastPos = str.find_first_not_of(delimiters, 0);

   // Find first "non-delimiter".
   string::size_type pos     = str.find_first_of(delimiters, lastPos);

   while (string::npos != pos || string::npos != lastPos) {
       // Found a token, add it to the vector.
       tokens.push_back(str.substr(lastPos, pos - lastPos));

       // Skip delimiters.  Note the "not_of"
       lastPos = str.find_first_not_of(delimiters, pos);

       // Find next "non-delimiter"
       pos = str.find_first_of(delimiters, lastPos);
   }
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
