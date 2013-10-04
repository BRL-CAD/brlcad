/*
 * Poly2Tri Copyright (c) 2009-2010, Poly2Tri Contributors
 * http://code.google.com/p/poly2tri/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of Poly2Tri nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Include guard
#ifndef SHAPES_H
#define SHAPES_H

#ifndef P2T_EXPORT
#  if defined(P2T_DLL_EXPORTS) && defined(P2T_DLL_IMPORTS)
#    error "Only P2T_DLL_EXPORTS or P2T_DLL_IMPORTS can be defined, not both."
#  elif defined(P2T_DLL_EXPORTS)
#    define P2T_EXPORT __declspec(dllexport)
#  elif defined(P2T_DLL_IMPORTS)
#    define P2T_EXPORT __declspec(dllimport)
#  else
#    define P2T_EXPORT
#  endif
#endif

#include <vector>
#include <cstddef>
#include <assert.h>
#include <cfloat>
#include <cmath>

/* Avoid exact floating point comparison warnings, but this macro
 * is used in some places where *very* tight equality is needed - 
 * use DBL_MIN */
#define EQ(v1, v2) ((v1 - v2 > -DBL_MIN) && (v1 - v2 < DBL_MIN))

namespace p2t {

struct Edge;

struct Point {

  double x, y;

  /// Default constructor does nothing (for performance).
  P2T_EXPORT Point()
  {
    x = 0.0;
    y = 0.0;
  }

  /// The edges this point constitutes an upper ending point
  std::vector<Edge*> edge_list;

  /// Construct using coordinates.
  P2T_EXPORT Point(double x_, double y_) : x(x_), y(y_) {}

  /// Set this point to all zeros.
  P2T_EXPORT void set_zero()
  {
    x = 0.0;
    y = 0.0;
  }

  /// Set this point to some specified coordinates.
  P2T_EXPORT void set(double x_, double y_)
  {
    x = x_;
    y = y_;
  }

  /// Negate this point.
  P2T_EXPORT Point operator -() const
  {
    Point v;
    v.set(-x, -y);
    return v;
  }

  /// Add a point to this point.
  P2T_EXPORT void operator +=(const Point& v)
  {
    x += v.x;
    y += v.y;
  }

  /// Subtract a point from this point.
  P2T_EXPORT void operator -=(const Point& v)
  {
    x -= v.x;
    y -= v.y;
  }

  /// Multiply this point by a scalar.
  P2T_EXPORT void operator *=(double a)
  {
    x *= a;
    y *= a;
  }

  /// Get the length of this point (the norm).
  P2T_EXPORT double Length() const
  {
    return sqrt(x * x + y * y);
  }

  /// Convert this point into a unit point. Returns the Length.
  P2T_EXPORT double Normalize()
  {
    double len = Length();
    x /= len;
    y /= len;
    return len;
  }

};

// Represents a simple polygon's edge
struct Edge {

  Point* p, *q;

  /// Constructor
  P2T_EXPORT Edge(Point& p1, Point& p2) : p(&p1), q(&p2)
  {
    if (p1.y > p2.y) {
      q = &p1;
      p = &p2;
    } else if (EQ(p1.y, p2.y)) {
      if (p1.x > p2.x) {
        q = &p1;
        p = &p2;
      } else if (EQ(p1.x, p2.x)) {
        // Repeat points
        assert(false);
      }
    }

    q->edge_list.push_back(this);
  }
};

// Triangle-based data structures are know to have better performance than quad-edge structures
// See: J. Shewchuk, "Triangle: Engineering a 2D Quality Mesh Generator and Delaunay Triangulator"
//      "Triangulations in CGAL"
class Triangle {
public:

/// Constructor
P2T_EXPORT Triangle(Point& a, Point& b, Point& c);

/// Flags to determine if an edge is a Constrained edge
bool constrained_edge[3];
/// Flags to determine if an edge is a Delauney edge
bool delaunay_edge[3];

P2T_EXPORT Point* GetPoint(const int& index);
P2T_EXPORT Point* PointCW(Point& point);
P2T_EXPORT Point* PointCCW(Point& point);
P2T_EXPORT Point* OppositePoint(Triangle& t, Point& p);

P2T_EXPORT Triangle* GetNeighbor(const int& index);
P2T_EXPORT void MarkNeighbor(Point* p1, Point* p2, Triangle* t);
P2T_EXPORT void MarkNeighbor(Triangle& t);

P2T_EXPORT void MarkConstrainedEdge(const int index);
P2T_EXPORT void MarkConstrainedEdge(Edge& edge);
P2T_EXPORT void MarkConstrainedEdge(Point* p, Point* q);

P2T_EXPORT int Index(const Point* p);
P2T_EXPORT int EdgeIndex(const Point* p1, const Point* p2);

P2T_EXPORT Triangle* NeighborCW(Point& point);
P2T_EXPORT Triangle* NeighborCCW(Point& point);
P2T_EXPORT Triangle* NeighborAcross(Point& opoint);
P2T_EXPORT bool GetConstrainedEdgeCCW(Point& p);
P2T_EXPORT bool GetConstrainedEdgeCW(Point& p);
P2T_EXPORT void SetConstrainedEdgeCCW(Point& p, bool ce);
P2T_EXPORT void SetConstrainedEdgeCW(Point& p, bool ce);
P2T_EXPORT bool GetDelunayEdgeCCW(Point& p);
P2T_EXPORT bool GetDelunayEdgeCW(Point& p);
P2T_EXPORT void SetDelunayEdgeCCW(Point& p, bool e);
P2T_EXPORT void SetDelunayEdgeCW(Point& p, bool e);

P2T_EXPORT bool Contains(Point* p);
P2T_EXPORT bool Contains(const Edge& e);
P2T_EXPORT bool Contains(Point* p, Point* q);
P2T_EXPORT void Legalize(Point& point);
P2T_EXPORT void Legalize(Point& opoint, Point& npoint);
/**
 * Clears all references to all other triangles and points
 */
P2T_EXPORT void Clear();
P2T_EXPORT void ClearNeighbor(Triangle *triangle );
P2T_EXPORT void ClearNeighbors();
P2T_EXPORT void ClearDelunayEdges();

P2T_EXPORT inline bool IsInterior();
P2T_EXPORT inline void IsInterior(bool b);

P2T_EXPORT inline bool IsChecked();
P2T_EXPORT inline void IsChecked(bool b);

P2T_EXPORT void DebugPrint();

private:

/// Triangle points
Point* points_[3];
/// Neighbor list
Triangle* neighbors_[3];

/// Has this triangle been marked as an interior triangle?
bool interior_;
/// Has this triangle been checked as an interior triangle?
bool checked_;
};

inline bool cmp(const Point* a, const Point* b)
{
  if (a->y < b->y) {
    return true;
  } else if (EQ(a->y, b->y)) {
    // Make sure q is point with greater x value
    if (a->x < b->x) {
      return true;
    }
  }
  return false;
}

/// Add two points_ component-wise.
inline Point operator +(const Point& a, const Point& b)
{
  return Point(a.x + b.x, a.y + b.y);
}

/// Subtract two points_ component-wise.
inline Point operator -(const Point& a, const Point& b)
{
  return Point(a.x - b.x, a.y - b.y);
}

/// Multiply point by scalar
inline Point operator *(double s, const Point& a)
{
  return Point(s * a.x, s * a.y);
}

inline bool operator ==(const Point& a, const Point& b)
{
  return EQ(a.x, b.x) && EQ(a.y, b.y);
}

inline bool operator !=(const Point& a, const Point& b)
{
  return !(a == b);
}

/// Peform the dot product on two vectors.
inline double Dot(const Point& a, const Point& b)
{
  return a.x * b.x + a.y * b.y;
}

/// Perform the cross product on two vectors. In 2D this produces a scalar.
inline double Cross(const Point& a, const Point& b)
{
  return a.x * b.y - a.y * b.x;
}

/// Perform the cross product on a point and a scalar. In 2D this produces
/// a point.
inline Point Cross(const Point& a, double s)
{
  return Point(s * a.y, -s * a.x);
}

/// Perform the cross product on a scalar and a point. In 2D this produces
/// a point.
inline Point Cross(const double s, const Point& a)
{
  return Point(-s * a.y, s * a.x);
}

inline Point* Triangle::GetPoint(const int& index)
{
  return points_[index];
}

inline Triangle* Triangle::GetNeighbor(const int& index)
{
  return neighbors_[index];
}

inline bool Triangle::Contains(Point* p)
{
  return p == points_[0] || p == points_[1] || p == points_[2];
}

inline bool Triangle::Contains(const Edge& e)
{
  return Contains(e.p) && Contains(e.q);
}

inline bool Triangle::Contains(Point* p, Point* q)
{
  return Contains(p) && Contains(q);
}

inline bool Triangle::IsInterior()
{
  return interior_;
}

inline void Triangle::IsInterior(bool b)
{
  interior_ = b;
}

inline bool Triangle::IsChecked()
{
  return checked_;
}

inline void Triangle::IsChecked(bool b)
{
	checked_ = b;
}

}

#endif

