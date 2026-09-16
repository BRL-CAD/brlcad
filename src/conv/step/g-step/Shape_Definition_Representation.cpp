/* S H A P E _ D E F I N I T I O N _ R E P R E S E N T A T I O N . C P P
 *
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
#include "STEPGeneratedAPI.h"
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
Add_Shape_Definition_Representation(struct directory *dp, AP203_Contents *sc, STEPentity *sdairep)
{
    // SHAPE_DEFINITION_REPRESENTATION
    STEPentity *shape_def_rep = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "SHAPE_DEFINITION_REPRESENTATION");
    brlcad::step::SetEntity(shape_def_rep, "used_representation", sdairep);

    // PRODUCT_DEFINITION_SHAPE
    STEPentity *prod_def_shape = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "PRODUCT_DEFINITION_SHAPE");
    brlcad::step::SetString(prod_def_shape, "name", "");
    brlcad::step::SetString(prod_def_shape, "description", "");
    brlcad::step::SetEntity(shape_def_rep, "definition", prod_def_shape);

    // PRODUCT_DEFINITION
    STEPentity *prod_def = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "PRODUCT_DEFINITION");
    brlcad::step::SetEntity(prod_def_shape, "definition", prod_def);
    brlcad::step::SetString(prod_def, "id", "");
    brlcad::step::SetString(prod_def, "description", "");

    // PRODUCT_DEFINITION_FORMATION
#if defined(AP203)
    /* AP203 edition 1's subtype_mandatory_product_definition_formation
     * global rule requires the specified-source subtype.  A plain formation
     * is syntactically readable but is not a conforming AP203 instance. */
    STEPentity *prod_def_form = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list,
	    "PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE");
#else
    STEPentity *prod_def_form = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "PRODUCT_DEFINITION_FORMATION");
#endif
    brlcad::step::SetEntity(prod_def, "formation", prod_def_form);
    brlcad::step::SetString(prod_def_form, "id", "");
    brlcad::step::SetString(prod_def_form, "description", "");
#if defined(AP203)
    brlcad::step::SetEnum(prod_def_form, "make_or_buy", "MADE");
#endif

    // PRODUCT
    STEPentity *prod = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "PRODUCT");
    brlcad::step::SetEntity(prod_def_form, "of_product", prod);
    brlcad::step::SetString(prod, "name", dp->d_namep);
    brlcad::step::SetString(prod, "description", "");
    brlcad::step::SetString(prod, "id", dp->d_namep);

    // MECHANICAL_CONTEXT
    STEPentity *mech_context = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "MECHANICAL_CONTEXT");
    brlcad::step::AddEntity(prod, "frame_of_reference", mech_context);
    brlcad::step::SetString(mech_context, "name", "");
    brlcad::step::SetString(mech_context, "discipline_type", "");

    // APPLICATION_CONTEXT
    brlcad::step::SetEntity(mech_context, "frame_of_reference",
	    sc->application_context);

    // DESIGN_CONTEXT
    if (sc->design_context)
	brlcad::step::SetEntity(prod_def, "frame_of_reference", sc->design_context);

    //return ret_entity;
    // The product definition is what is used to define assemblies, so return that
    return prod_def;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
