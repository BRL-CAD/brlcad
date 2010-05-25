
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMaxBalFlow.cpp
/// \brief  Methods for non-weighted balanced network flow problems

#include "staticStack.h"
#include "shrinkingNetwork.h"
#include "balancedToBalanced.h"
#include "moduleGuard.h"


TFloat abstractBalancedFNW::MaxBalFlow(TNode s) throw(ERRange)
{
    if (s>=n) s = DefaultSourceNode();

    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MaxBalFlow",s);

    #endif

    moduleGuard M(ModMaxBalFlow,*this,"Computing maximum balanced flow...",
        moduleGuard::NO_INDENT);

    TFloat val = 0;

    switch (CT.methMaxBalFlow)
    {
        case 0:
        {
            val = BNSAndAugment(s);
            break;
        }
        case 1:
        case 2:
        case 3:
        {
            val = MicaliVazirani(s);
            break;
        }
        case 4:
        {
            val = BalancedScaling(s);
            break;
        }
        case 5:
        {
            val = Anstee(s);
            break;
        }
        default:
        {
            val = BNSAndAugment(s);
        }
    }

    #if defined(_FAILSAVE_)

    try
    {
        if (CT.methFailSave==1 && val!=FlowValue(s,s^1))
        {
            InternalError("MaxBalFlow","Solutions are corrupted");
        }
    }
    catch (...)
    {
        InternalError("MaxBalFlow","Solutions are corrupted");
    }

    #endif

    return val;
}


TFloat abstractBalancedFNW::Anstee(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("Anstee",s);

    #endif

    moduleGuard M(ModAnstee,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n*n+n1+n*n1,n*n);

    #endif

    TFloat val = MaxFlow(s,s^1);

    if (CT.SolverRunning()) M.SetUpperBound(val);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n1);

    #endif

    CancelEven();

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n*n1);

    #endif

    val = CancelOdd();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractBalancedFNW::CancelOdd() throw()
{
    #if defined(_FAILSAVE_)

    if (Q==NULL) Error(ERR_REJECTED,"CancelOdd","No odd cycles present");

    #endif

    moduleGuard M(ModCycleCancel,*this,"Refining balanced flow...",
        moduleGuard::SYNC_BOUNDS);

    balancedToBalanced G(*this);

    return G.BNSAndAugment(G.DefaultSourceNode());
}


TFloat abstractBalancedFNW::BNSAndAugment(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("BNSAndAugment",s);

    #endif

    moduleGuard M(ModBalAugment,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    TNode* dist = RawNodeColours();
    TArc*  pred = InitPredecessors();
    InitProps();
    InitPetals();
    InitBlossoms();

    TNode t = s^1;

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val -= Flow(a^1);
    }

    Close(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(cap-val,2);

    #endif

    if (CT.methMaxBalFlow>5)       // Start heuristic a la greedy 
    {
        LogEntry(LOG_METH,"Balanced network is searched...");
        OpenFold();

        while (CT.SolverRunning() && BNSHeuristicsBF(s,t))
        {
            #if defined(_LOGGING_)

            CloseFold();
            LogEntry(LOG_METH2,"Expanding path for augmentation...");
            OpenFold();

            #endif

            Expand(dist,pred,s,t);
            CloseFold();
            TFloat Lambda = FindBalCap(pred,s,t);
            BalAugment(pred,s,t,Lambda);

            val += 2*Lambda;
            M.SetLowerBound(val);
            M.Trace((unsigned long)(2*Lambda));

            #if defined(_PROGRESS_)

            M.SetProgressNext(2);

            #endif

            if (dist[t]+5>TNode(CT.methMaxBalFlow)) break;

            LogEntry(LOG_METH,"Balanced network is searched...");
            OpenFold();
        }

        CloseFold();
    }

    while (CT.SolverRunning() && BNS(s,t))
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_METH2,"Expanding path for augmentation...");

        #endif

        OpenFold();
        Expand(dist,pred,s,t);
        CloseFold();
        TFloat Lambda = FindBalCap(pred,s,t);
        BalAugment(pred,s,t,Lambda);

        val += 2*Lambda;
        M.SetLowerBound(val);
        M.Trace((unsigned long)(2*Lambda));

        #if defined(_PROGRESS_)

        M.SetProgressNext(2);

        #endif
    }

    if (CT.SolverRunning()) M.SetUpperBound(val);

    ReleasePredecessors();
    ReleaseProps();
    ReleasePetals();

    return val;
}


TFloat abstractBalancedFNW::BalancedScaling(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("BalancedScaling",s);

    #endif

    moduleGuard M(ModBalScaling,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val += Flow(a^1);
    }

    Close(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(cap-val);

    #endif

    TCap delta = MaxUCap();

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
        LogEntry(LOG_METH,CT.logBuffer);
    }

    TNode t = s^1;
    TArc* pred = InitPredecessors();

    while (delta>1 && CT.SolverRunning())
    {
        delta = floor(delta/2);

        #if defined(_PROGRESS_)

        M.SetProgressNext(2*delta);

        #endif

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        OpenFold();

        while (   BFS(residualArcs(*this,delta),
                      singletonIndex<TNode>(s,n,CT),
                      singletonIndex<TNode>(t,n,CT)) != NoNode
               && CT.SolverRunning()
              )
        {
            TCap Lambda = FindBalCap(pred,s,t);
            BalAugment(pred,s,t,Lambda);

            val += 2*Lambda;
            M.SetLowerBound(val);
            M.Trace((unsigned long)(2*Lambda));

            #if defined(_PROGRESS_)

            M.SetProgressNext(2*delta);

            #endif
        }

        CloseFold();
    }

    LogEntry(LOG_METH,"Final scaling phase...");

    val = BNSAndAugment(s);

    return val;
}


bool abstractBalancedFNW::BNS(TNode s,TNode t) throw(ERRange,ERRejected)
{
    LogEntry(LOG_METH,"Balanced network is searched...");

    bool ret = 0;

    switch (CT.methBNS)
    {
        case 0:
        {
            ret = BNSKocayStone(s,t);
            break;
        }
        case 1:
        case 2:
        {
            ret = BNSKamedaMunro(s,t);
            if (!ret) ret = BNSKocayStone(s,t);
            break;
        }
        case 3:
        {
            ret = BNSHeuristicsBF(s,t);
            if (!ret) ret = BNSKocayStone(s,t);
            break;
        }
        default:
        {
            UnknownOption("BNS",CT.methBNS);
            throw ERRejected();
        }
    }

    return ret;
}


void abstractBalancedFNW::Expand(TNode* dist,TArc* pred,TNode x,TNode y) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (dist[y]<dist[x]) Error(ERR_REJECTED,"Expand","Missing start node");

    #endif

    if (x!=y)
    {
        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Expand(%lu,%lu) puts ",
                static_cast<unsigned long>(x),static_cast<unsigned long>(y));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        TArc a = prop[y];

        if (a!=NoArc)
        {
            pred[y] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (prop)",
                    static_cast<unsigned long>(y),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,x,StartNode(a));
        }
        else
        {
            a = petal[y];
            TNode u = StartNode(a);
            TNode v = EndNode(a);
            pred[v] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
                    static_cast<unsigned long>(v),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,x,u);
            CoExpand(dist,pred,v,y);
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
}


void abstractBalancedFNW::CoExpand(TNode* dist,TArc* pred,TNode v,TNode y) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (dist[ComplNode(v)]<dist[ComplNode(y)])
        Error(ERR_REJECTED,"CoExpand","Missing end node");

    #endif

    if (y!=v)
    {
        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"CoExpand(%lu,%lu) puts ",
                static_cast<unsigned long>(v),static_cast<unsigned long>(y));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        TNode cv = v^1;
        TArc a = prop[cv];

        if (a!=NoArc)
        {
            a = a^2;
            TNode x = EndNode(a);
            pred[x] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (co-prop)",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            CoExpand(dist,pred,x,y);
        }
        else
        {
            a = (petal[cv])^2;
            TNode u = StartNode(a);
            TNode w = EndNode(a);
            pred[w] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
                    static_cast<unsigned long>(w),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,v,u);
            CoExpand(dist,pred,w,y);
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
}


bool abstractBalancedFNW::BNSKocayStone(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSExact,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    InitBlossoms();
    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    InitPetals();
    dist[s] = 0;
    Bud(s);
    THandle H = Investigate();
    investigator &I = Investigator(H);
    dynamicStack<TNode> Support(n,CT);
    staticQueue<TNode> Q(n,CT);
    Q.Insert(s);
    bool searching = true;

    while (!Q.Empty() && searching)
    {
        TNode u = Q.Delete();
        TNode cu = u^1;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Expanding node %lu",static_cast<unsigned long>(u));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        OpenFold();

        while (I.Active(u) && searching)
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);
            TNode cv = v^1;

            if (dist[cv]==NoNode)
            {
                if (dist[v]==NoNode && BalCap(a)>0)
                {
                    // Generate Bud

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    dist[v] = dist[u]+1;
                    prop[v] = a;
                    Bud(v);
                    Q.Insert(v);
                    M.Trace(1);
                }
            }
            else
            {
                TNode x = Base(u);
                TNode y = Base(v);

                if (prop[u]!=(a^1) && prop[cu]!=(a^2) && BalCap(a)>0
                    && (x!=y || dist[v]==NoNode))
                {
                    // Shrink Blossom //

                    TNode tenacity = dist[u]+dist[cv]+1;

                    // Find common predecessor //

                    while (x!=y)
                    {
                        if (dist[x]>dist[y])
                        {
                            TNode z = x^1;

                            if (dist[z]==NoNode)
                            {
                                petal[z] = a^2;
                                dist[z] = tenacity-dist[x];
                                Q.Insert(z);
                                M.Trace(1);
                            }

                            Support.Insert(x);
                            x = Pred(x);
                        }
                        else
                        {
                            TNode z = y^1;

                            if (dist[z]==NoNode)
                            {
                                petal[z] = a;
                                dist[z] = tenacity-dist[y];
                                Q.Insert(z);
                                M.Trace(1);
                            }

                            Support.Insert(y);
                            y = Pred(y);
                        }
                    }

                    // Find base node //

                    while (x!=s && BalCap(prop[x])>1)
                    {
                        TNode z = x^1;

                        if (dist[z]==NoNode)
                        {
                            petal[z] = a;
                            dist[z] = tenacity-dist[x];
                            Q.Insert(z);
                            M.Trace(1);
                        }

                        Support.Insert(x);
                        x = Pred(x);
                    }

                    TNode z = x^1;

                    if (dist[z]==NoNode)
                    {
                        petal[z] = a;
                        dist[z] = tenacity-dist[x];
                        Q.Insert(z);
                        M.Trace(1);
                    }

                    // Unify Blossoms //

                    #if defined(_LOGGING_)

                    THandle LH = NoHandle;

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Shrinking %lu",
                            static_cast<unsigned long>(x));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    while (!Support.Empty())
                    {
                        y = Support.Delete();
                        Shrink(x,y);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(y));
                            LogAppend(LH,CT.logBuffer);
                        }

                        #endif
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"(tenacity %lu)",
                            static_cast<unsigned long>(tenacity));
                        LogEnd(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            if (t!=NoNode && dist[t]!=NoNode) searching = false;
        }

        CloseFold();
    }

    Close(H);

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSKamedaMunro(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSDepth,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    InitPetals();
    dist[s] = 0;
    THandle H = Investigate();
    investigator& I = Investigator(H);
    dynamicStack<TNode> S1(n,CT);
    staticStack<TNode> S2(n,CT);
    TNode u = s;
    TNode* timeStamp = NULL;
    TNode tsCount = 1;

    if (CT.methBNS==2)
    {
        timeStamp = new TNode[n];
        timeStamp[s] = 0;
    }

    while (u!=NoNode)
    {
        TNode cu = u^1;

        if (!(I.Active(u)))
        {
            // Backtracking

            if (dist[cu]==NoNode)
            {
                if (u==s) u = NoNode;
                else u = S1.Delete();
            }
            else
            {
                if (S2.Peek()==u) S2.Delete();

                if (!S2.Empty() && dist[S1.Peek()]<=dist[S2.Peek()]) u = S2.Peek();
                else
                {
                    u = S1.Delete();
                    if (S1.Empty()) u = NoNode;
                    else u = S1.Delete();
                }
            }

            #if defined(_LOGGING_)

            if (u!=NoNode && CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Backtracking to %lu",
                    static_cast<unsigned long>(u));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            continue;
        }

        TArc a = I.Read(u);
        TNode v = EndNode(a);
        TNode cv = ComplNode(v);

        if (dist[cv]==NoNode)
        {
            if (dist[v]==NoNode && BalCap(a)>0)
            {
                // Generate Bud

                dist[v] = dist[u]+1;

                if (CT.methBNS==2)
                {
                    timeStamp[v] = tsCount++;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}, timestamp = %lu",
                            static_cast<unsigned long>(v),
                            static_cast<unsigned long>(timeStamp[v]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }

                prop[v] = a;
                S1.Insert(u);
                u = v;

                M.Trace(1);
            }
        }
        else
        {
            if (BalCap(a)>0 && prop[cu]!=(a^2) && 
                ((CT.methBNS==2 && prop[u]!=(a^1)) ||
                 (CT.methBNS!=2 && dist[v]==NoNode)
                )
               )
            {
                // Shrink Blossom //

                TNode tenacity = dist[u]+dist[cv]+1;
                bool shrunk = false;

                #if defined(_LOGGING_)

                THandle LH = NoHandle;

                #endif

                if (dist[cu]!=NoNode) u = S1.Delete();
                else
                {
                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Shrinking %lu",
                            static_cast<unsigned long>(u));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    shrunk = true;
                    petal[cu] = a^2;
                    dist[cu] = tenacity-dist[u];

                    if (CT.methBNS==2) timeStamp[cu] = tsCount++;

                    S2.Insert(cu);
                    S2.Insert(u);

                    M.Trace(1);
                }

                while (!S1.Empty() &&
                        (BalCap(prop[u])>1
                         || (CT.methBNS!=2 && dist[u]>dist[cv])
                         || (CT.methBNS==2 && timeStamp[u]>timeStamp[cv]) ) )
                {
                    if (!shrunk)
                    {
                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"Shrinking %lu",
                                static_cast<unsigned long>(u));
                            LH = LogStart(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        shrunk = true;
                    }

                    u = S1.Delete();
                    cu = u^1;

                    if (dist[cu]!=NoNode) u = S1.Delete();
                    else
                    {
                        petal[cu] = a^2;
                        dist[cu] = tenacity-dist[u];

                        if (CT.methBNS==2) timeStamp[cu] = tsCount++;

                        S2.Insert(cu);
                        S2.Insert(u);

                        M.Trace(1);
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",
                            static_cast<unsigned long>(u));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }

                #if defined(_LOGGING_)

                if (shrunk && CT.logMeth>1)
                {
                    sprintf(CT.logBuffer," (tenacity %lu, target %lu)",
                        static_cast<unsigned long>(tenacity),
                        static_cast<unsigned long>(v));
                    LogEnd(LH,CT.logBuffer);
                }

                #endif

                S1.Insert(u);
                u = S2.Peek();

                #if defined(_LOGGING_)

                if (shrunk && CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Considering node %lu",
                        static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }

        if (t!=NoNode && dist[t]!=NoNode) break;
    }

    Close(H);

    if (CT.methBNS==2) delete[] timeStamp;

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSHeuristicsBF(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSBreadth,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    #if defined(_LOGGING_)

    THandle LH =  LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    dist[s] = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);
    staticQueue<TNode> Q(n,CT);
    Q.Insert(s);
    bool searching = true;
    TNode cs = s^1;

    while (!Q.Empty() && searching)
    {
        TNode u = Q.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu,",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u) && searching)
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);

            if (dist[v]!=NoNode || BalCap(a)==0) continue;

            TNode cv = v^1;

            if (dist[cv]==NoNode)
            {
                dist[v] = dist[u]+1;
                prop[v] = a;

                if (v==t) searching = false;
                else Q.Insert(v);

                M.Trace(1);

                continue;
            }

            TNode x = u;
            TNode y = cv;
            bool blocked = false;

            while (x!=y && !blocked)
            {
                if (prop[x]==(a^2) && BalCap(prop[x])==1)
                       blocked = true;

                if (dist[x]>dist[y]) x = StartNode(prop[x]); 
                else y = StartNode(prop[y]); 
            }

            if (!blocked)
            {
                while (x!=s && BalCap(prop[x])>1) x = StartNode(prop[x]);

                if (x==s && t==cs && cv!=s)
                {
                    petal[t] = a;
                    dist[t] = dist[u]+dist[cv]+1;
                    searching = false;
                }
                else
                {
                    dist[v] = dist[u]+1;
                    prop[v] = a;

                    if (v==t) searching = false;
                    else Q.Insert(v);

                    M.Trace(1);
                }
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSMicaliVazirani(TNode s,TNode t) throw(ERRange)
{
    MicaliVazirani(s,t);

    return NodeColour(t)!=NoNode;
}


TFloat abstractBalancedFNW::MicaliVazirani(TNode s,TNode tt) throw(ERRange)
{
    moduleGuard M(ModMicaliVazirani,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val += Flow(a^1);
    }

    Reset(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((tt!=NoNode) ? m : n*(n-1.0)*m, 0);

    #endif

    InitBlossoms();
    TNode* dist = InitNodeColours(NoNode);
    TArc* pred = InitPredecessors();

    staticQueue<TNode>** Q = new staticQueue<TNode>*[n];
    staticQueue<TArc>** Anomalies = new staticQueue<TArc>*[n];
    staticQueue<TArc>** Bridges = new staticQueue<TArc>*[2*n];

    Q[0] = new staticQueue<TNode>(n,CT);
    Bridges[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<n;i++)
    {
        Q[i] = NULL;
        Anomalies[i] = NULL;
    }

    for (TNode i=1;i<2*n;i++) Bridges[i] = NULL;

    Anomalies[0] = NULL;
    TNode t = s^1;
    Anomalies[t] = new staticQueue<TArc>(2*m,CT);

    layeredShrNetwork Aux(*this,s,Q,Anomalies,Bridges);

    TNode augmentations = 1;
    investigator &I = Investigator(H);

    while (augmentations>0 && CT.SolverRunning())
    {
        LogEntry(LOG_METH,"Building layered shrinking graph...");

        InitBlossoms();
        InitNodeColours(NoNode);
        InitDistanceLabels();
        InitPredecessors();
        dist[s] = 0;
        Bud(s);
        Q[0] -> Insert(s);

        I.Reset();
        augmentations = 0;

        for (TNode i=0;i<n && augmentations==0;i++)
        {
            // Exploring minlevel nodes

            if (CT.logMeth && Q[i]!=NULL && !Q[i]->Empty())
            {
                sprintf(CT.logBuffer,
                    "Exploring minlevel nodes with distance %lu...",
                    static_cast<unsigned long>(i+1));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) OpenFold();

            #endif

            while (Q[i]!=NULL && !Q[i]->Empty())
            {
                TNode v = Q[i]->Delete();
                TNode cv = v^1;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Expanding node %lu",
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                    OpenFold();
                }

                #endif

                while (I.Active(v))
                {
                    TArc a = I.Read(v);
                    TNode w = EndNode(a);
                    TNode cw = w^1;
                    if (dist[cw]==NoNode && dist[w]>i)
                    {
                        if (BalCap(a)>0)
                        {
                            if (dist[w]==NoNode && BalCap(a)>0)
                            {
                                // Generate Bud

                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,"Node %lu explored",
                                        static_cast<unsigned long>(w));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif

                                dist[w] = i+1;
                                Bud(w);

                                if (Q[i+1]==NULL)
                                    Q[i+1] = new staticQueue<TNode>(*Q[0]);

                                Q[i+1] -> Insert(w);
                            }

                            Aux.InsertProp(a);

                            #if defined(_LOGGING_)

                            if (CT.logMeth>1)
                            {
                                sprintf(CT.logBuffer,
                                    "Prop %lu with end node %lu inserted",
                                    static_cast<unsigned long>(a),
                                    static_cast<unsigned long>(w));
                                LogEntry(LOG_METH2,CT.logBuffer);
                            }

                            #endif
                        }
                    }
                    else
                    {
                        if (BalCap(a)>0 && (dist[cv]!=dist[cw]+1 || dist[cv]>dist[v]))
                        {
                            if (dist[cw]==NoNode)
                            {
                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,
                                        "Anomaly %lu with end node %lu detected",
                                        static_cast<unsigned long>(a),
                                        static_cast<unsigned long>(w));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif

                                if (Anomalies[cw]==NULL)
                                    Anomalies[cw] = new  staticQueue<TArc>(*Anomalies[t]);

                                Anomalies[cw] -> Insert(a);
                            }
                            else
                            {
                                TNode index = dist[v]+dist[cw]+1;

                                if (Bridges[index]==NULL)
                                    Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                                if (index>=dist[v]) Bridges[index] -> Insert(a);

                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,
                                        "Bridge %lu with tenacity %lu detected",
                                        static_cast<unsigned long>(a),
                                        static_cast<unsigned long>(index));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif
                            }
                        }
                    }
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>1) CloseFold();

                #endif

            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif

            if (i>0) {delete Q[i]; Q[i]=NULL;};

            // Exploring maxlevel nodes, shrinking blossoms, augmentation 

            Aux.Phase2();

            TNode tenacity = 2*i;
            if (Bridges[tenacity]!=NULL)
            {
                if (CT.logMeth && !Bridges[tenacity]->Empty())
                {
                    sprintf(CT.logBuffer,"Exploring maxlevel nodes with tenacity %lu...",
                        static_cast<unsigned long>(tenacity));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #if defined(_LOGGING_)

                OpenFold();

                #endif

                while (!Bridges[tenacity]->Empty())
                {
                    TArc a = Bridges[tenacity]->Delete();
                    TNode u = Base(StartNode(a));
                    TNode v = Base(EndNode(a));
                    if ((u!=v || dist[u^1]==NoNode) && BalCap(a)>0 &&
                        !Aux.Blocked(u) && !Aux.Blocked(v)
                       )
                    {
                        TNode b = Aux.DDFS(a);

                        if (b!=s || tt!=NoNode)
                            Aux.ShrinkBlossom(b,a,tenacity);
                        else
                        {
                            Aux.Augment(a);
                            augmentations ++;

                            if (BalCap(a)>0 && !Aux.Blocked(u) && !Aux.Blocked(v))
                                Bridges[tenacity] -> Insert(a);
                        }
                    }
                }

                #if defined(_LOGGING_)

                CloseFold();

                #endif
            }

            tenacity++;

            if (Bridges[tenacity]!=NULL)
            {
                if (CT.logMeth && !Bridges[tenacity]->Empty())
                {
                    sprintf(CT.logBuffer,"Exploring maxlevel nodes with tenacity %lu...",
                        static_cast<unsigned long>(tenacity));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #if defined(_LOGGING_)

                OpenFold();

                #endif

                while (!Bridges[tenacity]->Empty())
                {
                    TArc a = Bridges[tenacity]->Delete();
                    TNode u = Base(StartNode(a));
                    TNode v = Base(EndNode(a));

                    if ((u!=v || dist[u^1]==NoNode) && BalCap(a)>0 &&
                        !Aux.Blocked(u) && !Aux.Blocked(v)
                       )
                    {
                        TNode b = Aux.DDFS(a);

                        if (b!=s || tt!=NoNode)
                            Aux.ShrinkBlossom(b,a,tenacity);
                        else
                        {
                            TFloat Lambda = Aux.Augment(a);
                            augmentations ++;

                            if (BalCap(a)>0 && !Aux.Blocked(u) && !Aux.Blocked(v))
                                Bridges[tenacity] -> Insert(a);

                            val += 2*Lambda;
                            M.SetLowerBound(val);

                            #if defined(_PROGRESS_)

                            M.ProgressStep(n);

                            #endif
                        }
                    }
                }

                #if defined(_LOGGING_)

                CloseFold();

                #endif
            }

            if (i>0) {delete Bridges[2*i+1]; Bridges[2*i+1]=NULL;};

            if (augmentations>0)
            {
                if (CT.logMeth)
                {
                    sprintf(CT.logBuffer,"...Phase %lu complete",
                        static_cast<unsigned long>(i));
                    LogEntry(LOG_METH,CT.logBuffer);
                }

                #if defined(_PROGRESS_)

                M.SetProgressCounter(n*(tenacity+1.0)*m);

                #endif
            }

            M.Trace();

            Aux.Phase1();
        }

        if ((CT.methMaxBalFlow==2 || CT.methMaxBalFlow==3) && augmentations>0)
        {
            LogEntry(LOG_METH,"Checking for maximality...");

            if (!BNS(s,t)) augmentations = 0;

            if (CT.methMaxBalFlow==3 && augmentations==1)
            {
                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"Performing immediate augmentation...");

                #endif

                Expand(dist,pred,s,t);
                TFloat Lambda = FindBalCap(pred,s,t);
                BalAugment(pred,s,t,Lambda);

                val += 2*Lambda;
                M.SetLowerBound(val);
            }
        }

        for (TNode i=0;i<n;i++)
        {
            if (i>0 && Q[i]!=NULL)
            {
                delete Q[i];
                Q[i] = NULL;
            }

            if (i!=t && Anomalies[i]!=NULL)
            {
                delete Anomalies[i];
                Anomalies[i] = NULL;
            }
        }

        for (TNode i=0;i<2*n;i++)
            if (i>0 && Bridges[i]!=NULL)
            {
                delete Bridges[i];
                Bridges[i] = NULL;
            }

        while (!Anomalies[t]->Empty()) Anomalies[t]->Delete();

        if (tt!=NoNode && dist[tt]!=NoNode)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_METH2,"Extracting valid path of minimum length...");

            #endif

            Aux.Expand(s,tt);
        }
        else Aux.Init();
    }

    Close(H);

    delete Q[0];
    delete Anomalies[t];
    delete Bridges[0];

    delete[] Anomalies;
    delete[] Q;
    delete[] Bridges;

    ReleasePredecessors();

    return val;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMaxBalFlow.cpp
/// \brief  Methods for non-weighted balanced network flow problems

#include "staticStack.h"
#include "shrinkingNetwork.h"
#include "balancedToBalanced.h"
#include "moduleGuard.h"


TFloat abstractBalancedFNW::MaxBalFlow(TNode s) throw(ERRange)
{
    if (s>=n) s = DefaultSourceNode();

    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MaxBalFlow",s);

    #endif

    moduleGuard M(ModMaxBalFlow,*this,"Computing maximum balanced flow...",
        moduleGuard::NO_INDENT);

    TFloat val = 0;

    switch (CT.methMaxBalFlow)
    {
        case 0:
        {
            val = BNSAndAugment(s);
            break;
        }
        case 1:
        case 2:
        case 3:
        {
            val = MicaliVazirani(s);
            break;
        }
        case 4:
        {
            val = BalancedScaling(s);
            break;
        }
        case 5:
        {
            val = Anstee(s);
            break;
        }
        default:
        {
            val = BNSAndAugment(s);
        }
    }

    #if defined(_FAILSAVE_)

    try
    {
        if (CT.methFailSave==1 && val!=FlowValue(s,s^1))
        {
            InternalError("MaxBalFlow","Solutions are corrupted");
        }
    }
    catch (...)
    {
        InternalError("MaxBalFlow","Solutions are corrupted");
    }

    #endif

    return val;
}


TFloat abstractBalancedFNW::Anstee(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("Anstee",s);

    #endif

    moduleGuard M(ModAnstee,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n*n+n1+n*n1,n*n);

    #endif

    TFloat val = MaxFlow(s,s^1);

    if (CT.SolverRunning()) M.SetUpperBound(val);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n1);

    #endif

    CancelEven();

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n*n1);

    #endif

    val = CancelOdd();

    if (CT.SolverRunning()) M.SetUpperBound(val);

    return val;
}


TFloat abstractBalancedFNW::CancelOdd() throw()
{
    #if defined(_FAILSAVE_)

    if (Q==NULL) Error(ERR_REJECTED,"CancelOdd","No odd cycles present");

    #endif

    moduleGuard M(ModCycleCancel,*this,"Refining balanced flow...",
        moduleGuard::SYNC_BOUNDS);

    balancedToBalanced G(*this);

    return G.BNSAndAugment(G.DefaultSourceNode());
}


TFloat abstractBalancedFNW::BNSAndAugment(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("BNSAndAugment",s);

    #endif

    moduleGuard M(ModBalAugment,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    TNode* dist = RawNodeColours();
    TArc*  pred = InitPredecessors();
    InitProps();
    InitPetals();
    InitBlossoms();

    TNode t = s^1;

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val -= Flow(a^1);
    }

    Close(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(cap-val,2);

    #endif

    if (CT.methMaxBalFlow>5)       // Start heuristic a la greedy 
    {
        LogEntry(LOG_METH,"Balanced network is searched...");
        OpenFold();

        while (CT.SolverRunning() && BNSHeuristicsBF(s,t))
        {
            #if defined(_LOGGING_)

            CloseFold();
            LogEntry(LOG_METH2,"Expanding path for augmentation...");
            OpenFold();

            #endif

            Expand(dist,pred,s,t);
            CloseFold();
            TFloat Lambda = FindBalCap(pred,s,t);
            BalAugment(pred,s,t,Lambda);

            val += 2*Lambda;
            M.SetLowerBound(val);
            M.Trace((unsigned long)(2*Lambda));

            #if defined(_PROGRESS_)

            M.SetProgressNext(2);

            #endif

            if (dist[t]+5>TNode(CT.methMaxBalFlow)) break;

            LogEntry(LOG_METH,"Balanced network is searched...");
            OpenFold();
        }

        CloseFold();
    }

    while (CT.SolverRunning() && BNS(s,t))
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_METH2,"Expanding path for augmentation...");

        #endif

        OpenFold();
        Expand(dist,pred,s,t);
        CloseFold();
        TFloat Lambda = FindBalCap(pred,s,t);
        BalAugment(pred,s,t,Lambda);

        val += 2*Lambda;
        M.SetLowerBound(val);
        M.Trace((unsigned long)(2*Lambda));

        #if defined(_PROGRESS_)

        M.SetProgressNext(2);

        #endif
    }

    if (CT.SolverRunning()) M.SetUpperBound(val);

    ReleasePredecessors();
    ReleaseProps();
    ReleasePetals();

    return val;
}


TFloat abstractBalancedFNW::BalancedScaling(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("BalancedScaling",s);

    #endif

    moduleGuard M(ModBalScaling,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val += Flow(a^1);
    }

    Close(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(cap-val);

    #endif

    TCap delta = MaxUCap();

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Starting with delta = %.0f",delta);
        LogEntry(LOG_METH,CT.logBuffer);
    }

    TNode t = s^1;
    TArc* pred = InitPredecessors();

    while (delta>1 && CT.SolverRunning())
    {
        delta = floor(delta/2);

        #if defined(_PROGRESS_)

        M.SetProgressNext(2*delta);

        #endif

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Next scaling phase, delta = %.0f",delta);
            LogEntry(LOG_METH,CT.logBuffer);
        }

        OpenFold();

        while (   BFS(residualArcs(*this,delta),
                      singletonIndex<TNode>(s,n,CT),
                      singletonIndex<TNode>(t,n,CT)) != NoNode
               && CT.SolverRunning()
              )
        {
            TCap Lambda = FindBalCap(pred,s,t);
            BalAugment(pred,s,t,Lambda);

            val += 2*Lambda;
            M.SetLowerBound(val);
            M.Trace((unsigned long)(2*Lambda));

            #if defined(_PROGRESS_)

            M.SetProgressNext(2*delta);

            #endif
        }

        CloseFold();
    }

    LogEntry(LOG_METH,"Final scaling phase...");

    val = BNSAndAugment(s);

    return val;
}


bool abstractBalancedFNW::BNS(TNode s,TNode t) throw(ERRange,ERRejected)
{
    LogEntry(LOG_METH,"Balanced network is searched...");

    bool ret = 0;

    switch (CT.methBNS)
    {
        case 0:
        {
            ret = BNSKocayStone(s,t);
            break;
        }
        case 1:
        case 2:
        {
            ret = BNSKamedaMunro(s,t);
            if (!ret) ret = BNSKocayStone(s,t);
            break;
        }
        case 3:
        {
            ret = BNSHeuristicsBF(s,t);
            if (!ret) ret = BNSKocayStone(s,t);
            break;
        }
        default:
        {
            UnknownOption("BNS",CT.methBNS);
            throw ERRejected();
        }
    }

    return ret;
}


void abstractBalancedFNW::Expand(TNode* dist,TArc* pred,TNode x,TNode y) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (dist[y]<dist[x]) Error(ERR_REJECTED,"Expand","Missing start node");

    #endif

    if (x!=y)
    {
        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Expand(%lu,%lu) puts ",
                static_cast<unsigned long>(x),static_cast<unsigned long>(y));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        TArc a = prop[y];

        if (a!=NoArc)
        {
            pred[y] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (prop)",
                    static_cast<unsigned long>(y),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,x,StartNode(a));
        }
        else
        {
            a = petal[y];
            TNode u = StartNode(a);
            TNode v = EndNode(a);
            pred[v] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
                    static_cast<unsigned long>(v),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,x,u);
            CoExpand(dist,pred,v,y);
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
}


void abstractBalancedFNW::CoExpand(TNode* dist,TArc* pred,TNode v,TNode y) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (dist[ComplNode(v)]<dist[ComplNode(y)])
        Error(ERR_REJECTED,"CoExpand","Missing end node");

    #endif

    if (y!=v)
    {
        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"CoExpand(%lu,%lu) puts ",
                static_cast<unsigned long>(v),static_cast<unsigned long>(y));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        TNode cv = v^1;
        TArc a = prop[cv];

        if (a!=NoArc)
        {
            a = a^2;
            TNode x = EndNode(a);
            pred[x] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (co-prop)",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            CoExpand(dist,pred,x,y);
        }
        else
        {
            a = (petal[cv])^2;
            TNode u = StartNode(a);
            TNode w = EndNode(a);
            pred[w] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
                    static_cast<unsigned long>(w),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            Expand(dist,pred,v,u);
            CoExpand(dist,pred,w,y);
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif
    }
}


bool abstractBalancedFNW::BNSKocayStone(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSExact,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    InitBlossoms();
    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    InitPetals();
    dist[s] = 0;
    Bud(s);
    THandle H = Investigate();
    investigator &I = Investigator(H);
    dynamicStack<TNode> Support(n,CT);
    staticQueue<TNode> Q(n,CT);
    Q.Insert(s);
    bool searching = true;

    while (!Q.Empty() && searching)
    {
        TNode u = Q.Delete();
        TNode cu = u^1;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Expanding node %lu",static_cast<unsigned long>(u));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        OpenFold();

        while (I.Active(u) && searching)
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);
            TNode cv = v^1;

            if (dist[cv]==NoNode)
            {
                if (dist[v]==NoNode && BalCap(a)>0)
                {
                    // Generate Bud

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    dist[v] = dist[u]+1;
                    prop[v] = a;
                    Bud(v);
                    Q.Insert(v);
                    M.Trace(1);
                }
            }
            else
            {
                TNode x = Base(u);
                TNode y = Base(v);

                if (prop[u]!=(a^1) && prop[cu]!=(a^2) && BalCap(a)>0
                    && (x!=y || dist[v]==NoNode))
                {
                    // Shrink Blossom //

                    TNode tenacity = dist[u]+dist[cv]+1;

                    // Find common predecessor //

                    while (x!=y)
                    {
                        if (dist[x]>dist[y])
                        {
                            TNode z = x^1;

                            if (dist[z]==NoNode)
                            {
                                petal[z] = a^2;
                                dist[z] = tenacity-dist[x];
                                Q.Insert(z);
                                M.Trace(1);
                            }

                            Support.Insert(x);
                            x = Pred(x);
                        }
                        else
                        {
                            TNode z = y^1;

                            if (dist[z]==NoNode)
                            {
                                petal[z] = a;
                                dist[z] = tenacity-dist[y];
                                Q.Insert(z);
                                M.Trace(1);
                            }

                            Support.Insert(y);
                            y = Pred(y);
                        }
                    }

                    // Find base node //

                    while (x!=s && BalCap(prop[x])>1)
                    {
                        TNode z = x^1;

                        if (dist[z]==NoNode)
                        {
                            petal[z] = a;
                            dist[z] = tenacity-dist[x];
                            Q.Insert(z);
                            M.Trace(1);
                        }

                        Support.Insert(x);
                        x = Pred(x);
                    }

                    TNode z = x^1;

                    if (dist[z]==NoNode)
                    {
                        petal[z] = a;
                        dist[z] = tenacity-dist[x];
                        Q.Insert(z);
                        M.Trace(1);
                    }

                    // Unify Blossoms //

                    #if defined(_LOGGING_)

                    THandle LH = NoHandle;

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Shrinking %lu",
                            static_cast<unsigned long>(x));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    while (!Support.Empty())
                    {
                        y = Support.Delete();
                        Shrink(x,y);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(y));
                            LogAppend(LH,CT.logBuffer);
                        }

                        #endif
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"(tenacity %lu)",
                            static_cast<unsigned long>(tenacity));
                        LogEnd(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            if (t!=NoNode && dist[t]!=NoNode) searching = false;
        }

        CloseFold();
    }

    Close(H);

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSKamedaMunro(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSDepth,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    InitPetals();
    dist[s] = 0;
    THandle H = Investigate();
    investigator& I = Investigator(H);
    dynamicStack<TNode> S1(n,CT);
    staticStack<TNode> S2(n,CT);
    TNode u = s;
    TNode* timeStamp = NULL;
    TNode tsCount = 1;

    if (CT.methBNS==2)
    {
        timeStamp = new TNode[n];
        timeStamp[s] = 0;
    }

    while (u!=NoNode)
    {
        TNode cu = u^1;

        if (!(I.Active(u)))
        {
            // Backtracking

            if (dist[cu]==NoNode)
            {
                if (u==s) u = NoNode;
                else u = S1.Delete();
            }
            else
            {
                if (S2.Peek()==u) S2.Delete();

                if (!S2.Empty() && dist[S1.Peek()]<=dist[S2.Peek()]) u = S2.Peek();
                else
                {
                    u = S1.Delete();
                    if (S1.Empty()) u = NoNode;
                    else u = S1.Delete();
                }
            }

            #if defined(_LOGGING_)

            if (u!=NoNode && CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Backtracking to %lu",
                    static_cast<unsigned long>(u));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            continue;
        }

        TArc a = I.Read(u);
        TNode v = EndNode(a);
        TNode cv = ComplNode(v);

        if (dist[cv]==NoNode)
        {
            if (dist[v]==NoNode && BalCap(a)>0)
            {
                // Generate Bud

                dist[v] = dist[u]+1;

                if (CT.methBNS==2)
                {
                    timeStamp[v] = tsCount++;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}, timestamp = %lu",
                            static_cast<unsigned long>(v),
                            static_cast<unsigned long>(timeStamp[v]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"New bud {%lu}",
                            static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }

                prop[v] = a;
                S1.Insert(u);
                u = v;

                M.Trace(1);
            }
        }
        else
        {
            if (BalCap(a)>0 && prop[cu]!=(a^2) && 
                ((CT.methBNS==2 && prop[u]!=(a^1)) ||
                 (CT.methBNS!=2 && dist[v]==NoNode)
                )
               )
            {
                // Shrink Blossom //

                TNode tenacity = dist[u]+dist[cv]+1;
                bool shrunk = false;

                #if defined(_LOGGING_)

                THandle LH = NoHandle;

                #endif

                if (dist[cu]!=NoNode) u = S1.Delete();
                else
                {
                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Shrinking %lu",
                            static_cast<unsigned long>(u));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    shrunk = true;
                    petal[cu] = a^2;
                    dist[cu] = tenacity-dist[u];

                    if (CT.methBNS==2) timeStamp[cu] = tsCount++;

                    S2.Insert(cu);
                    S2.Insert(u);

                    M.Trace(1);
                }

                while (!S1.Empty() &&
                        (BalCap(prop[u])>1
                         || (CT.methBNS!=2 && dist[u]>dist[cv])
                         || (CT.methBNS==2 && timeStamp[u]>timeStamp[cv]) ) )
                {
                    if (!shrunk)
                    {
                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"Shrinking %lu",
                                static_cast<unsigned long>(u));
                            LH = LogStart(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        shrunk = true;
                    }

                    u = S1.Delete();
                    cu = u^1;

                    if (dist[cu]!=NoNode) u = S1.Delete();
                    else
                    {
                        petal[cu] = a^2;
                        dist[cu] = tenacity-dist[u];

                        if (CT.methBNS==2) timeStamp[cu] = tsCount++;

                        S2.Insert(cu);
                        S2.Insert(u);

                        M.Trace(1);
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",
                            static_cast<unsigned long>(u));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }

                #if defined(_LOGGING_)

                if (shrunk && CT.logMeth>1)
                {
                    sprintf(CT.logBuffer," (tenacity %lu, target %lu)",
                        static_cast<unsigned long>(tenacity),
                        static_cast<unsigned long>(v));
                    LogEnd(LH,CT.logBuffer);
                }

                #endif

                S1.Insert(u);
                u = S2.Peek();

                #if defined(_LOGGING_)

                if (shrunk && CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Considering node %lu",
                        static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }

        if (t!=NoNode && dist[t]!=NoNode) break;
    }

    Close(H);

    if (CT.methBNS==2) delete[] timeStamp;

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSHeuristicsBF(TNode s,TNode t) throw(ERRange)
{
    moduleGuard M(ModBNSBreadth,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    #if defined(_LOGGING_)

    THandle LH =  LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    TNode* dist = InitNodeColours(NoNode);
    InitProps();
    dist[s] = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);
    staticQueue<TNode> Q(n,CT);
    Q.Insert(s);
    bool searching = true;
    TNode cs = s^1;

    while (!Q.Empty() && searching)
    {
        TNode u = Q.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu,",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u) && searching)
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);

            if (dist[v]!=NoNode || BalCap(a)==0) continue;

            TNode cv = v^1;

            if (dist[cv]==NoNode)
            {
                dist[v] = dist[u]+1;
                prop[v] = a;

                if (v==t) searching = false;
                else Q.Insert(v);

                M.Trace(1);

                continue;
            }

            TNode x = u;
            TNode y = cv;
            bool blocked = false;

            while (x!=y && !blocked)
            {
                if (prop[x]==(a^2) && BalCap(prop[x])==1)
                       blocked = true;

                if (dist[x]>dist[y]) x = StartNode(prop[x]); 
                else y = StartNode(prop[y]); 
            }

            if (!blocked)
            {
                while (x!=s && BalCap(prop[x])>1) x = StartNode(prop[x]);

                if (x==s && t==cs && cv!=s)
                {
                    petal[t] = a;
                    dist[t] = dist[u]+dist[cv]+1;
                    searching = false;
                }
                else
                {
                    dist[v] = dist[u]+1;
                    prop[v] = a;

                    if (v==t) searching = false;
                    else Q.Insert(v);

                    M.Trace(1);
                }
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    return dist[t]!=NoNode;
}


bool abstractBalancedFNW::BNSMicaliVazirani(TNode s,TNode t) throw(ERRange)
{
    MicaliVazirani(s,t);

    return NodeColour(t)!=NoNode;
}


TFloat abstractBalancedFNW::MicaliVazirani(TNode s,TNode tt) throw(ERRange)
{
    moduleGuard M(ModMicaliVazirani,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    Symmetrize();

    // Determine bounds and flow value

    TFloat val = 0;
    TCap cap = 0;
    THandle H = Investigate();

    while (Active(H,s))
    {
        TArc a = Read(H,s);

        if (a%2==0)
        {
            val += Flow(a);
            cap += UCap(a);
        }
        else val += Flow(a^1);
    }

    Reset(H);

    M.SetBounds(val,cap);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((tt!=NoNode) ? m : n*(n-1.0)*m, 0);

    #endif

    InitBlossoms();
    TNode* dist = InitNodeColours(NoNode);
    TArc* pred = InitPredecessors();

    staticQueue<TNode>** Q = new staticQueue<TNode>*[n];
    staticQueue<TArc>** Anomalies = new staticQueue<TArc>*[n];
    staticQueue<TArc>** Bridges = new staticQueue<TArc>*[2*n];

    Q[0] = new staticQueue<TNode>(n,CT);
    Bridges[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<n;i++)
    {
        Q[i] = NULL;
        Anomalies[i] = NULL;
    }

    for (TNode i=1;i<2*n;i++) Bridges[i] = NULL;

    Anomalies[0] = NULL;
    TNode t = s^1;
    Anomalies[t] = new staticQueue<TArc>(2*m,CT);

    layeredShrNetwork Aux(*this,s,Q,Anomalies,Bridges);

    TNode augmentations = 1;
    investigator &I = Investigator(H);

    while (augmentations>0 && CT.SolverRunning())
    {
        LogEntry(LOG_METH,"Building layered shrinking graph...");

        InitBlossoms();
        InitNodeColours(NoNode);
        InitDistanceLabels();
        InitPredecessors();
        dist[s] = 0;
        Bud(s);
        Q[0] -> Insert(s);

        I.Reset();
        augmentations = 0;

        for (TNode i=0;i<n && augmentations==0;i++)
        {
            // Exploring minlevel nodes

            if (CT.logMeth && Q[i]!=NULL && !Q[i]->Empty())
            {
                sprintf(CT.logBuffer,
                    "Exploring minlevel nodes with distance %lu...",
                    static_cast<unsigned long>(i+1));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) OpenFold();

            #endif

            while (Q[i]!=NULL && !Q[i]->Empty())
            {
                TNode v = Q[i]->Delete();
                TNode cv = v^1;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Expanding node %lu",
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                    OpenFold();
                }

                #endif

                while (I.Active(v))
                {
                    TArc a = I.Read(v);
                    TNode w = EndNode(a);
                    TNode cw = w^1;
                    if (dist[cw]==NoNode && dist[w]>i)
                    {
                        if (BalCap(a)>0)
                        {
                            if (dist[w]==NoNode && BalCap(a)>0)
                            {
                                // Generate Bud

                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,"Node %lu explored",
                                        static_cast<unsigned long>(w));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif

                                dist[w] = i+1;
                                Bud(w);

                                if (Q[i+1]==NULL)
                                    Q[i+1] = new staticQueue<TNode>(*Q[0]);

                                Q[i+1] -> Insert(w);
                            }

                            Aux.InsertProp(a);

                            #if defined(_LOGGING_)

                            if (CT.logMeth>1)
                            {
                                sprintf(CT.logBuffer,
                                    "Prop %lu with end node %lu inserted",
                                    static_cast<unsigned long>(a),
                                    static_cast<unsigned long>(w));
                                LogEntry(LOG_METH2,CT.logBuffer);
                            }

                            #endif
                        }
                    }
                    else
                    {
                        if (BalCap(a)>0 && (dist[cv]!=dist[cw]+1 || dist[cv]>dist[v]))
                        {
                            if (dist[cw]==NoNode)
                            {
                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,
                                        "Anomaly %lu with end node %lu detected",
                                        static_cast<unsigned long>(a),
                                        static_cast<unsigned long>(w));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif

                                if (Anomalies[cw]==NULL)
                                    Anomalies[cw] = new  staticQueue<TArc>(*Anomalies[t]);

                                Anomalies[cw] -> Insert(a);
                            }
                            else
                            {
                                TNode index = dist[v]+dist[cw]+1;

                                if (Bridges[index]==NULL)
                                    Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                                if (index>=dist[v]) Bridges[index] -> Insert(a);

                                #if defined(_LOGGING_)

                                if (CT.logMeth>1)
                                {
                                    sprintf(CT.logBuffer,
                                        "Bridge %lu with tenacity %lu detected",
                                        static_cast<unsigned long>(a),
                                        static_cast<unsigned long>(index));
                                    LogEntry(LOG_METH2,CT.logBuffer);
                                }

                                #endif
                            }
                        }
                    }
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>1) CloseFold();

                #endif

            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif

            if (i>0) {delete Q[i]; Q[i]=NULL;};

            // Exploring maxlevel nodes, shrinking blossoms, augmentation 

            Aux.Phase2();

            TNode tenacity = 2*i;
            if (Bridges[tenacity]!=NULL)
            {
                if (CT.logMeth && !Bridges[tenacity]->Empty())
                {
                    sprintf(CT.logBuffer,"Exploring maxlevel nodes with tenacity %lu...",
                        static_cast<unsigned long>(tenacity));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #if defined(_LOGGING_)

                OpenFold();

                #endif

                while (!Bridges[tenacity]->Empty())
                {
                    TArc a = Bridges[tenacity]->Delete();
                    TNode u = Base(StartNode(a));
                    TNode v = Base(EndNode(a));
                    if ((u!=v || dist[u^1]==NoNode) && BalCap(a)>0 &&
                        !Aux.Blocked(u) && !Aux.Blocked(v)
                       )
                    {
                        TNode b = Aux.DDFS(a);

                        if (b!=s || tt!=NoNode)
                            Aux.ShrinkBlossom(b,a,tenacity);
                        else
                        {
                            Aux.Augment(a);
                            augmentations ++;

                            if (BalCap(a)>0 && !Aux.Blocked(u) && !Aux.Blocked(v))
                                Bridges[tenacity] -> Insert(a);
                        }
                    }
                }

                #if defined(_LOGGING_)

                CloseFold();

                #endif
            }

            tenacity++;

            if (Bridges[tenacity]!=NULL)
            {
                if (CT.logMeth && !Bridges[tenacity]->Empty())
                {
                    sprintf(CT.logBuffer,"Exploring maxlevel nodes with tenacity %lu...",
                        static_cast<unsigned long>(tenacity));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #if defined(_LOGGING_)

                OpenFold();

                #endif

                while (!Bridges[tenacity]->Empty())
                {
                    TArc a = Bridges[tenacity]->Delete();
                    TNode u = Base(StartNode(a));
                    TNode v = Base(EndNode(a));

                    if ((u!=v || dist[u^1]==NoNode) && BalCap(a)>0 &&
                        !Aux.Blocked(u) && !Aux.Blocked(v)
                       )
                    {
                        TNode b = Aux.DDFS(a);

                        if (b!=s || tt!=NoNode)
                            Aux.ShrinkBlossom(b,a,tenacity);
                        else
                        {
                            TFloat Lambda = Aux.Augment(a);
                            augmentations ++;

                            if (BalCap(a)>0 && !Aux.Blocked(u) && !Aux.Blocked(v))
                                Bridges[tenacity] -> Insert(a);

                            val += 2*Lambda;
                            M.SetLowerBound(val);

                            #if defined(_PROGRESS_)

                            M.ProgressStep(n);

                            #endif
                        }
                    }
                }

                #if defined(_LOGGING_)

                CloseFold();

                #endif
            }

            if (i>0) {delete Bridges[2*i+1]; Bridges[2*i+1]=NULL;};

            if (augmentations>0)
            {
                if (CT.logMeth)
                {
                    sprintf(CT.logBuffer,"...Phase %lu complete",
                        static_cast<unsigned long>(i));
                    LogEntry(LOG_METH,CT.logBuffer);
                }

                #if defined(_PROGRESS_)

                M.SetProgressCounter(n*(tenacity+1.0)*m);

                #endif
            }

            M.Trace();

            Aux.Phase1();
        }

        if ((CT.methMaxBalFlow==2 || CT.methMaxBalFlow==3) && augmentations>0)
        {
            LogEntry(LOG_METH,"Checking for maximality...");

            if (!BNS(s,t)) augmentations = 0;

            if (CT.methMaxBalFlow==3 && augmentations==1)
            {
                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"Performing immediate augmentation...");

                #endif

                Expand(dist,pred,s,t);
                TFloat Lambda = FindBalCap(pred,s,t);
                BalAugment(pred,s,t,Lambda);

                val += 2*Lambda;
                M.SetLowerBound(val);
            }
        }

        for (TNode i=0;i<n;i++)
        {
            if (i>0 && Q[i]!=NULL)
            {
                delete Q[i];
                Q[i] = NULL;
            }

            if (i!=t && Anomalies[i]!=NULL)
            {
                delete Anomalies[i];
                Anomalies[i] = NULL;
            }
        }

        for (TNode i=0;i<2*n;i++)
            if (i>0 && Bridges[i]!=NULL)
            {
                delete Bridges[i];
                Bridges[i] = NULL;
            }

        while (!Anomalies[t]->Empty()) Anomalies[t]->Delete();

        if (tt!=NoNode && dist[tt]!=NoNode)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_METH2,"Extracting valid path of minimum length...");

            #endif

            Aux.Expand(s,tt);
        }
        else Aux.Init();
    }

    Close(H);

    delete Q[0];
    delete Anomalies[t];
    delete Bridges[0];

    delete[] Anomalies;
    delete[] Q;
    delete[] Bridges;

    ReleasePredecessors();

    return val;
}
