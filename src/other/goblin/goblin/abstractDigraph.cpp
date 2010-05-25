
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractDigraph.cpp
/// \brief  #abstractDiGraph partial class implementation

#include "abstractDigraph.h"


abstractDiGraph::abstractDiGraph(TNode _n,TArc _m) throw() :
    abstractMixedGraph(_n,_m)
{
    LogEntry(LOG_MEM,"...Abstract digraph allocated");
}


abstractDiGraph::~abstractDiGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract digraph disallocated");
}


unsigned long abstractDiGraph::Allocated() const throw()
{
    return 0;
}


TFloat abstractDiGraph::Flow(TArc a) const throw(ERRange)
{
    return Sub(a);
}


void abstractDiGraph::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    #endif

    // This looks like cycling code since the default implementation of
    // SetSubRelative() also calls Push(). But note that represented graphs
    // reimplement SetSubRelative(), and all problem reimplement Push().
    // So, no recurrency occurs for concrete objects.

    SetSubRelative(a, (a&1) ? -Lambda : Lambda);
}


void abstractDiGraph::Augment(TArc* pred,TNode u,TNode v,TFloat Lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Augment",u);

    if (v>=n) NoSuchNode("Augment",v);

    if (Lambda<=0) Error(ERR_REJECTED,"Augment","Amount should be positive");

    if (!pred) Error(ERR_REJECTED,"Augment","Missing predecessor labels");

    TNode PathLength = 0;

    #endif

    TNode w = v;

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Augmenting by %g units of flow...",
            static_cast<double>(Lambda));
        LogEntry(LOG_METH2,CT.logBuffer);
        LogEntry(LOG_METH2,"Path in reverse order:");
        OpenFold();
        sprintf(CT.logBuffer,"(%lu",static_cast<unsigned long>(w));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    do
    {
        TArc a = pred[w];

        #if defined(_FAILSAVE_)

        if (PathLength>=n || a==NoArc)
        {
            InternalError("Augment","Missing start node");
        }

        PathLength++;

        #endif

        Push(a,Lambda);
        pred[w] = NoArc;
        w = StartNode(a);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"[%lu]%lu",
                static_cast<unsigned long>(a),static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }
    while (w!=u);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        LogEnd(LH,")");
        CloseFold();
    }

    #endif
}


TFloat abstractDiGraph::FindCap(TArc* pred,TNode u,TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("FindCap",u);

    if (v>=n) NoSuchNode("FindCap",v);

    if (!pred) Error(ERR_REJECTED,"FindCap","Missing predecessor labels");

    TNode PathLength = 0;

    #endif

    TNode w = v;
    TFloat Lambda = InfFloat;

    do
    {
        TArc a = pred[w];

        #if defined(_FAILSAVE_)

        if (PathLength>=n || a==NoArc)
        {
            InternalError("FindCap","Missing start node");
        }

        PathLength++;

        #endif

        if (ResCap(a)<Lambda) Lambda = ResCap(a);

        w = StartNode(a);
    }
    while (w!=u);

    return Lambda;
}


void abstractDiGraph::AdjustDivergence(TArc a,TFloat Lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("AdjustDivergence",a);

    if (Lambda<0)
        Error(ERR_REJECTED,"AdjustDivergence","Amount should be non-negative");

    #endif

    TNode u = StartNode(a);
    TNode v = EndNode(a);

    if (sDegIn!=NULL)
    {
        if (a&1)
        {
            sDegIn[u] -= Lambda;
            sDegOut[v] -= Lambda;
        }
        else
        {
            sDegOut[u] += Lambda;
            sDegIn[v] += Lambda;
        }
    }
}


TFloat abstractDiGraph::FlowValue(TNode s,TNode t) throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("FlowValue",s);

    if (t>=n) NoSuchNode("FlowValue",t);

    #endif

    LogEntry(LOG_METH,"Checking the flow labels...");

    for (TNode v=0;v<n;v++)
    {
        if (Divergence(v)!=Demand(v) && !(v==s || v==t))
            Error(ERR_CHECK,"FlowValue","Not a legal st-flow");
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Flow value: %g",Divergence(t));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return Divergence(t);
}


bool abstractDiGraph::Compatible() throw()
{
    LogEntry(LOG_METH,"Checking reduced length labels...");

    TFloat* potential = GetPotentials();

    for (TArc a=0;a<2*m;a++)
    {
        if (ResCap(a)>0 && RedLength(potential,a)<-CT.epsilon)
        {
            LogEntry(LOG_RES,"...solutions are not compatible");
            return false;
        }
    }

    LogEntry(LOG_RES,"...solutions are compatible");

    return true;
}


TFloat abstractDiGraph::MCF_DualObjective() throw()
{
    TFloat ret = 0;
    TFloat* potential = GetPotentials();

    for (TNode v=0;v<n && potential;v++)
    {
        ret += Demand(v)*potential[v];
    }

    for (TArc a=0;a<m;a++)
    {
        TFloat thisLength = RedLength(potential,2*a);

        if (thisLength>0) ret += LCap(2*a)*thisLength;
        if (thisLength<0) ret += UCap(2*a)*thisLength;
    }

    return ret;
}


supersaturatedNodes::supersaturatedNodes(const abstractDiGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),G(_G),delta(_delta)
{
}


supersaturatedNodes::~supersaturatedNodes() throw()
{
}


unsigned long supersaturatedNodes::Size() const throw()
{
    return sizeof(supersaturatedNodes) + managedObject::Allocated();
}


bool supersaturatedNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (const_cast<abstractDiGraph&>(G).Divergence(i)-G.Demand(i) > delta);
}


deficientNodes::deficientNodes(const abstractDiGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),G(_G),delta(_delta)
{
}


deficientNodes::~deficientNodes() throw()
{
}


unsigned long deficientNodes::Size() const throw()
{
    return sizeof(deficientNodes) + managedObject::Allocated();
}


bool deficientNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (const_cast<abstractDiGraph&>(G).Divergence(i)-G.Demand(i) < -delta);
}


residualArcs::residualArcs(const abstractMixedGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TArc>(2*_G.M(),_G.Context()),
    G(_G),delta(_delta)
{
}


residualArcs::~residualArcs() throw()
{
}


unsigned long residualArcs::Size() const throw()
{
    return sizeof(residualArcs) + managedObject::Allocated();
}


bool residualArcs::IsMember(const TArc i) const throw(ERRange)
{
    return ( G.Blocking(i|1) && G.ResCap(i)>delta)
        || (!G.Blocking(i|1) && G.UCap(i)>delta);
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractDigraph.cpp
/// \brief  #abstractDiGraph partial class implementation

#include "abstractDigraph.h"


abstractDiGraph::abstractDiGraph(TNode _n,TArc _m) throw() :
    abstractMixedGraph(_n,_m)
{
    LogEntry(LOG_MEM,"...Abstract digraph allocated");
}


abstractDiGraph::~abstractDiGraph() throw()
{
    LogEntry(LOG_MEM,"...Abstract digraph disallocated");
}


unsigned long abstractDiGraph::Allocated() const throw()
{
    return 0;
}


TFloat abstractDiGraph::Flow(TArc a) const throw(ERRange)
{
    return Sub(a);
}


void abstractDiGraph::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    #endif

    // This looks like cycling code since the default implementation of
    // SetSubRelative() also calls Push(). But note that represented graphs
    // reimplement SetSubRelative(), and all problem reimplement Push().
    // So, no recurrency occurs for concrete objects.

    SetSubRelative(a, (a&1) ? -Lambda : Lambda);
}


void abstractDiGraph::Augment(TArc* pred,TNode u,TNode v,TFloat Lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Augment",u);

    if (v>=n) NoSuchNode("Augment",v);

    if (Lambda<=0) Error(ERR_REJECTED,"Augment","Amount should be positive");

    if (!pred) Error(ERR_REJECTED,"Augment","Missing predecessor labels");

    TNode PathLength = 0;

    #endif

    TNode w = v;

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Augmenting by %g units of flow...",
            static_cast<double>(Lambda));
        LogEntry(LOG_METH2,CT.logBuffer);
        LogEntry(LOG_METH2,"Path in reverse order:");
        OpenFold();
        sprintf(CT.logBuffer,"(%lu",static_cast<unsigned long>(w));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    do
    {
        TArc a = pred[w];

        #if defined(_FAILSAVE_)

        if (PathLength>=n || a==NoArc)
        {
            InternalError("Augment","Missing start node");
        }

        PathLength++;

        #endif

        Push(a,Lambda);
        pred[w] = NoArc;
        w = StartNode(a);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"[%lu]%lu",
                static_cast<unsigned long>(a),static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }
    while (w!=u);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        LogEnd(LH,")");
        CloseFold();
    }

    #endif
}


TFloat abstractDiGraph::FindCap(TArc* pred,TNode u,TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("FindCap",u);

    if (v>=n) NoSuchNode("FindCap",v);

    if (!pred) Error(ERR_REJECTED,"FindCap","Missing predecessor labels");

    TNode PathLength = 0;

    #endif

    TNode w = v;
    TFloat Lambda = InfFloat;

    do
    {
        TArc a = pred[w];

        #if defined(_FAILSAVE_)

        if (PathLength>=n || a==NoArc)
        {
            InternalError("FindCap","Missing start node");
        }

        PathLength++;

        #endif

        if (ResCap(a)<Lambda) Lambda = ResCap(a);

        w = StartNode(a);
    }
    while (w!=u);

    return Lambda;
}


void abstractDiGraph::AdjustDivergence(TArc a,TFloat Lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("AdjustDivergence",a);

    if (Lambda<0)
        Error(ERR_REJECTED,"AdjustDivergence","Amount should be non-negative");

    #endif

    TNode u = StartNode(a);
    TNode v = EndNode(a);

    if (sDegIn!=NULL)
    {
        if (a&1)
        {
            sDegIn[u] -= Lambda;
            sDegOut[v] -= Lambda;
        }
        else
        {
            sDegOut[u] += Lambda;
            sDegIn[v] += Lambda;
        }
    }
}


TFloat abstractDiGraph::FlowValue(TNode s,TNode t) throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("FlowValue",s);

    if (t>=n) NoSuchNode("FlowValue",t);

    #endif

    LogEntry(LOG_METH,"Checking the flow labels...");

    for (TNode v=0;v<n;v++)
    {
        if (Divergence(v)!=Demand(v) && !(v==s || v==t))
            Error(ERR_CHECK,"FlowValue","Not a legal st-flow");
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Flow value: %g",Divergence(t));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return Divergence(t);
}


bool abstractDiGraph::Compatible() throw()
{
    LogEntry(LOG_METH,"Checking reduced length labels...");

    TFloat* potential = GetPotentials();

    for (TArc a=0;a<2*m;a++)
    {
        if (ResCap(a)>0 && RedLength(potential,a)<-CT.epsilon)
        {
            LogEntry(LOG_RES,"...solutions are not compatible");
            return false;
        }
    }

    LogEntry(LOG_RES,"...solutions are compatible");

    return true;
}


TFloat abstractDiGraph::MCF_DualObjective() throw()
{
    TFloat ret = 0;
    TFloat* potential = GetPotentials();

    for (TNode v=0;v<n && potential;v++)
    {
        ret += Demand(v)*potential[v];
    }

    for (TArc a=0;a<m;a++)
    {
        TFloat thisLength = RedLength(potential,2*a);

        if (thisLength>0) ret += LCap(2*a)*thisLength;
        if (thisLength<0) ret += UCap(2*a)*thisLength;
    }

    return ret;
}


supersaturatedNodes::supersaturatedNodes(const abstractDiGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),G(_G),delta(_delta)
{
}


supersaturatedNodes::~supersaturatedNodes() throw()
{
}


unsigned long supersaturatedNodes::Size() const throw()
{
    return sizeof(supersaturatedNodes) + managedObject::Allocated();
}


bool supersaturatedNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (const_cast<abstractDiGraph&>(G).Divergence(i)-G.Demand(i) > delta);
}


deficientNodes::deficientNodes(const abstractDiGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TNode>(_G.N(),_G.Context()),G(_G),delta(_delta)
{
}


deficientNodes::~deficientNodes() throw()
{
}


unsigned long deficientNodes::Size() const throw()
{
    return sizeof(deficientNodes) + managedObject::Allocated();
}


bool deficientNodes::IsMember(const TNode i) const throw(ERRange)
{
    return (const_cast<abstractDiGraph&>(G).Divergence(i)-G.Demand(i) < -delta);
}


residualArcs::residualArcs(const abstractMixedGraph& _G,TCap _delta) throw() :
    managedObject(_G.Context()),indexSet<TArc>(2*_G.M(),_G.Context()),
    G(_G),delta(_delta)
{
}


residualArcs::~residualArcs() throw()
{
}


unsigned long residualArcs::Size() const throw()
{
    return sizeof(residualArcs) + managedObject::Allocated();
}


bool residualArcs::IsMember(const TArc i) const throw(ERRange)
{
    return ( G.Blocking(i|1) && G.ResCap(i)>delta)
        || (!G.Blocking(i|1) && G.UCap(i)>delta);
}
