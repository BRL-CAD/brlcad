//- Class: CubitVector
//-
//- Description: This file defines the CubitVector class which is a
//- standard three-dimensional vector. All relevant arithmetic
//- operators are overloaded so CubitVectors can be used similar to
//- built-in types.
//-
//- Owner: Greg Sjaardema
//- Checked by: Randy Lober, January 94
//- Version: $Id
//
// This file is from the CGM archive at https://bitbucket.org/fathomteam/cgm and
// altered for building with BRL-CAD as a stub for the g-sat converter.

#ifndef CUBITVECTOR_HPP
#define CUBITVECTOR_HPP

#include "common.h"
#include "vmath.h"

typedef bool CubitBoolean;

struct CubitVectorStruct
{
  double xVal;
  double yVal;
  double zVal;
};

class CubitVector;
typedef void ( CubitVector::*transform_function )( double gamma,
                                                   double gamma2);
// a pointer to some function that transforms the point,
// taking a double parameter.  e.g. blow_out, rotate, and scale_angle

class CubitVector
{
public:

    //- Heading: Constructors and Destructor
  CubitVector();  //- Default constructor.

  explicit CubitVector(const double x, const double y, const double z);
    //- Constructor: create vector from three components

  explicit CubitVector( const double xyz[3] );
    //- Constructor: create vector from tuple

  explicit CubitVector (const CubitVector& tail, const CubitVector& head);
    //- Constructor for a CubitVector starting at tail and pointing
    //- to head.

  CubitVector(const CubitVector& copy_from);  //- Copy Constructor

  explicit CubitVector(const CubitVectorStruct& from);

    //- Heading: Set and Inquire Functions
  void set(const double x, const double y, const double z);
    //- Change vector components to {x}, {y}, and {z}

  void set( const double xyz[3] );
    //- Change vector components to xyz[0], xyz[1], xyz[2]

  void set(const CubitVector& tail, const CubitVector& head);
    //- Change vector to go from tail to head.

  void set(const CubitVector& to_copy);
    //- Same as i.=(const CubitVector&)

  double x() const; //- Return x component of vector

  double y() const; //- Return y component of vector

  double z() const; //- Return z component of vector

  double& x();
  double& y();
  double& z();

  void get_xyz( double &x, double &y, double &z ) const; //- Get x, y, z components
  void get_xyz( double xyz[3] ) const; //- Get xyz tuple

  double &r(); //- Return r component of vector, if (r,theta) format

  double &theta();  //- Return theta component of vector, if (r,theta) format

  void x( const double x ); //- Set x component of vector

  void y( const double y ); //- Set y component of vector

  void z( const double z ); //- Set z component of vector

  void r( const double x ); //- Set r component of vector, if (r,theta) format

  void theta( const double y ); //- Set theta component of vector, if (r,theta) format

  void xy_to_rtheta();
    //- convert from cartesian to polar coordinates, just 2d for now
    //- theta is in [0,2 PI)

  void rtheta_to_xy();
    //- convert from  polar to cartesian coordinates, just 2d for now

  void scale_angle(double gamma, double );
    //- tranform_function.
    //- transform  (x,y) to (r,theta) to (r,gamma*theta) to (x',y')
    //- plus some additional scaling so long chords won't cross short ones

  void blow_out(double gamma, double gamma2 = 0.0);
    //- transform_function
    //- blow points radially away from the origin,

  void rotate(double angle, double );
    //- transform function.
    //- transform  (x,y) to (r,theta) to (r,theta+angle) to (x',y')

  void project_to_plane( const CubitVector &planenormal );
    //- project this vector onto a plane.

  void project_to_line_segment( const CubitVector &pt0, const CubitVector &pt1 );
    //- project this vector onto a line segment.

  void reflect_about_xaxis(double dummy, double );
    //- dummy argument to make it a transform function

  double normalize();
    //- Normalize (set magnitude equal to 1) vector - return the magnitude

  CubitVector& length(const double new_length);
    //- Change length of vector to {new_length}. Can be used to move a
    //- location a specified distance from the origin in the current
    //- orientation.

  double length() const;
    //- Calculate the length of the vector.
    //- Use {length_squared()} if only comparing lengths, not adding.

  double distance_between(const CubitVector& test_vector) const;
    //- Calculate the distance from the head of one vector
    //  to the head of the test_vector.

  double distance_between_squared(const CubitVector& test_vector) const;
    //- Calculate the distance squared from the head of one vector
    //  to the head of the test_vector.

  double distance_from_infinite_line(const CubitVector& point_on_line,
                                     const CubitVector& line_direction) const;
    //- Calculate the minimum distance between the head of this vector and a
    // line of infinite length.

  double distance_from_infinite_line_squared(const CubitVector& point_on_line,
                                     const CubitVector& line_direction) const;
    //- Calculate the square of the minimum distance between the head of this
    //  vector and a line of infinite length.

  double length_squared() const;
    //- Calculate the squared length of the vector.
    //- Faster than {length()} since it eliminates the square root if
    //- only comparing other lengths.

  double interior_angle(const CubitVector &otherVector) const;
    //- Calculate the interior angle: acos((a%b)/(|a||b|))
    //- Returns angle in degrees.

  static bool colinear( const CubitVector &p0,
                        const CubitVector &p1,
                        const CubitVector &p2 );
    //- determine if 3 points are colinear.

  static CubitVector normal( const CubitVector &p0,
                             const CubitVector &p1,
                             const CubitVector &p2 );
    //- normal given 3 positions.

  static bool barycentric_coordinates( const CubitVector &v1,
                                       const CubitVector &v2,
                                       const CubitVector &v3,
                                       const CubitVector &point,
                                       double &coord_A,
                                       double &coord_B,
                                       double &coord_C );

  double vector_angle_quick(const CubitVector& vec1, const CubitVector& vec2);
    //- Calculate the interior angle between the projections of
    //- {vec1} and {vec2} onto the plane defined by the {this} vector.
    //- The angle returned is the right-handed angle around the {this}
    //- vector from {vec1} to {vec2}. Angle is in radians.

  double vector_angle( const CubitVector &vector1,
                       const CubitVector &vector2) const;
    //- Compute the angle between the projections of {vector1} and {vector2}
    //- onto the plane defined by *this. The angle is the
    //- right-hand angle, in radians, about *this from {vector1} to {vector2}.

  void perpendicular_z();
    //- Transform this vector to a perpendicular one, leaving
    //- z-component alone. Rotates clockwise about the z-axis by pi/2.

  void print_me();
    //- Prints out the coordinates of this vector.

  void orthogonal_vectors( CubitVector &vector2, CubitVector &vector3 ) const;
    //- Finds 2 (arbitrary) vectors that are orthogonal to this one

  void next_point( const CubitVector &direction, double distance,
                   CubitVector& out_point ) const;
    //- Finds the next point in space based on *this* point (starting point),
    //- a direction and the distance to extend in the direction. The
    //- direction vector need not be a unit vector.  The out_point can be
    //- "*this" (i.e., modify point in place).

  CubitBoolean about_equal( const CubitVector &w,
    const double relative_tolerance = 1.0e-6,
    const double absolute_tolerance = 1.0e-6 ) const;
    // Return true if vectors are equal within either relative tolerance
    // or absolute tolerance.
    //
    // More specifically:
    // Return true if the magnitude of the difference is less than
    // 1. absolute_tolerance,
    // OR
    // 2. relative_tolerance times the magnitude of the vectors
    //
    // E.g.
    // if v = <1, 1.0e-7, 0>, w = <1, -1.0e-7, 0> and relative_tol = 1.0e-6,
    // then return true.

  CubitBoolean within_tolerance(const CubitVector &vectorPtr2,
                                double tolerance) const;
    //- Compare two vectors to see if they are spatially equal.
    // Return TRUE if difference in x, y, and z are all within tolerance.

  CubitBoolean within_scaled_tolerance(const CubitVector &v2, double tol) const;
  // Return true if vectors are within_tolerance() or if, for each coord,
  // the ratio of the two vector's coords differs no more than tol from 1.0.
  // Can be used to see if two vectors are equal up to a certain number of
  // significant digits.  For example, passing in a tolerance of 1e-6 will
  // return true if the two vectors are equal with 6 significant digits.

    //- Heading: Operator Overloads  *****************************
  CubitVector&  operator+=(const CubitVector &vec);
    //- Compound Assignment: addition: {this = this + vec}

  CubitVector& operator-=(const CubitVector &vec);
    //- Compound Assignment: subtraction: {this = this - vec}

  CubitVector& operator*=(const CubitVector &vec);
    //- Compound Assignment: cross product: {this = this * vec},
    //- non-commutative

  CubitVector& operator*=(const double scalar);
    //- Compound Assignment: multiplication: {this = this * scalar}

  CubitVector& operator/=(const double scalar);
    //- Compound Assignment: division: {this = this / scalar}

  CubitVector operator-() const;
    //- unary negation.

  double operator[](int i) const;
    //- return the ith value of the vector (x, y, z)

  friend CubitVector operator~(const CubitVector &vec);
    //- normalize. Returns a new vector which is a copy of {vec},
    //- scaled such that {|vec|=1}. Uses overloaded bitwise NOT operator.

  friend CubitVector operator+(const CubitVector &v1,
                               const CubitVector &v2);
    //- vector addition

  friend CubitVector operator-(const CubitVector &v1,
                               const CubitVector &v2);
    //- vector subtraction

  friend CubitVector operator*(const CubitVector &v1,
                               const CubitVector &v2);
    //- vector cross product, non-commutative

  friend CubitVector operator*(const CubitVector &v1, const double sclr);
    //- vector * scalar

  friend CubitVector operator*(const double sclr, const CubitVector &v1);
    //- scalar * vector

  friend double operator%(const CubitVector &v1, const CubitVector &v2);
    //- dot product

  friend CubitVector operator/(const CubitVector &v1, const double sclr);
    //- vector / scalar

  friend int operator==(const CubitVector &v1, const CubitVector &v2);
    //- Equality operator

  friend int operator!=(const CubitVector &v1, const CubitVector &v2);
    //- Inequality operator

  friend CubitVector interpolate(const double param, const CubitVector &v1,
                                 const CubitVector &v2);
    //- Interpolate between two vectors. Returns (1-param)*v1 + param*v2

  CubitVector &operator=(const CubitVectorStruct &from);

  operator CubitVectorStruct()
    {
      CubitVectorStruct to = {xVal, yVal, zVal};
      return to;
    }

  CubitVector &operator=(const CubitVector& from);

private:

  double xVal;  //- x component of vector.
  double yVal;  //- y component of vector.
  double zVal;  //- z component of vector.

  static bool attempt_barycentric_coordinates_adjustment( const CubitVector &v1,
                                                          const CubitVector &v2,
                                                          const CubitVector &v3,
                                                          const CubitVector &point,
                                                          double &coord_A,
                                                          double &coord_B,
                                                          double &coord_C );
  static bool private_barycentric_coordinates( bool adjust_on_fail,
                                               const CubitVector &v1,
                                               const CubitVector &v2,
                                               const CubitVector &v3,
                                               const CubitVector &point,
                                               double &coord_A,
                                               double &coord_B,
                                               double &coord_C );

};

CubitVector vectorRotate(const double angle,
                         const CubitVector &normalAxis,
                         const CubitVector &referenceAxis);
  //- A new coordinate system is created with the xy plane corresponding
  //- to the plane normal to {normalAxis}, and the x axis corresponding to
  //- the projection of {referenceAxis} onto the normal plane.  The normal
  //- plane is the tangent plane at the root point.  A unit vector is
  //- constructed along the local x axis and then rotated by the given
  //- ccw angle to form the new point.  The new point, then is a unit
  //- distance from the global origin in the tangent plane.
  //- {angle} is in radians.

inline double CubitVector::x() const
{ return xVal; }
inline double CubitVector::y() const
{ return yVal; }
inline double CubitVector::z() const
{ return zVal; }
inline double& CubitVector::x()
{ return xVal; }
inline double& CubitVector::y()
{ return yVal; }
inline double& CubitVector::z()
{ return zVal; }
inline void CubitVector::get_xyz(double xyz[3]) const
{
  xyz[0] = xVal;
  xyz[1] = yVal;
  xyz[2] = zVal;
}
inline void CubitVector::get_xyz(double &xOut, double &yOut, double &zOut) const
{
  xOut = xVal;
  yOut = yVal;
  zOut = zVal;
}
inline double &CubitVector::r()
{ return xVal; }
inline double &CubitVector::theta()
{ return yVal; }
inline void CubitVector::x( const double xIn )
{ xVal = xIn; }
inline void CubitVector::y( const double yIn )
{ yVal = yIn; }
inline void CubitVector::z( const double zIn )
{ zVal = zIn; }
inline void CubitVector::r( const double xIn )
{ xVal = xIn; }
inline void CubitVector::theta( const double yIn )
{ yVal = yIn; }
inline CubitVector& CubitVector::operator+=(const CubitVector &v)
{
  xVal += v.xVal;
  yVal += v.yVal;
  zVal += v.zVal;
  return *this;
}

inline CubitVector& CubitVector::operator-=(const CubitVector &v)
{
  xVal -= v.xVal;
  yVal -= v.yVal;
  zVal -= v.zVal;
  return *this;
}

inline CubitVector& CubitVector::operator*=(const CubitVector &v)
{
  double xcross, ycross, zcross;
  xcross = yVal * v.zVal - zVal * v.yVal;
  ycross = zVal * v.xVal - xVal * v.zVal;
  zcross = xVal * v.yVal - yVal * v.xVal;
  xVal = xcross;
  yVal = ycross;
  zVal = zcross;
  return *this;
}

inline CubitVector::CubitVector(const CubitVector& copy_from)
    : xVal(copy_from.xVal), yVal(copy_from.yVal), zVal(copy_from.zVal)
{}

inline CubitVector::CubitVector()
    : xVal(0), yVal(0), zVal(0)
{}

inline CubitVector::CubitVector (const CubitVector& tail,
                                 const CubitVector& head)
    : xVal(head.xVal - tail.xVal),
      yVal(head.yVal - tail.yVal),
      zVal(head.zVal - tail.zVal)
{}

inline CubitVector::CubitVector(const double xIn,
                                const double yIn,
                                const double zIn)
    : xVal(xIn), yVal(yIn), zVal(zIn)
{}

inline CubitVector::CubitVector(const double xyz[3])
    : xVal(xyz[0]), yVal(xyz[1]), zVal(xyz[2])
{}

// This sets the vector to be perpendicular to it's current direction.
// NOTE:
//      This is a 2D function.  It only works in the XY Plane.
inline void CubitVector::perpendicular_z()
{
  double temp = xVal;
  x( yVal );
  y( -temp );
}

inline void CubitVector::set(const double xIn,
                             const double yIn,
                             const double zIn)
{
  xVal = xIn;
  yVal = yIn;
  zVal = zIn;
}

inline void CubitVector::set(const double xyz[3])
{
  xVal = xyz[0];
  yVal = xyz[1];
  zVal = xyz[2];
}

inline void CubitVector::set(const CubitVector& tail,
                             const CubitVector& head)
{
  xVal = head.xVal - tail.xVal;
  yVal = head.yVal - tail.yVal;
  zVal = head.zVal - tail.zVal;
}

inline CubitVector& CubitVector::operator=(const CubitVector &from)
{
  xVal = from.xVal;
  yVal = from.yVal;
  zVal = from.zVal;
  return *this;
}

inline void CubitVector::set(const CubitVector& to_copy)
{
  *this = to_copy;
}

// Scale all values by scalar.
inline CubitVector& CubitVector::operator*=(const double scalar)
{
  xVal *= scalar;
  yVal *= scalar;
  zVal *= scalar;
  return *this;
}

// Scales all values by 1/scalar
inline CubitVector& CubitVector::operator/=(const double scalar)
{
  if(NEAR_ZERO(scalar, SMALL_FASTF))
    throw ("Cannot divide by zero.");
  //assert (scalar != 0);
  xVal /= scalar;
  yVal /= scalar;
  zVal /= scalar;
  return *this;
}

inline CubitVector& CubitVector::operator=(const CubitVectorStruct &from)
{
  xVal = from.xVal;
  yVal = from.yVal;
  zVal = from.zVal;
  return *this;
}

inline CubitVector::CubitVector(const CubitVectorStruct &from)
{
  xVal = from.xVal;
  yVal = from.yVal;
  zVal = from.zVal;
}

// Returns the normalized 'this'.
inline CubitVector operator~(const CubitVector &vec)
{
  double mag = sqrt(vec.xVal*vec.xVal +
                    vec.yVal*vec.yVal +
                    vec.zVal*vec.zVal);

  CubitVector temp = vec;
  if (!NEAR_ZERO(mag,SMALL_FASTF))
  {
    temp /= mag;
  }
  return temp;
}

// Unary minus.  Negates all values in vector.
inline CubitVector CubitVector::operator-() const
{
  return CubitVector(-xVal, -yVal, -zVal);
}

inline double CubitVector::operator[](int i) const
{
  if(i < 0 || i > 2)
    throw ("Index Out of Bounds");
  //assert(i > -1 && i < 3);
  if      (i == 0) return xVal;
  else if (i == 1) return yVal;
  else             return zVal;
}

inline CubitVector operator+(const CubitVector &vector1,
                      const CubitVector &vector2)
{
  double xv = vector1.xVal + vector2.xVal;
  double yv = vector1.yVal + vector2.yVal;
  double zv = vector1.zVal + vector2.zVal;
  return CubitVector(xv,yv,zv);
//  return CubitVector(vector1) += vector2;
}

inline CubitVector operator-(const CubitVector &vector1,
                      const CubitVector &vector2)
{
  double xv = vector1.xVal - vector2.xVal;
  double yv = vector1.yVal - vector2.yVal;
  double zv = vector1.zVal - vector2.zVal;
  return CubitVector(xv,yv,zv);
//  return CubitVector(vector1) -= vector2;
}

// Cross products.
// vector1 cross vector2
inline CubitVector operator*(const CubitVector &vector1,
                      const CubitVector &vector2)
{
  return CubitVector(vector1) *= vector2;
}

// Returns a scaled vector.
inline CubitVector operator*(const CubitVector &vector1,
                      const double scalar)
{
  return CubitVector(vector1) *= scalar;
}

// Returns a scaled vector
inline CubitVector operator*(const double scalar,
                             const CubitVector &vector1)
{
  return CubitVector(vector1) *= scalar;
}

// Returns a vector scaled by 1/scalar
inline CubitVector operator/(const CubitVector &vector1,
                             const double scalar)
{
  return CubitVector(vector1) /= scalar;
}

inline int operator==(const CubitVector &v1, const CubitVector &v2)
{
  return (NEAR_EQUAL(v1.xVal, v2.xVal, SMALL_FASTF) && NEAR_EQUAL(v1.yVal, v2.yVal, SMALL_FASTF) && NEAR_EQUAL(v1.zVal, v2.zVal, SMALL_FASTF));
}

inline int operator!=(const CubitVector &v1, const CubitVector &v2)
{
  return (!NEAR_EQUAL(v1.xVal, v2.xVal, SMALL_FASTF) && !NEAR_EQUAL(v1.yVal, v2.yVal, SMALL_FASTF) && !NEAR_EQUAL(v1.zVal, v2.zVal, SMALL_FASTF));
}

inline double CubitVector::length_squared() const
{
  return( xVal*xVal + yVal*yVal + zVal*zVal );
}

inline double CubitVector::length() const
{
  return( sqrt(xVal*xVal + yVal*yVal + zVal*zVal) );
}

inline double CubitVector::normalize()
{
  double mag = length();
  if (!NEAR_ZERO(mag, SMALL_FASTF))
  {
    xVal = xVal / mag;
    yVal = yVal / mag;
    zVal = zVal / mag;
  }
  return mag;
}


// Dot Product.
inline double operator%(const CubitVector &vector1,
                        const CubitVector &vector2)
{
  return( vector1.xVal * vector2.xVal +
          vector1.yVal * vector2.yVal +
          vector1.zVal * vector2.zVal );
}

// Interpolate between two vectors.
// Returns (1-param)*v1 + param*v2
inline CubitVector interpolate(const double param, const CubitVector &v1,
                               const CubitVector &v2)
{
  CubitVector temp = (1.0 - param) * v1;
  temp += param * v2;
  return temp;
}

inline CubitVector CubitVector::normal( const CubitVector &p0,
                                        const CubitVector &p1,
                                        const CubitVector &p2 )
{
   CubitVector edge0( p0, p1 );
   CubitVector edge1( p0, p2 );

   return edge0 * edge1; // not normalized.
}

#endif

