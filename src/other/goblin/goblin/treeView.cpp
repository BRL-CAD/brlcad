
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   treeView.cpp
/// \brief  #goblinTreeView class implementation

#include "treeView.h"

goblinTreeView::goblinTreeView(TNode n,goblinController &thisContext) throw() :
    managedObject(thisContext),
    sparseDiGraph(n,thisContext)
{
    LogEntry(LOG_MEM,"Generating tree view...");
    InitNodeColours();
    SetLayoutParameter(TokLayoutArcVisibilityMode,graphDisplayProxy::ARC_DISPLAY_PREDECESSORS);
    SetLayoutParameter(TokLayoutNodeColourMode,graphDisplayProxy::NODES_FIXED_COLOURS);
    SetLayoutParameter(TokLayoutNodeLabelFormat,"#2");
}


goblinTreeView::~goblinTreeView() throw()
{
    LogEntry(LOG_MEM,"...tree view disallocated");
}


bool goblinTreeView::HiddenNode(TNode v) const throw(ERRange)
{
    return NodeColour(v)==NoNode;
}
