/*                 S T E P W R A P P E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2025 United States Government as represented by
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
/** @file step/STEPWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */

#include "common.h"
#include <cctype>
#include <algorithm>

#include <iostream>
#include <fstream>

/* interface header */
#include "./STEPWrapper.h"
#include "Factory.h"

/* implementation headers */
#include "AdvancedBrepShapeRepresentation.h"
#include "BSplineSurfaceWithKnots.h"
#include "GeometricallyBoundedSurfaceShapeRepresentation.h"
#include "GeometricSet.h"
#include "GeometricSetSelect.h"
#include "Axis2Placement3D.h"
#include "CartesianPoint.h"
#include "SurfacePatch.h"
#include "LocalUnits.h"
#include "ManifoldSurfaceShapeRepresentation.h"
#include "ContextDependentShapeRepresentation.h"
#include "ProductDefinition.h"
#include "ShapeDefinitionRepresentation.h"
#include "ShapeRepresentationRelationship.h"
#include "ShellBasedSurfaceModel.h"

STEPWrapper::STEPWrapper()
    : registry(NULL), sfile(NULL), dotg(NULL), verbose(false)
{
    int ownsInstanceMemory = 1;
    instance_list = new InstMgr(ownsInstanceMemory);
}


STEPWrapper::~STEPWrapper()
{
    delete instance_list;
    delete sfile;
    delete registry;
    dotg = NULL;
}

int
convert_WriteBrep(
	AdvancedBrepShapeRepresentation *aBrep,
	BRLCADWrapper *dot_g,
	std::string *pname,
	std::string *name,
	int dry_run)
{
    if (dry_run) return 0;
    LocalUnits::length = aBrep->GetLengthConversionFactor();
    LocalUnits::planeangle = aBrep->GetPlaneAngleConversionFactor();
    LocalUnits::solidangle = aBrep->GetSolidAngleConversionFactor();
    ON_Brep *onBrep = aBrep->GetONBrep();
    if (!onBrep) {
	return 1;
    } else {
	ON_TextLog tl;

	if (!onBrep->IsValid(&tl)) {
	    bu_log("WARNING: %s is not valid\n", name->c_str());
	}

	//onBrep->SpSplitClosedFaces();
	//ON_Brep *tbrep = TightenBrep(onBrep);

	mat_t mat;
	MAT_IDN(mat);

	Axis2Placement3D *axis = aBrep->GetAxis2Placement3d();
	if (axis != NULL) {
	    //assign matrix values
	    double translate_to[3];
	    const double *toXaxis = axis->GetXAxis();
	    const double *toYaxis = axis->GetYAxis();
	    const double *toZaxis = axis->GetZAxis();
	    mat_t rot_mat;

	    VMOVE(translate_to,axis->GetOrigin());
	    VSCALE(translate_to,translate_to,LocalUnits::length);

	    MAT_IDN(rot_mat);
	    VMOVE(&rot_mat[0], toXaxis);
	    VMOVE(&rot_mat[4], toYaxis);
	    VMOVE(&rot_mat[8], toZaxis);
	    bn_mat_inv(mat, rot_mat);
	    MAT_DELTAS_VEC(mat, translate_to);
	}

	dot_g->WriteBrep(*pname, onBrep,mat);

	delete onBrep;

	return 0;
    }
}

int
convert_WritePlateBrep(
	ShellBasedSurfaceModel *sBrep,
	BRLCADWrapper *dot_g,
	std::string *name,
	int dry_run)
{
    if (dry_run) return 0;
    ON_Brep *onBrep = sBrep->GetONBrep();
    if (!onBrep) {
	return 1;
    } else {
	ON_TextLog tl;

	if (!onBrep->IsValid(&tl)) {
	    bu_log("WARNING: %s is not valid\n", name->c_str());
	}

	mat_t mat;
	MAT_IDN(mat);

#if 0
	// TODO - manifold surface container has an axis...
	Axis2Placement3D *axis = aBrep->GetAxis2Placement3d();
	if (axis != NULL) {
	    //assign matrix values
	    double translate_to[3];
	    const double *toXaxis = axis->GetXAxis();
	    const double *toYaxis = axis->GetYAxis();
	    const double *toZaxis = axis->GetZAxis();
	    mat_t rot_mat;

	    VMOVE(translate_to,axis->GetOrigin());
	    VSCALE(translate_to,translate_to,LocalUnits::length);

	    MAT_IDN(rot_mat);
	    VMOVE(&rot_mat[0], toXaxis);
	    VMOVE(&rot_mat[4], toYaxis);
	    VMOVE(&rot_mat[8], toZaxis);
	    bn_mat_inv(mat, rot_mat);
	    MAT_DELTAS_VEC(mat, translate_to);
	}
#endif
	dot_g->WriteBrep(*name, onBrep, mat);

	delete onBrep;

	return 0;
    }
}

int
convert_WriteBSpline(
	BSplineSurfaceWithKnots *sB,
	BRLCADWrapper *dot_g,
	std::string *name,
	int dry_run)
{
    if (dry_run) return 0;
    ON_Brep *onBrep = sB->GetONBrep();
    if (!onBrep) {
	return 1;
    } else {
	ON_TextLog tl;

	if (!onBrep->IsValid(&tl)) {
	    bu_log("WARNING: %s is not valid\n", name->c_str());
	}

	mat_t mat;
	MAT_IDN(mat);

#if 0
	// TODO - manifold surface container has an axis...
	Axis2Placement3D *axis = aBrep->GetAxis2Placement3d();
	if (axis != NULL) {
	    //assign matrix values
	    double translate_to[3];
	    const double *toXaxis = axis->GetXAxis();
	    const double *toYaxis = axis->GetYAxis();
	    const double *toZaxis = axis->GetZAxis();
	    mat_t rot_mat;

	    VMOVE(translate_to,axis->GetOrigin());
	    VSCALE(translate_to,translate_to,LocalUnits::length);

	    MAT_IDN(rot_mat);
	    VMOVE(&rot_mat[0], toXaxis);
	    VMOVE(&rot_mat[4], toYaxis);
	    VMOVE(&rot_mat[8], toZaxis);
	    bn_mat_inv(mat, rot_mat);
	    MAT_DELTAS_VEC(mat, translate_to);
	}
#endif
	dot_g->WriteBrep(*name, onBrep, mat);

	delete onBrep;

	return 0;
    }
}

bool STEPWrapper::convert(BRLCADWrapper *dot_g)
{
    MAP_OF_PRODUCT_NAME_TO_ENTITY_ID name2id_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME id2name_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID id2productid_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID process_map;

    if (!dot_g) {
	return false;
    }

    this->dotg = dot_g;

    int num_ents = instance_list->InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))) {
	    ShapeDefinitionRepresentation *sdr = dynamic_cast<ShapeDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (!sdr) {
		bu_exit(1, "ERROR: unable to allocate a 'ShapeDefinitionRepresentation' entity\n");
	    } else {
		int sdr_id = sdr->GetId();
		std::string pname  = sdr->GetProductName();
		pname = dotg->CleanBRLCADName(pname);
		int product_id = sdr->GetProductId();

		id2productid_map[sdr_id] = product_id;

		AdvancedBrepShapeRepresentation *aBrep = sdr->GetAdvancedBrepShapeRepresentation();
		if (aBrep) {
		    if (pname.empty() || (pname.compare("''") == 0)) {
			std::string str = "Product@";
			pname = dotg->GetBRLCADName(str);
			id2name_map[aBrep->GetId()] = pname;
			id2name_map[product_id] = pname;
		    } else {
			id2name_map[aBrep->GetId()] = pname;
			id2name_map[product_id] = pname;
		    }
		    id2productid_map[aBrep->GetId()] = product_id;
		    /* This length is used in the hierarchy build - this is how
		     * it was getting set when the Brep build came before the
		     * hierarchy build, so leave it for now, but should there be
		     * a look-up in the hierarchy build instead of here?*/
		    LocalUnits::length = aBrep->GetLengthConversionFactor();

		} else { // must be an assembly
		    if (pname.empty() || (pname.compare("''") == 0)) {
			std::string str = "Assembly@";
			pname = dotg->GetBRLCADName(str);
		    }
		    ShapeRepresentation *aSR = sdr->GetShapeRepresentation();
		    if (aSR) {
			int sr_id = aSR->GetId();
			id2name_map[sr_id] = pname;
			id2name_map[product_id] = pname;
			id2productid_map[sr_id] = product_id;
		    }
		}
		Factory::DeleteObjects();
	    }
	}

	// Manifold Surface representations define a group of shells
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_manifold_surface_shape_representation))) {
	    ShapeRepresentation *sr = dynamic_cast<ShapeRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!sr) {
		bu_exit(1, "ERROR: unable to allocate a 'ShapeRepresentation' entity\n");
	    }
	    int id = sr->GetId();
	    std::string pname  = sr->Name();
	    pname = dotg->CleanBRLCADName(pname);
	    std::cout << "pname(" << id << "): " << pname << "\n";
	    if (pname.empty() || (pname.compare("''") == 0)) {
		std::string str = "ManifoldSurfaces@";
		pname = dotg->GetBRLCADName(str);
	    }
	    id2name_map[id] = pname;
	    // Find out which shell(s) are part of this manifold and add them to the map
	    LIST_OF_REPRESENTATION_ITEMS *items = sr->items_();
	    LIST_OF_REPRESENTATION_ITEMS::iterator ii;
	    for (ii = items->begin(); ii != items->end(); ++ii) {
		ShellBasedSurfaceModel *sm = dynamic_cast<ShellBasedSurfaceModel *>(*ii);
		if (sm != NULL) {
		    int iid = (*ii)->GetId();
		    std::cout << "iid: " << iid << "\n";
		    id2productid_map[iid] = id;
		}
	    }
	}

	// Geometric Sets define collections of surfaces
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_geometric_set))) {
	    GeometricSet *gs = dynamic_cast<GeometricSet*>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!gs) {
		bu_exit(1, "ERROR: unable to allocate a 'GeometricSet' entity\n");
	    }
	    int id = gs->GetId();
	    std::string pname  = gs->Name();
	    pname = dotg->CleanBRLCADName(pname);
	    std::cout << "pname(" << id << "): " << pname << "\n";
	    if (pname.empty() || (pname.compare("''") == 0)) {
		std::string str = "GeometricSet@";
		pname = dotg->GetBRLCADName(str);
	    }
	    id2name_map[id] = pname;
	}
    }
    /*
     * Pickup BREP related to SHAPE_REPRESENTATION through SHAPE_REPRESENTATION_RELATIONSHIP
     *
     * like the following found in OpenBook Part 'C':
     *    #21281=SHAPE_DEFINITION_REPRESENTATION(#21280,#21270);
     *        #21280=PRODUCT_DEFINITION_SHAPE('','SHAPE FOR C.',#21279);
     *            #21279=PRODUCT_DEFINITION('design','',#21278,#21275);
     *                #21278=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#21277,.MADE.);
     *                    #21277=PRODUCT('C','C','NOT SPECIFIED',(#21276));
     *        #21270=SHAPE_REPRESENTATION('',(#21259),#21267);
     *            #21259=AXIS2_PLACEMENT_3D('DANTE_BX_CPU_TOP_1',#21256,#21257,#21258);
     *            #21267=(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#21266))
     *                GLOBAL_UNIT_ASSIGNED_CONTEXT((#21260,#21264,#21265))REPRESENTATION_CONTEXT('ID1','3'));
     *
     *    #21271=SHAPE_REPRESENTATION_RELATIONSHIP('','',#21270,#21268);
     *        #21268=ADVANCED_BREP_SHAPE_REPRESENTATION('',(#21254),#21267);
     *    #21272=SHAPE_REPRESENTATION_RELATIONSHIP('','',#21270,#21269);
     *        #21269=MANIFOLD_SURFACE_SHAPE_REPRESENTATION('',(#21255),#21267);
     *
     */
    for (int i = 0; i < num_ents; i++) {
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
	    ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (srr) {
		ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());

		// First thing to try - Brep
		AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		if (!aBrep) { //try rep_1
		    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if ((aSR) && (aBrep)) {
		    int sr_id = aSR->GetId();
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(sr_id);
		    if (it != id2productid_map.end()) { // product found
			int product_id = (*it).second;
			int brep_id = aBrep->GetId();

			it = id2productid_map.find(brep_id);
			if (it == id2productid_map.end()) { // brep not loaded yet so lets do that here.
			    string pname = id2name_map[brep_id];
			    if (pname.empty() || (pname.compare("''") == 0)) {
				std::string str = "Brep_@";
				pname = dotg->GetBRLCADName(str);
				id2name_map[aBrep->GetId()] = pname;
			    } else {
				id2name_map[aBrep->GetId()] = pname;
			    }
			    id2productid_map[brep_id] = product_id;
			    /* This length is used in the hierarchy build - this is how
			     * it was getting set when the Brep build came before the
			     * hierarchy build, so leave it for now, but should there be
			     * a look-up in the hierarchy build instead of here?*/
			    LocalUnits::length = aBrep->GetLengthConversionFactor();

			    if (product_id != brep_id) {
				mat_t mat;

				MAT_IDN(mat);
				string comb = id2name_map[product_id];
				if (!dry_run)
				    dotg->AddMember(comb,pname,mat);
			    }
			}
		    }
		    Factory::DeleteObjects();
		    continue;
		}

		// If not a Brep, try a bounded surface shape
		// NOTE:  The modeling intent may be for the set of surfaces to
		// bound a closed volume, but the topological information
		// defining the BRep doesn't appear to be encoded explicitly in
		// this representation.  We will import it as plate mode
		// surfaces under a comb - it will be up to some sort of
		// "welding" operation to deduce and construct the actual
		// OpenNURBS BRep from the surface set, if they do in fact
		// define such a closed volume.
		GeometricallyBoundedSurfaceShapeRepresentation *aBS = dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation*>(srr->GetRepresentationRelationshipRep_2());
		if (!aBS) { //try rep_1
		    aBS = dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation*>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if (aSR && aBS) {
		    mat_t mat;
		    MAT_IDN(mat);
		    //std::cout << "GeometricallyBoundedSurfaceShapeRepresentation\n";
		    LIST_OF_REPRESENTATION_ITEMS *items = aBS->items_();
		    LIST_OF_REPRESENTATION_ITEMS::iterator ii;
		    for (ii = items->begin(); ii != items->end(); ++ii) {
			GeometricSet *gs = dynamic_cast<GeometricSet*>(*ii);
			string comb = id2name_map[gs->GetId()];
			dotg->AddMember(std::string("shells"), comb, mat);
			LIST_OF_GEOMETRIC_SET_SELECT *sitems = gs->GetElements();
			LIST_OF_GEOMETRIC_SET_SELECT::iterator sii;
			for (sii = sitems->begin(); sii != sitems->end(); ++sii) {
			    BSplineSurfaceWithKnots *ks = dynamic_cast<BSplineSurfaceWithKnots*>((*sii)->GetSurfaceElement());
			    if (ks) {
				int kd = ks->GetId();
				std::string kname  = ks->Name() + std::string("_") + std::to_string(kd);
				kname = dotg->CleanBRLCADName(kname);
				if (!dry_run)
				    dotg->AddMember(comb,kname,mat);

				convert_WriteBSpline(ks, dotg, &kname, dry_run);
			    }
			}
		    }

		    Factory::DeleteObjects();
		    continue;
		}

		Factory::DeleteObjects();

	    }
	}
    }

    for (int i = 0; i < num_ents; i++) {
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}

	// Find plate mode Brep objects through e_shell_based_surface_model
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shell_based_surface_model))) {
	    ShellBasedSurfaceModel *gr = dynamic_cast<ShellBasedSurfaceModel *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!gr) {
		bu_exit(1, "ERROR: unable to allocate a 'ShellBasedSurfaceModel' entity\n");
	    }

	    int id = gr->GetId();
	    std::string pname  = gr->Name();
	    pname = dotg->CleanBRLCADName(pname);
	    if (pname.empty() || (pname.compare("''") == 0)) {
		std::string str = "Brep_@";
		pname = dotg->GetBRLCADName(str);
		id2name_map[id] = pname;
	    } else {
		id2name_map[id] = pname;
	    }
	    std::cout << "\n" << pname << "(" << id << "): shell based surface model\n";


	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(id);
	    if (it != id2productid_map.end()) { // product found
		int product_id = (*it).second;
		mat_t mat;
		MAT_IDN(mat);
		string comb = id2name_map[product_id];
		if (!dry_run)
		    dotg->AddMember(comb,pname,mat);
	    }

	    Factory::DeleteObjects();
	}


	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
	    ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (srr) {
		ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		if (!aBrep) { //try rep_1
		    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if ((aSR) && (aBrep)) {
		    int sr_id = aSR->GetId();
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(sr_id);
		    if (it != id2productid_map.end()) { // product found
			int product_id = (*it).second;
			int brep_id = aBrep->GetId();

			it = id2productid_map.find(brep_id);
			if (it == id2productid_map.end()) { // brep not loaded yet so lets do that here.
			    string pname = id2name_map[brep_id];
			    if (pname.empty() || (pname.compare("''") == 0)) {
				std::string str = "Brep_@";
				pname = dotg->GetBRLCADName(str);
				id2name_map[aBrep->GetId()] = pname;
			    } else {
				id2name_map[aBrep->GetId()] = pname;
			    }
			    id2productid_map[brep_id] = product_id;
			    /* This length is used in the hierarchy build - this is how
			     * it was getting set when the Brep build came before the
			     * hierarchy build, so leave it for now, but should there be
			     * a look-up in the hierarchy build instead of here?*/
			    LocalUnits::length = aBrep->GetLengthConversionFactor();

			    if (product_id != brep_id) {
				mat_t mat;

				MAT_IDN(mat);
				string comb = id2name_map[product_id];
				if (!dry_run)
				    dotg->AddMember(comb,pname,mat);
			    }
			}
		    }
		}
		Factory::DeleteObjects();
	    }
	}
    }


    if (Verbose()) {
	std::cerr << std::endl << "     Generating BRL-CAD hierarchy." << std::endl;
    }

    for (int i = 0; i < num_ents; i++) {
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_context_dependent_shape_representation))) {
	    ContextDependentShapeRepresentation *aCDSR = dynamic_cast<ContextDependentShapeRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (aCDSR && aCDSR->GetRepresentationRelationshipRep_1() && aCDSR->GetRepresentationRelationshipRep_2()) {
		int rep_1_id = aCDSR->GetRepresentationRelationshipRep_1()->GetId();
		int rep_2_id = aCDSR->GetRepresentationRelationshipRep_2()->GetId();
		int pid_1 = id2productid_map[rep_1_id];
		int pid_2 = id2productid_map[rep_2_id];
		Axis2Placement3D *axis1 = NULL;
		Axis2Placement3D *axis2 = NULL;
		if ((id2name_map.find(rep_1_id) != id2name_map.end()) && (id2name_map.find(rep_2_id) != id2name_map.end())) {
		    string comb = id2name_map[rep_1_id];
		    string member = id2name_map[rep_2_id];
		    mat_t mat;
		    MAT_IDN(mat);

		    ProductDefinition *relatingProduct = aCDSR->GetRelatingProductDefinition();
		    ProductDefinition *relatedProduct = aCDSR->GetRelatedProductDefinition();
		    if (relatingProduct && relatedProduct) {
			string relatingName = relatingProduct->GetProductName();
			int relatingID = relatingProduct->GetProductId();
			string relatedName = relatedProduct->GetProductName();
			int relatedID = relatedProduct->GetProductId();

			if ((relatingID == pid_1) && (relatedID == pid_2)) {
			    axis1 = aCDSR->GetTransformItem_1();
			    axis2 = aCDSR->GetTransformItem_2();
			    comb = id2name_map[rep_1_id];
			    member = id2name_map[rep_2_id];
			} else if ((relatingID == pid_2) && (relatedID == pid_1)) {
			    axis1 = aCDSR->GetTransformItem_2();
			    axis2 = aCDSR->GetTransformItem_1();
			    comb = id2name_map[rep_2_id];
			    member = id2name_map[rep_1_id];
			} else {
			    std::cerr << "Error: Found Representation Relationship Rep_1(name=" << comb << ",Id=" << rep_1_id << ")" << std::endl;
			    std::cerr << "Error: Found Representation Relationship Rep_2(name=" << member << ",Id=" << rep_2_id << ")" << std::endl;
			    std::cerr << "Error: but Relating ProductDefinition (name=" << relatingName << ",Id=" << relatingID << ")"  << std::endl;
			    std::cerr << "Error:     Related ProductDefinition (name=" << relatedName << ",Id=" << relatedID << ")"  << std::endl;
			}
		    }

		    if ((axis1 != NULL) && (axis2 != NULL)) {
			mat_t to_mat;
			mat_t from_mat;
			mat_t toinv_mat;

			//assign matrix values
			double translate_to[3];
			double translate_from[3];
			const double *toXaxis = axis1->GetXAxis();
			const double *toYaxis = axis1->GetYAxis();
			const double *toZaxis = axis1->GetZAxis();
			const double *fromXaxis = axis2->GetXAxis();
			const double *fromYaxis = axis2->GetYAxis();
			const double *fromZaxis = axis2->GetZAxis();
			VMOVE(translate_to,axis1->GetOrigin());
			VSCALE(translate_to,translate_to,LocalUnits::length);

			VMOVE(translate_from,axis2->GetOrigin());
			VSCALE(translate_from,translate_from,-LocalUnits::length);

			// undo from trans/rot
			MAT_IDN(from_mat);
			VMOVE(&from_mat[0], fromXaxis);
			VMOVE(&from_mat[4], fromYaxis);
			VMOVE(&from_mat[8], fromZaxis);
			MAT_DELTAS_VEC(from_mat, translate_from);

			// do to trans/rot
			MAT_IDN(to_mat);
			VMOVE(&to_mat[0], toXaxis);
			VMOVE(&to_mat[4], toYaxis);
			VMOVE(&to_mat[8], toZaxis);
			bn_mat_inv(toinv_mat, to_mat);
			MAT_DELTAS_VEC(toinv_mat, translate_to);

			bn_mat_mul(mat, toinv_mat, from_mat);
		    }
		    if (!dry_run)
			dotg->AddMember(comb,member,mat);
		}
		Factory::DeleteObjects();
	    }
	}
    }
    if (!dry_run) {
	if (!dotg->WriteCombs()) {
	    std::cerr << "Error writing BRL-CAD hierarchy." << std::endl;
	}
    }

    for (int i = 0; i < num_ents; i++) {
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	/* Plate mode solid */
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shell_based_surface_model))) {
	    ShellBasedSurfaceModel *gr = dynamic_cast<ShellBasedSurfaceModel *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (gr) {
		int id = gr->GetId();
		name = id2name_map[id];
		if (convert_WritePlateBrep(gr, dot_g, &name, dry_run)) {
		    delete gr;
		    bu_exit(1, "ERROR: failure creating shell based surface model from %s\n", stepfile.c_str());
		}
	    }
	}

	/* Shape Definition Representation */
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))) {
	    ShapeDefinitionRepresentation *sdr = dynamic_cast<ShapeDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!sdr) {
		bu_exit(1, "ERROR: unable to allocate a 'ShapeDefinitionRepresentation' entity\n");
	    } else {
		std::string pname  = sdr->GetProductName();
		pname = dotg->CleanBRLCADName(pname);

		AdvancedBrepShapeRepresentation *aBrep = sdr->GetAdvancedBrepShapeRepresentation();
		if (aBrep) {

		    if (Verbose()) {
			if (!pname.empty() && (pname.compare("''") != 0)) {
			    std::cerr << std::endl << "     Generating Product - " << pname ;
			} else {
			    std::cerr << std::endl << "     Generating Product";
			}
		    }

		    LocalUnits::length = aBrep->GetLengthConversionFactor();
		    if (convert_WriteBrep(aBrep, dot_g, &pname, &name, dry_run)) {
			delete sdr;
			bu_exit(1, "ERROR: failure creating advanced boundary representation from %s\n", stepfile.c_str());
		    }

		}
		Factory::DeleteObjects();
	    }
	}
    }
    for (int i = 0; i < num_ents; i++) {
	/* Shape Representation Relationship */
	SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);


	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
	    ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (srr) {
		ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		if (!aBrep) { //try rep_1
		    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if ((aSR) && (aBrep)) {
		    int sr_id = aSR->GetId();
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(sr_id);
		    if (it != id2productid_map.end()) { // product found
			int product_id = (*it).second;
			int brep_id = aBrep->GetId();

			it = process_map.find(brep_id);
			if (it == process_map.end()) { // brep not loaded yet so lets do that here.
			    string pname = id2name_map[brep_id];
			    if (Verbose()) {
				if (!pname.empty() && (pname.compare("''") != 0)) {
				    std::cerr << std::endl << "     Generating Product - " << pname ;
				} else {
				    std::cerr << std::endl << "     Generating Product";
				}
			    }

			    LocalUnits::length = aBrep->GetLengthConversionFactor();
			    if (convert_WriteBrep(aBrep, dotg, &pname, &name, dry_run)) {
				bu_exit(1, "ERROR: failure creating advanced boundary representation from %s\n", stepfile.c_str());
			    } else {
				process_map[brep_id] = product_id;
			    }
			}
		    }/**/
		}
		Factory::DeleteObjects();
	    }
	}
    }

    if (summary_log_file) {
	ofstream step_log;
	step_log.open(summary_log_file);
	for (int i = 0; i < num_ents; i++) {
	    SDAI_Application_instance *sse = instance_list->GetSTEPentity(i);
	    if (sse == NULL) {
		continue;
	    }
	    std::string pname = id2name_map[sse->StepFileId()];
	    if (!pname.empty() && (pname.compare("''") != 0) && pname.length()) {
		step_log << pname << ",";
	    } else {
		step_log << "''" << ",";
	    }
	    std::map<int, int>::iterator e_it = entity_status.find(sse->StepFileId());
	    if (e_it != entity_status.end()) {
		switch (e_it->second) {
		    case STEP_LOADED:
			step_log << sse->StepFileId() << "," << sse->EntityName() << ",SUCCESS\n";
			break;
		    case STEP_LOAD_ERROR:
			step_log << sse->StepFileId() << "," << sse->EntityName() << ",LOAD_ERROR\n";
			break;
		    default:
			step_log << sse->StepFileId() << "," << sse->EntityName() << ",UNKNOWN_STATUS\n";
			break;
		}
	    } else {
		step_log << sse->StepFileId() << "," << sse->EntityName() << ",NOT_PROCESSED\n";
	    }
	}
	step_log.close();
    }

    return true;
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid)
{
    return instance_list->FindFileId(STEPid)->GetSTEPentity();
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid, const char *name)
{
    SDAI_Application_instance *se = getEntity(STEPid);

    if (se->IsComplex()) {
	se = getSuperType(STEPid, name);
    }
    return se;
}


SDAI_Application_instance *
STEPWrapper::getEntity(SDAI_Application_instance *sse, const char *name)
{
    if (sse->IsComplex()) {
	sse = getSuperType(sse, name);
    }
    return sse;
}


STEPattribute *
STEPWrapper::getAttribute(int STEPid, const char *name)
{
    STEPattribute *retValue = NULL;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(int STEPid)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(int STEPid, const char *name)
{
    Boolean retValue = BUnset;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getEnumAttribute(int STEPid, const char *name)
{
    int retValue = 0;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (*attr->ptr.e).asInt();
	    break;
	}
    }
    return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(int STEPid, const char *name)
{
    Logical retValue = LUnknown;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


std::string
STEPWrapper::getLogicalString(Logical v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case LFalse:
	    retValue = "LFalse";
	    break;
	case LTrue:
	    retValue = "LTrue";
	    break;
	case LUnset:
	    retValue = "LUnset";
	    break;
	case LUnknown:
	    retValue = "LUnknown";
	    break;
    }
    return retValue;
}


std::string
STEPWrapper::getBooleanString(Boolean v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case BFalse:
	    retValue = "BFalse";
	    break;
	case BTrue:
	    retValue = "BTrue";
	    break;
	case BUnset:
	    retValue = "BUnset";
	    break;
    }
    return retValue;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(int STEPid, const char *name)
{
    SDAI_Application_instance *retValue = NULL;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (SDAI_Application_instance *)*attr->ptr.c;
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getIntegerAttribute(int STEPid, const char *name)
{
    int retValue = 0;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(int STEPid, const char *name)
{
    double retValue = 0.0;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(int STEPid, const char *name)
{
    LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();
    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

	    EntityNode *sn = (EntityNode *)sa->GetHead();
	    SDAI_Application_instance *se;
	    while (sn != NULL) {
		se = (SDAI_Application_instance *)sn->node;

		l->push_back(se);
		sn = (EntityNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(int STEPid, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();
    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(int STEPid)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m)
{
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = instance_list->FindFileId(STEPid)->GetSTEPentity();
    return getStringAttribute(sse, name);
}


STEPattribute *
STEPWrapper::getAttribute(SDAI_Application_instance *sse, const char *name)
{
    STEPattribute *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(SDAI_Application_instance *sse)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(SDAI_Application_instance *sse, const char *name)
{
    Boolean retValue = BUnset;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getEnumAttribute(SDAI_Application_instance *sse, const char *name)
{
    int retValue = 0;
    std::string attrval;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (*attr->ptr.e).asInt();
	    //std::cout << "debug enum: " << (*attr->ptr.e).asStr(attrval) << std::endl;
	    break;
	}
    }
    return retValue;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(SDAI_Application_instance *sse, const char *name)
{
    SDAI_Application_instance *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attr->Nullable() && attr->is_null() && !attr->IsDerived()) {
	    continue;
	}
	if (attrname.compare(name) == 0) {
	    std::string attrval;
	    //std::cout << "attr:" << name << ":" << attr->TypeName() << ":" << attr->Name() << std::endl;
	    //std::cout << "attr:" << attr->asStr(attrval) << std::endl;
	    retValue = (SDAI_Application_instance *)*attr->ptr.c;
	    break;
	}
    }
    return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(SDAI_Application_instance *sse, const char *name)
{
    Logical retValue = LUnknown;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getIntegerAttribute(SDAI_Application_instance *sse, const char *name)
{
    int retValue = 0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(SDAI_Application_instance *sse, const char *name)
{
    double retValue = 0.0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


SDAI_Select *
STEPWrapper::getSelectAttribute(SDAI_Application_instance *sse, const char *name)
{
    SDAI_Select *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (SDAI_Select *)attr->ptr.sh;
	    break;
	}
    }
    return retValue;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

	    EntityNode *sn = (EntityNode *)sa->GetHead();
	    SDAI_Application_instance *se;
	    while (sn != NULL) {
		se = (SDAI_Application_instance *)sn->node;

		l->push_back(se);
		sn = (EntityNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_SELECTS *
STEPWrapper::getListOfSelects(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_SELECTS *l = new LIST_OF_SELECTS;
    std::string attrval;

    sse->ResetAttributes();
    STEPattribute *attr;

    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {

	    SelectAggregate *sa = (SelectAggregate *)attr->ptr.a;
	    SelectNode *sn = (SelectNode *)sa->GetHead();
	    while (sn) {
		l->push_back(sn->node);
		sn = (SelectNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_PATCHES *
STEPWrapper::getListOfListOfPatches(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_PATCHES *l = new LIST_OF_LIST_OF_PATCHES;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_PATCHES *patches;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		patches = parseListOfPatchEntities(eaStr);
		l->push_back(patches);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(SDAI_Application_instance *sse)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(SDAI_Application_instance *sse, MAP_OF_SUPERTYPES &m)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(SDAI_Application_instance *sse, const char *name)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(SDAI_Application_instance *sse, const char *name)
{
    std::string retValue = "";

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    const char *str = attr->asStr(attrval);
	    if (str != NULL) {
		retValue = str;
	    }
	    break;
	}
    }
    return retValue;
}


bool
STEPWrapper::load(std::string &step_file)
{
    registry = new Registry(SchemaInit);
    sfile = new STEPfile(*registry, *instance_list);

    stepfile = step_file;
    try {
	/* load STEP file */
	sfile->ReadExchangeFile(stepfile.c_str());

    } catch (std::exception &e) {
	std::cerr << e.what() << std::endl;
	return false;
    }
    return true;

}


LIST_OF_REALS *
STEPWrapper::parseListOfReals(const char *in)
{
    LIST_OF_REALS *l = new LIST_OF_REALS;
    ErrorDescriptor errdesc;
    RealAggregate *ra = new RealAggregate();

    //ra->StrToVal(in, &errdesc, SDAI_Real, instance_list, 0);
    ra->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::t_parameter_value, instance_list, 0);
    RealNode *rn = (RealNode *)ra->GetHead();
    while (rn != NULL) {
	l->push_back(rn->value);
	rn = (RealNode *)rn->NextNode();
    }
    /*
      EntityNode *sn = (EntityNode *)ra->GetHead();

      SDAI_Application_instance *sse;
      while (sn != NULL) {
      sse = (SDAI_Application_instance *)sn->node;
      CartesianPoint *aCP = new CartesianPoint(this, sse->STEPfile_id);
      if (aCP->Load(this, sse)) {
      l->push_back(aCP);
      } else {
      std::cout << "Error loading Real list." << std::endl;
      }
      sn = (EntityNode *)sn->NextNode();
      }*/
    delete ra;

    return l;
}


LIST_OF_POINTS *
STEPWrapper::parseListOfPointEntities(const char *in)
{
    LIST_OF_POINTS *l = new LIST_OF_POINTS;
    ErrorDescriptor errdesc;
    EntityAggregate *ag = new EntityAggregate();

    ag->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::e_cartesian_point, instance_list, 0);
    EntityNode *sn = (EntityNode *)ag->GetHead();

    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;
	CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(Factory::CreateObject(this,sse));
	if (aCP != NULL) {
	    l->push_back(aCP);
	} else {
	    std::cout << "Error loading CartesianPoint." << std::endl;
	}
	sn = (EntityNode *)sn->NextNode();
    }
    delete ag;

    return l;
}


LIST_OF_PATCHES *
STEPWrapper::parseListOfPatchEntities(const char *in)
{
    LIST_OF_PATCHES *l = new LIST_OF_PATCHES;
    ErrorDescriptor errdesc;
    EntityAggregate *ag = new EntityAggregate();

    ag->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::e_cartesian_point, instance_list, 0);
    EntityNode *sn = (EntityNode *)ag->GetHead();

    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;
	SurfacePatch *aCP = dynamic_cast<SurfacePatch *>(Factory::CreateObject(this,sse));
	if (aCP != NULL) {
	    l->push_back(aCP);
	} else {
	    std::cout << "Error loading SurfacePatch." << std::endl;
	}
	sn = (EntityNode *)sn->NextNode();
    }
    delete ag;

    return l;
}


void
STEPWrapper::printEntity(SDAI_Application_instance *se, int level)
{
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Entity:" << se->EntityName() << "(" << se->STEPfile_id << ")" << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Description:" << se->eDesc->Description() << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Entity Type:" << se->eDesc->Type() << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Attributes:" << std::endl;

    STEPattribute *attr;
    se->ResetAttributes();
    while ((attr = se->NextAttribute()) != NULL) {
	std::string attrval;

	for (int i = 0; i <= level; i++) {
	    std::cout << "    ";
	}
	std::cout << attr->Name() << ": " << attr->asStr(attrval) << " TypeName: " << attr->TypeName() << " Type: " << attr->Type() << std::endl;
	if (attr->Type() == 256) {
	    if (attr->IsDerived()) {
		for (int i = 0; i <= level; i++) {
		    std::cout << "    ";
		}
		std::cout << "        ********* DERIVED *********" << std::endl;
	    } else {
		printEntity(*(attr->ptr.c), level + 2);
	    }
	} else if ((attr->Type() == SET_TYPE) || (attr->Type() == LIST_TYPE)) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);

	    // std::cout << "aggr:" << sa->asStr(attrval) << "  BaseType:" << attr->BaseType() << std::endl;

	    if (attr->BaseType() == ENTITY_TYPE) {
		printEntityAggregate(sa, level + 2);
	    }
	}

    }
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printEntityAggregate(STEPaggregate *sa, int level)
{
    std::string strVal;

    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Aggregate:" << sa->asStr(strVal) << std::endl;

    EntityNode *sn = (EntityNode *)sa->GetHead();
    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;

	if (((sse->eDesc->Type() == SET_TYPE) || (sse->eDesc->Type() == LIST_TYPE)) && (sse->eDesc->BaseType() == ENTITY_TYPE)) {
	    printEntityAggregate((STEPaggregate *)sse, level + 2);
	} else if (sse->eDesc->Type() == ENTITY_TYPE) {
	    printEntity(sse, level + 2);
	} else {
	    std::cout << "Instance Type not handled:" << std::endl;
	}
	//std::cout << "sn - " << sn->asStr(attrval) << std::endl;

	sn = (EntityNode *)sn->NextNode();
    }
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printLoadStatistics()
{
    int num_ents = instance_list->InstanceCount();
    int num_schma_ents = registry->GetEntityCnt();

    // "Reset" the Schema and Entity hash tables... this sets things up
    // so we can walk through the table using registry->NextEntity()

    registry->ResetSchemas();
    registry->ResetEntities();

    // Print out what schema we're running through.

    const SchemaDescriptor *schema = registry->NextSchema();

    // "Loop" through the schema, building one of each entity type.

    const EntityDescriptor *ent;   // needs to be declared const...
    std::string filler = ".....................................................................";
    std::cout << "Loaded " << num_ents << " instances from ";
    if (BU_STR_EQUAL(stepfile.c_str(), "-")) {
	std::cout << "standard input" << std::endl;
    } else {
	std::cout << "STEP file \"" << stepfile << "\"" << std::endl;
    }

    int numEntitiesUsed = 0;
    for (int i = 0; i < num_schma_ents; i++) {
	ent = registry->NextEntity();

	int entCount = instance_list->EntityKeywordCount(ent->Name());
	// fix below with boost string formatter when available
	if (entCount > 0) {
	    std::cout << "\t" << ent->Name() << filler.substr(0, filler.length() - ((std::string)ent->Name()).length()) << entCount << std::endl;
	    numEntitiesUsed++;
	}
    }
    std::cout << "Used " << numEntitiesUsed << " entities of the available " << num_schma_ents << " in schema \"" << schema->Name() << std::endl;
}


const char *
STEPWrapper::getBaseType(int type)
{
    const char *retValue = NULL;

    switch (type) {
	case sdaiINSTANCE:
	    retValue = "sdaiINSTANCE";
	    break;
	case sdaiSELECT: // The name of a select is never written DAS 1/31/97
	    retValue = "sdaiSELECT";
	    break;
	case sdaiNUMBER:
	    retValue = "sdaiNUMBER";
	    break;
	case sdaiREAL:
	    retValue = "sdaiREAL";
	    break;
	case sdaiINTEGER:
	    retValue = "sdaiINTEGER";
	    break;
	case sdaiSTRING:
	    retValue = "sdaiSTRING";
	    break;
	case sdaiBOOLEAN:
	    retValue = "sdaiBOOLEAN";
	    break;
	case sdaiLOGICAL:
	    retValue = "sdaiLOGICAL";
	    break;
	case sdaiBINARY:
	    retValue = "sdaiBINARY";
	    break;
	case sdaiENUMERATION:
	    retValue = "sdaiENUMERATION";
	    break;
	case sdaiAGGR:
	    retValue = "sdaiAGGR";
	    break;
	case ARRAY_TYPE:
	    retValue = "ARRAY_TYPE";
	    break;
	case BAG_TYPE:
	    retValue = "BAG_TYPE";
	    break;
	case SET_TYPE:
	    retValue = "SET_TYPE";
	    break;
	case LIST_TYPE:
	    retValue = "LIST_TYPE";
	    break;
	case REFERENCE_TYPE: // this should never happen? DAS
	    retValue = "REFERENCE_TYPE";
	    break;
	default:
	    retValue = "Unknown";
	    break;
    }
    return retValue;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
