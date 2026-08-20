/*                 Factory.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/Factory.cpp
 *
 * Class implementation for STEP object "Factory".
 *
 */

/* interface header */
#include "Factory.h"

/* implementation headers */
#include "STEPEntity.h"
#include "PCurve.h"
#include "SurfaceCurve.h"

#define CLASSNAME "Factory"
const char *Factory::factoryname = "STEP Object Factory";
Factory::OBJECTS Factory::objects;
Factory::UNMAPPED_OBJECTS Factory::unmapped_objects;
int Factory::vertex_count = 0;
VECTOR_OF_OBJECTS Factory::vertices;
Factory::ID_TO_INDEX_MAP Factory::vertex_to_index;
Factory::INDEX_TO_ID_MAP Factory::vertex_index_to_id;

static bool
is_schema_entity(STEPWrapper *wrapper, SDAI_Application_instance *instance,
    const char *name)
{
    const EntityDescriptor *descriptor = wrapper ? wrapper->SchemaEntity(name) : NULL;
    return instance && descriptor && instance->IsA(descriptor);
}


static void
report_unmapped_factory(STEPWrapper *wrapper,
    const std::string &methodname)
{
    if (wrapper) {
	/* Factory availability is type-wide, not instance-specific.  Entity zero
	 * lets repeated lazy materializations increment one bounded diagnostic. */
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    0, methodname, "factory",
	    "no converter factory is registered for this entity type");
	return;
    }
    std::cerr << "Factory Method not mapped: " << methodname << std::endl;
}


Factory::Factory()
{
}


Factory::~Factory()
{
}


FACTORYMAP &
Factory::GetMap()
{
    static FACTORYMAP *factorymap = new FACTORYMAP; // Using the "construct on first use" idiom
    return *factorymap;
}


void Factory::Print()
{
    FACTORYMAP &methodmap = GetMap();

    std::cout << "Map size: " << methodmap.size() << std::endl;

    for (FACTORYMAP::iterator ii = methodmap.begin(); ii != methodmap.end(); ++ii) {
	std::cout << "\t" << (*ii).first << std::endl;
    }
}


STEPEntity *
Factory::CreateObject(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    std::string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (is_schema_entity(sw, sse, "B_SPLINE_CURVE")) {
	    return (STEPEntity *)CreateCurveObject(sw, sse);
	} else if (is_schema_entity(sw, sse, "B_SPLINE_SURFACE")) {
	    return (STEPEntity *)CreateSurfaceObject(sw, sse);
	} else if (is_schema_entity(sw, sse, "NAMED_UNIT")) {
	    return (STEPEntity *)CreateNamedUnitObject(sw, sse);
	} else if (is_schema_entity(sw, sse, "SURFACE_CURVE")) {
	    /* Part 42 represents a bounded surface curve as a complex instance
	     * combining BOUNDED_CURVE and SURFACE_CURVE.  The surface-curve
	     * wrapper already forwards EDGE_CURVE endpoint trimming to its exact
	     * curve_3d and retains the associated pcurves; selecting the incomplete
	     * BoundedSurfaceCurve conversion here would discard that working
	     * implementation. */
	    return SurfaceCurve::Create(sw, sse);
	} else if (is_schema_entity(sw, sse, "PCURVE")) {
	    /* The analogous Part 42 BOUNDED_PCURVE complex adds no geometry
	     * attributes beyond PCURVE and BOUNDED_CURVE.  PCurve already retains
	     * its bounded definitional-representation item, so use that complete
	     * adapter instead of rejecting the legal complex instance. */
	    return PCurve::Create(sw, sse);
	} else if (is_schema_entity(sw, sse, "TOPOLOGICAL_REPRESENTATION_ITEM")) {
	    //loop_path;
	    /*
	     * ONEOF (
	     VERTEX,
	     EDGE,
	     FACE_BOUND,
	     FACE,
	     VERTEX_SHELL,
	     WIRE_SHELL,
	     CONNECTED_EDGE_SET,
	     CONNECTED_FACE_SET,(
	     LOOP
	     ANDOR
	     PATH))
	    */
	} else if (is_schema_entity(sw, sse, "SHAPE_REPRESENTATION_RELATIONSHIP")) {
	    // not sure why complex here
	    return (STEPEntity *)CreateShapeRepresentationRelationshipObject(sw, sse);
	} else if (is_schema_entity(sw, sse, "REPRESENTATION_CONTEXT")) {
	    // not sure why complex here
	    return (STEPEntity *)CreateRepresentationContext(sw, sse);
	} else {
	    std::cerr << CLASSNAME << ": Error unknown complex type." << std::endl;
	    return NULL;
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
	if (f != NULL) {
	    return f(sw, sse);
	}
    }
    return NULL; // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateCurveObject(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (is_schema_entity(sw, sse, "B_SPLINE_CURVE")) {
	    if (is_schema_entity(sw, sse, "RATIONAL_B_SPLINE_CURVE")) {
		if (is_schema_entity(sw, sse, "UNIFORM_CURVE")) {
		    methodname = "Rational_Uniform_Curve";
		    //std::cout << "   Entity of type:rational_uniform_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "QUASI_UNIFORM_CURVE")) {
		    methodname = "Rational_Quasi_Uniform_Curve";
		    //std::cout << "   Entity of type:rational_quasi_uniform_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "BEZIER_CURVE")) {
		    methodname = "Rational_Bezier_Curve";
		    //std::cout << "   Entity of type:rational_bezier_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "B_SPLINE_CURVE_WITH_KNOTS")) {
		    methodname = "Rational_B_Spline_Curve_With_Knots";
		    //std::cout << "   Entity of type:rational_b_spline_curve_with_knots" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else {
		    std::cerr << "Unknown complex type for B_Spline_Curve." << std::endl;
		    return NULL;
		}
	    } else {
		// not sure if/why this would happen so error for now
		std::cerr << CLASSNAME << ": Tagged as complex B_Spline_Curve but not complex." << std::endl;
		return NULL;
	    }
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
    }
    if (f == NULL) {
	std::cerr << "Factory Method returned a NULL object creation method: " << methodname << std::endl;
	return NULL;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateNamedUnitObject(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (is_schema_entity(sw, sse, "NAMED_UNIT")) {
	    if (is_schema_entity(sw, sse, "SI_UNIT")) {
		if (is_schema_entity(sw, sse, "LENGTH_UNIT")) {
		    methodname = "Length_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "MASS_UNIT")) {
		    methodname = "Mass_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "TIME_UNIT")) {
		    methodname = "Time_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "ELECTRIC_CURRENT_UNIT")) {
		    methodname = "Electric_Current_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "THERMODYNAMIC_TEMPERATURE_UNIT")) {
		    methodname = "Thermodynamic_Temperature_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AMOUNT_OF_SUBSTANCE_UNIT")) {
		    methodname = "Amount_Of_Substance_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "LUMINOUS_INTENSITY_UNIT")) {
		    methodname = "Luminous_Intensity_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "PLANE_ANGLE_UNIT")) {
		    methodname = "Plane_Angle_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "SOLID_ANGLE_UNIT")) {
		    methodname = "Solid_Angle_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AREA_UNIT")) {
		    methodname = "Area_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "VOLUME_UNIT")) {
		    methodname = "Volume_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "RATIO_UNIT")) {
		    methodname = "Ratio_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else {
		    std::cerr << "Unknown complex type for SI_Named_Unit." << std::endl;
		    return NULL;
		}
	    } else if (is_schema_entity(sw, sse, "CONVERSION_BASED_UNIT")) {
		if (is_schema_entity(sw, sse, "LENGTH_UNIT")) {
		    methodname = "Length_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "MASS_UNIT")) {
		    methodname = "Mass_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "TIME_UNIT")) {
		    methodname = "Time_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "ELECTRIC_CURRENT_UNIT")) {
		    methodname = "Electric_Current_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "THERMODYNAMIC_TEMPERATURE_UNIT")) {
		    methodname = "Thermodynamic_Temperature_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AMOUNT_OF_SUBSTANCE_UNIT")) {
		    methodname = "Amount_Of_Substance_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "LUMINOUS_INTENSITY_UNIT")) {
		    methodname = "Luminous_Intensity_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "PLANE_ANGLE_UNIT")) {
		    methodname = "Plane_Angle_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "SOLID_ANGLE_UNIT")) {
		    methodname = "Solid_Angle_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AREA_UNIT")) {
		    methodname = "Area_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "VOLUME_UNIT")) {
		    methodname = "Volume_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "RATIO_UNIT")) {
		    methodname = "Ratio_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else {
		    std::cerr << "Unknown complex type for Conversion_Based_Named_Unit." << std::endl;
		    return NULL;
		}
	    } else if (is_schema_entity(sw, sse, "CONTEXT_DEPENDENT_UNIT")) {
		if (is_schema_entity(sw, sse, "LENGTH_UNIT")) {
		    methodname = "Length_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "MASS_UNIT")) {
		    methodname = "Mass_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "TIME_UNIT")) {
		    methodname = "Time_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "ELECTRIC_CURRENT_UNIT")) {
		    methodname = "Electric_Current_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "THERMODYNAMIC_TEMPERATURE_UNIT")) {
		    methodname = "Thermodynamic_Temperature_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AMOUNT_OF_SUBSTANCE_UNIT")) {
		    methodname = "Amount_Of_Substance_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "LUMINOUS_INTENSITY_UNIT")) {
		    methodname = "Luminous_Intensity_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "PLANE_ANGLE_UNIT")) {
		    methodname = "Plane_Angle_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "SOLID_ANGLE_UNIT")) {
		    methodname = "Solid_Angle_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "AREA_UNIT")) {
		    methodname = "Area_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "VOLUME_UNIT")) {
		    methodname = "Volume_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "RATIO_UNIT")) {
		    methodname = "Ratio_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else {
		    std::cerr << "Unknown complex type for Context_Dependent_Named_Unit." << std::endl;
		    return NULL;
		}
	    } else {
		// not sure if/why this would happen so error for now
		std::cerr << CLASSNAME << ": Tagged as complex Named_Unit but not complex." << std::endl;
		return NULL;
	    }
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
    }
    if (f == NULL) {
	std::cerr << "Factory Method returned a NULL object creation method: " << methodname << std::endl;
	return NULL;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateSurfaceObject(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (is_schema_entity(sw, sse, "B_SPLINE_SURFACE")) {
	    if (is_schema_entity(sw, sse, "RATIONAL_B_SPLINE_SURFACE")) {
		if (is_schema_entity(sw, sse, "UNIFORM_SURFACE")) {
		    methodname = "Rational_Uniform_Surface";
		    //std::cout << "   Entity of type:rational_uniform_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "QUASI_UNIFORM_SURFACE")) {
		    methodname = "Rational_Quasi_Uniform_Surface";
		    //std::cout << "   Entity of type:rational_quasi_uniform_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "BEZIER_SURFACE")) {
		    methodname = "Rational_Bezier_Surface";
		    //std::cout << "   Entity of type:rational_bezier_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else if (is_schema_entity(sw, sse, "B_SPLINE_SURFACE_WITH_KNOTS")) {
		    methodname = "Rational_B_Spline_Surface_With_Knots";
		    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			report_unmapped_factory(sw, methodname);
			return NULL;
		    }
		    f = (*i).second;
		} else {
		    std::cerr << "Unknown complex type for B_Spline_Surface." << std::endl;
		    return NULL;
		}
	    } else {
		// not sure if/why this would happen so error for now
		std::cerr << CLASSNAME << ": Tagged as complex B_Spline_Surface but not complex." << std::endl;
		return NULL;
	    }
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
    }
    if (f == NULL) {
	std::cerr << "Factory Method returned a NULL object creation method: " << methodname << std::endl;
	return NULL;
    }
    return f(sw, sse);
}


STEPEntity *
Factory::CreateShapeRepresentationRelationshipObject(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	if (is_schema_entity(sw, sse, "SHAPE_REPRESENTATION_RELATIONSHIP")) {
	    if ((i = methodmap.find(methodname)) == methodmap.end()) {
		report_unmapped_factory(sw, methodname);
		return NULL;
	    }
	    f = (*i).second;
	} else {
	    // not sure if/why this would happen so error for now
	    std::cerr << CLASSNAME << ": complex entity is not a SHAPE_REPRESENTATION_RELATIONSHIP." << std::endl;
	    return NULL;
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}

STEPEntity *
Factory::CreateRepresentationContext(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!sse) {
	return NULL;
    }

    const char *entityname = sse->EntityName();
    if (!entityname) {
	return NULL;
    }

    string methodname = std::string(entityname);
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	if (is_schema_entity(sw, sse, "GEOMETRIC_REPRESENTATION_CONTEXT")) {
	    methodname = "Geometric_Representation_Context";
	    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
	    if ((i = methodmap.find(methodname)) == methodmap.end()) {
		report_unmapped_factory(sw, methodname);
		return NULL;
	    }
	    f = (*i).second;
	} else if (is_schema_entity(sw, sse, "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT")) {
	    methodname = "Global_Uncertainty_Assigned_Context";
	    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
	    if ((i = methodmap.find(methodname)) == methodmap.end()) {
		report_unmapped_factory(sw, methodname);
		return NULL;
	    }
	    f = (*i).second;
	} else if (is_schema_entity(sw, sse, "GLOBAL_UNIT_ASSIGNED_CONTEXT")) {
	    methodname = "Global_Unit_Assigned_Context";
	    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
	    if ((i = methodmap.find(methodname)) == methodmap.end()) {
		report_unmapped_factory(sw, methodname);
		return NULL;
	    }
	    f = (*i).second;
	} else if (is_schema_entity(sw, sse, "PARAMETRIC_REPRESENTATION_CONTEXT")) {
	    methodname = "Parametric_Representation_Context";
	    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
	    if ((i = methodmap.find(methodname)) == methodmap.end()) {
		report_unmapped_factory(sw, methodname);
		return NULL;
	    }
	    f = (*i).second;
	} else {
	    // not sure if/why this would happen so error for now
	    std::cerr << CLASSNAME << ": complex entity is not a REPRESENTATION_CONTEXT." << std::endl;
	    return NULL;
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    report_unmapped_factory(sw, methodname);
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}

string Factory::RegisterClass(string methodname, FactoryMethod f)
{
    FACTORYMAP &methodmap = GetMap();

    FACTORYMAP::iterator i;
    //std::cout << "Adding Factory Method:" << methodname << std::endl;
    if ((i = methodmap.find(methodname)) == methodmap.end()) {
	methodmap[methodname] = f;
    } else {
	std::cerr << "Factory Method already mapped: " << methodname << std::endl;
    }
    return methodname;
}


void Factory::DeleteObjects()
{
    OBJECTS::iterator i = objects.begin();

    while (i != objects.end()) {
	delete(*i).second;
	objects.erase((*i).first);
	i = objects.begin();
    }
    UNMAPPED_OBJECTS::iterator j = unmapped_objects.begin();

    while (j != unmapped_objects.end()) {
	delete(*j);
	j = unmapped_objects.erase(j);
    }
}
Factory::OBJECTS::iterator Factory::FindObject(int id)
{
    Factory::OBJECTS::iterator i = objects.end();
    if (id > 0) {
	i = objects.find(id);
    }
    return i;
}


void Factory::AddObject(STEPEntity *se)
{
    if (se->STEPid() > 0) {
	objects[se->STEPid()] = se;
    } else {
	unmapped_objects.push_back(se);
    }
}


void Factory::AddVertex(STEPEntity *se)
{
    AddObject(se);
    if (se->STEPid() > 0) {
	vertices.insert(vertices.begin() + vertex_count, se);
	vertex_to_index[se->STEPid()] = vertex_count;
	vertex_index_to_id[vertex_count++] = se->STEPid();
    } else {
	std::cerr << "Warning: Factory::AddVertex(...) - Vertex with unmappable ID." << std::endl;
    }
}


VECTOR_OF_OBJECTS *
Factory::GetVertices()
{
    return &vertices;
}


int
Factory::GetVertexIndex(int id)
{
    return vertex_to_index[id];
}


STEPEntity *
Factory::GetVertexByIndex(int index)
{
    return objects[vertex_index_to_id[index]];
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
