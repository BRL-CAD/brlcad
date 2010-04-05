
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveAsyTSP.cpp
/// \brief  ATSP branch & bound and heuristic codes

#include "branchAsyTSP.h"
#include "sparseDigraph.h"
#include "denseDigraph.h"
#include "binaryHeap.h"
#include "abstractGraph.h"
#include "moduleGuard.h"


branchAsyTSP::branchAsyTSP(abstractMixedGraph& _G,TNode _root,
    abstractMixedGraph::TRelaxTSP method,int nCandidates) throw() :
    branchNode<TArc,TFloat>(_G.M(),_G.Context()),
    G(_G),root(_root),relaxationMethod(method)
{
    if (nCandidates>=0 && G.IsDense())
    {
        SetCandidateGraph(nCandidates);
    }
    else
    {
        X = new sparseDiGraph(G,OPT_CLONE);
    }

    H = X->Investigate();
    selected = 0;
    depth = TArc(ceil(X->N()*log(double(X->N())*0.1)));

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,G.Pi(v));

    for (TArc a=0;a<n;a++)
        if (X->StartNode(2*a)==X->EndNode(2*a)) Lower(a);

    for (TNode v=0;v<G.N();v++) CheckNode(v);

    LogEntry(LOG_MEM,"(asymmetric TSP)");
}


branchAsyTSP::branchAsyTSP(branchAsyTSP& Node) throw() :
    branchNode<TArc,TFloat>(Node.G.M(),Node.Context(),Node.scheme), G(Node.G)
{
    X = new sparseDiGraph(*Node.X,OPT_CLONE);
    H = X->Investigate();
    unfixed = Node.Unfixed();
    selected = Node.selected;
    root = Node.root;
    depth = Node.depth;

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,Node.X->Pi(v));

    for (TArc a=0;a<X->M();a++) X->SetSub(2*a,Node.X -> Sub(2*a));

    LogEntry(LOG_MEM,"(asymmetric TSP)");
}


branchAsyTSP::~branchAsyTSP() throw()
{
    X -> Close(H);
    delete X;

    LogEntry(LOG_MEM,"(asymmetric TSP)");
}


void branchAsyTSP::SetCandidateGraph(int nCandidates) throw()
{
    LogEntry(LOG_METH,"Constructing candidate graph...");

    CT.SuppressLogging();
    sparseDiGraph* Y = new sparseDiGraph(G,OPT_CLONE);
    Y->Representation() -> SetCUCap(1);
    Y -> InitSubgraph();
    CT.RestoreLogging();

    for (TNode v=0;v<G.N();v++)
        if (G.Pred(v)!=NoArc)
        {
            TNode u = G.StartNode(G.Pred(v));
            TNode w = G.EndNode(G.Pred(v));
            Y -> SetSub(Y->Adjacency(u,w),1);
        }

    for (int i=0;i<20;i++)
    {
        CT.SuppressLogging();
        TFloat ret = Y->TSP_HeuristicRandom();
        CT.RestoreLogging();

        if (ret<InfFloat)
        {
            for (TNode v=0;v<G.N();v++) Y -> SetSub(Y->Pred(v),1);

            #if defined(_LOGGING_)

            if (CT.logMeth>=2)
            {
                sprintf(CT.logBuffer,"Adding tour of length %g...",ret);
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            if (ret<G.Length())
            {
                TArc* predY = Y->GetPredecessors();
                TArc* predG = G.InitPredecessors();

                for (TNode v=0;v<G.N();v++)
                {
                    TArc a = predY[v];
                    TNode u = Y->StartNode(a);
                    TArc a2 = G.Adjacency(u,v);
                    predG[v] = a2;
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>=2)
                {
                    sprintf(CT.logBuffer,"...Saved to original graph");
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }
    }

    binaryHeap<TArc,TFloat> Q(2*Y->M(),CT);
    H = Y->Investigate();
    for (TNode v=0;v<G.N();v++)
    {
        while (Y->Active(H,v))
        {
            TArc a = Y->Read(H,v);
            Q.Insert(a,Y->Length(a));
        }

        int i = 0;
        while (!Q.Empty())
        {
            TArc a = Q.Delete();

            if (Y->Sub(a)==0 && !G.Blocking(a) &&
                (i<nCandidates || G.Sub(a)>0))
            {
                Y->SetSub(a,1);
                i++;
            }
        }

        Y -> Reset(H,v);
        while (Y->Active(H,v))
        {
            TArc a = Y->Read(H,v);
            Q.Insert(a,Y->Length(a));
        }

        i = 0;
        while (!Q.Empty())
        {
            TArc a = Q.Delete();

            if (Y->Sub(a)==0 && !G.Blocking(a^1) &&
                (i<nCandidates || G.Sub(a)>0))
            {
                Y->SetSub(a^1,1);
                i++;
            }
        }
    }
    Y->Close(H);

    X = new sparseDiGraph(*Y,OPT_SUB);
    X->Representation() -> SetCUCap(1);
    n = unfixed = X->M();

    if (CT.traceLevel==3) CT.Trace(OH);

    CT.SuppressLogging();
    delete Y;
    CT.RestoreLogging();
}


unsigned long branchAsyTSP::Size() const throw()
{
    return
          sizeof(branchAsyTSP)
        + managedObject::Allocated()
        + branchNode<TArc,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchAsyTSP::Allocated() const throw()
{
    return 0;
}


TArc branchAsyTSP::SelectVariable() throw()
{
    TArc ret0 = NoArc;
    TFloat maxDiff = -InfFloat;

    for (TNode v=0;v<X->N();v++)
    {
        TArc a0 = NoArc;
        TArc a1 = NoArc;
        TNode thisDegree = 0;
        bool fixedOut = false;

        X -> Reset(H,v);

        while (X->Active(H,v) && !fixedOut)
        {
            TArc a = X->Read(H,v);

            if (X->Sub(a)==1 && !(X->Blocking(a)))
            {
                thisDegree++;

                if (X->LCap(a)==0)
                {
                    if (a0==NoArc || X->Length(a)<X->Length(a0))
                    {
                        TArc aSwap = a0;
                        a0 = a;
                        a = aSwap;
                    }

                    if (a1==NoArc || X->Length(a)<X->Length(a1))
                    {
                        a1 = a;
                    }
                }
                else fixedOut = true;
            }
        }

        if (thisDegree<2) continue;

        // Now, we have fixedOut==0 and a1!=NoArc

        if ( X->Length(a1)-X->Length(a0)>maxDiff )
        {
            ret0 = a0;
            maxDiff = X->Length(a1)-X->Length(a0);
        }
    }

    if (ret0!=NoArc) return ret0>>1;

    #if defined(_FAILSAVE_)

    InternalError("SelectVariable","No branching variable found");

    #endif

    throw ERInternal();
}


branchNode<TArc,TFloat>::TBranchDir branchAsyTSP::DirectionConstructive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionConstructive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat>::TBranchDir branchAsyTSP::DirectionExhaustive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionExhaustive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat> *branchAsyTSP::Clone() throw()
{
    return new branchAsyTSP(*this);
}


void branchAsyTSP::Raise(TArc i) throw(ERRange)
{
    Raise(i,true);
}


void branchAsyTSP::Raise(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Raise",a);

    #endif

    if (X->Sub(2*a)==0)
    {
        X -> SetSub(2*a,1);
        if (objective!=InfFloat) solved = false;
    }

    X->Representation() -> SetLCap(2*a,1);
    unfixed--;
    selected++;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = false;
}


void branchAsyTSP::Lower(TArc i) throw(ERRange)
{
    Lower(i,true);
}


void branchAsyTSP::Lower(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Lower",a);

    #endif

    if (X->Sub(2*a)==1)
    {
        X -> SetSub(2*a,0);
        if (objective!=InfFloat) solved = false;
    }

    X->Representation() -> SetUCap(2*a,0);
    unfixed--;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = false;
}


void branchAsyTSP::CheckNode(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=X->N()) NoSuchNode("CheckNode",v);

    #endif

    char fixedIn = 0;
    char fixedOut = 0;
    X -> Reset(H,v);

    while (X->Active(H,v) && fixedIn<2 && fixedOut<2)
    {
        TArc a = X->Read(H,v);

        if (X->LCap(a)==1 && !(X->Blocking(a^1))) fixedIn++;
        if (X->LCap(a)==1 && !(X->Blocking(a))) fixedOut++;
    }

    if (fixedIn>1 || fixedOut>1)
    {
        // Fixed arcs cannot be extended to a Hamiltonian cycle
        solved = 1;
        objective = InfFloat;
    }

    if (fixedIn)
    {
        X -> Reset(H,v);

        while (X->Active(H,v))
        {
            TArc a2 = X->Read(H,v);

            if (X->LCap(a2)==0 && X->UCap(a2)==1 && !(X->Blocking(a2^1)))
            {
                Lower(a2>>1,false);
                CheckNode(X->EndNode(a2));
            }
        }
    }

    if (fixedOut)
    {
        X -> Reset(H,v);

        while (X->Active(H,v))
        {
            TArc a2 = X->Read(H,v);

            if (X->LCap(a2)==0 && X->UCap(a2)==1 && !(X->Blocking(a2)))
            {
                Lower(a2>>1,false);
                CheckNode(X->EndNode(a2));
            }
        }
    }

    if (fixedIn || fixedOut) return;

    TArc aIn = NoArc;
    TArc aOut = NoArc;
    TArc incidIn = 0;
    TArc incidOut = 0;
    X -> Reset(H,v);

    while ((incidIn<2 || incidOut<2) && X->Active(H,v))
    {
        TArc a = X->Read(H,v);

        if (X->UCap(a)==1)
        {
            if (!(X->Blocking(a^1)))
            {
                incidIn++;
                if (X->LCap(a)==0) aIn = a;
            }

            if (!(X->Blocking(a)))
            {
                incidOut++;
                if (X->LCap(a)==0) aOut = a;
            }
        }
    }

    if (incidIn==1 && aIn!=NoArc)
    {
        Raise(aIn>>1,false);
        CheckNode(X->EndNode(aIn));
    }

    if (incidOut==1 && aOut!=NoArc)
    {
        Raise(aOut>>1,false);
        CheckNode(X->EndNode(aIn));
    }
}


TFloat branchAsyTSP::SolveRelaxation() throw()
{
    for (TNode v=0;v<X->N();v++)
    {
        bool fixedIn = false;
        bool fixedOut = false;
        X -> Reset(H,v);

        while (X->Active(H,v))
        {
            TArc a = X->Read(H,v);
            if (X->LCap(a)==1 && !(X->Blocking(a^1)))
            {
                if (!fixedIn) fixedIn = 1;
                else return InfFloat;
            }

            if (X->LCap(a)==1 && !(X->Blocking(a)))
            {
                if (!fixedOut) fixedOut = 1;
                else return InfFloat;
            }
        }
    }

    CT.SuppressLogging();

    if (X->CutNodes()!=X->DFS_BICONNECTED || !(X->StronglyConnected()))
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    if (unfixed==0 && !Feasible()) return InfFloat;

    TFloat objective = InfFloat;
    try
    {
        objective = X->MinTree(X->MST_EDMONDS,X->MST_ONE_CYCLE_REDUCED,root);

        if (    scheme != NULL
             && relaxationMethod >= X->TSP_RELAX_FAST
             && scheme->nIterations > 1
             && unfixed > 0
             && scheme->SearchState() != scheme->INITIAL_DFS
             && objective < scheme->savedObjective-1+CT.epsilon
           )
        {
            // Apply subgradient optimization for most effective pruning

            X -> InitSubgraph();
            X -> ReleasePredecessors();
            TFloat upperBound = scheme->savedObjective;
            objective = X->TSP_SubOpt1Tree(relaxationMethod,root,upperBound,true);
            X -> MinTree(X->MST_EDMONDS,X->MST_ONE_CYCLE_REDUCED,root);
        }
    }
    catch (ERRejected)
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    X -> InitSubgraph();
    X -> AddToSubgraph();

    CT.RestoreLogging();

    return objective;
}


branchNode<TNode,TFloat>::TObjectSense branchAsyTSP::ObjectSense() const throw()
{
    return MINIMIZE;
}


TFloat branchAsyTSP::Infeasibility() const throw()
{
    return InfFloat;
}


bool branchAsyTSP::Feasible() throw()
{
    CT.SuppressLogging();

    bool feasible = (X->ExtractCycles()!=NoNode);

    if (!feasible) X -> ReleaseDegrees();

    CT.RestoreLogging();

    return feasible;
}


TFloat branchAsyTSP::LocalSearch() throw()
{
    TArc* predX = X->GetPredecessors();
    TArc* predG = G.InitPredecessors();

    for (TNode v=0;v<G.N();v++)
    {
        TArc a = predX[v];
        TNode u = X->StartNode(a);
        TArc a2 = G.Adjacency(u,v);
        predG[v] = a2;
    }

    CT.SuppressLogging();
    objective = G.TSP_LocalSearch(predG);
    CT.RestoreLogging();

    // It can happen that the found tour is better than every tour in the candidate graph
    scheme -> M.SetUpperBound(
        (objective>=scheme->M.LowerBound()) ? objective : scheme->M.LowerBound());

    if (CT.traceLevel==3) G.Display();

    return objective;
}


void branchAsyTSP::SaveSolution() throw()
{
}


TFloat abstractMixedGraph::TSP_BranchAndBound(TRelaxTSP method,int nCandidates,
    TNode root,TFloat upperBound) throw(ERRejected)
{
    moduleGuard M(ModTSP,*this,"ATSP Branch and Bound...",moduleGuard::SYNC_BOUNDS);

    branchAsyTSP *masterNode = new branchAsyTSP(*this,root,method,nCandidates);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif

    if (!GetPredecessors()) upperBound = InfFloat;

    branchScheme<TArc,TFloat>::TSearchLevel level =
        branchScheme<TArc,TFloat>::SEARCH_CONSTRUCT;

    if (nCandidates<0)
    {
        level = branchScheme<TArc,TFloat>::SEARCH_EXHAUSTIVE;
    }
    else
    {
        // The candidate tours may have improved the original upper bound
        upperBound = Length();
    }

    branchScheme<TArc,TFloat> scheme(masterNode,upperBound,ModTSP,level);

    if (scheme.savedObjective!=InfFloat)
    {
        M.SetUpperBound(scheme.savedObjective);

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,"...Optimal tour has length %g",
                scheme.savedObjective);
            M.Shutdown(LOG_RES,CT.logBuffer);
        }

        return scheme.savedObjective;
    }

    M.Shutdown(LOG_RES,"...Problem is infeasible");

    return InfFloat;
}


TFloat abstractMixedGraph::TSP(THeurTSP methHeur,TRelaxTSP methRelax1,TRelaxTSP methRelax2,TNode root)
    throw(ERRange,ERRejected)
{
    if (root>=n) root = DefaultRootNode();

    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("TSP",root);

    #endif

    if (methHeur==TSP_HEUR_DEFAULT)    methHeur   = THeurTSP(CT.methHeurTSP);
    if (methRelax1==TSP_RELAX_DEFAULT) methRelax1 = TRelaxTSP(CT.methRelaxTSP1);
    if (methRelax2==TSP_RELAX_DEFAULT) methRelax2 = TRelaxTSP(CT.methRelaxTSP2);

    moduleGuard M(ModTSP,*this,"Starting TSP solver...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1, 0);

    #endif

    TArc* pred = GetPredecessors();
    TArc* savedTour = NULL;
    TFloat savedLength = 0;

    if (pred)
    {
        TNode i = 0;
        TNode u = NoNode;
        TArc a = pred[0];
        while (i<n+1 && u!=0 && a!=NoArc && UCap(a)>=1)
        {
            savedLength += Length(a);
            i++;
            u = StartNode(a);
            a = pred[u];
        }

        for (a=0;a<m;a++)
        {
            if (LCap(2*a)>0 && pred[EndNode(2*a)]!=2*a && pred[EndNode(2*a+1)]!=2*a+1)
            {
                i = 0;
                break;
            }
        }

        if (i==n)
        {
            savedTour = new TArc[n];

            for (TNode v=0;v<n;v++) savedTour[v] = pred[v];

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"...Initial tour has length %g",savedLength);
                LogEntry(LOG_RES,CT.logBuffer);
            }

            M.SetUpperBound(savedLength);
        }
        else savedLength = InfFloat;
    }
    else
    {
        pred = RawPredecessors();
    }

    TFloat lower = InfFloat;

    if (!IsDense())
    {
        LogEntry(LOG_METH,"Checking for feasibility...");
        OpenFold();

        if (IsUndirected())
        {
            lower = MinTree(MST_DEFAULT,MST_ONE_CYCLE_REDUCED,root);
        }
        else
        {
            lower = MinTree(MST_EDMONDS,MST_ONE_CYCLE_REDUCED,root);
        }

        CloseFold();

        M.SetLowerBound(ceil(lower-CT.epsilon));

        if (lower==InfFloat)
        {
            M.Shutdown(LOG_RES,"...Graph is non-Hamiltonian");
            return InfFloat;
        }
        else LogEntry(LOG_RES,"...Check passed successfully");
    }

    #if defined(_PROGRESS_)

    if (methRelax1<TSP_RELAX_1TREE && methRelax2<TSP_RELAX_1TREE)
    {
        M.SetProgressNext(1);
    }

    #endif

    TFloat bestUpper = InfFloat;
    try
    {
        bestUpper = TSP_Heuristic(methHeur,root);
    }
    catch (ERRejected) {};

    if (savedTour)
    {
        if (savedLength<bestUpper)
        {
            for (TNode v=0;v<n;v++) pred[v] = savedTour[v];

            bestUpper = savedLength;
        }

        delete[] savedTour;
    }
    else if (bestUpper==InfFloat)
    {
        ReleasePredecessors();
    }

    if (methRelax1>=TSP_RELAX_1TREE)
    {
        LogEntry(LOG_METH,"Computing lower bound...");

        try
        {
            #ifdef _PROGRESS_

            if (methRelax2<TSP_RELAX_1TREE) M.SetProgressNext(1);

            #endif

            if (root==NoNode)
            {
                lower = TSP_SubOpt1Tree(methRelax1,0,bestUpper,false);
            }
            else
            {
                lower = TSP_SubOpt1Tree(methRelax1,root,bestUpper,false);
            }
        }
        catch (ERRejected) {};

        if (lower==InfFloat)
        {
            return InfFloat;
        }
    }

    if (methRelax2>=TSP_RELAX_1TREE && lower<bestUpper)
    {
        #ifdef _PROGRESS_

        M.SetProgressNext(1);

        #endif

        if (root==NoNode) root = 0;

        bestUpper = TSP_BranchAndBound(methRelax2,CT.methCandidates,root,bestUpper);
        M.SetUpperBound(bestUpper);
    }

    return bestUpper;
}


TFloat abstractMixedGraph::TSP_SubOpt1Tree(TRelaxTSP method,TNode root,
    TFloat& bestUpper,bool branchAndBound) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("TSP_SubOpt1Tree",root);

    #endif

    moduleGuard M(ModSubgradOptTSP,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1, 0);

    #endif

    TArc* pred = GetPredecessors();
    TArc *bestTour = new TArc[n];

    if (!pred) pred = InitPredecessors();

    for (TNode v=0;v<n;v++) bestTour[v] = pred[v];


    TFloat* potential = GetPotentials();
    TFloat *bestPi = new TFloat[n];
    TFloat maxPi = 0;

    if (!potential)
    {
        potential = InitPotentials();
    }
    else
    {
        for (TNode v=0;v<n;v++)
        {
            bestPi[v] = potential[v];

            if (fabs(potential[v])>maxPi) maxPi = fabs(potential[v]);
        }

        LogEntry(LOG_METH,"Using existing potentials..");
    }


    TFloat *oldDegOut = new TFloat[n];

    TFloat bestLower = -InfFloat;
    TFloat lastBound = -InfFloat;
    TFloat gap = InfFloat;
    TNode dev = n;
    TNode bestDev = n;
    unsigned long step = 0;

    sprintf(CT.logBuffer,"Computing minimum %lu-tree...",static_cast<unsigned long>(root));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();
    TFloat thisBound = MinTree(MST_EDMONDS,MST_ONE_CYCLE_REDUCED,root);
    CT.RestoreLogging();

    if (method==TSP_RELAX_1TREE)
    {
        for (TNode v=0;v<n;v++) pred[v] = bestTour[v];

        delete[] bestTour;
        delete[] bestPi;

        M.SetLowerBound(ceil(thisBound-CT.epsilon));

        if (CT.logRes && bestUpper!=InfFloat && thisBound!=-InfFloat)
        {
            sprintf(CT.logBuffer,"Found gap is [%g,%g] or %g percent",
                ceil(thisBound),bestUpper,(bestUpper/ceil(thisBound)-1)*100);
            M.Shutdown(LOG_RES,CT.logBuffer);
        }

        return ceil(thisBound);
    }

    // Compute initial step length

    TFloat t0 = (maxPi>0 && t0>maxPi/10) ? maxPi/10 : fabs(thisBound/n);
    TFloat t = t0;
    const TFloat tMin = 0.02;

    TFloat *oldPi = new TFloat[n];

    while (bestUpper+CT.epsilon>=ceil(bestLower-CT.epsilon)+1
           && t>tMin && dev>0 && step<5000
           && CT.SolverRunning())
    {
        step++;

        #if defined(_PROGRESS_)

        M.SetProgressCounter((t>=t0) ? 0 : (log(t0/tMin)-log(t/tMin)) / log(t0/tMin));

        #endif

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Step number %lu",step);
            LogEntry(LOG_METH2,CT.logBuffer);
            sprintf(CT.logBuffer,"Step length: %g",t);
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif


        if (step>1)
        {
            sprintf(CT.logBuffer,"Computing minimum %lu-tree...",static_cast<unsigned long>(root));
            LogEntry(LOG_METH,CT.logBuffer);
            CT.SuppressLogging();
            thisBound = MinTree(MST_EDMONDS,MST_ONE_CYCLE_REDUCED,root);
            CT.RestoreLogging();

            if (CT.logRes>=2)
            {
                sprintf(CT.logBuffer,"...Tree length is %g",thisBound);
                LogEntry(LOG_RES2,CT.logBuffer);
            }
        }

        dev = 0;
        TFloat turn = 0;
        TFloat normSqare1 = 0;
        TFloat normSqare2 = 0;

        for (TNode v=0;v<n;v++)
        {
            if (DegOut(v)!=1) dev++;

            TFloat oldDiff = potential[v]-oldPi[v];
            oldPi[v] = potential[v];

            if (step==1)
            {
                potential[v] = potential[v]+t*(DegOut(v)-1);
                oldDiff = 0;
            }
            else
            {
                potential[v] = potential[v]+t*(0.7*DegOut(v)+0.3*oldDegOut[v]-1);
            }

            TFloat newDiff = potential[v]-oldPi[v];
            turn += oldDiff*newDiff;
            normSqare1 += newDiff*newDiff;
            normSqare2 += oldDiff*oldDiff;
        }

        if (fabs(normSqare1)>0.001 && fabs(normSqare2)>0.001)
        {
            turn /= sqrt(normSqare1*normSqare2);
        }
        else
        {
            // This includes step == 1
            turn = 0;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>=2)
        {
            sprintf(CT.logBuffer,"Nodes with incorrect degrees: %lu",static_cast<unsigned long>(dev));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        bool newGap = false;

        if (thisBound>n*fabs(MaxLength())+1)
        {
            bestLower = bestUpper = InfFloat;
            dev = 0;
            LogEntry(LOG_RES,"Problem is infeasible");
            break;
        }
        else if (thisBound>bestLower+CT.epsilon)
        {
            bestLower = thisBound;

            if (!branchAndBound)
            {
                // Do not export the lower bound if the procedure was called
                // for a branch&bound subproblem
                M.SetLowerBound(ceil(bestLower-CT.epsilon));
            }

            newGap = true;

            for (TNode v=0;v<n;v++) bestPi[v] = potential[v];
        }

        if (!branchAndBound && (step==1 || dev<bestDev || dev<5+n/100))
        {
            LogEntry(LOG_METH,"Transforming to Hamiltonian cycle...");

            bestDev = dev;
            TFloat thisUpper = TSP_HeuristicTree(root);

            if (thisUpper<bestUpper)
            {
                #if defined(_LOGGING_)

                LogEntry(LOG_METH,"Saving tour...");

                #endif

                for (TNode v=0;v<n;v++) bestTour[v] = pred[v];

                bestUpper = thisUpper;
                M.SetUpperBound(bestUpper);
                newGap = true;
            }
        }

        if (newGap)
        {
            gap = bestUpper/ceil(bestLower-CT.epsilon)-1;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Gap decreases to %g percent",gap*100);
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        InitDegInOut();
        TFloat *swapDegOut = oldDegOut;
        oldDegOut = sDegOut;
        sDegOut = swapDegOut;

        if (method==TSP_RELAX_FAST)
        {
            if (turn<=0.1)
            {
                t *= 0.85;
            }
            else if (thisBound<=lastBound)
            {
                t *= 0.95;
            }

            if (newGap)
            {
                t *= 1.05;
            }
        }
        else
        {
            if (turn<=0.1)
            {
                t *= 0.95;
            }
            else if (thisBound<=lastBound)
            {
                t *= 0.98;
            }

            if (newGap)
            {
                t *= 1.15;
            }
        }

        lastBound = thisBound;
    }

    for (TNode v=0;v<n;v++) potential[v] = bestPi[v];
    delete[] oldPi;
    delete[] bestPi;

    delete[] sDegOut;
    sDegOut = oldDegOut;

    for (TNode v=0;v<n;v++) pred[v] = bestTour[v];
    delete[] bestTour;

    if (CT.logMeth==1)
    {
        sprintf(CT.logBuffer,"Total number of iterations: %lu",step);
        LogEntry(LOG_METH,CT.logBuffer);
    }

    if (CT.logRes && bestUpper!=InfFloat && bestLower!=-InfFloat)
    {
        sprintf(CT.logBuffer,"...Final gap is [%g,%g] or %g percent",
            ceil(bestLower),bestUpper,gap*100);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return bestLower;
}


TFloat denseDiGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("TSP_Heuristic",root);

    #endif

    moduleGuard M(ModTSP,*this,"Computing heuristic tour...",
        moduleGuard::SYNC_BOUNDS | moduleGuard::NO_INDENT);

    TFloat ret = InfFloat;

    if (!CLCap() || MaxLCap()>0 || !CUCap() || MaxUCap()<1)
    {
        LogEntry(LOG_METH2,"...Non-trivial capacity bounds impose restrictions on the feasibility set");

        ret = abstractMixedGraph::TSP_Heuristic(method,root);
    }
    else
    {
        if (method==TSP_HEUR_DEFAULT) method = THeurTSP(CT.methHeurTSP);

        switch (method)
        {
            case TSP_HEUR_RANDOM:
            {
                ret = TSP_HeuristicRandom();
                break;
            }
            case TSP_HEUR_NEAREST:
            case TSP_HEUR_FARTHEST:
            {
                ret = TSP_HeuristicInsert(method,root);
                break;
            }
            case TSP_HEUR_CHRISTOFIDES:
            case TSP_HEUR_TREE:
            {
                ret = TSP_HeuristicTree(root);
                break;
            }
            default:
            {
                UnknownOption("TSP_Heuristic",method);
            }
        }
    }

    return ret;
}


TFloat abstractMixedGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (CLCap() && MaxLCap()>0)
        Error(ERR_REJECTED,"TSP_Heuristic","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModTSP,*this,"Transforming to dense digraph...",
        moduleGuard::SYNC_BOUNDS);

    denseDiGraph G(n,0,CT);
    G.ImportLayoutData(*this);
    graphRepresentation* GR = G.Representation();

    TFloat offset = 0;
    TFloat undefLength = n*(fabs(MaxLength()+1));

    if (CLength()) undefLength = 2*Length(0);

    if (!CLCap() || MaxLCap()>0)
    {
        offset = undefLength;
        LogEntry(LOG_METH2,"...Non-trivial lower bounds impose restrictions on the feasibility set");
    }

    TArc* pred = GetPredecessors();
    TArc* predG = NULL;

    if (pred)
    {
        predG = G.RawPredecessors();

        for (TNode v=0;v<n;v++)
        {
            if (pred[v]!=NoArc) predG[v] = G.Adjacency(StartNode(pred[v]),EndNode(pred[v]));
        }
    }
    else
    {
        pred = RawPredecessors();
    }

    for (TArc i=0;i<G.M();i++)
    {
        TArc a = Adjacency(G.StartNode(2*i),G.EndNode(2*i));

        if (a!=NoArc && LCap(a)>0)
        {
            GR -> SetLength(2*i,Length(a));
        }
        else if (a!=NoArc && UCap(a)>=1)
        {
            GR -> SetLength(2*i,offset+Length(a));
        }
        else
        {
            GR -> SetLength(2*i,offset+undefLength);
        }
    }

    if (Dim()>0)
    {
        for (TNode v=0;v<n;v++)
        {
            GR -> SetC(v,0,C(v,0));
            GR -> SetC(v,1,C(v,1));
        }
    }

    int swapSolve = CT.methSolve;
    CT.methSolve = 1;

    G.TSP(root);

    TFloat length = 0;

    if (root==NoNode) root = 0;

    TNode v = root;

    do
    {
        TNode u = G.StartNode(predG[v]);
        TArc a = Adjacency(u,v);

        if (a!=NoArc)
        {
            pred[v] = a;
            length += Length(a);
        }
        else
        {
            length = InfFloat;
            break;
        }

        v = u;
    }
    while (v!=root);

    CT.methSolve = swapSolve;

    M.SetUpperBound(length);

    if (length<InfFloat)
    {
        if (CT.logRes)
        {
            sprintf(CT.logBuffer,"Tour has Length %g",length);
            M.Shutdown(LOG_RES,CT.logBuffer);
        }
    }
    else
    {
        M.Shutdown(LOG_RES,"Tour does not map to the original graph");
    }

    return length;
}


TFloat abstractMixedGraph::TSP_HeuristicRandom() throw(ERRejected)
{
    moduleGuard M(ModRandomTour,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1);

    #endif

    TArc* pred = InitPredecessors();
    TFloat sum = 0;

    TNode x = 1+(TNode)CT.Rand(n-1);
    pred[x] = Adjacency(0,x);

    if (pred[x]==NoArc)
    {
        #if defined(_LOGGING_)

        Error(MSG_WARN,"TSP_HeuristicRandom","Missing arc");

        #endif

        return InfFloat;
    }

    sum += Length(pred[x]);

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Constructed tour: 0,%lu",static_cast<unsigned long>(x));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    for (TNode i=2;i<n;i++)
    {
        TNode y = x;
        while (pred[y]!=NoArc) y = 1+(TNode)CT.Rand(n-1);
        pred[y] = Adjacency(x,y);

        if (pred[y]==NoArc)
        {
            #if defined(_LOGGING_)

            Error(MSG_WARN,"TSP_HeuristicRandom","Missing arc");

            #endif

            return InfFloat;
        }

        sum += Length(pred[y]);
        x = y;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(x));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    LogEnd(LH);

    #endif

    pred[0] = Adjacency(x,0);
    sum += Length(pred[0]);
    M.SetUpperBound(sum);

    M.Trace(1);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown();
    }

    sum = TSP_LocalSearch(pred);

    return sum;
}


TFloat abstractMixedGraph::TSP_HeuristicInsert(THeurTSP method,TNode r) throw(ERRange,ERRejected)
{
    moduleGuard M(ModFarthestInsert,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    if (r==NoNode)
    {
        #if defined(_PROGRESS_)

        M.InitProgressCounter(n*n);

        #endif

        TFloat bestBound = InfFloat;
        TArc* bestTour = new TArc[n];
        TArc* pred = InitPredecessors();

        for (TNode v=0;v<n && CT.SolverRunning();v++)
        {
            TFloat thisBound = TSP_HeuristicInsert(method,v);

            if (thisBound<bestBound)
            {
                bestBound = thisBound;
                for (TNode v=0;v<n;v++) bestTour[v] = pred[v];
            }
        }

        if (bestBound!=InfFloat)
        {
            for (TNode v=0;v<n;v++) pred[v] = bestTour[v];
        }
        else
        {
            for (TNode v=0;v<n;v++) pred[v] = NoArc;
        }

        return bestBound;
    }

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("TSP_HeuristicInsert",r);

    #endif

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Root node: %lu",static_cast<unsigned long>(r));
        LogEntry(LOG_METH2,CT.logBuffer);
        LH = LogStart(LOG_METH2,"Inserted nodes: ");
    }

    #endif

    TArc* pred = InitPredecessors();

    TNode u = r;
    TArc insArc1 = NoArc;
    TArc insArc2 = NoArc;

    TNode optNode = NoNode;
    TFloat optDist = -InfFloat;

    for (TNode v=0;v<n;v++)
    {
        if (v!=r)
        {
            TArc a1 = Adjacency(r,v);
            TArc a2 = Adjacency(v,r);

            if (a1!=NoArc && a2!=NoArc && Length(a1)+Length(a2)>optDist)
            {
                optDist = Length(a1)+Length(a2);
                optNode = v;
                insArc1 = a1;
                insArc2 = a2;
            }
        }
    }

    TFloat sum = 0;

    for (TNode i=0;i<n-1;i++)
    {
        if (optNode==NoNode)
        {
            #if defined(_LOGGING_)

            Error(MSG_WARN,"TSP_HeuristicInsert","Graph is disconnected");

            #endif

            return InfFloat;
        }

        pred[EndNode(insArc1)] = insArc1;
        pred[EndNode(insArc2)] = insArc2;
        sum += optDist;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu,",static_cast<unsigned long>(optNode));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        M.Trace(1);

        optDist = -InfFloat;
        optNode = NoNode;

        for (TNode v=0;v<n;v++)
        {
            if (pred[v]!=NoArc) continue;

            TNode w = r;
            TFloat minDelta = InfFloat;
            TArc minArc1 = NoArc;
            TArc minArc2 = NoArc;

            // Find the best (minimum length) place to insert w into the current subtour
            do
            {
                u = StartNode(pred[w]);
                TArc a1 = NoArc;
                TArc a2 = NoArc;
                TFloat delta = InfFloat;
                a1 = Adjacency(u,v);
                a2 = Adjacency(v,w);

                if (a1!=NoArc && a2!=NoArc)
                {
                    delta = Length(a1)+Length(a2)-Length(pred[w]);

                    if (delta<minDelta)
                    {
                        minArc1 = a1;
                        minArc2 = a2;
                        minDelta = delta;
                    }
                }

                w = u;
            }
            while (w!=r);

            if (minDelta==InfFloat) continue;

            if (  (method==TSP_HEUR_NEAREST && minDelta<optDist)
                || minDelta>optDist
               )
            {
                // Memorize w and the best place to insert it

                optNode = v;
                optDist = minDelta;
                insArc1 = minArc1;
                insArc2 = minArc2;
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    M.SetUpperBound(sum);
    M.Trace(1);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown();
    }

    if (CT.methLocal==LOCAL_OPTIMIZE) sum = TSP_LocalSearch(pred);

    return sum;
}


TFloat abstractMixedGraph::TSP_HeuristicTree(TNode r) throw(ERRange,ERRejected)
{
    moduleGuard M(ModTreeApproxTSP,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    if (r==NoNode)
    {
        #if defined(_PROGRESS_)

        M.InitProgressCounter(2*n*n);

        #endif

        TFloat bestBound = InfFloat;
        TArc* bestTour = new TArc[n];
        TArc* pred = InitPredecessors();

        for (TNode r=0;r<n && CT.SolverRunning();r++)
        {
            TFloat thisBound = TSP_HeuristicTree(r);

            if (thisBound<bestBound)
            {
                bestBound = thisBound;
                for (TNode v=0;v<n;v++) bestTour[v] = pred[v];
            }
        }

        if (bestBound!=InfFloat)
        {
            for (TNode v=0;v<n;v++) pred[v] = bestTour[v];
        }
        else
        {
            for (TNode v=0;v<n;v++) pred[v] = NoArc;
        }

        delete[] bestTour;

        return bestBound;
    }

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("TSP_HeuristicTree",r);

    #endif

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2*n,n);

    #endif

    if (!ExtractTree(r,abstractMixedGraph::MST_ONE_CYCLE))
    {
        try
        {
            MinTree(MST_EDMONDS,MST_ONE_CYCLE_REDUCED,r);
        }
        catch (ERCheck)
        {
            return InfFloat;
        }
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    const char leave = 1;
    const char reached = 2;

    TArc* pred = GetPredecessors();
    char* state = new char[n];

    for (TNode v=0;v<n;v++) state[v] = leave;

    for (TNode v=0;v<n;v++)
    {
        if (v==r) continue;

        if (pred[v]!=NoArc)
        {
            state[StartNode(pred[v])] = 0;
        }
        else
        {
            delete[] state;

            #if defined(_LOGGING_)

            Error(MSG_WARN,"TSP_HeuristicTree","Graph is disconnected");

            #endif

            return InfFloat;
        }
    }

    TNode prevLeave = NoNode;
    TFloat sum = 0;

    try
    {
        TFloat* potential = GetPotentials();

        for (TNode v=0;v<n;v++)
        {
            if (state[v]&leave)
            {
                TNode w = v;
                TArc a = pred[v];
                TNode u = StartNode(a);

                while (!(state[u]&reached) && u!=r)
                {
                    sum += RedLength(potential,a);
                    state[u] = state[u] | reached;
                    w = u;
                    a = pred[w];
                    u = StartNode(a);
                }

                if (prevLeave!=NoNode)
                {
                    a = Adjacency(prevLeave,w);
                    if (a==NoArc) throw ERRejected();
                    else pred[w] = a;
                }

                sum += RedLength(potential,a);
                prevLeave = v;

                M.Trace();
            }

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif
        }

        TArc a = Adjacency(prevLeave,r);
        if (a==NoArc) throw ERRejected();
        else pred[r] = a;
        sum += RedLength(potential,a);
    }
    catch (ERRejected)
    {
        delete[] state;

        #if defined(_LOGGING_)

        Error(MSG_WARN,"TSP_HeuristicTree","Missing arc(s)");

        #endif

        return InfFloat;
    }

    delete[] state;

    M.SetUpperBound(sum);
    M.Trace();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown();
    }

    if (CT.methLocal==LOCAL_OPTIMIZE) sum = TSP_LocalSearch(pred);

    return sum;
}


TFloat abstractMixedGraph::TSP_LocalSearch(TArc* pred) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"TSP_LocalSearch","Missing tour");

    #endif

    moduleGuard M(ModTSP,*this,"Searching for local optimality...",
        moduleGuard::SYNC_BOUNDS | moduleGuard::NO_INDENT);

    if (IsUndirected())
    {
        abstractGraph* G = static_cast<abstractGraph*>(this);

        while (CT.SolverRunning() && G->TSP_2Exchange(pred,-MaxLength())) {};

        while (CT.SolverRunning())
        {
            if (G->TSP_2Exchange(pred,0)) continue;
            if (!TSP_NodeExchange(pred,0)) break;
        }
    }
    else
    {
        while (CT.SolverRunning() && TSP_NodeExchange(pred,-MaxLength())) {};
        while (CT.SolverRunning() && TSP_NodeExchange(pred,0)) {};
    }

    TArc a = pred[0];
    TFloat l = Length(a);
    TNode x = StartNode(a);
    TNode count = 1;

    while (x!=0)
    {
        a = pred[x];
        l += Length(a);
        x = StartNode(a);
        count++;
    }

    M.SetUpperBound(l);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",l);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    if (count<n)
    {
        InternalError("TSP_LocalSearch","Tour is incomplete");
    }

    return l;
}


bool abstractMixedGraph::TSP_NodeExchange(TArc* pred,TFloat limit) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"TSP_NodeExchange","Missing tour");

    #endif

    moduleGuard M(ModNodeInsert,*this,moduleGuard::SYNC_BOUNDS);

    TNode r = CT.Rand(n);
    TNode v1 = r;
    TArc c1 = NoArc;

    while (v1!=r || c1==NoArc)
    {
        c1 = pred[v1];
        TNode x = StartNode(c1);
        TArc b1 = pred[x];
        TNode u1 = StartNode(b1);
        TArc a1 = Adjacency(u1,v1);

        TNode v2 = u1;

        while (v2!=v1 && a1!=NoArc)
        {
            TArc a2 = pred[v2];
            TNode u2 = StartNode(a2);
            TArc b2 = Adjacency(u2,x);
            TArc c2 = Adjacency(x,v2);

            TFloat diff = InfFloat;
            if (b2!=NoArc && c2!=NoArc)
                diff = Length(a1)+Length(b2)+Length(c2)
                     - Length(a2)-Length(b1)-Length(c1);

            if (diff<limit)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Local improvement (%g units, node insertion)",-diff);
                    LogEntry(LOG_METH2,CT.logBuffer);
                    sprintf(CT.logBuffer,
                        "New tour: ... %lu %lu ... %lu %lu %lu ...",
                        static_cast<unsigned long>(u1),
                        static_cast<unsigned long>(v1),
                        static_cast<unsigned long>(u2),
                        static_cast<unsigned long>(x),
                        static_cast<unsigned long>(v2));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                pred[x] = b2;
                pred[v2] = c2;
                pred[v1] = a1;

                M.Trace();

                return true;
            }

            v2 = u2;
        }

        v1 = x;
    }

    return false;
}
