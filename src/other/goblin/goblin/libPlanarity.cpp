
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libPlanarity.cpp
/// \brief  Fundamental methods for handling planar graphs

#include "abstractMixedGraph.h"
#include "staticStack.h"
#include "sparseGraph.h"
#include "binaryHeap.h"
#include "moduleGuard.h"


void abstractMixedGraph::RandomizeIncidenceOrder() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("RandomizeIncidenceOrder");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TArc* predArc = new TArc[2*m];

    for (TArc a=0;a<2*m;a++) predArc[a] = NoArc;

    for (TNode v=0; v<n;v++)
    {
        TArc a0 = First(v);
        TArc thisDeg = 0;

        if (a0==NoArc) continue;

        TArc a = a0;

        do
        {
            thisDeg++;
            a = Right(a,v);
        }
        while (a!=a0);

        a = a0;

        while (thisDeg>1)
        {
            TArc arcsToSkip = 1+CT.Rand(thisDeg);

            TArc a2 = a;

            while (arcsToSkip>0)
            {
                a2 = Right(a2,v);

                if (a2!=a0 && predArc[a2]==NoArc) arcsToSkip--;
            }

            predArc[a2] = a^1;
            a = a2;
            thisDeg--;
        }

        predArc[a0] = a^1;
    }

    X -> ReorderIncidences(predArc);
    SetExteriorArc(NoArc);

    delete[] predArc;
}


void abstractMixedGraph::IncidenceOrderFromDrawing() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("IncidenceOrderFromDrawing");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TArc* predArc = new TArc[2*m];
    binaryHeap<TArc,TFloat> Q(2*m,CT);

    TFloat minY = InfFloat;
    TArc exteriorArc = NoArc;

    for (TNode v=0; v<n;v++)
    {
        TArc a0 = First(v);

        if (a0==NoArc) continue;

        TArc a = a0;

        do
        {
            TNode w = PortNode(a);

            if (w==NoNode) w = EndNode(a);

            TFloat dx = C(w,0)-C(v,0);
            TFloat dy = C(w,1)-C(v,1);
            TFloat norm = sqrt(dx*dx+dy*dy);
            TFloat phi = 0;

            if (norm>0.0001) phi = acos(dy/norm);

            if (dx<0) phi *= -1;

            Q.Insert(a,phi);
            a = Right(a,v);
        }
        while (a!=a0);

        a0 = a = Q.Delete();

        if (C(v,1)<minY)
        {
            minY = C(v,1);
            exteriorArc = a0^1;
        }

        while (!(Q.Empty()))
        {
            TArc a2 = Q.Delete();
            predArc[a2] = a;
            a = a2;
        }

        predArc[a0] = a;
    }

    X -> ReorderIncidences(predArc,true);
    MarkExteriorFace(exteriorArc);

    delete[] predArc;
}


TNode abstractMixedGraph::ExtractEmbedding(TOptExtractEmbedding opt,void* arg)
    throw (ERRejected)
{
    switch (opt)
    {
        case PLANEXT_DEFAULT:
        {
            LogEntry(LOG_METH,"Extracting faces from incidence lists...");
            break;
        }
        case PLANEXT_GROW:
        {
            LogEntry(LOG_METH,"Checking for outerplanarity...");
            break;
        }
        case PLANEXT_DUAL:
        {
            LogEntry(LOG_METH,"Extracting dual incidence structure...");
            break;
        }
        case PLANEXT_CONNECT:
        {
            LogEntry(LOG_METH,"Augmenting to a connected planar graph...");
            break;
        }
    }


    bool preserveExteriorFace = false;
    bool preserveEmbedding = false;

    if (opt==PLANEXT_DUAL || opt==PLANEXT_DEFAULT)
    {
        if (ExteriorArc()!=NoArc)
        {
            preserveExteriorFace = true;
            if (face) preserveEmbedding = true;
        }
    }


    // Number of faces
    TNode k = 0;

    // Number of connected components
    TNode l = 0;

    // Vertex markers for the PLANEXT_CONNECT case
    TNode firstComp = NoNode;
    TNode prevComp = NoNode;

    // Queue the new edges in the PLANEXT_CONNECT case
    staticStack<TNode> Heads(n,CT);
    staticStack<TNode> Tails(n,CT);


    if (!preserveEmbedding)
    {
        OpenFold();

        // Number of cancelled arcs
        TNode c = 0;

        // Candidates for ExteriorArc() and the lenght of the incident region
        TArc maxRegion = NoArc;
        TNode maxLength = 0;

        // Cardinality of the inspected connected component
        TNode n0 = 0;

        // Assign connected components
        TNode* nodeColour = InitNodeColours();

        // Queue the unsearched nodes of the current component
        staticStack<TNode> Q(n,CT);

        THandle H = Investigate();
        investigator &I = Investigator(H);

        if (!face) face = new TNode[2*m];

        for (TArc i=0;i<2*m;i++) face[i] = NoNode;


        for (TNode r=0;r<n;r++)
        {
            if (nodeColour[r]!=NoNode) continue;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Component %lu:",static_cast<unsigned long>(l));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            OpenFold();

            // Search the connected component of r
            nodeColour[r] = l;
            Q.Insert(r);
            n0 = 1;

            while (!Q.Empty())
            {
                TNode u = Q.Peek();

                if (!I.Active(u))
                {
                    // Node u has been searched exhaustively
                    u = Q.Delete();
                    continue;
                }

                TArc a = I.Read(u);

                if (face[a]!=NoNode)
                {
                    // A face has been already assigned
                    continue;
                }

                TNode w = StartNode(a^1);

                if (w==NoNode)
                {
                    // Acknowledge the cancelled arc a
                    c++;
                    continue;
                }

                TArc faceLength = 1;
                TArc a2 = Right(a^1,w);
                face[a2] = k;
                TNode v = StartNode(a2^1);

                #if defined(_LOGGING_)

                THandle LH = NoHandle;

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Face %lu = (%lu [%lu] %lu",
                        static_cast<unsigned long>(k),
                        static_cast<unsigned long>(w),
                        static_cast<unsigned long>(a2),
                        static_cast<unsigned long>(v));
                    LH = LogStart(LOG_METH2,CT.logBuffer);
                }

                #endif

                while (a2!=a)
                {
                    if (nodeColour[w]==NoNode)
                    {
                        // Search w after this face has been handled
                        nodeColour[w] = l;
                        n0++;
                        Q.Insert(w);
                    }

                    faceLength++;
                    a2 = Right(a2^1,v);
                    w = v;
                    v = StartNode(a2^1);
                    face[a2] = k;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer," [%lu] %lu",
                            static_cast<unsigned long>(a2),static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogEnd(LH,")");

                #endif

                if (faceLength>maxLength)
                {
                    maxRegion = a;
                    maxLength = faceLength;
                }

                // Skip to the next region
                k++;
            }

            switch (opt)
            {
                case PLANEXT_GROW:
                {
                    CloseFold();

                    GrowExteriorFace(maxRegion);

                    maxLength = 0;

                    break;
                }
                case PLANEXT_DUAL:
                case PLANEXT_CONNECT:
                case PLANEXT_DEFAULT:
                {
                    CloseFold();

                    if (!preserveExteriorFace)
                    {
                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,
                                "...Exterior face %lu has %lu nodes",
                                static_cast<unsigned long>(face[maxRegion]),
                                static_cast<unsigned long>(maxLength));
                            LogEntry(LOG_RES,CT.logBuffer);
                        }

                        MarkExteriorFace(maxRegion);
                    }

                    maxLength = 0;

                    if (opt==PLANEXT_CONNECT)
                    {
                        if (prevComp!=NoNode)
                        {
                            Tails.Insert(prevComp);
                            Heads.Insert(StartNode(maxRegion));
                        }
                        else firstComp = StartNode(maxRegion);

                        prevComp = EndNode(maxRegion);
                    }

                    break;
                }
            }

            // Skip to the next connected component
            l++;
        }

        Close(H);
        CloseFold();

        #if defined(_FAILSAVE_)

        if (m-(c/2)-n+2*l!=k)
        {
            delete[] face;
            face = NULL;
            SetExteriorArc(NoArc);
            LogEntry(LOG_RES,"...Graph is not properly embedded\n");
            return NoNode;
        }

        #endif

        sprintf(CT.logBuffer,"...Graph has %lu faces and %lu connected components",
            static_cast<unsigned long>(k-l+1),static_cast<unsigned long>(l));
        LogEntry(LOG_RES,CT.logBuffer);
    }
    else
    {
        k = ND();

        TNode l = 0;
        TArc exteriorArc = ExteriorArc();
        TArc a = exteriorArc;

        do
        {
            TNode v = EndNode(a);
            a = Right(a^1,v);
            l++;
        }
        while (a!=exteriorArc);

        sprintf(CT.logBuffer,"...Graph has %lu faces",static_cast<unsigned long>(ND()));
        LogEntry(LOG_RES,CT.logBuffer);
        sprintf(CT.logBuffer,"...Exterior face %lu has %lu nodes",
            static_cast<unsigned long>(face[exteriorArc]),static_cast<unsigned long>(l));
        LogEntry(LOG_RES,CT.logBuffer);
    }


    switch (opt)
    {
        case PLANEXT_DEFAULT:
        {
            if (!arg) break;

            for (TArc a=0;a<2*m;a++) ((TNode*)(arg))[a] = face[a];
            break;
        }
        case PLANEXT_CONNECT:
        {
            if (l==1) break;

            #if defined(_FAILSAVE_)

            if (!IsSparse() || !Representation())
                NoSparseRepresentation("ExtractEmbedding");

            #endif

            sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());
            X -> SetCapacity(n,m+l-1);

            InsertArc(prevComp,firstComp);

            while (!Heads.Empty())
            {
                InsertArc(Tails.Delete(),Heads.Delete());
            }

            // The face[] indices are corrupted for all
            // faces merged into the exterior region
            if (face)
            {
                delete[] face;
                face = NULL;
            }

            break;
        }
        case PLANEXT_GROW:
        {
            break;
        }
        case PLANEXT_DUAL:
        {
            if (arg)
            {
                if (l>1)
                {
                    Error(ERR_REJECTED,"GrowExteriorFace",
                        "Dualization applies to connected graphs only");
                }

                abstractMixedGraph* G = reinterpret_cast<abstractMixedGraph*>(arg);
                TFloat* sumX = new TFloat[ND()];
                TFloat* sumY = new TFloat[ND()];
                TArc* nAdj   = new TArc[ND()];

                for (TNode f=0;f<ND();f++)
                {
                    sumX[f] = sumY[f] = 0;
                    nAdj[f] = 0;
                }

                for (TArc a=0;a<2*m;a++)
                {
                    sumX[face[a]] += C(StartNode(a),0);
                    sumY[face[a]] += C(StartNode(a),1);
                    nAdj[face[a]]++;
                }

                for (TNode f=0;f<G->N();f++)
                {
                    if (f>=G->N()) G->InsertNode();

                    G -> SetC(f,0,sumX[f]/nAdj[f]);
                    G -> SetC(f,1,sumY[f]/nAdj[f]);
                }

                delete[] sumX;
                delete[] sumY;
                delete[] nAdj;
            }

            break;
        }
    }

    return k;
}


void abstractMixedGraph::GrowExteriorFace() throw(ERRejected)
{
    if (!IsSparse())
        Error(ERR_REJECTED,"GrowExteriorFace",
            "Method applies to sparse graphs only");

    if (ExtractEmbedding(PLANEXT_GROW)==NoNode)
    {
        Error(ERR_REJECTED,"GrowExteriorFace","Graph is not embedded");
    }
}


static void ToggleEdgeColour(abstractMixedGraph& G,TArc* pred,TArc a,TArc c) throw()
{
    // Helper procedure for the construction of forbidden subgraphs in the
    // outerplanarity code. It must be called for the arcs in a special DFS like order

    TNode u = G.StartNode(a);
    TNode v = G.EndNode(a);

    if (pred[v]==NoArc)
    {
        G.SetEdgeColour(a,c);
        pred[u] = a^1;
    }
    else
    {
        // A coloured cycle is completed by the arc a.
        // Reduce the set of coloured arcs to a path by cancelling this cycle
        TNode w = v;

        do
        {
            TArc a2 = pred[w];
            G.SetEdgeColour(a2,NoArc);
            pred[w] = NoArc;
            w = G.StartNode(a2);
        }
        while (pred[w]!=NoArc);
    }
}

TNode abstractMixedGraph::GrowExteriorFace(TArc exteriorArc) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (exteriorArc>=2*m) NoSuchArc("GrowExteriorFace",exteriorArc);

    if (!face)
        Error(ERR_REJECTED,"GrowExteriorFace","Missing dual incidences");

    if (!IsSparse() || !Representation()) NoSparseRepresentation("GrowExteriorFace");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    moduleGuard M(ModPlanarity,*this,"Growing exterior face...");

    bool outerplanar = true;
    TNode fExterior = face[exteriorArc];

    MarkExteriorFace(exteriorArc);

    TArc a = exteriorArc;
    bool blocked = true;

    // Remember
    bool* searched = new bool[n];
    for (TNode v=0;v<n;v++) searched[v] = false;

    bool* visited = new bool[m];
    for (TArc i=0;i<m;i++) visited[i] = false;

    // Queue the searched exterior nodes
    staticStack<TNode> Q(n,CT);

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"(%lu",static_cast<unsigned long>(EndNode(a)));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    while (a!=exteriorArc || blocked)
    {
        TNode x = EndNode(a);
        TArc aNext = Right(a^1,x);
        TNode y = EndNode(aNext);

        Q.Insert(x,Q.INSERT_NO_THROW);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer," [%lu] %lu",
                static_cast<unsigned long>(aNext),static_cast<unsigned long>(y));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        if (!searched[x])
        {
            if (MoveInteriorBlocks(x,visited))
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogEnd(LH,"...");

                #endif

                LogEntry(LOG_METH,"...Blocks swapped to the exterior");

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"...%lu",static_cast<unsigned long>(x));
                    LH = LogStart(LOG_METH2,CT.logBuffer);
                }

                #endif
            }

            searched[x] = true;
        }

        if (x==y || face[aNext]==face[aNext^1])
        {
            // Skip exterior loops and bridges
            a = aNext;
            blocked = false;
            continue;
        }

        // Skip interior loops at x and parallels of aNext
        TArc aFirst = aNext;
        TNode z = y;

        do
        {
            aFirst = Right(aFirst,x);
            z = EndNode(aFirst);
        }
        while ((z==y || z==x) && aFirst!=(a^1));

        if (face[aNext]==face[aFirst^1])
        {
            // In the 2-block of a and aNext, node x has degree 2
            // (up to loops and the parallels of aNext). Nothing to do!
            a = aNext;
            blocked = false;
            continue;
        }

        // Follow up the face of aFirst^1 in clockwise
        // direction until an exterior node is reached.
        TArc aFirst2 = aFirst;
        TArc aFinal = aFirst;

        do
        {
            z = EndNode(aFinal);

            while (!ExteriorNode(z,fExterior))
            {
                aFinal = Left(aFinal^1);
                z = EndNode(aFinal);
            }

            if (z==x) aFinal = aFirst2 = aFinal^1;
        }
        while (z==x);

        if (z!=y)
        {
            if (ExteriorNode(EndNode(aFirst),fExterior))
            {
                // aFirst^1 and aNext are consecutive on the exterior of the respective 2-block
                a = aNext;
                continue;
            }

            // The arc aNext cannot be swapped to the interior.

            // Follow up the face of aFirst in counter-clockwise
            // direction until an exterior node w is reached
            TNode w = EndNode(aFirst);
            TArc aHelp = aFirst;

            while (!ExteriorNode(w,fExterior))
            {
                aHelp = Right(aHelp^1,w);
                w = EndNode(aHelp);
            }

            if (z!=w)
            {
                // Segment has at least 3 contact nodes with
                // the exterior face of this 2-block

                #if defined(_LOGGING_)

                if (CT.logMeth>1 && CT.logRes) LogEnd(LH,"...");

                #endif

                LogEntry(LOG_RES,"...K_4 minor found");

                #if defined(_LOGGING_)

                if (CT.logMeth>1 && CT.logRes)
                {
                    sprintf(CT.logBuffer,"...%lu",static_cast<unsigned long>(y));
                    LH = LogStart(LOG_METH2,CT.logBuffer);
                }

                #endif

                if (outerplanar)
                {
                    // Export a subdivision of the K_4 by the edge colours
                    InitEdgeColours(NoArc);

                    // In order to obtain paths for the colour classes
                    // (which corrrespond to the edges of the K_4),
                    // equally coloured cycles are canceled on the fly.
                    // This requires to memorize a DFS search path
                    TArc* pred = InitPredecessors();

                    aHelp ^= 1;
                    TNode u = EndNode(aHelp);
                    ToggleEdgeColour(*this,pred,aHelp,0);

                    while (u!=x)
                    {
                        aHelp = Left(aHelp^1);
                        ToggleEdgeColour(*this,pred,aHelp,0);
                        u = EndNode(aHelp);
                    }

                    // Follow up the face of aFirst^1 in clockwise
                    // direction until an exterior node is reached
                    TArc aFirst2 = aFirst;
                    TArc aFinal = aFirst;
                    TArc aSep = NoArc;

                    do
                    {
                        z = EndNode(aFinal);

                        while (true)
                        {
                            if (EdgeColour(aFinal)!=NoArc) aSep = aFinal;

                            ToggleEdgeColour(*this,pred,aFinal,0);

                            if (ExteriorNode(z,fExterior)) break;

                            aFinal = Left(aFinal^1);
                            z = EndNode(aFinal);
                        }

                        if (z==x) aFinal = aFirst2 = aFinal^1;
                    }
                    while (z==x);

                    aFinal = aFirst2;
                    TArc c = 1;
                    pred = InitPredecessors();

                    do
                    {
                        z = EndNode(aFinal);

                        while (true)
                        {
                            ToggleEdgeColour(*this,pred,aFinal,c);

                            if (aFinal==aSep) c = 2;

                            if (ExteriorNode(z,fExterior)) break;

                            aFinal = Left(aFinal^1);
                            z = EndNode(aFinal);
                        }

                        if (z==x) aFinal = aFirst2 = aFinal^1;
                    }
                    while (z==x);


                    // Follow the exterior face of this 2-block and assign
                    // colours for the three further edges of the K_4 minor
                    pred = InitPredecessors();
                    aHelp = aNext;
                    c = 3;

                    do
                    {
                        ToggleEdgeColour(*this,pred,aHelp,c);
                        u = EndNode(aHelp);
                        aHelp = aHelp^1;

                        if (u==w) c = 4;

                        if (u==z)
                        {
                            c = 5;
                            pred[x] = NoArc;
                        }

                        while (face[aHelp]!=fExterior) aHelp = Left(aHelp);
                    }
                    while (u!=x);
                }

                outerplanar = false;
            }
            else if (z!=y)
            {
                aHelp = aFirst2;
                while (face[aHelp^1]!=fExterior) aHelp = Right(aHelp,x);

                if (EndNode(aHelp)!=z)
                {
                    // Segment has 2 non-adjacent contact nodes

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1 && CT.logRes) LogEnd(LH,"...");

                    #endif

                    LogEntry(LOG_RES,"...K_2,3 minor found");

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1 && CT.logRes)
                    {
                        sprintf(CT.logBuffer,"...%lu",static_cast<unsigned long>(y));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    if (outerplanar)
                    {
                        // Export a subdivision of the K_2_3 by the edge colours
                        InitEdgeColours(NoArc);

                        // In order to obtain paths for the colour classes
                        // (which corrrespond to the edges of the K_2_3),
                        // equally coloured cycles are canceled on the fly.
                        // This requires to memorize a DFS search path
                        TArc* pred = InitPredecessors();

                        w = EndNode(aFirst);
                        aHelp = aFirst;
                        SetEdgeColour(aHelp,0);

                        while (w!=z)
                        {
                            aHelp = Right(aHelp^1,w);
                            ToggleEdgeColour(*this,pred,aHelp,1);
                            w = EndNode(aHelp);
                        }

                        aHelp = aNext;
                        TArc c = 3;

                        do
                        {
                            ToggleEdgeColour(*this,pred,aHelp,c);
                            w = EndNode(aHelp);
                            aHelp = aHelp^1;

                            if (w==z)
                            {
                                c = 5;
                                pred[x] = NoArc;
                            }

                            while (face[aHelp]!=fExterior)
                                aHelp = Left(aHelp);
                        }
                        while (aHelp!=a);

                        SetEdgeColour(a,4);
                        SetEdgeColour(aNext,2);
                    }

                    outerplanar = false;
                }
            }

            a = aNext;
            blocked = false;

            continue;
        }

        // aNext can be routed internally

        // Update face[] indices. Reuse face index
        TNode fNext = face[Left(aFirst)^1];
        TNode fFirst = face[aFirst^1];

        // The new face incident with aNext
        face[aNext] = fNext;

        TArc a4 = aFirst2;
        do
        {
            face[a4^1] = fNext;
            a4 = Left(a4^1);
        }
        while (StartNode(a4)!=y);

        // The new exterior segment
        a4 = aFirst;
        TNode w = EndNode(a4);
        face[a4] = fExterior;

        while (w!=y)
        {
            a4 = Right(a4^1,w);
            face[a4] = fExterior;
            w = EndNode(a4);
        }

        //
        TArc aHelp = Right(a4^1,y);
        a4 = aHelp;
        w = EndNode(a4);
        face[a4] = fFirst;

        while (w!=x)
        {
            a4 = Right(a4^1,w);
            face[a4] = fFirst;
            w = EndNode(a4);
        }

        // Update incidence lists
        X -> SetRight(aFirst2,aNext,Left(aFirst));
        X -> SetRight(Left(aFinal^1),aHelp,aNext^1);


        if (exteriorArc==aNext)
        {
            exteriorArc = a;
            blocked = true;
        }

        MarkExteriorFace(exteriorArc);

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH,"...");

        #endif

        LogEntry(LOG_METH,"...Cut arcs swapped to the interior");

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...%lu",static_cast<unsigned long>(x));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH,")");

    #endif

    delete[] searched;

    SetExteriorArc(exteriorArc);

    if (!outerplanar)
    {
        sprintf(CT.logBuffer,"...Exterior region has %lu nodes",
            static_cast<unsigned long>(Q.Cardinality()));
    }
    else if (Q.Cardinality()==n)
    {
        sprintf(CT.logBuffer,"...Graph is outerplanar");
    }
    else
    {
        sprintf(CT.logBuffer,"...Component is outerplanar");
    }

    M.Shutdown(LOG_RES,CT.logBuffer);

    return Q.Cardinality();
}


bool abstractMixedGraph::MoveInteriorBlocks(TNode x,bool* _visited)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (x>=n) NoSuchNode("MoveInteriorBlocks",x);

    if (!face) Error(ERR_REJECTED,"MoveInteriorBlocks","Missing dual incidences");

    if (!IsSparse() || !Representation()) NoSparseRepresentation("MoveInteriorBlocks");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode moved = 0;
    TNode fExterior = face[ExteriorArc()];
    TArc aFirst = Right(First(x),x);
    TArc a = aFirst;

    bool* visited = _visited;
    if (!visited)
    {
        visited = new bool[m];
        for (TArc i=0;i<m;i++) visited[i] = false;
    }

    if (aFirst==NoArc) return false;

    #if defined(_FAILSAVE_)

    if (!ExteriorNode(x))
    {
        sprintf(CT.logBuffer,"Not an exterior node: %lu",static_cast<unsigned long>(x));
        Error(ERR_REJECTED,"MoveInteriorBlocks",CT.logBuffer);
    }

    #endif

    do
    {
        TArc aNext = Right(a,x);
        TNode y = EndNode(aNext);

        if (x==y || face[aNext]==fExterior || face[aNext^1]==fExterior)
        {
            // Skip loops and exterior arcs
            a = aNext;
            continue;
        }

        // Traverse the face of aNext until an exterior node is reached
        TArc aFinal = aNext;
        TNode z = y;

        while (!ExteriorNode(z))
        {
            visited[aFinal>>1] = true;
            aFinal = Right(aFinal^1,z);
            z = EndNode(aFinal);
        }

        if (z!=x || (visited[aFinal>>1] && aFinal!=(aNext^1)))
        {
            // The arc aNext cannot be swapped to the exterior.
            a = aNext;
            visited[aFinal>>1] = true;
            continue;
        }

        visited[aFinal>>1] = true;

        // The interior segment encloses a 2-block
        // which can be swapped to the exterior

        // Update incidence lists. The block is inserted right-hand of First(x)
        // into the exterior face. Note that the block will be investigated
        // again to detect nested blocks.
        X -> SetRight(First(x),aNext,aFinal^1);

        // Update face[] indices. Mark the segment exterior
        TArc a4 = aNext;
        TNode w = EndNode(a4);
        face[a4] = fExterior;

        while (w!=x)
        {
            X -> SetFirst(w,a4^1);
            a4 = Right(a4^1,w);
            face[a4] = fExterior;
            w = EndNode(a4);
        }

        moved++;
    }
    while (a!=aFirst);

    if (!_visited) delete[] visited;

    if (CT.logMeth>1 && moved>0 && !_visited)
    {
        sprintf(CT.logBuffer,"...%lu blocks have been moved",
            static_cast<unsigned long>(moved));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return moved>0;
}


void abstractMixedGraph::PlanarConnectivityAugmentation() throw (ERRejected)
{
    if (ExtractEmbedding(PLANEXT_CONNECT)==NoNode)
    {
        Error(ERR_REJECTED,"PlanarConnectivityAugmentation","Graph is not embedded");
    }
}


void abstractMixedGraph::PlanarBiconnectivityAugmentation() throw (ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation())
        NoSparseRepresentation("PlanarBiconnectivityAugmentation");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());
    X -> SetCapacity(n,3*n-6);

    moduleGuard M(ModPlanarity,*this,"Augmenting to a biconnected planar graph...");

    // Biconnectivity augmentation
    TArc mOrig = m;
    TArc* predArc2 = new TArc[2*mOrig];

    // Collect all nodes of the currently inspected face
    staticStack<TNode> Q(n,CT);

    TArc savedExterior = ExteriorArc();

    for (TArc a=0;a<2*mOrig;a++) predArc2[a] = NoArc;

    for (TArc a=0;a<2*mOrig;a++)
    {
        if (predArc2[a]!=NoArc) continue;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Tracing face of arc %lu...",
                static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
            OpenFold();
        }

        #endif

        TArc a2 = a;
        bool prevCutNode = false;
        TNode threadNode = NoNode;

        do
        {
            TNode v = EndNode(a2);
            TArc a3 = Right(a2^1,v);

            // By this, an arc with end node v which is potentially inserted,
            // is inserted right-hand of a2^1, thus preserving the embedding
            X -> SetFirst(v,a2^1);

            // Avoid to inspect this arc again
            predArc2[a3] = a2;

            bool cutNode = false;

            if (!Q.IsMember(v)) Q.Insert(v);
            else
            {
                // Since v has been traversed before, it is a cut node
                // with a block enclosed in the currently inspected face.
                cutNode = true;
            }

            if (a3==(a2^1))
            {
                // v is a leave (has degree 1)

                if (threadNode==NoNode) threadNode = v;
                else
                {
                    InsertArc(threadNode,v);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Adding arc (%lu,%lu)",
                            static_cast<unsigned long>(threadNode),static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    threadNode = NoNode;
                }
            }
            else if (!cutNode && prevCutNode)
            {
                InsertArc(threadNode,v);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Adding arc (%lu,%lu)",
                        static_cast<unsigned long>(threadNode),static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                threadNode = NoNode;
            }
            else if (cutNode && threadNode==NoNode)
            {
                threadNode = StartNode(a2);
            }

            a2 = a3;
            prevCutNode = cutNode;
        }
        while (a2!=a);

        if (threadNode!=NoNode)
        {
            TNode v = EndNode(a2);
            X -> SetFirst(v,a2^1);

            if (threadNode==StartNode(a2))
            {
                v = EndNode(Right(a2^1,v));
                X -> SetFirst(v,Right(a2^1,v)^1);
            }

            InsertArc(threadNode,v);

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Adding arc (%lu,%lu)",
                    static_cast<unsigned long>(threadNode),static_cast<unsigned long>(v));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        Q.Init();

        #if defined(_LOGGING_)

        if (CT.logMeth>1) CloseFold();

        #endif
    }

    delete[] predArc2;

    if (savedExterior!=NoArc) MarkExteriorFace(savedExterior);

    X -> SetCapacity(n,m);
}


void abstractMixedGraph::Triangulation() throw (ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("Triangulation");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());
    X -> SetCapacity(n,3*n-6);

    moduleGuard M(ModPlanarity,*this,"Triangulating the graph...");

    TArc mOrig = m;
    TArc savedExterior = ExteriorArc();

    // predArc[a] stores the predecessor arc in the directed cycle
    // enclosing the face left hand of the arc a
    TArc* predArc = new TArc[2*mOrig];

    for (TArc a=0;a<2*mOrig;a++) predArc[a] = NoArc;

    for (TArc a=0;a<2*mOrig;a++)
    {
        // Proceed only if the face left hand of a has not been visited before
        if (predArc[a]!=NoArc) continue;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Tracing face of arc %lu...",static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
            OpenFold();
        }

        #endif

        MarkExteriorFace(a);

        // Traverse the directed cycle enclosing the face left hand of a
        TArc a2 = a;
        TArc l = 0;

        do
        {
            TArc a3 = Right(a2^1,EndNode(a2));
            predArc[a3] = a2;
            a2 = a3;
            l++;
        }
        while (a2!=a);


        // Search for a chord which can be inserted legally
        // (without introducing parallel arcs)
        TArc a3 = predArc[predArc[a]];

        // ...but only if the face left hand of a is not a triangle
        while (l>3)
        {
            TNode u = EndNode(a3);
            TNode v = EndNode(a2);
            a2 = Right(a2^1,v);

            if (a2==a3) break;

            if (Adjacency(u,v)!=NoArc || Adjacency(v,u)!=NoArc)
            {
                a2 = predArc[a2];
                v = StartNode(a2);
            }
            else
            {
                TArc aNew = InsertArc(u,v);
                X -> SetFirst(u,a3^1);
                X -> SetFirst(v,2*aNew+1);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Adding arc (%lu,%lu)",
                        static_cast<unsigned long>(u),static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }

            a3 = predArc[a3];
            u = EndNode(a3);

            if (a2==a3) break;

            if (Adjacency(u,v)!=NoArc || Adjacency(v,u)!=NoArc)
            {
                a3 = Right(a3^1,u);
                u = EndNode(a3);
            }
            else
            {
                TArc aNew = InsertArc(u,v);
                X -> SetFirst(u,a3^1);
                X -> SetFirst(v,2*aNew+1);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Adding arc (%lu,%lu)",
                        static_cast<unsigned long>(u),static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) CloseFold();

        #endif
    }

    delete[] predArc;

    if (savedExterior!=NoArc) MarkExteriorFace(savedExterior);
}


void abstractMixedGraph::MarkExteriorFace(TArc exteriorArc) throw (ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (exteriorArc>=2*m && exteriorArc!=NoArc) NoSuchArc("MarkExteriorFace",exteriorArc);

    if (!IsSparse() || !Representation()) NoSparseRepresentation("MarkExteriorFace");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());


    SetExteriorArc(exteriorArc);

    if (exteriorArc==NoArc) return;

    LogEntry(LOG_MEM,"Initializing exterior face...");

    TArc a = exteriorArc;
    do
    {
        TNode v = EndNode(a);
        X -> SetFirst(v,a^1);
        a = Right(a^1,v);
    }
    while (a!=exteriorArc);
}


TNode abstractMixedGraph::LMCOrderedPartition(
    TArc* cLeft,TArc* cRight,TNode* vRight)
    throw (ERRejected)
{
    moduleGuard M(ModLMCOrder,*this,"Computing LMC ordered partition...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(3*n,n);

    #endif


    // If possible, start with a known basis arc
    TArc aBasis = ExteriorArc();

    // Node deletions are performed on a copy
    sparseGraph G(*this,OPT_CLONE);
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    // Delete all loops and parallel arcs from G
    for (TArc a=0;a<m;a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);
        TArc a2 = G.Right(2*a,u);

        if (u==v && a2!=2*a+1)
        {
            if (aBasis==2*a) aBasis = a2;

            if (a2!=2*a+1) GR -> CancelArc(2*a);
        }
        else if (G.EndNode(a2)==v)
        {
            if (aBasis==2*a) aBasis = a2;

            GR -> CancelArc(2*a);
        }
    }


    // Determine the number of faces (dual nodes),
    // and the left hand face for every graph arc.

    TNode* face = new TNode[2*m];
    TNode nd = G.ExtractEmbedding(PLANEXT_DEFAULT,face);

    if (nd==NoNode)
    {
        // Graph is non-planar or not embedded yet
        delete[] face;
        Error(ERR_REJECTED,"LMCOrderedPartition","Graph is not embedded");
    }
    else if (nd!=m-n+2)
    {
        delete[] face;
        Error(ERR_REJECTED,"LMCOrderedPartition","Graph is disconnected");
    }

    if (aBasis==NoArc) aBasis = G.ExteriorArc();

    SetExteriorArc(aBasis);
    G.MarkExteriorFace(aBasis);


    // The extremal components and the unbounded face
    TNode v1 = G.EndNode(aBasis);
    TNode v2 = G.StartNode(aBasis);
    TNode fOut = face[aBasis];
    TNode fBasis = face[aBasis^1];

    // Choose the final node vn half the way on the exterior face
    TNode vn = G.EndNode(Right(aBasis^1,v1));
    TNode v = vn;
    do
    {
        v = G.EndNode(G.Right(G.First(v),v));

        if (v!=v2)
        {
            vn = G.EndNode(G.Right(G.First(vn),vn));
            v = G.EndNode(G.Right(G.First(v),v));
        }
    }
    while (v!=v2);


    // A node can be deleted only if it has been visited before
    bool* visited = new bool[n];
    for (v=0;v<n;v++) visited[v] = false;

    // A node can be deleted only if it is not in a separation pair.
    // fSep[] is maintained for exterior nodes only and = 0 otherwise
    TNode* fSep = new TNode[n];
    for (v=0;v<n;v++) fSep[v] = 0;

    #if defined(_TRACING_)

    if (CT.traceLevel>1) G.InitNodeColours(0);

    #endif

    // Number of components
    TNode k = 0;


    // Number of outer nodes / arcs incident with a face
    TNode* vOut = new TNode[nd];
    TArc*  aOut = new TArc[nd];

    // In which iteration has aOut[f] or vOut[f] changed the last time?
    TNode* prevChange = new TNode[nd];

    for (TNode f=0;f<nd;f++)
    {
        vOut[f] = aOut[f] = 0;
        prevChange[f] = NoNode;
    }


    // Compute vOut and aOut for the original graph

    LogEntry(LOG_METH,"Computing face incidence counters...");
    OpenFold();

    TArc a = aBasis;
    v = v2;

    do
    {
        aOut[face[a^1]]++;

        TArc a2 = G.Right(a,v);
        do
        {
            vOut[face[a2]]++;
            a2 = G.Right(a2,v);
        }
        while (a2!=a);

        // Traverse the unbounded face in clockwise order
        v = G.EndNode(a);
        a = G.Right(a^1,v);
    }
    while (a!=aBasis);


    // Partial 3-connectivity check: If this check is passed successfully,
    // then indeed fSep[v]=0 holds for every node v.

    bool connected = true;

    for (TNode f=0;f<nd;f++)
    {
        if (f==fOut) continue;

        // f shares a separation pair with the exterior face
        if (vOut[f]>aOut[f]+1 || aOut[f]>1) connected = false;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"vOut[%lu] = %lu, aOut[%lu] = %lu",
                static_cast<unsigned long>(f),
                static_cast<unsigned long>(vOut[f]),
                static_cast<unsigned long>(f),
                static_cast<unsigned long>(aOut[f]));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif
    }

    CloseFold();

    M.Trace(G,n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif


    // Candidate queue for the nodes which can be deleted currently
    staticStack<TNode> nodeQueue(n,CT);

    // Candidate queue for the faces which can be deleted currently.
    // Store the arc which connects the left contact node and the first
    // node of the later component
    staticStack<TArc> segmentQueue(2*m,CT);

    // Keep all faces which have become a separating face in an iteration
    staticStack<TArc> newSeparatingFaces(2*m,CT);


    // The component generated first consists of vn only
    visited[vn] = true;
    nodeQueue.Insert(vn);


    // Loop until only the basis arc is left or until it turns out that
    // the graph is not 3-connected

    // The left most and the right most contact node of the current component
    TNode cl = NoNode;
    TNode cr = NoNode;

    // The node / region which is deleted in the current iteration
    TNode vDel = NoNode;
    TNode fDel = NoNode;


    LogEntry(LOG_METH,"Deleting nodes and faces...");
    OpenFold();

    while (connected && G.Right(aBasis^1,v1)!=(aBasis^1))
    {
        // Construct the component to delete

        if (!nodeQueue.Empty())
        {
            vDel = nodeQueue.Delete();

            // Check if vDel is really admissible for deletion
            if (   fSep[vDel]>0
                || G.First(vDel)==NoArc
                || face[G.First(vDel)^1]!=fOut
               )
            {
                continue;
            }

            vRight[vDel] = NoNode;
            cLeft[k] = G.First(vDel)^1;
            cRight[k] = G.Right(G.First(vDel),vDel);

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Component %lu (Node): %lu",
                    static_cast<unsigned long>(k),static_cast<unsigned long>(vDel));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            cl = G.StartNode(cLeft[k]);
            cr = G.EndNode(cRight[k]);

            // Check for degree 1 nodes
            if (cr==cl)
            {
                connected = false;
                continue;
            }

            // Mark all contact nodes as visited
            TArc a = cRight[k];
            do
            {
                TNode w = G.EndNode(a);
                visited[w] = true;

                if (w!=v1 && w!=v2)
                    nodeQueue.Insert(w,nodeQueue.INSERT_NO_THROW);

                a = G.Right(a,vDel);
            }
            while (a!=cRight[k]);

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif
        }
        else if (!segmentQueue.Empty())
        {
            // This is the critical part in the O(n) time complexity. Arc are
            // added to the segmentQueue only once, namely when the left hand
            // face becomes exterior. In what follows, the arcs read from
            // segmentQueue are either rejected in constant time, or the right
            // hand side face is merged with the exterior face. In the latter
            // case, the segment traversed back and forth, is canceled and
            // hence cannot be considered again.

            TArc aDel = segmentQueue.Delete();

            if (vOut[face[aDel^1]]==NoNode)
            {
                // Face has been already canceled
                continue;
            }

            vDel = G.EndNode(aDel);
            fDel = face[aDel^1];

            if (aOut[fBasis]==vOut[fBasis])
            {
                // Final iteration
                aDel = G.Right(aBasis^1,v1);
                vDel = G.EndNode(aDel);
                fDel = fBasis;
            }
            else
            {
                if (face[aDel^1]==fBasis)
                {
                    // The face with the basis arc must not be deleted before the final iteration
                    continue;
                }

                // Check if fDel is really admissible for deletion
                if (vOut[fDel]>aOut[fDel]+1)
                {
                    // fDel is a separating face
                    continue;
                }

                if (aOut[fDel]<2)
                {
                    // Must delete at least one node with its two adjacent edges
                    continue;
                }

                while (G.Right(G.Right(aDel,G.StartNode(aDel)),G.StartNode(aDel))==aDel)
                {
                    // The queued arc is not the first outer arc of this face in clockwise order
                    vDel = G.StartNode(aDel);
                    aDel = Right(aDel,vDel)^1;
                }
            }

            cLeft[k] = aDel;
            TNode vPrev = NoNode;

            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Component %lu (Path): %lu",
                    static_cast<unsigned long>(k),static_cast<unsigned long>(vDel));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }

            #endif

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif

            while (face[G.Right(aDel^1,vDel)^1]==fDel)
            {
                // Check for degree 2 nodes
                if (!visited[vDel]) connected = false;

                aDel = G.Right(aDel^1,vDel);
                vPrev = vDel;
                vDel = G.EndNode(aDel);

                if (vDel==v2) break; // Only needed in the final iteration

                if (face[G.Right(aDel^1,vDel)^1]==fDel)
                {
                    vRight[vPrev] = vDel;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(vDel));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif

                    #if defined(_PROGRESS_)

                    M.ProgressStep(1);

                    #endif
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) LogEnd(LH);

            #endif

            if (!connected) continue;

            vRight[vPrev] = NoNode;
            cRight[k] = aDel;

            cl = G.StartNode(cLeft[k]);
            cr = G.EndNode(cRight[k]);


            // fDel contains a separation pair (the contact nodes cl and cr).
            // Traverse the new part of the exterior face and update fSep[]

            fSep[cr]--;

            #if defined(_TRACING_)

            if (CT.traceLevel>1) G.SetNodeColour(cr,fSep[cr]);

            #endif

            TArc a = cLeft[k]^1;
            TNode v = cl;
            while (v!=cr)
            {
                fSep[v]--;

                #if defined(_TRACING_)

                if (CT.traceLevel>1) G.SetNodeColour(v,fSep[v]);

                #endif

                a = G.Right(a^1,v);
                v = G.EndNode(a);
            }

            // Mark all contact nodes as visited
            visited[cl] = visited[cr] = true;

            // cl and cr have been visited and may become feasible for
            // deletion. The other nodes which are incident with the deleted
            // face have not been visited yet!

            if (cl!=v1 && fSep[cl]==0)
                nodeQueue.Insert(cl,nodeQueue.INSERT_NO_THROW);

            if (cr!=v2 && fSep[cr]==0)
                nodeQueue.Insert(cr,nodeQueue.INSERT_NO_THROW);
        }
        else
        {
            connected = false;
            continue;
        }


        // Cancel component from G instead of deleting it (preserve indices).

        TNode vNext = G.EndNode(cLeft[k]);
        GR -> CancelNode(vNext);

        while (vRight[vNext]!=NoNode)
        {
            vNext = vRight[vNext];
            GR -> CancelNode(vNext);
        }


        // Start updating vOut[], aOut[], fSep[] and collect candidates for segmentQueue

        TArc a = G.Right(G.First(cl),cl);
        TNode u = cl;
        TNode v = G.EndNode(a);
        TNode f = face[a^1];

        // Very special case: cl and cr, but no other nodes and arcs of f
        // were exterior before this iteration

        if (vOut[f]==2 && aOut[f]==0 && v==cr)
        {
            // The face f is no longer a separating face.
            // Adjust fSep[] for all nodes incident with f

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Not a separator face: %lu",
                    static_cast<unsigned long>(f));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            TArc a1 = a^1;
            do
            {
                TNode w = G.EndNode(a1);
                fSep[w]--;

                #if defined(_TRACING_)

                if (CT.traceLevel>1) G.SetNodeColour(w,fSep[w]);

                #endif

                a1 = G.Right(a1^1,w);
            }
            while (a1!=(a^1));

            // cl and cr have been visited and may become feasible for
            // deletion. The other nodes which are incident with the face f
            // are interior and have not been visited yet!

            if (cl!=v1 && fSep[cl]==0)
                nodeQueue.Insert(cl,nodeQueue.INSERT_NO_THROW);

            if (cr!=v2 && fSep[cr]==0)
                nodeQueue.Insert(cr,nodeQueue.INSERT_NO_THROW);
        }


        // Traverse the new part of the exterior face and update all data

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"New exterior path: (%lu",static_cast<unsigned long>(cl));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (u!=cr)
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer," [%lu] %lu",
                    static_cast<unsigned long>(a),static_cast<unsigned long>(v));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            // Consider the right-hand face of the arc a for merging with the exterior
            segmentQueue.Insert(a);

            vOut[face[a]] = NoNode;
            face[a] = fOut;
            TNode f = face[a^1];

            TArc aNext = G.Right(a^1,v);

            if (prevChange[f]!=k && vOut[f]==aOut[f]+1 && aOut[f]<2)
            {
                // f was not a separating face before the deletion of this component.
                // Adjust fSep[] after aOut[] and vOut[] have been updated completely.
                // For the time being, add to newSeparatingFaces
                prevChange[f] = k;
                newSeparatingFaces.Insert(a^1);
            }

            aOut[f]++;

            // By doing this, v is recognized as an external node later:
            GR -> SetFirst(v,a^1);

            TArc a2 = aNext;
            while (v!=cr && a2!=(a^1))
            {
                f = face[a2^1];

                if (   prevChange[f]!=k
                    && vOut[f]==aOut[f]+1 && aOut[f]<2
                    && G.Right(a2,v)!=(a^1)
                   )
                {
                    // f was not a separating face before the deletion of this
                    // component. Adjust fSep[] after aOut[] and vOut[] have
                    // been updated completely. For the time being, add to Q3
                    prevChange[f] = k;
                    newSeparatingFaces.Insert(a2^1);
                }

                vOut[f]++;

                a2 = G.Right(a2,v);
            }

            a = aNext;
            u = G.StartNode(a);
            v = G.EndNode(a);
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH,")");

        #endif

        while (!newSeparatingFaces.Empty())
        {
            TArc a2 = newSeparatingFaces.Delete();
            TNode f = face[a2];

            if (vOut[f]>aOut[f]+1 || aOut[f]>1)
            {
                // The face f has become a separating face.
                // Adjust fSep[] for all nodes incident with f

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"New separator face: %lu",
                        static_cast<unsigned long>(f));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                TArc a3 = a2;
                do
                {
                    TNode u = G.EndNode(a3);
                    fSep[u]++;

                    #if defined(_TRACING_)

                    if (CT.traceLevel>1) G.SetNodeColour(u,fSep[u]);

                    #endif

                    a3 = G.Right(a3^1,u);
                }
                while (a3!=a2);
            }
        }


        #if defined(_LOGGING_)

        for (TNode f=0;f<nd;f++)
        {
            if (f==fOut || vOut[f]==NoNode || vOut[f]==0) continue;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"vOut[%lu] = %lu, aOut[%lu] = %lu",
                    static_cast<unsigned long>(f),
                    static_cast<unsigned long>(vOut[f]),
                    static_cast<unsigned long>(f),
                    static_cast<unsigned long>(aOut[f]));
                LogEntry(LOG_METH2,CT.logBuffer);
            }
        }

        for (TNode v=0;v<n;v++)
        {
            if (G.First(v)==NoArc || fSep[v]==0) continue;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"fSep[%lu] = %lu",
                    static_cast<unsigned long>(v),static_cast<unsigned long>(fSep[v]));
                LogEntry(LOG_METH2,CT.logBuffer);
            }
        }

        #endif

        k++;

        M.Trace(G);
    }


    // Final component consists of v1 and v2

    vRight[v1] = v2;
    vRight[v2] = NoNode;

    #if defined(_LOGGING_)

    if (connected && CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Component %lu (Basis): %lu,%lu",
            static_cast<unsigned long>(k),
            static_cast<unsigned long>(v1),
            static_cast<unsigned long>(v2));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    k++;

    #if defined(_PROGRESS_)

    M.ProgressStep(2);

    #endif


    if (connected)
    {
        CloseFold();
        sprintf(CT.logBuffer,"Refine to leftmost canonical ordering...");
        LogEntry(LOG_METH,CT.logBuffer);
        OpenFold();

        // Lists for finding components by their rightmost contact node
        staticStack<TNode>** partByRight
            = new staticStack<TNode>*[n];
        partByRight[0] = new staticStack<TNode>(n,CT);

        for (TNode v=1;v<n;v++)
            partByRight[v] = new staticStack<TNode>(*partByRight[0]);

        for (TNode l=0;l<k-1;l++)
            partByRight[EndNode(cRight[l])] -> Insert(l);

        // Reverse incidence lists for the components
        TNode* vLeft = new TNode[n];

        for (TNode v=0;v<n;v++) vLeft[v] = NoNode;

        for (TNode v=0;v<n;v++)
            if (vRight[v]!=NoNode) vLeft[vRight[v]] = v;

        // Copies of cLeft and cRight in the refined ordering
        TArc* cLeftRefined = new TArc[k];
        TArc* cRightRefined = new TArc[k];


        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Component: %lu",static_cast<unsigned long>(k-1));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        nodeQueue.Init();
        nodeQueue.Insert(v2);

        TNode* layer = InitNodeColours();
        layer[v1] = layer[v2] = 0;
        TNode nr = 1;

        #if defined(_PROGRESS_)

        M.ProgressStep(2);

        #endif

        while (!nodeQueue.Empty())
        {
            TNode cr = nodeQueue.Peek();

            if (partByRight[cr]->Empty())
            {
                nodeQueue.Delete();
            }
            else
            {
                TNode l = partByRight[cr]->Delete();
                TNode u = StartNode(cRight[l]);

                while (u!=NoNode)
                {
                    nodeQueue.Insert(u);
                    layer[u] = nr;
                    u = vLeft[u];

                    #if defined(_PROGRESS_)

                    M.ProgressStep(1);

                    #endif
                }

                cLeftRefined[nr]  = cLeft[l];
                cRightRefined[nr] = cRight[l];
                nr++;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(l));
                    LogAppend(LH,CT.logBuffer);
                }

                #endif
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif

        cLeft[0] = cRight[0] = NoArc;

        for (TNode l=1;l<k;l++)
        {
            cLeft[l]  = cLeftRefined[l];
            cRight[l] = cRightRefined[l];
        }

        for (TNode v=1;v<n;v++)
            delete partByRight[v];

        delete partByRight[0];
        delete[] partByRight;
        delete[] vLeft;
        delete[] cLeftRefined;
        delete[] cRightRefined;

        M.Trace();
    }


    CloseFold();

    delete[] visited;
    delete[] fSep;
    delete[] vOut;
    delete[] aOut;
    delete[] prevChange;

    if (!connected)
    {
        Error(ERR_REJECTED,"LMCOrderedPartition","Graph is not 3-connected");
    }

    M.Shutdown(LOG_RES, "...Graph has been decomposed");

    return k;
}


void abstractMixedGraph::Layout_StraightLineDrawing(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (aBasis>=2*m && aBasis!=NoArc)
        NoSuchArc("Layout_StraightLineDrawing",aBasis);

    #endif

    moduleGuard M(ModPlanarity,*this,"Embedding the graph nodes...");

    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10,1);

    #endif

    GrowExteriorFace();

    sparseGraph G(*this,OPT_CLONE);
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());


    // Restrict to a simple graph
    staticStack<TArc> Q(2*m,CT);

    TNode* adjacent = new TNode[n];

    for (TNode w=0;w<n;w++) adjacent[w] = NoNode;

    THandle H = G.Investigate();
    investigator &I = G.Investigator(H);

    for (TNode u=0;u<n;u++)
    {
        while (I.Active(u))
        {
            TArc a = I.Read(u);
            TNode v = G.EndNode(a);

            if (u==v || (u>v && adjacent[v]==u))
            {
                Q.Insert(a);
            }
            else adjacent[v] = u;
        }
    }

    G.Close(H);

    delete[] adjacent;

    while (!Q.Empty())
    {
        TArc a = Q.Delete();

        if (G.StartNode(a)!=NoNode) GR -> CancelArc(a);
    }

    GR -> DeleteArcs();


    // Connectivity augmentation
    G.PlanarConnectivityAugmentation();

    M.Trace(G,1);


    // Biconnectivity augmentation
    G.PlanarBiconnectivityAugmentation();

    M.Trace(G,1);


    // Insert chords into the existing faces
    G.Triangulation();

    M.Trace(G,1);

    #if defined(_PROGRESS_)

    M.SetProgressNext(3);

    #endif


    if (m==G.M())
    {
        if (aBasis!=NoArc)
        {
            G.Layout_ConvexDrawing(aBasis,spacing);
        }
        else
        {
            G.Layout_ConvexDrawing(ExteriorArc(),spacing);
        }
    }
    else G.Layout_ConvexDrawing(NoArc,spacing);

    MarkExteriorFace(ExteriorArc());

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(4);

    #endif


    for (TNode v=0;v<n;v++)
    {
        SetC(v,0,G.C(v,0));
        SetC(v,1,G.C(v,1));
    }

    Layout_DefaultBoundingBox();


    if (CT.methLocal==LOCAL_OPTIMIZE) Layout_ForceDirected(FDP_RESTRICTED);

    M.Shutdown(LOG_RES, "...Straight line drawing found");
}


void abstractMixedGraph::Layout_ConvexDrawing(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation())
        NoSparseRepresentation("Layout_ConvexDrawing");

    if (aBasis>=2*m && aBasis!=NoArc) NoSuchArc("Layout_ConvexDrawing",aBasis);

    #endif

    moduleGuard M(ModConvexDrawing,*this,"Embedding the graph nodes...");

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((CT.traceLevel<3) ? 5*n : 4*n, 3*n);

    #endif

    if (aBasis!=NoArc) SetExteriorArc(aBasis);

    // Data structures associated with the ordered partition.
    // Number k of components is not known a priori but bounded by n.

    // Leftmost / rightmost contact arc of each component
    TArc* cLeft  = new TArc[n]; // Directed towards the component
    TArc* cRight = new TArc[n]; // Directed away from the component

    // Right hand node of each graph node in its component
    TNode* vRight = new TNode[n];


    TNode k = 0;

    // Compute the shelling order. Exit if graph is not triconnected
    try
    {
        k = LMCOrderedPartition(cLeft,cRight,vRight);
    }
    catch (ERRejected)
    {
        delete[] cLeft;
        delete[] cRight;
        delete[] vRight;

        throw ERRejected();
    }

    MarkExteriorFace(ExteriorArc());

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif


    LogEntry(LOG_METH,"Node placement...");
    OpenFold();

    TNode* layer = GetNodeColours();

    // The end nodes of the basis arc
    TNode v1 = EndNode(ExteriorArc());
    TNode v2 = StartNode(ExteriorArc());

    // Clockwise path on the exterior face of the currently embedded subgraph
    TNode* cNext = new TNode[n];
    TNode* cPrev = new TNode[n];
    cNext[v1] = v2;
    cPrev[v2] = v1;

    // ------------------------------------------------------------------------
    // This procedure implements the convex drawing algorithm twice:
    //
    //    The first code is O(n^2) and only used with the single step tracing.
    //    It depends on the data structures dShift[] and exterior[] and
    //    maintains the current x-coordinates for every intermediate step.
    //    For exterior nodes, dShift[] denotes the amount of shift in the
    //    current iteration. The embedded subgraph splits into node sets each
    //    of which includes exactly one exterior node. For an interior node v,
    //    this is exterior[v] and v is shifted with exterior[v].
    //
    //    The second code is O(m). It maintains the coordinates (xInsert[]) at
    //    the step of inserting a node. Current coordinates (xCurrent[]) can
    //    be determined for external nodes only, from xInsert[] and shift[]
    //    where shift[v] is a delayed shift of all exterior nodes right-hand
    //    from v. If correct[v] is true, then xCurrent[] is valid. In a second
    //    pass, the final x-coordinates are computed from xInsert[].
    // ------------------------------------------------------------------------

    // Data structures for the O(n^2) implementation
    TFloat* dShift = new TFloat[n];
    for (TNode v=0;v<n;v++) dShift[v] = 0;

    TNode* exterior = new TNode[n];
    for (TNode v=0;v<n;v++) exterior[v] = NoNode;

    // Data structures for the O(m) implementation are initialized inline
    TFloat* shift   = new TFloat[n];
    TFloat* xInsert  = new TFloat[n];
    TFloat* xCurrent = new TFloat[n];
    bool*   correct  = new bool[n];


    // Only for tracing purposes: Hide all nodes initially
    for (TNode v=0;v<n;v++) SetNodeVisibility(v,false);

    // Initial placement of the basis nodes v1 and v2
    SetC(v1,0,0);
    SetC(v1,1,0);
    SetC(v2,0,0);
    SetC(v2,1,0);
    xCurrent[v1] = xCurrent[v2] = xInsert[v1] = xInsert[v2] = 0;
    shift[v1] = shift[v2] = 0;
    correct[v1] = true;
    correct[v2] = false;

    #if defined(_PROGRESS_)

    M.ProgressStep(2);

    #endif

    for (TNode l=1;l<k;l++)
    {
        // Determine the cardinality q of the new component
        TNode vNext = EndNode(cLeft[l]);
        TNode q = 0;

        while (vNext!=NoNode)
        {
            vNext = vRight[vNext];
            q++;
        }

        #if defined(_PROGRESS_)

        M.ProgressStep(q);

        #endif


        // Set the right shift of the contact nodes
        TNode vl = StartNode(cLeft[l]);
        TNode vr = EndNode(cRight[l]);
        TNode v = vl;
        TArc a = cLeft[l]^1;

        if (CT.traceLevel>2)
        {
            // Helper variables to distinguish the shift segments
            TArc pointUp = NoArc;
            bool doRightShift = false;

            do
            {
                a = Right(a^1,v);
                TNode w = EndNode(a);

                // Trigger the shift operation
                if (layer[v]<=layer[w] || w==EndNode(cLeft[l]))
                {
                    if (   !doRightShift && v!=vl
                        && (layer[v]==layer[w] || w==EndNode(cLeft[l]))
                       )
                    {
                        dShift[v] = q;
                        exterior[v] = EndNode(cLeft[l]);
                    }

                    doRightShift = true;

                    // Skip the interior contact arcs of the l-th component
                    if (w==EndNode(cLeft[l]))
                    {
                        a = Right(a,v);
                        w = EndNode(a);
                        pointUp = NoArc;
                    }
                }

                // Save the arc when y coordinates eventually start increasing
                if (layer[v]<layer[w] && pointUp==NoArc) pointUp = a;

                v = w;

                if (v==vr)
                {
                    dShift[v] = 2*q;
                }
                else if (doRightShift)
                {
                    dShift[v] = q;
                    exterior[v] = EndNode(cLeft[l]);
                }
                else if (v!=vl)
                {
                    exterior[v] = vl;
                }
            }
            while (v!=vr);


            // Second pass: Some interior node must be shifted with vr
            //   to obtain a convex drawing
            if (pointUp!=NoArc)
            {
                a = pointUp;
                v = EndNode(a);

                while (v!=vr)
                {
                    dShift[v] = 2*q;
                    exterior[v] = vr;

                    a = Right(a^1,v);
                    v = EndNode(a);
                }
            }


            // Set the shift of the exterior nodes on the right-hand
            // side of the new component
            while (v!=v2)
            {
                v = cNext[v];
                dShift[v] = 2*q;
            }

            // Shift the nodes where necessary
            for (v=0;v<n;v++)
            {
                TFloat thisShift = dShift[v];

                // Nodes which were interior before this iteration
                if (exterior[v]!=NoNode && thisShift==0)
                {
                    thisShift = dShift[exterior[v]];

                    // Path compression
                    if (exterior[exterior[v]]!=NoNode)
                        exterior[v] = exterior[exterior[v]];
                }

                if (thisShift>0)
                {
                    SetC(v,0,C(v,0)+thisShift*spacing);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"x[%lu] += %g",
                            static_cast<unsigned long>(v),
                            static_cast<double>(thisShift*spacing));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }

            // Reset the shift for nodes which become interior by this iteration
            v = vl;
            while (cNext[v]!=vr)
            {
                v = cNext[v];
                dShift[v] = 0;
            }
        }

        // Place the k-th component and update cNext
        TFloat yl = C(vl,1);
        TFloat yr = C(vr,1);

        TFloat xl = C(vl,0);
        TFloat xr = C(vr,0)-2*(q-1)*spacing;

        if (CT.traceLevel<3)
        {
            // Update the x-coordinates of the exterior nodes in the left of vr
            v = vr;
            while (!correct[v]) v=cPrev[v];

            TFloat sumShift = 0;
            while (v!=vr)
            {
                v = cNext[v];

                if (v==vr)
                {
                    shift[v] += sumShift+2*q*spacing;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"x[%lu] += %g + %g",
                            static_cast<unsigned long>(v),
                            static_cast<double>(sumShift),
                            static_cast<double>(2*q*spacing));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    sumShift += shift[v];
                    xCurrent[v] += sumShift;
                    correct[v] = true;
                    // xCurrent[v] is correct only if v is still exterior !

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1 && sumShift>0)
                    {
                        sprintf(CT.logBuffer,"x[%lu] += %g",
                            static_cast<unsigned long>(v),static_cast<double>(sumShift));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }

            xl = xCurrent[vl];
            xr = xCurrent[vr]+shift[vr]-2*(q-1)*spacing;
        }


        TFloat xNew = xl + (xr+yr-xl-yl)/2;
        TFloat yNew = yl + (xr+yr-xl-yl)/2;

        vNext = EndNode(cLeft[l]);
        cNext[vl] = vNext;
        cPrev[vNext] = vl;
        TNode vOld = NoNode;

        while (vNext!=NoNode)
        {
            SetC(vNext,0,xNew);
            SetC(vNext,1,yNew);
            xCurrent[vNext] = xInsert[vNext] = xNew;
            shift[vNext] = 0;
            correct[vNext] = false;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Insert node %lu at (%g,%g)",
                    static_cast<unsigned long>(vNext),
                    static_cast<double>(xNew),static_cast<double>(yNew));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            vOld = vNext;
            vNext = vRight[vNext];
            if (vNext!=NoNode) cPrev[vNext] = vOld;
            cNext[vOld] = vNext;
            xNew += 2*spacing;
        }

        cNext[vOld] = vr;
        cPrev[vr] = vOld;

        if (CT.traceLevel>2)
        {
            // Reset the shift right-hand of vl on the updated exterior path
            v = vl;
            do
            {
                v = cNext[v];
                dShift[v] = 0;
            }
            while (v!=v2);

            X -> Layout_SetBoundingInterval(0,-spacing,C(v2,0)+spacing);
            X -> Layout_SetBoundingInterval(1,-spacing,yNew+spacing);

            M.Trace();
        }
    }


    // Second pass: Compute the final coordinates from the insert coordinates
    if (CT.traceLevel<3)
    {
        CloseFold();
        LogEntry(LOG_METH,"Compute final x-coordinates...");
        OpenFold();

        TFloat* rShift = new TFloat[n];

        for (TNode v=0;v<n;v++)
            shift[v] = rShift[v] = 0;

        #if defined(_PROGRESS_)

        M.ProgressStep(2);

        #endif

        for (TNode l=k-1;l>0;l--)
        {
            // Determine the cardinality q of the new component
            TNode vNext = EndNode(cLeft[l]);
            TNode q = 0;
            TFloat sumRShift = 0;

            while (vNext!=NoNode)
            {
                sumRShift += rShift[vNext];

                SetC(vNext,0,xInsert[vNext]+shift[vNext]+sumRShift);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"x[%lu] = %g + %g + %g",
                        static_cast<unsigned long>(vNext),
                        static_cast<double>(xInsert[vNext]),
                        static_cast<double>(shift[vNext]),
                        static_cast<double>(rShift[vNext]));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                vNext = vRight[vNext];
                q++;
            }

            #if defined(_PROGRESS_)

            M.ProgressStep(q);

            #endif


            // Set the right hand shift of the contact nodes
            TNode vl = StartNode(cLeft[l]);
            TNode vr = EndNode(cRight[l]);
            TNode v = vl;
            TArc a = cLeft[l]^1;

            // Helper variables to distinguish the shift segments
            TArc pointUp = NoArc;
            bool doRightShift = false;

            do
            {
                a = Right(a^1,v);
                TNode w = EndNode(a);

                // Trigger the shift operation
                if (layer[v]<=layer[w] || w==EndNode(cLeft[l]))
                {
                    if (   !doRightShift && v!=vl
                        && (layer[v]==layer[w] || w==EndNode(cLeft[l]))
                       )
                    {
                        shift[v] = shift[EndNode(cLeft[l])]+sumRShift+q*spacing;

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"shift[%lu] = %g + %g + %g",
                                static_cast<unsigned long>(v),
                                static_cast<double>(shift[EndNode(cLeft[l])]),
                                static_cast<double>(sumRShift),
                                static_cast<double>(q*spacing));
                            LogEntry(LOG_METH2,CT.logBuffer);
                        }

                        #endif
                    }

                    doRightShift = true;

                    // Skip the interior contact arcs of the l-th component
                    if (w==EndNode(cLeft[l]))
                    {
                        a = Right(a,v);
                        w = EndNode(a);
                        pointUp = NoArc;
                    }
                }

                // Save the arc when y coordinates eventually start increasing
                if (layer[v]<layer[w] && pointUp==NoArc) pointUp = a;

                v = w;

                if (v!=vr && doRightShift)
                {
                    shift[v] = shift[EndNode(cLeft[l])]+sumRShift+q*spacing;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"shift[%lu] = %g + %g + %g",
                            static_cast<unsigned long>(v),
                            static_cast<double>(shift[EndNode(cLeft[l])]),
                            static_cast<double>(sumRShift),
                            static_cast<double>(q*spacing));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else if (v!=vl && v!=vr)
                {
                    shift[v] = shift[StartNode(cLeft[l])];

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"shift[%lu] = %g",
                            static_cast<unsigned long>(v),
                            static_cast<double>(shift[StartNode(cLeft[l])]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
            while (v!=vr);

            rShift[vr] += sumRShift+2*q*spacing;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"rShift[%lu] += %g + %g",
                    static_cast<unsigned long>(v),
                    static_cast<double>(sumRShift),
                    static_cast<double>(2*q*spacing));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            // Second pass: Some interior node must be shifted with vr
            //   to obtain a convex drawing
            if (pointUp!=NoArc)
            {
                a = pointUp;
                v = EndNode(a);

                while (v!=vr)
                {
                    shift[v] = shift[vr]+rShift[vr];

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"shift[%lu] = %g + %g",
                            static_cast<unsigned long>(v),
                            static_cast<double>(shift[vr]),
                            static_cast<double>(rShift[vr]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    a = Right(a^1,v);
                    v = EndNode(a);
                }
            }
        }

        // The first component is the basis arc. Both nodes have been
        // inserted with x-coordinate 0 and v1 is not shifted at all.
        SetC(v2,0,rShift[v2]);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"x[%lu] = %g",
                static_cast<unsigned long>(v2),
                static_cast<double>(rShift[v2]));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        delete[] rShift;

        M.Trace();
    }


    CloseFold();

    sprintf(CT.logBuffer,"...Grid size: %lux%lu",
        static_cast<unsigned long>(C(v2,0)/spacing),
        static_cast<unsigned long>(C(EndNode(cLeft[k-1]),1)/spacing));
    LogEntry(LOG_RES,CT.logBuffer);

    delete[] cLeft;
    delete[] cRight;
    delete[] vRight;
    delete[] cNext;
    delete[] cPrev;
    delete[] dShift;
    delete[] exterior;
    delete[] shift;
    delete[] xInsert;
    delete[] xCurrent;
    delete[] correct;

    // Mirror the drawing so that incidence lists run in clockwise order
    for (TNode v=0;v<n;v++) SetC(v,1,C(v2,0)/2-C(v,1));

    X -> Layout_SetBoundingInterval(0,-spacing,C(v2,0)+spacing);
    X -> Layout_SetBoundingInterval(1,-spacing,C(v2,1)+spacing);

    M.Shutdown(LOG_RES, "...Convex drawing found");
}
