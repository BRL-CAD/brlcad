
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   itSurfaceGraph.cpp
/// \brief  #iSurfaceGraph class implementation

#include "surfaceGraph.h"


iSurfaceGraph::iSurfaceGraph(const surfaceGraph &GC) throw() :
    managedObject(GC.Context()), G(GC.G), N(GC), S(GC.S)
{
    N.MakeRef();

    n0 = N.n0;
    n1 = N.N1();
    nr = N.nr;
    nv = N.nv;
    n = N.n;
    m = N.m;

    Handle = G.Investigate();

    current = new TNode[2*nv];

    Reset();
}


unsigned long iSurfaceGraph::Size() const throw()
{
    return
          sizeof(iSurfaceGraph)
        + managedObject::Allocated()
        + iSurfaceGraph::Allocated();
}


unsigned long iSurfaceGraph::Allocated() const throw()
{
    return 2*nv*sizeof(TNode);
}


void iSurfaceGraph::Reset() throw()
{
    G.Reset(Handle);

    for (TNode v=0;v<nv;v++)
    {
        TNode x = S.First(v+nr);
        current[2*v] = current[2*v+1] = x;
    }
}


void iSurfaceGraph::Reset(TNode v) throw(ERRange)
{
    if (v<n0)
    {
        G.Reset(Handle,v);
        return;
    }

    if (v<n)
    {
        TNode w = S.First(v/2);

        if (w==nr+nv) return;

        current[v-n0] = w;
        TNode u = S.Next(w);
        TNode par = v%2;
        Reset(2*w+par);

        while (w!=u)
        {
            w = u;
            Reset(2*w+par);
            u = S.Next(w);
        }

        return;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("Reset",v);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::Read(TNode v) throw(ERRange,ERRejected)
{
    if (v<n0) return G.Read(Handle,v);

    if (v<n) return ReadBlossom(v,N.bprop[v/2-nr]);

    #if defined(_FAILSAVE_)

    NoSuchNode("Read",v);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::ReadBlossom(TNode v,TArc thisProp) throw(ERRange)
{
    if (v>=n0 && v<n)
    {
        TNode w = current[v-n0];
        TNode par = v%2;

        while (w!=S.Next(w) && !ActiveBlossom(2*w+par)) w = S.Next(w);

        current[v-n0] = w;

        if (w<nr)
        {
            TArc a = Read(2*w+par);

            if (a==(thisProp^1))
            {
                if (par) return a^3;
                else return a;
            }

            if (a==(thisProp^2))
            {
                if (par) return a;
                else return a^3;
            }

            // Handle loops
            if (G.StartNode(a)==(G.EndNode(a)^1)) return a;

            if (G.BalCap(a)>0)
            {
                if (par) return a^3;
                else return a;
            }

            if (G.BalCap(a^1)>0)
            {
                if (par) return a;
                else return a^3;
            }

            if (a&1)
            {
                if (par) return a;
                else return a^3;
            }
            else
            {
                if (par) return a^3;
                else return a;
            }
        }
        else return ReadBlossom(2*w+par,thisProp);
    }

    #if defined(_FAILSAVE_)

    sprintf(CT.logBuffer,"No such blossom: %lu",static_cast<unsigned long>(v));
    Error(ERR_RANGE,"ReadBlossom",CT.logBuffer);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::Peek(TNode v) throw(ERRange,ERRejected)
{
    if (v<n0) return G.Peek(Handle,v);

    if (v<n) return ReadBlossom(v,N.bprop[v/2-nr]);

    #if defined(_FAILSAVE_)

    NoSuchNode("Peek",v);

    #endif

    throw ERRange();
}


bool iSurfaceGraph::Active(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Active",v);

    #endif

    if (!S.Top(v/2)) return false;

    if (v<n0) return G.Active(Handle,v);
    else return ActiveBlossom(v);
}


bool iSurfaceGraph::ActiveBlossom(TNode v) const throw(ERRange)
{
    if (v<n0) return G.Active(Handle,v);

    if (v<n)
    {
        TNode w = current[v-n0];

        TNode par = v%2;

        if (w==NoNode || w==nr+nv) return false;

        while (w!=S.Next(w) && !ActiveBlossom(2*w+par)) w = S.Next(w);

        current[v-n0] = w;

        return ActiveBlossom(2*w+par);
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("ActiveBlossom",v);

    #endif

    throw ERRange();
}


iSurfaceGraph::~iSurfaceGraph() throw()
{
    delete[] current;

    N.ReleaseRef();
    G.Close(Handle);
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   itSurfaceGraph.cpp
/// \brief  #iSurfaceGraph class implementation

#include "surfaceGraph.h"


iSurfaceGraph::iSurfaceGraph(const surfaceGraph &GC) throw() :
    managedObject(GC.Context()), G(GC.G), N(GC), S(GC.S)
{
    N.MakeRef();

    n0 = N.n0;
    n1 = N.N1();
    nr = N.nr;
    nv = N.nv;
    n = N.n;
    m = N.m;

    Handle = G.Investigate();

    current = new TNode[2*nv];

    Reset();
}


unsigned long iSurfaceGraph::Size() const throw()
{
    return
          sizeof(iSurfaceGraph)
        + managedObject::Allocated()
        + iSurfaceGraph::Allocated();
}


unsigned long iSurfaceGraph::Allocated() const throw()
{
    return 2*nv*sizeof(TNode);
}


void iSurfaceGraph::Reset() throw()
{
    G.Reset(Handle);

    for (TNode v=0;v<nv;v++)
    {
        TNode x = S.First(v+nr);
        current[2*v] = current[2*v+1] = x;
    }
}


void iSurfaceGraph::Reset(TNode v) throw(ERRange)
{
    if (v<n0)
    {
        G.Reset(Handle,v);
        return;
    }

    if (v<n)
    {
        TNode w = S.First(v/2);

        if (w==nr+nv) return;

        current[v-n0] = w;
        TNode u = S.Next(w);
        TNode par = v%2;
        Reset(2*w+par);

        while (w!=u)
        {
            w = u;
            Reset(2*w+par);
            u = S.Next(w);
        }

        return;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("Reset",v);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::Read(TNode v) throw(ERRange,ERRejected)
{
    if (v<n0) return G.Read(Handle,v);

    if (v<n) return ReadBlossom(v,N.bprop[v/2-nr]);

    #if defined(_FAILSAVE_)

    NoSuchNode("Read",v);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::ReadBlossom(TNode v,TArc thisProp) throw(ERRange)
{
    if (v>=n0 && v<n)
    {
        TNode w = current[v-n0];
        TNode par = v%2;

        while (w!=S.Next(w) && !ActiveBlossom(2*w+par)) w = S.Next(w);

        current[v-n0] = w;

        if (w<nr)
        {
            TArc a = Read(2*w+par);

            if (a==(thisProp^1))
            {
                if (par) return a^3;
                else return a;
            }

            if (a==(thisProp^2))
            {
                if (par) return a;
                else return a^3;
            }

            // Handle loops
            if (G.StartNode(a)==(G.EndNode(a)^1)) return a;

            if (G.BalCap(a)>0)
            {
                if (par) return a^3;
                else return a;
            }

            if (G.BalCap(a^1)>0)
            {
                if (par) return a;
                else return a^3;
            }

            if (a&1)
            {
                if (par) return a;
                else return a^3;
            }
            else
            {
                if (par) return a^3;
                else return a;
            }
        }
        else return ReadBlossom(2*w+par,thisProp);
    }

    #if defined(_FAILSAVE_)

    sprintf(CT.logBuffer,"No such blossom: %lu",static_cast<unsigned long>(v));
    Error(ERR_RANGE,"ReadBlossom",CT.logBuffer);

    #endif

    throw ERRange();
}


TArc iSurfaceGraph::Peek(TNode v) throw(ERRange,ERRejected)
{
    if (v<n0) return G.Peek(Handle,v);

    if (v<n) return ReadBlossom(v,N.bprop[v/2-nr]);

    #if defined(_FAILSAVE_)

    NoSuchNode("Peek",v);

    #endif

    throw ERRange();
}


bool iSurfaceGraph::Active(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Active",v);

    #endif

    if (!S.Top(v/2)) return false;

    if (v<n0) return G.Active(Handle,v);
    else return ActiveBlossom(v);
}


bool iSurfaceGraph::ActiveBlossom(TNode v) const throw(ERRange)
{
    if (v<n0) return G.Active(Handle,v);

    if (v<n)
    {
        TNode w = current[v-n0];

        TNode par = v%2;

        if (w==NoNode || w==nr+nv) return false;

        while (w!=S.Next(w) && !ActiveBlossom(2*w+par)) w = S.Next(w);

        current[v-n0] = w;

        return ActiveBlossom(2*w+par);
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("ActiveBlossom",v);

    #endif

    throw ERRange();
}


iSurfaceGraph::~iSurfaceGraph() throw()
{
    delete[] current;

    N.ReleaseRef();
    G.Close(Handle);
}
