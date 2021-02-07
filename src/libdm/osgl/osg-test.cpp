/*                    O S G - T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

#include "common.h"

#include <map>

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

#include "bu/app.h"
#include "bu/list.h"
#include "raytrace.h"
#include "rt/func.h"


struct bu_list *
obj_vlist(const struct directory *dp, const struct db_i *dbip, mat_t mat)
{
    struct bu_list *plot_segments;
    struct rt_db_internal intern;
    const struct bn_tol tol = {BN_TOL_MAGIC, 0.0005, 0.0005 * 0.0005, 1e-6, 1 - 1e-6};
    const struct bg_tess_tol rttol = {BG_TESS_TOL_MAGIC, 0.0, 0.01, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource) < 0) {
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
add_vlist_to_geom(osg::Geometry *geom, struct bu_list *plot_segments)
{
    std::vector<int>startIndexes;
    std::vector<int>stopIndexes;
    osg::Vec3Array *vertArray = (osg::Vec3Array *)geom->getVertexArray();
    struct bn_vlist *vp;

    if (!vertArray) {
	osg::ref_ptr<osg::Vec3Array> new_vertArray = new osg::Vec3Array();
	geom->setVertexArray(new_vertArray);
	vertArray = (osg::Vec3Array *)geom->getVertexArray();
    }
    int currentVertex = vertArray->getNumElements();

    geom->setUseDisplayList(true);

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


    for (unsigned j=0 ; j < startIndexes.size() ; j++) {
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, startIndexes[j], (stopIndexes[j]-startIndexes[j])+1 ));
    }

}

osg::ref_ptr<osg::Group>
solid_node(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	const struct directory *dp, const struct db_i *dbip)
{
    osg::ref_ptr<osg::Group> wrapper;
    if ((*osg_nodes).find(dp) == (*osg_nodes).end()) {
	wrapper = new osg::Group;
	(*osg_nodes)[dp] = wrapper.get();
    } else {
	wrapper = (*osg_nodes)[dp];
    }

    /* Clear the children from the wrapper, if it has any */
    unsigned int children_cnt = wrapper->getNumChildren();
    wrapper->removeChildren(0, children_cnt);

    /* Set up the OSG superstructure for the solid geometry node */
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    wrapper->setName(dp->d_namep);
    geode->setName(dp->d_namep);
    geom->setName(dp->d_namep);     // set drawable to same name as region
    geode->addDrawable(geom);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    wrapper->addChild(geode);

    /* Actually add the geometry from the vlist */
    mat_t mat;
    MAT_IDN(mat);
    struct bu_list *plot_segments = obj_vlist(dp, dbip, mat);
    add_vlist_to_geom(geom, plot_segments);

    /* Cleanup */
    RT_FREE_VLIST(plot_segments);
    BU_PUT(plot_segments, struct bu_list);

    return wrapper;
}

void
create_solid_nodes(std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
      	struct directory *dp, struct db_i *dbip)
{
    const char *solid_search = "! -type comb";
    struct bu_ptbl solids = BU_PTBL_INIT_ZERO;
    (void)db_search(&solids, DB_SEARCH_RETURN_UNIQ_DP, solid_search, 1, &dp, dbip, NULL);

    for (int i = (int)BU_PTBL_LEN(&solids) - 1; i >= 0; i--) {
	/* Get the vlist associated with this particular object */
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&solids, i);
	if ((*osg_nodes).find(curr_dp) == (*osg_nodes).end()) {
	    osg::ref_ptr<osg::Group> node = solid_node(osg_nodes, curr_dp, dbip);
	    (*osg_nodes)[curr_dp] = node.get();
	}
    }
    db_search_free(&solids);

}

osg::ref_ptr<osg::Group>
bare_comb_node(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	const struct directory *dp, const struct db_i *dbip)
{
    osg::ref_ptr<osg::Group> comb;
    if ((*osg_nodes).find(dp) == (*osg_nodes).end()) {
	comb = new osg::Group;
	(*osg_nodes)[dp] = comb.get();
    } else {
	comb = (*osg_nodes)[dp];
    }

    comb->setName(dp->d_namep);

    struct rt_db_internal intern;
    (void)rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource);
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

    return comb;
}

int
subtracted_solid(struct db_full_path *path)
{
    int ret = 0;
    for (unsigned int i = 0; i < path->fp_len; i++) {
	if(path->fp_bool[i] == 4) ret = 1;
    }
    return ret;
}

void
add_comb_child(osg::Group *comb, osg::Group *child, struct db_full_path *child_path, struct db_i *dbip)
{
    mat_t m = MAT_INIT_IDN;
    mat_t m_idn = MAT_INIT_IDN;
    const struct bn_tol tol = {BN_TOL_MAGIC, 0.0005, 0.0005 * 0.0005, 1e-6, 1 - 1e-6};
    (void)db_path_to_mat(dbip, child_path, (matp_t)(&m), child_path->fp_len, &rt_uniresource);
    if (!bn_mat_is_equal(m, m_idn, &tol)) {
	mat_t mt = MAT_INIT_IDN;
	MAT_TRANSPOSE(mt, m);
	osg::Matrixd osgMat(mt);
	osg::ref_ptr<osg::MatrixTransform> osgmt = new osg::MatrixTransform(osgMat);
	osgmt->addChild(child);
	comb->addChild(osgmt);

	/* If this child_path is subtracted in this comb, stipple the line drawings */
	if (child_path->fp_bool[child_path->fp_len - 1] == 4) {
	    osg::StateSet* state = osgmt->getOrCreateStateSet();
	    osg::ref_ptr<osg::LineStipple> ls = new osg::LineStipple();
	    ls->setPattern(0xCF33);
	    ls->setFactor(1);
	    state->setAttributeAndModes(ls.get());
	}
    } else {
	if (child_path->fp_bool[child_path->fp_len - 1] == 4) {
	    osg::ref_ptr<osg::Group> property_wrapper = new osg::Group;
	    property_wrapper->setName(DB_FULL_PATH_CUR_DIR(child_path)->d_namep);
	    osg::StateSet* state = property_wrapper->getOrCreateStateSet();
	    osg::ref_ptr<osg::LineStipple> ls = new osg::LineStipple();
	    ls->setPattern(0xCF33);
	    ls->setFactor(1);
	    state->setAttributeAndModes(ls.get());
	    property_wrapper->addChild(child);
	    comb->addChild(property_wrapper);
	} else {
	    comb->addChild(child);
	}
    }
}

/* In the case of regions, we need to be able to remove and restore the individual
 * comb children, because for normal display (as opposed to editing mode) we need to
 * use a single composite wireframe for regions */
osg::ref_ptr<osg::Group>
full_comb_node(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
      	struct directory *dp, struct db_i *dbip)
{
    osg::ref_ptr<osg::Group> comb = bare_comb_node(osg_nodes, dp, dbip);

    /* Clear the immediate children from this node, if it has any */
    unsigned int children_cnt = comb->getNumChildren();
    comb->removeChildren(0, children_cnt);

    const char *comb_search = "-type comb";
    struct bu_ptbl combs = BU_PTBL_INIT_ZERO;
    (void)db_search(&combs, DB_SEARCH_RETURN_UNIQ_DP, comb_search, 1, &dp, dbip, NULL);
    for (int i = (int)BU_PTBL_LEN(&combs) - 1; i >= 0; i--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&combs, i);
	osg::ref_ptr<osg::Group> sub_comb = bare_comb_node(osg_nodes, curr_dp, dbip);
	const char *comb_children_search = "-mindepth 1 -maxdepth 1";
	struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
	(void)db_search(&comb_children, DB_SEARCH_TREE, comb_children_search, 1, &curr_dp, dbip, NULL);
	for (int j = (int)BU_PTBL_LEN(&comb_children) - 1; j >= 0; j--) {
	    struct db_full_path *curr_path = (struct db_full_path *)BU_PTBL_GET(&comb_children, j);
	    struct directory *curr_child_dp = DB_FULL_PATH_CUR_DIR(curr_path);
	    if (curr_child_dp->d_flags & RT_DIR_COMB) {
		osg::ref_ptr<osg::Group> new_node = bare_comb_node(osg_nodes, curr_child_dp, dbip);
		(*osg_nodes)[curr_child_dp] = new_node.get();
	    }
	    if (curr_child_dp->d_flags & RT_DIR_SOLID) {
		osg::ref_ptr<osg::Group> new_node = solid_node(osg_nodes, curr_child_dp, dbip);
		(*osg_nodes)[curr_child_dp] = new_node.get();
	    }
	    add_comb_child(sub_comb, (*osg_nodes)[curr_child_dp], curr_path, dbip);
	}
	db_search_free(&comb_children);
    }
    db_search_free(&combs);

    return comb;
}

osg::ref_ptr<osg::Group>
full_region_node(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	struct directory *dp, struct db_i *dbip)
{
    osg::ref_ptr<osg::Group> region = bare_comb_node(osg_nodes, dp, dbip);

    /* Clear the immediate children from this node, if it has any */
    unsigned int children_cnt = region->getNumChildren();
    region->removeChildren(0, children_cnt);

    /* We now create up to two pseudo-solid nodes holding the full region drawable info - one for non-subtracted
     * elements (no stipple) and one (if needed) for subtracted elements (stipple)*/
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    geode->setName(dp->d_namep);
    geom->setName(dp->d_namep);     // set drawable to same name as region
    geode->addDrawable(geom);
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    region->addChild(geode);

    /* Define the subtraction container, but don't add it as a child of the region unless we actually need it.*/
    osg::ref_ptr<osg::Group> subtraction_wrapper = new osg::Group;
    osg::ref_ptr<osg::Geode> subtraction_geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> subtraction_geom = new osg::Geometry;
    subtraction_wrapper->setName(dp->d_namep);
    subtraction_geode->setName(dp->d_namep);
    subtraction_geom->setName(dp->d_namep);
    osg::StateSet* subtraction_geode_state = subtraction_geode->getOrCreateStateSet();
    subtraction_geode_state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    osg::StateSet* subtraction_state = subtraction_wrapper->getOrCreateStateSet();
    osg::ref_ptr<osg::LineStipple> ls = new osg::LineStipple();
    ls->setPattern(0xCF33);
    ls->setFactor(1);
    subtraction_state->setAttributeAndModes(ls.get());
    subtraction_geode->addDrawable(subtraction_geom);
    subtraction_wrapper->addChild(subtraction_geode);

    /*Search for all the solids (using full paths) below the region - that's the list of vlists we need for this particular region,
     *using the full paths below the current region to place them in their final relative positions.
     */
    const char *region_vlist_search = "! -type comb";
    struct bu_ptbl region_vlist_contributors = BU_PTBL_INIT_ZERO;
    (void)db_search(&region_vlist_contributors, DB_SEARCH_TREE, region_vlist_search, 1, &dp, dbip, NULL);
    int have_subtraction = 0;
    for (int j = (int)BU_PTBL_LEN(&region_vlist_contributors) - 1; j >= 0; j--) {
	struct db_full_path *curr_path = (struct db_full_path *)BU_PTBL_GET(&region_vlist_contributors, j);

	/* Find out how to draw this particular wireframe */
	int subtraction_path = subtracted_solid(curr_path);

	/* Get the final matrix we will need for this vlist */
	mat_t tm;
	MAT_IDN(tm);
	(void)db_path_to_mat(dbip, curr_path, tm, curr_path->fp_len-1, &rt_uniresource);

	/* Actually add the geometry from the vlist */
	struct bu_list *plot_segments = obj_vlist(DB_FULL_PATH_CUR_DIR(curr_path), dbip, tm);
	if (subtraction_path) {
	    have_subtraction++;
	    add_vlist_to_geom(subtraction_geom, plot_segments);
	} else {
	    add_vlist_to_geom(geom, plot_segments);
	}

	/* Cleanup */
	RT_FREE_VLIST(plot_segments);
	BU_PUT(plot_segments, struct bu_list);

    }
    db_search_free(&region_vlist_contributors);
    if (have_subtraction) {
	region->addChild(subtraction_wrapper);
    }

    return region;
}


/* In the case of regions, we need to be able to remove and restore the individual
 * comb children, because for normal display (as opposed to editing mode) we need to
 * use a single composite wireframe for regions */
osg::ref_ptr<osg::Group>
full_assembly_node(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
      	struct directory *dp, struct db_i *dbip)
{
    osg::ref_ptr<osg::Group> assembly = bare_comb_node(osg_nodes, dp, dbip);

    /* Clear the immediate children from this node, if it has any */
    unsigned int children_cnt = assembly->getNumChildren();
    assembly->removeChildren(0, children_cnt);

    const char *assembly_search = "-type comb ! -below -type region ! -type region";
    struct bu_ptbl assemblies = BU_PTBL_INIT_ZERO;
    (void)db_search(&assemblies, DB_SEARCH_RETURN_UNIQ_DP, assembly_search, 1, &dp, dbip, NULL);
    for (int i = (int)BU_PTBL_LEN(&assemblies) - 1; i >= 0; i--) {
	struct directory *curr_dp = (struct directory *)BU_PTBL_GET(&assemblies, i);
	osg::ref_ptr<osg::Group> sub_comb = bare_comb_node(osg_nodes, curr_dp, dbip);
	const char *comb_children_search = "-mindepth 1 -maxdepth 1";
	struct bu_ptbl comb_children = BU_PTBL_INIT_ZERO;
	(void)db_search(&comb_children, DB_SEARCH_TREE, comb_children_search, 1, &curr_dp, dbip, NULL);
	for (int j = (int)BU_PTBL_LEN(&comb_children) - 1; j >= 0; j--) {
	    struct db_full_path *curr_path = (struct db_full_path *)BU_PTBL_GET(&comb_children, j);
	    struct directory *curr_child_dp = DB_FULL_PATH_CUR_DIR(curr_path);
	    if (curr_child_dp->d_flags & RT_DIR_COMB && !(curr_child_dp->d_flags & RT_DIR_REGION)) {
		int is_assembly = 0;
		for (int k = (int)BU_PTBL_LEN(&assemblies) - 1; k >= 0; k--) {
		    struct directory *assembly_dp = (struct directory *)BU_PTBL_GET(&assemblies, k);
		    if (assembly_dp == curr_child_dp) is_assembly = 1;
		}
		osg::ref_ptr<osg::Group> new_node;
		if (is_assembly) {
		    new_node = bare_comb_node(osg_nodes, curr_child_dp, dbip);
		} else {
		    new_node = full_comb_node(osg_nodes, curr_child_dp, dbip);
		}
		(*osg_nodes)[DB_FULL_PATH_CUR_DIR(curr_path)] = new_node.get();
	    }
	    if (curr_child_dp->d_flags & RT_DIR_REGION) {
		osg::ref_ptr<osg::Group> new_node = full_region_node(osg_nodes, curr_child_dp, dbip);
		(*osg_nodes)[curr_child_dp] = new_node.get();
	    }
	    if (curr_child_dp->d_flags & RT_DIR_SOLID) {
		osg::ref_ptr<osg::Group> new_node = solid_node(osg_nodes, curr_child_dp, dbip);
		(*osg_nodes)[curr_child_dp] = new_node.get();
	    }
	    add_comb_child(sub_comb, (*osg_nodes)[curr_child_dp], curr_path, dbip);
	}
	db_search_free(&comb_children);
    }
    db_search_free(&assemblies);

    return assembly;
}


/* Determine what type of dp we're dealing with:
 *
 * -1 - not drawable
 *  0 - solid
 *  1 - comb
 *  2 - region
 *  3 - assembly (comb with regions below it)
 */
int
characterize_dp(struct directory *dp, struct db_i *dbip)
{
    int ret = -1;
    const char *is_assembly_search = "-type region";
    if (dp->d_flags & RT_DIR_COMB) {
	ret = 1;
	if (dp->d_flags & RT_DIR_REGION) {
	    ret = 2;
	} else {
	    /* TODO - db_search should let us know the results without needing the table,
	     * but that doesn't seem to be working... */
	    struct bu_ptbl search_results = BU_PTBL_INIT_ZERO;
	    (void)db_search(&search_results, DB_SEARCH_QUIET, is_assembly_search, 1, &dp, dbip, NULL);
	    if (BU_PTBL_LEN(&search_results) > 0) ret = 3;
	    db_search_free(&search_results);
	}
    } else {
	if (dp->d_flags & RT_DIR_SOLID) ret = 0;
    }

    return ret;
}

void
add_dp_to_scene(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	struct directory *dp, struct db_i *dbip)
{
    int dp_type = characterize_dp(dp, dbip);
    switch (dp_type) {
	case 0:
	    (void)solid_node(osg_nodes, dp, dbip);
	    break;
	case 1:
	    full_comb_node(osg_nodes, dp, dbip);
	    break;
	case 2:
	    full_region_node(osg_nodes, dp, dbip);
	    break;
	case 3:
	    full_assembly_node(osg_nodes, dp, dbip);
	    break;
    }
}


int
add_path_to_scene(
	std::map<const struct directory *, osg::ref_ptr<osg::Group> > *osg_nodes,
	const struct db_full_path *path, struct db_i *dbip)
{
    /* Whatever the path looks like, we're going to have to add the
     * current directory */
    struct db_full_path working_path;
    db_full_path_init(&working_path);
    db_dup_full_path(&working_path, path);

    add_dp_to_scene(osg_nodes, DB_FULL_PATH_CUR_DIR(&working_path), dbip);

    while (working_path.fp_len > 1) {
	struct directory *curr_dp = DB_FULL_PATH_CUR_DIR(&working_path);
	struct directory *parent_dp = working_path.fp_names[working_path.fp_len - 2];

	osg::ref_ptr<osg::Group> comb = bare_comb_node(osg_nodes, parent_dp, dbip);
	osg::ref_ptr<osg::Group> child = (*osg_nodes)[curr_dp];

	add_comb_child(comb, child, &working_path, dbip);

	DB_FULL_PATH_POP(&working_path);
    }

    db_free_full_path(&working_path);
    return 0;
}

int main( int argc, char **argv )
{
    std::map<const struct directory *, osg::ref_ptr<osg::Group> > osg_nodes;
    struct db_i *dbip = DBI_NULL;
    struct db_full_path path;

    bu_setprogname(argv[0]);

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

    /* argv[2] may be a full path, not just an object name - handle it */
    if (db_string_to_path(&path, dbip, argv[2])) {
	bu_exit(1, "ERROR: Unable to find object %s in %s\n", argv[2], argv[1]);
    }

    /* Need to initialize this for rt_obj_plot, which may call RT_ADD_VLIST */
    BU_LIST_INIT(&rtg_vlfree);

    add_path_to_scene(&(osg_nodes), &path, dbip);

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
    osg::ref_ptr<osg::Group> toplevel = new osg::Group;
    toplevel->addChild(osg_nodes[DB_FULL_PATH_ROOT_DIR(&path)]);
    optimizer.optimize(toplevel);
    viewer.setSceneData(toplevel);

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
