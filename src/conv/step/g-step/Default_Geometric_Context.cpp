/*   D E F A U L T _ G E O M E T R I C _ C O N T E X T . C P P
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
/** @file Default_Geometric_Context.cpp
 *
 */

#include "AP203.h"
#include "Default_Geometric_Context.h"

STEPcomplex *
Add_Default_Geometric_Context(Registry *registry, InstMgr *instance_list)
{

    int instance_cnt = 0;
    STEPattribute *attr;
    STEPcomplex *stepcomplex;

    /* Uncertainty measure with unit */
    SdaiUncertainty_measure_with_unit *uncertainty = (SdaiUncertainty_measure_with_unit *)registry->ObjCreate("UNCERTAINTY_MEASURE_WITH_UNIT");
    uncertainty->name_("'DISTANCE_ACCURACY_VALUE'");
    uncertainty->description_("'Threshold below which geometry imperfections (such as overlaps) are not considered errors.'");
    instance_list->Append(uncertainty, completeSE);
    instance_cnt++;

    /** unit component of uncertainty measure with unit */
    const char *unitNmArr[4] = {"length_unit", "named_unit", "si_unit", "*"};
    STEPcomplex *unit_complex = new STEPcomplex(registry, (const char **)unitNmArr, instance_cnt);
    instance_list->Append((STEPentity *)unit_complex, completeSE);
    instance_cnt++;
    stepcomplex = unit_complex->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "prefix")) attr->ptr.e = new SdaiSi_prefix_var(Si_prefix__milli);
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__metre);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    SdaiUnit *new_unit = new SdaiUnit((SdaiNamed_unit *)unit_complex);
    uncertainty->ResetAttributes();
    {
	while ((attr = uncertainty->NextAttribute()) != NULL) {
	    if (!bu_strcmp(attr->Name(), "unit_component")) attr->ptr.sh = new_unit;
	    if (!bu_strcmp(attr->Name(), "value_component")) attr->StrToVal("0.05");
	}
    }

    /* Global Unit Assigned Context */
    const char *ua_entry_1_types[4] = {"named_unit", "si_unit", "solid_angle_unit", "*"};
    STEPcomplex *ua_entry_1 = new STEPcomplex(registry, (const char **)ua_entry_1_types, instance_cnt);
    stepcomplex = ua_entry_1->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__steradian);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }
    instance_list->Append((STEPentity *)ua_entry_1, completeSE);
    instance_cnt++;

    const char *ua_entry_3_types[4] = {"named_unit", "plane_angle_unit", "si_unit", "*"};
    STEPcomplex *ua_entry_3 = new STEPcomplex(registry, (const char **)ua_entry_3_types, instance_cnt);
    stepcomplex = ua_entry_3->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->ptr.e = new SdaiSi_unit_name_var(Si_unit_name__radian);
	    }
	}
	stepcomplex = stepcomplex->sc;
    }
    instance_list->Append((STEPentity *)ua_entry_3, completeSE);
    instance_cnt++;

    /* Plane Angle Measure */
    SdaiPlane_angle_measure_with_unit *p_ang_measure_with_unit = new SdaiPlane_angle_measure_with_unit();
    SdaiMeasure_value * p_ang_measure_value = new SdaiMeasure_value(DEG2RAD, SCHEMA_NAMESPACE::t_measure_value);
    p_ang_measure_value->SetUnderlyingType(SCHEMA_NAMESPACE::t_plane_angle_measure);
    p_ang_measure_with_unit->value_component_(p_ang_measure_value);
    SdaiUnit *p_ang_unit = new SdaiUnit((SdaiNamed_unit *)ua_entry_3);
    p_ang_measure_with_unit->unit_component_(p_ang_unit);
    instance_list->Append((STEPentity *)p_ang_measure_with_unit, completeSE);
    instance_cnt++;

    /* Conversion based unit */
    const char *ua_entry_2_types[4] = {"conversion_based_unit", "named_unit", "plane_angle_unit", "*"};
    STEPcomplex *ua_entry_2 = new STEPcomplex(registry, (const char **)ua_entry_2_types, instance_cnt);

    /** dimensional exponents **/
    SdaiDimensional_exponents *dimensional_exp = new SdaiDimensional_exponents();
    dimensional_exp->length_exponent_(0.0);
    dimensional_exp->mass_exponent_(0.0);
    dimensional_exp->time_exponent_(0.0);
    dimensional_exp->electric_current_exponent_(0.0);
    dimensional_exp->thermodynamic_temperature_exponent_(0.0);
    dimensional_exp->amount_of_substance_exponent_(0.0);
    dimensional_exp->luminous_intensity_exponent_(0.0);
    instance_list->Append((STEPentity *)dimensional_exp, completeSE);

    stepcomplex = ua_entry_2->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Conversion_Based_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "name")) attr->StrToVal("'DEGREES'");
		if (!bu_strcmp(attr->Name(), "conversion_factor")) {
		    attr->ptr.c = new (STEPentity *);
		    *(attr->ptr.c) = (STEPentity *)(p_ang_measure_with_unit);
		}
	    }
	}
	if (!bu_strcmp(stepcomplex->EntityName(), "Named_Unit")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "dimensions")) {
		    attr->ptr.c = new (STEPentity *);
		    *(attr->ptr.c) = (STEPentity *)(dimensional_exp);
		}
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    instance_list->Append((STEPentity *)ua_entry_2, completeSE);
    instance_cnt++;

    /*
     * Now that we have the pieces, build the final complex type from four other types:
     */
    const char *entNmArr[5] = {"geometric_representation_context", "global_uncertainty_assigned_context",
			       "global_unit_assigned_context", "representation_context", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(registry, (const char **)entNmArr, instance_cnt);
    stepcomplex = complex_entity->head;

    while (stepcomplex) {

	if (!bu_strcmp(stepcomplex->EntityName(), "Geometric_Representation_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "coordinate_space_dimension")) attr->StrToVal("3");
	    }
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Uncertainty_Assigned_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "uncertainty")) {
		    EntityAggregate *unc_agg = new EntityAggregate();
		    unc_agg->AddNode(new EntityNode((SDAI_Application_instance *)uncertainty));
		    attr->ptr.a = unc_agg;
		}
	    }

	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Unit_Assigned_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		std::string attrval;
		if (!bu_strcmp(attr->Name(), "units")) {
		    EntityAggregate *unit_assigned_agg = new EntityAggregate();
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)unit_complex));
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)ua_entry_2));
		    unit_assigned_agg->AddNode(new EntityNode((SDAI_Application_instance *)ua_entry_1));
		    attr->ptr.a = unit_assigned_agg;
		}
	    }
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Representation_Context")) {
	    stepcomplex->ResetAttributes();
	    while ((attr = stepcomplex->NextAttribute()) != NULL) {
		if (!bu_strcmp(attr->Name(), "context_identifier")) attr->StrToVal("'STANDARD'");
		if (!bu_strcmp(attr->Name(), "context_type")) attr->StrToVal("'3D'");
	    }
	}
	stepcomplex = stepcomplex->sc;
    }

    instance_list->Append((STEPentity *)complex_entity, completeSE);

    return complex_entity;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
