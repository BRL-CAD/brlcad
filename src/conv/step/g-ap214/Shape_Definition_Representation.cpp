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

#include "AP_Common.h"
#include "Shape_Definition_Representation.h"

/* Shape Definition Representation
 *
 */
STEPentity *
Add_Shape_Definition_Representation(struct directory *dp, AP203_Contents *sc, SdaiRepresentation *sdairep)
{
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();

    // SHAPE_DEFINITION_REPRESENTATION
    STEPentity *ret_entity = sc->registry->ObjCreate("SHAPE_DEFINITION_REPRESENTATION");
    sc->instance_list->Append(ret_entity, completeSE);
    SdaiShape_definition_representation *shape_def_rep = (SdaiShape_definition_representation *)ret_entity;
    shape_def_rep->used_representation_(sdairep);

    // PRODUCT_DEFINITION_SHAPE
    SdaiProduct_definition_shape *prod_def_shape = (SdaiProduct_definition_shape *)sc->registry->ObjCreate("PRODUCT_DEFINITION_SHAPE");
    sc->instance_list->Append((STEPentity *)prod_def_shape, completeSE);
    prod_def_shape->name_("''");
    prod_def_shape->description_("''");

    // PRODUCT_DEFINITION
    SdaiProduct_definition *prod_def = (SdaiProduct_definition *)sc->registry->ObjCreate("PRODUCT_DEFINITION");
    sc->instance_list->Append((STEPentity *)prod_def, completeSE);
    SdaiCharacterized_product_definition *char_def_prod = new SdaiCharacterized_product_definition(prod_def);
    SdaiCharacterized_definition *char_def= new SdaiCharacterized_definition(char_def_prod);
    prod_def_shape->definition_(char_def);
    prod_def->id_("''");
    prod_def->description_("''");

    // PRODUCT_DEFINITION_FORMATION
    SdaiProduct_definition_formation *prod_def_form = (SdaiProduct_definition_formation *)sc->registry->ObjCreate("PRODUCT_DEFINITION_FORMATION");
    sc->instance_list->Append((STEPentity *)prod_def_form, completeSE);
    prod_def->formation_(prod_def_form);
    prod_def_form->id_("''");
    prod_def_form->description_("''");

    // PRODUCT_RELATED_PRODUCT_CATEGORY
    SdaiProduct *prod_rel_prod_cat = (SdaiProduct *)sc->registry->ObjCreate("PRODUCT_RELATED_PRODUCT_CATEGORY");
    sc->instance_list->Append((STEPentity *)prod_rel_prod_cat, completeSE);
    prod_rel_prod_cat->id_("''");
    prod_rel_prod_cat->name_("''");
    prod_rel_prod_cat->description_("''");

    // PRODUCT
    SdaiProduct *prod = (SdaiProduct *)sc->registry->ObjCreate("PRODUCT");
    sc->instance_list->Append((STEPentity *)prod, completeSE);
    prod_def_form->of_product_(prod);
    prod->name_(str.c_str());
    prod->description_("''");
    prod->id_(str.c_str());

    // PRODUCT_DEFINITION_CONTEXT
    SdaiProduct_definition_context *prod_def_context = (SdaiProduct_definition_context *)sc->registry->ObjCreate("PRODUCT_DEFINITION_CONTEXT");
    sc->instance_list->Append((STEPentity *)prod_def_context, completeSE);
    prod_def->frame_of_reference_(prod_def_context);
    prod_def_context->life_cycle_stage_("'part definition'");

    // PRODUCT_CONTEXT
    SdaiProduct_context *product_context = (SdaiProduct_context *)sc->registry->ObjCreate("PRODUCT_CONTEXT");
    sc->instance_list->Append((STEPentity *)product_context, completeSE);
    product_context->name_("''");

    EntityAggregate *context_agg = new EntityAggregate();
    context_agg->AddNode(new EntityNode((SDAI_Application_instance *) product_context));
    prod->frame_of_reference_(context_agg);

    // APPLICATION_CONTEXT
    prod_def_context->frame_of_reference_(sc->application_context);
    product_context->frame_of_reference_(sc->application_context);

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
