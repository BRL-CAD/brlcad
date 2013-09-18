/*            A S S E M B L Y _ P R O D U C T . C P P
 *
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file Assembly_Product.cpp
 *
 */

#include "common.h"

#include "G_STEP_internal.h"
#include "ON_Brep.h"

/*
 * To associate multiple objects in STEP, we must define and relate product definitions
 * for both shapes (BReps) and assembly objects (combs) that will relate the shapes.
 * Below is an example of how the structural definitions relate - in this instance, one
 * BRep and two instances of that BRep moved and added to an assembly.
 *
 * // Representation context
 * #10=DIMENSIONAL_EXPONENTS(1.E0,0.E0,0.E0,0.E0,0.E0,0.E0,0.E0);
 * #11=(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.));
 * #12=LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(1E1),#11);
 * #13=(CONVERSION_BASED_UNIT('INCH',#12)LENGTH_UNIT()NAMED_UNIT(#10));
 * #14=DIMENSIONAL_EXPONENTS(0.E0,0.E0,0.E0,0.E0,0.E0,0.E0,0.E0);
 * #15=(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.));
 * #16=PLANE_ANGLE_MEASURE_WITH_UNIT(PLANE_ANGLE_MEASURE(0.0174532925199433),#15);
 * #17=(CONVERSION_BASED_UNIT('DEGREE',#16)NAMED_UNIT(*)PLANE_ANGLE_UNIT());
 * #18=(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.)SOLID_ANGLE_UNIT());
 * #19=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.05),#13,'accuracy','Tolerance');
 * #20=(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT
 * ((#19))GLOBAL_UNIT_ASSIGNED_CONTEXT((#13,#17,#18))REPRESENTATION_CONTEXT('ID52','3'));
 *
 * #100=APPLICATION_CONTEXT('CONFIGURATION CONTROLLED 3D DESIGNS OF MECHANICAL PARTS AND ASSEMBLIES');
 *
 * // seems to be a list of AXIS2_PLACEMENT_3D objects - does this put all the axis
 * // in a representation context?  Used by the REPRESENTATION_RELATIONSHIP objects.
 * #300=SHAPE_REPRESENTATION('',(...,#9004,#9013,...), #20);
 *
 * // Transformation definitions - I think these are similar to matrices over combs?
 *
 * #8001=CARTESIAN_POINT('',(0.E0,0.E0,0.E0));
 * #8002=DIRECTION('',(0.E0,0.E0,1.E0));
 * #8003=DIRECTION('',(1.E0,0.E0,0.E0));
 * #8004=AXIS2_PLACEMENT_3D('',#8001,#8002,#8003);
 *
 * #9001=CARTESIAN_POINT('',());
 * #9002=DIRECTION('',());
 * #9003=DIRECTION('',());
 * #9004=AXIS2_PLACEMENT_3D('',#9001,#9002,#9003);
 * #7001=ITEM_DEFINED_TRANSFORMATION('','',#8004,#9004);
 *
 * #9010=CARTESIAN_POINT('',());
 * #9011=DIRECTION('',());
 * #9012=DIRECTION('',());
 * #9013=AXIS2_PLACEMENT_3D('',#9010,#9011,#9012);
 * #7002=ITEM_DEFINED_TRANSFORMATION('','',#8004,#9013);
 *
 * // Solid shape representation  Looks like this is the bit that needs to be created
 * // for each solid - should be done by the SHAPE_DEFINITION_REPRESENTATION logic.
 * #200=ADVANCED_BREP_SHAPE_REPRESENTATION();
 * #1001=DESIGN_CONTEXT('',#100,'design');
 * #1002=MECHANICAL_CONTEXT('',#100,'mechanical');
 * #1003=PRODUCT('Geometric_Shape_01','Geometric_Shape_01','NOT SPECIFIED',(#1002));
 * #1004=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#1003,.MADE.);
 * #1005=PRODUCT_DEFINITION('design','',#1004,#1001);
 * #1006=PRODUCT_DEFINITION_SHAPE('','Shape For Geometric_Shape_01',#1005);
 * #1007=SHAPE_DEFINITION_REPRESENTATION(#1006,#200);
 *
 * // Assembly shape representation
 * #5001=MECHANICAL_CONTEXT('',#100,'mechanical');
 * #5002=PRODUCT('Comb_01','Comb_01','NOT SPECIFIED',(#5001));
 * #5003=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#5002,.MADE.);
 * #5004=PRODUCT_DEFINITION('design','',#5003,#1001);
 *
 * // Assembly components
 *
 * #2001=NEXT_ASSEMBLY_USAGE_OCCURRENCE('01','Next assembly relationship','Geometric_Shape_01',#5004,#1005,$);
 * #2002=PRODUCT_DEFINITION_SHAPE('Placement #01','Placement of Geometric_Shape_01 with respect to Comb_01',#2001);
 *
 * #3001=NEXT_ASSEMBLY_USAGE_OCCURRENCE('02','Next assembly relationship','Geometric_Shape_01',#5004,#1005,$);
 * #3002=PRODUCT_DEFINITION_SHAPE('Placement #02','Placement of Geometric_Shape_01 with respect to Comb_01',#3001);
 *
 * #4001=(REPRESENTATION_RELATIONSHIP('','',#200,#300)REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION(#7002)SHAPE_REPRESENTATION_RELATIONSHIP());
 * #4002=CONTEXT_DEPENDENT_SHAPE_REPRESENTATION(#4001,#3002);
 *
 * #6001=(REPRESENTATION_RELATIONSHIP('','',#200,#300)REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION(#7001)SHAPE_REPRESENTATION_RELATIONSHIP());
 * #6002=CONTEXT_DEPENDENT_SHAPE_REPRESENTATION(#6001,#2002);
 *
 *
 * Mapping from BRL-CAD to STEP
 *
 * Roughly speaking, the STEP requirement that each shape representation have an associated product appears
 * to translate to BRL-CAD as each brep object having a comb above it.  To avoid introducing unnecessary combs,
 * it will be necessary to recognize when a brep referenced by one or more comb trees listed for export has
 * one and only one parent comb in the existing tree structure, which in turn does not have any other children.
 * Otherwise, repeated imports and exports will bury the brep below an ever deepening hierarchy of combs.  When
 * a brep does not have this wrapper already in place, one will need to be explicitly created for it.
 * Hopefully, search can be used to quickly identify sets of geometry that need to be handled differently.
 * It may be that the assumption of shared naming - a comb with the same root name as its child brep - can be used
 * as a quick check to determine if the comb is a parent or not, if we accept that as a convention.
 *
 * Because combs reference combs in BRL-CAD, the step product definitions associated with combs will need to be
 * fully created before the assembly usage occurrences can start to be assembled.  A search that collects all combs
 * will provide a convenient list for multiple passes that create and then assemble combs, as will a similar search
 * for solids (currently just breps...).  Should also make sure the tree is union only for AP203, where booleans are
 * not supported. Once boolean evaluation is in place, should be able to default to brep solids for combs with non
 * union booleans below them.
 *
 */

void
Add_Assembly_Product(Registry *registry, InstMgr *instance_list)
{

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
