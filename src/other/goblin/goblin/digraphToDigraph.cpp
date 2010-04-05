
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   digraphToDigraph.cpp
/// \brief  #digraphToDigraph class implementation


#include "digraphToDigraph.h"
#include "sparseDigraph.h"
#include "moduleGuard.h"


digraphToDigraph::digraphToDigraph(abstractDiGraph& _G,TFloat* _potential) throw(ERRejected) :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N()+2,_G.M()+2*_G.N()+1),
    G(_G), piG(_potential)
{
    n0 = G.N();
    m0 = G.M();
    CheckLimits();

    s1 = n-1;
    t1 = n-2;
    ret1 = 2*(m-1);

    cap = new TCap[2*n0+1];
    flow = new TFloat[2*n0+1];

    G.MakeRef();

    if (piG)
    {
        if (CT.logMem || CT.logMan)
        {
            LogEntry(LOG_MAN,"Repairing complementary slackness...");
            if (!CT.logMan && CT.logMem)
                LogEntry(LOG_MEM,"Repairing complementary slackness...");
        }

        for (TArc i=0;i<m0;i++)
        {
            TArc a = 2*i;

            if (G.RedLength(piG,a)<0 && G.ResCap(a)>0)
            {
                G.Push(a,G.ResCap(a));
            }
            else if (G.RedLength(piG,a)>0 && G.ResCap(a^1)>0)
            {
                G.Push(a^1,G.ResCap(a^1));
            }
        }
    }

    if (CT.logMem || CT.logMan)
    {
        LogEntry(LOG_MAN,"Eliminating node imbalances...");

        if (!CT.logMan && CT.logMem)
            LogEntry(LOG_MEM,"Eliminating node imbalances...");
    }

    TFloat checkSum = 0;

    for (TNode v=0;v<n0;v++) flow[v] = 0;

    #if defined(_FAILSAVE_)

    if (checkSum!=0)
        Error(ERR_REJECTED,"digraphToDigraph","Node demands do not resolve");

    #endif

    for (TArc i=0;i<m0;i++)
    {
        TArc a = 2*i;
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);

        if (u==v) continue;

        flow[u] += G.Flow(a);
        flow[v] -= G.Flow(a);
    }

    for (TNode v=0;v<n0;v++)
    {
        TFloat thisDiv = flow[v]+G.Demand(v);

        if (thisDiv<0)
        {
            cap[v+n0] = flow[v+n0] = -thisDiv;
            cap[v] = flow[v] = 0;
        }
        else
        {
            cap[v] = flow[v] = thisDiv;
            cap[v+n0] = flow[v+n0] = 0;
        }
    }

    flow[2*n0] = cap[2*n0] = 0;

    if (CT.traceLevel==2) Display();
}


unsigned long digraphToDigraph::Size() const throw()
{
    return
          sizeof(digraphToDigraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + digraphToDigraph::Allocated();
}


unsigned long digraphToDigraph::Allocated() const throw()
{
    unsigned long tmpSize
        = (2*n0+1)*sizeof(TCap)     // cap[]
        + (2*n0+1)*sizeof(TFloat);  // flow[]

    return tmpSize;
}


digraphToDigraph::~digraphToDigraph() throw()
{
    if (CT.traceLevel==2) Display();

    TFloat* potential = GetPotentials();

    if (piG && potential)
    {
        LogEntry(LOG_MAN,"Updating original node potentials...");
        if (!CT.logMan && CT.logMem)
            LogEntry(LOG_MEM,"Updating original node potentials...");

        for (TNode v=0;v<n0;v++) piG[v] += potential[v];
    }

    G.ReleaseRef();

    delete[] cap;
    delete[] flow;
}


bool digraphToDigraph::Perfect() const throw()
{
    bool solved = true;

    for (TNode v=0;v<n0;v++)
        if (flow[v]>0) solved = false;

    if (CT.logRes)
    {
        if (solved) LogEntry(LOG_RES,"...Flow corresponds to a circulation");
        else LogEntry(LOG_RES,"...Flow does not correspond to a circulation");
    }

    return solved;
}


TNode digraphToDigraph::StartNode(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;
    TArc r2 = a%2;

    if (d2<m0) return G.StartNode(a);

    if (d2<m0+n0)
    {
        if (r2) return (TNode)(d2-m0);
        else return t1;
    }

    if (d2<m0+2*n0)
    {
        if (r2) return s1;
        else return (TNode)(d2-m0)-n0;
    }

    if (d2==m0+2*n0)
    {
        if (r2) return s1;
        else return t1;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("StartNode",a);

    #endif

    throw ERRange();
}


TNode digraphToDigraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return digraphToDigraph::StartNode(a^1);
}


TArc digraphToDigraph::First(TNode v) const throw(ERRange)
{
    if (v<n0) return G.First(v);
    if (v==t1) return ret1;
    if (v==s1) return ret1^1;

    #if defined(_FAILSAVE_)

    NoSuchNode("First",v);

    #endif

    throw ERRange();
}


TArc digraphToDigraph::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u!=StartNode(a))
        Error(ERR_REJECTED,"Right","Mismatching start nodes");

    #endif

    TArc d2 = a/2;
    TArc r2 = a%2;

    if (d2<m0)
    {
        TArc ar = G.Right(a,u);

        if (ar!=G.First(u)) return ar;
        else return 2*(m0+u)+1;
    }

    if (d2<m0+n0)
    {
        if (r2) return 2*(d2+n0);
        else
        {
            // u == t1
            if (d2==(m0+n0-1)) return ret1;
            else return a+2;
        }
    }

    if (d2<m0+2*n0)
    {
        if (!r2)
        {
            TArc af = G.First(d2-m0-n0);
            if (af==NoArc) return 2*(m0+u)+1;
            else return af;
        }
        else
        {
            // u == s1
            if (d2==(m0+2*n0-1)) return ret1^1;
            else return a+2;
        }
    }

    if (d2==m0+2*n0)
    {
        if (!r2) return 2*m0;
        else return 2*(m0+n0)+1;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("Right",a);

    #endif

    throw ERRange();
}


TCap digraphToDigraph::Demand(TNode v) const throw(ERRange)
{
    if (v<n0) return G.Demand(v);

    if (v==t1 || v==s1) return 0;

    #if defined(_FAILSAVE_)

    NoSuchNode("Demand",v);

    #endif

    throw ERRange();
}


TCap digraphToDigraph::UCap(TArc a) const throw(ERRange)
{
    if (a<2*m0) return G.UCap(a);

    if (a<2*m) return cap[(a>>1)-m0];

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TCap digraphToDigraph::LCap(TArc a) const throw(ERRange)
{
    if (a<2*m0) return G.LCap(a);

    if (a<2*m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TFloat digraphToDigraph::Flow(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Flow",a);

    #endif

    if (a<2*m0) return G.Flow(a);

    return flow[(a>>1)-m0];
}


TFloat digraphToDigraph::Length(TArc a) const throw(ERRange)
{
    if (a<2*m0) return G.RedLength(piG,a^(a&1));
    if (a<2*m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Length",a);

    #endif

    throw ERRange();
}


TFloat digraphToDigraph::C(TNode v,TDim i) const throw(ERRange)
{
    if (i==0)
    {
        if (v<n0) return G.C(v,0)+10;
        if (v==s1) return 0;
        if (v==t1) return G.CMax(0)+20;
    }
    else
    {
        if (v<n0) return G.C(v,1);
        if (v<n) return G.CMax(1)/2;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


TFloat digraphToDigraph::CMax(TDim i) const throw(ERRange)
{
    if (G.Dim()==0) return 0;
    if (i==0) return G.CMax(0)+20;
    if (i==1) return G.CMax(1);

    #if defined(_FAILSAVE_)

    NoSuchCoordinate("CMax",i);

    #endif

    throw ERRange();
}


bool digraphToDigraph::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<n0) return G.HiddenNode(v);
    if (v<n) return true;

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


void digraphToDigraph::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    #endif

    if (a<2*m0) G.Push(a,Lambda);
    else
    {
        TArc a0 = (a>>1)-m0;

        if (a&1) flow[a0] = flow[a0]-Lambda;
        else flow[a0] = flow[a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
}


bool abstractMixedGraph::AdmissibleBFlow() throw()
{
    moduleGuard M(ModMaxFlow,*this,"Computing admissible b-flow...");

    TCap checkSum = 0;

    for (TNode v=0;v<n;v++) checkSum += Demand(v);

    if (checkSum!=0)
    {
        M.Shutdown(LOG_RES,"...Node demands do not resolve");
        return false;
    }

    // Transformation applies to directed graphs only. In all other cases,
    // a complete orientation must be generated
    abstractDiGraph* G = NULL;

    if (IsDirected())
    {
        G = static_cast<abstractDiGraph*>(this);
    }
    else
    {
        G = new completeOrientation(*this);
        static_cast<completeOrientation*>(G) -> MapFlowForward(*this);
    }

    digraphToDigraph* H = new digraphToDigraph(*G);
    H -> MaxFlow(H->DefaultSourceNode(),H->DefaultTargetNode());
    bool ret = H->Perfect();

    delete H;

    if (!IsDirected())
    {
        static_cast<completeOrientation*>(G) -> MapFlowBackward(*this);
        delete G;
    }

    M.Trace();

    return ret;
}
