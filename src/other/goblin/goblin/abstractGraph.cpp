
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractGraph.cpp
/// \brief  #abstractGraph partial class implementation

#include "abstractGraph.h"


abstractGraph::abstractGraph(TNode _n,TArc _m) throw() :
    abstractMixedGraph(_n,_m)
{
    LogEntry(LOG_MEM,"...Abstract graph object allocated");
}


abstractGraph::~abstractGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract graph object disallocated");
}


unsigned long abstractGraph::Allocated() const throw()
{
    return 0;
}


bool abstractGraph::IsUndirected() const throw()
{
    return true;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractGraph.cpp
/// \brief  #abstractGraph partial class implementation

#include "abstractGraph.h"


abstractGraph::abstractGraph(TNode _n,TArc _m) throw() :
    abstractMixedGraph(_n,_m)
{
    LogEntry(LOG_MEM,"...Abstract graph object allocated");
}


abstractGraph::~abstractGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract graph object disallocated");
}


unsigned long abstractGraph::Allocated() const throw()
{
    return 0;
}


bool abstractGraph::IsUndirected() const throw()
{
    return true;
}
