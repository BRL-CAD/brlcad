
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, August 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file solvePlanarity.cpp
/// \brief Planarity tests and combinatorial embedding of planar graphs


#include "mixedGraph.h"
#include "staticQueue.h"
#include "staticStack.h"
#include "subgraph.h"
#include "segment.h"
#include "sparseBigraph.h"
#include "moduleGuard.h"

#include <exception>

enum TEdgeType {
    FreeEdge = 0,
    TreeEdge = 1,
    BackEdge = 2
};


// Requires 2-connectivity
void preparingDFS(abstractMixedGraph& G, TNode v,
                  attribute<bool>& V_T, int& card_V_T,
                  attribute<int>& EdgeType, attribute<TArc>& pred,
                  attribute<TNode>& PreO, attribute<TNode>& PostO, int& POST,
                  attribute<TNode>& Low1, attribute<TNode>& Low2)
{
    TArc Adj_v = G.First(v);
    TNode w;

    do {
        w = G.EndNode(Adj_v);

        if (V_T[w] == false) {
            V_T.SetValue(w,true);
            EdgeType.SetValue(Adj_v, TreeEdge);
            card_V_T++;
            pred.SetValue(w,Adj_v);
            PreO.SetValue(w,card_V_T);
            preparingDFS(G,w,V_T,card_V_T,EdgeType,pred,PreO,PostO,POST,Low1,Low2);
        }
        else if ((pred.GetValue(v) != NoArc) &&
                 (PreO[w] < PreO[G.StartNode(pred.GetValue(v))] )) {
            EdgeType.SetValue(Adj_v, BackEdge);
            Low1.SetValue(Adj_v>>1, PreO[w]);
            Low2.SetValue(Adj_v>>1, PreO[v]);
        }
        Adj_v = G.Right(Adj_v,NoNode);
    } while (Adj_v != G.First(v));

    PostO.SetValue(v,POST);
    POST++;

    TNode L1, L2;
    TNode a, b;
    TArc Adj_w;
    Adj_v = G.First(v);
    do {
        if (EdgeType[Adj_v] == TreeEdge) {
            w = G.EndNode(Adj_v);
            L1 = PreO[w];
            L2 = PreO[w];
            Adj_w = G.First(w);
            do {
                a = G.StartNode(Adj_w);
                b = G.EndNode(Adj_w);
                if ((EdgeType[Adj_w]==BackEdge) || (EdgeType[Adj_w]==TreeEdge)) {
                    if (Low1[Adj_w>>1] < L1) {
                        L2 = L1;
                        L1 = Low1[Adj_w>>1];
                    }
                    else if ((Low1[Adj_w>>1] > L1) && (Low1[Adj_w>>1] < L2)) {
                        L2 = Low1[Adj_w>>1];
                    }
                    if (EdgeType[Adj_w] == TreeEdge) {
                        if (Low2[Adj_w>>1] < L2) {
                            L2 = Low2[Adj_w>>1];
                        }
                    }
                }
                Adj_w = G.Right(Adj_w,NoNode);
            } while (Adj_w != G.First(w));
            Low1.SetValue(Adj_v>>1, L1);
            if ((PreO[G.EndNode(Adj_v)]!=L1) && (PreO[G.EndNode(Adj_v)]<L2))
                L2 = PreO[G.EndNode(Adj_v)];
            Low2.SetValue(Adj_v>>1, L2);
        }
        Adj_v = G.Right(Adj_v,NoNode);
    } while (Adj_v != G.First(v));
}


void reorder(abstractMixedGraph& G, attribute<int>& EdgeType,
        attribute<TNode>& PreO, attribute<TNode>& Low1, attribute<TNode>& Low2)
{
    TArc* bucket_first = new TArc[2*G.N()];
    for (TNode i=0; i<2*G.N(); i++) bucket_first[i] = NoArc;

    TArc* bucket_next  = new TArc[G.M()];
    for (TArc i=0; i<G.M(); i++) bucket_next[i] = NoArc;

    TArc e;
    for (e=0; e<2*G.M(); e++)
    {
        if (EdgeType.GetValue(e) != FreeEdge)
        {
            TIndex l = 2 * Low1.GetValue(e>>1);

            if (   EdgeType.GetValue(e) != BackEdge
                && Low2.GetValue(e>>1) < PreO[G.StartNode(e)]
               )
            {
                ++l;
            }

            if (bucket_first[l] != NoArc) bucket_next[e>>1] = bucket_first[l];

            bucket_first[l] = e;
        }
    }

    TArc* Last = new TArc[G.N()];
    for (TNode i=0; i<G.N(); i++) Last[i] = NoArc;

    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    for (TNode l=0; l<2*G.N(); l++)
    {
        e = bucket_first[l];
        while (e != NoArc)
        {
            if (EdgeType[e] == FreeEdge)
                throw exception();
            if (Last[G.StartNode(e)] == NoArc)
                GR->SetFirst(G.StartNode(e), e);
            else
                GR->SetRight(Last[G.StartNode(e)], e);
            Last[G.StartNode(e)] = e;

            e = bucket_next[e>>1];
        }
    }

    delete[] bucket_first;
    delete[] bucket_next;
    delete[] Last;
}



class TSegPath {
public:
    TArc              e;
    TArc              Low1Edge;
    // needed by Kuratowski algorithm
    size_t            Emanat_Index;
    // needed by embedding algorithm
    bool              alphaL;
    // children of this TSegPath
    vector<TSegPath*> Emanat;

    // temporary set by function stronglyplanar()
    list<TNode>       Att;

    TSegPath(TArc StartArc);
    ~TSegPath();
};

TSegPath::TSegPath(TArc StartArc) {
    e = StartArc;
    Low1Edge = NoArc;
    Emanat_Index = size_t(-1);
    alphaL = false;
    Emanat = vector<TSegPath*>();
    Att = list<TNode>();
}

TSegPath::~TSegPath()
{
    for (size_t i=0; i<Emanat.size(); i++)
        delete Emanat[i];
    Emanat.clear();
}


TSegPath* createSegPath(abstractMixedGraph& G, attribute<TNode>& PreO,
    attribute<int>& EdgeType, attribute<TNode>& Low1, attribute<TNode>& Low2,
    const TArc StartArc)
{
    G.OpenFold();
    sprintf(G.Context().logBuffer,"Create Seg(%lu,%lu): ",
        static_cast<unsigned long>(G.StartNode(StartArc)),
        static_cast<unsigned long>(G.EndNode(StartArc)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    TSegPath* Seg = new TSegPath(StartArc);
    vector<TSegPath*>::iterator EmanatPos = Seg->Emanat.begin();

    TArc g = StartArc;
    while (EdgeType[g] == TreeEdge)
    {
        g = G.First(G.EndNode(g));

        sprintf(G.Context().logBuffer,"add edge (%lu,%lu)",
            static_cast<unsigned long>(G.StartNode(g)),
            static_cast<unsigned long>(G.EndNode(g)));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);

        TArc e = G.Right(g,NoNode);
        while (e != g)
        {
            if ((EdgeType[e] == BackEdge) || (EdgeType[e] == TreeEdge))
            {
                TSegPath* ChildSeg = createSegPath(G,PreO,EdgeType,Low1,Low2,e);
                EmanatPos = Seg->Emanat.insert(EmanatPos,ChildSeg);
                EmanatPos++;
            }
            e = G.Right(e,NoNode);
        }

        if (!Seg->Emanat.empty())
            EmanatPos = Seg->Emanat.begin();
    }

    Seg->Low1Edge = g;
    for (TIndex i=0; i<Seg->Emanat.size(); i++)
        Seg->Emanat[i]->Emanat_Index = i;

    G.LogEntry(LOG_METH2, ";");
    G.CloseFold();
    return Seg;
}



struct TSegGrCompSide {
    list<TNode>     Att;        //absteigend sortiert
    list<TSegPath*> Segments;   //wie in Emanat sortiert
};

struct TSegGrComp {
    TSegGrCompSide* L;
    TSegGrCompSide* R;
};


void printList(abstractMixedGraph& G,list<TNode>& l)
{
    THandle LH = G.LogStart(LOG_METH2, "{");
    for (list<TNode>::iterator i=l.begin(); i!=l.end(); i++) {
        sprintf(G.Context().logBuffer,"%lu ",static_cast<unsigned long>(*i));
        G.LogAppend(LH, G.Context().logBuffer);
    }
    G.LogEnd(LH, "}");
}


void printPathTree(abstractMixedGraph& G, attribute<int>& EdgeType, TSegPath* Seg)
{
    TArc g = Seg->e;
    sprintf(G.Context().logBuffer," (%lu,",static_cast<unsigned long>(G.StartNode(g)));
    THandle LH = G.LogStart(LOG_METH2, G.Context().logBuffer);

    while (EdgeType.GetValue(g) == TreeEdge) {
        sprintf(G.Context().logBuffer,"%lu,",static_cast<unsigned long>(G.EndNode(g)));
        G.LogAppend(LH, G.Context().logBuffer);
        g = G.First(G.EndNode(g));
    }
    sprintf(G.Context().logBuffer,"%lu)",static_cast<unsigned long>(G.EndNode(g)));
    G.LogEnd(LH, G.Context().logBuffer);

    vector<TSegPath*>::iterator EmanatPos = Seg->Emanat.begin();
    for (; EmanatPos != Seg->Emanat.end(); EmanatPos++)
        printPathTree(G,EdgeType,*EmanatPos);
}


void printSegGrCs(abstractMixedGraph& G, attribute<TNode>& PreO,
                  list<TSegGrComp>& SGCs)
{
    sprintf(G.Context().logBuffer,"current SG:");
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    G.OpenFold();
    size_t k = SGCs.size() - 1;
    for (list<TSegGrComp>::iterator C=SGCs.begin(); C!=SGCs.end(); C++) {
        sprintf(G.Context().logBuffer,"Att(L(C_%u))=", k);
        G.LogEntry(LOG_METH2, G.Context().logBuffer);
        printList(G,C->L->Att);

        sprintf(G.Context().logBuffer,"Att(R(C_%u))=", k);
        G.LogEntry(LOG_METH2, G.Context().logBuffer);
        printList(G,C->R->Att);

        sprintf(G.Context().logBuffer,"L(C_%u)={", k);
        THandle LH = G.LogStart(LOG_METH2, G.Context().logBuffer);
        list<TSegPath*>::iterator S;
        for (S=C->L->Segments.begin(); S!=C->L->Segments.end(); S++) {
            sprintf(G.Context().logBuffer,"Seg(%lu,%lu),",
                static_cast<unsigned long>(PreO.GetValue(G.StartNode((*S)->e))),
                static_cast<unsigned long>(PreO.GetValue(G.EndNode((*S)->e))));
            G.LogAppend(LH, G.Context().logBuffer);
        }
        G.LogEnd(LH, "}");

        sprintf(G.Context().logBuffer,"R(C_%u)={", k);
        LH = G.LogStart(LOG_METH2, G.Context().logBuffer);
        for (S=C->R->Segments.begin(); S!=C->R->Segments.end(); S++)
        {
            sprintf(G.Context().logBuffer,"Seg(%lu,%lu),",
                static_cast<unsigned long>(PreO.GetValue(G.StartNode((*S)->e))),
                static_cast<unsigned long>(PreO.GetValue(G.EndNode((*S)->e))));
            G.LogAppend(LH, G.Context().logBuffer);
        }
        G.LogEnd(LH, "}");
        k--;
    }
    G.CloseFold();

    #if defined(_TRACING_)

    TNode L = 0;
    TNode R = 0;
    for (list<TSegGrComp>::iterator C=SGCs.begin(); C!=SGCs.end(); C++)
    {
        L += C->L->Segments.size();
        R += C->R->Segments.size();
    }

    sparseBiGraph* H = new sparseBiGraph(L,R,G.Context());
    H -> ImportLayoutData(G);
    TNode M = 4*max(L,R);
    TNode l = 0;
    TNode r = 0;
    attribute<TFloat>* Dist = H->Registers()->InitAttribute<TFloat>(*H,TokRegLabel,0);

    k = SGCs.size() - 1;
    for (list<TSegGrComp>::iterator C=SGCs.begin(); C!=SGCs.end(); C++)
    {
        for (list<TSegPath*>::iterator S=C->L->Segments.begin();
             S!=C->L->Segments.end(); S++)
        {
            H->SetC(l,0,-4);
            H->SetC(l,1,l*M/L);
            H->SetNodeColour(l,k);
            Dist->SetValue(l, (*S)->e);
            l++;
        }

        for (list<TSegPath*>::iterator S=C->R->Segments.begin();
             S!=C->R->Segments.end(); S++)
        {
            H->SetC(r+L,0,4);
            H->SetC(r+L,1,r*M/R);
            H->SetNodeColour(r+L,k);
            Dist->SetValue(r+L, (*S)->e);
            r++;
        }
        k--;
    }

    H -> SetLayoutParameter(TokLayoutNodeSize,150.0);
    H -> SetLayoutParameter(TokLayoutNodeLabelFormat,"#2");
    G.Context().Trace(H->Handle(),1);

    delete H;

    #endif
}


void extractKuratowski(abstractMixedGraph& G, attribute<TNode>& PreO,
                       attribute<int>& EdgeType, attribute<TNode>& Low1,
                       TSegPath& FatherSeg, TSegPath& Seg, TSegGrComp& C,
                       TSegPath* disturbingSeg_i);


/* stronglyplanar returns a sorted list of all attachment points of
   segment Seg in SegHeads */
bool stronglyplanar(abstractMixedGraph& G, attribute<TNode>& PreO,
                    attribute<int>& EdgeType, attribute<TNode>& Low1,
                    attribute<TNode>& Low2, TSegPath* FatherSeg, TSegPath& Seg,
                    bool extractMinor, list<TNode>& SegHeads)
{
    list<TSegGrComp> SegGrCs;       //indices sorted descendent
    list<TSegGrComp>::iterator C, C1;

    sprintf(G.Context().logBuffer,"Seg(%lu,%lu)... ",
        static_cast<unsigned long>(G.StartNode(Seg.e)),
        static_cast<unsigned long>(G.EndNode(Seg.e)));
    G.LogEntry(LOG_METH, G.Context().logBuffer);
    G.OpenFold();

    for (TIndex i=0; i<Seg.Emanat.size(); i++)
    {
        sprintf(G.Context().logBuffer,"calculate SG_%lu(Seg(%lu,%lu))",i,
            static_cast<unsigned long>(G.StartNode(Seg.e)),
            static_cast<unsigned long>(G.EndNode(Seg.e)));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);

        TSegPath* Seg_i = Seg.Emanat[i];

        if (!stronglyplanar(G,PreO,EdgeType,Low1,Low2,&Seg,*Seg_i,
                            extractMinor,Seg_i->Att))
        {
            for (C=SegGrCs.begin();C!=SegGrCs.end();C++) {
                delete C->L;
                delete C->R;
            }

            sprintf(G.Context().logBuffer,"... Seg(%lu,%lu) is not strongly planar",
                static_cast<unsigned long>(G.StartNode(Seg.e)),
                static_cast<unsigned long>(G.EndNode(Seg.e)));
            G.LogEntry(LOG_RES, G.Context().logBuffer);
            G.CloseFold();

            return false;
        }

        //test, if SG_i(e) (=SG_{i-1}(e)+Seg(e_i)) is bipartit
        C = SegGrCs.begin();
        while ( C != SegGrCs.end() && (
            (!C->L->Att.empty() && C->L->Att.front()>Low1[Seg_i->e>>1]) ||
            (!C->R->Att.empty() && C->R->Att.front()>Low1[Seg_i->e>>1]) ))
        {
            if (!C->L->Att.empty() && C->L->Att.front()>Low1[Seg_i->e>>1])
            {
                TSegGrCompSide* HSide = C->L;
                C->L = C->R;
                C->R = HSide;
                if (!C->L->Att.empty() && C->L->Att.front()>Low1[Seg_i->e>>1])
                {
                    sprintf(G.Context().logBuffer,
                        "... Seg(%lu,%lu) is not strongly planar, because adding of Seg(%lu,%lu)",
                        static_cast<unsigned long>(G.StartNode(Seg.e)),
                        static_cast<unsigned long>(G.EndNode(Seg.e)),
                        static_cast<unsigned long>(G.StartNode(Seg_i->e)),
                        static_cast<unsigned long>(G.EndNode(Seg_i->e)));
                    G.LogEntry(LOG_RES, G.Context().logBuffer);
                    sprintf(G.Context().logBuffer,
                        "to SG_%lu(Seg(%lu,%lu)) makes the segment graph non-bipartite",i,
                        static_cast<unsigned long>(G.StartNode(Seg.e)),
                        static_cast<unsigned long>(G.EndNode(Seg.e)));
                    G.LogEntry(LOG_RES, G.Context().logBuffer);
                    G.CloseFold();

                    if (extractMinor) {
                        Seg_i->Att.push_front(PreO[G.StartNode(Seg_i->e)]);
                        Seg.Att = SegHeads;  // sind doch noch leer?!?
                        extractKuratowski(G,PreO,EdgeType,Low1,*FatherSeg,
                                          Seg,*C,Seg_i);
                    }

                    for (C=SegGrCs.begin();C!=SegGrCs.end();C++) {
                        delete C->L;
                        delete C->R;
                    }

                    return false;
                }
            }
            C++;
        }
        C1 = C;

        // update SG_{i-1} to SG_i
        C = SegGrCs.begin();
        if (C != C1)
        {  // Seg_i is directly linked to (at least) last component of SegGrCs
            C++;
            while (C != C1) {
                sprintf(G.Context().logBuffer,
                    "merge C_%u into C_%u and delete C_%u from SG_%lu(Seg(%lu,%lu))",
                    SegGrCs.size()-1,SegGrCs.size()-2,SegGrCs.size()-1,
                    static_cast<unsigned long>(i),
                    static_cast<unsigned long>(G.StartNode(Seg.e)),
                    static_cast<unsigned long>(G.EndNode(Seg.e)));
                G.LogEntry(LOG_METH2, G.Context().logBuffer);

                C->L->Att.splice(C->L->Att.begin(), SegGrCs.front().L->Att);
                C->L->Segments.splice(C->L->Segments.begin(), SegGrCs.front().L->Segments);

                C->R->Att.splice(C->R->Att.begin(), SegGrCs.front().R->Att);
                C->R->Segments.splice(C->R->Segments.begin(), SegGrCs.front().R->Segments);

                C++;
                delete SegGrCs.front().L;
                delete SegGrCs.front().R;
                SegGrCs.pop_front();
            }
            C = SegGrCs.begin();
        }
        else {
            sprintf(G.Context().logBuffer,
                "create new component for Seg(%lu,%lu) in SG_%lu(Seg(%lu,%lu))",
                static_cast<unsigned long>(G.StartNode(Seg_i->e)),
                static_cast<unsigned long>(G.EndNode(Seg_i->e)),
                static_cast<unsigned long>(i),
                static_cast<unsigned long>(G.StartNode(Seg.e)),
                static_cast<unsigned long>(G.EndNode(Seg.e)));
            G.LogEntry(LOG_METH2, G.Context().logBuffer);

            TSegGrComp GC = {new TSegGrCompSide, new TSegGrCompSide};
            SegGrCs.push_front(GC);
            C = SegGrCs.begin();
        }

        // copy Seg_i.Att to front of C->L->Att
        C->L->Att.insert(C->L->Att.begin(),Seg_i->Att.begin(),Seg_i->Att.end());
        Seg_i->Att.push_front(PreO[G.StartNode(Seg_i->e)]);
        C->L->Segments.push_front(Seg_i);

        printSegGrCs(G,PreO,SegGrCs);


        sprintf(G.Context().logBuffer, "update Att_%li to Att_%li",
            static_cast<unsigned long>(i),static_cast<unsigned long>(i+1));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);

        TNode Tail_e_i1;
        if (i+1 < Seg.Emanat.size())
            Tail_e_i1 = PreO[G.StartNode(Seg.Emanat[i+1]->e)];
        else
            Tail_e_i1 = PreO[G.StartNode(Seg.e)];

        C = SegGrCs.begin();
        bool cont = true;
        while (cont && (C != SegGrCs.end())) {
            while (!C->L->Att.empty() && (C->L->Att.front() >= Tail_e_i1))
                C->L->Att.pop_front();

            while (!C->R->Att.empty() && (C->R->Att.front() >= Tail_e_i1))
                C->R->Att.pop_front();

            if (C->L->Att.empty() && C->R->Att.empty()) {
                // alphaL is set by bipartition
                list<TSegPath*>::iterator S;
                for(S=C->L->Segments.begin(); S!=C->L->Segments.end(); S++)
                    (*S)->alphaL = true;

                C = SegGrCs.begin();
                delete C->L;
                delete C->R;
                SegGrCs.pop_front();
                C = SegGrCs.begin();
            }
            else {
                cont = false;
            }
        }
    } // end of for-loop

    // Is Seg strongly planar?
    C = SegGrCs.end();
    size_t k = 0;  //only for logging purpose
    while (C != SegGrCs.begin()) {
        C--;
        if (!C->L->Att.empty() && (C->L->Att.front()>Low1[Seg.e>>1])) {
            TSegGrCompSide* HSide = C->L;
            C->L = C->R;
            C->R = HSide;

            if (!C->L->Att.empty() && (C->L->Att.front()>Low1[Seg.e>>1]))
            {
                sprintf(G.Context().logBuffer,
                    "... Seg(%lu,%lu) is planar, but not strongly planar, because ",
                    static_cast<unsigned long>(G.StartNode(Seg.e)),
                    static_cast<unsigned long>(G.EndNode(Seg.e)));
                G.LogEntry(LOG_RES, G.Context().logBuffer);
                sprintf(G.Context().logBuffer,
                    "Att(L(C[%lu]))>Low1(e)  AND  Att(R(C[%lu]))>Low1(e) with Low1(e)=%lu",
                    static_cast<unsigned long>(k),
                    static_cast<unsigned long>(k),
                    static_cast<unsigned long>(Low1.GetValue(Seg.e>>1)));
                G.LogEntry(LOG_RES, G.Context().logBuffer);
                G.CloseFold();

                if (extractMinor) {
                    Seg.Att = SegHeads;
                    extractKuratowski(G,PreO,EdgeType,Low1,*FatherSeg,
                                      Seg,*C,NULL);
                }

                while (C != SegGrCs.begin()) {
                    delete C->L;
                    delete C->R;
                    C--;
                }
                delete C->L;
                delete C->R;

                return false;
            }
        }

        SegHeads.splice(SegHeads.begin(), C->R->Att);
        SegHeads.splice(SegHeads.begin(), C->L->Att);

        // alphaL is set by bipartition
        list<TSegPath*>::iterator S;
        for (S=C->L->Segments.begin(); S != C->L->Segments.end(); S++)
            (*S)->alphaL = true;

        delete C->L;
        delete C->R;

        k++;
    }

    SegHeads.push_back(PreO[G.EndNode(Seg.Low1Edge)]);
    SegHeads.unique();  // remove double entries (not required by algorithm)
    for (TIndex i=0; i<Seg.Emanat.size(); i++)
        Seg.Emanat[i]->Att.clear();

    sprintf(G.Context().logBuffer,"... Seg(%lu,%lu) is strongly planar",
        static_cast<unsigned long>(G.StartNode(Seg.e)),
        static_cast<unsigned long>(G.EndNode(Seg.e)));
    G.LogEntry(LOG_RES, G.Context().logBuffer);
    G.CloseFold();

    return true;
}


/*
 Die Funktion IsDirectlyLinked prueft, ob zwei Segmente S1 und S2 direkt
 verbunden sind. Zur Repraesentation reichen deren Verbindungspunkte
 Att1 und Att2 aus. Wenn die beiden Segmente direkt verbunden sind und
 der Typ dafuer "asymmetrisch" ist, dann werden Knoten a, c von Att1 und
 y, z von Att2 zurueckgegeben und es gilt
 entweder a=Low1(S1) < y < c < z=Tail(S2)
  oder    y=Low1(S2) < a < z < c=Tail(S1).

 Bei direkt verbundenen Segmenten mit jeweils 3 Verbindungspunkten
 (Typ "aequivalent"), wird fuer a,c,y,z der Wert NoNode zurueckgegeben.
*/
bool IsDirectlyLinked(list<TNode>& Att1, list<TNode>& Att2,
                      TNode& a, TNode& c, TNode& y, TNode& z)
{
    list<TNode>* AttX;
    list<TNode>* AttY;
    bool swap = false;
    bool EqualTailCase;
    a = NoNode;
    c = NoNode;
    y = NoNode;
    z = NoNode;
    TNode x1, y1, x2, y2;

    AttX = &Att1;
    AttY = &Att2;
    if (Att1.front() == Att2.front())
    {
        EqualTailCase = true;
        if (Att1.back() == Att2.back())
        {
            Att1.unique();
            Att2.unique();
            if ((Att1.size() <= 2) || (Att2.size() <= 2))
                return false;
            if ((Att1.size() == 3) && (Att2.size() == 3) && (Att1 == Att2))
                return true;

            TNode low1X = AttX->back();
            TNode tailX = AttX->front();
            TNode low1Y = AttY->back();
            TNode tailY = AttY->front();
            list<TNode>::iterator ia = AttX->begin();
            ia++;
            list<TNode>::iterator iy = AttY->begin();
            iy++;

            if (*ia == *iy)
            {
                ia++;
                if (*ia == low1X)
                {
                    *ia--;
                    *iy++;
                }
            }

            if (*ia < *iy)
            {
                a = *ia;
                c = tailX;
                y = low1Y;
                z = *iy;
            }
            else
            {
                a = low1X;
                c = *ia;
                y = *iy;
                z = tailY;
            }
            return true;

        }
        else if (Att1.back() > Att2.back())
        {
            AttX = &Att2;
            AttY = &Att1;
            swap = true;
        }
    }
    else
    {
        EqualTailCase = false;
        if (Att1.front() < Att2.front())
        {
            AttX = &Att2;
            AttY = &Att1;
            swap = true;
        }
    }

    TNode low1X = AttX->back();
    TNode tailX = AttX->front();
    TNode low1Y = AttY->back();
    TNode tailY = AttY->front();

    list<TNode>::iterator ia = AttX->begin();
    while ((ia!=AttX->end()) && (tailY <= *ia))
        ia++;

    if ((ia!=AttX->end()) && (low1Y < *ia))
    {
        if (EqualTailCase)
        {
            x1 = low1X;
            y1 = *ia;
        }
        else
        {
            x1 = *ia;
            y1 = tailX;
        }
        x2 = low1Y;
        y2 = tailY;

        if (!swap)
        {
            a = x1;
            c = y1;
            y = x2;
            z = y2;
        }
        else
        {
            y = x1;
            z = y1;
            a = x2;
            c = y2;
        }
        return true;
    }
    else
        return false;
}

bool IsDirectlyLinked(list<TNode>& Att1, list<TNode>& Att2)
{
    TNode a,c,y,z;
    return IsDirectlyLinked(Att1, Att2, a,c,y,z);
}


void MinOddSGCycle(abstractMixedGraph& G, attribute<TNode>& PreO,
                   vector<TSegPath*>& MinCycle, TSegPath& Seg,
                   TSegGrComp& C, TSegPath& disturbingSeg_i)
{
    list<TSegPath*>::iterator SegX = C.L->Segments.begin();
    while ((SegX != C.L->Segments.end()) &&
            !IsDirectlyLinked((*SegX)->Att, disturbingSeg_i.Att) )
        SegX++;

    sprintf(G.Context().logBuffer,"SegX = Seg(%lu,%lu)",
        static_cast<unsigned long>(G.StartNode((*SegX)->e)),
        static_cast<unsigned long>(G.EndNode((*SegX)->e)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    list<TSegPath*>::iterator SegY = C.R->Segments.begin();
    while ((SegY != C.R->Segments.end()) &&
            !IsDirectlyLinked((*SegY)->Att, disturbingSeg_i.Att) )
        SegY++;

    sprintf(G.Context().logBuffer,"SegY = Seg(%lu,%lu)",
        static_cast<unsigned long>(G.StartNode((*SegY)->e)),
        static_cast<unsigned long>(G.EndNode((*SegY)->e)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    list<TSegPath*>::iterator A, B;
    TSegGrCompSide *SideA, *SideB;
    if ((*SegX)->Emanat_Index < (*SegY)->Emanat_Index)
    {
        A = SegX;
        B = SegY;
        SideA = C.L;
        SideB = C.R;
    }
    else
    {
        A = SegY;
        B = SegX;
        SideA = C.R;
        SideB = C.L;
    }

    vector<list<TSegPath*> > SideASegmentsOfAtt =
        vector<list<TSegPath*> >(G.N());
    G.LogEntry(LOG_METH2, "SideA: ");
    list<TSegPath*>::iterator S;
    for (S=SideA->Segments.begin(); S!=SideA->Segments.end(); S++)
    {
        sprintf(G.Context().logBuffer,"Seg(%lu,%lu)",
            static_cast<unsigned long>(G.StartNode((*S)->e)),
            static_cast<unsigned long>(G.EndNode((*S)->e)));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);

        list<TNode>::iterator v = (*S)->Att.begin();
        v++;
        for (; v!=(*S)->Att.end(); v++) {
            sprintf(G.Context().logBuffer,"->%lu",static_cast<unsigned long>(*v));
            G.LogAppend(LOG_METH2, G.Context().logBuffer);
            SideASegmentsOfAtt[*v].push_back(*S);
        }
    }

    vector<list<TSegPath*> > SideBSegmentsOfAtt =
        vector<list<TSegPath*> >(G.N());
    G.LogEntry(LOG_METH2, "SideB: ");
    for (S=SideB->Segments.begin(); S!=SideB->Segments.end(); S++)
    {
        sprintf(G.Context().logBuffer,"Seg(%lu,%lu)",
            static_cast<unsigned long>(G.StartNode((*S)->e)),
            static_cast<unsigned long>(G.EndNode((*S)->e)));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);

        list<TNode>::iterator v = (*S)->Att.begin();
        v++;
        for (; v!=(*S)->Att.end(); v++) {
            sprintf(G.Context().logBuffer,"->%lu",static_cast<unsigned long>(*v));
            G.LogAppend(LOG_METH, G.Context().logBuffer);
            SideBSegmentsOfAtt[*v].push_back(*S);
        }
    }

    //shortest path from B to A
    MinCycle.push_back(*B);
    TNode t  = PreO[G.StartNode(disturbingSeg_i.e)];
    TNode t2 = PreO[G.StartNode((*B)->e)];
    TArc g = Seg.e;
    while (PreO[G.StartNode(g)] != t)
        g = G.First(G.EndNode(g));

    while (!IsDirectlyLinked(MinCycle.back()->Att, (*A)->Att)) {
        list<TSegPath*>::iterator Smax;
        TNode v, TailSmax;

        TailSmax = 0;
        for (; PreO[G.StartNode(g)]!=t2; g=G.First(G.EndNode(g))) /*v in [t, t2)*/{
            v = PreO[G.StartNode(g)];
            for (S=SideASegmentsOfAtt[v].begin(); S!=SideASegmentsOfAtt[v].end(); S++)
            {
                sprintf(G.Context().logBuffer,"ASeg(%lu,%lu)",
                    static_cast<unsigned long>(G.StartNode((*S)->e)),
                    static_cast<unsigned long>(G.EndNode((*S)->e)));
                G.LogEntry(LOG_METH2, G.Context().logBuffer);
                if (TailSmax <= PreO[G.StartNode((*S)->e)]) {
                    Smax = S;
                    TailSmax = PreO[G.StartNode((*S)->e)];
                    G.LogAppend(LOG_METH2, "current max;");
                }
            }
        }

        MinCycle.push_back(*Smax);
        t  = t2;
        t2 = PreO[G.StartNode((*Smax)->e)];
        TailSmax = 0;
        for (; PreO[G.StartNode(g)]!=t2; g=G.First(G.EndNode(g))) /*v in [t, t2)*/{
            v = PreO[G.StartNode(g)];
            for (S=SideBSegmentsOfAtt[v].begin(); S!=SideBSegmentsOfAtt[v].end(); S++)
            {
                sprintf(G.Context().logBuffer,"BSeg(%lu,%lu)",
                    static_cast<unsigned long>(G.StartNode((*S)->e)),
                    static_cast<unsigned long>(G.EndNode((*S)->e)));
                G.LogEntry(LOG_METH2, G.Context().logBuffer);
                if (TailSmax <= PreO[G.StartNode((*S)->e)]) {
                    Smax = S;
                    TailSmax = PreO[G.StartNode((*S)->e)];
                    G.LogAppend(LOG_METH2, "current max;");
                }
            }
        }

        if (IsDirectlyLinked((*Smax)->Att, disturbingSeg_i.Att))
            MinCycle.clear();

        MinCycle.push_back(*Smax);
        t  = t2;
        t2 = PreO[G.StartNode((*Smax)->e)];
    }

    MinCycle.push_back(*A);
    MinCycle.push_back(&disturbingSeg_i);
}


void ColorPartOfCycle(abstractMixedGraph& G, TArc a, TNode t, TArc Colour,
                      attribute<TArc>& EdgeColour, bool log = true)
{
    if (log) {
        sprintf(G.Context().logBuffer,"cycle   from %lu to %lu (colour=%lu): ",
            static_cast<unsigned long>(G.EndNode(a)),
            static_cast<unsigned long>(t),
            static_cast<unsigned long>(Colour));
        G.LogEntry(LOG_METH2, G.Context().logBuffer);
    }
    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);

    while (G.EndNode(a) != t) {
        sprintf(G.Context().logBuffer," (%lu,%lu)",
            static_cast<unsigned long>(G.EndNode(a)),
            static_cast<unsigned long>(G.StartNode(a)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);

        EdgeColour.SetValue(a>>1,Colour);
        a = pred->GetValue(G.StartNode(a));
    }
}

void goThroughPath(abstractMixedGraph& G, attribute<TArc>& EdgeColour,
                   TSegPath& Seg, TArc Colour)
{
    sprintf(G.Context().logBuffer,"path    from %lu to %lu (colour=%lu): ",
        static_cast<unsigned long>(G.EndNode(Seg.Low1Edge)),
        static_cast<unsigned long>(G.StartNode(Seg.e)),
        static_cast<unsigned long>(Colour));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    ColorPartOfCycle(G, Seg.Low1Edge, G.StartNode(Seg.e), Colour, EdgeColour, false);
}

TArc findAttEdge(abstractMixedGraph& G, TSegPath& Seg, TNode a)
{
    if (G.EndNode(Seg.Low1Edge) == a)
        return Seg.Low1Edge;
    if (G.StartNode(Seg.e) == a)
        return Seg.e;

    for(vector<TSegPath*>::iterator S=Seg.Emanat.begin(); S!=Seg.Emanat.end(); S++)
    {
        TArc e = findAttEdge(G, **S, a);
        if (e != NoArc)
            return e;
    }
    return NoArc;
}

TNode goThroughSeg(abstractMixedGraph& G, attribute<TArc>& EdgeColour,
         attribute<TNode>& PreO, TSegPath& Seg, TNode a, TNode b, TArc Colour)
{
    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);
    sprintf(G.Context().logBuffer,"segment from %lu to %lu (colour=%lu): ",
        static_cast<unsigned long>(a),
        static_cast<unsigned long>(b),
        static_cast<unsigned long>(Colour));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    TArc e_a = findAttEdge(G, Seg, a);
    TArc e_b = findAttEdge(G, Seg, b);
    if (e_a == Seg.e) {
        ColorPartOfCycle(G, e_b, G.StartNode(e_a), Colour, EdgeColour, false);
        return a;
    }
    else if (e_b == Seg.e) {
        ColorPartOfCycle(G, e_a, G.StartNode(e_b), Colour, EdgeColour, false);
        return b;
    }
    else {
        sprintf(G.Context().logBuffer," (%lu,%lu)",
            static_cast<unsigned long>(G.StartNode(e_a)),
            static_cast<unsigned long>(G.EndNode(e_a)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
        EdgeColour.SetValue(e_a>>1, Colour);

        sprintf(G.Context().logBuffer," (%lu,%lu)",
            static_cast<unsigned long>(G.StartNode(e_b)),
            static_cast<unsigned long>(G.EndNode(e_b)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
        EdgeColour.SetValue(e_b>>1, Colour);

        while (G.StartNode(e_a) != G.StartNode(e_b)) {
            if (PreO.GetValue(G.StartNode(e_a))<PreO.GetValue(G.StartNode(e_b))) {
                e_b = pred->GetValue(G.StartNode(e_b));
                sprintf(G.Context().logBuffer," (%lu,%lu)",
                    static_cast<unsigned long>(G.StartNode(e_b)),
                    static_cast<unsigned long>(G.EndNode(e_b)));
                G.LogAppend(LOG_METH2, G.Context().logBuffer);
                EdgeColour.SetValue(e_b>>1, Colour);
            }
            else {
                e_a = pred->GetValue(G.StartNode(e_a));
                sprintf(G.Context().logBuffer," (%lu,%lu)",
                    static_cast<unsigned long>(G.StartNode(e_a)),
                    static_cast<unsigned long>(G.EndNode(e_a)));
                G.LogAppend(LOG_METH2, G.Context().logBuffer);
                EdgeColour.SetValue(e_a>>1, Colour);
            }
        }
        return G.StartNode(e_a);
    }
}

//Vorbedingung: x3,x1,y3 != G.StartNode(Seg.e) und x3,x1,y3 != G.EndNode(Seg.e)
//         (PreO[x3] < PreO[x1]) && (PreO[x1] < PreO[y3])
TNode oneNode(abstractMixedGraph& G, attribute<TArc>& EdgeColour,
              attribute<TNode>& PreO, attribute<int>& EdgeType,
              TSegPath& Seg, TNode x3, TNode x1, TNode y3)
{
    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);
    TArc e_x3 = findAttEdge(G, Seg, x3);
    TArc e_x1 = findAttEdge(G, Seg, x1);
    TArc e_y3 = findAttEdge(G, Seg, y3);

    x3 = G.StartNode(e_x3);
    sprintf(G.Context().logBuffer," ( %lu,%lu)",
        static_cast<unsigned long>(x3),
        static_cast<unsigned long>(G.EndNode(e_x3)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    x1 = G.StartNode(e_x1);
    sprintf(G.Context().logBuffer," ( %lu,%lu)",
        static_cast<unsigned long>(x1),
        static_cast<unsigned long>(G.EndNode(e_x1)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    y3 = G.StartNode(e_y3);
    sprintf(G.Context().logBuffer," ( %lu,%lu)",
        static_cast<unsigned long>(y3),
        static_cast<unsigned long>(G.EndNode(e_y3)));
    G.LogAppend(LOG_METH2, G.Context().logBuffer);

    while (x3 != x1)
    {
        if (PreO.GetValue(x3) < PreO.GetValue(x1))
        {
            e_x1 = pred->GetValue(x1);
            x1 = G.StartNode(e_x1);
            sprintf(G.Context().logBuffer," ( %lu,%lu)",
                static_cast<unsigned long>(x1),
                static_cast<unsigned long>(G.EndNode(e_x1)));
            G.LogAppend(LOG_METH2, G.Context().logBuffer);
        }
        else
        {
            e_x3 = pred->GetValue(x3);
            x3 = G.StartNode(e_x3);
            sprintf(G.Context().logBuffer," ( %lu,%lu)",
                static_cast<unsigned long>(x3),
                static_cast<unsigned long>(G.EndNode(e_x3)));
            G.LogAppend(LOG_METH2, G.Context().logBuffer);
        }
    }

    while (PreO.GetValue(x3) < PreO.GetValue(y3))
    {
        e_y3 = pred->GetValue(y3);
        y3 = G.StartNode(e_y3);
        sprintf(G.Context().logBuffer," ( %lu,%lu)",
            static_cast<unsigned long>(x1),
            static_cast<unsigned long>(G.EndNode(e_y3)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
    }

    if (x3 == y3) return x3;

    return NoNode;
}

struct TRedSeg {
    TSegPath*   Seg;
    list<TNode> Att;
//    list<TArc>  AttEdges;
};

void colorNodes(attribute<TNode>& NodeColour, TNode a1, TNode a2, TNode a3,
                                                TNode b1, TNode b2, TNode b3)
{
    NodeColour[a1] = 0;
    NodeColour[a2] = 0;
    NodeColour[a3] = 0;
    NodeColour[b1] = 2;
    NodeColour[b2] = 2;
    NodeColour[b3] = 2;
}

void sort4Att(TNode xA, TNode yA, TNode xB, TNode yB, list<TNode>& Att)
{
    if (yA < yB) {
        Att.push_back(yB);
        if (xB < yA) {
            Att.push_back(yA);
            if (xA < xB) {
                Att.push_back(xB);
                Att.push_back(xA);
            }
            else {
                Att.push_back(xA);
                Att.push_back(xB);
            }
        }
        else {
            Att.push_back(xB);
            Att.push_back(yA);
            Att.push_back(xA);
        }
    }
    else {
        Att.push_back(yA);
        if (xA < yB) {
            Att.push_back(yB);
            if (xA < xB) {
                Att.push_back(xB);
                Att.push_back(xA);
            }
            else {
                Att.push_back(xA);
                Att.push_back(xB);
            }
        }
        else {
            Att.push_back(xA);
            Att.push_back(yB);
            Att.push_back(xB);
        }
    }
    Att.unique();
}

void basicCase1(abstractMixedGraph& G, attribute<TNode>& PreO, TNode* OrigOrd,
                attribute<int>& EdgeType, vector<TSegPath*>& MinCycle)
{
    attribute<TNode>* NodeColour = G.Registers()->InitAttribute<TNode>(G,
            TokRegNodeColour, NoNode);
    attribute<TArc>* EdgeColour = G.Registers()->InitAttribute<TArc>(G,
            TokRegEdgeColour, NoArc);
    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);

    TSegPath* A = MinCycle[1];
    TSegPath* B = MinCycle[0];
    TSegPath* F = MinCycle[2];

    //Vorbereitung der Fallunterscheidung
    TNode xAB,yAB, xBA,yBA;
    IsDirectlyLinked(A->Att,B->Att, xAB,yAB,xBA,yBA);
    bool AB_equi = (xAB == NoNode);

    TNode xAF,yAF, xFA,yFA;
    IsDirectlyLinked(A->Att,F->Att, xAF,yAF,xFA,yFA);
    bool AF_equi = (xAF == NoNode);

    TNode xBF,yBF, xFB,yFB;
    IsDirectlyLinked(B->Att,F->Att, xBF,yBF,xFB,yFB);
    bool BF_equi = (xBF == NoNode);

    TRedSeg RA, RB, RF;
    if (AB_equi || AF_equi)
        RA.Att = A->Att;
    else
        sort4Att(xAB,yAB,xAF,yAF, RA.Att);
    RA.Seg = A;

    if (AB_equi || BF_equi)
        RB.Att = B->Att;
    else
        sort4Att(xBA,yBA,xBF,yBF, RB.Att);
    RB.Seg = B;

    if (AF_equi || BF_equi)
        RF.Att = F->Att;
    else
        sort4Att(xFA,yFA,xFB,yFB, RF.Att);
    RF.Seg = F;

    //maximal reduced segments:
    list<TNode>::iterator i,j;
    TNode x;
    i = RA.Att.begin();
    while (i!=RA.Att.end())
    {
        if (RA.Att.size() <= 2) break;
        j = i;
        j++;
        x = *i;
        RA.Att.erase(i);
        if (!IsDirectlyLinked(RA.Att,RB.Att) || !IsDirectlyLinked(RA.Att,RF.Att))
            RA.Att.insert(j,x);
        i = j;
    }

    i = RB.Att.begin();
    while (i!=RB.Att.end())
    {
        if (RB.Att.size() <= 2) break;
        j = i;
        j++;
        x = *i;
        RB.Att.erase(i);
        if (!IsDirectlyLinked(RB.Att,RA.Att) || !IsDirectlyLinked(RB.Att,RF.Att))
            RB.Att.insert(j,x);
        i = j;
    }

    i = RF.Att.begin();
    while (i!=RF.Att.end())
    {
        if (RF.Att.size() <= 2) break;
        j = i;
        j++;
        x = *i;
        RF.Att.erase(i);
        if (!IsDirectlyLinked(RF.Att,RA.Att) || !IsDirectlyLinked(RF.Att,RB.Att))
            RF.Att.insert(j,x);
        i = j;
    }

    //sort segments
    TRedSeg *S1 = &RA;
    TRedSeg *S2 = &RB;
    TRedSeg *S3 = &RF;
    TRedSeg *X;

    if (S1->Att.size() > S2->Att.size())
    {
        X = S1;
        S1 = S2;
        S2 = X;
    }
    else if (S1->Att.size() == S2->Att.size()) {
        if (S1->Att.front() < S2->Att.front()) {
            X = S1;
            S1 = S2;
            S2 = X;
        }
    }

    if (S2->Att.size() > S3->Att.size())
    {
        X = S2;
        S2 = S3;
        S3 = X;
    }
    else if (S2->Att.size() == S3->Att.size()) {
        if (S2->Att.front() < S3->Att.front()) {
            X = S2;
            S2 = S3;
            S3 = X;
        }
    }

    if (S1->Att.size() > S2->Att.size()) {
        X = S1;
        S1 = S2;
        S2 = X;
    }
    else if (S1->Att.size() == S2->Att.size()) {
        if (S1->Att.front() < S2->Att.front()) {
            X = S1;
            S1 = S2;
            S2 = X;
        }
    }

    sprintf(G.Context().logBuffer,"S1=(%lu,%lu); ",
        static_cast<unsigned long>(G.StartNode(S1->Seg->e)),
        static_cast<unsigned long>(G.EndNode(S1->Seg->e)));
    G.LogEntry(LOG_METH2, G.Context().logBuffer);
    sprintf(G.Context().logBuffer,"S2=(%lu,%lu); ",
        static_cast<unsigned long>(G.StartNode(S2->Seg->e)),
        static_cast<unsigned long>(G.EndNode(S2->Seg->e)));
    G.LogAppend(LOG_METH2, G.Context().logBuffer);
    sprintf(G.Context().logBuffer,"S3=(%lu,%lu);",
        static_cast<unsigned long>(G.StartNode(S3->Seg->e)),
        static_cast<unsigned long>(G.EndNode(S3->Seg->e)));
    G.LogAppend(LOG_METH2, G.Context().logBuffer);

    //Fallunterscheidung
    //Farben: 1 rot, 2 blau, 3 gelb, 4 türkis, 5 lila, 6 ocker, 7 stahlblau, 8 rosa, 9 umber
    TNode Color11 = 3;
    TNode Color12 = 9;
    TNode Color13 = 6;
    TNode Color21 = 5;
    TNode Color22 = 1;
    TNode Color23 = 8;
    TNode Color31 = 4;
    TNode Color32 = 7;
    TNode Color33 = 2;
    if (S1->Att.size() == 2)
    {
        if (S2->Att.size() == 2)
        {
            if (S3->Att.size() == 2)
            {
                G.LogEntry(LOG_METH2, "Fall (2,2,2)");
                TNode l1,l2,l3,t1,t2,t3;
                l3 = OrigOrd[S3->Att.back()];
                l2 = OrigOrd[S2->Att.back()];
                l1 = OrigOrd[S1->Att.back()];
                t3 = OrigOrd[S3->Att.front()];
                t2 = OrigOrd[S2->Att.front()];
                t1 = OrigOrd[S1->Att.front()];
                colorNodes(*NodeColour, l3,l1,t2, l2,t3,t1);

                ColorPartOfCycle(G, pred->GetValue(l3),t1, Color11, *EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,t3,Color12);
                ColorPartOfCycle(G, pred->GetValue(l2),l3, Color13, *EdgeColour);

                ColorPartOfCycle(G, pred->GetValue(l1),l2, Color21, *EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color22);
                ColorPartOfCycle(G, pred->GetValue(t3),l1, Color23, *EdgeColour);

                ColorPartOfCycle(G, pred->GetValue(t2),t3, Color31, *EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color32);
                ColorPartOfCycle(G, pred->GetValue(t1),t2, Color33, *EdgeColour);
            }
            else if (S3->Att.size() == 3)
            {
                G.LogEntry(LOG_METH2, "Fall (2,2,3)");
                TNode l1,l2,l3,t1,t2,t3,i1,i2;
                list<TNode>::iterator j2;
                l3 = OrigOrd[S3->Att.back()];
                l2 = OrigOrd[S2->Att.back()];
                l1 = OrigOrd[S1->Att.back()];
                j2 = S3->Att.begin();
                j2++;
                i2 = OrigOrd[*j2];
                t3 = OrigOrd[S3->Att.front()];
                t2 = OrigOrd[S2->Att.front()];
                t1 = OrigOrd[S1->Att.front()];

                if (t3 == t2)
                {
                    G.LogAppend(LOG_METH2, " a)");
                    ColorPartOfCycle(G, pred->GetValue(l3),t1, Color11, *EdgeColour);
                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,i2,Color12);
                    ColorPartOfCycle(G, pred->GetValue(l2),l3, Color13, *EdgeColour);

                    ColorPartOfCycle(G, pred->GetValue(i2),l2, Color21, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i1,i2,Color22);
                    if (i2 != l1)
                        ColorPartOfCycle(G, pred->GetValue(l1),i2, Color23, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color23);

                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color31);
                    ColorPartOfCycle(G, pred->GetValue(i1),t2, Color32, *EdgeColour);
                    ColorPartOfCycle(G, pred->GetValue(t1),t2, Color33, *EdgeColour);

                    colorNodes(*NodeColour, l3,i2,t2, l2,i1,t1);
                }
                else if (i2 == t1)
                {
                    G.LogAppend(LOG_METH2, " b)");
                    ColorPartOfCycle(G, pred->GetValue(l3),t1, Color11, *EdgeColour);
                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,i2,Color12);
                    if (l3 != l2)
                        ColorPartOfCycle(G, pred->GetValue(l2),l3, Color13, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color13);

                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i1,i2,Color21);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color22);
                    ColorPartOfCycle(G, pred->GetValue(t2),l1, Color23, *EdgeColour);

                    ColorPartOfCycle(G, pred->GetValue(t3),t2, Color31, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i1,t3,Color32);
                    ColorPartOfCycle(G, pred->GetValue(t1),t3, Color33, *EdgeColour);

                    colorNodes(*NodeColour, l3,l1,t3, i1,t2,t1);
                }
                else if ((l1 < i2) && (i2 < t2))
                {
                    G.LogAppend(LOG_METH2, " c)");
                    if (l3 != l2)
                        ColorPartOfCycle(G,pred->GetValue(l3),l2,Color11, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color11);
                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,i2,Color12);
                    ColorPartOfCycle(G,pred->GetValue(l1),l3,Color13, *EdgeColour);

                    ColorPartOfCycle(G,pred->GetValue(i2),l1,Color21, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i1,i2,Color22);
                    ColorPartOfCycle(G,pred->GetValue(t2),i2,Color23, *EdgeColour);

                    ColorPartOfCycle(G,pred->GetValue(t3),t2,Color31, *EdgeColour);
                    ColorPartOfCycle(G,pred->GetValue(i1),t3,Color32, *EdgeColour);
                    if (t3 != t1)
                        ColorPartOfCycle(G,pred->GetValue(t1),t3,Color33, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color33);

                    colorNodes(*NodeColour, l3,i2,t3, l1,i1,t2);
                }
                else if ((l2 < l3) && (l3 < l1))
                {
                    G.LogAppend(LOG_METH2, " d)");
                    ColorPartOfCycle(G, pred->GetValue(l2),t3, Color11, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color12);
                    ColorPartOfCycle(G, pred->GetValue(l3),l2, Color13, *EdgeColour);

                    ColorPartOfCycle(G, pred->GetValue(l1),l3, Color21, *EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color22);
                    if (t3 != t1)
                        ColorPartOfCycle(G, pred->GetValue(t3),t1, Color22, *EdgeColour);
                    ColorPartOfCycle(G, pred->GetValue(t2),l1, Color23, *EdgeColour);

                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,i2,Color31);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i2,t3,Color32);
                    ColorPartOfCycle(G, pred->GetValue(i1),t3, Color33, *EdgeColour);

                    colorNodes(*NodeColour, l2,l1,i1, l3,t2,t3);
                }
                else
                {
                    G.LogAppend(LOG_METH2, " e)");
                    ColorPartOfCycle(G,pred->GetValue(l2),t3,Color11,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color12);
                    if (t1 != i2)
                        ColorPartOfCycle(G,pred->GetValue(i2),t2,Color12,*EdgeColour);
                    ColorPartOfCycle(G,pred->GetValue(l1),l2,Color13,*EdgeColour);

                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l3,i2,Color21);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i2,t3,Color22);
                    ColorPartOfCycle(G,pred->GetValue(i1),t3,Color23,*EdgeColour);

                    ColorPartOfCycle(G,pred->GetValue(t1),i2,Color31,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color32);
                    ColorPartOfCycle(G,pred->GetValue(t3),t1,Color33,*EdgeColour);

                    colorNodes(*NodeColour, l2,i1,t1, l1,i2,t3);
                }
            }
            else
            {
                G.LogEntry(LOG_METH2, "Fall (2,2,4)");
                TNode l1,l2,t1,t2,i1,i2;
                l2 = OrigOrd[S2->Att.back()];
                l1 = OrigOrd[S1->Att.back()];
                t2 = OrigOrd[S2->Att.front()];
                t1 = OrigOrd[S1->Att.front()];

                i1 = oneNode(G,*EdgeColour,PreO,EdgeType,*S3->Seg, l2,l1,t2);

                if (i1 == NoNode)
                {
                    G.LogAppend(LOG_METH2," a)");

                    i1 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg, l2,l1,Color11);
                    i2 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg, l1,t2,Color12);
                    ColorPartOfCycle(G,pred->GetValue(i1),i2,Color13,*EdgeColour);

                    ColorPartOfCycle(G,pred->GetValue(t2),l1,Color21,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,Color22);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,t2,i2,Color23);

                    ColorPartOfCycle(G,pred->GetValue(i2),t1,Color31,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color32);
                    ColorPartOfCycle(G,pred->GetValue(l2),t1,Color33,*EdgeColour);

                    colorNodes(*NodeColour, i1,t2,t1, l2,l1,i2);
                }
                else
                {
                    G.LogAppend(LOG_METH2," b)");
                    (*NodeColour)[l2] = 1;
                    NodeColour->SetValue(l1,1);
                    NodeColour->SetValue(i1,1);
                    NodeColour->SetValue(t2,1);
                    NodeColour->SetValue(t1,1);

                    ColorPartOfCycle(G,pred->GetValue(t1),l2,1,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l2,t1,1);

                    ColorPartOfCycle(G,pred->GetValue(l2),t1,2,*EdgeColour);
                    goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,t2,2);
                    goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,2);
                    goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l1,t2,2);
                }
            }
        }
        else
        {
            G.LogEntry(LOG_METH2, "Fall (2,3,3)");
            TNode l1,l2,t1,t2,m2,i2,i3;
            list<TNode>::iterator j2;
            l1 = OrigOrd[S1->Att.back()];
            l2 = OrigOrd[S2->Att.back()];
            t1 = OrigOrd[S1->Att.front()];
            t2 = OrigOrd[S2->Att.front()];
            j2 = S2->Att.begin();
            j2++;
            m2 = OrigOrd[*j2];

            if (l1 < l2)
            {
                G.LogAppend(LOG_METH2, " a)");
                ColorPartOfCycle(G,pred->GetValue(l1),t2,Color11,*EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color12);
                if (t1 < m2)
                    ColorPartOfCycle(G,pred->GetValue(m2),t1,Color12,*EdgeColour);
                else if (m2 < t1)
                    ColorPartOfCycle(G,pred->GetValue(t1),m2,Color12,*EdgeColour);
                ColorPartOfCycle(G,pred->GetValue(l2),l1,Color13,*EdgeColour);

                i2 = goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,m2,Color21);
                goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,i2,t2,Color22);
                ColorPartOfCycle(G,pred->GetValue(i2),t2,Color23,*EdgeColour);

                i3 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l2,m2,Color31);
                goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i3,t2,Color32);
                ColorPartOfCycle(G,pred->GetValue(i3),t2,Color33,*EdgeColour);

                colorNodes(*NodeColour, l1,i2,i3, l2,m2,t2);
            }
            else if (l1 == l2)
            {
                G.LogAppend(LOG_METH2, " b)");
                ColorPartOfCycle(G,pred->GetValue(t1),m2,Color11,*EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color12);
                ColorPartOfCycle(G,pred->GetValue(t2),t1,Color13,*EdgeColour);

                i2 = goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,m2,Color21);
                goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,i2,t2,Color22);
                ColorPartOfCycle(G,pred->GetValue(i2),t2,Color23,*EdgeColour);

                i3 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l2,m2,Color31);
                goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i3,t2,Color32);
                ColorPartOfCycle(G,pred->GetValue(i3),t2,Color33,*EdgeColour);

                colorNodes(*NodeColour, i2,i3,t1, l2,m2,t2);
            }
            else if (t1 <= t2)
            {
                G.LogAppend(LOG_METH2, " c)");
                ColorPartOfCycle(G,pred->GetValue(l1),l2,Color11,*EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color12);
                ColorPartOfCycle(G,pred->GetValue(t2),t1,Color12,*EdgeColour);//für ehemals d) gebraucht?
                ColorPartOfCycle(G,pred->GetValue(m2),l1,Color13,*EdgeColour);

                i2 = goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,m2,Color21);
                goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,i2,t2,Color22);
                ColorPartOfCycle(G,pred->GetValue(i2),t2,Color23,*EdgeColour);

                i3 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l2,m2,Color31);
                goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i3,t2,Color32);
                ColorPartOfCycle(G,pred->GetValue(i3),t2,Color33,*EdgeColour);

                colorNodes(*NodeColour, l1,i2,i3, l2,m2,t2);
            }
            else
            {
                G.LogAppend(LOG_METH2, " d)");
                ColorPartOfCycle(G,pred->GetValue(t1),t2,Color11,*EdgeColour);
                goThroughSeg(G,*EdgeColour,PreO,*S1->Seg,l1,t1,Color12);
                if (l1 < m2)
                    ColorPartOfCycle(G,pred->GetValue(m2),l1,Color12,*EdgeColour);
                else if (m2 < l1)
                    ColorPartOfCycle(G,pred->GetValue(l1),m2,Color12,*EdgeColour);
                ColorPartOfCycle(G,pred->GetValue(l2),t1,Color13,*EdgeColour);

                i2 = goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,l2,m2,Color21);
                goThroughSeg(G,*EdgeColour,PreO,*S2->Seg,i2,t2,Color22);
                ColorPartOfCycle(G,pred->GetValue(i2),t2,Color23,*EdgeColour);

                i3 = goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,l2,m2,Color31);
                goThroughSeg(G,*EdgeColour,PreO,*S3->Seg,i3,t2,Color32);
                ColorPartOfCycle(G,pred->GetValue(i3),t2,Color33,*EdgeColour);

                colorNodes(*NodeColour, i2,i3,t1, l2,m2,t2);
            }
        }
    }
    else
    {
        G.LogAppend(LOG_METH2, "Fall (3,3,3)");
        list<TNode>::iterator a = S1->Att.begin();
        TNode t = OrigOrd[*a]; a++;
        TNode v = OrigOrd[*a]; a++;
        TNode l = OrigOrd[*a];
        TNode w;
        NodeColour->SetValue(t,2);
        NodeColour->SetValue(v,2);
        NodeColour->SetValue(l,2);

        w = goThroughSeg(G,*EdgeColour,PreO, *S1->Seg, v, l, Color11);
        NodeColour->SetValue(w,0);
        goThroughSeg(G, *EdgeColour,  PreO,  *S1->Seg, w, v, Color12);
        ColorPartOfCycle(G,pred->GetValue(w),t,Color13,*EdgeColour);

        w = goThroughSeg(G,*EdgeColour,PreO, *S2->Seg, v, l, Color21);
        NodeColour->SetValue(w,0);
        goThroughSeg(G, *EdgeColour,  PreO,  *S2->Seg, w, v, Color22);
        ColorPartOfCycle(G,pred->GetValue(w),t,Color23,*EdgeColour);

        w = goThroughSeg(G,*EdgeColour,PreO, *S3->Seg, v, l, Color31);
        NodeColour->SetValue(w,0);
        goThroughSeg(G, *EdgeColour,  PreO,  *S3->Seg, w, v, Color32);
        ColorPartOfCycle(G,pred->GetValue(w),t,Color33,*EdgeColour);
    }
}


//MinCycle == (B, X_3, X_4, ..., X_p, A, F)
void basicCase2(abstractMixedGraph& G, attribute<TNode>& PreO, TNode* OrigOrd,
                attribute<int>& EdgeType, vector<TSegPath*>& MinCycle)
{
    attribute<TNode>* NodeColour = G.Registers()->InitAttribute<TNode>(G,
            TokRegNodeColour, NoNode);
    attribute<TArc>* EdgeColour = G.Registers()->InitAttribute<TArc>(G,
            TokRegEdgeColour, NoArc);
    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);

    // Bestimme a2 und bpp
    TNode a2, bpp;
    TNode tailF = MinCycle[MinCycle.size()-1]->Att.front();
    list<TNode>& AttA = MinCycle[MinCycle.size()-2]->Att;
    list<TNode>::iterator ib = AttA.end();
    ib--;
    while ((ib!=AttA.begin()) && (*ib <= tailF))
    {
        a2 = *ib;
        ib--;
    }
    bpp = *ib;

    TNode a[5] = {NoNode,NoNode,NoNode,NoNode,NoNode};
    TNode b[5] = {NoNode,NoNode,NoNode,NoNode,NoNode};
    if (MinCycle[MinCycle.size()-3]->Att.back() <=
        MinCycle[MinCycle.size()-1]->Att.back())
    {
        a[0] = OrigOrd[MinCycle[MinCycle.size()-3]->Att.back()];
        list<TNode>::iterator v = MinCycle[MinCycle.size()-3]->Att.end();
        v--;
        while (*v < MinCycle[MinCycle.size()-5]->Att.front())
            v--;
        b[3] = OrigOrd[*v];
    }
    else {
        a[0] = OrigOrd[MinCycle[MinCycle.size()-3]->Att.front()];
        b[3] = OrigOrd[MinCycle[MinCycle.size()-3]->Att.back()];
    }
    a[1] = OrigOrd[a2]; //a_2
    a[2] = OrigOrd[MinCycle.back()->Att.front()]; //a_3
    a[3] = OrigOrd[MinCycle[0]->Att.front()]; //a_4
    a[4] = OrigOrd[MinCycle[MinCycle.size()-4]->Att.front()]; //a_p+1
    b[0] = OrigOrd[MinCycle[MinCycle.size()-1]->Att.back()]; //b_1
    b[1] = OrigOrd[MinCycle[0]->Att.back()]; //b_2
    b[2] = OrigOrd[MinCycle[1]->Att.back()]; //b_3
    b[4] = OrigOrd[bpp]; //b_p+1

    if ((MinCycle.size() == 5) && (a[0]==b[0]) && (a[1]==b[1]) && (a[2]==b[2]) &&
        (a[3]==b[3]) && (a[4]==b[4])) {
        G.LogEntry(LOG_METH2, "K_5 gefunden.");
        NodeColour->SetValue(a[0],1);
        NodeColour->SetValue(a[1],1);
        NodeColour->SetValue(a[2],1);
        NodeColour->SetValue(a[3],1);
        NodeColour->SetValue(a[4],1);

        ColorPartOfCycle(G,a[3],a[0],1,*EdgeColour);
        ColorPartOfCycle(G,a[0],a[3],1,*EdgeColour);
    }
    else {
        G.LogEntry(LOG_METH2, "K_3,3 gefunden; ");
        TNode k;
        if (a[3] != b[3])
            k = 0;
        else if (a[4] != b[4])
            k = 1;
        else if (a[0] != b[0])
            k = 2;
        else if (a[1] != b[1])
            k = 3;
        else // a[2] != b[2]
            k = 4;
        sprintf(G.Context().logBuffer,"(k = %lu)",static_cast<unsigned long>(k));
        G.LogAppend(LOG_METH2,G.Context().logBuffer);

        /* Legende: Betrachtet man bei k=0 die Knoten a_j und b_j, j=0,1,...,4,
         * aus HomKernel(SG(e)), so werden darueber die folgenden Wege definiert
         * (Indizes groesser als 4 sind modulo 5 zu betrachten):
         *  c[j]  := Weg von a_j nach b_j
         *  c_[j] := Weg von b_j nach a_j+1
         *  d[j]  := Weg von b_j nach a_j+2
         */

        //Farben der Wege (Farbe NoNode heisst nicht faerben)
        TNode c[5];
        TNode c_[5];
        TNode d[5];

        c[(0+k)%5] = 1;
        c[(1+k)%5] = 2;
        c[(2+k)%5] = 1;
        c[(3+k)%5] = 2;
        c[(4+k)%5] = 3;

        c_[(0+k)%5] = 1;
        c_[(1+k)%5] = NoNode;
        c_[(2+k)%5] = 2;
        c_[(3+k)%5] = 3;
        c_[(4+k)%5] = NoNode;

        d[(0+k)%5] = 1;
        d[(1+k)%5] = 2;
        d[(2+k)%5] = 3;
        d[(3+k)%5] = 1;
        d[(4+k)%5] = 3;

        //Wege von b0:
        //nach b2
        goThroughPath(G, *EdgeColour,*MinCycle.back(),d[0]);
        ColorPartOfCycle(G,pred->GetValue(b[2]),a[2],c[2], *EdgeColour);
        //nach a1
        ColorPartOfCycle(G,pred->GetValue(a[1]),b[0],c_[0],*EdgeColour);
        //nach b3
        ColorPartOfCycle(G,pred->GetValue(b[0]),a[0],c[0], *EdgeColour);
        if (PreO.GetValue(a[0]) > PreO.GetValue(b[3])) //von a0 nach b3 oder umgekehrt
            goThroughPath(G, *EdgeColour,*MinCycle[MinCycle.size()-3],d[3]);
        else
            goThroughSeg(G,*EdgeColour,PreO,*MinCycle[MinCycle.size()-3],
                         a[0],b[3], d[3]);

        //Wege von a3:
        //nach b3  (evtl. ueber mehrere Segmente)
        TNode li  = b[3];
        size_t i  = MinCycle.size()-5;
        TNode ti2 = G.StartNode(MinCycle[i]->e);
        ColorPartOfCycle(G,pred->GetValue(li),ti2,c[3], *EdgeColour);
        while (i > 1) {
            goThroughPath(G, *EdgeColour,*MinCycle[i],c[3]);
            TNode li  = G.EndNode(MinCycle[i]->Low1Edge);
            i = i - 2;
            TNode ti2 = G.StartNode(MinCycle[i]->e);
            ColorPartOfCycle(G,pred->GetValue(li),ti2,c[3], *EdgeColour);
        }
        //nach b2
        ColorPartOfCycle(G,pred->GetValue(a[3]),b[2],c_[2], *EdgeColour);
        //nach a1
        goThroughPath(G, *EdgeColour,*MinCycle[0],d[1]);
        ColorPartOfCycle(G,pred->GetValue(b[1]),a[1],c[1], *EdgeColour);

        //Wege von a4:
        //nach a1 (ueber die zwei Verb.punkte a und b)
        ColorPartOfCycle(G,pred->GetValue(b[4]),a[4],c[4], *EdgeColour);
        goThroughSeg(G,*EdgeColour,PreO,*MinCycle[MinCycle.size()-2],
                     b[4], a[1], d[4]);
        //nach b3
        ColorPartOfCycle(G,pred->GetValue(a[4]),b[3],c_[3], *EdgeColour);
        //nach b2  (evtl. ueber mehrere Segmente)
        i = MinCycle.size()-4;
        while (i > 1) {
            goThroughPath(G, *EdgeColour,*MinCycle[i],d[2]);
            TNode li  = G.EndNode(MinCycle[i]->Low1Edge);
            i = i - 2;
            TNode ti2 = G.StartNode(MinCycle[i]->e);
            ColorPartOfCycle(G,pred->GetValue(li),ti2,d[2], *EdgeColour);
        }
        goThroughPath(G, *EdgeColour,*MinCycle[1],d[2]);

        //nicht gefaerbte Kanten:
        ColorPartOfCycle(G,pred->GetValue(a[0]),b[4],c_[4], *EdgeColour);
        ColorPartOfCycle(G,pred->GetValue(a[2]),b[1],c_[1], *EdgeColour);

        //erst restliche Knoten:
//         NodeColour->SetValue(a[(0+k)%5],100);
//         NodeColour->SetValue(a[(2+k)%5],100);
//         NodeColour->SetValue(b[(1+k)%5],100);
//         NodeColour->SetValue(b[(4+k)%5],100);
        colorNodes(*NodeColour, b[(0+k)%5],a[(3+k)%5],a[(4+k)%5],
                                a[(1+k)%5],b[(2+k)%5],b[(3+k)%5]);
    }
}


void extractKuratowski(abstractMixedGraph& G, attribute<TNode>& PreO,
                       attribute<int>& EdgeType, attribute<TNode>& Low1,
                       TSegPath& FatherSeg, TSegPath& Seg, TSegGrComp& C,
                       TSegPath* disturbingSeg_i)
{
    moduleGuard(ModPlanarity,G,"Extracting Kuratowski Subgraph...");

    attribute<TArc>& pred = *G.Registers()->GetAttribute<TArc>(TokRegPredecessor);
    bool basicCaseB = false;

    if (disturbingSeg_i == NULL) {
        G.LogEntry(LOG_METH2,
            "Basisfall b): Forme Cycle(f) zu Segment von Cycle(e) um (f:=Father(e)).");
        basicCaseB = true;

        TArc g = FatherSeg.e;
        sprintf(G.Context().logBuffer," f=Seg(%lu,%lu) ",
            static_cast<unsigned long>(G.StartNode(FatherSeg.e)),
            static_cast<unsigned long>(G.EndNode(FatherSeg.e)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
        while (G.StartNode(g) != G.StartNode(Seg.e)) {
            g = G.First(G.EndNode(g));
        }
        disturbingSeg_i = new TSegPath(g);
        disturbingSeg_i->e = g;//Kante, die in StartNode(e) weggeht
        sprintf(G.Context().logBuffer,"-> e=(%lu,%lu),",
            static_cast<unsigned long>(G.StartNode(g)),
            static_cast<unsigned long>(G.EndNode(g)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);

        //Laufe Stem(f) rueckwaerts beginned bei StartNode(e) bis Low1(e)
        pred[G.EndNode(FatherSeg.Low1Edge)] = FatherSeg.Low1Edge;
        g = Seg.e;
        g = pred[G.StartNode(g)];
        while (PreO[G.EndNode(g)] != Low1[Seg.e>>1]) {
            g = pred[G.StartNode(g)];
        }
        disturbingSeg_i->Low1Edge = g;//Kante auf Cycle(f), die nach Low1(Seg->e) geht
        sprintf(G.Context().logBuffer,"-> Low1Edge=(%lu,%lu),",
            static_cast<unsigned long>(G.StartNode(g)),
            static_cast<unsigned long>(G.EndNode(g)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
        EdgeType[FatherSeg.Low1Edge] = TreeEdge;
        EdgeType[g] = BackEdge;

        disturbingSeg_i->Emanat_Index = 0xFFFFFFFF; //so gross wie moeglich
        disturbingSeg_i->Att = list<TNode>();
        disturbingSeg_i->Att.push_front(Low1[Seg.e>>1]);
        disturbingSeg_i->Att.push_front(PreO[G.StartNode(Seg.e)]);
    }

    // calculate odd cycle in C and shorten it
    vector<TSegPath*> MinCycle;
    MinOddSGCycle(G, PreO, MinCycle, Seg, C, *disturbingSeg_i);

    G.LogEntry(LOG_METH2, "MinOddSGCycle: ");
    for (vector<TSegPath*>::iterator S=MinCycle.begin(); S!=MinCycle.end(); S++)
    {
        sprintf(G.Context().logBuffer,"Seg(%lu,%lu),",
            static_cast<unsigned long>(G.StartNode((*S)->e)),
            static_cast<unsigned long>(G.EndNode((*S)->e)));
        G.LogAppend(LOG_METH2, G.Context().logBuffer);
    }
    G.LogEntry(LOG_METH2, "");

    pred[G.EndNode(Seg.Low1Edge)] = Seg.Low1Edge;


    TNode* OrigOrd = new TNode[G.N()];
    for (TIndex n=0; n<G.N(); n++)
        OrigOrd[ PreO[n] ] = n;

    if (MinCycle.size() <= 3) {
        G.LogEntry(LOG_METH2,"First basic case: ");
        basicCase1(G, PreO, OrigOrd, EdgeType, MinCycle);
    }
    else {
        G.LogEntry(LOG_METH2,"Second basic case: ");
        basicCase2(G, PreO, OrigOrd, EdgeType, MinCycle);
    }

    delete [] OrigOrd;
    if (basicCaseB) delete disturbingSeg_i;
}


void embedding(abstractMixedGraph& G, attribute<int>& EdgeType, TSegPath& Seg,
               bool canonical, list<TArc>& U, list<TArc>& A, TArc* predArc)
{
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    G.OpenFold();
    if (canonical != Seg.alphaL)
    {
        sprintf(G.Context().logBuffer,"Embed Seg(%lu,%lu) on right; ",
            static_cast<unsigned long>(G.StartNode(Seg.e)),
            static_cast<unsigned long>(G.EndNode(Seg.e)));
    }
    else
    {
        sprintf(G.Context().logBuffer,"Embed Seg(%lu,%lu) on left; ",
            static_cast<unsigned long>(G.StartNode(Seg.e)),
            static_cast<unsigned long>(G.EndNode(Seg.e)));
    }
    G.LogEntry(LOG_METH2, G.Context().logBuffer);

    attribute<TArc>* pred = G.Registers()->GetAttribute<TArc>(TokRegPredecessor);
    TArc g = Seg.Low1Edge;
    TNode v = G.StartNode(g);
    U.push_back(g);

    vector<TSegPath*>::iterator Seg_i = Seg.Emanat.begin();
    list<TArc> U2, A2, AL, AR;

    while (g != Seg.e) {
        while ((Seg_i != Seg.Emanat.end()) && (G.StartNode((*Seg_i)->e)==v))
        {
            if (EdgeType.GetValue((*Seg_i)->e) == TreeEdge)
            {
                embedding(G, EdgeType, **Seg_i, (canonical!=(*Seg_i)->alphaL),
                          U2, A2, predArc);
            }
            else
            {
                G.OpenFold();
                if (canonical != (*Seg_i)->alphaL)
                {
                    sprintf(G.Context().logBuffer,"Embed Seg(%lu,%lu) on right;",
                        static_cast<unsigned long>(G.StartNode((*Seg_i)->e)),
                        static_cast<unsigned long>(G.EndNode((*Seg_i)->e)));
                }
                else
                {
                    sprintf(G.Context().logBuffer,"Embed Seg(%lu,%lu) on left;",
                        static_cast<unsigned long>(G.StartNode((*Seg_i)->e)),
                        static_cast<unsigned long>(G.EndNode((*Seg_i)->e)));
                }
                G.LogEntry(LOG_METH2, G.Context().logBuffer);

                U2.push_back((*Seg_i)->e);
                A2.push_back((*Seg_i)->e ^ 1);
                G.CloseFold();
            }

            if (canonical != (*Seg_i)->alphaL)
            {
                U.splice(U.begin(), U2);
                AR.splice(AR.end(), A2);
            }
            else
            {
                U.splice(U.end(), U2);
                AL.splice(AL.begin(), A2);
            }
            Seg_i++;
        }

        g = pred->GetValue(v);
        v = G.StartNode(g);
        U.push_front(g^1);

        //output
        list<TArc>::iterator a = U.begin();
        TArc b = *a;
        GR->SetFirst(G.EndNode(g),*a);
        a++;
        while (a != U.end())
        {
            if (predArc)
                predArc[*a] = b^1;
            GR->SetRight(b,*a);
            b = *a;
            a++;
        }

        if (predArc)
            predArc[U.front()] = U.back() ^ 1;

        //preparing for next loop
        U.clear();
        while (!AR.empty() && (G.StartNode(AR.back()) == v))
        {
            U.push_back(AR.back());
            AR.pop_back();
        }
        U.push_front(g);
        while (!AL.empty() && (G.StartNode(AL.front()) == v))
        {
            U.splice(U.begin(), AL, AL.begin());
        }
    }
    A.splice(A.end(), AL);
    A.push_back(Seg.Low1Edge ^ 1);
    A.splice(A.end(), AR);

    G.CloseFold();
}


bool abstractMixedGraph::PlanarityHopcroftTarjan(TArc* predArc,
               bool extractMinor) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation())
        NoSparseRepresentation("PlanarityHopcroftTarjan");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    moduleGuard M(ModPlanarityHoTa,*this,moduleGuard::SHOW_TITLE);

    attribute<bool>* V_T    = registers.InitAttribute<bool>(*this,TokV_T,false);
    int card_V_T = 0;
    attribute<int>* EdgeType= registers.InitAttribute<int>(*this,TokEdgeType, 0);
    attribute<TArc>*  pred  = registers.InitAttribute<TArc>(*this,TokRegPredecessor,NoArc);
    attribute<TNode>* PreO  = registers.InitAttribute<TNode>(*this,TokPreO, 0);
    attribute<TNode>* PostO = registers.InitAttribute<TNode>(*this,TokPostO, 0);
    int POST = 0;
    attribute<TNode>* Low1  = registers.InitAttribute<TNode>(*this,TokLow1, 0);
    attribute<TNode>* Low2  = registers.InitAttribute<TNode>(*this,TokLow2, 0);

    V_T->SetValue(0,true);
    PreO->SetValue(0,0);

    LogEntry(LOG_METH,"First DFS...");
    preparingDFS(*this,0,*V_T,card_V_T,*EdgeType,*pred,*PreO,
                 *PostO,POST, *Low1,*Low2);

    LogEntry(LOG_METH,"Reordering arcs...");
    reorder(*this,*EdgeType,*PreO,*Low1,*Low2);

    LogEntry(LOG_METH,"Calculating the path tree...");
    TSegPath* RootSeg = createSegPath(*this,*PreO,*EdgeType,*Low1,*Low2,First(0));
    RootSeg->Emanat_Index = 0;
    RootSeg->alphaL = false;
    if (CT.logMeth>1) {
        LogEntry(LOG_METH2,"Elements of path tree are:");
        printPathTree(*this, *EdgeType, RootSeg);
        LogEntry(LOG_METH2,"Hint: Path (b1,b2,b3,...) is child of path (a1,a2,...), if the");
        LogEntry(LOG_METH2,"      first vertex b1 is equal to one vertex of path (a1,a2,...)");
    }

    LogEntry(LOG_METH,"Testing (strong) planarity...");
    attribute<TNode>* NodeColour = registers.InitAttribute<TNode>(*this,
                                                        TokRegNodeColour,0);
    attribute<TArc>* EdgeColour = registers.InitAttribute<TArc>(*this,
                                                        TokRegEdgeColour, 0);
    list<TNode> RootSegHeads;
    bool ret = stronglyplanar(*this,*PreO,*EdgeType,*Low1,*Low2,
                              NULL,*RootSeg,extractMinor,RootSegHeads);

    if (ret) {
        LogEntry(LOG_METH,"Calculating planar representation...");

        list<TArc> U, A;
        if (predArc)
            for (TArc a=0;a<2*m;a++)
                predArc[a] = NoArc;

        embedding(*this,*EdgeType,*RootSeg, false, U, A, predArc);

        list<TArc>::iterator a = U.begin();
        TArc b = *a;
        X -> SetFirst(StartNode(*a),*a);
        a++;
        while (a != U.end()) {
            if (predArc)
                predArc[*a] = b^1;
            X -> SetRight(b,*a);
            b = *a;
            a++;
        }
        a = A.begin();
        while (a != A.end()) {
            if (predArc)
                predArc[*a] = b^1;
            X -> SetRight(b,*a);
            b = *a;
            a++;
        }
        if (predArc)
            predArc[U.front()] = A.back()^1;


        //make the attribute PreO,Low1,Low2 visible via NodeColour,Length,UCap;
        for (TNode n = 0; n<N(); n++)
            NodeColour->SetValue(n,PreO->GetValue(n));
//0 "No Labels"; 1 "Indices {0,1,2,..}"; 6 "Indices {1,2,3,..}";
//5 "Demands";   2 "Distance Labels";    3 "Potentials";     4 "Node Colours"

        for (TArc a = 0; a<m; a++) {
            if ((EdgeType->GetValue(a<<1)==TreeEdge) ||
                (EdgeType->GetValue((a<<1)+1)==TreeEdge))
                EdgeColour->SetValue(a,1);
            else
                EdgeColour->SetValue(a,0);
            X -> SetLength(a<<1,Low1->GetValue(a));
            X -> SetUCap(a<<1,Low2->GetValue(a));
        }
// 0 "No Labels";          1 "Indices {0,1,2,..}";     7 "Indices {1,2,3,..}";
// 2 "Upper Cap. Bounds";  6 "Lower Cap. Bounds";      4 "Length Labels";
// 5 "Red.Length Labels";  3 "Subgraph / Flow Labels"; 8 "Edge Colours"
    }

    delete RootSeg;
    registers.ReleaseAttribute(TokV_T);
    registers.ReleaseAttribute(TokEdgeType);
    registers.ReleaseAttribute(TokPreO);
    registers.ReleaseAttribute(TokPostO);
    registers.ReleaseAttribute(TokLow1);
    registers.ReleaseAttribute(TokLow2);

    return ret;
}


typedef struct {
    segmentGraph*   (*Segments);
    TNode           Count;
    TNode           StartNewSeg;
    TNode           EmbSegNo;       //fr recalc == true
} TSegData;


typedef struct {
    TArc*                       predArc;
    goblinHashTable<TArc,TArc>* succArc;
    TNode*                      incidentRegion;
    int                         Count;
    int                         MinRegions;
    int                         Region1No,Region2No;
} TRegData;

typedef segmentGraph*  PSegmentGraph;


//forward declaration
static void determineSegments(TSegData& SD, abstractSubgraph* oldSegment,
        abstractSubgraph* Embedded, const abstractSubgraph* ToEmbed,
        abstractMixedGraph* G);
static void determinePossibleRegions(TSegData&,TRegData&,abstractMixedGraph*,bool);

static staticStack<TNode>* Q;


bool abstractMixedGraph::IsPlanar(TMethPlanarity method,TOptPlanarity options)
    throw (ERRejected)
{
    moduleGuard M(ModPlanarity,*this,"Testing planarity...");

    bool ret = PlanarityMethod(method,options,NULL);

    M.Shutdown(LOG_RES, ret ? "...Graph is planar" : "...Graph is non-planar");

    return ret;
}


bool abstractMixedGraph::PlanarizeIncidenceOrder(TMethPlanarity method)
    throw (ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("PlanarizeIncidenceOrder");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    moduleGuard M(ModPlanarity,*this,"Computing planar representation...");

    // Store the regions in terms of arc predecessor lists
    TArc *predArc = new TArc[2*m];

    bool ret = PlanarityMethod(method,PLANAR_NO_OPT,predArc);

    if (ret)
    {
        OpenFold();
        LogEntry(LOG_METH,"Updating incidence lists...");

        X -> ReorderIncidences(predArc);

        CloseFold();
    }

    delete[] predArc;

    M.Shutdown(LOG_RES, ret ? "...Graph is planar" : "...Graph is non-planar");

    return ret;
}


bool abstractMixedGraph::PlanarityMethod(
    TMethPlanarity method,TOptPlanarity options,TArc* predArc) throw (ERRejected)
{
    if (method==PLANAR_DEFAULT) method = TMethPlanarity(CT.methPlanarity);
    if (method==PLANAR_DEFAULT) method = PLANAR_HO_TA;

    OpenFold();

    sparseGraph G(*this,OPT_CLONE);

    bool isPlanar = true;

    // Store the constructed regions in terms of arc predecessor lists
    TArc* predArcLocal = new TArc[2*m];

    // For every embedded node, one of the embedded incident arcs must be known
    TArc* predFirst = new TArc[n];
    for (TNode v=0;v<n;v++) predFirst[v] = NoArc;

    // For every class of parallel arcs remember an embedded arc.
    TArc* embeddedParallel = new TArc[2*m];

    attribute<TArc>* pred = registers.MakeAttribute<TArc>(*this,TokRegPredecessor,
            attributePool::ATTR_FULL_RANK, 0);

    // The core planarity methods apply to 2-connected graphs only.
    // Compute the 2-blocks and embed them separately.
    G.CutNodes();

    // Partition the arc set into the index sets for each block
    staticQueue<TArc>** block = new staticQueue<TArc>*[1];
    block[0] = new staticQueue<TArc>(m,CT);
    TArc maxColour = 0;

    for (TArc a=0;a<m;a++)
    {
        if (predArc) predArc[2*a] = predArc[2*a+1] = NoArc;

        if (StartNode(2*a)==EndNode(2*a)) continue;

        TArc thisColour = G.EdgeColour(2*a);

        if (thisColour>maxColour)
        {
            block = (staticQueue<TArc>**)GoblinRealloc(block,
                        (thisColour+1)*sizeof(staticQueue<TArc>*));
            for (TArc i=maxColour+1;i<=thisColour;i++)
                block[i] = new staticQueue<TArc>(*block[0]);

            maxColour = thisColour;
        }

        block[thisColour] -> Insert(a);
    }


    TArc mEmbedded = 0;

    for (TArc c=0;c<=maxColour && isPlanar;c++)
    {
        // Export the c-th 2-block to a new graph H
        inducedSubgraph H(G,fullIndex<TNode>(n,CT),
            *block[c],OPT_MAPPINGS|OPT_NO_LOOPS);

        if (H.N()>3 && H.M()>3*TArc(H.N())-6 && !(options & PLANAR_MINOR))
        {
            isPlanar = false;
            break;
        }

        if (   (predArc && H.M()>1)
            || (H.M()>8 && H.N()>4 && H.M()>TArc(H.N())+2)
           )
        {
            sprintf(CT.logBuffer,"Embedding block %lu...",static_cast<unsigned long>(c));
            LogEntry(LOG_METH,CT.logBuffer);

            if (method==PLANAR_DMP)
            {
                isPlanar = H.PlanarityDMP64(predArcLocal);
            }
            else
            {
                isPlanar = H.PlanarityHopcroftTarjan(predArcLocal,options & PLANAR_MINOR);
            }
        }

        if (predArc && H.M()>0 && isPlanar)
        {
            if (H.M()==1)
            {
                predArcLocal[0] = 1;
                predArcLocal[1] = 0;
            }

            // Merge the embedding of H into the original graph
            for (TNode v=0;v<H.N();v++)
            {
                TArc a0 = H.First(v);

                TArc a = a0;

                do
                {
                    // Translate embedding of H to the original graph
                    TArc a1 = predArcLocal[a];
                    TArc a2 = H.OriginalOfArc(a);
                    predArc[a2] = H.OriginalOfArc(a1);
                    embeddedParallel[G.Adjacency(
                        G.StartNode(a2),G.EndNode(a2))] = a2;
                    a = a1^1;
                }
                while (a != a0);

                TNode v1 = H.OriginalOfNode(v);
                TArc aFirst = predFirst[v1];

                if (aFirst!=NoArc)
                {
                    // Link with the preceding 2-blocks

                    TArc aPred = predArc[aFirst];
                    predArc[aFirst] = predArc[H.OriginalOfArc(a0)];
                    predArc[H.OriginalOfArc(a0)] = aPred;
                }
                else predFirst[v1] = H.OriginalOfArc(a0);
            }
        }

        if (!isPlanar && (options & PLANAR_MINOR) && method!=PLANAR_DMP)
        {
            // Map the forbidden minor to the original graph
            TArc* edgeColour = InitEdgeColours();
            TArc* edgeColourH = H.GetEdgeColours();

            for (TArc a=0;a<H.M();a++)
            {
                edgeColour[H.OriginalOfArc(2*a)>>1] = edgeColourH[a];
            }

            TArc* predH = H.GetPredecessors();
            TNode* nodeColourH = H.GetNodeColours();

            for (TNode n=0; n<H.N(); n++)
            {
                pred->SetValue(H.OriginalOfNode(n), predH[n]);
                SetNodeColour(H.OriginalOfNode(n), nodeColourH[n]);
            }
        }

        mEmbedded += H.M();
    }

    delete[] predArcLocal;
    delete[] predFirst;
    for (TArc i=1;i<=maxColour;i++)
    {
        delete block[i];
    }
    delete block[0];
    delete[] block;

    if (predArc && mEmbedded<m && isPlanar)
    {
        LogEntry(LOG_METH,"Postprocessing...");

        TArc mLoops = 0;

        for (TArc a=0;a<m;a++)
        {
            TNode v = StartNode(2*a);

            if (EndNode(2*a)!=v) continue;

            TArc a2 = 2*a;

            do
            {
                a2 = Right(a2,v);
            }
            while (predArc[a2]==NoArc && a2!=2*a);

            if (a2==2*a)
            {
                // Only loops are incident with v,
                // and this is the first one to embed

                predArc[2*a+1]  = 2*a+1;
                predArc[2*a]    = 2*a;
            }
            else
            {
                predArc[2*a+1]  = 2*a+1;
                predArc[2*a]    = predArc[a2];
                predArc[a2]     = 2*a;
            }

            mLoops++;
        }

        if (mLoops>0 && CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...%lu loops embedded",static_cast<unsigned long>(mLoops));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        mEmbedded += mLoops;
    }

    if (predArc && mEmbedded<m && isPlanar)
    {
        TArc mParallels = 0;
        TArc* succArc = new TArc[2*m];

        for (TArc a=0;a<m;a++)
        {
            if (predArc[2*a]!=NoArc)
            {
                succArc[predArc[2*a]] = 2*a;
                succArc[predArc[2*a+1]] = 2*a+1;
            }
        }

        for (TArc a=0;a<m;a++)
        {
            TNode v = StartNode(2*a);
            TNode w = EndNode(2*a);

            if (v==w) continue;

            TArc a2 = embeddedParallel[G.Adjacency(v,w)];

            if (a==(a2>>1)) continue;

            TArc a3 = First(w);
            while (predArc[a3]!=a2) a3 = Right(a3,w);

            predArc[a3]     = 2*a;
            predArc[2*a+1]  = a2;
            predArc[2*a]    = predArc[a2];
            predArc[a2]     = 2*a+1;

            succArc[predArc[a3]]    = a3;
            succArc[predArc[2*a+1]] = 2*a+1;
            succArc[predArc[2*a]]   = 2*a;
            succArc[predArc[a2]]    = a2;

            mParallels++;
        }

        delete[] succArc;

        if (mParallels>0 && CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...%lu parallel arcs embedded",
                static_cast<unsigned long>(mParallels));
            LogEntry(LOG_METH2,CT.logBuffer);
        }
    }

    delete[] embeddedParallel;

    CloseFold();

    return isPlanar;
}


bool abstractMixedGraph::PlanarityDMP64(TArc* predArc) throw (ERRejected)
{
    TRegData          RegData;
    TSegData          SegData;


    // Determine the number of arcs in the underlying simple graph
    TArc m0 = 0;
    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (Adjacency(u,v)==2*a) m0++;
    }

    if (m0>3*TArc(n)-6) return false;


    moduleGuard M(ModPlanarityDMP,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m0+1,1);

    #endif

    RegData.predArc = predArc;
    RegData.succArc = new goblinHashTable<TArc,TArc>(n*m,2*(m+n),NoArc,CT);
    RegData.incidentRegion = new TNode[2*m+n];

    for (TArc a=0;a<2*m;a++)
    {
        RegData.predArc[a] = NoArc;
        RegData.incidentRegion[a] = NoNode;
    }

    sparseGraph G(*this,OPT_CLONE);

    // Queue for the nodes of G when computing connected components
    Q = new staticStack<TNode>(n,CT);

    // Export the embedded subgraph to a trace image
    G.InitSubgraph();


    LogEntry(LOG_METH,"Extracting an initial cycle...");
    OpenFold();

    // Construct the initial cycle by a DFS search rooted at node 0

    G.InitPredecessors();
    TArc a0 = G.First(0);
    TNode v = NoNode;

    do
    {
        v = G.EndNode(a0);
        G.SetPred(v,a0);
        a0 = G.Right(a0^1);
    }
    while (G.EndNode(a0)!=0 && G.Pred(G.EndNode(a0))==NoArc);

    TNode u = G.EndNode(a0);
    TNode w = u;
    TArc a = a0;

    // Now the arc a0 completes a cycle in G.
    // Trace back the search path until w is reached.

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1) LH = LogStart(LOG_METH2, "(");

    #endif

    while (v!=u)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer, "%lu[%lu]",
                static_cast<unsigned long>(w),static_cast<unsigned long>(a^1));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TArc a2 = G.Pred(v);
        RegData.predArc[a] = a2;
        RegData.predArc[a2^1] = (a^1);
        RegData.incidentRegion[a] = 0;
        RegData.incidentRegion[a2^1] = 1;
        RegData.succArc -> ChangeKey(0*n+v,a);
        RegData.succArc -> ChangeKey(1*n+v,a2^1);

        a = a2;
        w = v;
        v = G.StartNode(a);
    }

    RegData.predArc[a] = a0;
    RegData.predArc[a0^1] = (a^1);
    RegData.incidentRegion[a] = 0;
    RegData.incidentRegion[a0^1] = 1;
    RegData.succArc -> ChangeKey(0*n+v,a);
    RegData.succArc -> ChangeKey(1*n+v,a0^1);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer, "%lu[%lu]%lu)",
            static_cast<unsigned long>(w),
            static_cast<unsigned long>(a^1),
            static_cast<unsigned long>(v));
        LogEnd(LH,CT.logBuffer);
    }

    #endif

    CloseFold();


    LogEntry(LOG_METH, "Determine the initial segments...");
    OpenFold();

    abstractSubgraph* Embedded = new subgraph(&G);

    for (TArc a=0;a<G.M();a++)
    {
        if (RegData.predArc[2*a]!=NoArc)
        {
            Embedded->AddArc(2*a);

            // Export the embedded subgraph to a trace image
            G.SetSub(2*a,G.UCap(2*a));
        }
    }

    const abstractSubgraph* ToEmbed = Embedded->ComplementarySubgraph();


    SegData.Segments = NULL;
    SegData.Count = 0;
    SegData.StartNewSeg = 0;
    SegData.EmbSegNo = 0;

    determineSegments(SegData,NULL,Embedded,ToEmbed,&G);

    CloseFold();

    RegData.Count = 2;
    RegData.Region1No = 0;
    RegData.Region2No = 1;

    determinePossibleRegions(SegData,RegData,&G,false);

    LogEntry(LOG_METH, "Try to embed all segments...");
    OpenFold();

    int i = 1;
    while ( (Embedded->M() < G.M()) && (SegData.Count > 0) )
    {
        #if defined(_PROGRESS_)

        M.SetProgressCounter(Embedded->M());

        #endif

        M.Trace(G);

        segmentGraph* Segment = SegData.Segments[SegData.EmbSegNo];

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,
                "Segment %lu has a minimum number of possible regions",
                static_cast<unsigned long>(SegData.EmbSegNo));
            LogEntry(LOG_METH2,CT.logBuffer);
        }


        TArc* pred = G.InitPredecessors();

        THandle H = G.Investigate();
        investigator &I = G.Investigator(H);

        TNode u = Segment->GetContactNode(0);
        TNode w = u;

        while (w==u || !Embedded->HasNode(w))
        {
            if (I.Active(w))
            {
                TArc a = I.Read(w);
                TNode v = G.EndNode(a);

                if (pred[v]==NoArc && Segment->HasArc(a))
                {
                    pred[v] = a;
                    w = v;
                }
            }
            else w = G.StartNode(pred[w]);
        }

        G.Close(H);


        RegData.Region1No = Segment->GetRegion(0);
        RegData.Region2No = RegData.Count;
        RegData.Count++;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Splitting region %lu into two parts...",
                static_cast<unsigned long>(RegData.Region1No));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        // Determine the successor arc of w in the current region
        TArc aSuccW = RegData.succArc -> Key(RegData.Region1No*n+w);

        // Move uw-path from Region1 to Region2
        TArc a = aSuccW;
        while (G.StartNode(a) != u)
        {
            a = RegData.predArc[a];
            RegData.incidentRegion[a] = RegData.Region2No;
            RegData.succArc -> ChangeKey(RegData.Region2No*n+G.StartNode(a),a);
            RegData.succArc -> ChangeKey(RegData.Region1No*n+G.StartNode(a),NoArc);
        }

        TArc aSuccU = a;


        LogEntry(LOG_METH2,"Adding path to both regions and removing it from segment...");

        TNode v1, v1_succ;
        v1_succ = v1 = w;
        subgraph *oldSegment = new subgraph(Segment);
        Segment -> OmitNode(w);

        OpenFold();
        THandle LH = LogStart(LOG_METH2, "(");

        a = NoArc;
        TArc a2 = aSuccW;
        TArc a1 = RegData.predArc[a2];

        while (v1 != u)
        {
            a = pred[v1];
            v1 = G.StartNode(a);

            Segment -> OmitNode(v1);
            Embedded -> AddArc(a);

            RegData.predArc[a^1] = a1;
            RegData.predArc[a2] = a;
            RegData.incidentRegion[a^1] = RegData.Region2No;
            RegData.incidentRegion[a] = RegData.Region1No;
            RegData.succArc -> ChangeKey(RegData.Region2No*n+v1_succ,a^1);
            RegData.succArc -> ChangeKey(RegData.Region1No*n+v1,a);

            // Export the embedded subgraph to a trace image
            G.SetSub(a,G.UCap(a));

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer, "%lu[%lu]",
                    static_cast<unsigned long>(v1_succ),
                    static_cast<unsigned long>(a^1));
                LogAppend(LH,CT.logBuffer);
            }

            v1_succ = v1;
            a1 = (a^1);
            a2 = a;
        }

        a2 = aSuccU;
        a1 = RegData.predArc[a2];

        RegData.predArc[a2] = (a^1);
        RegData.predArc[a] = a1;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer, "%lu)",static_cast<unsigned long>(v1_succ));
            LogEnd(LH,CT.logBuffer);
        }
        CloseFold();


        LogEntry(LOG_METH2, "Determine new segments...");
        OpenFold();
        determineSegments(SegData,oldSegment,Embedded,NULL,&G);
        delete oldSegment;
        CloseFold();


        LogEntry(LOG_METH2, "Updating possible regions of all segments...");
        determinePossibleRegions(SegData,RegData,&G,true);

        if (RegData.MinRegions <= 0)
        {
            delete[] RegData.incidentRegion;
            delete RegData.succArc;

            // This implicitly deletes ToEmbed;
            delete Embedded;

            CloseFold();
            sprintf(CT.logBuffer,"Segment %lu cannot be embedded!",
                static_cast<unsigned long>(SegData.EmbSegNo));
            M.Shutdown(LOG_METH2,CT.logBuffer);

            return false;
        }

        i++;
    }

    CloseFold();

    delete[] RegData.incidentRegion;
    delete RegData.succArc;

    // This implicitly deletes ToEmbed;
    delete Embedded;

    delete Q;

    return true;
}


/*!
 * /param The data structure SD delivers the old segments
 * /param oldSegment is the segment from which a path has been embedded.
 *        The path is not deleted from the segment yet
 *        If oldSegment==NULL, the whole graph is considered.
 * /param Embedded is the embedded subgraph (including the new path)
 * /param ToEmbed is the node complement of Embedded
 * /param G is the entire graph
 */
void determineSegments(TSegData& SD, abstractSubgraph* oldSegment,
        abstractSubgraph* Embedded, const abstractSubgraph* ToEmbed,
        abstractMixedGraph* G)
{
    segmentGraph*       Segment;

    goblinController& CT = G->Context();

    if (oldSegment)
    {
        ToEmbed = SD.Segments[SD.EmbSegNo];
        TArc m = oldSegment->M();

        SD.Count--;
        for (TNode c=SD.EmbSegNo; c<SD.Count; c++)
            SD.Segments[c] = SD.Segments[c+1];

        sprintf(CT.logBuffer,"Former segment has %lu arcs",
            static_cast<unsigned long>(m));
        G->LogEntry(LOG_METH2,CT.logBuffer);

        if (m == 1)
        {
            SD.Segments = (segmentGraph **)GoblinRealloc(SD.Segments,
                SD.Count * sizeof(segmentGraph *));
            SD.StartNewSeg = SD.Count;
            delete ToEmbed;
            return;
        }
    }


    G -> LogEntry(LOG_METH2, "Computing connected components...");
    G -> OpenFold();

    TNode* nodeColour = G->InitNodeColours(NoNode);
    TNode CompCount = 0;

    THandle H = G->Investigate();
    investigator &I = G->Investigator(H);

    for (TNode u=0; u<G->N(); u++)
    {
        if (Embedded->HasNode(u) || nodeColour[u]!=NoNode ||
            (oldSegment && !oldSegment->HasNode(u))
           ) continue;

        Q -> Insert(u);
        nodeColour[u] = CompCount;

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Component %lu:",static_cast<unsigned long>(CompCount));
            LH = G->LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (!(Q->Empty()))
        {
            TNode u = Q->Delete();

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer," %lu",static_cast<unsigned long>(u));
                G -> LogAppend(LH,CT.logBuffer);
            }

            #endif

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = G->EndNode(a);

                if (nodeColour[v]==NoNode &&
                    !Embedded->HasNode(v) &&
                    ToEmbed->HasArc(a) &&
                    (!oldSegment || oldSegment->HasNode(v))
                   )
                {
                    nodeColour[v] = CompCount;
                    if (!Q->IsMember(v)) Q->Insert(v);
                }
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) G->LogEnd(LH);

        #endif

        CompCount++;
    }

    G -> Close(H);
    G -> CloseFold();


    G->LogEntry(LOG_METH2, "Create segment objects; marking all contact points...");

    // Allocate new segments for the found connected components.
    // Later, additional segments are added which consist of a single arc.
    if (!oldSegment)
    {
        SD.Count = 0;
        SD.Segments = new PSegmentGraph[CompCount*sizeof(PSegmentGraph)];
    }
    else
    {
        SD.Segments = (segmentGraph **)GoblinRealloc(SD.Segments,
            (SD.Count+CompCount) * sizeof(segmentGraph *));
        delete ToEmbed;
    }

    for (TNode c=SD.Count; c<SD.Count+CompCount; c++)
         SD.Segments[c] = new segmentGraph(G);
    SD.StartNewSeg = SD.Count;

    for (TArc a=0; a<G->M()*2; a++)
    {
        // Consider only arcs of the former segment
        if (oldSegment && !oldSegment->HasArc(a))
            continue;

        TNode sNode = G->StartNode(a);
        TNode eNode = G->EndNode(a);

        // Do not consider loops
        if (sNode==eNode) continue;

        // Do not consider parallel arcs
        if (!oldSegment && (a>>1) != (G->Adjacency(sNode,eNode)>>1)) continue;

        if (!Embedded->HasArc(a))
        {
            if (!Embedded->HasNode(sNode) || !Embedded->HasNode(eNode))
            {
                TNode color = G->NodeColour(sNode);
                if (color==NoNode) color = G->NodeColour(eNode);

                Segment = SD.Segments[color + SD.Count];
                Segment->AddArc(a);

                if (Embedded->HasNode(sNode))
                {
                    Segment -> MarkAsContactNode(sNode);
                }

                if (Embedded->HasNode(eNode))
                {
                    Segment -> MarkAsContactNode(eNode);
                }
            }
            else
            {
                // Check if a is already stored as a segment
                bool add = true;
                for (TNode s = 0; s < SD.Count+CompCount; s++)
                {
                    Segment = SD.Segments[s];
                    if ((Segment->GetContactNodeCount() == 2)
                        && Segment->HasArc(a))
                    {
                        add = false;
                        break;
                    }
                }

                if (add)
                {
                    CompCount++;
                    SD.Segments = (segmentGraph **)GoblinRealloc(SD.Segments,
                                   (SD.Count+CompCount)*sizeof(segmentGraph*));
                    SD.Segments[SD.Count+CompCount-1] = new segmentGraph(G);
                    Segment = SD.Segments[SD.Count+CompCount-1];
                    Segment->AddArc(a);
                    Segment->MarkAsContactNode(sNode);
                    Segment->MarkAsContactNode(eNode);
                }
            }
        }
    }


    TNode newSegCount = 0;
    for (TNode c=SD.Count; c<SD.Count+CompCount; c++)
    {
        Segment = SD.Segments[c];
        SD.Segments[SD.Count+newSegCount] = SD.Segments[c];
        newSegCount++;
    }

    SD.Count += newSegCount;
    SD.Segments = (segmentGraph **)GoblinRealloc(SD.Segments,SD.Count*sizeof(segmentGraph*));
}


void determinePossibleRegions(TSegData &SegData,TRegData &RegData,abstractMixedGraph *G,bool recalc)
{
    segmentGraph *Segment;

    #if defined(_LOGGING_)

    goblinController& CT = G->Context();

    #endif

    SegData.EmbSegNo = NoNode;
    RegData.MinRegions = 1000000;

    G -> OpenFold();

    // For each old segment:
    for (TNode s = 0; s<SegData.StartNewSeg && recalc; s++)
    {
        Segment = SegData.Segments[s];

        if (Segment->HasRegion(RegData.Region1No))
        {
            for (TNode CN = 0; CN<Segment->GetContactNodeCount(); CN++)
            {
                if (RegData.succArc ->
                      Key(RegData.Region1No*G->N()+Segment->GetContactNode(CN)) == NoArc)
                {
                    Segment->OmitRegion(RegData.Region1No);
                    break;
                }
            }

            bool fit = true;
            for (TNode CN = 0; CN<Segment->GetContactNodeCount(); CN++)
            {
                if (RegData.succArc ->
                      Key(RegData.Region2No*G->N()+Segment->GetContactNode(CN)) == NoArc)
                {
                    fit = false;
                    break;
                }
            }

            if (fit) Segment -> AddRegion(RegData.Region2No);
            // This region is new, and no segment is incident yet
            // Hence no Segment->omitRegion are necessary
        }

        if (Segment->GetRegionsCount() < RegData.MinRegions)
        {
            SegData.EmbSegNo = s;
            RegData.MinRegions = Segment->GetRegionsCount();
        }
    }

    // For each new segment:
    for (TNode s = SegData.StartNewSeg; s<SegData.Count; s++)
    {
        Segment = SegData.Segments[s];

        for (int r=0; r<RegData.Count; r++)
        {
            bool fit = true;
            for (TNode CN = 0; CN<Segment->GetContactNodeCount(); CN++)
            {
                if (RegData.succArc ->
                      Key(r*G->N()+Segment->GetContactNode(CN)) == NoArc)
                {
                    fit = false;
                    break;
                }
            }

            if (fit) Segment->AddRegion(r);
        }

        if (Segment->GetRegionsCount() < RegData.MinRegions)
        {
            SegData.EmbSegNo = s;
            RegData.MinRegions = Segment->GetRegionsCount();
        }
    }

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    for (TNode s = 0; s<SegData.Count && CT.logMeth>1; s++)
    {
        Segment = SegData.Segments[s];

        sprintf(CT.logBuffer,"Segment %lu fits into regions",
            static_cast<unsigned long>(s));
        LH = G->LogStart(LOG_METH2,CT.logBuffer);

        for (int r=0; r<RegData.Count; r++)
        {
            if (Segment->HasRegion(r))
            {
                sprintf(CT.logBuffer," %lu",static_cast<unsigned long>(r));
                G -> LogAppend(LH,CT.logBuffer);
            }
        }

        G -> LogEnd(LH);
        LH = G->LogStart(LOG_METH2,"Contact nodes:");

        for (TNode CN = 0; CN<Segment->GetContactNodeCount(); CN++)
        {
            sprintf(CT.logBuffer," %lu",
                static_cast<unsigned long>(Segment->GetContactNode(CN)));
            G -> LogAppend(LH,CT.logBuffer);
        }

        G -> LogEnd(LH);
    }

    #endif

    G -> CloseFold();
}
