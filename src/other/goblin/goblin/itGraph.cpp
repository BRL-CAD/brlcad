
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   itGraph.cpp
/// \brief  #iGraph class implementation

#include "abstractMixedGraph.h"


iGraph::iGraph(const abstractMixedGraph &GC) throw() :
    managedObject(GC.Context()), G(GC)
{
    G.MakeRef();
    n = G.N();
    current = new TArc[n];

    for (TNode i=0;i<n;i++) current[i] = NoArc;
}


unsigned long iGraph::Size() const throw()
{
    return
          sizeof(iGraph)
        + managedObject::Allocated()
        + iGraph::Allocated();
}


unsigned long iGraph::Allocated() const throw()
{
    return n*sizeof(TArc);        // current[]
}


void iGraph::Reset() throw()
{
    for (TNode i=0;i<n;i++) current[i] = NoArc;
}


void iGraph::Reset(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Reset",v);

    #endif

    current[v] = NoArc;
}


TArc iGraph::Read(TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Read",v);

    if (current[v]==G.First(v) || G.First(v)==NoArc) NoMoreArcs("Read",v);

    #endif

    if (current[v]==NoArc)
    {
        current[v] = G.Right(G.First(v),v);
        return G.First(v);
    }
    else
    {
        TArc ret = current[v];
        current[v] = G.Right(ret,v);
        return ret;
    }
}


TArc iGraph::Peek(TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Peek",v);

    if (current[v]==G.First(v) || G.First(v)==NoArc) NoMoreArcs("Peek",v);

    #endif

    if (current[v]==NoArc) return G.First(v);
    else return current[v];  
}


bool iGraph::Active(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Active",v);

    #endif

    return (current[v]!=G.First(v) && G.First(v)!=NoArc);
}


iGraph::~iGraph() throw()
{
    G.ReleaseRef();
    delete[] current;
}
