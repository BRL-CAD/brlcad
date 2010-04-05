
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   bigraphToDigraph.cpp
/// \brief  #bigraphToDigraph class implementation

#include "bigraphToDigraph.h"


bigraphToDigraph::bigraphToDigraph(abstractBiGraph& _G,TCap* pLower,TCap* pCap) throw() :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N()+4,_G.M()+2*_G.N()+4),
    G(_G)
{
    cap = pCap;
    lower = pLower;
    ccap = 1;

    Init();
}


bigraphToDigraph::bigraphToDigraph(abstractBiGraph& _G,TCap cCap) throw() :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N()+4,_G.M()+2*_G.N()+4),
    G(_G)
{
    cap = NULL;
    lower = NULL;
    ccap = cCap;

    Init();
}


bigraphToDigraph::bigraphToDigraph(abstractBiGraph& _G) throw() :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N()+4,_G.M()+2*_G.N()+4),
    G(_G)
{
    cap = NULL;

    if (G.CDemand())
    {
        lower = NULL;
        ccap = G.MaxDemand();
    }
    else
    {
        lower = new TCap[n0];

        for (TNode v=0;v<n0;v++) lower[v] = G.Demand(v);
    }

    Init();
}


void bigraphToDigraph::Init() throw()
{
    LogEntry(LOG_MEM,"Transforming into a flow network...");
    if (!CT.logMem && CT.logMan)
        LogEntry(LOG_MAN,"Transforming into a flow network...");

    n0 = G.N();
    n1 = G.N1();
    n2 = G.N2();
    m0 = G.M();
    CheckLimits();

    t2 = n-4;
    s2 = n-3;
    t1 = n-2;
    s1 = n-1;
    ret1 = 2*m-2;
    art1 = 2*m-4;
    ret2 = 2*m-6;
    art2 = 2*m-8;

    // When lower and upper demand bounds are different,
    // do not mainipulate the edge lengths. Otherwise
    // enforce non-negativity of the edge lengths
    minLength = 0;

    for (TArc a=0;a<G.M() && cap!=NULL;a++)
    {
        if (G.Length(2*a)<minLength) minLength = G.Length(2*a);
    }

    for (TArc a=0;a<m0;a++)
    {
        if (G.StartNode(2*a)>=n1 || G.EndNode(2*a)<n1)
            InternalError("bigraphToDigraph","Wrong arc orientations");
    }

    bool feasible = true;

    for (TNode v=0;v<G.N() && feasible;v++)
    {
        if (   (cap!=NULL && G.Deg(v)>cap[v])
            || (lower!=NULL && G.Deg(v)>lower[v])
            || (lower==NULL && G.Deg(v)>ccap))
        {
            feasible = false;
        }
    }

    if (!feasible) G.InitSubgraph();

    if (lower)
    {
        sumLower1 = 0;
        sumLower2 = 0;

        for (TNode v=0;v<n1;v++) sumLower1 += lower[v];
        for (TNode v=n1;v<n0;v++) sumLower2 += lower[v];
    }
    else
    {
        sumLower1 = n1*ccap;
        sumLower2 = n2*ccap;
    }

    if (cap)
    {
        sumUpper = 0;

        for (TNode v=0;v<n0;v++)
        {
            sumUpper += cap[v];
            cap[v] -= lower[v];
        }
    }
    else
    {
        sumUpper = sumLower1+sumLower2;
    }

    dgl = new TFloat[2*n0+4];
    SetDegrees();

    G.MakeRef();

    if (CT.traceLevel==2) Display();
}


void bigraphToDigraph::SetDegrees() throw()
{
    dgl[(art1>>1)-m0] = dgl[(ret1>>1)-m0] = 0;
    TFloat cardinality = 0;
    TFloat surplus1 = 0;
    TFloat surplus2 = 0;
    THandle H = G.Investigate();

    for (TNode v=0;v<n0;v++)
    {
        dgl[v] = 0;

        while (G.Active(H,v))
        {
            TArc a = G.Read(H,v);
            dgl[v] += G.Sub(a);
        }

        cardinality += dgl[v];

        TCap thisCap = UCap(2*(m0+v));

        if (dgl[v]<=thisCap) dgl[v+n0] = 0;
        else
        {
            dgl[v+n0] = dgl[v]-thisCap;
            dgl[v] = thisCap;
            if (v<n1) surplus1 += dgl[v+n0];
            else surplus2 += dgl[v+n0];
        }
    }

    G.Close(H);

    cardinality = cardinality/2;
    dgl[(ret2>>1)-m0] = cardinality;
    dgl[(art1>>1)-m0] = cardinality-surplus2;
    dgl[(art2>>1)-m0] = cardinality-surplus1;
    dgl[(ret1>>1)-m0] = dgl[(art1>>1)-m0]+dgl[(art2>>1)-m0];
}


unsigned long bigraphToDigraph::Size() const throw()
{
    return 
          sizeof(bigraphToDigraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + bigraphToDigraph::Allocated();
}


unsigned long bigraphToDigraph::Allocated() const throw()
{
    unsigned long tmpSize
        = n0*sizeof(TCap)            // lower[]
        + (2*n0+4)*sizeof(TFloat);   // dgl[]

    if (cap!=NULL)    tmpSize += n0*sizeof(TFloat);

    return tmpSize;
}


bool bigraphToDigraph::Perfect() const throw()
{
    bool solved = true;
    THandle H = Investigate();

    while (Active(H,s1) && solved)
    {
        TArc a = Read(H,s1);
        if (ResCap(a)>0) solved = 0;
    }

    Close(H);

    if (CT.logRes)
    {
        if (solved)
            LogEntry(LOG_RES,"...Flow corresponds to a perfect assignment");
        else
            LogEntry(LOG_RES,"...Flow does not correspond to a perfect assignment");
    }

    return solved;
}


bigraphToDigraph::~bigraphToDigraph() throw()
{
    if (CT.traceLevel==2) Display();

    G.ReleaseRef();

    if (cap!=NULL) delete[] cap;

    delete[] lower;
    delete[] dgl;
}


TNode bigraphToDigraph::StartNode(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;
    TArc r2 = a%2;

    if (d2<m0) return G.StartNode(a);

    if (d2<m0+n1)
    {
        if (r2) return (TNode)(d2-m0);
        else return s1;
    }

    if (d2<m0+n0)
    {
        if (r2) return t1;
        else return (TNode)(d2-m0);
    }

    if (d2<m0+n0+n1)
    {
        if (r2) return (TNode)(d2-m0-n0);
        else return s2;
    }

    if (d2<m0+2*n0)
    {
        if (r2) return t2;
        else return (TNode)(d2-m0-n0);
    }

    if (d2==(ret1/2))
    {
        if (r2) return s1;
        else return t1;
    }

    if (d2==(art1/2))
    {
        if (r2) return t2;
        else return s1;
    }

    if (d2==(art2/2))
    {
        if (r2) return t1;
        else return s2;
    }

    if (d2==(ret2/2))
    {
        if (r2) return s2;
        else return t2;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("StartNode",a);

    #endif

    throw ERRange();
}


TNode bigraphToDigraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return bigraphToDigraph::StartNode(a^1);
}


TArc bigraphToDigraph::First(TNode v) const throw(ERRange)
{
    if (v<n1) return 2*(m0+v)+1;
    if (v<n0) return 2*(m0+v);

    if (v==t2) return ret2;
    if (v==s2) return art2;
    if (v==t1) return ret1;
    if (v==s1) return art1;

    #if defined(_FAILSAVE_)

    NoSuchNode("First",v);

    #endif

    throw ERRange();
}


TArc bigraphToDigraph::Right(TArc a,TNode u) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Right",a);

    if (u!=StartNode(a))
    {
        InternalError("Right","Mismatching start nodes");
    }

    #endif

    if (u<n1)
    {
        if (a==2*(m0+u)+1) return 2*(m0+n0+u)+1;

        TArc af = G.First(u);

        if (a==2*(m0+n0+u)+1)
        {
            if (af==NoArc) return 2*(m0+u)+1;
            else return af;
        }

        TArc ar = G.Right(a,u);

        if (ar==af) return 2*(m0+u)+1;

        return ar;
    }

    if (u<n0)
    {
        if (a==2*(m0+u)) return 2*(m0+n0+u);

        TArc af = G.First(u);

        if (a==2*(m0+n0+u))
        {
            if (af==NoArc) return 2*(m0+u);
            else return af;
        }

        TArc ar = G.Right(a,u);

        if (ar==af) return 2*(m0+u);

        return ar;
    }

    if (u==s1)
    {
        if (a==(ret1^1)) return art1;
        if (a==art1) return 2*m0;
        if (a==2*(m0+n1)-2) return (ret1^1);
        return a+2;
    }

    if (u==t1)
    {
        if (a==ret1) return (art2^1);
        if (a==(art2^1)) return 2*(m0+n1)+1;
        if (a==2*(m0+n0)-1) return ret1;
        return a+2;
    }

    if (u==s2)
    {
        if (a==(ret2^1)) return art2;
        if (a==art2) return 2*(m0+n0);
        if (a==2*(m0+n0+n1)-2) return (ret2^1);
        return a+2;
    }

    if (u==t2)
    {
        if (a==ret2) return (art1^1);
        if (a==(art1^1)) return 2*(m0+n0+n1)+1;
        if (a==2*(m0+2*n0)-1) return ret2;
        return a+2;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("Right",u);

    #endif

    throw ERRange();
}


TCap bigraphToDigraph::Demand(TNode v) const throw(ERRange)
{
    if (v<n0+2) return 0;

    if (v==t1) return sumLower1+sumLower2;
    if (v==s1) return -sumLower1-sumLower2;

    #if defined(_FAILSAVE_)

    NoSuchNode("Demand",v);

    #endif

    throw ERRange();
}


TCap bigraphToDigraph::UCap(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.UCap(a);

    if (d2<m0+n0)
    {
        if (lower==NULL) return ccap;
        else return lower[d2-m0];
    }

    if (d2<m0+2*n0)
    {
        if (cap==NULL) return 0;
        else return cap[d2-m0-n0];
    }

    if (d2==(ret2/2)) return sumUpper;
    if (d2==(art1/2)) return sumLower2;
    if (d2==(art2/2)) return sumLower1;
    if (d2==(ret1/2)) return 0; // sumLower1+sumLower2;

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TFloat bigraphToDigraph::Length(TArc a) const throw(ERRange)
{
    if (a<2*m0) return G.Length(a)-minLength;

    if (a<2*m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Length",a);

    #endif

    throw ERRange();
}


TFloat bigraphToDigraph::C(TNode v,TDim i) const throw(ERRange)
{
    if (i==0)
    {
        if (v<n0) return G.C(v,0)+10;
        if (v==s1) return 0;
        if (v==t1) return G.CMax(0)+20;
        if (v==s2) return G.CMax(0)+15;
        if (v==t2) return 5;
    }
    else
    {
        if (v<n0) return G.C(v,1);
        if (v==s1) return G.CMax(1)/2;
        if (v==t1) return G.CMax(1)/2;
        if (v==s2) return G.CMax(1)+5;
        if (v==t2) return G.CMax(1)+5;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


TFloat bigraphToDigraph::CMax(TDim i) const throw(ERRange)
{
    if (G.Dim()==0) return 0;
    if (i==0) return G.CMax(0)+20;
    if (i==1) return G.CMax(1)+5;

    #if defined(_FAILSAVE_)

    NoSuchCoordinate("CMax",i);

    #endif

    throw ERRange();
}


bool bigraphToDigraph::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<n0) return G.HiddenNode(v);
    if (v<n) return true;

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


TFloat bigraphToDigraph::Flow(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.Sub(a);
    if (d2<m-1) return dgl[d2-m0];
    if (d2==m-1) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Flow",a);

    #endif

    throw ERRange();
}


void bigraphToDigraph::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    TArc d2 = a/2;

    #if defined(_FAILSAVE_)

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    if (d2>=m) NoSuchArc("Push",a);

    #endif

    if (d2<m0)
    {
        G.SetSubRelative(a, (a&1) ? -Lambda : Lambda);
    }
    else
    {
        TArc a0 = d2-m0;

        if (a&1) dgl[a0] = dgl[a0]-Lambda;
        else dgl[a0] = dgl[a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
}
