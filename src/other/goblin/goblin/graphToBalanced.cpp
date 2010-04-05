
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   graphToBalanced.cpp
/// \brief  #graphToBalanced class implementation

#include "graphToBalanced.h"
#include "balancedToBalanced.h"
#include "moduleGuard.h"


graphToBalanced::graphToBalanced(abstractGraph& _G,TCap *pLower,TCap *pCap) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(_G.N()+2,_G.M()+2*_G.N()+3),
    G(_G)
{
    cap = pCap;
    lower = pLower;
    ccap = 1;

    Init();
}


graphToBalanced::graphToBalanced(abstractGraph& _G,TCap cCap) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(_G.N()+2,_G.M()+2*_G.N()+3),
    G(_G)
{
    cap = NULL;
    lower = NULL;
    ccap = cCap;

    Init();
}


graphToBalanced::graphToBalanced(abstractGraph& _G) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(_G.N()+2,_G.M()+2*_G.N()+3),
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
        lower = new TCap[G.N()];

        for (TNode v=0;v<G.N();v++) lower[v] = G.Demand(v);
    }

    Init();
}


void graphToBalanced::Init() throw()
{
    LogEntry(LOG_MEM,"Transforming into a balanced flow network...");
    if (!CT.logMem && CT.logMan)
        LogEntry(LOG_MAN,"Transforming into a balanced flow network...");
    OpenFold();

    n0 = G.N();
    m0 = G.M();
    CheckLimits();

    t2 = n-4;
    s2 = n-3;
    t1 = n-2;
    s1 = n-1;
    ret1 = 2*m-4;
    ret2 = 2*m-12;
    art1 = 2*m-8;
    art2 = 2*m-6;

    // When lower and upper demand bounds are different,
    // do not mainipulate the edge lengths. Otherwise
    // enforce non-negativity of the edge lengths
    minLength = 0;

    for (TArc a=0;a<G.M() && cap;a++)
    {
        TFloat thisLength = G.Length(2*a);

        if (thisLength<minLength) minLength = thisLength;
    }

    G.MakeRef();

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

    flow = NULL;
    deg = NULL;

    if (lower==NULL) sumUpper = sumLower = n0*ccap;
    else
    {
        sumLower = 0;

        // Summarize the node degrees
        for (TNode v=0;v<n0;v++) sumLower += lower[v];

        if (cap!=NULL)
        {
            sumUpper = 0;
            for (TNode v=0;v<n0;v++)
            {
                sumUpper += cap[v];
                cap[v] -= lower[v];
            }
        }
        else sumUpper = sumLower;
    }

    if (!G.CLCap() || G.MaxLCap()>0)
    {
        LogEntry(LOG_MAN,"Eliminating lower arc capacities...");

        if (lower==NULL)
        {
            lower = new TCap[G.N()];

            for (TNode v=0;v<G.N();v++) lower[v] = ccap;
        }

        for (TArc a=0;a<2*G.M();a++)
        {
            if (G.LCap(a)>0)
            {
                lower[G.StartNode(a)] -= G.LCap(a);
                sumLower -= G.LCap(a);
                sumUpper -= G.LCap(a);
            }
        }
    }

    Symmetrize();
    CloseFold();

    if (CT.traceLevel==2) Display();
}


unsigned long graphToBalanced::Size() const throw()
{
    return
          sizeof(graphToBalanced)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + abstractBalancedFNW::Allocated()
        + graphToBalanced::Allocated();
}


unsigned long graphToBalanced::Allocated() const throw()
{
    unsigned long tmpSize = 0;

    if (cap!=NULL)      tmpSize += n0*sizeof(TCap);
    if (lower!=NULL)    tmpSize += n0*sizeof(TCap);
    if (deg!=NULL)      tmpSize += (2*n0+3)*sizeof(TFloat);
    if (flow!=NULL)     tmpSize += m*sizeof(TFloat);

    return tmpSize;
}


graphToBalanced::~graphToBalanced() throw()
{
    if (CT.traceLevel==2) Display();

    Symmetrize();
    ReleaseCycles();
    G.ReleaseRef();

    delete[] cap;
    delete[] lower;
    delete[] flow;
    delete[] deg;

    LogEntry(LOG_MEM,"...Balanced flow network disallocated");
}


void graphToBalanced::ExportDecomposition() throw()
{
    TNode* dist = GetNodeColours();
    TNode* nodeColour = G.RawNodeColours();

    for (TNode v=0;v<n0 && dist;v++)
    {
        if (dist[2*v]==NoNode && dist[2*v+1]==NoNode)
        {
            nodeColour[v] = 2;
        }
        else if (dist[2*v]!=NoNode)
        {
            nodeColour[v] = 1;
        }
        else
        {
            nodeColour[v] = 0;
        }
    }
}


bool graphToBalanced::Perfect() const throw()
{
    bool solved = true;
    THandle H = Investigate();

    while (Active(H,s1) && solved)
    {
        TArc a = Read(H,s1);
        if (BalCap(a)>0) solved = false;
    }

    Close(H);

    if (CT.logRes)
    {
        if (solved)
            LogEntry(LOG_RES,"...Flow corresponds to a perfect matching");
        else
            LogEntry(LOG_RES,"...Flow does not correspond to a perfect matching");
    }

    return solved;
}


void graphToBalanced::Symmetrize() throw()
{
    ReleaseDegrees();

    if (deg==NULL)
    {
        LogEntry(LOG_MEM,"Updating subgraph...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Updating subgraph...");

        deg = new TFloat[2*n0+3];

        if (flow!=NULL)
        {
            for (TArc a=0;a<m0;a++)
            {
                TFloat Lambda = (flow[2*a]+flow[2*a+1])/2-G.Sub(2*a)+G.LCap(2*a);
                G.SetSubRelative(2*a,Lambda);
            }

            delete[] flow;
            flow = NULL;
        }

        deg[2*n0] = deg[2*n0+2] = 0;
        TFloat cardinality = 0;
        TFloat surplus = 0;
        THandle H = G.Investigate();

        for (TNode v=0;v<n0;v++)
        {
            deg[v] = 0;

            while (G.Active(H,v))
            {
                TArc a = G.Read(H,v);
                deg[v] += G.Sub(a)-G.LCap(a);
            }

            cardinality += deg[v];

            TCap thisCap = UCap(4*(m0+v));

            if (deg[v]<=thisCap) deg[v+n0] = 0;
            else
            {
                deg[v+n0] = deg[v]-thisCap;
                deg[v] = thisCap;
                surplus += deg[v+n0];
            }
        }

        G.Close(H);

        deg[2*n0] = (cardinality/2);
        deg[2*n0+1] = deg[2*n0+2] = cardinality-surplus;

        TFloat delta1 = UCap(ret2)-deg[2*n0];
        TFloat delta2 = (UCap(art1)-deg[2*n0+1])/2;

        if (delta1<delta2) delta2 = delta1;

        deg[2*n0] += delta2;
        deg[2*n0+1] += 2*delta2;
        deg[2*n0+2] += 2*delta2;
    }
}


void graphToBalanced::Relax() throw()
{
    ReleaseDegrees();

    if (flow==NULL)
    {
        LogEntry(LOG_MEM,"Flow is initialized...");

        if (!CT.logMem && CT.logMan) LogEntry(LOG_MAN,"Flow is initialized...");

        flow = new TFloat[m];

        for (TArc a=0;a<m0;a++) flow[2*a] = flow[2*a+1] = G.Sub(2*a)-G.LCap(2*a);

        for (TNode v=0;v<n0;v++)
        {
            flow[2*(m0+v)] = flow[2*(m0+v)+1] = deg[v];
            flow[2*(m0+n0+v)] = flow[2*(m0+n0+v)+1] = deg[v+n0];
        };

        flow[2*(m0+2*n0)] = flow[2*(m0+2*n0)+1] = deg[2*n0];
        flow[2*(m0+2*n0)+2] = flow[2*(m0+2*n0)+3] = deg[2*n0+1];
        flow[2*(m0+2*n0)+4] = flow[2*(m0+2*n0)+5] = deg[2*n0+2];
        delete[] deg;
        deg = NULL;

        LogEntry(LOG_MEM,"Flow labels allocated");
    }
}


TNode graphToBalanced::StartNode(TArc a) const throw(ERRange)
{
    TArc d4 = a/4;
    TArc r4 = a%4;

    if (d4<m0)
    {
        switch (r4)
        {
            case 0: return 2*G.StartNode(d4*2);
            case 1: return 2*G.StartNode(d4*2+1)+1;
            case 2: return 2*G.StartNode(d4*2+1);
            default: return 2*G.StartNode(d4*2)+1;
        }
    }

    if (d4<m0+n0)
    {
        switch (r4)
        {
            case 0: return s1;
            case 1: return 2*(TNode)(d4-m0);    // An outer node
            case 2: return 2*(TNode)(d4-m0)+1;  // An inner node
            default: return t1;
        }
    }

    if (d4<m0+2*n0)
    {
        switch (r4)
        {
            case 0: return s2;
            case 1: return 2*(TNode)(d4-m0-n0);   // An outer node
            case 2: return 2*(TNode)(d4-m0-n0)+1; // An inner node
            default: return t2;
        }
    }

    if (d4==(ret1/4))
    {
        switch (r4)
        {
            case 0: return t1;
            case 1: return s1;
            case 2: return t1;
            default: return s1;
        }
    }

    if (d4==(ret2/4))
    {
        switch (r4)
        {
            case 0: return t2;
            case 1: return s2;
            case 2: return t2;
            default: return s2;
        }
    }

    if (d4==(art1/4))
    {
        switch (r4)
        {
            case 0: return s1;
            case 1: return t2;
            case 2: return s2;
            default: return t1;
        }
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("StartNode",a);

    #endif

    throw ERRange();
}


TNode graphToBalanced::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return graphToBalanced::StartNode(a^1);
}


TArc graphToBalanced::First(TNode v) const throw(ERRange)
{
    TNode v0 = v/2;

    if (v0<n0)
    {
        if (v&1) return 4*(m0+v0)+2;
        else return 4*(m0+v0)+1;
    }

    if (v==t2) return ret2;
    if (v==s2) return art2;
    if (v==t1) return ret1;
    if (v==s1) return art1;

    #if defined(_FAILSAVE_)

    NoSuchNode("First",v);

    #endif

    throw ERRange();
}


TArc graphToBalanced::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u!=StartNode(a))
        Error(ERR_REJECTED,"Right","Mismatching start nodes");

    #endif

    TNode u0 = u/2;

    if (u0<n0 && !(u&1))
    {
        if (a==4*(m0+u0)+1) return 4*(m0+n0+u0)+1;

        TArc af = 2*G.First(u0);
        if (a==4*(m0+n0+u0)+1)
        {
            if (af==2*NoArc) return 4*(m0+u0)+1;
            else return af;
        }

        TArc ar = 2*G.Right(a/2,u0);
        if (ar==af) return 4*(m0+u0)+1;
        return ar;
    }

    if (u0<n0 && (u&1))
    {
        if (a==4*(m0+u0)+2) return 4*(m0+n0+u0)+2;

        TArc af = ((2*G.First(u0))^3);
        if (a==4*(m0+n0+u0)+2)
        {
            if (af>=2*NoArc) return 4*(m0+u0)+2;
            else return af;
        }

        TArc ar = ((2*G.Right((a^3)/2,u0))^3);
        if (ar==af) return 4*(m0+u0)+2;
        return ar;
    }

    if (u==t2)
    {
        if (a==ret2) return (ret2^2);
        if (a==(ret2^2)) return (art1^1);
        if (a==(art1^1)) return 4*(m0+n0)+3;
        if (a==4*(m0+2*n0)-1) return ret2;
        return a+4;
    }

    if (u==s2)
    {
        if (a==(ret2^1)) return (ret2^3);
        if (a==(ret2^3)) return art2;
        if (a==art2) return 4*(m0+n0);
        if (a==4*(m0+2*n0)-4) return (ret2^1);
        return a+4;
    }

    if (u==t1)
    {
        if (a==ret1) return (ret1^2);
        if (a==(ret1^2)) return (art2^1);
        if (a==(art2^1)) return 4*m0+3;
        if (a==4*(m0+n0)-1) return ret1;
        return a+4;
    }

    if (u==s1)
    {
        if (a==(ret1^1)) return (ret1^3);
        if (a==(ret1^3)) return art1;
        if (a==art1) return 4*m0;
        if (a==4*(m0+n0)-4) return (ret1^1);
        return a+4;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("Right",u);

    #endif

    throw ERRange();
}


TCap graphToBalanced::Demand(TNode v) const throw(ERRange)
{
    if (v<2*n0+2) return 0;

    if (v==t1) return 2*sumLower;
    if (v==s1) return -2*sumLower;

    #if defined(_FAILSAVE_)

    NoSuchNode("Demand",v);

    #endif

    throw ERRange();
}


TCap graphToBalanced::UCap(TArc a) const throw(ERRange)
{
    TArc d4 = a/4;
    if (d4<m0)
    {
        if (G.Length(a>>1)==InfFloat) return 0;
        else return G.UCap(a>>1)-G.LCap(a>>1);
    }

    if (d4<m0+n0)
    {
        if (lower==NULL) return ccap;
        else return lower[d4-m0];
    }

    if (d4<m0+2*n0)
    {
        if (cap==NULL) return 0;
        else return cap[d4-m0-n0];
    }

    if (d4==(ret2/4)) return TCap(ceil(sumUpper/2));
    if (d4==(art1/4)) return sumLower;
    if (d4==(ret1/4)) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("UCap",a);

    #endif

    throw ERRange();
}


TCap graphToBalanced::LCap(TArc a) const throw(ERRange)
{
    if (a<2*m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("LCap",a);

    #endif

    throw ERRange();
}


TFloat graphToBalanced::Length(TArc a) const throw(ERRange)
{
    if (a<4*m0)
    {
        if (G.Length(a>>1)==InfFloat) return 0;
        else return G.Length(a>>1)-minLength;
    }

    if (a<2*m) return 0;

    #if defined(_FAILSAVE_)

    NoSuchArc("Length",a);

    #endif

    throw ERRange();
}


TFloat graphToBalanced::C(TNode v,TDim i) const throw(ERRange)
{
    if (i==0)
    {
        if (v<2*n0 && (v%2==0)) return 10;
        if (v<2*n0 && (v%2==1)) return 30;
        if (v==s1) return 0;
        if (v==t1) return 40;
        if (v==s2) return 38;
        if (v==t2) return 2;
    }
    else
    {
        if (v<2*n0) return 6*(v/2);
        if (v==s1) return 3*(n0-1)+1.5;
        if (v==t1) return 3*(n0-1)+1.5;
        if (v==s2) return 6*(n0-1)+3;
        if (v==t2) return 6*(n0-1)+3;
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


TFloat graphToBalanced::CMax(TDim i) const throw(ERRange)
{
    if (i==0) return 40;
    if (i==1) return 6*(n0-1)+3;

    #if defined(_FAILSAVE_)

    NoSuchCoordinate("CMax",i);

    #endif

    throw ERRange();
}

/*
TIndex graphToBalanced::OriginalOfNode(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("OriginalOfNode",v);

    #endif

}


TIndex graphToBalanced::OriginalOfArc(TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("OriginalArc",a);

    #endif

}
*/

bool graphToBalanced::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<2*n0) return G.HiddenNode(v);
    if (v<n) return false;

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


bool graphToBalanced::HiddenArc(TArc a) const throw(ERRange)
{
    if (a<2*m)
    {
        const TNode* dist = GetNodeColours();

        if (!dist) return false;

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (   dist[v]==dist[u]+1
            || dist[u]==dist[v]+1
            || dist[u^1]==dist[v^1]+1
            || dist[v^1]==dist[u^1]+1 )
        {
            return false;
        }
        else return true;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("HiddenArc",a);

    #endif

    throw ERRange();
}


TFloat graphToBalanced::Flow(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Flow",a);

    #endif

    if (a>=ret1) return 0;

    if (flow==NULL) return BalFlow(a);
    else return flow[a>>1];
}


TFloat graphToBalanced::BalFlow(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalFlow",a);

    #endif

    if (a>=ret1) return 0;

    #if defined(_FAILSAVE_)

    if (!deg)
    {
        Error(ERR_REJECTED,"BalFlow","Flow is not balanced");
    }

    #endif

    if (a<4*m0) return G.Sub(a>>1)-G.LCap(a>>1);
    else return deg[(a>>2)-m0];
}


void graphToBalanced::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    #endif

    if (flow==NULL) Relax();

    if (a&1) flow[a>>1] = flow[a>>1]-Lambda;
    else flow[a>>1] = flow[a>>1]+Lambda;

    AdjustDivergence(a,Lambda);
}


void graphToBalanced::BalPush(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalPush",a);

    if (Lambda<0 || Lambda>BalCap(a)) AmountOutOfRange("BalPush",Lambda);

    #endif

    if (deg==NULL) Symmetrize();

    TArc d4 = a/4;

    if (d4<m0)
    {
        G.SetSubRelative(2*d4, (a&1) ? -Lambda : Lambda);
    }
    else
    {
        TNode v0 = (TNode)(d4-m0);
        if (a&1) deg[v0] = deg[v0]-Lambda;
        else deg[v0] = deg[v0]+Lambda;
    }

    AdjustDivergence(a,Lambda);
    AdjustDivergence(a^2,Lambda);
}


TFloat graphToBalanced::CancelOdd() throw()
{
    #if defined(_FAILSAVE_)

    if (Q==NULL) Error(ERR_REJECTED,"CancelOdd","Odd sets required");

    #endif

    moduleGuard M(ModCycleCancel,*this,"Cancelling odd length cycles...",
        moduleGuard::SYNC_BOUNDS);

    TNode k1 = 0;
    TNode k2 = 0;

    for (TNode i=0;i<n0;i++)
    {
        TNode root = 2*i;
        if (Q[root]!=NoArc)
        {
            MakeIntegral(Q,root,root^1);

            if (BalCap(4*(m0+n0+i)+1)>0)
            {
                // Reduce flow on the artifical return arc

                BalPush(4*(m0+n0+i)+1,1);
                BalPush(ret2+1,0.5);

                if (BalCap(ret2)==floor(BalCap(ret2))) k1 += 2;
            }
            else BalPush(4*(m0+i)+1,1); // Reduce flow value

            k2++;
        }
    }

    if (BalCap(ret2)==floor(BalCap(ret2))+0.5)
    {
        if (BalCap(art1)>0) // Increase flow value
        {
            BalPush(art1,1);
            BalPush(ret2,0.5);
            k2--;
            k1 += 2;
        }
        else // Reduce flow value
        {
            BalPush(art1+1,1);
            BalPush(ret2+1,0.5);
            k2++;
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"%lu odd length cycles eliminated",
            static_cast<unsigned long>(k1)+k2);
        LogEntry(LOG_RES,CT.logBuffer);
        sprintf(CT.logBuffer,"Flow value decreases by %lu units",
            static_cast<unsigned long>(k2));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    ReleaseCycles();

    TFloat val = InfFloat;

    if (k2>1)
    {
        LogEntry(LOG_METH,"Refining balanced flow...");

        val = BNSAndAugment(DefaultSourceNode());
    }
    else
    {
        val = M.UpperBound();
        M.SetLowerBound(val);
    }

    return val;
}
