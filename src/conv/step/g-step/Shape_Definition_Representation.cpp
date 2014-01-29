/* S H A P E _ D E F I N I T I O N _ R E P R E S E N T A T I O N . C P P
 *
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file ON_Brep.cpp
 *
 * File for writing out a Shape Definition Representation
 *
 */

#include "AP203.h"
#include "Shape_Definition_Representation.h"

/* Shape Definition Representation
 *
 * SHAPE_DEFINITION_REPRESENTATION (SdaiShape_definition_representation -> SdaiProperty_definition_representation)
 * PRODUCT_DEFINITION_SHAPE (SdaiProduct_definition_shape -> SdaiProperty_definition)
 * PRODUCT_DEFINITION (SdaiProduct_definition)
 * PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE (SdaiProduct_definition_formation_with_specified_source -> SdaiProduct_definition_formation) Can we just use PRODUCT_DEFINITION_FORMATION here?
 * PRODUCT (SdaiProduct)
 * MECHANICAL_CONTEXT (SdaiMechanical_context -> SdaiProduct_context -> SdaiApplication_context_element)
 * APPLICATION_CONTEXT (SdaiApplication_context)
 * DESIGN_CONTEXT (SdaiDesign_context -> SdaiProduct_definition_context -> SdaiApplication_context_element)
 *
 */
STEPentity *
Add_Shape_Definition_Representation(Registry *registry, InstMgr *instance_list,
	SdaiRepresentation *sdairep)
{
    // SHAPE_DEFINITION_REPRESENTATION
    STEPentity *ret_entity = registry->ObjCreate("SHAPE_DEFINITION_REPRESENTATION");
    instance_list->Append(ret_entity, completeSE);
    SdaiShape_definition_representation *shape_def_rep = (SdaiShape_definition_representation *)ret_entity;
    shape_def_rep->used_representation_(sdairep);

    // PRODUCT_DEFINITION_SHAPE
    SdaiProduct_definition_shape *prod_def_shape = (SdaiProduct_definition_shape *)registry->ObjCreate("PRODUCT_DEFINITION_SHAPE");
    instance_list->Append((STEPentity *)prod_def_shape, completeSE);
    prod_def_shape->name_("''");
    prod_def_shape->description_("''");
    shape_def_rep->definition_(prod_def_shape);

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)registry->ObjCreate("PRODUCT_DEFINITION");
    instance_list->Append((STEPentity *)prod_def, completeSE);
    SdaiCharacterized_product_definition *char_def_prod = new SdaiCharacterized_product_definition(prod_def);
    SdaiCharacterized_definition *char_def= new SdaiCharacterized_definition(char_def_prod);
    prod_def_shape->definition_(char_def);
    prod_def->id_("''");
    prod_def->description_("''");

    // PRODUCT_DEFINITION_FORMATION
    SdaiProduct_definition_formation *prod_def_form = (SdaiProduct_definition_formation *)registry->ObjCreate("PRODUCT_DEFINITION_FORMATION");
    instance_list->Append((STEPentity *)prod_def_form, completeSE);
    prod_def->formation_(prod_def_form);
    prod_def_form->id_("''");
    prod_def_form->description_("''");

    // PRODUCT
    SdaiProduct *prod = (SdaiProduct *)registry->ObjCreate("PRODUCT");
    instance_list->Append((STEPentity *)prod, completeSE);
    prod_def_form->of_product_(prod);
    prod->id_("''");
    prod->name_("''");
    prod->description_("''");

    // MECHANICAL_CONTEXT
    SdaiMechanical_context *mech_context = (SdaiMechanical_context *)registry->ObjCreate("MECHANICAL_CONTEXT");
    instance_list->Append((STEPentity *)mech_context, completeSE);
    prod->frame_of_reference_()->AddNode(new EntityNode((SDAI_Application_instance *)mech_context));
    mech_context->name_("''");
    mech_context->discipline_type_("''");

    // APPLICATION_CONTEXT
    SdaiApplication_context *app_context = (SdaiApplication_context *)registry->ObjCreate("APPLICATION_CONTEXT");
    instance_list->Append((STEPentity *)app_context, completeSE);
    mech_context->frame_of_reference_(app_context);
    app_context->application_("''");

    // DESIGN_CONTEXT
    SdaiDesign_context *design_context = (SdaiDesign_context *)registry->ObjCreate("DESIGN_CONTEXT");
    instance_list->Append((STEPentity *)design_context, completeSE);
    prod_def->frame_of_reference_(design_context);
    design_context->name_("''");
    design_context->life_cycle_stage_("'Design'");
    design_context->frame_of_reference_(app_context);

    //return ret_entity;
    // The product definition is what is used to define assemblies, so return that
    return (STEPentity *)prod_def;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
