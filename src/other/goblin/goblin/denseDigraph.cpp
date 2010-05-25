
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseDigraph.cpp
/// \brief  #denseDiGraph class implementation

#include "denseDigraph.h"
#include "moduleGuard.h"


denseDiGraph::denseDiGraph(TNode _n,TOption options,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractDiGraph(_n,_n*_n),
    X(static_cast<const denseDiGraph&>(*this),options)
{
    X.SetCDemand(0);
    X.SetCOrientation(1);
    if (!CT.randUCap) X.SetCUCap(1);

    LogEntry(LOG_MEM,"...Dense digraph instanciated");
}


denseDiGraph::denseDiGraph(const char* fileName, goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractDiGraph(TNode(0),TArc(0)),
    X(static_cast<const denseDiGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading graph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading dense digraph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("dense_digraph");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

    X.SetCOrientation(1);

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


denseDiGraph::denseDiGraph(abstractDiGraph& G) throw() :
    managedObject(G.Context()),
    abstractDiGraph(G.N(),G.N()*G.N()),
    X(static_cast<const denseDiGraph&>(*this))
{
    ImportLayoutData(G);
    X.SetCOrientation(1);
    X.SetCUCap(0);

    LogEntry(LOG_MAN,"Converting into dense digraph...");

    TArc m0 = G.M();

    for (TArc i=0;i<m0;i++)
    {
        TArc a=2*i;
        InsertArc(G.StartNode(a),G.EndNode(a),G.UCap(a),G.Length(a),G.LCap(a));
    }

    if (CT.traceLevel==2) Display();
}


void denseDiGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    m = n*n;
    delete[] nodes;

    CheckLimits();
    X.Reserve(n,m,n+ni);
}


unsigned long denseDiGraph::Size() const throw()
{
    return
          sizeof(denseDiGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated();
}


unsigned long denseDiGraph::Allocated() const throw()
{
    return 0;
}


denseDiGraph::~denseDiGraph() throw()
{
    LogEntry(LOG_MEM,"...Dense digraph disallocated");

    if (CT.traceLevel==2) Display();
}


TArc denseDiGraph::Adjacency(TNode u,TNode v,TMethAdjacency) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = 2*(u*n+v);

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


TNode denseDiGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    if (a&1) return (TNode)(a>>1)%n;
    else return (TNode)(a>>1)/n;
}


TNode denseDiGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    if (a&1) return (TNode)(a>>1)/n;
    else return (TNode)(a>>1)%n;
}


TArc denseDiGraph::First(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=2*n) NoSuchNode("First",v);

    #endif

    return Adjacency(v,0);
}


TArc denseDiGraph::Right(TArc a,TNode u) const throw(ERRange)
{
    if (u==NoNode) u = StartNode(a);

    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u>=n) NoSuchNode("Right",u);

    #endif

    TNode v = EndNode(a);

    if (a&1)
    {
        if (v<n-1) return Adjacency(u,v+1);
        else return Adjacency(u,0);
    }
    else return Adjacency(v,u)^1;
}


distanceGraph::distanceGraph(abstractMixedGraph &G) throw(ERRejected) :
    managedObject(G.Context()),
    denseDiGraph(G.N(),(TOption)0,G.Context())
{
    moduleGuard M(ModFloydWarshall,*this,"Generating distance graph...");

    ImportLayoutData(G);

    if (G.Dim()>0)
    {
        for (TNode v=0;v<G.N();v++)
            for (TDim i=0;i<G.Dim();i++) X.SetC(v,i,G.C(v,i));
    }

    for (TNode u=0;u<n;u++)
        for (TNode v=0;v<n;v++)
            X.SetLength(Adjacency(u,v),(u==v) ? 0 : InfFloat);

    for (TArc a=0;a<2*G.M();a++)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        TArc a2 = Adjacency(u,v);
        if (G.Length(a)<Length(a2) && !G.Blocking(a))
            X.SetLength(a2,G.Length(a));
    }

    for (TNode u=0;u<n;u++)
        for (TNode v=0;v<n;v++)
            for (TNode w=0;w<n;w++)
            {
                TArc a1 = Adjacency(v,w);
                TArc a2 = Adjacency(v,u);
                TArc a3 = Adjacency(u,w);
                if (Length(a2)+Length(a3)<Length(a1))
                    X.SetLength(a1,Length(a2)+Length(a3));
            }
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseDigraph.cpp
/// \brief  #denseDiGraph class implementation

#include "denseDigraph.h"
#include "moduleGuard.h"


denseDiGraph::denseDiGraph(TNode _n,TOption options,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractDiGraph(_n,_n*_n),
    X(static_cast<const denseDiGraph&>(*this),options)
{
    X.SetCDemand(0);
    X.SetCOrientation(1);
    if (!CT.randUCap) X.SetCUCap(1);

    LogEntry(LOG_MEM,"...Dense digraph instanciated");
}


denseDiGraph::denseDiGraph(const char* fileName, goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractDiGraph(TNode(0),TArc(0)),
    X(static_cast<const denseDiGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading graph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading dense digraph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("dense_digraph");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

    X.SetCOrientation(1);

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


denseDiGraph::denseDiGraph(abstractDiGraph& G) throw() :
    managedObject(G.Context()),
    abstractDiGraph(G.N(),G.N()*G.N()),
    X(static_cast<const denseDiGraph&>(*this))
{
    ImportLayoutData(G);
    X.SetCOrientation(1);
    X.SetCUCap(0);

    LogEntry(LOG_MAN,"Converting into dense digraph...");

    TArc m0 = G.M();

    for (TArc i=0;i<m0;i++)
    {
        TArc a=2*i;
        InsertArc(G.StartNode(a),G.EndNode(a),G.UCap(a),G.Length(a),G.LCap(a));
    }

    if (CT.traceLevel==2) Display();
}


void denseDiGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    m = n*n;
    delete[] nodes;

    CheckLimits();
    X.Reserve(n,m,n+ni);
}


unsigned long denseDiGraph::Size() const throw()
{
    return
          sizeof(denseDiGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated();
}


unsigned long denseDiGraph::Allocated() const throw()
{
    return 0;
}


denseDiGraph::~denseDiGraph() throw()
{
    LogEntry(LOG_MEM,"...Dense digraph disallocated");

    if (CT.traceLevel==2) Display();
}


TArc denseDiGraph::Adjacency(TNode u,TNode v,TMethAdjacency) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = 2*(u*n+v);

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


TNode denseDiGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    if (a&1) return (TNode)(a>>1)%n;
    else return (TNode)(a>>1)/n;
}


TNode denseDiGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    if (a&1) return (TNode)(a>>1)/n;
    else return (TNode)(a>>1)%n;
}


TArc denseDiGraph::First(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=2*n) NoSuchNode("First",v);

    #endif

    return Adjacency(v,0);
}


TArc denseDiGraph::Right(TArc a,TNode u) const throw(ERRange)
{
    if (u==NoNode) u = StartNode(a);

    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u>=n) NoSuchNode("Right",u);

    #endif

    TNode v = EndNode(a);

    if (a&1)
    {
        if (v<n-1) return Adjacency(u,v+1);
        else return Adjacency(u,0);
    }
    else return Adjacency(v,u)^1;
}


distanceGraph::distanceGraph(abstractMixedGraph &G) throw(ERRejected) :
    managedObject(G.Context()),
    denseDiGraph(G.N(),(TOption)0,G.Context())
{
    moduleGuard M(ModFloydWarshall,*this,"Generating distance graph...");

    ImportLayoutData(G);

    if (G.Dim()>0)
    {
        for (TNode v=0;v<G.N();v++)
            for (TDim i=0;i<G.Dim();i++) X.SetC(v,i,G.C(v,i));
    }

    for (TNode u=0;u<n;u++)
        for (TNode v=0;v<n;v++)
            X.SetLength(Adjacency(u,v),(u==v) ? 0 : InfFloat);

    for (TArc a=0;a<2*G.M();a++)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        TArc a2 = Adjacency(u,v);
        if (G.Length(a)<Length(a2) && !G.Blocking(a))
            X.SetLength(a2,G.Length(a));
    }

    for (TNode u=0;u<n;u++)
        for (TNode v=0;v<n;v++)
            for (TNode w=0;w<n;w++)
            {
                TArc a1 = Adjacency(v,w);
                TArc a2 = Adjacency(v,u);
                TArc a3 = Adjacency(u,w);
                if (Length(a2)+Length(a3)<Length(a1))
                    X.SetLength(a1,Length(a2)+Length(a3));
            }
}
