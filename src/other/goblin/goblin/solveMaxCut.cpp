
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMaxCut.cpp
/// \brief  Maximum cut branch & bound, planar case and heuristic codes

#include "branchMaxCut.h"
#include "sparseGraph.h"
#include "staticQueue.h"
#include "moduleGuard.h"


branchMaxCut::branchMaxCut(abstractMixedGraph &GC,TNode s,TNode t) throw() :
    branchNode<TNode,TFloat>(GC.N(),GC.Context()), G(GC)
{
    currentVar = 0;

    totalWeight = 0;
    selectedWeight = 0;
    dismissedWeight = 0;

    chi = new char[n];
    leftWeight = new TFloat[n];
    rightWeight = new TFloat[n];

    for (TNode v=0;v<n;v++)
    {
        chi[v] = 1;
        leftWeight[v] = rightWeight[v] = 0;
    }

    // Compute all node capacities

    bool negWeights = false;

    for (TArc a=0;a<G.M();a++)
    {
        if (G.StartNode(2*a)==G.EndNode(2*a)) continue;

        leftWeight[G.StartNode(2*a)] += G.UCap(2*a)*G.Length(2*a);
        leftWeight[G.EndNode(2*a)] += G.UCap(2*a)*G.Length(2*a);

        if (G.Length(2*a)>=0)
            totalWeight += G.UCap(2*a)*G.Length(2*a);
        else negWeights = true;
    }

    if (negWeights) totalWeight = InfFloat;

    // Determine a node with maximum capacity and reset leftWeight[]

    TNode x = 0;
    TFloat maxNodeWeight = leftWeight[0];

    for (TNode v=1;v<n;v++)
    {
        if (leftWeight[v]>maxNodeWeight)
        {
            x = v;
            maxNodeWeight = leftWeight[v];
        }

        leftWeight[v] = 0;
    }

    if (t==NoNode)
    {
        if (s!=NoNode || G.IsUndirected())
        {
            if (s!=NoNode) x = s;

            // Mark x as a left-hand side node

            chi[x] = 0;
            unfixed--;

            // Update leftWeight[] with the arcs incident with x

            TArc a = G.First(x);
            do
            {
                TNode u = G.EndNode(a);

                if (!G.Blocking(a) && u!=x)
                    leftWeight[u] += G.UCap(a)*G.Length(a&(a^1));

                a = G.Right(a,x);
            }
            while (a!=G.First(x));
        }
    }
    else
    {
        // Mark t as a right-hand side node

        chi[t] = 2;
        unfixed--;

        // Update leftWeight[] with the arcs incident with x

        TArc a = G.First(t);
        do
        {
            TNode u = G.EndNode(a);

            if (!G.Blocking(a^1) && u!=t)
                rightWeight[G.EndNode(a)] += G.UCap(a)*G.Length(a&(a^1));

            a = G.Right(a,t);
        }
        while (a!=G.First(t));

        if (s!=NoNode) Lower(s);
    }

    source = s;
    target = t;

    LogEntry(LOG_MEM,"(maximum cut)");
}


branchMaxCut::branchMaxCut(branchMaxCut &Node) throw() :
    branchNode<TNode,TFloat>(Node.G.N(),Node.Context(),Node.scheme), G(Node.G)
{
    chi = new char[n];
    leftWeight = new TFloat[n];
    rightWeight = new TFloat[n];

    currentVar = Node.currentVar;

    totalWeight = Node.totalWeight;
    selectedWeight = Node.selectedWeight;
    dismissedWeight = Node.dismissedWeight;

    for (TNode v=0;v<n;v++)
    {
        chi[v] = Node.chi[v];
        if (chi[v]!=1) unfixed--;

        leftWeight[v] = Node.leftWeight[v];
        rightWeight[v] = Node.rightWeight[v];
    }

    source = Node.source;
    target = Node.target;

    LogEntry(LOG_MEM,"(maximum cut)");
}


branchMaxCut::~branchMaxCut() throw()
{
    delete[] chi;
    delete[] leftWeight;
    delete[] rightWeight;

    LogEntry(LOG_MEM,"(maximum cut)");
}


unsigned long branchMaxCut::Size() const throw()
{
    return
          sizeof(branchMaxCut)
        + managedObject::Allocated()
        + branchNode<TNode,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchMaxCut::Allocated() const throw()
{
    return n*(sizeof(char)+2*sizeof(TFloat));
}


TFloat branchMaxCut::SumWeight(TNode v) throw()
{
    return leftWeight[v]+rightWeight[v];
}


TFloat branchMaxCut::MinWeight(TNode v) throw()
{
    if (leftWeight[v]>rightWeight[v])
        return rightWeight[v];
    else return leftWeight[v];
}


TFloat branchMaxCut::MaxWeight(TNode v) throw()
{
    if (leftWeight[v]<rightWeight[v])
        return rightWeight[v];
    else return leftWeight[v];
}


TNode branchMaxCut::SelectVariable() throw()
{
    TNode maxNode = NoNode;
    TFloat maxWeight = -InfFloat;

    for (TNode v=0;v<n;v++)
    {
        if (chi[v]!=1) continue;

        TFloat thisWeight = SumWeight(v);

        if (thisWeight>maxWeight)
        {
            maxNode = v;
            maxWeight = thisWeight;
        }
    }

    #if defined(_FAILSAVE_)

    if (maxNode==NoNode)
    {
        sprintf(CT.logBuffer,"All variables are fixed: %lu",static_cast<unsigned long>(unfixed));
        InternalError1("Raise");
    }

    #endif

    return maxNode;
}


branchNode<TNode,TFloat>::TBranchDir branchMaxCut::DirectionConstructive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionConstructive",v);

    #endif

    if (rightWeight[v]<=leftWeight[v]) return LOWER_FIRST;

    return RAISE_FIRST;
}


branchNode<TNode,TFloat>::TBranchDir branchMaxCut::DirectionExhaustive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionExhaustive",v);

    #endif

    return DirectionConstructive(v);
}


branchNode<TNode,TFloat>* branchMaxCut::Clone() throw()
{
    return new branchMaxCut(*this);
}


void branchMaxCut::Raise(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Raise",i);

    #endif

    // Mark i as a right-hand side node

    chi[i] = 2;
    unfixed--;
    solved = false;

    // Update rightWeight with the incidences of i

    TArc a = G.First(i);
    do
    {
        TNode w = G.EndNode(a);

        if (w!=i)
        {
            switch (chi[w])
            {
                case 0:
                {
                    if (!G.Blocking(a^1))
                        selectedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 1:
                {
                    if (G.Blocking(a^1))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));
                    else rightWeight[w] += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 2:
                {
                    if (!G.Blocking(a))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
            }
        }

        a = G.Right(a,i);
    }
    while (a!=G.First(i));

    if (unfixed==1)
    {
        TNode uncoloured = NoNode;
        bool leftNodes = false;
        bool rightNodes = false;

        for (TNode v=0;v<n;v++)
        {
            if (chi[v]==0) leftNodes = true;
            if (chi[v]==1) uncoloured = v;
            if (chi[v]==2) rightNodes = true;
        }

        if (!leftNodes) Lower(uncoloured);
        if (!rightNodes) Raise(uncoloured);
    }
}


void branchMaxCut::Lower(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Lower",i);

    #endif

    // Mark i as a left-hand side node

    chi[i] = 0;
    unfixed--;
    solved = false;

    // Update leftWeight with the incidences of i

    TArc a = G.First(i);
    do
    {
        TNode w = G.EndNode(a);

        if (w!=i)
        {
            switch (chi[w])
            {
                case 0:
                {
                    if (!G.Blocking(a^1))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 1:
                {
                    if (G.Blocking(a))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));
                    else leftWeight[w] += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 2:
                {
                    if (!G.Blocking(a))
                        selectedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
            }
        }

        a = G.Right(a,i);
    }
    while (a!=G.First(i));

    if (unfixed==1)
    {
        TNode uncoloured = NoNode;
        bool leftNodes = false;
        bool rightNodes = false;

        for (TNode v=0;v<n;v++)
        {
            if (chi[v]==0) leftNodes = true;
            if (chi[v]==1) uncoloured = v;
            if (chi[v]==2) rightNodes = true;
        }

        if (!leftNodes) Lower(uncoloured);
        if (!rightNodes) Raise(uncoloured);
    }
}


TFloat branchMaxCut::SolveRelaxation() throw()
{
    if (totalWeight!=InfFloat)
    {
        // All arc weights are non-negative

        return totalWeight-dismissedWeight;
    }

    TFloat thisWeight = selectedWeight;

    for (TArc a=0;a<G.M();a++)
    {
        TNode v = G.StartNode(2*a);
        TNode w = G.EndNode(2*a);

        if (chi[v]!=1 && chi[w]!=1) continue;

        if (chi[v]==2 && G.Orientation(2*a)==1) continue;

        if (chi[w]==0 && G.Orientation(2*a)==1) continue;

        if (G.Length(2*a)>0) thisWeight += G.UCap(2*a)*G.Length(2*a);
    }

    return thisWeight;
}


branchNode<TNode,TFloat>::TObjectSense branchMaxCut::ObjectSense() const throw()
{
    return MAXIMIZE;
}


TFloat branchMaxCut::Infeasibility() const throw()
{
    return -InfFloat;
}


void branchMaxCut::ReallySaveSolution() throw()
{
    TNode* nodeColour = G.InitNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if (chi[v]==0) nodeColour[v] = 0;
        if (chi[v]==1) nodeColour[v] = NoNode;
        if (chi[v]==2) nodeColour[v] = 1;
    }
}


TFloat branchMaxCut::LocalSearch() throw()
{
    TNode* nodeColour = G.InitNodeColours();

    for (TNode v=0;v<n;v++) nodeColour[v] = chi[v]/2;

    CT.SuppressLogging();
    objective = G.MXC_LocalSearch(nodeColour,source,target);
    CT.RestoreLogging();

    if (CT.traceLevel==3) G.Display();

    return objective;
}


TFloat abstractMixedGraph::MXC_BranchAndBound(TNode s,TNode t,TFloat lowerBound)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_BranchAndBound",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_BranchAndBound",t);

    #endif

    moduleGuard M(ModMaxCut,*this,"Max-Cut branch and bound...",
        moduleGuard::SYNC_BOUNDS);

    branchMaxCut* rootNode = new branchMaxCut(*this,s,t);

    branchScheme<TNode,TFloat> scheme(rootNode,lowerBound,ModMaxCut,
        branchScheme<TNode,TFloat>::SEARCH_CONSTRUCT);

    M.SetBounds(scheme.savedObjective,scheme.bestBound);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Maximum cut has weight %g",
            scheme.savedObjective);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return scheme.savedObjective;
}


TFloat abstractMixedGraph::MaxCut(TMethMaxCut method,TNode s,TNode t) throw(ERRange)
{
    if (s>=n) s = DefaultSourceNode();
    if (t>=n) t = DefaultTargetNode();

    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MaxCut",s);
    if (t>=n && t!=NoNode) NoSuchNode("MaxCut",t);

    #endif

    moduleGuard M(ModMaxCut,*this,"Computing maximum edge cut...");

    TFloat totalWeight = 0;
    bool negWeights = false;

    for (TArc a=0;a<m;a++)
        if (UCap(2*a)>0 && Length(2*a)>0)
            totalWeight += UCap(2*a)*Length(2*a);
        else if (Length(2*a)<0) negWeights = true;

    TNode* savedCut = NULL;
    TFloat savedWeight = 0;

    TNode* nodeColour = GetNodeColours();

    if (nodeColour)
    {
        if (s!=NoNode && t!=NoNode && nodeColour[s]==nodeColour[t])
            savedWeight = -InfFloat;

        for (TArc a=0;a<2*m && savedWeight!=-InfFloat;a++)
        {
            TNode u = StartNode(a);
            TNode v = EndNode(a);

            if (nodeColour[u]==0 && nodeColour[v]==1 && !Blocking(a))
            {
                savedWeight += UCap(a)*Length(a&(a^1));
            }
            else if (nodeColour[u]>1 || nodeColour[v]>1) savedWeight = -InfFloat;
        }

        if (savedWeight != -InfFloat)
        {
            if ((s!=NoNode && nodeColour[s]==1) || (t!=NoNode && nodeColour[t]==0))
            {
                for (TNode v=0;v<n;v++) nodeColour[v] = 1-nodeColour[v];
            }

            savedCut = new TNode[n];
            for (TNode v=0;v<n;v++) savedCut[v] = nodeColour[v];

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"...Initial cut has weight %g",savedWeight);
                LogEntry(LOG_RES,CT.logBuffer);
            }

            M.SetBounds(savedWeight,totalWeight);
        }
    }
    else M.SetUpperBound(totalWeight);

    #if defined(_PROGRESS_)

    if (CT.methSolve>=2)
    {
        M.InitProgressCounter(1, 0);
    }

    #endif

    TFloat ret = MXC_Heuristic(method,s,t);

    if (savedCut)
    {
        if (savedWeight>ret)
        {
            for (TNode v=0;v<n;v++) nodeColour[v] = savedCut[v];
        }

        delete[] savedCut;
    }

    if (CT.methSolve>=2)
    {
        #if defined(_PROGRESS_)

        M.SetProgressNext(1);

        #endif

        ret = MXC_BranchAndBound(s,t,ret);
    }

    return ret;
}


TFloat abstractMixedGraph::MXC_Heuristic(TMethMaxCut method,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    LogEntry(LOG_METH,"Computing heuristic maximum cut...");

    return MXC_HeuristicGRASP(s,t);
}


TFloat abstractMixedGraph::MXC_HeuristicGRASP(TNode s,TNode t)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_HeuristicGRASP",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_HeuristicGRASP",t);

    #endif

    moduleGuard M(ModMaxCutGRASP,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    goblinQueue<TNode,TFloat>* Q = nHeap;

    if (Q!=NULL) Q -> Init();
    else Q = NewNodeHeap();

    branchMaxCut thisNode(*this,s,t);

    thisNode.ReallySaveSolution();
    TNode* nodeColour = GetNodeColours();

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    while (!thisNode.Feasible())
    {
        Q -> Init();

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==NoNode) Q->Insert(v,thisNode.MinWeight(v));

        TNode u = Q->Delete();

        TNode draw = CT.Rand(2);
        if (draw >= Q->Cardinality()) draw = Q->Cardinality();

        for (;draw>0;draw--) u = Q->Delete();

        branchNode<TNode,TFloat>::TBranchDir dir =
            thisNode.DirectionConstructive(u);

        if (dir==branchNode<TNode,TFloat>::RAISE_FIRST)
        {
            thisNode.Lower(u);
            nodeColour[u] = 0;
        }
        else
        {
            thisNode.Raise(u);
            nodeColour[u] = 1;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            if (dir) sprintf(CT.logBuffer,"%lu[S] ",static_cast<unsigned long>(u));
            else sprintf(CT.logBuffer,"%lu[T] ",static_cast<unsigned long>(u));

            LogAppend(LH,CT.logBuffer);
        }

        #endif

        M.SetLowerBound(thisNode.selectedWeight);
        M.Trace(1);
    }

    thisNode.ReallySaveSolution();

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    if (nHeap==NULL) delete Q;

    TFloat weight = thisNode.selectedWeight;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Cut has weight %g",weight);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    if (CT.methLocal==LOCAL_OPTIMIZE) weight = MXC_LocalSearch(nodeColour,s,t);

    return weight;
}


TFloat abstractMixedGraph::MXC_LocalSearch(TNode* nodeColour,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_LocalSearch",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_LocalSearch",t);

    if (!nodeColour)
        Error(ERR_REJECTED,"MXC_LocalSearch","Missing cut");

    #endif

    moduleGuard M(ModMaxCut,*this,"Searching for local optimality...",
        moduleGuard::SYNC_BOUNDS);

    TNode card = 0;
    for (TNode v=0;v<n;v++)
        if (nodeColour[v]==1) card++;

    TFloat weight = 0;
    for (TArc a=0;a<2*m;a++)
    {
        TNode u = StartNode(a);
        TNode w = EndNode(a);

        if (nodeColour[u]==0 && nodeColour[w]==1 && !Blocking(a))
            weight += UCap(a)*Length(a&(a^1));
    }

    TFloat* profit = new TFloat[n];

    while (true)
    {
        for (TNode v=0;v<n;v++) profit[v] = 0;

        for (TArc a=0;a<m;a++)
        {
            TNode u = StartNode(2*a);
            TNode w = EndNode(2*a);

            if (u==w) continue;

            if (nodeColour[u]==1 && nodeColour[w]==1)
            {
                profit[u] += UCap(2*a)*Length(2*a);

                if (Orientation(2*a)==0)
                    profit[w] += UCap(2*a)*Length(2*a); 
            }

            if (nodeColour[u]==0 && nodeColour[w]==0)
            {
                profit[w] += UCap(2*a)*Length(2*a);

                if (Orientation(2*a)==0)
                    profit[u] += UCap(2*a)*Length(2*a); 
            }

            if (nodeColour[u]==1 && nodeColour[w]==0 && Orientation(2*a)==0)
            {
                profit[w] -= UCap(2*a)*Length(2*a);
                profit[u] -= UCap(2*a)*Length(2*a);
            }

            if (nodeColour[u]==0 && nodeColour[w]==1)
            {
                profit[w] -= UCap(2*a)*Length(2*a);
                profit[u] -= UCap(2*a)*Length(2*a);
            }
        }

        TNode maxNode = NoNode;
        for (TNode v=0;v<n;v++)
        {
            if (   v!=s && v!=t
                && (maxNode==NoNode || profit[v]>profit[maxNode])
                && (card>1   || nodeColour[v]==0)
                && (card<n-1 || nodeColour[v]==1)
               )
            {
                maxNode = v;
            }
        }

        if (profit[maxNode]<=0) break;

        nodeColour[maxNode] = 1-nodeColour[maxNode];
        weight += profit[maxNode];

        if (nodeColour[maxNode]==1) card++; else card--;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,
                "Node %lu moves to component %c",
                static_cast<unsigned long>(maxNode),char('S'+nodeColour[maxNode]));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        M.SetLowerBound(weight);
        M.Trace();
    }

    delete[] profit;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Cut has weight: %g",weight);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return weight;
}


TFloat abstractGraph::MXC_Heuristic(TMethMaxCut method,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    moduleGuard M(ModMaxCut,*this,
        moduleGuard::NO_INDENT | moduleGuard::SYNC_BOUNDS);

    TFloat ret = InfFloat;

    if (method==MAX_CUT_DEFAULT) method = TMethMaxCut(CT.methMaxCut);

    switch (method)
    {
        case MAX_CUT_TJOIN:
        {
            LogEntry(LOG_METH,"Applying planar max-cut method...");

            bool solved = true;
            try
            {
                ret = MXC_DualTJoin(s);
            }
            catch (ERRejected)
            {
                solved = false;
            }

            TNode* nodeColour = GetNodeColours();

            if (solved && (t==NoNode || nodeColour[t]==1))
            {
                M.SetUpperBound(ret);
                break;
            }

            if (solved && t!=NoNode && nodeColour[t]==0)
                LogEntry(LOG_RES,"...Target node has not be separated");

            // Do not break
        }
        case MAX_CUT_GRASP:
        case MAX_CUT_TREE:
        {
            LogEntry(LOG_METH,"Computing heuristic maximum cut...");

            if (method==MAX_CUT_TREE)
            {
                try
                {
                    ret = MXC_HeuristicTree(s,t);
                    break;
                }
                catch (ERRejected) {};
            }

            ret = MXC_HeuristicGRASP(s,t);

            break;
        }
        default:
        {
            UnknownOption("MXC_Heuristic",method);
        }
    }

    return ret;
}


TFloat abstractGraph::MXC_DualTJoin(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_DualTJoin",s);

    #endif

    moduleGuard M(ModMaxCutTJoin,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n+2);

    #endif

    dualGraph* D = NULL;

    try
    {
        D = new dualGraph(*this);
    }
    catch (ERRejected)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin","Missing planar representation");
    }

    graphRepresentation* DR = D->Representation();
    bool negWeights = false;

    for (TArc a=0;a<m;a++)
    {
        TFloat thisWeight = Length(2*a)*UCap(2*a);
        DR -> SetLength(2*a,-thisWeight);

        if (thisWeight<0) negWeights = true;
    }

    if (negWeights)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin",
            "Negative edge weights are not supported");
    }

    M.Trace(1);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    D -> MinCTJoin(voidIndex<TNode>(D->N(),CT));

    M.Trace(n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif
 
    LogEntry(LOG_METH,"Extracting cut from T-join...");
    OpenFold();

    TNode* nodeColour = InitNodeColours(NoNode);

    staticQueue<TNode> Q(n,CT);

    if (s!=NoNode)
    {
        Q.Insert(s);
        nodeColour[s] = 0;
    }
    else
    {
        Q.Insert(0);
        nodeColour[0] = 0;
    }

    TNode nodeCount = 1;
    TFloat weight = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!(Q.Empty()))
    {
        M.Trace(Q);

        TNode u = Q.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u))
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);

            TNode reqColour = nodeColour[u];

            if (D -> Sub(a)>0)
            {
                reqColour = 1-nodeColour[u];

                if (u<v) weight += Length(a)*UCap(a);
            }

            if (nodeColour[v]==NoNode)
            {
                nodeColour[v] = reqColour;
                M.Trace(Q);
                Q.Insert(v);
                nodeCount++;
            }
            else if (nodeColour[v]!=reqColour)
            {
                InternalError("MXC_DualTJoin","Dual Subgraph is not a T-Join");
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    CloseFold();

    delete D;

    #if defined(_FAILSAVE_)

    if (nodeCount<n)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin","Graph is disconnected");
    }

    #endif

    M.SetLowerBound(weight);
    M.Trace(1);

    sprintf(CT.logBuffer,"...Cut has weight %g",weight);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return weight;
}


TFloat abstractGraph::MXC_HeuristicTree(TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_HeuristicTree",s);
    if (t>=n && t!=NoNode) NoSuchNode("MXC_HeuristicTree",t);

    #endif

    moduleGuard M(ModMaxCut,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH,"(Tree Heuristics)");

    #if defined(_PROGRESS_)

    if (CT.methSolve<2) M.SetProgressNext(1);

    #endif

    TNode* nodeColour = InitNodeColours();

    TNode r = s;
    if (r==NoNode) r = TNode(CT.Rand(n));

    for (TNode v=0;v<n;v++) nodeColour[v] = NoNode;

    nodeColour[r] = 0;

    if (t!=NoNode) nodeColour[t] = 1;

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TArc a=0;a<m;a++)
    {
        GR -> SetLength(2*a,UCap(2*a)*Length(2*a));
    }

    G.MinTree(MST_DEFAULT,MST_MAX,r);

    staticQueue<TNode> S(n,CT);

    for (TNode v=0;v<n;v++)
        if (nodeColour[v]==NoNode) S.Insert(v);

    while (!S.Empty())
    {
        TNode v = S.Delete();
        TArc a = G.Pred(v);

        if (a!=NoArc)
        {
            TNode u = G.StartNode(a);

            if (nodeColour[u]==NoNode) S.Insert(v);
            else nodeColour[v] = 1-nodeColour[u];
        }
        else
        {
            Error(ERR_REJECTED,"MXC_HeuristicTree","Graph is disconnected");
        }
    }

    TFloat weight = 0;
    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode w = EndNode(2*a);
        if (nodeColour[u]!=nodeColour[w]) weight += UCap(2*a)*Length(2*a);
    }

    M.SetLowerBound(weight);
    M.Trace();

    sprintf(CT.logBuffer,"...Cut has weight %g",weight);
    M.Shutdown(LOG_RES,CT.logBuffer);

    if (CT.methLocal==LOCAL_OPTIMIZE) weight = MXC_LocalSearch(nodeColour,s,t);

    return weight;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMaxCut.cpp
/// \brief  Maximum cut branch & bound, planar case and heuristic codes

#include "branchMaxCut.h"
#include "sparseGraph.h"
#include "staticQueue.h"
#include "moduleGuard.h"


branchMaxCut::branchMaxCut(abstractMixedGraph &GC,TNode s,TNode t) throw() :
    branchNode<TNode,TFloat>(GC.N(),GC.Context()), G(GC)
{
    currentVar = 0;

    totalWeight = 0;
    selectedWeight = 0;
    dismissedWeight = 0;

    chi = new char[n];
    leftWeight = new TFloat[n];
    rightWeight = new TFloat[n];

    for (TNode v=0;v<n;v++)
    {
        chi[v] = 1;
        leftWeight[v] = rightWeight[v] = 0;
    }

    // Compute all node capacities

    bool negWeights = false;

    for (TArc a=0;a<G.M();a++)
    {
        if (G.StartNode(2*a)==G.EndNode(2*a)) continue;

        leftWeight[G.StartNode(2*a)] += G.UCap(2*a)*G.Length(2*a);
        leftWeight[G.EndNode(2*a)] += G.UCap(2*a)*G.Length(2*a);

        if (G.Length(2*a)>=0)
            totalWeight += G.UCap(2*a)*G.Length(2*a);
        else negWeights = true;
    }

    if (negWeights) totalWeight = InfFloat;

    // Determine a node with maximum capacity and reset leftWeight[]

    TNode x = 0;
    TFloat maxNodeWeight = leftWeight[0];

    for (TNode v=1;v<n;v++)
    {
        if (leftWeight[v]>maxNodeWeight)
        {
            x = v;
            maxNodeWeight = leftWeight[v];
        }

        leftWeight[v] = 0;
    }

    if (t==NoNode)
    {
        if (s!=NoNode || G.IsUndirected())
        {
            if (s!=NoNode) x = s;

            // Mark x as a left-hand side node

            chi[x] = 0;
            unfixed--;

            // Update leftWeight[] with the arcs incident with x

            TArc a = G.First(x);
            do
            {
                TNode u = G.EndNode(a);

                if (!G.Blocking(a) && u!=x)
                    leftWeight[u] += G.UCap(a)*G.Length(a&(a^1));

                a = G.Right(a,x);
            }
            while (a!=G.First(x));
        }
    }
    else
    {
        // Mark t as a right-hand side node

        chi[t] = 2;
        unfixed--;

        // Update leftWeight[] with the arcs incident with x

        TArc a = G.First(t);
        do
        {
            TNode u = G.EndNode(a);

            if (!G.Blocking(a^1) && u!=t)
                rightWeight[G.EndNode(a)] += G.UCap(a)*G.Length(a&(a^1));

            a = G.Right(a,t);
        }
        while (a!=G.First(t));

        if (s!=NoNode) Lower(s);
    }

    source = s;
    target = t;

    LogEntry(LOG_MEM,"(maximum cut)");
}


branchMaxCut::branchMaxCut(branchMaxCut &Node) throw() :
    branchNode<TNode,TFloat>(Node.G.N(),Node.Context(),Node.scheme), G(Node.G)
{
    chi = new char[n];
    leftWeight = new TFloat[n];
    rightWeight = new TFloat[n];

    currentVar = Node.currentVar;

    totalWeight = Node.totalWeight;
    selectedWeight = Node.selectedWeight;
    dismissedWeight = Node.dismissedWeight;

    for (TNode v=0;v<n;v++)
    {
        chi[v] = Node.chi[v];
        if (chi[v]!=1) unfixed--;

        leftWeight[v] = Node.leftWeight[v];
        rightWeight[v] = Node.rightWeight[v];
    }

    source = Node.source;
    target = Node.target;

    LogEntry(LOG_MEM,"(maximum cut)");
}


branchMaxCut::~branchMaxCut() throw()
{
    delete[] chi;
    delete[] leftWeight;
    delete[] rightWeight;

    LogEntry(LOG_MEM,"(maximum cut)");
}


unsigned long branchMaxCut::Size() const throw()
{
    return
          sizeof(branchMaxCut)
        + managedObject::Allocated()
        + branchNode<TNode,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchMaxCut::Allocated() const throw()
{
    return n*(sizeof(char)+2*sizeof(TFloat));
}


TFloat branchMaxCut::SumWeight(TNode v) throw()
{
    return leftWeight[v]+rightWeight[v];
}


TFloat branchMaxCut::MinWeight(TNode v) throw()
{
    if (leftWeight[v]>rightWeight[v])
        return rightWeight[v];
    else return leftWeight[v];
}


TFloat branchMaxCut::MaxWeight(TNode v) throw()
{
    if (leftWeight[v]<rightWeight[v])
        return rightWeight[v];
    else return leftWeight[v];
}


TNode branchMaxCut::SelectVariable() throw()
{
    TNode maxNode = NoNode;
    TFloat maxWeight = -InfFloat;

    for (TNode v=0;v<n;v++)
    {
        if (chi[v]!=1) continue;

        TFloat thisWeight = SumWeight(v);

        if (thisWeight>maxWeight)
        {
            maxNode = v;
            maxWeight = thisWeight;
        }
    }

    #if defined(_FAILSAVE_)

    if (maxNode==NoNode)
    {
        sprintf(CT.logBuffer,"All variables are fixed: %lu",static_cast<unsigned long>(unfixed));
        InternalError1("Raise");
    }

    #endif

    return maxNode;
}


branchNode<TNode,TFloat>::TBranchDir branchMaxCut::DirectionConstructive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionConstructive",v);

    #endif

    if (rightWeight[v]<=leftWeight[v]) return LOWER_FIRST;

    return RAISE_FIRST;
}


branchNode<TNode,TFloat>::TBranchDir branchMaxCut::DirectionExhaustive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionExhaustive",v);

    #endif

    return DirectionConstructive(v);
}


branchNode<TNode,TFloat>* branchMaxCut::Clone() throw()
{
    return new branchMaxCut(*this);
}


void branchMaxCut::Raise(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Raise",i);

    #endif

    // Mark i as a right-hand side node

    chi[i] = 2;
    unfixed--;
    solved = false;

    // Update rightWeight with the incidences of i

    TArc a = G.First(i);
    do
    {
        TNode w = G.EndNode(a);

        if (w!=i)
        {
            switch (chi[w])
            {
                case 0:
                {
                    if (!G.Blocking(a^1))
                        selectedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 1:
                {
                    if (G.Blocking(a^1))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));
                    else rightWeight[w] += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 2:
                {
                    if (!G.Blocking(a))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
            }
        }

        a = G.Right(a,i);
    }
    while (a!=G.First(i));

    if (unfixed==1)
    {
        TNode uncoloured = NoNode;
        bool leftNodes = false;
        bool rightNodes = false;

        for (TNode v=0;v<n;v++)
        {
            if (chi[v]==0) leftNodes = true;
            if (chi[v]==1) uncoloured = v;
            if (chi[v]==2) rightNodes = true;
        }

        if (!leftNodes) Lower(uncoloured);
        if (!rightNodes) Raise(uncoloured);
    }
}


void branchMaxCut::Lower(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Lower",i);

    #endif

    // Mark i as a left-hand side node

    chi[i] = 0;
    unfixed--;
    solved = false;

    // Update leftWeight with the incidences of i

    TArc a = G.First(i);
    do
    {
        TNode w = G.EndNode(a);

        if (w!=i)
        {
            switch (chi[w])
            {
                case 0:
                {
                    if (!G.Blocking(a^1))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 1:
                {
                    if (G.Blocking(a))
                        dismissedWeight += G.UCap(a)*G.Length(a&(a^1));
                    else leftWeight[w] += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
                case 2:
                {
                    if (!G.Blocking(a))
                        selectedWeight += G.UCap(a)*G.Length(a&(a^1));

                    break;
                }
            }
        }

        a = G.Right(a,i);
    }
    while (a!=G.First(i));

    if (unfixed==1)
    {
        TNode uncoloured = NoNode;
        bool leftNodes = false;
        bool rightNodes = false;

        for (TNode v=0;v<n;v++)
        {
            if (chi[v]==0) leftNodes = true;
            if (chi[v]==1) uncoloured = v;
            if (chi[v]==2) rightNodes = true;
        }

        if (!leftNodes) Lower(uncoloured);
        if (!rightNodes) Raise(uncoloured);
    }
}


TFloat branchMaxCut::SolveRelaxation() throw()
{
    if (totalWeight!=InfFloat)
    {
        // All arc weights are non-negative

        return totalWeight-dismissedWeight;
    }

    TFloat thisWeight = selectedWeight;

    for (TArc a=0;a<G.M();a++)
    {
        TNode v = G.StartNode(2*a);
        TNode w = G.EndNode(2*a);

        if (chi[v]!=1 && chi[w]!=1) continue;

        if (chi[v]==2 && G.Orientation(2*a)==1) continue;

        if (chi[w]==0 && G.Orientation(2*a)==1) continue;

        if (G.Length(2*a)>0) thisWeight += G.UCap(2*a)*G.Length(2*a);
    }

    return thisWeight;
}


branchNode<TNode,TFloat>::TObjectSense branchMaxCut::ObjectSense() const throw()
{
    return MAXIMIZE;
}


TFloat branchMaxCut::Infeasibility() const throw()
{
    return -InfFloat;
}


void branchMaxCut::ReallySaveSolution() throw()
{
    TNode* nodeColour = G.InitNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if (chi[v]==0) nodeColour[v] = 0;
        if (chi[v]==1) nodeColour[v] = NoNode;
        if (chi[v]==2) nodeColour[v] = 1;
    }
}


TFloat branchMaxCut::LocalSearch() throw()
{
    TNode* nodeColour = G.InitNodeColours();

    for (TNode v=0;v<n;v++) nodeColour[v] = chi[v]/2;

    CT.SuppressLogging();
    objective = G.MXC_LocalSearch(nodeColour,source,target);
    CT.RestoreLogging();

    if (CT.traceLevel==3) G.Display();

    return objective;
}


TFloat abstractMixedGraph::MXC_BranchAndBound(TNode s,TNode t,TFloat lowerBound)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_BranchAndBound",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_BranchAndBound",t);

    #endif

    moduleGuard M(ModMaxCut,*this,"Max-Cut branch and bound...",
        moduleGuard::SYNC_BOUNDS);

    branchMaxCut* rootNode = new branchMaxCut(*this,s,t);

    branchScheme<TNode,TFloat> scheme(rootNode,lowerBound,ModMaxCut,
        branchScheme<TNode,TFloat>::SEARCH_CONSTRUCT);

    M.SetBounds(scheme.savedObjective,scheme.bestBound);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Maximum cut has weight %g",
            scheme.savedObjective);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return scheme.savedObjective;
}


TFloat abstractMixedGraph::MaxCut(TMethMaxCut method,TNode s,TNode t) throw(ERRange)
{
    if (s>=n) s = DefaultSourceNode();
    if (t>=n) t = DefaultTargetNode();

    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MaxCut",s);
    if (t>=n && t!=NoNode) NoSuchNode("MaxCut",t);

    #endif

    moduleGuard M(ModMaxCut,*this,"Computing maximum edge cut...");

    TFloat totalWeight = 0;
    bool negWeights = false;

    for (TArc a=0;a<m;a++)
        if (UCap(2*a)>0 && Length(2*a)>0)
            totalWeight += UCap(2*a)*Length(2*a);
        else if (Length(2*a)<0) negWeights = true;

    TNode* savedCut = NULL;
    TFloat savedWeight = 0;

    TNode* nodeColour = GetNodeColours();

    if (nodeColour)
    {
        if (s!=NoNode && t!=NoNode && nodeColour[s]==nodeColour[t])
            savedWeight = -InfFloat;

        for (TArc a=0;a<2*m && savedWeight!=-InfFloat;a++)
        {
            TNode u = StartNode(a);
            TNode v = EndNode(a);

            if (nodeColour[u]==0 && nodeColour[v]==1 && !Blocking(a))
            {
                savedWeight += UCap(a)*Length(a&(a^1));
            }
            else if (nodeColour[u]>1 || nodeColour[v]>1) savedWeight = -InfFloat;
        }

        if (savedWeight != -InfFloat)
        {
            if ((s!=NoNode && nodeColour[s]==1) || (t!=NoNode && nodeColour[t]==0))
            {
                for (TNode v=0;v<n;v++) nodeColour[v] = 1-nodeColour[v];
            }

            savedCut = new TNode[n];
            for (TNode v=0;v<n;v++) savedCut[v] = nodeColour[v];

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"...Initial cut has weight %g",savedWeight);
                LogEntry(LOG_RES,CT.logBuffer);
            }

            M.SetBounds(savedWeight,totalWeight);
        }
    }
    else M.SetUpperBound(totalWeight);

    #if defined(_PROGRESS_)

    if (CT.methSolve>=2)
    {
        M.InitProgressCounter(1, 0);
    }

    #endif

    TFloat ret = MXC_Heuristic(method,s,t);

    if (savedCut)
    {
        if (savedWeight>ret)
        {
            for (TNode v=0;v<n;v++) nodeColour[v] = savedCut[v];
        }

        delete[] savedCut;
    }

    if (CT.methSolve>=2)
    {
        #if defined(_PROGRESS_)

        M.SetProgressNext(1);

        #endif

        ret = MXC_BranchAndBound(s,t,ret);
    }

    return ret;
}


TFloat abstractMixedGraph::MXC_Heuristic(TMethMaxCut method,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    LogEntry(LOG_METH,"Computing heuristic maximum cut...");

    return MXC_HeuristicGRASP(s,t);
}


TFloat abstractMixedGraph::MXC_HeuristicGRASP(TNode s,TNode t)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_HeuristicGRASP",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_HeuristicGRASP",t);

    #endif

    moduleGuard M(ModMaxCutGRASP,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    goblinQueue<TNode,TFloat>* Q = nHeap;

    if (Q!=NULL) Q -> Init();
    else Q = NewNodeHeap();

    branchMaxCut thisNode(*this,s,t);

    thisNode.ReallySaveSolution();
    TNode* nodeColour = GetNodeColours();

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1);

    #endif

    while (!thisNode.Feasible())
    {
        Q -> Init();

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==NoNode) Q->Insert(v,thisNode.MinWeight(v));

        TNode u = Q->Delete();

        TNode draw = CT.Rand(2);
        if (draw >= Q->Cardinality()) draw = Q->Cardinality();

        for (;draw>0;draw--) u = Q->Delete();

        branchNode<TNode,TFloat>::TBranchDir dir =
            thisNode.DirectionConstructive(u);

        if (dir==branchNode<TNode,TFloat>::RAISE_FIRST)
        {
            thisNode.Lower(u);
            nodeColour[u] = 0;
        }
        else
        {
            thisNode.Raise(u);
            nodeColour[u] = 1;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            if (dir) sprintf(CT.logBuffer,"%lu[S] ",static_cast<unsigned long>(u));
            else sprintf(CT.logBuffer,"%lu[T] ",static_cast<unsigned long>(u));

            LogAppend(LH,CT.logBuffer);
        }

        #endif

        M.SetLowerBound(thisNode.selectedWeight);
        M.Trace(1);
    }

    thisNode.ReallySaveSolution();

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    if (nHeap==NULL) delete Q;

    TFloat weight = thisNode.selectedWeight;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Cut has weight %g",weight);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    if (CT.methLocal==LOCAL_OPTIMIZE) weight = MXC_LocalSearch(nodeColour,s,t);

    return weight;
}


TFloat abstractMixedGraph::MXC_LocalSearch(TNode* nodeColour,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_LocalSearch",s);

    if (t>=n && t!=NoNode) NoSuchNode("MXC_LocalSearch",t);

    if (!nodeColour)
        Error(ERR_REJECTED,"MXC_LocalSearch","Missing cut");

    #endif

    moduleGuard M(ModMaxCut,*this,"Searching for local optimality...",
        moduleGuard::SYNC_BOUNDS);

    TNode card = 0;
    for (TNode v=0;v<n;v++)
        if (nodeColour[v]==1) card++;

    TFloat weight = 0;
    for (TArc a=0;a<2*m;a++)
    {
        TNode u = StartNode(a);
        TNode w = EndNode(a);

        if (nodeColour[u]==0 && nodeColour[w]==1 && !Blocking(a))
            weight += UCap(a)*Length(a&(a^1));
    }

    TFloat* profit = new TFloat[n];

    while (true)
    {
        for (TNode v=0;v<n;v++) profit[v] = 0;

        for (TArc a=0;a<m;a++)
        {
            TNode u = StartNode(2*a);
            TNode w = EndNode(2*a);

            if (u==w) continue;

            if (nodeColour[u]==1 && nodeColour[w]==1)
            {
                profit[u] += UCap(2*a)*Length(2*a);

                if (Orientation(2*a)==0)
                    profit[w] += UCap(2*a)*Length(2*a); 
            }

            if (nodeColour[u]==0 && nodeColour[w]==0)
            {
                profit[w] += UCap(2*a)*Length(2*a);

                if (Orientation(2*a)==0)
                    profit[u] += UCap(2*a)*Length(2*a); 
            }

            if (nodeColour[u]==1 && nodeColour[w]==0 && Orientation(2*a)==0)
            {
                profit[w] -= UCap(2*a)*Length(2*a);
                profit[u] -= UCap(2*a)*Length(2*a);
            }

            if (nodeColour[u]==0 && nodeColour[w]==1)
            {
                profit[w] -= UCap(2*a)*Length(2*a);
                profit[u] -= UCap(2*a)*Length(2*a);
            }
        }

        TNode maxNode = NoNode;
        for (TNode v=0;v<n;v++)
        {
            if (   v!=s && v!=t
                && (maxNode==NoNode || profit[v]>profit[maxNode])
                && (card>1   || nodeColour[v]==0)
                && (card<n-1 || nodeColour[v]==1)
               )
            {
                maxNode = v;
            }
        }

        if (profit[maxNode]<=0) break;

        nodeColour[maxNode] = 1-nodeColour[maxNode];
        weight += profit[maxNode];

        if (nodeColour[maxNode]==1) card++; else card--;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,
                "Node %lu moves to component %c",
                static_cast<unsigned long>(maxNode),char('S'+nodeColour[maxNode]));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        M.SetLowerBound(weight);
        M.Trace();
    }

    delete[] profit;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Cut has weight: %g",weight);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return weight;
}


TFloat abstractGraph::MXC_Heuristic(TMethMaxCut method,TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    moduleGuard M(ModMaxCut,*this,
        moduleGuard::NO_INDENT | moduleGuard::SYNC_BOUNDS);

    TFloat ret = InfFloat;

    if (method==MAX_CUT_DEFAULT) method = TMethMaxCut(CT.methMaxCut);

    switch (method)
    {
        case MAX_CUT_TJOIN:
        {
            LogEntry(LOG_METH,"Applying planar max-cut method...");

            bool solved = true;
            try
            {
                ret = MXC_DualTJoin(s);
            }
            catch (ERRejected)
            {
                solved = false;
            }

            TNode* nodeColour = GetNodeColours();

            if (solved && (t==NoNode || nodeColour[t]==1))
            {
                M.SetUpperBound(ret);
                break;
            }

            if (solved && t!=NoNode && nodeColour[t]==0)
                LogEntry(LOG_RES,"...Target node has not be separated");

            // Do not break
        }
        case MAX_CUT_GRASP:
        case MAX_CUT_TREE:
        {
            LogEntry(LOG_METH,"Computing heuristic maximum cut...");

            if (method==MAX_CUT_TREE)
            {
                try
                {
                    ret = MXC_HeuristicTree(s,t);
                    break;
                }
                catch (ERRejected) {};
            }

            ret = MXC_HeuristicGRASP(s,t);

            break;
        }
        default:
        {
            UnknownOption("MXC_Heuristic",method);
        }
    }

    return ret;
}


TFloat abstractGraph::MXC_DualTJoin(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_DualTJoin",s);

    #endif

    moduleGuard M(ModMaxCutTJoin,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n+2);

    #endif

    dualGraph* D = NULL;

    try
    {
        D = new dualGraph(*this);
    }
    catch (ERRejected)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin","Missing planar representation");
    }

    graphRepresentation* DR = D->Representation();
    bool negWeights = false;

    for (TArc a=0;a<m;a++)
    {
        TFloat thisWeight = Length(2*a)*UCap(2*a);
        DR -> SetLength(2*a,-thisWeight);

        if (thisWeight<0) negWeights = true;
    }

    if (negWeights)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin",
            "Negative edge weights are not supported");
    }

    M.Trace(1);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    D -> MinCTJoin(voidIndex<TNode>(D->N(),CT));

    M.Trace(n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif
 
    LogEntry(LOG_METH,"Extracting cut from T-join...");
    OpenFold();

    TNode* nodeColour = InitNodeColours(NoNode);

    staticQueue<TNode> Q(n,CT);

    if (s!=NoNode)
    {
        Q.Insert(s);
        nodeColour[s] = 0;
    }
    else
    {
        Q.Insert(0);
        nodeColour[0] = 0;
    }

    TNode nodeCount = 1;
    TFloat weight = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!(Q.Empty()))
    {
        M.Trace(Q);

        TNode u = Q.Delete();

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u))
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);

            TNode reqColour = nodeColour[u];

            if (D -> Sub(a)>0)
            {
                reqColour = 1-nodeColour[u];

                if (u<v) weight += Length(a)*UCap(a);
            }

            if (nodeColour[v]==NoNode)
            {
                nodeColour[v] = reqColour;
                M.Trace(Q);
                Q.Insert(v);
                nodeCount++;
            }
            else if (nodeColour[v]!=reqColour)
            {
                InternalError("MXC_DualTJoin","Dual Subgraph is not a T-Join");
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    CloseFold();

    delete D;

    #if defined(_FAILSAVE_)

    if (nodeCount<n)
    {
        Error(ERR_REJECTED,"MXC_DualTJoin","Graph is disconnected");
    }

    #endif

    M.SetLowerBound(weight);
    M.Trace(1);

    sprintf(CT.logBuffer,"...Cut has weight %g",weight);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return weight;
}


TFloat abstractGraph::MXC_HeuristicTree(TNode s,TNode t)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("MXC_HeuristicTree",s);
    if (t>=n && t!=NoNode) NoSuchNode("MXC_HeuristicTree",t);

    #endif

    moduleGuard M(ModMaxCut,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH,"(Tree Heuristics)");

    #if defined(_PROGRESS_)

    if (CT.methSolve<2) M.SetProgressNext(1);

    #endif

    TNode* nodeColour = InitNodeColours();

    TNode r = s;
    if (r==NoNode) r = TNode(CT.Rand(n));

    for (TNode v=0;v<n;v++) nodeColour[v] = NoNode;

    nodeColour[r] = 0;

    if (t!=NoNode) nodeColour[t] = 1;

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TArc a=0;a<m;a++)
    {
        GR -> SetLength(2*a,UCap(2*a)*Length(2*a));
    }

    G.MinTree(MST_DEFAULT,MST_MAX,r);

    staticQueue<TNode> S(n,CT);

    for (TNode v=0;v<n;v++)
        if (nodeColour[v]==NoNode) S.Insert(v);

    while (!S.Empty())
    {
        TNode v = S.Delete();
        TArc a = G.Pred(v);

        if (a!=NoArc)
        {
            TNode u = G.StartNode(a);

            if (nodeColour[u]==NoNode) S.Insert(v);
            else nodeColour[v] = 1-nodeColour[u];
        }
        else
        {
            Error(ERR_REJECTED,"MXC_HeuristicTree","Graph is disconnected");
        }
    }

    TFloat weight = 0;
    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode w = EndNode(2*a);
        if (nodeColour[u]!=nodeColour[w]) weight += UCap(2*a)*Length(2*a);
    }

    M.SetLowerBound(weight);
    M.Trace();

    sprintf(CT.logBuffer,"...Cut has weight %g",weight);
    M.Shutdown(LOG_RES,CT.logBuffer);

    if (CT.methLocal==LOCAL_OPTIMIZE) weight = MXC_LocalSearch(nodeColour,s,t);

    return weight;
}
