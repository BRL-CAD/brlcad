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
 *  08/26/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  This is the standalone mesh decimation application.
 *
 */

/* Standard C/C++ libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

/* Custom libraries */
#include "gctcommon.h"
#include "MeshDecimation/meshdecimation.h"
#include "MeshDecimation/meshoptimizer.h"
#include "MeshDecimation/meshoptimization.h"

#if 0
 #define DECIMATION_FLAGS    MD_ATTRIB_FLAGS_NORMALIZE
#else
 #define DECIMATION_FLAGS    MD_ATTRIB_FLAGS_COMPUTE_NORMALS
#endif
#define OPTLEVEL 3  // 0=none, 1=low, 2=medium, 3=high

/* globals */
std::vector<float> gVertex;
std::vector<float> gNormal;
std::vector<float> gNewVertex;
std::vector<float> gNewNormal;
std::vector<long>  gNewVIndex;
std::vector<std::string> gGeom;
std::vector<long>  gOffset;

/*============ STATIC FUNCTIONS ============*/

//******************************************************************************
// NAME    mesh_decimate
//
// INPUTS
//    points - initial list of non-decimated vertices
//    normals - initial list of non-decimated normals
//    indexes - initial list of non-decimated indices
//    featuresize - the smallest feature size to keep undecimated
//    optlevel - optimization level
//
// DESCRIPTION
//    Function that actually performs the mesh decimation routines.
//******************************************************************************
static void
mesh_decimate(std::vector<float> &points,
	      std::vector<float> &normals,
	      std::vector<int> &indexes,
	      float featuresize,
	      int optlevel)
{
  int threadcount;
  long pointcount = points.size() / 3;
  long tricount = indexes.size() / 3;
  float *vertex = &points[0];
  float *normal = &normals[0];
  int *indices = &indexes[0];
  int *triindices, isize = (int) sizeof(int);
  mdOperation mdop;

  if (points.size() < 1) return;

  threadcount = (int) ideal_thread_count ();

  mdOperationInit(&mdop);
  mdOperationData(&mdop, pointcount, vertex, sizeof(float), 3*sizeof(float), 
		  tricount, indices, isize, 3*isize);
  mdOperationStrength(&mdop, featuresize);
  mdOperationAddAttrib(&mdop, normal, sizeof(float), 3, 3*sizeof(float), DECIMATION_FLAGS);

#if DEBUG_MESH_DECIMATION_VERBOSE
  fprintf( stderr, "Mesh Decimation Start, %d vertices, %d triangles, %f featuresize\n", 
	   (int)pointcount, (int)tricount, featuresize );
#endif
  mdMeshDecimation(&mdop, threadcount, 
		   MD_FLAGS_NORMAL_VERTEX_SPLITTING|MD_FLAGS_TRIANGLE_WINDING_CCW);
#if DEBUG_MESH_DECIMATION_VERBOSE
  fprintf( stderr, "Mesh Decimation End, %d vertices, %d triangles, %.3f seconds\n", 
	   (int)mdop.vertexcount, (int)mdop.tricount, (double)mdop.msecs / 1000.0 );
#endif

  pointcount = mdop.vertexcount;
  tricount = mdop.tricount;

  mesh_optimization (pointcount, tricount, indices, isize, threadcount, optlevel);

  /* indices need to be offset by the vertices already present */
  long offset = gNewVertex.size()/3;
  
  for( long i = pointcount, j = 0 ; i ; i--, j += 3 )
    {
      gNewVertex.push_back(vertex[j+0]);
      gNewVertex.push_back(vertex[j+1]);
      gNewVertex.push_back(vertex[j+2]);
      
      gNewNormal.push_back(normal[j+0]);
      gNewNormal.push_back(normal[j+1]);
      gNewNormal.push_back(normal[j+2]);
    }
  
  triindices = indices;
  for( long i = tricount ; i ; i--, triindices += 3 )
    {
      gNewVIndex.push_back( triindices[0] + offset );
      gNewVIndex.push_back( triindices[1] + offset );
      gNewVIndex.push_back( triindices[2] + offset );
    }
}

/*============ MAIN FUNCTION ============*/

//******************************************************************************
// NAME    main
//
// INPUTS
//    argc - commandline argument count
//    argv - array of commandline arguments
//
// DESCRIPTION
//    Perform mesh decimation on an ".obj" file generated from the
//    ballpivot (Ball Pivoting) or march (Marching Cubes) application.
//******************************************************************************
int
main (int argc, char **argv)
{
  FILE *fp1 = NULL, *fp2 = NULL;
  char buffer[255], ch, gname[100];
  float fsize = 0.25;
  std::vector<int> indexes;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: %s infile outfile [featuresize]\n", argv[0]);
      exit (1);
    }

  if (argc == 4)
    fsize = (float) atof (argv[3]);

  if (!(fp1 = fopen (argv[1], "r")))
    {
      fprintf (stderr, "Error opening infile(%s).\n", argv[1]);
      exit(2);
    }
  
  memset (gname, 0, sizeof (gname));

  long low = 0;
  long high = 0;
  long offset = 0;

  ch = fgetc (fp1);
  while (!feof(fp1))
    {
      ungetc (ch, fp1);
      memset (buffer, 0, sizeof (buffer));
      fgets (buffer, 255, fp1);

      int len = strlen(buffer);
      if (buffer[len-1] == '\n') buffer[len-1] = '\0';

      if (buffer[0] == 'g')
	{
	  if (buffer[1] != '\0')
	    {
	      if (low > 0 && high > 0)
		{
		  std::vector<float> points, normals;

		  /* set grouping info */
		  gOffset.push_back(gNewVIndex.size());
		  gGeom.push_back(gname);

#if DEBUG
		  fprintf (stdout, "Decimating %s...\n", gname);
#endif

		  /* stick current element's points into temp vectors */
		  for (long i = (low -1); i <= (high -1); i++)
		    {
		      long idx = i*3;

		      points.push_back (gVertex[idx+0]);
		      points.push_back (gVertex[idx+1]);
		      points.push_back (gVertex[idx+2]);

		      normals.push_back (gNormal[idx+0]);
		      normals.push_back (gNormal[idx+1]);
		      normals.push_back (gNormal[idx+2]);
		    }

		  /* do the mesh decimation */
		  mesh_decimate(points, normals, indexes, fsize, OPTLEVEL);

		  /* reset indices */
		  offset = high;
		  low = high = -1;    
		}

	      /* grab region name */
	      char *tok = strtok (buffer, " \n\r"); /* eat 'g' */
	      tok = strtok (NULL, " \n\r");
	      sprintf (gname, "%s", tok);

	      indexes.clear();
	    }
	}
      else if (buffer[0] == 'v')
	{
	  if (buffer[1] == ' ') /* vertex */
	    {
	      float x, y, z;
	      sscanf (buffer+2, "%f %f %f", &x, &y, &z);
	      gVertex.push_back (x);
	      gVertex.push_back (y);
	      gVertex.push_back (z);
	    }
	  else if (buffer[1] == 'n') /* normal */
	    {
	      float x, y, z;
	      sscanf (buffer+3, "%f %f %f", &x, &y, &z);
	      gNormal.push_back (x);
	      gNormal.push_back (y);
	      gNormal.push_back (z);
	    }
	}
      else if (buffer[0] == 'f')
	{
	  long idx[3];
	  char *tok = strtok(buffer, " \n");
	  tok = strtok(NULL, " \n"); sscanf(tok, "%ld", &idx[0]);
	  tok = strtok(NULL, " \n"); sscanf(tok, "%ld", &idx[1]);
	  tok = strtok(NULL, " \n"); sscanf(tok, "%ld", &idx[2]);

	  for (int i=0; i<3; i++)
	    {
	      int j = idx[i];
	      if (low < 1) low = j;
	      else if (j < low) low = j;
	      
	      if (high < 1) high = j;
	      else if (j > high) high = j;

	      indexes.push_back (idx[i] - offset - 1);
	    }
	}

      ch = fgetc (fp1);
      while (ch == '\n' || ch == '\r') ch = fgetc (fp1);
    }
  fclose (fp1);
  fp1 = NULL;

  if (low > 0 && high > 0)
    {
      std::vector<float> points, normals;
      
      /* set grouping info */
      gOffset.push_back(gNewVIndex.size());
      gGeom.push_back(gname);
      
#if DEBUG
      fprintf (stdout, "Decimating %s...\n", gname);
#endif
      
      /* stick current element's points into temp vectors */
      for (long i = (low -1); i <= (high -1); i++)
	{
	  long idx = i*3;
	  
	  points.push_back (gVertex[idx+0]);
	  points.push_back (gVertex[idx+1]);
	  points.push_back (gVertex[idx+2]);
	  
	  normals.push_back (gNormal[idx+0]);
	  normals.push_back (gNormal[idx+1]);
	  normals.push_back (gNormal[idx+2]);
	}
      
      /* do the mesh decimation */
      mesh_decimate(points, normals, indexes, fsize, OPTLEVEL);
    }

  /* output obj file */
  if (!(fp2 = fopen (argv[2], "w")))
    {
      fprintf (stderr, "Error opening OBJ file(%s).\n", argv[2]);
      exit(3);
    }

#if DEBUG
  fprintf (stdout, "Outputting OBJ file.\n", gname);
#endif
  /* output OBJ file */
  output_obj (fp2, gNewVertex, gNewNormal, gNewVIndex, gGeom, gOffset);

  /* close mesh file */
  fclose (fp2);
  fp2 = NULL;

  return 0;
}
