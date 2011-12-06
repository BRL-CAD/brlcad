/*                 Factory.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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


#define CLASSNAME "Factory"
const char* Factory::factoryname = "AP203e2 Object Factory";
Factory::OBJECTS Factory::objects;
Factory::UNMAPPED_OBJECTS Factory::unmapped_objects;
int Factory::vertex_count = 0;
VECTOR_OF_OBJECTS Factory::vertices;
Factory::ID_TO_INDEX_MAP Factory::vertex_to_index;
Factory::INDEX_TO_ID_MAP Factory::vertex_index_to_id;


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

    FACTORYMAP::iterator i;
    std::cout << "Map size: " << methodmap.size() << std::endl;

    for (FACTORYMAP::iterator ii = methodmap.begin(); ii != methodmap.end(); ++ii) {
	std::cout << "\t" << (*ii).first << std::endl;
    }
}


STEPEntity *
Factory::CreateObject(STEPWrapper *sw, SCLP23(Application_instance) *sse)
{
    string methodname = sse->EntityName();
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (sse->IsA(config_control_designe_b_spline_curve)) {
	    return (STEPEntity *)CreateCurveObject(sw,sse);
	} else if (sse->IsA(config_control_designe_b_spline_surface)) {
	    return (STEPEntity *)CreateSurfaceObject(sw,sse);
	} else if (sse->IsA(config_control_designe_named_unit)) {
	    return (STEPEntity *)CreateNamedUnitObject(sw,sse);
	} else if (sse->IsA(config_control_designe_surface_curve)) {
	    /*
	     * ONEOF (
	     INTERSECTION_CURVE,
	     SEAM_CURVE)
	     ANDOR
	     BOUNDED_SURFACE_CURVE
	    */
	} else if (sse->IsA(config_control_designe_topological_representation_item)) {
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
	} else {
	    std::cerr << CLASSNAME << ": Error unknown complex type." << std::endl;
	    return NULL;
	}
    } else {
	//std::cout << "Getting Factory Method for:" << methodname << std::endl;
	if ((i = methodmap.find(methodname)) == methodmap.end()) {
	    std::cerr << "Factory Method not mapped: " << methodname << std::endl;
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateCurveObject(STEPWrapper *sw, SCLP23(Application_instance) *sse)
{
    string methodname = sse->EntityName();
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (sse->IsA(config_control_designe_b_spline_curve)) {
	    if (sse->IsA(config_control_designe_rational_b_spline_curve)) {
		if (sse->IsA(config_control_designe_uniform_curve)) {
		    methodname = "Rational_Uniform_Curve";
		    //std::cout << "   Entity of type:rational_uniform_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_quasi_uniform_curve)) {
		    methodname = "Rational_Quasi_Uniform_Curve";
		    //std::cout << "   Entity of type:rational_quasi_uniform_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_bezier_curve)) {
		    methodname = "Rational_Bezier_Curve";
		    //std::cout << "   Entity of type:rational_bezier_curve" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_b_spline_curve_with_knots)) {
		    methodname = "Rational_B_Spline_Curve_With_Knots";
		    //std::cout << "   Entity of type:rational_b_spline_curve_with_knots" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
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
	    std::cerr << "Factory Method not mapped: " << methodname << std::endl;
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateNamedUnitObject(STEPWrapper *sw, SCLP23(Application_instance) *sse)
{
    string methodname = sse->EntityName();
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (sse->IsA(config_control_designe_named_unit)) {
	    if (sse->IsA(config_control_designe_si_unit)) {
		if (sse->IsA(config_control_designe_length_unit)) {
		    methodname = "Length_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_mass_unit)) {
		    methodname = "Mass_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_time_unit)) {
		    methodname = "Time_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_electric_current_unit)) {
		    methodname = "Electric_Current_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_thermodynamic_temperature_unit)) {
		    methodname = "Thermodynamic_Temperature_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_amount_of_substance_unit)) {
		    methodname = "Amount_Of_Substance_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_luminous_intensity_unit)) {
		    methodname = "Luminous_Intensity_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
		} else if (sse->IsA(config_control_designe_plane_angle_unit)) {
		    methodname = "Plane_Angle_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_solid_angle_unit)) {
		    methodname = "Solid_Angle_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_area_unit)) {
		    methodname = "Area_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_volume_unit)) {
		    methodname = "Volume_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_ratio_unit)) {
		    methodname = "Ratio_Si_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
		} else {
		    std::cerr << "Unknown complex type for SI_Named_Unit." << std::endl;
		    return NULL;
		}
	    } else if (sse->IsA(config_control_designe_conversion_based_unit)) {
		if (sse->IsA(config_control_designe_length_unit)) {
		    methodname = "Length_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_mass_unit)) {
		    methodname = "Mass_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_time_unit)) {
		    methodname = "Time_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_electric_current_unit)) {
		    methodname = "Electric_Current_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_thermodynamic_temperature_unit)) {
		    methodname = "Thermodynamic_Temperature_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_amount_of_substance_unit)) {
		    methodname = "Amount_Of_Substance_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_luminous_intensity_unit)) {
		    methodname = "Luminous_Intensity_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
		} else if (sse->IsA(config_control_designe_plane_angle_unit)) {
		    methodname = "Plane_Angle_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_solid_angle_unit)) {
		    methodname = "Solid_Angle_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_area_unit)) {
		    methodname = "Area_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_volume_unit)) {
		    methodname = "Volume_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_ratio_unit)) {
		    methodname = "Ratio_Conversion_Based_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
		} else {
		    std::cerr << "Unknown complex type for Conversion_Based_Named_Unit." << std::endl;
		    return NULL;
		}
	    } else if (sse->IsA(config_control_designe_context_dependent_unit)) {
		if (sse->IsA(config_control_designe_length_unit)) {
		    methodname = "Length_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_mass_unit)) {
		    methodname = "Mass_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_time_unit)) {
		    methodname = "Time_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_electric_current_unit)) {
		    methodname = "Electric_Current_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_thermodynamic_temperature_unit)) {
		    methodname = "Thermodynamic_Temperature_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_amount_of_substance_unit)) {
		    methodname = "Amount_Of_Substance_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_luminous_intensity_unit)) {
		    methodname = "Luminous_Intensity_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
		} else if (sse->IsA(config_control_designe_plane_angle_unit)) {
		    methodname = "Plane_Angle_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_solid_angle_unit)) {
		    methodname = "Solid_Angle_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_area_unit)) {
		    methodname = "Area_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_volume_unit)) {
		    methodname = "Volume_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#ifdef AP203e
		} else if (sse->IsA(config_control_designe_ratio_unit)) {
		    methodname = "Ratio_Context_Dependent_Unit";
		    //std::cout << "   Entity of type: " << methodname << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
#endif
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
	    std::cerr << "Factory Method not mapped: " << methodname << std::endl;
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse); // dynamic_cast<STEPEntity *>(Curve::Create(sw,sse));
}


STEPEntity *
Factory::CreateSurfaceObject(STEPWrapper *sw, SCLP23(Application_instance) *sse)
{
    string methodname = sse->EntityName();
    FACTORYMAP &methodmap = GetMap();
    FactoryMethod f = NULL;
    FACTORYMAP::iterator i;

    if (sse->IsComplex()) {
	//std::cout << "Complex Entity Instance Name:" << sse->EntityName() << " ID:"
	//		<< sse->STEPfile_id << std::endl;
	if (sse->IsA(config_control_designe_b_spline_surface)) {
	    if (sse->IsA(config_control_designe_rational_b_spline_surface)) {
		if (sse->IsA(config_control_designe_uniform_surface)) {
		    methodname = "Rational_Uniform_Surface";
		    //std::cout << "   Entity of type:rational_uniform_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_quasi_uniform_surface)) {
		    methodname = "Rational_Quasi_Uniform_Surface";
		    //std::cout << "   Entity of type:rational_quasi_uniform_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_bezier_surface)) {
		    methodname = "Rational_Bezier_Surface";
		    //std::cout << "   Entity of type:rational_bezier_surface" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
			return NULL;
		    }
		    f = (*i).second;
		} else if (sse->IsA(config_control_designe_b_spline_surface_with_knots)) {
		    methodname = "Rational_B_Spline_Surface_With_Knots";
		    //std::cout << "   Entity of type:rational_b_spline_surface_with_knots" << std::endl;
		    if ((i = methodmap.find(methodname)) == methodmap.end()) {
			std::cerr << "Factory Method not mapped: " << methodname << std::endl;
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
	    std::cerr << "Factory Method not mapped: " << methodname << std::endl;
	    return NULL;
	}
	f = (*i).second;
    }
    return f(sw, sse);
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

    while(i != objects.end()) {
	delete (*i).second;
	objects.erase((*i).first);
	i=objects.begin();
    }
    UNMAPPED_OBJECTS::iterator j = unmapped_objects.begin();

    while(j != unmapped_objects.end()) {
	delete (*j);
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
	vertices.insert(vertices.begin() + vertex_count,se);
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
