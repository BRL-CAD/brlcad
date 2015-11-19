/* ISampler.h
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
 *  10/09/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Interface class that specifies what is necessary to be a sampler.
 */

#ifndef ISAMPLER_H
#define ISAMPLER_H

// System headers
#include <vector>

// Qt headers
#if defined(COMPILE_FOR_VSL)
#define USE_QT 1
#else
#define USE_QT 0
#endif
#if USE_QT
#include <QObject>
#include <QList>
#include <QThread>
#include <QMutex>
#else
/* Pthread libraries */
#include <pthread.h>
#if defined (_GNU_SOURCE)
#include <sys/sysinfo.h>
#endif
#endif

// Local headers
#include "IGeometryTracer.h"

// Constructor Parameters are "defined" here (based on
//  using Qt or not) so it's more readable later.
// Also, preprocessor for other things
#if USE_QT
#define SAMPLER_CONST_DEF  Interface::INativeGeometry *ng, \
      std::vector<Interface::IGeometryTracer *> &tracers, \
      QList<QThread *> &threads, QObject *parent = NULL
#define SAMPLER_CONST_IMP  Interface::INativeGeometry *ng, \
      std::vector<Interface::IGeometryTracer *> &tracers, \
      QList<QThread *> &threads, QObject *parent
#define SAMPLER_CONST_PARAMS  ng, tracers, threads, parent
#define SAMPLER_CONST_INITS   QObject(parent), \
    m_threads(threads), \
    m_canceled(false)
#define SAMPLER_THREAD_EXIT() { ; }
#else
#define SAMPLER_CONST_DEF  Interface::INativeGeometry *ng, \
      std::vector<Interface::IGeometryTracer *> &tracers
#define SAMPLER_CONST_IMP  Interface::INativeGeometry *ng, \
      std::vector<Interface::IGeometryTracer *>& tracers
#define SAMPLER_CONST_PARAMS  ng, tracers
#define SAMPLER_THREAD_EXIT() pthread_exit(0);
#endif

namespace GeomConversion {
  
  namespace Interface {
    
    /**
     *  ISampler
     *
     *  Defines an interface that specifies what is necessary to be a sampler.
     */
    class ISampler
#if USE_QT
      : public QObject
#endif
    {
#if USE_QT
      Q_OBJECT
#endif
      
    public:
      ISampler(SAMPLER_CONST_DEF)
	:
#if USE_QT
	SAMPLER_CONST_INITS,
#endif
	m_geometry(ng)
	, m_threadsDone(0)
	, m_tracers(tracers)
	, m_prepped(false)
	{ m_threadCount = tracers.size(); }
      virtual ~ISampler() {;}
      
      virtual void setSpacing(float) = 0;
      virtual void prepSample() = 0;
      virtual void endSample() = 0;
      
#if USE_QT
      virtual void stopSampling() = 0;
      virtual float getProgress() { return 0.0; }
      virtual QString getStatusString() { return QString(); }
      virtual void updateProgress() { emit updateSamplerProgress(); }
      
    signals:
      void updateSamplerProgress();
      void finished();
      void startWorkers();
      
    public slots:
      virtual void workerFinished() = 0;
#endif
      virtual void runSample() = 0;
    
    protected:
#if USE_QT
      QList<QThread *> &m_threads;
      bool m_canceled;
#endif
      
      INativeGeometry *m_geometry;
      int m_threadCount;
      int m_threadsDone;
      std::vector<IGeometryTracer *> &m_tracers;
      bool m_prepped;
    };
    
  }  // namespace Interface

}  // namespace GeomConversion

#endif
