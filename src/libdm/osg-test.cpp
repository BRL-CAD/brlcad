/*                    O S G - T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file osg-test.cpp
 *
 *  BRL-CAD test application for OpenSceneGraph integration, based off of
 *  osghud example from OpenSceneGraph, which has the following license:
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

extern "C" {
#include "common.h"
#include "bu/log.h"
#include "bu/list.h"
#include "mater.h"
#include "raytrace.h"
#include "rtfunc.h"
}

#include <osgUtil/Optimizer>
#include <osgDB/ReadFile>

#include <osgViewer/Viewer>
#include <osgViewer/CompositeViewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/StateSetManipulator>


#include <osg/Material>
#include <osg/Geode>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/PolygonOffset>
#include <osg/MatrixTransform>
#include <osg/Camera>
#include <osg/RenderInfo>
#include <osg/LineStipple>

#include <osgDB/WriteFile>

#include <osgText/Text>

struct bu_list *
obj_vlist(const struct directory *dp, const struct db_i *dbip, matp_t UNUSED(mat))
{
    struct bu_list *plot_segments;
    struct rt_db_internal intern;
    const struct bn_tol tol = {BN_TOL_MAGIC, 0.0005, 0.0005 * 0.0005, 1e-6, 1 - 1e-6};
    const struct rt_tess_tol rttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0};
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_exit(1, "ERROR: Unable to get internal representation of %s\n", dp->d_namep);
    }
    BU_GET(plot_segments, struct bu_list);
    BU_LIST_INIT(plot_segments);
    if (rt_obj_plot(plot_segments, &intern, &rttol, &tol) < 0) {
	bu_exit(1, "ERROR: Unable to get plot segment list from %s\n", dp->d_namep);
    }
    rt_db_free_internal(&intern);
    return plot_segments;
}

void
create_solid_nodes(std::map<struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
      	struct db_i *dbip,
	struct directory *dp)
{
    std::map<struct directory *, struct bu_list *> vlists;

    const char *solid_search = "! -type comb";
    struct bu_ptbl solids = BU_PTBL_INIT_ZERO;
    (void)db_search(&solids, DB_SEARCH_RETURN_UNIQ_DP, solid_search, 1, &dp, dbip);

    for (int i = (int)BU_PTBL_LEN(&solids) - 1; i >= 0; i--) {
	/* Get the vlist associated with this particular object */
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&solids, i);
	if (vlists.find(curr_dp) == vlists.end()) {

	    /* Get the vlist from librt */
	    struct bu_list *plot_segments = obj_vlist(curr_dp, dbip, NULL);
	    vlists[curr_dp] = plot_segments;

	    /* Convert vlist into osg geometry */
	    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
	    osg::ref_ptr<osg::Vec3Array> vertArray = new osg::Vec3Array;
	    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
	    osg::ref_ptr<osg::Group> wrapper = new osg::Group;
	    geode->setName(curr_dp->d_namep);
	    wrapper->setName(curr_dp->d_namep);
	    {
		int currentVertex=0;
		std::vector<int>startIndexes;
		std::vector<int>stopIndexes;
		struct bn_vlist *vp;
		for (BU_LIST_FOR(vp, bn_vlist, plot_segments))
		{
		    int j;
		    int nused = vp->nused;
		    const int *cmd = vp->cmd;
		    point_t *pt = vp->pt;

		    for (j=0 ; j < nused ; j++, cmd++, pt++, currentVertex++)
		    {
			switch (*cmd)
			{
			    case BN_VLIST_POLY_START:
				break;
			    case BN_VLIST_POLY_MOVE:  // fallthrough
			    case BN_VLIST_LINE_MOVE:
				if (startIndexes.size() > 0)
				{
				    // finish off the last linestrip
				    stopIndexes.push_back(currentVertex-1);
				}
				// remember where the new linestrip begins
				startIndexes.push_back(currentVertex);

				// start a new linestrip
				vertArray->push_back(osg::Vec3((float)((*pt)[X]), (float)((*pt)[Y]),(float)(*pt)[Z]));

				break;
			    case BN_VLIST_POLY_DRAW: // fallthrough
			    case BN_VLIST_POLY_END: // fallthrough
			    case BN_VLIST_LINE_DRAW:
				// continue existing linestrip
				vertArray->push_back(osg::Vec3((float)((*pt)[X]), (float)((*pt)[Y]),(float)(*pt)[Z]));
				break;
			    case BN_VLIST_POINT_DRAW:
				break;
			}
		    }
		}

		if (startIndexes.size() > 0)
		    stopIndexes.push_back(--currentVertex);

		geom->setUseDisplayList(true);
		geom->setVertexArray(vertArray);

		for (unsigned j=0 ; j < startIndexes.size() ; j++) {
		    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, startIndexes[j], (stopIndexes[j]-startIndexes[j])+1 ));
		}

		geom->setName(curr_dp->d_namep);     // set drawable to same name as region
		geode->addDrawable(geom);
		osg::StateSet* state = geode->getOrCreateStateSet();
		state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	    }
	    wrapper->addChild(geode);
	    (*osg_nodes)[curr_dp] = wrapper.get();
	    RT_FREE_VLIST(plot_segments);
	    BU_PUT(plot_segments, struct bu_list);
	}
    }
    db_search_free(&solids);

}

void
create_comb_nodes(std::map<struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
      	struct db_i *dbip,
	struct directory *dp)
{
    const char *comb_search = "-type comb";
    struct bu_ptbl combs = BU_PTBL_INIT_ZERO;
    (void)db_search(&combs, DB_SEARCH_RETURN_UNIQ_DP, comb_search, 1, &dp, dbip);
    for (int i = (int)BU_PTBL_LEN(&combs) - 1; i >= 0; i--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&combs, i);
	osg::ref_ptr<osg::Group> comb = new osg::Group;
	comb->setName(curr_dp->d_namep);
	(*osg_nodes)[curr_dp] = comb.get();

	struct rt_db_internal intern;
	(void)rt_db_get_internal(&intern, curr_dp, dbip, (fastf_t *)NULL, &rt_uniresource);
	struct rt_comb_internal *comb_internal= (struct rt_comb_internal *)intern.idb_ptr;
	/* Set color for this comb */
	float color[3] = {-1.0, -1.0, -1.0};
	if (comb_internal->rgb_valid) {
	    VSCALE(color, comb_internal->rgb, (1.0/255.0) );
	} else {
	    struct mater *mp = MATER_NULL;
	    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		if (comb_internal->region_id <= mp->mt_high && comb_internal->region_id >= mp->mt_low) {
		    color[0] = mp->mt_r/255.0;
		    color[1] = mp->mt_g/255.0;
		    color[2] = mp->mt_b/255.0;
		    break;
		}
	    }
	}
	if (color[0] > -1) {
	    osg::ref_ptr<osg::Material> mtl = new osg::Material;
	    mtl->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(color[0], color[1], color[2], 1.0f));
	    osg::StateSet* state = comb->getOrCreateStateSet();
	    if (comb_internal->inherit || comb_internal->region_flag) {
		state->setAttributeAndModes(mtl.get(), osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
	    } else {
		state->setAttributeAndModes(mtl.get(), osg::StateAttribute::ON);
	    }
	}
	rt_db_free_internal(&intern);

    }
    for (int i = (int)BU_PTBL_LEN(&combs) - 1; i >= 0; i--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&combs, i);
	const char *comb_children_search = "-mindepth 1 -maxdepth 1";
	struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
	(void)db_search(&comb_children, DB_SEARCH_TREE, comb_children_search, 1, &curr_dp, dbip);
	for (int j = (int)BU_PTBL_LEN(&comb_children) - 1; j >= 0; j--) {
	    struct db_full_path *curr_path = (struct db_full_path *)BU_PTBL_GET(&comb_children, j);
	    if (curr_path->fp_mat[curr_path->fp_len - 1]) {
		mat_t m;
		MAT_TRANSPOSE(m, curr_path->fp_mat[curr_path->fp_len - 1]);
		osg::Matrixd osgMat(m);
		osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform(osgMat);
		mt->addChild((*osg_nodes)[DB_FULL_PATH_CUR_DIR(curr_path)]);
		(*osg_nodes)[curr_dp]->addChild(mt);

		/* If this child is subtracted in this comb, stipple the line drawings */
		if (curr_path->fp_bool[curr_path->fp_len - 1] == 4) {
		    osg::StateSet* state = mt->getOrCreateStateSet();
		    osg::ref_ptr<osg::LineStipple> ls = new osg::LineStipple();
		    ls->setPattern(0xCF33);
		    ls->setFactor(1);
		    state->setAttributeAndModes(ls.get());
		}
	    } else {
		if (curr_path->fp_bool[curr_path->fp_len - 1] == 4) {
		    osg::ref_ptr<osg::Group> property_wrapper = new osg::Group;
		    property_wrapper->setName(DB_FULL_PATH_CUR_DIR(curr_path)->d_namep);
		    osg::StateSet* state = property_wrapper->getOrCreateStateSet();
		    osg::ref_ptr<osg::LineStipple> ls = new osg::LineStipple();
		    ls->setPattern(0xCF33);
		    ls->setFactor(1);
		    state->setAttributeAndModes(ls.get());
		    property_wrapper->addChild((*osg_nodes)[DB_FULL_PATH_CUR_DIR(curr_path)]);
		    (*osg_nodes)[curr_dp]->addChild(property_wrapper);
		} else {
		    (*osg_nodes)[curr_dp]->addChild((*osg_nodes)[DB_FULL_PATH_CUR_DIR(curr_path)]);
		}
	    }
	}
	db_search_free(&comb_children);
    }
    db_search_free(&combs);
}

#if 0
void
create_region_nodes(std::map<struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	struct db_i *dbip,
	struct directory *dp)
{
    const struct bn_tol tol = {BN_TOL_MAGIC, 0.0005, 0.0005 * 0.0005, 1e-6, 1 - 1e-6};
    const struct rt_tess_tol rttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0};

    const char *region_search = "-type region";
    struct bu_ptbl regions = BU_PTBL_INIT_ZERO;
    (void)db_search(&regions, DB_SEARCH_RETURN_UNIQ_DP, region_search, 1, &dp, dbip);

    /*get the comb properies for the group above the region geode - need to refactor some of the above logic... */

    for (int i = (int)BU_PTBL_LEN(&regions) - 1; i >= 0; i--) {
	/*get the comb properies for the group above the region geode - need to refactor some of the above logic... */

	/*Search for all the full paths below this comb - that's the list of vlists we need for this particular region,
	 *using the full paths below the current region to place them in their final relative positions.
	 *(db_full_path_transformation_matrix)
	 */
    }

    db_search_free(&regions);

}
#endif

int main( int argc, char **argv )
{
    std::map<struct directory *, osg::ref_ptr<osg::Group> > osg_nodes;
    struct db_i *dbip = DBI_NULL;
    struct directory *dp = RT_DIR_NULL;

    if (argc != 3 || !argv) {
	bu_exit(1, "Error - please specify a .g file and an object\n");
    }
    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if ((dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to fine object %s in %s\n", argv[2], argv[1]);
    }

    /* Need to initialize this for rt_obj_plot, which may call RT_ADD_VLIST */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    create_solid_nodes(&(osg_nodes), dbip, dp);
    create_comb_nodes(&(osg_nodes), dbip, dp);

    // construct the viewer.
    osgViewer::Viewer viewer;
    viewer.setUpViewInWindow(0, 0, 640, 480);

    // add the state manipulator
    viewer.addEventHandler( new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()) );

    // add the thread model handler
    viewer.addEventHandler(new osgViewer::ThreadingHandler);

    // add the window size toggle handler
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);

    // add the stats handler
    viewer.addEventHandler(new osgViewer::StatsHandler);

    // The settings below do not seem to improve FPS, and the latter results in no geometry being
    // visible without improving the FPS.
    //viewer.getCamera()->setCullingMode(osg::CullSettings::NO_CULLING);
    //viewer.getCamera()->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);

    osgUtil::Optimizer optimizer;
    optimizer.optimize(osg_nodes[dp]);
    viewer.setSceneData(osg_nodes[dp]);

    viewer.realize();

    return viewer.run();
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
