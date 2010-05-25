
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractBigraph.cpp
/// \brief  #abstractBiGraph partial class implementation

#include "abstractBigraph.h"


abstractBiGraph::abstractBiGraph(TNode _n1,TNode _n2,TArc _m) throw(ERRange) :
    abstractGraph(_n1+_n2,_m), n1(_n1), n2(_n2)
{
    CheckLimits();

    LogEntry(LOG_MEM,"...Abstract bigraph object allocated");
}


abstractBiGraph::~abstractBiGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract bigraph object disallocated");
}


unsigned long abstractBiGraph::Allocated() const throw()
{
    return 0;
}


bool abstractBiGraph::IsBipartite() const throw()
{
    return true;
}


void abstractBiGraph::CheckLimits() throw(ERRange)
{
    #if defined(_FAILSAVE_)

    abstractMixedGraph::CheckLimits();

    if (2*n1>=CT.MaxNode())
    {
        InternalError("CheckLimits","Number of left-hand nodes is out of range");
    }

    if (2*n2>=CT.MaxNode())
    {
        InternalError("CheckLimits","Number of right-hand nodes is out of range");
    }

    #endif
}


void abstractBiGraph::RandomArcs(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc() || (!CT.randParallels && m+_m>TArc(n1*n2)))
        Error(ERR_REJECTED,"RandomArcs","Number of arcs is out of range");

    #endif

    if (CT.logMan && _m>0)
    {
        sprintf(CT.logBuffer,"Generating %lu edges...",static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    for (TArc i=0;i<_m;)
    {
        TNode u = (TNode)CT.Rand(n1);
        TNode v = n1+(TNode)CT.Rand(n2);

        TArc a = (!CT.randParallels && m>0) ? Adjacency(u,v) : NoArc;

        if (a==NoArc)
        {
            ReleaseInvestigators();
            InsertArc(u,v);
            i++;
        }
    }
}


void abstractBiGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode *nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    n1 = nodes[1];
    ni = nodes[2];
    n2 = n-n1;
    delete[] nodes;
}


leftHandNodes::leftHandNodes(const abstractBiGraph& _G) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()), G(_G)
{
}


leftHandNodes::~leftHandNodes() throw()
{
}


unsigned long leftHandNodes::Size() const throw()
{
    return sizeof(leftHandNodes) + managedObject::Allocated();
}


bool leftHandNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (i<G.N1());
}


TNode leftHandNodes::First() const throw()
{
    if (G.N1()>0) return 0; else return G.N();
}


TNode leftHandNodes::Successor(const TNode i) const throw(ERRange)
{
    if (i<G.N1()-1) return i+1; else return G.N();
}


rightHandNodes::rightHandNodes(const abstractBiGraph& _G) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()), G(_G)
{
}


rightHandNodes::~rightHandNodes() throw()
{
}


unsigned long rightHandNodes::Size() const throw()
{
    return sizeof(rightHandNodes) + managedObject::Allocated();
}


bool rightHandNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (i>=G.N1() && i<G.N());
}


TNode rightHandNodes::First() const throw()
{
    if (G.N2()>0) return G.N1(); else return G.N();
}


TNode rightHandNodes::Successor(const TNode i) const throw(ERRange)
{
    if (i<G.N()-1) return i+1; else return G.N();
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractBigraph.cpp
/// \brief  #abstractBiGraph partial class implementation

#include "abstractBigraph.h"


abstractBiGraph::abstractBiGraph(TNode _n1,TNode _n2,TArc _m) throw(ERRange) :
    abstractGraph(_n1+_n2,_m), n1(_n1), n2(_n2)
{
    CheckLimits();

    LogEntry(LOG_MEM,"...Abstract bigraph object allocated");
}


abstractBiGraph::~abstractBiGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract bigraph object disallocated");
}


unsigned long abstractBiGraph::Allocated() const throw()
{
    return 0;
}


bool abstractBiGraph::IsBipartite() const throw()
{
    return true;
}


void abstractBiGraph::CheckLimits() throw(ERRange)
{
    #if defined(_FAILSAVE_)

    abstractMixedGraph::CheckLimits();

    if (2*n1>=CT.MaxNode())
    {
        InternalError("CheckLimits","Number of left-hand nodes is out of range");
    }

    if (2*n2>=CT.MaxNode())
    {
        InternalError("CheckLimits","Number of right-hand nodes is out of range");
    }

    #endif
}


void abstractBiGraph::RandomArcs(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc() || (!CT.randParallels && m+_m>TArc(n1*n2)))
        Error(ERR_REJECTED,"RandomArcs","Number of arcs is out of range");

    #endif

    if (CT.logMan && _m>0)
    {
        sprintf(CT.logBuffer,"Generating %lu edges...",static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    for (TArc i=0;i<_m;)
    {
        TNode u = (TNode)CT.Rand(n1);
        TNode v = n1+(TNode)CT.Rand(n2);

        TArc a = (!CT.randParallels && m>0) ? Adjacency(u,v) : NoArc;

        if (a==NoArc)
        {
            ReleaseInvestigators();
            InsertArc(u,v);
            i++;
        }
    }
}


void abstractBiGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode *nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    n1 = nodes[1];
    ni = nodes[2];
    n2 = n-n1;
    delete[] nodes;
}


leftHandNodes::leftHandNodes(const abstractBiGraph& _G) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()), G(_G)
{
}


leftHandNodes::~leftHandNodes() throw()
{
}


unsigned long leftHandNodes::Size() const throw()
{
    return sizeof(leftHandNodes) + managedObject::Allocated();
}


bool leftHandNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (i<G.N1());
}


TNode leftHandNodes::First() const throw()
{
    if (G.N1()>0) return 0; else return G.N();
}


TNode leftHandNodes::Successor(const TNode i) const throw(ERRange)
{
    if (i<G.N1()-1) return i+1; else return G.N();
}


rightHandNodes::rightHandNodes(const abstractBiGraph& _G) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()), G(_G)
{
}


rightHandNodes::~rightHandNodes() throw()
{
}


unsigned long rightHandNodes::Size() const throw()
{
    return sizeof(rightHandNodes) + managedObject::Allocated();
}


bool rightHandNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (i>=G.N1() && i<G.N());
}


TNode rightHandNodes::First() const throw()
{
    if (G.N2()>0) return G.N1(); else return G.N();
}


TNode rightHandNodes::Successor(const TNode i) const throw(ERRange)
{
    if (i<G.N()-1) return i+1; else return G.N();
}
