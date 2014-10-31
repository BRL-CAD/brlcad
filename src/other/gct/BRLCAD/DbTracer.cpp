/* DbTracer.cpp
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
 *  Implements the IGeometryTracer interface for shotlining BRL-CAD targets.
 */

// Local headers
#include "DbTracer.h"

namespace GeomConversion {

  namespace BRLCAD {

    //*******************************************************************
    // NAME    hit_func
    //
    // INPUTS
    //    ap - application structure
    //    PartHeadp - pointer to the beginning of the trace list
    //
    // DESCRIPTION
    //    Called when a shotline produces a hit.  This user defined 
    //    callback copies the in and out hit points into structures and 
    //    stores them in the ray's result list.
    //*******************************************************************
    static int
    hit_func(struct application *ap, struct partition *PartHeadp, struct seg *)
    {
      /* iterating over partitions, this will keep track of the current
       * partition we're working on.
       */
      struct partition *pp;
    
      /* will serve as a pointer for the entry and exit hitpoints */
      struct hit *hitp;
    
      /* will serve as a pointer to the solid primitive we hit */
      struct soltab *stp;
    
      /* output variables */
      Interface::ray_results *result = (Interface::ray_results *) ap->a_uptr;
      Interface::one_hit singleHit;
    
      VMOVE(result->origin, ap->a_ray.r_pt);
      result->x = ap->a_x;
      result->y = ap->a_y;
    
      /* iterate over each partition until we get back to the head.
       * each partition corresponds to a specific homogeneous region of
       * material.
       */
      for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw)
	{
	  /* entry hit point, so we type less */
	  hitp = pp->pt_inhit;
	
	  /* primitive we encountered on entry */
	  stp = pp->pt_inseg->seg_stp;
	
	  /* clear data struct */
	  ONE_HIT_INIT(singleHit);
	
	  /* record IN hit point */
	  VJOIN1(singleHit.point, ap->a_ray.r_pt, 
		 hitp->hit_dist, ap->a_ray.r_dir);
	
	  /* calculate IN normal vector */
	  /* compute the normal vector at the entry point, flipping the
	   * normal if necessary.
	   */
	  RT_HIT_NORMAL(singleHit.normal, hitp, stp, 
			&(ap->a_ray), pp->pt_inflip);
	
	  /* save IN solid and surface numbers */
	  /* singleHit.solidNumber = stp->st_bit; */
	  singleHit.solidName = stp->st_dp->d_namep;
	  singleHit.solidSurface = hitp->hit_surfno;
	
	  /* push hit to the result list */
	  result->hitData.push_back(singleHit);
	
	  /* exit point, so we type less */
	  hitp = pp->pt_outhit;
	
	  /* primitive we exited from */
	  stp = pp->pt_outseg->seg_stp;
	
	  /* clear data struct */
	  ONE_HIT_INIT(singleHit);
	
	  /* record OUT hit point */
	  VJOIN1(singleHit.point, ap->a_ray.r_pt, 
		 hitp->hit_dist, ap->a_ray.r_dir);
	
	  /* calculate OUT normal vector */
	  /* compute the normal vector at the exit point, flipping the
	   * normal if necessary.
	   */
	  RT_HIT_NORMAL(singleHit.normal, hitp, stp, 
			&(ap->a_ray), pp->pt_outflip);
	
	  /* save OUT solid and surface numbers */
	  /* singleHit.solidNumber = stp->st_bit; */
	  singleHit.solidName = stp->st_dp->d_namep;
	  singleHit.solidSurface = hitp->hit_surfno;
	
	  /* push hit to the result list */
	  result->hitData.push_back(singleHit);
	}
    
      return 1;
    }
  
    //*******************************************************************
    // NAME    constructor
    //
    // INPUTS
    //    ti - pointer to a target
    //    cpuID - thread ID
    //
    // DESCRIPTION
    //    Construct the object.
    //*******************************************************************
    DbTracer::DbTracer(Interface::INativeGeometry *ti, int cpuID)
      : Interface::IGeometryTracer(ti)
      , m_cpuID(cpuID)
    {
      // set up the application object that will be used
      RT_APPLICATION_INIT(&m_ap);
      m_ap.a_rt_i = (struct rt_i *) ti->getGeometry();
      m_ap.a_hit = hit_func;
      m_ap.a_resource = ((DbGeometry *) ti)->getResource(cpuID);
      m_ap.a_user = cpuID;
    }
  
    //*******************************************************************
    // NAME    destructor
    //
    // DESCRIPTION
    //    Cleanup the object.
    //*******************************************************************
    DbTracer::~DbTracer()
    {
    }
  
    //*******************************************************************
    // NAME    fireRay
    //
    // INPUTS
    //    data - pointer to a tracer_ray_data
    //
    // DESCRIPTION
    //    Perform a shotline.
    //*******************************************************************
    void DbTracer::fireRay(Interface::trace_data *ray)
    {
      // set application data for this ray
      VMOVE(m_ap.a_ray.r_pt, ray->point);
      VMOVE(m_ap.a_ray.r_dir, ray->direc);
      m_ap.a_ray_length = ray->length;
      m_ap.a_uptr = (void *) &(ray->results);
      m_ap.a_x = ray->results.x;
      m_ap.a_y = ray->results.y;
    
      // perform shot
      (void)rt_shootray(&m_ap);
    }
  
  } // namespace BRLCAD

}  // namespace GeomConversion
