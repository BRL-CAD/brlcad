/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

ON_OBJECT_IMPLEMENT( ON_Point, ON_Geometry, "C3101A1D-F157-11d3-BFE7-0010830122F0" );

BOOL ON_Point::IsValid( ON_TextLog* text_log ) const 
{
  return TRUE;
}

void ON_Point::Dump( ON_TextLog& dump ) const
{
  dump.Print("ON_Point: ");
  dump.Print(point);
  dump.Print("\n");
}

BOOL ON_Point::Write( ON_BinaryArchive& file ) const
{
  BOOL rc = file.Write3dmChunkVersion(1,0);
  if (rc) rc = file.WritePoint( point );
  return rc;
}

BOOL ON_Point::Read( ON_BinaryArchive& file )
{
  int major_version = 0;
  int minor_version = 0;
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if (rc && major_version==1) {
    // common to all 1.x versions
    rc = file.ReadPoint(point);
  }
  return rc;
}

ON::object_type ON_Point::ObjectType() const
{
  return ON::point_object;
}

int ON_Point::Dimension() const
{
  return 3;
}

BOOL ON_Point::GetBBox( double* boxmin, double* boxmax, BOOL bGrowBox ) const
{
  return ON_GetPointListBoundingBox( 3, 0, 1, 3, &point.x, boxmin, boxmax, bGrowBox?true:false );
}

bool ON_Point::IsDeformable() const
{
  return true;
}

bool ON_Point::MakeDeformable()
{
  return true;
}

BOOL ON_Point::Transform( const ON_Xform& xform )
{
  TransformUserData(xform);
  return ON_TransformPointList(3,0,1,3,&point.x,xform);
}

BOOL ON_Point::SwapCoordinates( int i, int j )
{
  return ON_SwapPointListCoordinates( 1, 3, &point.x, i, j );
}

ON_Point::ON_Point() : point(0.0,0.0,0.0) 
{}

ON_Point::ON_Point(const ON_Point& src) : ON_Geometry(src), point(src.point) 
{}

ON_Point::ON_Point(const ON_3dPoint& pt) : point(pt) 
{}

ON_Point::ON_Point(double x,double y,double z) : point(x,y,z)
{}

ON_Point::~ON_Point() 
{}

ON_Point& ON_Point::operator=(const ON_Point& src) 
{
  if ( this != &src ) {
    ON_Geometry::operator=(src);
    point=src.point;
  }
  return *this;
}

ON_Point& ON_Point::operator=(const ON_3dPoint& pt) 
{
  point=pt;
  return *this;
}

ON_Point::operator double*() 
{
  return &point.x;
}

ON_Point::operator const double*() const 
{
  return &point.x;
}

ON_Point::operator ON_3dPoint*() 
{
  return &point;
}

ON_Point::operator const ON_3dPoint*() const 
{
  return &point;
}

ON_Point::operator ON_3dPoint&() 
{
  return point;
}

ON_Point::operator const ON_3dPoint&() const 
{
  return point;
}
