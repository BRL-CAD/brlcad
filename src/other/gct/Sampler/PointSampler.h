/* PointSampler.h
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
 *  08/21/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Implements a point sample technique to get (x, y, z) locations on
 *  a piece of geometry.
 */

#ifndef POINTSAMPLER_H
#define POINTSAMPLER_H

// System headers
#include <vector>

// Local headers
#include "Interface/ISampler.h"

namespace GeomConversion {
  
  namespace Sampler {
    
    // Structures
    
    //*******************************************************************
    // NAME    thread_model_data
    //
    // DESCRIPTION
    //    Holds data used across the entire geometric model.
    //*******************************************************************
    struct thread_model_data
    {
      int total_threads;
      int sub_division;
      bool do_subsample;
      double spacing;
      double model_min[3];
      double model_max[3];
      double model_span[3];
      double perCpuSlabWidth[3];
      int ray_total;
    };
    
    //*******************************************************************
    // NAME    PointSamplerWorker
    //
    // DESCRIPTION
    //    Holds data used/created on a per thread basis.
    //
    // NOTE
    //    Glorified structure declared as a class for Qt threading model.
    //*******************************************************************
    class PointSamplerWorker
#if USE_QT
      : public QObject
#endif
    {
#if USE_QT
      Q_OBJECT
#endif
      
    public:
#if USE_QT
      PointSamplerWorker(QObject *parent = NULL) : QObject(parent) { m_keepGoing = true; }
#endif
      
      /* input data */
      Interface::IGeometryTracer *tracer;
      struct thread_model_data *model;
      int cpuID;
      
      /* intermediate data */
      int completedRays;
      
      /* output data */
      std::vector<float> points;
      std::vector<float> normals;
      std::vector<std::string> solidNumber;
      std::vector<long> surfNumber;
      
#if USE_QT
      bool m_keepGoing;
      
      void pleaseStop() { m_keepGoing = false; }
      
    signals:
      void finished();
      
      public slots:
	void run();
#endif
    };
    
    
    /**
     *  PointSampler
     *
     *  Implements a point sample technique to get (x, y, z) locations on
     *  a piece of geometry.
     */
    class PointSampler : public Interface::ISampler
    {
#if USE_QT
      Q_OBJECT
#endif
      
    public:
      PointSampler(SAMPLER_CONST_DEF);
      ~PointSampler();
      
      void setSubdivisionDepth(int d) { m_subdivision = d; }
      
      virtual void setSpacing(float s) { m_spacing = s; }
      virtual void prepSample();
      virtual void endSample();
      
      std::vector<float> getPoints() { return m_points; }
      std::vector<float> getNormals() { return m_normals; }
      std::vector<std::string> getSolidNumbers() { return m_solidNumber; }
      std::vector<long> getSurfNumbers() { return m_surfNumber; }
      
#if USE_QT
      virtual void stopSampling();
      virtual float getProgress();
      
    public slots:
      virtual void workerFinished();
#endif
      virtual void runSample();
  
    private:
      /* necessary inputs */
      float m_spacing;
      int m_subdivision;
      struct thread_model_data m_model;
      std::vector<PointSamplerWorker *> m_workers;
      
      /* output storage */
      std::vector<float> m_points;
      std::vector<float> m_normals;
      std::vector<std::string> m_solidNumber;
      std::vector<long> m_surfNumber;
    };
    
  } // namespace Sampler
  
}  // namespace GeomConversion

#endif
