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

#include "opennurbs.h"

ON_OBJECT_IMPLEMENT(ON_SurfaceProxy,ON_Surface,"4ED7D4E2-E947-11d3-BFE5-0010830122F0");

ON_SurfaceProxy::ON_SurfaceProxy() : m_surface(0), m_bTransposed(0)
{}

ON_SurfaceProxy::ON_SurfaceProxy( const ON_Surface* s ) : m_surface(s), m_bTransposed(0)
{}

ON_SurfaceProxy::ON_SurfaceProxy( const ON_SurfaceProxy& src ) : ON_Surface(src), m_surface(0), m_bTransposed(0)
{
  *this = src;
}

unsigned int ON_SurfaceProxy::SizeOf() const
{
  unsigned int sz = ON_Surface::SizeOf();
  sz += (sizeof(*this) - sizeof(ON_Surface));
  // Do not add in size of m_surface - its memory is not
  // managed by this class.
  return sz;
}

ON__UINT32 ON_SurfaceProxy::DataCRC(ON__UINT32 current_remainder) const
{
  if ( m_surface )
    current_remainder = m_surface->DataCRC(current_remainder);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_bTransposed),&m_bTransposed);
  return current_remainder;
}

ON_SurfaceProxy& ON_SurfaceProxy::operator=( const ON_SurfaceProxy& src )
{
  if ( this != &src ) {
    ON_Surface::operator=(src);
    m_surface = src.m_surface;
    m_bTransposed = src.m_bTransposed;
  }
  return *this;
}

ON_SurfaceProxy::~ON_SurfaceProxy()
{
  m_surface = 0;
}

void ON_SurfaceProxy::SetProxySurface( const ON_Surface* proxy_surface )
{
  // setting m_surface=0 prevents crashes if user has deleted
  // "real" surface before calling SetProxySurface().
  m_surface = 0;

  DestroySurfaceTree();
  if ( proxy_surface == this )
    proxy_surface = 0;
  m_surface = proxy_surface;
  m_bTransposed = false;
}

const ON_Surface* ON_SurfaceProxy::ProxySurface() const
{
  return m_surface;
}

bool ON_SurfaceProxy::ProxySurfaceIsTransposed() const
{
  return m_bTransposed;
}


ON_Surface* ON_SurfaceProxy::DuplicateSurface() const
{
  ON_Surface* dup_srf = 0;
  if ( m_surface )
  {
    dup_srf = m_surface->Duplicate();
    if ( m_bTransposed && dup_srf )
      dup_srf->Transpose();
  }
  return dup_srf;
}


ON_BOOL32
ON_SurfaceProxy::IsValid( ON_TextLog* text_log ) const
{
  return ( m_surface ) ? m_surface->IsValid(text_log) : false;
}

void
ON_SurfaceProxy::Dump( ON_TextLog& dump ) const
{
  dump.Print("ON_SurfaceProxy uses %x\n",m_surface);
  if (m_surface ) 
    m_surface->Dump(dump);
}

ON_BOOL32 
ON_SurfaceProxy::Write(
       ON_BinaryArchive&  // open binary file
     ) const
{
  return false;
}

ON_BOOL32 
ON_SurfaceProxy::Read(
       ON_BinaryArchive&  // open binary file
     )
{
  return false;
}

int 
ON_SurfaceProxy::Dimension() const
{
  return ( m_surface ) ? m_surface->Dimension() : 0;
}

ON_BOOL32 
ON_SurfaceProxy::GetBBox( // returns true if successful
         double* boxmin,    // minimum
         double* boxmax,    // maximum
         ON_BOOL32 bGrowBox
         ) const
{
  return ( m_surface ) ? m_surface->GetBBox(boxmin,boxmax,bGrowBox) : false;
}

ON_BOOL32
ON_SurfaceProxy::Transform( 
    const ON_Xform& // xform - formal parameter intentionally ignored in this virtual function
    )
{
  return false; // cannot modify m_surface
}


ON_Interval
ON_SurfaceProxy::Domain( int dir ) const
{
  ON_Interval d;
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  if ( m_surface ) 
    d = m_surface->Domain(dir);
  return d;
}

ON_BOOL32 ON_SurfaceProxy::GetSurfaceSize( 
    double* width, 
    double* height 
    ) const
{
  ON_BOOL32 rc = false;
  if ( m_surface )
  {
    if ( m_bTransposed )
    {
      double* ptr = width;
      width = height;
      height = ptr;
    }
    rc = m_surface->GetSurfaceSize(width,height);
  }
  else
  {
    if ( width )
      *width = 0.0;
    if ( height )
      *height = 0.0;
  }
  return rc;
}

int
ON_SurfaceProxy::SpanCount( int dir ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->SpanCount(dir) : false;
}

ON_BOOL32
ON_SurfaceProxy::GetSpanVector( int dir, double* s ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->GetSpanVector(dir,s) : false;
}

int
ON_SurfaceProxy::Degree( int dir ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->Degree(dir) : false;
}


ON_BOOL32 
ON_SurfaceProxy::GetParameterTolerance(
         int dir,
         double t,  // t = parameter in domain
         double* tminus, // tminus
         double* tplus   // tplus
         ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->GetParameterTolerance(dir,t,tminus,tplus) : false;
}

ON_BOOL32 
ON_SurfaceProxy::IsClosed( int dir ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->IsClosed( dir ) : false;
}

ON_Surface::ISO 
ON_SurfaceProxy::IsIsoparametric( // returns isoparametric status of 2d curve
        const ON_Curve& crv,
        const ON_Interval* subdomain
        ) const
{
  // this is a virtual overide of an ON_Surface::IsIsoparametric

  const ON_Curve* pC = &crv;
	ON_Curve* pTranC = NULL;
	if(m_bTransposed)
  {
		pTranC = crv.DuplicateCurve();
		pTranC->SwapCoordinates(0,1);
		pC = pTranC;
	}	

	ON_Surface::ISO iso = m_surface->IsIsoparametric( *pC, subdomain);

	if (pTranC)
  {
		switch(iso)
    {
		case x_iso:
			iso = y_iso;
		case y_iso:
			iso = x_iso;
		case W_iso:
			iso = S_iso;
		case S_iso:
			iso = W_iso;
		case N_iso:
			iso = E_iso;
		case E_iso:
			iso = N_iso;
    default:
      // intentionally ignoring other ON_Surface::ISO enum values
      break;
		}
    delete pTranC;
	}

	return iso;
}

ON_Surface::ISO 
ON_SurfaceProxy::IsIsoparametric( // returns isoparametric status based on bounding box
        const ON_BoundingBox& box
        ) const
{	
  // this is a virtual overide of an ON_Surface::IsIsoparametric
	const ON_BoundingBox* pbox = &box;
	ON_BoundingBox Tbox(	ON_2dPoint( box.m_min[1],box.m_min[0]),
												ON_2dPoint( box.m_max[1],box.m_max[0]) );
	if(m_bTransposed)
		pbox = &Tbox;

	ON_Surface::ISO iso = m_surface->IsIsoparametric( *pbox);

	if( m_bTransposed){
		switch(iso)
    {
		case x_iso:
			iso = y_iso;
		case y_iso:
			iso = x_iso;
		case W_iso:
			iso = S_iso;
		case S_iso:
			iso = W_iso;
		case N_iso:
			iso = E_iso;
		case E_iso:
			iso = N_iso;
    default:
      // intentionally ignoring other ON_Surface::ISO enum values
      break;
		}
	}
	return iso;
}




ON_BOOL32 ON_SurfaceProxy::IsPlanar(
      ON_Plane* plane,
      double tolerance
      ) const
{
  ON_BOOL32 rc = false;
  if ( m_surface )
  {
    rc = m_surface->IsPlanar( plane, tolerance );
    if (rc && m_bTransposed && plane )
      plane->Flip();
  }
  return rc;
}

ON_BOOL32 
ON_SurfaceProxy::IsPeriodic( int dir ) const
{
  if ( m_bTransposed ) {
    dir = (dir) ? 0 : 1;
  }
  return ( m_surface ) ? m_surface->IsPeriodic( dir ) : false;
}

bool ON_SurfaceProxy::GetNextDiscontinuity( 
                int dir,
                ON::continuity c,
                double t0,
                double t1,
                double* t,
                int* hint,
                int* dtype,
                double cos_angle_tolerance,
                double curvature_tolerance
                ) const
{
  // untested code
  bool rc = false;

  if ( 0 != m_surface && dir >= 0 && dir <= 1 )
  {
    rc = m_surface->GetNextDiscontinuity(m_bTransposed?1-dir:dir,c,t0,t1,t,hint,dtype,cos_angle_tolerance,curvature_tolerance);
  }

  return rc;
}

ON_BOOL32 
ON_SurfaceProxy::IsSingular( int side ) const
{
  if ( m_bTransposed ) {
    switch(side) {
    case 0:
      side = 3;
      break;
    case 1:
      side = 2;
      break;
    case 2:
      side = 1;
      break;
    case 3:
      side = 0;
      break;
    }
  }
  return ( m_surface ) ? m_surface->IsSingular( side ) : false;
}

ON_BOOL32
ON_SurfaceProxy::Reverse( 
    int // dir - formal parameter intentionally ignored in this virtual function
    )
{
  return false; // cannot modify m_surface
}

ON_BOOL32
ON_SurfaceProxy::Transpose()
{
  DestroySurfaceTree();
  m_bTransposed = (m_bTransposed) ? false : true;
  return true;
}

bool ON_SurfaceProxy::IsContinuous(
    ON::continuity desired_continuity,
    double s, 
    double t, 
    int* hint, // default = NULL,
    double point_tolerance, // default=ON_ZERO_TOLERANCE
    double d1_tolerance, // default==ON_ZERO_TOLERANCE
    double d2_tolerance, // default==ON_ZERO_TOLERANCE
    double cos_angle_tolerance, // default==0.99984769515639123915701155881391
    double curvature_tolerance  // default==ON_SQRT_EPSILON
    ) const
{
  bool rc = true;
  if ( m_surface )
  {
    if ( m_bTransposed )
    {
      double x = s;
      s = t;
      t = x;
    }
    rc = m_surface->IsContinuous( desired_continuity, s, t, hint,
                                      point_tolerance, d1_tolerance, d2_tolerance,
                                      cos_angle_tolerance, curvature_tolerance );
  }
  return rc;
}



ON_BOOL32 
ON_SurfaceProxy::Evaluate( // returns false if unable to evaluate
       double s, double t, // evaluation parameters
       int der_count,  // number of derivatives (>=0)
       int v_stride,   // v[] array stride (>=Dimension())
       double* v,      // v[] array of length stride*(ndir+1)
       int side,       // optional - determines which side to evaluate from
                       //         0 = default
                       //      <  0 to evaluate from below, 
                       //      >  0 to evaluate from above
       int* hint       // optional - evaluation hint (int) used to speed
                       //            repeated evaluations
       ) const
{
  if ( m_bTransposed ) {
    double x = s; s = t; t = x;
  }
  return ( m_surface ) ? m_surface->Evaluate(s,t,der_count,v_stride,v,side,hint) : false;
}


ON_Curve* ON_SurfaceProxy::IsoCurve(
       int dir,
       double c
       ) const
{
  ON_Curve* isocurve = 0;

  if ( m_bTransposed )
  {
    dir = 1-dir;
  }

  if ( 0 != m_surface && dir >= 0 && dir <= 1 )
  {
    isocurve = m_surface->IsoCurve( dir, c );
  }

  return isocurve;
}

ON_Curve* ON_SurfaceProxy::Pushup( const ON_Curve& curve_2d,
                  double tolerance,
                  const ON_Interval* curve_2d_subdomain
                  ) const
{
  ON_Curve* pushupcurve = 0;
  if ( 0 != m_surface )
  {
    if ( m_bTransposed )
    {
      ON_Curve* transposedcurve = curve_2d.DuplicateCurve();
      if ( 0 != transposedcurve )
      {
        transposedcurve->SwapCoordinates(0,1);
        pushupcurve = m_surface->Pushup( *transposedcurve, tolerance, curve_2d_subdomain );
        delete transposedcurve;
        transposedcurve = 0;
      }
    }
    else
    {
      pushupcurve = m_surface->Pushup( curve_2d, tolerance, curve_2d_subdomain );
    }
  }
  return pushupcurve;
}

ON_Curve* ON_SurfaceProxy::Pullback( const ON_Curve& curve_3d,
                  double tolerance,
                  const ON_Interval* curve_3d_subdomain,
                  ON_3dPoint start_uv,
                  ON_3dPoint end_uv
                  ) const
{
  ON_Curve* pullbackcurve = 0;
  if ( 0 != m_surface )
  {
    pullbackcurve = m_surface->Pullback( curve_3d, tolerance, curve_3d_subdomain, start_uv, end_uv );
    if ( m_bTransposed && 0 != pullbackcurve )
    {
      pullbackcurve->SwapCoordinates(0,1);
    }
  }
  return pullbackcurve;
}




bool ON_SurfaceProxy::GetClosestPoint( const ON_3dPoint& test_point,
        double* s, double* t,
        double maximum_distance,
        const ON_Interval* sdomain,
        const ON_Interval* tdomain
        ) const
{
  bool rc = false;
  if ( m_surface ) 
  {
    if ( m_bTransposed ) 
    {
      rc = m_surface->GetClosestPoint( test_point, t, s, maximum_distance, 
                                       tdomain, sdomain );
    }
    else 
    {
      rc = m_surface->GetClosestPoint( test_point, s, t, maximum_distance, 
                                       sdomain, tdomain );
    }
  }
  return rc;
}


//////////
// Find parameters of the point on a surface that is locally closest to 
// the test_point.  The search for a local close point starts at 
// seed parameters. If a sub_domain parameter is not NULL, then
// the search is restricted to the specified portion of the surface.
//
// true if returned if the search is successful.  false is returned if
// the search fails.
ON_BOOL32 ON_SurfaceProxy::GetLocalClosestPoint( const ON_3dPoint& test_point,
        double s0,double t0,     // seed_parameters
        double* s, double* t,   // parameters of local closest point returned here
        const ON_Interval* sdomain, // first parameter sub_domain
        const ON_Interval* tdomain  // second parameter sub_domain
        ) const
{
  ON_BOOL32 rc = false;
  if ( m_surface ) {
    if ( m_bTransposed ) {
      rc = m_surface->GetLocalClosestPoint( test_point, t0, s0, t, s, 
                                       tdomain, sdomain );
    }
    else {
      rc = m_surface->GetLocalClosestPoint( test_point, s0, t0, s, t, 
                                       sdomain, tdomain );
    }
  }
  return rc;
}

ON_Surface* ON_SurfaceProxy::Offset(
      double offset_distance, 
      double tolerance, 
      double* max_deviation
      ) const
{
  ON_Surface* offset_srf = NULL;
  if ( m_surface )
  {
    if ( m_bTransposed )
      offset_distance = -offset_distance;
    offset_srf = m_surface->Offset( offset_distance, tolerance, max_deviation );
    if ( offset_srf && m_bTransposed )
    {
      offset_srf->Transpose();
    }
  }
  return offset_srf;
}


int 
ON_SurfaceProxy::GetNurbForm( // returns 0: unable to create NURBS representation
                   //            with desired accuracy.
                   //         1: success - returned NURBS parameterization
                   //            matches the surface's to wthe desired accuracy
                   //         2: success - returned NURBS point locus matches
                   //            the surfaces's to the desired accuracy but, on
                   //            the interior of the surface's domain, the 
                   //            surface's parameterization and the NURBS
                   //            parameterization may not match to the 
                   //            desired accuracy.
        ON_NurbsSurface& nurbs,
        double tolerance
        ) const
{
  ON_BOOL32 rc = ( m_surface ) ? m_surface->GetNurbForm(nurbs,tolerance) : false;
  if ( rc && m_bTransposed ) {
    rc = nurbs.Transpose();
  }
  return rc;
}

int 
ON_SurfaceProxy::HasNurbForm( // returns 0: unable to create NURBS representation
                   //            with desired accuracy.
                   //         1: success - returned NURBS parameterization
                   //            matches the surface's to wthe desired accuracy
                   //         2: success - returned NURBS point locus matches
                   //            the surfaces's to the desired accuracy but, on
                   //            the interior of the surface's domain, the 
                   //            surface's parameterization and the NURBS
                   //            parameterization may not match to the 
                   //            desired accuracy.
        ) const

{
  if (!m_surface)
    return 0;
  return m_surface->HasNurbForm();
}

bool ON_SurfaceProxy::GetSurfaceParameterFromNurbFormParameter(
      double nurbs_s, double nurbs_t,
      double* surface_s, double* surface_t
      ) const
{
  bool rc = false;
  if ( m_surface )
  {
    rc = m_bTransposed 
       ? m_surface->GetSurfaceParameterFromNurbFormParameter(nurbs_t,nurbs_s,surface_t,surface_s)
       : m_surface->GetSurfaceParameterFromNurbFormParameter(nurbs_s,nurbs_t,surface_s,surface_t);
  }
  return rc;
}

bool ON_SurfaceProxy::GetNurbFormParameterFromSurfaceParameter(
      double surface_s, double surface_t,
      double* nurbs_s,  double* nurbs_t
      ) const
{
  bool rc = false;
  if ( m_surface )
  {
    rc = m_bTransposed 
       ? m_surface->GetNurbFormParameterFromSurfaceParameter(surface_t,surface_s,nurbs_t,nurbs_s)
       : m_surface->GetNurbFormParameterFromSurfaceParameter(surface_s,surface_t,nurbs_s,nurbs_t);
  }
  return rc;
}
