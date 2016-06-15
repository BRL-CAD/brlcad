/* MarchingCubesSampler.h
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
 *  08/22/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Implements a marching cubes technique to create a faceted representation
 *  of a piece of geometry.
 */

#ifndef MARCHINGCUBESSAMPLER_H
#define MARCHINGCUBESSAMPLER_H

// System headers
#include <vector>

// Local headers
#include "Interface/ISampler.h"

namespace GeomConversion {
  
  namespace Sampler {
    
    // Structures
    struct edge_int
    {
      float pt[3];
      float norm[3];
      int   set;
    };

    struct cube_def
    {
      int ll[2], lr[2], ul[2], ur[2];
      double ymin, ymax;

      /* output lists */
      std::vector<float> points;
      std::vector<float> normals;
    };

    struct mc_thread_data
    {
      double cellsize;

      /* intermediate data */
      Interface::trace_data **rayGrid;
      int rowCount, colCount, rayCount;
      int currRow, currCol, doneRays;
#if USE_QT
      QMutex threadMutex;
#else
      pthread_mutex_t threadMutex;
#endif

      std::vector<struct cube_def> cubes;
      int currCube;
    };

    //*******************************************************************
    // NAME    MarchingCubesWorker
    //
    // DESCRIPTION
    //    Holds data used/created on a per thread basis.
    //
    // NOTE
    //    Glorified structure declared as a class for Qt threading model.
    //*******************************************************************
    class MarchingCubesWorker
#if USE_QT
      : public QObject
#endif
    {
#if USE_QT
      Q_OBJECT
#endif
	
    public:
      MarchingCubesWorker(struct mc_thread_data *td
#if USE_QT
			  , QObject *parent = NULL) : QObject(parent) ,
#else
	) :
#endif
      m_threadData(td)
	{
#if USE_QT 
	  m_keepGoing = true;
#endif
	}

      /* thread data */
      unsigned long m_ID;
      struct mc_thread_data *m_threadData;

      /* input data */
      Interface::IGeometryTracer *tracer;

      void incrementStep()
      {
#if USE_QT
	emit doneStep();
#endif
      }

#if USE_QT
      bool m_keepGoing;      
      void pleaseStop() { m_keepGoing = false; }
      
    signals:
      void doneStep();
      void finished();
      
    public slots:
      void run();
#endif
    };

    /**
     *  MarchingCubesSampler
     *
     *  Implements a marching cubes technique to create a faceted representation
     *  of a piece of geometry.
     */
    class MarchingCubesSampler : public Interface::ISampler
    {
#if USE_QT
      Q_OBJECT
#endif
      
    public:
      MarchingCubesSampler(SAMPLER_CONST_DEF);
      ~MarchingCubesSampler();
      
      virtual void setSpacing(float s) { m_threadData.cellsize = s; }
      virtual void prepSample();
      virtual void endSample();
      
      std::vector<float> getVertices() { return m_vertex; }
      std::vector<float> getNormals() { return m_normal; }
      std::vector<int> getIndices() { return m_index; }
      
#if USE_QT
      virtual void stopSampling();
      virtual float getProgress();
      virtual QString getStatusString() { return m_statusStr; }
      
    public slots:
      void incrementStep() { m_doneSteps++; updateProgress(); }
      virtual void workerFinished();
#endif
      virtual void runSample();
      
    private:
      /* threading data */
      std::vector<MarchingCubesWorker *> m_workers;
      struct mc_thread_data m_threadData;

      /* output storage */
      std::vector<float> m_vertex;
      std::vector<float> m_normal;
      std::vector<int> m_index;

#if USE_QT
      long m_doneSteps;
      long m_totalSteps;
      QString m_statusStr;
#endif
    };
    
  } // namespace Sampler
  
}  // namespace GeomConversion

#endif
