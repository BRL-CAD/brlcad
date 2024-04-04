/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2011 Robert McNeel & Associates. All rights reserved.
// OpenNURBS, Rhinoceros, and Rhino3D are registered trademarks of Robert
// McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "../opennurbs_public_examples.h"

#include "../example_userdata/example_ud.h"

#define INTERNAL_INITIALIZE_MODEL(model) Internal_SetExampleModelProperties(model,OPENNURBS__FUNCTION__,__FILE__)

static void Internal_SetExampleModelProperties(
  ONX_Model& model,
  const char* function_name,
  const char* source_file_name
  )
{
  const bool bHaveFunctionName = (nullptr != function_name && 0 != function_name[0]);
  if ( !bHaveFunctionName )
    function_name = "";

  const bool bHaveFileName = (nullptr != source_file_name && 0 != source_file_name[0]);
  if (!bHaveFileName)
    source_file_name = "";

  model.m_sStartSectionComments = "This was file created by OpenNURBS toolkit example code.";

  // set application information
  const ON_wString wide_function_name(function_name);
  const ON_wString wide_source_file_name(source_file_name);
  model.m_properties.m_Application.m_application_name
    = bHaveFunctionName
    ? ON_wString::FormatToString(L"OpenNURBS toolkit Example: %ls() function", static_cast<const wchar_t*>(wide_function_name))
    : ON_wString(L"OpenNURBS Examples");

  model.m_properties.m_Application.m_application_URL = L"http://www.opennurbs.org";
  model.m_properties.m_Application.m_application_details
    = bHaveFileName
    ? ON_wString::FormatToString(L"Opennurbs examples are in the file %ls.", static_cast<const wchar_t*>(wide_source_file_name))
    : ON_wString::FormatToString(L"Opennurbs examples are example_*.cpp files.");

  // some notes
  if (bHaveFunctionName && bHaveFileName)
  {
    model.m_properties.m_Notes.m_notes
      = ON_wString::FormatToString(
        L"This .3dm file was made with the OpenNURBS toolkit example function %s() defined in source code file %ls.",
        static_cast<const wchar_t*>(wide_function_name),
        static_cast<const wchar_t*>(wide_source_file_name));
    model.m_properties.m_Notes.m_bVisible = model.m_properties.m_Notes.m_notes.IsNotEmpty();
  }
  
  // set revision history information
  model.m_properties.m_RevisionHistory.NewRevision();
}

static bool Internal_WriteExampleModel(
  const ONX_Model& model,
  const wchar_t* filename,
  ON_TextLog& error_log
)
{
  int version = 0;

  // writes model to archive
  return model.Write( filename, version, &error_log );
}

ON_3dmObjectAttributes* Internal_CreateManagedAttributes(
  int layer_index,
  const wchar_t* name
)
{
  ON_3dmObjectAttributes* attributes = new ON_3dmObjectAttributes();
  attributes->m_layer_index = layer_index;
  attributes->m_name = name;
  return attributes;
}

static bool write_points_example( const wchar_t* filename, ON_TextLog& error_log  )
{
  // example demonstrates how to write a singe points and point clouds
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  // file settings (units, tolerances, views, ...)
  // OPTIONAL - change values from defaults
  model.m_settings.m_ModelUnitsAndTolerances.m_unit_system = ON::LengthUnitSystem::Meters;
  model.m_settings.m_ModelUnitsAndTolerances.m_absolute_tolerance = 0.01;
  model.m_settings.m_ModelUnitsAndTolerances.m_angle_tolerance = ON_PI/180.0; // radians
  model.m_settings.m_ModelUnitsAndTolerances.m_relative_tolerance = 0.01; // 1%

  // layer table
  // define some layers
  model.AddDefaultLayer(nullptr, ON_Color::UnsetColor);
  const int point1_layer_index = model.AddLayer(L"my layer",ON_Color::Black);
  const int pointcloud_layer_index = model.AddLayer(L"red points",ON_Color::SaturatedRed);
  const int point2_layer_index = model.AddLayer(L"one blue point",ON_Color::SaturatedBlue);

  // we'll put 2 red and one blue point in a group
  ON_Group group;
  group.SetName(L"group of points");
  group.SetIndex(0);
  model.AddModelComponent(group, true);


  // single point at (1,4,5) on default layer
  ON_Point* point1 = new ON_Point(ON_3dPoint( 1.0, 4.0, 5.0 ));
  point1->AttachUserData( new CExampleWriteUserData("write_points_example()-point1") );
  model.AddManagedModelGeometryComponent( 
    point1,
    Internal_CreateManagedAttributes(point1_layer_index,L"first point")
  );

  // point "cloud" with 3 points on red point cloud layer
  ON_PointCloud* pointcloud = new ON_PointCloud();
  pointcloud->AppendPoint(ON_3dPoint( 1.0, 6.0, 5.0 ));
  pointcloud->AppendPoint(ON_3dPoint( 1.5, 4.5, 6.0 ));
  pointcloud->AppendPoint(ON_3dPoint( 2.0, 5.0, 7.0 ));
  pointcloud->AttachUserData( new CExampleWriteUserData("write_points_example()-pointcloud") );
  ON_3dmObjectAttributes* pointcloud_attributes = Internal_CreateManagedAttributes(pointcloud_layer_index, L"3 points");
  pointcloud_attributes->AddToGroup(group.Index());
  model.AddManagedModelGeometryComponent( 
    pointcloud,
    pointcloud_attributes
  );

  // single point at (3,2,4) on red point layer and in group with the pointcloud
  ON_Point* point2 = new ON_Point(ON_3dPoint( 3.0, 2.0, 4.0  ));
  ON_3dmObjectAttributes* point2_attributes = Internal_CreateManagedAttributes(point2_layer_index, L"last point");
  point2_attributes->AddToGroup(group.Index());
  point2->AttachUserData( new CExampleWriteUserData("write_points_example()-point2") );
  model.AddManagedModelGeometryComponent( point2, point2_attributes);

  return Internal_WriteExampleModel(model, filename, error_log);
}

static bool write_curves_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // example demonstrates how to write a NURBS curve, line, and circle
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);
  
  // file settings (units, tolerances, views, ...)
  model.m_settings.m_ModelUnitsAndTolerances.m_unit_system = ON::LengthUnitSystem::Inches;
  model.m_settings.m_ModelUnitsAndTolerances.m_absolute_tolerance = 0.001;
  model.m_settings.m_ModelUnitsAndTolerances.m_angle_tolerance = ON_PI/180.0; // radians
  model.m_settings.m_ModelUnitsAndTolerances.m_relative_tolerance = 0.01; // 1%

  // add some layers
  model.AddDefaultLayer(nullptr, ON_Color::UnsetColor);
  const int line_layer_index = model.AddLayer(L"line layer",ON_Color::Black);
  const int wiggle_layer_index = model.AddLayer(L"green NURBS wiggle",ON_Color::SaturatedGreen);
  const int circles_layer_index = model.AddLayer(L"blue circles",ON_Color::SaturatedBlue);
  
  {
    // add a line
    ON_Object* managed_line = new ON_LineCurve( ON_Line( ON_3dPoint(1.0,2.0,-1.5), ON_3dPoint(5.0,3.0,2.0) ) );
    model.AddManagedModelGeometryComponent(
      managed_line,
      Internal_CreateManagedAttributes(line_layer_index, L"straight line curve")
      );
  }

  {
    // add a wiggly cubic curve
    ON_NurbsCurve* wiggle = new ON_NurbsCurve(
      3, // dimension
      false, // true if rational
      4,     // order = degree+1
      6      // number of control vertices
      );
    int i;
    for ( i = 0; i < wiggle->CVCount(); i++ ) {
      ON_3dPoint pt( 2*i, -i, (i-3)*(i-3) ); // pt = some 3d point
      wiggle->SetCV( i, pt );
    }

    // ON_NurbsCurve's have order+cv_count-2 knots.
    wiggle->SetKnot(0, 0.0);
    wiggle->SetKnot(1, 0.0);
    wiggle->SetKnot(2, 0.0);
    wiggle->SetKnot(3, 1.5);
    wiggle->SetKnot(4, 2.3);
    wiggle->SetKnot(5, 4.0);
    wiggle->SetKnot(6, 4.0);
    wiggle->SetKnot(7, 4.0);

    model.AddManagedModelGeometryComponent(
      wiggle,
      Internal_CreateManagedAttributes(wiggle_layer_index, L"wiggly cubic curve")
      );
  }

  {
    // add two circles
    ON_ArcCurve* circle1 = new ON_ArcCurve( ON_Circle( ON_3dPoint(1.0,2.0,-1.5), 3.0 ) );
    model.AddManagedModelGeometryComponent(
      circle1,
      Internal_CreateManagedAttributes(circles_layer_index, L"radius 3 circle")
      );

    ON_ArcCurve* circle2 = new ON_ArcCurve( ON_Circle( ON_3dPoint(1.0,2.0,-1.5), 5.0 ) );
    model.AddManagedModelGeometryComponent(
      circle2,
      Internal_CreateManagedAttributes(circles_layer_index, L"radius 5 circle")
      );
  }

  return Internal_WriteExampleModel(model, filename, error_log);
}


static bool write_surfaces_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // example demonstrates how to write a NURBS surface
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  //////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////

  // The code between the comment bands has nothing to do with I/O.
  // It is simply an easy way to get a NURBS surface to write.
  const int bIsRational = false;
  const int dim = 3;
  const int u_degree = 2;
  const int v_degree = 3;
  const int u_cv_count = 3;
  const int v_cv_count = 5;

  // The knot vectors do NOT have the 2 superfluous knots
  // at the start and end of the knot vector.  If you are
  // coming from a system that has the 2 superfluous knots,
  // just ignore them when writing a 3dm file.
  double u_knot[ u_cv_count + u_degree - 1 ];
  double v_knot[ v_cv_count + v_degree - 1 ];

  // make up a quadratic knot vector with no interior knots
  u_knot[0] = u_knot[1] = 0.0;
  u_knot[2] = u_knot[3] = 1.0;

  // make up a cubic knot vector with one simple interior knot
  v_knot[0] = v_knot[1] = v_knot[2] = 0.0;
  v_knot[3] = 1.5;
  v_knot[4] = v_knot[5] = v_knot[6] = 2.0;

  // Rational control points can be in either homogeneous
  // or euclidean form. Non-rational control points do not
  // need to specify a weight.  
  ON_3dPoint CV[u_cv_count][v_cv_count];

  int i, j;
  for ( i = 0; i < u_cv_count; i++ ) {
    for ( j = 0; j < v_cv_count; j++ ) {
      CV[i][j].x = i;
      CV[i][j].y = j;
      CV[i][j].z = i-j;
    }
  }

  // write a line on the default layer
  ON_NurbsSurface nurbs_surface( dim, bIsRational, 
                        u_degree+1, v_degree+1,
                        u_cv_count, v_cv_count );

  for ( i = 0; i < nurbs_surface.KnotCount(0); i++ )
    nurbs_surface.SetKnot( 0, i, u_knot[i] );

  for ( j = 0; j < nurbs_surface.KnotCount(1); j++ )
    nurbs_surface.SetKnot( 1, j, v_knot[j] );

  for ( i = 0; i < nurbs_surface.CVCount(0); i++ ) {
    for ( j = 0; j < nurbs_surface.CVCount(1); j++ ) {
      nurbs_surface.SetCV( i, j, CV[i][j] );
    }
  }

  model.AddModelGeometryComponent(&nurbs_surface, nullptr);
  //   model.AddDefaultLayer(L"NURBS surface", ON_Color::UnsetColor);

  return Internal_WriteExampleModel(model, filename, error_log);
}


static bool write_mesh_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // example demonstrates how to create and write a mesh
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  model.AddDefaultLayer(L"mesh", ON_Color::Black);

  // create a mesh to write
  // The mesh is a pyramid with 4 triangular sides and a quadranglar 
  // base.  The mesh has 5 vertices and 5 faces.  
  // The side faces share normals at their common vertices.  The
  // quadrangular base has normals different from the side normal.
  // Coincident vertices that have distinct normals must be
  // duplicated in the vertex list.
  //
  // The apex will be at (1,1.5,4) with normal (0,0,1).
  // The base corners will be at (0,0,0), (0,2,0), (2,3,0), (0,3,0).


  bool bHasVertexNormals = true; // we will specify vertex normals
  bool bHasTexCoords = false;    // we will not specify texture coordinates
  const int vertex_count = 5+4;  // 4 duplicates for different base normals
  const int face_count = 5; // 4 triangle sides and a quad base
  ON_Mesh mesh( face_count, vertex_count, bHasVertexNormals, bHasTexCoords);

  // The SetVertex(), SetNormal(), SetTCoord() and SetFace() functions
  // return true if successful and false if input is illegal.  It is
  // a good idea to inspect this returned value.

  // vertex #0: apex location and normal
  mesh.SetVertex( 0, ON_3dPoint(1.0,  1.5,  5.0) );
  mesh.SetVertexNormal( 0, ON_3dVector(0.0,  0.0,  1.0) );

  // vertex #1: SW corner vertex for sides
  mesh.SetVertex( 1, ON_3dPoint(0.0,  0.0,  0.0) );
  mesh.SetVertexNormal( 1, ON_3dVector(-1.0, -1.0,  0.0) ); // set normal will unitize if needed

  // vertex #2: SE corner vertex for sides
  mesh.SetVertex( 2, ON_3dPoint(2.0,  0.0,  0.0) );
  mesh.SetVertexNormal( 2, ON_3dVector(+1.0, -1.0,  0.0) );

  // vertex #3: NE corner vertex for sides
  mesh.SetVertex( 3, ON_3dPoint(2.0,  3.0,  0.0) );
  mesh.SetVertexNormal( 3, ON_3dVector(+1.0, +1.0,  0.0) );

  // vertex #4: NW corner vertex for sides
  mesh.SetVertex( 4, ON_3dPoint(0.0,  3.0,  0.0) );
  mesh.SetVertexNormal( 4, ON_3dVector(-1.0, +1.0,  0.0) );

  // vertex #5: SW corner vertex for base
  mesh.SetVertex( 5, ON_3dPoint(0.0,  0.0,  0.0) ); // == location of v1
  mesh.SetVertexNormal( 5, ON_3dVector(0.0,  0.0, -1.0) );

  // vertex #6: SE corner vertex for base
  mesh.SetVertex( 6, ON_3dPoint(2.0,  0.0,  0.0) ); // == location of v2
  mesh.SetVertexNormal( 6, ON_3dVector(0.0,  0.0, -1.0) );

  // vertex #7: SW corner vertex for base
  mesh.SetVertex( 7, ON_3dPoint(2.0,  3.0,  0.0) ); // == location of v3
  mesh.SetVertexNormal( 7, ON_3dVector(0.0,  0.0, -1.0) );

  // vertex #8: SW corner vertex for base
  mesh.SetVertex( 8, ON_3dPoint(0.0,  3.0,  0.0) ); // == location of v4
  mesh.SetVertexNormal( 8, ON_3dVector(0.0,  0.0, -1.0) );

  // faces have vertices ordered counter-clockwise

  // South side triangle
  mesh.SetTriangle( 0,   1, 2, 0 );

  // East side triangle
  mesh.SetTriangle( 1,   2, 3, 0 );

  // North side triangle
  mesh.SetTriangle( 2,   3, 4, 0 );

  // West side triangle
  mesh.SetTriangle( 3,   4, 1, 0 );

  // last face is quadrangular base
  mesh.SetQuad( 4,   5, 8, 7, 6 );

  if ( !mesh.HasVertexNormals() )
    mesh.ComputeVertexNormals();

  //////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////

  // Avoid copying the mesh - useful technique for large objects
  model.AddModelGeometryComponentForExperts(false, &mesh, false, nullptr, true);

  return Internal_WriteExampleModel(model, filename, error_log);
}

static bool write_mesh_with_material_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // example demonstrates how to create and write a mesh
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  model.AddDefaultLayer(L"mesh", ON_Color::Black);

  const bool bResolveIdAndNameConflicts = true;
  
  // example demonstrates how to create and write a mesh that uses
  // a rendering material.  You may want to study write_mesh_example() 
  // before examining this function.
  //
  // The key to attaching a texture is to create a mesh
  // with texture coordinates, create a material using the
  // texture as a bitmap, and to attach the material to the
  // mesh before writing the mesh.

  // rendering material with a texture map.
  ON_Material material;
  material.SetIndex(0);
  material.SetAmbient(  ON_Color(  40,  40,  40 ) );
  material.SetDiffuse(  ON_Color( 220, 220, 220 ) );
  material.SetEmission( ON_Color::Black );
  material.SetSpecular( ON_Color( 180, 180, 180 ) );

  material.SetShine( 0.35*ON_Material::MaxShine ); // 0 = flat
                                                        // MaxShine() = shiney

  material.SetTransparency( 0.2 );  // 0 = opaque, 1 = transparent

  // Texture and bump bitmaps can be Windows bitmap (.BMP), Targa (.TGA),
  // JPEG (.JPG), PCX or PNG files.  Version 1 of Rhino will not support
  // filenames using unicode or multibyte character sets.  As soon as
  // Rhino supports these character sets, the const char* filename will
  // changed to const _TCHAR*.

  // For Rhino to find the texture bitmap, the .3dm file and the
  // .bmp file need to be in the same directory.
  ON_Texture texture;
  texture.m_image_file_reference.SetRelativePath(L"./example_texture.bmp");
  material.AddTexture( texture );

  // The render material name is a string used to identify rendering
  // materials in RIB, POV, OBJ, ..., files.  In Rhino, the render
  // material name is set with the SetObjectMaterial command and can
  // be viewed in the Info tab of the dialog displayed by the
  // Properties command.
  material.SetName( L"my render material" );

  model.AddModelComponent(material, bResolveIdAndNameConflicts);

  bool bHasVertexNormals = false; // we will NOT specify vertex normals
  bool bHasTexCoords = true;      // we will specify texture coordinates
  const int vertex_count = 40;
  const int face_count = 28;
  ON_Mesh mesh( face_count, vertex_count, bHasVertexNormals, bHasTexCoords);

  // The SetVertex(), SetNormal(), SetTextureCoord() and SetQuad() functions
  // return true if successful and false if input is illegal.  It is
  // a good idea to inspect this returned value.

  // cook up a 5 x 8 grid of vertices
  int vertex_index = 0;
  int i, j;
  for ( i = 0; i <= 4; i++ ) {
    for ( j = 0; j <= 7; j++ ) {
      ON_3fPoint v( (float)i, (float)j, (float)(sin( 2.0*3.14159*j/7.0 ) + cos( 3.14159*i/4.0 )) );
      mesh.SetVertex( vertex_index, v ); // 3d location

      // normalized texture coordinate
      double tcoord_u = i/4.0;
      double tcoord_v = j/7.0;
      mesh.SetTextureCoord( vertex_index, tcoord_u, tcoord_v ); // 2d texture coordinate

      vertex_index++;
    }
  }

  // faces have vertices ordered counter-clockwise

  // cook up a 4 x 7 grid of quadrangular faces
  int face_index = 0;
  for ( i = 0; i < 4; i++ ) {
    for ( j = 0; j < 7; j++ ) {
      int vi[4]; // indices of corner vertices
      vi[0] = i*8 + j;  // vertex at "lower left" corner
      vi[1] = vi[0]+8;  // vertex at "lower right" corner
      vi[2] = vi[1]+1;  // vertex at "upper left" corner
      vi[3] = vi[0]+1;  // vertex at "upper right" corner
      mesh.SetQuad( face_index, vi[0], vi[1], vi[2], vi[3] );
      face_index++;
    }
  }

  // Most applications expect vertex normals.
  // If they are not present, ComputeVertexNormals sets
  // them by averaging face normals.
  if ( !mesh.HasVertexNormals() )
    mesh.ComputeVertexNormals();

  ON_3dmObjectAttributes attributes;
  attributes.m_name = "my mesh with material";
  attributes.m_material_index = 0;
  attributes.SetMaterialSource( ON::material_from_object );

  const bool bManagedGeometry = false; // mesh not copied
  const bool bManagedAttributes = false; // attributes not copied
  model.AddModelGeometryComponentForExperts(bManagedGeometry, &mesh, bManagedAttributes, &attributes, bResolveIdAndNameConflicts);

  return Internal_WriteExampleModel(model, filename, error_log);
}

static bool write_spot_light_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // create a blue spotlight shining on a white plane
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  // white material for surface
  ON_Material material;
  material.SetIndex(0);
  material.SetAmbient(  ON_Color::White );
  material.SetDiffuse(  ON_Color::White );
  material.SetEmission( ON_Color::Black );
  material.SetSpecular( ON_Color::White );
  material.SetShine( 0.35*ON_Material::MaxShine ); // 0 = flat
                                                        // MaxShine() = shiney
  material.SetTransparency( 0.0 );  // 0 = opaque, 1 = transparent
  material.SetName( L"white material" );
  
  model.AddModelComponent(material);

  model.AddDefaultLayer(nullptr, ON_Color::UnsetColor);
  const int surfaces_layer_index = model.AddLayer(L"surfaces", ON_Color::Black);
  const int lights_layer_index = model.AddLayer(L"lights", ON_Color::Black);


  // spotlight
  ON_Light light;
  light.SetLightIndex(0);
  light.SetLocation( ON_3dPoint(2.0, 3.0, 10.0) );
  light.SetDirection( ON_3dVector(-1.0, -1.0, -10.0) );
  light.SetDiffuse( ON_Color::SaturatedBlue );
  light.SetAmbient( ON_Color::Black );
  light.SetSpecular( ON_Color::SaturatedBlue );
  light.SetSpotExponent( 60 );    // 0 = hard, 128 = soft
  light.SetSpotAngleDegrees( 30.0 );
  light.SetStyle(ON::world_spot_light);

  light.SetLightName( "Blue spot light" );

  ON_3dmObjectAttributes light_attributes;
  light_attributes.Default();
  light_attributes.m_layer_index = lights_layer_index; // spotlights layer we defined above
  light_attributes.m_name = "Blue spot light";

  model.AddModelGeometryComponent(&light, &light_attributes);


  // quick and dirty plane
  ON_PlaneSurface plane (ON_xy_plane);
  plane.SetDomain( 0, -10.0, +10.0 );
  plane.SetDomain( 1, -10.0, +10.0 );

  ON_3dmObjectAttributes plane_attributes;
  plane_attributes.Default();
  plane_attributes.m_layer_index = surfaces_layer_index; // surfaces layer we defined above
  plane_attributes.m_material_index = material.Index(); // white material we defined above
  plane_attributes.SetMaterialSource(ON::material_from_object);
  plane_attributes.m_name = "20x20 plane";

  model.AddModelGeometryComponent(&plane, &plane_attributes);

  return Internal_WriteExampleModel(model, filename, error_log);
}


static bool write_viewport_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // create a model with 7 viewports
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  // views will display space inside the sphere
  ON_Sphere sphere ( ON_origin, 10.0 );

  // Writes a 7 viewport layout - 3 along the right side,
  // 3 along the left side, and 1 big on in the middle
  // that displays the space inside the sphere.

  // Viewports have a "target" point inside of the view frustum.
  // This target is the center of view rotations.

  // Viewports have a "construction plane".  This plane is
  // (optionally) displayed as a grid.
  //

  // OPTIONAL - change values from defaults
  model.m_settings.m_ModelUnitsAndTolerances.m_unit_system = ON::LengthUnitSystem::Millimeters;
  model.m_settings.m_ModelUnitsAndTolerances.m_absolute_tolerance = 0.01;
  model.m_settings.m_ModelUnitsAndTolerances.m_angle_tolerance = ON_PI/180.0; // radians
  model.m_settings.m_ModelUnitsAndTolerances.m_relative_tolerance = 0.01; // 1%

  // reserve room for 7 views
  model.m_settings.m_views.Reserve(7);

  // some values needed to fill in view information
  const double pos_x[4]  = {0.0,0.25,0.75,1.0};       // x: 0 = left, 1 = right
  const double pos_y[4]  = {0.0,1.0/3.0,2.0/3.0,1.0}; // y: 0 = top, 1 = bottom
  ON_3dVector camDir;
  double fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far;
  double target_distance;

  fr_left   = -sphere.radius;
  fr_right  =  sphere.radius;
  fr_bottom = -sphere.radius;
  fr_top    =  sphere.radius;
  fr_near         = 2.0*sphere.radius; // Rhino's default
  target_distance = 3.0*sphere.radius;
  fr_far          = 4.0*sphere.radius;

  // view number 1
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = -ON_zaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_yaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "+X+Y parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[0];
    view.m_position.m_wnd_right = pos_x[1];
    view.m_position.m_wnd_top = pos_y[2];
    view.m_position.m_wnd_bottom = pos_y[3];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane = ON_xy_plane;
  }

  // view number 2
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = ON_yaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_zaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "+X+Z parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[0];
    view.m_position.m_wnd_right = pos_x[1];
    view.m_position.m_wnd_top = pos_y[1];
    view.m_position.m_wnd_bottom = pos_y[2];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane = ON_zx_plane;
  }

  // view number 3
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = -ON_xaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_zaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "+Y+Z parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[0];
    view.m_position.m_wnd_right = pos_x[1];
    view.m_position.m_wnd_top = pos_y[0];
    view.m_position.m_wnd_bottom = pos_y[1];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane = ON_yz_plane;
  }

  // view number 4
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = ON_zaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_yaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "-X+Y parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[2];
    view.m_position.m_wnd_right = pos_x[3];
    view.m_position.m_wnd_top = pos_y[2];
    view.m_position.m_wnd_bottom = pos_y[3];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane.CreateFromFrame( ON_origin, -ON_xaxis, ON_yaxis );
  }

  // view number 5
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = -ON_yaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_zaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "-X+Z parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[2];
    view.m_position.m_wnd_right = pos_x[3];
    view.m_position.m_wnd_top = pos_y[1];
    view.m_position.m_wnd_bottom = pos_y[2];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane.CreateFromFrame( ON_origin, -ON_xaxis, ON_zaxis );
  }

  // view number 6
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    view.m_vp.SetProjection( ON::parallel_view );
    camDir = ON_xaxis;
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_zaxis );
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "-Y+Z parallel";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[2];
    view.m_position.m_wnd_right = pos_x[3];
    view.m_position.m_wnd_top = pos_y[0];
    view.m_position.m_wnd_bottom = pos_y[1];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane.CreateFromFrame( ON_origin, -ON_yaxis, ON_zaxis );
  }

  // view number 7
  {
    ON_3dmView& view = model.m_settings.m_views.AppendNew();

    // set primary view transformation information first
    target_distance = 10.0*sphere.radius;
    const double tan_half_angle = sphere.radius / target_distance;

    view.m_vp.SetProjection( ON::perspective_view );
    camDir = ON_3dVector(-40.0,75.0,-50.0);
    view.m_vp.SetCameraLocation( sphere.Center() - target_distance*camDir );
    view.m_vp.SetCameraDirection( camDir );
    view.m_vp.SetCameraUp( ON_zaxis );
    fr_near   = (target_distance - sphere.radius)/10.0;
    fr_far    = target_distance + 1.5*sphere.radius;
    double d  = fr_near*tan_half_angle;
    fr_left   = -d;
    fr_right  =  d;
    fr_bottom = -d;
    fr_top    =  d;
    view.m_vp.SetFrustum( fr_left, fr_right, fr_bottom, fr_top, fr_near, fr_far );

    view.SetTargetPoint(sphere.Center());

    // secondary view "fluff"
    view.m_name = "skew perspective";

    // position of viewport inside main window
    view.m_position.m_wnd_left = pos_x[1];
    view.m_position.m_wnd_right = pos_x[2];
    view.m_position.m_wnd_top = pos_y[0];
    view.m_position.m_wnd_bottom = pos_y[3];

    // construction plane
    view.m_cplane.Default(); // default grid settings
    view.m_cplane.m_plane = ON_xy_plane;
  }

  // Add a sphere
  model.AddDefaultLayer(L"sphere", ON_Color::Black);
  model.AddManagedModelGeometryComponent( sphere.RevSurfaceForm( true, nullptr ), nullptr );

  return Internal_WriteExampleModel(model, filename, error_log);
}

static void make_trimming_curves( ON_Brep& brep, 
                                  const ON_2dPoint& A2, // start point in parameter space
                                  const ON_2dPoint& B2, // end point in parameter space
                                  const ON_3dPoint& A3, // start point in parameter space
                                  const ON_3dPoint& B3  // end point in parameter space
                                  )
{
  ON_LineCurve* p2dCurve = new ON_LineCurve( A2, B2 );
  ON_LineCurve* p3dCurve = new ON_LineCurve( A3, B3 );

  // it is not necessary for the domains of the 2d and 3d curves
  // to match, but it makes it easier to understand the brep
  ON_Interval domain = p3dCurve->Domain();
  p2dCurve->SetDomain( domain.Min(), domain.Max() );

  brep.m_C2.Append(p2dCurve);

  brep.m_C3.Append(p3dCurve);
}


static bool write_trimmed_surface_example( const wchar_t* filename, ON_TextLog& error_log )
{
  // write a trimmed surface
  ONX_Model model;
  INTERNAL_INITIALIZE_MODEL(model);

  model.AddDefaultLayer(L"trimmed surface", ON_Color::Black);

  // trimmed surfaces are written as a CRhinoBrep that has
  // a single surface and a single CRhinoBrepFace.
  //
  // Trimming loops are simple closed curves and are oriented
  // so that the active portion of the trimmed surface's
  // domain lies to the left of the trimming curves.

  ON_Brep brep;
  ON_2dPoint q;

  // Create a 10x10 plane surface at z=3 with domain [0,1]x[0,1]
  ON_PlaneSurface* pSurface = new ON_PlaneSurface( ON_Plane( ON_3dPoint( 0, 0,3), 
                                                             ON_3dPoint(10,10,3), 
                                                             ON_3dPoint(10, 0,3) ) );
  pSurface->SetDomain(0,0.0,10.0);
  pSurface->SetDomain(1,0.0,10.0);

  // ~ON_Brep() will delete this surface
  const int si = brep.m_S.Count(); // index of surface
  brep.m_S.Append(pSurface);

  // create simple trimming triangle
  ON_2dPoint A2(1.0, 2.0); // parameter space locations of 2d trim corners
  ON_2dPoint B2(9.0, 1.5);
  ON_2dPoint C2(7.0, 8.0);

  ON_3dPoint A3 = pSurface->PointAt(A2.x,A2.y);
  ON_3dPoint B3 = pSurface->PointAt(B2.x,B2.y);
  ON_3dPoint C3 = pSurface->PointAt(C2.x,C2.y);

  make_trimming_curves( brep, A2, B2, A3, B3 ); // creates 2d and 3d curve
  make_trimming_curves( brep, B2, C2, B3, C3 );
  make_trimming_curves( brep, C2, A2, C3, A3 );

  // there are vertices at the 3 corners
  brep.NewVertex( pSurface->PointAt( A2.x, A2.y ) );
  brep.NewVertex( pSurface->PointAt( B2.x, B2.y ) );
  brep.NewVertex( pSurface->PointAt( C2.x, C2.y ) );

  // the vertices are exact since we have lines on a plane
  brep.m_V[0].m_tolerance = 0.0;
  brep.m_V[1].m_tolerance = 0.0;
  brep.m_V[2].m_tolerance = 0.0;

  // there are 3 edges along the sides of the triangle
  brep.NewEdge( brep.m_V[0], brep.m_V[1], 0 ); // start vertex, end vertex, 3d curve index
  brep.NewEdge( brep.m_V[1], brep.m_V[2], 1 ); // start vertex, end vertex, 3d curve index
  brep.NewEdge( brep.m_V[2], brep.m_V[0], 2 ); // start vertex, end vertex, 3d curve index

  // the edges are exact since we have lines on a plane
  brep.m_E[0].m_tolerance = 0.0;
  brep.m_E[1].m_tolerance = 0.0;
  brep.m_E[2].m_tolerance = 0.0;

  // there is 1 face
  ON_BrepFace& face = brep.NewFace( si );

  // outer boundary trimming loops
  ON_BrepLoop& loop = brep.NewLoop( ON_BrepLoop::outer, face );

  // geometrically, loops are made from a contiguous list of 2d parameter space
  // curves that form a simple closed curve.
  brep.NewTrim( brep.m_E[0], false, loop, 0 ); // A to B
  brep.NewTrim( brep.m_E[1], false, loop, 1 ); // B to C
  brep.NewTrim( brep.m_E[2], false, loop, 2 ); // C to A

  // the trims are exact since we have lines on a plane
  q = brep.m_C2[0]->PointAtStart();
  //brep.m_T[0].m_P[0] = pSurface->PointAt(q.x,q.y);
  q = brep.m_C2[0]->PointAtEnd();
  //brep.m_T[0].m_P[1] = pSurface->PointAt(q.x,q.y);
  brep.m_T[0].m_type = ON_BrepTrim::boundary;
  brep.m_T[0].m_tolerance[0] = 0.0;
  brep.m_T[0].m_tolerance[1] = 0.0;

  q = brep.m_C2[0]->PointAtStart();
  //brep.m_T[1].m_P[0] = pSurface->PointAt(q.x,q.y);
  q = brep.m_C2[0]->PointAtEnd();
  //brep.m_T[1].m_P[1] = pSurface->PointAt(q.x,q.y);
  brep.m_T[1].m_type = ON_BrepTrim::boundary;
  brep.m_T[1].m_tolerance[0] = 0.0;
  brep.m_T[1].m_tolerance[1] = 0.0;

  q = brep.m_C2[0]->PointAtStart();
  //brep.m_T[2].m_P[0] = pSurface->PointAt(q.x,q.y);
  q = brep.m_C2[0]->PointAtEnd();
  //brep.m_T[2].m_P[1] = pSurface->PointAt(q.x,q.y);
  brep.m_T[2].m_type = ON_BrepTrim::boundary;
  brep.m_T[2].m_tolerance[0] = 0.0;
  brep.m_T[2].m_tolerance[1] = 0.0;

  // when debugging your code, IsValid(), IsSolid(), IsManifold() are useful
  // to check.

  model.AddModelGeometryComponent(&brep, nullptr);


  return Internal_WriteExampleModel(model, filename, error_log);
}

//int main ( int argc, const char* argv[] )
int main ()
{
  bool rc = false;
  const wchar_t* filename;

  ON::Begin();
  // If you want to learn to write b-rep models, first work through
  // this example paying close attention to write_trimmed_surface_example(),
  // then examime example_brep.cpp.

  // errors printed to stdout
  ON_TextLog error_log;

  // messages printed to stdout
  ON_TextLog message_log;

  // errors logged in text file
  //FILE* error_log_fp = ON::OpenFile("error_log.txt","w");
  //ON_TextLog error_log(error_log_fp);
  filename = L"my_points.3dm";
  rc = write_points_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_curves.3dm";
  rc = write_curves_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_surfaces.3dm";
  rc = write_surfaces_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_mesh.3dm";
  rc = write_mesh_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_mesh_with_material.3dm";
  rc = write_mesh_with_material_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_spot_light.3dm";
  rc = write_spot_light_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_viewports.3dm";
  rc = write_viewport_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  filename = L"my_trimmed_surface.3dm";
  rc = write_trimmed_surface_example( filename, error_log );
  if (rc)
    message_log.Print(L"Successfully wrote %ls.\n",filename);
  else
    message_log.Print(L"Errors while writing %ls.\n",filename);

  ON::End();

  return 0;
}
