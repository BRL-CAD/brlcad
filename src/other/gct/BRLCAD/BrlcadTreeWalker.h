/* BrlcadTreeWalker.h
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
 *  08/19/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  This object walks a brlcad heirarchy starting at a particular
 *  directory entry.
 */

#ifndef BRLCADTREEWALKER_H
#define BRLCADTREEWALKER_H

/* Standard libraries */
#include <vector>

/* BRL-CAD libraries */
extern "C" {
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "bu.h"
#include "raytrace.h"
#include "rt/db4.h"
}

/* Interface headers */
#include "Interface/AppWalkerInterface.h"

/** enumeration of all the types of operators that might be on the
 * heirarchy of the graph that the BrlcadTreeWalker maintains
 *
 *  Shared by the BrlcadTreeWalker and BrlcadBotLoader
 */
enum treeFlags { FLAG_NONE=0,
                 FLAG_MATRIX=1,
                 FLAG_SUBTRACT=2,
                 FLAG_INTERSECT=4,
                 FLAG_UNION=8,
                 FLAG_NOT=16,
                 FLAG_XOR=32,
                 FLAG_LEAF=64
};

/**
 *  HELPER CLASS: StackNode
 *
 *  This encapsulates a single item on the directed acyclic graph of a
 *  BRLCAD heirarchy.  The stack is maintained by the BrlcadTreeWalker to
 *  keep track of various states along the path.
 *
 *  XXX This really should be private to the BrlcadTreeWalker.  No-one else has
 *  any business looking at this.
 */
class StackNode {
 public:
  enum ItemType { OBJECT, OPERATION, MATRIX };
  
 StackNode(struct directory *dp) : m_itemType(OBJECT) { m_dp = dp; }
 StackNode(int op) : m_itemType(OPERATION) { m_op = op; }
 StackNode(mat_t m) : m_itemType(MATRIX) { MAT_COPY(m_mat, m); }

  ItemType getType() const { return m_itemType; }
  struct directory *getDirectory() const
  {
    if (m_itemType == OBJECT) {
      return m_dp;
    }
    abort();
    return NULL; // make compiler happy
  }
  int getOperation() const
  {
    if (m_itemType == OPERATION) {
      return m_op;
    }
    abort();
    return 0; // make compiler happy
  }
  fastf_t * getMatrix() const
  {
    if (m_itemType == MATRIX) {
      return (fastf_t *)m_mat;
    }
    abort();
    return NULL; // make compiler happy
  }
  void print(const int depth);

 private:
  ItemType m_itemType;
  struct directory *m_dp;
  int m_op;
  mat_t m_mat;
};

/* Class */
class BrlcadTreeWalker
{
 public:
  BrlcadTreeWalker(struct db_i* dbip,
		   struct resource *resp,
		   AppWalkerInterface &appWalker);
  ~BrlcadTreeWalker();

  /// Start walking the heirarchy at a particular directory entry
  void brlcad_functree(struct directory *);
  int brlcad_get_internal(struct rt_db_internal *intern, 
			  struct directory *dp, 
			  mat_t matrix);
  inline int inRegion() const { return m_inRegion; }
  inline const char *regionName() const { return m_topRegionName.c_str(); }
  inline int getFlags() const { if (m_treeFlags.size() > 0) return m_treeFlags.back(); else return 0; }
  
 private:
  // This is a helper function for brlcad_functree which
  // handles walking the union tree structure for a single combination
  void brlcad_functree_subtree(union tree *, struct directory *);
  static inline int op2flag(int op)
  {
    switch (op) {
    case OP_UNION : return FLAG_UNION;
    case OP_SUBTRACT : return FLAG_SUBTRACT;
    case OP_INTERSECT : return FLAG_INTERSECT;
    case OP_XOR : return FLAG_XOR;
    }
    return 0;
  }
  
  struct db_i *m_dbip;
  struct resource *m_resp;
  int m_inRegion;
  std::string m_topRegionName;
  AppWalkerInterface &m_appWalker;
  std::vector<StackNode>m_objStack;
  std::vector<int> m_treeFlags;
};

#endif
