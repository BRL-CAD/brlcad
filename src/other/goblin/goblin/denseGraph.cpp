
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseGraph.cpp
/// \brief  #denseGraph class implementation

#include "denseGraph.h"


// ------------------------------------------------------


denseGraph::denseGraph(TNode _n,TOption options,goblinController& _CT)
    throw() :
    managedObject(_CT),
    abstractGraph(_n,_n*(_n+1)/2),
    X(static_cast<const denseGraph&>(*this),options)
{
    X.SetCDemand(1);
    if (!CT.randUCap) X.SetCUCap(1);

    LogEntry(LOG_MEM,"...Dense graph instanciated");
}


denseGraph::denseGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractGraph(TNode(0),TArc(0)),
    X(static_cast<const denseGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading dense graph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading dense graph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("dense_graph");
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


denseGraph::denseGraph(abstractGraph& G) throw() :
    managedObject(G.Context()),
    abstractGraph(G.N(),G.N()*(G.N()+1)/2),
    X(static_cast<const denseGraph&>(*this))
{
    LogEntry(LOG_MAN,"Converting into dense graph...");

    ImportLayoutData(G);
    TArc m0 = G.M();

    for (TArc i=0;i<m0;i++)
    {
        TArc a = 2*i;
        InsertArc(G.StartNode(a),G.EndNode(a),G.UCap(a),G.Length(a),G.LCap(a));
    }

    if (CT.traceLevel==2) Display();
}


denseGraph::~denseGraph() throw()
{
    LogEntry(LOG_MEM,"...Dense graph disallocated");

    if (CT.traceLevel==2) Display();
}


void denseGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    m = n*(n+1)/2;
    delete[] nodes;

    CheckLimits();
    X.Reserve(n,m,n+ni);
}


unsigned long denseGraph::Size() const throw()
{
    return
          sizeof(denseGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated();
}


unsigned long denseGraph::Allocated() const throw()
{
    return 0;
}


TArc denseGraph::Adjacency(TNode u,TNode v,TMethAdjacency) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = u*(u+1)+2*v;

    if (v>u) a = v*(v+1)+2*u+1;

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


TNode denseGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    if (a&1)
    {
        TNode u = TNode(floor(sqrt(a-0.75)-0.5));
        return (TNode(a)-(u*u)-u-1)/2;
    }
    else return TNode(floor(sqrt(a+0.25)-0.5));
}


TNode denseGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    if (a&1) return TNode(floor(sqrt(a-0.75)-0.5));
    else
    {
        TNode u = TNode(floor(sqrt(a+0.25)-0.5));
        return (TNode(a)-(u*u)-u)/2;
    }
}


TArc denseGraph::First(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=2*n) NoSuchNode("First",v);

    #endif

    return Adjacency(v,0);
}


TArc denseGraph::Right(TArc a,TNode u) const throw(ERRange)
{
    if (u==NoNode) u = StartNode(a);

    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u>=n) NoSuchNode("Right",u);

    #endif

    TNode v = EndNode(a);

    if (u==v && !(a&1)) return a^1;

    if (v<n-1) return Adjacency(u,v+1);
    else return Adjacency(u,0);
}


metricGraph::metricGraph(abstractGraph &G) throw(ERRejected) :
    managedObject(G.Context()),
    denseGraph(G.N(),TOption(0),G.Context())
{
    LogEntry(LOG_MAN,"Generating metric graph...");
    OpenFold();

    ImportLayoutData(G);

    if (G.Dim()>0)
    {
        for (TNode v=0;v<G.N();v++)
            for (TDim i=0;i<G.Dim();i++) X.SetC(v,i,G.C(v,i));
    }

    for (TNode u=0;u<n;u++)
    {
        G.ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(G),u);

        for (TNode v=0;v<=u;v++)
        {
            X.SetLength(Adjacency(u,v),(u==v) ? InfFloat : G.Dist(v));
        }
    }

    CloseFold();
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   denseGraph.cpp
/// \brief  #denseGraph class implementation

#include "denseGraph.h"


// ------------------------------------------------------


denseGraph::denseGraph(TNode _n,TOption options,goblinController& _CT)
    throw() :
    managedObject(_CT),
    abstractGraph(_n,_n*(_n+1)/2),
    X(static_cast<const denseGraph&>(*this),options)
{
    X.SetCDemand(1);
    if (!CT.randUCap) X.SetCUCap(1);

    LogEntry(LOG_MEM,"...Dense graph instanciated");
}


denseGraph::denseGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractGraph(TNode(0),TArc(0)),
    X(static_cast<const denseGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading dense graph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading dense graph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("dense_graph");
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


denseGraph::denseGraph(abstractGraph& G) throw() :
    managedObject(G.Context()),
    abstractGraph(G.N(),G.N()*(G.N()+1)/2),
    X(static_cast<const denseGraph&>(*this))
{
    LogEntry(LOG_MAN,"Converting into dense graph...");

    ImportLayoutData(G);
    TArc m0 = G.M();

    for (TArc i=0;i<m0;i++)
    {
        TArc a = 2*i;
        InsertArc(G.StartNode(a),G.EndNode(a),G.UCap(a),G.Length(a),G.LCap(a));
    }

    if (CT.traceLevel==2) Display();
}


denseGraph::~denseGraph() throw()
{
    LogEntry(LOG_MEM,"...Dense graph disallocated");

    if (CT.traceLevel==2) Display();
}


void denseGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    m = n*(n+1)/2;
    delete[] nodes;

    CheckLimits();
    X.Reserve(n,m,n+ni);
}


unsigned long denseGraph::Size() const throw()
{
    return
          sizeof(denseGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated();
}


unsigned long denseGraph::Allocated() const throw()
{
    return 0;
}


TArc denseGraph::Adjacency(TNode u,TNode v,TMethAdjacency) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = u*(u+1)+2*v;

    if (v>u) a = v*(v+1)+2*u+1;

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


TNode denseGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    if (a&1)
    {
        TNode u = TNode(floor(sqrt(a-0.75)-0.5));
        return (TNode(a)-(u*u)-u-1)/2;
    }
    else return TNode(floor(sqrt(a+0.25)-0.5));
}


TNode denseGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    if (a&1) return TNode(floor(sqrt(a-0.75)-0.5));
    else
    {
        TNode u = TNode(floor(sqrt(a+0.25)-0.5));
        return (TNode(a)-(u*u)-u)/2;
    }
}


TArc denseGraph::First(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=2*n) NoSuchNode("First",v);

    #endif

    return Adjacency(v,0);
}


TArc denseGraph::Right(TArc a,TNode u) const throw(ERRange)
{
    if (u==NoNode) u = StartNode(a);

    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u>=n) NoSuchNode("Right",u);

    #endif

    TNode v = EndNode(a);

    if (u==v && !(a&1)) return a^1;

    if (v<n-1) return Adjacency(u,v+1);
    else return Adjacency(u,0);
}


metricGraph::metricGraph(abstractGraph &G) throw(ERRejected) :
    managedObject(G.Context()),
    denseGraph(G.N(),TOption(0),G.Context())
{
    LogEntry(LOG_MAN,"Generating metric graph...");
    OpenFold();

    ImportLayoutData(G);

    if (G.Dim()>0)
    {
        for (TNode v=0;v<G.N();v++)
            for (TDim i=0;i<G.Dim();i++) X.SetC(v,i,G.C(v,i));
    }

    for (TNode u=0;u<n;u++)
    {
        G.ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(G),u);

        for (TNode v=0;v<=u;v++)
        {
            X.SetLength(Adjacency(u,v),(u==v) ? InfFloat : G.Dist(v));
        }
    }

    CloseFold();
}
