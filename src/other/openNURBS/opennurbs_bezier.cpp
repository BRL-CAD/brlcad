/* $Header$ */
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
#include <assert.h>
#include <float.h>
#include <list>

ON_PolynomialCurve::ON_PolynomialCurve()
		   : m_dim(0), m_is_rat(0), m_order(0), m_domain(0.0,1.0)
{}

ON_PolynomialCurve::ON_PolynomialCurve( int dim, BOOL is_rat, int order )
		   : m_dim(0), m_is_rat(0), m_order(0), m_domain(0.0,1.0)
{
  Create( dim, is_rat, order );
}

ON_PolynomialCurve::~ON_PolynomialCurve()
{
  Destroy();
}

ON_PolynomialCurve::ON_PolynomialCurve(const ON_PolynomialCurve& src)
		   : m_dim(0), m_is_rat(0), m_order(0), m_domain(0.0,1.0)
{
  *this = src;
}

ON_PolynomialCurve::ON_PolynomialCurve(const ON_BezierCurve& src)
		   : m_dim(0), m_is_rat(0), m_order(0), m_domain(0.0,1.0)
{
  *this = src;
}

BOOL ON_PolynomialCurve::Create( int dim, BOOL is_rat, int order )
{
  BOOL rc = true;
  if ( dim > 0 ) m_dim = dim; else {m_dim = 0; rc = false;}
  m_is_rat = is_rat?1:0;
  if ( order >= 1 ) m_order = order; else {m_order = 0; rc = false;}
  m_cv.SetCapacity(m_order);
  m_domain.m_t[0] = 0.0;
  m_domain.m_t[1] = 1.0;
  return rc;
}

void ON_PolynomialCurve::Destroy()
{
  m_dim = 0;
  m_is_rat = 0;
  m_order = 0;
  m_cv.Destroy();
  m_domain.m_t[0] = 0.0;
  m_domain.m_t[1] = 1.0;
}


ON_PolynomialCurve& ON_PolynomialCurve::operator=(const ON_PolynomialCurve& src)
{
  if ( this != &src ) {
    m_dim = src.m_dim;
    m_is_rat = src.m_is_rat;
    m_order = src.m_order;
    m_cv = src.m_cv;
    m_domain = src.m_domain;
  }
  return *this;
}

ON_PolynomialCurve& ON_PolynomialCurve::operator=(const ON_BezierCurve& src)
{
  int i;
  double d;
  m_dim = src.m_dim;
  m_is_rat = src.m_is_rat;
  m_order = src.m_order;
  m_cv.Reserve( src.m_order );
  m_cv.SetCount( src.m_order );
  m_cv.Zero();
  //m_domain = src.m_domain;

  if ( m_order >= 2 && src.CVSize() <= 4 ) {
    ON_BezierCurve s; // scratch surface for homogeneous evaluation
    s.m_dim = src.m_is_rat ? src.m_dim+1 : src.m_dim;
    s.m_is_rat = 0;
    s.m_order = src.m_order;
    s.m_cv = src.m_cv;
    //s.m_domain.m_t[0] = 0.0;
    //s.m_domain.m_t[1] = 1.0;
    if ( s.Evaluate( 0.0, m_order-1, 4, &m_cv[0].x ) ) {
      if ( m_is_rat ) {
	if ( m_dim < 3 ) {
	  for ( i = 0; i < m_order; i++ ) {
	    ON_4dPoint& cv = m_cv[i];
	    cv.w = cv[m_dim];
	    cv[m_dim] = 0.0;
	  }
	}
      }
      else {
	m_cv[0].w = 1.0;
      }
      for ( i = 2; i < m_order; i++ ) {
	d = 1.0/i;
	ON_4dPoint& cv = m_cv[i];
	cv.x *= d;
	cv.y *= d;
	cv.z *= d;
	cv.w *= d;
      }
    }
    else {
      m_cv.Zero();
      m_cv[0].w = 1.0;
    }
    s.m_cv = 0;
  }

  return *this;
}

BOOL ON_PolynomialCurve::Evaluate( // returns false if unable to evaluate
       double t,         // evaluation parameter
       int der_count,    // number of derivatives (>=0)
       int v_stride,     // array stride (>=Dimension())
       double* v         // array of length stride*(ndir+1)
       ) const
{
  BOOL rc = false;
  if ( m_order >= 1 && m_cv.Count() == m_order )
  {
    if ( m_domain.m_t[0] != 0.0 || m_domain.m_t[1] != 1.0 ) 
    {
      t = (1.0-t)*m_domain.m_t[0] + t*m_domain.m_t[1];
    }
    ON_4dPointArray p(der_count+1);
    ON_4dPoint c;
    double s;
    int i, j, der;
    p.Zero();
    for ( i = m_order-1; i >= 0; i-- ) {
      c = m_cv[i];
      p[0].x = t*p[0].x + c.x;
      p[0].y = t*p[0].y + c.y;
      p[0].z = t*p[0].z + c.z;
      p[0].w = t*p[0].w + c.w;
    }
    if ( der_count >= 1 ) {
      for ( i = m_order-1; i >= 1; i-- ) {
	c = m_cv[i];
	p[1].x = t*p[1].x + i*c.x;
	p[1].y = t*p[1].y + i*c.y;
	p[1].z = t*p[1].z + i*c.z;
	p[1].w = t*p[1].w + i*c.w;
      }
      for ( der = 2; der <= der_count; der++ ) {
	for ( i = m_order-1; i >= der; i-- ) {
	  s = i;
	  for ( j = 1; j < der; j++ ) {
	    s *= (i-j);
	  }
	  c = m_cv[i];
	  p[der].x = t*p[der].x + s*c.x;
	  p[der].y = t*p[der].y + s*c.y;
	  p[der].z = t*p[der].z + s*c.z;
	  p[der].w = t*p[der].w + s*c.w;
	}
      }
      if ( m_is_rat ) {
	ON_EvaluateQuotientRule( 3, der_count, 4, &p[0].x );
      }
    }
    const int sz = m_dim*sizeof(v[0]);
    for ( i = 0; i <= der_count; i++ ) {
      memcpy( v, &p[i].x, sz );
      v += v_stride;
    }
    rc = true;
  }
  return rc;
}

ON_PolynomialSurface::ON_PolynomialSurface() : m_dim(0), m_is_rat(0)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_domain[0].m_t[0] = 0.0;
  m_domain[0].m_t[1] = 1.0;
  m_domain[1].m_t[0] = 0.0;
  m_domain[1].m_t[1] = 1.0;
}

ON_PolynomialSurface::ON_PolynomialSurface( int dim, BOOL is_rat, int order0, int order1 )
{
  Create( dim, is_rat, order0, order1 );
}

ON_PolynomialSurface::~ON_PolynomialSurface()
{
  Destroy();
}

ON_PolynomialSurface::ON_PolynomialSurface(const ON_PolynomialSurface& src)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_domain[0].m_t[0] = 0.0;
  m_domain[0].m_t[1] = 1.0;
  m_domain[1].m_t[0] = 0.0;
  m_domain[1].m_t[1] = 1.0;
  *this = src;
}

ON_PolynomialSurface::ON_PolynomialSurface(const ON_BezierSurface& src)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_domain[0].m_t[0] = 0.0;
  m_domain[0].m_t[1] = 1.0;
  m_domain[1].m_t[0] = 0.0;
  m_domain[1].m_t[1] = 1.0;
  *this = src;
}

ON_PolynomialSurface& ON_PolynomialSurface::operator=(const ON_PolynomialSurface& src)
{
  if ( this != &src ) {
    if ( Create( src.m_dim, src.m_is_rat, src.m_order[0], src.m_order[1] ) ) {
      m_cv = src.m_cv;
      m_domain[0] = src.m_domain[0];
      m_domain[1] = src.m_domain[1];
    }
  }
  return *this;
}

ON_PolynomialSurface& ON_PolynomialSurface::operator=(const ON_BezierSurface& src)
{
  if ( Create( src.m_dim, src.m_is_rat, src.m_order[0], src.m_order[1] ) ) {
    //m_domain[0] = src.m_domain[0];
    //m_domain[1] = src.m_domain[1];
    // TODO - convert coefficients from Bernstein to monomial basis
  }
  return *this;
}

BOOL ON_PolynomialSurface::Create( int dim, int is_rat, int order0, int order1 )
{
  BOOL rc = true;
  if ( dim > 0 ) m_dim = dim; else {m_dim = 0; rc = false;};
  m_is_rat = is_rat?1:0;
  if ( order0 > 0 ) m_order[0] = order0; else { m_order[0] = 0; rc = false;}
  if ( order1 > 0 ) m_order[1] = order1; else { m_order[1] = 0; rc = false;}
  m_cv.SetCapacity( m_order[0]*m_order[1] );
  if ( m_order[0] > 0 && m_order[1] > 0 ) {
    m_cv.Zero();
    m_cv[0].w = 1.0;
  }
  return rc;
}

void ON_PolynomialSurface::Destroy()
{
  m_dim = 0;
  m_is_rat = 0;
  m_order[0] = 0;
  m_order[1] = 0;
  m_cv.Destroy();
}

BOOL ON_PolynomialSurface::Evaluate( // returns false if unable to evaluate
       double s, 
       double t,           // evaluation parameter
       int der_count,      // number of derivatives (>=0)
       int v_stride,       // array stride (>=Dimension())
       double* v           // array of length stride*(ndir+1)*(ndir+2)/2
       ) const
{
  ON_ERROR("TODO: - finish ON_PolynomialSurface::Evaluate()\n");
  return false;
}

ON_BezierCurve::ON_BezierCurve()
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0), 
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
}

ON_BezierCurve::ON_BezierCurve( int dim, BOOL is_rat, int order )
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  Create(dim,is_rat,order);
}

ON_BezierCurve::~ON_BezierCurve()
{
  Destroy();
}

ON_BezierCurve::ON_BezierCurve(const ON_BezierCurve& src)
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  *this = src;
}

ON_BezierCurve::ON_BezierCurve(const ON_PolynomialCurve& src)
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  *this = src;
}

ON_BezierCurve::ON_BezierCurve(const ON_2dPointArray& cv)
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  *this = cv;
}

ON_BezierCurve::ON_BezierCurve(const ON_3dPointArray& cv)
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  *this = cv;
}

ON_BezierCurve::ON_BezierCurve(const ON_4dPointArray& cv)
	       : m_dim(0), 
		 m_is_rat(0), 
		 m_order(0), 
		 m_cv_stride(0), 
		 m_cv(0),
		 m_cv_capacity(0)
{
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierCurve = 0;
#endif
  *this = cv;
}

ON_BezierCurve& ON_BezierCurve::operator=(const ON_2dPointArray& cv)
{
  if ( Create( 2, false, cv.Count() ) )
  {
    int i;
    for ( i = 0; i < m_order; i++ )
      SetCV( i, ON::intrinsic_point_style, cv[i] );
  }
  return *this;
}

ON_BezierCurve& ON_BezierCurve::operator=(const ON_3dPointArray& cv)
{
  if ( Create( 3, false, cv.Count() ) )
  {
    int i;
    for ( i = 0; i < m_order; i++ )
      SetCV( i, ON::intrinsic_point_style, cv[i] );
  }
  return *this;
}

ON_BezierCurve& ON_BezierCurve::operator=(const ON_4dPointArray& cv)
{
  if ( Create( 3, true, cv.Count() ) )
  {
    int i;
    for ( i = 0; i < m_order; i++ )
      SetCV( i, ON::intrinsic_point_style, cv[i] );
  }
  return *this;
}


ON_BezierCurve& ON_BezierCurve::operator=(const ON_BezierCurve& src)
{
  int i;
  if ( this != &src ) {
    //m_domain = src.m_domain;
    if ( Create( src.m_dim, src.m_is_rat, src.m_order ) ) {
      const int sizeof_cv = CVSize()*sizeof(m_cv[0]);
      for ( i = 0; i < m_order; i++ ) {
	memcpy( CV(i), src.CV(i), sizeof_cv );
      }
    }
  }
  return *this;
}

ON_BezierCurve& ON_BezierCurve::operator=(const ON_PolynomialCurve& src)
{
  //m_domain = src.m_domain;
  if ( src.m_dim > 0 && src.m_cv.Count() == src.m_order && src.m_order >= 2 ) 
  {
    int i;
    
    ON_PolynomialCurve s; // scratch poly for homogeneous evaluation
    s.m_dim = src.m_is_rat ? 4 : src.m_dim; 
    s.m_is_rat = 0;
    s.m_domain.m_t[0] = 0.0;
    s.m_domain.m_t[1] = 1.0;
    s.m_order = src.m_order;
    s.m_cv = src.m_cv;

    if ( src.m_is_rat ) {
      m_dim++;
      m_is_rat = 0;
    }     
    const int degree = src.m_order-1;
    const double d = 1.0/degree;
    double t;
    ON_4dPointArray pt(src.m_order);
    for ( i = 0; i < src.m_order; i++ ) 
    {
      if ( i == 0 )
	t = 0.0;
      else if ( i < degree )
	t = i*d;
      else
	t = 1.0;
      s.Evaluate( t, 0, 4, pt[i] );
    }
    s.m_cv = 0;
    if ( src.m_is_rat && src.m_dim < 3 ) {
      for ( i = 0; i < src.m_order; i++ )
	pt[i][src.m_dim] = pt[i].w;
    }
    Loft( src.m_is_rat ? src.m_dim+1 : src.m_dim, src.m_order, 4, &pt[0].x, 0, NULL );
    if ( IsValid() && src.m_is_rat ) {
      m_is_rat = 1;
      m_dim--;
    }
  }
  else {
    Destroy();
  }
  
  return *this;
}

bool ON_BezierCurve::Loft( const ON_3dPointArray& pt )
{
  const ON_3dPoint* p0 = pt.Array();
  return Loft( 3, pt.Count(), 3, p0?&p0->x:0, 0, NULL );
}


bool ON_BezierCurve::Loft( int pt_dim, int pt_count, int pt_stride, const double* pt,
			   int t_stride, const double* t )
{
  bool rc = false;
  if ( pt_dim >= 1 && pt_count >= 2 && pt_stride >= pt_dim && pt != NULL && (t_stride >= 1 || t == 0) ) {
    int i, j;
    ON_SimpleArray<double> uniform_t;
    double s;
    if ( !t ) {
      uniform_t.Reserve(pt_count);
      s = 1.0/(pt_count-1);
      for ( i = 0; i < pt_count; i++ ) {
	uniform_t.Append(i*s);
      }
      uniform_t[0] = 0.0;
      uniform_t[pt_count-1] = 1.0;
      t = uniform_t.Array();
      t_stride = 1;
    }
    Create( pt_dim, false, pt_count );
    const int sizeof_cv = CVSize()*sizeof(m_cv[0]);
    const int degree = m_order-1;
    const double t0 = t[0];
    const double t1 = t[t_stride*(pt_count-1)];
    //m_domain.Set(t0,t1);
    const double tm = 0.5*(t1-t0);
    //const double d = 1.0/m_domain.Length();
    const double d = 1.0/(t1-t0);
    ON_Matrix M( m_order, m_order );
    for ( i = 0; i < m_order; i++ ) {
      if ( t[i] <= tm ) {
	s = (t[i] - t[0])*d;
      }
      else {
	s = 1.0 - (t1 - t[i])*d;
      }
      for ( j = 0; j < m_order; j++ ) {
	M.m[i][j] = ON_EvaluateBernsteinBasis( degree, j, s );
      }
      memcpy( m_cv + i*m_cv_stride, pt + i*pt_stride, sizeof_cv );
    }  

    int rank = M.RowReduce( ON_EPSILON, m_dim, m_cv_stride, m_cv );
    M.BackSolve( ON_EPSILON, m_dim, m_order, 
		 m_cv_stride, m_cv, 
		 m_cv_stride, m_cv );
    if ( rank == m_order )
      rc = true;
  }
  return rc;
}

bool ON_BezierCurve::IsValid() const
{
  if ( m_dim <= 0 )
    return false;
  if ( m_is_rat != 0 && m_is_rat != 1 )
    return false;
  if ( m_order < 2 )
    return false;
  if ( m_cv_stride < m_dim+m_is_rat )
    return false;
  if ( m_cv_capacity > 0 && m_cv_capacity < m_cv_stride*m_order )
    return false;
  if ( m_cv == NULL )
    return false;
  return true;
}

void ON_BezierCurve::Dump( ON_TextLog& dump ) const
{
  dump.Print( "ON_BezierCurve dim = %d is_rat = %d\n"
	       "        order = %d \n",
	       m_dim, m_is_rat, m_order );
  dump.Print( "Control Points  %d %s points\n"
	       "  index               value\n",
	       m_order, 
	       (m_is_rat) ? "rational" : "non-rational" );
  if ( !m_cv ) {
    dump.Print("  NULL cv array\n");
  }
  else {
    dump.PrintPointList( m_dim, m_is_rat, 
		      m_order, m_cv_stride,
		      m_cv, 
		      "  CV" );
  }
}

int ON_BezierCurve::Dimension() const
{
  return m_dim;
}

bool ON_BezierCurve::Create( int dim, int is_rat, int order )
{
  m_dim = (dim>0) ? dim : 0;
  m_is_rat = is_rat ? 1 : 0;
  // 7 July 2005 Dale Lear
  //     Permit order=1 (degree=0) beziers with 1 cv
  //     for dots.
  m_order = (order >= 1) ? order : 0;
  m_cv_stride = (m_dim > 0) ? m_dim+m_is_rat : 0;
  m_cv_capacity = m_cv_stride*m_order;
  m_cv = (double*)onrealloc( m_cv, m_cv_capacity*sizeof(m_cv[0]) );
  //m_domain.m_t[0] = 0.0;
  //m_domain.m_t[1] = 1.0;
  return IsValid();
}

void ON_BezierCurve::Destroy()
{
  if ( m_cv && m_cv_capacity > 0 )
    onfree(m_cv);
  m_cv_capacity = 0;
  m_cv_stride = 0;
  m_cv = 0;
  m_dim = 0;
  m_is_rat = 0;
  m_order = 0;
  //m_domain.m_t[0] = 0.0;
  //m_domain.m_t[1] = 1.0;
}

void ON_BezierCurve::EmergencyDestroy()
{
  m_cv = 0;
}

bool ON_BezierCurve::GetBBox( // returns true if successful
       double* boxmin,    // minimum
       double* boxmax,    // maximum
       int bGrowBox
       ) const
{
  return ON_GetPointListBoundingBox( m_dim, m_is_rat, m_order, m_cv_stride, m_cv, boxmin, boxmax, bGrowBox );
}

bool ON_BezierCurve::GetBoundingBox( // returns true if successful
       ON_BoundingBox& bbox,
       int bGrowBox             // true means grow box
       ) const
{
  double *boxmin, *boxmax;
  if ( m_dim > 3 ) 
  {
    boxmin = (double*)alloca(2*m_dim*sizeof(*boxmin));
    memset( boxmin, 0, 2*m_dim*sizeof(*boxmin) );
    boxmax = boxmin + m_dim;
    if ( bGrowBox ) {
      boxmin[0] = bbox.m_min.x;
      boxmin[1] = bbox.m_min.y;
      boxmin[2] = bbox.m_min.z;
      boxmax[0] = bbox.m_max.x;
      boxmax[1] = bbox.m_max.y;
      boxmax[2] = bbox.m_max.z;
    }
  }
  else {
    boxmin = &bbox.m_min.x;
    boxmax = &bbox.m_max.x;
  }
  bool rc = GetBBox( boxmin, boxmax, bGrowBox );
  if ( rc && m_dim > 3 ) {
    bbox.m_min = boxmin;
    bbox.m_max = boxmax;
  }
  return rc;
}

ON_BoundingBox ON_BezierCurve::BoundingBox() const
{
  ON_BoundingBox bbox;
  GetBoundingBox( bbox );
  return bbox;
}

bool ON_BezierCurve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return m__GetBezierCurveTightBoundingBox 
	? m__GetBezierCurveTightBoundingBox(this,&tight_bbox,bGrowBox?true:false,xform) 
	: false;
}


bool ON_WorldBBoxIsInTightBBox( 
		    const ON_BoundingBox& tight_bbox, 
		    const ON_BoundingBox& world_bbox,
		    const ON_Xform* xform
		    )
{
  if ( xform && !xform->IsIdentity() )
  {
    ON_3dPoint P, Q;
    int i,j,k;
    for ( i = 0; i < 2; i++ )
    {
      P.x = (i) ? world_bbox.m_min.x : world_bbox.m_max.x;
      for ( j = 0; j < 2; j++ )
      {
	P.y = (j) ? world_bbox.m_min.y : world_bbox.m_max.y;
	for ( k = 0; k < 2; k++ )
	{
	  P.z = (k) ? world_bbox.m_min.z : world_bbox.m_max.z;
	  Q = (*xform)*P;
	  if ( !tight_bbox.IsPointIn(Q) )
	  {
	    return false;
	  }
	}
      }
    }
    return true;
  }

  return ( tight_bbox.Includes(world_bbox) );
}

bool ON_Line::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }
  if ( !bGrowBox )
  {
    tight_bbox.Destroy();
  }

  if ( xform && !xform->IsIdentity() )
  {
    ON_3dPoint P = (*xform)*from;
    tight_bbox.Set(P,bGrowBox);
    bGrowBox = true;
    P = (*xform)*to;
    tight_bbox.Set(P,bGrowBox);
  }
  else
  {
    tight_bbox.Set(from,bGrowBox);
    bGrowBox = true;
    tight_bbox.Set(to,bGrowBox);
  }

  return (0!=bGrowBox);
}

bool ON_Arc::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }
  if ( !bGrowBox )
  {
    tight_bbox.Destroy();
  }

  ON_NurbsCurve nurbs_arc;
  if ( GetNurbForm(nurbs_arc) )
  {
    if ( xform && !xform->IsIdentity() )
    {
      nurbs_arc.Transform(*xform);
    }
    ON_BezierCurve bez_arc;
    bez_arc.m_dim = nurbs_arc.m_dim;
    bez_arc.m_is_rat = nurbs_arc.m_is_rat;
    bez_arc.m_order = nurbs_arc.m_order;
    bez_arc.m_cv_stride = nurbs_arc.m_cv_stride;
    bez_arc.m_cv = nurbs_arc.m_cv;
    int i;
    for ( i = nurbs_arc.m_order-2; i < nurbs_arc.m_cv_count-1; i++, bez_arc.m_cv += bez_arc.m_cv_stride )
    {
      if ( nurbs_arc.m_knot[i] < nurbs_arc.m_knot[i+1] )
      {
	if ( bez_arc.GetTightBoundingBox( tight_bbox, bGrowBox, 0 ) )
	  bGrowBox = true;
      }
    }
    bez_arc.m_cv = 0;
  }

  return (0!=bGrowBox);
}

bool ON_Circle::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  ON_Arc arc(*this,2.0*ON_PI);
  return arc.GetTightBoundingBox(tight_bbox,bGrowBox,xform);
}

bool ON_ArcCurve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return m_arc.GetTightBoundingBox(tight_bbox,bGrowBox,xform);
}

bool ON_LineCurve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return m_line.GetTightBoundingBox(tight_bbox,bGrowBox,xform);
}

bool ON_PolylineCurve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return m_pline.GetTightBoundingBox(tight_bbox,bGrowBox,xform);
}

bool ON_PolyCurve::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  return m_segment.GetTightBoundingBox(tight_bbox,bGrowBox,xform);
}

bool ON_CurveArray::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform
    ) const
{
  if ( 1 == m_count && m_a[0] )
  {
    return m_a[0]->GetTightBoundingBox(tight_bbox,bGrowBox,xform);
  }

  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }
  if ( !bGrowBox )
  {
    tight_bbox.Destroy();
  }

  if ( m_count > 0 )
  {
    int i;
    // getting box of endpoints tends to help us avoid testing curves
    ON_3dPointArray P(2*m_count);
    for ( i = 0; i < m_count; i++ )
    {
      if ( m_a[i] )
      {
	P.Append( m_a[i]->PointAtStart() );
	P.Append( m_a[i]->PointAtEnd() );
      }
    }
    if ( P.GetTightBoundingBox(tight_bbox,bGrowBox,xform) )
    {
      bGrowBox = true;
    }

    for ( i = 0; i < m_count; i++ )
    {
      if ( m_a[i] )
      {
	if ( m_a[i]->GetTightBoundingBox(tight_bbox,bGrowBox,xform) )
	{
	  bGrowBox = true;
	}
      }
    }
  }

  return (0!=bGrowBox);  
}

bool ON_3dPointArray::GetTightBoundingBox( 
			ON_BoundingBox& tight_bbox, 
      int bGrowBox,
			const ON_Xform* xform
      ) const
{
  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }
  if ( !bGrowBox )
  {
    tight_bbox.Destroy();
  }
  if ( m_count > 0 )
  {
    ON_BoundingBox points_bbox;
    if ( xform && !xform->IsIdentity() )
    {
      ON_3dPoint P;
      points_bbox.m_min = (*xform)*m_a[0];
      points_bbox.m_max = points_bbox.m_min;
      int i;
      for ( i = 1; i < m_count; i++ )
      {
	P = (*xform)*m_a[i];
	if ( P.x < points_bbox.m_min.x ) points_bbox.m_min.x = P.x; else if ( P.x > points_bbox.m_max.x ) points_bbox.m_max.x = P.x;
	if ( P.y < points_bbox.m_min.y ) points_bbox.m_min.y = P.y; else if ( P.y > points_bbox.m_max.y ) points_bbox.m_max.y = P.y;
	if ( P.z < points_bbox.m_min.z ) points_bbox.m_min.z = P.z; else if ( P.z > points_bbox.m_max.z ) points_bbox.m_max.z = P.z;
      }
    }
    else
    {
      points_bbox = BoundingBox();
    }
    tight_bbox.Union(points_bbox);
    bGrowBox = true;
  }
  return (0!=bGrowBox);
}

bool ON_PointCloud::GetTightBoundingBox( 
			ON_BoundingBox& tight_bbox, 
      int bGrowBox,
			const ON_Xform* xform
      ) const
{
  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }

  if ( !bGrowBox )
  {
    tight_bbox.Destroy();
  }

  if ( m_P.Count() > 0 )
  {
    ON_BoundingBox pc_bbox = BoundingBox();
    if ( bGrowBox )
    {
      if ( ON_WorldBBoxIsInTightBBox( tight_bbox, pc_bbox, xform ) )
	return true;
    }

    if ( xform && !xform->IsIdentity() )
    {
      if ( m_P.GetTightBoundingBox(tight_bbox,bGrowBox,xform) )
	bGrowBox = true;
    }
    else
    {
      tight_bbox.Union(pc_bbox);
      bGrowBox = tight_bbox.IsValid();
    }
  }

  return (0!=bGrowBox);
}


bool ON_Mesh::GetTightBoundingBox( 
		ON_BoundingBox& tight_bbox, 
    int bGrowBox,
		const ON_Xform* xform 
    ) const
{
  if ( bGrowBox && !tight_bbox.IsValid() )
  {
    bGrowBox = false;
  }

  ON_BoundingBox mesh_bbox = BoundingBox();
  if ( xform && !xform->IsIdentity() )
  {
    if ( ON_WorldBBoxIsInTightBBox( tight_bbox, mesh_bbox, xform ) )
    {
      return true;
    }

    // calculate bbox of transformed mesh
    ON_3dPoint P;
    P = m_V[0];
    mesh_bbox.m_min = (*xform)*P;
    mesh_bbox.m_max = mesh_bbox.m_min;
    int i;
    const int vcount = m_V.Count();
    for ( i = 1; i < vcount; i++ )
    {
      P = m_V[i];
      P = (*xform)*P;
      if ( P.x < mesh_bbox.m_min.x ) mesh_bbox.m_min.x = P.x; else if (P.x > mesh_bbox.m_max.x) mesh_bbox.m_max.x = P.x;
      if ( P.y < mesh_bbox.m_min.y ) mesh_bbox.m_min.y = P.y; else if (P.y > mesh_bbox.m_max.y) mesh_bbox.m_max.y = P.y;
      if ( P.z < mesh_bbox.m_min.z ) mesh_bbox.m_min.z = P.z; else if (P.z > mesh_bbox.m_max.z) mesh_bbox.m_max.z = P.z;
    }
  }

  if ( !bGrowBox )
  {
    tight_bbox = mesh_bbox;
    bGrowBox = tight_bbox.IsValid();
  }
  else
  {
    tight_bbox.Union(mesh_bbox);
  }

  return (0!=bGrowBox);
}



bool ON_BezierCurve::Transform( 
       const ON_Xform& xform
       )
{
  if ( 0 == m_is_rat )
  {
    if ( xform.m_xform[3][0] != 0.0 || xform.m_xform[3][1] != 0.0 || xform.m_xform[3][2] != 0.0 )
    {
      MakeRational();
    }
  }
  return ON_TransformPointList( m_dim, m_is_rat, m_order, m_cv_stride, m_cv, xform );
}

bool ON_BezierCurve::Rotate(
      double sin_angle,          // sin(angle)
      double cos_angle,          // cos(angle)
      const ON_3dVector& axis, // axis of rotation
      const ON_3dPoint& center // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, axis, center );
  return Transform( rot );
}

bool ON_BezierCurve::Rotate(
      double angle,              // angle in radians
      const ON_3dVector& axis, // axis of rotation
      const ON_3dPoint& center // center of rotation
      )
{
  return Rotate( sin(angle), cos(angle), axis, center );
}

bool ON_BezierCurve::Translate( const ON_3dVector& delta )
{
  ON_Xform tr;
  tr.Translation( delta );
  return Transform( tr );
}

bool ON_BezierCurve::Scale( double x )
{
  ON_Xform s;
  s.Scale( x, x, x );
  return Transform( s );
}


ON_Interval ON_BezierCurve::Domain() const
{
  //return m_domain;
  return ON_Interval(0.0,1.0);
}

bool ON_BezierCurve::Reverse()
{
  bool rc = ON_ReversePointList( m_dim, m_is_rat, m_order, m_cv_stride, m_cv )?true:false;
  //if ( rc )
  //  m_domain.Reverse();
  return rc;
}

bool ON_BezierCurve::Evaluate( // returns false if unable to evaluate
       double t,      // evaluation parameter
       int der_count, // number of derivatives (>=0)
       int v_stride,  // array stride (>=Dimension())
       double* v      // array of length stride*(ndir+1)
       ) const
{
  return ON_EvaluateBezier( m_dim, m_is_rat, m_order, m_cv_stride, m_cv, 
			    0.0, 1.0, //m_domain[0], m_domain[1], 
			    der_count, t, v_stride, v )?true:false;
}

#define QUAD1 0
#define QUAD2 1
#define QUAD3 2
#define QUAD4 3

#define CASE_A 		0
#define CASE_B 		2
#define CASE_C 		3

int case_table[16]  = {		/* A = 0, B = 2, C = 3 */
  0,0,0,0,0,3,0,3,0,2,3,3,0,3,3,3
};

inline
int quadrant(const ON_3dPoint& axis, double* pt, double weight = 1.0) {
  double u = axis.x;
  double v = axis.y;
  if (pt[0] / weight > u)
    return (pt[1] / weight >= v) ? QUAD1 : QUAD4;
  else
    return (pt[1] / weight >= v) ? QUAD2 : QUAD3;
}

inline
int sign(double num) {
  return (num >= 0) ? 1 : -1;
}


int ON_BezierCurve::NumIntersectionsWith(const ON_Line& segment) const
{
  ON_TRACE("ON_BezierCurve::NumIntersectionsWith");
  // XXX - assumes the segment is horizontal and to the "right"
  std::list<ON_BezierCurve> stack;

  int num_isect = 0;
  stack.push_back(*this);
  while (stack.size() > 0) {
    ON_BezierCurve crv = stack.back();

// The assertion below does not allow rhino converted structures to raytrace.  
// Need to figure out why, and what it means for CVCount to be 0. djg 4/15/08
if (crv.CVCount() < 1 ) {
  return 0;
}
    stack.pop_back();

    int qstats = 0;
    ON_Interval dom = crv.Domain();
    assert(crv.CVCount() > 0);
    // determine the case (a la Nishita bezier intersection algorithm)
    for (int i1 = 0; i1 < crv.CVCount(); i1++) {
      qstats |= (1 << quadrant(segment[0], crv.CV(i1), crv.IsRational() ? crv.Weight(i1) : 1.0));
    }
    assert(qstats >= 0);
    int curve_case = case_table[qstats];

    // handle the specific cases
    switch (curve_case) {
    case CASE_A: // there is no possibility of intersection
      ON_TRACE("\tCASE A");
      break;
    case CASE_B: // there is either an even or odd number of intersections
      {
	ON_TRACE("\tCASE B");
	// check the endpoints of the curve to determine if they are in
	// the same or different quadrants
	int quad1 = quadrant(segment[0], crv.PointAt(dom.Min()));
	int quad2 = quadrant(segment[0], crv.PointAt(dom.Max()));
	// if they are in the same quadrant, then the num intersections is even
	// otherwise the number of intersections is odd
	num_isect += (quad1 == quad2) ? 0 : 1;
	break;
      }
    case CASE_C:
      {
	ON_TRACE("\tCASE C");
	// use Bezier clipping to eliminate segments of the curve that
	// cannot intersect the segment
	ON_2dPoint* d = static_cast<ON_2dPoint*>(alloca(crv.m_order * sizeof(ON_2dPoint))); // control points for the explicit Bezier curve
	for (int i = 0; i < crv.m_order; i++) {
	  d[i].x = ((double)i)/(double)(crv.m_order-1);
	  d[i].y = crv.CV(i)[1] - segment[0].y; // dist from the segment
	  ON_TRACE("d[" << i << "]: " << d[i].x << "," << d[i].y);
	}

	// calculate the trimming points
	// XXX - ack - make sure this handles all cases
	double tmin = DBL_MAX;
	double tmax = -DBL_MAX;
	for (int i2 = 0; i2 < (crv.m_order-1); i2++) {
	  for (int j = i2+1; j < crv.m_order; j++) {
	    int a = i2;
	    int b = j;
	    if (sign(d[a].y) != sign(d[b].y)) {
	      double t = d[a].x - d[a].y * (d[b].x - d[a].x) / (d[b].y - d[a].y);
	      if (t < tmin) tmin = t; // XXX why offsets?
	      if (t > tmax) tmax = t;
	    }
	  }
	}

	double tleft = tmin;
	double tright = tmax;
	ON_TRACE("tleft : " << tleft);
	ON_TRACE("tright: " << tright);
	//assert( tleft <= tright );
	if (fabs(tleft-tright) < 0.2) {
	  // XXX the tleft/tright calc doesn't seem to be working, otherwise
	  // why would we need this case?
	  tleft = .33;
	  tright = .66;
	  ON_BezierCurve left(crv), middle(crv), right(crv);
	  //Split(tleft, left, middle);
	  //middle.Split((tright-tleft)/(1.0-tleft), middle, right);
	  left.Trim(ON_Interval(0.0,tleft));
	  middle.Trim(ON_Interval(tleft, tright));
	  right.Trim(ON_Interval(tright,1.0));
	  stack.push_back(left);
	  stack.push_back(middle);
	  stack.push_back(right);
	} else {
	  ON_BezierCurve middle(crv);
	  middle.Trim(ON_Interval(tleft,tright));
	  ON_TRACE("After trim:");
	  ON_TRACE("middle domain: " << middle.Domain().Min() << " --> " << middle.Domain().Max());

	  // since we are limited to horizontal segments for now
	  // we know that left and right are CASE_A...
	  stack.push_back(middle);
	}
	break;
      }
    default:
      assert(false);
    }
  }
  return num_isect;
}

bool ON_BezierCurve::GetNurbForm( ON_NurbsCurve& n ) const
{
  bool rc = false;
  if ( n.Create( m_dim, m_is_rat, m_order, m_order ) ) {
    const int sizeof_cv = CVSize()*sizeof(m_cv[0]);
    int i;
    for ( i = 0; i < m_order; i++ ) {
      memcpy( n.CV(i), CV(i), sizeof_cv );
    }
    n.m_knot[m_order-2] = 0.0;//m_domain.Min();
    n.m_knot[m_order-1] = 1.0;//m_domain.Max();
    rc = ON_ClampKnotVector( n.m_order, n.m_cv_count, n.m_knot, 2 )?true:false;
  }
  return rc;
}

bool ON_BezierCurve::IsRational() const
{
  return m_is_rat ? true : false;
}
  
int ON_BezierCurve::CVSize() const
{
  return (m_dim>0&&m_is_rat) ? m_dim+1 : m_dim;
}

int ON_BezierCurve::Order() const
{
  return m_order;
}

int ON_BezierCurve::Degree() const
{
  return m_order>=2 ? m_order-1 : 0;
}

double* ON_BezierCurve::CV( int i ) const
{
  return m_cv ? m_cv + (i*m_cv_stride) : 0;
}

ON::point_style ON_BezierCurve::CVStyle() const
{
  return m_is_rat ? ON::homogeneous_rational : ON::not_rational;
}

double ON_BezierCurve::Weight( int i ) const 
{
  return ( m_is_rat && m_cv ) ? CV(i)[m_dim] : 1.0;
}

bool ON_BezierCurve::SetWeight( int i, double w )
{
  bool rc = false;
  if ( m_is_rat ) {
    double* cv = CV(i);
    if (cv) {
      cv[m_dim] = w;
      rc = true;
    }
  }
  else if ( w == 1.0 ) {
    rc = true;
  }
  return rc;
}

bool ON_BezierCurve::SetCV( int i, ON::point_style style, const double* Point )
{
  bool rc = true;
  int k;
  double w;

  // feeble but fast check for properly initialized class
  if ( !m_cv || i < 0 || i >= m_order )
    return false;

  double* cv = m_cv + i*m_cv_stride;

  switch ( style ) {

  case ON::not_rational:  // input Point is not rational
    memcpy( cv, Point, m_dim*sizeof(*cv) );
    if ( IsRational() ) {
      // NURBS curve is rational - set weight to one
      cv[m_dim] = 1.0;
    }
    break;

  case ON::homogeneous_rational:  // input Point is homogeneous rational
    if ( IsRational() ) {
      // NURBS curve is rational
      memcpy( cv, Point, (m_dim+1)*sizeof(*cv) );
    }
    else {
      // NURBS curve is not rational
      w = (Point[m_dim] != 0.0) ? 1.0/Point[m_dim] : 1.0;
      for ( k = 0; k < m_dim; k++ ) {
	cv[k] = w*Point[k];
      }
    }
    break;

  case ON::euclidean_rational:  // input Point is euclidean rational
    if ( IsRational() ) {
      // NURBS curve is rational - convert euclean point to homogeneous form
      w = Point[m_dim];
      for ( k = 0; k < m_dim; k++ )
	cv[k] = w*Point[k];
      cv[m_dim] = w;
    }
    else {
      // NURBS curve is not rational
      memcpy( cv, Point, m_dim*sizeof(*cv) );
    }
    break;

  case ON::intrinsic_point_style:
    k = m_is_rat?m_dim+1:m_dim;
    memcpy(cv,Point,k*sizeof(cv[0]));
    break;

  default:
    rc = false;
    break;
  }

  return rc;
}

bool ON_BezierCurve::SetCV( int i, const ON_3dPoint& point )
{
  bool rc = false;
  double* cv = CV(i);
  if ( cv ) {
    cv[0] = point.x;
    if ( m_dim > 1 ) {
      cv[1] = point.y;
      if ( m_dim > 2 )
	cv[2] = point.z;
      if ( m_dim > 3 ) {
	memset( &cv[3], 0, (m_dim-3)*sizeof(*cv) );
      }
    }
    if ( m_is_rat ) {
      cv[m_dim] = 1.0;
    }
    rc = true;
  }
  return rc;
}

bool ON_BezierCurve::SetCV( int i, const ON_4dPoint& point )
{
  bool rc = false;
  double* cv = CV(i);
  if ( cv ) {
    if ( m_is_rat ) {
      cv[0] = point.x;
      if ( m_dim > 1 ) {
	cv[1] = point.y;
	if ( m_dim > 2 )
	  cv[2] = point.z;
	if ( m_dim > 3 ) {
	  memset( &cv[3], 0, (m_dim-3)*sizeof(*cv) );
	}
      }
      cv[m_dim] = point.w;
      rc = true;
    }
    else {
      double w;
      if ( point.w != 0.0 ) {
	w = 1.0/point.w;
	rc = true;
      }
      else {
	w = 1.0;
      }
      cv[0] = w*point.x;
      if ( m_dim > 1 ) {
	cv[1] = w*point.y;
	if ( m_dim > 2 ) {
	  cv[2] = w*point.z;
	}
	if ( m_dim > 3 ) {
	  memset( &cv[3], 0, (m_dim-3)*sizeof(*cv) );
	}
      }
    }
  }
  return rc;
}

bool ON_BezierCurve::GetCV( int i, ON::point_style style, double* Point ) const
{
  const double* cv = CV(i);
  if ( !cv )
    return false;
  int dim = Dimension();
  double w = ( IsRational() ) ? cv[dim] : 1.0;
  switch(style) {
  case ON::euclidean_rational:
    Point[dim] = w;
    // no break here
  case ON::not_rational:
    if ( w == 0.0 )
      return false;
    w = 1.0/w;
    while(dim--) *Point++ = *cv++ * w;
    break;
  case ON::homogeneous_rational:
    Point[dim] = w;
    memcpy( Point, cv, dim*sizeof(*Point) );
    break;
  default:
    return false;
  }
  return true;
}

bool ON_BezierCurve::GetCV( int i, ON_3dPoint& point ) const
{
  bool rc = false;
  const double* cv = CV(i);
  if ( cv ) {
    if ( m_is_rat ) {
      if (cv[m_dim] != 0.0) {
	const double w = 1.0/cv[m_dim];
	point.x = cv[0]*w;
	point.y = (m_dim>1)? cv[1]*w : 0.0;
	point.z = (m_dim>2)? cv[2]*w : 0.0;
	rc = true;
      }
    }
    else {
      point.x = cv[0];
      point.y = (m_dim>1)? cv[1] : 0.0;
      point.z = (m_dim>2)? cv[2] : 0.0;
      rc = true;
    }
  }
  return rc;
}

bool 
ON_BezierCurve::GetCV( int i, ON_4dPoint& point ) const
{
  bool rc = false;
  const double* cv = CV(i);
  if ( cv ) {
    point.x = cv[0];
    point.y = (m_dim>1)? cv[1] : 0.0;
    point.z = (m_dim>2)? cv[2] : 0.0;
    point.w = (m_is_rat) ? cv[m_dim] : 1.0;
    rc = true;
  }
  return rc;
}

bool ON_BezierCurve::ZeroCVs()
{
  bool rc = false;
  int i;
  if ( m_cv ) {
    if ( m_cv_capacity > 0 ) {
      memset( m_cv, 0, m_cv_capacity*sizeof(*m_cv) );
      if ( m_is_rat ) {
	for ( i = 0; i < m_order; i++ ) {
	  SetWeight( i, 1.0 );
	}
      }
      rc = true;
    }
    else {
      double* cv;
      int s = CVSize()*sizeof(*cv);
      for ( i = 0; i < m_order; i++ ) {
	cv = CV(i);
	memset(cv,0,s);
	if ( m_is_rat )
	  cv[m_dim] = 1.0;
      }
      rc = (i>0) ? true : false;
    }
  }
  return rc;
}

int ON_BezierCurve::CVCount() const
{
  return Order();
}

bool ON_BezierCurve::MakeRational()
{
  if ( !IsRational() ) {
    const int dim = Dimension();
    const int cv_count = CVCount();
    if ( cv_count > 0 && m_cv_stride >= dim && dim > 0 ) {
      const int new_stride = (m_cv_stride == dim) ? dim+1 : m_cv_stride;
      ReserveCVCapacity( cv_count*new_stride );
      const double* old_cv;
      double* new_cv;
      int cvi, j;
      for ( cvi = cv_count-1; cvi>=0; cvi-- ) {
	old_cv = CV(cvi);
	new_cv = m_cv+(cvi*new_stride);
	for ( j = dim-1; j >= 0; j-- ) {
	  new_cv[j] = old_cv[j];
	}
	new_cv[dim] = 1.0;
      }
      m_cv_stride = new_stride;
      m_is_rat = 1;
    }
  }
  return IsRational();
}

bool ON_BezierCurve::MakeNonRational()
{
  if ( IsRational() ) {
    const int dim = Dimension();
    const int cv_count = CVCount();
    if ( cv_count > 0 && m_cv_stride >= dim+1 && dim > 0 ) {
      double w;
      const double* old_cv;
      double* new_cv = m_cv;
      int cvi, j;
      for ( cvi = 0; cvi < cv_count; cvi++ ) {
	old_cv = CV(cvi);
	w = old_cv[dim];
	w = ( w != 0.0 ) ? 1.0/w : 1.0;
	for ( j = 0; j < dim; j++ ) {
	  *new_cv++ = w*(*old_cv++);
	}
      }
      m_is_rat = 0;
      m_cv_stride = dim;
    }
  }
  return ( !IsRational() ) ? true : false;
}

bool ON_BezierCurve::IncreaseDegree( int desired_degree )
{
  bool rc = false;
  if ( desired_degree > 0 ) {
    if ( desired_degree == m_order-1 )
      rc = true;
    else if ( desired_degree >= m_order ) {
      ReserveCVCapacity( m_cv_stride*(desired_degree+1) );
      while ( m_order <= desired_degree ) {
	rc = ON_IncreaseBezierDegree( m_dim, m_is_rat, m_order, m_cv_stride, m_cv )?true:false;
	if ( !rc )
	  break;
	m_order++;
      }
    }
  }
  return rc;
}

/////////////////////////////////////////////////////////////////
// Tools for managing CV and knot memory
bool ON_BezierCurve::ReserveCVCapacity( int desired_capacity )
{
	bool rc = false;
	if ( desired_capacity > m_cv_capacity ) {
		if ( !m_cv ) {
			m_cv = (double*)onmalloc(desired_capacity*sizeof(*m_cv));
			if ( !m_cv ) {
				m_cv_capacity = 0;
			}
			else {
				m_cv_capacity = desired_capacity;
				rc = true;
			}
		}
		else if ( m_cv_capacity > 0 ) {
			m_cv = (double*)onrealloc(m_cv,desired_capacity*sizeof(*m_cv));
			if ( !m_cv ) {
				m_cv_capacity = 0;
			}
			else {
				m_cv_capacity = desired_capacity;
				rc = true;
			}
		}
	} 
	else 
		rc =true;
	return rc;
}

bool ON_BezierCurve::ChangeDimension( int dim )
{
  ON_NurbsCurve c;
  c.m_dim = m_dim;
  c.m_is_rat = m_is_rat;
  c.m_order = m_order;
  c.m_cv_count = m_order;
  c.m_cv_capacity = m_cv_capacity;
  c.m_cv_stride = m_cv_stride;
  c.m_cv = m_cv;
  bool rc = c.ChangeDimension(dim);
  m_dim = c.m_dim;
  m_cv_stride = c.m_cv_stride;
  m_cv = c.m_cv;
  m_cv_capacity = c.m_cv_capacity;

  // don't let destruction of stack "c" delete m_cv array.
  c.m_cv = 0;
  c.m_cv_capacity = 0;
  c.m_cv_stride = 0;
  return rc;
}
 
double ON_BezierCurve::ControlPolygonLength() const
{
  double length = 0.0;
  ON_GetPolylineLength( m_dim, m_is_rat, m_order, m_cv_stride, m_cv, &length );
  return length;
}

bool ON_BezierCurve::Trim( const ON_Interval& n )
{
  bool rc = n.IsIncreasing();
  if ( rc ) {
    double t0 = n.Min();
    double t1 = n.Max();
    int cvdim = CVSize(); // 9/9/03 fix rat trim bug - use cvdim instead of m_dim
    if ( t0 != 1.0 ) {
      double s1 = (t1-t0)/(1.0 - t0);
      ON_EvaluatedeCasteljau( cvdim, m_order, +1, m_cv_stride, m_cv, t0 );
      ON_EvaluatedeCasteljau( cvdim, m_order, -1, m_cv_stride, m_cv, s1 );
    }
    else {
      ON_EvaluatedeCasteljau( cvdim, m_order, -1, m_cv_stride, m_cv, t1 );
      if ( t0 != 0.0 )
      {
	// 9/9/03 fix add != 0 test to avoid divide by zero bug
	double s0 = t1/t0;
	ON_EvaluatedeCasteljau( cvdim, m_order, +1, m_cv_stride, m_cv, s0 );
      }
    }
  }
  return rc;
}

bool ON_BezierCurve::Split( 
       double t, // t = splitting parameter must 0 < t < 1
       ON_BezierCurve& left_bez, // left side returned here (can pass *this)
       ON_BezierCurve& right_bez // right side returned here (can pass *this)
       ) const
{
  bool rc = ( 0.0 < t && t < 1.0 && IsValid() ) ? true : false;
  if ( rc ) {
    const int cvdim = CVSize();
    int i,j,k,n;
    double *p, *r, s;
    const double* q;
    double** b = (double**)alloca((2*m_order-1)*sizeof(*b));
    
    if ( this != &left_bez )
    {
      if ( !left_bez.m_cv || (0 < left_bez.m_cv_capacity && left_bez.m_cv_capacity < cvdim*m_order) )
      {
	left_bez.Create( m_dim, m_is_rat, m_order );
      }
      else if ( left_bez.m_dim != m_dim && left_bez.m_is_rat != m_is_rat && left_bez.m_order != m_order || left_bez.m_cv_stride < cvdim )
      {
	left_bez.m_dim       = m_dim;
	left_bez.m_is_rat    = m_is_rat?1:0;
	left_bez.m_order     = m_order;
	left_bez.m_cv_stride = cvdim;
      }
    }

    if ( this != &right_bez )
    {
      if ( !right_bez.m_cv || (0 < right_bez.m_cv_capacity && right_bez.m_cv_capacity < cvdim*m_order) )
      {
	right_bez.Create( m_dim, m_is_rat, m_order );
      }
      else if ( right_bez.m_dim != m_dim && right_bez.m_is_rat != m_is_rat && right_bez.m_order != m_order || right_bez.m_cv_stride < cvdim )
      {
	right_bez.m_dim       = m_dim;
	right_bez.m_is_rat    = m_is_rat?1:0;
	right_bez.m_order     = m_order;
	right_bez.m_cv_stride = cvdim;
      }
    }

    b[0] = left_bez.m_cv;
    b[m_order-1] = right_bez.m_cv;
    for ( i = 1, j = m_order; i < m_order; i++, j++ ) {
      b[j] = b[j-1]+cvdim;
      b[i] = b[i-1]+cvdim;
    }

    if (m_cv == left_bez.m_cv) {
      // copy from back to front
      for (i = 2*m_order-2; i >= 0; i -= 2) {
	p = b[i]+cvdim;
	q = CV(i/2)+cvdim;
	k = cvdim;
	while(k--) *(--p) = *(--q);
      }
    }
    else {
      // copy from front to back
      for (i = 0; i < 2*m_order; i += 2) {
	p = b[i];
	q = CV(i/2);
	k = cvdim;
	while(k--) *p++ = *q++;
      }      
    }
    
    left_bez.m_dim = m_dim;
    left_bez.m_is_rat = m_is_rat;
    left_bez.m_order = m_order;
    left_bez.m_cv_stride = CVSize();

    right_bez.m_dim = left_bez.m_dim;
    right_bez.m_is_rat = left_bez.m_is_rat;
    right_bez.m_order = left_bez.m_order;
    right_bez.m_cv_stride = left_bez.m_cv_stride;
    
    // deCasteljau
    if (t == 0.5) {
      // use faster aritmetic for this common case
      for (i = 1, k = 2*m_order-2; i < k; i++, k--) {
	for (j = i; j < k; j++,j++)  {
	  p = b[j-1];
	  q = b[j+1];
	  r = b[j];
	  n = cvdim;
	  while(n--) 
	    *r++ = 0.5*(*p++ + *q++);
	}
      }
    }
    else {
      s = 1.0-t;
      for (i = 1, k = 2*m_order-2; i < k; i++, k--) {
	for (j = i; j < k; j++,j++) {
	  p = b[j-1];
	  q = b[j+1];
	  r = b[j];
	  n = cvdim;
	  while(n--) 
	    *r++ = (*p++ * s + *q++ * t);
	}
      }
    }
    
    // last cv of left side = first cv of right side
    p = right_bez.CV(0);
    q = left_bez.CV(m_order-1);
    if (p != q) {
      j = cvdim;
      while(j--) *p++ = *q++;
    }
  }
  return rc;
}

ON_BezierSurface::ON_BezierSurface()
		 : m_dim(0),
		   m_is_rat(0),
		   m_cv(0),
		   m_cv_capacity(0)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_cv_stride[0] = 0;
  m_cv_stride[1] = 0;
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierSurface = 0;
#endif
}

ON_BezierSurface::ON_BezierSurface( int dim, BOOL is_rat, int order0, int order1 )
		 : m_dim(0),
		   m_is_rat(0),
		   m_cv(0),
		   m_cv_capacity(0)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_cv_stride[0] = 0;
  m_cv_stride[1] = 0;
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierSurface = 0;
#endif
  Create( dim, is_rat, order0, order1 );
}

ON_BezierSurface::~ON_BezierSurface()
{
  Destroy();
}
  
ON_BezierSurface::ON_BezierSurface(const ON_BezierSurface& src)
		 : m_dim(0),
		   m_is_rat(0),
		   m_cv(0),
		   m_cv_capacity(0)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_cv_stride[0] = 0;
  m_cv_stride[1] = 0;
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierSurface = 0;
#endif
  *this = src;
}

ON_BezierSurface::ON_BezierSurface(const ON_PolynomialSurface& src)
		 : m_dim(0),
		   m_is_rat(0),
		   m_cv(0),
		   m_cv_capacity(0)
{
  m_order[0] = 0;
  m_order[1] = 0;
  m_cv_stride[0] = 0;
  m_cv_stride[1] = 0;
#if 8 == ON_SIZEOF_POINTER
  m_reserved_ON_BezierSurface = 0;
#endif
  *this = src;
}

ON_BezierSurface& ON_BezierSurface::operator=(const ON_BezierSurface& src)
{
  if ( this != &src ) {
    if ( Create( src.m_dim, src.m_is_rat, src.m_order[0], src.m_order[1] ) ) {
      const int sizeof_cv = src.CVSize()*sizeof(m_cv[0]);
      int i, j;
      for ( i = 0; i < m_order[0]; i++ ) for ( j = 0; j < m_order[1]; j++ ) {
	memcpy( CV(i,j), src.CV(i,j), sizeof_cv );
      }
    }
    else {
      Destroy();
    }
  }
  return *this;
}

ON_BezierSurface& ON_BezierSurface::operator=(const ON_PolynomialSurface& src)
{
  if ( Create( src.m_dim, src.m_is_rat, src.m_order[0], src.m_order[1] ) ) {
    // TODO - convert to bi-bezier
  }
  return *this;
}

bool ON_BezierSurface::IsValid() const
{
  if ( m_dim <= 0 )
    return false;
  if ( m_is_rat != 0 && m_is_rat != 1 )
    return false;
  if ( m_order[0] < 2 )
    return false;
  if ( m_order[0] < 2 )
    return false;
  if ( m_cv_stride[0] < m_dim+m_is_rat )
    return false;
  if ( m_cv_stride[1] < m_dim+m_is_rat )
    return false;
  if ( m_cv_capacity > 0 && m_cv_capacity < (m_dim+m_is_rat)*m_order[0]*m_order[1] )
    return false;
  //if ( !m_domain[0].IsIncreasing() )
  //  return false;
  //if ( !m_domain[1].IsIncreasing() )
  //  return false;
  if ( m_cv == NULL )
    return false;
  return true;
}

void ON_BezierSurface::Dump( ON_TextLog& dump ) const
{
  dump.Print( "ON_BezierSurface dim = %d is_rat = %d\n"
	       "        order = (%d, %d) \n",
	       m_dim, m_is_rat, m_order[0], m_order[1] );
  dump.Print( "Control Points  %d %s points\n"
	       "  index               value\n",
	       m_order[0]*m_order[1], 
	       (m_is_rat) ? "rational" : "non-rational" );
  if ( !m_cv ) {
    dump.Print("  NULL cv array\n");
  }
  else {
    int i;
    char sPreamble[128]; 
    memset(sPreamble,0,sizeof(sPreamble));
    for ( i = 0; i < m_order[0]; i++ )
    {
      if ( i > 0 )
	dump.Print("\n");
      sPreamble[0] = 0;
      sprintf(sPreamble,"  CV[%2d]",i);
      dump.PrintPointList( m_dim, m_is_rat, 
			m_order[1], m_cv_stride[1],
			CV(i,0), 
			sPreamble );
    }
  }
}

int ON_BezierSurface::Dimension() const
{
  return m_dim;
}

bool ON_BezierSurface::Create( int dim, BOOL is_rat, int order0, int order1 )
{
  if ( m_cv_capacity < 1 )
    m_cv = 0;
  m_dim = (dim>0) ? dim : 0;
  m_is_rat = is_rat ? 1 : 0;
  m_order[0] = (order0 >= 2) ? order0 : 0;
  m_order[1] = (order1 >= 2) ? order1 : 0;
  m_cv_stride[1] = (m_dim > 0) ? m_dim+m_is_rat : 0;
  m_cv_stride[0] = m_cv_stride[1]*m_order[1];
  m_cv_capacity = m_cv_stride[0]*m_order[0];
  m_cv = (double*)onrealloc( m_cv, m_cv_capacity*sizeof(m_cv[0]) );
  //m_domain[0].m_t[0] = 0.0;
  //m_domain[0].m_t[1] = 1.0;
  //m_domain[1].m_t[0] = 0.0;
  //m_domain[1].m_t[1] = 1.0;
  return IsValid();
}

void ON_BezierSurface::Destroy()
{
  if ( m_cv && m_cv_capacity > 0 )
    onfree(m_cv);
  m_cv_capacity = 0;
  m_cv_stride[0] = 0;
  m_cv_stride[1] = 0;
  m_cv = 0;
  m_dim = 0;
  m_is_rat = 0;
  m_order[0] = 0;
  m_order[1] = 0;
  //m_domain[0].m_t[0] = 0.0;
  //m_domain[0].m_t[1] = 1.0;
  //m_domain[1].m_t[0] = 0.0;
  //m_domain[1].m_t[1] = 1.0;
}

void ON_BezierSurface::EmergencyDestroy()
{
  m_cv = 0;
}

bool ON_BezierSurface::Loft( const ON_ClassArray<ON_BezierCurve>& curve_list )
{
  int i;
  int count = curve_list.Count();
  ON_SimpleArray<const ON_BezierCurve*> ptr_list(count);
  for (i = 0; i < count; i++ )
  {
    ptr_list.Append(&curve_list[i]);
  }
  return Loft(ptr_list.Count(),ptr_list.Array());
}

bool ON_BezierSurface::Loft( 
	int curve_count,
	const ON_BezierCurve* const* curve_list
	)
{
  // 06-06-06 Dale Fugier - tested
  bool rc = false;
  if (curve_count >= 2 && 0 != curve_list && 0 != curve_list[0] )
  {
    // determine order, dimension, is_rat, and cv_stride of compatible shape curves
    int shape_order = curve_list[0]->m_order;
    int shape_dim = curve_list[0]->m_dim;
    int shape_is_rat = (0 != curve_list[0]->m_is_rat) ? 1 : 0;
    if ( shape_dim < 1 || shape_order < 2 )
      return false;
    int i, j, k;
    for ( i = 0; i < curve_count; i++ )
    {
      if ( curve_list[i]->m_order < 2 || curve_list[i]->m_dim < 1 || 0 == curve_list[i]->m_cv)
	return false;
      if ( curve_list[i]->m_dim != shape_dim )
	return false;
      if ( curve_list[i]->m_order > shape_order )
	shape_order = curve_list[i]->m_order;
      if ( 0 != curve_list[i]->m_is_rat )
	shape_is_rat = 1;
    }

    // build a list of compatible shape curves
    const int shape_cv_stride = (shape_is_rat) ? (shape_dim + 1) : shape_dim;
    ON_SimpleArray<double> meta_point(curve_count*shape_cv_stride*shape_order);
    ON_BezierCurve* temp_shape = 0;
    for ( i = 0; i < curve_count; i++ )
    {
      const ON_BezierCurve* shape = curve_list[i];
      if (    shape->m_order != shape_order 
	   || shape->m_is_rat != shape_is_rat
	   || shape->m_cv_stride != shape_cv_stride )
      {
	if ( 0 == temp_shape )
	  temp_shape = new ON_BezierCurve();
	temp_shape->operator=(*shape);
	if ( shape_is_rat )
	  temp_shape->MakeRational();
	temp_shape->IncreaseDegree(shape_order-1);
	if ( temp_shape->m_dim != shape_dim 
	      || temp_shape->m_is_rat != shape_is_rat 
	      || temp_shape->m_order != shape_order 
	      || temp_shape->m_cv_stride != shape_cv_stride )
	{
	  break;
	}
	shape = temp_shape;
      }
      for ( int j = 0; j < shape->m_order; j++ )
      {
	const double* cv = shape->CV(j);
	for ( int k = 0; k < shape_cv_stride; k++ )
	  meta_point.Append(cv[k]);
      }
    }
    if ( 0 != temp_shape )
    {
      delete temp_shape;
      temp_shape = 0;
    }
    if ( meta_point.Count() == curve_count*shape_cv_stride*shape_order )
    {
      ON_BezierCurve bez;
      ON_SimpleArray<double>t(curve_count);
      double dt = 1.0/((double)curve_count);
      for ( i = 0; i < curve_count; i++ )
      {
	t.Append( i*dt );
      }
      t[curve_count-1] = 1.0;
      // use high dimensional curve loft trick
      rc = bez.Loft( shape_cv_stride*shape_dim, curve_count, shape_cv_stride*shape_dim, meta_point.Array(), 1, t.Array() )
	 ? true : false;
      if (rc)
      {
	Create(shape_dim,shape_is_rat,curve_count,shape_order);
	// fill in surface CVs
	for ( i = 0; i < curve_count; i++ )
	{
	  const double* bez_cv = bez.CV(i);
	  for ( j = 0; j < shape_order; j++ )
	  {
	    double* srf_cv = CV(i,j);
	    for ( k = 0; k < shape_cv_stride; k++ )
	      srf_cv[k] = *bez_cv++;
	  }
	}
      }
    }
  }
  return rc;
}

bool ON_BezierSurface::GetBBox( // returns true if successful
       double* boxmin,    // minimum
       double* boxmax,    // maximum
       int bGrowBox  // true means grow box
       ) const
{
  int i;
  bool rc = (m_order[0] > 0 && m_order[1] > 0) ? true : false;
  for ( i = 0; rc && i < m_order[0]; i++ ) {
    rc = ON_GetPointListBoundingBox( m_dim, m_is_rat, m_order[1], m_cv_stride[1],
				    CV(i,0), boxmin, boxmax, bGrowBox );
    bGrowBox = true;
  }
  return rc;
}

bool ON_BezierSurface::GetBoundingBox( // returns true if successful
       ON_BoundingBox& bbox,
       int bGrowBox             // true means grow box
       ) const
{
  double *boxmin, *boxmax;
  if ( m_dim > 3 ) 
  {
    boxmin = (double*)alloca(2*m_dim*sizeof(*boxmin));
    memset( boxmin, 0, 2*m_dim*sizeof(*boxmin) );
    boxmax = boxmin + m_dim;
    if ( bGrowBox ) {
      boxmin[0] = bbox.m_min.x;
      boxmin[1] = bbox.m_min.y;
      boxmin[2] = bbox.m_min.z;
      boxmax[0] = bbox.m_max.x;
      boxmax[1] = bbox.m_max.y;
      boxmax[2] = bbox.m_max.z;
    }
  }
  else {
    boxmin = &bbox.m_min.x;
    boxmax = &bbox.m_max.x;
  }
  bool rc = GetBBox( boxmin, boxmax, bGrowBox );
  if ( rc && m_dim > 3 ) {
    bbox.m_min = boxmin;
    bbox.m_max = boxmax;
  }
  return rc;
}

ON_BoundingBox ON_BezierSurface::BoundingBox() const
{
  ON_BoundingBox bbox;
  if ( !GetBoundingBox(bbox,false) )
    bbox.Destroy();
  return bbox;
}

bool ON_BezierSurface::Transform( const ON_Xform& xform )
{
  int i;
  bool rc = (m_order[0] > 0 && m_order[1] > 0) ? true : false;
  if (rc)
  {  
    if ( 0 == m_is_rat )
    {
      if ( xform.m_xform[3][0] != 0.0 || xform.m_xform[3][1] != 0.0 || xform.m_xform[3][2] != 0.0 )
      {
	MakeRational();
      }
    }
  
    for ( i = 0; rc && i < m_order[0]; i++ ) 
    {
      rc = ON_TransformPointList( m_dim, m_is_rat, 
				  m_order[1], m_cv_stride[1], 
				  CV(i,0), xform );
    }
  }
  return rc;
}

bool ON_BezierSurface::Rotate(
      double sin_angle,          // sin(angle)
      double cos_angle,          // cos(angle)
      const ON_3dVector& axis, // axis of rotation
      const ON_3dPoint& center // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, axis, center );
  return Transform( rot );
}

bool ON_BezierSurface::Rotate(
      double angle,              // angle in radians
      const ON_3dVector& axis, // axis of rotation
      const ON_3dPoint& center // center of rotation
      )
{
  return Rotate( sin(angle), cos(angle), axis, center );
}

bool ON_BezierSurface::Translate( const ON_3dVector& delta )
{
  ON_Xform tr;
  tr.Translation( delta );
  return Transform( tr );
}

bool ON_BezierSurface::Scale( double x )
{
  ON_Xform s;
  s.Scale( x, x, x );
  return Transform( s );
}

ON_Interval ON_BezierSurface::Domain( 
      int // dir - formal parameter intentionally ignored in this virtual function
      ) const
{
  // 0 = "u" domain, 1 = "v" domain
  return ON_Interval(0.0,1.0); //m_domain[(dir>0)?1:0];
}

bool ON_BezierSurface::Reverse( int  dir )
{
  int i;
  bool rc = (m_order[0] > 0 && m_order[1] > 0) ? true : false;
  if ( dir > 0 ) {
    for ( i = 0; rc && i < m_order[0]; i++ ) {
      rc = ON_ReversePointList( m_dim, m_is_rat, m_order[1], m_cv_stride[1], CV(i,0) );
    }
    //m_domain[1].Reverse();
  }
  else {
    for ( i = 0; rc && i < m_order[1]; i++ ) {
      rc = ON_ReversePointList( m_dim, m_is_rat, m_order[0], m_cv_stride[0], CV(0,i) );
    }
    //m_domain[0].Reverse();
  }
  return rc;
}

bool ON_BezierSurface::Transpose()
{
  // transpose surface parameterization (swap "s" and "t")
  int i = m_order[0]; m_order[0] = m_order[1]; m_order[1] = i;
  i = m_cv_stride[0]; m_cv_stride[0] = m_cv_stride[1]; m_cv_stride[1] = i;
  //ON_Interval d = m_domain[0]; m_domain[0] = m_domain[1]; m_domain[1] = d;
  return true;
}

bool ON_BezierSurface::Evaluate( // returns false if unable to evaluate
       double s, double t,       // evaluation parameter
       int der_count,            // number of derivatives (>=0)
       int v_stride,             // array stride (>=Dimension())
       double* v                 // array of length stride*(ndir+1)*(ndir+2)/2
       ) const
{
  // TODO: When time permits write a faster special case bezier surface evaluator.
  // For now, cook up some fake knot vectors and use NURBS surface span evaluator.
  double *knot0, *knot1, *p;
  int deg0 = m_order[0]-1;
  int deg1 = m_order[1]-1;
  int i = (deg0<=deg1)?deg1 : deg0;
  p = knot0 = (double*)alloca(i*2*sizeof(*knot0));
  memset(p,0,i*sizeof(*p));
  p += i;
  while (i--) *p++ = 1.0;
  if ( deg0 >= deg1 ) {
    knot1 = knot0 + (deg0-deg1);
  }
  else {
    knot1 = knot0;
    knot0 = knot1 + (deg1-deg0);
  }

  return ON_EvaluateNurbsSurfaceSpan(
	m_dim,                          // dimension
	m_is_rat,                       // true if NURBS is rational
	m_order[0], m_order[1],         // order0, order1
	knot0,                          // knot0[] array of (2*order0-2) doubles
	knot1,                          // knot1[] array of (2*order1-2) doubles
	m_cv_stride[0], m_cv_stride[1], // cv_stride0, cv_stride1
	m_cv,                          // cv at "lower left" of bispan
	der_count,                      // number of derivatives to compute (>=0)
	s,t,                            // evaluation parameters ("s" and "t")
	v_stride,                       // answer_stride (>=dimension)
	v                               // answer[] array of length (ndir+1)*answer_stride
	);
}

ON_3dPoint ON_BezierSurface::PointAt(double s, double t) const
{
  ON_3dPoint P;
  Evaluate(s,t,0,3,&P.x);
  return P;
}

ON_BezierCurve* ON_BezierSurface::IsoCurve(int dir, double t, ON_BezierCurve* pCrv) const
{
	if( pCrv == NULL )
  {
		pCrv = new ON_BezierCurve(m_dim, m_is_rat, m_order[dir]);
	}
	else if ( pCrv->m_dim!=m_dim || pCrv->m_is_rat!= m_is_rat || pCrv->m_order!= m_order[dir])
  {
		pCrv->Create(m_dim, m_is_rat, m_order[dir]);
  }
	

	int bigdim = CVSize() * m_order[dir];
	int stride;
	double* cv = 0;
	double* workspace = 0;
	if( m_cv_stride[1-dir]>m_cv_stride[dir])
  {
		stride = m_cv_stride[1-dir];
		cv = m_cv;
	}
	else
  {
		// purify FMM trauma - // workspace = new double[bigdim*m_order[1-dir]];
		workspace = (double*)onmalloc((bigdim*m_order[1-dir])*sizeof(*workspace));

		cv = workspace;
		stride = bigdim;

		int i,j;
		int cvsize = CVSize();
		int cvsize_bytes = cvsize*sizeof(double);
		double* dst = workspace;
		for(i=0; i<m_order[1-dir] ; i++)
    {
			double* src = dir ? CV(i,0): CV(0,i);
			for(j=0; j<m_order[dir]; j++,dst+=cvsize, src+=m_cv_stride[dir] )
				memcpy(dst, src, cvsize_bytes ); 
		}
	}

	ON_EvaluateBezier( bigdim, 0, m_order[1-dir], stride,  cv, 0.0, 1.0, 0, t, bigdim, pCrv->m_cv );

	if(workspace)
  {
		// purify FMM trauma - // delete[] workspace;
    onfree(workspace);
  }

	return pCrv;
}

bool ON_BezierSurface::GetNurbForm( ON_NurbsSurface& n ) const
{
  bool rc = false;
  if ( n.Create( m_dim, m_is_rat, m_order[0], m_order[1], m_order[0], m_order[1] ) ) 
  {
    if ( n.m_cv == m_cv )
    {
      n.m_cv_stride[0] = m_cv_stride[0];
      n.m_cv_stride[1] = m_cv_stride[1];
    }
    else
    {
      const int sizeof_cv = CVSize()*sizeof(m_cv[0]);
      int i,j;
      for ( i = 0; i < m_order[0]; i++ ) for ( j = 0; j < m_order[1]; j++ ) {
	memcpy( n.CV(i,j), CV(i,j), sizeof_cv );
      }
    }
    n.m_knot[0][m_order[0]-2] = 0.0;  //m_domain[0].Min();
    n.m_knot[0][m_order[0]-1] = 1.0;	//m_domain[0].Max();
    n.m_knot[1][m_order[1]-2] = 0.0;	//m_domain[1].Min();
    n.m_knot[1][m_order[1]-1] = 1.0;	//m_domain[1].Max();
    rc = ON_ClampKnotVector( n.m_order[0], n.m_cv_count[0], n.m_knot[0], 2 );
    rc = ON_ClampKnotVector( n.m_order[1], n.m_cv_count[1], n.m_knot[1], 2 );
  }
  return rc;
}

bool ON_BezierSurface::IsRational() const
{
  return m_is_rat ? true : false;
}

int ON_BezierSurface::CVSize() const
{
  return ( m_is_rat && m_dim>0 ) ? m_dim+1 : m_dim;
}

int ON_BezierSurface::Order( int dir ) const
{
  return (dir>=0&&dir<=1) ? m_order[dir] : 0;
}

int ON_BezierSurface::Degree(int dir) const
{
  int order = Order(dir);
  return (order>=2) ? order-1 : 0;
}

double* ON_BezierSurface::CV( int i, int j ) const
{
  return (m_cv) ? (m_cv + i*m_cv_stride[0] + j*m_cv_stride[1]) : NULL;
}

ON::point_style ON_BezierSurface::CVStyle() const
{
  return m_is_rat ? ON::homogeneous_rational : ON::not_rational;
}

double ON_BezierSurface::Weight( int i, int j ) const
{
  return (m_cv && m_is_rat) ? m_cv[i*m_cv_stride[0] + j*m_cv_stride[1] + m_dim] : 1.0;
}


bool ON_BezierSurface::SetWeight( int i, int j, double w )
{
  bool rc = false;
  if ( m_is_rat ) {
    double* cv = CV(i,j);
    if (cv) {
      cv[m_dim] = w;
      rc = true;
    }
  }
  else if ( w == 1.0 ) {
    rc = true;
  }
  return rc;
}

bool ON_BezierSurface::SetCV( int i, int j, ON::point_style style, const double* Point )
{
  bool rc = true;
  int k;
  double w;

  double* cv = CV(i,j);
  if ( !cv )
    return false;

  switch ( style ) {

  case ON::not_rational:  // input Point is not rational
    memcpy( cv, Point, m_dim*sizeof(*cv) );
    if ( IsRational() ) {
      // NURBS surface is rational - set weight to one
      cv[m_dim] = 1.0;
    }
    break;

  case ON::homogeneous_rational:  // input Point is homogeneous rational
    if ( IsRational() ) {
      // NURBS surface is rational
      memcpy( cv, Point, (m_dim+1)*sizeof(*cv) );
    }
    else {
      // NURBS surface is not rational
      w = (Point[m_dim] != 0.0) ? 1.0/Point[m_dim] : 1.0;
      for ( k = 0; k < m_dim; k++ ) {
	cv[k] = w*Point[k];
      }
    }
    break;

  case ON::euclidean_rational:  // input Point is euclidean rational
    if ( IsRational() ) {
      // NURBS surface is rational - convert euclean point to homogeneous form
      w = Point[m_dim];
      for ( k = 0; k < m_dim; k++ )
	cv[i] = w*Point[i];
      cv[m_dim] = w;
    }
    else {
      // NURBS surface is not rational
      memcpy( cv, Point, m_dim*sizeof(*cv) );
    }
    break;

  case ON::intrinsic_point_style:
    k = m_is_rat?m_dim+1:m_dim;
    memcpy(cv,Point,k*sizeof(*cv));
    break;
    
  default:
    rc = false;
    break;
  }
  return rc;
}

bool ON_BezierSurface::SetCV( int i, int j, const ON_3dPoint& point )
{
  bool rc = false;
  double* cv = CV(i,j);
  if ( cv ) {
    cv[0] = point.x;
    if ( m_dim > 1 ) {
      cv[1] = point.y;
      if ( m_dim > 2 )
	cv[2] = point.z;
    }
    if ( m_is_rat ) {
      cv[m_dim] = 1.0;
    }
    rc = true;
  }
  return rc;
}

bool ON_BezierSurface::SetCV( int i, int j, const ON_4dPoint& point )
{
  bool rc = false;
  double* cv = CV(i,j);
  if ( cv ) {
    if ( m_is_rat ) {
      cv[0] = point.x;
      if ( m_dim > 1 ) {
	cv[1] = point.y;
	if ( m_dim > 2 )
	  cv[2] = point.z;
      }
      cv[m_dim] = point.w;
      rc = true;
    }
    else {
      double w;
      if ( point.w != 0.0 ) {
	w = 1.0/point.w;
	rc = true;
      }
      else {
	w = 1.0;
      }
      cv[0] = w*point.x;
      if ( m_dim > 1 ) {
	cv[1] = w*point.y;
	if ( m_dim > 2 ) {
	  cv[2] = w*point.z;
	}
      }
    }
  }
  return rc;
}

bool ON_BezierSurface::GetCV( int i, int j, ON::point_style style, double* Point ) const
{
  const double* cv = CV(i,j);
  if ( !cv )
    return false;
  int dim = Dimension();
  double w = ( IsRational() ) ? cv[dim] : 1.0;
  switch(style) {
  case ON::euclidean_rational:
    Point[dim] = w;
    // no break here
  case ON::not_rational:
    if ( w == 0.0 )
      return false;
    w = 1.0/w;
    while(dim--) *Point++ = *cv++ * w;
    break;
  case ON::homogeneous_rational:
    Point[dim] = w;
    memcpy( Point, cv, dim*sizeof(*Point) );
    break;
  default:
    return false;
  }
  return true;
}

bool ON_BezierSurface::GetCV( int i, int j, ON_3dPoint& point ) const
{
  bool rc = false;
  const double* cv = CV(i,j);
  if ( cv ) {
    if ( m_is_rat ) {
      if (cv[m_dim] != 0.0) {
	const double w = 1.0/cv[m_dim];
	point.x = cv[0]*w;
	point.y = (m_dim>1)? cv[1]*w : 0.0;
	point.z = (m_dim>2)? cv[2]*w : 0.0;
	rc = true;
      }
    }
    else {
      point.x = cv[0];
      point.y = (m_dim>1)? cv[1] : 0.0;
      point.z = (m_dim>2)? cv[2] : 0.0;
      rc = true;
    }
  }
  return rc;
}

bool ON_BezierSurface::GetCV( int i, int j, ON_4dPoint& point ) const
{
  bool rc = false;
  const double* cv = CV(i,j);
  if ( cv ) {
    point.x = cv[0];
    point.y = (m_dim>1)? cv[1] : 0.0;
    point.z = (m_dim>2)? cv[2] : 0.0;
    point.w = (m_is_rat) ? cv[m_dim] : 1.0;
    rc = true;
  }
  return rc;
}

bool ON_BezierSurface::ZeroCVs()
{
  // zeros control vertices and, if rational, sets weights to 1
  bool rc = false;
  int i,j;
  if ( m_cv ) {
    if ( m_cv_capacity > 0 ) {
      memset( m_cv, 0, m_cv_capacity*sizeof(*m_cv) );
      if ( m_is_rat ) {
	for ( i = 0; i < m_order[0]; i++ ) {
	  for ( j = 0; j < m_order[1]; j++ ) {
	    SetWeight( i,j, 1.0 );
	  }
	}
      }
      rc = true;
    }
    else {
      double* cv;
      int s = CVSize()*sizeof(*cv);
      for ( i = 0; i < m_order[0]; i++ ) {
	for ( j = 0; j < m_order[1]; j++ ) {
	  cv = CV(i,j);
	  memset(cv,0,s);
	  if ( m_is_rat )
	    cv[m_dim] = 1.0;
	}
      }
      rc = (i>0) ? true : false;
    }
  }
  return rc;
}

bool ON_BezierSurface::MakeRational()
{
  if ( !IsRational() ) {
    const int dim = Dimension();
    if ( m_order[0] > 0 && m_order[1] > 0 && dim > 0 ) {
      const double* old_cv;
      double* new_cv;
      int cvi, cvj, j, cvstride;
      if ( m_cv_stride[0] < m_cv_stride[1] ) {
	cvstride = m_cv_stride[0] > dim ? m_cv_stride[0] : dim+1;
	ReserveCVCapacity( cvstride*m_order[0]*m_order[1] );
	new_cv = m_cv + cvstride*m_order[0]*m_order[1]-1;
				for ( cvj = m_order[1]-1; cvj >= 0; cvj-- ) {
	  for ( cvi = m_order[0]-1; cvi >= 0; cvi-- ) {
	    old_cv = CV(cvi,cvj)+dim-1;
	    *new_cv-- = 1.0;
	    for ( j = 0; j < dim; j++ ) {
	      *new_cv-- = *old_cv--;
	    }
	  }
	}
	m_cv_stride[0] = dim+1;
	m_cv_stride[1] = (dim+1)*m_order[0];
      }
      else {
	cvstride = m_cv_stride[1] > dim ? m_cv_stride[1] : dim+1;
	ReserveCVCapacity( cvstride*m_order[0]*m_order[1] );
	new_cv = m_cv + cvstride*m_order[0]*m_order[1]-1;
	for ( cvi = m_order[0]-1; cvi >= 0; cvi-- ) {
	  for ( cvj = m_order[1]-1; cvj >= 0; cvj-- ) {
	    old_cv = CV(cvi,cvj)+dim-1;
	    *new_cv-- = 1.0;
	    for ( j = 0; j < dim; j++ ) {
	      *new_cv-- = *old_cv--;
	    }
	  }
	}
	m_cv_stride[1] = dim+1;
	m_cv_stride[0] = (dim+1)*m_order[1];
      }
      m_is_rat = 1;
    }
  }
  return IsRational();
}

bool ON_BezierSurface::MakeNonRational()
{
  if ( IsRational() ) {
    const int dim = Dimension();
    if ( m_order[0] > 0 && m_order[1] > 0 && dim > 0 ) {
      double w;
      const double* old_cv;
      double* new_cv = m_cv;
      int cvi, cvj, j;
      if ( m_cv_stride[0] < m_cv_stride[1] ) {
	for ( cvj = 0; cvj < m_order[1]; cvj++ ) {
	  for ( cvi = 0; cvi < m_order[0]; cvi++ ) {
	    old_cv = CV(cvi,cvj);
	    w = old_cv[dim];
	    w = ( w != 0.0 ) ? 1.0/w : 1.0;
	    for ( j = 0; j < dim; j++ ) {
	      *new_cv++ = w*(*old_cv++);
	    }
	  }
	}
	m_cv_stride[0] = dim;
	m_cv_stride[1] = dim*m_order[0];
      }
      else {
	for ( cvi = 0; cvi < m_order[0]; cvi++ ) {
	  for ( cvj = 0; cvj < m_order[1]; cvj++ ) {
	    old_cv = CV(cvi,cvj);
	    w = old_cv[dim];
	    w = ( w != 0.0 ) ? 1.0/w : 1.0;
	    for ( j = 0; j < dim; j++ ) {
	      *new_cv++ = w*(*old_cv++);
	    }
	  }
	}
	m_cv_stride[1] = dim;
	m_cv_stride[0] = dim*m_order[1];
      }
      m_is_rat = 0;
    }
  }
  return ( !IsRational() ) ? true : false;
}

/////////////////////////////////////////////////////////////////
// Tools for managing CV and knot memory
bool ON_BezierSurface::ReserveCVCapacity(
  int capacity// number of doubles to reserve
  )
{
  if ( m_cv_capacity < capacity ) {
    if ( m_cv ) {
      if ( m_cv_capacity ) {
	m_cv = (double*)onrealloc( m_cv, capacity*sizeof(*m_cv) );
	m_cv_capacity = (m_cv) ? capacity : 0;
      }
      // else user supplied m_cv[] array
    }
    else {
      m_cv = (double*)onmalloc( capacity*sizeof(*m_cv) );
      m_cv_capacity = (m_cv) ? capacity : 0;
    }
  }
  return ( m_cv ) ? true : false;
}

bool ON_BezierSurface::Trim(
      int dir,
      const ON_Interval& domain
      )
{
  bool rc = false;
  ON_BezierCurve crv;
  int i, j;
  const double* cv;
  const int k = m_is_rat ? (m_dim+1) : m_dim;
  const int sizeofcv = k*sizeof(*cv);
  if ( 0 == dir )
  {
    crv.Create(k*m_order[1],false,m_order[0]);
    rc = crv.Trim(domain);
    if (rc)
    {
      if ( k == m_cv_stride[1] && m_cv_stride[0] == k*m_order[1] && crv.m_cv_stride == m_cv_stride[0] )
      {
	memcpy( m_cv, crv.m_cv, m_order[0]*m_order[1]*k*sizeof(*m_cv) );
      }
      else
      {
	for (i = 0; i < m_order[0]; i++ )
	{
	  cv = crv.CV(i);
	  for ( j = 0; j < m_order[1]; j++ )
	  {
	    memcpy( CV(i,j), cv, sizeofcv );
	    cv += k;
	  }
	}
      }
    }
  }
  else if ( 1 == dir )
  {
    crv.Create(k*m_order[0],false,m_order[1]);
    rc = crv.Trim(domain);
    if (rc)
    {
      if ( k == m_cv_stride[0] && m_cv_stride[1] == k*m_order[0] && crv.m_cv_stride == m_cv_stride[1] )
      {
	memcpy( m_cv, crv.m_cv, m_order[0]*m_order[1]*k*sizeof(*m_cv) );
      }
      else
      {
	for ( j = 0; j < m_order[1]; j++ )
	{
	  cv = crv.CV(j);
	  for (i = 0; i < m_order[0]; i++ )
	  {
	    memcpy( CV(i,j), cv, sizeofcv );
	    cv += k;
	  }
	}
      }
    }
  }
  return rc;
}


bool ON_BezierSurface::Split( 
       int dir, // 0 split at "u"=t, 1= split at "v"=t
       double t, // t = splitting parameter must 0 < t < 1
       ON_BezierSurface& left_bez, // west/south side returned here (can pass *this)
       ON_BezierSurface& right_bez // east/north side returned here (can pass *this)
       ) const
{
  bool rc = false;
  if ( 0.0 < t && t < 1.0 ) {
    double *crvcv;
    int i, j;
    const int hdim = (m_is_rat) ? m_dim+1 : m_dim;
    const int crvdim =  hdim*m_order[dir?0:1];
    ON_BezierCurve leftcrv, rightcrv;
    ON_BezierCurve crv(crvdim,0,m_order[dir?1:0]);
    if (dir) {
      for ( j = 0; j < m_order[1]; j++ ) {
	crvcv = crv.CV(j);
	for ( i = 0; i < m_order[0]; i++) {
	  memcpy( crvcv, CV(i,j), hdim*sizeof(crvcv[0]) );
	  crvcv += hdim;
	}
      }
    }
    else {
      for ( i = 0; i < m_order[0]; i++) {
	crvcv = crv.CV(i);
	for ( j = 0; j < m_order[1]; j++ ) {
	  memcpy( crvcv, CV(i,j), hdim*sizeof(crvcv[0]) );
	  crvcv += hdim;
	}
      }
    }

    // transfer output srf cv memory to output curves
    leftcrv.m_cv_capacity = left_bez.m_cv_capacity;
    leftcrv.m_cv = left_bez.m_cv;
    left_bez.m_cv = 0;

    rightcrv.m_cv_capacity = right_bez.m_cv_capacity;
    rightcrv.m_cv = right_bez.m_cv;
    right_bez.m_cv = 0;

    // call curve splitter
    rc = crv.Split( t, leftcrv, rightcrv );

    // transfer output crv cv memory back to output surfaces
    left_bez.m_cv_capacity = leftcrv.m_cv_capacity;
    left_bez.m_cv = leftcrv.m_cv;
    leftcrv.m_cv = 0;

    right_bez.m_cv_capacity = rightcrv.m_cv_capacity;
    right_bez.m_cv = rightcrv.m_cv;
    rightcrv.m_cv = 0;

    if ( rc ) 
    {
      // Greg Arden,	12 May 2003 Fixes TRR 10627.  This block of code was wrong. 
      right_bez.m_dim      = left_bez.m_dim      = m_dim;
      right_bez.m_is_rat   = left_bez.m_is_rat   = m_is_rat;
      right_bez.m_order[0] = left_bez.m_order[0] = m_order[0];
      right_bez.m_order[1] = left_bez.m_order[1] = m_order[1];
      right_bez.m_cv_stride[1-dir] = left_bez.m_cv_stride[1-dir] = hdim;			
      left_bez.m_cv_stride[dir]    = leftcrv.m_cv_stride;  // 3 March 2005 - Dale Lear changed crvdim to m_cv_stride
      right_bez.m_cv_stride[dir]   = rightcrv.m_cv_stride;
    }
  }

  return rc;
}


bool ON_BezierSurface::IsSingular(		 // true if surface side is collapsed to a point
       int side														 // side of parameter space to test
																			// 0 = south, 1 = east, 2 = north, 3 = west
				) const
{
  int i,j,k=0;
  ON_3dPoint p[2];
  double fuzz[2] = {0.0,0.0};
  p[0].Zero();
  p[1].Zero();
  int i0 = 0;
  int i1 = 0;
  int j0 = 0;
  int j1 = 0;
  switch ( side ) {
  case 0: // south
      i0 = 0;
      i1 = Order(0);
      j0 = 0;
      j1 = 1;
    break;
  case 1: // east
      i0 = Order(0)-1;
      i1 = Order(0);
      j0 = 0;
      j1 = Order(1);
    break;
  case 2: // north
      i0 = 0;
      i1 = Order(0);
      j0 = Order(1)-1;
      j1 = Order(1);
    break;
  case 3: // west
      i0 = 0;
      i1 = 1;
      j0 = 0;
      j1 = Order(1);
    break;
  default:
    return false;
    break;
  }

  GetCV(i0,j0,p[k]);
  fuzz[k] = p[k].Fuzz();
  for ( i = i0; i < i1; i++ ) for ( j = j0; j < j1; j++ ) {
    k = (k+1)%2;
    GetCV( i, j, p[k] );
    fuzz[k] = p[k].Fuzz();
    if ( (p[0]-p[1]).MaximumCoordinate() > fuzz[0]+fuzz[1] )
      return false;
  }
  return true;
}

bool ON_BezierCurve::Reparametrize( double c )
{
  if ( 1.0 == c )
    return true;
  if ( ON_UNSET_VALUE == c )
    return false;

  MakeRational();

  int i, j;
  double* cv;
  int cvdim = CVSize();
  double d = c;
  for ( i = 1; i < m_order; i++ )
  {
    cv = CV(i);
    j = cvdim;
    while(j--) 
      *cv++ *= d;
    d *= c;
  }

  return true;
}

bool ON_BezierCurve::ScaleConrolPoints( int i, double w )
{
  if ( i < 0 || i >= m_order || w == 0.0 || w == ON_UNSET_VALUE )
    return false;
  if ( w == Weight(i) )
    return true;

  if ( !IsRational() )
    MakeRational();

  double c = Weight(i);
  if ( 0.0 == c || ON_UNSET_VALUE == c )
    return false;
  c = w/c;

  int k, j;
  double* cv;
  int cvdim = CVSize();
  for ( k = 0; k < m_order; k++ )
  {
    cv = CV(k);
    j = cvdim;
    while(j--) 
      *cv++ *= c;
  }
  CV(i)[m_dim] = w;

  return true;
}

bool ON_BezierCurve::ChangeWeights( int i0, double w0, int i1, double w1 )
{
  // 2 June 2003 Dale Lear bug fixes made this function work

  if ( i0 < 0 || i0 >= m_order || i1 < 0 || i1 >= m_order )
    return false;
  if ( 0.0 == w0 || !ON_IsValid(w0) || 0.0 == w1 || !ON_IsValid(w1) )
    return false;
  if ( w0 < 0.0 && w1 > 0.0 )
    return false;
  if ( w0 > 0.0 && w1 < 0.0 )
    return false;
  if ( i0 == i1 && w0 != w1 )
    return false;

  double r, s, v0, v1;
  int i;
  if (i0 > i1) 
  {
    i = i0; i0 = i1; i1 = i; r = w0; w0 = w1; w1 = r;
  }

  v0 = Weight(i0);
  v1 = Weight(i1);
  if ( w0 == v0 && w1 == v1 ) 
    return true;

  if ( 0.0 == v0 || !ON_IsValid(v0) || 0.0 == v1 || !ON_IsValid(v1) )
    return false;
  if ( v0 < 0.0 && v1 > 0.0 )
    return false;
  if ( v0 > 0.0 && v1 < 0.0 )
    return false;

  if (i0 == 0 || i0 == i1) 
  {
    s = w0/v0;
    r = (i0 != i1) ? pow( (w1/v1)/s, 1.0/((double)i1)) : 1.0;
  }
  else 
  {
    r = pow( (w1/v1)*(v0/w0), 1.0/((double)(i1-i0)) );
    s = (w0/v0)/pow(r,(double)i0);
  }
  if (r <= 0.0) 
    return false;
  
  MakeRational();
  int j, cvdim = CVSize();
  double* cv;
  if (s != 1.0) 
  {
    for ( i = 0; i < m_order; i++ )
    {
      cv = CV(i);
      j = cvdim;
      while (j--) *cv++ *= s;
    }
  } 
  if (r != 1.0)
    Reparametrize(r);

  // make sure weights agree to the last bit! 
  CV(i0)[m_dim] = w0;
  CV(i1)[m_dim] = w1;

  return true;
}

//////////////////////////////////////


ON_3dPoint ON_BezierCurve::PointAt( double t ) const
{
  ON_3dPoint p(0.0,0.0,0.0);
  EvPoint(t,p);
  return p;
}

ON_3dVector ON_BezierCurve::DerivativeAt( double t ) const
{
  ON_3dPoint  p(0.0,0.0,0.0);
  ON_3dVector d(0.0,0.0,0.0);
  Ev1Der(t,p,d);
  return d;
}

ON_3dVector ON_BezierCurve::TangentAt( double t ) const
{
  ON_3dPoint point;
  ON_3dVector tangent;
  EvTangent( t, point, tangent );
  return tangent;
}

ON_3dVector ON_BezierCurve::CurvatureAt( double t ) const
{
  ON_3dPoint point;
  ON_3dVector tangent, kappa;
  EvCurvature( t, point, tangent, kappa );
  return kappa;
}

bool ON_BezierCurve::EvTangent(
       double t,
       ON_3dPoint& point,
       ON_3dVector& tangent
       ) const
{
  ON_3dVector D1, D2;//, K;
  tangent.Zero();
  bool rc = Ev1Der( t, point, tangent );
  if ( rc && !tangent.Unitize() ) 
  {
    if ( Ev2Der( t, point, D1, D2 ) )
    {
      // Use l'Hopital's rule to show that if the unit tanget
      // exists, the 1rst derivative is zero, and the 2nd
      // derivative is nonzero, then the unit tangent is equal
      // to +/-the unitized 2nd derivative.  The sign is equal
      // to the sign of D1(s) o D2(s) as s approaches the 
      // evaluation parameter.
      tangent = D2;
      rc = tangent.Unitize();
      if ( rc )
      {
	ON_Interval domain = Domain();
	double tminus = 0.0;
	double tplus = 0.0;
	if ( domain.IsIncreasing() && ON_GetParameterTolerance( domain[0], domain[1], t, &tminus, &tplus ) )
	{
	  ON_3dPoint p;
	  ON_3dVector d1, d2;
	  double eps = 0.0;
	  double d1od2tol = 0.0; //1.0e-10; // 1e-5 is too big
	  double d1od2;
	  double tt = t;
	  //double dt = 0.0;

	  if ( t < domain[1] )
	  {
	    eps = tplus-t;
	    if ( eps <= 0.0 || t+eps > domain.ParameterAt(0.1) )
	      return rc;
	  }
	  else 
	  {
	    eps = tminus - t;
	    if ( eps >= 0.0 || t+eps < domain.ParameterAt(0.9) )
	      return rc;
	  }

	  int i, negative_count=0, zero_count=0;
	  int test_count = 3;
	  for ( i = 0; i < test_count; i++, eps *= 0.5 )
	  {
	    tt = t + eps;
	    if ( tt == t )
	      break;
	    if (!Ev2Der( tt, p, d1, d2 ))
	      break;
	    d1od2 = d1*d2;
	    if ( d1od2 > d1od2tol )
	      break;
	    if ( d1od2 < d1od2tol )
	      negative_count++;
	    else
	      zero_count++;
	  }
	  if ( negative_count > 0 && test_count == negative_count+zero_count )
	  {
	    // all sampled d1od2 values were <= 0 
	    // and at least one was strictly < 0.
	    tangent.Reverse();
	  }
	}
      }
    }
  }
  return rc;
}

bool ON_BezierCurve::EvCurvature(
       double t,
       ON_3dPoint& point,
       ON_3dVector& tangent,
       ON_3dVector& kappa
       ) const
{
  ON_3dVector d1, d2;
  bool rc = Ev2Der( t, point, d1, d2 );
  if ( rc )
  {
    rc = ON_EvCurvature( d1, d2, tangent, kappa )?true:false;
  }
  return rc;
}


bool ON_BezierCurve::EvPoint( // returns false if unable to evaluate
       double t,         // evaluation parameter
       ON_3dPoint& point   // returns value of curve
       ) const
{
  bool rc = false;
  double ws[128];
  double* v;
  if ( Dimension() <= 3 ) {
    v = &point.x;
    point.x = 0.0;
    point.y = 0.0;
    point.z = 0.0;
  }
  else if ( Dimension() <= 128 ) {
    v = ws;
  }
  else {
    v = (double*)onmalloc(Dimension()*sizeof(*v));
  }
  rc = Evaluate( t, 0, Dimension(), v );
  if ( Dimension() > 3 ) {
    point.x = v[0];
    point.y = v[1];
    point.z = v[2];
    if ( Dimension() > 128 )
      onfree(v);
  }
  return rc;
}

bool ON_BezierCurve::Ev1Der( // returns false if unable to evaluate
       double t,         // evaluation parameter
       ON_3dPoint& point,
       ON_3dVector& derivative
       ) const
{
  bool rc = false;
  const int dim = Dimension();
  double ws[2*64];
  double* v;
  point.x = 0.0;
  point.y = 0.0;
  point.z = 0.0;
  derivative.x = 0.0;
  derivative.y = 0.0;
  derivative.z = 0.0;
  if ( dim <= 64 ) {
    v = ws;
  }
  else {
    v = (double*)onmalloc(2*dim*sizeof(*v));
  }
  rc = Evaluate( t, 1, dim, v);
  point.x = v[0];
  derivative.x = v[dim];
  if ( dim > 1 ) {
    point.y = v[1];
    derivative.y = v[dim+1];
    if ( dim > 2 ) {
      point.z = v[2];
      derivative.z = v[dim+2];
      if ( dim > 64 )
	onfree(v);
    }
  }

  return rc;
}

bool ON_BezierCurve::Ev2Der( // returns false if unable to evaluate
       double t,         // evaluation parameter
       ON_3dPoint& point,
       ON_3dVector& firstDervative,
       ON_3dVector& secondDervative
       ) const
{
  bool rc = false;
  const int dim = Dimension();
  double ws[3*64];
  double* v;
  point.x = 0.0;
  point.y = 0.0;
  point.z = 0.0;
  firstDervative.x = 0.0;
  firstDervative.y = 0.0;
  firstDervative.z = 0.0;
  secondDervative.x = 0.0;
  secondDervative.y = 0.0;
  secondDervative.z = 0.0;
  if ( dim <= 64 ) {
    v = ws;
  }
  else {
    v = (double*)onmalloc(3*dim*sizeof(*v));
  }
  rc = Evaluate( t, 2, dim, v );
  point.x = v[0];
  firstDervative.x = v[dim];
  secondDervative.x = v[2*dim];
  if ( dim > 1 ) {
    point.y = v[1];
    firstDervative.y = v[dim+1];
    secondDervative.y = v[2*dim+1];
    if ( dim > 2 ) {
      point.z = v[2];
      firstDervative.z = v[dim+2];
      secondDervative.z = v[2*dim+2];
      if ( dim > 64 )
	onfree(v);
    }
  }

  return rc;
}






