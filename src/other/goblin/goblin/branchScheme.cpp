
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   branchScheme.cpp
/// \brief  Implementation of a universal branch & bound solver

#include "branchScheme.h"
#include "moduleGuard.h"


template <class TItem,class TObj>
branchNode<TItem,TObj>::branchNode(TItem nn,goblinController &thisContext,
        branchScheme<TItem,TObj> *thisScheme) throw() :
    managedObject(thisContext)
{
    n = unfixed = nn;
    depth = n;
    solved = false;
    index = NoNode;
    scheme = thisScheme;

    LogEntry(LOG_MEM,"...Branch node instanciated");
}


template <class TItem,class TObj>
branchNode<TItem,TObj>::~branchNode() throw()
{
    LogEntry(LOG_MEM,"...Branch node disallocated");
}


template <class TItem,class TObj>
unsigned long branchNode<TItem,TObj>::Allocated() const throw()
{
    return 0;
}


template <class TItem,class TObj>
TObj branchNode<TItem,TObj>::Objective() throw()
{
    if (!solved)
    {
        objective = SolveRelaxation();
        solved = true;
    }

    return objective;
}


template <class TItem,class TObj>
bool branchNode<TItem,TObj>::Feasible() throw()
{
    return (unfixed==0);
}


template <class TItem,class TObj>
TObj branchNode<TItem,TObj>::LocalSearch() throw()
{
    return objective;
}


#if defined(_TRACING_)

branchTree::branchTree(goblinController &thisContext) throw() :
    managedObject(thisContext),
    sparseDiGraph(TNode(0),thisContext)
{
    LogEntry(LOG_MAN,"Generating branch tree...");
    SetLayoutParameter(TokLayoutArcVisibilityMode,graphDisplayProxy::ARC_DISPLAY_PREDECESSORS);
    SetLayoutParameter(TokLayoutNodeColourMode,graphDisplayProxy::NODES_FIXED_COLOURS);
    SetLayoutParameter(TokLayoutArcLabelFormat,"#4");
}

#endif


template <class TItem,class TObj>
branchScheme<TItem,TObj>::branchScheme(branchNode<TItem,TObj>* root,
    TObj aPrioriBound,TModule module,TSearchLevel thisLevel) throw() :
    managedObject(root->Context()),
    M(module,*this,"Branching...",moduleGuard::SYNC_BOUNDS)
{
    nIterations = 0;
    nActive = 0;
    maxActive = 0;
    nDFS = 0;
    depth = root->depth;
    firstActive = NULL;
    savedObjective = aPrioriBound;
    bestBound = root->Objective();

    if (root->ObjectSense()==MAXIMIZE)
    {
        sign = -1;

        M.SetLowerBound(savedObjective);

        if (bestBound>=savedObjective) M.SetUpperBound(bestBound);
    }
    else
    {
        sign = 1;

        M.SetUpperBound(savedObjective);

        if (bestBound<=savedObjective) M.SetLowerBound(bestBound);
    }

    feasible = !(savedObjective==root->Infeasibility());
    level = thisLevel;
    root->scheme = this;

    #if defined(_TRACING_)

    if (CT.traceLevel>1) Tree = new branchTree(CT);

    #endif

    LogEntry(LOG_MEM,"...B&B scheme instanciated");

    #if defined(_LOGGING_)

    if (CT.logMeth>1 && CT.logGaps==0)
    {
        LogEntry(LOG_METH2,"");
        LogEntry(LOG_METH2,"Iteration        Objective    Free  "
                  "Status      Saved Obj       Best Bound  Active  Select");
        LogEntry(LOG_METH2,"------------------------------------------------------------------------------------------");
    }

    #endif

    if (Inspect(root))
    {
         if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH,"  STOP");
         delete root;
    }
    else Optimize();

    if (CT.logMeth>1 && CT.logGaps==0) LogEntry(LOG_METH2,"");

    if (sign*bestBound<=sign*(savedObjective+CT.epsilon)-1 ||
        (sign*bestBound<=sign*(savedObjective+CT.epsilon) && !feasible && nActive>0))
    {
        M.Shutdown(LOG_METH,"...Interrupted!");
    }
    else
    {
        bestBound = savedObjective;

        if (sign==-1)
        {
            M.SetUpperBound(bestBound);
        }
        else
        {
            M.SetLowerBound(bestBound);
        }
    }

    if (CT.logMeth==1 || CT.logGaps>0)
    {
        sprintf(CT.logBuffer,"...Total number of branch nodes: %lu",nIterations);
        M.Shutdown(LOG_METH,CT.logBuffer);
    }

    #if defined(_TRACING_)

    if (CT.traceLevel==2 && nIterations>2)
    {
        Tree -> Layout_PredecessorTree();
        Tree -> Display();
    }

    #endif
}


template <class TItem,class TObj>
branchScheme<TItem,TObj>::~branchScheme() throw()
{
    branchNode<TItem,TObj> *thisNode = firstActive;

    while (thisNode!=NULL)
    {
        branchNode<TItem,TObj>* predNode = thisNode;
        thisNode = thisNode->succNode;
        delete predNode;
    }

    #if defined(_TRACING_)

    if (CT.traceLevel>1)
    {
        int savedLevel = CT.traceLevel;
        CT.traceLevel = 1;
        delete Tree;
        CT.traceLevel = savedLevel;
    }

    #endif

    LogEntry(LOG_MEM,"...B&B scheme disallocated");
}


template <class TItem,class TObj>
unsigned long branchScheme<TItem,TObj>::Size() const throw()
{
    return
          sizeof(branchScheme<TItem,TObj>)
        + Allocated();
}


template <class TItem,class TObj>
unsigned long branchScheme<TItem,TObj>::Allocated() const throw()
{
    return 0;
}


template <class TItem,class TObj>
void branchScheme<TItem,TObj>::Optimize() throw()
{
    branchNode<TItem,TObj> *activeNode = NULL;
    branchNode<TItem,TObj> *leftChild = NULL;
    branchNode<TItem,TObj> *rightChild = NULL;

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1);

    double combEstimate = 0;

    #endif

    while (   CT.SolverRunning() && nActive>0
           && nActive<unsigned(CT.maxBBNodes)*100
           && (level!=SEARCH_FEASIBLE || !feasible)
           && (   sign*bestBound<=sign*savedObjective+CT.epsilon-1
               || (sign*bestBound<=sign*savedObjective+CT.epsilon && !feasible) )
           && (   CT.maxBBIterations<0
               || nIterations<unsigned(CT.maxBBIterations)*1000 )
          )
    {
        activeNode = SelectActiveNode();

        #if defined(_TRACING_)

        TNode activeIndex = activeNode->index;

        #endif

        TItem i = activeNode->SelectVariable();

        typedef typename branchNode<TItem,TObj>::TBranchDir TBranchDir;
        TBranchDir dir =
            activeNode->DirectionConstructive(i);

        if (feasible) dir = activeNode->DirectionExhaustive(i);

        leftChild = activeNode;
        rightChild = activeNode->Clone();

        rightChild -> Raise(i);
        leftChild -> Lower(i);

        bool deleteLeft = false;
        bool deleteRight = false;

        if (dir==branchNode<TItem,TObj>::RAISE_FIRST)
        {
            deleteLeft = Inspect(leftChild);
            if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH);
            deleteRight = Inspect(rightChild);
        }
        else
        {
            deleteRight = Inspect(rightChild);
            if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH);
            deleteLeft = Inspect(leftChild);
        }

        if (leftChild->ObjectSense()==MAXIMIZE)
        {
            if (M.LowerBound()<=bestBound) M.SetUpperBound(bestBound);
        }
        else
        {
            if (M.UpperBound()>=bestBound) M.SetLowerBound(bestBound);
        }

        #if defined(_TRACING_)

        if (CT.traceLevel>1)
        {
            Tree->SetNodeColour(activeIndex,TNode(PROCESSED));

            TArc a = 2*(Tree->InsertArc(activeIndex,leftChild->index));
            Tree -> SetPred(leftChild->index,a);
            Tree->Representation() -> SetLength(a,i);

            a = 2*(Tree->InsertArc(activeIndex,rightChild->index));
            Tree -> SetPred(rightChild->index,a);
            Tree->Representation() -> SetLength(a,i);

            if (CT.traceLevel==3 && nIterations>1)
            {
                Tree->Layout_PredecessorTree();
            }
        }

        #endif

        if (deleteLeft) delete leftChild;
        if (deleteRight) delete rightChild;

        #if defined(_PROGRESS_)

        double thisEstimate =
            sqrt((nIterations+1-2*nActive)/(nIterations+1.0));

        // Filter the progress counter used for display
        combEstimate = 0.85*combEstimate + 0.15*thisEstimate;
        double thisEstimate2 =
                combEstimate*combEstimate*combEstimate*combEstimate;

        if (CT.maxBBIterations>0)
        {
            // At least, in the DFS case, a different estimation is required
            double thisEstimate3 = nIterations/(CT.maxBBIterations*1000.0);

            if (thisEstimate2<thisEstimate3)
            {
                thisEstimate2 = thisEstimate3;
            }
        }

        M.SetProgressCounter(thisEstimate2);

        #endif
    }

    if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH,"  STOP");
}


template <class TItem,class TObj>
bool branchScheme<TItem,TObj>::Inspect(branchNode<TItem,TObj> *thisNode)
    throw()
{
    TObj thisObjective = thisNode->Objective();

    #if defined(_TRACING_)

    if (CT.traceLevel>1) thisNode->index = Tree->InsertNode();

    #endif

    #if defined(_LOGGING_)

    if (CT.logMeth>=2 && CT.logGaps==0)
    {
        sprintf(CT.logBuffer,"%9.1lu  ",
            static_cast<unsigned long>(nIterations));
        LH = LogStart(LOG_METH2,CT.logBuffer);

        if (thisObjective!=thisNode->Infeasibility())
        {
            sprintf(CT.logBuffer,"%15.10g",
                static_cast<double>(thisObjective));
            LogAppend(LH,CT.logBuffer);
        }
        else LogAppend(LH,"     INFEASIBLE");

        sprintf(CT.logBuffer,"  %6.1lu  ",
            static_cast<unsigned long>(thisNode->Unfixed()));
        LogAppend(LH,CT.logBuffer);
    }

    #endif

    bool ret = false;

    if (sign*thisObjective<=sign*savedObjective+CT.epsilon-1 ||
        (sign*thisObjective<=sign*savedObjective+CT.epsilon &&
            thisObjective!=thisNode->Infeasibility() && !feasible))
    {
        if (thisNode->Feasible())
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>=2 && CT.logGaps==0) LogAppend(LH,"SAVED ");

            #endif

            #if defined(_TRACING_)

            if (CT.traceLevel>1) Tree -> SetNodeColour(thisNode->index,TNode(SAVED));

            #endif

            feasible = true;
            savedObjective = thisNode->LocalSearch();

            if (thisNode->ObjectSense()==MAXIMIZE)
            {
                M.SetLowerBound(savedObjective);
            }
            else
            {
                M.SetUpperBound(savedObjective);
            }

            thisNode -> SaveSolution();
            StripQueue();
            ret = true;
            nDFS = 0;
        }
        else
        {
            QueueExploredNode(thisNode);

            #if defined(_LOGGING_)

            if (CT.logMeth>=2 && CT.logGaps==0) LogAppend(LH,"QUEUED");

            #endif

            #if defined(_TRACING_)

            if (CT.traceLevel>1) Tree -> SetNodeColour(thisNode->index,TNode(QUEUED));

            #endif
        }
    }
    else
    {
        ret = true;

        #if defined(_LOGGING_)

        if (CT.logMeth>=2 && CT.logGaps==0) LogAppend(LH,"CUTOFF");

        #endif

        #if defined(_TRACING_)

        if (CT.traceLevel>1) Tree -> SetNodeColour(thisNode->index,TNode(CUTOFF));

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>=2 && CT.logGaps==0)
    {
        if (savedObjective==thisNode->Infeasibility())
            sprintf(CT.logBuffer,"       UNSOLVED");
        else sprintf(CT.logBuffer,"%15.10g",static_cast<double>(savedObjective));

        if (bestBound==thisNode->Infeasibility())
            sprintf(CT.logBuffer,"%s       INFEASIBLE",CT.logBuffer);
        else sprintf(CT.logBuffer,"%s  %15.10g",CT.logBuffer,static_cast<double>(bestBound));

        sprintf(CT.logBuffer,"%s  %6.1lu",CT.logBuffer,
            static_cast<unsigned long>(nActive));
        LogAppend(LH,CT.logBuffer);
    }

    #endif

    nIterations++;

    return ret;
}


template <class TItem,class TObj>
void branchScheme<TItem,TObj>::QueueExploredNode(branchNode<TItem,TObj> *thisNode)
    throw()
{
    thisNode->succNode = firstActive;
    firstActive = thisNode;
    nActive++;

    if (nActive>maxActive) maxActive = nActive;

    if (sign*(thisNode->Objective())<sign*bestBound)
    {
        bestBound = thisNode->Objective();
    }
}


template <class TItem,class TObj>
typename branchScheme<TItem,TObj>::TSearchState branchScheme<TItem,TObj>::SearchState() throw()
{
    if (nIterations<5.0*depth && level!=SEARCH_EXHAUSTIVE)
    {
        // Start branching with a DFS strategy for
        // quick construction of feasible solutions.
        return INITIAL_DFS;
    }

    if (nActive+depth+2.0>CT.maxBBNodes*100.0)
    {
        // When reaching the possible memory usage,
        // restrict nActive by switch to DFS again.
        return EXHAUSTIVE_DFS;
    }

    if (   level!=SEARCH_EXHAUSTIVE && depth>0
        && nIterations%(depth/2)>depth/20.0 )
    {
        // During constructive search, it is useful to
        // apply DFS. Sometimes, interrupt the DFS and
        // restart it from another subproblem which
        // is then determined by a best first strategy.
        return EXHAUSTIVE_DFS;
    }

    if (   level==SEARCH_EXHAUSTIVE
        || 3.0*nActive>2.0*(CT.maxBBNodes)*100.0 )
    {
        // Apply best first search. When getting close
        // the possible memory usage, or during exhaustive
        // search (required for optimality proofs)
        // determine the best possible lower bounds.
        return EXHAUSTIVE_BFS;
    }

    // Apply best first search and and any lower
    // bounding procedure which is convenient
    return CONSTRUCT_BFS;
}


template <class TItem,class TObj>
branchNode<TItem,TObj> *branchScheme<TItem,TObj>::SelectActiveNode() throw()
{
    branchNode<TItem,TObj> *retNode = firstActive;

    if (SearchState()!=CONSTRUCT_BFS && SearchState()!=EXHAUSTIVE_BFS)
    {
        firstActive = firstActive->succNode;
        nDFS++;

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH,"  DEPTH");

        #endif
    }
    else
    {
        branchNode<TItem,TObj> *thisNode = firstActive;
        branchNode<TItem,TObj> *bestPredNode = NULL;
        bestBound = firstActive->Objective();
        nDFS = 0;

        while (thisNode)
        {
            branchNode<TItem,TObj> *predNode = thisNode;
            thisNode = thisNode->succNode;

            if (thisNode && sign*(thisNode->Objective())<sign*bestBound)
            {
                bestPredNode = predNode;
                bestBound = thisNode->Objective();
            }
        }

        if (!bestPredNode) firstActive = firstActive->succNode;
        else
        {
            retNode = bestPredNode->succNode;
            bestPredNode->succNode = retNode->succNode;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && CT.logGaps==0) LogEnd(LH,"  BEST");

        #endif
    }

    bestBound = savedObjective;
    branchNode<TItem,TObj> *thisNode = firstActive;

    while (thisNode)
    {
        if (sign*(thisNode->Objective())<sign*bestBound)
            bestBound = thisNode->Objective();

        thisNode = thisNode->succNode;
    }

    nActive--;

    return retNode;
}


template <class TItem,class TObj>
void branchScheme<TItem,TObj>::StripQueue() throw()
{
    branchNode<TItem,TObj> *thisNode = firstActive;

    while (thisNode)
    {
        branchNode<TItem,TObj> *predNode = thisNode;
        thisNode = thisNode->succNode;

        if (thisNode &&
            sign*(thisNode->Objective())>sign*savedObjective+CT.epsilon-1
           )
        {
            predNode->succNode = thisNode->succNode;
            delete thisNode;
            thisNode = predNode->succNode;
            nActive--;
        }
    }
}


#ifndef DO_NOT_DOCUMENT_THIS

template class branchNode<TIndex,TFloat>;
template class branchScheme<TIndex,TFloat>;

#ifndef _BIG_NODES_

template class branchNode<TNode,TFloat>;
template class branchScheme<TNode,TFloat>;

#endif // _BIG_NODES_

#endif // DO_NOT_DOCUMENT_THIS
