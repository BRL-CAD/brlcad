
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   treeView.h
/// \brief  #goblinTreeView class interface

#ifndef _TREE_VIEW_H_
#define _TREE_VIEW_H_

#include "sparseDigraph.h"


/// \brief  Trees to display hierarchical data

class goblinTreeView : public sparseDiGraph
{
public:

    goblinTreeView(TNode,goblinController &) throw();
    ~goblinTreeView() throw();

    bool    HiddenNode(TNode v) const throw(ERRange);

};


#endif



