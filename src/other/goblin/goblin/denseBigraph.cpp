
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseBigraph.cpp
/// \brief  #denseBiGraph class implementation

#include "denseBigraph.h"


denseBiGraph::denseBiGraph(TNode _n1,TNode _n2,TOption options,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractBiGraph(_n1,_n2,_n1*_n2),
    X(static_cast<const denseBiGraph&>(*this),options)
{
    X.SetCDemand(1);
    if (!CT.randUCap) X.SetCUCap(1);

    LogEntry(LOG_MEM,"...Dense bigraph instanciated");
}


denseBiGraph::denseBiGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractBiGraph(TNode(0),TNode(0),TArc(0)),
    X(static_cast<const denseBiGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading dense bigraph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading dense bigraph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("dense_bigraph");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

    int l = strlen(fileName)-4;
    char* tmpLabel = new char[l+1];
    memcpy(tmpLabel,fileName,l);
    tmpLabel[l] = 0;
    SetLabel(tmpLabel);
    delete[] tmpLabel;

    CT.SetMaster(Handle());

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


denseBiGraph::denseBiGraph(abstractBiGraph& G) throw() :
    managedObject(G.Context()),
    abstractBiGraph(G.N1(),G.N2(),G.N1()*G.N2()),
    X(static_cast<const denseBiGraph&>(*this))
{
    LogEntry(LOG_MAN,"Converting into dense bigraph...");

    ImportLayoutData(G);
    TArc m0 = G.M();
    X.SetCUCap(0);

    for (TArc i=0;i<m0;i++)
    {
        TArc a=2*i;
        InsertArc(G.StartNode(a),G.EndNode(a),G.UCap(a),G.Length(a),G.LCap(a));
    }

    if (CT.traceLevel==2) Display();
}


void denseBiGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    n1 = nodes[1];
    ni = nodes[2];
    n2 = n-n1;
    m = n1*n2;
    delete[] nodes;

    CheckLimits();
    X.Reserve(n,m,n+ni);
}


unsigned long denseBiGraph::Size() const throw()
{
    return 
          sizeof(denseBiGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated()
        + abstractBiGraph::Allocated();
}


unsigned long denseBiGraph::Allocated() const throw()
{
    return 0;
}


denseBiGraph::~denseBiGraph() throw()
{
    LogEntry(LOG_MEM,"...Dense bigraph disallocated");

    if (CT.traceLevel==2) Display();
}


TArc denseBiGraph::Adjacency(TNode u,TNode v,TMethAdjacency) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = NoArc;

    if (u<v)
    {
        if (v>=n1 && u<n1) a = 2*(u*n2+(v-n1));
    }
    else
    {
        if (u>=n1 && v<n1) a = 2*(v*n2+(u-n1))+1;
    }

    if (a!=NoArc && UCap(a)==0) a = NoArc;

    #if defined(_LOGGING_)

    if (CT.logRes>2)
    {
        sprintf(CT.logBuffer,
            "The nodes %lu and %lu are adjacent by the arc %lu",
            static_cast<unsigned long>(u),
            static_cast<unsigned long>(v),
            static_cast<unsigned long>(a));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return a;
}


TNode denseBiGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    if (a&1) return n1+(TNode)(a>>1)%n2;
    else return (TNode)(a>>1)/n2;
}


TNode denseBiGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    if (a&1) return (TNode)(a>>1)/n2;
    else return n1+(TNode)(a>>1)%n2;
}


TArc denseBiGraph::First(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=2*n) NoSuchNode("First",v);

    #endif

    if (v<n1) return Adjacency(v,n1);
    else return Adjacency(v,0);
}


TArc denseBiGraph::Right(TArc a,TNode u) const throw(ERRange)
{
    if (u==NoNode) u = StartNode(a);

    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u>=n) NoSuchNode("Right",u);

    #endif

    TNode v = EndNode(a);

    if (v==n1-1) return Adjacency(u,0);
    if (v==n-1) return Adjacency(u,n1);

    return Adjacency(u,v+1);
}
