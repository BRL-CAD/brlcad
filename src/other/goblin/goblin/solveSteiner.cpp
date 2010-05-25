
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveSteiner.cpp
/// \brief  Codes for Steiner tree enumeration and heuristics

#include "sparseGraph.h"
#include "sparseDigraph.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::SteinerTree(const indexSet<TNode>& Terminals,TNode root) throw(ERRange,ERRejected)
{
    if (root>=n) root = DefaultRootNode();

    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("SteinerTree",root);

    #endif

    if (root!=NoNode)
    {
        sprintf(CT.logBuffer,"Computing minimum %lu-Steiner tree...",static_cast<unsigned long>(root));
    }
    else
    {
        sprintf(CT.logBuffer,"Computing minimum Steiner tree...");
    }

    moduleGuard M(ModSteiner,*this,CT.logBuffer);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2,1);

    #endif

    TArc* savedTree = NULL;
    TFloat savedLength = 0;

    TArc* pred = GetPredecessors();

    if (pred)
    {
        bool feasible = true;

        TNode rank = 0;
        for (TNode v=0;v<n;v++)
        {
            TArc a = pred[v];
            if (a!=NoArc)
            {
                savedLength += Length(a);
                rank++;
                if (v==root) feasible = false;
            }
        }

        if (rank<n-1) feasible = false;

        // Must add a check for connectivity

        if (feasible)
        {
            savedLength -= STT_TrimLeaves(Terminals,pred);
            savedTree = new TArc[n];

            for (TNode v=0;v<n;v++)  savedTree[v] = pred[v];

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"Initial tree has length %g",savedLength);
                LogEntry(LOG_METH,CT.logBuffer);
            }
            M.SetUpperBound(savedLength);
        }
    }

    LogEntry(LOG_METH,"Applying Heuristic...");
    TFloat bestUpper = STT_Heuristic(Terminals,root);

    if (savedTree)
    {
        if (savedLength<bestUpper)
        {
            for (TNode v=0;v<n;v++)  pred[v] = savedTree[v];

            bestUpper = savedLength;
        }

        delete[] savedTree;
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    if (CT.methSolve==2)
    {
        LogEntry(LOG_METH,"Exhaustive search...");
        bestUpper = STT_Enumerate(Terminals,root);
    }

    if (bestUpper==InfFloat) throw ERRejected();

    return bestUpper;
}


TFloat abstractMixedGraph::STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Heuristic",CT.logBuffer);
    }

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n+1,n);

    #endif

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    TFloat ret = MinTree(root);

    #if defined(_PROGRESS_)

    M.ProgressStep(n);
    M.SetProgressNext(1);

    #endif

    ret -= STT_TrimLeaves(Terminals,GetPredecessors());

    M.SetUpperBound(ret);

    sprintf(CT.logBuffer,"...Steiner tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}


TFloat abstractMixedGraph::STT_TrimLeaves(const indexSet<TNode>& Terminals,TArc* pred) throw()
{
    moduleGuard M(ModSteiner,*this,"Trimming the leaves...",
        moduleGuard::SYNC_BOUNDS);

    TNode* odg = new TNode[n];

    for (TNode v=0;v<n;v++) odg[v] = 0;

    for (TNode v=0;v<n;v++)
        if (pred[v]!=NoArc) odg[StartNode(pred[v])]++;

    TFloat diff = 0;
    for (TNode v=0;v<n;v++)
    {
        TNode w = v;
        while (pred[w]!=NoArc && !Terminals.IsMember(w) && odg[w]==0)
        {
            diff += Length(pred[w]);
            TNode x = StartNode(pred[w]);
            odg[x]--;
            pred[w] = NoArc;

            sprintf(CT.logBuffer,"Node %lu deleted",static_cast<unsigned long>(w));
            LogEntry(LOG_METH,CT.logBuffer);

            w = x;
        }
    }

    delete[] odg;

    M.Trace();

    if (CT.logRes && diff!=0)
    {
        sprintf(CT.logBuffer,"...Tree length decreases by %g",diff);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return diff;
}


TFloat abstractMixedGraph::STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Enumerate",CT.logBuffer);
    }

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH2,"<Steiner node enumeration>");

    TNode k = 0;
    for (TNode v=0;v<n;v++)
        if (!Terminals.IsMember(v)) k++;

    sprintf(CT.logBuffer,"...%lu Steiner nodes detected",static_cast<unsigned long>(k));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();

    #ifdef _PROGRESS_

    M.InitProgressCounter(pow(2.0,double(k)),1);

    #endif

    TFloat ret = InfFloat;

    if (k==0) ret = MinTree(root);

    if (k==n)
    {
        InitPredecessors();
        ret = 0;
    }

    TNode* nodeColour = InitNodeColours(0);
    bool searching = (k!=0 && k!=n);
    unsigned long itCount = 0;

    while (CT.SolverRunning() && searching)
    {
        itCount++;

        completeOrientation G(*this);
        graphRepresentation* GR = G.Representation();

        for (TArc a=0;a<G.M();a++)
        {
            TArc a0 = G.OriginalOfArc(2*a);

            if (nodeColour[StartNode(a0)]==1)
            {
                GR -> SetLength(2*a,InfFloat);
            }

            if (nodeColour[EndNode(a0)]==1)
            {
                GR -> SetLength(2*a,0);
            }
        }

        #ifdef _TRACING_

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==1) G.SetNodeVisibility(v,false);

        #endif

        G.MinTree(root);

        TArc* thisPred = G.GetPredecessors();
        TFloat thisRet = 0;

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                thisRet += Length(thisPred[v]);

        if (thisRet<ret)
        {
            TNode uncovered = 0;
            TArc* thisPred = G.GetPredecessors();

            for (TNode v=0;v<n;v++)
                if (Terminals.IsMember(v) && thisPred[v]==NoArc) uncovered++;

            if (uncovered==1)
            {
                ret = thisRet;
                TArc* pred = InitPredecessors();

                for (TNode v=0;v<n;v++)
                    if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                        pred[v] = G.OriginalOfArc(thisPred[v]);

                CT.RestoreLogging();
                M.SetUpperBound(ret);
                CT.SuppressLogging();
            }
        }

        if (M.LowerBound()>=ret) break;

        TNode v = n;
        searching = false;

        while (v>0)
        {
            v--;

            if (Terminals.IsMember(v)) continue;

            if (nodeColour[v]==1)
            {
                nodeColour[v] = 0;
            }
            else
            {
                nodeColour[v] = 1;
                searching = true;
                break;
            }
        }

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        M.Trace(G,0);
    }

    CT.RestoreLogging();
    sprintf(CT.logBuffer,"...Solved %lu spanning tree problems",itCount);
    LogEntry(LOG_METH,CT.logBuffer);

    sprintf(CT.logBuffer,"...Best tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}


TFloat abstractGraph::STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Heuristic",CT.logBuffer);
    }

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    if (root==NoNode)
        Error(ERR_REJECTED,"STT_Heuristic","No terminal node found");

    #endif

    moduleGuard M(ModMehlhorn,*this,moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(3,1);

    #endif

    voronoiDiagram G(*this,Terminals);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    TFloat lower = ceil(G.MinTree(MST_PRIM2,MST_PLAIN)/(2*(1-1/TFloat(G.N()))));
    M.SetLowerBound(lower);

    G.UpdateSubgraph();

    M.Trace(1);

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting tree from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TFloat length = 0;

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = root;

    while (I.Active(w) || w!=root)
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode u = EndNode(a);

            if (Sub(a)>0 && a!=(pred[w]^1))
            {
                if (pred[u]==NoArc)
                {
                    pred[u] = a;
                    length += Length(a);
                    if (u!=root) w = u;
                }
            }
        }
        else w = StartNode(pred[w]);
    }

    Close(H);

    M.SetUpperBound(length);

    M.Trace(1);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Steiner tree has length %g",length);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return length;
}


TFloat abstractGraph::STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Enumerate",CT.logBuffer);
    }

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    if (root==NoNode)
        Error(ERR_REJECTED,"STT_Enumerate","No terminal node found");

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH2,"<Steiner node enumeration>");

    TNode k = 0;
    for (TNode v=0;v<n;v++)
        if (!Terminals.IsMember(v)) k++;

    sprintf(CT.logBuffer,"...%lu Steiner nodes detected",static_cast<unsigned long>(k));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();

    #if defined(_PROGRESS_)

    M.InitProgressCounter(pow(2.0,double(k)),1);

    #endif

    TFloat ret = InfFloat;

    if (k==0) ret = MinTree(MST_PRIM2,MST_PLAIN,root);

    if (k==n)
    {
        InitPredecessors();
        ret = 0;
    }

    TNode* nodeColour = InitNodeColours(0);
    bool searching = (k!=0 && k!=n);
    unsigned long itCount = 0;

    while (CT.SolverRunning() && searching)
    {
        itCount++;

        sparseGraph G(*this,OPT_CLONE);
        graphRepresentation* GR = G.Representation();

        for (TArc a=0;a<2*m;a++)
        {
            if (nodeColour[StartNode(a)]==1)
            {
                GR -> SetLength(a,InfFloat);
            }
        }

        #ifdef _TRACING_

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==1) G.SetNodeVisibility(v,false);

        #endif

        G.MinTree(MST_PRIM2,MST_PLAIN,root);
        TArc* thisPred = G.GetPredecessors();

        TFloat thisRet = 0;

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                thisRet += Length(thisPred[v]);

        if (thisRet<ret)
        {
            TNode uncovered = 0;
            for (TNode v=0;v<n;v++)
                if (Terminals.IsMember(v) && thisPred[v]==NoArc) uncovered++;

            if (uncovered==1)
            {
                ret = thisRet;
                TArc* pred = InitPredecessors();

                for (TNode v=0;v<n;v++)
                    if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                        pred[v] = thisPred[v];
                CT.RestoreLogging();
                M.SetUpperBound(ret);
                CT.SuppressLogging();
            }

            if (M.LowerBound()>=ret) break;
        }

        TNode v = n;
        searching = false;

        while (v>0)
        {
            v--;

            if (Terminals.IsMember(v)) continue;

            if (nodeColour[v]==1)
            {
                nodeColour[v] = 0;
            }
            else
            {
                nodeColour[v] = 1;
                searching = true;
                break;
            }
        }

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        M.Trace(G,0);
    }

    CT.RestoreLogging();
    sprintf(CT.logBuffer,"...Solved %lu spanning tree problems",itCount);
    LogEntry(LOG_METH,CT.logBuffer);

    sprintf(CT.logBuffer,"...Best tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveSteiner.cpp
/// \brief  Codes for Steiner tree enumeration and heuristics

#include "sparseGraph.h"
#include "sparseDigraph.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::SteinerTree(const indexSet<TNode>& Terminals,TNode root) throw(ERRange,ERRejected)
{
    if (root>=n) root = DefaultRootNode();

    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("SteinerTree",root);

    #endif

    if (root!=NoNode)
    {
        sprintf(CT.logBuffer,"Computing minimum %lu-Steiner tree...",static_cast<unsigned long>(root));
    }
    else
    {
        sprintf(CT.logBuffer,"Computing minimum Steiner tree...");
    }

    moduleGuard M(ModSteiner,*this,CT.logBuffer);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2,1);

    #endif

    TArc* savedTree = NULL;
    TFloat savedLength = 0;

    TArc* pred = GetPredecessors();

    if (pred)
    {
        bool feasible = true;

        TNode rank = 0;
        for (TNode v=0;v<n;v++)
        {
            TArc a = pred[v];
            if (a!=NoArc)
            {
                savedLength += Length(a);
                rank++;
                if (v==root) feasible = false;
            }
        }

        if (rank<n-1) feasible = false;

        // Must add a check for connectivity

        if (feasible)
        {
            savedLength -= STT_TrimLeaves(Terminals,pred);
            savedTree = new TArc[n];

            for (TNode v=0;v<n;v++)  savedTree[v] = pred[v];

            if (CT.logMeth)
            {
                sprintf(CT.logBuffer,"Initial tree has length %g",savedLength);
                LogEntry(LOG_METH,CT.logBuffer);
            }
            M.SetUpperBound(savedLength);
        }
    }

    LogEntry(LOG_METH,"Applying Heuristic...");
    TFloat bestUpper = STT_Heuristic(Terminals,root);

    if (savedTree)
    {
        if (savedLength<bestUpper)
        {
            for (TNode v=0;v<n;v++)  pred[v] = savedTree[v];

            bestUpper = savedLength;
        }

        delete[] savedTree;
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    if (CT.methSolve==2)
    {
        LogEntry(LOG_METH,"Exhaustive search...");
        bestUpper = STT_Enumerate(Terminals,root);
    }

    if (bestUpper==InfFloat) throw ERRejected();

    return bestUpper;
}


TFloat abstractMixedGraph::STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Heuristic",CT.logBuffer);
    }

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n+1,n);

    #endif

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    TFloat ret = MinTree(root);

    #if defined(_PROGRESS_)

    M.ProgressStep(n);
    M.SetProgressNext(1);

    #endif

    ret -= STT_TrimLeaves(Terminals,GetPredecessors());

    M.SetUpperBound(ret);

    sprintf(CT.logBuffer,"...Steiner tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}


TFloat abstractMixedGraph::STT_TrimLeaves(const indexSet<TNode>& Terminals,TArc* pred) throw()
{
    moduleGuard M(ModSteiner,*this,"Trimming the leaves...",
        moduleGuard::SYNC_BOUNDS);

    TNode* odg = new TNode[n];

    for (TNode v=0;v<n;v++) odg[v] = 0;

    for (TNode v=0;v<n;v++)
        if (pred[v]!=NoArc) odg[StartNode(pred[v])]++;

    TFloat diff = 0;
    for (TNode v=0;v<n;v++)
    {
        TNode w = v;
        while (pred[w]!=NoArc && !Terminals.IsMember(w) && odg[w]==0)
        {
            diff += Length(pred[w]);
            TNode x = StartNode(pred[w]);
            odg[x]--;
            pred[w] = NoArc;

            sprintf(CT.logBuffer,"Node %lu deleted",static_cast<unsigned long>(w));
            LogEntry(LOG_METH,CT.logBuffer);

            w = x;
        }
    }

    delete[] odg;

    M.Trace();

    if (CT.logRes && diff!=0)
    {
        sprintf(CT.logBuffer,"...Tree length decreases by %g",diff);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return diff;
}


TFloat abstractMixedGraph::STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Enumerate",CT.logBuffer);
    }

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH2,"<Steiner node enumeration>");

    TNode k = 0;
    for (TNode v=0;v<n;v++)
        if (!Terminals.IsMember(v)) k++;

    sprintf(CT.logBuffer,"...%lu Steiner nodes detected",static_cast<unsigned long>(k));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();

    #ifdef _PROGRESS_

    M.InitProgressCounter(pow(2.0,double(k)),1);

    #endif

    TFloat ret = InfFloat;

    if (k==0) ret = MinTree(root);

    if (k==n)
    {
        InitPredecessors();
        ret = 0;
    }

    TNode* nodeColour = InitNodeColours(0);
    bool searching = (k!=0 && k!=n);
    unsigned long itCount = 0;

    while (CT.SolverRunning() && searching)
    {
        itCount++;

        completeOrientation G(*this);
        graphRepresentation* GR = G.Representation();

        for (TArc a=0;a<G.M();a++)
        {
            TArc a0 = G.OriginalOfArc(2*a);

            if (nodeColour[StartNode(a0)]==1)
            {
                GR -> SetLength(2*a,InfFloat);
            }

            if (nodeColour[EndNode(a0)]==1)
            {
                GR -> SetLength(2*a,0);
            }
        }

        #ifdef _TRACING_

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==1) G.SetNodeVisibility(v,false);

        #endif

        G.MinTree(root);

        TArc* thisPred = G.GetPredecessors();
        TFloat thisRet = 0;

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                thisRet += Length(thisPred[v]);

        if (thisRet<ret)
        {
            TNode uncovered = 0;
            TArc* thisPred = G.GetPredecessors();

            for (TNode v=0;v<n;v++)
                if (Terminals.IsMember(v) && thisPred[v]==NoArc) uncovered++;

            if (uncovered==1)
            {
                ret = thisRet;
                TArc* pred = InitPredecessors();

                for (TNode v=0;v<n;v++)
                    if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                        pred[v] = G.OriginalOfArc(thisPred[v]);

                CT.RestoreLogging();
                M.SetUpperBound(ret);
                CT.SuppressLogging();
            }
        }

        if (M.LowerBound()>=ret) break;

        TNode v = n;
        searching = false;

        while (v>0)
        {
            v--;

            if (Terminals.IsMember(v)) continue;

            if (nodeColour[v]==1)
            {
                nodeColour[v] = 0;
            }
            else
            {
                nodeColour[v] = 1;
                searching = true;
                break;
            }
        }

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        M.Trace(G,0);
    }

    CT.RestoreLogging();
    sprintf(CT.logBuffer,"...Solved %lu spanning tree problems",itCount);
    LogEntry(LOG_METH,CT.logBuffer);

    sprintf(CT.logBuffer,"...Best tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}


TFloat abstractGraph::STT_Heuristic(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Heuristic",CT.logBuffer);
    }

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    if (root==NoNode)
        Error(ERR_REJECTED,"STT_Heuristic","No terminal node found");

    #endif

    moduleGuard M(ModMehlhorn,*this,moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(3,1);

    #endif

    voronoiDiagram G(*this,Terminals);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    TFloat lower = ceil(G.MinTree(MST_PRIM2,MST_PLAIN)/(2*(1-1/TFloat(G.N()))));
    M.SetLowerBound(lower);

    G.UpdateSubgraph();

    M.Trace(1);

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting tree from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TFloat length = 0;

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = root;

    while (I.Active(w) || w!=root)
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode u = EndNode(a);

            if (Sub(a)>0 && a!=(pred[w]^1))
            {
                if (pred[u]==NoArc)
                {
                    pred[u] = a;
                    length += Length(a);
                    if (u!=root) w = u;
                }
            }
        }
        else w = StartNode(pred[w]);
    }

    Close(H);

    M.SetUpperBound(length);

    M.Trace(1);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Steiner tree has length %g",length);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return length;
}


TFloat abstractGraph::STT_Enumerate(const indexSet<TNode>& Terminals,TNode root) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (root!=NoNode && (root>=n || !Terminals.IsMember(root)))
    {
        sprintf(CT.logBuffer,"Inappropriate root node: %lu",static_cast<unsigned long>(root));
        Error(ERR_RANGE,"STT_Enumerate",CT.logBuffer);
    }

    for (TNode v=0;root==NoNode && v<n;v++)
        if (Terminals.IsMember(v)) root = v;

    if (root==NoNode)
        Error(ERR_REJECTED,"STT_Enumerate","No terminal node found");

    #endif

    moduleGuard M(ModSteiner,*this,moduleGuard::SYNC_BOUNDS);

    LogEntry(LOG_METH2,"<Steiner node enumeration>");

    TNode k = 0;
    for (TNode v=0;v<n;v++)
        if (!Terminals.IsMember(v)) k++;

    sprintf(CT.logBuffer,"...%lu Steiner nodes detected",static_cast<unsigned long>(k));
    LogEntry(LOG_METH,CT.logBuffer);
    CT.SuppressLogging();

    #if defined(_PROGRESS_)

    M.InitProgressCounter(pow(2.0,double(k)),1);

    #endif

    TFloat ret = InfFloat;

    if (k==0) ret = MinTree(MST_PRIM2,MST_PLAIN,root);

    if (k==n)
    {
        InitPredecessors();
        ret = 0;
    }

    TNode* nodeColour = InitNodeColours(0);
    bool searching = (k!=0 && k!=n);
    unsigned long itCount = 0;

    while (CT.SolverRunning() && searching)
    {
        itCount++;

        sparseGraph G(*this,OPT_CLONE);
        graphRepresentation* GR = G.Representation();

        for (TArc a=0;a<2*m;a++)
        {
            if (nodeColour[StartNode(a)]==1)
            {
                GR -> SetLength(a,InfFloat);
            }
        }

        #ifdef _TRACING_

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==1) G.SetNodeVisibility(v,false);

        #endif

        G.MinTree(MST_PRIM2,MST_PLAIN,root);
        TArc* thisPred = G.GetPredecessors();

        TFloat thisRet = 0;

        for (TNode v=0;v<n;v++)
            if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                thisRet += Length(thisPred[v]);

        if (thisRet<ret)
        {
            TNode uncovered = 0;
            for (TNode v=0;v<n;v++)
                if (Terminals.IsMember(v) && thisPred[v]==NoArc) uncovered++;

            if (uncovered==1)
            {
                ret = thisRet;
                TArc* pred = InitPredecessors();

                for (TNode v=0;v<n;v++)
                    if (nodeColour[v]==0 && v!=root && thisPred[v]!=NoArc)
                        pred[v] = thisPred[v];
                CT.RestoreLogging();
                M.SetUpperBound(ret);
                CT.SuppressLogging();
            }

            if (M.LowerBound()>=ret) break;
        }

        TNode v = n;
        searching = false;

        while (v>0)
        {
            v--;

            if (Terminals.IsMember(v)) continue;

            if (nodeColour[v]==1)
            {
                nodeColour[v] = 0;
            }
            else
            {
                nodeColour[v] = 1;
                searching = true;
                break;
            }
        }

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        M.Trace(G,0);
    }

    CT.RestoreLogging();
    sprintf(CT.logBuffer,"...Solved %lu spanning tree problems",itCount);
    LogEntry(LOG_METH,CT.logBuffer);

    sprintf(CT.logBuffer,"...Best tree has length %g",ret);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}
