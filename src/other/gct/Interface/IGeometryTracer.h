/* IGeometryTracer.h
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2014 SURVICE Engineering. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ----------------------------------------------------------------------
 *
 * Revision Date:
 *  10/02/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Interface class that specifies necessary functions to define a 
 *  geometry tracer.
 */

#ifndef IGEOMETRYTRACER_H
#define IGEOMETRYTRACER_H

// System headers
#include <string.h>
#include <string>
#include <vector>

// Local headers
#include "INativeGeometry.h"

namespace GeomConversion {

  namespace Interface {
    
    //*******************************************************************
    // NAME    one_hit
    //
    // DESCRIPTION
    //    Holds data used to hold a single hit (in or out) along a ray.
    //*******************************************************************
    struct one_hit
    {
      double point[3];
      double normal[3];
      // long solidNumber;
      std::string solidName;
      long solidSurface;
    };
#define ONE_HIT_INIT(onehit) {			\
      memset(onehit.point, 0, sizeof(double)*3);	\
      memset(onehit.normal, 0, sizeof(double)*3);	\
      onehit.solidName = "";				\
      onehit.solidSurface = 0;				\
    }
    
    //*******************************************************************
    // NAME    ray_result
    //
    // DESCRIPTION
    //    Holds data of all hits along a ray.
    //*******************************************************************
    struct ray_results
    {
      double origin[3];
      double dir[3];
      int x, y;
      std::vector<struct one_hit> hitData;
    };
#define RAY_RESULTS_INIT(rayres) {		\
      memset(rayres.origin, 0, sizeof(double)*3);	\
      memset(rayres.dir, 0, sizeof(double)*3);		\
      rayres.x = rayres.y = 0;				\
      rayres.hitData.clear();				\
    }
#define RAY_RESULTS_COPY(dest, src) {		     \
      RAY_RESULTS_INIT(dest);				\
      memcpy(dest.origin, src.origin, sizeof(double)*3);	\
      memcpy(dest.dir, src.dir, sizeof(double)*3);		\
      dest.x = src.x;							\
      dest.y = src.y;							\
      dest.hitData.insert(dest.hitData.end(), src.hitData.begin(), src.hitData.end()); \
    }
    
    //*******************************************************************
    // NAME    trace_ray_data
    //
    // DESCRIPTION
    //    Holds data info pertaining to a ray, including the results.
    //*******************************************************************
    struct trace_data
    {
      double point[3];
      double direc[3];
      double length;
      struct ray_results results;
    };
#define TRACE_DATA_INIT(trd) {			\
      memset(trd.point, 0, sizeof(double)*3);	\
      memset(trd.direc, 0, sizeof(double)*3);	\
      trd.length = 0;				\
      RAY_RESULTS_INIT(trd.results);		\
    }
#define TRACE_DATA_COPY(dest, src) {			\
      memcpy(dest.point, src.point, sizeof(double)*3);	\
      memcpy(dest.direc, src.direc, sizeof(double)*3);	\
      dest.length = src.length;				\
      RAY_RESULTS_COPY(dest.results, src.results);	\
    }
    
    /**
     *  IGeometryTracer
     *
     *  Defines an interface that specifies necessary functions to define a 
     *  geometry tracer.
     */
    class IGeometryTracer
    {
    public:
      IGeometryTracer(INativeGeometry *ng)
	: m_geometry(ng) { ; }
      virtual ~IGeometryTracer() { ; }
      
      /* User must define how to shoot a ray */
      virtual void fireRay(struct trace_data*) = 0;
      
    protected:
      INativeGeometry *m_geometry;  /* pointer to a geometry */
    };
    
  }  // namespace Interface

}  // namespace GeomConversion

#endif
