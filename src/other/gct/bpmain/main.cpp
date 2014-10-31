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
 *  This is the standalone ball pivoting application.
 *
 */

/* Standard C/C++ libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

/* BRL-CAD libraries */
/* needed to set uint32_t in ballpivoting.h */
#include "bu.h"

/* Custom libraries */
#include "gctcommon.h"
#include "BallPivoting/ballpivoting.h"

/* defines */
#define DEBUG_BALL_PIVOTING_VERBOSE   0

/* structures */
typedef struct
{
  float *vertex;
  float *normal;
} BallPivotingPack;

/* globals */
std::vector<float> gVertex;
std::vector<float> gNormal;
std::vector<long> gVIndex;
std::vector<std::string> gGeom;
std::vector<long> gOffset;

/*============ STATIC FUNCTIONS ============*/

//******************************************************************************
// NAME    ballpivoting_pack_callback
//
// INPUTS
//    ???
//
// DESCRIPTION
//    Callback from ball pivoting code to repack vertices.
//******************************************************************************
static void 
ballpivoting_pack_callback( void *opaquepointer,
			    long newvertexindex,
			    long oldvertexindex )
{
  BallPivotingPack *bppack;
  float *src, *dst;
  
  bppack = (BallPivotingPack *)opaquepointer;
  src = &bppack->vertex[3*oldvertexindex];
  dst = &bppack->vertex[3*newvertexindex];
  memcpy( dst, src, 3*sizeof(float) );
  
  src = &bppack->normal[3*oldvertexindex];
  dst = &bppack->normal[3*newvertexindex];
  memcpy( dst, src, 3*sizeof(float) );
  
  return;
}

//******************************************************************************
// NAME    ball_pivot
//
// INPUTS
//    points - list of 3D points
//    normals - list of normals for the points
//    ballrange - ball range ratio
//    spacing - cell spacing that was used to get points
//
// DESCRIPTION
//    Function that actually performs the ball pivoting routines.
//******************************************************************************
static void
ball_pivot(std::vector<float> &points,
	   std::vector<float> &normals,
	   float ballrange,
	   float spacing)
{
  int threadcount;
  int pointcount = points.size() / 3;
  float *vertex = &points[0];
  float *normal = &normals[0];
  bpOperation bp;
  bpOutput output;
  BallPivotingPack bppack;
  int *bpindices;
  int *triindices;
  long bptricount, bpvertexcount;
  
  bpOperationInit( &bp );
  bpOperationSetInput( &bp,
		       pointcount,
		       vertex, 3*sizeof(float),
		       normal, 3*sizeof(float) );
  if( ballrange <= 1.0 )
    bpOperationSetBall( &bp, spacing );
  else
    bpOperationSetBallRange( &bp, spacing, ballrange * spacing );
  bpOperationSetOutput( &bp,
			BP_TRIANGLE_WINDING_CCW,
			3*sizeof(int),
			sizeof(int),
			0, 0, 0 );
  bpOperationNormalAgreement( &bp, 0.5, 0.5 );
  
  /* Set up callback to pack vertices */
  bppack.vertex = vertex;
  bppack.normal = normal;
  bpOperationVertexPackCallback( &bp, ballpivoting_pack_callback, &bppack );
  
  threadcount = (int) ideal_thread_count ();
  
  /*
   * Note from Alexis:
   * It could be a good idea to expose memory consumption limits...
   *   By default, it's set to 50% of system RAM.
   * Indirectly, it acts to reduce the sampling rate when it's excessive for large parts.
   *
   * bp.maxmemorysize = 1*1024*1024*1024;
   *
   * Or, what about an adaptive sampling rate? 0.25mm is great for a truck's
   *   engine, not so much for the hood.
   */
  
#if DEBUG_BALL_PIVOTING_VERBOSE
  fprintf( stderr, "Ball Pivoting Start, %d points, %f spacing, %d threads\n",
	   (int)pointcount, spacing, threadcount );
#endif
  bpindices = (int *)bpConstructMesh( &bp,
				      &output,
				      threadcount,
				      BP_FLAGS_PACK_VERTICES | BP_FLAGS_ALLOW_REDUCTION );
#if DEBUG_BALL_PIVOTING_VERBOSE
  fprintf( stderr, "Ball Pivoting End, %d vertices, %d triangles, %.3f seconds ( %d warnings, %d errors )\n",
	   (int)output.vertexcount, (int)output.trianglecount,
	   (double)output.msecs / 1000.0, output.warningcount,
	   output.errorcount );
#endif

  bptricount = output.trianglecount;
  bpvertexcount = output.vertexcount;
  long offset = gVertex.size()/3;  /* indices need to be offset by the vertices already present */

  for( long i = bpvertexcount, j = 0 ; i ; i--, j += 3 )
    {
      gVertex.push_back(vertex[j+0]);
      gVertex.push_back(vertex[j+1]);
      gVertex.push_back(vertex[j+2]);

      gNormal.push_back(normal[j+0]);
      gNormal.push_back(normal[j+1]);
      gNormal.push_back(normal[j+2]);
    }
  
  triindices = bpindices;
  for( long i = bptricount ; i ; i--, triindices += 3 )
    {
      gVIndex.push_back( triindices[0] + offset );
      gVIndex.push_back( triindices[1] + offset );
      gVIndex.push_back( triindices[2] + offset );
    }

  free( bpindices );
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
//    Perform ball pivoting on a ".psamp" file generated from the
//    pointsampler application.
//******************************************************************************
int
main (int argc, char **argv)
{
  FILE *fp1 = NULL, *fp2 = NULL;
  char buffer[255], ch, gname[100];
  float csize = 0.0;
  float ballrange = 1.0;
  std::vector<float> points, normals;

  if (argc < 3)
    {
      fprintf (stderr, "Usage: %s pointfile objfile [ballrange]\n", argv[0]);
      exit (1);
    }

  if (argc == 4)
    ballrange = (float) atof (argv[3]);

  if (!(fp1 = fopen (argv[1], "r")))
    {
      fprintf (stderr, "Error opening Point file(%s).\n", argv[1]);
      exit(2);
    }
  
  memset (gname, 0, sizeof (gname));

  /* read cellsize */
  memset (buffer, 0, sizeof (buffer));
  fgets (buffer, 255, fp1);
  csize = (float) atof (buffer);

  points.clear();
  normals.clear();

  gVIndex.clear();
  gOffset.clear();
  gGeom.clear();
  gVertex.clear();
  gNormal.clear();
  
  /* read points */
  ch = fgetc (fp1);
  while (!feof(fp1))
    {
      ungetc (ch, fp1);
      memset (buffer, 0, sizeof (buffer));
      fgets (buffer, 255, fp1);

      if (buffer[0] == '#')
	{
	  if (points.size() > 0)
	    {
	      /* set grouping info */
	      gOffset.push_back(gVIndex.size());
	      gGeom.push_back(gname);

#if DEBUG
	      fprintf (stdout, "Ball Pivoting %s...\n", gname);
#endif
	      /* perform ball pivoting */
	      ball_pivot (points, normals, ballrange, csize);
	    }

	  /* empty points & normals */
	  points.clear();
	  normals.clear();

	  /* read geometry name */
	  char *tok = strtok (buffer, " \n\r"); /* eat '#' */
	  tok = strtok (NULL, " \n\r");
	  sprintf (gname, "%s", tok);
	}
      else
	{
	  float px, py, pz;
	  float dx, dy, dz;
	  
	  sscanf (buffer, "%f %f %f %f %f %f",
		  &px, &py, &pz, &dx, &dy, &dz);
	  
	  points.push_back (px);
	  points.push_back (py);
	  points.push_back (pz);
	  
	  normals.push_back (dx);
	  normals.push_back (dy);
	  normals.push_back (dz);
	}

      ch = fgetc (fp1);
      while (ch == '\n' || ch == '\r') ch = fgetc (fp1);
    }

  /* close point file */
  fclose (fp1);  
  fp1 = NULL;

  if (points.size() > 0)
    {
      /* set grouping info */
      gOffset.push_back(gVIndex.size());
      gGeom.push_back(gname);

#if DEBUG
      fprintf (stdout, "Ball Pivoting %s...\n", gname);
#endif
      /* perform ball pivoting */
      ball_pivot (points, normals, ballrange, csize);
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
  output_obj (fp2, gVertex, gNormal, gVIndex, gGeom, gOffset);

  /* close mesh file */
  fclose (fp2);
  fp2 = NULL;

  return 0;
}
