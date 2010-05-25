
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libPerfect.cpp
/// \brief  Recognition of, and optimization on classes of perfect graphs

#include "abstractMixedGraph.h"
#include "moduleGuard.h"


TNode abstractMixedGraph::PerfectEliminationOrder(TOptComplement complementarityMode)
    throw()
{
    moduleGuard M(ModChordality,*this,"Computing perfect elimination order...");

    // Lexicographic breadth first search:
    // Start with a single virtual set, containing all nodes. Successively split
    // into active subsets according to the adjacencies of the pivot nodes.

    // Keep double linked, circular lists of nodes. Let every active node point
    // to the active subset setIndex[] containing it, and every active subset to
    // one of its content nodes canonical[].

    TNode* predNode = new TNode[n];
    TNode* succNode = new TNode[n];
    TNode* setIndex = new TNode[n];
    TNode* canonical = new TNode[n];
    TNode* cardinality = new TNode[n];
    TNode* hits = new TNode[n];
    TNode* split = new TNode[n];

    // Save the found order of nodes to the node colour register.
    TNode* position = InitNodeColours(NoNode);

    // The current number of active subsets
    TNode setCount = 0;

    for (TNode v=0;v<n;++v)
    {
        succNode[(v+1)%n] = v;
        predNode[v] = (v+1)%n;
        setIndex[v] = setCount;
    }

    canonical[setCount] = n-1;
    cardinality[setCount] = n;


    // Also keep a linked list of the current active subsets. When a set is
    // split, it is maintained as one subset, and one new subset is inserted
    // in front of the original set.

    TNode* succSet = new TNode[n];
    TNode* predSet = new TNode[n];
    TNode activeSubset = 0;
    succSet[activeSubset] = predSet[activeSubset] = NoNode;
    TNode pivotNode = NoNode;


    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Active node sets for lexicographic BFS:");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode i=0;i<n;++i)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            THandle LH = LogStart(LOG_METH2,"(");

            TNode displaySet = activeSubset;

            do
            {
                TNode displayNode = canonical[displaySet];

                do
                {
                    sprintf(CT.logBuffer,"%lu",static_cast<unsigned long>(displayNode));
                    LogAppend(LH,CT.logBuffer);
                    displayNode = succNode[displayNode];

                    if (displayNode!=canonical[displaySet])  LogAppend(LH," ");
                }
                while (displayNode!=canonical[displaySet]);

                displaySet = succSet[displaySet];

                if (displaySet!=NoNode) LogAppend(LH,") (");
            }
            while (displaySet!=NoNode);

            LogEnd(LH,")");
        }

        #endif

        pivotNode = canonical[activeSubset];

        if (cardinality[activeSubset]==1)
        {
            // Active subset is a singleton
            activeSubset = succSet[activeSubset];
        }
        else
        {
            // Eliminate the pivot node from the active subset
            TNode pivotPredNode = predNode[pivotNode];
            TNode pivotSuccNode = succNode[pivotNode];
            succNode[pivotPredNode] = pivotSuccNode;
            predNode[pivotSuccNode] = pivotPredNode;
            canonical[activeSubset] = pivotSuccNode;
            --cardinality[activeSubset];
        }

        position[pivotNode] = n-i-1;

        // First pass: Reset hit[] and split[] of all adjacent active subsets
        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;

            hits[setIndex[v]] = 0;
            split[setIndex[v]] = NoNode;
        }

        // Second pass: Determine the hits in all adjacent active subsets
        I.Reset(pivotNode);

        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;

            ++hits[setIndex[v]];
        }

        // Third pass: Split the adjacent active subsets, if necessary
        I.Reset(pivotNode);

        // To distinguish the new subsets from those
        // which have existed before this pivoting step:
        TNode firstNewSubset = setCount+1;

        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;
            if (hits[setIndex[v]]==0) continue;
            if (hits[setIndex[v]]==cardinality[setIndex[v]]) continue;
            if (setIndex[v]>=firstNewSubset) continue;

            // Eliminate v from its current active subset
            TNode vPredNode = predNode[v];
            TNode vSuccNode = succNode[v];
            succNode[vPredNode] = vSuccNode;
            predNode[vSuccNode] = vPredNode;
            --cardinality[setIndex[v]];
            --hits[setIndex[v]];

            if (canonical[setIndex[v]]==v)
                canonical[setIndex[v]] = vSuccNode;

            if (split[setIndex[v]]==NoNode)
            {
                // Create a new subset split[setIndex[v]]
                ++setCount;
                canonical[setCount] = v;
                hits[setCount] = cardinality[setCount] = 1;
                split[setIndex[v]] = setCount;

                // ...and add split[setIndex[v]] into the list active subsets...
                if (complementarityMode==PERFECT_AS_IS)
                {
                    //...right before setIndex[v]
                    succSet[split[setIndex[v]]]   = setIndex[v];
                    predSet[split[setIndex[v]]]   = predSet[setIndex[v]];

                    if (predSet[setIndex[v]]!=NoNode)
                        succSet[predSet[setIndex[v]]] = split[setIndex[v]];

                    predSet[setIndex[v]] = split[setIndex[v]];

                    if (activeSubset==setIndex[v])
                        activeSubset = split[setIndex[v]];
                }
                else // if (complementarityMode==PERFECT_COMPLEMENT)
                {
                    // ...right after setIndex[v]
                    succSet[split[setIndex[v]]]   = succSet[setIndex[v]];
                    predSet[split[setIndex[v]]]   = setIndex[v];

                    if (succSet[setIndex[v]]!=NoNode)
                        predSet[succSet[setIndex[v]]] = split[setIndex[v]];

                    succSet[setIndex[v]]          = split[setIndex[v]];
                }

                // Add v to split[setIndex[v]]
                succNode[v] = v;
                predNode[v] = v;
                setIndex[v] = split[setIndex[v]];
            }
            else
            {
                // Insert v into its new subset split[setIndex[v]]
                setIndex[v] = split[setIndex[v]];
                succNode[v] = canonical[setIndex[v]];
                predNode[v] = predNode[succNode[v]];
                predNode[succNode[v]] = v;
                succNode[predNode[v]] = v;
                ++hits[setIndex[v]];
                ++cardinality[setIndex[v]];
            }
        }
    }

    Close(H);


    delete[] predNode;
    delete[] succNode;
    delete[] setIndex;
    delete[] canonical;
    delete[] cardinality;
    delete[] hits;
    delete[] split;
    delete[] succSet;

    return pivotNode;
}


bool abstractMixedGraph::IsChordal(TOptComplement complementarityMode)
    throw()
{
    moduleGuard M(ModChordality,*this,"Performing chordality check...");

    PerfectEliminationOrder(complementarityMode);

    LogEntry(LOG_METH,"Verifying perfect elimination order...");

    TNode* position = GetNodeColours();


    // It is to verify that position[] is indeed a perfect elimination order.
    // That is, RN[v] = {w<n : position[w] > position[v]} forms a clique for
    // every node v.

    // Sort the edges by the position[] of both end nodes. For this goal,
    // orientate the edge from the lower to higher position[] of its end nodes.
    // Use the start node's position[] as the primary key, and end node's
    // position[] as the secondary key.
    goblinQueue<TArc,TFloat>* Q = NewArcHeap();

    for (TArc a=0;a<2*m;++a)
    {
        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (position[u]>=position[v]) continue;

        Q->Insert(a>>1,position[u]*n+position[v]);
    }

    TArc* RNConcat = RawEdgeColours();
    TArc* RNOffset = new TArc[m];
    TNode v=0;

    for (;v<n;++v) RNOffset[v] = NoArc;

    for (TArc i=0;i<m;++i)
    {
        if (Q->Empty())
        {
            // This is only reached if loops have been sorted out earlier
            RNConcat[i] = NoArc;
            continue;
        }

        TArc a = 2*(Q->Delete());
        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (position[u]>position[v])
        {
            // Reverse arc
            a = a^1;
            u = StartNode(a);
            v = EndNode(a);
        }

        RNConcat[i] = a;

// cout << "(" << StartNode(a) << "," << EndNode(a) << ") ";
        if (i==0 || StartNode(RNConcat[i-1])!=u) RNOffset[u] = i;
    }
// cout << endl;
    delete Q;


    // At this point, RNConcat[] is the concatenation of the desired RN[] lists.
    // The entry points of the RN[] lists are given by RNOffset[]

    TNode rightMostViolation = NoNode;

    for (v=0;v<n;++v)
    {
        TArc i1 = RNOffset[v];

        if (i1==NoArc) continue;

        TNode parent = EndNode(RNConcat[i1]);
        TArc i2 = RNOffset[parent];

        // It is sufficient to verify that RN[v]\{parent} is a subset of RN[parent]
        // or, in the complementary case, that RN[parent] is a subset of RN[v]\{parent}

        // parent is the first node in RN[v] and must be skipped
        while (i1<m && EndNode(RNConcat[i1])==parent) ++i1;

        while (true)
        {
/*
    if (i1>=m)
    sprintf(CT.logBuffer,"Node %lu complete",static_cast<unsigned long>(v));
    else if (i2>=m)
    sprintf(CT.logBuffer,"x1 = %lu, parent = %lu",
        static_cast<unsigned long>(EndNode(RNConcat[i1])),
        static_cast<unsigned long>(parent));
    else
    sprintf(CT.logBuffer,"x1 = %lu, x2 = %lu",
        static_cast<unsigned long>(EndNode(RNConcat[i1])),
        static_cast<unsigned long>(EndNode(RNConcat[i2])));
    LogEntry(LOG_METH2,CT.logBuffer);
*/
            bool newViolation = false;

            if (complementarityMode==PERFECT_AS_IS)
            {
                if (i1>=m || StartNode(RNConcat[i1])!=v)
                {
                    // RN[v] exhausted, check for node v successfully passed
                    break;
                }

                if (   i2>=m
                    || StartNode(RNConcat[i2])!=parent
                    || position[EndNode(RNConcat[i1])]<position[EndNode(RNConcat[i2])]
                   )
                {
                    // RN[parent] is exhausted or, at least, EndNode(RNConcat[i1])
                    // is not in RN[parent]. Check for node v has failed
                    newViolation = true;
                }
            }
            else // if (complementarityMode==PERFECT_COMPLEMENT)
            {
                if (i2>=m || StartNode(RNConcat[i2])!=v)
                {
                    // RN[parent] exhausted, check for node v successfully passed
                    break;
                }

                if (   i1>=m
                    || StartNode(RNConcat[i1])!=parent
                    || position[EndNode(RNConcat[i2])]<position[EndNode(RNConcat[i1])]
                   )
                {
                    // RN[v] is exhausted or, at least, EndNode(RNConcat[i2])
                    // is not in RN[v]. Check for node v has failed
                    newViolation = true;
                }
            }

            if (newViolation)
            {
                if (rightMostViolation==NoNode || position[v]>position[rightMostViolation])
                {
                    rightMostViolation = v;
                }

                #if defined(_LOGGING_)

                if (CT.logRes>1)
                {
                    sprintf(CT.logBuffer, "...Clique condition for node %lu violated",
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_RES2,CT.logBuffer);
                }

                #endif

                break;
            }

            if (complementarityMode==PERFECT_AS_IS)
            {
                 if (position[EndNode(RNConcat[i1])]==position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[v]
                    ++i1;
                }
                else // if (position[EndNode(RNConcat[i1])]>position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[parent]
                    ++i2;
                }
            }
            else // if (complementarityMode==PERFECT_COMPLEMENT)
            {
                if (position[EndNode(RNConcat[i1])]==position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[v]
                    ++i2;
                }
                else // if (position[EndNode(RNConcat[i2])]>position[EndNode(RNConcat[i1])])
                {
                    // Step in RN[parent]
                    ++i1;
                }
            }
        }
    }


    // For sake of visualization, let the predecessor arc register point to the
    // first member in the RN[] list. The RNConcat[] will be exported as well.
    TArc* pred = InitPredecessors();

    for (v=0;v<n;++v)
    {
        if (RNOffset[v]!=NoArc) pred[v] = RNConcat[RNOffset[v]]^1;
    }


    delete[] RNOffset;


    if (rightMostViolation!=NoNode)
    {
        M.Shutdown(LOG_RES,(complementarityMode==PERFECT_AS_IS) ?
             "...Graph is not chordal" : "...Graph is not co-chordal");

        return false;
    }

    M.Shutdown(LOG_RES,(complementarityMode==PERFECT_AS_IS) ?
        "...Graph is chordal" : "...Graph is co-chordal");

    return true;
}



//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libPerfect.cpp
/// \brief  Recognition of, and optimization on classes of perfect graphs

#include "abstractMixedGraph.h"
#include "moduleGuard.h"


TNode abstractMixedGraph::PerfectEliminationOrder(TOptComplement complementarityMode)
    throw()
{
    moduleGuard M(ModChordality,*this,"Computing perfect elimination order...");

    // Lexicographic breadth first search:
    // Start with a single virtual set, containing all nodes. Successively split
    // into active subsets according to the adjacencies of the pivot nodes.

    // Keep double linked, circular lists of nodes. Let every active node point
    // to the active subset setIndex[] containing it, and every active subset to
    // one of its content nodes canonical[].

    TNode* predNode = new TNode[n];
    TNode* succNode = new TNode[n];
    TNode* setIndex = new TNode[n];
    TNode* canonical = new TNode[n];
    TNode* cardinality = new TNode[n];
    TNode* hits = new TNode[n];
    TNode* split = new TNode[n];

    // Save the found order of nodes to the node colour register.
    TNode* position = InitNodeColours(NoNode);

    // The current number of active subsets
    TNode setCount = 0;

    for (TNode v=0;v<n;++v)
    {
        succNode[(v+1)%n] = v;
        predNode[v] = (v+1)%n;
        setIndex[v] = setCount;
    }

    canonical[setCount] = n-1;
    cardinality[setCount] = n;


    // Also keep a linked list of the current active subsets. When a set is
    // split, it is maintained as one subset, and one new subset is inserted
    // in front of the original set.

    TNode* succSet = new TNode[n];
    TNode* predSet = new TNode[n];
    TNode activeSubset = 0;
    succSet[activeSubset] = predSet[activeSubset] = NoNode;
    TNode pivotNode = NoNode;


    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Active node sets for lexicographic BFS:");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode i=0;i<n;++i)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            THandle LH = LogStart(LOG_METH2,"(");

            TNode displaySet = activeSubset;

            do
            {
                TNode displayNode = canonical[displaySet];

                do
                {
                    sprintf(CT.logBuffer,"%lu",static_cast<unsigned long>(displayNode));
                    LogAppend(LH,CT.logBuffer);
                    displayNode = succNode[displayNode];

                    if (displayNode!=canonical[displaySet])  LogAppend(LH," ");
                }
                while (displayNode!=canonical[displaySet]);

                displaySet = succSet[displaySet];

                if (displaySet!=NoNode) LogAppend(LH,") (");
            }
            while (displaySet!=NoNode);

            LogEnd(LH,")");
        }

        #endif

        pivotNode = canonical[activeSubset];

        if (cardinality[activeSubset]==1)
        {
            // Active subset is a singleton
            activeSubset = succSet[activeSubset];
        }
        else
        {
            // Eliminate the pivot node from the active subset
            TNode pivotPredNode = predNode[pivotNode];
            TNode pivotSuccNode = succNode[pivotNode];
            succNode[pivotPredNode] = pivotSuccNode;
            predNode[pivotSuccNode] = pivotPredNode;
            canonical[activeSubset] = pivotSuccNode;
            --cardinality[activeSubset];
        }

        position[pivotNode] = n-i-1;

        // First pass: Reset hit[] and split[] of all adjacent active subsets
        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;

            hits[setIndex[v]] = 0;
            split[setIndex[v]] = NoNode;
        }

        // Second pass: Determine the hits in all adjacent active subsets
        I.Reset(pivotNode);

        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;

            ++hits[setIndex[v]];
        }

        // Third pass: Split the adjacent active subsets, if necessary
        I.Reset(pivotNode);

        // To distinguish the new subsets from those
        // which have existed before this pivoting step:
        TNode firstNewSubset = setCount+1;

        while (I.Active(pivotNode))
        {
            TArc a = I.Read(pivotNode);
            TNode v = EndNode(a);

            if (position[v]!=NoNode) continue;
            if (hits[setIndex[v]]==0) continue;
            if (hits[setIndex[v]]==cardinality[setIndex[v]]) continue;
            if (setIndex[v]>=firstNewSubset) continue;

            // Eliminate v from its current active subset
            TNode vPredNode = predNode[v];
            TNode vSuccNode = succNode[v];
            succNode[vPredNode] = vSuccNode;
            predNode[vSuccNode] = vPredNode;
            --cardinality[setIndex[v]];
            --hits[setIndex[v]];

            if (canonical[setIndex[v]]==v)
                canonical[setIndex[v]] = vSuccNode;

            if (split[setIndex[v]]==NoNode)
            {
                // Create a new subset split[setIndex[v]]
                ++setCount;
                canonical[setCount] = v;
                hits[setCount] = cardinality[setCount] = 1;
                split[setIndex[v]] = setCount;

                // ...and add split[setIndex[v]] into the list active subsets...
                if (complementarityMode==PERFECT_AS_IS)
                {
                    //...right before setIndex[v]
                    succSet[split[setIndex[v]]]   = setIndex[v];
                    predSet[split[setIndex[v]]]   = predSet[setIndex[v]];

                    if (predSet[setIndex[v]]!=NoNode)
                        succSet[predSet[setIndex[v]]] = split[setIndex[v]];

                    predSet[setIndex[v]] = split[setIndex[v]];

                    if (activeSubset==setIndex[v])
                        activeSubset = split[setIndex[v]];
                }
                else // if (complementarityMode==PERFECT_COMPLEMENT)
                {
                    // ...right after setIndex[v]
                    succSet[split[setIndex[v]]]   = succSet[setIndex[v]];
                    predSet[split[setIndex[v]]]   = setIndex[v];

                    if (succSet[setIndex[v]]!=NoNode)
                        predSet[succSet[setIndex[v]]] = split[setIndex[v]];

                    succSet[setIndex[v]]          = split[setIndex[v]];
                }

                // Add v to split[setIndex[v]]
                succNode[v] = v;
                predNode[v] = v;
                setIndex[v] = split[setIndex[v]];
            }
            else
            {
                // Insert v into its new subset split[setIndex[v]]
                setIndex[v] = split[setIndex[v]];
                succNode[v] = canonical[setIndex[v]];
                predNode[v] = predNode[succNode[v]];
                predNode[succNode[v]] = v;
                succNode[predNode[v]] = v;
                ++hits[setIndex[v]];
                ++cardinality[setIndex[v]];
            }
        }
    }

    Close(H);


    delete[] predNode;
    delete[] succNode;
    delete[] setIndex;
    delete[] canonical;
    delete[] cardinality;
    delete[] hits;
    delete[] split;
    delete[] succSet;

    return pivotNode;
}


bool abstractMixedGraph::IsChordal(TOptComplement complementarityMode)
    throw()
{
    moduleGuard M(ModChordality,*this,"Performing chordality check...");

    PerfectEliminationOrder(complementarityMode);

    LogEntry(LOG_METH,"Verifying perfect elimination order...");

    TNode* position = GetNodeColours();


    // It is to verify that position[] is indeed a perfect elimination order.
    // That is, RN[v] = {w<n : position[w] > position[v]} forms a clique for
    // every node v.

    // Sort the edges by the position[] of both end nodes. For this goal,
    // orientate the edge from the lower to higher position[] of its end nodes.
    // Use the start node's position[] as the primary key, and end node's
    // position[] as the secondary key.
    goblinQueue<TArc,TFloat>* Q = NewArcHeap();

    for (TArc a=0;a<2*m;++a)
    {
        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (position[u]>=position[v]) continue;

        Q->Insert(a>>1,position[u]*n+position[v]);
    }

    TArc* RNConcat = RawEdgeColours();
    TArc* RNOffset = new TArc[m];
    TNode v=0;

    for (;v<n;++v) RNOffset[v] = NoArc;

    for (TArc i=0;i<m;++i)
    {
        if (Q->Empty())
        {
            // This is only reached if loops have been sorted out earlier
            RNConcat[i] = NoArc;
            continue;
        }

        TArc a = 2*(Q->Delete());
        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (position[u]>position[v])
        {
            // Reverse arc
            a = a^1;
            u = StartNode(a);
            v = EndNode(a);
        }

        RNConcat[i] = a;

// cout << "(" << StartNode(a) << "," << EndNode(a) << ") ";
        if (i==0 || StartNode(RNConcat[i-1])!=u) RNOffset[u] = i;
    }
// cout << endl;
    delete Q;


    // At this point, RNConcat[] is the concatenation of the desired RN[] lists.
    // The entry points of the RN[] lists are given by RNOffset[]

    TNode rightMostViolation = NoNode;

    for (v=0;v<n;++v)
    {
        TArc i1 = RNOffset[v];

        if (i1==NoArc) continue;

        TNode parent = EndNode(RNConcat[i1]);
        TArc i2 = RNOffset[parent];

        // It is sufficient to verify that RN[v]\{parent} is a subset of RN[parent]
        // or, in the complementary case, that RN[parent] is a subset of RN[v]\{parent}

        // parent is the first node in RN[v] and must be skipped
        while (i1<m && EndNode(RNConcat[i1])==parent) ++i1;

        while (true)
        {
/*
    if (i1>=m)
    sprintf(CT.logBuffer,"Node %lu complete",static_cast<unsigned long>(v));
    else if (i2>=m)
    sprintf(CT.logBuffer,"x1 = %lu, parent = %lu",
        static_cast<unsigned long>(EndNode(RNConcat[i1])),
        static_cast<unsigned long>(parent));
    else
    sprintf(CT.logBuffer,"x1 = %lu, x2 = %lu",
        static_cast<unsigned long>(EndNode(RNConcat[i1])),
        static_cast<unsigned long>(EndNode(RNConcat[i2])));
    LogEntry(LOG_METH2,CT.logBuffer);
*/
            bool newViolation = false;

            if (complementarityMode==PERFECT_AS_IS)
            {
                if (i1>=m || StartNode(RNConcat[i1])!=v)
                {
                    // RN[v] exhausted, check for node v successfully passed
                    break;
                }

                if (   i2>=m
                    || StartNode(RNConcat[i2])!=parent
                    || position[EndNode(RNConcat[i1])]<position[EndNode(RNConcat[i2])]
                   )
                {
                    // RN[parent] is exhausted or, at least, EndNode(RNConcat[i1])
                    // is not in RN[parent]. Check for node v has failed
                    newViolation = true;
                }
            }
            else // if (complementarityMode==PERFECT_COMPLEMENT)
            {
                if (i2>=m || StartNode(RNConcat[i2])!=v)
                {
                    // RN[parent] exhausted, check for node v successfully passed
                    break;
                }

                if (   i1>=m
                    || StartNode(RNConcat[i1])!=parent
                    || position[EndNode(RNConcat[i2])]<position[EndNode(RNConcat[i1])]
                   )
                {
                    // RN[v] is exhausted or, at least, EndNode(RNConcat[i2])
                    // is not in RN[v]. Check for node v has failed
                    newViolation = true;
                }
            }

            if (newViolation)
            {
                if (rightMostViolation==NoNode || position[v]>position[rightMostViolation])
                {
                    rightMostViolation = v;
                }

                #if defined(_LOGGING_)

                if (CT.logRes>1)
                {
                    sprintf(CT.logBuffer, "...Clique condition for node %lu violated",
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_RES2,CT.logBuffer);
                }

                #endif

                break;
            }

            if (complementarityMode==PERFECT_AS_IS)
            {
                 if (position[EndNode(RNConcat[i1])]==position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[v]
                    ++i1;
                }
                else // if (position[EndNode(RNConcat[i1])]>position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[parent]
                    ++i2;
                }
            }
            else // if (complementarityMode==PERFECT_COMPLEMENT)
            {
                if (position[EndNode(RNConcat[i1])]==position[EndNode(RNConcat[i2])])
                {
                    // Step in RN[v]
                    ++i2;
                }
                else // if (position[EndNode(RNConcat[i2])]>position[EndNode(RNConcat[i1])])
                {
                    // Step in RN[parent]
                    ++i1;
                }
            }
        }
    }


    // For sake of visualization, let the predecessor arc register point to the
    // first member in the RN[] list. The RNConcat[] will be exported as well.
    TArc* pred = InitPredecessors();

    for (v=0;v<n;++v)
    {
        if (RNOffset[v]!=NoArc) pred[v] = RNConcat[RNOffset[v]]^1;
    }


    delete[] RNOffset;


    if (rightMostViolation!=NoNode)
    {
        M.Shutdown(LOG_RES,(complementarityMode==PERFECT_AS_IS) ?
             "...Graph is not chordal" : "...Graph is not co-chordal");

        return false;
    }

    M.Shutdown(LOG_RES,(complementarityMode==PERFECT_AS_IS) ?
        "...Graph is chordal" : "...Graph is co-chordal");

    return true;
}


