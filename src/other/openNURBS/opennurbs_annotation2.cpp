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

ON_VIRTUAL_OBJECT_IMPLEMENT( ON_Annotation2,       ON_Geometry,    "8D820224-BC6C-46b4-9066-BF39CC13AEFB");
ON_OBJECT_IMPLEMENT( ON_LinearDimension2,  ON_Annotation2, "BD57F33B-A1B2-46e9-9C6E-AF09D30FFDDE");
ON_OBJECT_IMPLEMENT( ON_RadialDimension2,  ON_Annotation2, "B2B683FC-7964-4e96-B1F9-9B356A76B08B");
ON_OBJECT_IMPLEMENT( ON_AngularDimension2, ON_Annotation2, "841BC40B-A971-4a8e-94E5-BBA26D67348E");
ON_OBJECT_IMPLEMENT( ON_TextEntity2,       ON_Annotation2, "46F75541-F46B-48be-AA7E-B353BBE068A7");
ON_OBJECT_IMPLEMENT( ON_Leader2,           ON_Annotation2, "14922B7A-5B65-4f11-8345-D415A9637129");
ON_OBJECT_IMPLEMENT( ON_TextDot,           ON_Geometry,    "74198302-CDF4-4f95-9609-6D684F22AB37");
ON_OBJECT_IMPLEMENT( ON_OrdinateDimension2,ON_Annotation2, "C8288D69-5BD8-4f50-9BAF-525A0086B0C3");

// class ON_Annotation2
//--------------------------------------------------------------------

int ON_Annotation2::Index() const
{
  return m_index;
}

void ON_Annotation2::SetIndex( int index)
{
  m_index = index;
}

void ON_Annotation2::Create()
{
  m_type = ON::dtNothing;
  m_textdisplaymode = ON::dtAboveLine;
  m_plane = ON_xy_plane;
  m_points.Empty();
  m_usertext.Empty();
  m_userpositionedtext = false;
  m_index = -1;
  m_textheight = 1.0;
}

void ON_Annotation2::Destroy()
{
  // 10-27-03 LW memory leak prevention
  m_points.Empty();
  m_usertext.Empty();
  m_type = ON::dtNothing;
  m_plane = ON_xy_plane;
  m_userpositionedtext = false;
}

void ON_Annotation2::EmergencyDestroy()
{
  m_points.EmergencyDestroy();
  m_usertext.EmergencyDestroy();
}

ON_Annotation2::ON_Annotation2()
{
  Create();
}

ON_Annotation2::~ON_Annotation2()
{
  Destroy();
}

bool ON_Annotation2::EvaluatePoint( const ON_ObjRef& objref, ON_3dPoint& P) const
{
  bool rc = false;
  switch( objref.m_component_index.m_type )
  {
  case ON_COMPONENT_INDEX::dim_linear_point:
  case ON_COMPONENT_INDEX::dim_radial_point:
  case ON_COMPONENT_INDEX::dim_angular_point:
  case ON_COMPONENT_INDEX::dim_ordinate_point:
  case ON_COMPONENT_INDEX::dim_text_point:
    {
      ON_2dPoint uv = Point(objref.m_component_index.m_index);
      if ( uv.IsValid() )
      {
        P = m_plane.PointAt(uv.x,uv.y);
        rc = true;
      }
    }
    break;
  }
  if (!rc)
  {
    P = ON_UNSET_POINT;
  }
  return rc;
}

// convert from old style annotation
ON_Annotation2& ON_Annotation2::operator=(const ON_Annotation& src)
{
  // get a clean and empty "this"
  Destroy();
  Create();
  ON_Geometry::operator=(src);

  m_type = src.Type();
  m_textdisplaymode = src.TextDisplayMode();
  m_plane = src.Plane();

  // 13 November 2006 Dale Lear
  //    Copying 5 points isn't always the right
  //    thing to do.  Sometimes there should be
  //    no points or fewer points.  I'm leaving
  //    this unchnaged for now and fixing the
  //    immediate bugs downstream.  When we
  //    have a larger window (> 1 week before
  //    release) for beta testing, I'll change
  //    this to copy the number of points
  //    that actually exist in src
  m_points.Reserve(5);
  for( int i = 0; i < 5; i++)
    SetPoint( 0, src.Point( i));


  m_usertext = src.UserText();
  m_userpositionedtext = src.UserPositionedText()?true:false;
  m_index = 0;
  m_textheight = 1.0;

  return *this;
}

ON_Annotation2::ON_Annotation2(const ON_Annotation& src)
{
  Create();
  *this = src;
}


BOOL ON_Annotation2::IsValid( ON_TextLog* text_log ) const
{
  if ( !m_plane.IsValid() )
  {
    if ( text_log )
    {
      text_log->Print("ON_Annotation2 - m_plane is not valid\n");
    }
    return false;
  }

  const int points_count = m_points.Count();

  int i;
  for ( i = 0; i < points_count; i++ )
  {
    if ( !m_points[i].IsValid() )
    {
      if ( text_log )
      {
        text_log->Print("ON_Annotation2 - m_points[%d] is not valid.\n");
      }
      return false;
    }
  }

  switch ( m_type )
  {
  case ON::dtDimLinear:
  case ON::dtDimAligned:
  case ON::dtDimAngular:
  case ON::dtDimDiameter:
  case ON::dtDimRadius:
  case ON::dtLeader:
  case ON::dtTextBlock:
  case ON::dtDimOrdinate:
    break;

  default:
    if ( text_log )
    {
      text_log->Print("ON_Annotation2 - m_type = %d is not a valid enum value\n",m_type);
    }
    return false;
    break;
  }

  return true;
}

BOOL ON_LinearDimension2::IsValid( ON_TextLog* text_log ) const
{
  if ( m_type != ON::dtDimLinear && m_type != ON::dtDimAligned )
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - m_type !=  ON::dtDimLinear or ON::dtDimAligned.\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( m_points.Count() != 5 )
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - m_points.Count() = %d (should be 5).\n",m_points.Count());
    }
    return false;
  }

  if ( m_points[1].x != m_points[0].x )
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - m_points[1].x = %g != %g = m_points[0].x (should be equal)\n",
                      m_points[1].x, m_points[0].x
                      );
    }
    return false;
  }

  if ( m_points[3].x != m_points[2].x )
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - m_points[3].x = %g != %g = m_points[2].x\n",
                      m_points[3].x, m_points[2].x
                     );
    }
    return false;
  }

  if ( m_points[3].y != m_points[1].y )
  {
    if ( text_log )
    {
      text_log->Print("ON_LinearDimension2 - m_points[3].y = %g != %g = m_points[1].y\n",
                      m_points[3].y, m_points[1].y
                      );
    }
    return false;
  }

  return true;
}


BOOL ON_RadialDimension2::IsValid( ON_TextLog* text_log ) const
{
  if ( m_type != ON::dtDimRadius && m_type != ON::dtDimDiameter  )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - m_type !=  ON::dtDimRadius or ON::dtDimDiameter\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_RadialDimension2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( 4 != m_points.Count() )
  {
    if ( text_log )
    {
      text_log->Print("ON_RadialDimension2 - m_points.Count() = %d (should be 4 or 5)\n", m_points.Count() );
    }
    return false;
  }

  return true;
}


BOOL ON_AngularDimension2::IsValid( ON_TextLog* text_log ) const
{
  if ( m_type != ON::dtDimAngular )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - m_type !=  ON::dtDimAngular\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( 4 != m_points.Count() )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - m_points.Count() = %d (should be 4)\n", m_points.Count() );
    }
    return false;
  }

  if ( !ON_IsValid(m_angle) || m_angle <= 0.0 || m_angle > 2.0*ON_PI )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - bogus m_angle = %g\n",m_angle);
    }
    return false;
  }

  if ( !ON_IsValid(m_radius) || m_radius <= 0.0 )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - bogus m_radius = %g\n",m_radius);
    }
    return false;
  }

  if ( 0.0 == m_points[1].x && 0.0 == m_points[1].y )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - angle dim m_points[1] = center (should be on start ray).\n");
    }
    return false;
  }

  if ( 0.0 == m_points[2].x && 0.0 == m_points[2].y )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - angle dim m_points[2] = center (should be on end ray).\n");
    }
    return false;
  }

  if ( 0.0 == m_points[3].x && 0.0 == m_points[3].y )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - angle dim m_points[3] = center (should be on interior of arc).\n");
    }
    return false;
  }

  double a1 = atan2(m_points[1].y, m_points[1].x);
  double a2 = atan2(m_points[2].y, m_points[2].x);
  double a3 = atan2(m_points[3].y, m_points[3].x);
  if ( a1 < 0.0 )
    a1 += 2.0*ON_PI;
  while ( a2 <= a1 )
    a2 += 2.0*ON_PI;
  while ( a3 <= a1 )
    a3 += 2.0*ON_PI;

  if ( fabs(m_angle - (a2-a1)) > ON_ZERO_TOLERANCE + m_angle*ON_SQRT_EPSILON )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - m_angle = %g != %g = (end angle - start angle)\n",m_angle,a2-a1);
    }
    return false;
  }

  double r = ON_2dVector(m_points[3]).Length();
  if ( fabs(r - m_radius) > ON_ZERO_TOLERANCE + m_radius*ON_SQRT_EPSILON )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - m_radius = %g != %g = |m_point[3])|\n",m_radius,r);
    }
    return false;
  }
  

  if ( a3 >= a2 )
  {
    if ( text_log )
    {
      text_log->Print("ON_AngularDimension2 - angle dim m_points[3] = not on arc interior.\n");
    }
    return false;
  }

  return true;
}



BOOL ON_TextEntity2::IsValid( ON_TextLog* text_log ) const
{
  if ( m_type != ON::dtTextBlock )
  {
    if ( text_log )
    {
      text_log->Print("ON_TextEntity2 - m_type !=  ON::dtTextBlock\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_TextEntity2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( 0 != m_points.Count() )
  {
    if ( text_log )
    {
      text_log->Print("ON_TextEntity2 - m_points.Count() = %d (should be 0)\n", m_points.Count() );
    }
    return false;
  }

  return true;
}

BOOL ON_Leader2::IsValid( ON_TextLog* text_log ) const
{
  if ( m_type != ON::dtLeader )
  {
    if ( text_log )
    {
      text_log->Print("ON_Leader2 - m_type !=  ON::dtLeader\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_Leader2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( m_points.Count() < 2 )
  {
    if ( text_log )
    {
      text_log->Print("ON_Leader2 - m_points.Count() = %d (should be >= 2)\n", m_points.Count() );
    }
    return false;
  }

  return true;
}


BOOL ON_LinearDimension2::GetBBox(
        double* boxmax,
        double* boxmin,
        BOOL bGrowBox
        ) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  if ( 5 == m_points.Count() )
  {
    ON_3dPointArray P(5);
    ON_2dPoint uv;
    if ( m_userpositionedtext )
    {
      uv = m_points[0]; // point someplace in text
      P.Append( m_plane.PointAt(uv.x,uv.y) );
    }

    P.Append( m_plane.origin );

    uv.x = 0.0;
    uv.y = m_points[1].y;
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[2];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv.y = m_points[1].y;
    P.Append( m_plane.PointAt(uv.x,uv.y) );
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}




BOOL ON_RadialDimension2::GetBBox(
        double* boxmax,
        double* boxmin,
        BOOL bGrowBox
        ) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  if ( 4 == m_points.Count() )
  {
    ON_3dPointArray P(4);
    ON_2dPoint uv;
    if ( m_userpositionedtext )
    {
      uv = m_points[0]; // point someplace in text
      P.Append( m_plane.PointAt(uv.x,uv.y) );
    }

    P.Append( m_plane.origin ); // + sign at center of dimension

    uv = m_points[1];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[2];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[3];
    P.Append( m_plane.PointAt(uv.x,uv.y) );
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}





BOOL ON_AngularDimension2::GetBBox(
        double* boxmax,
        double* boxmin,
        BOOL bGrowBox
        ) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  ON_Arc arc;
  if ( GetArc(arc) )
  {
    if ( arc.GetTightBoundingBox(bbox,bGrowBox?true:false,0) )
      bGrowBox = true;    
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}





BOOL ON_TextEntity2::GetBBox(
        double* boxmax,
        double* boxmin,
        BOOL bGrowBox
        ) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  if ( 1 == m_points.Count() )
  {
    ON_2dPoint uv = m_points[0];
    bbox.Set( m_plane.PointAt(uv.x,uv.y), bGrowBox );
    bGrowBox = true;
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}





BOOL ON_Leader2::GetBBox(
        double* boxmax,
        double* boxmin,
        BOOL bGrowBox
        ) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  const int point_count = m_points.Count();
  if ( point_count > 0 )
  {
    ON_3dPointArray P(point_count);
    int i;
    for ( i = 0; i < point_count; i++ )
    {
      ON_2dPoint uv = m_points[i];
      P.Append( m_plane.PointAt(uv.x,uv.y));
    }
    if ( P.GetBoundingBox(bbox,bGrowBox?true:false) )
      bGrowBox = true;
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}



bool ON_LinearDimension2::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( 5 == m_points.Count() )
  {
    ON_3dPointArray P(5);
    ON_2dPoint uv;
    if ( m_userpositionedtext )
    {
      uv = m_points[0]; // point someplace in text
      P.Append( m_plane.PointAt(uv.x,uv.y) );
    }

    P.Append( m_plane.origin );

    uv.x = 0.0;
    uv.y = m_points[1].y;
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[2];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv.y = m_points[1].y;
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform ) )
      bGrowBox = true;
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);
}

bool ON_RadialDimension2::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( 5 == m_points.Count() )
  {
    ON_3dPointArray P(5);
    ON_2dPoint uv;

    uv = m_points[0]; // + sign at center of dimension (usually at (0,0)
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[1];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[2];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    uv = m_points[3];
    P.Append( m_plane.PointAt(uv.x,uv.y) );

    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform ) )
      bGrowBox = true;
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);
}

bool ON_AngularDimension2::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  ON_Arc arc;
  if ( GetArc(arc) )
  {
    if ( arc.GetTightBoundingBox(tight_bbox,bGrowBox,xform) )
      bGrowBox = true;    
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);

}

bool ON_TextEntity2::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( 1 == m_points.Count() )
  {
    ON_3dPointArray P(1);
    P.Append( m_plane.PointAt(m_points[0].x,m_points[0].y) );
    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform ) )
      bGrowBox = true;
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);
}

bool ON_Leader2::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  const int point_count = m_points.Count();
  if ( point_count >= 2 )
  {
    ON_3dPointArray P(point_count);
    int i;
    for ( i = 0; i < point_count; i++ )
    {
      ON_2dPoint uv = m_points[i];
      P.Append( m_plane.PointAt(uv.x,uv.y));
    }
    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform ) )
      bGrowBox = true;
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);
}

/*
bool ON_LinearDimension2::GetAnnotationBoundingBox(
          const ON_DimStyle* dimstyle,
          ON_2dPoint text0,
          ON_2dPoint text1,
			    ON_BoundingBox& tight_bbox, 
          int bGrowBox,
			    const ON_Xform* xform
        ) const
{
  if ( 5 == m_points.Count() )
  {
    // Get 5 points that are always in the 
    ON_3dPointArray P(10);
    ON_2dPoint uv0, uv1, uv2, uv3;

    P.Append( m_plane.origin );

    uv0 = m_points[0];
    uv1 = uv0;
    uv1.y = m_points[1].y;
    uv2 = m_points[2];
    uv3.x = uv2.x;
    uv3.y = uv1.y;
    P.Append( m_plane.PointAt(uv0.x,uv0.y) );
    P.Append( m_plane.PointAt(uv1.x,uv1.y) );
    P.Append( m_plane.PointAt(uv2.x,uv2.y) );
    P.Append( m_plane.PointAt(uv3.x,uv3.y) );

    if ( dimstyle )
    {
      // The ends of the displayed extension lines 
      // have their "y" values adjusted by the dimstyle.
      const double el_offset    = dimstyle->ExtOffset();
      const double el_extension = dimstyle->ExtExtension();

      const double v0 = (uv0.y < uv1.y) ? -1.0 : 1.0;
      uv0.y += v0*el_offset;
      uv1.y += v0*el_extension;

      const double v1 = (uv2.y < uv3.y) ? -1.0 : 1.0;
      uv2.y += v1*el_offset;
      uv3.y += v1*el_extension;

      P.Append( m_plane.PointAt(uv0.x,uv0.y) );
      P.Append( m_plane.PointAt(uv1.x,uv1.y) );
      P.Append( m_plane.PointAt(uv2.x,uv2.y) );
      P.Append( m_plane.PointAt(uv3.x,uv3.y) );
    }

    ON_2dPoint corners[4];
    if ( GetAnnotationText2dBoundingBox(dimstyle,text0,text1,corners) )
    {
      P.Append( m_plane.PointAt(corners[0].x,corners[0].y) );
      P.Append( m_plane.PointAt(corners[1].x,corners[1].y) );
      P.Append( m_plane.PointAt(corners[2].x,corners[2].y) );
      P.Append( m_plane.PointAt(corners[3].x,corners[3].y) );
    }

    // TODO Add arrowheads
    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform ) )
      bGrowBox = true;
  }
  else if ( bGrowBox && !tight_bbox.IsValid() )
  {
    // invalid linear dimension
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return (0!=bGrowBox);
}
*/

BOOL ON_Annotation2::Write( ON_BinaryArchive& file ) const
{
  // NOTE WELL - If you make any changes to this code,
  //             you break file IO for some annotation
  //             objects.  Please discuss all changes
  //             with Dale Lear BEFORE you check in any
  //             changes.

  int i;
  BOOL rc = file.Write3dmChunkVersion( 1, 0 );

  i = m_type;
  if( rc) rc = file.WriteInt( i);
  i = m_textdisplaymode;
  if( rc) rc = file.WriteInt( i);
  if( rc) rc = file.WritePlane( m_plane);

  ON_2dPointArray points = m_points;
  int bUserPositionedText = m_userpositionedtext?1:0;
  switch( m_type )
  {
  case ON::dtDimAligned:
  case ON::dtDimLinear:
    if ( 4 == points.Count() )
    {
      // so old versions will read enough points.
      points.AppendNew();
      points[4].Set(0.5*(points[0].x + points[2].x),points[1].y);
      bUserPositionedText = false;
    }
    break;

  case ON::dtDimAngular:
    //user positioned text is supported.
    break;

  case ON::dtDimRadius:
  case ON::dtDimDiameter:
    // 9 August 2005 Dale Lear - radial dimensions do
    // not support user postioned text.  The never have
    // in Rhino, but the old files had 5 points in them.
    if ( 4 == points.Count() )
    {
      // so old versions will read enough points.
      points.AppendNew();
    }
    if ( points.Count() >= 5 )
    {
      points[4] = points[2];
    }
    bUserPositionedText = false;
    break;

  default:
    bUserPositionedText = false;
    break;
  }
  if( rc) rc = file.WriteArray( points);
  if( rc) rc = file.WriteString( m_usertext);
  // 7-9-03 lw removed extra text string getting written
  if( rc) rc = file.WriteInt( bUserPositionedText );
  if( rc) rc = file.WriteInt( m_index);
  if( rc) rc = file.WriteDouble( m_textheight);

  // NOTE WELL - If you make any changes to this code,
  //             you break file IO for some annotation
  //             objects.  Please discuss all changes
  //             with Dale Lear BEFORE you check in any
  //             changes.

  return rc;
}

BOOL ON_Annotation2::Read( ON_BinaryArchive& file )
{
  // NOTE WELL - If you make any changes to this code,
  //             you break file IO for some annotation
  //             objects.  Please discuss all changes
  //             with Dale Lear BEFORE you check in any
  //             changes.

  Destroy();
  int major_version = 0;
  int minor_version = 0;
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if ( rc && major_version == 1 )
  {
    int i;
    if (rc)
    {
      rc = file.ReadInt( &i);
      if( rc) 
        m_type = ON::AnnotationType( i);
    }
    if( rc) 
    {
      rc = file.ReadInt( &i);
      if( rc) 
        m_textdisplaymode = ON::eTextDisplayMode( i);
    }
    if( rc) rc = file.ReadPlane( m_plane);
    if( rc) rc = file.ReadArray( m_points);
    if( rc) rc = file.ReadString( m_usertext);
    i = 0;
    if( rc) rc = file.ReadInt( &i );
    if (rc) m_userpositionedtext = i ? true : false;
    if( rc) rc = file.ReadInt( &m_index);
    if( rc) rc = file.ReadDouble( &m_textheight);

    switch( m_type )
    {
    case ON::dtDimAligned:
    case ON::dtDimLinear:
      if ( m_points.Count() < 5 )
      {
        m_userpositionedtext = false;
      }
      break;

    case ON::dtDimAngular:
      if ( m_points.Count() <= 0 )
      {
        m_userpositionedtext = false;
      }
      break;

    case ON::dtDimRadius:
    case ON::dtDimDiameter:
      // 9 August 2005 Dale Lear - radial dimensions do
      // not support user postioned text.  The never have
      // in Rhino, but the old files had 5 points in them.
      if ( 5 == m_points.Count() )
      {
        m_points.SetCount(4);
      }
      m_userpositionedtext = false;
      break;

    default:
      m_userpositionedtext = false;
      break;
    }

  }

  // NOTE WELL - If you make any changes to this code,
  //             you break file IO for some annotation
  //             objects.  Please discuss all changes
  //             with Dale Lear BEFORE you check in any
  //             changes.

  return rc;
}

ON::object_type ON_Annotation2::ObjectType() const
{
  return ON::annotation_object;
}

int ON_Annotation2::Dimension() const
{
  return 3;
}

BOOL ON_Annotation2::Transform( const ON_Xform& xform )
{
  ON_Geometry::Transform(xform);

  ON_2dPoint p;
  ON_Xform scalexf;
  double scale = xform.Determinant();
  scale = fabs( scale);
  if( fabs( scale - 1.0) > ON_SQRT_EPSILON && fabs( scale) > ON_SQRT_EPSILON)
  {
    scale = pow( scale, 1.0/3.0);
    scalexf.Scale( scale, scale, scale);
    for( int i = 0; i < m_points.Count(); i++)
    {
      p = Point( i);
      p.Transform( scalexf);
      SetPoint( i, p);
    }
    // This scales the text height on text but not on dimensions
    if( ON_Annotation2::IsText())
    {
      SetHeight( scale * Height());
    }
  }

  return m_plane.Transform( xform );
 
}

int ON_Plane_Repair(ON_Plane& plane)
{
  int rc;
  if ( plane.IsValid() )
  {
    rc = 1;
  }
  else
  {
    rc = 2;
    if (!plane.origin.IsValid())
    {
      plane.origin.Set(0.0,0.0,0.0);
    }

    bool bGoodX = (plane.xaxis.IsValid() && !plane.xaxis.IsZero() );
    bool bGoodY = (plane.yaxis.IsValid() && !plane.yaxis.IsZero() );
    bool bGoodZ = (plane.zaxis.IsValid() && !plane.zaxis.IsZero() );
    if ( bGoodX )
    {
      if ( fabs(plane.xaxis.Length()-1.0) > ON_SQRT_EPSILON )
        plane.xaxis.Unitize();
    }
    if ( bGoodY )
    {
      if ( fabs(plane.yaxis.Length()-1.0) > ON_SQRT_EPSILON )
        plane.yaxis.Unitize();
    }
    if ( bGoodZ )
    {
      if ( fabs(plane.zaxis.Length()-1.0) > ON_SQRT_EPSILON )
        plane.zaxis.Unitize();
    }

    if ( bGoodZ )
    {
      double x = bGoodX ? fabs(plane.zaxis*plane.xaxis) : 99.0;
      double y = bGoodX ? fabs(plane.zaxis*plane.yaxis) : 99.0;
      if ( x <= ON_SQRT_EPSILON )
      {
        if ( y > ON_SQRT_EPSILON )
        {
          plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
          plane.yaxis.Unitize();
        }
      }
      else if ( y <= ON_SQRT_EPSILON )
      {
        plane.xaxis = ON_CrossProduct(plane.yaxis,plane.zaxis);
        plane.xaxis.Unitize();
      }
      else if ( x <= y && x < 1.0 )
      {
        plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
        if( plane.yaxis.Unitize() )
        {
          plane.xaxis = ON_CrossProduct(plane.yaxis,plane.zaxis);
          plane.xaxis.Unitize();
        }
        else if ( y < 1.0 )
        {
          plane.CreateFromNormal( plane.origin, plane.zaxis );
        }
      }
      else if ( y < 1.0 )
      {
        plane.xaxis = ON_CrossProduct(plane.yaxis,plane.zaxis);
        if( plane.xaxis.Unitize() )
        {
          plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
          plane.yaxis.Unitize();
        }
        else
        {
          plane.CreateFromNormal( plane.origin, plane.zaxis );
        }
      }
    }
    else if ( bGoodX )
    {
      if ( bGoodY )
      {
        plane.zaxis = ON_CrossProduct(plane.xaxis,plane.yaxis);
        if ( plane.zaxis.Unitize() )
        {
          if ( fabs(plane.yaxis*plane.xaxis) > ON_SQRT_EPSILON )
          {
            plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
            plane.yaxis.Unitize();
          }
        }
        else
        {
          plane.yaxis.PerpendicularTo(plane.xaxis);
          plane.yaxis.Unitize();
          plane.zaxis = ON_CrossProduct(plane.xaxis,plane.yaxis);
          plane.zaxis.Unitize();
        }
      }
      else
      {
        plane.yaxis.PerpendicularTo(plane.xaxis);
        plane.yaxis.Unitize();
        plane.zaxis = ON_CrossProduct(plane.xaxis,plane.yaxis);
        plane.zaxis.Unitize();
      }
    }
    else if ( bGoodY )
    {
      plane.zaxis.PerpendicularTo(plane.yaxis);
      plane.zaxis.Unitize();
      plane.xaxis = ON_CrossProduct(plane.yaxis,plane.zaxis);
      plane.xaxis.Unitize();
    }
    else
    {
      plane.xaxis.Set(1.0,0.0,0.0);
      plane.yaxis.Set(0.0,1.0,0.0);
      plane.zaxis.Set(0.0,0.0,1.0);
    }
    plane.UpdateEquation();
  }
  return rc;
}

int ON_LinearDimension2::Repair()
{
  // returns 0 = unable to repair
  //         1 = in perfect condtion
  //         2 == repaired.

  const int ext0_pt_index   = ON_LinearDimension2::ext0_pt_index;
  const int arrow0_pt_index = ON_LinearDimension2::arrow0_pt_index;
  const int ext1_pt_index   = ON_LinearDimension2::ext1_pt_index;
  const int arrow1_pt_index = ON_LinearDimension2::arrow1_pt_index;
  const int userpositionedtext_pt_index = ON_LinearDimension2::userpositionedtext_pt_index;
  const int dim_pt_count    = ON_LinearDimension2::dim_pt_count;

  int rc = 0;
  if (    m_points.Count() >= dim_pt_count 
       && m_points[ext0_pt_index].IsValid() 
       && m_points[ext1_pt_index].IsValid() )
  {
    rc = 1;
    if ( !m_plane.IsValid() )
    {
      rc = ON_Plane_Repair(m_plane);
    }

    if ( m_points.Count() > dim_pt_count )
    {
      rc = 2;
      m_points.SetCount(dim_pt_count);
    }

    // m_points[ext0_pt_index] must be at (0,0)
    ON_2dVector v = m_points[ext0_pt_index];
    double d;
    int i;
    if ( !v.IsZero() )
    {
      rc = 2;
      m_plane.origin = m_plane.PointAt(v.x,v.y);
      m_plane.UpdateEquation();
      v.Reverse();
      for ( i = 0; i < dim_pt_count; i++ )
      {
        m_points[i] += v;
      }
      m_points[ext0_pt_index].Set(0.0,0.0);
    }

    if ( ON::dtDimAligned == m_type && (m_points[ext1_pt_index].x < 0.0 || m_points[ext1_pt_index].y != 0.0) )
    {
      rc = 2;
      // Aligned dims must have m_points[ext1_pt_index].x = m_points[ext0_pt_index].x
      if (   m_points[ext1_pt_index].x > 100.0*ON_SQRT_EPSILON
          && fabs(m_points[ext1_pt_index].y) <= ON_SQRT_EPSILON )
      {
        m_points[ext1_pt_index].y = 0.0;
      }
      else
      {
        // Dimension line parallel to point0 -> point2
        // rotate the plane so p2 is on the x axis
        v = m_points[ext1_pt_index];
        d = v.Length();
        v.Unitize();
        m_plane.Rotate(v.y,v.x,m_plane.zaxis,m_plane.origin);

        // rotate points in opposite direction
        v.y = -v.y; 
        for ( i = 0; i < dim_pt_count; i++ )
        {
          ON_2dPoint p = m_points[i];
          m_points[i].Set( v.x*p.x - v.y*p.y, v.y*p.x + v.x*p.y );
        }
        m_points[ext0_pt_index].Set(0.0,0.0);
        m_points[ext1_pt_index].Set(d,0.0);
      }
    }
    else if ( ON::dtDimLinear != m_type )
    {
      rc = 2;
      m_type = ON::dtDimLinear;
    }

    if ( m_points[arrow0_pt_index].x != m_points[ext0_pt_index].x )
    {
      rc = 2;
      m_points[arrow0_pt_index].x = m_points[ext0_pt_index].x;
    }

    if ( m_points[arrow1_pt_index].x != m_points[ext1_pt_index].x )
    {
      rc = 2;
      m_points[arrow1_pt_index].x = m_points[ext1_pt_index].x;
    }

    if ( !ON_IsValid(m_points[arrow0_pt_index].y) )
    {
      rc = 2;
      if ( !ON_IsValid(m_points[arrow1_pt_index].y) )
        m_points[arrow1_pt_index].y = 0.5*(m_points[ext0_pt_index].y + m_points[ext1_pt_index].y);
      m_points[arrow0_pt_index].y = m_points[arrow1_pt_index].y;
    }
    else if ( !ON_IsValid(m_points[arrow1_pt_index].y) )
    {
      rc = 2;
      m_points[arrow1_pt_index].y = m_points[arrow0_pt_index].y;
    }
    else if ( m_points[arrow0_pt_index].y != m_points[arrow1_pt_index].y )
    {
      rc = 2;
      d = 0.5*(m_points[arrow0_pt_index].y + m_points[arrow1_pt_index].y);
      m_points[arrow0_pt_index].y = d;
      m_points[arrow1_pt_index].y = d;
    }

    if ( m_userpositionedtext )
    {
      if ( !m_points[userpositionedtext_pt_index].IsValid() )
      {
        rc = 2;
        m_userpositionedtext = false;
      }
    }
    
    if ( !m_userpositionedtext )
    {
      if (    m_points[userpositionedtext_pt_index].y != m_points[arrow0_pt_index].y
           || m_points[userpositionedtext_pt_index].x != 0.5*(m_points[arrow0_pt_index].x + m_points[arrow1_pt_index].x) )
      {
        rc = 2;
        m_points[userpositionedtext_pt_index].y = m_points[arrow0_pt_index].y;
        m_points[userpositionedtext_pt_index].x = 0.5*(m_points[arrow0_pt_index].x + m_points[arrow1_pt_index].x);
      }
    }

    if ( !m_plane.IsValid() )
    {
      rc = 2;
      ON_Plane_Repair(m_plane);
    }
  }
  return rc;
}

BOOL ON_LinearDimension2::Transform( const ON_Xform& xform )
{
  // Dale Lear - this override fixes RR 11114 by correctly
  //             handling non uniform scaling.
  bool rc = xform.IsIdentity();
  if ( !rc)
  {
    ON_Plane plane = m_plane;
    if ( dim_pt_count == m_points.Count() && plane.Transform( xform ) )
    {
      rc = true;
      ON_3dPoint P[dim_pt_count], Q[dim_pt_count];
      ON_2dVector p2[dim_pt_count], q2[dim_pt_count];
      int i;
      bool bUpdatePoints = false;
      for ( i = 0; i < dim_pt_count && rc; i++ )
      {
        p2[i] = m_points[i];
        P[i] = m_plane.PointAt( p2[i].x, p2[i].y );
        Q[i] = xform*P[i];
        if( !plane.ClosestPointTo(Q[i],&q2[i].x,&q2[i].y) )
          rc = false;
        if (   fabs(p2[i].x - q2[i].x) > ON_SQRT_EPSILON 
            || fabs(p2[i].y - q2[i].y) > ON_SQRT_EPSILON )
        {
          // transformation is not in SL3
          bUpdatePoints = true;
        }
      }

      if (rc)
      {
        ON_Geometry::Transform(xform);
        m_plane = plane;
        if ( bUpdatePoints )
        {
          for ( i = 0; i < dim_pt_count && rc; i++ )
          {
            m_points[i] = q2[i];
          }
          // Repair() will properly align the arrow points, etc.
          Repair();
        }
      }
    }
  }

  return rc;
}

// overrides virtual ON_Geometry::Transform()
BOOL ON_RadialDimension2::Transform( const ON_Xform& xform )
{
  // TODO fill in something that works for non-rigid transforms
  return ON_Annotation2::Transform(xform);
}

BOOL ON_Leader2::Transform( const ON_Xform& xform )
{
  bool rc = xform.IsIdentity();
  if ( !rc)
  {
    ON_Plane plane = m_plane;
    rc = plane.Transform(xform);
    if ( rc )
    {
      int i;
      const int point_count =  m_points.Count();
      ON_2dPointArray q(point_count);
      ON_2dPoint p2, q2;
      ON_3dPoint P, Q;
      bool bUpdatePoints = false;
      for ( i = 0; i < point_count && rc; i++ )
      {
        p2 = m_points[i];
        P = m_plane.PointAt( p2.x, p2.y );
        Q = xform*P;
        if( !plane.ClosestPointTo(Q,&q2.x,&q2.y) )
          rc = false;
        if ( fabs(p2.x - q2.x) <= ON_SQRT_EPSILON )
          q2.x = p2.x;
        else
          bUpdatePoints = true;
        if ( fabs(p2.y - q2.y) <= ON_SQRT_EPSILON )
          q2.y = p2.y;
        else
          bUpdatePoints = true;
        q.Append(q2);
      }

      if(rc)
      {
        ON_Geometry::Transform(xform);
        m_plane = plane;

        if ( bUpdatePoints )
          m_points = q;

        if ( m_points[0].x != 0.0 || m_points[0].y != 0.0 )
        {
          ON_2dVector v = m_points[0];
          if ( !v.IsZero() )
          {
            m_plane.origin = m_plane.PointAt(v.x,v.y);
            m_plane.UpdateEquation();
            v.Reverse();
            for ( i = 1; i < point_count; i++ )
            {
              m_points[i] += v;
            }
            m_points[0].Set(0.0,0.0);
          }
        }
      }
    }
  }
  return rc;
}

BOOL ON_AngularDimension2::Transform( const ON_Xform& xform )
{
  // Dale Lear - this override fixes RR 11114 by correctly
  //             handling non uniform scaling.
  bool rc = xform.IsIdentity();
  if ( !rc)
  {
    ON_Plane plane = m_plane;
    if ( dim_pt_count == m_points.Count() && plane.Transform( xform ) )
    {
      rc = true;
      ON_3dPoint P[dim_pt_count], Q[dim_pt_count], A[3], B[3];
      ON_2dVector p2[dim_pt_count], q2[dim_pt_count], a[3], b[3];
      double r[3];
      int i;
      bool bUpdatePoints = false;
      double a0 = 0.0;
      double a1 = m_angle;
      a[0].Set( m_radius*cos(a0), m_radius*sin(a0) );
      a[1].Set( m_radius*cos(0.5*(a0+a1)), m_radius*sin(0.5*(a0+a1)) );
      a[2].Set( m_radius*cos(a1), m_radius*sin(a1) );
      for ( i = 0; i < dim_pt_count && rc; i++ )
      {
        p2[i] = m_points[i];
        P[i] = m_plane.PointAt( p2[i].x, p2[i].y );
        Q[i] = xform*P[i];
        if( !plane.ClosestPointTo(Q[i],&q2[i].x,&q2[i].y) )
          rc = false;
        if (   fabs(p2[i].x - q2[i].x) > ON_SQRT_EPSILON 
            || fabs(p2[i].y - q2[i].y) > ON_SQRT_EPSILON )
        {
          // transformation is not a rigid motion
          bUpdatePoints = true;
        }
      }

      for ( i = 0; i < 3 && rc; i++ )
      {
        A[i] = m_plane.PointAt( a[i].x, a[i].y );
        B[i] = xform*A[i];
        if( !plane.ClosestPointTo(B[i],&b[i].x,&b[i].y) )
          rc = false;
        r[i] = B[i].DistanceTo(plane.origin);
        if (   fabs(a[i].x - b[i].x) > ON_SQRT_EPSILON 
            || fabs(a[i].y - b[i].y) > ON_SQRT_EPSILON )
        {
          // transformation is not a rigid motion
          bUpdatePoints = true;
        }
        if ( r[i] < ON_SQRT_EPSILON )
          rc = false;
        else if ( r[i] > m_radius*(1.0+ON_SQRT_EPSILON) )
          bUpdatePoints = true;
        else if ( r[i] < m_radius*(1.0-ON_SQRT_EPSILON) )
          bUpdatePoints = true;
      }

      if (rc)
      {
        if ( bUpdatePoints )
        {
          ON_3dVector X = B[0] - plane.origin;
          X.Unitize();

          ON_3dVector Y1 = B[1] - plane.origin;
          Y1.Unitize();
          ON_3dVector Z1 = ON_CrossProduct(X,Y1);
          double z1 = Z1.Length();
          Z1.Unitize();

          ON_3dVector Y2 = B[2] - plane.origin;
          Y2.Unitize();
          ON_3dVector Z2 = ON_CrossProduct(X,Y2);
          double z2 = Z2.Length();
          Z2.Unitize();

          if ( z2 >= z1 && z2 >= 0.05 )
          {
            plane.xaxis = X;
            plane.zaxis = Z2;
            if ( Z1*Z2 < 0.0 )
              plane.zaxis.Reverse();
            plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
            plane.yaxis.Unitize();
          }
          else if ( z1 >= 0.05 )
          {
            plane.xaxis = X;
            plane.zaxis = Z1;
            plane.yaxis = ON_CrossProduct(plane.zaxis,plane.xaxis);
            plane.yaxis.Unitize();
          }
          else
          {
            rc = false;
          }

          if (rc)
          {
            plane.UpdateEquation();
            ON_3dVector V = B[2] - plane.origin;
            double x = V*plane.xaxis;
            double y = V*plane.yaxis;
            double angle = atan2(y,x);
            if ( angle < 0.0 )
              angle += 2.0*ON_PI;
            double radius = (r[0]+r[1]+r[2])/3.0;

            double arc_pt_angle = 1.0/3.0;
            if ( m_angle > 0.0 && m_points[arc_pt_index].IsValid() )
            {
              arc_pt_angle = atan2(m_points[arc_pt_index].y,m_points[arc_pt_index].x);
              if ( arc_pt_angle < 0.0 ) arc_pt_angle += 2.0*ON_PI;
              if ( arc_pt_angle > m_angle )
                arc_pt_angle = 1.0/3.0;
              else
                arc_pt_angle /= m_angle;

              if ( arc_pt_angle < 0.0 ) 
                arc_pt_angle = 0.0; 
              else if ( arc_pt_angle > 1.0 ) 
                arc_pt_angle = 1.0;
            }
            arc_pt_angle *= angle;


            ON_Geometry::Transform(xform);
            m_plane = plane;
            m_radius = radius;
            m_angle = angle;
            m_points[start_pt_index].Set(m_radius,0.0);
            m_points[end_pt_index].Set(m_radius*cos(m_angle),m_radius*sin(m_angle));
            m_points[arc_pt_index].Set(m_radius*cos(arc_pt_angle),m_radius*sin(arc_pt_angle));
            if ( m_userpositionedtext )
            {
              m_plane.ClosestPointTo(Q[userpositionedtext_pt_index],
                &m_points[userpositionedtext_pt_index].x,
                &m_points[userpositionedtext_pt_index].y
                );
            }
            else
            {
              m_points[userpositionedtext_pt_index].Set(m_radius*cos(0.5*m_angle),m_radius*sin(0.5*m_angle));
            }

          }
        }
        else
        {
          ON_Geometry::Transform(xform);
          m_plane = plane;
        }
      }
    }
  }

  return rc;
}

bool ON_Annotation2::IsText() const 
{ return (ON::dtTextBlock == m_type); }

bool ON_Annotation2::IsLeader() const 
{ return (ON::dtLeader == m_type); }

bool ON_Annotation2::IsDimension() const 
{ return (ON::dtTextBlock != m_type && ON::dtLeader != m_type); }

double ON_Annotation2::NumericValue() const
{ return ON_UNSET_VALUE; }

void ON_Annotation2::SetHeight( double ht) 
{ if( ht > ON_SQRT_EPSILON) m_textheight = ht; }

double ON_Annotation2::Height() const 
{ return m_textheight; }

void ON_Annotation2::SetType( ON::eAnnotationType type ) 
{ m_type = type; }

ON::eAnnotationType ON_Annotation2::Type() const 
{ return m_type; }

void ON_Annotation2::SetPlane( const ON_Plane& plane ) 
{ m_plane = plane; }

const ON_Plane& ON_Annotation2::Plane() const 
{ return m_plane; }

void ON_Annotation2::SetPointCount( int count) 
{
  if( m_points.Count() < count)
  {
    m_points.Reserve( count);
    for( int i = m_points.Count(); i < count; i++)
      m_points.Append( ON_2dPoint());
  }
}

int ON_Annotation2::PointCount() const 
{ return m_points.Count(); }

void ON_Annotation2::SetPoints( const ON_2dPointArray& points ) 
{ m_points = points; }

const ON_2dPointArray& ON_Annotation2::Points() const 
{ return m_points; }

void ON_Annotation2::SetPoint( int idx, const ON_2dPoint& point )
{
  if ( idx >= 0 )
  {
    if ( idx < m_points.Count() )
      m_points[idx] = point;
    else if ( idx == m_points.Count() )
      m_points.Append(point);
  }
}

ON_2dPoint ON_Annotation2::Point( int idx ) const
{
  return ( idx >= 0 && idx < m_points.Count() ) 
         ? m_points[idx] 
         : ON_2dPoint( 0.0, 0.0 );
}

void ON_Annotation2::SetUserText( const wchar_t* string ) {m_usertext = string; }
const ON_wString& ON_Annotation2::UserText() const { return m_usertext; }

void ON_Annotation2::SetUserPositionedText( int bMoved ) 
{ m_userpositionedtext = bMoved?true:false; }
bool ON_Annotation2::UserPositionedText() const 
{ return m_userpositionedtext; }


// Converts 2d points in annotation to 3d WCS points
BOOL ON_Annotation2::GetECStoWCSXform( ON_Xform& xform ) const
{
  ON_3dVector z = ON_CrossProduct( m_plane.xaxis, m_plane.yaxis );
  return xform.ChangeBasis( m_plane.origin, m_plane.xaxis, m_plane.yaxis, z,
                            ON_origin, ON_xaxis, ON_yaxis, ON_zaxis );
}

// Converts from WCS 3d points to 2d points in annotation
BOOL ON_Annotation2::GetWCStoECSXform( ON_Xform& xform ) const
{
  ON_3dVector z = ON_CrossProduct( m_plane.xaxis, m_plane.yaxis );
  return xform.ChangeBasis( ON_origin, ON_xaxis, ON_yaxis, ON_zaxis,
                            m_plane.origin, m_plane.xaxis, m_plane.yaxis, z );
}

void ON_Annotation2::ReservePoints( int count)
{
  m_points.SetCapacity( count);
  m_points.SetCount( count);
}

const wchar_t* ON_Annotation2::DefaultText() { return L""; }


void ON_Annotation2::SetTextDisplayMode( ON::eTextDisplayMode mode)
{
  m_textdisplaymode = mode;
}

ON::eTextDisplayMode ON_Annotation2::TextDisplayMode() const
{
  return m_textdisplaymode;
}


void ON_Annotation2::ConvertBack( ON_Annotation& target)
{
  target.SetType( Type());
  target.SetTextDisplayMode( TextDisplayMode());
  target.SetPlane( Plane());
  target.SetPoints( Points());
  target.SetUserText( UserText());
  target.SetDefaultText( DefaultText());
  target.SetUserPositionedText( UserPositionedText());
}

void ON_Annotation2::SetJustification( unsigned int justification)
{
}

unsigned int ON_Annotation2::Justification()
{
  return 0;
}


//----- ON_LinearDimension2 ------------------------------------------
ON_LinearDimension2::ON_LinearDimension2()
{
  m_type = ON::dtDimLinear;
  m_textdisplaymode = ON::dtAboveLine;
  m_plane = ON_xy_plane;
  m_usertext = DefaultText();
  m_points.Reserve(ON_LinearDimension2::dim_pt_count);
  m_points.SetCount(ON_LinearDimension2::dim_pt_count);
  m_points.Zero();
}

ON_LinearDimension2::~ON_LinearDimension2()
{
}

double ON_LinearDimension2::NumericValue() const
{
  // Use y coords of ext points instead of 3d distance
  // to reduce noise in the answer.
  return (m_points.Count() >= dim_pt_count) 
         ? fabs(m_points[ext0_pt_index].x - m_points[ext1_pt_index].x) 
         : 0.0;
}

int ON_LinearDimension2::StyleIndex() const
{
  return ON_Annotation2::Index();
}

void ON_LinearDimension2::SetStyleIndex( int i)
{
  ON_Annotation2::SetIndex( i);
}

/*

    ext0_pt_index    = 0, // end of first extension line
    arrow0_pt_index  = 1, // arrowhead tip on first extension line
    ext1_pt_index    = 2, // end of second extension line
    arrow1_pt_index  = 3, // arrowhead tip on second extension line
    dim_pt_count     = 5, // number of m_points[] in an angular dim

    // Points calculated from values in m_points[]
    text_pivot_pt = 10000 // center of dimension text
    dim_mid_pt    = 10001 // midpoint of dimension line

*/

ON_2dPoint ON_LinearDimension2::Dim2dPoint( int point_index ) const
{
  ON_2dPoint p2;
  if ( m_points.Count() < dim_pt_count )
  {
    p2.x = p2.y = ON_UNSET_VALUE;
  }
  else
  {
    if ( text_pivot_pt == point_index )
    {
      point_index = m_userpositionedtext ? userpositionedtext_pt_index : dim_mid_pt;
    }

    const ON_2dPoint* points = m_points.Array();
    switch(point_index)
    {
    case ext0_pt_index:
      p2 = points[0];
      break;

    case arrow0_pt_index:
      p2.x = points[0].x;
      p2.y = points[1].y;
      break;

    case ext1_pt_index:
      p2 = points[2];
      break;

    case arrow1_pt_index:
      p2.x = points[2].x;
      p2.y = points[1].y;
      break;

    case userpositionedtext_pt_index:
      p2.x = points[4].x;
      p2.y = points[4].y;
      break;

    case dim_mid_pt:
      p2.x = 0.5*(points[0].x + points[2].x);
      p2.y = points[1].y;
      break;

    default:
      p2.x = p2.y = ON_UNSET_VALUE;
      break;
    }
  }
  return p2;
}

ON_3dPoint ON_LinearDimension2::Dim3dPoint( int point_index ) const
{
  ON_2dPoint p2 = Dim2dPoint(point_index);
  return (ON_UNSET_VALUE == p2.x) ? ON_UNSET_POINT : m_plane.PointAt(p2.x,p2.y);
}


// 6-23-03 lw Added v2 file writing of annotation
void ON_LinearDimension2::GetV2Form( ON_LinearDimension& dim)
{
  ON_Annotation2::ConvertBack( dim);
}



bool ON_LinearDimension2::CreateFromV2( 
    const ON_Annotation& v2_ann,
    const ON_3dmAnnotationSettings& settings,
    int dimstyle_index
    )
{
  bool rc = false;
  if ( ON::dtDimLinear == v2_ann.m_type || ON::dtDimAligned == v2_ann.m_type )
  {
    if ( v2_ann.m_points.Count() >=  ON_LinearDimension2::dim_pt_count )
    {
      m_points.Reserve(ON_LinearDimension2::dim_pt_count);
      m_points.SetCount(0);
      m_points.Append(ON_LinearDimension2::dim_pt_count,v2_ann.Points().Array());
      m_userpositionedtext = v2_ann.UserPositionedText();
      m_type = v2_ann.m_type;
      m_usertext = v2_ann.UserText();

      m_plane = v2_ann.m_plane;
      m_plane.UpdateEquation();

      switch( settings.m_textalign)
      {
      case 1:
        m_textdisplaymode =  ON::dtInLine;
        break;
      case 2:
        m_textdisplaymode = ON::dtHorizontal;
        break;
      default:
        m_textdisplaymode = ON::dtAboveLine;
        break;
      }

      ON_2dVector v = m_points[0];
      if ( !v.IsZero() )
      {
        m_plane.origin = m_plane.PointAt(v.x,v.y);
        m_plane.UpdateEquation();
        m_points[0].Set(0.0,0.0);
        v.Reverse();
        int i;
        for ( i = 1; i < ON_LinearDimension2::dim_pt_count; i++ )
        {
          m_points[i] += v;
        }
      }

      m_index = dimstyle_index;
      rc = true;
    }
  }
  return rc;
}

int ON_LinearDimension2::GetDimensionLineSegments(
    ON_RECT gdi_text_rect,
    int gdi_height_of_I,
    ON_Xform gdi_to_world,
    const ON_DimStyle& dimstyle,
    double dimscale,
    const ON_Viewport* vp,
    double x[6],
    bool& bInside
    ) const
{
  int rc = 0;

  x[0] = 0.0;
  x[1] = 0.0;
  x[2] = 0.0;
  x[3] = 0.0;
  x[4] = 0.0;
  x[5] = 0.0;
  bInside = true;

  if ( m_points.Count() < 3 )
    return 0;

  int i = (m_points[ext0_pt_index].x <= m_points[ext1_pt_index].x) 
      ? ext0_pt_index 
      : ext1_pt_index;
  const double x0 = m_points[i].x;
  const double x1 = m_points[(ext0_pt_index==i) ? ext1_pt_index : ext0_pt_index].x;
  x[0] = x0;  // left end of dimension line
  x[1] = x1;
  x[2] = x0;
  x[3] = x1;  // right end of dimension line
  x[4] = x0;
  x[5] = x1;

  if ( 0 == gdi_height_of_I )
  {
    // Default to height of Ariel 'I'
    gdi_height_of_I = (165*ON_Font::normal_font_height)/256;
  }

  if ( 0.0 == dimscale )
  {
    dimscale = 1.0;
  }

  double t;


  ON::eTextDisplayMode textdisplay = ON::TextDisplayMode(dimstyle.TextAlignment());
  if ( ON::dtHorizontal == textdisplay && !vp )
    textdisplay = ON::dtInLine;

  const double text_height_of_I    = dimscale*dimstyle.TextHeight();
  const double textgap             = fabs(dimscale*dimstyle.TextGap());
  const double gdi_to_plane_scale  = text_height_of_I/gdi_height_of_I;
  const double textwidth           = fabs(gdi_to_plane_scale*(gdi_text_rect.right - gdi_text_rect.left));
  const double arrowwidth          = dimscale*dimstyle.ArrowSize();
  const double tailwidth           = 0.5*arrowwidth;
  const double dimwidth            = x1 - x0;
  const double mindimwidth         = (ON::dtInLine == textdisplay)
                                   ? (2.0*(arrowwidth + tailwidth + textgap) + textwidth)
                                   : (2.0*arrowwidth + tailwidth);
  const double dimextension        = dimscale*dimstyle.DimExtension();

  if ( dimwidth < mindimwidth )
  {
    // arrowheads have to be "outside"
    t = arrowwidth + tailwidth;
    x[0] = x0;
    x[1] = x0-t;
    x[2] = x1+t;
    x[3] = x1;
    x[4] = x0;  // arrow tips
    x[5] = x1;
    if( dimextension != 0.0)
    {
      x[0] += dimextension;
      x[3] -= dimextension;
    }
    bInside = false;
    rc = 2;
  }
  else if ( (ON::dtHorizontal == textdisplay && vp) || m_userpositionedtext )
  {
    // use projected rectangle to clip dimension line
    double xx0, xx1, xx, y0, y1, t;
    ON_3dPoint P, R;
    ON_2dPoint corners[4];
    ON_Line ray;

    ON_3dVector vp_zaxis = (ON::dtHorizontal == textdisplay && vp)
                         ? vp->CameraZ() 
                         : m_plane.zaxis;

    const double gdi_gap = fabs(textgap/gdi_to_plane_scale);

    R.Set(gdi_text_rect.left-gdi_gap,gdi_text_rect.bottom+gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[0].x,&corners[0].y);

    R.Set(gdi_text_rect.right+gdi_gap,gdi_text_rect.bottom+gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[1].x,&corners[1].y);

    R.Set(gdi_text_rect.right+gdi_gap,gdi_text_rect.top-gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[2].x,&corners[2].y);

    R.Set(gdi_text_rect.left-gdi_gap,gdi_text_rect.top-gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[3].x,&corners[3].y);

    xx0 = xx1 = ON_UNSET_VALUE;
    for ( i = 0; i < 4; i++ )
    {
      y0 = corners[i].y       - m_points[1].y;
      y1 = corners[(i+1)%4].y - m_points[1].y;
      if ( (y0 > 0.0 && y1 > 0.0) || (y0 < 0.0 && y1 < 0.0) || y0 == y1 )
        continue;

      t = y0/(y0-y1);
      xx = (1.0-t)*corners[i].x + t*corners[(i+1)%4].x;
      if ( ON_UNSET_VALUE == xx0 )
      {
        xx0 = xx1 = xx;
      }
      else if ( xx < xx0 )
      {
        xx0 = xx;
      }
      else if ( xx > xx1 )
      {
        xx1 = xx;
      }
    }

    // xx0 is left edge of text rect
    // xx1 is right edge of text rect
    t = arrowwidth + tailwidth;
    if ( x0 + t <= xx0 && xx0 < xx1 && xx1 <= x1 - t )
    {
      // clip line, 2 segments arrows inside
      x[0] = x0;
      x[1] = xx0;
      x[2] = xx1;
      x[3] = x1;
      x[4] = x0;
      x[5] = x1;
      if( dimextension != 0.0)
      {
        x[0] -= dimextension;
        x[3] += dimextension;
      }
      rc = 2;
    }
    else  // 2 segments, put the arrows outside the extension lines
    {
      x[0] = x0;
      x[1] = x0 - t;
      x[2] = x1 + t;
      x[3] = x1;
      x[4] = x0;
      x[5] = x1;
      if( dimextension != 0.0)
      {
        x[0] += dimextension;
        x[3] -= dimextension;
      }
      bInside = false;
      rc = 2;
    }
  }
  else if ( ON::dtAboveLine == textdisplay || m_userpositionedtext )
  {
    // 1 segment, arrows inside
    x[0] = x0;
    x[1] = x1;
    x[2] = x0;
    x[3] = x1;
    x[4] = x0;
    x[5] = x1;
    if( dimextension != 0.0)
    {
      x[0] -= dimextension;
      x[1] += dimextension;
    }
    rc = 1;
  }
  else if ( ON::dtInLine == textdisplay )
  {
    // 2 segments, arrows inside
    t = 0.5*(dimwidth - textwidth - 2.0*textgap);
    x[0] = x0;
    x[1] = x0+t;
    x[2] = x1-t;
    x[3] = x1;
    x[4] = x0;
    x[5] = x1;
    if( dimextension != 0.0)
    {
      x[0] -= dimextension;
      x[3] += dimextension;
    }

    rc = 2;
  }

  return rc;
}


const wchar_t* ON_LinearDimension2::DefaultText() { return L"<>"; }




//----- ON_RadialDimension2 ------------------------------------------
ON_RadialDimension2::ON_RadialDimension2()
{
  m_type = ON::dtDimDiameter;
  m_textdisplaymode = ON::dtInLine;
  m_usertext = DefaultDiameterText();
  m_points.Reserve(ON_RadialDimension2::dim_pt_count);
  m_points.SetCount(ON_RadialDimension2::dim_pt_count);
  m_points.Zero();
}

ON_RadialDimension2::~ON_RadialDimension2()
{
}

ON_2dPoint ON_RadialDimension2::Dim2dPoint(
      int point_index
      ) const
{
  ON_2dPoint p2;
  if ( m_points.Count() < dim_pt_count || point_index < 0 )
  {
    p2.x = p2.y = ON_UNSET_VALUE;
  }
  else
  {
    if ( text_pivot_pt == point_index )
    {
      point_index = tail_pt_index;
    }
    if ( point_index < dim_pt_count )
    {
      p2 = m_points[point_index];
    }    
    else
    {
      p2.x = p2.y = ON_UNSET_VALUE;
    }
  }
  return p2;
}

ON_3dPoint ON_RadialDimension2::Dim3dPoint(
      int point_index
      ) const
{
  ON_2dPoint p2 = Dim2dPoint(point_index);
  return (ON_UNSET_VALUE == p2.x) ? ON_UNSET_POINT : m_plane.PointAt(p2.x,p2.y);
}

// set the plane, center, and point on curve.
// the rest will be set by the Rhino dimension
bool ON_RadialDimension2::CreateFromPoints( 
  ON_3dPoint center, 
  ON_3dPoint arrowtip, 
  ON_3dVector xaxis, 
  ON_3dVector normal,
  double offset_distance)
{
  if ( m_type != ON::dtDimDiameter )
    m_type = ON::dtDimRadius;
  bool rc = false;
  if ( center.IsValid() && arrowtip.IsValid() && normal.IsValid() && !normal.IsZero() && xaxis.IsValid() && !xaxis.IsZero() )
  {
    ON_Plane plane( center, normal);
    double c = xaxis*plane.xaxis;
    double s = xaxis*plane.yaxis;
    if ( c != 0.0 || s != 0.0 )
    {
      if ( c <= 0.0 || s != 0.0 )
      {
        plane.Rotate( s, c, plane.zaxis );
      }
      m_plane = plane;
      ON_2dVector tip;
      if ( m_plane.ClosestPointTo(arrowtip,&tip.x,&tip.y) )
      {
        //double r = tip.Length();
        m_points.SetCapacity(dim_pt_count);
        m_points.SetCount(dim_pt_count);

        m_points[center_pt_index].Set(0.0,0.0);
        m_points[arrow_pt_index] = tip;
        tip.Unitize();

        m_points[knee_pt_index] = m_points[arrow_pt_index] + offset_distance*tip;
        m_points[tail_pt_index] = m_points[knee_pt_index];
        if ( m_points[arrow_pt_index].x < 0.0 )
          m_points[tail_pt_index].x -= offset_distance;
        else
          m_points[tail_pt_index].x += offset_distance;
        m_plane = plane;
        m_userpositionedtext = false;

        rc = true;
      }
    }
  }
  return rc;
}


double ON_RadialDimension2::NumericValue() const
{
  double d = 0.0;
  if ( m_points.Count() >= dim_pt_count )
  {
    d = (m_points[center_pt_index] - m_points[arrow_pt_index]).Length();
    if( ON::dtDimDiameter == m_type )
      d *= 2.0;
  }
  return d;
}

int ON_RadialDimension2::StyleIndex() const
{
  return ON_Annotation2::Index();
}

void ON_RadialDimension2::SetStyleIndex( int i)
{
  ON_Annotation2::SetIndex( i);
}


const wchar_t* ON_RadialDimension2::DefaultRadiusText()
{
  static wchar_t defstr[4] = { radiussym,L'<',L'>',0 };
  return defstr;
}

const wchar_t* ON_RadialDimension2::DefaultDiameterText()
{
  static wchar_t defstr[4] = { diametersym,L'<',L'>',0 };
  return defstr;
}

// 6-23-03 lw Added v2 file writing of annotation
void ON_RadialDimension2::GetV2Form( ON_RadialDimension& dim)
{
  ON_Annotation2::ConvertBack( dim);
}


bool ON_RadialDimension2::CreateFromV2( 
    const ON_Annotation& v2_ann,
    const ON_3dmAnnotationSettings& settings,
    int dimstyle_index
    )
{
  bool rc = false;
  if( ON::dtDimRadius == v2_ann.m_type || ON::dtDimDiameter == v2_ann.m_type )
  {
    const ON_SimpleArray<ON_2dPoint>& points = v2_ann.Points();
    if ( points.Count() >= 4 )
    {
      m_points.Reserve(4);
      m_points.SetCount(0);
      m_points.Append(4,points.Array());
      m_plane = v2_ann.m_plane;
      m_plane.UpdateEquation();
      m_usertext = v2_ann.UserText();
      m_userpositionedtext = false;
      m_type = v2_ann.Type();
      m_textdisplaymode = ( 2 == settings.m_textalign )
                        ? ON::dtHorizontal
                        : ON::dtInLine;
      m_index = dimstyle_index;
      ON_2dVector v = m_points[0];
      if ( !v.IsZero() )
      {
        m_plane.origin = m_plane.PointAt(v.x,v.y);
        m_plane.UpdateEquation();
        v.Reverse();
        m_points[0].Set(0.0,0.0);
        m_points[1] += v;
        m_points[2] += v;
        m_points[3] += v;
      }

      rc = true;
    }
  }
  return rc;
}


//----- ON_AngularDimension2 -----------------------------------------
ON_AngularDimension2::ON_AngularDimension2() : m_angle(0.0), m_radius(1.0)
{
  m_type = ON::dtDimAngular;
  m_textdisplaymode = ON::dtAboveLine;
  m_usertext = DefaultText();
  m_points.Reserve(ON_AngularDimension2::dim_pt_count);
  m_points.SetCount(ON_AngularDimension2::dim_pt_count);
  m_points.Zero();
}

ON_AngularDimension2::~ON_AngularDimension2()
{
}


ON_2dPoint ON_AngularDimension2::Dim2dPoint( int point_index ) const
{
  ON_2dPoint p2;
  if ( m_points.Count() < dim_pt_count || point_index < 0 )
  {
    p2.x = p2.y = ON_UNSET_VALUE;
  }
  else
  {
    if ( text_pivot_pt == point_index )
    {
      point_index = m_userpositionedtext 
                  ? userpositionedtext_pt_index
                  : arcmid_pt;
    }
    
    if ( point_index < dim_pt_count )
    {
      p2 = m_points[point_index];
    }
    else
    {
      switch(point_index)
      {
      case arcstart_pt:
        p2.x = m_radius;
        p2.y = 0.0;
        break;
      case arcend_pt:
        p2.x = m_radius*cos(m_angle);
        p2.y = m_radius*sin(m_angle);
        break;
      case arccenter_pt:
        p2.x = 0.0;
        p2.y = 0.0;
        break;
      case arcmid_pt:
        p2.x = m_radius*cos(0.5*m_angle);
        p2.y = m_radius*sin(0.5*m_angle);
        break;
      default:
        p2.x = p2.y = ON_UNSET_VALUE;
        break;
      }
    }
  }
  return p2;
}

ON_3dPoint ON_AngularDimension2::Dim3dPoint( int point_index ) const
{
  ON_2dPoint p2 = Dim2dPoint(point_index);
  return (ON_UNSET_VALUE == p2.x) ? ON_UNSET_POINT : m_plane.PointAt(p2.x,p2.y);
}


BOOL ON_AngularDimension2::Write( ON_BinaryArchive& file ) const
{
  BOOL rc = ON_Annotation2::Write( file);
  if( rc) rc = file.WriteDouble( m_angle);
  if( rc) rc = file.WriteDouble( m_radius);
  return rc;
}

BOOL ON_AngularDimension2::Read( ON_BinaryArchive& file )
{
  BOOL rc = ON_Annotation2::Read( file);
  if( rc) rc = file.ReadDouble( &m_angle);
  if( rc) rc = file.ReadDouble( &m_radius);
  return rc;
}

static BOOL VectorAngle( const ON_2dVector& v, double& angle)
{
  if( v.IsTiny())
    return false;

  angle = atan2( v.y, v.x);

  if( angle < 0.0)
    angle += 2.0 * ON_PI;

  if( angle > 2.0 * ON_PI)
    angle -= 2.0 * ON_PI;

  return true;
}

bool ON_AngularDimension2::CreateFromV2( 
    const ON_Annotation& v2_ann,
    const ON_3dmAnnotationSettings& settings,
    int dimstyle_index
    )
{
  if( ON::dtDimAngular != v2_ann.Type() )
    return false;

  if ( v2_ann.m_points.Count() < 3 )
    return false;

  ON_Plane plane = v2_ann.m_plane;
  plane.UpdateEquation();
  if ( !plane.IsValid() )
    return false;

  ON_2dVector p0 = v2_ann.m_points[0]; // point on first line
  ON_2dVector p1 = v2_ann.m_points[1]; // point on second line
  ON_2dPoint tp = v2_ann.m_points[2]; // user positioned text point
  if ( !p0.IsValid() || !p1.IsValid() || p0.IsZero() || p1.IsZero() )
    return false;

  bool bUserPositionedText = v2_ann.UserPositionedText() && tp.IsValid();

  if ( p0.x <= 0.0 && 0.0 != p0.y )
  {
    bUserPositionedText = false;
    ON_3dPoint P0 = plane.PointAt(p0.x,p0.y);
    ON_3dPoint P1 = plane.PointAt(p1.x,p1.y);
    plane.xaxis = P0-plane.origin;
    if ( !plane.xaxis.Unitize() )
      return false;
    plane.yaxis = ON_CrossProduct( plane.zaxis, plane.xaxis );
    plane.yaxis.Unitize();
    if ( !plane.IsValid() )
      return false;
    p0.x = p0.Length();
    p0.y = 0.0;
    if ( !plane.ClosestPointTo(P1,&p1.x,&p1.y) )
      return false;
  }

  if ( p1.x >= 0.0 && p1.y == 0.0 )
    return false;

  double angle = atan2(p1.y,p1.x);
  if ( angle < 0.0 )
    angle += 2.0*ON_PI;

  double radius = 0.5*(p0.Length() + p1.Length());

  const ON_AngularDimension* v2_angdim = ON_AngularDimension::Cast(&v2_ann);
  if ( v2_angdim && v2_angdim->Radius() > 0.0 )
    radius = v2_angdim->Radius();

  if ( !bUserPositionedText )
  {
    tp.x = radius*cos(0.5*angle);
    tp.y = radius*sin(0.5*angle);
  }

  ON_2dPoint arcpt( radius*cos(angle/3.0), radius*sin(angle/3.0) );

  m_plane = plane;
  m_points.SetCapacity(4);
  m_points.SetCount(4);
  m_points[0] = tp;
  m_points[1] = p0;
  m_points[2] = p1;
  m_points[3] = arcpt;
  m_angle = angle;
  m_radius = radius;

  m_usertext = v2_ann.UserText();
  m_userpositionedtext = bUserPositionedText;

  switch( settings.m_textalign)
  {
  case 1:
    m_textdisplaymode = ON::dtInLine;
    break;
  case 2:
    m_textdisplaymode = ON::dtHorizontal;
    break;
  default:
    m_textdisplaymode = ON::dtAboveLine;
    break;
  }

  m_index = dimstyle_index;

  return true;
}

bool ON_AngularDimension2::CreateFromArc( const ON_Arc& arc )
{
  Destroy();
  Create();
  bool rc = arc.IsValid();
  if (rc)
  {
    double r = arc.Radius();
    double a = arc.AngleRadians();
    ON_3dPoint C = arc.Center();
    ON_3dVector X = arc.StartPoint() - C;
    X.Unitize();
    ON_3dVector Y = ON_CrossProduct( arc.plane.zaxis, X );
    Y.Unitize();

    m_type = ON::dtDimAngular;
    m_plane = arc.plane;
    m_plane.xaxis = X;
    m_plane.yaxis = Y;
    m_plane.UpdateEquation();

    m_points.SetCapacity(4);
    m_points.SetCount(4);
    m_points[0].Set(0.0,0.0);
    m_points[1].Set(r,0.0);
    m_points[2].Set(r*cos(a),r*sin(a));
    m_points[3].Set(r*cos(a/3.0),r*sin(a/3.0));

    m_angle = a;
    m_radius = r;
  }

  return rc;
}

bool ON_AngularDimension2::GetArc( ON_Arc& arc ) const
{
  bool rc = false;
  if ( ON_IsValid(m_radius) && m_radius > 0.0
      && ON_IsValid(m_angle) && m_angle > 0.0 && m_angle <= 2.0*ON_PI
      && m_plane.origin.IsValid() 
      && m_plane.xaxis.IsValid() 
      && m_plane.yaxis.IsValid() 
      && m_plane.zaxis.IsValid()
      && fabs( m_plane.zaxis.Length() - 1.0 ) <= ON_SQRT_EPSILON
      && 4 == m_points.Count()
      )
  {
    ON_3dVector X = m_plane.PointAt( m_points[1].x, m_points[1].y ) - m_plane.origin;
    if ( fabs(X.Length()-1.0) <= ON_SQRT_EPSILON || X.Unitize() )
    {
      if ( fabs(X*m_plane.zaxis) <= ON_SQRT_EPSILON )
      {
        ON_3dVector Y = ON_CrossProduct( m_plane.zaxis, X );
        if ( fabs(Y.Length()-1.0) <= ON_SQRT_EPSILON || Y.Unitize() )
        {
          arc.plane = m_plane;
          arc.plane.xaxis = X;
          arc.plane.yaxis = Y;
          arc.plane.UpdateEquation();
          arc.SetAngleIntervalRadians( ON_Interval(0.0,m_angle) );
          arc.radius = m_radius;
          rc = true;
        }
      }
    }
  }

  return rc;
}

bool ON_AngularDimension2::CreateFromPoints( 
            const ON_3dPoint& pc, 
            const ON_3dPoint& p0in,
            const ON_3dPoint& p1in,
            ON_3dPoint& arcpt, 
            ON_3dVector& Normal)
{
  ON_3dPoint p0, p1;
  p0 = p0in;
  p1 = p1in;

  ON_Plane plane( pc, Normal);

  ON_2dPoint pa, pp0, pp1;

  if( !plane.ClosestPointTo( p0, &pp0.x, &pp0.y))
    return false;

  // rotate so that p0 is on x-axis
  ON_2dVector v0( pp0);
  v0.Unitize();
  plane.Rotate( v0.y, v0.x, plane.Normal());

  if( !plane.ClosestPointTo( p0, &pp0.x, &pp0.y))
    return false;

  if( !plane.ClosestPointTo( arcpt, &pa.x, &pa.y))
    return false;

  if( !plane.ClosestPointTo( p1, &pp1.x, &pp1.y))
    return false;

  double a1, aa;
  if( !VectorAngle( ON_2dVector( pp1), a1) || !VectorAngle( ON_2dVector( pa), aa))    
    return false;

  if( aa > a1)  // the angle is really the bigger one ( > 180)
  {
    // rotate so that p0 is on x-axis
    v0.Set( pp1.x, pp1.y);
    v0.Unitize();
    plane.Rotate( v0.y, v0.x, plane.Normal());

    if( !plane.ClosestPointTo( arcpt, &pa.x, &pa.y))
      return false;
    if( !plane.ClosestPointTo( p1, &pp0.x, &pp0.y))
      return false;
    if( !plane.ClosestPointTo( p0, &pp1.x, &pp1.y))
      return false;
  }

  VectorAngle( ON_2dVector( pp1), a1);

  SetAngle( a1);
  SetRadius( ON_2dVector( pa).Length());

  ReservePoints( 4);
  SetPlane( plane);
  SetPoint( 1, pp0);
  SetPoint( 2, pp1);
  SetPoint( 3, pa);


  return true;
}

void ON_AngularDimension2::SetAngle( double angle)
{
  m_angle = angle;
}

double ON_AngularDimension2::Angle() const
{
  return m_angle;
}

void ON_AngularDimension2::SetRadius( double radius)
{
  m_radius = radius;
}

double ON_AngularDimension2::Radius() const
{
  return m_radius;
}

double ON_AngularDimension2::NumericValue() const
{
  // This returns degrees - if we get another angular unit system, add it
  return (m_angle*180.0/ON_PI);
}

int ON_AngularDimension2::StyleIndex() const
{
  return ON_Annotation2::Index();
}

void ON_AngularDimension2::SetStyleIndex( int i)
{
  ON_Annotation2::SetIndex( i);
}

const wchar_t* ON_AngularDimension2::DefaultText()
{
  static wchar_t defstr[4] = { L'<',L'>',degreesym,0 };
  return defstr;
}

void ON_AngularDimension2::ConvertBack( 
        ON_AngularDimension2& // target - formal parameter intentionally ignored in this virtual function
        )
{
}

// 6-23-03 lw Added v2 file writing of annotation
void ON_AngularDimension2::GetV2Form( ON_AngularDimension& dim)
{
  ON_Annotation2::ConvertBack( dim);
  dim.SetPoint(0, Point(1));
  dim.SetPoint(1, Point(2));
  dim.SetPoint(2, Point(3));
  dim.SetPoint(3, Point(0)); // text point or apex
  dim.SetAngle( Angle());
  dim.SetRadius( Radius());
}



static void OrientRectHelper( ON_2dVector corners[4] )
{
  double twice_area = 0.0;
  ON_2dVector p0, p1;
  int i;
  p1 = corners[3];
  for ( i = 0; i < 4; i++ ) 
  {
    p0 = p1;
    p1 = corners[i];
    twice_area += (p0.x-p1.x)*(p0.y+p1.y);
  }
  if ( twice_area < 0.0 )
  {
    p1 = corners[1];
    corners[1] = corners[3];
    corners[3] = p1;
  }
}

int ON_AngularDimension2::GetDimensionArcSegments(
    ON_RECT gdi_text_rect,
    int gdi_height_of_I,
    ON_Xform gdi_to_world,
    const ON_DimStyle& dimstyle,
    double dimscale,
    const ON_Viewport* vp,
    double a[6],
    bool& bInside
    ) const
{
  int rc = 0;

  a[0] = 0.0;
  a[1] = 0.0;

  if ( m_angle <= 0.0 || !ON_IsValid(m_angle) )
    return 0;

  a[1] = m_angle;

  if ( m_radius <= 0.0 || !ON_IsValid(m_radius) || m_angle >= 2.0*ON_PI )
    return 0;

  if ( 0 == gdi_height_of_I )
  {
    // Default to height of Ariel 'I' (this code still works if ON_Font::normal_font_height != 256)
    gdi_height_of_I = (165*ON_Font::normal_font_height)/256;
  }

  if ( 0.0 == dimscale )
  {
    dimscale = 1.0;
  }

  double t;

  ON::eTextDisplayMode textdisplay = ON::TextDisplayMode(dimstyle.TextAlignment());
  if ( ON::dtHorizontal == textdisplay && !vp )
    textdisplay = ON::dtInLine;

  const double text_height_of_I    = dimscale*dimstyle.TextHeight();
  const double textgap             = dimscale*dimstyle.TextGap();
  const double gdi_to_plane_scale  = text_height_of_I/gdi_height_of_I;
  const double textwidth           = fabs(gdi_to_plane_scale*(gdi_text_rect.right - gdi_text_rect.left));
  const double arrowwidth          = fabs(dimscale*dimstyle.ArrowSize());
  const double tailwidth           = 0.5*arrowwidth;
  const double dimextension        = fabs(dimscale*dimstyle.DimExtension());
  const double a0 = 0.0;
  const double a1 = m_angle;

  double sin_angle = 0.5*dimextension/m_radius;
  if ( sin_angle > 1.0 ) sin_angle = 1.0; else if (sin_angle < -1.0) sin_angle = -1.0;
  const double  dimextension_angle = 2.0*asin(sin_angle);

  sin_angle = 0.5*arrowwidth/m_radius;
  if ( sin_angle > 1.0 ) sin_angle = 1.0; else if (sin_angle < -1.0) sin_angle = -1.0;
  const double arrowangle = 2.0*asin(sin_angle);

  sin_angle = 0.5*tailwidth/m_radius;
  if ( sin_angle > 1.0 ) sin_angle = 1.0; else if (sin_angle < -1.0) sin_angle = -1.0;
  const double tailangle = 2.0*asin(sin_angle);

  if ( m_radius <= arrowwidth + tailwidth || m_radius*(a1-a0) < 2.0*(arrowwidth) + tailwidth )
  {
    // arc is tiny with respect to arrowhead size - arrowheads have to be "outside"
    // 2 arc segments, arrowheads outside
    a[0] = a0;
    a[1] = a0 - arrowangle - tailangle;
    a[2] = a1 + arrowangle + tailangle;
    a[3] = a1;
    a[4] = a0;
    a[5] = a1;
    if( dimextension_angle != 0.0)
    {
      a[0] += dimextension_angle;
      a[3] -= dimextension_angle;
    }
    bInside = false;
    return 2;
  }
  
  if ( ON::dtInLine == textdisplay && m_radius <= 0.5*(textwidth+2.0*textgap) )
  {
    // 2 arc segments, arrowheads outside
    a[0] = a0;
    a[1] = a0 - arrowangle - tailangle;
    a[2] = a1 + arrowangle + tailangle;
    a[3] = a1;
    a[4] = a0;
    a[5] = a1;
    if( dimextension_angle != 0.0)
    {
      a[0] += dimextension_angle;
      a[3] -= dimextension_angle;
    }
    bInside = false;
    return 2;
  }

  sin_angle = 0.5*(textwidth+2.0*textgap)/m_radius;
  if ( sin_angle > 1.0 ) sin_angle = 1.0; else if (sin_angle < -1.0) sin_angle = -1.0;
  const double textangle = (ON::dtInLine == textdisplay && !m_userpositionedtext )
    ? 2.0*asin(sin_angle)
    : 0.0;

  if ( (a1-a0) <= 2.0*(arrowangle + tailangle) + textangle )
  {
    // 2 arc segments, arrowheads outside
    a[0] = a0;
    a[1] = a0 - arrowangle - tailangle;
    a[2] = a1 + arrowangle + tailangle;
    a[3] = a1;
    a[4] = a0;
    a[5] = a1;
    if( dimextension_angle != 0.0)
    {
      a[0] += dimextension_angle;
      a[3] -= dimextension_angle;
    }
    bInside = false;
    return 2;
  }
  
  if ( (ON::dtHorizontal == textdisplay && vp) || m_userpositionedtext )
  {
    // use projected rectangle to clip dimension arc
    double aa0, aa1, aa, r0, r1, t, tt[2];
    ON_3dPoint P, R, c[2];
    ON_2dVector corners[4], N;
    ON_Line ray;
    int i,xi, xrc;
    const ON_Circle circle(ON_xy_plane,1.0);

    ON_3dVector vp_zaxis = (ON::dtHorizontal == textdisplay && vp)
                         ? vp->CameraZ() 
                         : m_plane.zaxis;

    const double gdi_gap = fabs(textgap/gdi_to_plane_scale);

    R.Set(gdi_text_rect.left-gdi_gap,gdi_text_rect.bottom+gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[0].x,&corners[0].y);

    R.Set(gdi_text_rect.right+gdi_gap,gdi_text_rect.bottom+gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[1].x,&corners[1].y);

    R.Set(gdi_text_rect.right+gdi_gap,gdi_text_rect.top-gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[2].x,&corners[2].y);

    R.Set(gdi_text_rect.left-gdi_gap,gdi_text_rect.top-gdi_gap,0.0);
    ray.from = gdi_to_world*R; ray.to = ray.from + vp_zaxis;
    P = ( ON_Intersect(ray,m_plane,&t) ) ? ray.PointAt(t) : ray.from;
    m_plane.ClosestPointTo(P,&corners[3].x,&corners[3].y);

    // orient projected rect so it is counter-clockwise
    OrientRectHelper(corners);

    aa0 = a0;
    aa1 = a1;
    P.Set(cos(a0)*m_radius,sin(a0)*m_radius,0.0);
    R.Set(cos(a1)*m_radius,sin(a1)*m_radius,0.0);
    for ( i = 0; i < 4; i++ )
    {
      N.x = (corners[i].y-corners[(i+1)%4].y);
      N.y = (corners[(i+1)%4].x-corners[i].x);
      if ( !N.Unitize() )
        continue;
      if ( (P.x - corners[i].x)*N.x + (P.y - corners[i].y)*N.y  < 0.0 )
      {
        // end point not in text box
        aa0 = ON_UNSET_VALUE;
      }
      if ( (R.x - corners[i].x)*N.x + (R.y - corners[i].y)*N.y  < 0.0 )
      {
        // start point not in text box
        aa1 = ON_UNSET_VALUE;
      }
    }

    if ( ON_UNSET_VALUE == aa0 )
      aa0 = aa1;
    else if ( ON_UNSET_VALUE == aa1 )
      aa1 = aa0;

    r1 = corners[3].Length() - m_radius;
    ray.to.x = corners[3].x/m_radius; ray.to.y = corners[3].y/m_radius; 
    ray.to.z = ray.from.z = 0.0;
    for ( i = 0; i < 4; i++ )
    {
      r0 = r1;
      r1 = corners[i].Length() - m_radius;
      ray.from = ray.to;
      ray.to.x = corners[i].x/m_radius; ray.to.y = corners[i].y/m_radius;
      if ( r0 < 0.0 && r1 < 0.0 )
      {
        continue; // ray is inside circle
      }
      if ( r0 > 0.0 && r1 > 0.0 )
      {
        if ( !ray.ClosestPointTo(ON_2dPoint(0.0,0.0),&t ) )
          continue;
        R = ray.PointAt(t);
        if ( R.x*R.x + R.y*R.y >= 1.0 )
          continue; // ray is outside circle
      }


      xrc = 0;
      if ( 0.0 == r0 )
        tt[xrc++] = 0.0;

      if ( 0.0 == r1 )
        tt[xrc++] = 1.0;

      if ( 0 == xrc )
        xrc = ON_Intersect(ray,circle,&tt[0],c[0],&tt[1],c[1]);

      if ( xrc < 1 || xrc > 2 )
        continue; // ray does not intersect circle;

      for ( xi = 0; xi < xrc; xi++ )
      {
        if ( tt[xi] < 0.0 || tt[xi] > 1.0 )
          continue; // intersection point not on segment

        P = ray.PointAt(tt[xi]);
        if ( 0.0 == P.x && 0.0 == P.y )
          continue; // bogus

        aa = atan2(P.y,P.x);
        if ( aa < a0 ) aa += 2.0*ON_PI; else if ( aa > a1 ) aa -= 2.0*ON_PI;
        if ( aa < a0 || aa > a1 )
        {
          continue;
        }
        if ( ON_UNSET_VALUE == aa0 )
          aa0 = aa1 = aa;
        else if ( aa < aa0 )
          aa0 = aa;
        else if ( aa > aa1 )
          aa1 = aa;
      }
    }
    if ( ON_UNSET_VALUE != aa0 && ON_UNSET_VALUE != aa1 
        && a0 <= aa0 && aa0 < aa1 && aa1 <= a1 )
    {
      t = arrowangle + tailangle;
      if ( aa0 < a0+t && aa1 > a1-t )
      {
        // text box hits both arrowheads
        // 2 arc segments, arrowheads outside
        a[0] = a0;
        a[1] = a0 - arrowangle - tailangle;
        a[2] = a1 + arrowangle + tailangle;
        a[3] = a1;
        a[4] = a0;
        a[5] = a1;
        if( dimextension_angle != 0.0)
        {
          a[0] += dimextension_angle;
          a[3] -= dimextension_angle;
        }
        bInside = false;
        rc = 2;
      }
      else
      {
        if ( aa0 < a0+t ) aa0 = a0+t;
        if ( aa1 > a1-t ) aa1 = a1-t;
        if ( a0 < aa0 && aa0 < aa1 && aa1 < a1 )
        {
          // clip arc to text rectangle
          // 2 arc segments, arrowheads inside
          a[0] = a0;
          a[1] = aa0;
          a[2] = aa1;
          a[3] = a1;
          a[4] = a0;
          a[5] = a1;
          if( dimextension_angle != 0.0)
          {
            a[0] -= dimextension_angle;
            a[3] += dimextension_angle;
          }
          bInside = true;
          rc = 2;
        }
        else
        {
          // nasty case - don't bother clipping
          // 1 segment, arrows inside
          a[0] = a0;
          a[1] = a1;
          a[2] = a0;
          a[3] = a1;
          a[4] = a0;
          a[5] = a1;
          if( dimextension_angle != 0.0)
          {
            a[0] -= dimextension_angle;
            a[1] += dimextension_angle;
          }
          bInside = true;
          rc = 1;
        }
      }
    }
    else
    {
      // 1 segment, arrows inside
      a[0] = a0;
      a[1] = a1;
      a[2] = a0;
      a[3] = a1;
      a[4] = a0;
      a[5] = a1;
      if( dimextension_angle != 0.0)
      {
        a[0] -= dimextension_angle;
        a[1] += dimextension_angle;
      }
      bInside = true;
      rc = 1;
    }
  }
  else if ( ON::dtAboveLine == textdisplay || m_userpositionedtext )
  {
    // 1 segment, arrows inside
    a[0] = a0;
    a[1] = a1;
    a[2] = a0;
    a[3] = a1;
    a[4] = a0;
    a[5] = a1;
    if( dimextension_angle != 0.0)
    {
      a[0] -= dimextension_angle;
      a[1] += dimextension_angle;
    }
    bInside = true;
    rc = 1;
  }
  else if ( ON::dtInLine == textdisplay )
  {
    // 2 segments, arrows inside
    t = 0.5*((a1-a0) - textangle);
    a[0] = a0;
    a[1] = a0+t;
    a[2] = a1-t;
    a[3] = a1;
    a[4] = a0;
    a[5] = a1;
    if( dimextension_angle != 0.0)
    {
      a[0] -= dimextension_angle;
      a[1] += dimextension_angle;
    }
    bInside = true;
    rc = 2;
  }

  return rc;
}


//----- ON_OrdinateDimension2 -----------------------------------------
ON_OrdinateDimension2::ON_OrdinateDimension2()
{
  m_type = ON::dtDimOrdinate;
  m_usertext = DefaultText();
  m_direction = -1;  // undetermined direction
  m_points.Reserve(ON_OrdinateDimension2::dim_pt_count);
  m_points.SetCount(ON_OrdinateDimension2::dim_pt_count);
  m_points.Zero();
  m_kink_offset_0 = ON_UNSET_VALUE;
  m_kink_offset_1 = ON_UNSET_VALUE;
}

ON_OrdinateDimension2::~ON_OrdinateDimension2()
{
}

BOOL ON_OrdinateDimension2::Transform( const ON_Xform& xform )
{
  bool rc = xform.IsIdentity();
  if ( !rc)
  {
    return ON_Annotation2::Transform(xform);
  }
  return rc;
}

ON_2dPoint ON_OrdinateDimension2::Dim2dPoint( int point_index, double default_offset) const
{
  ON_2dPoint p2( ON_UNSET_VALUE, ON_UNSET_VALUE);
  int dir = m_direction;
  if( dir == -1 && ( point_index == offset_pt_0 || point_index == offset_pt_1))
  {
    if( fabs( m_points[definition_pt_index].y - m_points[leader_end_pt_index].y) > 
        fabs( m_points[definition_pt_index].x - m_points[leader_end_pt_index].x))
      dir = 0;
    else
      dir = 1;
  }

  if( point_index >= 0 && point_index < dim_pt_count && m_points.Count() == dim_pt_count)
  {
    p2 = m_points[point_index];
  }
  else if( point_index == text_pivot_pt)
  {
    // ON_UNSET_POINT
  }
  else if( point_index == offset_pt_0)
  {
    double offset;
    if( m_kink_offset_0 == ON_UNSET_VALUE)
      offset = default_offset;
    else
      offset = m_kink_offset_0;

    if( dir == x)
    {
      p2 = m_points[leader_end_pt_index];
      if( p2.y > m_points[definition_pt_index].y)
        p2.y -= offset;
      else
        p2.y += offset;
    }
    else if( dir == y)
    {
      p2 = m_points[leader_end_pt_index];
      if( p2.x > m_points[definition_pt_index].x)
        p2.x -= offset;
      else
        p2.x += offset;
    }
  }
  else if( point_index == offset_pt_1)
  {
    double offset0;
    if( m_kink_offset_0 == ON_UNSET_VALUE)
      offset0 = default_offset;
    else
      offset0 = m_kink_offset_0;

    double offset1;
    if( m_kink_offset_1 == ON_UNSET_VALUE)
      offset1 = default_offset;
    else
      offset1 = m_kink_offset_1;

    if( dir == x)
    {
      p2.x = m_points[definition_pt_index].x;
      if( m_points[leader_end_pt_index].y > m_points[definition_pt_index].y)
        p2.y = m_points[leader_end_pt_index].y - offset0 - offset1;
      else
        p2.y = m_points[leader_end_pt_index].y + offset0 + offset1;
    }
    else if( dir == y)
    {
      p2.y = m_points[definition_pt_index].y;
      if( m_points[leader_end_pt_index].x > m_points[definition_pt_index].x)
        p2.x = m_points[leader_end_pt_index].x - offset0 - offset1;
      else
        p2.x = m_points[leader_end_pt_index].x + offset0 + offset1;
    }
  }
  return p2;
}

ON_3dPoint ON_OrdinateDimension2::Dim3dPoint( int point_index, double default_offset) const
{
  ON_2dPoint p2 = Dim2dPoint(point_index, default_offset);
  return (ON_UNSET_VALUE == p2.x) ? ON_UNSET_POINT : m_plane.PointAt(p2.x,p2.y);
}

BOOL ON_OrdinateDimension2::IsValid( ON_TextLog* text_log) const
{
  if ( m_type != ON::dtDimOrdinate)
  {
    if ( text_log )
    {
      text_log->Print("ON_OrdinateDimension2 - m_type !=  ON::dtDimOrdinate.\n");
    }
    return false;
  }

  if ( !ON_Annotation2::IsValid( text_log ))
  {
    if ( text_log )
    {
      text_log->Print("ON_OrdinateDimension2 - invalid ON_Annotation2 base class.\n");
    }
    return false;
  }

  if ( m_points.Count() != 2 )
  {
    if ( text_log )
    {
      text_log->Print("ON_OrdinateDimension2 - m_points.Count() = %d (should be 2).\n",m_points.Count());
    }
    return false;
  }

  return true;

}

BOOL ON_OrdinateDimension2::Write( ON_BinaryArchive& file ) const
{
  // put the entire ON_OrdinateDimension2 in a chunk so we can
  // add fields without breaking the file IO for old product.
  bool rc = file.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,1);
  if (rc)
  {
    rc = file.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,0);
    if (rc)
    {
      // The output of ON_Annotation2::Write must be wrapped
      // in an additional chunk because it does not put 
      // itself in a chunk.  If you don't put it in a chunk,
      // the versioning in the ON_Annotation2::Write is useless and
      // will cause serious IO bugs if fields are ever added.
      rc = ON_Annotation2::Write( file) ? true : false;
      if (!file.EndWrite3dmChunk() )
        rc = false;
    }
    if (rc)
      rc = file.WriteInt( m_direction);
    // kink offsets, ver 1.1 added 2-4-06
    if (rc)
      rc = file.WriteDouble( m_kink_offset_0);
    if (rc)
      rc = file.WriteDouble( m_kink_offset_1);

    // end of ON_OrdinateDimension2 chunk
    if (!file.EndWrite3dmChunk() )
      rc = false;
  }

  return rc;
}

BOOL ON_OrdinateDimension2::Read( ON_BinaryArchive& file )
{
  int major_version=0, minor_version=0;
  bool rc = file.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&major_version,&minor_version);
  if (rc)
  {
    if ( 1 != major_version )
    {
      rc = false;
    }
    else
    {
      int submajor_version=0, subminor_version=0;

      // subchunk wraps ON_Annotation2 field so this
      // function won't break if ON_Annotation2 changes.
      rc = file.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&submajor_version,&subminor_version);
      if (rc)
      {
        if ( 1 != submajor_version )
          rc = false;
        else
        {
          rc = ON_Annotation2::Read( file) ? true : false;
        }
        if ( !file.EndRead3dmChunk() )
          rc = false;
      }

      if( rc) 
        rc = file.ReadInt( &m_direction);

      if( minor_version > 0)
      {
        if( rc) 
          rc = file.ReadDouble( &m_kink_offset_0);
        if( rc) 
          rc = file.ReadDouble( &m_kink_offset_1);
      }
    }

    if (!file.EndRead3dmChunk())
      rc = false;
  }
  return rc;
}

double ON_OrdinateDimension2::NumericValue() const
{
  if( m_direction == 0)
    return m_points[1].x - m_points[0].x;
  else
    return m_points[1].y - m_points[0].y;
}

int ON_OrdinateDimension2::StyleIndex() const
{
  return ON_Annotation2::Index();
}

void ON_OrdinateDimension2::SetStyleIndex( int i)
{
  ON_Annotation2::SetIndex( i);
}

BOOL ON_OrdinateDimension2::GetBBox( double* boxmin,
                                     double* boxmax,
                                     BOOL bGrowBox) const
{
  ON_BoundingBox bbox;
  if ( bGrowBox )
  {
    bbox.m_min.x = boxmin[0]; 
    bbox.m_min.y = boxmin[1]; 
    bbox.m_min.z = boxmin[2];
    bbox.m_max.x = boxmax[0]; 
    bbox.m_max.y = boxmax[1]; 
    bbox.m_max.z = boxmax[2];
    if ( !bbox.IsValid() )
    {
      bbox.Destroy();
      bGrowBox = false;
    }
  }

  if( m_points.Count() == 2)
  {
    ON_3dPointArray P( 2);

    P.Append( m_plane.PointAt( m_points[0].x, m_points[0].y));
    P.Append( m_plane.PointAt( m_points[1].x, m_points[1].y));
  }

  if ( bGrowBox )
  {
    boxmin[0] = bbox.m_min.x; 
    boxmin[1] = bbox.m_min.y; 
    boxmin[2] = bbox.m_min.z; 
    boxmax[0] = bbox.m_max.x; 
    boxmax[1] = bbox.m_max.y; 
    boxmax[2] = bbox.m_max.z; 
  }

  return bGrowBox;
}

bool ON_OrdinateDimension2::GetTightBoundingBox( ON_BoundingBox& tight_bbox,
                                                 int bGrowBox,
                                                 const ON_Xform* xform) const
{
  if( m_points.Count() == 2)
  {
    ON_3dPointArray P(2);

    P.Append( m_plane.PointAt( m_points[0].x, m_points[0].y));
    P.Append( m_plane.PointAt( m_points[1].x, m_points[1].y));

    if ( P.GetTightBoundingBox( tight_bbox, bGrowBox, xform))
      bGrowBox = true;
  }
  else if( bGrowBox && !tight_bbox.IsValid())
  {
    tight_bbox.Destroy();
    bGrowBox = false;
  }

  return( 0 != bGrowBox);
}

int ON_OrdinateDimension2::ImpliedDirection() const
{
  int direction = -1;
  const ON_2dPoint& p0 = m_points[definition_pt_index];
  const ON_2dPoint& p1 = m_points[leader_end_pt_index];
  if( fabs( p1.x - p0.x) <= fabs( p1.y - p0.y))
    direction = 0; // measures along x axis
  else
    direction = 1; // measures along y axis
  
  return direction;
} 

int ON_OrdinateDimension2::Direction() const
{
  return m_direction;
} 

void ON_OrdinateDimension2::SetDirection( int direction)
{
  m_direction = direction;
}

const wchar_t* ON_OrdinateDimension2::DefaultText() { return L"<>"; }

double ON_OrdinateDimension2::KinkOffset( int index) const
{
  if( index == 0)
    return m_kink_offset_0;
  else if( index == 1)
    return m_kink_offset_1;
  else
    return ON_UNSET_VALUE;
}

void ON_OrdinateDimension2::SetKinkOffset( int index, double offset)
{
  if( index == 0)
    m_kink_offset_0 = offset;
  else  if( index == 1)
    m_kink_offset_1 = offset;
}

void ON_OrdinateDimension2::CalcKinkPoints( ON_2dPoint p0, ON_2dPoint p1, 
                                            int direction, double default_offset,
                                            ON_2dPoint& k0, ON_2dPoint& k1) const
{
  double offset0 = KinkOffset( 0);
  double offset1 = KinkOffset( 1);

  // if these haven't been set by dragging the offset points
  // use 2*textheight
  if( offset0 == ON_UNSET_VALUE)
    offset0 = default_offset;
  if( offset1 == ON_UNSET_VALUE)
    offset1 = default_offset;

  if( p0[1-direction] > p1[1-direction])
  {
    offset0 = -offset0;
    offset1 = -offset1;
  }

  //double d = fabs( p0[1-direction] - p1[1-direction]);

  if( direction == 0)
  {
    //if( d - fabs( offset0) > default_offset)
    //{
      k1.x = p0.x;
      k1.y = p1.y - offset0 - offset1;
    //}
    //else
    //{
    //  k1.x = p0.x;
    //  k1.y = p1.y + offset0 - offset1;
    //}

    k0.x = p1.x;
    k0.y = p1.y - offset0;
  }
  else
  {
    //if( d - fabs( offset0) > default_offset)
    //{
      k1.x = p1.x - offset0 - offset1;
      k1.y = p0.y;
    //}
    //else
    //{
    //  k1.x = p1.x + offset0 - offset1;
    //  k1.y = p0.y;
    //}

    k0.x = p1.x - offset0;
    k0.y = p1.y;
  }
}



//----- ON_TextEntity2 -----------------------------------------------
ON_TextEntity2::ON_TextEntity2()
{
  m_type = ON::dtTextBlock;
  m_textdisplaymode = ON::dtNormal;
}

ON_TextEntity2::~ON_TextEntity2()
{
}

int ON_TextEntity2::FontIndex() const
{
  return m_index;
}

void ON_TextEntity2::SetFontIndex( int i)
{
  m_index = i;
}

BOOL ON_TextEntity2::Transform( const ON_Xform& xform )
{
  // Dale Lear - this override fixes RR 11114 by correctly
  //             handling non uniform scaling.
  bool rc = xform.IsIdentity();
  if ( !rc)
  {
    rc = ON_Geometry::Transform(xform)?true:false;
    if (rc)
    {
      ON_3dPoint P0 = xform*m_plane.origin;
      ON_3dPoint P1 = xform*(m_plane.origin + m_plane.xaxis);
      double s = P0.DistanceTo(P1);
      if ( s <= ON_ZERO_TOLERANCE )
      {
        P1 = xform*(m_plane.origin + m_plane.yaxis);
        s = P0.DistanceTo(P1);
      }
      m_plane.Transform(xform);
      if ( s > ON_ZERO_TOLERANCE && fabs(s-1.0) > ON_SQRT_EPSILON )
      {
        s *= m_textheight;
        if ( s > ON_SQRT_EPSILON )
          m_textheight = s;
      }
    }
  }

  return rc;
}



// 6-23-03 lw Added v2 file writing of annotation
void ON_TextEntity2::GetV2Form( ON_TextEntity& text)
{
  ON_Annotation2::ConvertBack( text);
  text.SetHeight( Height());
  // 8-20-03 lw convert from lower-left to upper-left reference point
  ON_Plane plane = Plane();
  plane.origin  += plane.yaxis * 1.1 * m_textheight;
  plane.UpdateEquation();
  text.SetPlane( plane);
}

//----- ON_Leader2 ------------------------------------------
ON_Leader2::ON_Leader2()
{
  m_type = ON::dtLeader;
  m_textdisplaymode = ON::dtInLine;
}

ON_Leader2::~ON_Leader2()
{
}

ON_2dPoint ON_Leader2::Dim2dPoint(
      int point_index
      ) const
{
  ON_2dPoint p2;
  int point_count = m_points.Count();
  if ( point_index < 0 || point_count < 1 )
  {
    p2.x = p2.y = ON_UNSET_VALUE;
  }
  else
  {
    switch(point_index)
    {
    case arrow_pt_index:
      p2 = m_points[0];
      break;

    case text_pivot_pt:
    case tail_pt:
      p2 = *m_points.Last();
      break;

    default:
      if ( point_index < point_count )
      {
        p2 = m_points[point_index];
      }
      else
      {
        p2.x = p2.y = ON_UNSET_VALUE;
      }
      break;
    }
  }
  return p2;
}

ON_3dPoint ON_Leader2::Dim3dPoint(
      int point_index
      ) const
{
  ON_2dPoint p2 = Dim2dPoint(point_index);
  return (ON_UNSET_VALUE == p2.x) ? ON_UNSET_POINT : m_plane.PointAt(p2.x,p2.y);
}


void ON_Leader2::AddPoint( const ON_2dPoint& point )
{
  m_points.Append( point);

}

bool ON_Leader2::RemovePoint( int idx )
{
  bool rc = true;
  if( idx == -1)  // -1 removes the last point
  {
    m_points.Remove();
  }
  else if( idx >= 0 && idx < m_points.Count())
  {
    m_points.Remove( idx);
  }
  else
  {
    rc = false;
  }

  return rc;
}

bool ON_Leader2::GetArrowHeadDirection( ON_2dVector& arrowhead_dir ) const
{
  bool rc = false;
  const int point_count = m_points.Count();
  if ( point_count < 2 )
  {
    arrowhead_dir.Set(-1.0,0.0);
  }
  else
  {
    int i;
    for ( i = 1; i < point_count; i++ )
    {
      arrowhead_dir = m_points[0] -  m_points[i];
      if ( arrowhead_dir.Unitize() )
      {
        rc = true;
        break;
      }
      arrowhead_dir.Set(-1.0,0.0);
    }
  }
  return rc;
}

bool ON_Leader2::GetArrowHeadTip( ON_2dPoint& arrowhead_tip ) const
{
  bool rc = false;
  switch( m_points.Count())
  {
  case 0:
    arrowhead_tip.Set(0.0,0.0);
    break;
  case 1:
    arrowhead_tip = m_points[0];
    break;
  default:
    arrowhead_tip = m_points[0];
    rc = true;
    break;
  }
  return rc;
}



bool ON_RadialDimension2::GetArrowHeadDirection( ON_2dVector& arrowhead_dir ) const
{
  bool rc = false;
  if ( m_points.Count() < 4 )
  {
    arrowhead_dir.Set(-1.0,0.0);
  }
  else
  {
    arrowhead_dir = m_points[1] - m_points[3];
    if ( !(rc=arrowhead_dir.Unitize()) )
    {
      arrowhead_dir = m_points[1] - m_points[2];
      if ( !(rc=arrowhead_dir.Unitize()) )
      {
        arrowhead_dir = m_points[0] - m_points[1];
        rc = arrowhead_dir.Unitize();
      }
    }
  }
  return rc;
}

bool ON_RadialDimension2::GetArrowHeadTip( ON_2dPoint& arrowhead_tip ) const
{
  bool rc = false;
  if ( m_points.Count() >= 2 )
  {
    arrowhead_tip = m_points[1];
    rc = true;
  }
  else
  {
    arrowhead_tip.Set(0.0,0.0);
    rc = false;
  }
  return rc;
}

// 6-23-03 lw Added v2 file writing of annotation
void ON_Leader2::GetV2Form( ON_Leader& leader)
{
  ON_Annotation2::ConvertBack( leader);
}


bool ON_Leader2::CreateFromV2( 
    const ON_Annotation& v2_ann,
    const ON_3dmAnnotationSettings& settings,
    int dimstyle_index
    )
{
  bool rc = false;
  if( ON::dtLeader == v2_ann.m_type && v2_ann.m_points.Count() >= 2 )
  {
    m_plane = v2_ann.m_plane;
    m_plane.UpdateEquation();
    m_points.Reserve(v2_ann.m_points.Count());
    m_points.SetCount(0);
    m_points.Append(v2_ann.m_points.Count(),v2_ann.m_points.Array());
    ON_2dVector v = m_points[0];
    m_usertext = v2_ann.UserText();
    m_userpositionedtext = false;
    m_textdisplaymode = ( 2 == settings.m_textalign )
                      ? ON::dtHorizontal
                      : ON::dtInLine;
    m_type = ON::dtLeader;
    m_index = dimstyle_index;

    if ( !v.IsZero() )
    {
      m_plane.origin = m_plane.PointAt(v.x,v.y);
      m_plane.UpdateEquation();
      v.Reverse();
      int i;
      for ( i = 1; i < m_points.Count(); i++ )
      {
        m_points[i] += v;
      }
      m_points[0].Set(0.0,0.0);
    }
    rc = true;
  }
  return rc;
}


// class ON_TextDot
//--------------------------------------------------------------------
ON_TextDot::ON_TextDot() :
  m_point( ON_origin), m_height( 14), m_text( L'1'), 
  m_fontface( L"Arial bold"), m_display( 0)
{
}

ON_TextDot::~ON_TextDot()
{
}

void ON_TextDot::EmergencyDestroy()
{
  m_text.EmergencyDestroy();
  m_fontface.EmergencyDestroy();
  m_point = ON_origin;
  m_height = 0;
  m_display = 0;
}

BOOL ON_TextDot::IsValid( 
            ON_TextLog* text_log
            ) const
{
  // 5/6/03 LW made dots with no text valid.
  if ( !m_point.IsValid() )
  {
    if ( 0 != text_log )
    {
      text_log->Print("ON_TextDot m_point is not valid\n");
    }
    return false;
  }
  return true;
}

void ON_TextDot::Dump( ON_TextLog& log) const
{
  log.Print("ON_TextDot \"%S\" at ",m_text.Array());
  log.Print( m_point);  // ON_Geometry 3d location
  log.Print("\n");
}

BOOL ON_TextDot::Write( ON_BinaryArchive& file) const
{
  BOOL rc = file.Write3dmChunkVersion(1,0);
  if (rc) rc = file.WritePoint( m_point );
  if (rc) rc = file.WriteInt( m_height);
  if (rc) rc = file.WriteString( m_text);
  if (rc) rc = file.WriteString( m_fontface);
  if (rc) rc = file.WriteInt( m_display);

  return rc;
}

BOOL ON_TextDot::Read( ON_BinaryArchive& file)
{
  m_text.Empty();
  int major_version = 0;
  int minor_version = 0;
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if ( major_version == 1 ) {
    if (rc) rc = file.ReadPoint( m_point);
    if (rc) rc = file.ReadInt( &m_height);
    if (rc) rc = file.ReadString( m_text);
    if (rc) rc = file.ReadString( m_fontface);
    if (rc) rc = file.ReadInt( &m_display);
  }
  else {
    rc = false;
  }
  return rc;
}

ON::object_type ON_TextDot::ObjectType() const
{
  return ON::text_dot;
}

int ON_TextDot::Dimension() const
{
  return 3;
}

BOOL ON_TextDot::GetBBox( double* box_min, double* box_max, BOOL grow_box /*= false*/) const
{
  return ON_GetPointListBoundingBox( 3, 0, 1, 3, &m_point.x, box_min, box_max, grow_box?true:false );
}

BOOL ON_TextDot::Transform( const ON_Xform& xform)
{
  TransformUserData( xform);
  return ON_TransformPointList( 3, 0, 1, 3, &m_point.x, xform);
}

bool ON_TextDot::IsDeformable() const
{
  return true;
}

bool ON_TextDot::MakeDeformable()
{
  return true;
}

const ON_3dPoint& ON_TextDot::Point() const
{
  return m_point;
}

void ON_TextDot::SetPoint( const ON_3dPoint& point)
{
  m_point =  point;
}

int ON_TextDot::Height() const
{
  return m_height;
}

void ON_TextDot::SetHeight( int height)
{
  if( height > 2)
    m_height = height;
}

const wchar_t* ON_TextDot::TextString() const 
{
  if( m_text.IsEmpty())
    return L"";
  else
    return m_text;
}

void ON_TextDot::SetTextString( const wchar_t* string)
{
  if( string)
    m_text = string;
  else
    m_text.Empty();
}

const wchar_t* ON_TextDot::FontFace() const
{
  if( m_fontface.IsEmpty())
    return L"";
  else
    return m_fontface;
}

void ON_TextDot::SetFontFace( const wchar_t* face)
{
  if( face)
    m_fontface = face;
  else
    m_fontface.Empty();
}

ON_Annotation2Text::ON_Annotation2Text()
{
  memset(&m_rect,0,sizeof(m_rect));
}

ON_Annotation2Text::~ON_Annotation2Text()
{
}

ON_Annotation2Text& ON_Annotation2Text::operator=(const char* s)
{
  SetText(s);
  return *this;
}

ON_Annotation2Text& ON_Annotation2Text::operator=(const wchar_t* s)
{
  SetText(s);
  return *this;
}

void ON_Annotation2Text::SetText(const char* s)
{
  ON_wString::operator=(s);
  memset(&m_rect,0,sizeof(m_rect));
}

void ON_Annotation2Text::SetText(const wchar_t* s)
{
  ON_wString::operator=(s);
  memset(&m_rect,0,sizeof(m_rect));
}

bool ON_Annotation2::GetTextXform( 
      ON_RECT gdi_text_rect,
      const ON_Font& font,
      const ON_DimStyle& dimstyle,
      double dimscale,
      const ON_Viewport* vp,
      ON_Xform& xform
      ) const
{
  const int gdi_height_of_I = font.HeightOfI();
  const double dimstyle_textheight = dimstyle.TextHeight();
  const double dimstyle_textgap = dimstyle.TextGap();
  const ON::eTextDisplayMode dimstyle_textalignment 
            =  ON::TextDisplayMode(dimstyle.TextAlignment()) ;
  const ON_3dVector cameraX = (vp) ? vp->CameraX() : m_plane.xaxis;
  const ON_3dVector cameraY = (vp) ? vp->CameraY() : m_plane.yaxis;

  return GetTextXform( 
      gdi_text_rect,
      gdi_height_of_I,
      dimstyle_textheight, dimstyle_textgap, dimstyle_textalignment,
      dimscale,
      cameraX, cameraY, 
      xform
      );
}

static bool GetLeaderEndAndDirection( const ON_Annotation2* pAnn,
                                     ON_2dPoint& E,
                                     ON_2dVector& R )
{
  bool rc = false;

  ON::eAnnotationType ann_type = pAnn->m_type;
  const ON_2dPointArray& ann_m_points = pAnn->m_points;


  R.Set(1.0,0.0); // unit vector points to end
  E.Set(0.0,0.0); // end point

  if ( ann_m_points.Count() >= 4 && (ON::dtDimDiameter == ann_type || ON::dtDimRadius == ann_type) )
  {
    E = ann_m_points[2]; // end of radial dimension
    R = E - ann_m_points[3];
    if ( !R.Unitize() )
    {
      R = E - ann_m_points[1];
      if ( !R.Unitize() )
      {
        R = E - ann_m_points[0];
        if ( !R.Unitize() )
        {
          R.Set(1.0,0.0);
        }
      }
    }
    rc = true;
  }
  else if ( ann_m_points.Count() >= 2 && ON::dtLeader == ann_type )
  {
    int i;
    E = *ann_m_points.Last();
    for (i = ann_m_points.Count()-2; i >= 0; i-- )
    {
      R = E - ann_m_points[i];
      if ( R.Unitize() )
      {
        break;
      }
      R.Set(1.0,0.0);
    }
    rc = true;
  }
  else if ( ann_m_points.Count() >= 2 && ON::dtDimOrdinate == ann_type )
  {
    E = ann_m_points[1];

    int direction = (( ON_OrdinateDimension2*)pAnn)->Direction();
    if( direction == -1)
    {
      if( fabs( ann_m_points[1].x - ann_m_points[0].x) 
       <= fabs( ann_m_points[1].y - ann_m_points[0].y))
        direction = 0;
      else
        direction = 1;
    }

    if( direction == 0)
      R.Set( 0.0, ann_m_points[1].y - ann_m_points[0].y);
    else
      R.Set( ann_m_points[1].x - ann_m_points[0].x, 0.0);

    if( !R.Unitize())
      R.Set(1.0,0.0);

    rc = true;
  }

  return rc;
}

bool ON_Annotation2::GetTextXform( 
      ON_RECT gdi_text_rect,
      int gdi_height_of_I,
      double dimstyle_textheight,
      double dimstyle_textgap,
      ON::eTextDisplayMode dimstyle_textalignment,
      double dimscale,
      ON_3dVector cameraX,
      ON_3dVector cameraY,
      ON_Xform& xform
      ) const
{
  const ON_Annotation2* ann = this;

  const ON::eAnnotationType ann_type = ann->m_type;

  if ( 0 == gdi_height_of_I )
  {
    // Default to height of Ariel 'I'
    gdi_height_of_I = (165*ON_Font::normal_font_height)/256;
  }

  if ( 0.0 == dimscale )
  {
    dimscale = 1.0;
  }

  dimstyle_textheight *= dimscale;
  dimstyle_textgap *= dimscale;

  double textheight = ( ON::dtTextBlock == ann_type )
                    ? m_textheight*dimscale
                    : dimstyle_textheight;
  if ( 0.0 == textheight )
    textheight = 1.0;

  ON_3dVector cameraZ = ON_CrossProduct( cameraX, cameraY );
  if ( fabs( 1.0 - cameraZ.Length() ) > ON_SQRT_EPSILON )
  {
    cameraZ.Unitize();
  }

  // This xform is a scale from Windows gdi coordinates 
  // to annotation plane coordinates.
  const double gdi_to_plane_scale = textheight/gdi_height_of_I;
  ON_Xform gdi_to_plane(1.0);
  gdi_to_plane.m_xform[0][0] =  gdi_to_plane_scale;
  gdi_to_plane.m_xform[1][1] = -gdi_to_plane_scale;

  // width and height of text line in Rhino units.
  const double text_line_width  = gdi_to_plane_scale*(gdi_text_rect.right - gdi_text_rect.left);
  //const double text_line_height = gdi_to_plane_scale*(gdi_text_rect.bottom - gdi_text_rect.top);

  if ( ON::dtTextBlock == ann_type )
  {
    // The orientation of the text is text blocks
    // does not depend on the view or text alignment
    // settings.  The position and orientation of 
    // the text in every other annotation depends on
    // the view and text alignment settings.  
    //
    // It simplifies the code for the rest of the
    // annotation settings to quickly deal with text
    // blocks here.
    ON_Xform plane_to_world(1.0);
    plane_to_world.Rotation(ON_xy_plane,ann->m_plane);
    xform = plane_to_world*gdi_to_plane;
    return true;
  }


  // text_position_mode
  //   1 = linear, aligned, or anglular dimension
  //       (dimension definition determines center point of text box)
  //   2 = radial, diameter, leader
  //       (dimension definition determined end point of text box)
  int position_style = 0;
  switch( ann_type )
  {
  case ON::dtDimAligned:
  case ON::dtDimLinear:
  case ON::dtDimAngular:
    // dimension definition determines center point of text box
    position_style = 1;
    break;

  case ON::dtLeader:
  case ON::dtDimRadius:
  case ON::dtDimDiameter:
  case ON::dtDimOrdinate:
    // dimension definition determines end of text box
    position_style = 2;
    break;

  case ON::dtTextBlock:
  case ON::dtNothing:
    break;
  }


  // This translation puts the center of the fist line of text at
  // (0,0) in the annotation's plane.
  if ( ON::dtHorizontal != dimstyle_textalignment || 1 == position_style )
  {
    gdi_to_plane.m_xform[0][3] = -0.5*text_line_width;
  }
  gdi_to_plane.m_xform[1][3] = -0.5*textheight;

  if ( ON::dtHorizontal != dimstyle_textalignment )
  {
    if ( ((cameraZ*m_plane.zaxis) < -ON_SQRT_EPSILON) )
    {
      // Viewing dimension from the backside
      ON_Xform flip(1.0);
      switch ( position_style )
      {
      case 1: // ON::dtDimLinear, ON::dtDimAligned, ON::dtDimAngular
        flip.m_xform[0][0] = -1.0;
        flip.m_xform[0][3] = gdi_text_rect.left + gdi_text_rect.right;
        break;

      case 2: // ON::dtDimDiameter, ON::dtDimRadius, ON::dtLeader
        flip.m_xform[1][1] = -1.0;
        flip.m_xform[1][3] = gdi_text_rect.top + gdi_text_rect.bottom;
        break;
      }
      gdi_to_plane = gdi_to_plane*flip;
    }
  }

  // text_centering_rotation rotates about the "center".  Angular,
  // radial, and leader dimensions use this rotation.
  ON_2dVector text_centering_rotation(1.0,0.0);

  // text_centering_translation is a small translation deals with
  // text that is above or to the right of the "center" point.  
  // It is no larger than dimstyle_gap + 1/2 the size of the
  // text's bounding box.
  ON_2dVector text_centering_translation(0.0,0.0);

  double x, y;

  if ( ON::dtHorizontal != dimstyle_textalignment )
  {
    if ( ON::dtDimLinear  == ann_type || ON::dtDimAligned == ann_type )
    {
      if ( ON::dtAboveLine == dimstyle_textalignment )
      {
        text_centering_translation.y =  0.5*textheight+dimstyle_textgap;
      }
      y =  ann->m_plane.yaxis*cameraY;
      x = -ann->m_plane.yaxis*cameraX;
      if ( fabs(y) <= ON_SQRT_EPSILON && fabs(x) > ON_SQRT_EPSILON )
      {
        y = x;
      }
      if ( y < 0.0 )
      {
        text_centering_translation.Reverse();
        text_centering_rotation.Reverse(); // rotate 180 degrees
      }
    }
    else if ( ON::dtDimAngular == ann_type )
    {
      // This transform rotates the text in the annotation plane.
      const ON_AngularDimension2* angular_dim = ON_AngularDimension2::Cast(ann);
      if ( 0 != angular_dim )
      {
        double a = 0.5*angular_dim->m_angle;
        ON_2dVector R(cos(a),sin(a));
        a -= 0.5*ON_PI;
        text_centering_rotation.x = cos(a);
        text_centering_rotation.y = sin(a);
        ON_3dVector V = R.x*m_plane.xaxis + R.y*m_plane.yaxis;
        x = V*cameraX;
        y = V*cameraY;
        if ( fabs(y) <= ON_SQRT_EPSILON && fabs(x) > ON_SQRT_EPSILON )
        {
          y = -x;
        }
        if ( y < 0.0 )
        {
          text_centering_rotation.Reverse(); // add another 180 degrees of rotation
        }

        if ( ON::dtAboveLine == dimstyle_textalignment )
        {
          y = 0.5*textheight + dimstyle_textgap;
          text_centering_translation.x = -y*text_centering_rotation.y;
          text_centering_translation.y =  y*text_centering_rotation.x;
        }
      }
    }
    else if (    ON::dtDimDiameter == ann_type 
              || ON::dtDimRadius == ann_type 
              || ON::dtLeader == ann_type 
              || ON::dtDimOrdinate == ann_type)
    {
      ON_2dPoint E(0.0,0.0); // end point
      ON_2dVector R(1.0,0.0); // unit vector from penultimate point to end point
      GetLeaderEndAndDirection( this, E, R );

      text_centering_rotation = R;

      text_centering_translation = (dimstyle_textgap + 0.5*text_line_width)*text_centering_rotation;

      ON_3dVector V = text_centering_rotation.x*m_plane.xaxis + text_centering_rotation.y*m_plane.yaxis;
      x = V*cameraX;
      y = V*cameraY;
      if ( fabs(x) <= ON_SQRT_EPSILON && fabs(y) > ON_SQRT_EPSILON )
      {
        x = y;
      }
      if ( x < 0.0 )
      {
        text_centering_rotation.Reverse(); // rotate 180 degrees
      }
    }
  }

  ON_Xform text_centering_xform(1.0);
  text_centering_xform.m_xform[0][0] =  text_centering_rotation.x;
  text_centering_xform.m_xform[0][1] = -text_centering_rotation.y;
  text_centering_xform.m_xform[1][0] =  text_centering_rotation.y;
  text_centering_xform.m_xform[1][1] =  text_centering_rotation.x;
  // Since the translation happens after the rotation about (0,0),
  // we can just tack it on here.
  text_centering_xform.m_xform[0][3] =  text_centering_translation.x;
  text_centering_xform.m_xform[1][3] =  text_centering_translation.y;

  // This transform translates the text in the annotation plane
  // It can be a large translation
  ON_2dVector text_offset_translation(0.0,0.0); // CRhinoText::Offset() = text->Offset()
  switch( ann_type )
  {
  case ON::dtDimLinear:
  case ON::dtDimAligned:
    if ( m_points.Count() >= ON_LinearDimension2::dim_pt_count )
    {
      const ON_LinearDimension2* linear_dim = ON_LinearDimension2::Cast(ann);
      if ( linear_dim )
      {
        text_offset_translation = linear_dim->Dim2dPoint(ON_LinearDimension2::text_pivot_pt);
      }
    }
    break;

  case ON::dtDimAngular:
    if ( m_points.Count() >= ON_AngularDimension2::dim_pt_count )
    {
      const ON_AngularDimension2* angular_dim = ON_AngularDimension2::Cast(ann);
      if ( angular_dim )
      {
        text_offset_translation = angular_dim->Dim2dPoint(ON_AngularDimension2::text_pivot_pt);
      }
    }
    break;

  case ON::dtDimDiameter:
  case ON::dtDimRadius:
    if ( m_points.Count() >= ON_RadialDimension2::dim_pt_count )
    {
      // No user positioned text on radial dimensions.
      text_offset_translation = m_points[ON_RadialDimension2::tail_pt_index];
    }
    break;

  case ON::dtLeader:
    if ( m_points.Count() > 0 )
    {
      // No user positioned text on leaders.
      text_offset_translation = *m_points.Last();
    }
    break;

  case ON::dtDimOrdinate:
    if ( m_points.Count() == 2 )
    {
      // No user positioned text on leaders.
      text_offset_translation = m_points[1];
    }
    break;

  case ON::dtTextBlock:
  case ON::dtNothing:
    break;
  }

  ON_Xform plane_translation(1.0);
  plane_translation.m_xform[0][3] = text_offset_translation.x;
  plane_translation.m_xform[1][3] = text_offset_translation.y;

  // this transform maps a point in the annotation plane to world coordinates
  ON_Xform plane_to_world(1.0);
  plane_to_world.Rotation(ON_xy_plane,ann->m_plane);

  ON_Xform horizonal_xform(1.0);
  if ( ON::dtHorizontal == dimstyle_textalignment )
  {
    ON_3dPoint fixed_point = ann->m_plane.PointAt(text_offset_translation.x,text_offset_translation.y);
    horizonal_xform.Rotation( 
        fixed_point,
        ann->m_plane.xaxis,
        ann->m_plane.yaxis,
        ann->m_plane.zaxis,
        fixed_point,
        cameraX,
        cameraY,
        cameraZ
        );

    if ( 2 == position_style )
    {
      // leaders, radial, and diameter
      ON_2dPoint E(0.0,0.0); // end point
      ON_2dVector R(1.0,0.0); // unit vector from penultimate point to end point
      GetLeaderEndAndDirection( this, E, R );
      ON_3dVector V = R.x*m_plane.xaxis + R.y*m_plane.yaxis;
      x = V*cameraX;
      y = ( x > -ON_SQRT_EPSILON )
        ? dimstyle_textgap
        : -(dimstyle_textgap + text_line_width);
      V = y*cameraX;
      horizonal_xform.m_xform[0][3] += V.x;
      horizonal_xform.m_xform[1][3] += V.y;
      horizonal_xform.m_xform[2][3] += V.z;
    }
  }

  ON_Xform gdi_to_world;
  gdi_to_world = horizonal_xform
                * plane_to_world
                * plane_translation
                * text_centering_xform
                * gdi_to_plane;

  xform = gdi_to_world;

  return true;
}


bool ON_Annotation2::GetTextPoint( ON_2dPoint& text_2d_point ) const
{
  bool rc = false;
  switch ( m_type )
  {
  case ON::dtTextBlock:
    text_2d_point.Set(0.0,0.0);
    rc = true;
    break;

  case ON::dtDimLinear:
  case ON::dtDimAligned:
    if ( m_userpositionedtext )
    {
      if ( m_points.Count() >= 5 )
      {
        text_2d_point = m_points[4];
        rc = true;
      }
    }
    else if ( m_points.Count() >= 3 )
    {
      text_2d_point.x = 0.5*(m_points[0].x + m_points[2].x);
      text_2d_point.y = m_points[2].y;
      rc = true;
    }
    break;

  case ON::dtLeader:
    if ( m_points.Count() > 0 )
    {
      text_2d_point = *m_points.Last();
      rc = true;
    }
    break;

  case ON::dtDimAngular:
    {
      const ON_AngularDimension2* angular_dim = ON_AngularDimension2::Cast(this);
      if ( angular_dim )
      {
        if ( m_userpositionedtext )
        {
          if ( m_points.Count() >= 0 )
          {
            text_2d_point = m_points[0];
          }
        }
        else
        {
          text_2d_point.x = angular_dim->m_radius*cos(angular_dim->m_angle);
          text_2d_point.y = angular_dim->m_radius*sin(angular_dim->m_angle);
          rc = true;
        }
      }
    }
    break;

  case ON::dtDimRadius:
  case ON::dtDimDiameter:
    // no user positioned text
    if ( m_points.Count() >= 3 )
    {
      text_2d_point = m_points[2];
      rc = true;
    }

    break;

  case ON::dtDimOrdinate:
  case ON::dtNothing:
    break;
  }
  return rc;
}

