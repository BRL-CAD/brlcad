
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   ilpCompose.cpp
/// \brief  All problem transformations to linear programs

#include "ilpWrapper.h"
#include "fileExport.h"
#include "abstractMixedGraph.h"


mipInstance* mipInstance::Clone() throw(ERRejected)
{
    mipFactory* theMipFactory = (mipFactory*)CT.pMipFactory;
    mipInstance* XLP  =
        theMipFactory->NewInstance(K(),L(),NZ(),ObjectSense(),CT);


    for (TVar i=0;i<L();i++)
    {
        XLP -> AddVar(LRange(i),URange(i),Cost(i),VarType(i));
    }


    TIndex* index = new TIndex[L()];
    TFloat* val   = new TFloat[L()];

    for (TRestr i=0;i<K();i++)
    {
        XLP -> AddRestr(LBound(i),UBound(i));

        TIndex nz = GetRow(i,index,val);
        XLP -> SetRow(i,nz,index,val);
    }

    delete[] index;
    delete[] val;


    XLP -> ResetBasis();

    for (TVar j=0;j<L();j++)
    {
        TRestr i = Index(j);
        XLP -> SetIndex(i,j,TLowerUpper(RestrType(i)));
    }

    return XLP;
}


mipInstance* mipInstance::DualForm() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    for (TVar i=0;i<L();i++)
        if (VarType(i)==VAR_INT)
            Error(ERR_REJECTED,"DualForm","Integer variable found");

    #endif


    mipFactory* theMipFactory = (mipFactory*)CT.pMipFactory;
    mipInstance* XLP  =
        theMipFactory->NewInstance(L(),2*K(),NZ(),MAXIMIZE,CT);

    TFloat sign = 1;

    if (ObjectSense()==MAXIMIZE)
    {
        XLP->SetObjectSense(MINIMIZE);
        sign = -1;
    }


    TVar j = 0;

    TIndex* index = new TIndex[L()];
    TFloat* val   = new TFloat[L()];

    for (TVar i=0;i<L();i++)
    {
        // Transform variables to structural restrictions

        XLP -> AddRestr(Cost(i),Cost(i));

        if (strlen(VarLabel(i))+3>bufferLength)
        {
            bufferLength = strlen(VarLabel(i))+3;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        sprintf(labelBuffer,"%s",VarLabel(i));
        XLP -> SetRestrLabel(i,labelBuffer,OWNED_BY_SENDER);

        if (ObjectSense()==MINIMIZE)
        {
            if (URange(i)==0)
                XLP -> SetUBound(i,InfFloat);
            else if (LRange(i)==0)
                XLP -> SetLBound(i,-InfFloat);
        }
        else
        {
            if (URange(i)==0)
                XLP -> SetLBound(i,-InfFloat);
            else if (LRange(i)==0)
                XLP -> SetUBound(i,InfFloat);
        }
    }


    for (TRestr i=0;i<K();i++)
    {
        // Transform structural restrictions to variables

        if (UBound(i)==InfFloat && LBound(i)==-InfFloat) continue;

        TIndex nz = GetRow(i,index,val);

        if (strlen(RestrLabel(i))+2>bufferLength)
        {
            bufferLength = strlen(RestrLabel(i))+2;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        if (LBound(i)>-InfFloat && LBound(i)<UBound(i))
        {
            if (ObjectSense()==MINIMIZE)
                XLP -> AddVar(0,InfFloat,LBound(i));
            else
                XLP -> AddVar(-InfFloat,0,LBound(i));

            XLP -> SetColumn(j,nz,index,val);

            if (UBound(i)<InfFloat)
                sprintf(labelBuffer,"%sl",RestrLabel(i));
            else sprintf(labelBuffer,"%s",RestrLabel(i));

            XLP -> SetVarLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }

        if (UBound(i)<InfFloat)
        {
            if (LBound(i)<UBound(i))
            {
                if (ObjectSense()==MINIMIZE)
                    XLP -> AddVar(-InfFloat,0,UBound(i));
                else
                    XLP -> AddVar(0,InfFloat,UBound(i));
            }
            else
            {
                XLP -> AddVar(-InfFloat,InfFloat,UBound(i));
            }

            XLP -> SetColumn(j,nz,index,val);

            if (LBound(i)>-InfFloat && LBound(i)<UBound(i))
                sprintf(labelBuffer,"%su",RestrLabel(i));
            else sprintf(labelBuffer,"%s",RestrLabel(i));

            XLP -> SetVarLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }
    }


    for (TVar i=0;i<L();i++)
    {
        // Transform variable range restrictions

        if (URange(i)==InfFloat && LRange(i)==-InfFloat) continue;
        if (LRange(i)==-InfFloat && URange(i)==0) continue;
        if (URange(i)==InfFloat && LRange(i)==0) continue;

        index[0] = i;
        val[0] = 1;

        if (LRange(i)>-InfFloat && (LRange(i)!=0 || LRange(i)==URange(i)))
        {
            if (ObjectSense()==MINIMIZE)
                XLP -> AddVar(0,InfFloat,LRange(i));
            else
                XLP -> AddVar(-InfFloat,0,LRange(i));

            XLP -> SetColumn(j,1,index,val);

            if (URange(i)<InfFloat && URange(i)!=0)
                sprintf(labelBuffer,"a%sl",VarLabel(i));
            else sprintf(labelBuffer,"a%s",VarLabel(i));

            XLP -> SetVarLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }

        if (URange(i)<InfFloat && URange(i)!=0)
        {
            if (LRange(i)<URange(i))
            {
                if (ObjectSense()==MINIMIZE)
                    XLP -> AddVar(-InfFloat,0,URange(i));
                else
                    XLP -> AddVar(0,InfFloat,URange(i));
            }
            else
            {
                XLP -> AddVar(-InfFloat,InfFloat,URange(i));
            }

            XLP -> SetColumn(j,1,index,val);

            if (LRange(i)>-InfFloat && (LRange(i)!=0 || LRange(i)==URange(i)))
                sprintf(labelBuffer,"a%su",VarLabel(i));
            else sprintf(labelBuffer,"a%s",VarLabel(i));

            XLP -> SetVarLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }
    }

    XLP -> ResetBasis();


    delete[] index;
    delete[] val;

    return XLP;
}


mipInstance* mipInstance::StandardForm() throw(ERRejected)
{
    mipFactory* theMipFactory = (mipFactory*)CT.pMipFactory;
    mipInstance* XLP  =
        theMipFactory->NewInstance(2*K()+L()+1,2*K()+L()+1,2*(NZ()+L()),ObjectSense(),CT);

    TVar* index = new TVar[L()];
    TFloat* val = new TFloat[L()];
    TVar* map = new TVar[2*L()];


    // Transform original variables, ranges and objective function

    TRestr k = 0;       // Number of restrictions in the transformed LP
    TVar l = 0;         // Number of variable in the transformed LP
    TFloat offset = 0;  // Constant to achieve the same optimum in both LPs

    for (TVar i=0;i<L();i++)
    {
        if (LRange(i)==URange(i))
        {
            // Variable can be omitted

            offset += Cost(i)*LRange(i);

            map[2*i] = map[2*i+1] = NoVar;

            continue;
        }

        if (strlen(VarLabel(i))+3>bufferLength)
        {
            bufferLength = strlen(VarLabel(i))+3;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        if (LRange(i)>-InfFloat)
        {
            XLP -> AddVar(0,InfFloat,Cost(i),VarType(i));

            sprintf(labelBuffer,"%s",VarLabel(i));
            XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

            offset += Cost(i)*LRange(i);

            map[2*i] = l;
            map[2*i+1] = NoVar;

            if (URange(i)<InfFloat)
            {
                // Add explicit restriction for upper variable bound

                XLP -> AddRestr(URange(i)-LRange(i),URange(i)-LRange(i));
                XLP -> SetCoeff(k,l,1);

                sprintf(labelBuffer,"r%s",VarLabel(i));
                XLP -> SetRestrLabel(k,labelBuffer,OWNED_BY_SENDER);

                // Add slack variable

                l++;

                XLP -> AddVar(0,InfFloat,0,VAR_FLOAT);
                XLP -> SetCoeff(k,l,1);

                sprintf(labelBuffer,"s%sp",VarLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                k++;
            }

            l++;
        }
        else
        {
            if (URange(i)<InfFloat)
            {
                XLP -> AddVar(0,InfFloat,-Cost(i),VarType(i));

                sprintf(labelBuffer,"%s",VarLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                offset += Cost(i)*URange(i);

                map[2*i] = NoVar;
                map[2*i+1] = l;

                l++;
            }
            else
            {
                // Requires two non-negative variables

                XLP -> AddVar(0,InfFloat,Cost(i),VarType(i));

                sprintf(labelBuffer,"%sp",VarLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                map[2*i] = l;

                l++;

                XLP -> AddVar(0,InfFloat,-Cost(i),VarType(i));

                sprintf(labelBuffer,"%sn",VarLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                map[2*i+1] = l;

                l++;
            }
        }
    }


    // Add structural restrictions and respective slack variables

    for (TRestr i=0;i<K();i++)
    {
        if (UBound(i)==InfFloat && LBound(i)==-InfFloat) continue;

        // Determine right hand side

        TIndex nz = GetRow(i,index,val);
        TFloat RHS = 0;

        for (TIndex j=0;j<nz;j++)
        {
            if (LRange(index[j])>-InfFloat)
                RHS += LRange(index[j])*val[j];
            else if (URange(index[j])<InfFloat)
                RHS += URange(index[j])*val[j];
        }

        if (strlen(RestrLabel(i))+3>bufferLength)
        {
            bufferLength = strlen(RestrLabel(i))+3;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        if (UBound(i)>LBound(i))
        {
            if (UBound(i)<InfFloat)
            {
                // Replace "less or equal" condition by using slack variable

                XLP -> AddVar(0,InfFloat,0,VAR_FLOAT);

                sprintf(labelBuffer,"s%sp",RestrLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                // Add equality restriction

                XLP -> AddRestr(UBound(i)-RHS,UBound(i)-RHS);

                sprintf(labelBuffer,"%su",RestrLabel(i));
                XLP -> SetRestrLabel(k,labelBuffer,OWNED_BY_SENDER);

                XLP -> SetCoeff(k,l,1);

                // Copy matrix coefficients

                for (TIndex j=0;j<nz;j++)
                {
                    if (map[2*index[j]]!=NoVar)
                        XLP -> SetCoeff(k,map[2*index[j]],val[j]);

                    if (map[2*index[j]+1]!=NoVar)
                        XLP -> SetCoeff(k,map[2*index[j]+1],-val[j]);
                }

                k++;
                l++;
            }

            if (LBound(i)>-InfFloat)
            {
                // Replace "greater or equal" condition using a slack variable

                XLP -> AddVar(0,InfFloat,0,VAR_FLOAT);

                sprintf(labelBuffer,"s%sn",RestrLabel(i));
                XLP -> SetVarLabel(l,labelBuffer,OWNED_BY_SENDER);

                // Add equality restriction

                XLP -> AddRestr(LBound(i)-RHS,LBound(i)-RHS);

                sprintf(labelBuffer,"%sl",RestrLabel(i));
                XLP -> SetRestrLabel(k,labelBuffer,OWNED_BY_SENDER);

                XLP -> SetCoeff(k,l,-1);

                // Copy matrix coefficients

                for (TIndex j=0;j<nz;j++)
                {
                    if (map[2*index[j]]!=NoVar)
                        XLP -> SetCoeff(k,map[2*index[j]],val[j]);

                    if (map[2*index[j]+1]!=NoVar)
                        XLP -> SetCoeff(k,map[2*index[j]+1],-val[j]);
                }

                k++;
                l++;
            }
        }
        else
        {
            // Already an equality restriction

            XLP -> AddRestr(LBound(i)-RHS,LBound(i)-RHS);

            sprintf(labelBuffer,"%s",RestrLabel(i));
            XLP -> SetRestrLabel(k,labelBuffer,OWNED_BY_SENDER);

            // Copy matrix coefficients

            for (TIndex j=0;j<nz;j++)
            {
                if (map[2*index[j]]!=NoVar)
                    XLP -> SetCoeff(k,map[2*index[j]],val[j]);

                if (map[2*index[j]+1]!=NoVar)
                    XLP -> SetCoeff(k,map[2*index[j]+1],-val[j]);
            }

            k++;
        }
    }


    // Add dummy variable for the constant offset

    TFloat sign = 1;
    if (offset<0) sign = -1;

    XLP -> AddVar(0,InfFloat,sign,VAR_FLOAT);
    XLP -> SetVarLabel(l,"xofs",OWNED_BY_SENDER);

    XLP -> AddRestr(offset,offset);
    XLP -> SetRestrLabel(k,"rofs",OWNED_BY_SENDER);

    XLP -> SetCoeff(k,l,sign);

    k++;
    l++;


    XLP -> ResetBasis();


    delete[] index;
    delete[] val;
    delete[] map;

    return XLP;
}


mipInstance* mipInstance::CanonicalForm() throw(ERRejected)
{
    mipFactory* theMipFactory = (mipFactory*)CT.pMipFactory;
    mipInstance* XLP  =
        theMipFactory->NewInstance(2*(K()+L()),L(),2*(NZ()+L()),ObjectSense(),CT);

    TVar* index = new TVar[L()];
    TFloat* val = new TFloat[L()];
    TRestr j = 0;

    // Transform variables and objective function

    for (TVar i=0;i<L();i++)
    {
        XLP -> AddVar(-InfFloat,InfFloat,Cost(i),VarType(i));

        if (strlen(VarLabel(i))+2>bufferLength)
        {
            bufferLength = strlen(VarLabel(i))+2;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        sprintf(labelBuffer,"%s",VarLabel(i));
        XLP -> SetVarLabel(i,labelBuffer,OWNED_BY_SENDER);
    }

    // Transform structural restrictions

    for (TRestr i=0;i<K();i++)
    {
        if (UBound(i)==InfFloat && LBound(i)==-InfFloat) continue;

        TIndex nz = GetRow(i,index,val);

        if (strlen(RestrLabel(i))+2>bufferLength)
        {
            bufferLength = strlen(RestrLabel(i))+2;
            labelBuffer = (char*)GoblinRealloc(labelBuffer,bufferLength);
        }

        if (UBound(i)<InfFloat)
        {
            XLP -> AddRestr(-InfFloat,UBound(i));
            XLP -> SetRow(j,nz,index,val);

            if (LBound(i)>-InfFloat)
                sprintf(labelBuffer,"%su",RestrLabel(i));
            else sprintf(labelBuffer,"%s",RestrLabel(i));
            XLP -> SetRestrLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }

        if (LBound(i)>-InfFloat)
        {
            for (TVar k=0;k<nz;k++) val[k] *= -1;

            XLP -> AddRestr(-InfFloat,-LBound(i));
            XLP -> SetRow(j,nz,index,val);

            sprintf(labelBuffer,"%sl",RestrLabel(i));
            XLP -> SetRestrLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }
    }

    // Transform variable range restrictions

    for (TVar i=0;i<L();i++)
    {
        if (URange(i)==InfFloat && LRange(i)==-InfFloat) continue;

        if (URange(i)<InfFloat)
        {
            XLP -> AddRestr(-InfFloat,URange(i));
            XLP -> SetCoeff(j,i,1);

            sprintf(labelBuffer,"%su",VarLabel(i));
            XLP -> SetRestrLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }

        if (LRange(i)>-InfFloat)
        {
            XLP -> AddRestr(-InfFloat,-LRange(i));
            XLP -> SetCoeff(j,i,-1);

            sprintf(labelBuffer,"%sl",VarLabel(i));
            XLP -> SetRestrLabel(j,labelBuffer,OWNED_BY_SENDER);

            j++;
        }
    }

    XLP -> ResetBasis();

    delete[] index;
    delete[] val;

    return XLP;
}


void mipInstance::FlipObjectSense() throw()
{
    if (ObjectSense()==MAXIMIZE)
    {
        SetObjectSense(MINIMIZE);
    }
    else if (ObjectSense()==MINIMIZE)
    {
        SetObjectSense(MAXIMIZE);
    }

    for (TVar i=0;i<L();i++) SetCost(i,-Cost(i));
}


managedObject* abstractMixedGraph::BFlowToLP() throw()
{
    mipFactory *theMipFactory =
        (mipFactory*)goblinController::pMipFactory;
    mipInstance *XLP  =
        theMipFactory->NewInstance(n,m,2*m,mipInstance::MINIMIZE,CT);

    for (TNode v=0;v<n;v++)
    {
        XLP -> AddRestr(-Demand(v),-Demand(v));
    }

    TRestr rowIndex[2] = {NoRestr,NoRestr};
    TFloat rowCoeff[2] = {1,-1};

    for (TArc a=0;a<m;a++)
    {
        XLP -> AddVar(LCap(2*a),UCap(2*a),Length(2*a),
                        mipInstance::VAR_FLOAT);

        rowIndex[0] = StartNode(2*a);
        rowIndex[1] = EndNode(2*a);
        XLP -> SetColumn(a,2,rowIndex,rowCoeff);
    }


    XLP -> ResetBasis();

    return XLP;
}


managedObject* abstractMixedGraph::VerticalCoordinatesModel(TNode* nodeLayer) throw()
{
    TArc* edgeColour = GetEdgeColours();

    if (!edgeColour) return NULL;

    mipFactory *theMipFactory =
        (mipFactory*)goblinController::pMipFactory;
    mipInstance *XLP  =
        theMipFactory->NewInstance(m,n,2*m,mipInstance::MINIMIZE,CT);

    for (TVar v=0;v<TVar(n);v++)
    {
        TFloat div = 0;
        TArc a = First(v);

        if (a!=NoArc)
        {
            do
            {
                if (   (!(edgeColour[a>>1]&1) && !(a&1))
                    || ( (edgeColour[a>>1]&1) &&  (a&1))
                   )
                {
                    div -= 1;
                }
                else
                {
                    div += 1;
                }

                a = Right(a,v);
            }
            while (a!=First(v));
        }

        if (nodeLayer && nodeLayer[v]!=NoNode)
        {
            XLP -> AddVar(nodeLayer[v],nodeLayer[v],div,mipInstance::VAR_FLOAT);
        }
        else
        {
            XLP -> AddVar(0,InfFloat,div,mipInstance::VAR_FLOAT);
        }
    }

    TRestr columnIndex[2] = {NoRestr,NoRestr};
    TFloat columnCoeff[2] = {-1,1};

    for (TArc a=0;a<m;a++)
    {
        if (!(edgeColour[a]&1))
        {
            columnIndex[0] = StartNode(2*a);
            columnIndex[1] = EndNode(2*a);
        }
        else
        {
            columnIndex[0] = StartNode(2*a+1);
            columnIndex[1] = EndNode(2*a+1);
        }

        if (columnIndex[0]!=columnIndex[1])
        {
            XLP -> AddRestr(1,InfFloat);
            XLP -> SetRow(a,2,columnIndex,columnCoeff);
        }
    }

    XLP -> ResetBasis();

    return XLP;
}


managedObject* abstractMixedGraph::HorizontalCoordinatesModel() throw()
{
    mipFactory *theMipFactory =
        (mipFactory*)goblinController::pMipFactory;
    mipInstance *XLP  =
        theMipFactory->NewInstance(2*m+n,n+m,2*m,mipInstance::MINIMIZE,CT);

    for (TVar v=0;v<TVar(n);v++)
    {
        // Add a variable for every graph node. Set the cost coefficient
        // to the sum of lengths of the incident arcs.
        TFloat thisLength = 0;
        TArc a = First(v);

        if (a!=NoArc)
        {
            do
            {
                thisLength += Length(a&(a^1));
                a = Right(a,v);
            }
            while (a!=First(v));
        }

        XLP -> AddVar(0,InfFloat,thisLength,mipInstance::VAR_FLOAT);
    }

    for (TArc a=0;a<m;a++)
    {
        // Add a variable for every graph edge. Use the original length label
        XLP -> AddVar(0,InfFloat,-2*Length(2*a),mipInstance::VAR_FLOAT);
    }

    TRestr columnIndex[2] = {NoRestr,NoRestr};
    TFloat columnCoeff[2] = {-1,1};

    // Add two constraints for every graph edge. The objective function
    // describes the absolute difference between the variable values for
    // both end nodes, weighted with the original edge length label
    for (TArc a=0;a<2*m;a++)
    {
        columnIndex[0] = n+(a>>1);
        columnIndex[1] = EndNode(a);

        XLP -> AddRestr(0,InfFloat);
        XLP -> SetRow(a,2,columnIndex,columnCoeff);
    }

    // Add constraints for separating the nodes in the same layer.
    // By these, the relative order of nodes in each layer is maintained
    for (TVar v=0;v<TVar(n);v++)
    {
        TNode predNode = NoNode;
        TFloat thisX = C(v,0);

        for (TVar w=0;w<TVar(n);w++)
        {
            // Tie breaking for conicident nodes: Sort by the node indices.
            // Due to the fact that Layout_SubdivideArcs() adds control points
            // arc by arc, a given pair of arcs can cross only once
            if (   fabs(C(w,1)-C(v,1))<CT.epsilon
                && (   C(w,0)<thisX
                    || (C(w,0)==thisX && w<v)
                   )
                && (
                       predNode==NoNode
                    || C(w,0)>C(predNode,0)
                    || (C(w,0)==C(predNode,0) && w>predNode)
                   )
               )
            {
                predNode = w;
            }
        }

        if (predNode!=NoNode)
        {
            columnIndex[0] = predNode;
            columnIndex[1] = v;
            XLP -> AddRestr(1,InfFloat);
            XLP -> SetRow(2*m+v,2,columnIndex,columnCoeff);
        }
        else
        {
            // Dummy restriction in order to obtain obvious mappings
            // between the graph and the LP instance
            columnIndex[0] = v;
            XLP -> AddRestr(-InfFloat,0);
            XLP -> SetRow(2*m+v,1,columnIndex,columnCoeff);
        }
    }

    XLP -> ResetBasis();

    return XLP;
}


managedObject* abstractMixedGraph::StableSetToMIP() throw()
{
    TNode* nodeColour = GetNodeColours();

    // Compute a clique cover

    int savedMaxBBIterations = CT.maxBBIterations;
    CT.maxBBIterations = 0;
    TNode nColours = CliqueCover();
    CT.maxBBIterations = savedMaxBBIterations;

    TIndex mShrunk = 0;
    for (TRestr i=0;i<TRestr(m);i++)
    {
        TNode u = StartNode(2*TArc(i));
        TNode v = EndNode(2*TArc(i));

        if (nodeColour && nodeColour[u]==nodeColour[v]) mShrunk++;
    }


    mipFactory* theMipFactory = (mipFactory*)CT.pMipFactory;
    mipInstance* XLP  =
        theMipFactory->NewInstance(m-mShrunk+nColours,n,2*m,MAXIMIZE,CT);


    for (TVar i=0;i<TVar(n);i++)
    {
        XLP -> AddVar(0,1,1,mipInstance::VAR_INT);
    }


    TIndex* index = new TIndex[n];
    TFloat* val = new TFloat[n];

    for (TNode v=0;v<n;v++) val[v] = 1;

    TRestr rowCount = 0;


    // Add a restriction for the edges not in the clique cover. Suppress parallel edges

    for (TRestr i=0;i<TRestr(m);i++)
    {
        TNode u = StartNode(2*TArc(i));
        TNode v = EndNode(2*TArc(i));

        if (nodeColour && nodeColour[u]==nodeColour[v]) continue;

        if (Adjacency(u,v)!=2*TArc(i)) continue;

        index[0] = TIndex(u);
        index[1] = TIndex(v);

        XLP -> AddRestr(0,1);
        XLP -> SetRow(rowCount++,2,index,val);
    }


    // Add a restriction for each clique

    for (TNode c=0;c<nColours;c++)
    {
        TNode cardinality = 0;

        for (TNode v=0;v<n;v++)
            if (nodeColour && nodeColour[v]==c) index[cardinality++] = TIndex(v);

        if (cardinality>1)
        {
            XLP -> AddRestr(0,1);
            XLP -> SetRow(rowCount++,cardinality,index,val);
        }
    }

    XLP -> ResetBasis();

    delete[] index;
    delete[] val;

    return XLP;
}
