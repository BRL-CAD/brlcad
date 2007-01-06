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

bool ON_IsFinite(double x)
{
  // Returns true if x is a finite double.  Specifically,
  // _finite returns a nonzero value (true) if its argument x
  // is not infinite, that is, if -INF < x < +INF. 
  // It returns 0 (false) if the argument is infinite or a NaN.
  //
  // If you are trying to compile opennurbs on a platform
  // that does not support finite(), then see if you can
  // use _fpclass(), fpclass(), _isnan(), or isnan().  If
  // you can't find anything, then just set this
  // function to return true.

#if defined(ON_COMPILER_GNU)
  return (finite(x)?true:false);
#else
  return (_finite(x)?true:false);
#endif
}

bool ON_IsValid(double x)
{
  return (x != ON_UNSET_VALUE && ON_IsFinite(x) );
}

ON_Interval::ON_Interval()
{
  m_t[0] = m_t[1] = ON_UNSET_VALUE; 
}

ON_Interval::ON_Interval(double t0, double t1)
{
  Set(t0,t1);
}

ON_Interval::~ON_Interval()
{}

double&
ON_Interval::operator[](int i)
{
  return m_t[(i<=0)?0:1];
}

double
ON_Interval::operator[](int i) const
{
  return m_t[(i<=0)?0:1];
}

double
ON_Interval::Min() const
{
  return m_t[IsDecreasing()?1:0];
}

void ON_Interval::Destroy()
{
  Set(ON_UNSET_VALUE,ON_UNSET_VALUE);
}

void ON_Interval::Set(double t0,double t1)
{
  m_t[0] = t0;
  m_t[1] = t1;
}

double ON_Interval::ParameterAt(double x) const
{
  return (ON_IsValid(x) ? ((1.0-x)*m_t[0] + x*m_t[1]) : ON_UNSET_VALUE);
}

ON_Interval ON_Interval::ParameterAt(ON_Interval x) const
{
  return ON_Interval( ParameterAt(x[0]), ParameterAt(x[1]) );
}

double ON_Interval::NormalizedParameterAt( // returns x so that min*(1.0-x) + max*x = input
  double t
  ) const
{
  if (!ON_IsValid(t))
    return ON_UNSET_VALUE; // added 29 Sep 2006

  double x = m_t[0];
  if ( m_t[0] != m_t[1] ) {
    x = ( t == m_t[1]) ? 1.0 : (t - m_t[0])/(m_t[1] - m_t[0]);
  }
  return x;
}

ON_Interval ON_Interval::NormalizedParameterAt( // returns x so that min*(1.0-x) + max*x = input
  ON_Interval t
  ) const
{
	return  ON_Interval(	NormalizedParameterAt(t[0]) , 
												NormalizedParameterAt(t[1]) );
}

double
ON_Interval::Max() const
{
  return m_t[IsDecreasing()?0:1];
}

double
ON_Interval::Mid() const
{
  return 0.5*(m_t[0]+m_t[1]);
}

double
ON_Interval::Length() const
{
  return ( ON_IsValid(m_t[0]) && ON_IsValid(m_t[1]) ) ? m_t[1]-m_t[0] : 0.0;
}

bool
ON_Interval::IsIncreasing() const
{
  return ( m_t[0] < m_t[1] && ON_IsValid(m_t[0]) && ON_IsValid(m_t[1]) ) ? true : false;
}

bool
ON_Interval::IsDecreasing() const
{
  return ( m_t[0] > m_t[1] && ON_IsValid(m_t[0]) && ON_IsValid(m_t[1]) ) ? true : false;
}

bool
ON_Interval::IsInterval() const
{
  return ( m_t[0] != m_t[1] && ON_IsValid(m_t[0]) && ON_IsValid(m_t[1]) ) ? true : false;
}


bool
ON_Interval::IsSingleton() const
{
  return ( m_t[0] == m_t[1] && ON_IsValid(m_t[1]) ) ? true : false;
}

bool
ON_Interval::IsEmptySet() const
{
  return ( m_t[0] == ON_UNSET_VALUE && m_t[1] == ON_UNSET_VALUE ) ? true : false;
}

bool
ON_Interval::IsValid() const
{
  return ( ON_IsValid(m_t[0]) && ON_IsValid(m_t[0]) );
}

bool 
ON_Interval::MakeIncreasing()		// returns true if resulting interval IsIncreasing() 
{
	if( IsDecreasing()){ 
		Swap();
		return true;
	}
	return IsIncreasing();
}

bool 
ON_Interval::operator!=(const ON_Interval& other) const
{
  return Compare(other) ? true : false;
}

bool 
ON_Interval::operator==(const ON_Interval& other) const
{
  return Compare(other) ? false : true;
}


int
ON_Interval::Compare( const ON_Interval& other ) const
{
  if ( m_t[0] < other.m_t[0] )
    return -1;
  if ( m_t[0] > other.m_t[0] )
    return 1;
  if ( m_t[1] < other.m_t[1] )
    return -1;
  if ( m_t[1] > other.m_t[1] )
    return 1;
  return 0;
}

bool
ON_Interval::Includes( double t, bool bTestOpenInterval) const
{
  bool rc = false;
  if ( ON_IsValid(t) && ON_IsValid(m_t[0]) && ON_IsValid(m_t[1]) )
  {
    int i = (m_t[0] <= m_t[1]) ? 0 : 1;
    if ( bTestOpenInterval )
    {
      rc = ( m_t[i] < t && t < m_t[1-i] ) ? true : false;
    }
    else
    {
      rc = ( m_t[i] <= t && t <= m_t[1-i] ) ? true : false;
    }
  }
  return rc;
}

bool
ON_Interval::Includes( const ON_Interval& other, bool bProperSubSet ) const
{
  bool rc = ( Includes( other.m_t[0] ) && Includes( other.m_t[1] ) ) ? true : false;
  if ( rc && bProperSubSet )
  {
    if ( !Includes( other.m_t[0], true ) && !Includes( other.m_t[1], true ) )
      rc = false;
  }
  return rc;
}

void
ON_Interval::Reverse()
{
  if ( !IsEmptySet() ) {
    const double x = -m_t[0];
    m_t[0] = -m_t[1];
    m_t[1] = x;
  }
}

void
ON_Interval::Swap()
{
  const double x = m_t[0];
  m_t[0] = m_t[1];
  m_t[1] = x;
}

//////////
// If the intersection is not empty, then 
// intersection = [max(this.Min(),arg.Min()), min(this.Max(),arg.Max())]
// Intersection() returns true if the intersection is not empty.
// The interval [ON_UNSET_VALUE,ON_UNSET_VALUE] is considered to be
// the empty set interval.  The result of any intersection involving an
// empty set interval or disjoint intervals is the empty set interval.
bool ON_Interval::Intersection( // this = this intersect arg
       const ON_Interval& other
       )
{
  bool rc = false;
  if ( IsEmptySet() && other.IsEmptySet() )
    Destroy();
  else {
    double a, b, mn, mx;
    a = Min();
    b = other.Min();
    mn = (a>=b) ? a : b;
    a = Max();
    b = other.Max();
    mx = (a<=b) ? a : b;
    if ( mn <= mx ) {
      Set(mn,mx);
      rc = true;
    }
    else
      Destroy();
  }
  return rc;
}

//////////
// If the intersection is not empty, then 
// intersection = [max(argA.Min(),argB.Min()), min(argA.Max(),argB.Max())]
// Intersection() returns true if the intersection is not empty.
// The interval [ON_UNSET_VALUE,ON_UNSET_VALUE] is considered to be
// the empty set interval.  The result of any intersection involving an
// empty set interval or disjoint intervals is the empty set interval.
bool ON_Interval::Intersection( // this = intersection of two args
       const ON_Interval& ain, 
       const ON_Interval& bin
       )
{
  bool rc = false;
  if ( ain.IsEmptySet() && bin.IsEmptySet() )
    Destroy();
  else {
    double a, b, mn, mx;
    a = ain.Min();
    b = bin.Min();
    mn = (a>=b) ? a : b;
    a = ain.Max();
    b = bin.Max();
    mx = (a<=b) ? a : b;
    if ( mn <= mx ) {
      Set(mn,mx);
      rc = true;
    }
    else
      Destroy();
  }
  return rc;
}

//////////
  // The union of an empty set and an increasing interval is the increasing
  // interval.  The union of two empty sets is empty. The union of an empty
  // set an a non-empty interval is the non-empty interval.
  // The union of two non-empty intervals is
// union = [min(this.Min(),arg.Min()), max(this.Max(),arg.Max()),]
// Union() returns true if the union is not empty.
bool ON_Interval::Union( // this = this union arg
       const ON_Interval& other
       )
{
  bool rc = false;
  if ( other.IsEmptySet() ) {
    // this may be increasing, decreasing, or empty
    Set( Min(), Max() );
    rc = !IsEmptySet();
  }
  else if ( IsEmptySet() ) {
    // other may be increasing or decreasing
    Set( other.Min(), other.Max() );
    rc = true;
  }
  else {
    double a, b, mn, mx;
    a = Min();
    b = other.Min();
    mn = (a<=b) ? a : b;
    a = Max();
    b = other.Max();
    mx = (a>=b) ? a : b;
    if ( mn <= mx ) {
      Set(mn,mx);
      rc = true;
    }
    else
      Destroy();
  }
  return rc;
}

//////////
  // The union of an empty set and an increasing interval is the increasing
  // interval.  The union of two empty sets is empty. The union of an empty
  // set an a non-empty interval is the non-empty interval.
  // The union of two non-empty intervals is
// union = [min(argA.Min(),argB.Min()), max(argA.Max(),argB.Max()),]
// Union() returns true if the union is not empty.
bool ON_Interval::Union( // this = union of two args
       const ON_Interval& ain, 
       const ON_Interval& bin
       )
{
  bool rc = false;
  if ( bin.IsEmptySet() ) {
    // ain may be increasing, decreasing, or empty
    Set( ain.Min(), ain.Max() );
    rc = !IsEmptySet();
  }
  else if ( ain.IsEmptySet() ) {
    // bin may be increasing or decreasing
    Set( bin.Min(), bin.Max() );
    rc = true;
  }
  else {
    double a, b, mn, mx;
    a = ain.Min();
    b = bin.Min();
    mn = (a<=b) ? a : b;
    a = ain.Max();
    b = bin.Max();
    mx = (a>=b) ? a : b;
    if ( mn <= mx ) {
      Set(mn,mx);
      rc = true;
    }
    else
      Destroy();
  }
  return rc;
}


bool ON_3dVector::Decompose( // Computes a, b, c such that this vector = a*X + b*Y + c*Z
       //
       // If X,Y,Z is known to be an orthonormal frame,
       // then a = V*X, b = V*Y, c = V*Z will compute
       // the same result more quickly.
       const ON_3dVector& X,
       const ON_3dVector& Y,
       const ON_3dVector& Z,
       double* a,
       double* b,
       double* c
       ) const
{
  int rank;
  double pivot_ratio = 0.0;
  double row0[3], row1[3], row2[3];
  row0[0] = X*X;   row0[1] = X*Y;   row0[2] = X*Z;
  row1[0] = row0[1]; row1[1] = Y*Y;   row1[2] = Y*Z;
  row2[0] = row0[2]; row2[1] = row1[2]; row2[2] = Z*Z;
  rank = ON_Solve3x3( row0, row1, row2, 
                    (*this)*X, (*this)*Y, (*this)*Z,
                    a, b, c, &pivot_ratio );
  return (rank == 3) ? true : false;
}

int ON_3dVector::IsParallelTo( 
      // returns  1: this and other vectors are and parallel
      //         -1: this and other vectors are anti-parallel
      //          0: this and other vectors are not parallel
      //             or at least one of the vectors is zero
      const ON_3dVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  int rc = 0;
  const double ll = Length()*v.Length();
  if ( ll > 0.0 ) {
    const double cos_angle = (x*v.x + y*v.y + z*v.z)/ll;
    const double cos_tol = cos(angle_tolerance);
    if ( cos_angle >= cos_tol )
      rc = 1;
    else if ( cos_angle <= -cos_tol )
      rc = -1;
  }
  return rc;
}

bool ON_3fVector::IsPerpendicularTo(
      // returns true:  this and other vectors are perpendicular
      //         false: this and other vectors are not perpendicular
      //                or at least one of the vectors is zero
      const ON_3fVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  ON_3dVector V(*this);
  return V.IsPerpendicularTo(ON_3dVector(v),angle_tolerance);
}

bool ON_3dVector::IsPerpendicularTo(
      // returns true:  this and other vectors are perpendicular
      //         false: this and other vectors are not perpendicular
      //                or at least one of the vectors is zero
      const ON_3dVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  bool rc = false;
  const double ll = Length()*v.Length();
  if ( ll > 0.0 ) {
    if ( fabs((x*v.x + y*v.y + z*v.z)/ll) <= sin(angle_tolerance) )
      rc = true;
  }
  return rc;
}

bool ON_3fVector::PerpendicularTo( const ON_3fVector& v )
{
  ON_3dVector V(*this);
  return V.IsPerpendicularTo(ON_3dVector(v));
}

bool ON_3dVector::PerpendicularTo( const ON_3dVector& v )
{
  //bool rc = false;
  int i, j, k; 
  double a, b;
  k = 2;
  if ( fabs(v.y) > fabs(v.x) ) {
    if ( fabs(v.z) > fabs(v.y) ) {
      // |v.z| > |v.y| > |v.x|
      i = 2;
      j = 1;
      k = 0;
      a = v.z;
      b = -v.y;
    }
    else if ( fabs(v.z) >= fabs(v.x) ){
      // |v.y| >= |v.z| >= |v.x|
      i = 1;
      j = 2;
      k = 0;
      a = v.y;
      b = -v.z;
    }
    else {
      // |v.y| > |v.x| > |v.z|
      i = 1;
      j = 0;
      k = 2;
      a = v.y;
      b = -v.x;
    }
  }
  else if ( fabs(v.z) > fabs(v.x) ) {
    // |v.z| > |v.x| >= |v.y|
    i = 2;
    j = 0;
    k = 1;
    a = v.z;
    b = -v.x;
  }
  else if ( fabs(v.z) > fabs(v.y) ) {
    // |v.x| >= |v.z| > |v.y|
    i = 0;
    j = 2;
    k = 1;
    a = v.x;
    b = -v.z;
  }
  else {
    // |v.x| >= |v.y| >= |v.z|
    i = 0;
    j = 1;
    k = 2;
    a = v.x;
    b = -v.y;
  }
  double* this_v = &x;
  this_v[i] = b;
  this_v[j] = a;
  this_v[k] = 0.0;
  return (a != 0.0) ? true : false;
}

bool
ON_3dVector::PerpendicularTo( 
      const ON_3dPoint& P0, const ON_3dPoint& P1, const ON_3dPoint& P2
      )
{
  // Find a the unit normal to a triangle defined by 3 points
  ON_3dVector V0, V1, V2, N0, N1, N2;

  Zero();

  V0 = P2 - P1;
  V1 = P0 - P2;
  V2 = P1 - P0;

  N0 = ON_CrossProduct( V1, V2 );
  if ( !N0.Unitize() )
    return false;
  N1 = ON_CrossProduct( V2, V0 );
  if ( !N1.Unitize() )
    return false;
  N2 = ON_CrossProduct( V0, V1 );
  if ( !N2.Unitize() )
    return false;

  const double s0 = 1.0/V0.Length();
  const double s1 = 1.0/V1.Length();
  const double s2 = 1.0/V2.Length();

  // choose normal with smallest total error
  const double e0 = s0*fabs(ON_DotProduct(N0,V0)) + s1*fabs(ON_DotProduct(N0,V1)) + s2*fabs(ON_DotProduct(N0,V2));
  const double e1 = s0*fabs(ON_DotProduct(N1,V0)) + s1*fabs(ON_DotProduct(N1,V1)) + s2*fabs(ON_DotProduct(N1,V2));
  const double e2 = s0*fabs(ON_DotProduct(N2,V0)) + s1*fabs(ON_DotProduct(N2,V1)) + s2*fabs(ON_DotProduct(N2,V2));

  if ( e0 <= e1 ) {
    if ( e0 <= e2 ) {
      *this = N0;
    }
    else {
      *this = N2;
    }
  }
  else if (e1 <= e2) {
    *this = N1;
  }
  else {
    *this = N2;
  }
  
  return true;
}

void ON_2dPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,ww;
  if ( xform.m_xform ) {
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][3];
    if ( ww != 0.0 )
      ww = 1.0/ww;
    xx = ww*(xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][3]);
    yy = ww*(xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][3]);
    x = xx;
    y = yy;
  }
}

void ON_3dPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,zz,ww;
  if ( xform.m_xform ) {
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][2]*z + xform.m_xform[3][3];
    if ( ww != 0.0 )
      ww = 1.0/ww;
    xx = ww*(xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][2]*z + xform.m_xform[0][3]);
    yy = ww*(xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][2]*z + xform.m_xform[1][3]);
    zz = ww*(xform.m_xform[2][0]*x + xform.m_xform[2][1]*y + xform.m_xform[2][2]*z + xform.m_xform[2][3]);
    x = xx;
    y = yy;
    z = zz;
  }
}

void ON_4dPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,zz,ww;
  if ( xform.m_xform ) {
    xx = xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][2]*z + xform.m_xform[0][3]*w;
    yy = xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][2]*z + xform.m_xform[1][3]*w;
    zz = xform.m_xform[2][0]*x + xform.m_xform[2][1]*y + xform.m_xform[2][2]*z + xform.m_xform[2][3]*w;
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][2]*z + xform.m_xform[3][3]*w;
    x = xx;
    y = yy;
    z = zz;
    w = ww;
  }
}

void ON_2fPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,ww;
  if ( xform.m_xform ) {
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][3];
    if ( ww != 0.0 )
      ww = 1.0/ww;
    xx = ww*(xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][3]);
    yy = ww*(xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][3]);
    x = (float)xx;
    y = (float)yy;
  }
}

void ON_2fPoint::Rotate( 
      double angle,               // angle in radians
      const ON_2fPoint& center  // center of rotation
      )
{
  Rotate( sin(angle), cos(angle), center );
}

void ON_2fPoint::Rotate( 
      double sin_angle,           // sin(angle)
      double cos_angle,           // cos(angle)
      const ON_2fPoint& center  // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, ON_zaxis, ON_3dPoint(center) );
  Transform(rot);
}

void ON_3fPoint::Rotate( 
      double angle,               // angle in radians
      const ON_3fVector& axis,  // axis of rotation
      const ON_3fPoint& center  // center of rotation
      )
{
  Rotate( sin(angle), cos(angle), axis, center );
}

void ON_3fPoint::Rotate( 
      double sin_angle,           // sin(angle)
      double cos_angle,           // cos(angle)
      const ON_3fVector& axis,  // axis of rotation
      const ON_3fPoint& center  // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, ON_3dVector(axis), ON_3dPoint(center) );
  Transform(rot);
}

void ON_3fPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,zz,ww;
  if ( xform.m_xform ) {
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][2]*z + xform.m_xform[3][3];
    if ( ww != 0.0 )
      ww = 1.0/ww;
    xx = ww*(xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][2]*z + xform.m_xform[0][3]);
    yy = ww*(xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][2]*z + xform.m_xform[1][3]);
    zz = ww*(xform.m_xform[2][0]*x + xform.m_xform[2][1]*y + xform.m_xform[2][2]*z + xform.m_xform[2][3]);
    x = (float)xx;
    y = (float)yy;
    z = (float)zz;
  }
}

void ON_4fPoint::Transform( const ON_Xform& xform )
{
  double xx,yy,zz,ww;
  if ( xform.m_xform ) {
    xx = xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][2]*z + xform.m_xform[0][3]*w;
    yy = xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][2]*z + xform.m_xform[1][3]*w;
    zz = xform.m_xform[2][0]*x + xform.m_xform[2][1]*y + xform.m_xform[2][2]*z + xform.m_xform[2][3]*w;
    ww = xform.m_xform[3][0]*x + xform.m_xform[3][1]*y + xform.m_xform[3][2]*z + xform.m_xform[3][3]*w;
    x = (float)xx;
    y = (float)yy;
    z = (float)zz;
    w = (float)ww;
  }
}

double ON_3fPoint::Fuzz( 
          double absolute_tolerance // (default =  ON_ZERO_TOLERANCE) 
          ) const
{
  double t = MaximumCoordinate()* ON_SQRT_EPSILON;
  return(t > absolute_tolerance) ? t : absolute_tolerance;
}

bool ON_4dPoint::Normalize()
{
  bool rc = false;
  const int i = MaximumCoordinateIndex();
  double f[4];
  f[0] = fabs(x);
  f[1] = fabs(y);
  f[2] = fabs(z);
  f[3] = fabs(w);
  const double c = f[i];
  if ( c > 0.0 ) {
    double len = 1.0/c;
    f[0] *= len;
    f[1] *= len;
    f[2] *= len;
    f[3] *= len;
    f[i] = 1.0;
		// GBA 7/1/04.  Fixed typo
    const double s = 1.0/( c*sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2] + f[3]*f[3]));
    x *= s;
    y *= s;
    z *= s;
    w *= s;
    rc = true;
  }
  return rc;
}

bool ON_4fPoint::Normalize()
{
  bool rc = false;
  const int i = MaximumCoordinateIndex();
  double f[4];
  f[0] = fabs(x);
  f[1] = fabs(y);
  f[2] = fabs(z);
  f[3] = fabs(w);
  const double c = f[i];
  if ( c > 0.0 ) {
    double len = 1.0/c;
    f[0] *= len;
    f[1] *= len;
    f[2] *= len;
    f[3] *= len;
    f[i] = 1.0;
		// GBA 7/1/04.  Fixed typo
    const double s = 1.0/(c*sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2] + f[3]*f[3]));
    x = (float)(s*x);
    y = (float)(s*y);
    z = (float)(s*z);
    w = (float)(s*w);
    rc = true;
  }
  return rc;
}

bool ON_2fVector::Decompose( // Computes a, b such that this vector = a*X + b*Y
       //
       // If X,Y is known to be an orthonormal frame,
       // then a = V*X, b = V*Y will compute
       // the same result more quickly.
       const ON_2fVector& X,
       const ON_2fVector& Y,
       double* a,
       double* b
       ) const
{
  ON_2dVector V(*this);
  return V.Decompose(ON_2dVector(X),ON_2dVector(Y),a,b);
}


bool ON_2dVector::Decompose( // Computes a, b such that this vector = a*X + b*Y
       //
       // If X,Y is known to be an orthonormal frame,
       // then a = V*X, b = V*Y will compute
       // the same result more quickly.
       const ON_2dVector& X,
       const ON_2dVector& Y,
       double* a,
       double* b
       ) const
{
  int rank;
  double pivot_ratio = 0.0;
  double XoY = X*Y;
  rank = ON_Solve2x2( X*X, XoY, Y*Y, XoY,
                    (*this)*X, (*this)*Y, 
                    a, b, &pivot_ratio );
  return (rank == 2) ? true : false;
}


int ON_2fVector::IsParallelTo( 
      // returns  1: this and other vectors are and parallel
      //         -1: this and other vectors are anti-parallel
      //          0: this and other vectors are not parallel
      //             or at least one of the vectors is zero
      const ON_2fVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  ON_2dVector V(*this);
  return V.IsParallelTo(ON_2dVector(v),angle_tolerance);
}

int ON_2dVector::IsParallelTo( 
      // returns  1: this and other vectors are and parallel
      //         -1: this and other vectors are anti-parallel
      //          0: this and other vectors are not parallel
      //             or at least one of the vectors is zero
      const ON_2dVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  int rc = 0;
  const double ll = Length()*v.Length();
  if ( ll > 0.0 ) {
    const double cos_angle = (x*v.x + y*v.y)/ll;
    const double cos_tol = cos(angle_tolerance);
    if ( cos_angle >= cos_tol )
      rc = 1;
    else if ( cos_angle <= -cos_tol )
      rc = -1;
  }
  return rc;
}


bool ON_2fVector::IsPerpendicularTo(
      // returns true:  this and other vectors are perpendicular
      //         false: this and other vectors are not perpendicular
      //                or at least one of the vectors is zero
      const ON_2fVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  ON_2dVector V(*this);
  return V.IsPerpendicularTo(ON_2dVector(v),angle_tolerance);
}

bool ON_2dVector::IsPerpendicularTo(
      // returns true:  this and other vectors are perpendicular
      //         false: this and other vectors are not perpendicular
      //                or at least one of the vectors is zero
      const ON_2dVector& v,
      double angle_tolerance // (default=ON_DEFAULT_ANGLE_TOLERANCE) radians
      ) const
{
  bool rc = false;
  const double ll = Length()*v.Length();
  if ( ll > 0.0 ) {
    if ( fabs((x*v.x + y*v.y)/ll) <= sin(angle_tolerance) )
      rc = true;
  }
  return rc;
}

void ON_2dVector::Transform( const ON_Xform& xform )
{
  double xx,yy;
  xx = xform.m_xform[0][0]*x + xform.m_xform[0][1]*y;
  yy = xform.m_xform[1][0]*x + xform.m_xform[1][1]*y;
  x = xx;
  y = yy;
}

void ON_3fVector::Transform( const ON_Xform& xform )
{
  const double dx = x;
  const double dy = y;
  const double dz = z;
  double xx = xform.m_xform[0][0]*dx + xform.m_xform[0][1]*dy + xform.m_xform[0][2]*dz;
  double yy = xform.m_xform[1][0]*dx + xform.m_xform[1][1]*dy + xform.m_xform[1][2]*dz;
  double zz = xform.m_xform[2][0]*dx + xform.m_xform[2][1]*dy + xform.m_xform[2][2]*dz;
  x = (float)xx;
  y = (float)yy;
  z = (float)zz;
}

void ON_3dVector::Transform( const ON_Xform& xform )
{
  double xx,yy,zz;
  xx = xform.m_xform[0][0]*x + xform.m_xform[0][1]*y + xform.m_xform[0][2]*z;
  yy = xform.m_xform[1][0]*x + xform.m_xform[1][1]*y + xform.m_xform[1][2]*z;
  zz = xform.m_xform[2][0]*x + xform.m_xform[2][1]*y + xform.m_xform[2][2]*z;
  x = xx;
  y = yy;
  z = zz;
}

void ON_3dPoint::Rotate( 
      double angle,               // angle in radians
      const ON_3dVector& axis,  // axis of rotation
      const ON_3dPoint& center  // center of rotation
      )
{
  Rotate( sin(angle), cos(angle), axis, center );
}

void ON_3dPoint::Rotate( 
      double sin_angle,           // sin(angle)
      double cos_angle,           // cos(angle)
      const ON_3dVector& axis,  // axis of rotation
      const ON_3dPoint& center  // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, axis, center );
  Transform(rot);
}

void ON_2dPoint::Rotate( 
      double angle,               // angle in radians
      const ON_2dPoint& center  // center of rotation
      )
{
  Rotate( sin(angle), cos(angle), center );
}

void ON_2dPoint::Rotate( 
      double sin_angle,           // sin(angle)
      double cos_angle,           // cos(angle)
      const ON_2dPoint& center  // center of rotation
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, ON_zaxis, center );
  Transform(rot);
}

void ON_2dVector::Rotate( 
      double angle // angle in radians
      )
{
  Rotate( sin(angle), cos(angle) );
}

void ON_2dVector::Rotate( 
      double sin_angle, // sin(angle)
      double cos_angle  // cos(angle)
      )
{
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, ON_zaxis, ON_origin );
  Transform(rot);
}

bool ON_IsOrthogonalFrame( const ON_2dVector& X,  const ON_2dVector& Y )
{
  // returns true if X, Y, Z is an orthogonal frame
  double lx = X.Length();
  double ly = Y.Length();
  if ( lx <=  ON_SQRT_EPSILON )
    return false;
  if ( ly <=  ON_SQRT_EPSILON )
    return false;
  lx = 1.0/lx;
  ly = 1.0/ly;
  double x = ON_DotProduct( X, Y )*lx*ly;
  if ( fabs(x) >  ON_SQRT_EPSILON )
    return false;
  return true;
}

bool ON_IsOrthonormalFrame( const ON_2dVector& X,  const ON_2dVector& Y )
{
  // returns true if X, Y, Z is an orthonormal frame
  if ( !ON_IsOrthogonalFrame( X, Y ) )
    return false;
  double x = X.Length();
  if ( fabs(x-1.0) >  ON_SQRT_EPSILON )
    return false;
  x = Y.Length();
  if ( fabs(x-1.0) >  ON_SQRT_EPSILON )
    return false;

  return true;
}

bool ON_IsRightHandFrame( const ON_2dVector& X,  const ON_2dVector& Y )
{
  // returns true if X, Y, Z is an orthonormal right hand frame
  if ( !ON_IsOrthonormalFrame(X,Y) )
    return false;
  double x = ON_DotProduct( ON_CrossProduct( X, Y ), ON_zaxis );
  if ( x <=  ON_SQRT_EPSILON )
    return false;
  return true;
}
void ON_3fVector::Rotate( 
      double angle,              // angle in radians
      const ON_3fVector& axis   // axis of rotation
      )
{
  Rotate( sin(angle), cos(angle), axis );
}

void ON_3fVector::Rotate( 
      double sin_angle,        // sin(angle)
      double cos_angle,        // cos(angle)
      const ON_3fVector& axis  // axis of rotation
      )
{
  //bool rc = false;
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, ON_3dVector(axis), ON_origin );
  Transform(rot);
}

void ON_3dVector::Rotate( 
      double angle,              // angle in radians
      const ON_3dVector& axis   // axis of rotation
      )
{
  Rotate( sin(angle), cos(angle), axis );
}

void ON_3dVector::Rotate( 
      double sin_angle,        // sin(angle)
      double cos_angle,        // cos(angle)
      const ON_3dVector& axis  // axis of rotation
      )
{
  //bool rc = false;
  ON_Xform rot;
  rot.Rotation( sin_angle, cos_angle, axis, ON_origin );
  Transform(rot);
}

bool ON_IsOrthogonalFrame( const ON_3dVector& X,  const ON_3dVector& Y,  const ON_3dVector& Z )
{
  // returns true if X, Y, Z is an orthogonal frame
  if (! X.IsValid() || !Y.IsValid() || !Z.IsValid() )
    return false;

  double lx = X.Length();
  double ly = Y.Length();
  double lz = Z.Length();
  if ( lx <=  ON_SQRT_EPSILON )
    return false;
  if ( ly <=  ON_SQRT_EPSILON )
    return false;
  if ( lz <=  ON_SQRT_EPSILON )
    return false;
  lx = 1.0/lx;
  ly = 1.0/ly;
  lz = 1.0/lz;
  double xy = (X.x*Y.x + X.y*Y.y + X.z*Y.z)*lx*ly;
  double yz = (Y.x*Z.x + Y.y*Z.y + Y.z*Z.z)*ly*lz;
  double zx = (Z.x*X.x + Z.y*X.y + Z.z*X.z)*lz*lx;
  if (    fabs(xy) > ON_SQRT_EPSILON 
       || fabs(yz) > ON_SQRT_EPSILON
       || fabs(zx) > ON_SQRT_EPSILON
     )
  {
    double t = 0.0000152587890625;
    if ( fabs(xy) >= t || fabs(yz)  >= t || fabs(zx) >= t )
      return false;

    // do a more careful (and time consuming check)
    // This fixes RR 22219 and 22276
    ON_3dVector V;
    V = (lx*ly)*ON_CrossProduct(X,Y);
    t = fabs((V.x*Z.x + V.y*Z.y + V.z*Z.z)*lz);
    if ( fabs(t-1.0) > ON_SQRT_EPSILON )
      return false;

    V = (ly*lz)*ON_CrossProduct(Y,Z);
    t = fabs((V.x*X.x + V.y*X.y + V.z*X.z)*lx);
    if ( fabs(t-1.0) > ON_SQRT_EPSILON )
      return false;

    V = (lz*lx)*ON_CrossProduct(Z,X);
    t = fabs((V.x*Y.x + V.y*Y.y + V.z*Y.z)*ly);
    if ( fabs(t-1.0) > ON_SQRT_EPSILON )
      return false;
  }
  return true;
}

bool ON_IsOrthonormalFrame( const ON_3dVector& X,  const ON_3dVector& Y,  const ON_3dVector& Z )
{
  // returns true if X, Y, Z is an orthonormal frame
  if ( !ON_IsOrthogonalFrame( X, Y, Z ) )
    return false;
  double x = X.Length();
  if ( fabs(x-1.0) >  ON_SQRT_EPSILON )
    return false;
  x = Y.Length();
  if ( fabs(x-1.0) >  ON_SQRT_EPSILON )
    return false;
  x = Z.Length();
  if ( fabs(x-1.0) >  ON_SQRT_EPSILON )
    return false;

  return true;
}

bool ON_IsRightHandFrame( const ON_3dVector& X,  const ON_3dVector& Y,  const ON_3dVector& Z )
{
  // returns true if X, Y, Z is an orthonormal right hand frame
  if ( !ON_IsOrthonormalFrame(X,Y,Z) )
    return false;
  double x = ON_DotProduct( ON_CrossProduct( X, Y ), Z );
  if ( x <=  ON_SQRT_EPSILON )
    return false;
  return true;
}

ON_2dPoint ON_2dPoint::operator*( const ON_Xform& xform ) const
{
  double hx[4], w;
  xform.ActOnRight(x,y,0.0,1.0,hx);
  w = (hx[3] != 0.0) ? 1.0/hx[3] : 1.0;
  return ON_2dPoint( w*hx[0], w*hx[1] );
}

ON_3dPoint ON_3dPoint::operator*( const ON_Xform& xform ) const
{
  double hx[4], w;
  xform.ActOnRight(x,y,z,1.0,hx);
  w = (hx[3] != 0.0) ? 1.0/hx[3] : 1.0;
  return ON_3dPoint( w*hx[0], w*hx[1], w*hx[2] );
}

double ON_3dPoint::Fuzz( 
          double absolute_tolerance // (default =  ON_ZERO_TOLERANCE) 
          ) const
{
  double t = MaximumCoordinate()* ON_SQRT_EPSILON;
  return(t > absolute_tolerance) ? t : absolute_tolerance;
}

ON_4dPoint ON_4dPoint::operator*( const ON_Xform& xform ) const
{
  double hx[4];
  xform.ActOnRight(x,y,z,w,hx);
  return ON_4dPoint( hx[0],hx[1],hx[2],hx[3] );
}

ON_2dVector ON_2dVector::operator*( const ON_Xform& xform ) const
{
  double hx[4];
  xform.ActOnRight(x,y,0.0,0.0,hx);
  return ON_2dVector( hx[0],hx[1] );
}

ON_3dVector ON_3dVector::operator*( const ON_Xform& xform ) const
{
  double hx[4];
  xform.ActOnRight(x,y,z,0.0,hx);
  return ON_3dVector( hx[0],hx[1],hx[2] );
}

double ON_3fVector::Fuzz(
          double absolute_tolerance // (default =  ON_ZERO_TOLERANCE) 
          ) const
{
  double t = MaximumCoordinate()* ON_SQRT_EPSILON;
  return(t > absolute_tolerance) ? t : absolute_tolerance;
}


double ON_3dVector::Fuzz(
          double absolute_tolerance // (default =  ON_ZERO_TOLERANCE) 
          ) const
{
  double t = MaximumCoordinate()* ON_SQRT_EPSILON;
  return(t > absolute_tolerance) ? t : absolute_tolerance;
}

const ON_3dPoint  ON_UNSET_POINT(ON_UNSET_VALUE, ON_UNSET_VALUE, ON_UNSET_VALUE);
const ON_3dVector ON_UNSET_VECTOR(ON_UNSET_VALUE, ON_UNSET_VALUE, ON_UNSET_VALUE);

const ON_3dPoint  ON_origin(0.0, 0.0,0.0);
const ON_3dVector ON_xaxis(1.0, 0.0, 0.0);
const ON_3dVector ON_yaxis(0.0, 1.0, 0.0);
const ON_3dVector ON_zaxis(0.0, 0.0, 1.0);

const ON_3fPoint  ON_forigin(0.0f, 0.0f, 0.0f);
const ON_3fVector ON_fxaxis(1.0f, 0.0f, 0.0f);
const ON_3fVector ON_fyaxis(0.0f, 1.0f, 0.0f);
const ON_3fVector ON_fzaxis(0.0f, 0.0f, 1.0f);



//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
//
// ON_2fPoint
//

ON_2fPoint::ON_2fPoint()
{}

ON_2fPoint::ON_2fPoint( const double* p )
{
  if (p) {
    x = (float)p[0]; y = (float)p[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2fPoint::ON_2fPoint( const float* p )
{
  if (p) {
    x = p[0]; y = p[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2fPoint::ON_2fPoint(float xx,float yy)
{x=xx;y=yy;}

ON_2fPoint::ON_2fPoint(const ON_3fPoint& p)
{x=p.x;y=p.y;}

ON_2fPoint::ON_2fPoint(const ON_4fPoint& h)
{
  x=h.x;y=h.y;
  const float w = (h.w != 1.0f && h.w != 0.0f) ? 1.0f/h.w : 1.0f;
  x *= w;
  y *= w;
}

ON_2fPoint::ON_2fPoint(const ON_2fVector& v)
{x=v.x;y=v.y;}

ON_2fPoint::operator float*()
{
  return &x;
}

ON_2fPoint::operator const float*() const
{
  return &x;
}

ON_2fPoint& ON_2fPoint::operator=(const double* p)
{
  if ( p ) {
    x = (float)p[0];
    y = (float)p[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2fPoint& ON_2fPoint::operator=(const float* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2fPoint& ON_2fPoint::operator=(const ON_3fPoint& p)
{
  x = p.x;
  y = p.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator=(const ON_4fPoint& h)
{
  const float w = (h.w != 1.0f && h.w != 0.0f) ? 1.0f/h.w : 1.0f;
  x = w*h.x;
  y = w*h.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator=(const ON_2fVector& v)
{
  x = v.x;
  y = v.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator*=(float d)
{
  x *= d;
  y *= d;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator/=(float d)
{
  const float one_over_d = 1.0f/d;
  x *= one_over_d;
  y *= one_over_d;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator+=(const ON_2fPoint& p)
{
  x += p.x;
  y += p.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator+=(const ON_2fVector& v)
{
  x += v.x;
  y += v.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator-=(const ON_2fPoint& p)
{
  x -= p.x;
  y -= p.y;
  return *this;
}

ON_2fPoint& ON_2fPoint::operator-=(const ON_2fVector& v)
{
  x -= v.x;
  y -= v.y;
  return *this;
}

ON_2fPoint ON_2fPoint::operator*( float d ) const
{
  return ON_2fPoint(x*d,y*d);
}

ON_2fPoint ON_2fPoint::operator/( float d ) const
{
  const float one_over_d = 1.0f/d;
  return ON_2fPoint(x*one_over_d,y*one_over_d);
}

ON_2fPoint ON_2fPoint::operator+( const ON_2fPoint& p ) const
{
  return ON_2fPoint(x+p.x,y+p.y);
}

ON_2fPoint ON_2fPoint::operator+( const ON_2fVector& v ) const
{
  return ON_2fPoint(x+v.x,y+v.y);
}

ON_2fVector ON_2fPoint::operator-( const ON_2fPoint& p ) const
{
  return ON_2fVector(x-p.x,y-p.y);
}

ON_2fPoint ON_2fPoint::operator-( const ON_2fVector& v ) const
{
  return ON_2fPoint(x-v.x,y-v.y);
}

float ON_2fPoint::operator*(const ON_2fPoint& h) const
{
  return x*h.x + y*h.y;
}

float ON_2fPoint::operator*(const ON_2fVector& h) const
{
  return x*h.x + y*h.y;
}

float ON_2fPoint::operator*(const ON_4fPoint& h) const
{
  return x*h.x + y*h.y + h.w;
}

bool ON_2fPoint::operator==( const ON_2fPoint& p ) const
{
  return (x==p.x&&y==p.y)?true:false;
}

bool ON_2fPoint::operator!=( const ON_2fPoint& p ) const
{
  return (x!=p.x||y!=p.y)?true:false;
}

bool ON_2fPoint::operator<=( const ON_2fPoint& p ) const
{
  // dictionary order
  return ( (x<p.x) ? true : ((x==p.x&&y<=p.y)?true:false) );
}

bool ON_2fPoint::operator>=( const ON_2fPoint& p ) const
{
  // dictionary order
  return ( (x>p.x) ? true : ((x==p.x&&y>=p.y)?true:false) );
}

bool ON_2fPoint::operator<( const ON_2fPoint& p ) const
{
  // dictionary order
  return ( (x<p.x) ? true : ((x==p.x&&y<p.y)?true:false) );
}

bool ON_2fPoint::operator>( const ON_2fPoint& p ) const
{
  // dictionary order
  return ( (x>p.x) ? true : ((x==p.x&&y>p.y)?true:false) );
}

float ON_2fPoint::operator[](int i) const
{
  return (i<=0) ? x : y;
}

float& ON_2fPoint::operator[](int i)
{
  float* pd = (i<=0)? &x : &y;
  return *pd;
}

double ON_2fPoint::DistanceTo( const ON_2fPoint& p ) const
{
  return (p - *this).Length();
}

int ON_2fPoint::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? 1 : 0;
}

double ON_2fPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y);
  return c;
}

void ON_2fPoint::Zero()
{
  x = y = 0.0;
}

ON_2fPoint operator*(float d, const ON_2fPoint& p)
{
  return ON_2fPoint(d*p.x,d*p.y);
}

////////////////////////////////////////////////////////////////
//
// ON_3fPoint
//

ON_3fPoint::ON_3fPoint()
{}

ON_3fPoint::ON_3fPoint( const double* p )
{
  if (p) {
    x = (float)p[0]; y = (float)p[1]; z = (float)p[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3fPoint::ON_3fPoint( const float* p )
{
  if (p) {
    x = p[0]; y = p[1]; z = p[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3fPoint::ON_3fPoint(float xx,float yy,float zz) // :x(xx),y(yy),z(zz)
{x=xx;y=yy;z=zz;}

ON_3fPoint::ON_3fPoint(const ON_2fPoint& p) // : x(p.x),y(p.y),z(0.0)
{x=p.x;y=p.y;z=0.0;}

ON_3fPoint::ON_3fPoint(const ON_4fPoint& p) // :x(p.x),y(p.y),z(p.z)
{
  x=p.x;y=p.y;z=p.z;
  const float w = (p.w != 1.0f && p.w != 0.0f) ? 1.0f/p.w : 1.0f;
  x *= w;
  y *= w;
  z *= w;
}

ON_3fPoint::ON_3fPoint(const ON_3fVector& v) // : x(p.x),y(p.y),z(0.0)
{x=v.x;y=v.y;z=v.z;}

ON_3fPoint::operator float*()
{
  return &x;
}

ON_3fPoint::operator const float*() const
{
  return &x;
}

ON_3fPoint& ON_3fPoint::operator=(const double* p)
{
  if ( p ) {
    x = (float)p[0];
    y = (float)p[1];
    z = (float)p[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3fPoint& ON_3fPoint::operator=(const float* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
    z = p[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3fPoint& ON_3fPoint::operator=(const ON_2fPoint& p)
{
  x = p.x;
  y = p.y;
  z = 0.0;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator=(const ON_4fPoint& p)
{
  const float w = (p.w != 1.0f && p.w != 0.0f) ? 1.0f/p.w : 1.0f;
  x = w*p.x;
  y = w*p.y;
  z = w*p.z;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator=(const ON_3fVector& v)
{
  x = v.x;
  y = v.y;
  z = v.z;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator*=(float d)
{
  x *= d;
  y *= d;
  z *= d;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator/=(float d)
{
  const float one_over_d = 1.0f/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator+=(const ON_3fPoint& p)
{
  x += p.x;
  y += p.y;
  z += p.z;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator+=(const ON_3fVector& v)
{
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator-=(const ON_3fPoint& p)
{
  x -= p.x;
  y -= p.y;
  z -= p.z;
  return *this;
}

ON_3fPoint& ON_3fPoint::operator-=(const ON_3fVector& v)
{
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

ON_3fPoint ON_3fPoint::operator*( float d ) const
{
  return ON_3fPoint(x*d,y*d,z*d);
}

ON_3fPoint ON_3fPoint::operator/( float d ) const
{
  const float one_over_d = 1.0f/d;
  return ON_3fPoint(x*one_over_d,y*one_over_d,z*one_over_d);
}

ON_3fPoint ON_3fPoint::operator+( const ON_3fPoint& p ) const
{
  return ON_3fPoint(x+p.x,y+p.y,z+p.z);
}

ON_3fPoint ON_3fPoint::operator+( const ON_3fVector& v ) const
{
  return ON_3fPoint(x+v.x,y+v.y,z+v.z);
}

ON_3fVector ON_3fPoint::operator-( const ON_3fPoint& p ) const
{
  return ON_3fVector(x-p.x,y-p.y,z-p.z);
}

ON_3fPoint ON_3fPoint::operator-( const ON_3fVector& v ) const
{
  return ON_3fPoint(x-v.x,y-v.y,z-v.z);
}

float ON_3fPoint::operator*(const ON_3fPoint& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

float ON_3fPoint::operator*(const ON_3fVector& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

float ON_3fPoint::operator*(const ON_4fPoint& h) const
{
  return x*h.x + y*h.y + z*h.z + h.w;
}

bool ON_3fPoint::operator==( const ON_3fPoint& p ) const
{
  return (x==p.x&&y==p.y&&z==p.z)?true:false;
}

bool ON_3fPoint::operator!=( const ON_3fPoint& p ) const
{
  return (x!=p.x||y!=p.y||z!=p.z)?true:false;
}

bool ON_3fPoint::operator<=( const ON_3fPoint& p ) const
{
  // dictionary order
  return ((x<p.x)?true:((x==p.x)?((y<p.y)?true:(y==p.y&&z<=p.z)?true:false):false));
}

bool ON_3fPoint::operator>=( const ON_3fPoint& p ) const
{
  // dictionary order
  return ((x>p.x)?true:((x==p.x)?((y>p.y)?true:(y==p.y&&z>=p.z)?true:false):false));
}

bool ON_3fPoint::operator<( const ON_3fPoint& p ) const
{
  // dictionary order
  return ((x<p.x)?true:((x==p.x)?((y<p.y)?true:(y==p.y&&z<p.z)?true:false):false));
}

bool ON_3fPoint::operator>( const ON_3fPoint& p ) const
{
  // dictionary order
  return ((x>p.x)?true:((x==p.x)?((y>p.y)?true:(y==p.y&&z>p.z)?true:false):false));
}

float ON_3fPoint::operator[](int i) const
{
  return ( (i<=0)?x:((i>=2)?z:y) );
}

float& ON_3fPoint::operator[](int i)
{
  float* pd = (i<=0)? &x : ( (i>=2) ?  &z : &y);
  return *pd;
}

double ON_3fPoint::DistanceTo( const ON_3fPoint& p ) const
{
  return (p - *this).Length();
}

int ON_3fPoint::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? ((fabs(z)>fabs(y))?2:1) : ((fabs(z)>fabs(x))?2:0);
}

double ON_3fPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z);
  return c;
}

void ON_3fPoint::Zero()
{
  x = y = z = 0.0;
}

ON_3fPoint operator*(float d, const ON_3fPoint& p)
{
  return ON_3fPoint(d*p.x,d*p.y,d*p.z);
}

////////////////////////////////////////////////////////////////
//
// ON_4fPoint
//

ON_4fPoint::ON_4fPoint()
{}

ON_4fPoint::ON_4fPoint( const double* p )
{
  if (p) {
    x = (float)p[0]; y = (float)p[1]; z = (float)p[2]; w = (float)p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
}

ON_4fPoint::ON_4fPoint( const float* p )
{
  if (p) {
    x = p[0]; y = p[1]; z = p[2]; w = p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
}

ON_4fPoint::ON_4fPoint(float xx,float yy,float zz,float ww)
{x=xx;y=yy;z=zz;w=ww;}

ON_4fPoint::ON_4fPoint(const ON_2fPoint& p)
{x=p.x;y=p.y;z=0.0;w=1.0;}

ON_4fPoint::ON_4fPoint(const ON_3fPoint& p)
{
  x=p.x;y=p.y;z=p.z;w=1.0;
}

ON_4fPoint::ON_4fPoint(const ON_2fVector& v)
{x=v.x;y=v.y;z=w=0.0;}

ON_4fPoint::ON_4fPoint(const ON_3fVector& v)
{x=v.x;y=v.y;z=v.z;w=0.0;}

ON_4fPoint::operator float*()
{
  return &x;
}

ON_4fPoint::operator const float*() const
{
  return &x;
}

ON_4fPoint& ON_4fPoint::operator=(const double* p)
{
  if ( p ) {
    x = (float)p[0];
    y = (float)p[1];
    z = (float)p[2];
    w = (float)p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
  return *this;
}

ON_4fPoint& ON_4fPoint::operator=(const float* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
    z = p[2];
    w = p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
  return *this;
}

ON_4fPoint& ON_4fPoint::operator=(const ON_2fPoint& p)
{
  x = p.x;
  y = p.y;
  z = 0.0;
  w = 1.0;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator=(const ON_3fPoint& p)
{
  x = p.x;
  y = p.y;
  z = p.z;
  w = 1.0;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator=(const ON_2fVector& v)
{
  x = v.x;
  y = v.y;
  z = 0.0;
  w = 0.0;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator=(const ON_3fVector& v)
{
  x = v.x;
  y = v.y;
  z = v.z;
  w = 0.0;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator*=(float d)
{
  x *= d;
  y *= d;
  z *= d;
  w *= d;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator/=(float d)
{
  const float one_over_d = 1.0f/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  w *= one_over_d;
  return *this;
}

ON_4fPoint& ON_4fPoint::operator+=(const ON_4fPoint& p)
{
  // sum w = sqrt(w1*w2)
  if ( p.w == w ) {
    x += p.x;
    y += p.y;
    z += p.z;
  }
  else if (p.w == 0.0 ) {
    x += p.x;
    y += p.y;
    z += p.z;
  }
  else if ( w == 0.0 ) {
    x += p.x;
    y += p.y;
    z += p.z;
    w = p.w;
  }
  else {
    const double sw1 = (w>0.0) ? sqrt(w) : -sqrt(-w);
    const double sw2 = (p.w>0.0) ? sqrt(p.w) : -sqrt(-p.w);
    const double s1 = sw2/sw1;
    const double s2 = sw1/sw2;
    x = (float)(x*s1 + p.x*s2);
    y = (float)(y*s1 + p.y*s2);
    z = (float)(z*s1 + p.z*s2);
    w = (float)(sw1*sw2);
  }
  return *this;
}

ON_4fPoint& ON_4fPoint::operator-=(const ON_4fPoint& p)
{
  // difference w = sqrt(w1*w2)
  if ( p.w == w ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
  }
  else if (p.w == 0.0 ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
  }
  else if ( w == 0.0 ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
    w = p.w;
  }
  else {
    const double sw1 = (w>0.0) ? sqrt(w) : -sqrt(-w);
    const double sw2 = (p.w>0.0) ? sqrt(p.w) : -sqrt(-p.w);
    const double s1 = sw2/sw1;
    const double s2 = sw1/sw2;
    x = (float)(x*s1 - p.x*s2);
    y = (float)(y*s1 - p.y*s2);
    z = (float)(z*s1 - p.z*s2);
    w = (float)(sw1*sw2);
  }
  return *this;
}

ON_4fPoint ON_4fPoint::operator+( const ON_4fPoint& p ) const
{
  ON_4fPoint q(x,y,z,w);
  q+=p;
  return q;
}

ON_4fPoint ON_4fPoint::operator-( const ON_4fPoint& p ) const
{
  ON_4fPoint q(x,y,z,w);
  q-=p;
  return q;
}


ON_4fPoint ON_4fPoint::operator*( float d ) const
{
  return ON_4fPoint(x*d,y*d,z*d,w*d);
}

ON_4fPoint ON_4fPoint::operator/( float d ) const
{
  const float one_over_d = 1.0f/d;
  return ON_4fPoint(x*one_over_d,y*one_over_d,z*one_over_d,w*one_over_d);
}

float ON_4fPoint::operator*(const ON_4fPoint& h) const
{
  return x*h.x + y*h.y + z*h.z + w*h.w;
}

bool ON_4fPoint::operator==( ON_4fPoint p ) const
{
  ON_4fPoint a = *this; a.Normalize(); p.Normalize();
  if ( fabs(a.x-p.x) >  ON_SQRT_EPSILON ) return false;
  if ( fabs(a.y-p.y) >  ON_SQRT_EPSILON ) return false;
  if ( fabs(a.z-p.z) >  ON_SQRT_EPSILON ) return false;
  if ( fabs(a.w-p.w) >  ON_SQRT_EPSILON ) return false;
  return true;
}

bool ON_4fPoint::operator!=( const ON_4fPoint& p ) const
{
  return (*this == p)?false:true;
}

float ON_4fPoint::operator[](int i) const
{
  return ((i<=0)?x:((i>=3)?w:((i==1)?y:z)));
}

float& ON_4fPoint::operator[](int i)
{
  float* pd = (i<=0) ? &x : ( (i>=3) ? &w : (i==1)?&y:&z);
  return *pd;
}

int ON_4fPoint::MaximumCoordinateIndex() const
{
  const float* a = &x;
  int i = ( fabs(y) > fabs(x) ) ? 1 : 0;
  if (fabs(z) > fabs(a[i])) i = 2; if (fabs(w) > fabs(a[i])) i = 3;
  return i;
}

double ON_4fPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z); if (fabs(w)>c) c=fabs(w);
  return c;
}

void ON_4fPoint::Zero()
{
  x = y = z = w = 0.0;
}

ON_4fPoint operator*( float d, const ON_4fPoint& p )
{
  return ON_4fPoint( d*p.x, d*p.y, d*p.z, d*p.w );
}

////////////////////////////////////////////////////////////////
//
// ON_2fVector
//

// static
const ON_2fVector& ON_2fVector::UnitVector(int index)
{
  static ON_2fVector o(0.0,0.0);
  static ON_2fVector x(1.0,0.0);
  static ON_2fVector y(0.0,1.0);
  switch(index)
  {
  case 0:
    return x;
  case 1:
    return y;
  }
  return o;
}

ON_2fVector::ON_2fVector()
{}

ON_2fVector::ON_2fVector( const double* v )
{
  if (v) {
    x = (float)v[0]; y = (float)v[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2fVector::ON_2fVector( const float* v )
{
  if (v) {
    x = v[0]; y = v[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2fVector::ON_2fVector(float xx,float yy)
{x=xx;y=yy;}

ON_2fVector::ON_2fVector(const ON_3fVector& v)
{x=v.x;y=v.y;}

ON_2fVector::ON_2fVector(const ON_2fPoint& p)
{x=p.x;y=p.y;}

ON_2fVector::operator float*()
{
  return &x;
}

ON_2fVector::operator const float*() const
{
  return &x;
}

ON_2fVector& ON_2fVector::operator=(const double* v)
{
  if ( v ) {
    x = (float)v[0];
    y = (float)v[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2fVector& ON_2fVector::operator=(const float* v)
{
  if ( v ) {
    x = v[0];
    y = v[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2fVector& ON_2fVector::operator=(const ON_3fVector& v)
{
  x = v.x;
  y = v.y;
  return *this;
}

ON_2fVector& ON_2fVector::operator=(const ON_2fPoint& p)
{
  x = p.x;
  y = p.y;
  return *this;
}

ON_2fVector ON_2fVector::operator-() const
{
  return ON_2fVector(-x,-y);
}

ON_2fVector& ON_2fVector::operator*=(float d)
{
  x *= d;
  y *= d;
  return *this;
}

ON_2fVector& ON_2fVector::operator/=(float d)
{
  const float one_over_d = 1.0f/d;
  x *= one_over_d;
  y *= one_over_d;
  return *this;
}

ON_2fVector& ON_2fVector::operator+=(const ON_2fVector& v)
{
  x += v.x;
  y += v.y;
  return *this;
}

ON_2fVector& ON_2fVector::operator-=(const ON_2fVector& v)
{
  x -= v.x;
  y -= v.y;
  return *this;
}

ON_2fVector ON_2fVector::operator*( float d ) const
{
  return ON_2fVector(x*d,y*d);
}

float ON_2fVector::operator*( const ON_2fVector& v ) const
{
  return (x*v.x + y*v.y);
}

float ON_2fVector::operator*( const ON_2fPoint& v ) const
{
  return (x*v.x + y*v.y);
}

double ON_2fVector::operator*( const ON_2dVector& v ) const
{
  return (x*v.x + y*v.y);
}

ON_2fVector ON_2fVector::operator/( float d ) const
{
  const float one_over_d = 1.0f/d;
  return ON_2fVector(x*one_over_d,y*one_over_d);
}

ON_2fVector ON_2fVector::operator+( const ON_2fVector& v ) const
{
  return ON_2fVector(x+v.x,y+v.y);
}

ON_2fPoint ON_2fVector::operator+( const ON_2fPoint& p ) const
{
  return ON_2fPoint(x+p.x,y+p.y);
}

ON_2fVector ON_2fVector::operator-( const ON_2fVector& v ) const
{
  return ON_2fVector(x-v.x,y-v.y);
}

float ON_2fVector::operator*(const ON_4fPoint& h) const
{
  return x*h.x + y*h.y;
}

bool ON_2fVector::operator==( const ON_2fVector& v ) const
{
  return (x==v.x&&y==v.y)?true:false;
}

bool ON_2fVector::operator!=( const ON_2fVector& v ) const
{
  return (x!=v.x||y!=v.y)?true:false;
}

bool ON_2fVector::operator<=( const ON_2fVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x&&y<=v.y)?true:false));
}

bool ON_2fVector::operator>=( const ON_2fVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x&&y>=v.y)?true:false));
}

bool ON_2fVector::operator<( const ON_2fVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x&&y<v.y)?true:false));
}

bool ON_2fVector::operator>( const ON_2fVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x&&y>v.y)?true:false));
}

float ON_2fVector::operator[](int i) const
{
  return ((i<=0)?x:y);
}

float& ON_2fVector::operator[](int i)
{
  float* pd = (i<=0)? &x : &y;
  return *pd;
}

int ON_2fVector::MaximumCoordinateIndex() const
{
  return ( (fabs(y)>fabs(x)) ? 1 : 0 );
}

double ON_2fVector::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y);
  return c;
}

double ON_2fVector::LengthSquared() const
{
  return (x*x + y*y);
}

double ON_2fVector::Length() const
{
  double len;
  double fx = fabs(x);
  double fy = fabs(y);
  if ( fy > fx ) {
    len = fx; fx = fy; fy = len;
  }

  // 15 September 2003 Dale Lear
  //     For small denormalized doubles (positive but smaller
  //     than DBL_MIN), some compilers/FPUs set 1.0/fx to +INF.
  //     Without the ON_DBL_MIN test we end up with
  //     microscopic vectors that have infinte length!
  //
  //     Since this code starts with floats, none of this
  //     should be necessary, but it doesn't hurt anything.
  if ( fx > ON_DBL_MIN )
  {
    len = 1.0/fx;
    fy *= len;
    len = fx*sqrt(1.0 + fy*fy);
  }
  else if ( fx > 0.0 && ON_IsFinite(fx) )
    len = fx;
  else
    len = 0.0;
  return len;
}

void ON_2fVector::Zero()
{
  x = y = 0.0;
}

void ON_2fVector::Reverse()
{
  x = -x;
  y = -y;
}

bool ON_2fVector::Unitize()
{
  bool rc = false;
  // Since x,y are floats, d will not be denormalized and the
  // ON_DBL_MIN tests in ON_2dVector::Unitize() are not needed.
  double d = Length();
  if ( d > 0.0 ) 
  {
    d = 1.0/d;
    double dx = (double)x;
    double dy = (double)y;
    x = (float)(d*dx);
    y = (float)(d*dy);
    rc = true;
  }
  return rc;
}


bool ON_2fVector::IsTiny( double tiny_tol ) const
{
  return (fabs(x) <= tiny_tol && fabs(y) <= tiny_tol );
}

bool ON_2fVector::IsZero() const
{
  return (x==0.0f && y==0.0f);
}


// set this vector to be perpendicular to another vector
bool ON_2fVector::PerpendicularTo( // Result is not unitized. 
                      // returns false if input vector is zero
      const ON_2fVector& v
      )
{
  y = v.x;
  x = -v.y;
  return (x!= 0.0f || y!= 0.0f) ? true : false;
}

// set this vector to be perpendicular to a line defined by 2 points
bool ON_2fVector::PerpendicularTo( 
      const ON_2fPoint& p, 
      const ON_2fPoint& q
      )
{
  return PerpendicularTo(q-p);
}

ON_2fVector operator*(float d, const ON_2fVector& v)
{
  return ON_2fVector(d*v.x,d*v.y);
}

float ON_DotProduct( const ON_2fVector& a , const ON_2fVector& b )
{
  // inner (dot) product between 3d vectors
  return (a.x*b.x + a.y*b.y );
}

ON_3fVector ON_CrossProduct( const ON_2fVector& a , const ON_2fVector& b )
{
  return ON_3fVector(0.0, 0.0, a.x*b.y - b.x*a.y );
}

////////////////////////////////////////////////////////////////
//
// ON_3fVector
//

// static
const ON_3fVector& ON_3fVector::UnitVector(int index)
{
  static ON_3fVector o(0.0,0.0,0.0);
  static ON_3fVector x(1.0,0.0,0.0);
  static ON_3fVector y(0.0,1.0,0.0);
  static ON_3fVector z(0.0,0.0,1.0);
  switch(index)
  {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  }
  return o;
}

ON_3fVector::ON_3fVector()
{}

ON_3fVector::ON_3fVector( const double* v )
{
  if (v) {
    x = (float)v[0]; y = (float)v[1]; z = (float)v[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3fVector::ON_3fVector( const float* v )
{
  if (v) {
    x = v[0]; y = v[1]; z = v[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3fVector::ON_3fVector(float xx,float yy,float zz)
{x=xx;y=yy;z=zz;}

ON_3fVector::ON_3fVector(const ON_2fVector& v)
{x=v.x;y=v.y;z=0.0;}

ON_3fVector::ON_3fVector(const ON_3fPoint& p)
{x=p.x;y=p.y;z=p.z;}

ON_3fVector::operator float*()
{
  return &x;
}

ON_3fVector::operator const float*() const
{
  return &x;
}

ON_3fVector& ON_3fVector::operator=(const double* v)
{
  if ( v ) {
    x = (float)v[0];
    y = (float)v[1];
    z = (float)v[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3fVector& ON_3fVector::operator=(const float* v)
{
  if ( v ) {
    x = v[0];
    y = v[1];
    z = v[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3fVector& ON_3fVector::operator=(const ON_2fVector& v)
{
  x = v.x;
  y = v.y;
  z = 0.0;
  return *this;
}

ON_3fVector& ON_3fVector::operator=(const ON_3fPoint& p)
{
  x = p.x;
  y = p.y;
  z = p.z;
  return *this;
}

ON_3fVector ON_3fVector::operator-() const
{
  return ON_3fVector(-x,-y,-z);
}

ON_3fVector& ON_3fVector::operator*=(float d)
{
  x *= d;
  y *= d;
  z *= d;
  return *this;
}

ON_3fVector& ON_3fVector::operator/=(float d)
{
  const float one_over_d = 1.0f/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  return *this;
}

ON_3fVector& ON_3fVector::operator+=(const ON_3fVector& v)
{
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

ON_3fVector& ON_3fVector::operator-=(const ON_3fVector& v)
{
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

ON_3fVector ON_3fVector::operator*( float d ) const
{
  return ON_3fVector(x*d,y*d,z*d);
}

float ON_3fVector::operator*( const ON_3fVector& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

float ON_3fVector::operator*( const ON_3fPoint& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

double ON_3fVector::operator*( const ON_3dVector& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

ON_3fVector ON_3fVector::operator/( float d ) const
{
  const float one_over_d = 1.0f/d;
  return ON_3fVector(x*one_over_d,y*one_over_d,z*one_over_d);
}

ON_3fVector ON_3fVector::operator+( const ON_3fVector& v ) const
{
  return ON_3fVector(x+v.x,y+v.y,z+v.z);
}

ON_3fPoint ON_3fVector::operator+( const ON_3fPoint& p ) const
{
  return ON_3fPoint(x+p.x,y+p.y,z+p.z);
}

ON_3fVector ON_3fVector::operator-( const ON_3fVector& v ) const
{
  return ON_3fVector(x-v.x,y-v.y,z-v.z);
}

float ON_3fVector::operator*(const ON_4fPoint& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

bool ON_3fVector::operator==( const ON_3fVector& v ) const
{
  return (x==v.x&&y==v.y&&z==v.z)?true:false;
}

bool ON_3fVector::operator!=( const ON_3fVector& v ) const
{
  return (x!=v.x||y!=v.y||z!=v.z)?true:false;
}

bool ON_3fVector::operator<=( const ON_3fVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x)?((y<v.y)?true:(y==v.y&&z<=v.z)?true:false):false));
}

bool ON_3fVector::operator>=( const ON_3fVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x)?((y>v.y)?true:(y==v.y&&z>=v.z)?true:false):false));
}

bool ON_3fVector::operator<( const ON_3fVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x)?((y<v.y)?true:(y==v.y&&z<v.z)?true:false):false));
}

bool ON_3fVector::operator>( const ON_3fVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x)?((y>v.y)?true:(y==v.y&&z>v.z)?true:false):false));
}

float ON_3fVector::operator[](int i) const
{
  return ( (i<=0)?x:((i>=2)?z:y) );
}

float& ON_3fVector::operator[](int i)
{
  float* pd = (i<=0)? &x : ( (i>=2) ?  &z : &y);
  return *pd;
}

int ON_3fVector::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? ((fabs(z)>fabs(y))?2:1) : ((fabs(z)>fabs(x))?2:0);
}

double ON_3fVector::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z);
  return c;
}

double ON_3fVector::LengthSquared() const
{
  return (x*x + y*y + z*z);
}

double ON_3fVector::Length() const
{
  double len;
  double fx = fabs(x);
  double fy = fabs(y);
  double fz = fabs(z);
  if ( fy >= fx && fy >= fz ) {
    len = fx; fx = fy; fy = len;
  }
  else if ( fz >= fx && fz >= fy ) {
    len = fx; fx = fz; fz = len;
  }

  // 15 September 2003 Dale Lear
  //     For small denormalized doubles (positive but smaller
  //     than DBL_MIN), some compilers/FPUs set 1.0/fx to +INF.
  //     Without the ON_DBL_MIN test we end up with
  //     microscopic vectors that have infinte length!
  //
  //     Since this code starts with floats, none of this
  //     should be necessary, but it doesn't hurt anything.
  if ( fx > ON_DBL_MIN )
  {
    len = 1.0/fx;
    fy *= len;
    fz *= len;
    len = fx*sqrt(1.0 + fy*fy + fz*fz);
  }
  else if ( fx > 0.0 && ON_IsFinite(fx) )
    len = fx;
  else
    len = 0.0;
  return len;
}

void ON_3fVector::Zero()
{
  x = y = z = 0.0;
}

void ON_3fVector::Reverse()
{
  x = -x;
  y = -y;
  z = -z;
}

bool ON_3fVector::Unitize()
{
  bool rc = false;
  // Since x,y,z are floats, d will not be denormalized and the
  // ON_DBL_MIN tests in ON_2dVector::Unitize() are not needed.
  double d = Length();
  if ( d > 0.0 ) 
  {
    d = 1.0/d;
    double dx = x;
    double dy = y;
    double dz = z;
    x = (float)(d*dx);
    y = (float)(d*dy);
    z = (float)(d*dz);
    rc = true;
  }
  return rc;
}

bool ON_3fVector::IsTiny( double tiny_tol ) const
{
  return (fabs(x) <= tiny_tol && fabs(y) <= tiny_tol && fabs(z) <= tiny_tol );
}

bool ON_3fVector::IsZero() const
{
  return (x==0.0f && y==0.0f && z==0.0f);
}


ON_3fVector operator*(float d, const ON_3fVector& v)
{
  return ON_3fVector(d*v.x,d*v.y,d*v.z);
}

float ON_DotProduct( const ON_3fVector& a , const ON_3fVector& b )
{
  // inner (dot) product between 3d vectors
  return (a.x*b.x + a.y*b.y + a.z*b.z);
}

ON_3fVector ON_CrossProduct( const ON_3fVector& a , const ON_3fVector& b )
{
  return ON_3fVector(a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y );
}

ON_3fVector ON_CrossProduct( const float* a, const float* b )
{
  return ON_3fVector(a[1]*b[2] - b[1]*a[2], a[2]*b[0] - b[2]*a[0], a[0]*b[1] - b[0]*a[1] );
}

float ON_TripleProduct( const ON_3fVector& a, const ON_3fVector& b, const ON_3fVector& c )
{
  // = a o (b x c )
  return (a.x*(b.y*c.z - b.z*c.y) + a.y*(b.z*c.x - b.x*c.z) + a.z*(b.x*c.y - b.y*c.x));
}

float ON_TripleProduct( const float* a, const float* b, const float* c )
{
  // = a o (b x c )
  return (a[0]*(b[1]*c[2] - b[2]*c[1]) + a[1]*(b[2]*c[0] - b[0]*c[2]) + a[2]*(b[0]*c[1] - b[1]*c[0]));
}

////////////////////////////////////////////////////////////////
//
// ON_2dPoint
//

ON_2dPoint::ON_2dPoint()
{}

ON_2dPoint::ON_2dPoint( const float* p )
{
  if (p) {
    x = (double)p[0]; y = (double)p[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2dPoint::ON_2dPoint( const double* p )
{
  if (p) {
    x = p[0]; y = p[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2dPoint::ON_2dPoint(double xx,double yy)
{x=xx;y=yy;}

ON_2dPoint::ON_2dPoint(const ON_3dPoint& p)
{x=p.x;y=p.y;}

ON_2dPoint::ON_2dPoint(const ON_4dPoint& h)
{
  x=h.x;y=h.y;
  const double w = (h.w != 1.0 && h.w != 0.0) ? 1.0/h.w : 1.0;
  x *= w;
  y *= w;
}

ON_2dPoint::ON_2dPoint(const ON_2dVector& v)
{x=v.x;y=v.y;}

ON_2dPoint::operator double*()
{
  return &x;
}

ON_2dPoint::operator const double*() const
{
  return &x;
}

ON_2dPoint& ON_2dPoint::operator=(const float* p)
{
  if ( p ) {
    x = (double)p[0];
    y = (double)p[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2dPoint& ON_2dPoint::operator=(const double* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2dPoint& ON_2dPoint::operator=(const ON_3dPoint& p)
{
  x = p.x;
  y = p.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator=(const ON_4dPoint& h)
{
  const double w = (h.w != 1.0 && h.w != 0.0) ? 1.0/h.w : 1.0;
  x = w*h.x;
  y = w*h.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator=(const ON_2dVector& v)
{
  x = v.x;
  y = v.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator*=(double d)
{
  x *= d;
  y *= d;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator/=(double d)
{
  const double one_over_d = 1.0/d;
  x *= one_over_d;
  y *= one_over_d;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator+=(const ON_2dPoint& p)
{
  x += p.x;
  y += p.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator+=(const ON_2dVector& v)
{
  x += v.x;
  y += v.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator-=(const ON_2dPoint& p)
{
  x -= p.x;
  y -= p.y;
  return *this;
}

ON_2dPoint& ON_2dPoint::operator-=(const ON_2dVector& v)
{
  x -= v.x;
  y -= v.y;
  return *this;
}

ON_2dPoint ON_2dPoint::operator*( double d ) const
{
  return ON_2dPoint(x*d,y*d);
}

ON_2dPoint ON_2dPoint::operator/( double d ) const
{
  const double one_over_d = 1.0/d;
  return ON_2dPoint(x*one_over_d,y*one_over_d);
}

ON_2dPoint ON_2dPoint::operator+( const ON_2dPoint& p ) const
{
  return ON_2dPoint(x+p.x,y+p.y);
}

ON_2dPoint ON_2dPoint::operator+( const ON_2dVector& v ) const
{
  return ON_2dPoint(x+v.x,y+v.y);
}

ON_2dVector ON_2dPoint::operator-( const ON_2dPoint& p ) const
{
  return ON_2dVector(x-p.x,y-p.y);
}

ON_2dPoint ON_2dPoint::operator-( const ON_2dVector& v ) const
{
  return ON_2dPoint(x-v.x,y-v.y);
}

double ON_2dPoint::operator*(const ON_2dPoint& h) const
{
  return x*h.x + y*h.y;
}

double ON_2dPoint::operator*(const ON_2dVector& h) const
{
  return x*h.x + y*h.y;
}

double ON_2dPoint::operator*(const ON_4dPoint& h) const
{
  return x*h.x + y*h.y + h.w;
}

bool ON_2dPoint::operator==( const ON_2dPoint& p ) const
{
  return (x==p.x&&y==p.y)?true:false;
}

bool ON_2dPoint::operator!=( const ON_2dPoint& p ) const
{
  return (x!=p.x||y!=p.y)?true:false;
}

bool ON_2dPoint::operator<=( const ON_2dPoint& p ) const
{
  // dictionary order
  return ( (x<p.x) ? true : ((x==p.x&&y<=p.y)?true:false) );
}

bool ON_2dPoint::operator>=( const ON_2dPoint& p ) const
{
  // dictionary order
  return ( (x>p.x) ? true : ((x==p.x&&y>=p.y)?true:false) );
}

bool ON_2dPoint::operator<( const ON_2dPoint& p ) const
{
  // dictionary order
  return ( (x<p.x) ? true : ((x==p.x&&y<p.y)?true:false) );
}

bool ON_2dPoint::operator>( const ON_2dPoint& p ) const
{
  // dictionary order
  return ( (x>p.x) ? true : ((x==p.x&&y>p.y)?true:false) );
}

double ON_2dPoint::operator[](int i) const
{
  return (i<=0) ? x : y;
}

double& ON_2dPoint::operator[](int i)
{
  double* pd = (i<=0)? &x : &y;
  return *pd;
}

double ON_2dPoint::DistanceTo( const ON_2dPoint& p ) const
{
  return (p - *this).Length();
}

int ON_2dPoint::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? 1 : 0;
}

double ON_2dPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y);
  return c;
}

int ON_2dPoint::MinimumCoordinateIndex() const
{
  return (fabs(y)<fabs(x)) ? 1 : 0;
}

double ON_2dPoint::MinimumCoordinate() const
{
  double c = fabs(x); if (fabs(y)<c) c=fabs(y);
  return c;
}


void ON_2dPoint::Zero()
{
  x = y = 0.0;
}

ON_2dPoint operator*(double d, const ON_2dPoint& p)
{
  return ON_2dPoint(d*p.x,d*p.y);
}

////////////////////////////////////////////////////////////////
//
// ON_3dPoint
//

ON_3dPoint::ON_3dPoint()
{}

ON_3dPoint::ON_3dPoint( const float* p )
{
  if (p) {
    x = (double)p[0]; y = (double)p[1]; z = (double)p[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3dRay::ON_3dRay()
{}

ON_3dRay::~ON_3dRay()
{}

ON_3dPoint::ON_3dPoint( const double* p )
{
  if (p) {
    x = p[0]; y = p[1]; z = p[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3dPoint::ON_3dPoint(double xx,double yy,double zz) // :x(xx),y(yy),z(zz)
{x=xx;y=yy;z=zz;}

ON_3dPoint::ON_3dPoint(const ON_2dPoint& p) // : x(p.x),y(p.y),z(0.0)
{x=p.x;y=p.y;z=0.0;}

ON_3dPoint::ON_3dPoint(const ON_4dPoint& p) // :x(p.x),y(p.y),z(p.z)
{
  x=p.x;y=p.y;z=p.z;
  const double w = (p.w != 1.0 && p.w != 0.0) ? 1.0/p.w : 1.0;
  x *= w;
  y *= w;
  z *= w;
}

ON_3dPoint::ON_3dPoint(const ON_3dVector& v) // : x(p.x),y(p.y),z(0.0)
{x=v.x;y=v.y;z=v.z;}

ON_3dPoint::operator double*()
{
  return &x;
}

ON_3dPoint::operator const double*() const
{
  return &x;
}

ON_3dPoint& ON_3dPoint::operator=(const float* p)
{
  if ( p ) {
    x = (double)p[0];
    y = (double)p[1];
    z = (double)p[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3dPoint& ON_3dPoint::operator=(const double* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
    z = p[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3dPoint& ON_3dPoint::operator=(const ON_2dPoint& p)
{
  x = p.x;
  y = p.y;
  z = 0.0;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator=(const ON_4dPoint& p)
{
  const double w = (p.w != 1.0 && p.w != 0.0) ? 1.0/p.w : 1.0;
  x = w*p.x;
  y = w*p.y;
  z = w*p.z;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator=(const ON_3dVector& v)
{
  x = v.x;
  y = v.y;
  z = v.z;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator*=(double d)
{
  x *= d;
  y *= d;
  z *= d;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator/=(double d)
{
  const double one_over_d = 1.0/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator+=(const ON_3dPoint& p)
{
  x += p.x;
  y += p.y;
  z += p.z;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator+=(const ON_3dVector& v)
{
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator-=(const ON_3dPoint& p)
{
  x -= p.x;
  y -= p.y;
  z -= p.z;
  return *this;
}

ON_3dPoint& ON_3dPoint::operator-=(const ON_3dVector& v)
{
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

ON_3dPoint ON_3dPoint::operator*( double d ) const
{
  return ON_3dPoint(x*d,y*d,z*d);
}

ON_3dPoint ON_3dPoint::operator/( double d ) const
{
  const double one_over_d = 1.0/d;
  return ON_3dPoint(x*one_over_d,y*one_over_d,z*one_over_d);
}

ON_3dPoint ON_3dPoint::operator+( const ON_3dPoint& p ) const
{
  return ON_3dPoint(x+p.x,y+p.y,z+p.z);
}

ON_3dPoint ON_3dPoint::operator+( const ON_3dVector& v ) const
{
  return ON_3dPoint(x+v.x,y+v.y,z+v.z);
}

ON_3dVector ON_3dPoint::operator-( const ON_3dPoint& p ) const
{
  return ON_3dVector(x-p.x,y-p.y,z-p.z);
}

ON_3dPoint ON_3dPoint::operator-( const ON_3dVector& v ) const
{
  return ON_3dPoint(x-v.x,y-v.y,z-v.z);
}

double ON_3dPoint::operator*(const ON_3dPoint& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

double ON_3dPoint::operator*(const ON_3dVector& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

double ON_3dPoint::operator*(const ON_4dPoint& h) const
{
  return x*h.x + y*h.y + z*h.z + h.w;
}

bool ON_3dPoint::operator==( const ON_3dPoint& p ) const
{
  return (x==p.x&&y==p.y&&z==p.z)?true:false;
}

bool ON_3dPoint::operator!=( const ON_3dPoint& p ) const
{
  return (x!=p.x||y!=p.y||z!=p.z)?true:false;
}

bool ON_3dPoint::operator<=( const ON_3dPoint& p ) const
{
  // dictionary order
  return ((x<p.x)?true:((x==p.x)?((y<p.y)?true:(y==p.y&&z<=p.z)?true:false):false));
}

bool ON_3dPoint::operator>=( const ON_3dPoint& p ) const
{
  // dictionary order
  return ((x>p.x)?true:((x==p.x)?((y>p.y)?true:(y==p.y&&z>=p.z)?true:false):false));
}

bool ON_3dPoint::operator<( const ON_3dPoint& p ) const
{
  // dictionary order
  return ((x<p.x)?true:((x==p.x)?((y<p.y)?true:(y==p.y&&z<p.z)?true:false):false));
}

bool ON_3dPoint::operator>( const ON_3dPoint& p ) const
{
  // dictionary order
  return ((x>p.x)?true:((x==p.x)?((y>p.y)?true:(y==p.y&&z>p.z)?true:false):false));
}

double ON_3dPoint::operator[](int i) const
{
  return ( (i<=0)?x:((i>=2)?z:y) );
}

double& ON_3dPoint::operator[](int i)
{
  double* pd = (i<=0)? &x : ( (i>=2) ?  &z : &y);
  return *pd;
}

double ON_3dPoint::DistanceTo( const ON_3dPoint& p ) const
{
  return (p - *this).Length();
}

int ON_3dPoint::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? ((fabs(z)>fabs(y))?2:1) : ((fabs(z)>fabs(x))?2:0);
}

double ON_3dPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z);
  return c;
}

int ON_3dPoint::MinimumCoordinateIndex() const
{
  return (fabs(y)<fabs(x)) ? ((fabs(z)<fabs(y))?2:1) : ((fabs(z)<fabs(x))?2:0);
}

double ON_3dPoint::MinimumCoordinate() const
{
  double c = fabs(x); if (fabs(y)<c) c=fabs(y); if (fabs(z)<c) c=fabs(z);
  return c;
}

void ON_3dPoint::Zero()
{
  x = y = z = 0.0;
}

ON_3dPoint operator*(double d, const ON_3dPoint& p)
{
  return ON_3dPoint(d*p.x,d*p.y,d*p.z);
}

////////////////////////////////////////////////////////////////
//
// ON_4dPoint
//

ON_4dPoint::ON_4dPoint()
{}

ON_4dPoint::ON_4dPoint( const float* p )
{
  if (p) {
    x = (double)p[0]; y = (double)p[1]; z = (double)p[2]; w = (double)p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
}

ON_4dPoint::ON_4dPoint( const double* p )
{
  if (p) {
    x = p[0]; y = p[1]; z = p[2]; w = p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
}

ON_4dPoint::ON_4dPoint(double xx,double yy,double zz,double ww)
{x=xx;y=yy;z=zz;w=ww;}

ON_4dPoint::ON_4dPoint(const ON_2dPoint& p)
{x=p.x;y=p.y;z=0.0;w=1.0;}

ON_4dPoint::ON_4dPoint(const ON_3dPoint& p)
{
  x=p.x;y=p.y;z=p.z;w=1.0;
}

ON_4dPoint::ON_4dPoint(const ON_2dVector& v)
{x=v.x;y=v.y;z=w=0.0;}

ON_4dPoint::ON_4dPoint(const ON_3dVector& v)
{x=v.x;y=v.y;z=v.z;w=0.0;}

ON_4dPoint::operator double*()
{
  return &x;
}

ON_4dPoint::operator const double*() const
{
  return &x;
}

ON_4dPoint& ON_4dPoint::operator=(const float* p)
{
  if ( p ) {
    x = (double)p[0];
    y = (double)p[1];
    z = (double)p[2];
    w = (double)p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_2fPoint& p)
{
  x = (double)p.x;
  y = (double)p.y;
  z = 0.0;
  w = 1.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_3fPoint& p)
{
  x = (double)p.x;
  y = (double)p.y;
  z = (double)p.z;
  w = 1.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_2fVector& v)
{
  x = (double)v.x;
  y = (double)v.y;
  z = 0.0;
  w = 0.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_3fVector& v)
{
  x = (double)v.x;
  y = (double)v.y;
  z = (double)v.z;
  w = 0.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const double* p)
{
  if ( p ) {
    x = p[0];
    y = p[1];
    z = p[2];
    w = p[3];
  }
  else {
    x = y = z = 0.0; w = 1.0;
  }
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_2dPoint& p)
{
  x = p.x;
  y = p.y;
  z = 0.0;
  w = 1.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_3dPoint& p)
{
  x = p.x;
  y = p.y;
  z = p.z;
  w = 1.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_2dVector& v)
{
  x = v.x;
  y = v.y;
  z = 0.0;
  w = 0.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator=(const ON_3dVector& v)
{
  x = v.x;
  y = v.y;
  z = v.z;
  w = 0.0;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator*=(double d)
{
  x *= d;
  y *= d;
  z *= d;
  w *= d;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator/=(double d)
{
  const double one_over_d = 1.0/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  w *= one_over_d;
  return *this;
}

ON_4dPoint& ON_4dPoint::operator+=(const ON_4dPoint& p)
{
  // sum w = sqrt(w1*w2)
  if ( p.w == w ) {
    x += p.x;
    y += p.y;
    z += p.z;
  }
  else if (p.w == 0.0 ) {
    x += p.x;
    y += p.y;
    z += p.z;
  }
  else if ( w == 0.0 ) {
    x += p.x;
    y += p.y;
    z += p.z;
    w = p.w;
  }
  else {
    const double sw1 = (w>0.0) ? sqrt(w) : -sqrt(-w);
    const double sw2 = (p.w>0.0) ? sqrt(p.w) : -sqrt(-p.w);
    const double s1 = sw2/sw1;
    const double s2 = sw1/sw2;
    x = x*s1 + p.x*s2;
    y = y*s1 + p.y*s2;
    z = z*s1 + p.z*s2;
    w = sw1*sw2;
  }
  return *this;
}

ON_4dPoint& ON_4dPoint::operator-=(const ON_4dPoint& p)
{
  // difference w = sqrt(w1*w2)
  if ( p.w == w ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
  }
  else if (p.w == 0.0 ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
  }
  else if ( w == 0.0 ) {
    x -= p.x;
    y -= p.y;
    z -= p.z;
    w = p.w;
  }
  else {
    const double sw1 = (w>0.0) ? sqrt(w) : -sqrt(-w);
    const double sw2 = (p.w>0.0) ? sqrt(p.w) : -sqrt(-p.w);
    const double s1 = sw2/sw1;
    const double s2 = sw1/sw2;
    x = x*s1 - p.x*s2;
    y = y*s1 - p.y*s2;
    z = z*s1 - p.z*s2;
    w = sw1*sw2;
  }
  return *this;
}

ON_4dPoint ON_4dPoint::operator+( const ON_4dPoint& p ) const
{
  ON_4dPoint q(x,y,z,w);
  q+=p;
  return q;
}

ON_4dPoint ON_4dPoint::operator-( const ON_4dPoint& p ) const
{
  ON_4dPoint q(x,y,z,w);
  q-=p;
  return q;
}

ON_4dPoint ON_4dPoint::operator*( double d ) const
{
  return ON_4dPoint(x*d,y*d,z*d,w*d);
}

ON_4dPoint ON_4dPoint::operator/( double d ) const
{
  const double one_over_d = 1.0/d;
  return ON_4dPoint(x*one_over_d,y*one_over_d,z*one_over_d,w*one_over_d);
}

double ON_4dPoint::operator*(const ON_4dPoint& h) const
{
  return x*h.x + y*h.y + z*h.z + w*h.w;
}

bool ON_4dPoint::operator==( ON_4dPoint p ) const
{
  ON_4dPoint a = *this; a.Normalize(); p.Normalize();
  if ( fabs(a.x-p.x) > ON_SQRT_EPSILON ) return false;
  if ( fabs(a.y-p.y) > ON_SQRT_EPSILON ) return false;
  if ( fabs(a.z-p.z) > ON_SQRT_EPSILON ) return false;
  if ( fabs(a.w-p.w) > ON_SQRT_EPSILON ) return false;
  return true;
}

bool ON_4dPoint::operator!=( const ON_4dPoint& p ) const
{
  return (*this == p)?false:true;
}

double ON_4dPoint::operator[](int i) const
{
  return ((i<=0)?x:((i>=3)?w:((i==1)?y:z)));
}

double& ON_4dPoint::operator[](int i)
{
  double* pd = (i<=0) ? &x : ( (i>=3) ? &w : (i==1)?&y:&z);
  return *pd;
}

int ON_4dPoint::MaximumCoordinateIndex() const
{
  const double* a = &x;
  int i = ( fabs(y) > fabs(x) ) ? 1 : 0;
  if (fabs(z) > fabs(a[i])) i = 2; if (fabs(w) > fabs(a[i])) i = 3;
  return i;
}

double ON_4dPoint::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z); if (fabs(w)>c) c=fabs(w);
  return c;
}

int ON_4dPoint::MinimumCoordinateIndex() const
{
  const double* a = &x;
  int i = ( fabs(y) < fabs(x) ) ? 1 : 0;
  if (fabs(z) < fabs(a[i])) i = 2; if (fabs(w) < fabs(a[i])) i = 3;
  return i;
}

double ON_4dPoint::MinimumCoordinate() const
{
  double c = fabs(x); if (fabs(y)<c) c=fabs(y); if (fabs(z)<c) c=fabs(z); if (fabs(w)<c) c=fabs(w);
  return c;
}


void ON_4dPoint::Zero()
{
  x = y = z = w = 0.0;
}

ON_4dPoint operator*( double d, const ON_4dPoint& p )
{
  return ON_4dPoint( d*p.x, d*p.y, d*p.z, d*p.w );
}

////////////////////////////////////////////////////////////////
//
// ON_2dVector
//

// static
const ON_2dVector& ON_2dVector::UnitVector(int index)
{
  static ON_2dVector o(0.0,0.0);
  static ON_2dVector x(1.0,0.0);
  static ON_2dVector y(0.0,1.0);
  switch(index)
  {
  case 0:
    return x;
  case 1:
    return y;
  }
  return o;
}

ON_2dVector::ON_2dVector()
{}

ON_2dVector::ON_2dVector( const float* v )
{
  if (v) {
    x = (double)v[0]; y = (double)v[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2dVector::ON_2dVector( const double* v )
{
  if (v) {
    x = v[0]; y = v[1];
  }
  else {
    x = y = 0.0;
  }
}

ON_2dVector::ON_2dVector(double xx,double yy)
{x=xx;y=yy;}

ON_2dVector::ON_2dVector(const ON_3dVector& v)
{x=v.x;y=v.y;}

ON_2dVector::ON_2dVector(const ON_2dPoint& p)
{x=p.x;y=p.y;}

ON_2dVector::operator double*()
{
  return &x;
}

ON_2dVector::operator const double*() const
{
  return &x;
}

ON_2dVector& ON_2dVector::operator=(const float* v)
{
  if ( v ) {
    x = (double)v[0];
    y = (double)v[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2dVector& ON_2dVector::operator=(const double* v)
{
  if ( v ) {
    x = v[0];
    y = v[1];
  }
  else {
    x = y = 0.0;
  }
  return *this;
}

ON_2dVector& ON_2dVector::operator=(const ON_3dVector& v)
{
  x = v.x;
  y = v.y;
  return *this;
}

ON_2dVector& ON_2dVector::operator=(const ON_2dPoint& p)
{
  x = p.x;
  y = p.y;
  return *this;
}

ON_2dVector ON_2dVector::operator-() const
{
  return ON_2dVector(-x,-y);
}

ON_2dVector& ON_2dVector::operator*=(double d)
{
  x *= d;
  y *= d;
  return *this;
}

ON_2dVector& ON_2dVector::operator/=(double d)
{
  const double one_over_d = 1.0/d;
  x *= one_over_d;
  y *= one_over_d;
  return *this;
}

ON_2dVector& ON_2dVector::operator+=(const ON_2dVector& v)
{
  x += v.x;
  y += v.y;
  return *this;
}

ON_2dVector& ON_2dVector::operator-=(const ON_2dVector& v)
{
  x -= v.x;
  y -= v.y;
  return *this;
}

ON_2dVector ON_2dVector::operator*( double d ) const
{
  return ON_2dVector(x*d,y*d);
}

double ON_2dVector::operator*( const ON_2dVector& v ) const
{
  return (x*v.x + y*v.y);
}

double ON_2dVector::operator*( const ON_2dPoint& v ) const
{
  return (x*v.x + y*v.y);
}

double ON_2dVector::operator*( const ON_2fVector& v ) const
{
  return (x*v.x + y*v.y);
}

ON_2dVector ON_2dVector::operator/( double d ) const
{
  const double one_over_d = 1.0/d;
  return ON_2dVector(x*one_over_d,y*one_over_d);
}

ON_2dVector ON_2dVector::operator+( const ON_2dVector& v ) const
{
  return ON_2dVector(x+v.x,y+v.y);
}

ON_2dPoint ON_2dVector::operator+( const ON_2dPoint& p ) const
{
  return ON_2dPoint(x+p.x,y+p.y);
}

ON_2dVector ON_2dVector::operator-( const ON_2dVector& v ) const
{
  return ON_2dVector(x-v.x,y-v.y);
}

double ON_2dVector::operator*(const ON_4dPoint& h) const
{
  return x*h.x + y*h.y;
}

bool ON_2dVector::operator==( const ON_2dVector& v ) const
{
  return (x==v.x&&y==v.y)?true:false;
}

bool ON_2dVector::operator!=( const ON_2dVector& v ) const
{
  return (x!=v.x||y!=v.y)?true:false;
}

bool ON_2dVector::operator<=( const ON_2dVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x&&y<=v.y)?true:false));
}

bool ON_2dVector::operator>=( const ON_2dVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x&&y>=v.y)?true:false));
}

bool ON_2dVector::operator<( const ON_2dVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x&&y<v.y)?true:false));
}

bool ON_2dVector::operator>( const ON_2dVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x&&y>v.y)?true:false));
}

double ON_2dVector::operator[](int i) const
{
  return ((i<=0)?x:y);
}

double& ON_2dVector::operator[](int i)
{
  double* pd = (i<=0)? &x : &y;
  return *pd;
}

int ON_2dVector::MaximumCoordinateIndex() const
{
  return ( (fabs(y)>fabs(x)) ? 1 : 0 );
}

double ON_2dVector::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y);
  return c;
}

int ON_2dVector::MinimumCoordinateIndex() const
{
  return (fabs(y)<fabs(x)) ? 1: 0;
}

double ON_2dVector::MinimumCoordinate() const
{
  double c = fabs(x); if (fabs(y)<c) c=fabs(y);
  return c;
}

double ON_2dVector::LengthSquared() const
{
  return (x*x + y*y);
}

double ON_2dVector::Length() const
{
  double len;
  double fx = fabs(x);
  double fy = fabs(y);
  if ( fy > fx ) {
    len = fx; fx = fy; fy = len;
  }
 
  // 15 September 2003 Dale Lear
  //     For small denormalized doubles (positive but smaller
  //     than DBL_MIN), some compilers/FPUs set 1.0/fx to +INF.
  //     Without the ON_DBL_MIN test we end up with
  //     microscopic vectors that have infinte length!
  //
  //     This code is absolutely necessary.  It is a critical
  //     part of the bug fix for RR 11217.
  if ( fx > ON_DBL_MIN )
  {
    len = 1.0/fx;
    fy *= len;
    len = fx*sqrt(1.0 + fy*fy);
  }
  else if ( fx > 0.0 && ON_IsFinite(fx) )
    len = fx;
  else
    len = 0.0;

  return len;
}

double ON_2dVector::WedgeProduct(const ON_2dVector& B) const{
	return x*B.y - y*B.x;
}

void ON_2dVector::Zero()
{
  x = y = 0.0;
}

void ON_2dVector::Reverse()
{
  x = -x;
  y = -y;
}

bool ON_2dVector::Unitize()
{
  // 15 September 2003 Dale Lear
  //     Added the ON_DBL_MIN test.  See ON_2dVector::Length()
  //     for details.
  bool rc = false;
  double d = Length();
  if ( d > ON_DBL_MIN ) 
  {
    d = 1.0/d;
    x *= d;
    y *= d;
    rc = true;
  }
  else if ( d > 0.0 && ON_IsFinite(d) )
  {
    // This code is rarely used and can be slow.
    // It multiplies by 2^1023 in an attempt to 
    // normalize the coordinates.
    // If the renormalization works, then we're
    // ok.  If the renormalization fails, we
    // return false.
    ON_2dVector tmp;
    tmp.x = x*8.9884656743115795386465259539451e+307;
    tmp.y = y*8.9884656743115795386465259539451e+307;
    d = tmp.Length();
    if ( d > ON_DBL_MIN )
    {
      d = 1.0/d;
      x = tmp.x*d;
      y = tmp.y*d;
      rc = true;
    }
    else
    {
      x = 0.0;
      y = 0.0;
    }
  }
  else
  {
    x = 0.0;
    y = 0.0;
  }

  return rc;
}

bool ON_2dVector::IsTiny( double tiny_tol ) const
{
  return (fabs(x) <= tiny_tol && fabs(y) <= tiny_tol );
}

bool ON_2dVector::IsZero() const
{
  return (x==0.0 && y==0.0);
}

// set this vector to be perpendicular to another vector
bool ON_2dVector::PerpendicularTo( // Result is not unitized. 
                      // returns false if input vector is zero
      const ON_2dVector& v
      )
{
  y = v.x;
  x = -v.y;
  return (x!= 0.0 || y!= 0.0) ? true : false;
}

// set this vector to be perpendicular to a line defined by 2 points
bool ON_2dVector::PerpendicularTo( 
      const ON_2dPoint& p, 
      const ON_2dPoint& q
      )
{
  return PerpendicularTo(q-p);
}

ON_2dVector operator*(double d, const ON_2dVector& v)
{
  return ON_2dVector(d*v.x,d*v.y);
}

double ON_DotProduct( const ON_2dVector& a , const ON_2dVector& b )
{
  // inner (dot) product between 3d vectors
  return (a.x*b.x + a.y*b.y );
}

double ON_WedgeProduct( const ON_2dVector& a , const ON_2dVector& b )
{
  // exterior (wedge) product between 2d vectors
  return (a.x*b.y - a.y*b.x );
}

ON_3dVector ON_CrossProduct( const ON_2dVector& a , const ON_2dVector& b )
{
  return ON_3dVector(0.0, 0.0, a.x*b.y - b.x*a.y );
}

////////////////////////////////////////////////////////////////
//
// ON_3dVector
//

// static
const ON_3dVector& ON_3dVector::UnitVector(int index)
{
  static ON_3dVector o(0.0,0.0,0.0);
  static ON_3dVector x(1.0,0.0,0.0);
  static ON_3dVector y(0.0,1.0,0.0);
  static ON_3dVector z(0.0,0.0,1.0);
  switch(index)
  {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  }
  return o;
}

ON_3dVector::ON_3dVector()
{}

ON_3dVector::ON_3dVector( const float* v )
{
  if (v) {
    x = (double)v[0]; y = (double)v[1]; z = (double)v[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3dVector::ON_3dVector( const double* v )
{
  if (v) {
    x = v[0]; y = v[1]; z = v[2];
  }
  else {
    x = y = z = 0.0;
  }
}

ON_3dVector::ON_3dVector(double xx,double yy,double zz)
{x=xx;y=yy;z=zz;}

ON_3dVector::ON_3dVector(const ON_2dVector& v)
{x=v.x;y=v.y;z=0.0;}

ON_3dVector::ON_3dVector(const ON_3dPoint& p)
{x=p.x;y=p.y;z=p.z;}

ON_3dVector::operator double*()
{
  return &x;
}

ON_3dVector::operator const double*() const
{
  return &x;
}

ON_3dVector& ON_3dVector::operator=(const float* v)
{
  if ( v ) {
    x = (double)v[0];
    y = (double)v[1];
    z = (double)v[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3dVector& ON_3dVector::operator=(const double* v)
{
  if ( v ) {
    x = v[0];
    y = v[1];
    z = v[2];
  }
  else {
    x = y = z = 0.0;
  }
  return *this;
}

ON_3dVector& ON_3dVector::operator=(const ON_2dVector& v)
{
  x = v.x;
  y = v.y;
  z = 0.0;
  return *this;
}

ON_3dVector& ON_3dVector::operator=(const ON_3dPoint& p)
{
  x = p.x;
  y = p.y;
  z = p.z;
  return *this;
}

ON_3dVector ON_3dVector::operator-() const
{
  return ON_3dVector(-x,-y,-z);
}

ON_3dVector& ON_3dVector::operator*=(double d)
{
  x *= d;
  y *= d;
  z *= d;
  return *this;
}

ON_3dVector& ON_3dVector::operator/=(double d)
{
  const double one_over_d = 1.0/d;
  x *= one_over_d;
  y *= one_over_d;
  z *= one_over_d;
  return *this;
}

ON_3dVector& ON_3dVector::operator+=(const ON_3dVector& v)
{
  x += v.x;
  y += v.y;
  z += v.z;
  return *this;
}

ON_3dVector& ON_3dVector::operator-=(const ON_3dVector& v)
{
  x -= v.x;
  y -= v.y;
  z -= v.z;
  return *this;
}

ON_3dVector ON_3dVector::operator*( double d ) const
{
  return ON_3dVector(x*d,y*d,z*d);
}

double ON_3dVector::operator*( const ON_3dVector& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

double ON_3dVector::operator*( const ON_3dPoint& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

double ON_3dVector::operator*( const ON_3fVector& v ) const
{
  return (x*v.x + y*v.y + z*v.z);
}

ON_3dVector ON_3dVector::operator/( double d ) const
{
  const double one_over_d = 1.0/d;
  return ON_3dVector(x*one_over_d,y*one_over_d,z*one_over_d);
}

ON_3dVector ON_3dVector::operator+( const ON_3dVector& v ) const
{
  return ON_3dVector(x+v.x,y+v.y,z+v.z);
}

ON_3dPoint ON_3dVector::operator+( const ON_3dPoint& p ) const
{
  return ON_3dPoint(x+p.x,y+p.y,z+p.z);
}

ON_3dVector ON_3dVector::operator-( const ON_3dVector& v ) const
{
  return ON_3dVector(x-v.x,y-v.y,z-v.z);
}

double ON_3dVector::operator*(const ON_4dPoint& h) const
{
  return x*h.x + y*h.y + z*h.z;
}

bool ON_3dVector::operator==( const ON_3dVector& v ) const
{
  return (x==v.x&&y==v.y&&z==v.z)?true:false;
}

bool ON_3dVector::operator!=( const ON_3dVector& v ) const
{
  return (x!=v.x||y!=v.y||z!=v.z)?true:false;
}

bool ON_3dVector::operator<=( const ON_3dVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x)?((y<v.y)?true:(y==v.y&&z<=v.z)?true:false):false));
}

bool ON_3dVector::operator>=( const ON_3dVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x)?((y>v.y)?true:(y==v.y&&z>=v.z)?true:false):false));
}

bool ON_3dVector::operator<( const ON_3dVector& v ) const
{
  // dictionary order
  return ((x<v.x)?true:((x==v.x)?((y<v.y)?true:(y==v.y&&z<v.z)?true:false):false));
}

bool ON_3dVector::operator>( const ON_3dVector& v ) const
{
  // dictionary order
  return ((x>v.x)?true:((x==v.x)?((y>v.y)?true:(y==v.y&&z>v.z)?true:false):false));
}

double ON_3dVector::operator[](int i) const
{
  return ( (i<=0)?x:((i>=2)?z:y) );
}

double& ON_3dVector::operator[](int i)
{
  double* pd = (i<=0)? &x : ( (i>=2) ?  &z : &y);
  return *pd;
}

int ON_3dVector::MaximumCoordinateIndex() const
{
  return (fabs(y)>fabs(x)) ? ((fabs(z)>fabs(y))?2:1) : ((fabs(z)>fabs(x))?2:0);
}

double ON_3dVector::MaximumCoordinate() const
{
  double c = fabs(x); if (fabs(y)>c) c=fabs(y); if (fabs(z)>c) c=fabs(z);
  return c;
}

int ON_3dVector::MinimumCoordinateIndex() const
{
  return (fabs(y)<fabs(x)) ? ((fabs(z)<fabs(y))?2:1) : ((fabs(z)<fabs(x))?2:0);
}

double ON_3dVector::MinimumCoordinate() const
{
  double c = fabs(x); if (fabs(y)<c) c=fabs(y); if (fabs(z)<c) c=fabs(z);
  return c;
}

double ON_3dVector::LengthSquared() const
{
  return (x*x + y*y + z*z);
}

double ON_3dVector::Length() const
{
  double len;
  double fx = fabs(x);
  double fy = fabs(y);
  double fz = fabs(z);
  if ( fy >= fx && fy >= fz ) {
    len = fx; fx = fy; fy = len;
  }
  else if ( fz >= fx && fz >= fy ) {
    len = fx; fx = fz; fz = len;
  }

  // 15 September 2003 Dale Lear
  //     For small denormalized doubles (positive but smaller
  //     than DBL_MIN), some compilers/FPUs set 1.0/fx to +INF.
  //     Without the ON_DBL_MIN test we end up with
  //     microscopic vectors that have infinte length!
  //
  //     This code is absolutely necessary.  It is a critical
  //     part of the bug fix for RR 11217.
  if ( fx > ON_DBL_MIN ) 
  {
    len = 1.0/fx;
    fy *= len;
    fz *= len;
    len = fx*sqrt(1.0 + fy*fy + fz*fz);
  }
  else if ( fx > 0.0 && ON_IsFinite(fx) )
    len = fx;
  else
    len = 0.0;

  return len;
}

void ON_3dVector::Zero()
{
  x = y = z = 0.0;
}

void ON_3dVector::Reverse()
{
  x = -x;
  y = -y;
  z = -z;
}

bool ON_3dVector::Unitize()
{
  // 15 September 2003 Dale Lear
  //     Added the ON_DBL_MIN test.  See ON_3dVector::Length()
  //     for details.
  bool rc = false;
  double d = Length();
  if ( d > ON_DBL_MIN )
  {
    d = 1.0/d;
    x *= d;
    y *= d;
    z *= d;
    rc = true;
  }
  else if ( d > 0.0 && ON_IsFinite(d) )
  {
    // This code is rarely used and can be slow.
    // It multiplies by 2^1023 in an attempt to 
    // normalize the coordinates.
    // If the renormalization works, then we're
    // ok.  If the renormalization fails, we
    // return false.
    ON_3dVector tmp;
    tmp.x = x*8.9884656743115795386465259539451e+307;
    tmp.y = y*8.9884656743115795386465259539451e+307;
    tmp.z = z*8.9884656743115795386465259539451e+307;
    d = tmp.Length();
    if ( d > ON_DBL_MIN )
    {
      d = 1.0/d;
      x = tmp.x*d;
      y = tmp.y*d;
      z = tmp.z*d;
      rc = true;
    }
    else
    {
      x = 0.0;
      y = 0.0;
      z = 0.0;
    }
  }
  else
  {
    x = 0.0;
    y = 0.0;
    z = 0.0;
  }

  return rc;
}


bool ON_3dVector::IsTiny( double tiny_tol ) const
{
  return (fabs(x) <= tiny_tol && fabs(y) <= tiny_tol && fabs(z) <= tiny_tol );
}

bool ON_3dVector::IsZero() const
{
  return (x==0.0 && y==0.0 && z==0.0);
}

ON_3dVector operator*(double d, const ON_3dVector& v)
{
  return ON_3dVector(d*v.x,d*v.y,d*v.z);
}

double ON_DotProduct( const ON_3dVector& a , const ON_3dVector& b )
{
  // inner (dot) product between 3d vectors
  return (a.x*b.x + a.y*b.y + a.z*b.z);
}

ON_3dVector ON_CrossProduct( const ON_3dVector& a , const ON_3dVector& b )
{
  return ON_3dVector(a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y );
}

ON_3dVector ON_CrossProduct( const double* a, const double* b )
{
  return ON_3dVector(a[1]*b[2] - b[1]*a[2], a[2]*b[0] - b[2]*a[0], a[0]*b[1] - b[0]*a[1] );
}

double ON_TripleProduct( const ON_3dVector& a, const ON_3dVector& b, const ON_3dVector& c )
{
  // = a o (b x c )
  return (a.x*(b.y*c.z - b.z*c.y) + a.y*(b.z*c.x - b.x*c.z) + a.z*(b.x*c.y - b.y*c.x));
}

double ON_TripleProduct( const double* a, const double* b, const double* c )
{
  // = a o (b x c )
  return (a[0]*(b[1]*c[2] - b[2]*c[1]) + a[1]*(b[2]*c[0] - b[0]*c[2]) + a[2]*(b[0]*c[1] - b[1]*c[0]));
}

bool ON_2dVector::IsValid() const
{
  return ( ON_IsValid(x) && ON_IsValid(y) ) ? true : false;
}

bool ON_3dVector::IsValid() const
{
  return ( ON_IsValid(x) && ON_IsValid(y) && ON_IsValid(z) ) ? true : false;
}

bool ON_2dPoint::IsValid() const
{
  return (ON_IsValid(x) && ON_IsValid(y)) ? true : false;
}

bool ON_3dPoint::IsValid() const
{
  return (ON_IsValid(x) && ON_IsValid(y) && ON_IsValid(z) ) ? true : false;
}

bool ON_4dPoint::IsValid() const
{
  return (ON_IsValid(x) && ON_IsValid(y) && ON_IsValid(z) && ON_IsValid(w)) ? true : false;
}

void ON_2dPoint::Set(double xx, double yy)
{
  x = xx; y = yy;
}

void ON_3dPoint::Set(double xx, double yy, double zz)
{
  x = xx; y = yy; z = zz;
}


void ON_4dPoint::Set(double xx, double yy, double zz, double ww)
{
  x = xx; y = yy; z = zz; w = ww;
}

void ON_2dVector::Set(double xx, double yy)
{
  x = xx; y = yy;
}

void ON_3dVector::Set(double xx, double yy, double zz)
{
  x = xx; y = yy; z = zz;
}


void ON_2fPoint::Set(float xx, float yy)
{
  x = xx; y = yy;
}

void ON_3fPoint::Set(float xx, float yy, float zz)
{
  x = xx; y = yy; z = zz;
}


void ON_4fPoint::Set(float xx, float yy, float zz, float ww)
{
  x = xx; y = yy; z = zz; w = ww;
}

void ON_2fVector::Set(float xx, float yy)
{
  x = xx; y = yy;
}

void ON_3fVector::Set(float xx, float yy, float zz)
{
  x = xx; y = yy; z = zz;
}


bool ON_PlaneEquation::Create( ON_3dPoint P, ON_3dVector N )
{
  bool rc = false;
  if ( P.IsValid() && N.IsValid() )
  {
    x = N.x;
    y = N.y;
    z = N.z;
    rc = ( fabs(1.0 - Length()) > ON_ZERO_TOLERANCE ) ? Unitize() : true;
    d = -(x*P.x + y*P.y + z*P.z);
  }
  return rc;
}

bool ON_PlaneEquation::IsValid() const
{
  return (ON_3dVector::IsValid() && ON_IsValid(d));
}

bool ON_PlaneEquation::Transform( const ON_Xform& xform )
{
  bool rc = IsValid();
  if ( rc )
  {
    // Apply the inverse transpose to the equation parameters
    ON_Xform T(xform);
    rc = T.Invert();
    if ( rc )
    {
      const double xx=x;
      const double yy=y;
      const double zz=z;
      const double dd=d;
      x = T.m_xform[0][0]*xx + T.m_xform[1][0]*yy + T.m_xform[2][0]*zz + T.m_xform[3][0]*dd;
      y = T.m_xform[0][1]*xx + T.m_xform[1][1]*yy + T.m_xform[2][1]*zz + T.m_xform[3][1]*dd;
      z = T.m_xform[0][2]*xx + T.m_xform[1][2]*yy + T.m_xform[2][2]*zz + T.m_xform[3][2]*dd;
      d = T.m_xform[0][3]*xx + T.m_xform[1][3]*yy + T.m_xform[2][3]*zz + T.m_xform[3][3]*dd;
    }
  }
  return rc;
}

double ON_PlaneEquation::ValueAt(ON_3dPoint P) const
{
  return (x*P.x + y*P.y + z*P.z + d);
}

double ON_PlaneEquation::ValueAt(ON_4dPoint P) const
{
  return (x*P.x + y*P.y + z*P.z + d*P.w);
}

double ON_PlaneEquation::ValueAt(ON_3dVector P) const
{
  return (x*P.x + y*P.y + z*P.z + d);
}

double ON_PlaneEquation::ValueAt(double xx, double yy, double zz) const
{
  return (x*xx + y*yy + z*zz + d);
}

ON_3dPoint ON_PlaneEquation::ClosestPointTo( ON_3dPoint P ) const
{
  const double t = -(x*P.x + y*P.y + z*P.z + d)/(x*x+y*y+z*z);
  return ON_3dPoint( P.x + t*x, P.y + t*y, P.z + t*z);
}

double ON_PlaneEquation::MinimumValueAt(const ON_BoundingBox& bbox) const
{
  double xx, yy, zz, s;

  s = x*bbox.m_min.x;
  if ( s < (xx = x*bbox.m_max.x) ) xx = s;

  s = y*bbox.m_min.y;
  if ( s < (yy = y*bbox.m_max.y) ) yy = s;

  s = z*bbox.m_min.z;
  if ( s < (zz = z*bbox.m_max.z) ) zz = s;

  return (xx + yy + zz + d);
}

double ON_PlaneEquation::MaximumValueAt(const ON_BoundingBox& bbox) const
{
  double xx, yy, zz, s;

  s = x*bbox.m_min.x;
  if ( s > (xx = x*bbox.m_max.x) ) xx = s;

  s = y*bbox.m_min.y;
  if ( s > (yy = y*bbox.m_max.y) ) yy = s;

  s = z*bbox.m_min.z;
  if ( s > (zz = z*bbox.m_max.z) ) zz = s;

  return (xx + yy + zz + d);
}

bool ON_PlaneEquation::IsNearerThan( 
        const ON_BezierCurve& bezcrv,
        double s0,
        double s1,
        int sample_count,
        double endpoint_tolerance,
        double interior_tolerance,
        double* smin,
        double* smax
        ) const
{
  int i, n;
  double smn, smx, vmn, vmx, s, v, w;
  ON_3dPoint P;
  P.z = 0.0; // for 2d curves

  sample_count--;
  s = 0.5*(s0+s1);
  bezcrv.Evaluate(s,0,3,&P.x);
  vmn = vmx = x*P.x + y*P.y + z*P.z + d;
  smn = smx = s;
  if ( vmn > interior_tolerance )
  {
    if ( smin )
      *smin = s;
    if ( smax )
      *smax = s;
    return false;
  }

  if ( endpoint_tolerance >= 0.0 )
  {
    bezcrv.Evaluate(s0,0,3,&P.x);
    v = x*P.x + y*P.y + z*P.z + d;
    if (v > endpoint_tolerance )
    {
      if ( smin )
        *smin = smn;
      if ( smax )
        *smax = s0;
      return false;
    }
    if ( v < vmn ) { vmn = v; smn = s0; } else if (v > vmx) { vmx = v; smx = s0; }

    bezcrv.Evaluate(s1,0,3,&P.x);
    v = x*P.x + y*P.y + z*P.z + d;
    if (v > endpoint_tolerance )
    {
      if ( smin )
        *smin = smn;
      if ( smax )
        *smax = s1;
      return false;
    }
    if ( v < vmn ) { vmn = v; smn = s1; } else if (v > vmx) { vmx = v; smx = s1; }
  }

  w = 0.5;
  for ( n = 4; sample_count > 0; n *= 2 )
  {
    w *= 0.5;
    for ( i = 1; i < n; i+= 2 ) // do not test sample_count here
    {
      s = w*i;
      s = (1.0-s)*s0 + s*s1;
      bezcrv.Evaluate(s,0,3,&P.x);
      v = x*P.x + y*P.y + z*P.z + d;

      if ( v < vmn ) 
      { 
        vmn = v; 
        smn = s; 
      } 
      else if (v > vmx) 
      { 
        vmx = v; 
        smx = s; 
        if ( vmx > interior_tolerance )
        {
          if ( smin )
            *smin = smn;
          if ( smax )
            *smax = s;
          return false;
        }
      }

      sample_count--;
    }
  }

  if ( smin )
    *smin = smn;
  if ( smax )
    *smax = smx;
  return true;
}




int ON_Get3dConvexHull( 
          const ON_SimpleArray<ON_3dPoint>& points, 
          ON_SimpleArray<ON_PlaneEquation>& hull 
          )
{
  // This is a slow and stupid way to get the convex hull.
  // It works for small point sets.  If you need something
  // that is efficient, look elsewhere.

  const int point_count = points.Count();
  if ( point_count < 3 )
    return 0;

  const int count0 = hull.Count();
  hull.Reserve(count0+4);

  int i,j,k,n;
  ON_3dPoint A,B,C;
  ON_PlaneEquation e;
  double d0,d1,h0,h1,d;
  bool bGoodSide;
  for ( i = 0; i < point_count; i++ )
  {
    A = points[i];
    for ( j = i+1; j < point_count; j++ )
    {
      B = points[j];
      for (k = j+1; k < point_count; k++ )
      {
        C = points[k];
        e.ON_3dVector::operator=(ON_CrossProduct(B-A,C-A));
        if ( !e.ON_3dVector::Unitize() )
          continue;
        e.d = -(A.x*e.x + A.y*e.y + A.z*e.z);
        d0 = d1 = e.ValueAt(A);
        d = e.ValueAt(B); if ( d < d0 ) d0 = d; else if (d > d1) d1 = d;
        d = e.ValueAt(C); if ( d < d0 ) d0 = d; else if (d > d1) d1 = d;
        if ( d0 > -ON_ZERO_TOLERANCE )
          d0 = -ON_ZERO_TOLERANCE;
        if ( d1 < ON_ZERO_TOLERANCE )
          d1 = ON_ZERO_TOLERANCE;

        h0 = 0.0; h1 = 0.0;

        bGoodSide = true;
        for ( n = 0; n < point_count && bGoodSide; n++ )
        {
          d = e.ValueAt(points[n]);
          if ( d < h0 )
          {
            h0 = d;
            bGoodSide = (d0 <= h0 || h1 <= d1);
          }
          else if ( d > h1 )
          {
            h1 = d;
            bGoodSide = (d0 <= h0 || h1 <= d1);
          }
        }

        if ( bGoodSide )
        {
          if ( h1 <= d1 )
          {
            // all points are "below" the plane
            if ( d0 <= h0  )
            {
              // all points are also "above" the plane,
              hull.SetCount(count0);
              ON_PlaneEquation& e0 = hull.AppendNew();
              e0.x = -e.x;
              e0.y = -e.y;
              e0.z = -e.z;
              e0.d = -(e.d-h0);
            }
            ON_PlaneEquation& e1 = hull.AppendNew();
            e1.x = e.x;
            e1.y = e.y;
            e1.z = e.z;
            e1.d = (e.d-h1);
            if ( d0 <= h0  )
            {
              // points are (nearly) planar
              return 2;
            }
          }
          else if ( d0 <= h0  )
          {
            // all points are "above" the plane
            ON_PlaneEquation& e0 = hull.AppendNew();
            e0.x = -e.x;
            e0.y = -e.y;
            e0.z = -e.z;
            e0.d = -(e.d-h0);
          }
        }
      }
    }
  }

  if ( hull.Count() < count0 + 4 )
    hull.SetCount(count0);

  return hull.Count() - count0;
}

