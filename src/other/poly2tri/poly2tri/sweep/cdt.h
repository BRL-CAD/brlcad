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

#ifndef CDT_H
#define CDT_H

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

#include "advancing_front.h"
#include "sweep_context.h"
#include "sweep.h"

/**
 *
 * @author Mason Green <mason.green@gmail.com>
 *
 */

namespace p2t {

class CDT
{
public:

  /**
   * Constructor - add polyline with non repeating points
   *
   * @param polyline
   */
  P2T_EXPORT CDT();
  P2T_EXPORT CDT(std::vector<Point*> &polyline);

   /**
   * Destructor - clean up memory
   */
  P2T_EXPORT ~CDT();
  
  /**
   * Add outer loop
   *
   * @param polyline
   */
  P2T_EXPORT void AddOuterLoop(std::vector<Point*> &polyline);

  /**
   * Add a hole
   *
   * @param polyline
   */
  P2T_EXPORT void AddHole(std::vector<Point*> &polyline);

  /**
   * Add a steiner point
   *
   * @param point
   */
  P2T_EXPORT void AddPoint(Point* point);

  P2T_EXPORT std::vector<Point*>& GetPoints();

  /**
   * Triangulate - do this AFTER you've added the polyline, holes, and Steiner points
   */
  P2T_EXPORT void Triangulate(bool finalize = true, int num_points = -1);

  /**
   * Get CDT triangles
   */
  P2T_EXPORT std::vector<Triangle*>& GetTriangles();

  /**
   * Get triangle map
   */
  P2T_EXPORT std::list<Triangle*>& GetMap();

  private:

  /**
   * Internals
   */

  SweepContext* sweep_context_;
  Sweep* sweep_;

};

}

#endif

