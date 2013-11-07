/*                    O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/osg.cpp
 *
 * An interface to OSG.
 *
 */
/** @} */

#include "common.h"

#include "ged.h"
#include "solid.h"
#include <assert.h>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/RenderInfo>
#include <osgViewer/Viewer>
#include <osgUtil/Optimizer>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>


__BEGIN_DECLS
void ged_osgLoadScene(struct bu_list *hdlp, void *osgData);
__END_DECLS


struct osg_stuff {
    osg::ref_ptr<osgViewer::Viewer>	viewer;
    double left, right, bottom, top, near, far;
    int prev_pflag;
};

HIDDEN void
_osgLoadHiddenSolid(osg::Geode *geode, struct solid *sp)
{
    register struct bn_vlist *vp = (struct bn_vlist *)&sp->s_vlist;
    osg::Vec3dArray* vertices;
}


HIDDEN void
_osgLoadSolid(osg::Geode *geode, osg::Geometry *geom, osg::Vec3dArray *vertices, osg::Vec3dArray *normals, struct solid *sp)
{
    struct bn_vlist *tvp;
    int first;
#if 0
    int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
#endif
    register struct bn_vlist *vp = (struct bn_vlist *)&sp->s_vlist;
    int begin;
    int nverts;

    bu_log("ged_osgLoadSolid: enter\n");

#if 0
    if (line_style != sp->s_soldash) {
	line_style = sp->s_soldash;
	DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
    }

    if (sp->s_iflag == UP)
	DM_SET_FGCOLOR(dmp, 255, 255, 255, 0, sp->s_transparency);
    else
	DM_SET_FGCOLOR(dmp,
		       (unsigned char)sp->s_color[0],
		       (unsigned char)sp->s_color[1],
		       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

    if (sp->s_hiddenLine) {
	DM_DRAW_VLIST_HIDDEN_LINE(dmp, (struct bn_vlist *)&sp->s_vlist);
    } else {
	DM_DRAW_VLIST(dmp, (struct bn_vlist *)&sp->s_vlist);
    }
#endif


    /* Viewing region is from -1.0 to +1.0 */
    begin = 0;
    nverts = 0;
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
#if 0
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (dmp->dm_transparency)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(*pt);
#else
		    /* Move, start line */
		    if (first == 0) {
			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));
			//geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,0,vertices->size()));

			bu_log("Add linestrip: begin - %d, nverts - %d\n", begin, nverts);

			// add the points geometry to the geode.
			//geode->addDrawable(geom);
		    } else
			first = 0;

		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    normals->push_back(osg::Vec3(0.0f,-1.0f,0.0f));
		    begin += nverts;
		    nverts = 1;
		    //bu_log("ged_osgLoadSolid: loaded point - (%lf %lf %lf)\n", (*pt)[X], (*pt)[Y], (*pt)[Z]);
#endif
		    break;
		case BN_VLIST_POLY_START:
#if 0
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

			switch (dmp->dm_light) {
			case 1:
			    break;
			case 2:
			    glMaterialfv(GL_BACK, GL_DIFFUSE, diffuseColor);
			    break;
			case 3:
			    glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorDark);
			    break;
			default:
			    glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorLight);
			    break;
			}

			if (dmp->dm_transparency)
			    glEnable(GL_BLEND);
		    }

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(*pt);
#endif
		    normals->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    begin += nverts;
		    nverts = 0;

		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    ++nverts;

		    //bu_log("ged_osgLoadSolid: loaded point - (%lf %lf %lf)\n", (*pt)[X], (*pt)[Y], (*pt)[Z]);
		    break;
		case BN_VLIST_POLY_END:
#if 0
		    /* Draw, End Polygon */
		    glVertex3dv(*pt);
		    glEnd();
#endif
		    //vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    //++nverts;
		    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON,begin,nverts));
		    first = 1;

		    bu_log("Add polygon: begin - %d, nverts - %d\n", begin, nverts);

		    break;
		case BN_VLIST_POLY_VERTNORM:
#if 0
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(*pt);
#endif
		    break;
	    }
	}
    }

    if (first == 0) {
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));
	//geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,0,vertices->size()));

	bu_log("Add linestrip: begin - %d, nverts - %d\n", begin, nverts);

	// add the points geometry to the geode.
	//geode->addDrawable(geom);
    }

    bu_log("ged_osgLoadSolid: leave\n");
}


void
ged_osgLoadScene(struct bu_list *hdlp, void *osgData)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    struct solid *sp;
    struct osg_stuff *osp = (struct osg_stuff *)osgData;
#if 0
    int line_style = -1;
#endif

    //bu_log("ged_osgLoadScene: do nothing!\n");
    //return;

#if 0
    bu_log("ged_osgLoadScene: part A\n");
    osp->viewer->setSceneData(osgDB::readNodeFile("/home/bparker/brlcad/bin/a10_skin.osg"));
#else
    bu_log("ged_osgLoadScene: part B\n");
    osg::Group* root = new osg::Group();

    // create the Geode (Geometry Node) to contain all our osg::Geometry objects.
    osg::Geode* geode = new osg::Geode();

    bu_log("before: max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());
#if 1
    bu_log("ged_osgLoadScene: enter\n");
    gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    if (sp->s_hiddenLine) {
		_osgLoadHiddenSolid(geode, sp);
	    } else {
		osg::Geometry* geom = new osg::Geometry();
		osg::Vec3dArray* vertices = new osg::Vec3dArray;
		osg::Vec3dArray* normals = new osg::Vec3dArray;
		_osgLoadSolid(geode, geom, vertices, normals, sp);
		geom->setVertexArray(vertices);
		geom->setNormalArray(normals);
		geom->setNormalBinding(osg::Geometry::BIND_PER_PRIMITIVE_SET);
		//osg::RenderInfo ri(osp->viewer->getCamera()->getGraphicsContext()->getState(), osp->viewer->getCamera()->getView());
		//geom->compileGLObjects(ri);
		geom->setUseDisplayList(true);
		geode->addDrawable(geom);
	    }
	}

	gdlp = next_gdlp;
    }
#endif

#if 1
    //osp->viewer->setSceneData(osgDB::readNodeFile("/home/bparker/brlcad/bin/a10_skin.osg"));
    root->addChild(geode);
    osp->viewer->setSceneData(root);
#else
    // Made no difference
    //osgUtil::Optimizer optimizer;
    //optimizer.optimize(geode);

    osp->viewer->setSceneData(geode);
#endif
    bu_log("after: max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());

//    if (!osgDB::writeNodeFile(*geode, "a10_skin.osg"))
//	bu_log("ged_osgLoadScene: failed to write geode to a10.osg\n");

    bu_log("ged_osgLoadScene: loaded geode\n");
#endif
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
