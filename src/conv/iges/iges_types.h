/*                    I G E S _ T Y P E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file iges_types.h
 *
 */

#define NTYPES 78

struct types typecount[NTYPES+1]={
	{ 0 , "Unknown entity type" , 0 },
	{ 100 , "Circular Arc" , 0 },
	{ 102 , "Composite Curve" , 0 },
	{ 104 , "Conic Arc" , 0 },
	{ 106 , "Copious Data" , 0 },
	{ 108 , "Plane" , 0 },
	{ 110 , "Line" , 0 },
	{ 112 , "Parametric Spline Curve" , 0 },
	{ 114 , "Parametric Spline Surface" , 0 },
	{ 116 , "Point" , 0 },
	{ 118 , "Ruled Surface" , 0 },
	{ 120 , "Surface of Revolution" , 0 },
	{ 122 , "Tabulated Cylinder" , 0 },
	{ 123 , "Direction" , 0 },
	{ 124 , "Transformation Matrix (3X4)" , 0 },
	{ 125 , "Flash" , 0 },
	{ 126 , "Rational B-Spline Curve" , 0 },
	{ 128 , "Rational B-Spline Surface" , 0 },
	{ 130 , "Offset Curve" , 0 },
	{ 132 , "Connect Point" , 0 },
	{ 134 , "Node" , 0 },
	{ 136 , "Finite Element" , 0 },
	{ 138 , "Nodal Displacement and Rotation" , 0 },
	{ 140 , "Offset Surface" , 0 },
	{ 142 , "Curve on a Parametric Surface" , 0 },
	{ 144 , "Trimmed Parametric Surface" , 0 },
	{ 150 , "Block" , 0 },
	{ 152 , "Right Angular Wedge" , 0 },
	{ 154 , "Right Circular Cylinder" , 0 },
	{ 156 , "Right Circular Cone Frustum" , 0 },
	{ 158 , "Sphere" , 0 },
	{ 160 , "Torus" , 0 },
	{ 162 , "Revolution Primitive" , 0 },
	{ 164 , "Linear Sketch Extrusion" , 0 },
	{ 168 , "Ellipsoid" , 0 },
	{ 180 , "Boolean Tree" , 0 },
	{ 184 , "Assembly Primitive" , 0 },
	{ 186 , "Manifold Boundary Representation" , 0 },
	{ 190 , "Plane Surface" , 0 },
	{ 202 , "Angular Dimension" , 0 },
	{ 206 , "Diameter Dimension" , 0 },
	{ 208 , "Flag Note" , 0 },
	{ 210 , "General Label" , 0 },
	{ 212 , "General Note" , 0 },
	{ 214 , "Leader (Arrow)" , 0 },
	{ 216 , "Linear Dimension" , 0 },
	{ 218 , "Ordinate Dimension" , 0 },
	{ 220 , "Point Dimension" , 0 },
	{ 222 , "Radius Dimension" , 0 },
	{ 228 , "General Symbol" , 0 },
	{ 230 , "Sectioned Area" , 0 },
	{ 302 , "Associative Definition" , 0 },
	{ 304 , "Line Font Definition" , 0 },
	{ 306 , "MACRO Definition" , 0 },
	{ 308 , "Subfigure Definition" , 0 },
	{ 310 , "Text Font Definition" , 0 },
	{ 312 , "Text Display Template" , 0 },
	{ 314 , "Color Definition" , 0 },
	{ 320 , "Network Subfigure Definition" , 0 },
	{ 322 , "Attribute Definition" , 0 },
	{ 402 , "Associativity Instance" , 0 },
	{ 404 , "Drawing" , 0 },
	{ 406 , "Property" , 0 },
	{ 408 , "Singular Subfigure Instance" , 0 },
	{ 410 , "View" , 0 },
	{ 412 , "Rectangular Array Subfigure" , 0 },
	{ 414 , "Circular Array Subfigure" , 0 },
	{ 416 , "External Reference" , 0 },
	{ 418 , "Nodal Load/Constraint" , 0 },
	{ 420 , "Network Subfigure Instance" , 0 },
	{ 422 , "Attribute Instance" , 0 },
	{ 430 , "Primitive Instance" , 0 },
	{ 502 , "Vertex List" , 0 },
	{ 504 , "Edge List" , 0 },
	{ 508 , "Loop" , 0 },
	{ 510 , "Face" , 0 },
	{ 514 , "Shell" , 0 },
	{ 600 , "MACRO Instance" , 0 },
	{ 700 , "Transformation Matrix (4X4)" , 0 }
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
