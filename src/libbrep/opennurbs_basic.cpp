/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
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

#include "common.h"

#include "opennurbs.h"

int ON_ArcCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
        double UNUSED(intersection_tolerance),
        const ON_Interval* UNUSED(curve_domain)
        ) const
{
  return 0;
}

int ON_LineCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
        double UNUSED(intersection_tolerance),
        const ON_Interval* UNUSED(curve_domain)
        ) const
{
  return 0;
}

#if !defined(OPENNURBS_PLUS_INC_)

////////////////////////////////////////////////////////////////
//
// Basic ON_Line functions
//

int ON_Line::IntersectSurface( 
          const ON_Surface* UNUSED(surfaceB),
          ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
          double UNUSED(intersection_tolerance),
          double UNUSED(overlap_tolerance),
          const ON_Interval* UNUSED(line_domain),
          const ON_Interval* UNUSED(surfaceB_udomain),
          const ON_Interval* UNUSED(surfaceB_vdomain)
          ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

////////////////////////////////////////////////////////////////
//
// Basic ON_PlaneEquation functions
//

double ON_PlaneEquation::MinimumValueAt(const ON_SurfaceLeafBox& UNUSED(srfleafbox)) const
{
  // The working function is part of the Rhino SDK.
  return 0.0;
}

double ON_PlaneEquation::MaximumValueAt(const ON_SurfaceLeafBox& UNUSED(srfleafbox)) const
{
  // The working function is part of the Rhino SDK.
  return 0.0;
}

double ON_PlaneEquation::MinimumValueAt(const class ON_CurveLeafBox& UNUSED(crvleafbox)) const
{
  // The working function is part of the Rhino SDK.
  return 0.0;
}

double ON_PlaneEquation::MaximumValueAt(const class ON_CurveLeafBox& UNUSED(crvleafbox)) const
{
  // The working function is part of the Rhino SDK.
  return 0.0;
}

////////////////////////////////////////////////////////////////
//
// Basic ON_BezierCurve functions
//

int ON_BezierCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
        double UNUSED(intersection_tolerance)
        ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

int ON_BezierCurve::IntersectCurve( 
        const ON_BezierCurve* UNUSED(bezierB),
        ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
        double UNUSED(intersection_tolerance),
        double UNUSED(overlap_tolerance),
        const ON_Interval* UNUSED(bezierA_domain),
        const ON_Interval* UNUSED(bezierB_domain)
        ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

int ON_BezierCurve::IntersectSurface( 
          const ON_BezierSurface* UNUSED(bezsrfB),
          ON_SimpleArray<ON_X_EVENT>& UNUSED(x),
          double UNUSED(intersection_tolerance),
          double UNUSED(overlap_tolerance),
          const ON_Interval* UNUSED(bezierA_domain),
          const ON_Interval* UNUSED(bezsrfB_udomain),
          const ON_Interval* UNUSED(bezsrfB_vdomain)
          ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

bool ON_BezierCurve::GetLocalClosestPoint( 
        ON_3dPoint UNUSED(P),
        double UNUSED(seed_parameter),
        double* UNUSED(t),
        const ON_Interval* UNUSED(sub_domain)
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_BezierCurve::GetClosestPoint( 
        ON_3dPoint UNUSED(P),
        double* UNUSED(t),
        double UNUSED(maximum_distance),
        const ON_Interval* UNUSED(sub_domain)
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_BezierCurve::GetLocalCurveIntersection( 
        const ON_BezierCurve* UNUSED(other_bezcrv),
        double UNUSED(this_seed_t),
        double UNUSED(other_seed_t),
        double* UNUSED(this_t),
        double* UNUSED(other_t),
        const ON_Interval* UNUSED(this_domain),
        const ON_Interval* UNUSED(other_domain)
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}


bool ON_BezierCurve::GetLocalSurfaceIntersection( 
          const ON_BezierSurface* UNUSED(bezsrf),
          double UNUSED(seed_t),
          double UNUSED(seed_u),
          double UNUSED(seed_v),
          double* UNUSED(t),
          double* UNUSED(u),
          double* UNUSED(v),
          const ON_Interval* UNUSED(tdomain),
          const ON_Interval* UNUSED(udomain),
          const ON_Interval* UNUSED(vdomain)
          ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}


////////////////////////////////////////////////////////////////
//
// Basic ON_BezierSurface functions
//

bool ON_BezierSurface::GetLocalClosestPoint( 
        ON_3dPoint UNUSED(P),
        double UNUSED(s_seed),
        double UNUSED(t_seed),
        double* UNUSED(s),
        double* UNUSED(t),
        const ON_Interval* UNUSED(sub_domain0),
        const ON_Interval* UNUSED(sub_domain1)
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_BezierSurface::GetClosestPoint( 
        ON_3dPoint UNUSED(P),
        double* UNUSED(s),
        double* UNUSED(t),
        double UNUSED(maximum_distance),
        const ON_Interval* UNUSED(sub_domain0),
        const ON_Interval* UNUSED(sub_domain1)
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}


////////////////////////////////////////////////////////////////
//
// Basic ON_X_EVENT functions
//

bool ON_X_EVENT::IsValid(ON_TextLog* UNUSED(text_log),
                          double UNUSED(intersection_tolerance),
                          double UNUSED(overlap_tolerance),
                          const ON_Curve* UNUSED(curveA),
                          const ON_Interval* UNUSED(curveA_domain),
                          const ON_Curve* UNUSED(curveB),
                          const ON_Interval* UNUSED(curveB_domain),
                          const ON_Surface* UNUSED(surfaceB),
                          const ON_Interval* UNUSED(surfaceB_domain0),
                          const ON_Interval* UNUSED(surfaceB_domain1)
                          ) const
{
  // The working function is part of the Rhino SDK.
  return true;
}

void ON_X_EVENT::CopyEventPart(
      const ON_X_EVENT& UNUSED(src),
      int UNUSED(i),
      ON_X_EVENT& UNUSED(dst),
      int UNUSED(j)
      )
{
  // The working function is part of the Rhino SDK.
}

bool ON_X_EVENT::IsValidList(
        int UNUSED(xevent_count),
        const ON_X_EVENT* UNUSED(xevent),
        ON_TextLog* UNUSED(text_log),
        double UNUSED(intersection_tolerance),
        double UNUSED(overlap_tolerance),
        const class ON_Curve* UNUSED(curveA),
        const class ON_Interval* UNUSED(curveA_domain),
        const class ON_Curve* UNUSED(curveB),
        const class ON_Interval* UNUSED(curveB_domain),
        const class ON_Surface* UNUSED(surfaceB),
        const class ON_Interval* UNUSED(surfaceB_domain0),
        const class ON_Interval* UNUSED(surfaceB_domain1)
        )
{
  // The working function is part of the Rhino SDK.
  return true;
}

int ON_X_EVENT::CleanList(
        double UNUSED(event_tolerance),
        double UNUSED(overlap_tolerance),
        int xevent_count,
        ON_X_EVENT* UNUSED(xevent)
        )
{
  // The working function is part of the Rhino SDK.
  return xevent_count;
}

bool ON_X_EVENT::IsValidCurveCurveOverlap(
          ON_Interval,
          int,
          double,
          const class ON_CurveTreeNode*,
          const class ON_CurveTreeNode*,
          const ON_Interval*
          )
{
  // The working function is part of the Rhino SDK.
  return true;
}

bool ON_X_EVENT::IsValidCurveSurfaceOverlap( 
                      ON_Interval,
                      int,
                      double,
                      const class ON_CurveTreeNode*, 
                      const class ON_SurfaceTreeNode*,
                      const ON_Interval*,
                      const ON_Interval*
                      )
{
  // The working function is part of the Rhino SDK.
  return true;
}

bool ON_X_EVENT::IsValidCurvePlaneOverlap( 
          ON_Interval,
          int,
          double,
          double,
          const class ON_CurveTreeNode*,
          const ON_PlaneEquation*
          )
{
  // The working function is part of the Rhino SDK.
  return true;
}

ON_CurveTree* ON_Curve::CreateCurveTree() const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

bool ON_Curve::GetClosestPoint( 
        const ON_3dPoint&,
        double*,
        double,
        const ON_Interval*
        ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

int ON_Curve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>&,
        double,
        const ON_Interval*
        ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

int ON_Curve::IntersectCurve( 
          const ON_Curve*,
          ON_SimpleArray<ON_X_EVENT>&,
          double,
          double,
          const ON_Interval*,
          const ON_Interval*
          ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

int ON_Curve::IntersectSurface( 
          const ON_Surface*,
          ON_SimpleArray<ON_X_EVENT>&,
          double,
          double,
          const ON_Interval*,
          const ON_Interval*,
          const ON_Interval*
          ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

int ON_Surface::IntersectSurface( 
        const ON_Surface*,
        ON_ClassArray<ON_SSX_EVENT>&,
        double,
        double,
        double,
        const ON_Interval*,
        const ON_Interval*,
        const ON_Interval*,
        const ON_Interval*
        ) const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

ON_SurfaceTree* ON_Surface::CreateSurfaceTree() const
{
  // The working function is part of the Rhino SDK.
  return 0;
}

class ON_NurbsSurface* ON_Surface::CreateCubicLoft(
    int,
    const ON_Curve* const*,
    double,
    int,
    ON::cubic_loft_end_condition,
    ON::cubic_loft_end_condition,
    class ON_NurbsSurface*
    )
{
  // The working function is part of the Rhino SDK.
  return 0;
}

bool ON_Surface::AreaMassProperties(
  ON_MassProperties&,
  bool,
  bool,
  bool,
  bool,
  double,
  double
  ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_Surface::VolumeMassProperties(
  ON_MassProperties&, 
  bool,
  bool,
  bool,
  bool,
  ON_3dPoint,
  double,
  double
  ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_Curve::LengthMassProperties(
  ON_MassProperties&,
  bool,
  bool,
  bool,
  bool,
  double,
  double
  ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

bool ON_Curve::AreaMassProperties(
    ON_3dPoint,
    ON_3dVector,
    ON_MassProperties&, 
    bool,
    bool,
    bool,
    bool,
    double,
    double
    ) const
{
  // The working function is part of the Rhino SDK.
  return false;
}

ON_PolylineCurve* ON_Curve::MeshCurve(
    ON_MeshCurveParameters&,
    ON_PolylineCurve*,
    bool,
    const ON_Interval*
    )
{
  // The working function is part of the Rhino SDK.
  return 0;
}


#endif
