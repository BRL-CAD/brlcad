
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveStable.cpp
/// \brief  Stable set branch & bound with application to cliques and vertex cover

#include "branchStable.h"
#include "branchMIP.h"
#include "sparseGraph.h"
#include "abstractBigraph.h"
#include "moduleGuard.h"


branchStable::branchStable(abstractMixedGraph &GC) throw() :
    branchNode<TNode,TFloat>(GC.N(),GC.Context()), G(GC)
{
    int savedMaxBBIterations = CT.maxBBIterations;
    CT.maxBBIterations = 1;
    TFloat initialBound = G.CliqueCover();
    if (scheme) scheme->M.SetUpperBound(initialBound);
    CT.maxBBIterations = savedMaxBBIterations;

    for (TNode v=0;v<n;v++) G.SetDist(v,TFloat(G.NodeColour(v)));

    if (CT.traceLevel==3) G.Display();

    chi = new char[n];
    currentVar = 0;
    selected = 0;
    H = G.Investigate();

    for (TNode v=0;v<n;v++) chi[v] = 1;

    LogEntry(LOG_MEM,"(stable sets)");
}


branchStable::branchStable(branchStable &Node) throw() :
    branchNode<TNode,TFloat>(Node.G.N(),Node.Context(),Node.scheme), G(Node.G)
{
    chi = new char[n];
    currentVar = Node.currentVar;
    selected = Node.selected;

    for (TNode v=0;v<n;v++)
    {
        chi[v] = Node.chi[v];
        if (chi[v]!=1) unfixed--;
    }

    H = G.Investigate();

    LogEntry(LOG_MEM,"(stable sets)");
}


branchStable::~branchStable() throw()
{
    delete[] chi;
    G.Close(H);

    LogEntry(LOG_MEM,"(stable sets)");
}


unsigned long branchStable::Size() const throw()
{
    return
          sizeof(branchStable)
        + managedObject::Allocated()
        + branchNode<TNode,TFloat>::Allocated()
        + Allocated();
}


unsigned long branchStable::Allocated() const throw()
{
    return n*sizeof(char);
}


TNode branchStable::SelectVariable() throw()
{
    TNode* reducedDeg = new TNode[n];
    for (TNode i=0;i<n;i++) reducedDeg[i] = 0;

    for (TArc a=0;a<2*G.M();a++)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        if (chi[u]==1 && chi[v]==1) reducedDeg[u]++;
    }

    TNode minNode = NoNode;
    TNode minDeg = 0;
    for (TNode i=0;i<n;i++)
    {
        if (chi[i]==1 && (minNode==NoNode || reducedDeg[i]<minDeg))
        {
            minNode = i;
            minDeg = reducedDeg[i];
        }
    }

    delete[] reducedDeg;

    if (minNode!=NoNode) return minNode;

    #if defined(_FAILSAVE_)

    InternalError("SelectVariable","Solution is fixed");

    #endif

    throw ERInternal();
}


branchNode<TNode,TFloat>::TBranchDir branchStable::DirectionConstructive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionConstructive",v);

    #endif

    return RAISE_FIRST;
}


branchNode<TNode,TFloat>::TBranchDir branchStable::DirectionExhaustive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionExhaustive",v);

    #endif

    return RAISE_FIRST;
}


branchNode<TNode,TFloat> *branchStable::Clone() throw()
{
    return new branchStable(*this);
}


void branchStable::Raise(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Raise",i);

    #endif

    chi[i] = 2;
    unfixed--;
    selected++;
    solved = false;

    G.Reset(H,i);
    while (G.Active(H,i))
    {
        TArc a = G.Read(H,i);
        TNode w = G.EndNode(a);
        if (chi[w]==1)
        {
            chi[w] = 0;
            unfixed--;
        }

        #if defined(_FAILSAVE_)

        if (chi[w]==2)
        {
            sprintf(CT.logBuffer,"Conflicting nodes: %lu, %lu",
                static_cast<unsigned long>(i),static_cast<unsigned long>(w));
            InternalError1("Raise");
        }

        #endif
    }
}


void branchStable::Lower(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Lower",i);

    #endif

    chi[i] = 0;
    unfixed--;
    solved = false;
}


TFloat branchStable::SolveRelaxation() throw()
{
    TNode maxColour = 0;

    for (TNode v=0;v<n;v++)
        if (TNode(G.Dist(v))>maxColour) maxColour = TNode(G.Dist(v));

    TFloat objective = selected;
    for (TNode c=0;c<=maxColour;c++)
    {
        bool blocked = true;
        for (TNode v=0;v<n;v++)
        {
            if (chi[v]==1 && TNode(G.Dist(v))==c) blocked = false;
        }

        if (!blocked) objective += 1;
    }

    return objective;
}


branchNode<TNode,TFloat>::TObjectSense branchStable::ObjectSense() const throw()
{
    return MAXIMIZE;
}


TFloat branchStable::Infeasibility() const throw()
{
    return -InfFloat;
}


void branchStable::SaveSolution() throw()
{
    for (TNode v=0;v<n;v++) G.SetNodeColour(v,chi[v]/2);

    if (CT.traceLevel==3) G.Display();
}


TNode abstractMixedGraph::StableSet() throw()
{
    moduleGuard M(ModStable,*this,"Computing maximum independent nodes set...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10000, 3000);

    #endif

    TNode* nodeColour = GetNodeColours();
    TNode* savedStable = NULL;

    bool isStable = (nodeColour!=NULL);
    bool isCover = (nodeColour!=NULL);
    TNode cardInitial = 0;

    for (TArc a=0;a<m && isStable;a++)
    {
        if (nodeColour[StartNode(2*a)]>0 && nodeColour[EndNode(2*a)]>0)
            isStable = false;
    }

    for (TNode w=0;w<n && isCover;w++)
    {
        for (TNode v=w+1;v<n && isCover;v++)
            if (nodeColour[v]==nodeColour[w] && Adjacency(v,w)==NoArc) isCover = false;
    }

    if (isStable)
    {
        savedStable = new TNode[n];

        for (TNode w=0;w<n;w++)
        {
            savedStable[w] = nodeColour[w];
            if (nodeColour[w]>0) cardInitial++;
        }
    }
    else if (!isCover)
    {
        nodeColour = InitNodeColours(0);
        nodeColour[0] = 1;
        cardInitial = 1;
    }
    else
    {
        sprintf(CT.logBuffer,"...Starting with clique partition");
        LogEntry(LOG_METH,CT.logBuffer);
    }

    if (CT.logMeth && !isCover)
    {
        sprintf(CT.logBuffer,"...Starting with stable set of size %lu",
            static_cast<unsigned long>(cardInitial));
        LogEntry(LOG_METH,CT.logBuffer);
    }

    M.SetLowerBound(cardInitial);


    TNode cardinality = cardInitial;

    if (CT.methSolve<3)
    {
        // Combinatorial branch & bound

        branchStable* rootNode = new branchStable(*this);

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(7000);

        #endif

        branchScheme<TNode,TFloat> scheme(rootNode,cardInitial,ModStable);

        cardinality = TNode(scheme.savedObjective);
    }
    else
    {
        // Integer linear branch & bound

        mipInstance* X =
            static_cast<mipInstance*>(StableSetToMIP());
        branchMIP* rootNode = new branchMIP(*X);

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(7000);

        #endif

        branchScheme<TIndex,TFloat> scheme(rootNode,cardInitial,ModStable);

        cardinality = TNode(scheme.savedObjective);

        if (cardinality>cardInitial)
        {
            for (TNode i=0;i<n;i++)
            {
                if (X->VarValue(TVar(i)) > 0.5)
                {
                    nodeColour[i] = 1;
                }
                else nodeColour[i] = 0;
            }
        }

        delete X;
    }

    if (cardinality>cardInitial)
    {
        sprintf(CT.logBuffer,
            "...Stable set has cardinality %lu",static_cast<unsigned long>(cardinality));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    if (savedStable)
    {
        if (cardinality<=cardInitial)
        {
            for (TNode w=0;w<n;w++) nodeColour[w] = savedStable[w];
        }

        delete[] savedStable;
    }

    return cardinality;
}


TNode abstractMixedGraph::Clique() throw()
{
    moduleGuard M(ModStable,*this,"Computing maximum clique...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10000, 500);

    #endif

    TNode* nodeColour = GetNodeColours();

    bool isClique = (nodeColour!=NULL);
    bool isColoured = (nodeColour!=NULL);
    TNode cardInitial = 0;

    for (TNode w=0;w<n && isClique;w++)
    {
        for (TNode v=w+1;nodeColour[w]>0 && v<n && isClique;v++)
            if (nodeColour[v]>0 && Adjacency(v,w)==NoArc) isClique = false;
    }

    for (TArc a=0;a<m && isColoured;a++)
    {
        if (nodeColour[StartNode(2*a)]==nodeColour[EndNode(2*a)])
            isColoured = false;
    }

    if (isClique)
    {
        for (TNode w=0;w<n;w++)
            if (nodeColour[w]>0) cardInitial++;
    }
    else if (!isColoured)
    {
        nodeColour = InitNodeColours(0);

        if (m>0)
        {
            nodeColour[StartNode(0)] = nodeColour[EndNode(0)] = 1;
            cardInitial = 2;
        }
        else
        {
            nodeColour[0] = 1;
            cardInitial = 1;
        }
    }

    if (CT.logMeth && !isColoured)
    {
        sprintf(CT.logBuffer,"...Starting with %lu-clique",static_cast<unsigned long>(cardInitial));
        LogEntry(LOG_METH,CT.logBuffer);
    }

    M.SetLowerBound(cardInitial);


    TNode cardinality = cardInitial;

    complementaryGraph* G = new complementaryGraph(*this,(TOption)0);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(2500);

    #endif

    TNode* complColour = G->RawNodeColours();

    for (TNode v=0;v<n;v++) complColour[v] = nodeColour[v];

    if (CT.methSolve<3)
    {
        // Combinatorial branch & bound

        branchStable* rootNode = new branchStable(*G);

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(7000);

        #endif

        branchScheme<TNode,TFloat> scheme(rootNode,cardInitial,ModStable);

        cardinality = TNode(scheme.savedObjective);

        for (TNode v=0;v<n && cardinality>cardInitial;v++)
        {
            nodeColour[v] = complColour[v];
        }
    }
    else
    {
        // Integer linear branch & bound

        mipInstance* X =
            static_cast<mipInstance*>(G->StableSetToMIP());

        branchMIP* rootNode = new branchMIP(*X);

        #if defined(_PROGRESS_)

        M.ProgressStep();
        M.SetProgressNext(7000);

        #endif

        branchScheme<TIndex,TFloat> scheme(rootNode,cardInitial,ModStable);

        cardinality = TNode(scheme.savedObjective);

        for (TNode i=0;i<N() && cardinality>cardInitial;i++)
        {
            if (X->VarValue(TVar(i)) > 0.5)
            {
                nodeColour[i] = 1;
            }
            else nodeColour[i] = 0;
        }

        delete X;
    }

    if (cardinality>cardInitial)
    {
        sprintf(CT.logBuffer,
            "...Improved clique has cardinality %lu",static_cast<unsigned long>(cardinality));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    delete G;

    return cardinality;
}


TNode abstractMixedGraph::VertexCover() throw()
{
    moduleGuard M(ModStable,*this,"Computing minimum vertex cover...");

    TNode ret = StableSet();
    TNode* nodeColour = GetNodeColours();

    for (TNode v=0;v<n;v++) nodeColour[v] = 1-nodeColour[v];

    return n-ret;
}


TNode abstractBiGraph::StableSet() throw()
{
    moduleGuard M(ModStable,*this,"Computing maximum stable set...");

    TNode* nodeColour = RawNodeColours();

    for (TNode v=0;v<n1;v++)
    {
        if (n1<n2) nodeColour[v] = 0;
        else nodeColour[v] = 1;
    }

    for (TNode v=n1;v<n;v++)
    {
        if (n2<=n1) nodeColour[v] = 0;
        else nodeColour[v] = 1;
    }

    if (n1<n2) return n2;
    else return n1;
}
