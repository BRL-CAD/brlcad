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

ON__m__GetLocalClosestPointOnBezierCurve   ON_BezierCurve::m__GetLocalClosestPointOnBezierCurve = 0;
ON__m__GetLocalBezierCurveSurfaceIntersection ON_BezierCurve::m__GetLocalBezierCurveSurfaceIntersection = 0;
ON__m__GetLocalBezierCurveCurveIntersection ON_BezierCurve::m__GetLocalBezierCurveCurveIntersection = 0;
ON__m__GetBezierCurveTightBoundingBox ON_BezierCurve::m__GetBezierCurveTightBoundingBox = 0;

ON__m__GetClosestPointOnBezierSurface ON_BezierSurface::m__GetClosestPointOnBezierSurface = 0;

int ON_ArcCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& x,
        double intersection_tolerance,
        const ON_Interval* curve_domain
        ) const
{
  return 0;
}

int ON_LineCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& x,
        double intersection_tolerance,
        const ON_Interval* curve_domain
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
          const ON_Surface* surfaceB,
          ON_SimpleArray<ON_X_EVENT>& x,
          double intersection_tolerance,
          double overlap_tolerance,
          const ON_Interval* line_domain,
          const ON_Interval* surfaceB_udomain,
          const ON_Interval* surfaceB_vdomain
          ) const
{
  return 0;
}

////////////////////////////////////////////////////////////////
//
// Basic ON_PlaneEquation functions
//

double ON_PlaneEquation::MinimumValueAt(const ON_SurfaceLeafBox& srfleafbox) const
{
  return 0.0;
}

double ON_PlaneEquation::MaximumValueAt(const ON_SurfaceLeafBox& srfleafbox) const
{
  return 0.0;
}

double ON_PlaneEquation::MinimumValueAt(const class ON_CurveLeafBox& crvleafbox) const
{
  return 0.0;
}

double ON_PlaneEquation::MaximumValueAt(const class ON_CurveLeafBox& crvleafbox) const
{
  return 0.0;
}

////////////////////////////////////////////////////////////////
//
// Basic ON_BezierCurve functions
//

int ON_BezierCurve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& x,
        double intersection_tolerance
        ) const
{
  return 0;
}

int ON_BezierCurve::IntersectCurve( 
        const ON_BezierCurve* bezierB,
        ON_SimpleArray<ON_X_EVENT>& x,
        double intersection_tolerance,
        double overlap_tolerance,
        const ON_Interval* bezierA_domain,
        const ON_Interval* bezierB_domain
        ) const
{
  return 0;
}

int ON_BezierCurve::IntersectSurface( 
          const ON_BezierSurface* bezsrfB,
          ON_SimpleArray<ON_X_EVENT>& x,
          double intersection_tolerance,
          double overlap_tolerance,
          const ON_Interval* bezierA_domain,
          const ON_Interval* bezsrfB_udomain,
          const ON_Interval* bezsrfB_vdomain
          ) const
{
  return 0;
}

bool ON_BezierCurve::GetLocalClosestPoint( 
        ON_3dPoint P,
        double seed_parameter,
        double* t,
        const ON_Interval* sub_domain
        ) const
{
  return false;
}

bool ON_BezierCurve::GetClosestPoint( 
        ON_3dPoint P,
        double* t,
        double maximum_distance,
        const ON_Interval* sub_domain
        ) const
{
  return false;
}

bool ON_BezierCurve::GetLocalCurveIntersection( 
        const ON_BezierCurve* other_bezcrv,
        double this_seed_t,
        double other_seed_t,
        double* this_t,
        double* other_t,
        const ON_Interval* this_domain,
        const ON_Interval* other_domain
        ) const
{
  return false;
}


bool ON_BezierCurve::GetLocalSurfaceIntersection( 
          const ON_BezierSurface* bezsrf,
          double seed_t,
          double seed_u,
          double seed_v,
          double* t,
          double* u,
          double* v,
          const ON_Interval* tdomain,
          const ON_Interval* udomain,
          const ON_Interval* vdomain
          ) const
{
  return false;
}


////////////////////////////////////////////////////////////////
//
// Basic ON_BezierSurface functions
//

bool ON_BezierSurface::GetLocalClosestPoint( 
        ON_3dPoint P,
        double s_seed,
        double t_seed,
        double* s,
        double* t,
        const ON_Interval* sub_domain0,
        const ON_Interval* sub_domain1
        ) const
{
  return false;
}

bool ON_BezierSurface::GetClosestPoint( 
        ON_3dPoint P,
        double* s,
        double* t,
        double maximum_distance,
        const ON_Interval* sub_domain0,
        const ON_Interval* sub_domain1
        ) const
{
  return false;
}


////////////////////////////////////////////////////////////////
//
// Basic ON_X_EVENT functions
//

bool ON_X_EVENT::IsValid(ON_TextLog* text_log,
                          double intersection_tolerance,
                          double overlap_tolerance,
                          const ON_Curve* curveA,
                          const ON_Interval* curveA_domain,
                          const ON_Curve* curveB,
                          const ON_Interval* curveB_domain,
                          const ON_Surface* surfaceB,
                          const ON_Interval* surfaceB_domain0,
                          const ON_Interval* surfaceB_domain1
                          ) const
{
  return true;
}

void ON_X_EVENT::CopyEventPart(
      const ON_X_EVENT& src, 
      int i,
      ON_X_EVENT& dst, 
      int j 
      )
{
}

bool ON_X_EVENT::IsValidList(
        int xevent_count,
        const ON_X_EVENT* xevent,
        ON_TextLog* text_log,
        double intersection_tolerance,
        double overlap_tolerance,
        const class ON_Curve* curveA,
        const class ON_Interval* curveA_domain,
        const class ON_Curve* curveB,
        const class ON_Interval* curveB_domain,
        const class ON_Surface* surfaceB,
        const class ON_Interval* surfaceB_domain0,
        const class ON_Interval* surfaceB_domain1
        )
{
  return true;
}

int ON_X_EVENT::CleanList(
        double event_tolerance,
        double overlap_tolerance,
        int xevent_count,
        ON_X_EVENT* xevent
        )
{
  return xevent_count;
}

bool ON_X_EVENT::IsValidCurveCurveOverlap( 
          ON_Interval curveA_domain,
          int sample_count,
          double overlap_tolerance,
          const class ON_CurveTreeNode* cnodeA, 
          const class ON_CurveTreeNode* cnodeB,
          const ON_Interval* curveB_domain
          )
{
  return true;
}

bool ON_X_EVENT::IsValidCurveSurfaceOverlap( 
                      ON_Interval curveA_domain,
                      int sample_count,
                      double overlap_tolerance,
                      const class ON_CurveTreeNode* cnodeA, 
                      const class ON_SurfaceTreeNode* snodeB,
                      const ON_Interval* surfaceB_udomain,
                      const ON_Interval* surfaceB_vdomain
                      )
{
  return true;
}

bool ON_X_EVENT::IsValidCurvePlaneOverlap( 
          ON_Interval curveA_domain,
          int sample_count,
          double endpoint_tolerance,
          double overlap_tolerance,
          const class ON_CurveTreeNode* cnodeA,
          const ON_PlaneEquation* plane_equation
          )
{
  return true;
}


////////////////////////////////////////////////////////////////
//
// Basic ON_Curve functions
//

ON_CurveTree* ON_Curve::CreateCurveTree() const
{
  return 0;
}

bool ON_Curve::GetClosestPoint( 
        const ON_3dPoint& test_point,
        double* t,       // parameter of local closest point returned here
        double maximum_distance,
        const ON_Interval* sub_domain
        ) const
{
  return false;
}

bool ON_Curve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return false;
}

int ON_Curve::IntersectSelf( 
        ON_SimpleArray<ON_X_EVENT>& x,
        double intersection_tolerance,
        const ON_Interval* curve_domain
        ) const
{
  return 0;
}

int ON_Curve::IntersectCurve( 
          const ON_Curve* curveB,
          ON_SimpleArray<ON_X_EVENT>& x,
          double intersection_tolerance,
          double overlap_tolerance,
          const ON_Interval* curveA_domain,
          const ON_Interval* curveB_domain
          ) const
{
  return 0;
}

int ON_Curve::IntersectSurface( 
          const ON_Surface* surfaceB,
          ON_SimpleArray<ON_X_EVENT>& x,
          double intersection_tolerance,
          double overlap_tolerance,
          const ON_Interval* curveA_domain,
          const ON_Interval* surfaceB_udomain,
          const ON_Interval* surfaceB_vdomain
          ) const
{
  return 0;
}

#endif
