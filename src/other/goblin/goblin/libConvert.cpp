
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libConvert.cpp
/// \brief  Procedures for converting computational results to other data structures

#include "abstractMixedGraph.h"


void abstractMixedGraph::AddToSubgraph(TNode v) throw(ERRange,ERRejected)
{
    TArc* pred = GetPredecessors();

    #if defined(_FAILSAVE_)

    if (v>=n && v!=NoNode) NoSuchNode("AddToSubgraph",v);

    if (!pred) Error(ERR_REJECTED,"AddToSubgraph","Missing predecessor labels");

    #endif

    if (v==NoNode)
    {
        for (TNode w=0;w<n;w++)
        {
            TArc a = pred[w];

            if (a!=NoArc && LCap(a)==0) SetSub(a,UCap(a));
        }
    }
    else
    {
        TNode u = v;
        TArc a = NoArc;

        while ((a = pred[u])!=NoArc && (u = StartNode(a))!=v) SetSub(a,UCap(a));

        if (a!=NoArc) SetSub(a,UCap(a));
    }
}


TNode abstractMixedGraph::ExtractTrees() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting forest from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TNode count = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        if (pred[v]==NoArc)
        {
            TNode w = v;
            count++;

            while (I.Active(w) || w!=v)
            {
                if (I.Active(w))
                {
                    TArc a = I.Read(w);
                    TNode u = EndNode(a);

                    if (Sub(a)>CT.epsilon && a!=(pred[w]^1))
                    {
                        if (pred[u]==NoArc)
                        {
                            pred[u] = a;
                            w = u;
                        }
                        else
                        {
                            #if defined(_LOGGING_)

                            LogEntry(LOG_RES2,"...Subgraph contains cycles");

                            #endif

                            return NoNode;
                        }
                    }
                }
                else w = StartNode(pred[w]);
            }
        }
    }

    Close(H);

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Forest has %lu components",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    return count;
}


bool abstractMixedGraph::ExtractTree(TNode r,TOptMST characteristic)
    throw(ERRejected)
{
    TArc* pred = RawPredecessors();

    return ExtractTree(pred,r,characteristic);
}


bool abstractMixedGraph::ExtractTree(TArc* pred,TNode r,TOptMST characteristic)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"ExtractTree","Missing predecessor labels");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting tree from subgraph...");

    #endif

    for (TNode v=0;v<n;v++) pred[v] = NoArc;

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = r;

    while (I.Active(w) || w!=r)
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode u = EndNode(a);

            if (   Sub(a)>CT.epsilon
                && a!=(pred[w]^1)
                && (!Blocking(a) || (characteristic & MST_UNDIRECTED)) )
            {
                if (pred[u]==NoArc)
                {
                    pred[u] = a;

                    if (u!=r) w = u;
                }
                else
                {
                    Close(H);

                    #if defined(_LOGGING_)

                    LogEntry(LOG_RES2,"...Subgraph is neither a tree nor a one cycle tree");

                    #endif

                    return false;
                }
            }
        }
        else w = StartNode(pred[w]);
    }

    Close(H);

    for (TNode w=0;w<n;w++)
    {
        if (w!=r && pred[w]==NoArc)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is disconnected");

            #endif

            return false;
        }
    }

    if (pred[r]==NoArc)
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_RES2,"...Subgraph is a tree");

        #endif

        if (characteristic & MST_ONE_CYCLE)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is not a one cycle tree");

            #endif

            return false;
        }
    }
    else
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_RES2,"...Subgraph is a one cycle tree");

        #endif

        if (!(characteristic & MST_ONE_CYCLE))
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is not a tree");

            #endif

            return false;
        }
    }

    return true;
}


TNode abstractMixedGraph::ExtractPath(TNode u,TNode v) throw()
{
    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Extracting (%lu,%lu)-path from subgraph...",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = u;
    TNode count = 0;
    TArc* pred = RawPredecessors();

    while (I.Active(w))
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode x = EndNode(a);

            if (Sub(a)>CT.epsilon && !Blocking(a) && a!=(pred[w]^1))
            {
                if (pred[x]==NoArc)
                {
                    pred[x] = a;
                    w = x;
                    count++;

                    if (w==v) break;
                }
                else
                {
                    Close(H);

                    #if defined(_LOGGING_)

                    LogEntry(LOG_RES2,"...Unexpected branch");

                    #endif

                    return NoNode;
                }
            }
        }
        else
        {
            Close(H);

            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Missing end node");

            #endif

            return NoNode;
        }
    }

    Close(H);

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Path of length %lu found",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return count;
}


TNode abstractMixedGraph::Extract1Matching() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting 1-factor from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TNode cardinality = 0;

    for (TArc a=0;a<m;a++)
    {
        if (Sub(2*a)>CT.epsilon)
        {
            TNode u = StartNode(2*a);
            TNode v = EndNode(2*a);

            if (pred[u]==NoArc && pred[v]==NoArc && fabs(Sub(2*a)-1)<CT.epsilon)
            {
                pred[u] = 2*a+1;
                pred[v] = 2*a;
                cardinality++;
            }
            else
            {
                #if defined(_LOGGING_)

                LogEntry(LOG_RES2,"...Subgraph is not a 1-matching");

                #endif

                return NoNode;
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...1-matching of cardinality %lu found",
            static_cast<unsigned long>(cardinality));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return cardinality;
}


TNode abstractMixedGraph::ExtractEdgeCover() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting edge cover from subgraph...");

    #endif

    CT.SuppressLogging();
    TNode cardinality = Extract1Matching();
    CT.RestoreLogging();

    if (cardinality==NoNode) return NoNode;

    TArc* pred = GetPredecessors();

    for (TNode v=0;v<n;v++)
    {
        if (pred[v]!=NoArc) continue;

        if (First(v)!=NoArc)
        {
            pred[v] = (First(v)^1);
            cardinality++;
        }
        else
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Isolated vertex found");

            #endif

            return NoNode;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Edge cover of cardinality %lu found",
            static_cast<unsigned long>(cardinality));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return cardinality;
}


TNode abstractMixedGraph::ExtractCycles() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting 2-factor from subgraph...");

    #endif

    TArc* pred = InitPredecessors();

    TNode count = 0;
    for (TNode v=0;v<n;v++)
    {
        if (Deg(v)+DegIn(v)+DegOut(v)!=2 || DegIn(v)>1 || DegOut(v)>1)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"Subgraph is not a 2-factor");

            #endif

            return NoNode;
        }

        if (pred[v]==NoArc)
        {
            if (ExtractPath(v,v)==NoNode) return NoNode;

            count++;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Subgraph splits into %lu cycles",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return count;
}


void abstractMixedGraph::ExtractCut() throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();

    #if defined(_FAILSAVE_)

    if (*dist) Error(ERR_REJECTED,"ExtractCut","No distance labels found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting cut from distance labels...");

    #endif

    TNode* nodeColour = RawNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if (dist[v]==InfFloat) nodeColour[v] = 1;
        else nodeColour[v] = 0;
    }
}


void abstractMixedGraph::ExtractBipartition() throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();

    #if defined(_FAILSAVE_)

    if (!dist) Error(ERR_REJECTED,"ExtractBipartition","No distance labels found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting bipartition from distance labels...");

    #endif

    TNode* chi = InitNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if ((int(dist[v])&1) || dist[v]==InfFloat)
        {
            chi[v] = 1;
        }
        else chi[v] = 0;
    }
}


void abstractMixedGraph::ExtractColours() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (partition==NULL) Error(ERR_REJECTED,"ExtractColours","No partition found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting colours from node partition...");

    #endif

    TNode* nodeColour = InitNodeColours();

    TNode i = 0;
    for (TNode v=0;v<n;v++)
    {
        if (nodeColour[Find(v)]==NoNode) nodeColour[Find(v)] = i++;
        nodeColour[v] = nodeColour[Find(v)];
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...%lu colour classes found",
            static_cast<unsigned long>(i));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libConvert.cpp
/// \brief  Procedures for converting computational results to other data structures

#include "abstractMixedGraph.h"


void abstractMixedGraph::AddToSubgraph(TNode v) throw(ERRange,ERRejected)
{
    TArc* pred = GetPredecessors();

    #if defined(_FAILSAVE_)

    if (v>=n && v!=NoNode) NoSuchNode("AddToSubgraph",v);

    if (!pred) Error(ERR_REJECTED,"AddToSubgraph","Missing predecessor labels");

    #endif

    if (v==NoNode)
    {
        for (TNode w=0;w<n;w++)
        {
            TArc a = pred[w];

            if (a!=NoArc && LCap(a)==0) SetSub(a,UCap(a));
        }
    }
    else
    {
        TNode u = v;
        TArc a = NoArc;

        while ((a = pred[u])!=NoArc && (u = StartNode(a))!=v) SetSub(a,UCap(a));

        if (a!=NoArc) SetSub(a,UCap(a));
    }
}


TNode abstractMixedGraph::ExtractTrees() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting forest from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TNode count = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        if (pred[v]==NoArc)
        {
            TNode w = v;
            count++;

            while (I.Active(w) || w!=v)
            {
                if (I.Active(w))
                {
                    TArc a = I.Read(w);
                    TNode u = EndNode(a);

                    if (Sub(a)>CT.epsilon && a!=(pred[w]^1))
                    {
                        if (pred[u]==NoArc)
                        {
                            pred[u] = a;
                            w = u;
                        }
                        else
                        {
                            #if defined(_LOGGING_)

                            LogEntry(LOG_RES2,"...Subgraph contains cycles");

                            #endif

                            return NoNode;
                        }
                    }
                }
                else w = StartNode(pred[w]);
            }
        }
    }

    Close(H);

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Forest has %lu components",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    return count;
}


bool abstractMixedGraph::ExtractTree(TNode r,TOptMST characteristic)
    throw(ERRejected)
{
    TArc* pred = RawPredecessors();

    return ExtractTree(pred,r,characteristic);
}


bool abstractMixedGraph::ExtractTree(TArc* pred,TNode r,TOptMST characteristic)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"ExtractTree","Missing predecessor labels");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting tree from subgraph...");

    #endif

    for (TNode v=0;v<n;v++) pred[v] = NoArc;

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = r;

    while (I.Active(w) || w!=r)
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode u = EndNode(a);

            if (   Sub(a)>CT.epsilon
                && a!=(pred[w]^1)
                && (!Blocking(a) || (characteristic & MST_UNDIRECTED)) )
            {
                if (pred[u]==NoArc)
                {
                    pred[u] = a;

                    if (u!=r) w = u;
                }
                else
                {
                    Close(H);

                    #if defined(_LOGGING_)

                    LogEntry(LOG_RES2,"...Subgraph is neither a tree nor a one cycle tree");

                    #endif

                    return false;
                }
            }
        }
        else w = StartNode(pred[w]);
    }

    Close(H);

    for (TNode w=0;w<n;w++)
    {
        if (w!=r && pred[w]==NoArc)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is disconnected");

            #endif

            return false;
        }
    }

    if (pred[r]==NoArc)
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_RES2,"...Subgraph is a tree");

        #endif

        if (characteristic & MST_ONE_CYCLE)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is not a one cycle tree");

            #endif

            return false;
        }
    }
    else
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_RES2,"...Subgraph is a one cycle tree");

        #endif

        if (!(characteristic & MST_ONE_CYCLE))
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Subgraph is not a tree");

            #endif

            return false;
        }
    }

    return true;
}


TNode abstractMixedGraph::ExtractPath(TNode u,TNode v) throw()
{
    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Extracting (%lu,%lu)-path from subgraph...",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);
    TNode w = u;
    TNode count = 0;
    TArc* pred = RawPredecessors();

    while (I.Active(w))
    {
        if (I.Active(w))
        {
            TArc a = I.Read(w);
            TNode x = EndNode(a);

            if (Sub(a)>CT.epsilon && !Blocking(a) && a!=(pred[w]^1))
            {
                if (pred[x]==NoArc)
                {
                    pred[x] = a;
                    w = x;
                    count++;

                    if (w==v) break;
                }
                else
                {
                    Close(H);

                    #if defined(_LOGGING_)

                    LogEntry(LOG_RES2,"...Unexpected branch");

                    #endif

                    return NoNode;
                }
            }
        }
        else
        {
            Close(H);

            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Missing end node");

            #endif

            return NoNode;
        }
    }

    Close(H);

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Path of length %lu found",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return count;
}


TNode abstractMixedGraph::Extract1Matching() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting 1-factor from subgraph...");

    #endif

    TArc* pred = InitPredecessors();
    TNode cardinality = 0;

    for (TArc a=0;a<m;a++)
    {
        if (Sub(2*a)>CT.epsilon)
        {
            TNode u = StartNode(2*a);
            TNode v = EndNode(2*a);

            if (pred[u]==NoArc && pred[v]==NoArc && fabs(Sub(2*a)-1)<CT.epsilon)
            {
                pred[u] = 2*a+1;
                pred[v] = 2*a;
                cardinality++;
            }
            else
            {
                #if defined(_LOGGING_)

                LogEntry(LOG_RES2,"...Subgraph is not a 1-matching");

                #endif

                return NoNode;
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...1-matching of cardinality %lu found",
            static_cast<unsigned long>(cardinality));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return cardinality;
}


TNode abstractMixedGraph::ExtractEdgeCover() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting edge cover from subgraph...");

    #endif

    CT.SuppressLogging();
    TNode cardinality = Extract1Matching();
    CT.RestoreLogging();

    if (cardinality==NoNode) return NoNode;

    TArc* pred = GetPredecessors();

    for (TNode v=0;v<n;v++)
    {
        if (pred[v]!=NoArc) continue;

        if (First(v)!=NoArc)
        {
            pred[v] = (First(v)^1);
            cardinality++;
        }
        else
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"...Isolated vertex found");

            #endif

            return NoNode;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Edge cover of cardinality %lu found",
            static_cast<unsigned long>(cardinality));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return cardinality;
}


TNode abstractMixedGraph::ExtractCycles() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting 2-factor from subgraph...");

    #endif

    TArc* pred = InitPredecessors();

    TNode count = 0;
    for (TNode v=0;v<n;v++)
    {
        if (Deg(v)+DegIn(v)+DegOut(v)!=2 || DegIn(v)>1 || DegOut(v)>1)
        {
            #if defined(_LOGGING_)

            LogEntry(LOG_RES2,"Subgraph is not a 2-factor");

            #endif

            return NoNode;
        }

        if (pred[v]==NoArc)
        {
            if (ExtractPath(v,v)==NoNode) return NoNode;

            count++;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Subgraph splits into %lu cycles",
            static_cast<unsigned long>(count));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    return count;
}


void abstractMixedGraph::ExtractCut() throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();

    #if defined(_FAILSAVE_)

    if (*dist) Error(ERR_REJECTED,"ExtractCut","No distance labels found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting cut from distance labels...");

    #endif

    TNode* nodeColour = RawNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if (dist[v]==InfFloat) nodeColour[v] = 1;
        else nodeColour[v] = 0;
    }
}


void abstractMixedGraph::ExtractBipartition() throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();

    #if defined(_FAILSAVE_)

    if (!dist) Error(ERR_REJECTED,"ExtractBipartition","No distance labels found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting bipartition from distance labels...");

    #endif

    TNode* chi = InitNodeColours();

    for (TNode v=0;v<n;v++)
    {
        if ((int(dist[v])&1) || dist[v]==InfFloat)
        {
            chi[v] = 1;
        }
        else chi[v] = 0;
    }
}


void abstractMixedGraph::ExtractColours() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (partition==NULL) Error(ERR_REJECTED,"ExtractColours","No partition found");

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Extracting colours from node partition...");

    #endif

    TNode* nodeColour = InitNodeColours();

    TNode i = 0;
    for (TNode v=0;v<n;v++)
    {
        if (nodeColour[Find(v)]==NoNode) nodeColour[Find(v)] = i++;
        nodeColour[v] = nodeColour[Find(v)];
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...%lu colour classes found",
            static_cast<unsigned long>(i));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif
}
