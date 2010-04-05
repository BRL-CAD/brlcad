
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   surfaceGraph.cpp
/// \brief  #surfaceGraph class implementation

#include "surfaceGraph.h"
#include "staticQueue.h"
#include "dynamicStack.h"
#include "moduleGuard.h"


surfaceGraph::surfaceGraph(abstractBalancedFNW& _G) throw() :
    managedObject(_G.Context()),
    abstractBalancedFNW(3*_G.N1(),_G.M()/2),
    G(_G), S(_G.N1(),2*_G.N1(),_G.Context()),
    piG(G.GetPotentials()), pi(InitPotentials())
{
    G.MakeRef();

    n0 = G.n;
    nr = G.N1();
    nv = 2*nr;

    real = false;

    if (CT.methModLength==0)
    {
        modlength = new TFloat[m];

        for (TArc a=0;a<m;a++) modlength[a] = G.RedLength(piG,2*a);
    }
    else modlength = NULL;

    bprop = new TArc[nv];

    SetLayoutParameter(TokLayoutNodeLabelFormat,"#3");
    SetLayoutParameter(TokLayoutArcLabelFormat,"#5");

    LogEntry(LOG_MEM,"...Surface graph allocated");

    InitProps();

    if (CT.traceLevel==2) Display();
}


unsigned long surfaceGraph::Size() const throw()
{
    return
          sizeof(surfaceGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + abstractBalancedFNW::Allocated()
        + surfaceGraph::Allocated();
}


unsigned long surfaceGraph::Allocated() const throw()
{
    unsigned long tmpSize
        = nv*sizeof(TArc);          // bprop

    if (modlength!=NULL) tmpSize += m*sizeof(TFloat);

    return tmpSize;
}


surfaceGraph::~surfaceGraph() throw()
{
    if (CT.traceLevel==2) Display();

    G.ReleaseRef();

    delete[] modlength;
    delete[] bprop;

    LogEntry(LOG_MEM,"...Surface graph disallocated");
}


void surfaceGraph::Init() throw()
{
    InitProps();
    InitPotentials();
}


TNode surfaceGraph::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    TNode v = G.StartNode(a);

    if (real) return v;

    TNode u = v/2;

    TNode x = S.Set(u);

    if (x>=3*nr) x = u;

    if (x<nr) return v;
    else
    {
        if (a==(bprop[x-nr]^2)) return 2*x+1;
        if (a==(bprop[x-nr]^1)) return 2*x;

        if (BalCap(a)>0) return 2*x;
        if (BalCap(a^1)>0) return 2*x+1;

        if (a&1) return 2*x+1;
        else return 2*x;
    }
}


TNode surfaceGraph::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    TNode v = G.EndNode(a);

    if (real) return v;

    TNode u = v/2;

    TNode x = S.Set(u);

    if (x>=nr+nv) x = u;

    if (x<nr) return v;
    else
    {
        if (a==(bprop[x-nr]^0)) return 2*x;
        if (a==(bprop[x-nr]^3)) return 2*x+1;

        if (BalCap(a)>0) return 2*x+1;
        if (BalCap(a^1)>0) return 2*x;

        if (a&1) return 2*x;
        else return 2*x+1;
    }
}


TArc surfaceGraph::First(TNode v) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"First","Not implemented");
    throw ERRejected();
}


TArc surfaceGraph::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"Right","Not implemented");
    throw ERRejected();
}


TCap surfaceGraph::UCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("UCap",a);

    #endif

    return G.UCap(a);
}


TFloat surfaceGraph::Flow(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Flow",a);

    #endif

    return G.Flow(a);
}


TFloat surfaceGraph::BalFlow(TArc a) const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalFlow",a);

    #endif

    return G.BalFlow(a);
}


TCap surfaceGraph::LCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("LCap",a);

    #endif

    return G.LCap(a);
}


TFloat surfaceGraph::Length(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Length",a);

    #endif

    return G.Length(a);
}


TFloat surfaceGraph::C(TNode v,TDim i) const throw(ERRange)
{
    if (v<n0)
    {
        if (HiddenNode(v)) return -InfFloat;
        else return G.C(v,i);
    }

    if (v<n)
    {
        TNode u = S.Set(v/2);

        if (u>=nr+nv) return -InfFloat;

        TNode w = G.EndNode(bprop[u-nr]);

        if (v&1) return G.C(w^1,i);
        else return G.C(w,i);
    }

    #if defined(_FAILSAVE_)

    NoSuchNode("C",v);

    #endif

    throw ERRange();
}


bool surfaceGraph::HiddenNode(TNode v) const throw(ERRange)
{
    if (v<n0) return !Top(v);
    if (v<n) return S.Set(v/2)!=(v/2);

    #if defined(_FAILSAVE_)

    NoSuchNode("HiddenNode",v);

    #endif

    throw ERRange();
}


bool surfaceGraph::HiddenArc(TArc a) const throw(ERRange)
{
    if (a<2*m)
    {
        // if (ModLength(a)!=0) return 1;

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (((u/2)!=(v/2) || u<n0) &&
                 (a==prop[v] || (a^1)==prop[u])
                    || ((a^3)==prop[v^1] || (a^2)==prop[u^1]))
        {
            return 0;
        }
        else return 1;
    }

    #if defined(_FAILSAVE_)

    NoSuchArc("HiddenArc",a);

    #endif

    throw ERRange();
}


void surfaceGraph::Push(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Push",a);

    if (Lambda<0 || Lambda>ResCap(a)) AmountOutOfRange("Push",Lambda);

    #endif

    G.Push(a,Lambda);

    AdjustDivergence(a,Lambda);
}


void surfaceGraph::BalPush(TArc a,TFloat Lambda) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("BalPush",a);

    if (Lambda<0 || Lambda>BalCap(a)) AmountOutOfRange("BalPush",Lambda);

    #endif

    G.BalPush(a,Lambda);

    AdjustDivergence(a,Lambda);
    AdjustDivergence(a^2,Lambda);
}


investigator* surfaceGraph::NewInvestigator() const throw()
{
    return new iSurfaceGraph(*this);
}


TNode surfaceGraph::MakeBlossom(TNode b,TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (b>=n) NoSuchNode("MakeBlossom",b);

    if (a>=2*m) NoSuchArc("MakeBlossom",a);

    #endif

    TNode x = S.MakeSet();

    bprop[x-nr] = a;

    S.Merge(x,b/2);

    return 2*x;
}


bool surfaceGraph::Top(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Top",v);

    #endif
    return S.Top(v/2);
}


TFloat surfaceGraph::ModLength(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("ModLength",a);

    #endif

    if (modlength==NULL) return RModLength(a);
    else
    {
        if (a&1) return -modlength[a>>1];
        else return modlength[a>>1];
    }
}


TFloat surfaceGraph::RModLength(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("RModLength",a);

    #endif

    TNode u = StartNode(a);
    TNode v = EndNode(a);

    if (u<n0 && v<n0) return G.RedLength(piG,a) + pi[u] - pi[v];

    if (u==(v^1))
    {
        S.Block(u/2);
        TFloat ret = RModLength(a);
        S.UnBlock(u/2);
        return ret;
    }

    if (u>=n0)
    {
        S.Block(u/2);
        TFloat ret = RModLength(a);
        S.UnBlock(u/2);
        return ret + pi[u];
    }
    else
    {
        S.Block(v/2);
        TFloat ret = RModLength(a);
        S.UnBlock(v/2);
        return ret - pi[v];
    }
}


void surfaceGraph::ShiftPotential(TNode v,TFloat epsilon) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("ShiftPotential",v);

    #endif

    pi[v] += epsilon;
    pi[v^1] -= epsilon;
}


void surfaceGraph::ShiftModLength(TArc a,TFloat epsilon) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("ShiftModLength",a);

    #endif

    if (modlength==NULL) return;

    if (a&1)
    {
        modlength[a>>1] -= epsilon;
        modlength[(a>>1)^1] -= epsilon;
    }
    else
    {
        modlength[a>>1] += epsilon;
        modlength[(a>>1)^1] += epsilon;
    }
}


void surfaceGraph::Traverse(TArc* pred,TArc aIn,TArc aOut) throw()
{
    TNode v = EndNode(aIn);

    if (v!=StartNode(aOut))
    {
        sprintf(CT.logBuffer,"Mismatching end nodes of arcs %lu,%lu",
            static_cast<unsigned long>(aIn),static_cast<unsigned long>(aOut));
        Error(MSG_WARN,"Traverse",CT.logBuffer);
        sprintf(CT.logBuffer,"Found end nodes: %lu,%lu",
            static_cast<unsigned long>(v),static_cast<unsigned long>(StartNode(aOut)));
        InternalError1("Traverse");
    }

    if (v<n0)
    {
        pred[v] = aIn;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"pred[%lu] = %lu",
                static_cast<unsigned long>(v),static_cast<unsigned long>(aIn));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif
    }
    else
    {
        TNode b = v/2-nr;
        S.Block(v/2);
        Expand(pred,aIn,aOut);
        S.UnBlock(v/2);

        if (v&1) bprop[b] = aIn^3;
        else bprop[b] = aOut^1;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,
                "Prop of blossom %lu changes to %lu",
                    static_cast<unsigned long>(v),static_cast<unsigned long>(bprop[b]));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif
    }
}


void surfaceGraph::Expand(TArc* pred,TArc aIn,TArc aOut) throw()
{
    TNode x = EndNode(aIn);
    TNode y = StartNode(aOut);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Expand(%lu,%lu) started...",
            static_cast<unsigned long>(aIn),static_cast<unsigned long>(aOut));
        LogEntry(LOG_METH2,CT.logBuffer);
        OpenFold();
    }

    #endif

    if (Q[x]!=NoArc && Q[y]!=NoArc)
    {
        TNode z = y;

        while (z!=x && Q[z]!=(NoArc^1)) z = StartNode(Q[z]);

        if (z==x)
        {
            TArc a = aOut;
            z = y;

            while (z!=x)
            {
                Traverse(pred,Q[z],a);
                a = Q[z];
                z = StartNode(a);
            }

            Traverse(pred,aIn,a);
        }
        else
        {
            TArc a = aIn;
            z = x;

            while (z!=y)
            {
                Traverse(pred,a,Q[z]^1);
                a = Q[z]^1;
                z = EndNode(a);
            }

            Traverse(pred,a,aOut);
        }
    }

    if (Q[x]==NoArc && Q[y]==NoArc)
    {
        TNode z = y^1;

        while (z!=(x^1) && Q[z^1]==NoArc) z = StartNode(Q[z]);

        if (z==(x^1))
        {
            TArc a = aOut;
            z = y;

            while (z!=x)
            {
                Traverse(pred,Q[z^1]^3,a);
                a = Q[z^1]^3;
                z = StartNode(a);
            }

            Traverse(pred,aIn,a);
        }
        else
        {
            TArc a = aIn;
            z = x;

            while (z!=y)
            {
                Traverse(pred,a,Q[z^1]^2);
                a = Q[z^1]^2;
                z = EndNode(a);
            }

            Traverse(pred,a,aOut);
        }
    }

    if (Q[x]==NoArc && Q[y]!=NoArc)
    {
        bool reached = false;
        TNode z = x^1;

        while (Q[z]!=(NoArc^1))
        {
            if (z==y) reached = true;

            z = StartNode(Q[z]);
        }

        if (z==y) reached = true;

        if (reached)
        {
            TArc a = Q[z^1]^3;
            z = StartNode(a);

            Expand(pred,a,aOut);

            while (z!=x)
            {
                Traverse(pred,Q[z^1]^3,a);
                a = Q[z^1]^3;
                z = StartNode(a);
            }

            Traverse(pred,aIn,a);
        }
        else
        {
            TArc a = aIn;
            TNode z = x;

            while (Q[z]==NoArc)
            {
                Traverse(pred,a,Q[z^1]^2);
                a = Q[z^1]^2;
                z = EndNode(a);
            }

            Expand(pred,a,aOut);
        }
    }

    if (Q[y]==NoArc && Q[x]!=NoArc)
    {
        bool reached = false;
        TNode z = y^1;

        while (Q[z]!=(NoArc^1))
        {
            if (z==x) reached = true;

            z = StartNode(Q[z]);
        }

        if (z==x) reached = true;

        if (reached)
        {
            TArc a = Q[z^1]^2;
            z = EndNode(a);

            Expand(pred,aIn,a);

            while (z!=y)
            {
                Traverse(pred,a,Q[z^1]^2);
                a = Q[z^1]^2;
                z = EndNode(a);
            }

            Traverse(pred,a,aOut);
        }
        else
        {
            TArc a = aOut;
            TNode z = y;

            while (Q[z]==NoArc)
            {
                Traverse(pred,Q[z^1]^3,a);
                a = Q[z^1]^3;
                z = StartNode(a);
            }

            Expand(pred,aIn,a);
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) CloseFold();

    #endif
}


TCap surfaceGraph::ExpandAndAugment(TArc aIn,TArc aOut) throw()
{
    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        LogEntry(LOG_METH2,"Extracting path for augmentation...");
        OpenFold();
    }

    #endif

    TArc* pred = InitPredecessors();
    TNode u = StartNode(aIn);
    TNode v = EndNode(aOut);
    Expand(pred,aIn,aOut);
    pred[v] = aOut;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"pred[%lu] = %lu",
            static_cast<unsigned long>(v),static_cast<unsigned long>(aOut));
        LogEntry(LOG_METH2,CT.logBuffer);
        CloseFold();
    }

    #endif

    real = true;
    TFloat Lambda = FindBalCap(pred,u,v);
    BalAugment(pred,u,v,Lambda);
    real = false;

    return 2*Lambda;
}


bool surfaceGraph::Compatible() throw()
{
    LogEntry(LOG_METH,"Checking modified length labels...");

    bool ret = true;

    for (TArc a=0;(ret && a<2*m);a++)
        if (BalCap(a)>0 && ModLength(a)<0) ret = false;

    if (CT.logRes)
    {
        if (ret) LogEntry(LOG_RES,"...solutions are compatible");
        else LogEntry(LOG_RES,"...solutions are not compatible");
    }

    return ret;
}


void surfaceGraph::CheckDual() throw()
{
    for (TArc a=0;a<2*m;a++)
    {
        TFloat ret = RModLength(a);

        if (BalCap(a)>0 && modlength!=NULL && ModLength(a)!=ret)
        {
            Error(MSG_WARN,"ComputeEpsilon","Diverging length labels");
            sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                static_cast<unsigned long>(a),
                static_cast<unsigned long>(G.StartNode(a)),
                static_cast<unsigned long>(G.EndNode(a)));
            Error(MSG_WARN," ",CT.logBuffer);
            sprintf(CT.logBuffer,"Explicit label %g, implicit label %g",
                static_cast<double>(ModLength(a)),static_cast<double>(ret));
            InternalError1("CheckDual");
        }

        if (BalCap(a)>0 && ret<0)
        {
            sprintf(CT.logBuffer,"Negative modified length: %g",ret);
            Error(MSG_WARN,"ComputeEpsilon",CT.logBuffer);
            sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                static_cast<unsigned long>(a),
                static_cast<unsigned long>(G.StartNode(a)),
                static_cast<unsigned long>(G.EndNode(a)));
            Error(MSG_WARN," ",CT.logBuffer);
            sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                static_cast<unsigned long>(a),
                static_cast<unsigned long>(StartNode(a)),
                static_cast<unsigned long>(EndNode(a)));
            InternalError1("CheckDual");
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
        LogEntry(LOG_METH2,"...Modified length labels are consistent");

    #endif
}


TArc surfaceGraph::FindSupport(TFloat* dist,TNode s,TArc a,
    dynamicStack<TNode> &Support) throw()
{
    // Find common predecessor //

    TNode x = StartNode(a);
    TNode y = EndNode(a)^1;
    Q[y^1] = a;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Q[%lu] = %lu",
            static_cast<unsigned long>(y^1),static_cast<unsigned long>(Q[y^1]));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    TArc thisProp = a;
    while (x!=y)
    {
        if (dist[x]>dist[y])
        {
            Q[x] = prop[x];
            Q[x^1] = NoArc;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(Q[x]));
                LogEntry(LOG_METH2,CT.logBuffer);
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(x^1),static_cast<unsigned long>(Q[x^1]));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            thisProp = prop[x];
            Support.Insert(x);
            x = StartNode(thisProp); 
        }
        else
        {
            Q[y] = NoArc;
            Support.Insert(y);
            TArc thisProp = prop[y];
            y = StartNode(thisProp); 
            Q[y^1] = thisProp^2;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(y),static_cast<unsigned long>(Q[y]));
                LogEntry(LOG_METH2,CT.logBuffer);
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(y^1),static_cast<unsigned long>(Q[y^1]));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }

    // Find base node //

    while (x!=s && BalCap(prop[x])>1)
    {
        thisProp = prop[x];
        Q[x] = thisProp;
        Support.Insert(x);
        x = StartNode(prop[x]);
        Q[x^1] = thisProp^2;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(Q[x]));
                LogEntry(LOG_METH2,CT.logBuffer);
                sprintf(CT.logBuffer,"Q[%lu] = %lu",
                    static_cast<unsigned long>(x)^1,static_cast<unsigned long>(Q[x^1]));
                LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif
    }

    Q[x] = NoArc^1;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Q[%lu] = %lu",
            static_cast<unsigned long>(x),static_cast<unsigned long>(Q[x]));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    return thisProp;
}


TFloat surfaceGraph::ComputeEpsilon(TFloat* dist) throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Computing epsilon...");

    #endif

    TFloat epsilon1 = InfFloat;
    TFloat epsilon2 = InfFloat;
    TFloat epsilon3 = InfFloat;

    for (TNode i=0;i<nv;i++)
    {
        TNode v = n0+2*i+1;
        if (dist[v]<InfFloat && Top(v) && pi[v]<epsilon3) epsilon3 = pi[v];
    }

    TFloat epsilon = epsilon3;

    for (TArc a=0;a<2*m && epsilon>0.5;a++)
    {
        TNode u = StartNode(a);
        TNode v = EndNode(a);
        TNode cv = v^1;

        if (BalCap(a)>0 && ModLength(a)<0)
        {
            sprintf(CT.logBuffer,"Negative modified length %g",
                static_cast<double>(a));
            Error(MSG_WARN,"ComputeEpsilon",CT.logBuffer);
            sprintf(CT.logBuffer,"Implicit modified length %g",
                static_cast<double>(RModLength(a)));
            Error(MSG_WARN," ",CT.logBuffer);
            sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                static_cast<unsigned long>(a),
                static_cast<unsigned long>(u),
                static_cast<unsigned long>(v));
            InternalError1("ComputeEpsilon");
        }

        if (BalCap(a)>0 && dist[u]<InfFloat && dist[v]==InfFloat)
        {
            if (dist[cv]<InfFloat && ModLength(a)<epsilon2*2 && (u!=cv || v<n0)
                && (a^1)!=prop[EndNode(a^1)] && (a^3)!=prop[EndNode(a^3)])
            {
                epsilon2 = ModLength(a)/2;

                if (epsilon2==0)
                {
                    Error(MSG_WARN,"ComputeEpsilon","No dual improvement");
                    sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    InternalError1("ComputeEpsilon");
                }

                if (epsilon2<epsilon) epsilon = epsilon2;
            }

            if (dist[cv]==InfFloat && ModLength(a)<epsilon1)
            {
                epsilon1 = ModLength(a);

                if (epsilon1==0)
                {
                    Error(MSG_WARN,"ComputeEpsilon","No dual improvement");
                    sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    InternalError1("ComputeEpsilon");
                }

                if (epsilon1<epsilon) epsilon = epsilon1;
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        OpenFold();

        if (epsilon>0.5 || epsilon1==0.5)
            sprintf(CT.logBuffer,"epsilon1 = %g",static_cast<double>(epsilon1));
        else
            sprintf(CT.logBuffer,"epsilon1 <= %g",static_cast<double>(epsilon1));

        LogEntry(LOG_METH2,CT.logBuffer);

        if (epsilon>0.5 || epsilon2==0.5)
            sprintf(CT.logBuffer,"epsilon2 = %g",static_cast<double>(epsilon2));
        else
            sprintf(CT.logBuffer,"epsilon2 <= %g",static_cast<double>(epsilon2));

        LogEntry(LOG_METH2,CT.logBuffer);

        if (epsilon>0.5 || epsilon3==0.5)
            sprintf(CT.logBuffer,"epsilon3 = %g",static_cast<double>(epsilon3));
        else
            sprintf(CT.logBuffer,"epsilon3 <= %g",static_cast<double>(epsilon3));
        LogEntry(LOG_METH2,CT.logBuffer);

        CloseFold();
    }

    #endif

    return epsilon;
}


void surfaceGraph::PrimalDual0(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("PrimalDual0",s);

    #endif

    moduleGuard M(ModMinCBalFlow,*this,
        moduleGuard::NO_INDENT | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    TFloat deficiency = 0;

    THandle H1 = Investigate();
    while (Active(H1,s))
    {
        TArc a = Read(H1,s);
        deficiency += BalCap(a);
    }

    Close(H1);

    M.InitProgressCounter(deficiency);

    #endif

    TNode t = s^1;

    Init();
    InitProps();
    TFloat* dist = InitDistanceLabels();
    InitCycles();

    THandle H = Investigate();
    investigator &I = Investigator(H);

    dynamicStack<TNode> Support(n,CT);
    staticQueue<TArc> Queue(2*m,CT);

    TFloat epsilon = 0;

    while (epsilon<InfFloat)
    {
        bool searching = true;

        while (searching && epsilon<InfFloat)
        {
            // Search Surface Graph

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                LogEntry(LOG_METH2,"Searching Surface Graph...");
                OpenFold();
            }

            #endif

            for (TNode w=0;w<n;w++)
                if (Top(w))
                {
                    prop[w] = NoArc;
                    dist[w] = InfFloat;
                }

            Queue.Init();

            dist[s] = 0;
            Queue.Insert(s);

            while (searching && !Queue.Empty())
            {
                TNode u = Queue.Delete();
                I.Reset(u);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Searching node %lu...",
                        static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                    OpenFold();
                }

                #endif

                while (searching && Top(u) && I.Active(u))
                {
                    TArc a = I.Read(u);
                    TNode v = EndNode(a);
                    TNode cv = v^1;

                    if (dist[cv]==InfFloat)
                    {
                        if (dist[v]==InfFloat && BalCap(a)>0 && ModLength(a)==0)
                        {
                            // Generate Bud

                            #if defined(_LOGGING_)

                            if (CT.logMeth>1)
                            {
                                sprintf(CT.logBuffer,"Prop %lu with end node %lu found",
                                    static_cast<unsigned long>(a),
                                    static_cast<unsigned long>(v));
                                LogEntry(LOG_METH2,CT.logBuffer);
                            }

                            #endif

                            dist[v] = dist[u]+1;
                            prop[v] = a;
                            Queue.Insert(v);
                            M.Trace();
                        }

                        continue;
                    }

                    if (prop[u]==(a^1) || BalCap(a)==0 ||
                        ModLength(a)!=0 || (u==cv && v>=n0)) continue;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Petal %lu with end node %lu found",
                            static_cast<unsigned long>(a),static_cast<unsigned long>(v));
                        LogEntry(LOG_METH2,CT.logBuffer);
                        OpenFold();
                    }

                    #endif

                    TArc firstProp = FindSupport(dist,s,a,Support);
                    TNode base = StartNode(firstProp);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) CloseFold();

                    #endif

                    if (base==s)
                    {
                        // Augment Flow
                        searching = false;
                        Support.Init();

                        // Only needed for tracing the augm. path
                        prop[v] = a; 

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            CloseFold();
                            CloseFold();
                        }

                        #endif

                        TCap lambda = ExpandAndAugment(firstProp,Q[t]);
                        M.Trace(lambda);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            OpenFold();
                            OpenFold();
                        }

                        #endif

                        M.Trace(G);

                        continue;
                    }


                    // Shrink Blossom

                    #if defined(_LOGGING_)

                    THandle LH = NoHandle;

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Shrinking %lu",
                            static_cast<unsigned long>(base));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    TNode z = MakeBlossom(base,prop[base]);

                    while (!Support.Empty())
                    {
                        TNode y = Support.Delete();
                        S.Merge(z/2,y/2);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,",%lu",
                                static_cast<unsigned long>(y));
                            LogAppend(LH,CT.logBuffer);
                        }

                        #endif
                    }

                    S.FixSet(z/2);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer," into node %lu",
                            static_cast<unsigned long>(z));
                        LogEnd(LH,CT.logBuffer);
                    }

                    #endif

                    Queue.Insert(z);
                    dist[z] = dist[base];
                    prop[z] = prop[base];
                    M.Trace();
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>1) CloseFold();

                #endif
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif


            if (searching)
            {
                // Dual Improvement

                epsilon = ComputeEpsilon(dist);

                if (epsilon<InfFloat && epsilon>0)
                {
                    LogEntry(LOG_METH,"Dual Improvement...");

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) OpenFold();

                    #endif

                    for (TNode v=0;v<n;v++)
                    {
                        if (!Top(v) || dist[v]==InfFloat) continue;

                        ShiftPotential(v,-epsilon);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"pi[%lu] = %g",
                                static_cast<unsigned long>(v),
                                static_cast<double>(pi[v]));
                            LogEntry(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        TNode w = v^1;

                        if (v<n0 || !(v&1)) w = v;

                        I.Reset(w);
                        TNode cw = w^1;

                        while (I.Active(w))
                        {
                            TArc a = I.Read(w);
                            TNode u = EndNode(a);

                            if (u!=w && (u!=cw || u<n0))
                            {
                                if (w==v) ShiftModLength(a,-epsilon);
                                else ShiftModLength(a,epsilon);
                            }
                        }
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) CloseFold();

                    #endif
                }

                if (epsilon<InfFloat)
                {
                    for (TNode i=0;i<nv;i++)
                    {
                        TNode v = n0+2*i+1;

                        if (pi[v]!=0 || dist[v]==InfFloat || !Top(v)) continue;

                        #if defined(_LOGGING_)

                        if (CT.logMeth)
                        {
                            sprintf(CT.logBuffer,"Expanding node %lu...",
                                static_cast<unsigned long>(v));
                            LogEntry(LOG_METH,CT.logBuffer);
                        }

                        #endif

                        S.Split(v/2);
                    }
                }
            }
        }
    }

    Close(H);
}


void surfaceGraph::Explore(TFloat* dist,goblinQueue<TArc,TFloat> &Q,
    investigator &I,TNode v) throw()
{
    I.Reset(v);

    while (I.Active(v))
    {
        try
        {
            TArc a = I.Read(v);

            if (BalCap(a)>0 && (dist[v^1]==InfFloat || prop[v^1]!=(a^2)))
            {
                TFloat thisLength = ModLength(a);

                if (thisLength==0) Q.Insert(a,0);

                if (thisLength>=0 && CT.methPrimalDual==2)
                {
                    TNode w = EndNode(a);
                    TArc a2 = prop[w];

                    if (dist[w]==InfFloat && (v<n0 || (w/2)!=(v/2)) &&
                        (a2==NoArc || thisLength<ModLength(a2)))
                    {
                        prop[w] = a;
                    }
                }

                if (thisLength<0)
                {
                    sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu) has modified length %g",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(G.StartNode(a)),
                        static_cast<unsigned long>(G.EndNode(a)),
                        static_cast<double>(thisLength));
                    InternalError1("Explore");
                }
            }
        }
        catch (ERCheck) {}
    }
}


TFloat surfaceGraph::ComputeEpsilon1(TFloat* dist) throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Computing epsilon...");

    #endif

    TFloat epsilon1 = InfFloat;
    TFloat epsilon2 = InfFloat;
    TFloat epsilon3 = InfFloat;

    for (TNode i=0;i<nv;i++)
    {
        TNode v = n0+2*i+1;
        if (dist[v]<InfFloat && Top(v) && pi[v]<epsilon3) epsilon3 = pi[v];
    }

    TFloat epsilon = epsilon3;

    for (TNode v=0;v<n && epsilon>0.5;v++)
    {
        TArc a = prop[v];

        if (a!=NoArc && dist[v]==InfFloat && Top(v))
        {
            TNode u = StartNode(a);
            TNode cv = v^1;
            TFloat thisLength = ModLength(a);

            if (dist[u]<InfFloat && dist[cv]<InfFloat &&
                thisLength<epsilon2*2 && (u!=cv || v<n0)
               )
            {
                epsilon2 = thisLength/2;

                if (epsilon2<=0)
                {
                    Error(MSG_WARN,"ComputeEpsilon1",
                        "Reduced cost labels are corrupted");
                    sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    Error(MSG_WARN," ",CT.logBuffer);
                    sprintf(CT.logBuffer,"Modified length labels %g and %g",
                        static_cast<double>(ModLength(a)),
                        static_cast<double>(RModLength(a)));
                    InternalError1(" ");
                }

                if (epsilon2<epsilon) epsilon = epsilon2;
            }

            if (dist[u]<InfFloat && dist[cv]==InfFloat && thisLength<epsilon1)
            {
                epsilon1 = thisLength;

                if (epsilon1==0)
                {
                    Error(MSG_WARN,"ComputeEpsilon1",
                        "Reduced cost labels are corrupted");
                    sprintf(CT.logBuffer,"Arc %lu=(%lu,%lu)",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    Error(MSG_WARN," ",CT.logBuffer);
                    sprintf(CT.logBuffer,"Modified length labels %g and %g",
                        static_cast<double>(ModLength(a)),
                        static_cast<double>(RModLength(a)));
                    InternalError1("ComputeEpsilon1");
                }

                if (epsilon1<epsilon) epsilon = epsilon1;
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        OpenFold();

        if (epsilon>0.5 || epsilon1==0.5)
            sprintf(CT.logBuffer,"epsilon1 = %g",static_cast<double>(epsilon1));
        else
            sprintf(CT.logBuffer,"epsilon1 <= %g",static_cast<double>(epsilon1));

        LogEntry(LOG_METH2,CT.logBuffer);

        if (epsilon>0.5 || epsilon2==0.5)
            sprintf(CT.logBuffer,"epsilon2 = %g",static_cast<double>(epsilon2));
        else
            sprintf(CT.logBuffer,"epsilon2 <= %g",static_cast<double>(epsilon2));

        LogEntry(LOG_METH2,CT.logBuffer);

        if (epsilon>0.5 || epsilon3==0.5)
            sprintf(CT.logBuffer,"epsilon3 = %g",static_cast<double>(epsilon3));
        else
            sprintf(CT.logBuffer,"epsilon3 <= %g",static_cast<double>(epsilon3));

        LogEntry(LOG_METH2,CT.logBuffer);
        CloseFold();
    }

    #endif

    return epsilon;
}


void surfaceGraph::PrimalDual1(TNode s) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("PrimalDual1",s);

    #endif

    moduleGuard M(ModMinCBalFlow,*this,
        moduleGuard::NO_INDENT | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    TFloat deficiency = 0;

    THandle H1 = Investigate();
    while (Active(H1,s))
    {
        TArc a = Read(H1,s);
        deficiency += BalCap(a);
    }

    Close(H1);

    M.InitProgressCounter(deficiency);

    #endif

    TNode t = ComplNode(s);

    Init();
    InitProps();
    TFloat* dist = InitDistanceLabels();
    InitCycles();

    THandle H = Investigate();
    investigator &I = Investigator(H);

    dynamicStack<TNode> Support(n,CT);
    dynamicStack<TNode> Swap(n,CT);
    staticQueue<TArc> Queue(2*m,CT);

    TFloat epsilon = 0;

    while (epsilon<InfFloat)
    {
        bool searching = true;

        Queue.Init();

        for (TNode w=0;w<n;w++)
            if (Top(w))
            {
                prop[w] = NoArc;
                dist[w] = InfFloat;
            }

        dist[s] = 0;
        Explore(dist,Queue,I,s);

        while (searching && epsilon<InfFloat)
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                LogEntry(LOG_METH2,"Searching Surface Graph...");
                OpenFold();
            }

            #endif

            while (searching && !Queue.Empty())
            {
                TArc a = Queue.Delete();
                TNode u = StartNode(a);
                TNode v = EndNode(a);
                TNode cv = v^1;

                if (dist[u]==InfFloat || u==v) continue;

                if (dist[cv]==InfFloat)
                {
                    if (dist[v]==InfFloat)
                    {
                        // Generate Bud

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,
                                "Prop %lu=(%lu,%lu) found",
                                static_cast<unsigned long>(a),
                                static_cast<unsigned long>(u),
                                static_cast<unsigned long>(v));
                            LogEntry(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        dist[v] = dist[u]+1;
                        prop[v] = a;
                        Explore(dist,Queue,I,v);
                        M.Trace();
                    }

                    continue;
                }

                if (prop[u]==(a^1) || (u==cv && v>=n0)) continue;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,
                        "Petal %lu=(%lu,%lu) found",
                        static_cast<unsigned long>(a),
                        static_cast<unsigned long>(u),
                        static_cast<unsigned long>(v));
                    LogEntry(LOG_METH2,CT.logBuffer);
                    OpenFold();
                }

                #endif

                TArc firstProp = FindSupport(dist,s,a,Support);
                TNode base = StartNode(firstProp);

                #if defined(_LOGGING_)

                if (CT.logMeth>1) CloseFold();

                #endif

                if (base==s)
                {
                    searching = false;
                    Support.Init();

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) CloseFold();

                    #endif

                    // Only needed for tracing the augm. path
                    prop[v] = a;

                    TCap lambda = ExpandAndAugment(firstProp,Q[t]);
                    M.Trace(lambda);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) OpenFold();

                    #endif

                    M.Trace(G);

                    continue;
                }

                if (CT.methPrimalDual==1)
                {
                    prop[v] = a;
                    M.Trace();
                }

                #if defined(_LOGGING_)

                THandle LH = NoHandle;

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Shrinking %lu",
                        static_cast<unsigned long>(base));
                    LH = LogStart(LOG_METH2,CT.logBuffer);
                }

                #endif

                Explore(dist,Queue,I,base^1);

                while (!Support.Empty())
                {
                    TNode y = Support.Delete();
                    Swap.Insert(y);
                    Explore(dist,Queue,I,y^1);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(y));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif

                    dist[y] = InfFloat;
                }

                TNode z = MakeBlossom(base,prop[base]);

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer," into node %lu",static_cast<unsigned long>(z));
                    LogEnd(LH,CT.logBuffer);
                }

                #endif

                while (!Swap.Empty())
                {
                    TNode y = Swap.Delete();
                    S.Merge(z/2,y/2);
                }

                S.FixSet(z/2);

                dist[z] = dist[base];
                dist[base] = InfFloat;
                prop[z] = prop[base];

                if (CT.methPrimalDual==2)
                {
                    TNode cz = z^1;
                    TArc bestProp = NoArc;
                    TFloat bestLength = InfFloat;
                    I.Reset(cz);

                    while (I.Active(cz))
                    {
                        TArc a2 = (I.Read(cz))^1;
                        TNode y = StartNode(a2);

                        if (y!=z && dist[y]<InfFloat && BalCap(a2)>0)
                        {
                            TFloat thisLength = ModLength(a2);

                            if (thisLength<bestLength)
                            {
                                bestProp = a2;
                                bestLength = thisLength;
                            }
                        }
                    }

                    prop[cz] = bestProp;
                }

                M.Trace();
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif

            if (searching)
            {
                // Dual Improvement

                if (CT.methPrimalDual==2) epsilon = ComputeEpsilon1(dist);
                else epsilon = ComputeEpsilon(dist);

                if (epsilon<InfFloat && epsilon>0)
                {
                    LogEntry(LOG_METH,"Dual Improvement...");

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) OpenFold();

                    #endif

                    for (TNode v=0;v<n;v++)
                    {
                        if (!Top(v) || dist[v]==InfFloat) continue;

                        ShiftPotential(v,-epsilon);

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"pi[%lu] = %g",
                                static_cast<unsigned long>(v),
                                static_cast<double>(pi[v]));
                            LogEntry(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        TNode w = v^1;

                        if (v<n0 || !(v&1)) w = v;

                        I.Reset(w);
                        TNode cw = w^1;

                        while (I.Active(w))
                        {
                            TArc a = I.Read(w);
                            TNode u = EndNode(a);

                            if (u==w || (u==cw && u>=n0)) continue;

                            if (w==v)
                            {
                                ShiftModLength(a,-epsilon);
                                if (BalCap(a)>0 && ModLength(a)==0)
                                {
                                    Queue.Insert(a);
                                }
                            }
                            else
                            {
                                ShiftModLength(a,epsilon);
                                a = a^3;
                                if (BalCap(a)>0 && ModLength(a)==0)
                                {
                                    Queue.Insert(a);
                                }
                            }
                        }
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1) CloseFold();

                    #endif
                }

                if (epsilon<InfFloat)
                {
                    for (TNode i=0;i<nv;i++)
                    {
                        TNode v = n0+2*i+1;

                        if (pi[v]!=0 || dist[v]==InfFloat || !Top(v)) continue;

                        #if defined(_LOGGING_)

                        if (CT.logMeth)
                        {
                            sprintf(CT.logBuffer,"Expanding node %lu...",
                                static_cast<unsigned long>(v));
                            LogEntry(LOG_METH,CT.logBuffer);
                        }

                        #endif

                        S.Split(v/2);
                        dist[v] = InfFloat;
                        prop[v] = NoArc;
                        searching = false;
                    }
                }
            }

            if (CT.methFailSave==1) CheckDual();
        }
    }

    Close(H);
}
