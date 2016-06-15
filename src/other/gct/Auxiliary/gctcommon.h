/* gctcommon.h
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
 *  10/03/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Any common use functions between all the tools.
 *
 */

#ifndef GCTCOMMON_H
#define GCTCOMMON_H

/* Standard C/C++ libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#ifndef COMPILE_FOR_VSL
#include <ctype.h>
#include <algorithm>
#endif

/* Pthread libraries */
#include <pthread.h>
#if defined (_GNU_SOURCE)
#include <sys/sysinfo.h>
#endif

#ifndef COMPILE_FOR_VSL
extern "C" {
#include "common.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
}
#endif

#ifndef SQ
#define SQ(a) (a)*(a)
#endif

/* Stuff defined in BRL-CAD but might not be for others */
#ifndef X
#define X 0
#endif
#ifndef Y
#define Y 1
#endif
#ifndef Z
#define Z 2
#endif

#ifndef VINIT
#define VINIT(a,s) (a)[X] = (a)[Y] = (a)[Z] = (s)
#endif

#ifndef VSET
#define VSET(a,b,c,d) { \
    (a)[X] = (b); \
    (a)[Y] = (c); \
    (a)[Z] = (d); }
#endif

#ifndef VADD2
#define VADD2(a, b, c) { \
    (a)[X] = (b)[X] + (c)[X]; \
    (a)[Y] = (b)[Y] + (c)[Y]; \
    (a)[Z] = (b)[Z] + (c)[Z]; \
}
#endif

#ifndef VEQUAL
#define VEQUAL(a, b)	((a)[X]==(b)[X] && (a)[Y]==(b)[Y] && (a)[Z]==(b)[Z])
#endif

#ifndef VMOVE
#define VMOVE(a, b) { \
    (a)[X] = (b)[X]; \
    (a)[Y] = (b)[Y]; \
    (a)[Z] = (b)[Z]; \
}
#endif

#ifndef VSCALE
#define VSCALE(a, b, c) { \
    (a)[X] = (b)[X] * (c); \
    (a)[Y] = (b)[Y] * (c); \
    (a)[Z] = (b)[Z] * (c); \
}
#endif

#ifndef VSUB2
#define VSUB2(a, b, c) { \
    (a)[X] = (b)[X] - (c)[X]; \
    (a)[Y] = (b)[Y] - (c)[Y]; \
    (a)[Z] = (b)[Z] - (c)[Z]; \
}
#endif

#ifndef VCROSS
#define VCROSS(a,b,c) { \
    (a)[X] = (b)[Y] * (c)[Z] - (b)[Z] * (c)[Y]; \
    (a)[Y] = (b)[Z] * (c)[X] - (b)[X] * (c)[Z]; \
    (a)[Z] = (b)[X] * (c)[Y] - (b)[Y] * (c)[X]; }
#endif

#ifndef VMAGSQ
#define VMAGSQ(a) (SQ((a)[X]) + SQ((a)[Y]) + SQ((a)[Z]))
#endif

#ifndef VMAG
#define VMAG(a) sqrt(VMAGSQ(a))
#endif

#ifndef VUNITIZE
#define VUNITIZE(a) { \
    register double _f = VMAGSQ(a); \
    _f = sqrt(_f); \
    if (!VM_NEAR(_f, 1.0, VM_UNITIZE_TOL)) { \
      if (_f < VM_DIVIDE_TOL) { \
	VINIT((a), 0.0); \
      } else { \
	_f = 1.0/_f; \
	(a)[X] *= _f; \
	(a)[Y] *= _f; \
	(a)[Z] *= _f; \
      } \
    }\
  }
#endif

#ifndef VM_NEAR
#define VM_NEAR(v1,v2,tol) ((((v2) - (tol)) <= (v1)) && ((v1) <= ((v2) + (tol))))
#endif

#ifndef VM_DIVIDE_TOL
#define VM_DIVIDE_TOL (1.0e-20)
#endif

#ifndef VM_UNITIZE_TOL
#define VM_UNITIZE_TOL (1.0e-15)
#endif

#ifndef DIST_PT_PT_SQ
#define DIST_PT_PT_SQ(_a,_b) \
  SQ((_a)[X] - (_b)[X]) + \
  SQ((_a)[Y] - (_b)[Y]) + \
  SQ((_a)[Z] - (_b)[Z])
#endif

#ifndef DIST_PT_PT
#define DIST_PT_PT(_a,_b) sqrt(DIST_PT_PT_SQ(_a, _b))
#endif

//******************************************************************************
// NAME    ideal_thread_count
//
// INPUTS
//    none
//
// DESCRIPTION
//    Determine the ideal number of threads to create for multithreading
//    applications, based on what is available on the target system.
//******************************************************************************
static inline 
unsigned ideal_thread_count()
{
  int count = 0;
#if defined(PTW32_VERSION) || defined(__hpux)
  count = pthread_num_processors_np();
#elif defined(__APPLE__) || defined(__FreeBSD__)
  size_t size=sizeof(count);
  count = (sysctlbyname("hw.ncpu",&count,&size,NULL,0)?0:count);
#elif defined(BOOST_HAS_UNISTD_H) && defined(_SC_NPROCESSORS_ONLN)
  count = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_GNU_SOURCE)
  count = get_nprocs();
#elif defined(WIN32)
  SYSTEM_INFO info={{0}};
  GetSystemInfo(&info);
  count = info.dwNumberOfProcessors;
#endif
  return (unsigned) (count>0)?count:1;
}

#ifndef COMPILE_FOR_VSL
//******************************************************************************
// NAME    output_obj
//
// INPUTS
//    fp - file pointer to write
//    vertex - list of vertices
//    normal - list of normals
//    indices - list of indices that make up triangles
//    gnames - list of geometry names
//    offsets - list of offsets between geometry items
//
// DESCRIPTION
//    Runs the given lists and creates an OBJ file that holds all the data.
//******************************************************************************
static inline 
void output_obj (FILE *fp,
		 std::vector<float> &vertex,
		 std::vector<float> &normal,
		 std::vector<long> &indices,
		 std::vector<std::string> &gnames,
		 std::vector<long> &offsets)
{
  long i;
  
  /* output vertices */
  fprintf (fp, "g\n");
  for (i = 0; i < (long)vertex.size(); i += 3)
    fprintf (fp, "v %.4f %.4f %.4f\n", 
	     vertex[i], vertex[i+1], vertex[i+2]);
  fprintf (fp, "\n# %d vertices\n", (int)vertex.size()/3);
  
  fprintf (fp, "\n# 0 vertex parms\n");
  fprintf (fp, "\n# 0 texture vertices\n");
  fprintf (fp, "\n");
  
  /* output normals */
  fprintf (fp, "g\n");
  for (i = 0; i < (long)normal.size(); i += 3)
    fprintf (fp, "vn %.4f %.4f %.4f\n",
	     normal[i], normal[i+1], normal[i+2]);
  fprintf (fp, "\n# %d normals\n", (int)normal.size()/3);
  fprintf (fp, "\n");
  
  /* out geometries */
  for (i = 0; i < (long)gnames.size(); i++)
    {
      fprintf (fp, "g %s\n", gnames.at(i).c_str());
      fprintf (fp, "s off\n");
      
      long end = (long)indices.size();
      if (i < ((long)gnames.size() -1))
	end = offsets[i+1];
      
      /* add 1 to everything because OBJ indices are 1 based, not 0 */
      for (long j = offsets[i]; j < end; j += 3)
	fprintf (fp, "f %ld//%ld %ld//%ld %ld//%ld\n",
		 (indices[j+0] +1), (indices[j+0] +1),
		 (indices[j+1] +1), (indices[j+1] +1),
		 (indices[j+2] +1), (indices[j+2] +1));
      fprintf (fp, "\n# %ld facets\n", end - offsets[i]);
    }
  fprintf (fp, "\n# %d elements\n", (int)gnames.size());  
}

//******************************************************************************
// NAME    output_brlcad
//
// INPUTS
//    ofile - BRL-CAD database file name
//    vertex - list of vertices
//    normal - list of normals
//    indices - list of indices that make up triangles
//    gnames - list of geometry names
//    offsets - list of offsets between geometry items
//
// DESCRIPTION
//    Runs the given lists and creates a BRLCAD database that holds all the data.
//******************************************************************************
static inline 
void output_brlcad (char *ofile,
		    std::vector<float> &vertex,
		    std::vector<float> &normal,
		    std::vector<long> &indices,
		    std::vector<std::string> &gnames,
		    std::vector<long> &offsets)
{
  struct rt_wdb *db_fp;
  struct wmember wm_hd; /* defined in wdb.h */
  long i;
  unsigned char rgb[3];  /* region color (red) */

  if ((db_fp = wdb_fopen(ofile)) == NULL)
    {
      perror(ofile);
      return;
    }
  
  VSET(rgb, 255, 0, 0);
  mk_id(db_fp, ofile); /* create the database header record */
  
  /* assume everything is in mm at this point */

  /* output components (i.e. Primitives/Regions pairs) */
  for (i = 0; i < (long)gnames.size(); i++)
    {
      char name[255];
      fastf_t *vertices = NULL;
      fastf_t *normals = NULL;
      int *faces = NULL;
      int nverts = 0, nnrmls = 0, nfaces = 0;
      
      std::string gname = gnames.at(i).c_str();
      std::size_t pos = gname.find(".");

      /* remove '.r', if it exists */
      if (pos != std::string::npos)
	gname = gname.substr(0, pos);

      memset (name, 0, sizeof (char) * 255);
      sprintf (name, "%s.s", gname.c_str());

      long end = (long)indices.size();
      if (i < ((long)gnames.size() -1))
	end = offsets[i+1];

      /* allocate array to hold face combinations */
      nfaces = (int) (end - offsets[i]);
      faces = (int *) malloc (sizeof (int) * nfaces);
      memset (faces, 0, sizeof (int) * nfaces);
      
      /* vector to tell if index is already added to vert/nrml lists */
      std::vector<long> already;

      /* add vertices, normals, and indices */
      for (long j = offsets[i]; j < end; j++)
	{
	  long index = indices[j];
	  long myidx = j - offsets[i];

	  if (std::find(already.begin(),
			already.end(), index) == already.end())
	    {
	      vertices = (fastf_t*) realloc ((void *) vertices, sizeof(fastf_t) * (nverts + 3));
	      normals = (fastf_t*) realloc ((void *) normals, sizeof(fastf_t) * (nnrmls + 3));

	      vertices[nverts] = vertex[index*3+0]; ++nverts;
	      vertices[nverts] = vertex[index*3+1]; ++nverts;
	      vertices[nverts] = vertex[index*3+2]; ++nverts;

	      normals[nnrmls] = normal[index*3+0]; ++nnrmls;
	      normals[nnrmls] = normal[index*3+1]; ++nnrmls;
	      normals[nnrmls] = normal[index*3+2]; ++nnrmls;

	      already.push_back(index);
	      
	      faces[myidx] = (nverts / 3) -1;
	    }
	  else
	    {
	      for (long k = 0; k < (long)already.size(); k++)
		if (already.at(k) == index)
		  {
		    faces[myidx] = k;
		    break;
		  }
	    }
	}

      already.clear();

      /* make a BOT primitive */
      mk_bot_w_normals(db_fp, name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
		       (nverts / 3), (nfaces / 3), vertices, faces, 
		       (fastf_t *)NULL, (struct bu_bitv *)NULL,
		       (nnrmls / 3), normals, faces);

      /* Make a region that is the union of this one BOT object.
       * To accomplish this, we need to create a linked list of items
       * that make up the combination.  The wm_hd structure serves as
       * the head of the list items.
       */
      BU_LIST_INIT(&wm_hd.l);

      /* Create a wmember structure for the item that we want
       * in the combination.  The return from mk_addmember is a pointer
       * to the wmember structure.
       */
      (void)mk_addmember(name, &wm_hd.l, NULL, WMOP_UNION);

      /* Create the combination.  In this case we are going to make it
       * a region, hence the is_region flag is set, and we provide shader
       * parameter information.
       */
      memset (name, 0, sizeof (char) * 255);
      sprintf (name, "%s.r", gname.c_str());
      mk_lcomb(db_fp,
	       name,           /* name of db region */
	       &wm_hd,        /* list of elements & boolean operations */
	       1,             /* is_region flag */
	       "plastic",     /* optical shader (just guessed) */
	       "di=.8 sp=.2", /* shader params (again guessing) */
	       rgb,           /* item color */
	       0);            /* inherit (override) flag */

      if (vertices) free (vertices);
      if (normals) free (normals);
      if (faces) free (faces);
    }

  wdb_close(db_fp);
}

//******************************************************************************
// NAME    output_fastgen
//
// INPUTS
//    fp - file pointer to write
//    vertex - list of vertices
//    indices - list of indices that make up triangles
//    gnames - list of geometry names
//    offsets - list of offsets between geometry items
//    modes - list of geometry modes
//    materials - list of geometry materials
//    thicknesses - list of geometry thicknesses
//    positions - list of geometry positions
//
// DESCRIPTION
//    Runs the given lists and creates a FASTGEN file that holds all the data.
//******************************************************************************
static inline 
void output_fastgen (FILE *fp,
		     std::vector<float> &vertex,
		     std::vector<long> &indices,
		     std::vector<std::string> &gnames,
		     std::vector<long> &offsets,
		     std::vector<int> &modes,
		     std::vector<int> &materials,
		     std::vector<float> &thicknesses,
		     std::vector<int> &positions)
{
  long i;
  int cID = 0;

  /* output components (i.e. SECTIONs) */
  for (i = 0; i < (long)gnames.size(); i++)
    {
      std::string gname = gnames.at(i).c_str();
      std::size_t pos = gname.find(".");

      /* pick 1st comp ID as the one to use */
      if (pos != std::string::npos)
	gname = gname.substr(0, pos);

      /* convert name to number */
      if (isdigit (gname.c_str()[0]))
	cID = (int) atoi (gname.c_str());
      else cID++;

      /* print SECTION card */
      fprintf (fp, "%-8s%8d%8d%8d%8d\n", "SECTION", (cID / 1000),
	       (cID % 1000), modes.at(i), 1);

      long end = (long) indices.size();
      if (i < ((long) gnames.size() -1))
	end = offsets[i+1];

      /* print GRID cards */
      for (long j = 0, id = 1; j < (long)vertex.size(); j+=3, id++)
	{
	  float x = vertex[j+0];
	  float y = vertex[j+1];
	  float z = vertex[j+2];

	  fprintf (fp, "%-8s%8d%8s%8.2f%8.2f%8.2f\n", "GRID", (int) id, " ",
		   x, y, z);
	}

      /* print CTRI cards */
      /* add 1 to everything because FASTGEN indices are 1 based, not 0 */
      for (long j = offsets[i], id = 1; j < end; j += 3, id++)
	{
	  fprintf (fp, "%-8s%8d%8d%8ld%8ld%8ld%8s%8.2f%8d\n", "CTRI", (int) id,
		   materials.at(i),
		   indices[j+0] +1,
		   indices[j+1] +1,
		   indices[j+2] +1,
		   " ", thicknesses.at(i), 
		   positions.at(i));
	}
    }

  fprintf (fp, "%-8s\n", "ENDDATA");
}

//******************************************************************************
// NAME    output_vrml
//
// INPUTS
//    fp - file pointer to write
//    vertex - list of vertices
//    normal - list of normals
//    indices - list of indices that make up triangles
//    gnames - list of geometry names
//    offsets - list of offsets between geometry items
//
// DESCRIPTION
//    Runs the given lists and creates a VRML file that holds all the data.
//******************************************************************************
static inline 
void output_vrml (FILE *fp,
		  std::vector<float> &vertex,
		  std::vector<float> &normal,
		  std::vector<long> &indices,
		  std::vector<std::string> &gnames,
		  std::vector<long> &offsets)
{
  long i;
  int cID = 0;

  fprintf (fp, "#VRML V2.0 utf8\n");

  /* output components (i.e. Shapes) */
  for (i = 0; i < (long)gnames.size(); i++)
    {
      std::string gname = gnames.at(i).c_str();
      std::size_t pos = gname.find(".");

      /* pick 1st comp ID as the one to use */
      if (pos != std::string::npos)
	gname = gname.substr(0, pos);

      /* convert name to number */
      if (isdigit (gname.c_str()[0]))
	cID = (int) atoi (gname.c_str());
      else cID++;

      /* print Shape header */
      fprintf (fp, "DEF comp_%d Shape {\n", cID);
      fprintf (fp, "\t# Component_ID: %s\n\n", gname.c_str());

      /* print Geometry header */
      fprintf (fp, "\tgeometry IndexedFaceSet {\n");
      fprintf (fp, "\t\tcoord Coordinate {\n");
      fprintf (fp, "\t\t\tpoint [\n");
      
      long end = (long) indices.size();
      if (i < ((long) gnames.size() -1))
	end = offsets[i+1];

      /* print points */
      for (long j = 0, id = 0; j < (long)vertex.size(); j+=3, id++)
	{
	  float x = vertex[j+0];
	  float y = vertex[j+1];
	  float z = vertex[j+2];

	  fprintf (fp, "\t\t\t\t%.3f %.3f %.3f, # point %d\n", x, y, z, (int) id);
	}

      fprintf (fp, "\t\t\t]\n");  /* close point */
      fprintf (fp, "\t\t}\n"); /* close coord */

      fprintf (fp, "\t\tcoordIndex [\n");
      for (long j = offsets[i]; j < end; j += 3)
	{
	  fprintf (fp, "\t\t\t%ld, %ld, %ld, -1\n",
		   indices[j+0], indices[j+1], indices[j+2]);
	}
      fprintf (fp, "\t\t]\n");  /* close coordIndex */

      fprintf (fp, "\t\tnormalPerVertex FALSE\n");
      fprintf (fp, "\t\tconvex TRUE\n");
      fprintf (fp, "\t\tcreaseAngle 0.5\n");
      fprintf (fp, "\t\tsolid FALSE\n");
      fprintf (fp, "\t}\n");  /* close geometry */
      fprintf (fp, "}\n");  /* close Shape */
    }
}
#endif  // COMPILE_FOR_VSL

#endif
