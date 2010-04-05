
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file layoutForceDirected.cpp
/// \brief Implementations of force directed layout methods

#include "mixedGraph.h"
#include "matrix.h"
#include "moduleGuard.h"


void abstractMixedGraph::Layout_ForceDirected(TOptFDP method,TFloat spacing)
    throw(ERRejected)
{
    graphRepresentation* X = Representation();

    #if defined(_FAILSAVE_)

    if (!X) NoRepresentation("Layout_ForceDirected");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_ForceDirected","Coordinates are fixed");

    #endif

    moduleGuard M(ModForceDirected,*this,"Force directed drawing...",
        moduleGuard::NO_INDENT);

    if (method==FDP_DEFAULT) method = TOptFDP(CT.methFDP);
    if (method==FDP_DEFAULT) method = FDP_GEM;

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    switch (method)
    {
        case FDP_GEM:
        case FDP_RESTRICTED:
        {
            Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
            Layout_GEMDrawing(method,spacing);
            Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);
            break;
        }
        case FDP_SPRING:
        {
            Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
            Layout_SpringEmbedder(spacing);
            Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);
            break;
        }
        case FDP_LAYERED:
        case FDP_LAYERED_RESTR:
        {
            // Method applies to properly layered graphs only

            explicitSubdivision G(*this,OPT_MAPPINGS);
            G.Layout_LayeredFDP(method,spacing);

            for (TNode v=0;v<G.N();v++)
            {
                for (TDim i=0;i<G.Dim();i++)
                {
                    X -> SetC(G.OriginalOfNode(v),i,G.C(v,i));
                }
            }

            if (IsSparse())
            {
                static_cast<sparseRepresentation*>(X) -> Layout_AdoptBoundingBox(G);
            }

            TFloat spacing = 0.0;
            GetLayoutParameter(TokLayoutFineSpacing,spacing);

            for (TArc a=0;a<m;a++)
            {
                TNode x = ArcLabelAnchor(2*a);
                if (x==NoNode) continue;

                TNode y = ThreadSuccessor(x);
                if (y==NoNode) continue;

                for (TDim i=0;i<G.Dim();i++)
                {
                    X -> SetC(x,i,C(y,i) + ((i==0) ? spacing : 0));
                }
            }

            break;
        }
        default:
        {
            UnknownOption("Layout_ForceDirected",method);
        }
    }
}


void abstractMixedGraph::Layout_SpringEmbedder(TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_SpringEmbedder","Coordinates are fixed");

    #endif

    moduleGuard M(ModSpringEmbedder,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1000000);

    TFloat firstStepLength = InfFloat;
    double combEstimate = 0;

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    double gamma = 1.0/double(spacing);
    double delta = 1.0/double(spacing);

    denseMatrix<TIndex,TFloat> J1(2*n-2,2*n-2);
    denseMatrix<TIndex,TFloat> J2(2*n-2,2*n-2);
    denseMatrix<TIndex,TFloat> F(2*n-2,1);
    denseMatrix<TIndex,TFloat> F2(2*n-2,1);

    TFloat          stepLength  = InfFloat;
    unsigned long   stepCount   = 1;
    bool            stepValid   = true;
    TFloat          redFactor   = 1.0;

    Layout_ReleaseBoundingBox();

    while (CT.SolverRunning() && stepLength>0.1*log(double(n)))
    {
        // Compute Jacobian matrix J1 and reset J2 to identity

        for (TNode i=0;i<n-1 && stepValid;i++)
        {
            F.SetCoeff(2*i,0,0);
            F.SetCoeff(2*i+1,0,0);

            J1.SetCoeff(2*i   ,2*i   ,0);
            J1.SetCoeff(2*i   ,2*i+1 ,0);
            J1.SetCoeff(2*i+1 ,2*i   ,0);
            J1.SetCoeff(2*i+1 ,2*i+1 ,0);

            for (TNode j=0;j<n;j++)
            {
                if (j<n-1)
                {
                    J2.SetCoeff(2*i   ,2*j+1, 0);
                    J2.SetCoeff(2*i+1 ,2*j,   0);

                    if (i==j)
                    {
                        J2.SetCoeff(2*i+1 ,2*j+1, 1);
                        J2.SetCoeff(2*i   ,2*j,   1);
                        continue;
                    }
                    else
                    {
                        J2.SetCoeff(2*i+1 ,2*j+1, 0);
                        J2.SetCoeff(2*i   ,2*j,   0);
                    }
                }

                TFloat diffx = C(i,0)-C(j,0);
                TFloat diffy = C(i,1)-C(j,1);
                TFloat diff2 = delta*(diffx*diffx+diffy*diffy);

                if (diff2<CT.epsilon)
                {
                    sprintf(CT.logBuffer,"diff2 = %g",diff2);
                    LogEntry(LOG_METH2,CT.logBuffer);
                    stepValid = false;
                    break;
                }

                TFloat coeff_x_x = 2*diffx*diffx/(diff2*diff2)-1/diff2;
                TFloat coeff_x_y = 2*diffx*diffy/(diff2*diff2);
                TFloat coeff_y_y = 2*diffy*diffy/(diff2*diff2)-1/diff2;

                F.SetCoeff(2*i  ,0,F.Coeff(2*i  ,0)+diffx/diff2);
                F.SetCoeff(2*i+1,0,F.Coeff(2*i+1,0)+diffy/diff2);

                if (Adjacency(i,j)!=NoArc)
                {
                    coeff_x_x += gamma;
                    coeff_y_y += gamma;

                    F.SetCoeff(2*i  ,0,F.Coeff(2*i  ,0)-diffx*gamma);
                    F.SetCoeff(2*i+1,0,F.Coeff(2*i+1,0)-diffy*gamma);
                }

                if (j<n-1)
                {
                    J1.SetCoeff(2*i   ,2*j   ,coeff_x_x);
                    J1.SetCoeff(2*i   ,2*j+1 ,coeff_x_y);
                    J1.SetCoeff(2*i+1 ,2*j   ,coeff_x_y);
                    J1.SetCoeff(2*i+1 ,2*j+1 ,coeff_y_y);
                }

                J1.SetCoeff(2*i   ,2*i   ,J1.Coeff(2*i  ,2*i  )-coeff_x_x);
                J1.SetCoeff(2*i   ,2*i+1 ,J1.Coeff(2*i  ,2*i+1)-coeff_x_y);
                J1.SetCoeff(2*i+1 ,2*i   ,J1.Coeff(2*i+1,2*i  )-coeff_x_y);
                J1.SetCoeff(2*i+1 ,2*i+1 ,J1.Coeff(2*i+1,2*i+1)-coeff_y_y);
            }
        }

        if (stepValid)
        {
            try
            {
                // Transform to J2 := J1^1, J1 := Id
                J1.GaussElim(J2,0.00000001);
            }
            catch (ERRejected)
            {
                stepValid = false;
            }
        }

        if (stepValid)
        {
            // Do Newton step x = x + J1^(-1)*F

            // Compute F2 := J1^1 * F
            F2.Product(J2,F);

            stepLength = 0;

            for (TNode i=0;i<n-1;i++)
            {
                SetC(i,0,C(i,0)-redFactor*F2.Coeff(2*i,0));
                SetC(i,1,C(i,1)-redFactor*F2.Coeff(2*i+1,0));
                stepLength += redFactor*(fabs(F2.Coeff(2*i,0))+fabs(F2.Coeff(2*i+1,0)));
            }

            sprintf(CT.logBuffer,"...Step %ld has length %g",stepCount,stepLength);
            LogEntry(LOG_METH2,CT.logBuffer);

            #if defined(_PROGRESS_)

            if (firstStepLength==InfFloat) firstStepLength = stepLength;

            double thisEstimate = 0;

            if (stepLength<=firstStepLength)
                thisEstimate = pow(1-stepLength/firstStepLength,20);

            combEstimate = 0.9*combEstimate + 0.1*thisEstimate;

            M.SetProgressCounter((unsigned long)(combEstimate*1000000));

            #endif
        }
        else
        {
            LogEntry(LOG_METH2,"...Jacobian is undefined or singular");

            if (stepCount>1)
            {
                for (TNode i=0;i<n-1;i++)
                {
                    SetC(i,0,C(i,0)-0.1*F2.Coeff(2*i,0));
                    SetC(i,1,C(i,1)-0.1*F2.Coeff(2*i+1,0));
                }

                redFactor *= 0.99;

                LogEntry(MSG_APPEND," (step repeated)");
            }
            else
            {
                stepLength = 0;

                LogEntry(MSG_APPEND," (start with different embedding)");
            }

            stepValid = true;
        }

        stepCount++;

        if (CT.traceLevel>2 && IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) -> Layout_ArcRouting();
        }

        M.Trace();
    }

    Layout_FreezeBoundingBox();

    if (CT.logRes==1)
    {
        sprintf(CT.logBuffer,"...%ld Newton iterations in total",stepCount-1);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
}


void abstractMixedGraph::Layout_GEMDrawing(TOptFDP options,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_GEMDrawing","Coordinates are fixed");

    #endif

    moduleGuard M(ModGEM,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1000000);

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    TFloat theta = 10.0/16.0;    // Gravity constant
    TFloat delta = spacing;      // Optimal edge length
    TFloat gamma = spacing;      // Edge separation constant
    TFloat epsilon = 0.001;      // Numeric separation constant;

    TFloat tInit = delta/8;
    TFloat tMin = delta/250;
    TFloat tMax = delta/4;
    TFloat tGlobal = tInit*n;
    TFloat* tCurrent = new TFloat[n];
    for (TNode v=0;v<n;v++) tCurrent[v] = tInit;

    TFloat* skewGauge = new TFloat[n];
    for (TNode v=0;v<n;v++) skewGauge[v] = 0;

    TFloat* px = new TFloat[n];
    TFloat* py = new TFloat[n];
    for (TNode v=0;v<n;v++) px[v] = py[v] = 0;

    TFloat sx = 0;
    TFloat sy = 0;
    for (TNode v=0;v<n;v++)
    {
        sx += C(v,0);
        sy += C(v,1);
    }

    unsigned long stepCount = 0;

    Layout_ReleaseBoundingBox();

    while (CT.SolverRunning() && tGlobal>tMin*n && stepCount<n*300.0)
    {
        // Choose a vertex v to update. Replace by a more sophisticated
        // scheme later
        TNode v = stepCount%n;


        // Compute impulse
        TFloat pcx = theta*(sx/n-C(v,0));
        TFloat pcy = theta*(sy/n-C(v,1));

        // Forces repelling from other nodes
        for (TNode w=0;w<n;w++)
        {
            if (v==w) continue;

            TFloat dx = C(v,0)-C(w,0);
            TFloat dy = C(v,1)-C(w,1);
            TFloat nsq = dx*dx+dy*dy;

            if (nsq>epsilon)
            {
                pcx += dx*delta*delta/nsq;
                pcy += dy*delta*delta/nsq;
            }
        }

        // Attracting forces
        TArc a = First(v);
        while (a!=NoArc)
        {
            TNode w = EndNode(a);

            TFloat dx = C(v,0)-C(w,0);
            TFloat dy = C(v,1)-C(w,1);
            TFloat dist = sqrt(dx*dx+dy*dy);

            if (dist>delta)
            {
                pcx -= dx*(dist-delta)/delta*(dist-delta)/delta*1.0;
                pcy -= dy*(dist-delta)/delta*(dist-delta)/delta*1.0;
            }

            a = Right(a,v);

            if (a==First(v)) break;
        }

        // Forces repelling from close edges
        for (TArc a=0;a<m && (options & FDP_RESTRICTED);a++)
        {
            TNode u = StartNode(2*a);
            TNode w = EndNode(2*a);

            if (u==w) continue;

            if (v==u)
            {
                u = w;
                w = StartNode(2*a);
            }

            TFloat dx_uv = C(v,0)-C(u,0);
            TFloat dy_uv = C(v,1)-C(u,1);

            if (v==w)
            {
                // When v is shifted, a may cross another vertex

                TFloat dx = C(u,0)-C(v,0);
                TFloat dy = C(u,1)-C(v,1);
                TFloat nsq = dx_uv*dx_uv+dy_uv*dy_uv;

                // Degenerate arcs
                if (nsq<epsilon) continue;

                for (TNode w=0;w<n;w++)
                {
                    if (v==w || u==w) continue;

                    TFloat lambda = (dx*(C(w,0)-C(v,0))+dy*(C(w,1)-C(v,1)))/nsq;

                    if (lambda>1+epsilon || lambda<-epsilon)
                    {
                        // Minimum distance of w and a is achieved at an end point
                        continue;
                    }

                    TFloat dx2 = C(v,0)+lambda*dx-C(w,0);
                    TFloat dy2 = C(v,1)+lambda*dy-C(w,1);
                    TFloat dist = sqrt(dx2*dx2+dy2*dy2);

                    // Avoid collinear configurations of u, v and w
                    if (dist>epsilon && dist<gamma)
                    {
                        TFloat fak = (gamma-dist)*(gamma-dist)/dist/gamma*250.0;
                        pcx += dx2*fak;
                        pcy += dy2*fak;
                    }
                }
            }
            else
            {
                TFloat dx_uw = C(w,0)-C(u,0);
                TFloat dy_uw = C(w,1)-C(u,1);
                TFloat nsq = dx_uw*dx_uw+dy_uw*dy_uw;

                // Degenerate arcs
                if (nsq<epsilon) continue;

                TFloat lambda = (dx_uw*dx_uv+dy_uw*dy_uv)/nsq;

                // Minimum distance of v and a is achieved at an end point
                if (lambda>1+epsilon || lambda<-epsilon) continue;

                TFloat dx_ortho = lambda*dx_uw-dx_uv;
                TFloat dy_ortho = lambda*dy_uw-dy_uv;
                TFloat dist = sqrt(dx_ortho*dx_ortho+dy_ortho*dy_ortho);

                // Avoid collinear configurations of u, v and w
                if (dist>epsilon && dist<gamma)
                {
                    TFloat fak = (gamma-dist)*(gamma-dist)/dist/gamma*250.0;
                    pcx -= dx_ortho*fak;
                    pcy -= dy_ortho*fak;
                }
            }
        }


        // Update position and temperature
        TFloat npc = sqrt(pcx*pcx+pcy*pcy);

        if (npc>epsilon)
        {
            pcx = pcx/npc;
            pcy = pcy/npc;

            TFloat shift = InfFloat;

            // Occasionally reduce shift to maintain or to avoid edge crossings
            for (TArc a=0;a<m && (options & FDP_RESTRICTED);a++)
            {
                TNode u = StartNode(2*a);
                TNode w = EndNode(2*a);

                if (u==w) continue;

                if (v==u)
                {
                    u = w;
                    w = StartNode(2*a);
                }

                if (v==w)
                {
                    // When v is shifted, a may cross another vertex

                    TFloat dx_uv = C(v,0)-C(u,0);
                    TFloat dy_uv = C(v,1)-C(u,1);
                    TFloat nd_uv = sqrt(dx_uv*dx_uv+dy_uv*dy_uv);

                    for (TNode w=0;w<n;w++)
                    {
                        if (v==w || u==w) continue;

                        // Determine the multiple of (pcx,pcy) which meets the
                        // line spanned by u and w. Avoid to cross this line
                        // with v. That is: Solve v-u+mu*pc = lambda(w-u)

                        TFloat dx_vw = C(w,0)-C(v,0);
                        TFloat dy_vw = C(w,1)-C(v,1);
                        TFloat nd_vw = sqrt(dx_vw*dx_vw+dy_vw*dy_vw);

                        TFloat dx_uw = C(w,0)-C(u,0);
                        TFloat dy_uw = C(w,1)-C(u,1);
                        TFloat nd_uw = sqrt(dx_uw*dx_uw+dy_uw*dy_uw);

                        if (nd_vw<epsilon || nd_uv<epsilon || nd_uw<epsilon)
                        {
                            // u,v and w are collinear.

                            if (nd_vw>epsilon && dx_vw*pcx+dy_vw*pcy > nd_vw*(1-epsilon))
                            {
                                // pc and w-v point to the same direction
                                // if (shift>nd_vw-epsilon) shift = nd_vw-epsilon;
                            }

                            continue;
                        }

                        if (fabs(dx_uw*pcx+dy_uw*pcy) > nd_uw*(1-epsilon))
                        {
                            // pc and w-u are linearly dependent
                            continue;
                        }

                        TFloat lambda = (dx_uv*pcy-dy_uv*pcx)/(dx_uw*pcy-dy_uw*pcx);

                        if (lambda<1-epsilon)
                        {
                            // The edge a=uv cannot cross w
                            continue;
                        }

                        TFloat mu = 0;

                        if (fabs(pcx)>fabs(pcy))
                        {
                            mu = (lambda*dx_uw-dx_uv)/pcx;
                        }
                        else
                        {
                            mu = (lambda*dy_uw-dy_uv)/pcy;
                        }

                        if (mu<-epsilon)
                        {
                            // pc points away from the line uw
                            continue;
                        }


                        // Compute the minimum distance and leave some space
                        // between a and w. That is: Solve <v-u,v-nu(v-u)-w> = 0
                        TFloat nu = -(dx_uv*dx_vw+dy_uv*dy_vw)/(nd_uv*nd_uv);

                        TFloat dx_ortho = -dx_vw-nu*dx_uv;
                        TFloat dy_ortho = -dy_vw-nu*dy_uv;

                        if (nu<-epsilon)
                        {
                            // Minimum distance is achieved at node v
                            dx_ortho = -dx_vw;
                            dy_ortho = -dy_vw;
                        }
                        else if (nu>1+epsilon)
                        {
                            // Minimum distance is achieved at node u
                            dx_ortho = -dx_vw-dx_uv;
                            dy_ortho = -dy_vw-dy_uv;
                        }

                        TFloat dist = sqrt(dx_ortho*dx_ortho+dy_ortho*dy_ortho);

                        if (dist<4*epsilon)
                        {
                            // u,v and w are almost collinear
                            shift = 0;
                            break;
                        }

                        TFloat thisShift = dist*0.7; // mu*(dist-gamma)/dist;

                        // if (nu<-epsilon || nu>1+epsilon) thisShift = dist;

                        if (shift>thisShift) shift = thisShift;
                    }

                    continue;
                }

                // Determine the multiple of (pcx,pcy) which meets the line
                // spanned by a. Avoid to cross this line with v.
                // That is: Solve v+mu*pc = w+lambda(u-w)

                TFloat dx_vw = C(w,0)-C(v,0);
                TFloat dy_vw = C(w,1)-C(v,1);
                TFloat dx_uw = C(w,0)-C(u,0);
                TFloat dy_uw = C(w,1)-C(u,1);

                TFloat lambda = (dx_vw*pcy-dy_vw*pcx)/(dx_uw*pcy-dy_uw*pcx);

                if (lambda<-epsilon || lambda>1+epsilon)
                {
                    // The node v cannot cross a=uw
                    continue;
                }

                TFloat mu = 0;

                if (fabs(pcx)>fabs(pcy))
                {
                    mu = (dx_vw-lambda*dx_uw)/pcx;
                }
                else
                {
                    mu = (dy_vw-lambda*dy_uw)/pcy;
                }

                if (mu<-epsilon)
                {
                    // v cannot cross a by moving in the direction of (pcx,pcy)
                    continue;
                }

                // Compute the minimum distance and leave some space
                // between a and v. That is: Solve <w-u,w-nu(w-u)-v> = 0
                TFloat nsq = dx_uw*dx_uw+dy_uw*dy_uw;
                TFloat nu = 0;

                if (nsq>epsilon) nu = (dx_uw*dx_vw+dy_uw*dy_vw)/nsq;

                TFloat dx_ortho = dx_vw-nu*dx_uw;
                TFloat dy_ortho = dy_vw-nu*dy_uw;

                if (nu<-epsilon)
                {
                    // Minimum distance is achieved at node w
                    dx_ortho = dx_vw;
                    dy_ortho = dy_vw;
                }
                else if (nu>1+epsilon)
                {
                    // Minimum distance is achieved at node u
                    dx_ortho = dx_vw-dx_uw;
                    dy_ortho = dy_vw-dy_uw;
                }

                TFloat dist = sqrt(dx_ortho*dx_ortho+dy_ortho*dy_ortho);

                if (dist<4*epsilon)
                {
                    // u,v and w are almost collinear
                    shift = 0;
                    break;
                }

                TFloat thisShift = dist*0.7; // mu*(dist-gamma)/dist;

                // if (nu<-epsilon || nu>1+epsilon) thisShift = dist;

                if (shift>thisShift) shift = thisShift;
            }


            // Update position
            if (tCurrent[v]>shift)
            {
                SetC(v,0,C(v,0)+shift*pcx);
                SetC(v,1,C(v,1)+shift*pcy);

                sx += shift*pcx;
                sy += shift*pcy;
            }
            else
            {
                SetC(v,0,C(v,0)+tCurrent[v]*pcx);
                SetC(v,1,C(v,1)+tCurrent[v]*pcy);

                sx += tCurrent[v]*pcx;
                sy += tCurrent[v]*pcy;
            }

            npc = sqrt(px[v]*px[v]+py[v]*py[v]);

            if (shift>0 && npc>0.001)
            {
                tGlobal -= tCurrent[v];

                TFloat pox = px[v]/npc;
                TFloat poy = py[v]/npc;

                TFloat cosBeta = pcx*pox+pcy*poy;
                TFloat sinBeta = pcx*poy-pcy*pox;

                if (tCurrent[v]>shift)
                {
                    tCurrent[v] *= 0.8;
                }
                else if (fabs(cosBeta)>0.8)
                {
                    // Handle oscillations
                    if (cosBeta>0)
                    {
                        tCurrent[v] *= 1.1;
                    }
                    else tCurrent[v] *= 0.8;
                }
                else if (fabs(skewGauge[v]-sinBeta)<0.1)
                {
                    // Handle rotations
                    tCurrent[v] *= 0.9;
                }
                else if (fabs(skewGauge[v]-sinBeta)>1.0)
                {
                    tCurrent[v] *= 1.1;
                }

                if (tCurrent[v]>tMax) tCurrent[v] = tMax;

                skewGauge[v] = 0.7*skewGauge[v]+0.3*sinBeta;

                tGlobal += tCurrent[v];
            }
            else if (options & FDP_RESTRICTED)
            {
                tGlobal -= 0.8*tCurrent[v];
                tCurrent[v] *= 0.8;
            }

            if (shift==0) pcx = pcy = 0;
        }

        px[v] = pcx;
        py[v] = pcy;

        stepCount++;

        if (v==n-1)
        {
            sprintf(CT.logBuffer,"Temperature now at %g",tGlobal/n);
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        if (CT.traceLevel>2 && IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) -> Layout_ArcRouting();
        }

        #if defined(_PROGRESS_)

        M.SetProgressCounter((unsigned long)(
                            (1-(tGlobal/n-tMin)/(tMax-tMin))*1000000));

        #endif

        M.Trace();
    }

    Layout_FreezeBoundingBox();

    delete[] skewGauge;
    delete[] tCurrent;
    delete[] px;
    delete[] py;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu iterations in total",stepCount-1);
        LogEntry(LOG_RES,CT.logBuffer);
        sprintf(CT.logBuffer,"...final temperature is %g",tGlobal/n);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
}


void abstractMixedGraph::Layout_LayeredFDP(TOptFDP method,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_LayeredFDP","Coordinates are fixed");

    #endif

    moduleGuard M(ModLayeredFDP,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(1000000);

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    TFloat delta = spacing;   // Optimal edge length
    TFloat epsilon = 0.001;      // Numeric separation constant;

    TFloat tInit = delta/10;
    TFloat tMin = delta/250;
    TFloat tMax = delta/4;
    TFloat tGlobal = tInit*n;
    TFloat* tCurrent = new TFloat[n];
    TFloat* px = new TFloat[n];
    TFloat sx = 0;

    for (TNode v=0;v<n;v++)
    {
        tCurrent[v] = tInit;
        px[v] = 0;
        sx += C(v,0);
    }

    unsigned long stepCount = 0;

    while (CT.SolverRunning() && tGlobal>tMin*n && stepCount<n*300.0)
    {
        // Choose a vertex v to update. Replace by a more sophisticated
        // scheme later
        TNode v = stepCount%n;

        // Compute impulse
        TFloat pcx = 0;
        TFloat minX = -InfFloat;
        TFloat maxX = InfFloat;


        // Force attracting to the mean value in case of multiple components
        TFloat dx = C(v,0)-sx/n;

        if (fabs(dx)>epsilon)
        {
            pcx -= dx/delta;
        }

        // Forces repelling from other nodes in the same layer
        for (TNode w=0;w<n;w++)
        {
            if (v==w) continue;

            TFloat dy = C(v,1)-C(w,1);

            if (fabs(dy)>epsilon) continue;

            TFloat wx = C(w,0);
            TFloat dx = C(v,0)-wx;

            if (method==FDP_LAYERED_RESTR)
            {
                if (wx<maxX && wx>C(v,0))
                {
                    maxX = wx;
                }
                else if (wx>minX && wx<C(v,0))
                {
                    minX = wx;
                }
            }

            if (fabs(dx)>epsilon)
            {
                pcx += 10*delta/dx;
            }
        }

        // Attracting forces
        TArc a = First(v);
        while (a!=NoArc)
        {
            TNode w = EndNode(a);

            TFloat dx = C(v,0)-C(w,0);

            if (fabs(dx)>epsilon)
            {
                pcx -= dx/delta;
            }

            a = Right(a,v);

            if (a==First(v)) break;
        }

        // Update position and temperature
        TFloat newX = C(v,0)+tCurrent[v]*pcx;

        if (newX<(C(v,0)+2*minX)/3.0)
        {
            SetC(v,0,(C(v,0)+2*minX)/3.0);
        }
        else if (newX>(C(v,0)+2*maxX)/3.0)
        {
            SetC(v,0,(C(v,0)+2*maxX)/3.0);
        }
        else
        {
            sx -= C(v,0);
            sx += newX;
            SetC(v,0,newX);
        }


        if (true || fabs(pcx)>0.001)
        {
            tGlobal -= tCurrent[v];

            if (px[v]*pcx<=0 || fabs(pcx)<tMin)
            {
                tCurrent[v] *= 0.7;
            }
            else if (fabs(pcx)>0.5*fabs(px[v]))
            {
                tCurrent[v] *= 1.1;
            }

            if (tCurrent[v]>tMax) tCurrent[v] = tMax;

            tGlobal += tCurrent[v];
        }

        px[v] = pcx;

        stepCount++;

        if (v==n-1)
        {
            sprintf(CT.logBuffer,"Temperature now at %g",tGlobal/n);
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        if (CT.traceLevel>2 && IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) -> Layout_ArcRouting();
            // Here, a bounding box for display should be computed
        }

        #if defined(_PROGRESS_)

        M.SetProgressCounter((unsigned long)(
                            (1-(tGlobal/n-tMin)/(tMax-tMin))*1000000));

        #endif

        M.Trace();
    }

    delete[] tCurrent;
    delete[] px;

    Layout_DefaultBoundingBox();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu iterations in total",stepCount-1);
        LogEntry(LOG_RES,CT.logBuffer);
        sprintf(CT.logBuffer,"...final temperature is %g",tGlobal/n);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
}
