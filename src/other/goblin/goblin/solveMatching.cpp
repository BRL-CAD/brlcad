
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMatching.cpp
/// \brief  All entry points for matching problem solvers and application to minimum edge covers

#include "sparseGraph.h"
#include "sparseBigraph.h"
#include "graphToBalanced.h"
#include "bigraphToDigraph.h"
#include "binaryHeap.h"
#include "moduleGuard.h"


bool abstractGraph::MaximumMatching() throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum matching...");

    graphToBalanced G(*this);
    G.MaxBalFlow(G.DefaultSourceNode());
    G.ExportDecomposition();
    M.Trace();

    return (G.Perfect());
}


bool abstractGraph::MaximumMatching(TCap cDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum matching...");

    graphToBalanced G(*this,cDeg);
    G.MaxBalFlow(G.DefaultSourceNode());
    G.ExportDecomposition();
    M.Trace();

    return (G.Perfect());
}


bool abstractGraph::MaximumMatching(TCap *pLower,TCap *pDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum matching...");

    graphToBalanced G(*this,pLower,pDeg);
    G.MaxBalFlow(G.DefaultSourceNode());
    G.ExportDecomposition();
    M.Trace();

    return (G.Perfect());
}


bool abstractGraph::MinCMatching() throw()
{
    moduleGuard M(ModMatching,*this,"Computing minimum cost matching...");

    bool ret = false;

    if (IsDense() && CT.methCandidates>=0)
    {
        ret = PMHeuristicsCandidates();
    }
    else
    {
        InitSubgraph();

        try
        {
            graphToBalanced G(*this);
            G.MinCBalFlow(G.DefaultSourceNode());

            ret = G.Perfect();
        }
        catch (ERRejected) {}

        M.Trace();
    }

    return ret;
}


bool abstractGraph::MinCMatching(TCap cDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing minimum cost matching...");

    InitSubgraph();
    bool ret = false;

    try
    {
        graphToBalanced G(*this,cDeg);
        G.MinCBalFlow(G.DefaultSourceNode());
        ret = G.Perfect();
    }
    catch (ERRejected) {}

    M.Trace();

    return ret;
}


bool abstractGraph::MinCMatching(TCap *pLower,TCap *pDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing minimum cost matching...");

    InitSubgraph();
    bool ret = false;

    try
    {
        graphToBalanced G(*this,pLower,pDeg);
        G.MinCBalFlow(G.DefaultSourceNode());
        ret = G.Perfect();
    }
    catch (ERRejected) {}

    M.Trace();

    return ret;
}


bool abstractGraph::PMHeuristicsCandidates() throw()
{
    #if defined(_FAILSAVE_)

    TCap sumDemands = 0;

    for (TNode i=0;i<n;i++) sumDemands += Demand(i);

    if (int(sumDemands)%2!=0)
        Error(ERR_REJECTED,"PMHeuristicsCandidates","Mismatching node demands");

    #endif

    moduleGuard M(ModMatching,*this,
        moduleGuard::NO_INDENT || moduleGuard::SYNC_BOUNDS);
    LogEntry(LOG_METH2,"<Candidate Subgraph Heuristics>");

    sparseGraph G(n,CT);
    graphRepresentation* GR = G.Representation();

    for (int i=0;i<10 && CT.methSolve==0;)
    {
        LogEntry(LOG_METH,"Searching for candidate matching...");
        TFloat ret = PMHeuristicsRandom();

        if (ret<InfFloat)
        {
            for (TArc a=0;a<m;a++)
            {
                TNode u = StartNode(2*a);
                TNode v = EndNode(2*a);

                if (Sub(2*a)>0 && G.Adjacency(u,v,ADJ_SEARCH)==NoArc)
                    G.InsertArc(u,v,UCap(2*a),Length(2*a));
            }

            i++;
        }
    }

    binaryHeap<TArc,TFloat> Q(n,CT);

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v,Demand(v));

        #if defined(_TRACING_)

        GR -> SetC(v,0,C(v,0));
        GR -> SetC(v,1,C(v,1));

        #endif

        TArc a = First(v);

        if (a==NoArc) continue;

        do
        {
            if (EndNode(a)!=v)
                Q.Insert(EndNode(a),Length(a));

            a = Right(a,v);
        }
        while (a!=First(v));

        for (int i=0;!Q.Empty();i++)
        {
            TNode w = Q.Delete();
            TArc a = Adjacency(v,w);

            if (i<CT.methCandidates && G.Adjacency(v,w,ADJ_SEARCH)==NoArc)
                G.InsertArc(v,w,UCap(a),Length(a));
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...candidate subgraph has %lu arcs",
            static_cast<unsigned long>(G.M()));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (CT.traceLevel==3) CT.Trace(OH);

    if (CT.methSolve==0)
    {
        // Candidate heuristics

        bool ret = G.MinCMatching();

        if (ret)
        {
            InitSubgraph();

            for (TArc a=0;a<G.M();a++)
            {
                TNode u = G.StartNode(2*a);
                TNode v = G.EndNode(2*a);
                SetSub(Adjacency(u,v),G.Sub(2*a));
            }
        }

        return ret;
    }
    else
    {
        // Exact method

        graphToBalanced H1(G);

        #if defined(_PROGRESS_)

        M.InitProgressCounter(n+n+1+n*n,n);

        #endif

        H1.MinCostSTFlow(H1.DefaultSourceNode(),H1.DefaultTargetNode());

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(n);

        #endif

        InitSubgraph();

        graphToBalanced H2(*this);

        for (TNode v=0;v<H1.N();v++) H2.SetPotential(v,H1.Pi(v));

        for (TArc a=0;a<H1.M();a++)
        {
            TNode u = H1.StartNode(2*a);
            TNode v = H1.EndNode(2*a);
            H2.SetSub(H2.Adjacency(u,v),H1.Flow(2*a));
        }

        H2.MinCostBFlow(abstractDiGraph::MCF_BF_CAPA);

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(1);

        #endif

        H2.CancelEven();

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(n*n);

        #endif

        H2.CancelPD();

        return H2.Perfect();
    }
}


TFloat abstractGraph::PMHeuristicsRandom() throw()
{
    OpenFold();
    LogEntry(LOG_METH,"(Random Heuristics)");

    InitSubgraph();
    TFloat weight = 0;

    goblinQueue<TNode,TFloat> *Q = NewNodeHeap();
    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode i=0;i<n;i++)
    {
        TNode v = TNode(CT.Rand(n));
        TNode shift = 0;

        while (Deg(v)==Demand(v) && shift<n)
        {
            v = (v+1)%n;
            shift++;
        }

        if (shift==n) break;

        I.Reset(v);
        while (I.Active(v))
        {
            TArc a = I.Read(v);
            if (EndNode(a)!=v) 
                Q -> Insert(EndNode(a),Length(a));
        }

        while (Deg(v)<Demand(v))
        {
            if (Q->Empty())
            {
                LogEntry(LOG_RES,"...no matching found");
                CloseFold();
                Close(H);
                delete Q;
                return InfFloat;
            }

            TNode w = Q->Delete();
            TArc a = Adjacency(v,w);
            TFloat lambda = UCap(a)-Sub(a);
            TFloat lambda2 = Demand(v)-Deg(v);

            if (v==w) lambda2 = floor(lambda2/2);
            else if (Demand(w)-Deg(w)<lambda2)
                lambda2 = Demand(w)-Deg(w);

            if (lambda2<lambda) lambda = lambda2;

            if (lambda>0)
            {
                SetSubRelative(a,lambda);
                weight += lambda*Length(a);
                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Adding arc %lu(%lu)%lu with multiplicity %.3f",
                        static_cast<unsigned long>(v),
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(w),
                        static_cast<double>(lambda));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }
            }
        }

        Q -> Init();
    }

    Close(H);
    delete Q;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Matching of weight %.3f found",weight);
        LogEntry(LOG_RES,CT.logBuffer);
    }
    CloseFold();

    return weight;
}


TFloat abstractGraph::MinCEdgeCover() throw(ERRejected)
{
    // As described in RefSch03, p.317

    moduleGuard M(ModMatching,*this,"Computing minimum cost edge cover...");

    sparseGraph G(2*n,CT);
    sparseRepresentation* GR =
        static_cast<sparseRepresentation*>(G.Representation());

    GR -> SetCapacity(2*n,2*m+n);

    for (TArc a=0;a<m;a++)
    {
        G.InsertArc(2*StartNode(2*a),2*EndNode(2*a),1,Length(2*a));
        G.InsertArc(2*StartNode(2*a)+1,2*EndNode(2*a)+1,1,Length(2*a));
    }

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    for (TNode v=0;v<n;v++)
    {
        TFloat minLength = InfFloat;
        TArc a = First(v);

        if (a==NoArc)
            Error(ERR_REJECTED,"MinEdgeCover","Isolated vertex found");

        do
        {
            if (Length(a)<minLength) minLength = Length(a);

            a = Right(a,v);
        }
        while (a!=First(v));

        G.InsertArc(2*v,2*v+1,1,2*minLength);

        if (Dim()>1)
        {
            GR -> SetC(2*v,0,C(v,0));
            GR -> SetC(2*v,1,C(v,1));
            GR -> SetC(2*v+1,0,C(v,0)+nodeSpacing/sqrt(2.0));
            GR -> SetC(2*v+1,1,C(v,1)+nodeSpacing/sqrt(2.0));
        }
    }

    if (CT.traceLevel==1) G.Display();

    try
    {
        G.MinCMatching(1);
    }
    catch (ERRejected) {}


    // Map the matching to an edge cover of the original graph
    TFloat ret = 0;
    InitEdgeColours(0);

    for (TArc a=0;a<m;a++)
    {
        if (G.Sub(4*a)>CT.epsilon)
        {
            SetEdgeColour(2*a,1);
            ret += Length(2*a);
        }
    }

    for (TNode v=0;v<n;v++)
    {
        TArc aArtifical = 4*m+2*v;

        if (G.Sub(aArtifical)<CT.epsilon) continue;

        TArc a = First(v);

        do
        {
            if (fabs(2*Length(a)-G.Length(aArtifical))<CT.epsilon)
            {
                SetEdgeColour(a,1);
                ret += Length(a);
                break;
            }

            a = Right(a,v);
        }
        while (a!=First(v));
    }

    if (CT.traceLevel==1) G.Display();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Minimum edge cover has length %g",ret);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return ret;
}


bool abstractBiGraph::MaximumAssignment() throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum assignment...");

    bigraphToDigraph G(*this);
    G.MaxFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
    M.Trace();

    return (G.Perfect());
}


bool abstractBiGraph::MaximumAssignment(TCap cDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum assignment...");

    bigraphToDigraph G(*this,cDeg);
    G.MaxFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
    M.Trace();

    return (G.Perfect());
}


bool abstractBiGraph::MaximumAssignment(TCap *pLower,TCap *pDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing maximum assignment...");

    bigraphToDigraph G(*this,pLower,pDeg);
    G.MaxFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
    M.Trace();

    return (G.Perfect());
}


bool abstractBiGraph::MinCAssignment() throw()
{
    moduleGuard M(ModMatching,*this,"Computing optimal assignment...");

    bool ret = false;

    if (IsDense() && CT.methCandidates>=0)
    {
        ret = PMHeuristicsCandidates();
    }
    else
    {
        InitSubgraph();

        try
        {
            bigraphToDigraph G(*this);
            G.MinCostSTFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
            ret = G.Perfect();
        }
        catch (ERRejected) {}

        M.Trace();
    }

    return ret;
}


bool abstractBiGraph::MinCAssignment(TCap cDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing optimal assignment...");

    InitSubgraph();
    bool ret = false;

    try
    {
        bigraphToDigraph G(*this,cDeg);
        G.MinCostSTFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
        ret = G.Perfect();
    }
    catch (ERRejected) {CloseFold();};

    M.Trace();

    return (ret);
}


bool abstractBiGraph::MinCAssignment(TCap *pLower,TCap *pDeg) throw()
{
    moduleGuard M(ModMatching,*this,"Computing optimal assignment...");

    InitSubgraph();
    bool ret = false;

    try
    {
        bigraphToDigraph G(*this,pLower,pDeg);
        G.MinCostSTFlow(G.DefaultSourceNode(),G.DefaultTargetNode());
        ret = G.Perfect();
    }
    catch (ERRejected) {CloseFold();};

    M.Trace();

    return ret;
}


bool abstractBiGraph::PMHeuristicsCandidates() throw()
{
    moduleGuard M(ModMatching,*this,
        moduleGuard::NO_INDENT || moduleGuard::SYNC_BOUNDS);
    LogEntry(LOG_METH2,"<Candidate Subgraph Heuristics>");

    sparseBiGraph G(n1,n2,CT);
    G.ImportLayoutData(*this);
    graphRepresentation* GR = G.Representation();

    int i = 0;
    while (i<10)
    {
        LogEntry(LOG_METH,"Searching for candidate matching...");
        TFloat ret = PMHeuristicsRandom();
        if (ret<InfFloat)
        {
            for (TArc a=0;a<m;a++)
                if (Sub(2*a)>0 && G.Adjacency(StartNode(2*a),EndNode(2*a),ADJ_SEARCH)==NoArc)
                    G.InsertArc(StartNode(2*a),EndNode(2*a),UCap(2*a),Length(2*a));

            i++;
        }
    }

    binaryHeap<TArc,TFloat> Q(n,CT);
    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v,Demand(v));

        #if defined(_TRACING_)

        GR -> SetC(v,0,C(v,0));
        GR -> SetC(v,1,C(v,1));

        #endif

        while (I.Active(v))
        {
            TArc a = I.Read(v);
            if (EndNode(a)!=v) 
                Q.Insert(EndNode(a),Length(a));
        }

        int i = 0;

        while (!Q.Empty())
        {
            TNode w = Q.Delete();
            TArc a = Adjacency(v,w);

            if (i<CT.methCandidates && G.Adjacency(v,w,ADJ_SEARCH)==NoArc)
            {
                if (v<n1) G.InsertArc(v,w,UCap(a),Length(a));
                else  G.InsertArc(w,v,UCap(a),Length(a));
            }

            i++;
        }
    }

    Close(H);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...candidate subgraph has %lu arcs",
            static_cast<unsigned long>(G.M()));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (CT.traceLevel==3) CT.Trace(OH);

    bool ret = false;

    if (CT.methSolve==0)
    {
        // Candidate heuristics

        G.InitSubgraph();
        ret = G.MinCAssignment();

        if (ret)
        {
            InitSubgraph();

            for (TArc a=0;a<G.M();a++)
            {
                SetSub(Adjacency(G.StartNode(2*a),G.EndNode(2*a)),G.Sub(2*a));
            }
        }
    }
    else
    {
        // Exact method

        bigraphToDigraph H1(G);

        #if defined(_PROGRESS_)

        M.InitProgressCounter(2,1);

        #endif

        H1.MinCostSTFlow(H1.DefaultSourceNode(),H1.DefaultTargetNode());

        InitSubgraph();

        for (TArc a=0;a<G.M();a++)
        {
            SetSub(Adjacency(G.StartNode(2*a),G.EndNode(2*a)),G.Sub(2*a));
        }

        bigraphToDigraph H2(*this);

        for (TNode v=0;v<H1.N();v++) H2.SetPotential(v,H1.Pi(v)); 

        #if defined(_PROGRESS_)

        M.ProgressStep(1);

        #endif

        H2.MinCostBFlow(abstractDiGraph::MCF_BF_SAP);

        ret = H2.Perfect();
    }

    return ret;
}
