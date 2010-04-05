
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   itAuxiliaryNetwork.cpp
/// \brief  #iLayeredAuxNetwork class implementation

#include "auxiliaryNetwork.h"


iLayeredAuxNetwork::iLayeredAuxNetwork(const layeredAuxNetwork &GC) throw() : G(GC)
{
    n = G.n;
    currentIndex = new TArc[n];

    for (TNode i=0;i<n;i++) currentIndex[i] = 0;
}


unsigned long iLayeredAuxNetwork::Size() const throw()
{
    return
          sizeof(iLayeredAuxNetwork)
        + managedObject::Allocated()
        + iLayeredAuxNetwork::Allocated();
}


unsigned long iLayeredAuxNetwork::Allocated() const throw()
{
    return n*sizeof(TArc);        // currentIndex[]
}


void iLayeredAuxNetwork::Reset() throw()
{
    for (TNode i=0;i<n;i++) currentIndex[i] = 0;
}


void iLayeredAuxNetwork::Reset(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Reset",v);

    #endif

    currentIndex[v] = 0;
}


TArc iLayeredAuxNetwork::Read(TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Read",v);

    if (currentIndex[v]>=G.inDegree[v]) NoMoreArcs("Read",v);

    #endif

    return G.prop[v][currentIndex[v]++];
}


TArc iLayeredAuxNetwork::Peek(TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Peek",v);

    if (currentIndex[v]>=G.inDegree[v]) NoMoreArcs("Peek",v);

    #endif

    return G.prop[v][currentIndex[v]];
}


void iLayeredAuxNetwork::Skip(TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Skip",v);

    if (currentIndex[v]>=G.inDegree[v]) NoMoreArcs("Skip",v);

    #endif

    currentIndex[v]++;
}


bool iLayeredAuxNetwork::Active(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Active",v);

    #endif

    return currentIndex[v]<G.inDegree[v];
}


iLayeredAuxNetwork::~iLayeredAuxNetwork() throw()
{
    delete[] currentIndex;
}
