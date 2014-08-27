/* MarchingCubesSampler.cpp
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

#define MC_DEBUG 0

// System heades
#include <math.h>
#if MC_DEBUG
#include <stdio.h>
#endif
#include <unistd.h>

// Local headers
#include "MarchingCubesSampler.h"
#include "Auxiliary/gctcommon.h"

namespace GeomConversion {
  
  namespace Sampler {

#if USE_QT
#define MC_MUTEX_INIT() { ; }
#define MC_MUTX_DESTROY() { ; }
#define MC_MUTEX_LOCK(mcs) mcs->m_threadData->threadMutex.lock()
#define MC_MUTEX_UNLOCK(mcs) mcs->m_threadData->threadMutex.unlock()
#define MC_CHECK_CANCEL() (! mcs->m_keepGoing)
#define MC_STATUS_STR(str) m_statusStr = QString(str)
#define MC_INCREMENT(s) {			\
      if (s > 1) {				\
        m_doneSteps += s;			\
      }						\
      incrementStep();				\
    }
#else
#define MC_MUTEX_INIT() pthread_mutex_init(&m_threadData.threadMutex, NULL);
#define MC_MUTX_DESTROY() pthread_mutex_destroy(&m_threadData.threadMutex);
#define MC_MUTEX_LOCK(mcs) pthread_mutex_lock(&(mcs->m_threadData->threadMutex))
#define MC_MUTEX_UNLOCK(mcs) pthread_mutex_unlock(&(mcs->m_threadData->threadMutex))
#define MC_CHECK_CANCEL() (0)
#define MC_STATUS_STR(str) { ; }
#define MC_INCREMENT(s) { ; }
#endif
    
    /* constants */
    /* the edge table tells which edges are intersected based on
     * index determined during the vertex check process */
    const int cEdgeTable[256] = {
      0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
      0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
      0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
      0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
      0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
      0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
      0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
      0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
      0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
      0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
      0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
      0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
      0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
      0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
      0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
      0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
      0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
      0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
      0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
      0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
      0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
      0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
      0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
      0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
      0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
      0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
      0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
      0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
      0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
      0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
      0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
      0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
    };
    /* The tri table is the stitching table used to make the facets necessary
     * to represent the surface that passes through the referencing cube. */
    const int cTriTable[256][16] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
      {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
      {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
      {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
      {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
      {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
      {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
      {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
      {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
      {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
      {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
      {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
      {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
      {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
      {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
      {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
      {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
      {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
      {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
      {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
      {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
      {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
      {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
      {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
      {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
      {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
      {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
      {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
      {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
      {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
      {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
      {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
      {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
      {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
      {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
      {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
      {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
      {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
      {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
      {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
      {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
      {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
      {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
      {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
      {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
      {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
      {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
      {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
      {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
      {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
      {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
      {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
      {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
      {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
      {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
      {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
      {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
      {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
      {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
      {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
      {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
      {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
      {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
      {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
      {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
      {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
      {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
      {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
      {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
      {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
      {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
      {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
      {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
      {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
      {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
      {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
      {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
      {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
      {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
      {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
      {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
      {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
      {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
      {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
      {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
      {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
      {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
      {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
      {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
      {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
      {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
      {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
      {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
      {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
      {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
      {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
      {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
      {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
      {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
      {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
      {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
      {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
      {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
      {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
      {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
      {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
      {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
      {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
      {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
      {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
      {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
      {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
      {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
      {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
      {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
      {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
      {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
      {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
      {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
      {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
      {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
      {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
      {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
      {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
      {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
      {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
      {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
      {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
      {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
      {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
      {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
      {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
      {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
      {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
      {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
      {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
      {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
      {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
      {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
      {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
      {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
      {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
      {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
      {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
      {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
      {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
      {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
      {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
      {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
      {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
      {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
      {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
      {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
      {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
      {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
      {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
      {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
      {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
      {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
      {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
      {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
      {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
      {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
      {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
      {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
    };
  
    /* structures */
  
    //*******************************************************************
    // NAME    vertex_struct
    //
    // DESCRIPTION
    //    Structure that marries a point and normal together.
    //*******************************************************************
    struct vertex_struct
    {
      float point[3];
      float normal[3];
    };
    std::vector<struct vertex_struct *>gVerList;  // list of vertices
  
    //*******************************************************************
    // NAME    find_vertex
    //
    // INPUTS
    //    pt - 3D point
    //    nrm - normal for the point
    //
    // DESCRIPTION
    //    Locate the point/normal combo in the vertex list.  If found the index
    //    in the list is returned, otherwise it returns -1.
    //*******************************************************************
    static int
    find_vertex (float *pt, float *nrm)
    {
      if (gVerList.size() > 0)
	{
	  for (unsigned i=0; i<gVerList.size(); i++)
	    {
	      struct vertex_struct *vs = gVerList[i];
	    
	      if (VEQUAL(vs->point, pt) && VEQUAL(vs->normal, nrm))
		return i;
	    }
	}
    
      return -1;
    }
  
    //*******************************************************************
    // NAME    add_vertex
    //
    // INPUTS
    //    x, y, z - 3D point
    //    dx, dy, dz - normal for the point
    //
    // DESCRIPTION
    //    Add a point/normal combo if it does not already exist in the list.
    //    Returns 1 if successful, 0 if memory failed.
    //*******************************************************************
    static short
    add_vertex (float x, float y, float z,
		float dx, float dy, float dz)
    {
      float pt[3];
      float nrm[3];
    
      pt[X]  = x;  pt[Y]  = y;  pt[Z]  = z;
      nrm[X] = dx; nrm[Y] = dy; nrm[Z] = dz;
      if (find_vertex (pt, nrm) > -1)
	return 1;
    
      struct vertex_struct *vs = (struct vertex_struct *) malloc (sizeof (struct vertex_struct));
      if (!vs) return 0;

      memset (vs, 0, sizeof (struct vertex_struct));
      VMOVE(vs->point, pt);
      VMOVE(vs->normal, nrm);
      gVerList.push_back(vs);
      return 1;
    }
  
    //*******************************************************************
    // NAME    segment_check
    //
    // INPUTS
    //    lower, upper - bounds of a cell
    //    inloc, outloc - in/out locations of a trace segment
    //
    // DESCRIPTION
    //    Checks to see what case of the trace vs. the cell you currently have.
    //*******************************************************************
    static int
    segment_check (float lower, float upper, float inloc, float outloc)
    {
      /* Possible cases:
       * 1) Both entry and exit are before the front cube face - no facets.
       * 2) Both entry and exit are behind the back cube face - no facets.
       * 3) The entry is before the front cube face and the exit is behind the back cube face - no facets.
       * 4) The entry is behind the front cube face and the exit is before the back cube face - no facets.
       * 5) The entry is before the front cube face and the exit is before the back cube face - facets.
       * 6) The entry is before the back cube face and the exit is behind the back cube face - facets.
       */
      if (inloc < lower)
	{
	  if (outloc <= lower) return 1;
	
	  if (outloc > upper) return 3;
	
	  return 5;
	}
      else if (inloc > upper)
	return 2;
    
      if (outloc <= upper) return 4;
    
      return 6;
    }
  
    //*******************************************************************
    // NAME    get_cube_index
    //
    // INPUTS
    //    mcs - worker class for the thread
    //    cube - cube being inspected
    //
    // DESCRIPTION
    //    Determines which vertices of the cube are in/out of the geometry and
    //    returns an index into the cEdgeTable to reflect that.
    //*******************************************************************
    static int
    get_cube_index (MarchingCubesWorker *mcs, struct cube_def &cube)
    {
      Interface::ray_results *currResults;
      int cubeindex = 0;
    
#if MC_DEBUG
      fprintf (stderr, "\t\tGetting Cube Index\n");
#endif
      
      for (int i=0; i<4; i++)
	{
	  int cube_in=0;
	
	  /* determine which partition list you need */
	  switch (i)
	    {
	    case 0:
	      currResults = &((mcs->m_threadData->rayGrid[cube.ll[0]][cube.ll[1]]).results);
	      break;
	    case 1:
	      currResults = &((mcs->m_threadData->rayGrid[cube.lr[0]][cube.lr[1]]).results);
	      break;
	    case 2:
	      currResults = &((mcs->m_threadData->rayGrid[cube.ul[0]][cube.ul[1]]).results);
	      break;
	    case 3:
	      currResults = &((mcs->m_threadData->rayGrid[cube.ur[0]][cube.ur[1]]).results);
	      break;
	    }
	
	  if (currResults->hitData.size() > 0)
	    {
	      /* run the partition list for this ray to get the state of the vertices */
#if MC_DEBUG
	      fprintf (stderr, "\t\t\tHit Size: %d\n", (int) currResults->hitData.size());
#endif
	      for (unsigned p = 0; p < currResults->hitData.size(); p+=2)
		{
		  Interface::one_hit &ihit = currResults->hitData[p];
		  Interface::one_hit &ohit = currResults->hitData[p+1];
		  int state;
		
		  /* determine if there is a partition overlap with the cube */
		  state = segment_check (cube.ymin, cube.ymax, (float) ihit.point[Y], 
					 (float) ohit.point[Y]);
#if MC_DEBUG
		  fprintf (stderr, "\t\t\tRAY: %d IN: %g  OUT: %g  LOW: %g  UP: %g\n",
			   i, ihit.point[Y], ohit.point[Y], cube.ymin, cube.ymax);
		  fprintf (stderr, "\t\t\tSTATE -- %d\n", state);
#endif
		
		  if (state == 3 || state == 5 || state == 6)
		    {
		      cube_in = 1;
		    
#if MC_DEBUG
		      if (state == 3)
			fprintf (stderr, "\t\t\tPartition is inside geometry!\n");
		      else
			fprintf (stderr, "\t\t\tFound a partiton intersecting the cube!\n");
#endif
		      /* only cases 5 & 6 require one of the vertices along this ray
		       * to be marked in or out */
		      switch (i)
			{
			case 0:
			  {
			    switch (state)
			      {
			      case 5:
				cubeindex |= 1;
				break;
			      case 6:
				cubeindex |= 8;
				break;
			      }
			  }
			  break;
			case 1:
			  {
			    switch (state)
			      {
			      case 5:
				cubeindex |= 2;
				break;
			      case 6:
				cubeindex |= 4;
				break;
			      }
			  }
			  break;
			case 2:
			  {
			    switch (state)
			      {
			      case 5:
				cubeindex |= 16;
				break;
			      case 6:
				cubeindex |= 128;
				break;
			      }
			  }
			  break;
			case 3:
			  {
			    switch (state)
			      {
			      case 5:
				cubeindex |= 32;
				break;
			      case 6:
				cubeindex |= 64;
				break;
			      }
			  }
			  break;
			}		  
		    }
		
		  /* if you got a segment inside the cube you only want to look at the 1st one */
		  if (cube_in)
		    break;
		}
	    }
	
	  if (currResults->hitData.size() == 0 || cube_in == 0)
	    {
	      /* there were no partitons that intersected this cube for this ray....
	       * mark both the vertices along the ray as out */
#if MC_DEBUG
	      if (currResults->hitData.size() == 0)
		fprintf (stderr, "\t\t\tPointer %d is NULL\n", i);
	      else
		fprintf (stderr, "\t\t\tNo partitions along this line intersect the cube.\n");
#endif
	      switch (i)
		{
		case 0:
		  cubeindex |= 1;
		  cubeindex |= 8;
		  break;
		case 1:
		  cubeindex |= 2;
		  cubeindex |= 4;
		  break;
		case 2:
		  cubeindex |= 16;
		  cubeindex |= 128;
		  break;
		case 3:
		  cubeindex |= 32;
		  cubeindex |= 64;
		  break;
		}
	    }
	}
    
#if MC_DEBUG
      fprintf (stderr, "\t\tReturning Cube Index of %d\n", cubeindex);
#endif
      return cubeindex;
    }
  
    //*******************************************************************
    // NAME    get_edge_intersections
    //
    // INPUTS
    //    mcs - worker class for the thread
    //    cube - cube being inspected
    //    cubeindex - index into the cEdgeTable
    //    edgePts - storage of points/normals along the cube edges
    //
    // DESCRIPTION
    //    Perform shotlining along the cube edges to get where geometry boundaries
    //    are crossed.
    //*******************************************************************
    static void
    get_edge_intersections (MarchingCubesWorker *mcs,
			    struct cube_def &cube,
			    int cubeindex, 
			    struct edge_int *edgePts)
    {
      int i;
      Interface::ray_results edge_hits[12];
      Interface::trace_data rdata;
    
      double xmin = mcs->m_threadData->rayGrid[cube.ll[0]][cube.ll[1]].point[X];
      double ymin = cube.ymin;
      double zmin = mcs->m_threadData->rayGrid[cube.ll[0]][cube.ll[1]].point[Z];
      double xmax = xmin + mcs->m_threadData->cellsize;
      double ymax = cube.ymax;
      double zmax = zmin + mcs->m_threadData->cellsize;

#if MC_DEBUG
      fprintf (stderr, "\t\tEntering Intersections Routine\n");
#endif
    
#define OFFSET 0.125
      TRACE_DATA_INIT(rdata);
      rdata.length = mcs->m_threadData->cellsize * (1 + (4.0 * OFFSET));  // length of cellsize
    
      /* Cube is entirely in/out of the component */
      if (cEdgeTable[cubeindex] == 0)
	return;
    
      /* shoot necessary rays to get intersections */
      if (cEdgeTable[cubeindex] & 1)
	{
	  /* shotline Edge0 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 0\n");
#endif
	  rdata.point[X] = xmin-OFFSET; rdata.point[Y] = ymax; rdata.point[Z] = zmin;
	  rdata.direc[X] = 1.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[0], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 2)
	{
	  /* shotline Edge1 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 1\n");
#endif
	  rdata.point[X] = xmax; rdata.point[Y] = ymin-OFFSET; rdata.point[Z] = zmin;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 1.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[1], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 4)
	{
	  /* shotline Edge2 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 2\n");
#endif
	  rdata.point[X] = xmin-OFFSET; rdata.point[Y] = ymin; rdata.point[Z] = zmin;
	  rdata.direc[X] = 1.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[2], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 8)
	{
	  /* shotline Edge3 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 3\n");
#endif
	  rdata.point[X] = xmin; rdata.point[Y] = ymin-OFFSET; rdata.point[Z] = zmin;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 1.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[3], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 16)
	{
	  /* shotline Edge4 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 4\n");
#endif
	  rdata.point[X] = xmin-OFFSET; rdata.point[Y] = ymax; rdata.point[Z] = zmax;
	  rdata.direc[X] = 1.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[4], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 32)
	{
	  /* shotline Edge5 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 5\n");
#endif
	  rdata.point[X] = xmax; rdata.point[Y] = ymin-OFFSET; rdata.point[Z] = zmax;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 1.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[5], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 64)
	{
	  /* shotline Edge6 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 6\n");
#endif
	  rdata.point[X] = xmin-OFFSET; rdata.point[Y] = ymin; rdata.point[Z] = zmax;
	  rdata.direc[X] = 1.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[6], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 128)
	{
	  /* shotline Edge7 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 7\n");
#endif
	  rdata.point[X] = xmin; rdata.point[Y] = ymin-OFFSET; rdata.point[Z] = zmax;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 1.0; rdata.direc[Z] = 0.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[7], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 256)
	{
	  /* shotline Edge8 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 8\n");
#endif
	  rdata.point[X] = xmin; rdata.point[Y] = ymax; rdata.point[Z] = zmin-OFFSET;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 1.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[8], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 512)
	{
	  /* shotline Edge9 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 9\n");
#endif
	  rdata.point[X] = xmax; rdata.point[Y] = ymax; rdata.point[Z] = zmin-OFFSET;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 1.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[9], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      if (cEdgeTable[cubeindex] & 1024)
	{
	  /* shotline Edge10 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 10\n");
#endif
	  rdata.point[X] = xmax; rdata.point[Y] = ymin; rdata.point[Z] = zmin-OFFSET;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 1.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[10], rdata.results);
	  rdata.results.hitData.clear();
	}      std::vector<int> index;

    
      if (cEdgeTable[cubeindex] & 2048)
	{
	  /* shotline Edge11 */
#if MC_DEBUG
	  fprintf (stderr, "\t\t\tShooting Edge 11\n");
#endif
	  rdata.point[X] = xmin; rdata.point[Y] = ymin; rdata.point[Z] = zmin-OFFSET;
	  rdata.direc[X] = 0.0; rdata.direc[Y] = 0.0; rdata.direc[Z] = 1.0;
	  mcs->tracer->fireRay(&rdata);
	  RAY_RESULTS_COPY (edge_hits[11], rdata.results);
	  rdata.results.hitData.clear();
	}
    
      /* run partitons and get the edge hit locations and normals */
      for (i=0; i<12; i++)
	{
	  float in, out;
	  float low, upp;
	  float off = 0;  /* used to extend the edge of a cube in the event that a hit
			   * is not reported when it should be.  this is mostly likely
			   * due to a hit right on the vertex of a cube. */
	
	  if (edge_hits[i].hitData.size() == 0) continue;
	
	  do
	    {
#if MC_DEBUG
	      fprintf (stderr, "\t\t\t%ld Hits Along Edge\n", edge_hits[i].hitData.size());
#endif
	      for (unsigned p = 0; p < edge_hits[i].hitData.size(); p+=2)
		{
		  Interface::one_hit &ihit = edge_hits[i].hitData[p];
		  Interface::one_hit &ohit = edge_hits[i].hitData[p+1];
		  int state;
		
		  switch (i)
		    {
		    case 0:
		    case 2:
		    case 4:
		    case 6:
		      in = (float) ihit.point[X];
		      out = (float) ohit.point[X];
		    
		      low = xmin;
		      upp = xmax;
		      break;
		    case 1:
		    case 3:
		    case 5:
		    case 7:
		      in = (float) ihit.point[Y];
		      out = (float) ohit.point[Y];
		    
		      low = ymin;
		      upp = ymax;
		      break;
		    case 8:
		    case 9:
		    case 10:
		    case 11:
		      in = (float) ihit.point[Z];
		      out = (float) ohit.point[Z];
		    
		      low = zmin;
		      upp = zmax;
		      break;
		    }
		
		  /* check to see if the segment is inbounds and store point/normal and in/out */
		  state = segment_check (low-off, upp+off, in, out);
#if MC_DEBUG
		  fprintf (stderr, "\t\t\tRAY: %d OFF: %g IN: %g  OUT: %g  LOW: %g  UP: %g\n",
			   i, off, in, out, low-off, upp+off);
		  fprintf (stderr, "\t\t\tSTATE -- %d\n", state);
#endif
		
		  /* only worried about the 1st intersection on an edge */
		  if ((state == 5 || state == 6))
		    {
#if MC_DEBUG
		      fprintf (stderr, "\t\t\tEDGE %d\n", i);
#endif
		      switch (state)
			{
			case 5:
			  for (int j=0; j<3; j++)
			    {
			      edgePts[i].pt[j] = (float) ohit.point[j];
			      edgePts[i].norm[j] = (float) ohit.normal[j];
			    }
			  edgePts[i].set = 1;
#if MC_DEBUG
			  fprintf (stderr,"\t\t\tOUT PT (%g, %g, %g)\n",
				   edgePts[i].pt[X], edgePts[i].pt[Y], edgePts[i].pt[Z]);
			  fprintf (stderr,"\t\t\tOUT NM (%g, %g, %g)\n",
				   edgePts[i].norm[X], edgePts[i].norm[Y], edgePts[i].norm[Z]);
#endif
			  break;
			
			case 6:
			  for (int j=0; j<3; j++)
			    {
			      edgePts[i].pt[j] = (float) ihit.point[j];
			      edgePts[i].norm[j] = (float) ihit.normal[j];
			    }
			  edgePts[i].set = 1;
#if MC_DEBUG
			  fprintf (stderr,"\t\t\tIN PT (%g, %g, %g)\n",
				   edgePts[i].pt[X], edgePts[i].pt[Y], edgePts[i].pt[Z]);
			  fprintf (stderr,"\t\t\tIN NM (%g, %g, %g)\n",
				   edgePts[i].norm[X], edgePts[i].norm[Y], edgePts[i].norm[Z]);
#endif
			  break;
			}
		    
		      break;
		    }
#if MC_DEBUG
		  else if (state == 4)
		    {
		      fprintf (stderr,"\t\t\tIN PT (%g, %g, %g)\n",
			       ihit.point[X], ihit.point[Y], ihit.point[Z]);
		      fprintf (stderr,"\t\t\tOUT PT (%g, %g, %g)\n",
			       ohit.point[X], ohit.point[Y], ohit.point[Z]);
		    }
#endif
		}
	      off += (mcs->m_threadData->cellsize * OFFSET);
	    }
	  while (!edgePts[i].set && off < (mcs->m_threadData->cellsize * OFFSET * 3.0));
	
	  /* cleanup the temporary edge partitions */
	  edge_hits[i].hitData.clear();
	}
  
#if MC_DEBUG
      fprintf (stderr, "\t\tLeaving Intersections Routine\n");
#endif
    }
  
    //*******************************************************************
    // NAME    construct_triangles
    //
    // INPUTS
    //    cube - cube being inspected
    //    cubeindex - index into the cEdgeTable
    //    edgePts - points/normals along the cube edges
    //
    // DESCRIPTION
    //    Use the edgePts and cubeindex to determine and construct the triangles
    //    for the given cube.
    //*******************************************************************
    static void
    construct_triangles (struct cube_def *cube,
			 int cubeindex, 
			 struct edge_int *edgePts)
    {
      /* create triangles */
      for (int i=0; cTriTable[cubeindex][i]!=-1; i+=3)
	{
	  int pt1 = cTriTable[cubeindex][i];
	  int pt2 = cTriTable[cubeindex][i+1];
	  int pt3 = cTriTable[cubeindex][i+2];
	
#if MC_DEBUG
	  if (!edgePts[pt1].set)
	    fprintf (stderr, "PROBLEM: edge point 1 (%d) was not set!\n", pt1);
	  if (!edgePts[pt2].set)
	    fprintf (stderr, "PROBLEM: edge point 2 (%d) was not set!\n", pt2);
	  if (!edgePts[pt3].set)
	    fprintf (stderr, "PROBLEM: edge point 3 (%d) was not set!\n", pt3);
#endif
	
	  /* This is a check in case for some reason that an edge hit
	   * was not set.  It happens, and in that event there will just
	   * be a whole were the triangle(s) belong.  But it's better that
	   * having a triangle edge go arbitraily to the origin. */
	  if (!edgePts[pt1].set) continue;
	  if (!edgePts[pt2].set) continue;
	  if (!edgePts[pt3].set) continue;
	
	  /* check for denerate triangles */
	  if ((edgePts[pt1].pt[X] == edgePts[pt2].pt[X]) &&
	      (edgePts[pt1].pt[Y] == edgePts[pt2].pt[Y]) &&
	      (edgePts[pt1].pt[Z] == edgePts[pt2].pt[Z])) continue;  /* pt1 & pt2 */
	  if ((edgePts[pt1].pt[X] == edgePts[pt3].pt[X]) &&
	      (edgePts[pt1].pt[Y] == edgePts[pt3].pt[Y]) &&
	      (edgePts[pt1].pt[Z] == edgePts[pt3].pt[Z])) continue;  /* pt1 & pt3 */
	  if ((edgePts[pt2].pt[X] == edgePts[pt3].pt[X]) &&
	      (edgePts[pt2].pt[Y] == edgePts[pt3].pt[Y]) &&
	      (edgePts[pt2].pt[Z] == edgePts[pt3].pt[Z])) continue;  /* pt2 & pt3 */
	
	  /* add points and normals to the lists */
	  cube->points.push_back(edgePts[pt1].pt[X]);
	  cube->points.push_back(edgePts[pt1].pt[Y]);
	  cube->points.push_back(edgePts[pt1].pt[Z]);
	
	  cube->normals.push_back(edgePts[pt1].norm[X]);
	  cube->normals.push_back(edgePts[pt1].norm[Y]);
	  cube->normals.push_back(edgePts[pt1].norm[Z]);
	
	  cube->points.push_back(edgePts[pt3].pt[X]);
	  cube->points.push_back(edgePts[pt3].pt[Y]);
	  cube->points.push_back(edgePts[pt3].pt[Z]);
	
	  cube->normals.push_back(edgePts[pt3].norm[X]);
	  cube->normals.push_back(edgePts[pt3].norm[Y]);
	  cube->normals.push_back(edgePts[pt3].norm[Z]);
	
	  cube->points.push_back(edgePts[pt2].pt[X]);
	  cube->points.push_back(edgePts[pt2].pt[Y]);
	  cube->points.push_back(edgePts[pt2].pt[Z]);
	
	  cube->normals.push_back(edgePts[pt2].norm[X]);
	  cube->normals.push_back(edgePts[pt2].norm[Y]);
	  cube->normals.push_back(edgePts[pt2].norm[Z]);
	}
    }
  
    //*******************************************************************
    // NAME    analyze_voxel
    //
    // INPUTS
    //    mcs - worker class for the thread
    //    idx - cube index
    //
    // DESCRIPTION
    //    Perform all the necessary operations on a cube.
    //*******************************************************************
    static void
    analyze_voxel (MarchingCubesWorker *mcs, int idx)
    {
      int cubeindex = 0;
      struct cube_def &cube = mcs->m_threadData->cubes[idx];;
      struct edge_int edge_pts[12];
    
#if MC_DEBUG
      double xmin = mcs->m_threadData->rayGrid[cube.ll[0]][cube.ll[1]].point[X];
      double ymin = cube.ymin;
      double zmin = mcs->m_threadData->rayGrid[cube.ll[0]][cube.ll[1]].point[Z];
      double xmax = xmin + mcs->m_threadData->cellsize;
      double ymax = cube.ymax;
      double zmax = zmin + mcs->m_threadData->cellsize;
      fprintf (stderr, "\tAnalyzing Voxel (%.3f, %.3f, %.3f) -> (%.3f, %.3f, %.3f)\n",
	       xmin, ymin, zmin, xmax, ymax, zmax);
#endif
    
      /* Step 1:  Determine which cube vertices are in/out of the component. */
      cubeindex = get_cube_index (mcs, cube);
      mcs->incrementStep();

      /* Step 2: Based on which vertices are in/out of the component, determine
       *  what other edges you must shotline to get intersections */
      for (int i=0; i<12; i++)
	memset (&(edge_pts[i]), 0, sizeof (struct edge_int));
    
      get_edge_intersections (mcs, cube, cubeindex, edge_pts);
      mcs->incrementStep();
    
      /* Step 3: Construct triangles. */
      construct_triangles (&cube, cubeindex, edge_pts);
      mcs->incrementStep();
    }
  
    //*******************************************************************
    // NAME    process_shotlines
    //
    // INPUTS
    //    mcs - worker class for the thread
    //
    // DESCRIPTION
    //    Process any shotlines in the grid.
    //*******************************************************************
    static int 
    process_shotlines (MarchingCubesWorker *mcs)
    {
      int i, j;

      /* get the index of the next ray */
      MC_MUTEX_LOCK(mcs);
      i = mcs->m_threadData->currRow;  j = mcs->m_threadData->currCol;
      mcs->m_threadData->currRow++;
      if (mcs->m_threadData->currRow >= mcs->m_threadData->rowCount)
	{
	  mcs->m_threadData->currCol++;
	  mcs->m_threadData->currRow = 0;
	}
      MC_MUTEX_UNLOCK(mcs);
      
      if (j < mcs->m_threadData->colCount)
	{
	  while (1)
	    {
	      /* process the ray */
	      mcs->tracer->fireRay(&(mcs->m_threadData->rayGrid[i][j]));
	      mcs->incrementStep();
	      
	      /* get the index of the next ray */
	      MC_MUTEX_LOCK(mcs);
	      
#if MC_DEBUG
	      fprintf (stderr, "THREAD %ld -- process [%d,%d]\n", mcs->m_ID, i, j);
	      if (mcs->m_threadData->rayGrid[i][j].results.hitData.size() > 0)
		fprintf (stderr, "\t hit something\n");
	      else
		fprintf (stderr, "\t hit nothing\n");
#endif
	      
	      mcs->m_threadData->doneRays++;
	      
	      i = mcs->m_threadData->currRow;  j = mcs->m_threadData->currCol;
	      mcs->m_threadData->currRow++;
	      if (mcs->m_threadData->currRow >= mcs->m_threadData->rowCount)
		{
		  mcs->m_threadData->currCol++;
		  mcs->m_threadData->currRow = 0;
		}
	      MC_MUTEX_UNLOCK(mcs);
	      
	      if (j >= mcs->m_threadData->colCount) break;
	      if (MC_CHECK_CANCEL()) break;
	    }
	}

      return 0;
    }
  
    //*******************************************************************
    // NAME    process_cubes
    //
    // INPUTS
    //    mcs - worker class for the thread
    //
    // DESCRIPTION
    //    Process any cubes in the list.
    //*******************************************************************
    static int 
    process_cubes (MarchingCubesWorker *mcs)
    {
      int i;

      /* get the index of the next cube */
      MC_MUTEX_LOCK(mcs);
      i = mcs->m_threadData->currCube;
      mcs->m_threadData->currCube++;
      MC_MUTEX_UNLOCK(mcs);
      
      if ((unsigned long)i < mcs->m_threadData->cubes.size())
	{
	  while (1)
	    {
#if MC_DEBUG
	      struct cube_def cube = mcs->m_threadData->cubes[i];
	      fprintf (stderr, "THREAD %ld -- process cube %d\n", mcs->m_ID, i);
	      fprintf (stderr, "\tLL: [%d,%d]  LR: [%d,%d]  UL: [%d,%d]  UR: [%d,%d]\n", 
		       cube.ll[0], cube.ll[1],
		       cube.lr[0], cube.lr[1],
		       cube.ul[0], cube.ul[1],
		       cube.ur[0], cube.ur[1]);
#endif

	      /* anlyze this cube */
	      analyze_voxel (mcs, i);
	      
	      /* get the index of the next cube */
	      MC_MUTEX_LOCK(mcs);
	      i = mcs->m_threadData->currCube;
	      mcs->m_threadData->currCube++;
	      MC_MUTEX_UNLOCK(mcs);
	      
	      if ((unsigned long)i >= mcs->m_threadData->cubes.size()) break;
	      if (MC_CHECK_CANCEL()) break;
	    }
	}

      return 0;
    }

    //*******************************************************************
    // NAME    marchcubes_worker
    //
    // INPUTS
    //    mcs - MarchingCubesWorker does the work of 1 thread
    //
    // DESCRIPTION
    //    Function called to perform work for each thread.
    //*******************************************************************
    void *
    marchcubes_worker (void *td)
    {
      MarchingCubesWorker *mcs = (MarchingCubesWorker *) td;
    
      if (MC_CHECK_CANCEL()) goto endWorker;
      
      /* Algorithm:
       *  This is needs to happen in 2 phases of parallelism.  Here are
       *  the steps.
       *
       *  1) Shotline grid for the XZ plane.
       *  2) Synchronize threads.
       *  3) Process the cubes.
       *  4) Cleanup shotline memory.
       */

      /* STEP 1: process shotlines */
      process_shotlines(mcs);
      if (MC_CHECK_CANCEL()) goto endWorker;
      
      /* STEP 2: Synchronize threads to make sure ALL rays are processed */
      while (1)
	{
	  if (mcs->m_threadData->doneRays == mcs->m_threadData->rayCount)
	    break;
	  
	  sleep(1);  // sleep for a second
	}
      
#if MC_DEBUG
      MC_MUTEX_LOCK(mcs);
      fprintf (stderr, "THREAD %ld synchronized... \n", mcs->m_ID);
      for (int i = 0; i < mcs->m_threadData->rowCount; i++)
	for (int j = 0; j < mcs->m_threadData->colCount; j++)
	  fprintf (stderr, "[%d][%d]: %ld\n", i, j, mcs->m_threadData->rayGrid[i][j].results.hitData.size());
      MC_MUTEX_UNLOCK(mcs);
#endif
      if (MC_CHECK_CANCEL()) goto endWorker;
      
      /* STEP 3: process the cubes */
      process_cubes(mcs);
      
    endWorker:
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
    void MarchingCubesWorker::run()
    {
      marchcubes_worker((void *) this);
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
    MarchingCubesSampler::MarchingCubesSampler(SAMPLER_CONST_IMP)
      : Interface::ISampler(SAMPLER_CONST_PARAMS)
    {
      m_threadData.cellsize = 1.0;
      m_threadData.rayGrid = NULL;
      m_threadData.rowCount = 0;
      m_threadData.colCount = 0;
      m_threadData.rayCount = 0;
      m_vertex.clear();
      m_normal.clear();
      m_index.clear();
      MC_MUTEX_INIT();
    }
  
    //*******************************************************************
    // NAME    destructor
    //
    // DESCRIPTION
    //    Cleanup the object.
    //*******************************************************************
    MarchingCubesSampler::~MarchingCubesSampler()
    {
      m_vertex.clear();
      m_normal.clear();
      m_index.clear();

      if (m_threadData.rayGrid)
	{
	  for (int i=0; i<m_threadData.rowCount; i++)
	    {
	      for (int j=0; j<m_threadData.colCount; j++)
		RAY_RESULTS_INIT(m_threadData.rayGrid[i][j].results);
	      free (m_threadData.rayGrid[i]);
	    }
	  free (m_threadData.rayGrid);
	}
      MC_MUTX_DESTROY();
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
    void MarchingCubesSampler::prepSample()
    {
      double min[3], span[3];
      double *tmin, *tmax;
      double x, y, z;
    
      // load geometry and determine overall model data
      tmin = m_geometry->getGeometryMin();
      tmax = m_geometry->getGeometryMax();
    
      for (int i=0; i<3; i++)
	{
	  min[i] = tmin[i];
	  span[i] = tmax[i] - tmin[i];
	}

      /* set the 2D grid of rays */
      z = min[Z];
      y = min[Y];
      // add 2 so it covers the 1st row and anything not evenly divisible
      m_threadData.rowCount = (int) (span[Z] / (double) m_threadData.cellsize) + 2;
      m_threadData.colCount = (int) (span[X] / (double) m_threadData.cellsize) + 2;
      m_threadData.rayCount = m_threadData.rowCount * m_threadData.colCount;
      m_threadData.rayGrid = (Interface::trace_data**) malloc (sizeof (Interface::trace_data*) *
							       m_threadData.rowCount);
      if (! m_threadData.rayGrid) return;
      memset (m_threadData.rayGrid, 0, sizeof(Interface::trace_data*) * m_threadData.rowCount);

      for (int i=0; i < m_threadData.rowCount; i++)
	{
	  m_threadData.rayGrid[i] =
	    (Interface::trace_data*) malloc (sizeof (Interface::trace_data) * m_threadData.colCount);
	  if (! m_threadData.rayGrid[i]) return;
	  memset (m_threadData.rayGrid[i], 0, sizeof(Interface::trace_data) * m_threadData.colCount);

	  x = min[X];
	  for (int j=0; j < m_threadData.colCount; j++)
	    {
	      TRACE_DATA_INIT(m_threadData.rayGrid[i][j]);

	      // set ray info here
	      m_threadData.rayGrid[i][j].point[X] = x;
	      m_threadData.rayGrid[i][j].point[Y] = y;
	      m_threadData.rayGrid[i][j].point[Z] = z;

	      m_threadData.rayGrid[i][j].direc[X] = 0.0;
	      m_threadData.rayGrid[i][j].direc[Y] = 1.0;
	      m_threadData.rayGrid[i][j].direc[Z] = 0.0;

	      // increase X
	      x += m_threadData.cellsize;
	    }

	  // increase Z
	  z += m_threadData.cellsize;
	}
      m_threadData.currRow = m_threadData.currCol = m_threadData.doneRays = 0;

      /* create list of cubes */
      int ycount = (int) (span[Y] / (double) m_threadData.cellsize) + 2;
      for (int i=1; i < m_threadData.rowCount; i++)
	{
	  for (int j=1; j < m_threadData.colCount; j++)
	    {
	      y = min[Y];

	      for (int k=0; k < ycount; k++)
		{
		  struct cube_def cube;

		  /* set pointers to ray grid */
		  cube.ll[0] = i-1;  cube.ll[1] = j-1;
		  cube.lr[0] = i-1;  cube.lr[1] = j;
		  cube.ul[0] = i;    cube.ul[1] = j-1;
		  cube.ur[0] = i;    cube.ur[1] = j;

		  /* set Y depth for cube */
		  cube.ymin = y;
		  y += m_threadData.cellsize;
		  cube.ymax = y;

		  cube.points.clear();
		  cube.normals.clear();

		  m_threadData.cubes.push_back(cube);
		}
	    }
	}
      m_threadData.currCube = 0;

      
#if USE_QT
      m_statusStr = QString();
      m_doneSteps = 0;
      m_totalSteps = m_threadData.rayCount;  // # of rays
      m_totalSteps += (m_threadData.cubes.size() * 4); // # of cubes
      m_totalSteps += 4; // steps for creating indexed lists
#endif

      for (int i=0; i<m_threadCount; i++)
	{
	  MarchingCubesWorker *worker = new MarchingCubesWorker(&m_threadData);
	
	  worker->m_ID = (unsigned long) i;

	  /* assign tracer object */
	  worker->tracer = m_tracers[i];

	  m_workers.push_back(worker);
	
#if USE_QT
	  // move the worker to it's thread
	  worker->moveToThread(m_threads.at(i));
	
	  // connect signals
	  connect(worker, SIGNAL(finished()),
		  this, SLOT(workerFinished()),
		  Qt::QueuedConnection);
	  connect(worker, SIGNAL(doneStep()),
          this, SLOT(incrementStep()),
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
    //    Does the work to perform the marching cubes algorithm, including 
    //    subdividing the work amongst a desired number of threads.
    //*******************************************************************
    void MarchingCubesSampler::runSample()
    {
      if (m_prepped)
	{
	  MC_STATUS_STR("Sampling Geometry");
#if USE_QT
	  emit startWorkers();
#else
	  // set up threading data
	  pthread_attr_t attr;
	  pthread_t *ptid = NULL;
	
	  pthread_attr_init (&attr);
	  ptid = (pthread_t *) malloc (sizeof (pthread_t) * m_threadCount);
	  if (! ptid) return;
	  memset (ptid, 0, sizeof (pthread_t) * m_threadCount);
	  
	  /* start the threading */
	  for (int i=0; i<m_threadCount; i++)
	    pthread_create (&ptid[i], &attr, marchcubes_worker, (void*) (m_workers.at(i)));
	  
	  /* thread join */
	  for (int i=0; i<m_threadCount; i++)
	    pthread_join(ptid[i], NULL);
	  
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
    void MarchingCubesSampler::endSample()
    {
      std::vector<float> points;
      std::vector<float> normals;
    
#if MC_DEBUG
      fprintf (stderr, "Entering End Sample\n");
#endif
      MC_STATUS_STR("Transferring Cube Points");
      
      /* worker cleanup */
#if MC_DEBUG
      fprintf (stderr, "\tWorker Cleanup\n");
#endif
      for (int i=0; i<m_threadCount; i++)
	{
	  MarchingCubesWorker *worker = m_workers.at(i);
	  delete worker;
	}
      m_workers.clear();
      m_prepped = false;

      /* get output data */
      for (unsigned long i=0; i<m_threadData.cubes.size(); i++)
	{
	  const std::vector<float> *pts = &(m_threadData.cubes[i]).points;
	  const std::vector<float> *nrms = &(m_threadData.cubes[i]).normals;
      
#if MC_DEBUG
	  fprintf (stderr, "\tTransfering Points & Normals to Local Lists\n");
	  fprintf (stderr, "\t\tCube: %ld -- points: %ld\n", i, pts->size() / 3);
#endif
	  points.insert(points.end(), pts->begin(), pts->end());
	  normals.insert(normals.end(), nrms->begin(), nrms->end());
	
	  /* increment progress bar */
	  MC_INCREMENT(1);
	}
    
      /* create vectors of fused vertices, normals, and indices */
      MC_STATUS_STR("Building Vertex/Normal Lists");
      if (points.size () > 0)
	{
#if MC_DEBUG
	  fprintf (stderr, "\tCreate Index Vertice List\n");
#endif
	  for (unsigned i=0; i<points.size(); i+=3)
	    if (! add_vertex (points[i], points[i+1], points[i+2],
			      normals[i], normals[i+1], normals[i+2]))
	      break;
	  
	  /* increment progress bar */
	  MC_INCREMENT(1);
	  
#if MC_DEBUG
	  fprintf (stderr, "\tTransfering Points & Normals to Class Lists\n");
#endif
	  for (unsigned i=0; i<gVerList.size(); i++)
	    {
	      struct vertex_struct *vs = gVerList[i];
	      m_vertex.push_back (vs->point[X]);
	      m_vertex.push_back (vs->point[Y]);
	      m_vertex.push_back (vs->point[Z]);
	    
	      m_normal.push_back (vs->normal[X]);
	      m_normal.push_back (vs->normal[Y]);
	      m_normal.push_back (vs->normal[Z]);
	    }
	
	  /* increment progress bar */
	  MC_INCREMENT(1);
	  
#if MC_DEBUG
	  fprintf (stderr, "\tCreate Class Index List\n");
#endif
	  MC_STATUS_STR("Building Index List");
	  for (unsigned i=0; i<points.size(); i+=3)
	    {
	      float pt[3], nrm[3];
	      pt[X]  = points[i];  pt[Y]  = points[i+1];  pt[Z]  = points[i+2];
	      nrm[X] = normals[i]; nrm[Y] = normals[i+1]; nrm[Z] = normals[i+2];
	      int index = find_vertex(pt, nrm);
	      if (index > -1)
		m_index.push_back(index);
	    }
	  
	  /* increment progress bar */
	  MC_INCREMENT(1);
	  
#if MC_DEBUG
	  fprintf (stderr, "\tCleanup Temp Vert List\n");
#endif
	  while (gVerList.size())
	    {
	      struct vertex_struct *vs = gVerList.back();
	      gVerList.pop_back();
	      free (vs);
	    }
	  /* increment progress bar */
	  MC_INCREMENT(1);
	}
      else
	{
#if MC_DEBUG
	  fprintf (stderr, "\tPoints Array is emtpy!\n");
#endif
	  MC_INCREMENT(3);
	}
#if MC_DEBUG
      fprintf (stderr, "Leaving End Sample\n");
#endif
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
    void MarchingCubesSampler::workerFinished()
    {
      // wait for the threads to finish
      m_threadsDone++;
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
    float MarchingCubesSampler::getProgress()
    {
      float progress = 0.0;
    
      progress = (((float) m_doneSteps) / ((float) m_totalSteps)) * 100.0;
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
    void MarchingCubesSampler::stopSampling()
    {
      m_canceled = true;
      for (unsigned i=0; i<m_workers.size(); i++)
	{
	  MarchingCubesWorker *worker = m_workers.at(i);
	  worker->pleaseStop();
	}
    }
#endif
  
  }  // namespace Sampler

}  // namespace GeomConversion
