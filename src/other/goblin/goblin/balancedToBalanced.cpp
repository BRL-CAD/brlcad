
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   balancedToBalanced.cpp
/// \brief  #balancedToBalanced class implementation

#include "balancedToBalanced.h"
#include "dynamicStack.h"


balancedToBalanced::balancedToBalanced(abstractBalancedFNW& _G) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(_G.N1()+2,_G.M()/2+3),
    G(_G)
{
    LogEntry(LOG_MEM,"Canceling odd cycles...");

    if (!CT.logMem && CT.logMan)
        LogEntry(LOG_MAN,"Canceling odd cycles...");

    OpenFold();

    symm = 1;
    n0 = G.N();
    m0 = G.M();
    CheckLimits();

    s1 = n-1;
    t1 = n-2;
    s2 = n-3;
    t2 = n-4;

    G.MakeRef();

    artifical = new TArc[n0];

    for (TNode i=0;i<n0;i++) artifical[i] = NoArc;

    k2 = 0;
    dynamicStack<TNode> S(n,CT);

    for (TNode root=0;root<n0;root++)
    {
        if (G.Q[root]!=NoArc)
        {
            TNode v = root;

            while (G.Pi(v)>0) v = G.StartNode(G.Q[v]);

            TNode cv = v^1;

            G.MakeIntegral(G.Q,v,cv);
            artifical[v] = 2*m0+4*k2+2;
            artifical[cv] = 2*m0+4*k2+1;
            S.Insert(v);
            S.Insert(cv);
            k2++;
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"%lu odd length cycles eliminated",static_cast<unsigned long>(k2));
        LogEntry(LOG_RES,CT.logBuffer);
    }


    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    #endif

    G.ReleaseCycles();

    k1 = k2/2;
    m = m0+2*k2+6;

    ret2 = 2*m-12;
    art1 = 2*m-8;
    art2 = 2*m-6;
    ret1 = 2*m-4;

    repr = new TNode[2*k2];

    for (TNode i=0;i<k2;i++)
    {
        repr[2*(k2-i-1)] = S.Delete();
        repr[2*(k2-i-1)+1] = S.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            if (i==0)
            {
                sprintf(CT.logBuffer,"Canonical elements: %lu",
                    static_cast<unsigned long>(repr[2*(k2-i-1)]));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }
            else
            {
                sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(repr[2*(k2-i-1)]));
                LogAppend(LH,CT.logBuffer);
            }
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    flow = new TFloat[2*k2+6];

    for (TNode i=0;i<2*k2;i++) flow[i] = 1;

    flow[2*k2] = flow[2*k2+1] = k1;
    flow[2*k2+2] = flow[2*k2+3] = flow[2*k2+4] = flow[2*k2+5] = 0;

    TFloat* piG = G.GetPotentials();

    if (piG)
    {
        TFloat* potential = RawPotentials();

        for (TNode i=0;i<n0;i++) potential[i] = piG[i];

        potential[s1] = potential[s2] = potential[t1] = potential[t2] = 0;
    }

    CloseFold();

    if (CT.traceLevel==2) Display();
}


unsigned long balancedToBalanced::Size() const throw()
{
    return
          sizeof(balancedToBalanced)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + abstractBalancedFNW::Allocated()
        + balancedToBalanced::Allocated();
}


unsigned long balancedToBalanced::Allocated() const throw()
{
    unsigned long tmpSize
        = (2*k2+6)*sizeof(TFloat)     // flow
        + n0*sizeof(TArc)             // artifical
        + 2*k2*sizeof(TNode);         // repr

    return tmpSize;
}


balancedToBalanced::~balancedToBalanced() throw()
{
    if (CT.traceLevel==2) Display();

    ReleaseCycles();
    G.ReleaseRef();

    delete[] flow;
    delete[] artifical;
    delete[] repr;

    LogEntry(LOG_MEM,"...Balanced flow network disallocated");
}


bool balancedToBalanced::Perfect() const throw()
{
    bool solved = true;

    THandle H = Investigate();

    while (Active(H,s1))
    {
        TArc a = Read(H,s1);
        if (BalCap(a)>0) solved = 0;
    }

    Close(H);

    if (CT.logRes)
    {
        if (solved) LogEntry(LOG_RES,"...Flow is perfect");
        else LogEntry(LOG_RES,"...Flow is deficient");
    }

    return solved;
}


void balancedToBalanced::Relax() throw()
{ 
    if (symm)
    {
        LogEntry(LOG_MEM,"Relaxing symmetry...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Relaxing symmetry...");

        G.Relax();
        symm = 0;
    }
}


void balancedToBalanced::Symmetrize() throw()
{
    if (!symm)
    {
        LogEntry(LOG_MEM,"Symmetrizing flow...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Symmetrizing flow...");

        G.Symmetrize();

        for (TArc i=0;i<TArc(k2+3);++i)
        {
            TFloat Lambda = (flow[2*i]+flow[2*i+1])/2;
            flow[2*i] = flow[2*i+1] = Lambda;
        }

        TFloat* potential = GetPotentials();

        for (TArc i=0;i<n1 && potential;i++)
        {
            TFloat thisPi = (potential[2*i]-potential[2*i+1])/2;
            potential[2*i] = thisPi;
            potential[2*i+1] = -thisPi;
        }

        symm = 1;
    }
}


TNode balancedToBalanced::StartNode(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.StartNode(a);

    if (d2<m0+2*k2)
    {
        TArc r4 = a%4;

        switch (r4)
        {
            case 0: return s2;
            case 1: return repr[d2-m0];
            case 2: return repr[d2-m0];
            case 3: return t2;
            default: return NoNode;
        }
    }

    TArc r2 = a%2;

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

    TArc d4 = a/4;

    if (d4==ret2/4)
    {
        if (r2) return s2;
        else return t2;
    }

    if (d4==ret1/4)
    {
        if (r2) return s1;
        else return t1;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("StartNode",a);

    #endif

    throw ERRange();
}


TNode balancedToBalanced::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return balancedToBalanced::StartNode(a^1);
}


TArc balancedToBalanced::First(TNode v) const throw(ERRange)
{
    if (v<n0) return G.First(v);
    if (v==t2) return ret2;
    if (v==s2) return ret2^1;
    if (v==t1) return ret1;
    if (v==s1) return ret1^1;

    #if defined(_FAILSAVE_)

    NoSuchNode("First",v);

    #endif

    throw ERRange();
}


TArc balancedToBalanced::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u!=StartNode(a))
        Error(ERR_REJECTED,"Right","Mismatching start nodes");

    #endif

    TArc d2 = a/2;

    if (d2<m0)
    {
        TArc ar = G.Right(a,u);
        TNode u = G.StartNode(a);

        if (ar!=G.First(u) || artifical[u]==NoArc) return ar;
        else return artifical[u];
    }

    TArc r4 = a%4;

    if (d2<m0+2*(k2-1))
    {
        switch (r4)
        {
            case 0: return a+4;
            case 1: return G.First(repr[d2-m0]);
            case 2: return G.First(repr[d2-m0]);
            case 3: return a+4;
            default: return NoNode;
        }
    }

    if (d2<m0+2*k2)
    {
        switch (r4)
        {
            case 0: return art2;
            case 1: return G.First(repr[d2-m0]);
            case 2: return G.First(repr[d2-m0]);
            case 3: return art1^1;
            default: return NoNode;
        }
    }

    TArc d4 = a/4;

    if (d4==(art1/4))
    {
        switch (r4)
        {
            case 0: return ret1^1;
            case 1: return ret2;
            case 2: return ret2^1;
            case 3: return ret1;
            default: return NoArc;
        }
    }

    if (d4==(ret2/4))
    {
        switch (r4)
        {
            case 0: return ret2^2;
            case 1: return ret2^3;
            case 2:
            {
                if (k2>0) return artifical[repr[1]]^1;
                else return art1^1;
            }
            case 3:
            {
                if (k2>0) return artifical[repr[0]]^1;
                else return art2;
            }
            default: return NoArc;
        }
    }

    if (d4==ret1/4)
    {
        switch (r4)
        {
            case 0: return ret1^2;
            case 1: return ret1^3;
            case 2: return art2^1;
            case 3: return art1;
            default: return NoArc;
        }
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("Right",a);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::Demand(TNode v) const throw(ERRange)
{
    if (v<n0) return G.Demand(v);

    if (v==t2) return 0;
    if (v==s2) return 0;
    if (v==t1) return k1;
    if (v==s1) return -k1;

    #if defined(_FAILSAVE_)

    NoSuchNode("Demand",v);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::UCap(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.UCap(a);
    if (d2<m0+2*k2) return 1;

    TArc d4 = a/4;

    if (d4==(ret2/4)) return k1;
    if (d4==(art1/4)) return k2;
    if (d4==(ret1/4)) return 0; // k1;

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::LCap(TArc a) const throw(ERRange)
{ 
    TArc d2 = a/2;

    if (d2<m0) return G.LCap(a);
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("LCap",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::Length(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.Length(a);
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Length",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::C(TNode v,TDim i) const throw(ERRange)
{
    if (i==0)
    {
        if (v<n0) return G.C(v,0)+10;
        if (v==s1) return G.CMax(0)+15;
        if (v==t1) return 5;
        if (v==s2) return 0;
        if (v==t2) return G.CMax(0)+20;
    }
    else
    {
        if (v<n0) return G.C(v,1);
        if (v==s1) return G.CMax(1)+5;
        if (v==t1) return G.CMax(1)+5;
        if (v==s2) return G.CMax(1)/2;
        if (v==t2) return G.CMax(1)/2;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::CMax(TDim i) const throw(ERRange)
{
    if (G.Dim()==0) return 0;
    if (i==0) return G.CMax(0)+20;
    if (i==1) return G.CMax(1)+5;

    #if defined(_FAILSAVE_)

    NoSuchCoordinate("CMax",i);

    #endif

    throw ERRange();
}


bool balancedToBalanced::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<n0) return G.HiddenNode(v);
    if (v<n) return true;

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::Flow(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.Flow(a);
    if (d2<m-2) return flow[d2-m0];
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Flow",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::BalFlow(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!symm)
    {
        Error(ERR_REJECTED,"BalFlow","Flow is not balanced");
    }

    #endif

    TArc d2 = a/2;

    if (d2<m0) return G.BalFlow(a);
    if (d2<m-2) return flow[d2-m0];
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("BalFlow",a);

    #endif

    throw ERRange();
}


void balancedToBalanced::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    #endif

    Relax();

    if (a<2*m0) G.Push(a,Lambda);
    else
    {
        TArc a0 = (a/2)-m0;

        if (a&1) flow[a0] = flow[a0]-Lambda;
        else flow[a0] = flow[a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
}


void balancedToBalanced::BalPush(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalPush",a);

    if (Lambda<0 || Lambda>BalCap(a)) AmountOutOfRange("BalPush",Lambda);

    #endif

    if (!symm) Symmetrize();

    if (a<2*m0) G.BalPush(a,Lambda);
    else
    {
        TArc a0 = (a-2*m0)/4;

        if (a&1) flow[2*a0] = flow[2*a0+1] = flow[2*a0]-Lambda;
        else flow[2*a0] = flow[2*a0+1] = flow[2*a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
    AdjustDivergence(a^2,Lambda);
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   balancedToBalanced.cpp
/// \brief  #balancedToBalanced class implementation

#include "balancedToBalanced.h"
#include "dynamicStack.h"


balancedToBalanced::balancedToBalanced(abstractBalancedFNW& _G) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(_G.N1()+2,_G.M()/2+3),
    G(_G)
{
    LogEntry(LOG_MEM,"Canceling odd cycles...");

    if (!CT.logMem && CT.logMan)
        LogEntry(LOG_MAN,"Canceling odd cycles...");

    OpenFold();

    symm = 1;
    n0 = G.N();
    m0 = G.M();
    CheckLimits();

    s1 = n-1;
    t1 = n-2;
    s2 = n-3;
    t2 = n-4;

    G.MakeRef();

    artifical = new TArc[n0];

    for (TNode i=0;i<n0;i++) artifical[i] = NoArc;

    k2 = 0;
    dynamicStack<TNode> S(n,CT);

    for (TNode root=0;root<n0;root++)
    {
        if (G.Q[root]!=NoArc)
        {
            TNode v = root;

            while (G.Pi(v)>0) v = G.StartNode(G.Q[v]);

            TNode cv = v^1;

            G.MakeIntegral(G.Q,v,cv);
            artifical[v] = 2*m0+4*k2+2;
            artifical[cv] = 2*m0+4*k2+1;
            S.Insert(v);
            S.Insert(cv);
            k2++;
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"%lu odd length cycles eliminated",static_cast<unsigned long>(k2));
        LogEntry(LOG_RES,CT.logBuffer);
    }


    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    #endif

    G.ReleaseCycles();

    k1 = k2/2;
    m = m0+2*k2+6;

    ret2 = 2*m-12;
    art1 = 2*m-8;
    art2 = 2*m-6;
    ret1 = 2*m-4;

    repr = new TNode[2*k2];

    for (TNode i=0;i<k2;i++)
    {
        repr[2*(k2-i-1)] = S.Delete();
        repr[2*(k2-i-1)+1] = S.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            if (i==0)
            {
                sprintf(CT.logBuffer,"Canonical elements: %lu",
                    static_cast<unsigned long>(repr[2*(k2-i-1)]));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }
            else
            {
                sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(repr[2*(k2-i-1)]));
                LogAppend(LH,CT.logBuffer);
            }
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    flow = new TFloat[2*k2+6];

    for (TNode i=0;i<2*k2;i++) flow[i] = 1;

    flow[2*k2] = flow[2*k2+1] = k1;
    flow[2*k2+2] = flow[2*k2+3] = flow[2*k2+4] = flow[2*k2+5] = 0;

    TFloat* piG = G.GetPotentials();

    if (piG)
    {
        TFloat* potential = RawPotentials();

        for (TNode i=0;i<n0;i++) potential[i] = piG[i];

        potential[s1] = potential[s2] = potential[t1] = potential[t2] = 0;
    }

    CloseFold();

    if (CT.traceLevel==2) Display();
}


unsigned long balancedToBalanced::Size() const throw()
{
    return
          sizeof(balancedToBalanced)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + abstractBalancedFNW::Allocated()
        + balancedToBalanced::Allocated();
}


unsigned long balancedToBalanced::Allocated() const throw()
{
    unsigned long tmpSize
        = (2*k2+6)*sizeof(TFloat)     // flow
        + n0*sizeof(TArc)             // artifical
        + 2*k2*sizeof(TNode);         // repr

    return tmpSize;
}


balancedToBalanced::~balancedToBalanced() throw()
{
    if (CT.traceLevel==2) Display();

    ReleaseCycles();
    G.ReleaseRef();

    delete[] flow;
    delete[] artifical;
    delete[] repr;

    LogEntry(LOG_MEM,"...Balanced flow network disallocated");
}


bool balancedToBalanced::Perfect() const throw()
{
    bool solved = true;

    THandle H = Investigate();

    while (Active(H,s1))
    {
        TArc a = Read(H,s1);
        if (BalCap(a)>0) solved = 0;
    }

    Close(H);

    if (CT.logRes)
    {
        if (solved) LogEntry(LOG_RES,"...Flow is perfect");
        else LogEntry(LOG_RES,"...Flow is deficient");
    }

    return solved;
}


void balancedToBalanced::Relax() throw()
{ 
    if (symm)
    {
        LogEntry(LOG_MEM,"Relaxing symmetry...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Relaxing symmetry...");

        G.Relax();
        symm = 0;
    }
}


void balancedToBalanced::Symmetrize() throw()
{
    if (!symm)
    {
        LogEntry(LOG_MEM,"Symmetrizing flow...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Symmetrizing flow...");

        G.Symmetrize();

        for (TArc i=0;i<TArc(k2+3);++i)
        {
            TFloat Lambda = (flow[2*i]+flow[2*i+1])/2;
            flow[2*i] = flow[2*i+1] = Lambda;
        }

        TFloat* potential = GetPotentials();

        for (TArc i=0;i<n1 && potential;i++)
        {
            TFloat thisPi = (potential[2*i]-potential[2*i+1])/2;
            potential[2*i] = thisPi;
            potential[2*i+1] = -thisPi;
        }

        symm = 1;
    }
}


TNode balancedToBalanced::StartNode(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.StartNode(a);

    if (d2<m0+2*k2)
    {
        TArc r4 = a%4;

        switch (r4)
        {
            case 0: return s2;
            case 1: return repr[d2-m0];
            case 2: return repr[d2-m0];
            case 3: return t2;
            default: return NoNode;
        }
    }

    TArc r2 = a%2;

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

    TArc d4 = a/4;

    if (d4==ret2/4)
    {
        if (r2) return s2;
        else return t2;
    }

    if (d4==ret1/4)
    {
        if (r2) return s1;
        else return t1;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("StartNode",a);

    #endif

    throw ERRange();
}


TNode balancedToBalanced::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return balancedToBalanced::StartNode(a^1);
}


TArc balancedToBalanced::First(TNode v) const throw(ERRange)
{
    if (v<n0) return G.First(v);
    if (v==t2) return ret2;
    if (v==s2) return ret2^1;
    if (v==t1) return ret1;
    if (v==s1) return ret1^1;

    #if defined(_FAILSAVE_)

    NoSuchNode("First",v);

    #endif

    throw ERRange();
}


TArc balancedToBalanced::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u!=StartNode(a))
        Error(ERR_REJECTED,"Right","Mismatching start nodes");

    #endif

    TArc d2 = a/2;

    if (d2<m0)
    {
        TArc ar = G.Right(a,u);
        TNode u = G.StartNode(a);

        if (ar!=G.First(u) || artifical[u]==NoArc) return ar;
        else return artifical[u];
    }

    TArc r4 = a%4;

    if (d2<m0+2*(k2-1))
    {
        switch (r4)
        {
            case 0: return a+4;
            case 1: return G.First(repr[d2-m0]);
            case 2: return G.First(repr[d2-m0]);
            case 3: return a+4;
            default: return NoNode;
        }
    }

    if (d2<m0+2*k2)
    {
        switch (r4)
        {
            case 0: return art2;
            case 1: return G.First(repr[d2-m0]);
            case 2: return G.First(repr[d2-m0]);
            case 3: return art1^1;
            default: return NoNode;
        }
    }

    TArc d4 = a/4;

    if (d4==(art1/4))
    {
        switch (r4)
        {
            case 0: return ret1^1;
            case 1: return ret2;
            case 2: return ret2^1;
            case 3: return ret1;
            default: return NoArc;
        }
    }

    if (d4==(ret2/4))
    {
        switch (r4)
        {
            case 0: return ret2^2;
            case 1: return ret2^3;
            case 2:
            {
                if (k2>0) return artifical[repr[1]]^1;
                else return art1^1;
            }
            case 3:
            {
                if (k2>0) return artifical[repr[0]]^1;
                else return art2;
            }
            default: return NoArc;
        }
    }

    if (d4==ret1/4)
    {
        switch (r4)
        {
            case 0: return ret1^2;
            case 1: return ret1^3;
            case 2: return art2^1;
            case 3: return art1;
            default: return NoArc;
        }
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("Right",a);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::Demand(TNode v) const throw(ERRange)
{
    if (v<n0) return G.Demand(v);

    if (v==t2) return 0;
    if (v==s2) return 0;
    if (v==t1) return k1;
    if (v==s1) return -k1;

    #if defined(_FAILSAVE_)

    NoSuchNode("Demand",v);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::UCap(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.UCap(a);
    if (d2<m0+2*k2) return 1;

    TArc d4 = a/4;

    if (d4==(ret2/4)) return k1;
    if (d4==(art1/4)) return k2;
    if (d4==(ret1/4)) return 0; // k1;

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TCap balancedToBalanced::LCap(TArc a) const throw(ERRange)
{ 
    TArc d2 = a/2;

    if (d2<m0) return G.LCap(a);
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("LCap",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::Length(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.Length(a);
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Length",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::C(TNode v,TDim i) const throw(ERRange)
{
    if (i==0)
    {
        if (v<n0) return G.C(v,0)+10;
        if (v==s1) return G.CMax(0)+15;
        if (v==t1) return 5;
        if (v==s2) return 0;
        if (v==t2) return G.CMax(0)+20;
    }
    else
    {
        if (v<n0) return G.C(v,1);
        if (v==s1) return G.CMax(1)+5;
        if (v==t1) return G.CMax(1)+5;
        if (v==s2) return G.CMax(1)/2;
        if (v==t2) return G.CMax(1)/2;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::CMax(TDim i) const throw(ERRange)
{
    if (G.Dim()==0) return 0;
    if (i==0) return G.CMax(0)+20;
    if (i==1) return G.CMax(1)+5;

    #if defined(_FAILSAVE_)

    NoSuchCoordinate("CMax",i);

    #endif

    throw ERRange();
}


bool balancedToBalanced::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<n0) return G.HiddenNode(v);
    if (v<n) return true;

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::Flow(TArc a) const throw(ERRange)
{
    TArc d2 = a/2;

    if (d2<m0) return G.Flow(a);
    if (d2<m-2) return flow[d2-m0];
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Flow",a);

    #endif

    throw ERRange();
}


TFloat balancedToBalanced::BalFlow(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!symm)
    {
        Error(ERR_REJECTED,"BalFlow","Flow is not balanced");
    }

    #endif

    TArc d2 = a/2;

    if (d2<m0) return G.BalFlow(a);
    if (d2<m-2) return flow[d2-m0];
    if (d2<m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("BalFlow",a);

    #endif

    throw ERRange();
}


void balancedToBalanced::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    #endif

    Relax();

    if (a<2*m0) G.Push(a,Lambda);
    else
    {
        TArc a0 = (a/2)-m0;

        if (a&1) flow[a0] = flow[a0]-Lambda;
        else flow[a0] = flow[a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
}


void balancedToBalanced::BalPush(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalPush",a);

    if (Lambda<0 || Lambda>BalCap(a)) AmountOutOfRange("BalPush",Lambda);

    #endif

    if (!symm) Symmetrize();

    if (a<2*m0) G.BalPush(a,Lambda);
    else
    {
        TArc a0 = (a-2*m0)/4;

        if (a&1) flow[2*a0] = flow[2*a0+1] = flow[2*a0]-Lambda;
        else flow[2*a0] = flow[2*a0+1] = flow[2*a0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
    AdjustDivergence(a^2,Lambda);
}
