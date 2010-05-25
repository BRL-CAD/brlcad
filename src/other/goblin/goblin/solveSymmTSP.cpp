
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveSymmTSP.cpp
/// \brief  ATSP branch & bound and heuristic codes

#include "branchSymmTSP.h"
#include "sparseGraph.h"
#include "denseGraph.h"
#include "binaryHeap.h"
#include "dynamicQueue.h"
#include "moduleGuard.h"


branchSymmTSP::branchSymmTSP(abstractGraph& _G,TNode _root,
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
        X = new sparseGraph(G,OPT_CLONE);
    }

    H = X->Investigate();
    selected = 0;
    depth = TArc(ceil(X->N()*log(double(X->N())*0.1)));

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,G.Pi(v));

    for (TArc a=0;a<n;a++)
        if (X->StartNode(2*a)==X->EndNode(2*a)) Lower(a);

    for (TNode v=0;v<G.N();v++) CheckNode(v);

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


branchSymmTSP::branchSymmTSP(branchSymmTSP& Node) throw() :
    branchNode<TArc,TFloat>(Node.G.M(),Node.Context(),Node.scheme),
    G(Node.G),root(Node.root),relaxationMethod(Node.relaxationMethod)
{
    X = new sparseGraph(*Node.X,OPT_CLONE);
    H = X->Investigate();
    unfixed = Node.Unfixed();
    selected = Node.selected;
    depth = TArc(X->N()*log(double(X->N())*0.1));

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,Node.X->Pi(v));

    for (TArc a=0;a<X->M();a++) X->SetSub(2*a,Node.X -> Sub(2*a));

    objective = Node.Objective();
    solved = true;

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


branchSymmTSP::~branchSymmTSP() throw()
{
    X -> Close(H);
    delete X;

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


void branchSymmTSP::SetCandidateGraph(int nCandidates) throw()
{
    LogEntry(LOG_METH,"Constructing candidate graph...");

    CT.SuppressLogging();
    sparseGraph* Y = new sparseGraph(G,OPT_CLONE);
    Y->Representation() -> SetCUCap(1);
    Y -> InitSubgraph();
    CT.RestoreLogging();

    for (TNode v=0;v<G.N();v++)
    {
        if (G.Pred(v)!=NoArc)
        {
            TNode u = G.StartNode(G.Pred(v));
            TNode w = G.EndNode(G.Pred(v));
            Y -> SetSub(Y->Adjacency(u,w),1);
        }
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
                G.InitPredecessors();

                for (TNode v=0;v<G.N();v++)
                {
                    TArc a = Y->Pred(v);
                    TNode u = Y->StartNode(a);
                    TArc a2 = G.Adjacency(u,v);
                    G.SetPred(v,a2);
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

            if (Y->Sub(a)==0 && (i<nCandidates || G.Sub(a)>0))
            {
                Y->SetSub(a,1);
                i++;
            }
        }
    }

    Y->Close(H);

    X = new sparseGraph(*Y,OPT_SUB);
    X->Representation() -> SetCUCap(1);
    n = unfixed = X->M();

    if (CT.traceLevel==2) X -> Display();

    CT.SuppressLogging();
    delete Y;
    CT.RestoreLogging();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Candidate subgraph has %lu arcs",static_cast<unsigned long>(X->M()));
        LogEntry(LOG_RES,CT.logBuffer);
    }
}


unsigned long branchSymmTSP::Size() const throw()
{
    return
          sizeof(branchSymmTSP)
        + managedObject::Allocated()
        + branchNode<TArc,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchSymmTSP::Allocated() const throw()
{
    return 0;
}


TArc branchSymmTSP::SelectVariable() throw()
{
    TArc ret0 = NoArc;
    TFloat maxDiff = -InfFloat;

    for (TNode v=0;v<X->N();v++)
    {
        TArc a0 = NoArc;
        TArc a1 = NoArc;
        TNode thisDegree = 0;
        TNode fixedIncidences = 0;

        X -> Reset(H,v);

        while (X->Active(H,v) && fixedIncidences<2)
        {
            TArc a = X->Read(H,v);

            if (X->Sub(a)==1)
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

                    if (a1==NoArc || X->Length(a)<X->Length(a1)) a1 = a;
                }
                else fixedIncidences++;
            }
        }

        if (thisDegree<=2) continue;

        // Now, we have fixedIncidences<2 and a1!=NoArc

        // By using a big 'M', let fixed incidences dominate among the
        // selection criteria.

        if ( X->Length(a1)-X->Length(a0)+100000*fixedIncidences>maxDiff )
        {
            ret0 = a0;
            maxDiff = X->Length(a1)-X->Length(a0)+100000*fixedIncidences;
        }
    }

    if (ret0!=NoArc) return ret0>>1;

    #if defined(_FAILSAVE_)

    InternalError("SelectVariable","No branching variable found");

    #endif

    throw ERInternal();
}


branchNode<TArc,TFloat>::TBranchDir branchSymmTSP::DirectionConstructive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionConstructive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat>::TBranchDir branchSymmTSP::DirectionExhaustive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionExhaustive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat> *branchSymmTSP::Clone() throw()
{
    return new branchSymmTSP(*this);
}


void branchSymmTSP::Raise(TArc i) throw(ERRange)
{
    Raise(i,true);
}


void branchSymmTSP::Raise(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Raise",a);

    #endif

    if (X->Sub(2*a)==0)
    {
        X -> SetSub(2*a,1);
        if (objective!=InfFloat) solved = 0;
    }

    X->Representation() -> SetLCap(2*a,1);
    unfixed--;
    selected++;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = 0;
}


void branchSymmTSP::Lower(TArc i) throw(ERRange)
{
    Lower(i,true);
}


void branchSymmTSP::Lower(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Lower",a);

    #endif

    if (X->Sub(2*a)==1)
    {
        X -> SetSub(2*a,0);
        if (objective!=InfFloat) solved = 0;
    }

    X->Representation() -> SetUCap(2*a,0);
    unfixed--;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = 0;
}


void branchSymmTSP::CheckNode(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=X->N()) NoSuchNode("CheckNode",v);

    #endif

    char fixedIncidences = 0;
    X -> Reset(H,v);

    while (X->Active(H,v) && fixedIncidences<=2)
    {
        TArc a2 = X->Read(H,v);

        if (X->LCap(a2)==1) fixedIncidences++;
    }

    if (fixedIncidences>2)
    {
        // Fixed arcs cannot be extended to a Hamiltonian cycle
        solved = 1;
        objective = InfFloat;
    }

    if (fixedIncidences==2)
    {
        X -> Reset(H,v);

        while (X->Active(H,v))
        {
            TArc a2 = X->Read(H,v);

            if (X->LCap(a2)==0 && X->UCap(a2)==1)
            {
                Lower(a2>>1,false);
                CheckNode(X->EndNode(a2));
            }
        }

        return;
    }

    TArc a1 = NoArc;
    TArc a2 = NoArc;
    char incidences = 0;
    X -> Reset(H,v);

    while (incidences<3 && X->Active(H,v))
    {
        TArc a = X->Read(H,v);

        if (X->UCap(a)==1)
        {
            incidences++;

            if (X->LCap(a)==0)
            {
                if (a1==NoArc) a1 = a;
                else if (a2==NoArc) a2 = a;
            }
        }
    }

    if (incidences<=2)
    {
        if (a1!=NoArc)
        {
            Raise(a1>>1,false);
            CheckNode(X->EndNode(a1));
        }

        if (a2!=NoArc && X->UCap(a2)==1 && X->LCap(a2)==0)
        {
            Raise(a2>>1,false);
            CheckNode(X->EndNode(a2));
        }
    }
}


TFloat branchSymmTSP::SolveRelaxation() throw()
{
    for (TNode v=0;v<X->N();v++)
    {
        char fixedIncidences = 0;
        X -> Reset(H,v);
        while (X->Active(H,v) && fixedIncidences<3)
        {
            TArc a2 = X->Read(H,v);
            if (X->LCap(a2)==1) fixedIncidences++;
        }

        if (fixedIncidences>2) return InfFloat;
    }

    CT.SuppressLogging();

    if (X->CutNodes()!=X->DFS_BICONNECTED)
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    TFloat objective = InfFloat;
    try
    {
        objective = X->MinTree(X->MST_DEFAULT,X->MST_ONE_CYCLE_REDUCED,root);

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
            X -> MinTree(X->MST_DEFAULT,X->MST_ONE_CYCLE_REDUCED,root);
        }
    }
    catch (ERRejected)
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    CT.RestoreLogging();

    if (unfixed==0 && !Feasible()) return InfFloat;

    return objective;
}


branchNode<TNode,TFloat>::TObjectSense branchSymmTSP::ObjectSense() const throw()
{
    return MINIMIZE;
}


TFloat branchSymmTSP::Infeasibility() const throw()
{
    return InfFloat;
}


bool branchSymmTSP::Feasible() throw()
{
    CT.SuppressLogging();

    bool feasible = (X->ExtractCycles()!=NoNode);

    if (!feasible) X -> ReleaseDegrees();

    CT.RestoreLogging();

    return feasible;
}


TFloat branchSymmTSP::LocalSearch() throw()
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


void branchSymmTSP::SaveSolution() throw()
{
}


TFloat abstractGraph::TSP_BranchAndBound(TRelaxTSP method,int nCandidates,
    TNode root,TFloat upperBound) throw()
{
    moduleGuard M(ModTSP,*this,"TSP branch and bound...",
        moduleGuard::SYNC_BOUNDS);

    branchSymmTSP *masterNode = new branchSymmTSP(*this,root,method,nCandidates);

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

    branchScheme<TArc,TFloat>* scheme =
        new branchScheme<TArc,TFloat>(masterNode,upperBound,ModTSP,level);
    TFloat ret = scheme->savedObjective;

    if (ret!=InfFloat)
    {
        M.SetUpperBound(ret);

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,
                "...Optimal tour has Length %g",ret);
            M.Shutdown(LOG_RES,CT.logBuffer);
        }

        delete scheme;
        return ret;
    }

    M.Shutdown(LOG_RES,"...Problem is infeasible");

    delete scheme;
    return InfFloat;
}


TFloat abstractGraph::TSP_SubOpt1Tree(TRelaxTSP method,TNode root,TFloat& bestUpper,bool branchAndBound)
    throw(ERRange)
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
        TFloat sum = 0;

        for (TNode v=0;v<n;v++) sum += potential[v];

        for (TNode v=0;v<n;v++)
        {
            potential[v] -= sum/n;
            bestPi[v] = potential[v];

            if (fabs(potential[v])>maxPi) maxPi = fabs(potential[v]);
        }

        LogEntry(LOG_METH,"Using existing potentials..");
    }


    TFloat* oldDeg = new TFloat[n];

    TFloat bestLower = -InfFloat;
    TFloat lastBound = -InfFloat;
    TFloat gap = InfFloat;
    TNode dev = n;
    TNode bestDev = n;
    unsigned int step = 0;

    sprintf(CT.logBuffer,"Computing minimum %lu-tree...",static_cast<unsigned long>(root));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();
    TFloat thisBound = MinTree(MST_DEFAULT,MST_ONE_CYCLE_REDUCED,root);
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

    TFloat t0 = fabs(thisBound/n);
    if (maxPi>0) t0 = 0.3*t0+0.7*maxPi/10;
    TFloat t = t0;
    const TFloat tMin = 0.02;

    TFloat *oldPi = new TFloat[n];

    while (bestUpper+CT.epsilon>=ceil(bestLower-CT.epsilon)+1
           && t>tMin && dev>0 && step<5000
           && CT.SolverRunning())
    {
        step++;

        #if defined(_PROGRESS_)

        M.SetProgressCounter((t>=t0) ? 0 : (log(t0/tMin)-log(t/tMin)) / log(t0/tMin) );

        #endif

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Step number %d",step);
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
            thisBound = MinTree(MST_DEFAULT,MST_ONE_CYCLE_REDUCED,root);
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
            if (Deg(v)!=2) dev++;

            TFloat oldDiff = potential[v]-oldPi[v];
            oldPi[v] = potential[v];

            if (step==1)
            {
                potential[v] = potential[v]+t*(Deg(v)-2);
                oldDiff = 0;
            }
            else
            {
                potential[v] = potential[v]+t*(0.7*Deg(v)+0.3*oldDeg[v]-2);
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

        if (thisBound>=InfFloat)
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
            sprintf(CT.logBuffer,"Transforming to Hamiltonian cycle...");
            LogEntry(LOG_METH,CT.logBuffer);

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

        InitDegrees();
        TFloat *swapDeg = oldDeg;
        oldDeg = sDeg;
        sDeg = swapDeg;

        if (method==TSP_RELAX_FAST)
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
                t *= 1.05;
            }
        }
        else
        {
            if (turn<=-0.2)
            {
                t *= 0.96;
            }
            else if (thisBound<=lastBound)
            {
                t *= 0.99;
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

    delete[] sDeg;
    sDeg = oldDeg;

    for (TNode v=0;v<n;v++) pred[v] = bestTour[v];
    delete[] bestTour;

    if (CT.logMeth==1)
    {
        sprintf(CT.logBuffer,"Total number of iterations: %d",step);
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


TFloat denseGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
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

        ret = abstractGraph::TSP_Heuristic(method,root);
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
            case TSP_HEUR_TREE:
            {
                ret = TSP_HeuristicTree(root);
                break;
            }
            case TSP_HEUR_CHRISTOFIDES:
            {
                ret = TSP_HeuristicChristofides(root);
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


TFloat abstractGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (CLCap() && MaxLCap()>0)
        Error(ERR_REJECTED,"TSP_Heuristic","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModTSP,*this,"Transforming to dense graph...",
        moduleGuard::SYNC_BOUNDS);

    denseGraph G(n,0,CT);
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
        TNode v = NoNode;
        for (v=0;v<n;v++)
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

    while (true)
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

        if (v==root) break;
    }

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


TFloat abstractGraph::TSP_HeuristicChristofides(TNode r) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (r>=n && r!=NoNode) NoSuchNode("TSP_HeuristicChristofides",r);

    #endif

    moduleGuard M(ModChristofides,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2+n,1);

    #endif

    if (r==NoNode) r = TNode(CT.Rand(n));
    MinTree(r);

    TNode *map = new TNode[n];
    TNode no = 0;

    for (TNode v=0;v<n;v++)
    {
        if (((TArc)Deg(v))%2) map[v] = no++;
        else map[v] = NoNode;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"Spanning tree has %lu odd vertices",static_cast<unsigned long>(no));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    LogEntry(LOG_METH,"Constructing matching problem...");
    OpenFold();

    denseGraph *G = new denseGraph(no,0,CT);
    G -> ImportLayoutData(*this);
    graphRepresentation* GR = G->Representation();
    TNode *revmap = new TNode[no];

    for (TNode u=0;u<n;u++)
    {
        if (map[u]!=NoNode)
        {
            revmap[map[u]] = u;

            if (Dim()>=2)
            {
                GR -> SetC(map[u],0,C(u,0));
                GR -> SetC(map[u],1,C(u,1));
            }

            for (TNode v=0;v<u;v++)
            {
                if (map[v]!=NoNode)
                {
                    TArc a = Adjacency(u,v);
                    TArc a0 = G->Adjacency(map[u],map[v]);
                    GR -> SetLength(a0,(u==v) ? InfFloat : Length(a));
                }
            }
        }
    }

    CloseFold();

    M.Trace(1);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    G -> MinCMatching(1);

    M.Trace(n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif

    LogEntry(LOG_METH,"Constructing Eulerian cycle...");
    OpenFold();

    dynamicQueue<TArc> Q(2*m,CT);

    TNode root = r;
    TNode u = root;

    THandle H = Investigate();
    THandle Ho = G->Investigate();

    while (Q.Cardinality()<TArc(n)-1+no/2)
    {
        if (Active(H,u))
        {
            // Add tree arc

            TArc a = Read(H,u);
            if (Sub(a)>0)
            {
                SetSub(a,0);
                Q.Insert(a);
                u = EndNode(a);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Adding arc %lu with end node %lu",
                        static_cast<unsigned long>(a),static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }
        else
        {
            TNode uo = map[u];
            if (uo!=NoNode && G->Active(Ho,uo))
            {
                // Arc matching arc

                TNode ao = G->Read(Ho,uo);
                if (G->Sub(ao)>0)
                {
                    G -> SetSub(ao,0);
                    TNode v = revmap[EndNode(ao)];
                    TArc a = Adjacency(u,v);
                    Q.Insert(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Adding arc %lu with end node %lu",
                            static_cast<unsigned long>(a),static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    u = v;
                }
            }
            else
            {
                while (!Active(H,u) && (map[u]==NoNode || !G->Active(Ho,map[u])))
                {
                    TArc a = Q.Delete();
                    Q.Insert(a);
                    u = EndNode(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Shifting arc %lu with end node %lu",
                            static_cast<unsigned long>(a),static_cast<unsigned long>(u));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }

                root = u;
            }
        }
    }

    G -> Close(Ho);
    Close(H);

    CloseFold();
    LogEntry(LOG_METH,"Extracting tour...");
    OpenFold();

    TArc* pred = InitPredecessors();
    TNode last = root;
    TFloat sum = 0;

    while (!Q.Empty())
    {
        TArc a = Q.Delete();
        TNode v = EndNode(a);

        if (v!=root && pred[v]==NoArc)
        {
            a = Adjacency(last,v);
            pred[v] = a;
            last = v;
            sum += Length(a);
        }
    }

    TArc a = Adjacency(last,root);
    pred[root] = a;
    sum += Length(a);

    CloseFold();

    M.SetUpperBound(sum);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",sum);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    M.Trace(1);

    delete G;
    delete[] map;
    delete[] revmap;

    if (CT.methLocal==LOCAL_OPTIMIZE) sum = TSP_LocalSearch(pred);

    return sum;
}


bool abstractGraph::TSP_2Exchange(TArc* pred,TFloat limit) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"TSP_2Exchange","Missing tour");

    #endif

    moduleGuard M(Mod2OptTSP,*this,moduleGuard::SYNC_BOUNDS);

    TNode r = CT.Rand(n);
    TNode v1 = r;
    TArc a1 = pred[r];
    TNode u1 = StartNode(a1);

    while (u1!=r)
    {
        TNode v2 = StartNode(pred[u1]);
        TArc a2 = pred[v2];
        TNode u2 = StartNode(a2);

        while (u2!=r && u2!=v1)
        {
            TArc a1prime = Adjacency(u1,u2);
            TArc a2prime = Adjacency(v1,v2);

            TFloat diff = InfFloat;
            if (a1prime!=NoArc && a2prime!=NoArc)
                diff = Length(a1prime)+Length(a2prime)
                     - Length(a1)-Length(a2);

            if (diff<limit)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Local improvement (%g units, 2-exchange)",-diff);
                    LogEntry(LOG_METH2,CT.logBuffer);
                    sprintf(CT.logBuffer,
                        "New tour: ... %lu %lu ... %lu %lu ...",
                        static_cast<unsigned long>(u1),
                        static_cast<unsigned long>(u2),
                        static_cast<unsigned long>(v1),
                        static_cast<unsigned long>(v2));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                TNode x = u2;
                TArc a = pred[x];

                // Flip arc directions

                while (x!=v1)
                {
                    TNode y = StartNode(a);
                    TArc an = pred[y];
                    pred[y] = (a^1);
                    x = y;
                    a = an;
                }

                pred[v2] = a2prime;
                pred[u2] = a1prime;

                M.Trace();

                return true;
            }

            v2 = u2;
            a2 = pred[u2];
            u2 = StartNode(a2);
        }

        v1 = u1;
        a1 = pred[u1];
        u1 = StartNode(a1);
    }

    return false;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveSymmTSP.cpp
/// \brief  ATSP branch & bound and heuristic codes

#include "branchSymmTSP.h"
#include "sparseGraph.h"
#include "denseGraph.h"
#include "binaryHeap.h"
#include "dynamicQueue.h"
#include "moduleGuard.h"


branchSymmTSP::branchSymmTSP(abstractGraph& _G,TNode _root,
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
        X = new sparseGraph(G,OPT_CLONE);
    }

    H = X->Investigate();
    selected = 0;
    depth = TArc(ceil(X->N()*log(double(X->N())*0.1)));

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,G.Pi(v));

    for (TArc a=0;a<n;a++)
        if (X->StartNode(2*a)==X->EndNode(2*a)) Lower(a);

    for (TNode v=0;v<G.N();v++) CheckNode(v);

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


branchSymmTSP::branchSymmTSP(branchSymmTSP& Node) throw() :
    branchNode<TArc,TFloat>(Node.G.M(),Node.Context(),Node.scheme),
    G(Node.G),root(Node.root),relaxationMethod(Node.relaxationMethod)
{
    X = new sparseGraph(*Node.X,OPT_CLONE);
    H = X->Investigate();
    unfixed = Node.Unfixed();
    selected = Node.selected;
    depth = TArc(X->N()*log(double(X->N())*0.1));

    for (TNode v=0;v<G.N();v++) X->SetPotential(v,Node.X->Pi(v));

    for (TArc a=0;a<X->M();a++) X->SetSub(2*a,Node.X -> Sub(2*a));

    objective = Node.Objective();
    solved = true;

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


branchSymmTSP::~branchSymmTSP() throw()
{
    X -> Close(H);
    delete X;

    LogEntry(LOG_MEM,"(symmetric TSP)");
}


void branchSymmTSP::SetCandidateGraph(int nCandidates) throw()
{
    LogEntry(LOG_METH,"Constructing candidate graph...");

    CT.SuppressLogging();
    sparseGraph* Y = new sparseGraph(G,OPT_CLONE);
    Y->Representation() -> SetCUCap(1);
    Y -> InitSubgraph();
    CT.RestoreLogging();

    for (TNode v=0;v<G.N();v++)
    {
        if (G.Pred(v)!=NoArc)
        {
            TNode u = G.StartNode(G.Pred(v));
            TNode w = G.EndNode(G.Pred(v));
            Y -> SetSub(Y->Adjacency(u,w),1);
        }
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
                G.InitPredecessors();

                for (TNode v=0;v<G.N();v++)
                {
                    TArc a = Y->Pred(v);
                    TNode u = Y->StartNode(a);
                    TArc a2 = G.Adjacency(u,v);
                    G.SetPred(v,a2);
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

            if (Y->Sub(a)==0 && (i<nCandidates || G.Sub(a)>0))
            {
                Y->SetSub(a,1);
                i++;
            }
        }
    }

    Y->Close(H);

    X = new sparseGraph(*Y,OPT_SUB);
    X->Representation() -> SetCUCap(1);
    n = unfixed = X->M();

    if (CT.traceLevel==2) X -> Display();

    CT.SuppressLogging();
    delete Y;
    CT.RestoreLogging();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Candidate subgraph has %lu arcs",static_cast<unsigned long>(X->M()));
        LogEntry(LOG_RES,CT.logBuffer);
    }
}


unsigned long branchSymmTSP::Size() const throw()
{
    return
          sizeof(branchSymmTSP)
        + managedObject::Allocated()
        + branchNode<TArc,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchSymmTSP::Allocated() const throw()
{
    return 0;
}


TArc branchSymmTSP::SelectVariable() throw()
{
    TArc ret0 = NoArc;
    TFloat maxDiff = -InfFloat;

    for (TNode v=0;v<X->N();v++)
    {
        TArc a0 = NoArc;
        TArc a1 = NoArc;
        TNode thisDegree = 0;
        TNode fixedIncidences = 0;

        X -> Reset(H,v);

        while (X->Active(H,v) && fixedIncidences<2)
        {
            TArc a = X->Read(H,v);

            if (X->Sub(a)==1)
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

                    if (a1==NoArc || X->Length(a)<X->Length(a1)) a1 = a;
                }
                else fixedIncidences++;
            }
        }

        if (thisDegree<=2) continue;

        // Now, we have fixedIncidences<2 and a1!=NoArc

        // By using a big 'M', let fixed incidences dominate among the
        // selection criteria.

        if ( X->Length(a1)-X->Length(a0)+100000*fixedIncidences>maxDiff )
        {
            ret0 = a0;
            maxDiff = X->Length(a1)-X->Length(a0)+100000*fixedIncidences;
        }
    }

    if (ret0!=NoArc) return ret0>>1;

    #if defined(_FAILSAVE_)

    InternalError("SelectVariable","No branching variable found");

    #endif

    throw ERInternal();
}


branchNode<TArc,TFloat>::TBranchDir branchSymmTSP::DirectionConstructive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionConstructive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat>::TBranchDir branchSymmTSP::DirectionExhaustive(TArc i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchArc("DirectionExhaustive",i);

    #endif

    return RAISE_FIRST;
}


branchNode<TArc,TFloat> *branchSymmTSP::Clone() throw()
{
    return new branchSymmTSP(*this);
}


void branchSymmTSP::Raise(TArc i) throw(ERRange)
{
    Raise(i,true);
}


void branchSymmTSP::Raise(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Raise",a);

    #endif

    if (X->Sub(2*a)==0)
    {
        X -> SetSub(2*a,1);
        if (objective!=InfFloat) solved = 0;
    }

    X->Representation() -> SetLCap(2*a,1);
    unfixed--;
    selected++;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = 0;
}


void branchSymmTSP::Lower(TArc i) throw(ERRange)
{
    Lower(i,true);
}


void branchSymmTSP::Lower(TArc a,bool recursive) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=n) NoSuchArc("Lower",a);

    #endif

    if (X->Sub(2*a)==1)
    {
        X -> SetSub(2*a,0);
        if (objective!=InfFloat) solved = 0;
    }

    X->Representation() -> SetUCap(2*a,0);
    unfixed--;

    if (recursive)
    {
        CheckNode(X->StartNode(2*a));
        CheckNode(X->EndNode(2*a));
    }

    if (unfixed==0 && objective!=InfFloat) solved = 0;
}


void branchSymmTSP::CheckNode(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=X->N()) NoSuchNode("CheckNode",v);

    #endif

    char fixedIncidences = 0;
    X -> Reset(H,v);

    while (X->Active(H,v) && fixedIncidences<=2)
    {
        TArc a2 = X->Read(H,v);

        if (X->LCap(a2)==1) fixedIncidences++;
    }

    if (fixedIncidences>2)
    {
        // Fixed arcs cannot be extended to a Hamiltonian cycle
        solved = 1;
        objective = InfFloat;
    }

    if (fixedIncidences==2)
    {
        X -> Reset(H,v);

        while (X->Active(H,v))
        {
            TArc a2 = X->Read(H,v);

            if (X->LCap(a2)==0 && X->UCap(a2)==1)
            {
                Lower(a2>>1,false);
                CheckNode(X->EndNode(a2));
            }
        }

        return;
    }

    TArc a1 = NoArc;
    TArc a2 = NoArc;
    char incidences = 0;
    X -> Reset(H,v);

    while (incidences<3 && X->Active(H,v))
    {
        TArc a = X->Read(H,v);

        if (X->UCap(a)==1)
        {
            incidences++;

            if (X->LCap(a)==0)
            {
                if (a1==NoArc) a1 = a;
                else if (a2==NoArc) a2 = a;
            }
        }
    }

    if (incidences<=2)
    {
        if (a1!=NoArc)
        {
            Raise(a1>>1,false);
            CheckNode(X->EndNode(a1));
        }

        if (a2!=NoArc && X->UCap(a2)==1 && X->LCap(a2)==0)
        {
            Raise(a2>>1,false);
            CheckNode(X->EndNode(a2));
        }
    }
}


TFloat branchSymmTSP::SolveRelaxation() throw()
{
    for (TNode v=0;v<X->N();v++)
    {
        char fixedIncidences = 0;
        X -> Reset(H,v);
        while (X->Active(H,v) && fixedIncidences<3)
        {
            TArc a2 = X->Read(H,v);
            if (X->LCap(a2)==1) fixedIncidences++;
        }

        if (fixedIncidences>2) return InfFloat;
    }

    CT.SuppressLogging();

    if (X->CutNodes()!=X->DFS_BICONNECTED)
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    TFloat objective = InfFloat;
    try
    {
        objective = X->MinTree(X->MST_DEFAULT,X->MST_ONE_CYCLE_REDUCED,root);

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
            X -> MinTree(X->MST_DEFAULT,X->MST_ONE_CYCLE_REDUCED,root);
        }
    }
    catch (ERRejected)
    {
        CT.RestoreLogging();
        return InfFloat;
    }

    CT.RestoreLogging();

    if (unfixed==0 && !Feasible()) return InfFloat;

    return objective;
}


branchNode<TNode,TFloat>::TObjectSense branchSymmTSP::ObjectSense() const throw()
{
    return MINIMIZE;
}


TFloat branchSymmTSP::Infeasibility() const throw()
{
    return InfFloat;
}


bool branchSymmTSP::Feasible() throw()
{
    CT.SuppressLogging();

    bool feasible = (X->ExtractCycles()!=NoNode);

    if (!feasible) X -> ReleaseDegrees();

    CT.RestoreLogging();

    return feasible;
}


TFloat branchSymmTSP::LocalSearch() throw()
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


void branchSymmTSP::SaveSolution() throw()
{
}


TFloat abstractGraph::TSP_BranchAndBound(TRelaxTSP method,int nCandidates,
    TNode root,TFloat upperBound) throw()
{
    moduleGuard M(ModTSP,*this,"TSP branch and bound...",
        moduleGuard::SYNC_BOUNDS);

    branchSymmTSP *masterNode = new branchSymmTSP(*this,root,method,nCandidates);

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

    branchScheme<TArc,TFloat>* scheme =
        new branchScheme<TArc,TFloat>(masterNode,upperBound,ModTSP,level);
    TFloat ret = scheme->savedObjective;

    if (ret!=InfFloat)
    {
        M.SetUpperBound(ret);

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,
                "...Optimal tour has Length %g",ret);
            M.Shutdown(LOG_RES,CT.logBuffer);
        }

        delete scheme;
        return ret;
    }

    M.Shutdown(LOG_RES,"...Problem is infeasible");

    delete scheme;
    return InfFloat;
}


TFloat abstractGraph::TSP_SubOpt1Tree(TRelaxTSP method,TNode root,TFloat& bestUpper,bool branchAndBound)
    throw(ERRange)
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
        TFloat sum = 0;

        for (TNode v=0;v<n;v++) sum += potential[v];

        for (TNode v=0;v<n;v++)
        {
            potential[v] -= sum/n;
            bestPi[v] = potential[v];

            if (fabs(potential[v])>maxPi) maxPi = fabs(potential[v]);
        }

        LogEntry(LOG_METH,"Using existing potentials..");
    }


    TFloat* oldDeg = new TFloat[n];

    TFloat bestLower = -InfFloat;
    TFloat lastBound = -InfFloat;
    TFloat gap = InfFloat;
    TNode dev = n;
    TNode bestDev = n;
    unsigned int step = 0;

    sprintf(CT.logBuffer,"Computing minimum %lu-tree...",static_cast<unsigned long>(root));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();
    TFloat thisBound = MinTree(MST_DEFAULT,MST_ONE_CYCLE_REDUCED,root);
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

    TFloat t0 = fabs(thisBound/n);
    if (maxPi>0) t0 = 0.3*t0+0.7*maxPi/10;
    TFloat t = t0;
    const TFloat tMin = 0.02;

    TFloat *oldPi = new TFloat[n];

    while (bestUpper+CT.epsilon>=ceil(bestLower-CT.epsilon)+1
           && t>tMin && dev>0 && step<5000
           && CT.SolverRunning())
    {
        step++;

        #if defined(_PROGRESS_)

        M.SetProgressCounter((t>=t0) ? 0 : (log(t0/tMin)-log(t/tMin)) / log(t0/tMin) );

        #endif

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Step number %d",step);
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
            thisBound = MinTree(MST_DEFAULT,MST_ONE_CYCLE_REDUCED,root);
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
            if (Deg(v)!=2) dev++;

            TFloat oldDiff = potential[v]-oldPi[v];
            oldPi[v] = potential[v];

            if (step==1)
            {
                potential[v] = potential[v]+t*(Deg(v)-2);
                oldDiff = 0;
            }
            else
            {
                potential[v] = potential[v]+t*(0.7*Deg(v)+0.3*oldDeg[v]-2);
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

        if (thisBound>=InfFloat)
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
            sprintf(CT.logBuffer,"Transforming to Hamiltonian cycle...");
            LogEntry(LOG_METH,CT.logBuffer);

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

        InitDegrees();
        TFloat *swapDeg = oldDeg;
        oldDeg = sDeg;
        sDeg = swapDeg;

        if (method==TSP_RELAX_FAST)
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
                t *= 1.05;
            }
        }
        else
        {
            if (turn<=-0.2)
            {
                t *= 0.96;
            }
            else if (thisBound<=lastBound)
            {
                t *= 0.99;
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

    delete[] sDeg;
    sDeg = oldDeg;

    for (TNode v=0;v<n;v++) pred[v] = bestTour[v];
    delete[] bestTour;

    if (CT.logMeth==1)
    {
        sprintf(CT.logBuffer,"Total number of iterations: %d",step);
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


TFloat denseGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
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

        ret = abstractGraph::TSP_Heuristic(method,root);
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
            case TSP_HEUR_TREE:
            {
                ret = TSP_HeuristicTree(root);
                break;
            }
            case TSP_HEUR_CHRISTOFIDES:
            {
                ret = TSP_HeuristicChristofides(root);
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


TFloat abstractGraph::TSP_Heuristic(THeurTSP method,TNode root) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (CLCap() && MaxLCap()>0)
        Error(ERR_REJECTED,"TSP_Heuristic","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModTSP,*this,"Transforming to dense graph...",
        moduleGuard::SYNC_BOUNDS);

    denseGraph G(n,0,CT);
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
        TNode v = NoNode;
        for (v=0;v<n;v++)
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

    while (true)
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

        if (v==root) break;
    }

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


TFloat abstractGraph::TSP_HeuristicChristofides(TNode r) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (r>=n && r!=NoNode) NoSuchNode("TSP_HeuristicChristofides",r);

    #endif

    moduleGuard M(ModChristofides,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2+n,1);

    #endif

    if (r==NoNode) r = TNode(CT.Rand(n));
    MinTree(r);

    TNode *map = new TNode[n];
    TNode no = 0;

    for (TNode v=0;v<n;v++)
    {
        if (((TArc)Deg(v))%2) map[v] = no++;
        else map[v] = NoNode;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"Spanning tree has %lu odd vertices",static_cast<unsigned long>(no));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    LogEntry(LOG_METH,"Constructing matching problem...");
    OpenFold();

    denseGraph *G = new denseGraph(no,0,CT);
    G -> ImportLayoutData(*this);
    graphRepresentation* GR = G->Representation();
    TNode *revmap = new TNode[no];

    for (TNode u=0;u<n;u++)
    {
        if (map[u]!=NoNode)
        {
            revmap[map[u]] = u;

            if (Dim()>=2)
            {
                GR -> SetC(map[u],0,C(u,0));
                GR -> SetC(map[u],1,C(u,1));
            }

            for (TNode v=0;v<u;v++)
            {
                if (map[v]!=NoNode)
                {
                    TArc a = Adjacency(u,v);
                    TArc a0 = G->Adjacency(map[u],map[v]);
                    GR -> SetLength(a0,(u==v) ? InfFloat : Length(a));
                }
            }
        }
    }

    CloseFold();

    M.Trace(1);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    G -> MinCMatching(1);

    M.Trace(n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif

    LogEntry(LOG_METH,"Constructing Eulerian cycle...");
    OpenFold();

    dynamicQueue<TArc> Q(2*m,CT);

    TNode root = r;
    TNode u = root;

    THandle H = Investigate();
    THandle Ho = G->Investigate();

    while (Q.Cardinality()<TArc(n)-1+no/2)
    {
        if (Active(H,u))
        {
            // Add tree arc

            TArc a = Read(H,u);
            if (Sub(a)>0)
            {
                SetSub(a,0);
                Q.Insert(a);
                u = EndNode(a);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Adding arc %lu with end node %lu",
                        static_cast<unsigned long>(a),static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }
        else
        {
            TNode uo = map[u];
            if (uo!=NoNode && G->Active(Ho,uo))
            {
                // Arc matching arc

                TNode ao = G->Read(Ho,uo);
                if (G->Sub(ao)>0)
                {
                    G -> SetSub(ao,0);
                    TNode v = revmap[EndNode(ao)];
                    TArc a = Adjacency(u,v);
                    Q.Insert(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Adding arc %lu with end node %lu",
                            static_cast<unsigned long>(a),static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    u = v;
                }
            }
            else
            {
                while (!Active(H,u) && (map[u]==NoNode || !G->Active(Ho,map[u])))
                {
                    TArc a = Q.Delete();
                    Q.Insert(a);
                    u = EndNode(a);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Shifting arc %lu with end node %lu",
                            static_cast<unsigned long>(a),static_cast<unsigned long>(u));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }

                root = u;
            }
        }
    }

    G -> Close(Ho);
    Close(H);

    CloseFold();
    LogEntry(LOG_METH,"Extracting tour...");
    OpenFold();

    TArc* pred = InitPredecessors();
    TNode last = root;
    TFloat sum = 0;

    while (!Q.Empty())
    {
        TArc a = Q.Delete();
        TNode v = EndNode(a);

        if (v!=root && pred[v]==NoArc)
        {
            a = Adjacency(last,v);
            pred[v] = a;
            last = v;
            sum += Length(a);
        }
    }

    TArc a = Adjacency(last,root);
    pred[root] = a;
    sum += Length(a);

    CloseFold();

    M.SetUpperBound(sum);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Tour has length %g",sum);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    M.Trace(1);

    delete G;
    delete[] map;
    delete[] revmap;

    if (CT.methLocal==LOCAL_OPTIMIZE) sum = TSP_LocalSearch(pred);

    return sum;
}


bool abstractGraph::TSP_2Exchange(TArc* pred,TFloat limit) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"TSP_2Exchange","Missing tour");

    #endif

    moduleGuard M(Mod2OptTSP,*this,moduleGuard::SYNC_BOUNDS);

    TNode r = CT.Rand(n);
    TNode v1 = r;
    TArc a1 = pred[r];
    TNode u1 = StartNode(a1);

    while (u1!=r)
    {
        TNode v2 = StartNode(pred[u1]);
        TArc a2 = pred[v2];
        TNode u2 = StartNode(a2);

        while (u2!=r && u2!=v1)
        {
            TArc a1prime = Adjacency(u1,u2);
            TArc a2prime = Adjacency(v1,v2);

            TFloat diff = InfFloat;
            if (a1prime!=NoArc && a2prime!=NoArc)
                diff = Length(a1prime)+Length(a2prime)
                     - Length(a1)-Length(a2);

            if (diff<limit)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Local improvement (%g units, 2-exchange)",-diff);
                    LogEntry(LOG_METH2,CT.logBuffer);
                    sprintf(CT.logBuffer,
                        "New tour: ... %lu %lu ... %lu %lu ...",
                        static_cast<unsigned long>(u1),
                        static_cast<unsigned long>(u2),
                        static_cast<unsigned long>(v1),
                        static_cast<unsigned long>(v2));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                TNode x = u2;
                TArc a = pred[x];

                // Flip arc directions

                while (x!=v1)
                {
                    TNode y = StartNode(a);
                    TArc an = pred[y];
                    pred[y] = (a^1);
                    x = y;
                    a = an;
                }

                pred[v2] = a2prime;
                pred[u2] = a1prime;

                M.Trace();

                return true;
            }

            v2 = u2;
            a2 = pred[u2];
            u2 = StartNode(a2);
        }

        v1 = u1;
        a1 = pred[u1];
        u1 = StartNode(a1);
    }

    return false;
}
