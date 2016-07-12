/* BrlcadTreeWalker.cpp
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

#include "BrlcadTreeWalker.h"

BrlcadTreeWalker::BrlcadTreeWalker(struct db_i* dbip,
				   struct resource *resp,
				   AppWalkerInterface &appWalker)
  : m_dbip(dbip)
  , m_resp(resp)
  , m_inRegion(0)
  , m_appWalker(appWalker)
{
  m_treeFlags.push_back(0);
  RT_CK_DBI(m_dbip);
  if (m_resp)
    {
      RT_CK_RESOURCE(m_resp);
    }

  // XXX This dreck is necessary because the ft_plot() functions expect RTG to be initialized
  if (BU_LIST_FIRST(bu_list, &RTG.rtg_vlfree) == 0)
    {
      char *envflags;
      envflags = getenv("LIBRT_DEBUG");
      if (envflags)
	{
	  if (RTG.debug)
	    bu_log("WARNING: discarding LIBRT_DEBUG value in favor of application specified flags\n");
	  else
	    RTG.debug = strtol(envflags, NULL, 0x10);
        }

      BU_LIST_INIT(&RTG.rtg_vlfree);
    }
}

BrlcadTreeWalker::~BrlcadTreeWalker()
{
}

int BrlcadTreeWalker::brlcad_get_internal(struct rt_db_internal *intern, 
					  struct directory *dp, 
					  matp_t matrix)
{
  return rt_db_get_internal(intern, dp, m_dbip, matrix, m_resp);
}

void BrlcadTreeWalker::brlcad_functree(struct directory *dp)
{
  RT_CK_DBI(m_dbip);
  if (m_resp)
    {
      RT_CK_RESOURCE(m_resp);
    }

    if (!dp)
      {
        return; /* nothing to do */
      }

    m_objStack.push_back(StackNode(dp));

    if (dp->d_flags & RT_DIR_COMB)
      {
        if (dp->d_flags & RT_DIR_REGION)
	  {
            if (m_inRegion == 0)
	      {
                m_topRegionName = dp->d_namep;
	      }
            m_inRegion++;
	  }

        m_appWalker.comb_pre_func(dp, this);
        struct rt_db_internal in;
        struct rt_comb_internal *comb = (struct rt_comb_internal *)NULL;        
	
        if (rt_db_get_internal(&in, dp, m_dbip, /*mat_t*/ NULL, m_resp) < 0)
	  return;

        comb = (struct rt_comb_internal *)in.idb_ptr;
        if (!comb)
	  {
            bu_log("%s:%d null comb pointer for combination called %s",
		   __FILE__, __LINE__, dp->d_namep);
            bu_bomb("");
	  }

        m_treeFlags.push_back(FLAG_NONE);
        brlcad_functree_subtree(comb->tree, dp);
        m_treeFlags.pop_back();

        /* Finally, the combination post process  */
        m_appWalker.comb_post_func(dp, comb, this);

        rt_db_free_internal(&in);

        if (dp->d_flags & RT_DIR_REGION)
	  {
            m_inRegion--;
            if (m_inRegion == 0)
	      m_topRegionName.clear();
	  }
      }
    else if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK)
      {
        m_appWalker.primitive_func(dp, this);
      }
    else if (strcmp("_GLOBAL", dp->d_namep))
      {
        // This is not the _GLOBAL and isn't a solid/primtive or combination
        bu_log("brlcad_functree:  %s is neither COMB nor SOLID?  %d %d\n",
               dp->d_namep, dp->d_major_type, dp->d_minor_type);
      }

    m_objStack.pop_back();
}

/***
 *  The parent argument exists solely to be able to report error status against
 *  a particular object in the BRLCAD database.
 */
void BrlcadTreeWalker::brlcad_functree_subtree(union tree *tp,
                                               struct directory *parent)
{
  if (!tp)
    return;
  
  switch (tp->tr_op)
    {
    case OP_DB_LEAF:
      {
	struct directory *dp;
	
	if (tp->tr_l.tl_mat)
	  {
	    m_objStack.push_back(StackNode(tp->tr_l.tl_mat));
	    m_treeFlags.push_back(m_treeFlags.back()|FLAG_MATRIX);
	  }
	
        if ((dp=db_lookup(m_dbip, tp->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	  {
            break;
	  }

        brlcad_functree(dp);

        if (tp->tr_l.tl_mat)
	  {
            m_treeFlags.pop_back();
            m_objStack.pop_back();
	  }
        break;
      }
    case OP_UNION:
    case OP_INTERSECT:
    case OP_SUBTRACT:
    case OP_XOR:
      m_objStack.push_back(StackNode(tp->tr_op));
      // add union to treeFlags & push new value
      m_treeFlags.push_back(m_treeFlags.back()|FLAG_UNION);
      brlcad_functree_subtree(tp->tr_b.tb_left, parent);
      m_treeFlags.pop_back();

      // add flag to treeFlags & push new value
      m_treeFlags.push_back(m_treeFlags.back()|op2flag(tp->tr_op));
      brlcad_functree_subtree(tp->tr_b.tb_right, parent);
      m_treeFlags.pop_back();

      m_objStack.pop_back();
      break;
    case OP_SOLID:
    case OP_REGION:
    case OP_NOP:
    case OP_NOT:
    case OP_GUARD:
    case OP_XNOP:
    case OP_NMG_TESS:
    case OP_FREE:
      // These types not implemented.
      break;
    default:
      bu_log("%s:%d brlcad_functree_subtree: unrecognized operator %d in \"%s\"\n",
	     __FILE__, __LINE__, tp->tr_op, parent->d_namep);
      bu_bomb("");
    }
}
