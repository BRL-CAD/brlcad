/* PointSampler.cpp
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
 *  LB
 *
 * Description:
 *  Implements a point sample technique to get (x, y, z) locations on
 *  a piece of geometry.
 */

// System headers
#include <math.h>

// Local headers
#include "PointSampler.h"
#include "Auxiliary/gctcommon.h"

namespace GeomConversion {

  namespace Sampler {
    
#if USE_QT
#define PS_CHECK_CANCEL() (! twd->m_keepGoing)
#else
#define PS_CHECK_CANCEL() (0)
#endif

    //*******************************************************************
    // NAME    ray_results_differ
    //
    // INPUTS
    //    previousRayResults, currentRayResults - the ray results for the rays
    //
    // DESCRIPTION
    //    Check to see if the results of two rays are different.
    //     true = different.
    //*******************************************************************
    bool 
    ray_results_differ(const Interface::ray_results &previousRayResults,
		       const Interface::ray_results &currentRayResults)
    {
      // If the list sizes differ that is a dead give-away
      if (previousRayResults.hitData.size() != currentRayResults.hitData.size())
	return true;
      
      for (unsigned i=0 ; i < previousRayResults.hitData.size() ; i++)
	{
	  const Interface::one_hit &prevHit = previousRayResults.hitData[i];
	  const Interface::one_hit &currHit = currentRayResults.hitData[i];
	  
	  // compare things in relative order of difficulty of the comparison
	  
	  if (prevHit.solidSurface != currHit.solidSurface )
	    return true;
	  
	  if (prevHit.solidName.compare(currHit.solidName))
	    return true;
	}
      
      return false;
    }
    
    //*******************************************************************
    // NAME    sub_sample
    //
    // INPUTS
    //    thisThreadResults - all the ray results for the current thread
    //    twd - pointer to the current thread's thread_worker_data
    //    prevResults - results from the previously shot ray
    //    currResults - results from the current ray
    //    depth, limit - variables to control depth of recursion
    //
    // DESCRIPTION
    //    Function performs sub sampling to determine if/where an edge occurs
    //    between 2 rays.
    //*******************************************************************
    void
    sub_sample (std::vector<Interface::ray_results> &thisThreadResults,
		const PointSamplerWorker *twd,
		const Interface::ray_results &prevResults,
		const Interface::ray_results &currResults,
		const int depth, const int limit)
    {
      if (depth >= limit)
	return;
      
      Interface::trace_data middle_ray;
      TRACE_DATA_INIT(middle_ray);
      
      VADD2(middle_ray.point, currResults.origin, prevResults.origin);
      VSCALE(middle_ray.point, middle_ray.point, 0.5);
      VMOVE(middle_ray.direc, currResults.dir);
      middle_ray.results.x = -1;  // note that this is a sub-sampling ray
      middle_ray.results.y = -1;
      VMOVE(middle_ray.results.origin, middle_ray.point);
      VMOVE(middle_ray.results.dir, middle_ray.direc);
      
      twd->tracer->fireRay(&middle_ray);
      
      bool diffLeft = ray_results_differ(prevResults, middle_ray.results);
      bool diffRight = ray_results_differ(middle_ray.results, currResults);
      unsigned remember = thisThreadResults.size();
      
      if (diffLeft && diffRight)
	{
	  sub_sample(thisThreadResults, twd, 
		     prevResults, middle_ray.results, 
		     depth+1, limit);
	  sub_sample(thisThreadResults, twd, 
		     middle_ray.results, currResults, 
		     depth+1, limit);
	  thisThreadResults.push_back(middle_ray.results);
	}
      else if (diffLeft)
	{
	  sub_sample(thisThreadResults, twd, 
		     prevResults, middle_ray.results, 
		     depth+1, limit);
	  for (unsigned pos=remember; pos < thisThreadResults.size(); pos++)
	    {
	      Interface::ray_results &newerResults = thisThreadResults[pos];
	      if (!ray_results_differ(newerResults, currResults))
		{
		  // somethig to my left also matches right so I am not needed
		  goto rightFound;
		}
	    }
	  thisThreadResults.push_back(middle_ray.results);
	  
	rightFound:
	  ;
	}
      else if (diffRight)
	{
	  sub_sample(thisThreadResults, twd, 
		     middle_ray.results, currResults, 
		     depth+1, limit);
	  for (unsigned pos=remember ; pos < thisThreadResults.size() ; pos++)
	    {
	      Interface::ray_results &newerResults = thisThreadResults[pos];
	      if (!ray_results_differ(newerResults, prevResults))
		{
		  // somethig to my right also matches left so I am not needed
		  goto leftFound;
		}
	    }
	  thisThreadResults.push_back(middle_ray.results);
	leftFound:
	  ;
	}
    }
    
    //*******************************************************************
    // NAME    check_subsample
    //
    // INPUTS
    //    twd - pointer to the current thread's thread_worker_data
    //    j - ???
    //    currResults - results from the current ray
    //    prevResults - results from the previously shot ray
    //    thisThreadResults - all the ray results for the current thread
    //
    // DESCRIPTION
    //    Check whether there is any sub-sampling needed and perform it if so.
    //*******************************************************************
    void
    check_subsample(const PointSamplerWorker *twd,
		    const int j,
		    const Interface::ray_results &currResults,
		    const Interface::ray_results &prevResults,
		    std::vector<Interface::ray_results> &thisThreadResults)
    {
      Interface::ray_results lowerRowRayResults, diagonalRayResults;
      
      RAY_RESULTS_INIT(lowerRowRayResults);
      RAY_RESULTS_INIT(diagonalRayResults);
      
      // look to see if we crossed a physical boundary
      // along the i_axis and should sub-sample
      if (ray_results_differ(prevResults, currResults))
	{
	  sub_sample(thisThreadResults, twd,
		     prevResults, currResults,
		     0, twd->model->sub_division);
	}
      
      // look to see if we crossed a physical boundary
      // along the j_axis and should sub-sample
      if (j == 0)
	return;
      
      // look backwards for matching x,y-1 entry
      for (int prevIdx=thisThreadResults.size()-1; prevIdx >= 0; prevIdx--)
	{
	  if (thisThreadResults.at(prevIdx).x == currResults.x &&
	      thisThreadResults.at(prevIdx).y ==(currResults.y-1))
	    {
	      // found it
	      RAY_RESULTS_COPY(lowerRowRayResults, thisThreadResults[prevIdx]);
	      if (ray_results_differ(lowerRowRayResults, currResults))
		{
		  sub_sample(thisThreadResults, twd,
			     lowerRowRayResults, currResults,
			     0, twd->model->sub_division);
		}
	      
	      // sample diagonally across the "cell"
	      if (ray_results_differ(prevResults, lowerRowRayResults))
		{
		  sub_sample(thisThreadResults, twd,
			     prevResults, lowerRowRayResults,
			     0, twd->model->sub_division);
		}
	      
	      // now go looking back for the x-1,y-1 entry
	      // so we can sample along the other diagonal
	      for (int diagIdx=thisThreadResults.size()-1; diagIdx >= 0; 
		   diagIdx--)
		{
		  if (thisThreadResults.at(diagIdx).x ==(currResults.x-1) &&
		      thisThreadResults.at(diagIdx).y ==(currResults.y-1))
		    {
		      // found it
		      RAY_RESULTS_COPY(diagonalRayResults, 
				       thisThreadResults[diagIdx]);
		      if (ray_results_differ(diagonalRayResults, currResults))
			{
			  sub_sample(thisThreadResults, twd,
				     diagonalRayResults, currResults,
				     0, twd->model->sub_division);
			}
		      
		      break;
		    } // found x-1, y-1
		}
	      break;
	    } // found x, y-1
	}
    }
    
    //*******************************************************************
    // NAME    pointsample_worker
    //
    // INPUTS
    //    td - pointer to PointSamplerWorker
    //
    // DESCRIPTION
    //    Function called to perform work for each thread.
    //*******************************************************************
    void *
    pointsample_worker (void *td)
    {
      PointSamplerWorker *twd = (PointSamplerWorker *) td;
      Interface::trace_data prevRay, currentRay;
      std::vector<Interface::ray_results> thisViewResults;  // the results for the current view
      std::vector<Interface::ray_results> thisThreadResults;  // the total results for this thread
      
      // make sure everythign is clear
      thisThreadResults.clear();
      TRACE_DATA_INIT(prevRay);
      
      /* We shoot rays in the shootAxis direction,
       * we step from bounding box min..max along the i_axis.
       * we step from slabStartLineNumber..slabEndLineNumber along j_axis
       *
       * Conceptually, the bounding box of the object is divided into slabs
       * along the j_axis.  Each thread of computation handles one
       * slab.  Within a slab, the thread shoots rays along the individal
       * lines (rows) stepping from mdl_min[i_axis]..mdl_max[i_axis].
       */
      for (int shootAxis = 0; shootAxis < 3; shootAxis++)
	{
	  if (PS_CHECK_CANCEL()) goto endWorker;
	  
	  // make sure everything is clear
	  thisViewResults.clear();
	  TRACE_DATA_INIT(currentRay);
	  
	  /* set direction vector */
	  int i_axis = (shootAxis + 1) % 3;  // the axis we step between rays
	  int j_axis = (shootAxis + 2) % 3;  // the slab axis
	  
	  currentRay.point[shootAxis] = twd->model->model_min[shootAxis];
	  currentRay.direc[shootAxis] = 1.0;
	  currentRay.direc[i_axis] = 0.0;
	  currentRay.direc[j_axis] = 0.0;
	  
	  // compute the dimensions of the slab along the j_axis dimension
	  float slabBottom = twd->cpuID * twd->model->perCpuSlabWidth[j_axis];
	  float slabTop = (twd->cpuID+1) * twd->model->perCpuSlabWidth[j_axis];
	  
	  int slabStartLineNumber = ceil(slabBottom / twd->model->spacing);
	  int slabEndLineNumber   = ceil(slabTop / twd->model->spacing);
	  
	  for (long j=slabStartLineNumber ; j <= slabEndLineNumber ; j++)
	    {
	      if (PS_CHECK_CANCEL()) goto endWorker;
	      currentRay.point[j_axis] = twd->model->model_min[j_axis] + (j * twd->model->spacing);
	      
	      prevRay.results.hitData.clear();
	      prevRay.results.x = -1;
	      prevRay.results.y = -1;
	      
	      long iAxisSteps = ceil(twd->model->model_span[i_axis] / twd->model->spacing);
	      
	      for (long i=0 ; i <= iAxisSteps ; i++)
		{
		  if (PS_CHECK_CANCEL()) goto endWorker;
		  currentRay.point[i_axis] = twd->model->model_min[i_axis] + (i * twd->model->spacing);
		  
		  currentRay.results.x = i;
		  currentRay.results.y = j;
		  VMOVE(currentRay.results.origin, currentRay.point);
		  VMOVE(currentRay.results.dir, currentRay.direc);
		  currentRay.results.hitData.clear();
		  
		  // fire the ray
		  twd->tracer->fireRay(&currentRay);
		  
		  if (j < slabEndLineNumber || (twd->cpuID+1) == twd->model->total_threads)
		    {
		      // record results
		      thisViewResults.push_back(currentRay.results);
		    }
		  
		  if (twd->model->do_subsample && twd->model->sub_division > 0)
		    {
		      if (i > 0)
			{
			  check_subsample(twd, j, currentRay.results,
					  prevRay.results, thisViewResults);
			}
		    }
		  
		  twd->completedRays++;
		  TRACE_DATA_COPY(prevRay, currentRay);
		}
	    }
	  
	  // copy the view results to the overall results
	  thisThreadResults.insert(thisThreadResults.end(), 
				   thisViewResults.begin(), 
				   thisViewResults.end());
	}
      
      thisViewResults.clear();  // not needed anymore
      
      if (PS_CHECK_CANCEL()) goto endWorker;
      
      // move all points to proper lists
      for (unsigned i = 0; i < thisThreadResults.size(); i++)
	{
	  std::vector<float> *points = &(twd->points);
	  std::vector<float> *normals = &(twd->normals);
	  std::vector<std::string> *solNumber = &(twd->solidNumber);
	  std::vector<long> *surfNumber = &(twd->surfNumber);
	  
	  for (unsigned j = 0; j < thisThreadResults[i].hitData.size(); j++)
	    {
	      Interface::one_hit &hit = thisThreadResults[i].hitData[j];
	      
	      /* save points and normals */
	      for (int a=0; a<3; a++)
		{
		  points->push_back(hit.point[a]);
		  normals->push_back(hit.normal[a]);
		}
	      
	      /* save surface numbers and solid numbers */
	      solNumber->push_back(hit.solidName);
	      surfNumber->push_back(hit.solidSurface);
	    }
	}
      
    endWorker:
      thisThreadResults.clear();
      
      //fprintf (stderr, "Ending CPU %d\n", twd->cpuID);  
      SAMPLER_THREAD_EXIT();
      return NULL;
    }
    
#if USE_QT
    //*******************************************************************
    // NAME    run
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Start the worker.
    //*******************************************************************
    void PointSamplerWorker::run()
    {
      pointsample_worker((void *) this);
      emit finished();
    }
#endif
    
    //*******************************************************************
    // NAME    constructor
    //
    // INPUTS
    //    ti - pointer to a target
    //    tracers - list of tracer objects
    //    threads - list of already instantiated QThreads (Qt only)
    //    parent - parent QObject (Qt only, should be NULL)
    //
    // DESCRIPTION
    //    Construct the object.
    //*******************************************************************
    PointSampler::PointSampler(SAMPLER_CONST_IMP)
      : Interface::ISampler(SAMPLER_CONST_PARAMS)
      , m_spacing(1.0)
      , m_subdivision(4)
    {
      m_points.clear();
      m_normals.clear();
      m_surfNumber.clear();
      m_solidNumber.clear();
    }
    
    //*******************************************************************
    // NAME    destructor
    //
    // DESCRIPTION
    //    Cleanup the object.
    //*******************************************************************
    PointSampler::~PointSampler()
    {
      m_points.clear();
      m_normals.clear();
      m_surfNumber.clear();
      m_solidNumber.clear();
    }
    
    //*******************************************************************
    // NAME    prepSample
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Preps data used across the entire model.
    //*******************************************************************
    void PointSampler::prepSample()
    {
      memset (&m_model, 0, sizeof (struct thread_model_data));
      
      // load geometry and determine overall model data
      VMOVE(m_model.model_min, m_geometry->getGeometryMin());
      VMOVE(m_model.model_max, m_geometry->getGeometryMax());
      VSUB2(m_model.model_span, m_model.model_max, m_model.model_min);
      m_model.total_threads = m_threadCount;
      m_model.sub_division = m_subdivision;
      m_model.do_subsample = true;  // always set to true
      m_model.spacing = m_spacing;
      
      for (int currentAxis = 0; currentAxis < 3; currentAxis++)
	{
	  m_model.perCpuSlabWidth[currentAxis] = m_model.model_span[currentAxis] / m_threadCount;
	  m_model.ray_total += m_model.model_span[currentAxis] / m_spacing;
	}
      
      for (int i=0; i<m_threadCount; i++)
	{
	  PointSamplerWorker *worker = new PointSamplerWorker();
	  
	  // assign tracer object
	  worker->tracer = m_tracers[i];
	  
	  // set pointer to model data
	  worker->model = &m_model;
	  
	  // set completed rays
	  worker->completedRays = 0;
	  
	  // set cpu ID
	  worker->cpuID = i;
	  
	  // make sure output vectors are empty
	  worker->points.clear();
	  worker->normals.clear();
	  worker->surfNumber.clear();
	  worker->solidNumber.clear();
	  
	  m_workers.push_back(worker);
	  
#if USE_QT
	  // move the worker to it's thread
	  worker->moveToThread(m_threads.at(i));
	  
	  // connect signals
	  connect(worker, SIGNAL(finished()),
		  this, SLOT(workerFinished()),
		  Qt::QueuedConnection);
	  connect(this, SIGNAL(startWorkers()),
		  worker, SLOT(run()),
		  Qt::QueuedConnection);
#endif
	}
      
      m_threadsDone = 0;
      m_prepped = true;
    }
    
    //*******************************************************************
    // NAME    runSample
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Does the work to perform the point sampling, including subdividing
    //    the work amongst a desired number of threads.
    //*******************************************************************
    void PointSampler::runSample()
    {
      if (m_prepped)
	{
#if USE_QT
	  emit startWorkers();
#else
	  // set up threading data
	  pthread_attr_t attr;
	  pthread_t *ptid = NULL;
	  
	  pthread_attr_init (&attr);
	  ptid = (pthread_t *) malloc (sizeof (pthread_t) * m_threadCount);
	  memset (ptid, 0, sizeof (pthread_t) * m_threadCount);
	  
	  /* start threading */
	  for (int i=0; i<m_threadCount; i++)
	    pthread_create (&ptid[i], &attr, pointsample_worker, (void*) (m_workers.at(i)));
	  
	  /* thread join */
	  for (int i=0; i<m_threadCount; i++)
	    pthread_join(ptid[i], NULL);
	  
	  /* get output data */
	  endSample();
	  
	  if (ptid) free (ptid);
	  pthread_attr_destroy (&attr);
#endif
	}
    }
    
    //*******************************************************************
    // NAME    endSample
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Move all the worker data to the main lists.
    //*******************************************************************
    void PointSampler::endSample()
    {
      /* get output data */
      for (int i=0; i<m_threadCount; i++)
	{
	  //      if (i != 1) continue;
	  
	  PointSamplerWorker *worker = m_workers.at(i);
	  
	  std::vector<float> *points = &(worker->points);
	  std::vector<float> *normals = &(worker->normals);
	  
	  m_points.insert(m_points.end(), points->begin(), points->end());
	  m_normals.insert(m_normals.end(), normals->begin(), normals->end());
	  
	  std::vector<std::string> *solNum = &(worker->solidNumber);
	  std::vector<long> *surfNum = &(worker->surfNumber);
	  
	  m_surfNumber.insert(m_surfNumber.end(), surfNum->begin(), surfNum->end());
	  m_solidNumber.insert(m_solidNumber.end(), solNum->begin(), solNum->end());
	  
	  /* cleanup */
	  worker->tracer = NULL;
	  worker->points.clear();
	  worker->normals.clear();
	  worker->surfNumber.clear();
	  worker->solidNumber.clear();
	  delete worker;
	}
      m_workers.clear();
      m_prepped = false;
    }
    
#if USE_QT
    //*******************************************************************
    // NAME    workerFinished
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Returns here when a worker is done it's part.  Once all are done it
    //    does the finishing work.
    //*******************************************************************
    void PointSampler::workerFinished()
    {
      // wait for the threads to finish
      m_threadsDone++;
      updateProgress();
      if (m_threadsDone < m_threadCount)
	return;
      
      if (!m_canceled)
	endSample();
      emit finished();
    }
    
    //*******************************************************************
    // NAME    getProgress
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Return the overall progress of the workers
    //*******************************************************************
    float PointSampler::getProgress()
    {
      int done_rays = 0;
      float progress = 0.0;
      
      for (unsigned i=0; i < m_workers.size(); i++)
	done_rays += (m_workers[i])->completedRays;
      
      if (m_model.ray_total)
	progress = (float) done_rays / (float) m_model.ray_total;
      if( progress > 100.0 )
	progress = 100.0;
      
      return progress;
    }
    
    //*******************************************************************
    // NAME    stopSampling
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Tell all the workers to stop
    //*******************************************************************
    void PointSampler::stopSampling()
    {
      m_canceled = true;
      for (unsigned i=0; i<m_workers.size(); i++)
	{
	  PointSamplerWorker *worker = m_workers.at(i);
	  worker->pleaseStop();
	}
    }
#endif
    
  } // namespace Sampler

}  // namespace GeomConversion
