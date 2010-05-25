
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractMixedGraph.cpp
/// \brief  #abstractMixedGraph partial class implementation

#include "exportToXFig.h"
#include "exportToTk.h"
#include "exportToDot.h"
#include "mixedGraph.h"
#include "disjointFamily.h"
#include "basicHeap.h"
#include "fibonacciHeap.h"
#include "binaryHeap.h"
#include "denseRepresentation.h"
#include "sparseBigraph.h"


// ------------------------------------------------------


abstractMixedGraph::abstractMixedGraph(TNode nn,TArc mm) throw() :
    registers(listOfRegisters,TokRegEndSection,attributePool::ATTR_FULL_RANK)
{
    n = nn;
    ni = 0;
    m = mm;
    CheckLimits();

    itCounter = 0;
    LH = RH = NoHandle;
    pInvestigator = NULL;
    partition = NULL;
    nHeap = NULL;
    adj = NULL;

    sDeg = NULL;
    sDegIn = NULL;
    sDegOut = NULL;

    face = NULL;

    defaultSourceNode = NoNode;
    defaultTargetNode = NoNode;
    defaultRootNode = NoNode;

    LogEntry(LOG_MEM,"...Abstract mixed graph allocated");
}


abstractMixedGraph::~abstractMixedGraph() throw()
{
    ReleaseInvestigators();
    ReleasePredecessors();
    ReleaseLabels();
    ReleasePartition();
    ReleasePotentials();
    ReleaseNodeColours();
    ReleaseEdgeColours();
    ReleaseDegrees();
    ReleaseAdjacencies();
    ReleaseEmbedding();
    ReleaseNodeMapping();
    ReleaseArcMapping();

    delete[] pInvestigator;

    LogEntry(LOG_MEM,"...Abstract mixed graph disallocated");
}


void abstractMixedGraph::CheckLimits() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (n>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of nodes is out of range: %lu",
            static_cast<unsigned long>(n));
        Error(ERR_REJECTED,"CheckLimits",CT.logBuffer);
    }

    if (2*m>CT.MaxArc()-2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"CheckLimits",CT.logBuffer);
    }

    #endif
}


unsigned long abstractMixedGraph::Allocated() const throw()
{
    unsigned long tmpSize = 0;

    tmpSize += registers.Size();

    if (pInvestigator!=NULL)    tmpSize += itCounter*sizeof(investigator**);
    if (sDeg!=NULL)             tmpSize += n*sizeof(TFloat);
    if (sDegIn!=NULL)           tmpSize += n*sizeof(TFloat);
    if (sDegOut!=NULL)          tmpSize += n*sizeof(TFloat);
    if (face!=NULL)             tmpSize += 2*m*sizeof(TNode);

    return tmpSize;
}


goblinQueue<TNode,TFloat> * abstractMixedGraph::NewNodeHeap() throw(ERRejected)
{
    switch (CT.methPQ)
    {
        case 0:
        {
            return new basicHeap<TNode,TFloat>(n,CT);
        }
        case 1:
        {
            return new binaryHeap<TNode,TFloat>(n,CT);
        }
        case 2:
        {
            return new fibonacciHeap<TNode,TFloat>(n,CT);
        }
    }

    UnknownOption("NewNodeHeap",CT.methPQ);

    return NULL;
}


goblinQueue<TArc,TFloat> * abstractMixedGraph::NewArcHeap() throw(ERRejected)
{
    switch (CT.methPQ)
    {
        case 0:
        {
            return new basicHeap<TArc,TFloat>(m,CT);
        }
        case 1:
        {
            return new binaryHeap<TArc,TFloat>(m,CT);
        }
        case 2:
        {
            return new fibonacciHeap<TArc,TFloat>(m,CT);
        }
    }

    UnknownOption("NewArcHeap",CT.methPQ);

    return NULL;
}


bool abstractMixedGraph::IsGraphObject() const throw()
{
    return true;
}


bool abstractMixedGraph::IsBipartite() const throw()
{
    return false;
}


bool abstractMixedGraph::IsDirected() const throw()
{
    return false;
}


bool abstractMixedGraph::IsUndirected() const throw()
{
    return false;
}


bool abstractMixedGraph::IsSparse() const throw()
{
    return false;
}


bool abstractMixedGraph::IsDense() const throw()
{
    return false;
}


bool abstractMixedGraph::IsBalanced() const throw()
{
    return false;
}


TNode abstractMixedGraph::DefaultSourceNode() const throw()
{
    return defaultSourceNode;
}


TNode abstractMixedGraph::DefaultTargetNode() const throw()
{
    return defaultTargetNode;
}


TNode abstractMixedGraph::DefaultRootNode() const throw()
{
    return defaultRootNode;
}


TArc abstractMixedGraph::InsertArc(TNode u,TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("InsertArc",u);

    if (v>=n) NoSuchNode("InsertArc",v);

    #endif

    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("InsertArc");

    #endif

    TFloat thisLength = RepresentationalData()->DefaultValue<TFloat>(TokReprLength,1);
    if (CT.randLength) thisLength = TFloat(CT.SignedRand());

    TCap thisUCap = RepresentationalData()->DefaultValue<TCap>(TokReprUCap,1);
    if (CT.randUCap) thisUCap = TCap(1+CT.UnsignedRand());

    TCap thisLCap = RepresentationalData()->DefaultValue<TCap>(TokReprLCap,0);
    if (CT.randLCap) thisLCap = TCap(CT.Rand(1+int(thisUCap)));


    if (IsSparse())
    {
        if (IsBalanced())
        {
            m++;
            static_cast<sparseRepresentation*>(X)
                ->InsertArc(v^1,u^1,thisUCap,thisLength,thisLCap);
        }

        m++;
        return static_cast<sparseRepresentation*>(X)
                ->InsertArc(u,v,thisUCap,thisLength,thisLCap);
    }

    #if defined(_FAILSAVE_)

    TArc a = Adjacency(u,v);

    if (a==NoArc)
    {
        // This happens if the graph is bipartite,
        // and u and v are in the same partition
        sprintf(CT.logBuffer,"Nodes %lu and %lu are non-adjacent",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"InsertArc",CT.logBuffer);
    }

    if (a&1)
    {
        // This happens if the graph is undirected and v>u
        Error(MSG_WARN,"InsertArc","End nodes are flipped");
    }

    #endif

    if (UCap(a)>0)
    {
        // Do not change the length of already existing arcs
        thisLength = Length(a);
    }

    return static_cast<denseRepresentation*>(X)
            ->InsertArc(a>>1,thisUCap,thisLength,thisLCap);
}


TArc abstractMixedGraph::InsertArc(TNode u,TNode v,TCap cc,TFloat ll,TCap bb)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("InsertArc",u);

    if (v>=n) NoSuchNode("InsertArc",v);

    #endif

    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("InsertArc");

    #endif

    if (IsSparse())
    {
        if (IsBalanced())
        {
            m++;
            static_cast<sparseRepresentation*>(X) -> InsertArc(v^1,u^1,cc,ll,bb);
        }

        m++;
        return static_cast<sparseRepresentation*>(X) -> InsertArc(u,v,cc,ll,bb);
    }

    TArc a = Adjacency(u,v);

    #if defined(_FAILSAVE_)

    if (a==NoArc)
    {
        // This happens if the graph is bipartite,
        // and u and v are in the same partition
        sprintf(CT.logBuffer,"Nodes %lu and %lu are non-adjacent",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"InsertArc",CT.logBuffer);
    }

    if (a&1)
    {
        // This happens if the graph is undirected and v>u
        Error(MSG_WARN,"InsertArc","End nodes are flipped");
    }

    #endif

    return static_cast<denseRepresentation*>(X) -> InsertArc(a>>1,cc,ll,bb);
}


TNode abstractMixedGraph::InsertNode() throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X || IsDense()) NoSparseRepresentation("InsertNode");

    #endif

    n++;
    TNode ret = static_cast<sparseRepresentation*>(X)->InsertNode();

    ReleaseAdjacencies();
    ReleasePartition();

    if (sDegIn)
    {
        sDegIn = (TFloat*)GoblinRealloc(sDegIn,n*sizeof(TFloat));
        sDegIn[n-1] = 0;
    }

    if (sDegOut)
    {
        sDegOut = (TFloat*)GoblinRealloc(sDegOut,n*sizeof(TFloat));
        sDegOut[n-1] = 0;
    }

    if (sDeg)
    {
        sDeg = (TFloat*)GoblinRealloc(sDeg,n*sizeof(TFloat));
        sDeg[n-1] = 0;
    }

    return ret;
}


void abstractMixedGraph::DeleteNode(TNode v) throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X || IsDense()) NoSparseRepresentation("DeleteNode");

    #endif

    sparseRepresentation* Y = static_cast<sparseRepresentation*>(X);

    if (IsBalanced() && v<n)
    {
        if (v&1)
        {
            Y -> DeleteNode(v);
            Y -> DeleteNode(v^1);
        }
        else
        {
            Y -> DeleteNode(v^1);
            Y -> DeleteNode(v);
        }

        return;
    }

    if (IsBipartite())
    {
        sparseBiGraph* G = static_cast<sparseBiGraph*>(this);

        if (v<G->N1())
        {
            // Left hand noded can be deleted only after moving them to the
            // right-hand side. And this is possible only after deleting all
            // incident arcs.
            Y -> CancelNode(v);
            G -> SwapNode(v);
        }
    }

    Y -> DeleteNode(v);
}


void abstractMixedGraph::RandomArcs(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc())
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"RandomArcs",CT.logBuffer);
    }

    #endif

    if (CT.logMan && _m>0)
    {
        sprintf(CT.logBuffer,"Generating %lu edges...",
            static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    for (TArc i=0;i<_m;)
    {
        TNode u = (TNode)CT.Rand(n);
        TNode v = (TNode)CT.Rand(n);

        TArc a = NoArc;

        if (!CT.randParallels && m>0) a = Adjacency(u,v,ADJ_SEARCH);

        if ((a==NoArc || UCap(a)==0) && (CT.randParallels || u!=v))
        {
            ReleaseInvestigators();
            InsertArc(u,v);
            i++;
        }
    }
}


void abstractMixedGraph::RandEulerian(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc() || _m<2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"RandEulerian",CT.logBuffer);
    }

    #endif

    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"Generating eulerian cycle of length %lu...",
            static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    TNode start = (TNode)CT.Rand(n);
    TNode u = start;
    TNode v = start;

    for (TArc i=0;i<_m-1;++i)
    {
        for (char timeOut=100;timeOut>0;timeOut--)
        {
            while (u==v) v = TNode(CT.Rand(n));

            if (!CT.randParallels && m>0)
            {
                TArc a = Adjacency(u,v,ADJ_SEARCH);
                if (a==NoArc || UCap(a)==0) timeOut = 0;
            }
            else timeOut = 0;
        }

        ReleaseInvestigators();
        InsertArc(u,v);
        u = v;
    }

    InsertArc(v,start);
}


void abstractMixedGraph::RandRegular(TArc k) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (n&k&1)
    {
        sprintf(CT.logBuffer,"Parity mismatch: n=%lu, k=%lu",
            static_cast<unsigned long>(n),static_cast<unsigned long>(k));
        Error(ERR_REJECTED,"RandRegular",CT.logBuffer);
    }

    #endif

    TNode minDeg = 0;
    TNode nMin = n;
    TNode nDef = n;

    TArc* dg = new TArc[n];

    for (TNode v=0;v<n;++v) dg[v]=0;

    while (minDeg<k)
    {
        while (nMin>0)
        {
            TNode shift = 1+CT.Rand(nMin);
            TNode v = n;

            while (shift>0)
            {
                v--;
                if (dg[v]==minDeg) shift--;
            }

            shift = 1+CT.Rand(nDef-1);
            TNode w = n;

            while (shift>0)
            {
                w--;
                if (dg[w]<k && v!=w) shift--;
            }

            nMin--;

            if (dg[w]==minDeg) nMin--;

            InsertArc(v,w);
            dg[v]++;
            dg[w]++;

            if (dg[v]==k) nDef--;
            if (dg[w]==k) nDef--;

        }

        minDeg++;

        for (TNode v=0;v<n;++v)
        {
            if (dg[v]==minDeg) nMin++;
        }
    }

    delete[] dg;

    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"...Random %lu-regular graph generated",
            static_cast<unsigned long>(k));
        LogEntry(LOG_MAN,CT.logBuffer);
    }
}


void abstractMixedGraph::AddGraphByNodes(abstractMixedGraph& G,TMergeLayoutMode mergeLayoutMode)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || G.IsDense())
        Error(ERR_REJECTED,"AddGraphByNodes","Cannot merge dense graphs");

    if (!Representation()) NoRepresentation("AddGraphByNodes");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode nOrig = n;

    TFloat xMin = 0.0,xMax = 0.0,yMin = 0.0,yMax = 0.0;

    Layout_GetBoundingInterval(0,xMin,xMax);
    Layout_GetBoundingInterval(1,yMin,yMax);

    TFloat xMinG = 0.0,xMaxG = 0.0,yMinG = 0.0,yMaxG = 0.0;

    G.Layout_GetBoundingInterval(0,xMinG,xMaxG);
    G.Layout_GetBoundingInterval(1,yMinG,yMaxG);

    TFloat xShift = (mergeLayoutMode==MERGE_ALIGN_RIGHT) ? xMaxG-xMin : 0;
    TFloat yShift = (mergeLayoutMode==MERGE_ALIGN_BELOW) ? yMaxG-yMin : 0;


    // Add the nodes of G, position as required by mergeLayoutMode
    for (TNode v=0;v<G.N();++v)
    {
        TNode w = InsertNode();
        X -> SetC(w,0,G.C(v,0)+xShift);
        X -> SetC(w,1,G.C(v,1)+yShift);
    }

    // Add the arcs of G
    for (TArc a=0;a<G.M();a++)
    {
        TNode u = nOrig+G.StartNode(2*a);
        TNode v = nOrig+G.EndNode(2*a);

        TArc a2 = InsertArc(u,v,G.UCap(2*a),G.Length(2*a),G.LCap(2*a));

        if (!IsDirected() && !IsUndirected())
            X -> SetOrientation(2*a2,G.Orientation(2*a));
    }


    // Assign an explicit bounding box
    if (mergeLayoutMode==MERGE_ALIGN_RIGHT)
    {
        X -> Layout_SetBoundingInterval(0,xMin,xMax+xMaxG-xMinG);
    }
    else
    {
        X -> Layout_SetBoundingInterval(0,(xMin<xMinG) ? xMin : xMinG,(xMax<xMaxG) ? xMaxG : xMax);
    }

    if (mergeLayoutMode==MERGE_ALIGN_BELOW)
    {
        X -> Layout_SetBoundingInterval(1,yMin,yMax+yMaxG-yMinG);
    }
    else
    {
        X -> Layout_SetBoundingInterval(1,(yMin<yMinG) ? yMin : yMinG,(yMax<yMaxG) ? yMaxG : yMax);
    }


    // Assign node colours such that the added graph can be distinguished
    TNode* colour = GetNodeColours();
    TNode newColour = 1;

    if (colour)
    {
        for (TNode v=0;v<nOrig;++v)
        {
            if (colour[v]>=newColour) newColour = colour[v]+1;
        }
    }
    else
    {
        colour = InitNodeColours(0);
    }

    for (TNode v=nOrig;v<N();++v) colour[v] = newColour;


    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"...Graph \"%s\" merged",G.Label());
        LogEntry(LOG_MAN,CT.logBuffer);
    }
}


static void vectorProduct(const vector<TFloat>& x,const vector<TFloat>& y, vector<TFloat>& out)
{
    out[0] = x[1]*y[2]-x[2]*y[1];
    out[1] = x[2]*y[0]-x[0]*y[2];
    out[2] = x[0]*y[1]-x[1]*y[0];
}


static TFloat normalizeVector(vector<TFloat>& vec)
{
    TFloat norm = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);

    if (fabs(norm)<10e-12) return 0.0;

    vec[0] /= norm;
    vec[1] /= norm;
    vec[2] /= norm;

    return norm;
}


static TFloat solveLEQ3(const vector<TFloat>& rh,const vector<TFloat>& col0,
    const vector<TFloat>& col1, const vector<TFloat>& col2, vector<TFloat>& out)
{
    TFloat det = col0[0]*col1[1]*col2[2] + col0[1]*col1[2]*col2[0] + col0[2]*col1[0]*col2[1]
               - col0[0]*col1[2]*col2[1] - col0[1]*col1[0]*col2[2] - col0[2]*col1[1]*col2[0];

    if (fabs(det)<10e-12) return 0.0;

    vector<TFloat> inv(3);
    inv[0] = col1[1]*col2[2]-col1[2]*col2[1];
    inv[1] = col1[2]*col2[0]-col1[0]*col2[2];
    inv[2] = col1[0]*col2[1]-col1[1]*col2[0];
    out[0] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    inv[0] = col0[2]*col2[1]-col0[1]*col2[2];
    inv[1] = col0[0]*col2[2]-col0[2]*col2[0];
    inv[2] = col0[1]*col2[0]-col0[0]*col2[1];
    out[1] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    inv[0] = col0[1]*col1[2]-col0[2]*col1[1];
    inv[1] = col0[2]*col1[0]-col0[0]*col1[2];
    inv[2] = col0[0]*col1[1]-col0[1]*col1[0];
    out[2] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    return det;
}


void abstractMixedGraph::FacetIdentification(abstractMixedGraph& G)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || G.IsDense())
        Error(ERR_REJECTED,"FacetIdentification","Cannot merge dense graphs");

    if (!Representation()) NoRepresentation("FacetIdentification");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode nG = G.N();
    TArc  mG = G.M();

    TArc aExtG = G.ExteriorArc();

    #if defined(_FAILSAVE_)

    if (aExtG>=2*mG)
        Error(ERR_REJECTED,"FacetIdentification","Missing exterior arc");

    #endif

    // Determine the length of the exterior cycle of G and its center point
    TArc aG = G.Left(aExtG);
    TNode vG = G.StartNode(aG);
    TArc exteriorLength = 1;
    vector<TFloat> centerG(3);
    vector<TFloat> perpendicularG(3);
    TDim i = 0;
    for (;i<3;++i) centerG[i] = G.C(vG,i);

    while (aG!=(aExtG^1))
    {
        aG = G.Left(aG^1);
        vG = G.StartNode(aG);
        ++exteriorLength;
        for (i=0;i<3;++i) centerG[i] += G.C(vG,i);

        if (exteriorLength>nG)
            Error(ERR_REJECTED,"FacetIdentification","No exterior face found");
    }

    vector<TFloat> unitXG(3);
    vector<TFloat> unitYG(3);
    vector<TFloat> unitZG(3);

    // Now, vG is the EndNode(aExtG)
    TNode wG = G.StartNode(aExtG);

    for (i=0;i<3;++i)
    {
        centerG[i] /= exteriorLength;
        unitXG[i] = G.C(vG,i)-centerG[i];
        unitYG[i] = G.C(wG,i)-G.C(vG,i);
    }

    TFloat lengthG = normalizeVector(unitXG);
    vectorProduct(unitXG,unitYG,unitZG);
    normalizeVector(unitZG);
    vectorProduct(unitZG,unitXG,unitYG);
    normalizeVector(unitYG);


    TNode n0 = N();
    TArc m0 = M();
    TDim dim = (Dim()<3) ? Dim() : 3;


    // First pass: Determine the number of facets to be split
    bool *visited = new bool[2*m0];
    TNode nCopies = 0;
    TArc a = 0;

    for (;a<2*m;++a) visited[a] = false;

    for (TArc a0=0;a0<2*m0;++a0)
    {
        if (visited[a0]) continue;

        // Determine the length of the cycle enclosing the facet of a0
        a = Right(a0^1,EndNode(a0));
        TArc facetLength = 1;
        visited[a0] = true;

        while (a!=a0)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
            ++facetLength;
        }

        if (facetLength!=exteriorLength) continue;

        ++nCopies;
    }

    // Forecast the final dimensions, for sake of performance
    TArc nFinal = TArc(n0)+(nG-exteriorLength)*nCopies;
    TArc mFinal = m0+(mG-exteriorLength)*nCopies;
    X -> SetCapacity(nFinal,mFinal);

    // To obtain the node incidence ordering, initialize with the original ordering
    TArc *right = new TArc[2*mFinal];
    for (a=0;a<2*m0;++a) right[a] = Right(a,StartNode(a));
    for (a=2*m0;a<2*mFinal;++a) right[a] = NoArc;

    // Second pass: Insert G into the appropriate faces
    for (a=0;a<2*m;++a) visited[a] = false;

    // Do not subdivide the exterior face. If no exterior face is defined,
    // this indicates a 3D embedding (a solid representation)
    if (ExteriorArc()!=NoArc)
    {
        // Mark the exterior arcs as visited
        TArc aExt = ExteriorArc();
        visited[aExt] = true;
        a = Right(aExt^1,EndNode(aExt));

        while (a!=aExt)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
        }
    }

    TNode* mapNode = new TNode[nG];
    TArc* mapArc = new TArc[2*mG];

    for (TArc a0=0;a0<2*m0;++a0)
    {
        if (visited[a0]) continue;

        // Determine the length of the cycle enclosing the facet of a0
        a = Right(a0^1,EndNode(a0));
        TArc facetLength = 1;
        visited[a0] = true;

        while (a!=a0)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
            ++facetLength;
        }

        if (facetLength!=exteriorLength) continue;

        // So, G has to to be inserted here
        TNode vG = 0;

        for (;vG<nG;++vG) mapNode[vG] = NoNode;
        for (a=0;a<2*mG;++a) mapArc[a] = NoArc;

        // First, map the contact nodes and arcs, and determine the center point

        TNode v = EndNode(a0);
        a = Right(a0^1,v);
        aG = G.Left(aExtG);
        mapNode[G.StartNode(aG)] = v;
        mapArc[aG] = a;
        mapArc[aG^1] = a^1;
        vector<TFloat> center(3);

        for (i=0;i<3;++i) center[i] = C(v,i);

        while (a!=a0)
        {
            v = EndNode(a);
            a = Right(a^1,v);
            aG = G.Left(aG^1);
            mapNode[G.StartNode(aG)] = v;
            mapArc[aG] = a;
            mapArc[aG^1] = a^1;

            for (i=0;i<3;++i) center[i] += C(v,i);
        }

        vector<TFloat> unitX(3);
        vector<TFloat> unitY(3);
        vector<TFloat> unitZ(3);

        // v is still the StartNode(a0)
        TNode w = EndNode(a0);

        for (i=0;i<3;++i)
        {
            center[i] /= facetLength;
            unitX[i] = C(v,i)-center[i];
            unitY[i] = C(w,i)-C(v,i);
        }

        TFloat length = normalizeVector(unitX);
        vectorProduct(unitX,unitY,unitZ);
        normalizeVector(unitZ);
        vectorProduct(unitZ,unitX,unitY);
        normalizeVector(unitY);

        // Map the remaining nodes of G
        for (vG=0;vG<nG;++vG)
        {
            if (mapNode[vG]!=NoNode) continue;

            v = mapNode[vG] = InsertNode();

            if (dim>1 && G.Dim()>1)
            {
                vector<TFloat> in(3);
                vector<TFloat> out(3);

                for (i=0;i<dim;++i) in[i] = G.C(vG,i)-centerG[i];

                solveLEQ3(in,unitXG,unitYG,unitZG,out);

                for (i=0;i<dim;++i)
                {
                    SetC(v,i,center[i]+length/lengthG*(unitX[i]*out[0]+unitY[i]*out[1]-unitZ[i]*out[2]));
                }
            }
        }

        // Map the edges of G, but omit the arcs on the exterior face
        MarkExteriorFace(a0);

        for (aG=0;aG<mG;++aG)
        {
            if (mapArc[2*aG]<2*m0) continue;

            TNode uG = G.StartNode(2*aG);
            vG = G.EndNode(2*aG);
            TArc aMap = InsertArc(mapNode[uG],mapNode[vG],G.UCap(2*aG),G.Length(2*aG),G.LCap(2*aG));
            mapArc[2*aG] = 2*aMap;
            mapArc[2*aG+1] = 2*aMap+1;
        }

        // Set the right-hand node indences, but only for the interior arcs of G
        for (aG=0;aG<mG;++aG)
        {
            TArc aMap = mapArc[2*aG];

            if (aMap<2*m0) continue;

            right[aMap] = mapArc[G.Right(2*aG,G.StartNode(2*aG))];
            right[aMap^1] = mapArc[G.Right(2*aG+1,G.StartNode(2*aG+1))];
        }

        // Set the right-hand node indences for the exterior arcs
        aG = G.Left(aExtG);
        right[mapArc[aG^1]] = mapArc[G.Right(aG^1,G.StartNode(aG^1))];

        while (aG!=(aExtG^1))
        {
            aG = G.Left(aG^1);
            right[mapArc[aG^1]] = mapArc[G.Right(aG^1,G.StartNode(aG^1))];
        }
    }

    X -> ReorderIncidences(right,true);

    delete[] right;
    delete[] visited;
    delete[] mapNode;
    delete[] mapArc;
}


size_t abstractMixedGraph::SizeInfo(TArrayDim arrayDim,TSizeInfo) const throw()
{
    switch (arrayDim)
    {
        case DIM_GRAPH_NODES:
        {
            return size_t(n);
        }
        case DIM_GRAPH_ARCS:
        {
            return size_t(m);
        }
        case DIM_ARCS_TWICE:
        {
            return size_t(2*m);
        }
        case DIM_LAYOUT_NODES:
        {
            return size_t(n+ni);
        }
        case DIM_SINGLETON:
        {
            return size_t(1);
        }
        default:
        {
            return size_t(0);
        }
    }
}


graphRepresentation* abstractMixedGraph::Representation() throw()
{
    return NULL;
}


const graphRepresentation* abstractMixedGraph::Representation() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::RepresentationalData() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::Geometry() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::LayoutData() const throw()
{
    return NULL;
}


abstractMixedGraph::TMetricType abstractMixedGraph::MetricType() const throw()
{
    attributePool* geometry = Geometry();

    if (!geometry) return METRIC_DISABLED;

    return abstractMixedGraph::TMetricType(geometry->GetValue<int>(TokGeoMetric,0,int(METRIC_DISABLED)));
}


void abstractMixedGraph::SetSourceNode(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("SetSourceNode",s);

    #endif

    defaultSourceNode = s;
}


void abstractMixedGraph::SetTargetNode(TNode t) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (t>=n && t!=NoNode) NoSuchNode("SetTargetNode",t);

    #endif

    defaultTargetNode = t;
}


void abstractMixedGraph::SetRootNode(TNode r) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (r>=n && r!=NoNode) NoSuchNode("SetRootNode",r);

    #endif

    defaultRootNode = r;
}


TNode abstractMixedGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return StartNode(a^1);
}


TArc abstractMixedGraph::Left(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Left",a);

    #endif

    TNode v = StartNode(a);
    TArc a2 = a;

    while (Right(a2,v)!=a) a2 = Right(a2,v);

    return a2;
}


TCap abstractMixedGraph::Demand(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprDemand,v,X->defaultDemand);
}


TCap abstractMixedGraph::MaxDemand() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().MaxValue<TCap>(TokReprDemand,X->defaultDemand);
}


bool abstractMixedGraph::CDemand() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprDemand);
}


TCap abstractMixedGraph::UCap(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprUCap,a>>1,X->defaultUCap);
}


TCap abstractMixedGraph::MaxUCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TCap max = 0;

        for (TArc a=0;a<m;a++)
            if (LCap(2*a)>max) max = UCap(2*a);

        return max;
    }

    return X->RepresentationalData().MaxValue<TCap>(TokReprUCap,X->defaultUCap);
}


bool abstractMixedGraph::CUCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprUCap);
}


TCap abstractMixedGraph::LCap(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprLCap,a>>1,X->defaultLCap);
}


TCap abstractMixedGraph::MaxLCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TCap max = 0;

        for (TArc a=0;a<m;a++)
            if (LCap(2*a)>max) max = LCap(2*a);

        return max;
    }

    return X->RepresentationalData().MaxValue<TCap>(TokReprLCap,X->defaultLCap);
}


bool abstractMixedGraph::CLCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprLCap);
}


TFloat abstractMixedGraph::Length(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 1;

    return X->Length(a);
}


TFloat abstractMixedGraph::MaxLength() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TFloat max = 0;

        for (TArc a=0;a<m;a++)
        {
            TFloat thisLength = Length(2*a);

            if (thisLength>max) max = thisLength;
            if (-thisLength>max) max = -thisLength;
        }

        return max;
    }

    return X->MaxLength();
}


bool abstractMixedGraph::CLength() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TFloat>(TokReprLength);
}


char abstractMixedGraph::Orientation(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        return IsDirected() ? 1 : 0;
    }

    return X->RepresentationalData().GetValue<char>(TokReprOrientation,a>>1,X->defaultOrientation);
}


char abstractMixedGraph::Orientation() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        return IsDirected() ? 1 : 0;
    }

    return X->RepresentationalData().DefaultValue<char>(TokReprOrientation,X->defaultOrientation);
}


bool abstractMixedGraph::COrientation() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<char>(TokReprOrientation);
}


TFloat abstractMixedGraph::C(TNode v,TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().GetValue<TFloat>(TokGeoAxis0+i,v,X->defaultC);
}


void abstractMixedGraph::SetC(TNode v,TDim i,TFloat pos) throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("SetC");

    #endif

    X -> SetC(v,i,pos);
}


TFloat abstractMixedGraph::CMin(TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().MinValue<TFloat>(TokGeoAxis0+i,X->defaultC);
}


TFloat abstractMixedGraph::CMax(TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().MaxValue<TFloat>(TokGeoAxis0+i,X->defaultC);
}


TDim abstractMixedGraph::Dim() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Dim();
}


bool abstractMixedGraph::Blocking(TArc a)
    const throw(ERRange)
{
    if (Orientation(a)==0) return false;

    if (a&1) return true;

    return false;
}


TFloat abstractMixedGraph::RedLength(const TFloat* potential,TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("RedLength",a);

    #endif

    if (Orientation(a)==0)
    {
        if (!potential) return Length(a);

        return Length(a)+potential[StartNode(a)]+potential[EndNode(a)];
    }

    TFloat thisLength = ((a&1) ? -Length(a) : Length(a));

    if (!potential) return thisLength;

    return thisLength+potential[StartNode(a)]-potential[EndNode(a)];
}


TNode abstractMixedGraph::ArcLabelAnchor(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return NoNode;

    return static_cast<const sparseRepresentation*>(X) -> ArcLabelAnchor(a);
}


bool abstractMixedGraph::NoArcLabelAnchors() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return true;

    return static_cast<const sparseRepresentation*>(X) -> NoArcLabelAnchors();
}


TNode abstractMixedGraph::ThreadSuccessor(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return NoNode;

    return static_cast<const sparseRepresentation*>(X) -> ThreadSuccessor(v);
}


bool abstractMixedGraph::NoThreadSuccessors() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return true;

    return static_cast<const sparseRepresentation*>(X) -> NoThreadSuccessors();
}


bool abstractMixedGraph::HiddenNode(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (fabs(C(v,0))>=InfFloat) return true;

    return (X && X->HiddenNode(v));
}


bool abstractMixedGraph::HiddenArc(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    return (X && X->HiddenArc(a));
}


void abstractMixedGraph::SetNodeVisibility(TNode v,bool visible) throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || visible) return;

    SetC(v,TDim(0),-InfFloat);
}


void abstractMixedGraph::SetArcVisibility(TArc a,bool visible) throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return;
}


TArc abstractMixedGraph::Adjacency(TNode u,TNode v,TMethAdjacency method) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = NoArc;

    if (adj==NULL)
    {
        if (method==ADJ_MATRIX)
        {
            if (m>0)
            {
                adj = new goblinHashTable<TArc,TArc>(n*n,2*m,NoArc,CT);

                for (TArc a1=0;a1<2*m;a1++)
                {
                    TNode u0 = StartNode(a1);
                    TNode v0 = EndNode(a1);
                    TArc a0 = u0*n+v0;
                    TArc a2 = adj->Key(a0);

                    // Check if a1 is preferable before a2

                    if (a2==NoArc || (Blocking(a2) && !Blocking(a1)))
                    {
                        adj -> ChangeKey(a0,a1);
                        continue;
                    }

                    if (Blocking(a1) && !Blocking(a2)) continue;

                    if (a1<a2) adj -> ChangeKey(a0,a1);
                }

                a = adj->Key(u*n+v);
            }
        }
        else // (method==ADJ_SEARCH)
        {
            TArc a0 = First(u);

            if (a0!=NoArc && EndNode(a0)==v) a = a0;

            while (a0!=NoArc && Right(a0,u)!=First(u))
            {
                a0 = Right(a0,u);

                if (EndNode(a0)!=v) continue;

                if (a==NoArc || (Blocking(a) && !Blocking(a0)))
                {
                    a = a0;
                    continue;
                }

                if (Blocking(a0) && !Blocking(a)) continue;

                if (a0<a) a = a0;
            }

        }
    }
    else a = adj->Key(u*n+v);

    if (a==NoArc)
    {
        #if defined(_LOGGING_)

        if (CT.logWarn>1)
        {
            sprintf(CT.logBuffer,"Nodes are non-adjacent: %lu, %lu",
                static_cast<unsigned long>(u),static_cast<unsigned long>(v));
            Error(MSG_WARN,"Adjacency",CT.logBuffer);
        }

        #endif

        return NoArc;
    }
    else
    {
        #if defined(_LOGGING_)

        if (CT.logRes>2)
        {
            sprintf(CT.logBuffer,
                "The nodes %lu and %lu are adjacent by the arc %lu",
                static_cast<unsigned long>(u),
                static_cast<unsigned long>(v),
                static_cast<unsigned long>(a));
            LogEntry(LOG_RES2,CT.logBuffer);
        }

        #endif

        return a;
    }
}


void abstractMixedGraph::MarkAdjacency(TNode u,TNode v,TArc a)
    throw(ERRange,ERRejected)
{
    if (adj==NULL) return;

    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("MarkAdjacency",u);

    if (v>=n) NoSuchNode("MarkAdjacency",v);

    if (a>=2*m && a!=NoArc) NoSuchArc("MarkAdjacency",a);

    if (a!=NoArc && (StartNode(a)!=u || EndNode(a)!=v || Blocking(a)))
        Error(ERR_REJECTED,"MarkAdjacency","Mismatching end nodes");

    #endif

    TArc a0 = u*n+v;
    adj -> ChangeKey(a0,a);
}


TCap abstractMixedGraph::Cardinality() const throw()
{
    TCap sum = 0;

    for (TArc a=0;a<m;a++) sum += fabs(Sub(2*a));

    return sum;
}


TFloat abstractMixedGraph::Weight() const throw()
{
    TFloat sum = 0;

    for (TArc a=0;a<m;a++) sum += fabs(Sub(2*a))*Length(2*a);

    return sum;
}


TFloat abstractMixedGraph::Length() const throw()
{
    TArc* pred = GetPredecessors();

    if (!pred) return 0;

    TFloat sum = 0;

    for (TNode v=0;v<n;++v)
        if (pred[v]!=NoArc) sum += Length(pred[v]);

    return sum;
}


investigator *abstractMixedGraph::NewInvestigator() const throw()
{
    return new iGraph(*this);
}


investigator &abstractMixedGraph::Investigator(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Investigator",H);

    #endif

    return *pInvestigator[H];
}


THandle abstractMixedGraph::Investigate() const throw(ERRejected)
{
    if (LH!=NoHandle)
    {
        THandle tmpHandle = LH;
        LH = NoHandle;
        pInvestigator[tmpHandle] -> Reset();
        return tmpHandle;
    }

    if (RH!=NoHandle)
    {
        THandle tmpHandle = RH;
        RH = NoHandle;
        pInvestigator[tmpHandle] -> Reset();
        return tmpHandle;
    }

    for (THandle i=0;i<itCounter;++i)
    {
        if (pInvestigator[i]==NULL)
        {
            pInvestigator[i] = NewInvestigator();
            return i;
        }
    }

    if (itCounter==NoHandle)
        Error(ERR_REJECTED,"Investigate","No more handles available");

    pInvestigator = (investigator**)GoblinRealloc(pInvestigator,sizeof(investigator*)*(itCounter+1));
    pInvestigator[itCounter] = NewInvestigator();
    return itCounter++;
}


void abstractMixedGraph::Reset(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Reset",H);

    #endif

    pInvestigator[H]->Reset();
}


void abstractMixedGraph::Reset(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Reset",H);

    #endif

    pInvestigator[H]->Reset(v);
}


TArc abstractMixedGraph::Read(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Read",H);

    #endif

    return pInvestigator[H]->Read(v);
}


TArc abstractMixedGraph::Peek(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Peek",H);

    #endif

    return pInvestigator[H]->Peek(v);
}


bool abstractMixedGraph::Active(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Active",H);

    #endif

    return pInvestigator[H]->Active(v);
}


void abstractMixedGraph::Close(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)&&(pInvestigator[H]==NULL)) NoSuchHandle("Close",H);

    if (H==LH || H==RH)
    {
        Error(ERR_REJECTED,"Close","Repeated close of investigator");
    }

    #endif

    if ((LH==NoHandle)||(RH==NoHandle))
    {
        if (LH==NoHandle) LH = H;
        else RH = H;
        LogEntry(LOG_MEM,"Investigator cached");
    }
    else
    {
        delete pInvestigator[H];
        pInvestigator[H] = NULL;
        LogEntry(LOG_MEM,"Investigator disallocated");
    }
}


void abstractMixedGraph::ReleaseInvestigators() const throw()
{
    if (LH!=NoHandle)
    {
        delete pInvestigator[LH];
        pInvestigator[LH] = NULL;
        LH = NoHandle;
    }

    if (RH!=NoHandle)
    {
        delete pInvestigator[RH];
        pInvestigator[RH] = NULL;
        RH = NoHandle;
    }

    for (THandle i=0; i<itCounter; i++)
        if (pInvestigator[i]!=NULL)
        {
            delete pInvestigator[i];
            pInvestigator[i] = NULL;
        }

    delete[] pInvestigator;
    pInvestigator = NULL;
    itCounter = 0;

    LogEntry(LOG_MEM,"Investigator cache cleared");
}


void abstractMixedGraph::StripInvestigators() const throw()
{
    THandle i = itCounter;
    for (;((i>0)&&(pInvestigator[i-1]==NULL));i--) {};

    pInvestigator = (investigator**)GoblinRealloc(pInvestigator,sizeof(investigator*)*i);
    itCounter = i;
}


void abstractMixedGraph::Bud(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Bud",v);

    #endif

    partition -> Bud(v);
}


void abstractMixedGraph::Merge(TNode u,TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Merge",u);

    if (v>=n) NoSuchNode("Merge",v);

    #endif

    partition -> Merge(u,v);
}


TNode abstractMixedGraph::Find(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Find",v);

    #endif

    if (partition==NULL) return v;
    else return partition -> Find(v);
}


TFloat abstractMixedGraph::Dist(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Dist",v);

    #endif

    TFloat* dist = GetDistanceLabels();

    if (!dist) return InfFloat;

    return dist[v];
}


TFloat abstractMixedGraph::Pi(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Pi",v);

    #endif

    TFloat* potential = GetPotentials();

    if (!potential) return 0;

    else return potential[v];
}


TNode abstractMixedGraph::NodeColour(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("NodeColour",v);

    #endif

    TNode* nodeColour = GetNodeColours();

    if (!nodeColour) return NoNode;

    return nodeColour[v];
}


TArc abstractMixedGraph::EdgeColour(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EdgeColour",a);

    #endif

    TArc* edgeColour = GetEdgeColours();

    if (!edgeColour) return NoArc;

    return edgeColour[a>>1];
}


TArc abstractMixedGraph::Pred(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Pred",v);

    #endif

    TArc* pred = GetPredecessors();

    if (!pred) return NoArc;

    return pred[v];
}


TArc abstractMixedGraph::ExteriorArc() const throw()
{
    if (!LayoutData()) return NoArc;

    return LayoutData() -> GetValue<TArc>(TokLayoutExteriorArc,0,NoArc);
}


void abstractMixedGraph::SetExteriorArc(TArc exteriorArc) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (exteriorArc>=2*m && exteriorArc!=NoArc) NoSuchArc("SetExteriorArc",exteriorArc);

    if (!LayoutData())
        Error(ERR_REJECTED,"SetExteriorArc","No layout data pool found");

    #endif

    if (exteriorArc==NoArc)
    {
        LayoutData() -> ReleaseAttribute(TokLayoutExteriorArc);
    }
    else
    {
        LayoutData() -> InitAttribute(*this,TokLayoutExteriorArc,exteriorArc);
    }
}


TNode abstractMixedGraph::Face(TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Face",a);

    #endif

    if (!face) ExtractEmbedding();

    if (!face) return NoNode;

    return face[a];
}


TIndex abstractMixedGraph::OriginalOfNode(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("OriginalOfNode",v);

    #endif

    TIndex* originalNode = registers.GetArray<TIndex>(TokRegOriginalNode);

    if (!originalNode) return NoIndex;

    return originalNode[v];
}


TIndex abstractMixedGraph::OriginalOfArc(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("OriginalOfArc",a);

    #endif

    TIndex* originalArc = registers.GetArray<TIndex>(TokRegOriginalArc);

    if (!originalArc) return NoIndex;

    return originalArc[a>>1]^(a&1);
}


bool abstractMixedGraph::ExteriorNode(TNode v,TNode thisFace)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("ExteriorNode",v);

    #endif

    TArc exteriorArc = ExteriorArc();

    if (!face || (exteriorArc==NoNode && thisFace==NoNode)) return false;

    if (thisFace!=NoNode) return (face[First(v)^1]==thisFace);

    return (face[First(v)^1]==face[exteriorArc]);
}


void abstractMixedGraph::SetSub(TArc a,TFloat multiplicity)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("SetSub",a);

    if (fabs(multiplicity)>UCap(a) || fabs(multiplicity)<LCap(a))
         AmountOutOfRange("SetSub",multiplicity);

    #endif

    SetSubRelative(a,multiplicity-Sub(a));
}


void abstractMixedGraph::SetSubRelative(TArc a,TFloat lambda) throw(ERRange)
{
    a ^= (a&1);

    if (lambda<0)
    {
        Push(a^1,-lambda);
    }
    else
    {
        Push(a,lambda);
    }
}


void abstractMixedGraph::Push(TArc a,TFloat lambda) throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"Push","Not implemented");
}


TFloat abstractMixedGraph::ResCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("ResCap",a);

    #endif

    if (a&1)
    {
        return Sub(a)-LCap(a);
    }
    else if (UCap(a)<InfCap)
    {
        return UCap(a)-Sub(a);
    }
    else if (Sub(a)<InfCap)
    {
        return InfCap-Sub(a);
    }
    else
    {
        return 0;
    }
}


void abstractMixedGraph::SetDist(TNode v,TFloat thisDist) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetDist",v);

    #endif

    TFloat* dist = GetDistanceLabels();

    if (!dist)
    {
        if (thisDist!=InfFloat)
        {
            dist = InitDistanceLabels();
            dist[v] = thisDist;
        }
    }
    else dist[v] = thisDist;
}


void abstractMixedGraph::SetPotential(TNode v,TFloat thisPi) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetPotential",v);

    #endif

    TFloat* potential = GetPotentials();

    if (!potential)
    {
        if (thisPi!=0)
        {
            potential = InitPotentials();
            potential[v] = thisPi;
        }
    }
    else potential[v] = thisPi;
}


void abstractMixedGraph::SetNodeColour(TNode v,TNode thisColour) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetNodeColour",v);

    if (thisColour>=n && thisColour!=NoNode)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %lu",
            static_cast<unsigned long>(thisColour));
        Error(MSG_WARN,"SetNodeColour",CT.logBuffer);
    }

    #endif

    TNode* nodeColour = GetNodeColours();

    if (!nodeColour)
    {
        if (thisColour!=NoNode)
        {
            nodeColour = InitNodeColours();
            nodeColour[v] = thisColour;
        }
    }
    else nodeColour[v] = thisColour;
}


void abstractMixedGraph::SetEdgeColour(TArc a,TArc thisColour) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("SetEdgeColour",a);

    if (thisColour>=m && thisColour!=NoArc)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %lu",
            static_cast<unsigned long>(thisColour));
        Error(MSG_WARN,"SetEdgeColour",CT.logBuffer);
    }

    #endif

    TArc* edgeColour = GetEdgeColours();

    if (!edgeColour)
    {
        if (thisColour!=NoArc)
        {
            edgeColour = InitEdgeColours();
            edgeColour[a>>1] = thisColour;
        }
    }
    else edgeColour[a>>1] = thisColour;
}


void abstractMixedGraph::SetPred(TNode v,TArc thisPred)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetPred",v);

    if (thisPred>=2*m && thisPred!=NoArc) NoSuchArc("SetPred",thisPred);

    if (thisPred!=NoArc && EndNode(thisPred)!=v)
        Error(ERR_REJECTED,"SetPred","Mismatching end node");

    #endif

    TArc* pred = GetPredecessors();

    if (!pred)
    {
        if (thisPred!=NoArc)
        {
            pred = InitPredecessors();
            pred[v] = thisPred;
        }
    }
    else pred[v] = thisPred;
}


void abstractMixedGraph::PushPotential(TNode v,TFloat epsilon) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("PushPotential",v);

    #endif

    TFloat* potential = RawPotentials();

    potential[v] += epsilon;
}


TFloat abstractMixedGraph::Deg(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Deg",v);

    #endif

    if (!sDeg) InitDegrees();

    return sDeg[v];
}


TFloat abstractMixedGraph::DegIn(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DegIn",v);

    #endif

    if (!sDegIn) InitDegInOut();

    return sDegIn[v];
}


TFloat abstractMixedGraph::DegOut(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DegOut",v);

    #endif

    if (!sDegOut) InitDegInOut();

    return sDegOut[v];
}


void abstractMixedGraph::AdjustDegrees(TArc a,TFloat lambda) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("AdjustDegrees",a);

    #endif

    TNode u = StartNode(a);
    TNode v = EndNode(a);

    if (sDeg && !Blocking(a) && !Blocking(a^1))
    {
        sDeg[u] += lambda;
        sDeg[v] += lambda;
    }
    else
    {
        if (sDegIn)
        {
            if (!Blocking(a^1)) sDegIn[u] += lambda;
            else sDegOut[u] += lambda;

            if (!Blocking(a)) sDegIn[v] += lambda;
            else sDegOut[v] += lambda;
        }
    }
}


void abstractMixedGraph::InitDegrees() throw()
{
    if (sDeg==NULL)
    {
        LogEntry(LOG_MEM,"Generating degree labels...");
        sDeg = new TFloat[n];

        THandle H = Investigate();
        investigator &I = Investigator(H);

        for (TNode u=0;u<n;u++)
        {
            TFloat thisDeg = 0;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                if (!Blocking(a) && !Blocking(a^1)) thisDeg += Sub(a);
            }

            sDeg[u] = thisDeg;
        }

        Close(H);
    }
}


void abstractMixedGraph::InitDegInOut() throw()
{
    if (!sDegIn)
    {
        sDegIn =  new TFloat[n];
        sDegOut =  new TFloat[n];
        LogEntry(LOG_MEM,"Generating IO degree labels...");

        THandle H = Investigate();
        investigator &I = Investigator(H);

        for (TNode v=0;v<n;++v)
        {
            TFloat thisDegIn = 0;
            TFloat thisDegOut = 0;

            while (I.Active(v))
            {
                TArc a = I.Read(v);
                if (Blocking(a)) thisDegIn += Sub(a);
                if (Blocking(a^1)) thisDegOut += Sub(a);
            }

            sDegIn[v] = thisDegIn;
            sDegOut[v] = thisDegOut;
        }

        Close(H);
    }
}


TFloat* abstractMixedGraph::InitDistanceLabels(TFloat def) throw()
{
    return registers.InitArray<TFloat>(*this,TokRegLabel,def);
}


TFloat* abstractMixedGraph::InitPotentials(TFloat def) throw()
{
    return registers.InitArray<TFloat>(*this,TokRegPotential,def);
}


TNode* abstractMixedGraph::InitNodeColours(TNode def) throw()
{
    return registers.InitArray<TNode>(*this,TokRegNodeColour,def);
}


TArc* abstractMixedGraph::InitEdgeColours(TArc def) throw()
{
    return registers.InitArray<TArc>(*this,TokRegEdgeColour,def);
}


TArc* abstractMixedGraph::InitPredecessors() throw()
{
    return registers.InitArray<TArc>(*this,TokRegPredecessor,NoArc);
}


void abstractMixedGraph::UpdatePotentials(TFloat cutValue) throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    #if defined(_FAILSAVE_)

    if (!dist)
        Error(ERR_REJECTED,"UpdatePotentials","No distance labels present");

    #endif

    if (potential)
    {
        for (TNode i=0;i<n;++i)
        {
            if (dist[i]<cutValue)
            {
                potential[i] += dist[i];
            }
            else potential[i] += cutValue;
        }
    }
    else
    {
        potential = RawPotentials();

        for (TNode i=0;i<n;++i)
        {
            if (dist[i]<cutValue)
            {
                potential[i] = dist[i];
            }
            else potential[i] = cutValue;
        }
    }

    LogEntry(LOG_MEM,"...Node potentials updated");
}


void abstractMixedGraph::InitPartition() throw()
{
    if (!partition)
    {
        partition = new disjointFamily<TNode>(n,CT);
        LogEntry(LOG_MEM,"...Partition allocated");
    }
    else
    {
        partition -> Init();

        #if defined(_LOGGING_)

        Error(MSG_WARN,"InitPartition","A partition is already present");

        #endif
    }
}


void abstractMixedGraph::InitSubgraph() throw()
{
    ReleaseDegrees();

    for (TArc a=0;a<m;a++) SetSub(2*a,LCap(2*a));
}


TFloat* abstractMixedGraph::GetDistanceLabels() const throw()
{
    return registers.GetArray<TFloat>(TokRegLabel);
}


TFloat* abstractMixedGraph::GetPotentials() const throw()
{
    return registers.GetArray<TFloat>(TokRegPotential);
}


TNode* abstractMixedGraph::GetNodeColours() const throw()
{
    return registers.GetArray<TNode>(TokRegNodeColour);
}


TArc* abstractMixedGraph::GetEdgeColours() const throw()
{
    return registers.GetArray<TArc>(TokRegEdgeColour);
}


TArc* abstractMixedGraph::GetPredecessors() const throw()
{
    return registers.GetArray<TArc>(TokRegPredecessor);
}


TFloat* abstractMixedGraph::RawDistanceLabels() throw()
{
    return registers.RawArray<TFloat>(*this,TokRegLabel);
}


TFloat* abstractMixedGraph::RawPotentials() throw()
{
    TFloat* potential = GetPotentials();

    if (potential) return potential;

    return InitPotentials();
}


TNode* abstractMixedGraph::RawNodeColours() throw()
{
    return registers.RawArray<TNode>(*this,TokRegNodeColour);
}


TArc* abstractMixedGraph::RawPredecessors() throw()
{
    return registers.RawArray<TArc>(*this,TokRegPredecessor);
}


TArc* abstractMixedGraph::RawEdgeColours() throw()
{
    return registers.RawArray<TArc>(*this,TokRegEdgeColour);
}


TNode* abstractMixedGraph::RandomNodeOrder() throw()
{
    TNode* nodeColour = registers.RawArray<TNode>(*this,TokRegNodeColour);
    TNode u = 0;

    for (;u<n;++u) nodeColour[u] = u;

    for (u=0;u<n-2;++u)
    {
        TNode v = u+1+TNode(CT.Rand(n-u-1));
        TNode w = nodeColour[u];
        nodeColour[u] = nodeColour[v];
        nodeColour[v] = w;
    }

    return nodeColour;
}


void abstractMixedGraph::ReleaseAdjacencies() throw()
{
    if (adj)
    {
        delete adj;
        adj = NULL;
        LogEntry(LOG_MEM,"...Adjacencies disallocated");
    }
}


void abstractMixedGraph::ReleaseDegrees() throw()
{
    if (sDegIn)
    {
        delete[] sDegIn;
        sDegIn = NULL;
        LogEntry(LOG_MEM,"...Indegree labels disallocated");
    }

    if (sDegOut)
    {
        delete[] sDegOut;
        sDegOut = NULL;
        LogEntry(LOG_MEM,"...Outdegree labels disallocated");
    }

    if (sDeg)
    {
        delete[] sDeg;
        sDeg = NULL;
        LogEntry(LOG_MEM,"...Degree labels disallocated");
    }
}


void abstractMixedGraph::ReleaseLabels() throw()
{
    registers.ReleaseAttribute(TokRegLabel);
}


void abstractMixedGraph::ReleasePotentials() throw()
{
    registers.ReleaseAttribute(TokRegPotential);
}


void abstractMixedGraph::ReleaseNodeColours() throw()
{
    registers.ReleaseAttribute(TokRegNodeColour);
}


void abstractMixedGraph::ReleaseEdgeColours() throw()
{
    registers.ReleaseAttribute(TokRegEdgeColour);
}


void abstractMixedGraph::ReleasePredecessors() throw()
{
    registers.ReleaseAttribute(TokRegPredecessor);
}


void abstractMixedGraph::ReleaseEmbedding() throw()
{
    if (face)
    {
        delete[] face;
        face = NULL;
        LogEntry(LOG_MEM,"...Dual incidences disallocated");
    }
}


void abstractMixedGraph::ReleasePartition() throw()
{
    if (partition)
    {
        delete partition;
        partition = NULL;
        LogEntry(LOG_MEM,"...Partition disallocated");
    }
}


void abstractMixedGraph::ReleaseNodeMapping() throw()
{
    registers.ReleaseAttribute(TokRegOriginalNode);
}


void abstractMixedGraph::ReleaseArcMapping() throw()
{
    registers.ReleaseAttribute(TokRegOriginalArc);
}


void abstractMixedGraph::Write(const char* fileName) const throw(ERFile)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    char* className = "mixed";

    if (IsBalanced())
    {
        className = "balanced_fnw";
    }
    else if (IsBipartite())
    {
        if (IsDense())
        {
            className = "dense_bigraph";
        }
        else
        {
            className = "bigraph";
        }
    }
    else if (IsDirected())
    {
        if (IsDense())
        {
            className = "dense_digraph";
        }
        else
        {
            className = "digraph";
        }
    }
    else if (IsUndirected())
    {
        if (IsDense())
        {
            className = "dense_graph";
        }
        else
        {
            className = "graph";
        }
    }

    sprintf(CT.logBuffer,"Writing \"%s\" object to \"%s\"...",className,fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    goblinExport F(fileName,CT);

    F.StartTuple(className,0);

    F.StartTuple("definition",0);

    F.StartTuple("nodes",1);
    F.MakeItem(n,0);

    TNode n1 = n;

    if (IsBipartite())
    {
        const abstractBiGraph* G = static_cast<const abstractBiGraph*>(this);
        n1 = G->N1();
    }

    F.MakeItem(n1,0);
    F.MakeItem(ni,0);
    F.EndTuple();

    if (!IsDense())
    {
        F.StartTuple("arcs",1);
        F.MakeItem(m,0);
        F.EndTuple();

        WriteIncidences(&F);
    }

    WriteUCap(&F);
    WriteLCap(&F);
    WriteLength(&F);
    WriteDemand(&F);
    WriteOrientation(&F);

    F.EndTuple();   // definition

    WriteGeometry(&F);
    WriteLayout(&F);
    WriteRegisters(&F);

    CT.sourceNodeInFile = DefaultSourceNode();
    CT.targetNodeInFile = DefaultTargetNode();
    CT.rootNodeInFile   = DefaultRootNode();
    F.WriteConfiguration(CT);

    F.EndTuple();   // class name

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void abstractMixedGraph::WriteSpecial(goblinExport& F,const attributePool& pool,TPoolEnum token) const throw()
{
    if (&pool==&registers)
    {
        if (token==TokRegSubgraph)
        {
            WriteSubgraph(F);
        }
    }
}


void abstractMixedGraph::WriteIncidences(goblinExport* F) const throw()
{
    F -> StartTuple("incidences",0);

    int mLength = CT.ExternalIntLength(2*m-1);

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;++v)
    {
        F -> StartTuple(v,10);

        while (I.Active(v))
        {
            TArc a = I.Read(v);
            F -> MakeItem(a,mLength);
        }

        F -> EndTuple();
    }

    Close(H);

    F -> EndTuple();
}


void abstractMixedGraph::WriteUCap(goblinExport* F) const throw()
{
    if (CUCap())
    {
        F -> StartTuple("ucap",1);
        if (MaxUCap()==InfCap) F -> MakeNoItem(0);
        else F -> MakeItem((TFloat)MaxUCap(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxUCap());
        F -> StartTuple("ucap",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)UCap(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteLCap(goblinExport* F) const throw()
{
    if (CLCap())
    {
        F -> StartTuple("lcap",1);
        F -> MakeItem((TFloat)MaxLCap(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxLCap());
        F -> StartTuple("lcap",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)LCap(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteLength(goblinExport* F) const throw()
{
    if (CLength())
    {
        F -> StartTuple("length",1);
        F -> MakeItem(MaxLength(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxLength());
        F -> StartTuple("length",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)Length(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteDemand(goblinExport* F) const throw()
{
    if (CDemand())
    {
        F -> StartTuple("demand",1);
        F -> MakeItem(Demand(0),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxDemand());
        F -> StartTuple("demand",10);

        for (TNode v=0;v<n;++v)
            F -> MakeItem(Demand(v),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteOrientation(goblinExport* F) const throw()
{
    if (COrientation())
    {
        if (Orientation()==0) return;

        F -> StartTuple("directed",1);
        F -> MakeItem(int(Orientation()),0);
    }
    else
    {
        int lLength = 1;
        F -> StartTuple("directed",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem(int(Orientation(2*a)),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteGeometry(goblinExport* F) const throw()
{
    char* header = "geometry";
    F -> StartTuple(header,0);

    header = "metrics";
    F -> StartTuple(header,1);
    F -> MakeItem(static_cast<long>(MetricType()),0);
    F -> EndTuple();

    header = "dim";
    F -> StartTuple(header,1);
    F -> MakeItem(int(Dim()),0);
    F -> EndTuple();

    if (Dim()>0)
    {
        header = "coordinates";
        F -> StartTuple(header,0);

        for (TDim i=0;i<Dim();++i)
        {
            int xLength = 0;

            for (TNode v=0;v<n+ni;++v)
            {
                int thisLength = CT.ExternalLength<TFloat>(C(v,i));

                if (thisLength>xLength) xLength = thisLength;
            }

            char strToken[10];
            sprintf(strToken,"axis%lu",static_cast<unsigned long>(i));
            F -> StartTuple(strToken,10);

            for (TNode v=0;v<n+ni;++v) F -> MakeItem(C(v,i),xLength);

            F -> EndTuple();
        }

        F -> EndTuple(); // coordinates
    }

    if (Geometry())
    {
        TNode* pBound = Geometry()->GetArray<TNode>(TokGeoMinBound);

        if (pBound)
        {
            header = "minBound";
            F -> StartTuple(header,1);
            F -> MakeItem(int(*pBound),0);
            F -> EndTuple();
        }

        pBound = Geometry()->GetArray<TNode>(TokGeoMaxBound);

        if (pBound)
        {
            header = "maxBound";
            F -> StartTuple(header,1);
            F -> MakeItem(int(*pBound),0);
            F -> EndTuple();
        }
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteRegisters(goblinExport* F) const throw()
{
    registers.WritePool(*this,*F,"solutions");
}


void abstractMixedGraph::WriteRegister(goblinExport& F,TPoolEnum token) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (token>=TokRegEndSection)
    {
        sprintf(CT.logBuffer,"No such register: %lu",
            static_cast<unsigned long>(token));
        Error(ERR_RANGE,"WriteRegister",CT.logBuffer);
    }

    #endif

    registers.WriteAttribute(*this,F,token);
}


void abstractMixedGraph::WriteSubgraph(goblinExport& F) const throw()
{
    // The internal representations differs from the other registers.
    // Therefore, a special handling is (temporarily) required.

    bool voidSubgraph = true;

    for (TArc a=0;a<m;a++)
        if (Sub(2*a)>LCap(a)) voidSubgraph = false;

    if (!voidSubgraph)
    {
        F.StartTuple("subgraph",10);

        int length = 1;

        for (TArc a=0;a<m;a++)
        {
            int thisLength = CT.ExternalFloatLength(Sub(2*a));

            if (thisLength>length) length = thisLength;
        }

        for (TArc a=0;a<m;a++) F.MakeItem(Sub(2*a),length);

        F.EndTuple();
    }
}


void abstractMixedGraph::ReadSpecial(goblinImport& F,attributePool& pool,TPoolEnum token) throw(ERParse)
{
    if (&pool==RepresentationalData())
    {
        switch (token)
        {
            case TokReprIncidences:
            {
                graphRepresentation* X = Representation();

                if (X)
                {
                    static_cast<sparseRepresentation*>(X) -> ReadIncidences(F);
                    return;
                }
                else
                {
                    NoSparseRepresentation("ReadSpecial");
                }
            }
            case TokReprNNodes:
            {
                ReadNNodes(F);
                return;
            }
            case TokReprNArcs:
            {
                ReadNArcs(F);
                return;
            }
        }
    }
    else if (&pool==Geometry())
    {
        switch (token)
        {
            case TokGeoCoordinates:
            {
                // This code compensates an additional nesting level
                Geometry() -> ReadPool(*this,F);
                return;
            }
        }
    }
    else if (&pool==LayoutData())
    {
    }
    else if (&pool==&registers)
    {
        switch (token)
        {
            case TokRegSubgraph:
            {
                ReadSubgraph(F);
                return;
            }
        }
    }
    else
    {
        // This is considered to be the root pool which is
        // only locally known to the constructor method

        switch (token)
        {
            case TokGraphRepresentation:
            {
                ReadRepresentation(F);
                return;
            }
            case TokGraphObjectives:
            {
                pool.ReadPool(*this,F);
                return;
            }
            case TokGraphGeometry:
            {
                ReadGeometry(F);
                return;
            }
            case TokGraphLayout:
            {
                ReadLayoutData(F);
                return;
            }
            case TokGraphRegisters:
            {
                ReadRegisters(F);
                return;
            }
            case TokGraphConfigure:
            {
                F.ReadConfiguration();
                return;
            }
            case TokGraphLength:
            {
                RepresentationalData() -> ReadPool(*this,F);
                return;
            }
        }
    }
}


void abstractMixedGraph::ReadAllData(goblinImport& F) throw(ERParse)
{
    attributePool graphRoot(listOfGraphPars,TokGraphEndSection,attributePool::ATTR_FULL_RANK);
    graphRoot.ReadPool(*this,F);
}


void abstractMixedGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    delete[] nodes;
}


void abstractMixedGraph::ReadNArcs(goblinImport& F) throw(ERParse)
{
    graphRepresentation* X = Representation();

    if (X)
    {
        TArc* arcs = F.GetTArcTuple(1);
        m = arcs[0];
        delete[] arcs;

        CheckLimits();

        if (!IsDense()) X -> Reserve(n,m,n+ni);
    }
}


void abstractMixedGraph::ReadRepresentation(goblinImport& F) throw(ERParse)
{
    RepresentationalData() -> ReadPool(*this,F);
}


void abstractMixedGraph::ReadGeometry(goblinImport& F) throw(ERParse)
{
    Geometry() -> ReadPool(*this,F);
}


void abstractMixedGraph::ReadRegisters(goblinImport& F) throw(ERParse)
{
    registers.ReadPool(*this,F);
}


void abstractMixedGraph::ReadSubgraph(goblinImport& F) throw(ERParse)
{
    // The internal representations differs from the other registers.
    // Therefore, a special handling is (temporarily) required.

    TFloat *tmpSub = F.GetTFloatTuple(m);
    bool isConstant = F.Constant();

    for (TArc a=0;a<m;a++)
    {
        TFloat thisSub = InfFloat;

        if (!isConstant) thisSub = tmpSub[a];
        else thisSub = tmpSub[0];

        if (fabs(thisSub)<LCap(2*a) || fabs(thisSub)>UCap(2*a))
        {
            sprintf(CT.logBuffer,"Arc multiplicity exeeds capacity bounds: %lu",
                static_cast<unsigned long>(a));
            Error(ERR_RANGE,"ReadSubgraph",CT.logBuffer);
        }

        SetSub(2*a,thisSub);
    }

    delete[] tmpSub;
}


char* abstractMixedGraph::Display() const throw(ERRejected,ERFile)
{
    if (CT.displayMode==0)
    {
        TextDisplay();
        return NULL;
    }

    #if defined(_TRACING_)

    CT.IncreaseCounter();

    if (CT.displayMode==3)
    {
        char* gobName = new char[strlen(CT.Label())+15];
        sprintf(gobName,"%s.trace%lu.gob",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        Write(gobName);
        if (CT.traceEventHandler) CT.traceEventHandler(gobName);
        delete[] gobName;

        return const_cast<char*>(CT.Label());
    }

    if (CT.displayMode==1)
    {
        char* figName = new char[strlen(CT.Label())+10];
        sprintf(figName,"%s.%lu.fig",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        ExportToXFig(figName);
        delete[] figName;

        char* commandStr = new char[strlen(CT.Label())+15];
        sprintf(commandStr,"xfig %s.%lu.fig &",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        system (commandStr);
        delete[] commandStr;
    }

    if (CT.displayMode==2)
    {
        char* tkName = new char[strlen(CT.Label())+10];
        sprintf(tkName,"%s.%lu.tk",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        ExportToTk(tkName);
        delete[] tkName;

        char* commandStr = new char[strlen(CT.Label())+15];
        sprintf(commandStr,"wish display %s.%lu &",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        system (commandStr);
        delete[] commandStr;
    }

    #endif

    return NULL;
}


void abstractMixedGraph::TextDisplay(TNode i,TNode j) const throw()
{
    sprintf(CT.logBuffer,"Graph has %lu nodes and %lu arcs",
        static_cast<unsigned long>(n),static_cast<unsigned long>(m));
    LogEntry(MSG_TRACE,CT.logBuffer);

    if (m==0) return;

    TArc* pred = GetPredecessors();
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    TNode first = 0;
    TNode last = n-1;

    if (i!=NoNode)
    {
        first = i;

        if (j==NoNode) last = i;
        else last = j;
    }

    int lArc   = CT.ExternalIntLength(2*m)+1;
    int lNode  = CT.ExternalIntLength(n)+4;
    int lFloat = CT.ExternalFloatLength(1.0e20/3.0)+1;
    int lRow   = 2 + lArc + lNode + lFloat;

    if (!CUCap())   lRow += lFloat;
    if (!CLCap())   lRow += lFloat;
    if (!CLength()) lRow += lFloat;
    if (potential)  lRow += lFloat;

    THandle LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    LH = LogStart(MSG_TRACE2,"");
    sprintf(CT.logBuffer,"  %*.*s",lArc,lArc,"arc");
    LogAppend(LH,CT.logBuffer);
    sprintf(CT.logBuffer,"%*.*s",lNode,lNode,"tail");
    LogAppend(LH,CT.logBuffer);

    if (!CUCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"ucap");
        LogAppend(LH,CT.logBuffer);
    }

    sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"subgraph");
    LogAppend(LH,CT.logBuffer);

    if (!CLCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"lcap");
        LogAppend(LH,CT.logBuffer);
    }

    if (!CLength())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"length");
        LogAppend(LH,CT.logBuffer);
    }

    if (potential)
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"redlength");
        LogAppend(LH,CT.logBuffer);
    }

    LogEnd(LH);

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=first;v<=last;++v)
    {
        LH = LogStart(MSG_TRACE2,"");

        for (int k=0;k<lRow;++k) LogAppend(LH,"_");

        LogEnd(LH);

        LogEntry(MSG_TRACE2,"");
        sprintf(CT.logBuffer,"Node %lu",static_cast<unsigned long>(v));
        LH = LogStart(MSG_TRACE2,CT.logBuffer);

        if (CMax(0)!=0 || CMax(1)!=0)
        {
            sprintf(CT.logBuffer," (%g|%g)",
                static_cast<double>(C(v,0)),static_cast<double>(C(v,1)));
            LogAppend(LH,CT.logBuffer);
        }

        LogAppend(LH,":");

        bool first = true;

        if (dist)
        {
            LogAppend(LH," (");
            sprintf(CT.logBuffer,"Distance label %g",static_cast<double>(dist[v]));
            LogAppend(LH,CT.logBuffer);
            first = false;
        }

        if (potential)
        {
            if (first) LogAppend(LH," (");
            else LogAppend(LH,", ");

            sprintf(CT.logBuffer,"Node potential %g",static_cast<double>(potential[v]));
            LogAppend(LH,CT.logBuffer);
            first = false;
        }

        if (!first) LogAppend(LH,")");

        LogEnd(LH);

        LogEntry(MSG_TRACE2,"");

        while (I.Active(v))
        {
            LH = LogStart(MSG_TRACE2,"");
            TArc a = I.Read(v);

            #if defined(_FAILSAVE_)

            if (StartNode(a)!=v)
            {
                sprintf(CT.logBuffer,"Incidence mismatch: %lu",
                    static_cast<unsigned long>(a));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
                sprintf(CT.logBuffer,"Concerning nodes %lu and %lu",
                    static_cast<unsigned long>(v),
                    static_cast<unsigned long>(StartNode(a)));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
            }

            #endif

            if (pred && pred[v]==(a^1)) LogAppend(LH,"P");
            else LogAppend(LH," ");

            if (Blocking(a)) LogAppend(LH,"B");
            else LogAppend(LH," ");

            sprintf(CT.logBuffer,"%*.*lu",
                lArc,lArc,static_cast<unsigned long>(a));
            LogAppend(LH,CT.logBuffer);
            sprintf(CT.logBuffer,"%*.*lu",
                lNode,lNode,static_cast<unsigned long>(EndNode(a)));
            LogAppend(LH,CT.logBuffer);

            if (!CUCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(UCap(a)));
                LogAppend(LH,CT.logBuffer);
            }

            sprintf(CT.logBuffer,"%*.*g",
                lFloat,lFloat,static_cast<double>(Sub(a)));
            LogAppend(LH,CT.logBuffer);

            if (!CLCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(LCap(a)));
                LogAppend(LH,CT.logBuffer);
            }

            if (!CLength())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(Length(a)));
                LogAppend(LH,CT.logBuffer);
            }

            if (potential)
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(RedLength(potential,a)));
                LogAppend(LH,CT.logBuffer);
            }

            LogEnd(LH);
        }
    }

    LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    LogEntry(MSG_TRACE2,"");

    if (CUCap())
    {
        sprintf(CT.logBuffer,"Capacities: %g",static_cast<double>(MaxUCap()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    if (CLCap())
    {
        sprintf(CT.logBuffer,"Lower bounds: %g",static_cast<double>(MaxLCap()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    if (CLength())
    {
        sprintf(CT.logBuffer,"Length labels: %g",static_cast<double>(MaxLength()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    Close(H);
}


void abstractMixedGraph::DisplayPath(TNode u,TNode v) throw(ERRange,ERRejected)
{
    TArc* pred = GetPredecessors();

    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"DisplayPath","Missing predecessor labels");

    if (u>=n) NoSuchNode("DisplayPath",u);

    if (v>=n) NoSuchNode("DisplayPath",v);

    #endif

    TFloat l = 0;
    TNode count = 0;
    TNode w = v;

    LogEntry(LOG_RES,"Encoded path in reverse order:");
    sprintf(CT.logBuffer," (%lu",static_cast<unsigned long>(w));
    THandle LH = LogStart(LOG_RES,CT.logBuffer);

    while (w!=u || count==0)
    {
        TArc a = pred[w];
        l += Length(a);
        w = StartNode(a);
        sprintf(CT.logBuffer,", %lu",static_cast<unsigned long>(w));
        LogAppend(LH,CT.logBuffer);
        count++;

        #if defined(_FAILSAVE_)

        if (count>n) Error(ERR_REJECTED,"DisplayPath","Missing start node");

        #endif
    }

    LogEnd(LH,")");
    sprintf(CT.logBuffer,"Total length: %g",static_cast<double>(l));
    LogEntry(LOG_RES,CT.logBuffer);
    sprintf(CT.logBuffer,"Total number of arcs: %lu",static_cast<unsigned long>(count));
    LogEntry(LOG_RES,CT.logBuffer);
}


TFloat abstractMixedGraph::CutCapacity(TNode separator) throw(ERCheck)
{
    TNode* dist = GetNodeColours();

    if (!dist) Error(ERR_CHECK,"CutCapacity","No edge cut specified");

    LogEntry(LOG_METH,"Checking the d-labels...");
    OpenFold();

    TFloat sCap = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Cut edges: ");

    #endif

    for (TArc a=0;a<2*m;++a)
    {
        if (dist[StartNode(a)]<separator && dist[EndNode(a)]>=separator)
        {
            TCap thisCap = 0;

            if (!Blocking(a) && !Blocking(a^1))
            {
                if (a&1==0) thisCap = UCap(a);
            }
            else if (!Blocking(a) && Blocking(a^1))
            {
                thisCap = UCap(a);
            }
            else if (Blocking(a) && !Blocking(a^1))
            {
                thisCap = -LCap(a);
            }

            #if defined(_LOGGING_)

            if (thisCap>0 && CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu[%g] ",
                    static_cast<unsigned long>(a),static_cast<double>(thisCap));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            sCap += thisCap;
        }
    }

    #if defined(_LOGGING_)

    LogEnd(LH);

    #endif

    CloseFold();
    sprintf(CT.logBuffer,"...Total capacity: %g",static_cast<double>(sCap));
    LogEntry(LOG_RES,CT.logBuffer);

    return sCap;
}


void abstractMixedGraph::ExportToXFig(const char* fileName) const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing xFig canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToXFig E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToTk(const char* fileName) const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing Tk canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToTk E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToDot(const char* fileName)
    const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing Tk canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToDot E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToAscii(const char* fileName,TOption format)
    const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing text form to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    ofstream expFile(fileName, ios::out);

    if (n==0) return;

    TArc* pred = GetPredecessors();
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    int lArc   = CT.ExternalIntLength(2*m)+1;
    int lNode  = CT.ExternalIntLength(n)+4;
    int lFloat = CT.ExternalFloatLength(1.0e20/3.0)+1;
    int lRow   = 2 + lArc + lNode + lFloat;

    if (!CUCap())   lRow += lFloat;
    if (!CLCap())   lRow += lFloat;
    if (!CLength()) lRow += lFloat;
    if (potential)  lRow += lFloat;

    for (int k=0;k<lRow;++k) expFile << "-";

    expFile << endl;

    sprintf(CT.logBuffer,"  %*.*s",lArc,lArc,"arc");
    expFile << CT.logBuffer;
    sprintf(CT.logBuffer,"%*.*s",lNode,lNode,"tail");
    expFile << CT.logBuffer;

    if (!CUCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"ucap");
        expFile << CT.logBuffer;
    }

    sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"subgraph");
    expFile << CT.logBuffer;

    if (!CLCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"lcap");
        expFile << CT.logBuffer;
    }

    if (!CLength())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"length");
        expFile << CT.logBuffer;
    }

    if (potential)
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"redlength");
        expFile << CT.logBuffer;
    }

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;++v)
    {
        expFile << endl;

        for (int k=0;k<lRow;++k) expFile << "-";

        sprintf(CT.logBuffer,"Node %lu",static_cast<unsigned long>(v));
        expFile << endl << CT.logBuffer;

        if (CMax(0)!=0 || CMax(1)!=0)
        {
            sprintf(CT.logBuffer," (%g|%g)",
                static_cast<double>(C(v,0)),static_cast<double>(C(v,1)));
            expFile << CT.logBuffer;
        }

        expFile << ":";

        bool first = true;

        if (dist)
        {
            expFile << " (";
            sprintf(CT.logBuffer,"Distance label %g",static_cast<double>(dist[v]));
            expFile << CT.logBuffer;
            first = false;
        }

        if (potential)
        {
            if (first) expFile << " (";
            else expFile << ", ";

            sprintf(CT.logBuffer,"Node potential %g",static_cast<double>(potential[v]));
            expFile << CT.logBuffer;
            first = false;
        }

        if (!first) expFile << ")";

        expFile << endl;

        while (I.Active(v))
        {
            expFile << endl;
            TArc a = I.Read(v);

            #if defined(_FAILSAVE_)

            if (StartNode(a)!=v)
            {
                sprintf(CT.logBuffer,"Incidence mismatch: %lu",
                    static_cast<unsigned long>(a));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
                sprintf(CT.logBuffer,"Concerning nodes %lu and %lu",
                    static_cast<unsigned long>(v),
                    static_cast<unsigned long>(StartNode(a)));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
            }

            #endif

            if (pred && pred[v]==(a^1)) expFile << "P";
            else expFile << " ";

            if (Blocking(a)) expFile << "B";
            else expFile << " ";

            sprintf(CT.logBuffer,"%*.*lu",
                lArc,lArc,static_cast<unsigned long>(a));
            expFile << CT.logBuffer;
            sprintf(CT.logBuffer,"%*.*lu",
                lNode,lNode,static_cast<unsigned long>(EndNode(a)));
            expFile << CT.logBuffer;

            if (!CUCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(UCap(a)));
                expFile << CT.logBuffer;
            }

            sprintf(CT.logBuffer,"%*.*g",
                lFloat,lFloat,static_cast<double>(Sub(a)));
            expFile << CT.logBuffer;

            if (!CLCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(LCap(a)));
                expFile << CT.logBuffer;
            }

            if (!CLength())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(Length(a)));
                expFile << CT.logBuffer;
            }

            if (potential)
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(RedLength(potential,a)));
                expFile << CT.logBuffer;
            }
        }
    }

    expFile << endl;

    for (int k=0;k<lRow;++k) expFile << "-";

    bool constantsFound = false;

    if (CUCap())
    {
        sprintf(CT.logBuffer,"Capacities: %g",static_cast<double>(MaxUCap()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (CLCap())
    {
        sprintf(CT.logBuffer,"Lower bounds: %g",static_cast<double>(MaxLCap()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (CLength())
    {
        sprintf(CT.logBuffer,"Length labels: %g",static_cast<double>(MaxLength()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (!constantsFound) return;

    expFile << endl;

    for (int k=0;k<lRow;++k) expFile << "-";

    expFile << endl;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   abstractMixedGraph.cpp
/// \brief  #abstractMixedGraph partial class implementation

#include "exportToXFig.h"
#include "exportToTk.h"
#include "exportToDot.h"
#include "mixedGraph.h"
#include "disjointFamily.h"
#include "basicHeap.h"
#include "fibonacciHeap.h"
#include "binaryHeap.h"
#include "denseRepresentation.h"
#include "sparseBigraph.h"


// ------------------------------------------------------


abstractMixedGraph::abstractMixedGraph(TNode nn,TArc mm) throw() :
    registers(listOfRegisters,TokRegEndSection,attributePool::ATTR_FULL_RANK)
{
    n = nn;
    ni = 0;
    m = mm;
    CheckLimits();

    itCounter = 0;
    LH = RH = NoHandle;
    pInvestigator = NULL;
    partition = NULL;
    nHeap = NULL;
    adj = NULL;

    sDeg = NULL;
    sDegIn = NULL;
    sDegOut = NULL;

    face = NULL;

    defaultSourceNode = NoNode;
    defaultTargetNode = NoNode;
    defaultRootNode = NoNode;

    LogEntry(LOG_MEM,"...Abstract mixed graph allocated");
}


abstractMixedGraph::~abstractMixedGraph() throw()
{
    ReleaseInvestigators();
    ReleasePredecessors();
    ReleaseLabels();
    ReleasePartition();
    ReleasePotentials();
    ReleaseNodeColours();
    ReleaseEdgeColours();
    ReleaseDegrees();
    ReleaseAdjacencies();
    ReleaseEmbedding();
    ReleaseNodeMapping();
    ReleaseArcMapping();

    delete[] pInvestigator;

    LogEntry(LOG_MEM,"...Abstract mixed graph disallocated");
}


void abstractMixedGraph::CheckLimits() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (n>=CT.MaxNode())
    {
        sprintf(CT.logBuffer,"Number of nodes is out of range: %lu",
            static_cast<unsigned long>(n));
        Error(ERR_REJECTED,"CheckLimits",CT.logBuffer);
    }

    if (2*m>CT.MaxArc()-2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"CheckLimits",CT.logBuffer);
    }

    #endif
}


unsigned long abstractMixedGraph::Allocated() const throw()
{
    unsigned long tmpSize = 0;

    tmpSize += registers.Size();

    if (pInvestigator!=NULL)    tmpSize += itCounter*sizeof(investigator**);
    if (sDeg!=NULL)             tmpSize += n*sizeof(TFloat);
    if (sDegIn!=NULL)           tmpSize += n*sizeof(TFloat);
    if (sDegOut!=NULL)          tmpSize += n*sizeof(TFloat);
    if (face!=NULL)             tmpSize += 2*m*sizeof(TNode);

    return tmpSize;
}


goblinQueue<TNode,TFloat> * abstractMixedGraph::NewNodeHeap() throw(ERRejected)
{
    switch (CT.methPQ)
    {
        case 0:
        {
            return new basicHeap<TNode,TFloat>(n,CT);
        }
        case 1:
        {
            return new binaryHeap<TNode,TFloat>(n,CT);
        }
        case 2:
        {
            return new fibonacciHeap<TNode,TFloat>(n,CT);
        }
    }

    UnknownOption("NewNodeHeap",CT.methPQ);

    return NULL;
}


goblinQueue<TArc,TFloat> * abstractMixedGraph::NewArcHeap() throw(ERRejected)
{
    switch (CT.methPQ)
    {
        case 0:
        {
            return new basicHeap<TArc,TFloat>(m,CT);
        }
        case 1:
        {
            return new binaryHeap<TArc,TFloat>(m,CT);
        }
        case 2:
        {
            return new fibonacciHeap<TArc,TFloat>(m,CT);
        }
    }

    UnknownOption("NewArcHeap",CT.methPQ);

    return NULL;
}


bool abstractMixedGraph::IsGraphObject() const throw()
{
    return true;
}


bool abstractMixedGraph::IsBipartite() const throw()
{
    return false;
}


bool abstractMixedGraph::IsDirected() const throw()
{
    return false;
}


bool abstractMixedGraph::IsUndirected() const throw()
{
    return false;
}


bool abstractMixedGraph::IsSparse() const throw()
{
    return false;
}


bool abstractMixedGraph::IsDense() const throw()
{
    return false;
}


bool abstractMixedGraph::IsBalanced() const throw()
{
    return false;
}


TNode abstractMixedGraph::DefaultSourceNode() const throw()
{
    return defaultSourceNode;
}


TNode abstractMixedGraph::DefaultTargetNode() const throw()
{
    return defaultTargetNode;
}


TNode abstractMixedGraph::DefaultRootNode() const throw()
{
    return defaultRootNode;
}


TArc abstractMixedGraph::InsertArc(TNode u,TNode v) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("InsertArc",u);

    if (v>=n) NoSuchNode("InsertArc",v);

    #endif

    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("InsertArc");

    #endif

    TFloat thisLength = RepresentationalData()->DefaultValue<TFloat>(TokReprLength,1);
    if (CT.randLength) thisLength = TFloat(CT.SignedRand());

    TCap thisUCap = RepresentationalData()->DefaultValue<TCap>(TokReprUCap,1);
    if (CT.randUCap) thisUCap = TCap(1+CT.UnsignedRand());

    TCap thisLCap = RepresentationalData()->DefaultValue<TCap>(TokReprLCap,0);
    if (CT.randLCap) thisLCap = TCap(CT.Rand(1+int(thisUCap)));


    if (IsSparse())
    {
        if (IsBalanced())
        {
            m++;
            static_cast<sparseRepresentation*>(X)
                ->InsertArc(v^1,u^1,thisUCap,thisLength,thisLCap);
        }

        m++;
        return static_cast<sparseRepresentation*>(X)
                ->InsertArc(u,v,thisUCap,thisLength,thisLCap);
    }

    #if defined(_FAILSAVE_)

    TArc a = Adjacency(u,v);

    if (a==NoArc)
    {
        // This happens if the graph is bipartite,
        // and u and v are in the same partition
        sprintf(CT.logBuffer,"Nodes %lu and %lu are non-adjacent",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"InsertArc",CT.logBuffer);
    }

    if (a&1)
    {
        // This happens if the graph is undirected and v>u
        Error(MSG_WARN,"InsertArc","End nodes are flipped");
    }

    #endif

    if (UCap(a)>0)
    {
        // Do not change the length of already existing arcs
        thisLength = Length(a);
    }

    return static_cast<denseRepresentation*>(X)
            ->InsertArc(a>>1,thisUCap,thisLength,thisLCap);
}


TArc abstractMixedGraph::InsertArc(TNode u,TNode v,TCap cc,TFloat ll,TCap bb)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("InsertArc",u);

    if (v>=n) NoSuchNode("InsertArc",v);

    #endif

    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("InsertArc");

    #endif

    if (IsSparse())
    {
        if (IsBalanced())
        {
            m++;
            static_cast<sparseRepresentation*>(X) -> InsertArc(v^1,u^1,cc,ll,bb);
        }

        m++;
        return static_cast<sparseRepresentation*>(X) -> InsertArc(u,v,cc,ll,bb);
    }

    TArc a = Adjacency(u,v);

    #if defined(_FAILSAVE_)

    if (a==NoArc)
    {
        // This happens if the graph is bipartite,
        // and u and v are in the same partition
        sprintf(CT.logBuffer,"Nodes %lu and %lu are non-adjacent",
            static_cast<unsigned long>(u),static_cast<unsigned long>(v));
        Error(ERR_REJECTED,"InsertArc",CT.logBuffer);
    }

    if (a&1)
    {
        // This happens if the graph is undirected and v>u
        Error(MSG_WARN,"InsertArc","End nodes are flipped");
    }

    #endif

    return static_cast<denseRepresentation*>(X) -> InsertArc(a>>1,cc,ll,bb);
}


TNode abstractMixedGraph::InsertNode() throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X || IsDense()) NoSparseRepresentation("InsertNode");

    #endif

    n++;
    TNode ret = static_cast<sparseRepresentation*>(X)->InsertNode();

    ReleaseAdjacencies();
    ReleasePartition();

    if (sDegIn)
    {
        sDegIn = (TFloat*)GoblinRealloc(sDegIn,n*sizeof(TFloat));
        sDegIn[n-1] = 0;
    }

    if (sDegOut)
    {
        sDegOut = (TFloat*)GoblinRealloc(sDegOut,n*sizeof(TFloat));
        sDegOut[n-1] = 0;
    }

    if (sDeg)
    {
        sDeg = (TFloat*)GoblinRealloc(sDeg,n*sizeof(TFloat));
        sDeg[n-1] = 0;
    }

    return ret;
}


void abstractMixedGraph::DeleteNode(TNode v) throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X || IsDense()) NoSparseRepresentation("DeleteNode");

    #endif

    sparseRepresentation* Y = static_cast<sparseRepresentation*>(X);

    if (IsBalanced() && v<n)
    {
        if (v&1)
        {
            Y -> DeleteNode(v);
            Y -> DeleteNode(v^1);
        }
        else
        {
            Y -> DeleteNode(v^1);
            Y -> DeleteNode(v);
        }

        return;
    }

    if (IsBipartite())
    {
        sparseBiGraph* G = static_cast<sparseBiGraph*>(this);

        if (v<G->N1())
        {
            // Left hand noded can be deleted only after moving them to the
            // right-hand side. And this is possible only after deleting all
            // incident arcs.
            Y -> CancelNode(v);
            G -> SwapNode(v);
        }
    }

    Y -> DeleteNode(v);
}


void abstractMixedGraph::RandomArcs(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc())
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"RandomArcs",CT.logBuffer);
    }

    #endif

    if (CT.logMan && _m>0)
    {
        sprintf(CT.logBuffer,"Generating %lu edges...",
            static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    for (TArc i=0;i<_m;)
    {
        TNode u = (TNode)CT.Rand(n);
        TNode v = (TNode)CT.Rand(n);

        TArc a = NoArc;

        if (!CT.randParallels && m>0) a = Adjacency(u,v,ADJ_SEARCH);

        if ((a==NoArc || UCap(a)==0) && (CT.randParallels || u!=v))
        {
            ReleaseInvestigators();
            InsertArc(u,v);
            i++;
        }
    }
}


void abstractMixedGraph::RandEulerian(TArc _m) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (m+_m>=CT.MaxArc() || _m<2)
    {
        sprintf(CT.logBuffer,"Number of arcs is out of range: %lu",
            static_cast<unsigned long>(m));
        Error(ERR_REJECTED,"RandEulerian",CT.logBuffer);
    }

    #endif

    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"Generating eulerian cycle of length %lu...",
            static_cast<unsigned long>(_m));
        LogEntry(LOG_MAN,CT.logBuffer);
    }

    TNode start = (TNode)CT.Rand(n);
    TNode u = start;
    TNode v = start;

    for (TArc i=0;i<_m-1;++i)
    {
        for (char timeOut=100;timeOut>0;timeOut--)
        {
            while (u==v) v = TNode(CT.Rand(n));

            if (!CT.randParallels && m>0)
            {
                TArc a = Adjacency(u,v,ADJ_SEARCH);
                if (a==NoArc || UCap(a)==0) timeOut = 0;
            }
            else timeOut = 0;
        }

        ReleaseInvestigators();
        InsertArc(u,v);
        u = v;
    }

    InsertArc(v,start);
}


void abstractMixedGraph::RandRegular(TArc k) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (n&k&1)
    {
        sprintf(CT.logBuffer,"Parity mismatch: n=%lu, k=%lu",
            static_cast<unsigned long>(n),static_cast<unsigned long>(k));
        Error(ERR_REJECTED,"RandRegular",CT.logBuffer);
    }

    #endif

    TNode minDeg = 0;
    TNode nMin = n;
    TNode nDef = n;

    TArc* dg = new TArc[n];

    for (TNode v=0;v<n;++v) dg[v]=0;

    while (minDeg<k)
    {
        while (nMin>0)
        {
            TNode shift = 1+CT.Rand(nMin);
            TNode v = n;

            while (shift>0)
            {
                v--;
                if (dg[v]==minDeg) shift--;
            }

            shift = 1+CT.Rand(nDef-1);
            TNode w = n;

            while (shift>0)
            {
                w--;
                if (dg[w]<k && v!=w) shift--;
            }

            nMin--;

            if (dg[w]==minDeg) nMin--;

            InsertArc(v,w);
            dg[v]++;
            dg[w]++;

            if (dg[v]==k) nDef--;
            if (dg[w]==k) nDef--;

        }

        minDeg++;

        for (TNode v=0;v<n;++v)
        {
            if (dg[v]==minDeg) nMin++;
        }
    }

    delete[] dg;

    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"...Random %lu-regular graph generated",
            static_cast<unsigned long>(k));
        LogEntry(LOG_MAN,CT.logBuffer);
    }
}


void abstractMixedGraph::AddGraphByNodes(abstractMixedGraph& G,TMergeLayoutMode mergeLayoutMode)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || G.IsDense())
        Error(ERR_REJECTED,"AddGraphByNodes","Cannot merge dense graphs");

    if (!Representation()) NoRepresentation("AddGraphByNodes");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode nOrig = n;

    TFloat xMin = 0.0,xMax = 0.0,yMin = 0.0,yMax = 0.0;

    Layout_GetBoundingInterval(0,xMin,xMax);
    Layout_GetBoundingInterval(1,yMin,yMax);

    TFloat xMinG = 0.0,xMaxG = 0.0,yMinG = 0.0,yMaxG = 0.0;

    G.Layout_GetBoundingInterval(0,xMinG,xMaxG);
    G.Layout_GetBoundingInterval(1,yMinG,yMaxG);

    TFloat xShift = (mergeLayoutMode==MERGE_ALIGN_RIGHT) ? xMaxG-xMin : 0;
    TFloat yShift = (mergeLayoutMode==MERGE_ALIGN_BELOW) ? yMaxG-yMin : 0;


    // Add the nodes of G, position as required by mergeLayoutMode
    for (TNode v=0;v<G.N();++v)
    {
        TNode w = InsertNode();
        X -> SetC(w,0,G.C(v,0)+xShift);
        X -> SetC(w,1,G.C(v,1)+yShift);
    }

    // Add the arcs of G
    for (TArc a=0;a<G.M();a++)
    {
        TNode u = nOrig+G.StartNode(2*a);
        TNode v = nOrig+G.EndNode(2*a);

        TArc a2 = InsertArc(u,v,G.UCap(2*a),G.Length(2*a),G.LCap(2*a));

        if (!IsDirected() && !IsUndirected())
            X -> SetOrientation(2*a2,G.Orientation(2*a));
    }


    // Assign an explicit bounding box
    if (mergeLayoutMode==MERGE_ALIGN_RIGHT)
    {
        X -> Layout_SetBoundingInterval(0,xMin,xMax+xMaxG-xMinG);
    }
    else
    {
        X -> Layout_SetBoundingInterval(0,(xMin<xMinG) ? xMin : xMinG,(xMax<xMaxG) ? xMaxG : xMax);
    }

    if (mergeLayoutMode==MERGE_ALIGN_BELOW)
    {
        X -> Layout_SetBoundingInterval(1,yMin,yMax+yMaxG-yMinG);
    }
    else
    {
        X -> Layout_SetBoundingInterval(1,(yMin<yMinG) ? yMin : yMinG,(yMax<yMaxG) ? yMaxG : yMax);
    }


    // Assign node colours such that the added graph can be distinguished
    TNode* colour = GetNodeColours();
    TNode newColour = 1;

    if (colour)
    {
        for (TNode v=0;v<nOrig;++v)
        {
            if (colour[v]>=newColour) newColour = colour[v]+1;
        }
    }
    else
    {
        colour = InitNodeColours(0);
    }

    for (TNode v=nOrig;v<N();++v) colour[v] = newColour;


    if (CT.logMan)
    {
        sprintf(CT.logBuffer,"...Graph \"%s\" merged",G.Label());
        LogEntry(LOG_MAN,CT.logBuffer);
    }
}


static void vectorProduct(const vector<TFloat>& x,const vector<TFloat>& y, vector<TFloat>& out)
{
    out[0] = x[1]*y[2]-x[2]*y[1];
    out[1] = x[2]*y[0]-x[0]*y[2];
    out[2] = x[0]*y[1]-x[1]*y[0];
}


static TFloat normalizeVector(vector<TFloat>& vec)
{
    TFloat norm = sqrt(vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2]);

    if (fabs(norm)<10e-12) return 0.0;

    vec[0] /= norm;
    vec[1] /= norm;
    vec[2] /= norm;

    return norm;
}


static TFloat solveLEQ3(const vector<TFloat>& rh,const vector<TFloat>& col0,
    const vector<TFloat>& col1, const vector<TFloat>& col2, vector<TFloat>& out)
{
    TFloat det = col0[0]*col1[1]*col2[2] + col0[1]*col1[2]*col2[0] + col0[2]*col1[0]*col2[1]
               - col0[0]*col1[2]*col2[1] - col0[1]*col1[0]*col2[2] - col0[2]*col1[1]*col2[0];

    if (fabs(det)<10e-12) return 0.0;

    vector<TFloat> inv(3);
    inv[0] = col1[1]*col2[2]-col1[2]*col2[1];
    inv[1] = col1[2]*col2[0]-col1[0]*col2[2];
    inv[2] = col1[0]*col2[1]-col1[1]*col2[0];
    out[0] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    inv[0] = col0[2]*col2[1]-col0[1]*col2[2];
    inv[1] = col0[0]*col2[2]-col0[2]*col2[0];
    inv[2] = col0[1]*col2[0]-col0[0]*col2[1];
    out[1] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    inv[0] = col0[1]*col1[2]-col0[2]*col1[1];
    inv[1] = col0[2]*col1[0]-col0[0]*col1[2];
    inv[2] = col0[0]*col1[1]-col0[1]*col1[0];
    out[2] = (inv[0]*rh[0]+inv[1]*rh[1]+inv[2]*rh[2])/det;

    return det;
}


void abstractMixedGraph::FacetIdentification(abstractMixedGraph& G)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || G.IsDense())
        Error(ERR_REJECTED,"FacetIdentification","Cannot merge dense graphs");

    if (!Representation()) NoRepresentation("FacetIdentification");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    TNode nG = G.N();
    TArc  mG = G.M();

    TArc aExtG = G.ExteriorArc();

    #if defined(_FAILSAVE_)

    if (aExtG>=2*mG)
        Error(ERR_REJECTED,"FacetIdentification","Missing exterior arc");

    #endif

    // Determine the length of the exterior cycle of G and its center point
    TArc aG = G.Left(aExtG);
    TNode vG = G.StartNode(aG);
    TArc exteriorLength = 1;
    vector<TFloat> centerG(3);
    vector<TFloat> perpendicularG(3);
    TDim i = 0;
    for (;i<3;++i) centerG[i] = G.C(vG,i);

    while (aG!=(aExtG^1))
    {
        aG = G.Left(aG^1);
        vG = G.StartNode(aG);
        ++exteriorLength;
        for (i=0;i<3;++i) centerG[i] += G.C(vG,i);

        if (exteriorLength>nG)
            Error(ERR_REJECTED,"FacetIdentification","No exterior face found");
    }

    vector<TFloat> unitXG(3);
    vector<TFloat> unitYG(3);
    vector<TFloat> unitZG(3);

    // Now, vG is the EndNode(aExtG)
    TNode wG = G.StartNode(aExtG);

    for (i=0;i<3;++i)
    {
        centerG[i] /= exteriorLength;
        unitXG[i] = G.C(vG,i)-centerG[i];
        unitYG[i] = G.C(wG,i)-G.C(vG,i);
    }

    TFloat lengthG = normalizeVector(unitXG);
    vectorProduct(unitXG,unitYG,unitZG);
    normalizeVector(unitZG);
    vectorProduct(unitZG,unitXG,unitYG);
    normalizeVector(unitYG);


    TNode n0 = N();
    TArc m0 = M();
    TDim dim = (Dim()<3) ? Dim() : 3;


    // First pass: Determine the number of facets to be split
    bool *visited = new bool[2*m0];
    TNode nCopies = 0;
    TArc a = 0;

    for (;a<2*m;++a) visited[a] = false;

    for (TArc a0=0;a0<2*m0;++a0)
    {
        if (visited[a0]) continue;

        // Determine the length of the cycle enclosing the facet of a0
        a = Right(a0^1,EndNode(a0));
        TArc facetLength = 1;
        visited[a0] = true;

        while (a!=a0)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
            ++facetLength;
        }

        if (facetLength!=exteriorLength) continue;

        ++nCopies;
    }

    // Forecast the final dimensions, for sake of performance
    TArc nFinal = TArc(n0)+(nG-exteriorLength)*nCopies;
    TArc mFinal = m0+(mG-exteriorLength)*nCopies;
    X -> SetCapacity(nFinal,mFinal);

    // To obtain the node incidence ordering, initialize with the original ordering
    TArc *right = new TArc[2*mFinal];
    for (a=0;a<2*m0;++a) right[a] = Right(a,StartNode(a));
    for (a=2*m0;a<2*mFinal;++a) right[a] = NoArc;

    // Second pass: Insert G into the appropriate faces
    for (a=0;a<2*m;++a) visited[a] = false;

    // Do not subdivide the exterior face. If no exterior face is defined,
    // this indicates a 3D embedding (a solid representation)
    if (ExteriorArc()!=NoArc)
    {
        // Mark the exterior arcs as visited
        TArc aExt = ExteriorArc();
        visited[aExt] = true;
        a = Right(aExt^1,EndNode(aExt));

        while (a!=aExt)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
        }
    }

    TNode* mapNode = new TNode[nG];
    TArc* mapArc = new TArc[2*mG];

    for (TArc a0=0;a0<2*m0;++a0)
    {
        if (visited[a0]) continue;

        // Determine the length of the cycle enclosing the facet of a0
        a = Right(a0^1,EndNode(a0));
        TArc facetLength = 1;
        visited[a0] = true;

        while (a!=a0)
        {
            visited[a] = true;
            a = Right(a^1,EndNode(a));
            ++facetLength;
        }

        if (facetLength!=exteriorLength) continue;

        // So, G has to to be inserted here
        TNode vG = 0;

        for (;vG<nG;++vG) mapNode[vG] = NoNode;
        for (a=0;a<2*mG;++a) mapArc[a] = NoArc;

        // First, map the contact nodes and arcs, and determine the center point

        TNode v = EndNode(a0);
        a = Right(a0^1,v);
        aG = G.Left(aExtG);
        mapNode[G.StartNode(aG)] = v;
        mapArc[aG] = a;
        mapArc[aG^1] = a^1;
        vector<TFloat> center(3);

        for (i=0;i<3;++i) center[i] = C(v,i);

        while (a!=a0)
        {
            v = EndNode(a);
            a = Right(a^1,v);
            aG = G.Left(aG^1);
            mapNode[G.StartNode(aG)] = v;
            mapArc[aG] = a;
            mapArc[aG^1] = a^1;

            for (i=0;i<3;++i) center[i] += C(v,i);
        }

        vector<TFloat> unitX(3);
        vector<TFloat> unitY(3);
        vector<TFloat> unitZ(3);

        // v is still the StartNode(a0)
        TNode w = EndNode(a0);

        for (i=0;i<3;++i)
        {
            center[i] /= facetLength;
            unitX[i] = C(v,i)-center[i];
            unitY[i] = C(w,i)-C(v,i);
        }

        TFloat length = normalizeVector(unitX);
        vectorProduct(unitX,unitY,unitZ);
        normalizeVector(unitZ);
        vectorProduct(unitZ,unitX,unitY);
        normalizeVector(unitY);

        // Map the remaining nodes of G
        for (vG=0;vG<nG;++vG)
        {
            if (mapNode[vG]!=NoNode) continue;

            v = mapNode[vG] = InsertNode();

            if (dim>1 && G.Dim()>1)
            {
                vector<TFloat> in(3);
                vector<TFloat> out(3);

                for (i=0;i<dim;++i) in[i] = G.C(vG,i)-centerG[i];

                solveLEQ3(in,unitXG,unitYG,unitZG,out);

                for (i=0;i<dim;++i)
                {
                    SetC(v,i,center[i]+length/lengthG*(unitX[i]*out[0]+unitY[i]*out[1]-unitZ[i]*out[2]));
                }
            }
        }

        // Map the edges of G, but omit the arcs on the exterior face
        MarkExteriorFace(a0);

        for (aG=0;aG<mG;++aG)
        {
            if (mapArc[2*aG]<2*m0) continue;

            TNode uG = G.StartNode(2*aG);
            vG = G.EndNode(2*aG);
            TArc aMap = InsertArc(mapNode[uG],mapNode[vG],G.UCap(2*aG),G.Length(2*aG),G.LCap(2*aG));
            mapArc[2*aG] = 2*aMap;
            mapArc[2*aG+1] = 2*aMap+1;
        }

        // Set the right-hand node indences, but only for the interior arcs of G
        for (aG=0;aG<mG;++aG)
        {
            TArc aMap = mapArc[2*aG];

            if (aMap<2*m0) continue;

            right[aMap] = mapArc[G.Right(2*aG,G.StartNode(2*aG))];
            right[aMap^1] = mapArc[G.Right(2*aG+1,G.StartNode(2*aG+1))];
        }

        // Set the right-hand node indences for the exterior arcs
        aG = G.Left(aExtG);
        right[mapArc[aG^1]] = mapArc[G.Right(aG^1,G.StartNode(aG^1))];

        while (aG!=(aExtG^1))
        {
            aG = G.Left(aG^1);
            right[mapArc[aG^1]] = mapArc[G.Right(aG^1,G.StartNode(aG^1))];
        }
    }

    X -> ReorderIncidences(right,true);

    delete[] right;
    delete[] visited;
    delete[] mapNode;
    delete[] mapArc;
}


size_t abstractMixedGraph::SizeInfo(TArrayDim arrayDim,TSizeInfo) const throw()
{
    switch (arrayDim)
    {
        case DIM_GRAPH_NODES:
        {
            return size_t(n);
        }
        case DIM_GRAPH_ARCS:
        {
            return size_t(m);
        }
        case DIM_ARCS_TWICE:
        {
            return size_t(2*m);
        }
        case DIM_LAYOUT_NODES:
        {
            return size_t(n+ni);
        }
        case DIM_SINGLETON:
        {
            return size_t(1);
        }
        default:
        {
            return size_t(0);
        }
    }
}


graphRepresentation* abstractMixedGraph::Representation() throw()
{
    return NULL;
}


const graphRepresentation* abstractMixedGraph::Representation() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::RepresentationalData() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::Geometry() const throw()
{
    return NULL;
}


attributePool* abstractMixedGraph::LayoutData() const throw()
{
    return NULL;
}


abstractMixedGraph::TMetricType abstractMixedGraph::MetricType() const throw()
{
    attributePool* geometry = Geometry();

    if (!geometry) return METRIC_DISABLED;

    return abstractMixedGraph::TMetricType(geometry->GetValue<int>(TokGeoMetric,0,int(METRIC_DISABLED)));
}


void abstractMixedGraph::SetSourceNode(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n && s!=NoNode) NoSuchNode("SetSourceNode",s);

    #endif

    defaultSourceNode = s;
}


void abstractMixedGraph::SetTargetNode(TNode t) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (t>=n && t!=NoNode) NoSuchNode("SetTargetNode",t);

    #endif

    defaultTargetNode = t;
}


void abstractMixedGraph::SetRootNode(TNode r) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (r>=n && r!=NoNode) NoSuchNode("SetRootNode",r);

    #endif

    defaultRootNode = r;
}


TNode abstractMixedGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return StartNode(a^1);
}


TArc abstractMixedGraph::Left(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Left",a);

    #endif

    TNode v = StartNode(a);
    TArc a2 = a;

    while (Right(a2,v)!=a) a2 = Right(a2,v);

    return a2;
}


TCap abstractMixedGraph::Demand(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprDemand,v,X->defaultDemand);
}


TCap abstractMixedGraph::MaxDemand() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().MaxValue<TCap>(TokReprDemand,X->defaultDemand);
}


bool abstractMixedGraph::CDemand() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprDemand);
}


TCap abstractMixedGraph::UCap(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprUCap,a>>1,X->defaultUCap);
}


TCap abstractMixedGraph::MaxUCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TCap max = 0;

        for (TArc a=0;a<m;a++)
            if (LCap(2*a)>max) max = UCap(2*a);

        return max;
    }

    return X->RepresentationalData().MaxValue<TCap>(TokReprUCap,X->defaultUCap);
}


bool abstractMixedGraph::CUCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprUCap);
}


TCap abstractMixedGraph::LCap(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->RepresentationalData().GetValue<TCap>(TokReprLCap,a>>1,X->defaultLCap);
}


TCap abstractMixedGraph::MaxLCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TCap max = 0;

        for (TArc a=0;a<m;a++)
            if (LCap(2*a)>max) max = LCap(2*a);

        return max;
    }

    return X->RepresentationalData().MaxValue<TCap>(TokReprLCap,X->defaultLCap);
}


bool abstractMixedGraph::CLCap() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TCap>(TokReprLCap);
}


TFloat abstractMixedGraph::Length(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 1;

    return X->Length(a);
}


TFloat abstractMixedGraph::MaxLength() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        TFloat max = 0;

        for (TArc a=0;a<m;a++)
        {
            TFloat thisLength = Length(2*a);

            if (thisLength>max) max = thisLength;
            if (-thisLength>max) max = -thisLength;
        }

        return max;
    }

    return X->MaxLength();
}


bool abstractMixedGraph::CLength() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<TFloat>(TokReprLength);
}


char abstractMixedGraph::Orientation(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        return IsDirected() ? 1 : 0;
    }

    return X->RepresentationalData().GetValue<char>(TokReprOrientation,a>>1,X->defaultOrientation);
}


char abstractMixedGraph::Orientation() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X)
    {
        return IsDirected() ? 1 : 0;
    }

    return X->RepresentationalData().DefaultValue<char>(TokReprOrientation,X->defaultOrientation);
}


bool abstractMixedGraph::COrientation() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return true;

    return X->RepresentationalData().IsConstant<char>(TokReprOrientation);
}


TFloat abstractMixedGraph::C(TNode v,TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().GetValue<TFloat>(TokGeoAxis0+i,v,X->defaultC);
}


void abstractMixedGraph::SetC(TNode v,TDim i,TFloat pos) throw(ERRange,ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("SetC");

    #endif

    X -> SetC(v,i,pos);
}


TFloat abstractMixedGraph::CMin(TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().MinValue<TFloat>(TokGeoAxis0+i,X->defaultC);
}


TFloat abstractMixedGraph::CMax(TDim i) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Geometry().MaxValue<TFloat>(TokGeoAxis0+i,X->defaultC);
}


TDim abstractMixedGraph::Dim() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X) return 0;

    return X->Dim();
}


bool abstractMixedGraph::Blocking(TArc a)
    const throw(ERRange)
{
    if (Orientation(a)==0) return false;

    if (a&1) return true;

    return false;
}


TFloat abstractMixedGraph::RedLength(const TFloat* potential,TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("RedLength",a);

    #endif

    if (Orientation(a)==0)
    {
        if (!potential) return Length(a);

        return Length(a)+potential[StartNode(a)]+potential[EndNode(a)];
    }

    TFloat thisLength = ((a&1) ? -Length(a) : Length(a));

    if (!potential) return thisLength;

    return thisLength+potential[StartNode(a)]-potential[EndNode(a)];
}


TNode abstractMixedGraph::ArcLabelAnchor(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return NoNode;

    return static_cast<const sparseRepresentation*>(X) -> ArcLabelAnchor(a);
}


bool abstractMixedGraph::NoArcLabelAnchors() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return true;

    return static_cast<const sparseRepresentation*>(X) -> NoArcLabelAnchors();
}


TNode abstractMixedGraph::ThreadSuccessor(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return NoNode;

    return static_cast<const sparseRepresentation*>(X) -> ThreadSuccessor(v);
}


bool abstractMixedGraph::NoThreadSuccessors() const throw()
{
    const graphRepresentation* X = Representation();

    if (!X || !IsSparse()) return true;

    return static_cast<const sparseRepresentation*>(X) -> NoThreadSuccessors();
}


bool abstractMixedGraph::HiddenNode(TNode v) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (fabs(C(v,0))>=InfFloat) return true;

    return (X && X->HiddenNode(v));
}


bool abstractMixedGraph::HiddenArc(TArc a) const throw(ERRange)
{
    const graphRepresentation* X = Representation();

    return (X && X->HiddenArc(a));
}


void abstractMixedGraph::SetNodeVisibility(TNode v,bool visible) throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X || visible) return;

    SetC(v,TDim(0),-InfFloat);
}


void abstractMixedGraph::SetArcVisibility(TArc a,bool visible) throw(ERRange)
{
    const graphRepresentation* X = Representation();

    if (!X) return;
}


TArc abstractMixedGraph::Adjacency(TNode u,TNode v,TMethAdjacency method) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Adjacency",u);

    if (v>=n) NoSuchNode("Adjacency",v);

    #endif

    TArc a = NoArc;

    if (adj==NULL)
    {
        if (method==ADJ_MATRIX)
        {
            if (m>0)
            {
                adj = new goblinHashTable<TArc,TArc>(n*n,2*m,NoArc,CT);

                for (TArc a1=0;a1<2*m;a1++)
                {
                    TNode u0 = StartNode(a1);
                    TNode v0 = EndNode(a1);
                    TArc a0 = u0*n+v0;
                    TArc a2 = adj->Key(a0);

                    // Check if a1 is preferable before a2

                    if (a2==NoArc || (Blocking(a2) && !Blocking(a1)))
                    {
                        adj -> ChangeKey(a0,a1);
                        continue;
                    }

                    if (Blocking(a1) && !Blocking(a2)) continue;

                    if (a1<a2) adj -> ChangeKey(a0,a1);
                }

                a = adj->Key(u*n+v);
            }
        }
        else // (method==ADJ_SEARCH)
        {
            TArc a0 = First(u);

            if (a0!=NoArc && EndNode(a0)==v) a = a0;

            while (a0!=NoArc && Right(a0,u)!=First(u))
            {
                a0 = Right(a0,u);

                if (EndNode(a0)!=v) continue;

                if (a==NoArc || (Blocking(a) && !Blocking(a0)))
                {
                    a = a0;
                    continue;
                }

                if (Blocking(a0) && !Blocking(a)) continue;

                if (a0<a) a = a0;
            }

        }
    }
    else a = adj->Key(u*n+v);

    if (a==NoArc)
    {
        #if defined(_LOGGING_)

        if (CT.logWarn>1)
        {
            sprintf(CT.logBuffer,"Nodes are non-adjacent: %lu, %lu",
                static_cast<unsigned long>(u),static_cast<unsigned long>(v));
            Error(MSG_WARN,"Adjacency",CT.logBuffer);
        }

        #endif

        return NoArc;
    }
    else
    {
        #if defined(_LOGGING_)

        if (CT.logRes>2)
        {
            sprintf(CT.logBuffer,
                "The nodes %lu and %lu are adjacent by the arc %lu",
                static_cast<unsigned long>(u),
                static_cast<unsigned long>(v),
                static_cast<unsigned long>(a));
            LogEntry(LOG_RES2,CT.logBuffer);
        }

        #endif

        return a;
    }
}


void abstractMixedGraph::MarkAdjacency(TNode u,TNode v,TArc a)
    throw(ERRange,ERRejected)
{
    if (adj==NULL) return;

    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("MarkAdjacency",u);

    if (v>=n) NoSuchNode("MarkAdjacency",v);

    if (a>=2*m && a!=NoArc) NoSuchArc("MarkAdjacency",a);

    if (a!=NoArc && (StartNode(a)!=u || EndNode(a)!=v || Blocking(a)))
        Error(ERR_REJECTED,"MarkAdjacency","Mismatching end nodes");

    #endif

    TArc a0 = u*n+v;
    adj -> ChangeKey(a0,a);
}


TCap abstractMixedGraph::Cardinality() const throw()
{
    TCap sum = 0;

    for (TArc a=0;a<m;a++) sum += fabs(Sub(2*a));

    return sum;
}


TFloat abstractMixedGraph::Weight() const throw()
{
    TFloat sum = 0;

    for (TArc a=0;a<m;a++) sum += fabs(Sub(2*a))*Length(2*a);

    return sum;
}


TFloat abstractMixedGraph::Length() const throw()
{
    TArc* pred = GetPredecessors();

    if (!pred) return 0;

    TFloat sum = 0;

    for (TNode v=0;v<n;++v)
        if (pred[v]!=NoArc) sum += Length(pred[v]);

    return sum;
}


investigator *abstractMixedGraph::NewInvestigator() const throw()
{
    return new iGraph(*this);
}


investigator &abstractMixedGraph::Investigator(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Investigator",H);

    #endif

    return *pInvestigator[H];
}


THandle abstractMixedGraph::Investigate() const throw(ERRejected)
{
    if (LH!=NoHandle)
    {
        THandle tmpHandle = LH;
        LH = NoHandle;
        pInvestigator[tmpHandle] -> Reset();
        return tmpHandle;
    }

    if (RH!=NoHandle)
    {
        THandle tmpHandle = RH;
        RH = NoHandle;
        pInvestigator[tmpHandle] -> Reset();
        return tmpHandle;
    }

    for (THandle i=0;i<itCounter;++i)
    {
        if (pInvestigator[i]==NULL)
        {
            pInvestigator[i] = NewInvestigator();
            return i;
        }
    }

    if (itCounter==NoHandle)
        Error(ERR_REJECTED,"Investigate","No more handles available");

    pInvestigator = (investigator**)GoblinRealloc(pInvestigator,sizeof(investigator*)*(itCounter+1));
    pInvestigator[itCounter] = NewInvestigator();
    return itCounter++;
}


void abstractMixedGraph::Reset(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Reset",H);

    #endif

    pInvestigator[H]->Reset();
}


void abstractMixedGraph::Reset(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Reset",H);

    #endif

    pInvestigator[H]->Reset(v);
}


TArc abstractMixedGraph::Read(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Read",H);

    #endif

    return pInvestigator[H]->Read(v);
}


TArc abstractMixedGraph::Peek(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Peek",H);

    #endif

    return pInvestigator[H]->Peek(v);
}


bool abstractMixedGraph::Active(THandle H, TNode v) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)||(pInvestigator[H]==NULL)) NoSuchHandle("Active",H);

    #endif

    return pInvestigator[H]->Active(v);
}


void abstractMixedGraph::Close(THandle H) const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if ((H>=itCounter)&&(pInvestigator[H]==NULL)) NoSuchHandle("Close",H);

    if (H==LH || H==RH)
    {
        Error(ERR_REJECTED,"Close","Repeated close of investigator");
    }

    #endif

    if ((LH==NoHandle)||(RH==NoHandle))
    {
        if (LH==NoHandle) LH = H;
        else RH = H;
        LogEntry(LOG_MEM,"Investigator cached");
    }
    else
    {
        delete pInvestigator[H];
        pInvestigator[H] = NULL;
        LogEntry(LOG_MEM,"Investigator disallocated");
    }
}


void abstractMixedGraph::ReleaseInvestigators() const throw()
{
    if (LH!=NoHandle)
    {
        delete pInvestigator[LH];
        pInvestigator[LH] = NULL;
        LH = NoHandle;
    }

    if (RH!=NoHandle)
    {
        delete pInvestigator[RH];
        pInvestigator[RH] = NULL;
        RH = NoHandle;
    }

    for (THandle i=0; i<itCounter; i++)
        if (pInvestigator[i]!=NULL)
        {
            delete pInvestigator[i];
            pInvestigator[i] = NULL;
        }

    delete[] pInvestigator;
    pInvestigator = NULL;
    itCounter = 0;

    LogEntry(LOG_MEM,"Investigator cache cleared");
}


void abstractMixedGraph::StripInvestigators() const throw()
{
    THandle i = itCounter;
    for (;((i>0)&&(pInvestigator[i-1]==NULL));i--) {};

    pInvestigator = (investigator**)GoblinRealloc(pInvestigator,sizeof(investigator*)*i);
    itCounter = i;
}


void abstractMixedGraph::Bud(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Bud",v);

    #endif

    partition -> Bud(v);
}


void abstractMixedGraph::Merge(TNode u,TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("Merge",u);

    if (v>=n) NoSuchNode("Merge",v);

    #endif

    partition -> Merge(u,v);
}


TNode abstractMixedGraph::Find(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Find",v);

    #endif

    if (partition==NULL) return v;
    else return partition -> Find(v);
}


TFloat abstractMixedGraph::Dist(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Dist",v);

    #endif

    TFloat* dist = GetDistanceLabels();

    if (!dist) return InfFloat;

    return dist[v];
}


TFloat abstractMixedGraph::Pi(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Pi",v);

    #endif

    TFloat* potential = GetPotentials();

    if (!potential) return 0;

    else return potential[v];
}


TNode abstractMixedGraph::NodeColour(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("NodeColour",v);

    #endif

    TNode* nodeColour = GetNodeColours();

    if (!nodeColour) return NoNode;

    return nodeColour[v];
}


TArc abstractMixedGraph::EdgeColour(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EdgeColour",a);

    #endif

    TArc* edgeColour = GetEdgeColours();

    if (!edgeColour) return NoArc;

    return edgeColour[a>>1];
}


TArc abstractMixedGraph::Pred(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Pred",v);

    #endif

    TArc* pred = GetPredecessors();

    if (!pred) return NoArc;

    return pred[v];
}


TArc abstractMixedGraph::ExteriorArc() const throw()
{
    if (!LayoutData()) return NoArc;

    return LayoutData() -> GetValue<TArc>(TokLayoutExteriorArc,0,NoArc);
}


void abstractMixedGraph::SetExteriorArc(TArc exteriorArc) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (exteriorArc>=2*m && exteriorArc!=NoArc) NoSuchArc("SetExteriorArc",exteriorArc);

    if (!LayoutData())
        Error(ERR_REJECTED,"SetExteriorArc","No layout data pool found");

    #endif

    if (exteriorArc==NoArc)
    {
        LayoutData() -> ReleaseAttribute(TokLayoutExteriorArc);
    }
    else
    {
        LayoutData() -> InitAttribute(*this,TokLayoutExteriorArc,exteriorArc);
    }
}


TNode abstractMixedGraph::Face(TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Face",a);

    #endif

    if (!face) ExtractEmbedding();

    if (!face) return NoNode;

    return face[a];
}


TIndex abstractMixedGraph::OriginalOfNode(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("OriginalOfNode",v);

    #endif

    TIndex* originalNode = registers.GetArray<TIndex>(TokRegOriginalNode);

    if (!originalNode) return NoIndex;

    return originalNode[v];
}


TIndex abstractMixedGraph::OriginalOfArc(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("OriginalOfArc",a);

    #endif

    TIndex* originalArc = registers.GetArray<TIndex>(TokRegOriginalArc);

    if (!originalArc) return NoIndex;

    return originalArc[a>>1]^(a&1);
}


bool abstractMixedGraph::ExteriorNode(TNode v,TNode thisFace)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("ExteriorNode",v);

    #endif

    TArc exteriorArc = ExteriorArc();

    if (!face || (exteriorArc==NoNode && thisFace==NoNode)) return false;

    if (thisFace!=NoNode) return (face[First(v)^1]==thisFace);

    return (face[First(v)^1]==face[exteriorArc]);
}


void abstractMixedGraph::SetSub(TArc a,TFloat multiplicity)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("SetSub",a);

    if (fabs(multiplicity)>UCap(a) || fabs(multiplicity)<LCap(a))
         AmountOutOfRange("SetSub",multiplicity);

    #endif

    SetSubRelative(a,multiplicity-Sub(a));
}


void abstractMixedGraph::SetSubRelative(TArc a,TFloat lambda) throw(ERRange)
{
    a ^= (a&1);

    if (lambda<0)
    {
        Push(a^1,-lambda);
    }
    else
    {
        Push(a,lambda);
    }
}


void abstractMixedGraph::Push(TArc a,TFloat lambda) throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"Push","Not implemented");
}


TFloat abstractMixedGraph::ResCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("ResCap",a);

    #endif

    if (a&1)
    {
        return Sub(a)-LCap(a);
    }
    else if (UCap(a)<InfCap)
    {
        return UCap(a)-Sub(a);
    }
    else if (Sub(a)<InfCap)
    {
        return InfCap-Sub(a);
    }
    else
    {
        return 0;
    }
}


void abstractMixedGraph::SetDist(TNode v,TFloat thisDist) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetDist",v);

    #endif

    TFloat* dist = GetDistanceLabels();

    if (!dist)
    {
        if (thisDist!=InfFloat)
        {
            dist = InitDistanceLabels();
            dist[v] = thisDist;
        }
    }
    else dist[v] = thisDist;
}


void abstractMixedGraph::SetPotential(TNode v,TFloat thisPi) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetPotential",v);

    #endif

    TFloat* potential = GetPotentials();

    if (!potential)
    {
        if (thisPi!=0)
        {
            potential = InitPotentials();
            potential[v] = thisPi;
        }
    }
    else potential[v] = thisPi;
}


void abstractMixedGraph::SetNodeColour(TNode v,TNode thisColour) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetNodeColour",v);

    if (thisColour>=n && thisColour!=NoNode)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %lu",
            static_cast<unsigned long>(thisColour));
        Error(MSG_WARN,"SetNodeColour",CT.logBuffer);
    }

    #endif

    TNode* nodeColour = GetNodeColours();

    if (!nodeColour)
    {
        if (thisColour!=NoNode)
        {
            nodeColour = InitNodeColours();
            nodeColour[v] = thisColour;
        }
    }
    else nodeColour[v] = thisColour;
}


void abstractMixedGraph::SetEdgeColour(TArc a,TArc thisColour) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("SetEdgeColour",a);

    if (thisColour>=m && thisColour!=NoArc)
    {
        sprintf(CT.logBuffer,"Illegal assignment: %lu",
            static_cast<unsigned long>(thisColour));
        Error(MSG_WARN,"SetEdgeColour",CT.logBuffer);
    }

    #endif

    TArc* edgeColour = GetEdgeColours();

    if (!edgeColour)
    {
        if (thisColour!=NoArc)
        {
            edgeColour = InitEdgeColours();
            edgeColour[a>>1] = thisColour;
        }
    }
    else edgeColour[a>>1] = thisColour;
}


void abstractMixedGraph::SetPred(TNode v,TArc thisPred)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("SetPred",v);

    if (thisPred>=2*m && thisPred!=NoArc) NoSuchArc("SetPred",thisPred);

    if (thisPred!=NoArc && EndNode(thisPred)!=v)
        Error(ERR_REJECTED,"SetPred","Mismatching end node");

    #endif

    TArc* pred = GetPredecessors();

    if (!pred)
    {
        if (thisPred!=NoArc)
        {
            pred = InitPredecessors();
            pred[v] = thisPred;
        }
    }
    else pred[v] = thisPred;
}


void abstractMixedGraph::PushPotential(TNode v,TFloat epsilon) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("PushPotential",v);

    #endif

    TFloat* potential = RawPotentials();

    potential[v] += epsilon;
}


TFloat abstractMixedGraph::Deg(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Deg",v);

    #endif

    if (!sDeg) InitDegrees();

    return sDeg[v];
}


TFloat abstractMixedGraph::DegIn(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DegIn",v);

    #endif

    if (!sDegIn) InitDegInOut();

    return sDegIn[v];
}


TFloat abstractMixedGraph::DegOut(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("DegOut",v);

    #endif

    if (!sDegOut) InitDegInOut();

    return sDegOut[v];
}


void abstractMixedGraph::AdjustDegrees(TArc a,TFloat lambda) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("AdjustDegrees",a);

    #endif

    TNode u = StartNode(a);
    TNode v = EndNode(a);

    if (sDeg && !Blocking(a) && !Blocking(a^1))
    {
        sDeg[u] += lambda;
        sDeg[v] += lambda;
    }
    else
    {
        if (sDegIn)
        {
            if (!Blocking(a^1)) sDegIn[u] += lambda;
            else sDegOut[u] += lambda;

            if (!Blocking(a)) sDegIn[v] += lambda;
            else sDegOut[v] += lambda;
        }
    }
}


void abstractMixedGraph::InitDegrees() throw()
{
    if (sDeg==NULL)
    {
        LogEntry(LOG_MEM,"Generating degree labels...");
        sDeg = new TFloat[n];

        THandle H = Investigate();
        investigator &I = Investigator(H);

        for (TNode u=0;u<n;u++)
        {
            TFloat thisDeg = 0;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                if (!Blocking(a) && !Blocking(a^1)) thisDeg += Sub(a);
            }

            sDeg[u] = thisDeg;
        }

        Close(H);
    }
}


void abstractMixedGraph::InitDegInOut() throw()
{
    if (!sDegIn)
    {
        sDegIn =  new TFloat[n];
        sDegOut =  new TFloat[n];
        LogEntry(LOG_MEM,"Generating IO degree labels...");

        THandle H = Investigate();
        investigator &I = Investigator(H);

        for (TNode v=0;v<n;++v)
        {
            TFloat thisDegIn = 0;
            TFloat thisDegOut = 0;

            while (I.Active(v))
            {
                TArc a = I.Read(v);
                if (Blocking(a)) thisDegIn += Sub(a);
                if (Blocking(a^1)) thisDegOut += Sub(a);
            }

            sDegIn[v] = thisDegIn;
            sDegOut[v] = thisDegOut;
        }

        Close(H);
    }
}


TFloat* abstractMixedGraph::InitDistanceLabels(TFloat def) throw()
{
    return registers.InitArray<TFloat>(*this,TokRegLabel,def);
}


TFloat* abstractMixedGraph::InitPotentials(TFloat def) throw()
{
    return registers.InitArray<TFloat>(*this,TokRegPotential,def);
}


TNode* abstractMixedGraph::InitNodeColours(TNode def) throw()
{
    return registers.InitArray<TNode>(*this,TokRegNodeColour,def);
}


TArc* abstractMixedGraph::InitEdgeColours(TArc def) throw()
{
    return registers.InitArray<TArc>(*this,TokRegEdgeColour,def);
}


TArc* abstractMixedGraph::InitPredecessors() throw()
{
    return registers.InitArray<TArc>(*this,TokRegPredecessor,NoArc);
}


void abstractMixedGraph::UpdatePotentials(TFloat cutValue) throw(ERRejected)
{
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    #if defined(_FAILSAVE_)

    if (!dist)
        Error(ERR_REJECTED,"UpdatePotentials","No distance labels present");

    #endif

    if (potential)
    {
        for (TNode i=0;i<n;++i)
        {
            if (dist[i]<cutValue)
            {
                potential[i] += dist[i];
            }
            else potential[i] += cutValue;
        }
    }
    else
    {
        potential = RawPotentials();

        for (TNode i=0;i<n;++i)
        {
            if (dist[i]<cutValue)
            {
                potential[i] = dist[i];
            }
            else potential[i] = cutValue;
        }
    }

    LogEntry(LOG_MEM,"...Node potentials updated");
}


void abstractMixedGraph::InitPartition() throw()
{
    if (!partition)
    {
        partition = new disjointFamily<TNode>(n,CT);
        LogEntry(LOG_MEM,"...Partition allocated");
    }
    else
    {
        partition -> Init();

        #if defined(_LOGGING_)

        Error(MSG_WARN,"InitPartition","A partition is already present");

        #endif
    }
}


void abstractMixedGraph::InitSubgraph() throw()
{
    ReleaseDegrees();

    for (TArc a=0;a<m;a++) SetSub(2*a,LCap(2*a));
}


TFloat* abstractMixedGraph::GetDistanceLabels() const throw()
{
    return registers.GetArray<TFloat>(TokRegLabel);
}


TFloat* abstractMixedGraph::GetPotentials() const throw()
{
    return registers.GetArray<TFloat>(TokRegPotential);
}


TNode* abstractMixedGraph::GetNodeColours() const throw()
{
    return registers.GetArray<TNode>(TokRegNodeColour);
}


TArc* abstractMixedGraph::GetEdgeColours() const throw()
{
    return registers.GetArray<TArc>(TokRegEdgeColour);
}


TArc* abstractMixedGraph::GetPredecessors() const throw()
{
    return registers.GetArray<TArc>(TokRegPredecessor);
}


TFloat* abstractMixedGraph::RawDistanceLabels() throw()
{
    return registers.RawArray<TFloat>(*this,TokRegLabel);
}


TFloat* abstractMixedGraph::RawPotentials() throw()
{
    TFloat* potential = GetPotentials();

    if (potential) return potential;

    return InitPotentials();
}


TNode* abstractMixedGraph::RawNodeColours() throw()
{
    return registers.RawArray<TNode>(*this,TokRegNodeColour);
}


TArc* abstractMixedGraph::RawPredecessors() throw()
{
    return registers.RawArray<TArc>(*this,TokRegPredecessor);
}


TArc* abstractMixedGraph::RawEdgeColours() throw()
{
    return registers.RawArray<TArc>(*this,TokRegEdgeColour);
}


TNode* abstractMixedGraph::RandomNodeOrder() throw()
{
    TNode* nodeColour = registers.RawArray<TNode>(*this,TokRegNodeColour);
    TNode u = 0;

    for (;u<n;++u) nodeColour[u] = u;

    for (u=0;u<n-2;++u)
    {
        TNode v = u+1+TNode(CT.Rand(n-u-1));
        TNode w = nodeColour[u];
        nodeColour[u] = nodeColour[v];
        nodeColour[v] = w;
    }

    return nodeColour;
}


void abstractMixedGraph::ReleaseAdjacencies() throw()
{
    if (adj)
    {
        delete adj;
        adj = NULL;
        LogEntry(LOG_MEM,"...Adjacencies disallocated");
    }
}


void abstractMixedGraph::ReleaseDegrees() throw()
{
    if (sDegIn)
    {
        delete[] sDegIn;
        sDegIn = NULL;
        LogEntry(LOG_MEM,"...Indegree labels disallocated");
    }

    if (sDegOut)
    {
        delete[] sDegOut;
        sDegOut = NULL;
        LogEntry(LOG_MEM,"...Outdegree labels disallocated");
    }

    if (sDeg)
    {
        delete[] sDeg;
        sDeg = NULL;
        LogEntry(LOG_MEM,"...Degree labels disallocated");
    }
}


void abstractMixedGraph::ReleaseLabels() throw()
{
    registers.ReleaseAttribute(TokRegLabel);
}


void abstractMixedGraph::ReleasePotentials() throw()
{
    registers.ReleaseAttribute(TokRegPotential);
}


void abstractMixedGraph::ReleaseNodeColours() throw()
{
    registers.ReleaseAttribute(TokRegNodeColour);
}


void abstractMixedGraph::ReleaseEdgeColours() throw()
{
    registers.ReleaseAttribute(TokRegEdgeColour);
}


void abstractMixedGraph::ReleasePredecessors() throw()
{
    registers.ReleaseAttribute(TokRegPredecessor);
}


void abstractMixedGraph::ReleaseEmbedding() throw()
{
    if (face)
    {
        delete[] face;
        face = NULL;
        LogEntry(LOG_MEM,"...Dual incidences disallocated");
    }
}


void abstractMixedGraph::ReleasePartition() throw()
{
    if (partition)
    {
        delete partition;
        partition = NULL;
        LogEntry(LOG_MEM,"...Partition disallocated");
    }
}


void abstractMixedGraph::ReleaseNodeMapping() throw()
{
    registers.ReleaseAttribute(TokRegOriginalNode);
}


void abstractMixedGraph::ReleaseArcMapping() throw()
{
    registers.ReleaseAttribute(TokRegOriginalArc);
}


void abstractMixedGraph::Write(const char* fileName) const throw(ERFile)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    char* className = "mixed";

    if (IsBalanced())
    {
        className = "balanced_fnw";
    }
    else if (IsBipartite())
    {
        if (IsDense())
        {
            className = "dense_bigraph";
        }
        else
        {
            className = "bigraph";
        }
    }
    else if (IsDirected())
    {
        if (IsDense())
        {
            className = "dense_digraph";
        }
        else
        {
            className = "digraph";
        }
    }
    else if (IsUndirected())
    {
        if (IsDense())
        {
            className = "dense_graph";
        }
        else
        {
            className = "graph";
        }
    }

    sprintf(CT.logBuffer,"Writing \"%s\" object to \"%s\"...",className,fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    goblinExport F(fileName,CT);

    F.StartTuple(className,0);

    F.StartTuple("definition",0);

    F.StartTuple("nodes",1);
    F.MakeItem(n,0);

    TNode n1 = n;

    if (IsBipartite())
    {
        const abstractBiGraph* G = static_cast<const abstractBiGraph*>(this);
        n1 = G->N1();
    }

    F.MakeItem(n1,0);
    F.MakeItem(ni,0);
    F.EndTuple();

    if (!IsDense())
    {
        F.StartTuple("arcs",1);
        F.MakeItem(m,0);
        F.EndTuple();

        WriteIncidences(&F);
    }

    WriteUCap(&F);
    WriteLCap(&F);
    WriteLength(&F);
    WriteDemand(&F);
    WriteOrientation(&F);

    F.EndTuple();   // definition

    WriteGeometry(&F);
    WriteLayout(&F);
    WriteRegisters(&F);

    CT.sourceNodeInFile = DefaultSourceNode();
    CT.targetNodeInFile = DefaultTargetNode();
    CT.rootNodeInFile   = DefaultRootNode();
    F.WriteConfiguration(CT);

    F.EndTuple();   // class name

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void abstractMixedGraph::WriteSpecial(goblinExport& F,const attributePool& pool,TPoolEnum token) const throw()
{
    if (&pool==&registers)
    {
        if (token==TokRegSubgraph)
        {
            WriteSubgraph(F);
        }
    }
}


void abstractMixedGraph::WriteIncidences(goblinExport* F) const throw()
{
    F -> StartTuple("incidences",0);

    int mLength = CT.ExternalIntLength(2*m-1);

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;++v)
    {
        F -> StartTuple(v,10);

        while (I.Active(v))
        {
            TArc a = I.Read(v);
            F -> MakeItem(a,mLength);
        }

        F -> EndTuple();
    }

    Close(H);

    F -> EndTuple();
}


void abstractMixedGraph::WriteUCap(goblinExport* F) const throw()
{
    if (CUCap())
    {
        F -> StartTuple("ucap",1);
        if (MaxUCap()==InfCap) F -> MakeNoItem(0);
        else F -> MakeItem((TFloat)MaxUCap(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxUCap());
        F -> StartTuple("ucap",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)UCap(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteLCap(goblinExport* F) const throw()
{
    if (CLCap())
    {
        F -> StartTuple("lcap",1);
        F -> MakeItem((TFloat)MaxLCap(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxLCap());
        F -> StartTuple("lcap",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)LCap(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteLength(goblinExport* F) const throw()
{
    if (CLength())
    {
        F -> StartTuple("length",1);
        F -> MakeItem(MaxLength(),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxLength());
        F -> StartTuple("length",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem((TFloat)Length(2*a),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteDemand(goblinExport* F) const throw()
{
    if (CDemand())
    {
        F -> StartTuple("demand",1);
        F -> MakeItem(Demand(0),0);
    }
    else
    {
        int lLength = CT.ExternalLength<TCap>(MaxDemand());
        F -> StartTuple("demand",10);

        for (TNode v=0;v<n;++v)
            F -> MakeItem(Demand(v),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteOrientation(goblinExport* F) const throw()
{
    if (COrientation())
    {
        if (Orientation()==0) return;

        F -> StartTuple("directed",1);
        F -> MakeItem(int(Orientation()),0);
    }
    else
    {
        int lLength = 1;
        F -> StartTuple("directed",10);

        for (TArc a=0;a<m;a++)
            F -> MakeItem(int(Orientation(2*a)),lLength);
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteGeometry(goblinExport* F) const throw()
{
    char* header = "geometry";
    F -> StartTuple(header,0);

    header = "metrics";
    F -> StartTuple(header,1);
    F -> MakeItem(static_cast<long>(MetricType()),0);
    F -> EndTuple();

    header = "dim";
    F -> StartTuple(header,1);
    F -> MakeItem(int(Dim()),0);
    F -> EndTuple();

    if (Dim()>0)
    {
        header = "coordinates";
        F -> StartTuple(header,0);

        for (TDim i=0;i<Dim();++i)
        {
            int xLength = 0;

            for (TNode v=0;v<n+ni;++v)
            {
                int thisLength = CT.ExternalLength<TFloat>(C(v,i));

                if (thisLength>xLength) xLength = thisLength;
            }

            char strToken[10];
            sprintf(strToken,"axis%lu",static_cast<unsigned long>(i));
            F -> StartTuple(strToken,10);

            for (TNode v=0;v<n+ni;++v) F -> MakeItem(C(v,i),xLength);

            F -> EndTuple();
        }

        F -> EndTuple(); // coordinates
    }

    if (Geometry())
    {
        TNode* pBound = Geometry()->GetArray<TNode>(TokGeoMinBound);

        if (pBound)
        {
            header = "minBound";
            F -> StartTuple(header,1);
            F -> MakeItem(int(*pBound),0);
            F -> EndTuple();
        }

        pBound = Geometry()->GetArray<TNode>(TokGeoMaxBound);

        if (pBound)
        {
            header = "maxBound";
            F -> StartTuple(header,1);
            F -> MakeItem(int(*pBound),0);
            F -> EndTuple();
        }
    }

    F -> EndTuple();
}


void abstractMixedGraph::WriteRegisters(goblinExport* F) const throw()
{
    registers.WritePool(*this,*F,"solutions");
}


void abstractMixedGraph::WriteRegister(goblinExport& F,TPoolEnum token) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (token>=TokRegEndSection)
    {
        sprintf(CT.logBuffer,"No such register: %lu",
            static_cast<unsigned long>(token));
        Error(ERR_RANGE,"WriteRegister",CT.logBuffer);
    }

    #endif

    registers.WriteAttribute(*this,F,token);
}


void abstractMixedGraph::WriteSubgraph(goblinExport& F) const throw()
{
    // The internal representations differs from the other registers.
    // Therefore, a special handling is (temporarily) required.

    bool voidSubgraph = true;

    for (TArc a=0;a<m;a++)
        if (Sub(2*a)>LCap(a)) voidSubgraph = false;

    if (!voidSubgraph)
    {
        F.StartTuple("subgraph",10);

        int length = 1;

        for (TArc a=0;a<m;a++)
        {
            int thisLength = CT.ExternalFloatLength(Sub(2*a));

            if (thisLength>length) length = thisLength;
        }

        for (TArc a=0;a<m;a++) F.MakeItem(Sub(2*a),length);

        F.EndTuple();
    }
}


void abstractMixedGraph::ReadSpecial(goblinImport& F,attributePool& pool,TPoolEnum token) throw(ERParse)
{
    if (&pool==RepresentationalData())
    {
        switch (token)
        {
            case TokReprIncidences:
            {
                graphRepresentation* X = Representation();

                if (X)
                {
                    static_cast<sparseRepresentation*>(X) -> ReadIncidences(F);
                    return;
                }
                else
                {
                    NoSparseRepresentation("ReadSpecial");
                }
            }
            case TokReprNNodes:
            {
                ReadNNodes(F);
                return;
            }
            case TokReprNArcs:
            {
                ReadNArcs(F);
                return;
            }
        }
    }
    else if (&pool==Geometry())
    {
        switch (token)
        {
            case TokGeoCoordinates:
            {
                // This code compensates an additional nesting level
                Geometry() -> ReadPool(*this,F);
                return;
            }
        }
    }
    else if (&pool==LayoutData())
    {
    }
    else if (&pool==&registers)
    {
        switch (token)
        {
            case TokRegSubgraph:
            {
                ReadSubgraph(F);
                return;
            }
        }
    }
    else
    {
        // This is considered to be the root pool which is
        // only locally known to the constructor method

        switch (token)
        {
            case TokGraphRepresentation:
            {
                ReadRepresentation(F);
                return;
            }
            case TokGraphObjectives:
            {
                pool.ReadPool(*this,F);
                return;
            }
            case TokGraphGeometry:
            {
                ReadGeometry(F);
                return;
            }
            case TokGraphLayout:
            {
                ReadLayoutData(F);
                return;
            }
            case TokGraphRegisters:
            {
                ReadRegisters(F);
                return;
            }
            case TokGraphConfigure:
            {
                F.ReadConfiguration();
                return;
            }
            case TokGraphLength:
            {
                RepresentationalData() -> ReadPool(*this,F);
                return;
            }
        }
    }
}


void abstractMixedGraph::ReadAllData(goblinImport& F) throw(ERParse)
{
    attributePool graphRoot(listOfGraphPars,TokGraphEndSection,attributePool::ATTR_FULL_RANK);
    graphRoot.ReadPool(*this,F);
}


void abstractMixedGraph::ReadNNodes(goblinImport& F) throw(ERParse)
{
    TNode* nodes = F.GetTNodeTuple(3);
    n = nodes[0];
    ni = nodes[2];
    delete[] nodes;
}


void abstractMixedGraph::ReadNArcs(goblinImport& F) throw(ERParse)
{
    graphRepresentation* X = Representation();

    if (X)
    {
        TArc* arcs = F.GetTArcTuple(1);
        m = arcs[0];
        delete[] arcs;

        CheckLimits();

        if (!IsDense()) X -> Reserve(n,m,n+ni);
    }
}


void abstractMixedGraph::ReadRepresentation(goblinImport& F) throw(ERParse)
{
    RepresentationalData() -> ReadPool(*this,F);
}


void abstractMixedGraph::ReadGeometry(goblinImport& F) throw(ERParse)
{
    Geometry() -> ReadPool(*this,F);
}


void abstractMixedGraph::ReadRegisters(goblinImport& F) throw(ERParse)
{
    registers.ReadPool(*this,F);
}


void abstractMixedGraph::ReadSubgraph(goblinImport& F) throw(ERParse)
{
    // The internal representations differs from the other registers.
    // Therefore, a special handling is (temporarily) required.

    TFloat *tmpSub = F.GetTFloatTuple(m);
    bool isConstant = F.Constant();

    for (TArc a=0;a<m;a++)
    {
        TFloat thisSub = InfFloat;

        if (!isConstant) thisSub = tmpSub[a];
        else thisSub = tmpSub[0];

        if (fabs(thisSub)<LCap(2*a) || fabs(thisSub)>UCap(2*a))
        {
            sprintf(CT.logBuffer,"Arc multiplicity exeeds capacity bounds: %lu",
                static_cast<unsigned long>(a));
            Error(ERR_RANGE,"ReadSubgraph",CT.logBuffer);
        }

        SetSub(2*a,thisSub);
    }

    delete[] tmpSub;
}


char* abstractMixedGraph::Display() const throw(ERRejected,ERFile)
{
    if (CT.displayMode==0)
    {
        TextDisplay();
        return NULL;
    }

    #if defined(_TRACING_)

    CT.IncreaseCounter();

    if (CT.displayMode==3)
    {
        char* gobName = new char[strlen(CT.Label())+15];
        sprintf(gobName,"%s.trace%lu.gob",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        Write(gobName);
        if (CT.traceEventHandler) CT.traceEventHandler(gobName);
        delete[] gobName;

        return const_cast<char*>(CT.Label());
    }

    if (CT.displayMode==1)
    {
        char* figName = new char[strlen(CT.Label())+10];
        sprintf(figName,"%s.%lu.fig",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        ExportToXFig(figName);
        delete[] figName;

        char* commandStr = new char[strlen(CT.Label())+15];
        sprintf(commandStr,"xfig %s.%lu.fig &",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        system (commandStr);
        delete[] commandStr;
    }

    if (CT.displayMode==2)
    {
        char* tkName = new char[strlen(CT.Label())+10];
        sprintf(tkName,"%s.%lu.tk",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        ExportToTk(tkName);
        delete[] tkName;

        char* commandStr = new char[strlen(CT.Label())+15];
        sprintf(commandStr,"wish display %s.%lu &",CT.Label(),
            static_cast<unsigned long>(CT.fileCounter));
        system (commandStr);
        delete[] commandStr;
    }

    #endif

    return NULL;
}


void abstractMixedGraph::TextDisplay(TNode i,TNode j) const throw()
{
    sprintf(CT.logBuffer,"Graph has %lu nodes and %lu arcs",
        static_cast<unsigned long>(n),static_cast<unsigned long>(m));
    LogEntry(MSG_TRACE,CT.logBuffer);

    if (m==0) return;

    TArc* pred = GetPredecessors();
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    TNode first = 0;
    TNode last = n-1;

    if (i!=NoNode)
    {
        first = i;

        if (j==NoNode) last = i;
        else last = j;
    }

    int lArc   = CT.ExternalIntLength(2*m)+1;
    int lNode  = CT.ExternalIntLength(n)+4;
    int lFloat = CT.ExternalFloatLength(1.0e20/3.0)+1;
    int lRow   = 2 + lArc + lNode + lFloat;

    if (!CUCap())   lRow += lFloat;
    if (!CLCap())   lRow += lFloat;
    if (!CLength()) lRow += lFloat;
    if (potential)  lRow += lFloat;

    THandle LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    LH = LogStart(MSG_TRACE2,"");
    sprintf(CT.logBuffer,"  %*.*s",lArc,lArc,"arc");
    LogAppend(LH,CT.logBuffer);
    sprintf(CT.logBuffer,"%*.*s",lNode,lNode,"tail");
    LogAppend(LH,CT.logBuffer);

    if (!CUCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"ucap");
        LogAppend(LH,CT.logBuffer);
    }

    sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"subgraph");
    LogAppend(LH,CT.logBuffer);

    if (!CLCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"lcap");
        LogAppend(LH,CT.logBuffer);
    }

    if (!CLength())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"length");
        LogAppend(LH,CT.logBuffer);
    }

    if (potential)
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"redlength");
        LogAppend(LH,CT.logBuffer);
    }

    LogEnd(LH);

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=first;v<=last;++v)
    {
        LH = LogStart(MSG_TRACE2,"");

        for (int k=0;k<lRow;++k) LogAppend(LH,"_");

        LogEnd(LH);

        LogEntry(MSG_TRACE2,"");
        sprintf(CT.logBuffer,"Node %lu",static_cast<unsigned long>(v));
        LH = LogStart(MSG_TRACE2,CT.logBuffer);

        if (CMax(0)!=0 || CMax(1)!=0)
        {
            sprintf(CT.logBuffer," (%g|%g)",
                static_cast<double>(C(v,0)),static_cast<double>(C(v,1)));
            LogAppend(LH,CT.logBuffer);
        }

        LogAppend(LH,":");

        bool first = true;

        if (dist)
        {
            LogAppend(LH," (");
            sprintf(CT.logBuffer,"Distance label %g",static_cast<double>(dist[v]));
            LogAppend(LH,CT.logBuffer);
            first = false;
        }

        if (potential)
        {
            if (first) LogAppend(LH," (");
            else LogAppend(LH,", ");

            sprintf(CT.logBuffer,"Node potential %g",static_cast<double>(potential[v]));
            LogAppend(LH,CT.logBuffer);
            first = false;
        }

        if (!first) LogAppend(LH,")");

        LogEnd(LH);

        LogEntry(MSG_TRACE2,"");

        while (I.Active(v))
        {
            LH = LogStart(MSG_TRACE2,"");
            TArc a = I.Read(v);

            #if defined(_FAILSAVE_)

            if (StartNode(a)!=v)
            {
                sprintf(CT.logBuffer,"Incidence mismatch: %lu",
                    static_cast<unsigned long>(a));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
                sprintf(CT.logBuffer,"Concerning nodes %lu and %lu",
                    static_cast<unsigned long>(v),
                    static_cast<unsigned long>(StartNode(a)));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
            }

            #endif

            if (pred && pred[v]==(a^1)) LogAppend(LH,"P");
            else LogAppend(LH," ");

            if (Blocking(a)) LogAppend(LH,"B");
            else LogAppend(LH," ");

            sprintf(CT.logBuffer,"%*.*lu",
                lArc,lArc,static_cast<unsigned long>(a));
            LogAppend(LH,CT.logBuffer);
            sprintf(CT.logBuffer,"%*.*lu",
                lNode,lNode,static_cast<unsigned long>(EndNode(a)));
            LogAppend(LH,CT.logBuffer);

            if (!CUCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(UCap(a)));
                LogAppend(LH,CT.logBuffer);
            }

            sprintf(CT.logBuffer,"%*.*g",
                lFloat,lFloat,static_cast<double>(Sub(a)));
            LogAppend(LH,CT.logBuffer);

            if (!CLCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(LCap(a)));
                LogAppend(LH,CT.logBuffer);
            }

            if (!CLength())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(Length(a)));
                LogAppend(LH,CT.logBuffer);
            }

            if (potential)
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(RedLength(potential,a)));
                LogAppend(LH,CT.logBuffer);
            }

            LogEnd(LH);
        }
    }

    LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    LogEntry(MSG_TRACE2,"");

    if (CUCap())
    {
        sprintf(CT.logBuffer,"Capacities: %g",static_cast<double>(MaxUCap()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    if (CLCap())
    {
        sprintf(CT.logBuffer,"Lower bounds: %g",static_cast<double>(MaxLCap()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    if (CLength())
    {
        sprintf(CT.logBuffer,"Length labels: %g",static_cast<double>(MaxLength()));
        LogEntry(MSG_TRACE2,CT.logBuffer);
    }

    LH = LogStart(MSG_TRACE2,"");

    for (int k=0;k<lRow;++k) LogAppend(LH,"_");

    LogEnd(LH);

    Close(H);
}


void abstractMixedGraph::DisplayPath(TNode u,TNode v) throw(ERRange,ERRejected)
{
    TArc* pred = GetPredecessors();

    #if defined(_FAILSAVE_)

    if (!pred) Error(ERR_REJECTED,"DisplayPath","Missing predecessor labels");

    if (u>=n) NoSuchNode("DisplayPath",u);

    if (v>=n) NoSuchNode("DisplayPath",v);

    #endif

    TFloat l = 0;
    TNode count = 0;
    TNode w = v;

    LogEntry(LOG_RES,"Encoded path in reverse order:");
    sprintf(CT.logBuffer," (%lu",static_cast<unsigned long>(w));
    THandle LH = LogStart(LOG_RES,CT.logBuffer);

    while (w!=u || count==0)
    {
        TArc a = pred[w];
        l += Length(a);
        w = StartNode(a);
        sprintf(CT.logBuffer,", %lu",static_cast<unsigned long>(w));
        LogAppend(LH,CT.logBuffer);
        count++;

        #if defined(_FAILSAVE_)

        if (count>n) Error(ERR_REJECTED,"DisplayPath","Missing start node");

        #endif
    }

    LogEnd(LH,")");
    sprintf(CT.logBuffer,"Total length: %g",static_cast<double>(l));
    LogEntry(LOG_RES,CT.logBuffer);
    sprintf(CT.logBuffer,"Total number of arcs: %lu",static_cast<unsigned long>(count));
    LogEntry(LOG_RES,CT.logBuffer);
}


TFloat abstractMixedGraph::CutCapacity(TNode separator) throw(ERCheck)
{
    TNode* dist = GetNodeColours();

    if (!dist) Error(ERR_CHECK,"CutCapacity","No edge cut specified");

    LogEntry(LOG_METH,"Checking the d-labels...");
    OpenFold();

    TFloat sCap = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Cut edges: ");

    #endif

    for (TArc a=0;a<2*m;++a)
    {
        if (dist[StartNode(a)]<separator && dist[EndNode(a)]>=separator)
        {
            TCap thisCap = 0;

            if (!Blocking(a) && !Blocking(a^1))
            {
                if (a&1==0) thisCap = UCap(a);
            }
            else if (!Blocking(a) && Blocking(a^1))
            {
                thisCap = UCap(a);
            }
            else if (Blocking(a) && !Blocking(a^1))
            {
                thisCap = -LCap(a);
            }

            #if defined(_LOGGING_)

            if (thisCap>0 && CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu[%g] ",
                    static_cast<unsigned long>(a),static_cast<double>(thisCap));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            sCap += thisCap;
        }
    }

    #if defined(_LOGGING_)

    LogEnd(LH);

    #endif

    CloseFold();
    sprintf(CT.logBuffer,"...Total capacity: %g",static_cast<double>(sCap));
    LogEntry(LOG_RES,CT.logBuffer);

    return sCap;
}


void abstractMixedGraph::ExportToXFig(const char* fileName) const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing xFig canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToXFig E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToTk(const char* fileName) const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing Tk canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToTk E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToDot(const char* fileName)
    const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing Tk canvas to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    exportToDot E(*this,fileName);
    E.DisplayGraph();
}


void abstractMixedGraph::ExportToAscii(const char* fileName,TOption format)
    const throw(ERFile)
{
    sprintf(CT.logBuffer,"Writing text form to \"%s\"...",fileName);
    LogEntry(LOG_IO,CT.logBuffer);

    ofstream expFile(fileName, ios::out);

    if (n==0) return;

    TArc* pred = GetPredecessors();
    TFloat* dist = GetDistanceLabels();
    TFloat* potential = GetPotentials();

    int lArc   = CT.ExternalIntLength(2*m)+1;
    int lNode  = CT.ExternalIntLength(n)+4;
    int lFloat = CT.ExternalFloatLength(1.0e20/3.0)+1;
    int lRow   = 2 + lArc + lNode + lFloat;

    if (!CUCap())   lRow += lFloat;
    if (!CLCap())   lRow += lFloat;
    if (!CLength()) lRow += lFloat;
    if (potential)  lRow += lFloat;

    for (int k=0;k<lRow;++k) expFile << "-";

    expFile << endl;

    sprintf(CT.logBuffer,"  %*.*s",lArc,lArc,"arc");
    expFile << CT.logBuffer;
    sprintf(CT.logBuffer,"%*.*s",lNode,lNode,"tail");
    expFile << CT.logBuffer;

    if (!CUCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"ucap");
        expFile << CT.logBuffer;
    }

    sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"subgraph");
    expFile << CT.logBuffer;

    if (!CLCap())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"lcap");
        expFile << CT.logBuffer;
    }

    if (!CLength())
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"length");
        expFile << CT.logBuffer;
    }

    if (potential)
    {
        sprintf(CT.logBuffer,"%*.*s",lFloat,lFloat,"redlength");
        expFile << CT.logBuffer;
    }

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;++v)
    {
        expFile << endl;

        for (int k=0;k<lRow;++k) expFile << "-";

        sprintf(CT.logBuffer,"Node %lu",static_cast<unsigned long>(v));
        expFile << endl << CT.logBuffer;

        if (CMax(0)!=0 || CMax(1)!=0)
        {
            sprintf(CT.logBuffer," (%g|%g)",
                static_cast<double>(C(v,0)),static_cast<double>(C(v,1)));
            expFile << CT.logBuffer;
        }

        expFile << ":";

        bool first = true;

        if (dist)
        {
            expFile << " (";
            sprintf(CT.logBuffer,"Distance label %g",static_cast<double>(dist[v]));
            expFile << CT.logBuffer;
            first = false;
        }

        if (potential)
        {
            if (first) expFile << " (";
            else expFile << ", ";

            sprintf(CT.logBuffer,"Node potential %g",static_cast<double>(potential[v]));
            expFile << CT.logBuffer;
            first = false;
        }

        if (!first) expFile << ")";

        expFile << endl;

        while (I.Active(v))
        {
            expFile << endl;
            TArc a = I.Read(v);

            #if defined(_FAILSAVE_)

            if (StartNode(a)!=v)
            {
                sprintf(CT.logBuffer,"Incidence mismatch: %lu",
                    static_cast<unsigned long>(a));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
                sprintf(CT.logBuffer,"Concerning nodes %lu and %lu",
                    static_cast<unsigned long>(v),
                    static_cast<unsigned long>(StartNode(a)));
                Error(MSG_WARN,"TextDisplay",CT.logBuffer);
            }

            #endif

            if (pred && pred[v]==(a^1)) expFile << "P";
            else expFile << " ";

            if (Blocking(a)) expFile << "B";
            else expFile << " ";

            sprintf(CT.logBuffer,"%*.*lu",
                lArc,lArc,static_cast<unsigned long>(a));
            expFile << CT.logBuffer;
            sprintf(CT.logBuffer,"%*.*lu",
                lNode,lNode,static_cast<unsigned long>(EndNode(a)));
            expFile << CT.logBuffer;

            if (!CUCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(UCap(a)));
                expFile << CT.logBuffer;
            }

            sprintf(CT.logBuffer,"%*.*g",
                lFloat,lFloat,static_cast<double>(Sub(a)));
            expFile << CT.logBuffer;

            if (!CLCap())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(LCap(a)));
                expFile << CT.logBuffer;
            }

            if (!CLength())
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(Length(a)));
                expFile << CT.logBuffer;
            }

            if (potential)
            {
                sprintf(CT.logBuffer,"%*.*g",
                    lFloat,lFloat,static_cast<double>(RedLength(potential,a)));
                expFile << CT.logBuffer;
            }
        }
    }

    expFile << endl;

    for (int k=0;k<lRow;++k) expFile << "-";

    bool constantsFound = false;

    if (CUCap())
    {
        sprintf(CT.logBuffer,"Capacities: %g",static_cast<double>(MaxUCap()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (CLCap())
    {
        sprintf(CT.logBuffer,"Lower bounds: %g",static_cast<double>(MaxLCap()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (CLength())
    {
        sprintf(CT.logBuffer,"Length labels: %g",static_cast<double>(MaxLength()));
        expFile << endl << CT.logBuffer;
        constantsFound = true;
    }

    if (!constantsFound) return;

    expFile << endl;

    for (int k=0;k<lRow;++k) expFile << "-";

    expFile << endl;
}
