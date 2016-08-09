/* main.cpp
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
 *  This is the standalone point sampling application.
 *
 */

/* Standard C/C++ libraries */
#include <stdio.h>
#include <vector>

/* BRL-CAD libraries */
#include "bu.h"

/* Custom libraries */
#include "BRLCAD/BrlcadLoader.h"
#include "BRLCAD/DbGeometry.h"
#include "BRLCAD/DbTracer.h"
#include "Sampler/PointSampler.h"

#include <gctcommon.h>  // this has to be behind the brlcad headers!!!

/*============ MAIN FUNCTION ============*/

//******************************************************************************
// NAME    main
//
// INPUTS
//    argc - commandline argument count
//    argv - array of commandline arguments
//
// DESCRIPTION
//    Perform point sampling on BRL-CAD geometry.
//******************************************************************************
int
main (int argc, char **argv)
{
  int i, thread_count = (int) ideal_thread_count();
  float csize = 0.0;
  GLoaderInterface *loader = NULL;
  std::vector<std::string> gnames;

  if (argc < 3)
    bu_exit(1, "Usage: %s model.g cellsize [item0 ...]\n", argv[0]);

  /* get the cube size */
  csize = (float) atof (argv[2]);
  if (csize <= 0.0)
    bu_exit(2, "Cube size (%s) needs to be a float greater than 0.0.", argv[2]);

  if (argc > 3)
    {
      /* user specified piece(s) of geometry */
      for (i=3; i<argc; i++)
	gnames.push_back(argv[i]);
    }
  else
    {
      /* no geometry specified... convert them all */

      /* create loader and start loading -- right now assume BRLCAD */
      loader = new BrlcadLoader(argv[1]);
      loader->startLoader();
      
      if (loader->getStatus())
	{
	  /* get region names */
	  gnames = loader->getGeomNames();
	}
      
      /* cleanup loader */
      delete loader;
      loader = NULL;
    }

  FILE *fp = NULL;
  char fname[100];
  memset (fname, 0, sizeof (char) * 100);
  sprintf (fname, "%s.psamp", argv[1]);
  if (!(fp = fopen (fname, "w")))
    bu_exit(5, "Opening output PSAMP file failed.\n");

  /* print cellsize */
  fprintf (fp, "%.4f\n", csize);

  /* create target & tracers -- right now assume BRLCAD */
  GeomConversion::Interface::INativeGeometry *target = new GeomConversion::BRLCAD::DbGeometry(argv[1]);
  std::vector<GeomConversion::Interface::IGeometryTracer *> tracers;
  for (i=0; i<thread_count; i++)
    tracers.push_back(new GeomConversion::BRLCAD::DbTracer(target, i));

  for (i=0; i<gnames.size(); i++)
    {
#ifdef DEBUG
      fprintf (stdout, "Processing %s...\n", gnames.at(i).c_str());
#endif

      /* create the point sampler, set details, and start running */
      GeomConversion::Sampler::PointSampler *psamp = new GeomConversion::Sampler::PointSampler(target, tracers);
      target->loadGeometry(gnames.at(i), thread_count);
      psamp->setSpacing(csize);
      psamp->prepSample();
      psamp->runSample();
      target->cleanupGeometry();
      
      /* Get points and normals for this geometry */
      std::vector<float> points = psamp->getPoints();
      std::vector<float> normals = psamp->getNormals();
#if SURF_SOL_CAPTURE
      std::vector<std::string> sols = psamp->getSolidNumbers();
      std::vector<int> surfs = psamp->getSurfNumbers();
#endif

      /* cleanup sampler */
      delete psamp;
      psamp = NULL;

      /* output points & normals */
      fprintf (fp, "# %s\n", gnames.at(i).c_str());
      for (int j=0; j<points.size(); j+=3)
	{
	  fprintf (fp, "%.4f %.4f %.4f %.4f %.4f %.4f", 
		   points[j], points[j+1], points[j+2],
		   normals[j], normals[j+1], normals[j+2]);
#if SURF_SOL_CAPTURE
	  fprintf (fp, " %s %d", sols[j/3].c_str(), surfs[j/3]);
#endif

	  fprintf (fp, "\n");
	}
    }

  fclose (fp);
  
  /* cleanup target & tracers */
  while (tracers.size() > 0)
    {
      GeomConversion::Interface::IGeometryTracer *ti = tracers.back();
      tracers.pop_back();
      delete ti;
    }
  delete target;
  target = NULL;

  return 0;
}
