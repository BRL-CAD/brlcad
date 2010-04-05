
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveColour.cpp
/// \brief  Node colour enumeration, methods for planar node colouring, edge colouring and clique covers

#include "branchColour.h"
#include "mixedGraph.h"
#include "sparseGraph.h"
#include "staticQueue.h"
#include "abstractBigraph.h"
#include "moduleGuard.h"


branchColour::branchColour(abstractMixedGraph& GC,TNode kk,char mode) throw():
    branchNode<TNode,TFloat>(GC.N(),GC.Context()), G(GC)
{
    n = G.N();
    nActive = n;
    nDominated = 0;
    nColoured = 0;
    m = G.M();
    master = 1;

    k = kk;
    if (k<2) k = 2;
    depth = NoNode; //  n*(k-1)+1;

    exhaustive = (mode>0);

    DOMINATED = NoNode;

    colour = new TNode[n];
    active = new bool[n];
    conflicts = new TNode[n];

    for (TNode w=0;w<n;w++)
    {
        colour[w] = 0;
        active[w] = true;
        conflicts[w] = 0;
    }

    for (TArc a=0;a<2*m;a++)
    {
        TNode w = G.StartNode(a);
        conflicts[w]++;
    }

    kMax = 0;
    TNode vMax = 0;
    for (TNode w=0;w<n;w++)
    {
        if (conflicts[w]>kMax)
        {
            kMax = conflicts[w];
            vMax = w;
        }
    }

    neighbours = new TNode*[n];
    for (TNode w=0;w<n;w++)
    {
        neighbours[w] = new TNode[k];
        for (TNode c=0;c<k;c++) neighbours[w][c] = 0;
    }

    Dominated = new staticStack<TNode>(n,CT);

    I = G.NewInvestigator();

    LogEntry(LOG_MEM,"...Partial colouring generated");

    bool isClique = true;
    for (TNode w=0;w<n && isClique;w++)
        for (TNode v=w+1;G.NodeColour(w)>0 && v<n && isClique;v++)
            if (G.NodeColour(v)>0 && G.Adjacency(v,w)==NoArc) isClique = false;

    if (isClique)
    {
        TNode i = 0;
        for (TNode w=0;w<n;w++)
        {
            if (G.NodeColour(w)==0)
            {
                if (i>k) colour[w] = k;
            }
            else if (G.NodeColour(w)>0)
            {
                if (active[w] && i<k) SetColour(w,i);

                i++;
            }
        }

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Starting with %lu-clique",
                static_cast<unsigned long>(i));
            LogEntry(LOG_METH,CT.logBuffer);
        }

        if (scheme) scheme->M.SetLowerBound(i);
    }
    else
    {
        SetColour(vMax,0);
        I -> Reset(vMax);
        TNode v1 = NoNode;
        while (I->Active(vMax))
        {
            TNode w = G.EndNode(I->Read(vMax));
            if (active[w] && (v1==NoNode || conflicts[w]>conflicts[v1])) v1 = w;
        }

        if (v1!=NoNode) SetColour(v1,1);

        if (scheme) scheme->M.SetLowerBound(2);
    }

    Reduce();
}


branchColour::branchColour(branchColour& node) throw():
    branchNode<TNode,TFloat>(node.G.N(),node.Context(),node.scheme), G(node.G)
{
    n = G.N();
    nActive = node.nActive;
    nDominated = node.nDominated;
    nColoured = node.nColoured;
    m = G.M();
    k = node.k;
    kMax = node.kMax;
    depth = NoNode; // n*(k-1)+1;
    master = 0;
    exhaustive = node.exhaustive;

    DOMINATED = NoNode;

    colour = new TNode[n];
    active = new bool[n];
    conflicts = new TNode[n];
    neighbours = new TNode*[n];

    for (TNode w=0;w<n;w++)
    {
        colour[w] = node.colour[w];
        active[w] = node.active[w];
        conflicts[w] = node.conflicts[w];
        neighbours[w] = new TNode[k];

        for (TNode c=0;c<k;c++)
            neighbours[w][c] = node.neighbours[w][c];
    }

    Dominated = new staticStack<TNode>(n,CT);
    staticStack<TNode> S(n,CT);

    while (!node.Dominated->Empty())
        S.Insert(node.Dominated->Delete());

    while (!S.Empty())
    {
        TNode w = S.Delete();
        node.Dominated->Insert(w);
        Dominated -> Insert(w);
    }

    I = G.NewInvestigator();

    LogEntry(LOG_MEM,"...Partial colouring generated");
}


void branchColour::Show() throw()
{
    sparseGraph H(G,OPT_CLONE);

    for (TNode v=0;v<n;v++)
        if (active[v]) H.SetNodeColour(v,n);
        else H.SetNodeColour(v,colour[v]);

    H.Display();
}


unsigned long branchColour::Size() const throw()
{
    return
          sizeof(branchColour)
        + managedObject::Allocated()
        + branchNode<TNode,TFloat>::Allocated()
        + branchColour::Allocated();
}


unsigned long branchColour::Allocated() const throw()
{
    unsigned long tmpSize
        = n*sizeof(TNode)             // colour[]
        + n*sizeof(bool)              // active[]
        + n*sizeof(TNode)             // conflicts[]
        + n*sizeof(TNode*)            // neighbours[]
        + n*k*sizeof(TNode);          // neighbours[][]

    return tmpSize;
}


branchColour::~branchColour() throw()
{
    for (TNode w=0;w<n;w++) delete[] neighbours[w];
    delete[] neighbours;

    delete[] conflicts;
    delete[] active;
    delete[] colour;

    delete I;
    delete Dominated;

    LogEntry(LOG_MEM,"...Partial colouring disallocated");
}


void branchColour::SetColour(TNode v,TNode c) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetColour",v);

    if (c>=k)
    {
        sprintf(CT.logBuffer,"No such colour: %lu",
            static_cast<unsigned long>(c));
        Error(ERR_RANGE,"SetColour",CT.logBuffer);
    }

    if (!active[v])
    {
        sprintf(CT.logBuffer,"Node is coloured or dominated: %lu",
            static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"SetColour",CT.logBuffer);
    }

    #endif

    colour[v] = c;
    nColoured++;
    active[v] = false;
    nActive--;

    if (CT.traceLevel==3) Show();

    I -> Reset(v);
    while (I->Active(v))
    {
        TNode w = G.EndNode(I->Read(v));

        if (neighbours[w][c]>0) conflicts[w]--;
        neighbours[w][c]++;
        if (conflicts[w]<k && active[w]) Reduce(w);
    }

    unfixed = nActive;
}


void branchColour::Reduce(TNode w) throw(ERRange,ERRejected)
{
    staticQueue<TNode> Q(n,CT);

    if (w==NoNode)
    {
        for (TNode v=0;v<n;v++)
            if (conflicts[v]<k && active[v])
        {
            Q.Insert(v);
            colour[v] = DOMINATED;
            nDominated++;
            active[v] = false;
            nActive--;

            if (CT.traceLevel==3) Show();
        }
    }
    else
    {
        #if defined(_FAILSAVE_)

        if (w>=n) NoSuchNode("Reduce",w);

        if (conflicts[w]>=k || !active[w])
        {
            sprintf(CT.logBuffer,"Inappropriate node: %lu",
                static_cast<unsigned long>(w));
            Error(ERR_REJECTED,"Reduce",CT.logBuffer);
        }

        #endif

        Q.Insert(w);
        colour[w] = DOMINATED;
        nDominated++;
        active[w] = false;
        nActive--;

        if (CT.traceLevel==3) Show();
    }

    TNode j = 0;
    while (!Q.Empty())
    {
        TNode v = Q.Delete();
        Dominated -> Insert(v);
        j++;

        I -> Reset(v);
        while (I->Active(v))
        {
            TNode u = G.EndNode(I->Read(v));
            conflicts[u]--;
            if (conflicts[u]<k && active[u])
            {
                Q.Insert(u);
                colour[u] = DOMINATED;
                nDominated++;
                active[u] = false;
                nActive--;

                if (CT.traceLevel==3) Show();
            }
        }
    }

    unfixed = nActive;
}


void branchColour::PlanarComplete() throw(ERCheck)
{
    staticQueue<TNode> Q(n,CT);

    TNode* nodeColour = G.GetNodeColours();
    if (!nodeColour) nodeColour = G.InitNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if (active[v]) Q.Insert(v);

        nodeColour[v] = colour[v];
    }

    while (!(Dominated->Empty() && Q.Empty()))
    {
        TNode v = NoNode;
        if (!Q.Empty()) v = Q.Delete();
        else v = Dominated->Delete();

        if (colour[v]!=0 && colour[v]!=DOMINATED) continue;

        TNode c = 0;
        for (;c<5 && neighbours[v][c]>0;c++) {};

        if (c<5)
        {
            active[v] = true;
            nActive++;
            nDominated++;
            SetColour(v,c);
            nodeColour[v] = c;
        }
        else
        {
            TArc a1 = G.First(v);
            TArc a2 = G.Right(a1,v);
            TArc a3 = G.Right(a2,v);
            TArc a4 = G.Right(a3,v);
            TNode x1 = G.EndNode(a1);
            TNode x2 = G.EndNode(a2);
            TNode x3 = G.EndNode(a3);
            TNode x4 = G.EndNode(a4);
            TNode c1 = G.NodeColour(x1);

            if (G.NCKempeExchange(nodeColour,x1,x3))
            {
                active[v] = true;
                nActive++;
                nDominated++;
                SetColour(v,c1);
                nodeColour[v] = c1;
            }
            else
            {
                TNode c2 = G.NodeColour(x2);
                if (G.NCKempeExchange(nodeColour,x2,x4))
                {
                    active[v] = true;
                    nActive++;
                    nDominated++;
                    SetColour(v,c2);
                    nodeColour[v] = c2;
                }
                else Error(ERR_CHECK,"PlanarComplete","Not a planar graph");
            }
        }
    }
}


bool branchColour::Complete() throw(ERRejected)
{
    for (TNode v=0;v<n;v++)
    {
        if (active[v])
        {
            TNode c = colour[v];
            for (;c<k && neighbours[v][c]>0;c++) {};

            if (c==k)
            {
                sprintf(CT.logBuffer,"Got stuck at node %lu",
                    static_cast<unsigned long>(v));
                Error(ERR_REJECTED,"Complete",CT.logBuffer);
            }

            SetColour(v,c);
        }
    }

    while (!Dominated->Empty())
    {
        TNode v = Dominated->Delete();

        if (colour[v]==DOMINATED)
        {
            TNode c = 0;
            for (;c<k && neighbours[v][c]>0;c++) {};

            if (c==k)
            {
                sprintf(CT.logBuffer,"Got stuck at node %lu",
                    static_cast<unsigned long>(v));
                Error(ERR_REJECTED,"Complete",CT.logBuffer);
            }

            active[v] = 1;
            nActive++;
            nDominated++;
            SetColour(v,c);
        }
    }

    return true;
}


TNode branchColour::SelectVariable() throw()
{
    TNode retNode = NoNode;
    TNode maxDanger1 = NoNode;
    TNode maxDanger2 = 0;

    for (TNode v=0;v<n;v++)
    {
        if (!active[v]) continue;

        TNode thisDanger1 = colour[v];

        for (TNode c = colour[v];c<k;c++)
        {
            if (neighbours[v][c]>0) thisDanger1++;
        }

        if (maxDanger1==NoNode || thisDanger1>=maxDanger1)
        {
            if (   maxDanger1==NoNode
                || thisDanger1>maxDanger1 )
            {
                maxDanger1 = thisDanger1;
                maxDanger2 = NoNode;
                retNode = v;
            }
            else if (thisDanger1==maxDanger1)
            {
                TNode thisDanger2 = 0;
                I -> Reset(v);

                while (I->Active(v))
                {
                    TNode w = G.EndNode(I->Read(v));

                    if (!active[w]) continue;

                    if (CT.methColour==0)
                    {
                        for (TNode c = colour[v];c<k;c++)
                        {
                            if (neighbours[v][c]==0 && neighbours[w][c]==0)
                                thisDanger2++;
                        }
                    }
                    else
                    {
                        if (active[w]) thisDanger2++;
                    }
                }

                if (maxDanger2==NoNode)
                {
                    maxDanger2 = 0;
                    I -> Reset(retNode);

                    while (I->Active(retNode))
                    {
                        TNode w = G.EndNode(I->Read(retNode));

                        if (!active[w]) continue;

                        if (CT.methColour==0)
                        {
                            for (TNode c = colour[retNode];c<k;c++)
                            {
                                if (neighbours[retNode][c]==0 && neighbours[w][c]==0)
                                    thisDanger2++;
                            }
                        }
                        else
                        {
                            if (active[w]) thisDanger2++;
                        }
                    }
                }

                if (maxDanger2<thisDanger2)
                {
                    maxDanger1 = thisDanger1;
                    maxDanger2 = thisDanger2;
                    retNode = v;
                }
            }
        }
    }

    return retNode;
}


branchNode<TNode,TFloat>::TBranchDir branchColour::DirectionConstructive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionConstructive",v);

    #endif

    return LOWER_FIRST;
}


branchNode<TNode,TFloat>::TBranchDir branchColour::DirectionExhaustive(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DirectionExhaustive",v);

    #endif

    return DirectionConstructive(v);
}


void branchColour::Raise(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Raise",i);

    #endif

    colour[i]++;

    solved = 0;
}


void branchColour::Lower(TNode i) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=n) NoSuchNode("Lower",i);

    #endif

    SetColour(i,colour[i]);

    solved = 0;
}


TFloat branchColour::SolveRelaxation() throw()
{
    bool feasible = true;
    bool reduction = true;

    while (reduction && feasible)
    {
        reduction = false;

        for (TNode w=0;w<n && feasible;w++)
        {
            if (active[w])
            {
                TNode freeColour = NoNode;
                TNode nFree = 0;

                for (TNode c=colour[w];c<k;c++)
                    if (neighbours[w][c]==0)
                    {
                        if (nFree==0) freeColour = c;
                        nFree++;
                    }

                if (nFree>0) colour[w] = freeColour;
                else feasible = false;

                if (nFree==1)
                {
                    SetColour(w,freeColour);
                    reduction = true;
                }
            }
        }
    }

    if (!feasible) return Infeasibility();

    if (Feasible()) Complete();

    TNode maxColour = 0;

    for (TNode w=0;w<n && feasible;w++)
        if (colour[w]!=DOMINATED && colour[w]>maxColour) maxColour = colour[w];

    return maxColour+1;
}


branchNode<TNode,TFloat>::TObjectSense branchColour::ObjectSense() const throw()
{
    return MINIMIZE;
}


TFloat branchColour::Infeasibility() const throw()
{
    return NoNode;
}


branchNode<TNode,TFloat> *branchColour::Clone() throw()
{
    return new branchColour(*this);
}


void branchColour::SaveSolution() throw()
{
    bool isUncoloured = false;
    TNode* nodeColour = G.GetNodeColours();

    if (!nodeColour)
    {
        nodeColour = G.InitNodeColours();
        isUncoloured = true;
    }

    for (TArc a=0;a<m && !isUncoloured;a++)
        if (nodeColour[G.StartNode(2*a)]==nodeColour[G.EndNode(2*a)])
            isUncoloured = true;

    TNode maxColourNew = 0;
    TNode maxColourOld = 0;

    for (TNode v=0;v<n;v++)
    {
        if (nodeColour[v]>maxColourOld) maxColourOld = nodeColour[v];
        if (colour[v]>maxColourNew) maxColourNew = colour[v];
    }

    if (maxColourOld>maxColourNew || isUncoloured)
    {
        for (TNode v=0;v<n;v++) nodeColour[v] = colour[v];
        if (scheme) scheme->M.SetUpperBound(maxColourNew+1);
    }
}


TNode abstractMixedGraph::NodeColouring(TNode k) throw()
{
    // Handle already existing colourings and cliques

    TNode maxColour = NoNode;
    TNode* savedClique = NULL;
    TNode* savedColour = NULL;

    TNode* nodeColour = GetNodeColours();

    if (nodeColour)
    {
        bool isColoured = true;

        for (TArc a=0;a<m && isColoured;a++)
        {
            if (nodeColour[StartNode(2*a)]==nodeColour[EndNode(2*a)])
                isColoured = false;
        }

        if (isColoured)
        {
            maxColour = 0;

            for (TNode v=0;v<n;v++)
                if (nodeColour[v]>maxColour) maxColour = nodeColour[v];

            maxColour++;
        }

        if (!isColoured && k==NoNode)
        {
            // The branch scheme is called several times and overwrites
            // the nodeColour[] array. A potential clique should be available
            // in every iteration. The branch scheme will determine lower
            // bounds, we do not need to know the clique cardinality here.

            savedClique = new TNode[n];
            savedColour = new TNode[n];

            for (TNode v=0;v<n;v++) savedClique[v] = nodeColour[v];
        }
    }


    int savedNumBBIterations = CT.maxBBIterations;

    // Handle special cases

    if (k>=5 && m<=3*TArc(n)-6)
    {
        maxColour = PlanarColouring()-1;

        if (maxColour<=2)
        {
            if (savedClique) delete[] savedClique;
            if (savedColour) delete[] savedColour;

            return maxColour;
        }

        for (TNode v=0;v<n && savedClique;v++)
        {
            savedColour[v] = nodeColour[v];
            nodeColour[v] = savedClique[v];
        }
    }


    // Execute branch scheme with k0 = k or k0 = n

    TNode k0 = k;
    if (k==NoNode) k0 = n;

    sprintf(CT.logBuffer,"Searching for %lu-node colouring...",
        static_cast<unsigned long>(k0));
    moduleGuard M(ModColour,*this,CT.logBuffer);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10000);

    TNode numIterations = 1;

    M.SetProgressNext((k!=NoNode) ? 10000 : 5000/n);

    #endif

    if (maxColour<NoNode)
    {
        sprintf(CT.logBuffer,
            "Starting with %lu-node colouring...",
            static_cast<unsigned long>(maxColour));
        LogEntry(LOG_METH,CT.logBuffer);
        M.SetUpperBound(maxColour);
    }

    for (TNode v=0;v<n && savedClique;v++) nodeColour[v] = savedClique[v];

    branchColour *rootNode = new branchColour(*this,k0,1);

    if (k==NoNode && (k0>=maxColour || maxColour>=k)) CT.maxBBIterations = 1;

    branchScheme<TNode,TFloat> *scheme
        = new branchScheme<TNode,TFloat>(rootNode,maxColour,ModColour);

    if (!scheme->feasible)
    {
        delete scheme;
        sprintf(CT.logBuffer,"...No %lu-node colouring exists",
            static_cast<unsigned long>(k0));
        M.Shutdown(LOG_RES,CT.logBuffer);
        CT.maxBBIterations = savedNumBBIterations;

        return 0;
    }

    if (maxColour > TNode(scheme->savedObjective))
    {
        maxColour = TNode(scheme->savedObjective);
        for (TNode v=0;v<n && savedColour;v++) savedColour[v] = nodeColour[v];

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,"...%lu-node colouring found",
                static_cast<unsigned long>(maxColour));
            LogEntry(LOG_RES,CT.logBuffer);
        }
    }


    // If no k was specified, repeat the scheme with decreasing k0

    while (   maxColour<=k0 && k0>2
           && k==NoNode
           && CT.SolverRunning()
           && M.LowerBound()<M.UpperBound() )
    {
        delete scheme;

        for (TNode v=0;v<n && savedClique;v++) nodeColour[v] = savedClique[v];

        if (maxColour+10<k0)
        {
            k0 = maxColour+10;
        }
        else k0--;

        if (CT.logMeth)
        {
            sprintf(CT.logBuffer,"Searching for %lu-node colouring...",
                static_cast<unsigned long>(k0));
            LogEntry(LOG_METH,CT.logBuffer);
        }

        OpenFold();

        #if defined(_PROGRESS_)

        if (k0<maxColour)
        {
            M.SetProgressCounter(5000);
            M.SetProgressNext(5000);
        }
        else
        {
            M.SetProgressCounter(
                5000*numIterations/(numIterations+k0-maxColour+1));
            M.SetProgressNext(5000/(numIterations+k0-maxColour+1));
        }

        numIterations++;

        #endif

        if (k0<maxColour)
        {
            // There is only little hope to improve the colouring. In order to
            // prove optimality, use a special branching rule which keeps the
            // number of subproblems small
            CT.maxBBIterations = savedNumBBIterations;
            rootNode = new branchColour(*this,k0,1);
        }
        else
        {
            rootNode = new branchColour(*this,k0,1);
        }

        scheme = new branchScheme<TNode,TFloat>(rootNode,maxColour,ModColour);

        CloseFold();

        if (maxColour > TNode(scheme->savedObjective))
        {
            maxColour = TNode(scheme->savedObjective);
            for (TNode v=0;v<n && savedColour;v++) savedColour[v] = nodeColour[v];

            if (CT.logRes)
            {
                sprintf(CT.logBuffer,"...%lu-node colouring found",
                    static_cast<unsigned long>(maxColour));
                LogEntry(LOG_RES,CT.logBuffer);
            }
        }
    }

    delete scheme;

    if (savedClique)
    {
        for (TNode v=0;v<n;v++) nodeColour[v] = savedColour[v];
        delete[] savedClique;
    }

    if (CT.methLocal==LOCAL_OPTIMIZE && M.LowerBound()<M.UpperBound())
    {
        maxColour = NCLocalSearch();
    }

    return maxColour;
}


TNode abstractMixedGraph::PlanarColouring() throw(ERRejected)
{
    moduleGuard M(ModColour,*this,"Searching for planar 5-node colouring...",
        moduleGuard::SYNC_BOUNDS);

    if (m==0)
    {
        M.Shutdown(LOG_RES,"...Graph is empty");
        InitNodeColours(0);
        return 1;
    }

    branchColour rootNode(*this,5);

    try
    {
        rootNode.PlanarComplete();
    }
    catch (ERCheck)
    {
        M.Shutdown(LOG_RES,"...Planar colouring failed");
        return NoNode;
    }

    TNode* nodeColour = GetNodeColours();
    TNode maxColour = 0;
    for (TNode v=0;v<n;v++)
        if (nodeColour[v]>maxColour) maxColour = nodeColour[v];
    maxColour++;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu-node colouring found",
            static_cast<unsigned long>(maxColour));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (CT.methLocal==LOCAL_OPTIMIZE && M.LowerBound()<M.UpperBound())
    {
        maxColour = NCLocalSearch();
    }

    return maxColour+1;
}


TNode abstractMixedGraph::NCLocalSearch() throw(ERRejected)
{
    moduleGuard M(ModColour,*this,"Searching for local optimality...",
        moduleGuard::SYNC_BOUNDS);

    TNode* nodeColour = GetNodeColours();

    #if defined(_FAILSAVE_)

    if (!nodeColour)
        Error(ERR_REJECTED,"NCLocalSearch","Missing node colours");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode count = 0;

    while (count<50*n && CT.SolverRunning())
    {
        TArc a1 = CT.Rand(2*m);
        TNode v = StartNode(a1);
        TNode w = EndNode(a1);
        TNode cOld = nodeColour[v];
        TNode c = nodeColour[w];
        count++;

        if (cOld==c)
        {
            // This cannot happen with correctly coloured graphs,
            // but just to be safe with with loops

            continue;
        }
        else if (cOld<c)
        {
            a1 = a1^1;
            v = StartNode(a1);
            w = EndNode(a1);
            c = nodeColour[w];
            cOld = nodeColour[v];
        }

        if (cOld<2)
        {
            // There is no chance to decrease the colour of v
            continue;
        }

        bool unique = true;
        TNode cMax = c;
        I.Reset(v);
        while (I.Active(v))
        {
            TNode u = EndNode(I.Read(v));

            if (nodeColour[u]==c && u!=w)
            {
                unique = false;
            }
            else if (nodeColour[u]>cMax)
            {
                cMax = nodeColour[u];
            }
        }

        if (cOld>cMax+1)
        {
            nodeColour[v] = cMax+1;
        }
        else if (!unique)
        {
            // It is intended to apply a Kempe exchange to w and then to assign
            // v with the colour c. But this is possible only if there is no
            // other neighbour of v with this colour.
            continue;
        }
        else
        {
            TNode c2 = c;
            TNode w2 = NoNode;

            while (c==c2) c2 = CT.Rand(nodeColour[v]);

            I.Reset(v);
            while (I.Active(v))
            {
                TNode u = EndNode(I.Read(v));

                if (nodeColour[u]==c2)
                {
                    w2 = u;
                    break;
                }
            }

            if (w2==NoNode)
            {
                nodeColour[v] = c2;
            }
            else if (NCKempeExchange(nodeColour,w,w2))
            {
                // Check if the Kempe exchange has coloured another neighbour of v

                TNode w3 = NoNode;
                I.Reset(v);

                while (I.Active(v))
                {
                    TNode u = EndNode(I.Read(v));

                    if (nodeColour[u]==c)
                    {
                        w3 = u;
                        break;
                    }
                }

                if (w3==NoNode) nodeColour[v] = c;
            }
        }

        if (nodeColour[v]!=cOld)
        {
            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "Colour of node %lu is changed to %lu",
                    static_cast<unsigned long>(v),
                    static_cast<unsigned long>(nodeColour[v]));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            TNode count = 0;
            for (TNode x=0;x<n;x++) if (nodeColour[x]==cOld) count++;

            if (count==0)
            {
                for (TNode x=0;x<n;x++)
                    if (nodeColour[x]>cOld) nodeColour[x]--;

                LogEntry(LOG_METH2,"One colour index saved");
            }
        }

        M.Trace(n*m);
    }

    Close(H);

    TNode maxColour = 0;
    for (TNode v=0;v<n;v++)
        if (nodeColour[v]>maxColour) maxColour = nodeColour[v];

    M.SetUpperBound(maxColour+1);

    return maxColour+1;
}


bool abstractMixedGraph::NCKempeExchange(TNode* nodeColour,TNode r,TNode x) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("NCKempeExchange",r);

    if (x>=n) NoSuchNode("NCKempeExchange",x);

    if (!nodeColour)
        Error(ERR_REJECTED,"NCKempeExchange","Missing node colours");

    #endif

    TNode colour1 = nodeColour[r];
    TNode colour2 = nodeColour[x];

    bool* marked = new bool[n];
    for (TNode w=0;w<n;w++) marked[w] = false;

    staticQueue<TNode> Q(n,CT);
    Q.Insert(r);
    nodeColour[r] = colour2;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Alternating colours %lu,%lu...",
            static_cast<unsigned long>(colour1),static_cast<unsigned long>(colour2));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!Q.Empty())
    {
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

            if (nodeColour[u]==nodeColour[v] && !marked[v])
            {
                marked[v] = true;

                if (nodeColour[v]==colour1) nodeColour[v] = colour2;
                else nodeColour[v] = colour1;

                Q.Insert(v);
            }
        }
    }

    #if defined(_LOGGING_)

    LogEnd(LH);

    #endif

    Close(H);

    delete[] marked;

    return nodeColour[x]==colour2;
}


TNode abstractMixedGraph::CliqueCover(TNode k) throw()
{
    if (k==NoNode)
    {
        sprintf(CT.logBuffer,"Computing minimum clique cover...");
    }
    else
    {
        sprintf(CT.logBuffer,"Computing %lu-clique cover...",
            static_cast<unsigned long>(k));
    }

    moduleGuard M(ModColour,*this,CT.logBuffer);

    complementaryGraph *G = new complementaryGraph(*this,(TOption)0);

    TNode* nodeColour = GetNodeColours();

    if (nodeColour)
    {
        for (TNode v=0;v<n;v++) G->SetNodeColour(v,nodeColour[v]);
    }
    else
    {
        nodeColour = InitNodeColours();
    }

    TNode chi = G->NodeColouring(k);

    for (TNode v=0;v<n;v++) nodeColour[v] = G->NodeColour(v);

    delete G;

    if (CT.logRes)
    {
        if (chi>0)
        {
            sprintf(CT.logBuffer,"...%lu-clique cover found",
                static_cast<unsigned long>(chi));
        }
        else
        {
            sprintf(CT.logBuffer,"...No %lu-clique cover found",
                static_cast<unsigned long>(k));
        }

        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return chi;
}


TArc abstractMixedGraph::EdgeColouring(TArc k) throw(ERRange)
{
    if (k==NoNode)
    {
        sprintf(CT.logBuffer,"Computing minimum edge colouring...");
    }
    else
    {
        sprintf(CT.logBuffer,"Computing %lu-edge colouring...",
            static_cast<unsigned long>(k));
    }

    moduleGuard M(ModColour,*this,CT.logBuffer);

    lineGraph *G = new lineGraph(*this,(TOption)0);
    TNode* nodeColour = G->InitNodeColours();

    THandle H = Investigate();
    investigator& I = Investigator(H);
    goblinQueue<TArc,TFloat>* Q = NewArcHeap();
    goblinQueue<TArc,TFloat>* Q2 = NewArcHeap();

    for (TArc a=0;a<m;a++)
    {
        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Colouring arc %lu...",
                static_cast<unsigned long>(2*a));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        TArc aCurrent = 2*a;
        TNode u = StartNode(aCurrent);
        TNode v1 = EndNode(aCurrent);

        // Compute the smallest colour free at node u

        I.Reset(u);
        Q -> Init();
        TArc thisDeg = 0;
        while (I.Active(u))
        {
            TArc a = I.Read(u);
            Q->Insert(a>>1,nodeColour[a>>1]);
            thisDeg++;
        }

        M.SetLowerBound(thisDeg);

        if (thisDeg>k)
        {
            delete G;

            sprintf(CT.logBuffer,"...No %lu-edge colouring exists",
                static_cast<unsigned long>(k));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return NoNode;
        }

        TNode alpha = NoNode;
        TArc c2 = NoNode;
        while (!Q->Empty() && alpha==NoNode)
        {
            TArc a2 = Q->Delete();
            TArc c3 = nodeColour[a2];
            if (c2==NoNode && c3>0) alpha = 0;
            if (c2!=NoNode && c2<c3-1) alpha = c2+1;
            c2 = c3;
        }

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...Available colour is %lu",
                static_cast<unsigned long>(alpha));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        // Check if the colour alpha is free at node v1

        I.Reset(v1);
        TArc aNext = NoArc;
        while (I.Active(v1) && aNext==NoArc)
        {
            TArc a = I.Read(v1);
            if (nodeColour[a>>1]==alpha) aNext = a;
        }

        bool searching = true;
        if (aNext==NoArc)
        {
            nodeColour[aCurrent>>1] = alpha;
            searching = false;

            LogEntry(LOG_METH2,"...Arc is coloured");
            M.Trace(*G,m);
        }

        TNode beta = NoNode;
        while (searching)
        {
            // Compute the smallest colour free at node v1

            I.Reset(v1);
            Q2 -> Init();
            while (I.Active(v1))
            {
                TArc a = I.Read(v1);
                Q2->Insert(a>>1,nodeColour[a>>1]);
            }

            TNode betaOld = beta;
            beta = NoNode;
            TArc c2 = NoNode;
            while (!Q2->Empty() && beta==NoNode)
            {
                TArc a2 = Q2->Delete();
                TArc c3 = nodeColour[a2];

                if (c2==NoNode && c3>0)
                {
                    if (betaOld!=0) beta = 0;
                    else if (c3>1) beta = 1;
                }

                if (c2!=NoNode && c2<c3-1)
                {
                    if (betaOld!=c2+1) beta = c2+1;
                    else if (c2<c3-2)  beta = c2+2;
                }
                c2 = c3;
            }

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"...Available colour is %lu",
                    static_cast<unsigned long>(beta));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            // Check if the colour beta is free at node u

            I.Reset(u);
            TArc aNext = NoArc;
            while (I.Active(u) && aNext==NoArc)
            {
                TArc a2 = I.Read(u);
                if (nodeColour[a2>>1]==beta) aNext = a2;
            }

            nodeColour[aCurrent>>1] = beta;

            if (aNext==NoArc)
            {
                searching = false;
                LogEntry(LOG_METH2,"...arc is coloured");
                M.Trace(*G,m);
                break;
            }

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"...Next arc is %lu",
                    static_cast<unsigned long>(aNext));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            nodeColour[aNext>>1] = NoNode;

            TNode v2 = EndNode(aNext);

            // Check if alpha is free at node v2

            I.Reset(v2);
            TArc aAux = NoArc;
            while (I.Active(v2) && aAux==NoArc)
            {
                TArc a2 = I.Read(v2);
                if (nodeColour[a2>>1]==alpha) aAux = a2;
            }

            if (aAux==NoArc ||
                G->NCKempeExchange(nodeColour,TNode(aAux>>1),TNode(aCurrent>>1)))
            {
                nodeColour[aNext>>1] = alpha;
                searching = false;
                LogEntry(LOG_METH2,"...Arc is coloured");
            }
            else
            {
                G->NCKempeExchange(nodeColour,TNode(aAux>>1),TNode(aCurrent>>1));

                v1 = v2;
                aCurrent = aNext;
            }

            M.Trace(*G,m);
        }
    }

    TArc chi = 0;
    for (TArc a=0;a<m;a++)
        if (chi<=nodeColour[a]) chi = nodeColour[a]+1;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu-edge colouring found",
            static_cast<unsigned long>(chi));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    M.SetUpperBound(chi);

    // Apply local search if a k = chi-1 colouring was requested

    if (CT.methLocal==LOCAL_OPTIMIZE && k<chi)
    {
        chi = G->NCLocalSearch();
    }

    // Fall back to enumeration if a k = chi-1 colouring was requested

    if (chi>k) chi = G->NodeColouring(k);

    Close(H);
    delete Q;
    delete Q2;

    for (TArc a=0;a<m;a++)
    {
        SetEdgeColour(2*a,nodeColour[a]);

        if (chi<=nodeColour[a]) chi = nodeColour[a]+1;
    }

    delete G;

    if (CT.logRes)
    {
        if (chi>0)
        {
            sprintf(CT.logBuffer,"...%lu-edge colouring found",
                static_cast<unsigned long>(chi));
        }
        else
        {
            sprintf(CT.logBuffer,"...No %lu-edge colouring found",
                static_cast<unsigned long>(k));
        }

        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return chi;
}


TNode abstractBiGraph::NodeColouring(TNode dummy) throw()
{
    moduleGuard M(ModColour,*this,"Computing minimum node colouring...");

    TNode* nodeColour = RawNodeColours();

    for (TNode v=0;v<n;++v)
    {
        if (v<n1 || m==0)
        {
            nodeColour[v] = 0;
        }
        else nodeColour[v] = 1;
    }

    if (m==0) return 1;

    return 2;
}
