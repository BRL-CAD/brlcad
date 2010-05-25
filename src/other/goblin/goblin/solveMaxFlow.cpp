
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file solveMaxFlow.cpp
/// \brief Maximum flow solver and methods

#include "sparseDigraph.h"
#include "denseDigraph.h"
#include "staticQueue.h"
#include "auxiliaryNetwork.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::MaxFlow(TMethMXF method,TNode s,TNode t) throw(ERRange,ERRejected)
{
    if (s>=n) s = DefaultSourceNode();
    if (t>=n) t = DefaultTargetNode();

    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MaxFlow",s);

    if (t>=n) NoSuchNode("MaxFlow",t);

    #endif

    // Methods apply to directed graphs only. In all other cases,
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

    sprintf(CT.logBuffer,"Computing maximum (%lu,%lu)-flow...",
        static_cast<unsigned long>(s),static_cast<unsigned long>(t));
    moduleGuard M(ModMaxFlow,*this,CT.logBuffer,moduleGuard::NO_INDENT);

    if (method==MXF_DEFAULT) method = TMethMXF(CT.methMXF);

    TFloat ret = InfCap;

    switch (method)
    {
        case MXF_SAP:
        {
            ret = G->MXF_EdmondsKarp(s,t);
            break;
        }
        case MXF_DINIC:
        {
            ret = G->MXF_Dinic(s,t);
            break;
        }
        case MXF_PREFLOW_FIFO:
        case MXF_PREFLOW_HIGH:
        case MXF_PREFLOW_SCALE:
        {
            ret = G->MXF_PushRelabel(method,s,t);
            G -> BFS(residualArcs(*G),singletonIndex<TNode>(s,n,CT),singletonIndex<TNode>(t,n,CT));
            break;
        }
        case MXF_SAP_SCALE:
        {
            ret = G->MXF_CapacityScaling(s,t);
            break;
        }
        default:
        {
            if (!IsDirected()) delete G;

            UnknownOption("MaxFlow",method);
        }
    }

    if (!IsDirected())
    {
        static_cast<completeOrientation*>(G) -> MapFlowBackward(*this);

        TNode* distG = G->GetNodeColours();
        TNode* dist = RawNodeColours();

        for (TNode v=0;v<n;++v) dist[v] = distG[v];
    }

    #if defined(_FAILSAVE_)

    try
    {
        if (ret!=InfCap && CT.methFailSave==1 && G->CutCapacity()!=G->FlowValue(s,t))
        {
            if (!IsDirected()) delete G;
            sprintf(CT.logBuffer,"Solutions are corrupted (capacity = %g, flow value = %g)",
                G->CutCapacity(),G->FlowValue(s,t));
            InternalError("MaxFlow",CT.logBuffer);
        }
    }
    catch (ERCheck())
    {
        if (!IsDirected()) delete G;
        InternalError("MaxFlow","Solutions are corrupted");
    }

    #endif

    if (!IsDirected()) delete G;

    return ret;
}


TFloat abstractDiGraph::MXF_EdmondsKarp(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModEdmondsKarp,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m*(n-1), 1);
    TNode phase = 1;

    #endif

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    TArc* pred = InitPredecessors();

    while (   CT.SolverRunning()
           && BFS(residualArcs(*this),
                  singletonIndex<TNode>(s,n,CT),
                  singletonIndex<TNode>(t,n,CT))!=NoNode
          )
    {
        #if defined(_PROGRESS_)

        TNode dt = NodeColour(t);

        if (dt>phase)
        {
            M.SetProgressCounter(m*(dt-1.0));
            phase = dt;
        }

        #endif

        TCap Lambda = FindCap(pred,s,t);

        if (Lambda==InfCap)
        {
            M.Trace(m);
            M.Shutdown(LOG_RES,"...Problem is unbounded");
            return InfCap;
        }

        Augment(pred,s,t,Lambda);
        val += Lambda;
        M.SetLowerBound(val);
        M.Trace(1);
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractDiGraph::MXF_CapacityScaling(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModMaxFlow,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH,"(Capacity scaling method)");

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    TCap delta =  MaxUCap();

    if (delta>0) delta -= 1;

    #if defined(_PROGRESS_)

    double nPhases = 1;
    if (delta>0) nPhases = floor(log(delta)/log(double(2)))+2;
    M.InitProgressCounter(m*nPhases*m);

    #endif

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
        LogEntry(LOG_METH,CT.logBuffer);
    }

    TArc* pred = InitPredecessors();

    while (CT.SolverRunning())
    {
        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        #if defined(_PROGRESS_)

        double newPhases = 1;
        if (delta>0) newPhases = floor(log(delta)/log(double(2)))+2;
        M.SetProgressCounter(m*(nPhases-newPhases)*m);

        #endif

        while (   CT.SolverRunning()
               && BFS(residualArcs(*this,delta),
                      singletonIndex<TNode>(s,n,CT),
                      singletonIndex<TNode>(t,n,CT))!=NoNode
              )
        {
            TCap Lambda = FindCap(pred,s,t);

            if (Lambda==InfCap)
            {
                M.Trace(m);
                M.Shutdown(LOG_RES,"...Problem is unbounded");
                delta = 0;
                return InfCap;
            }

            Augment(pred,s,t,Lambda);
            val += Lambda;
            M.SetLowerBound(val);
            M.Trace(m);
        }

        if (delta==0) break;

        delta = floor(delta/2);
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractDiGraph::MXF_Dinic(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModDinic,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    TArc* pred = InitPredecessors();
    layeredAuxNetwork Aux(*this,s);

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((n+1.0)*m*(n-1.0));

    #endif

    while (CT.SolverRunning())
    {
        TNode* dist = InitNodeColours(NoNode);
        dist[s] = 0;
        staticQueue<TNode> Q(n,CT);
        Q.Insert(s);

        LogEntry(LOG_METH,"Graph is searched...");

        #if defined(_LOGGING_)

        OpenFold();
        THandle LH = LogStart(LOG_METH2,"Found props: ");

        #endif

        THandle H = Investigate();
        investigator &I = Investigator(H);

        while (!(Q.Empty()))
        {
            TNode u = Q.Delete();
            if (dist[u]==dist[t]) break;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);

                if (dist[v]>=dist[u]+1 && ResCap(a)>0)
                {
                    if (dist[v]==NoNode)
                    {
                        dist[v] = dist[u]+1;
                        Q.Insert(v);
                    }
                    Aux.InsertProp(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu(%lu)%lu ",
                            static_cast<unsigned long>(u),
                            static_cast<unsigned long>(a),
                            static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }
        }

        Close(H);

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);
        CloseFold();

        #endif

        if (dist[t]==NoNode) break;

        #if defined(_PROGRESS_)

        M.SetProgressCounter((n+1.0)*m*(dist[t]-1.0));

        #endif

        M.Trace(m);

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Phase %lu augmenting...",
                static_cast<unsigned long>(dist[t]));
            LogEntry(LOG_METH,CT.logBuffer);
        }

        OpenFold();

        Aux.Phase2();

        while (!(Aux.Blocked(t)))
        {
            M.Trace(Aux,n);

            TFloat Lambda = Aux.FindPath(t);
            if (Lambda==InfCap)
            {
                M.Trace(n*m);
                M.Shutdown(LOG_RES,"...Problem is unbounded");
                return InfCap;
            }

            val += Lambda;

            TArc a;
            TNode w = t;

            OpenFold();

            while (w!=s)
            {
                a = pred[w];
                Push(a,Lambda);
                pred[w] = NoArc;
                w = StartNode(a);

                if (ResCap(a)==0) Aux.TopErasure(a);
            }

            CloseFold();
        }

        CloseFold();
        M.SetLowerBound(val);

        Aux.Phase1();
        Aux.Init();
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    M.Trace();

    return val;
}


TFloat abstractDiGraph::MXF_PushRelabel(TMethMXF method,TNode s,TNode t) throw(ERRange)
{
    // Push-and-Relabel method which supports the following selection
    // strategies:
    //
    // method == MXF_PREFLOW_FIFO  : First-in first-out, O(n^3)
    // method == MXF_PREFLOW_HIGH  : Highest label, O(n^2 m^(1/2))
    // method == MXF_PREFLOW_SCALE : Excess scaling, O(nm + n^2 log(U))

    moduleGuard M(ModPushRelabel,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1);

    #endif

    TFloat initialFlowValue = Divergence(t);

    // A queue for the active nodes

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (method!=MXF_PREFLOW_FIFO)
    {
        Q = NewNodeHeap();
    }
    else
    {
        Q = new staticQueue<TNode>(n,CT);
    }

    // Additional incidence structure which is updated by relabel(u)
    // operations. Admissible[u] contains of all arcs a=uv such that
    // rescap(a)>0 and dist[u]=dist[v]+1.

    staticQueue<TArc> **Admissible = new staticQueue<TArc>*[n];
    Admissible[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<n;i++)
        Admissible[i] = new staticQueue<TArc>(*Admissible[0]);

    // Temporary incidence lists used in relabel operations

    staticQueue<TArc> S1(2*m,CT);
    staticQueue<TArc> S2(S1);

    TNode* dist = InitNodeColours(0);
    dist[s] = n;

    // Push all flow away from the root node

    THandle H = Investigate();
    investigator &I = Investigator(H);
    bool unbounded = false;
    while (I.Active(s))
    {
        TArc a = I.Read(s);

        if (UCap(a)!=InfCap) Push(a,ResCap(a));
        else unbounded = true;
    }


    TCap delta = 0;

    if (method==MXF_PREFLOW_SCALE)
    {
        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && Divergence(v)>delta)
            {
                delta = Divergence(v);
            }
        }

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        #if defined(_LOGGING_)

        THandle LH = LogStart(LOG_METH2,"Active nodes: ");

        #endif

        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && 2*Divergence(v)>=delta)
            {
                Q -> Insert(v,0);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                    LogAppend(LH,CT.logBuffer);
                }

                #endif
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
    else
    {
        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && Divergence(v)>0) Q -> Insert(v,0);
        }
    }

    M.SetBounds(Divergence(t),-Divergence(s));

    #if defined(_PROGRESS_)

    M.SetProgressCounter(-Divergence(t)/Divergence(s));

    #endif

    M.Trace();

    while (!(Q->Empty()) && !unbounded)
    {
        TNode u = Q->Delete();
        bool canRelabel = true;

        while (!Admissible[u]->Empty() && Divergence(u)>0)
        {
            TArc a = Admissible[u]->Delete();
            TNode v = EndNode(a);

            TFloat lambda = ResCap(a);
            if (lambda>Divergence(u)) lambda = Divergence(u);

            if (method==MXF_PREFLOW_SCALE)
            {
                if (v!=t && lambda>delta-Divergence(v))
                {
                    lambda = delta-Divergence(v);
                }

                // All non-saturating pushes must carry at least delta/2 units
                if (2*lambda<delta && lambda<ResCap(a))
                {
                    if (dist[u]==dist[v]+1) canRelabel = false;
                    continue;
                }
            }

            if (dist[u]==dist[v]+1)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Push %g flow units from node %lu to node %lu",
                        static_cast<double>(lambda),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                if (v!=t && v!=s)
                {
                    if (method!=MXF_PREFLOW_SCALE)
                    {
                        if (Divergence(v)==0) Q->Insert(v,-dist[v]);
                    }
                    else if (    2*Divergence(v)<delta
                              && 2*(Divergence(v)+lambda)>=delta
                            )
                    {
                        Q->Insert(v,dist[v]);

                        #if defined(_LOGGING_)

                        sprintf(CT.logBuffer,"Activating node %lu ",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);

                        #endif
                    }
                }

                Push(a,lambda);

                if (v==s) M.SetUpperBound(-Divergence(s));
                if (v==t) M.SetLowerBound(Divergence(t));

                #if defined(_PROGRESS_)

                M.SetProgressCounter(-Divergence(t)/Divergence(s));

                #endif

                M.Trace();

                if (ResCap(a)>0) Admissible[u]->Insert(a);

                if (   method==MXF_PREFLOW_SCALE
                    && 2*Divergence(u)<delta
                   )
                {
                    canRelabel = false;
                    break;
                }
            }
        }

        if (canRelabel && Divergence(u)>0)
        {
            I.Reset(u);
            TNode dMin = NoNode;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                if (ResCap(a)>0 && dist[v]<=dMin)
                {
                    dMin = dist[v];
                    S1.Insert(a);
                }

                if (ResCap(a^1)>0 && dMin+2>=dist[v]) S2.Insert(a^1);
            }

            while (!S1.Empty())
            {
                TArc a = S1.Delete();
                TNode v = EndNode(a);

                if (dMin==dist[v]) Admissible[u]->Insert(a);
            }

            while (!S2.Empty())
            {
                TArc a = S2.Delete();
                TNode v = StartNode(a);

                if (dMin+2==dist[v]) Admissible[v]->Insert(a);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Relabelled node %lu: %lu -> %lu",
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(dist[u]),
                    static_cast<unsigned long>(dMin+1));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            dist[u] = dMin+1;

            if (method!=MXF_PREFLOW_SCALE)
            {
                Q->Insert(u,-dist[u]);
            }
            else
            {
                Q->Insert(u,dist[u]);
            }

            M.Trace();
        }

        if (method==MXF_PREFLOW_SCALE && Q->Empty() && delta>2)
        {
            delta = ceil(delta/2);

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
                LogEntry(LOG_METH,CT.logBuffer);
            }

            #if defined(_LOGGING_)

            THandle LH = LogStart(LOG_METH2,"Active nodes: ");

            #endif

            for (TNode v=0;v<n;v++)
            {
                if (v!=t && v!=s && 2*Divergence(v)>=delta)
                {
                    Q->Insert(v,dist[v]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) LogEnd(LH);

            #endif
        }
    }

    Close(H);

    delete Q;

    for (TNode i=1;i<n;i++) delete Admissible[i];
    delete Admissible[0];
    delete[] Admissible;

    ReleasePredecessors();

    if (unbounded) Error(ERR_RANGE,"MXF_PushRelabel",
        "Arcs emanating from source must have finite capacities");

    return Divergence(t)-initialFlowValue;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file solveMaxFlow.cpp
/// \brief Maximum flow solver and methods

#include "sparseDigraph.h"
#include "denseDigraph.h"
#include "staticQueue.h"
#include "auxiliaryNetwork.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::MaxFlow(TMethMXF method,TNode s,TNode t) throw(ERRange,ERRejected)
{
    if (s>=n) s = DefaultSourceNode();
    if (t>=n) t = DefaultTargetNode();

    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MaxFlow",s);

    if (t>=n) NoSuchNode("MaxFlow",t);

    #endif

    // Methods apply to directed graphs only. In all other cases,
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

    sprintf(CT.logBuffer,"Computing maximum (%lu,%lu)-flow...",
        static_cast<unsigned long>(s),static_cast<unsigned long>(t));
    moduleGuard M(ModMaxFlow,*this,CT.logBuffer,moduleGuard::NO_INDENT);

    if (method==MXF_DEFAULT) method = TMethMXF(CT.methMXF);

    TFloat ret = InfCap;

    switch (method)
    {
        case MXF_SAP:
        {
            ret = G->MXF_EdmondsKarp(s,t);
            break;
        }
        case MXF_DINIC:
        {
            ret = G->MXF_Dinic(s,t);
            break;
        }
        case MXF_PREFLOW_FIFO:
        case MXF_PREFLOW_HIGH:
        case MXF_PREFLOW_SCALE:
        {
            ret = G->MXF_PushRelabel(method,s,t);
            G -> BFS(residualArcs(*G),singletonIndex<TNode>(s,n,CT),singletonIndex<TNode>(t,n,CT));
            break;
        }
        case MXF_SAP_SCALE:
        {
            ret = G->MXF_CapacityScaling(s,t);
            break;
        }
        default:
        {
            if (!IsDirected()) delete G;

            UnknownOption("MaxFlow",method);
        }
    }

    if (!IsDirected())
    {
        static_cast<completeOrientation*>(G) -> MapFlowBackward(*this);

        TNode* distG = G->GetNodeColours();
        TNode* dist = RawNodeColours();

        for (TNode v=0;v<n;++v) dist[v] = distG[v];
    }

    #if defined(_FAILSAVE_)

    try
    {
        if (ret!=InfCap && CT.methFailSave==1 && G->CutCapacity()!=G->FlowValue(s,t))
        {
            if (!IsDirected()) delete G;
            sprintf(CT.logBuffer,"Solutions are corrupted (capacity = %g, flow value = %g)",
                G->CutCapacity(),G->FlowValue(s,t));
            InternalError("MaxFlow",CT.logBuffer);
        }
    }
    catch (ERCheck())
    {
        if (!IsDirected()) delete G;
        InternalError("MaxFlow","Solutions are corrupted");
    }

    #endif

    if (!IsDirected()) delete G;

    return ret;
}


TFloat abstractDiGraph::MXF_EdmondsKarp(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModEdmondsKarp,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m*(n-1), 1);
    TNode phase = 1;

    #endif

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    TArc* pred = InitPredecessors();

    while (   CT.SolverRunning()
           && BFS(residualArcs(*this),
                  singletonIndex<TNode>(s,n,CT),
                  singletonIndex<TNode>(t,n,CT))!=NoNode
          )
    {
        #if defined(_PROGRESS_)

        TNode dt = NodeColour(t);

        if (dt>phase)
        {
            M.SetProgressCounter(m*(dt-1.0));
            phase = dt;
        }

        #endif

        TCap Lambda = FindCap(pred,s,t);

        if (Lambda==InfCap)
        {
            M.Trace(m);
            M.Shutdown(LOG_RES,"...Problem is unbounded");
            return InfCap;
        }

        Augment(pred,s,t,Lambda);
        val += Lambda;
        M.SetLowerBound(val);
        M.Trace(1);
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractDiGraph::MXF_CapacityScaling(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModMaxFlow,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH,"(Capacity scaling method)");

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    TCap delta =  MaxUCap();

    if (delta>0) delta -= 1;

    #if defined(_PROGRESS_)

    double nPhases = 1;
    if (delta>0) nPhases = floor(log(delta)/log(double(2)))+2;
    M.InitProgressCounter(m*nPhases*m);

    #endif

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
        LogEntry(LOG_METH,CT.logBuffer);
    }

    TArc* pred = InitPredecessors();

    while (CT.SolverRunning())
    {
        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        #if defined(_PROGRESS_)

        double newPhases = 1;
        if (delta>0) newPhases = floor(log(delta)/log(double(2)))+2;
        M.SetProgressCounter(m*(nPhases-newPhases)*m);

        #endif

        while (   CT.SolverRunning()
               && BFS(residualArcs(*this,delta),
                      singletonIndex<TNode>(s,n,CT),
                      singletonIndex<TNode>(t,n,CT))!=NoNode
              )
        {
            TCap Lambda = FindCap(pred,s,t);

            if (Lambda==InfCap)
            {
                M.Trace(m);
                M.Shutdown(LOG_RES,"...Problem is unbounded");
                delta = 0;
                return InfCap;
            }

            Augment(pred,s,t,Lambda);
            val += Lambda;
            M.SetLowerBound(val);
            M.Trace(m);
        }

        if (delta==0) break;

        delta = floor(delta/2);
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractDiGraph::MXF_Dinic(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModDinic,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    TArc* pred = InitPredecessors();
    layeredAuxNetwork Aux(*this,s);

    TFloat val = 0;
    TArc a = First(s);
    do
    {
        if (!Blocking(a)) val += Flow(a);
        a = Right(a,s);
    }
    while (a!=First(s));

    M.SetLowerBound(val);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((n+1.0)*m*(n-1.0));

    #endif

    while (CT.SolverRunning())
    {
        TNode* dist = InitNodeColours(NoNode);
        dist[s] = 0;
        staticQueue<TNode> Q(n,CT);
        Q.Insert(s);

        LogEntry(LOG_METH,"Graph is searched...");

        #if defined(_LOGGING_)

        OpenFold();
        THandle LH = LogStart(LOG_METH2,"Found props: ");

        #endif

        THandle H = Investigate();
        investigator &I = Investigator(H);

        while (!(Q.Empty()))
        {
            TNode u = Q.Delete();
            if (dist[u]==dist[t]) break;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);

                if (dist[v]>=dist[u]+1 && ResCap(a)>0)
                {
                    if (dist[v]==NoNode)
                    {
                        dist[v] = dist[u]+1;
                        Q.Insert(v);
                    }
                    Aux.InsertProp(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu(%lu)%lu ",
                            static_cast<unsigned long>(u),
                            static_cast<unsigned long>(a),
                            static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }
        }

        Close(H);

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);
        CloseFold();

        #endif

        if (dist[t]==NoNode) break;

        #if defined(_PROGRESS_)

        M.SetProgressCounter((n+1.0)*m*(dist[t]-1.0));

        #endif

        M.Trace(m);

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Phase %lu augmenting...",
                static_cast<unsigned long>(dist[t]));
            LogEntry(LOG_METH,CT.logBuffer);
        }

        OpenFold();

        Aux.Phase2();

        while (!(Aux.Blocked(t)))
        {
            M.Trace(Aux,n);

            TFloat Lambda = Aux.FindPath(t);
            if (Lambda==InfCap)
            {
                M.Trace(n*m);
                M.Shutdown(LOG_RES,"...Problem is unbounded");
                return InfCap;
            }

            val += Lambda;

            TArc a;
            TNode w = t;

            OpenFold();

            while (w!=s)
            {
                a = pred[w];
                Push(a,Lambda);
                pred[w] = NoArc;
                w = StartNode(a);

                if (ResCap(a)==0) Aux.TopErasure(a);
            }

            CloseFold();
        }

        CloseFold();
        M.SetLowerBound(val);

        Aux.Phase1();
        Aux.Init();
    }

    ReleasePredecessors();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    M.Trace();

    return val;
}


TFloat abstractDiGraph::MXF_PushRelabel(TMethMXF method,TNode s,TNode t) throw(ERRange)
{
    // Push-and-Relabel method which supports the following selection
    // strategies:
    //
    // method == MXF_PREFLOW_FIFO  : First-in first-out, O(n^3)
    // method == MXF_PREFLOW_HIGH  : Highest label, O(n^2 m^(1/2))
    // method == MXF_PREFLOW_SCALE : Excess scaling, O(nm + n^2 log(U))

    moduleGuard M(ModPushRelabel,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1);

    #endif

    TFloat initialFlowValue = Divergence(t);

    // A queue for the active nodes

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (method!=MXF_PREFLOW_FIFO)
    {
        Q = NewNodeHeap();
    }
    else
    {
        Q = new staticQueue<TNode>(n,CT);
    }

    // Additional incidence structure which is updated by relabel(u)
    // operations. Admissible[u] contains of all arcs a=uv such that
    // rescap(a)>0 and dist[u]=dist[v]+1.

    staticQueue<TArc> **Admissible = new staticQueue<TArc>*[n];
    Admissible[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<n;i++)
        Admissible[i] = new staticQueue<TArc>(*Admissible[0]);

    // Temporary incidence lists used in relabel operations

    staticQueue<TArc> S1(2*m,CT);
    staticQueue<TArc> S2(S1);

    TNode* dist = InitNodeColours(0);
    dist[s] = n;

    // Push all flow away from the root node

    THandle H = Investigate();
    investigator &I = Investigator(H);
    bool unbounded = false;
    while (I.Active(s))
    {
        TArc a = I.Read(s);

        if (UCap(a)!=InfCap) Push(a,ResCap(a));
        else unbounded = true;
    }


    TCap delta = 0;

    if (method==MXF_PREFLOW_SCALE)
    {
        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && Divergence(v)>delta)
            {
                delta = Divergence(v);
            }
        }

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        #if defined(_LOGGING_)

        THandle LH = LogStart(LOG_METH2,"Active nodes: ");

        #endif

        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && 2*Divergence(v)>=delta)
            {
                Q -> Insert(v,0);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                    LogAppend(LH,CT.logBuffer);
                }

                #endif
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
    else
    {
        for (TNode v=0;v<n;v++)
        {
            if (v!=t && v!=s && Divergence(v)>0) Q -> Insert(v,0);
        }
    }

    M.SetBounds(Divergence(t),-Divergence(s));

    #if defined(_PROGRESS_)

    M.SetProgressCounter(-Divergence(t)/Divergence(s));

    #endif

    M.Trace();

    while (!(Q->Empty()) && !unbounded)
    {
        TNode u = Q->Delete();
        bool canRelabel = true;

        while (!Admissible[u]->Empty() && Divergence(u)>0)
        {
            TArc a = Admissible[u]->Delete();
            TNode v = EndNode(a);

            TFloat lambda = ResCap(a);
            if (lambda>Divergence(u)) lambda = Divergence(u);

            if (method==MXF_PREFLOW_SCALE)
            {
                if (v!=t && lambda>delta-Divergence(v))
                {
                    lambda = delta-Divergence(v);
                }

                // All non-saturating pushes must carry at least delta/2 units
                if (2*lambda<delta && lambda<ResCap(a))
                {
                    if (dist[u]==dist[v]+1) canRelabel = false;
                    continue;
                }
            }

            if (dist[u]==dist[v]+1)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Push %g flow units from node %lu to node %lu",
                        static_cast<double>(lambda),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                if (v!=t && v!=s)
                {
                    if (method!=MXF_PREFLOW_SCALE)
                    {
                        if (Divergence(v)==0) Q->Insert(v,-dist[v]);
                    }
                    else if (    2*Divergence(v)<delta
                              && 2*(Divergence(v)+lambda)>=delta
                            )
                    {
                        Q->Insert(v,dist[v]);

                        #if defined(_LOGGING_)

                        sprintf(CT.logBuffer,"Activating node %lu ",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);

                        #endif
                    }
                }

                Push(a,lambda);

                if (v==s) M.SetUpperBound(-Divergence(s));
                if (v==t) M.SetLowerBound(Divergence(t));

                #if defined(_PROGRESS_)

                M.SetProgressCounter(-Divergence(t)/Divergence(s));

                #endif

                M.Trace();

                if (ResCap(a)>0) Admissible[u]->Insert(a);

                if (   method==MXF_PREFLOW_SCALE
                    && 2*Divergence(u)<delta
                   )
                {
                    canRelabel = false;
                    break;
                }
            }
        }

        if (canRelabel && Divergence(u)>0)
        {
            I.Reset(u);
            TNode dMin = NoNode;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                if (ResCap(a)>0 && dist[v]<=dMin)
                {
                    dMin = dist[v];
                    S1.Insert(a);
                }

                if (ResCap(a^1)>0 && dMin+2>=dist[v]) S2.Insert(a^1);
            }

            while (!S1.Empty())
            {
                TArc a = S1.Delete();
                TNode v = EndNode(a);

                if (dMin==dist[v]) Admissible[u]->Insert(a);
            }

            while (!S2.Empty())
            {
                TArc a = S2.Delete();
                TNode v = StartNode(a);

                if (dMin+2==dist[v]) Admissible[v]->Insert(a);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Relabelled node %lu: %lu -> %lu",
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(dist[u]),
                    static_cast<unsigned long>(dMin+1));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            dist[u] = dMin+1;

            if (method!=MXF_PREFLOW_SCALE)
            {
                Q->Insert(u,-dist[u]);
            }
            else
            {
                Q->Insert(u,dist[u]);
            }

            M.Trace();
        }

        if (method==MXF_PREFLOW_SCALE && Q->Empty() && delta>2)
        {
            delta = ceil(delta/2);

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
                LogEntry(LOG_METH,CT.logBuffer);
            }

            #if defined(_LOGGING_)

            THandle LH = LogStart(LOG_METH2,"Active nodes: ");

            #endif

            for (TNode v=0;v<n;v++)
            {
                if (v!=t && v!=s && 2*Divergence(v)>=delta)
                {
                    Q->Insert(v,dist[v]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) LogEnd(LH);

            #endif
        }
    }

    Close(H);

    delete Q;

    for (TNode i=1;i<n;i++) delete Admissible[i];
    delete Admissible[0];
    delete[] Admissible;

    ReleasePredecessors();

    if (unbounded) Error(ERR_RANGE,"MXF_PushRelabel",
        "Arcs emanating from source must have finite capacities");

    return Divergence(t)-initialFlowValue;
}
