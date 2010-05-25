
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpRepresentation.cpp
/// \brief  #goblinLPSolver partial class implementation

#include "lpSolver.h"
#include "fileImport.h"


goblinLPSolver::goblinLPSolver(TRestr kk,TVar ll,TIndex size,
    TObjectSense cDir,goblinController& thisContext) throw() :
    mipInstance(thisContext)
{
    if (kk>0) kMax = kk; else kMax = 1;
    if (ll>0) lMax = ll; else lMax = 1;
    if (size==0) size++;
    kAct = 0;
    lAct = 0;

    cCost    = 0;
    cUBound  = InfFloat;
    cLBound  = -InfFloat;
    cURange  = InfFloat;
    cLRange  = -InfFloat;
    cVarType = VAR_FLOAT;

    coeff = new goblinHashTable<TIndex,TFloat>(kMax*lMax,size,0,CT);

    cost       = NULL;
    uBound     = NULL;
    lBound     = NULL;
    uRange     = NULL;
    lRange     = NULL;
    varType    = NULL;
    restrLabel = NULL;
    varLabel   = NULL;

    varIndex   = NULL;
    restrIndex = NULL;

    restrType = new TRestrType[kMax+lMax];
    index     = new TVar[lMax];
    revIndex  = new TVar[kMax+lMax];
    baseInv   = NULL;
    keptInv   = NULL;
    x         = NULL;
    y         = NULL;

    baseInitial = true;
    baseValid  = false;
    dataValid  = false;

    for (TIndex i=0;i<kMax+lMax;i++)
    {
        restrType[i] = NON_BASIC;
        revIndex[i]  = NoVar;
        if (i<lMax) index[i] = NoRestr;
    }

    dir = cDir;

    LogEntry(LOG_MEM,"...Native LP allocated");
}


goblinLPSolver::goblinLPSolver(const char* impFileName,
    goblinController& thisContext) throw(ERFile,ERParse) :
    mipInstance(thisContext)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading native LP...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading native LP...");

    goblinImport F(impFileName,CT);

    F.Scan("mixed_integer");

    F.Scan("rows");
    TIndex* nrows = F.GetTIndexTuple(1);
    kMax = nrows[0];
    delete[] nrows;

    F.Scan("columns");
    TIndex* ncols = F.GetTIndexTuple(1);
    lMax = ncols[0];
    delete[] ncols;

    F.Scan("size");
    TIndex* nzero = F.GetTIndexTuple(1);
    TIndex size = nzero[0];
    delete[] nzero;

    if (kMax==0) kMax = 1;
    if (lMax==0) lMax = 1;
    if (size==0) size++;

    kAct = lAct = 0;

    cCost    = 0;
    cUBound  = InfFloat;
    cLBound  = -InfFloat;
    cURange  = InfFloat;
    cLRange  = -InfFloat;
    cVarType = VAR_FLOAT;

    coeff = new goblinHashTable<TIndex,TFloat>(kMax*lMax,size,0,CT);

    cost       = NULL;
    uBound     = NULL;
    lBound     = NULL;
    uRange     = NULL;
    lRange     = NULL;
    varType    = NULL;
    restrLabel = NULL;
    varLabel   = NULL;

    varIndex   = NULL;
    restrIndex = NULL;

    restrType = new TRestrType[kMax+lMax];
    index     = new TVar[lMax];
    revIndex  = new TVar[kMax+lMax];
    baseInv   = NULL;
    keptInv   = NULL;
    x         = NULL;
    y         = NULL;

    baseInitial = true;
    baseValid  = false;
    dataValid  = false;

    for (TIndex i=0;i<kMax+lMax;i++)
    {
        restrType[i] = NON_BASIC;
        revIndex[i]  = NoVar;
        if (i<lMax) index[i] = NoRestr;
    }

    F.Scan("pivot");
    TIndex* pPivot = F.GetTIndexTuple(3);

    if (pPivot[0]==NoIndex)
    {
        pivotRow = NoRestr;
        pivotColumn = NoVar;
        pivotDir = LOWER;
    }
    else
    {
        pivotRow = pPivot[0];
        pivotColumn = pPivot[1];
        pivotDir = TLowerUpper(pPivot[2]);
    }

    delete[] pPivot;

    ReadVarValues(&F,lMax);

    // Variable values are released by the following operations
    TFloat* swapVarValue = varValue;
    TVar swapNumVars = numVars;
    varValue = NULL;

    F.Scan("rowvis");
    char* pDummy = F.GetCharTuple(kAct);
    delete[] pDummy;

    F.Scan("colvis");
    pDummy = F.GetCharTuple(lAct);
    delete[] pDummy;

    F.Scan("configure");
    F.ReadConfiguration();

    F.Scan(); // mixed_integer

    ReadMPSFile(F.Stream());
    ReadBASFile(F.Stream());

    int l = strlen(impFileName)-4;
    char* tmpLabel = new char[l+1];
    memcpy(tmpLabel,impFileName,l);
    tmpLabel[l] = 0;
    SetLabel(tmpLabel);
    delete[] tmpLabel;
    CT.SetMaster(Handle());

    varValue = swapVarValue;
    numVars = swapNumVars;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


goblinLPSolver::~goblinLPSolver() throw()
{
    delete[] cost;
    delete[] uBound;
    delete[] lBound;
    delete[] uRange;
    delete[] lRange;
    delete[] varType;
    delete[] restrLabel;
    delete[] varLabel;

    if (varIndex) delete varIndex;
    if (restrIndex) delete restrIndex;

    delete[] restrType;
    delete[] index;
    delete[] revIndex;
    delete baseInv;
    delete keptInv;
    delete[] x;
    delete[] y;

    delete coeff;

    LogEntry(LOG_MEM,"...Native LP disallocated");
}


unsigned long goblinLPSolver::Size() const throw()
{
    return
          sizeof(goblinLPSolver)
        + managedObject::Allocated()
        + goblinLPSolver::Allocated();
}


unsigned long goblinLPSolver::Allocated() const throw()
{
    unsigned long tmpSize = (2*kMax+3*lMax)*(sizeof(TRestrType)+sizeof(TVar));
      // restrType, index, revIndex

    if (cost!=NULL)         tmpSize += lMax*sizeof(TFloat);
    if (uBound!=NULL)       tmpSize += kMax*sizeof(TFloat);
    if (lBound!=NULL)       tmpSize += kMax*sizeof(TFloat);
    if (uRange!=NULL)       tmpSize += lMax*sizeof(TFloat);
    if (lRange!=NULL)       tmpSize += lMax*sizeof(TFloat);
    if (varType!=NULL)      tmpSize += lMax*sizeof(TVarType);

    if (restrLabel!=NULL)
    {
        tmpSize += kMax*sizeof(char);
        for (TRestr i=0;i<kAct;i++)
            if (restrLabel[i]!=NULL)
                tmpSize += sizeof(char)*(strlen(restrLabel[i])+1);
    }

    if (varLabel!=NULL)
    {
        tmpSize += lMax*sizeof(char);
        for (TVar i=0;i<lAct;i++)
            if (varLabel[i]!=NULL)
                tmpSize += sizeof(char)*(strlen(varLabel[i])+1);
    }

    if (baseValid)          tmpSize += (kAct+2*lAct)*sizeof(TFloat);

    return tmpSize;
}


TRestr goblinLPSolver::K() const throw()
{
    return kAct;
}


TVar goblinLPSolver::L() const throw()
{
    return lAct;
}


TIndex goblinLPSolver::NZ() const throw()
{
    return coeff->NZ();
}


TRestr goblinLPSolver::AddRestr(TFloat l,TFloat u)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (l>u) Error(ERR_REJECTED,"AddRestr","Incompatible bounds");

    #endif

    if (kAct==kMax) Resize(2*kMax,2*lAct,coeff->NMax());

    kAct++;

    try
    {
        if (UBound(kAct-1)!=InfFloat)
        {
            SetLBound(kAct-1,-InfFloat);
            SetUBound(kAct-1,u);
            SetLBound(kAct-1,l);
        }
        else
        {
            SetLBound(kAct-1,l);
            SetUBound(kAct-1,u);
        }

        restrType[kAct-1] = NON_BASIC;
    }
    catch (ERRange)
    {
        kAct--;
        throw ERRejected();
    }

    revIndex[kAct-1] = NoVar;

    for (TVar j=0;j<lAct;j++) SetCoeff(kAct-1,j,0);

    if (y!=NULL)
    {
        // Extend dual solution

        TFloat *y2 = new TFloat[kAct+lAct];

        if (baseValid)
        {
            for (TRestr j=0;j<kAct-1;j++) y2[j] = y[j];

            y2[kAct-1] = 0;

            for (TVar j=0;j<lAct;j++) y2[j+kAct] = y[j+kAct-1];
        }

        delete[] y;
        y = y2;
    }

    return kAct-1;
}


TRestr goblinLPSolver::AddVar(TFloat l,TFloat u,TFloat c,TVarType vt)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (l>u) Error(ERR_REJECTED,"AddVar","Incompatible bounds");

    #endif

    ReleaseVarValues();

    if (lAct==lMax) Resize(2*kAct,2*lMax,coeff->NMax());

    lAct++;

    try
    {
        SetCost(lAct-1,c);
        SetLRange(lAct-1,-InfFloat);
        SetURange(lAct-1,u);
        SetLRange(lAct-1,l);
        SetVarType(lAct-1,vt);
        if (u==InfFloat)
            SetIndex(kAct+lAct-1,lAct-1,LOWER);
        else SetIndex(kAct+lAct-1,lAct-1,UPPER);
    }
    catch (ERRange)
    {
        lAct--;
        throw ERRejected();
    }

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,lAct-1,0);

    if (baseInv)
    {
        if (baseValid)
        {
            // Reuse old basis information

            denseMatrix<TIndex,TFloat> *newInv
                = new denseMatrix<TIndex,TFloat>(lAct,lAct);

            for (TIndex i=0;i<lAct-1;i++)
            {
                for (TIndex j=0;j<lAct-1;j++)
                    newInv -> SetCoeff(i,j,baseInv->Coeff(i,j));

                newInv -> SetCoeff(lAct-1,i,0);
                newInv -> SetCoeff(i,lAct-1,0);
            }

            newInv -> SetCoeff(lAct-1,lAct-1,1);

            delete keptInv;
            keptInv = new denseMatrix<TIndex,TFloat>(lAct,lAct);

            x = (TFloat *)GoblinRealloc(index,lAct*sizeof(TFloat));

            if (RestrType(kAct+lAct-1)==BASIC_UB)
                x[lAct-1] = -UBound(kAct+lAct-1);
            else x[lAct-1] = LBound(kAct+lAct-1);

            y = (TFloat *)GoblinRealloc(index,(kAct+lAct)*sizeof(TFloat));
            y[kAct+lAct-1] = Cost(lAct-1);
        }
        else
        {
            delete baseInv;
            baseInv   = NULL;
            delete keptInv;
            keptInv   = NULL;
            delete[] x;
            x         = NULL;
            delete[] y;
            y         = NULL;
        }
    }

    return lAct-1;
}


void goblinLPSolver::DeleteRestr(TRestr i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("DeleteRestr",i);
    if (restrType[i]!=NON_BASIC)
        Error(ERR_RANGE,"DeleteRestr","Cannot delete a restriction in basis");

    #endif

    for (TVar j=0;j<lAct;j++) SetCoeff(i,j,0);

    restrType[i] = RESTR_CANCELED;

    delete[] restrLabel[i];
    restrLabel[i] = NULL;
}


void goblinLPSolver::DeleteVar(TVar j)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("DeleteVar",j);
    if (RestrType(j+kMax)!=NON_BASIC &&
        RestrType(j+kMax)!=RESTR_CANCELED &&
        RevIndex(j+kMax)!=j)
    {
        Error(ERR_REJECTED,"DeleteVar","Cannot delete a restriction in basis");
    }

    #endif

    ReleaseVarValues();

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,lAct-1,0);

    restrType[index[j]] = NON_BASIC;
    revIndex[index[j]] = NoVar;

    varType[j] = VAR_CANCELED;
    restrType[kMax+j] = RESTR_CANCELED;

    delete[] varLabel[j];
    varLabel[j] = NULL;
}


void goblinLPSolver::Resize(TRestr kk,TVar ll,TIndex nz)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (kk<kAct || ll<lAct || nz<coeff->NZ())
        Error(ERR_RANGE,"Resize","Parameters insufficient to maintain LP");

    #endif

    if (kk==0) kk++;
    if (ll==0) ll++;
    if (nz==0) nz++;

    // Reallocate coefficient matrix

    goblinHashTable<TIndex,TFloat> *newCoeff
        = new goblinHashTable<TIndex,TFloat>(kk*ll,nz,0,CT);

    for (TIndex i=0;i<kAct;i++)
    {
        for (TIndex j=0;j<lAct;j++)
        {
            newCoeff -> ChangeKey(ll*i+j,coeff->Key(lMax*i+j));
        }
    }

    delete coeff;
    coeff = newCoeff;


    // Update basis information

    index = (TVar *)GoblinRealloc(index,ll*sizeof(TVar));
    restrType = (TRestrType *)GoblinRealloc(restrType,(kk+ll)*sizeof(TRestrType));
    revIndex = (TVar *)GoblinRealloc(revIndex,(kk+ll)*sizeof(TVar));

    if (kk>kMax)
    {
        for (TIndex i=kk+ll-1;i>=kMax;i--)
        {
            if (i<kk || i>=kk+lAct)
            {
                restrType[i] = NON_BASIC;
                revIndex[i]  = NoVar;
            }
            else
            {    
                restrType[i] = restrType[i+kMax-kk];
                revIndex[i]  = revIndex[i+kMax-kk];

                if (revIndex[i]!=NoIndex) index[revIndex[i]] = i;
            }
        }
    }
    else
    {
        for (TIndex i=kMax;i<kk+ll;i++)
        {
            if (i<kk || i>=kk+lAct)
            {
                restrType[i] = NON_BASIC;
                revIndex[i]  = NoVar;
            }
            else
            {
                restrType[i] = restrType[i+kMax-kk];
                revIndex[i]  = revIndex[i+kMax-kk];

                if (revIndex[i]!=NoIndex) index[revIndex[i]] = i;
            }
        }
    }

    for (TIndex i=lMax;i<ll;i++) index[i]  = NoRestr;

    // Update varable/restriction labels

    if (lBound!=NULL)
    {
        lBound = (TFloat *)GoblinRealloc(lBound,kk*sizeof(TFloat));
        for (TIndex i=kMax;i<kk;i++) lBound[i] = cLBound;
    }

    if (uBound!=NULL)
    {
        uBound = (TFloat *)GoblinRealloc(uBound,kk*sizeof(TFloat));
        for (TIndex i=kMax;i<kk;i++) uBound[i] = cUBound;
    }

    if (lRange!=NULL)
    {
        lRange = (TFloat *)GoblinRealloc(lRange,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) lRange[i] = cLRange;
    }

    if (uRange!=NULL)
    {
        uRange = (TFloat *)GoblinRealloc(uRange,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) uRange[i] = cURange;
    }

    if (cost!=NULL)
    {
        cost = (TFloat *)GoblinRealloc(cost,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) cost[i] = cCost;
    }

    if (varType!=NULL)
    {
        varType = (TVarType *)GoblinRealloc(varType,ll*sizeof(TVarType));
        for (TIndex i=lMax;i<ll;i++) varType[i] = cVarType;
    }

    if (varLabel!=NULL)
    {
        varLabel = (char**)GoblinRealloc(varLabel,ll*sizeof(char*));
        for (TIndex i=lMax;i<ll;i++) varLabel[i] = NULL;
    }

    if (restrLabel!=NULL)
    {
        restrLabel = (char**)GoblinRealloc(restrLabel,kk*sizeof(char*));
        for (TIndex i=kMax;i<kk;i++) restrLabel[i] = NULL;
    }

    kMax = kk;
    lMax = ll;
}


TFloat goblinLPSolver::Coeff(TRestr i,TVar j)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("Coeff",i);
    if (j>=lAct)  NoSuchVar("Coeff",j);

    #endif

    return coeff->Key(lMax*i+j);
}


void goblinLPSolver::SetCoeff(TRestr i,TVar j,TFloat aa)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("SetCoeff",i);
    if (j>=lAct)  NoSuchVar("SetCoeff",j);

    if (aa>=InfFloat || aa<=-InfFloat)
        Error(ERR_RANGE,"SetCoeff","Finite matrix coefficients required");

    #endif

    coeff -> ChangeKey(lMax*i+j,aa);

    baseValid = false;
    dataValid = false;
}


TVar goblinLPSolver::GetRow(TRestr i,TVar* index,double* val)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("GetRow",i);

    #endif

    TVar nz = 0;

    for (TVar j=0;j<lAct;j++)
    {
        TFloat thisCoeff = Coeff(i,j);

        if (thisCoeff==0) continue;

        index[nz] = TIndex(j);
        val[nz] = thisCoeff;
        nz++;
    }

    return nz;
}


void goblinLPSolver::SetRow(TRestr i,TVar len,TVar* index,double* val)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetRow",i);

    #endif

    for (TVar j=0;j<lAct;j++) SetCoeff(i,j,0);

    for (TVar j=0;j<len;j++)
    {
        #if defined(_FAILSAVE_)

        if (index[j]>=lAct) NoSuchVar("SetRow",index[j]);

        #endif

        SetCoeff(i,index[j],val[j]);
    }
}


TRestr goblinLPSolver::GetColumn(TVar j,TRestr* index,double* val)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("GetColumn",j);

    #endif

    TVar nz = 0;

    for (TRestr i=0;i<lAct;i++)
    {
        TFloat thisCoeff = Coeff(i,j);

        if (thisCoeff==0) continue;

        index[nz] = TIndex(i);
        val[nz] = thisCoeff;
        nz++;
    }

    return nz;
}


void goblinLPSolver::SetColumn(TVar j,TRestr len,TRestr* index,double* val)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("SetColumn",j);

    #endif

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,j,0);

    for (TRestr i=0;i<len;i++)
    {
        #if defined(_FAILSAVE_)

        if (index[i]>=kAct) NoSuchVar("SetColumn",index[i]);

        #endif

        SetCoeff(index[i],j,val[i]);
    }
}


TFloat goblinLPSolver::Cost(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("Cost",i);

    #endif

    if (CCost()) return cCost;
    else return cost[i];
}


void goblinLPSolver::SetCost(TVar i,TFloat cc)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetCost",i);

    if (cc>=InfFloat || cc<=-InfFloat)
        Error(ERR_RANGE,"SetCost","Finite cost coefficients required");

    #endif

    if ((cost==NULL) && (cc != cCost))
    {
        cost = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) cost[j] = cCost;

        LogEntry(LOG_MEM,"...Upper variable bounds allocated");
    }

    if (cost!=NULL)
    {
        if (cc>=cCost) cost[i] = cCost = cc;
        else
        {
            if (cost[i]==cCost)
            {
                cCost = -InfFloat;
                for (TVar j=0;j<lAct;j++)
                    if (cost[j]>cCost) cCost = cost[j];
            }

            cost[i] = cc;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::URange(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("URange",i);

    #endif

    if (CURange()) return cURange;
    else return uRange[i];
}


void goblinLPSolver::SetURange(TVar i,TFloat uu)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetURange",i);

    if (uu<LRange(i) || uu<=-InfFloat)
        Error(ERR_RANGE,"SetURange","Incompatible bounds");

    #endif

    if ((uRange==NULL) && (uu != cURange))
    {
        uRange = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) uRange[j] = cURange;

        LogEntry(LOG_MEM,"...Upper variable bounds allocated");
    }

    if (uRange!=NULL)
    {
        if (uu>=cURange) uRange[i] = cURange = uu;
        else
        {
            if (uRange[i]==cURange)
            {
                cURange = -InfFloat;

                for (TVar j=0;j<lAct && cURange<InfFloat;j++)
                    if (uRange[j]>cURange) cURange = uRange[j];
            }

            uRange[i] = uu;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::LRange(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("LRange",i);

    #endif

    if (CLRange()) return cLRange;
    else return lRange[i];
}


void goblinLPSolver::SetLRange(TVar i,TFloat ll)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetLRange",i);

    if (ll>URange(i) || ll>=InfFloat)
        Error(ERR_RANGE,"SetLRange","Incompatible bounds");

    #endif

    if ((lRange==NULL) && (ll != cLRange))
    {
        lRange = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) lRange[j] = cLRange;

        LogEntry(LOG_MEM,"...Lower variable bounds allocated");
    }

    if (lRange!=NULL)
    {
        if (ll>=cLRange) lRange[i] = cLRange = ll;
        else
        {
            if (lRange[i]==cLRange)
            {
                cLRange = InfFloat;

                for (TVar j=0;j<lAct && cLRange>-InfFloat;j++)
                    if (lRange[j]<cLRange) cLRange = lRange[j];
            }

            lRange[i] = ll;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::UBound(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("UBound",i);

    #endif

    if (i>=kAct) return URange(i-kAct);

    if (CUBound()) return cUBound;
    else return uBound[i];
}


void goblinLPSolver::SetUBound(TRestr i,TFloat uu)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetUBound",i);

    if (uu<LBound(i) || uu<=-InfFloat)
    {
        InternalError("SetUBound","Incompatible bounds");
    }

    #endif

    if ((uBound==NULL) && (uu != cUBound))
    {
        uBound = new TFloat[kMax];

        for (TRestr j=0;j<kMax;j++) uBound[j] = cUBound;

        LogEntry(LOG_MEM,"...Upper bounds allocated");
    }

    if (uBound!=NULL)
    {
        if (uu>=cUBound) uBound[i] = cUBound = uu;
        else
        {
            if (uBound[i]==cUBound)
            {
                cUBound = -InfFloat;
                for (TRestr j=0;j<kAct && cUBound<InfFloat;j++)
                    if (uBound[j]>cUBound) cUBound = uBound[j];
            }

            uBound[i] = uu;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::LBound(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("LBound",i);

    #endif

    if (i>=kAct) return LRange(i-kAct);

    if (CLBound()) return cLBound;
    else return lBound[i];
}


void goblinLPSolver::SetLBound(TRestr i,TFloat ll)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetLBound",i);

    if (ll>UBound(i) || ll>=InfFloat)
        Error(ERR_RANGE,"SetLBound","Incompatible bounds");

    #endif

    if ((lBound==NULL) && (ll != cLBound))
    {
        lBound = new TFloat[kMax];
        for (TRestr j=0;j<kMax;j++) lBound[j] = cLBound;
        LogEntry(LOG_MEM,"...Lower bounds allocated");
    }

    if (lBound!=NULL)
    {
        if (ll>=cLBound) lBound[i] = cLBound = ll;
        else
        {
            if (lBound[i]==cLBound)
            {
                cLBound = InfFloat;

                for (TRestr j=0;j<kAct && cLBound>-InfFloat;j++)
                    if (lBound[j]<cLBound) cLBound = lBound[j];
            }

            lBound[i] = ll;
        }
    }

    dataValid = false;
}


void goblinLPSolver::SetObjectSense(TObjectSense dd) throw() 
{
    if (dd==NO_OBJECTIVE)
    {
        if (cost)
        {
            delete[] cost;
            cost = NULL;
        }

        cCost = 0;
        dataValid = false;
    }

    dir = dd;
}


mipInstance::TVarType goblinLPSolver::VarType(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("VarType",i);

    #endif

    if (CVarType()) return cVarType;
    else return varType[i];
}


void goblinLPSolver::SetVarType(TVar i,TVarType vt)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetVarType",i);

    #endif

    if ((varType==NULL) && (vt != cVarType))
    {
        if (lAct==1) cVarType = vt;
        else
        {
            varType = new TVarType[lMax];
            for (TVar j=0;j<lAct;j++) varType[j] = cVarType;
            LogEntry(LOG_MEM,"...Variable types allocated");
        }
    }

    if (varType!=NULL) varType[i] = vt;
}


static const char maxLabelLength = 20;
static char thisVarLabel[maxLabelLength] = "";

char* goblinLPSolver::VarLabel(TVar i,TOwnership tp)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("VarLabel",i);

    #endif

    if (varLabel==NULL || varLabel[i]==NULL)
    {
        sprintf(thisVarLabel,"%ld",lMax);
        int len = strlen(thisVarLabel);

        sprintf(thisVarLabel,"x%*.*ld",len,len,i+1);
    }
    else sprintf(thisVarLabel,"%s",varLabel[i]);

    if (tp==OWNED_BY_RECEIVER) return thisVarLabel;

    char* retLabel = new char[strlen(thisVarLabel)+1];
    strcpy(retLabel,thisVarLabel);
    return retLabel;
}


void goblinLPSolver::SetVarLabel(TVar i,char* newLabel,TOwnership tp)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetVarLabel",i);
    if (newLabel!=NULL && strlen(newLabel)>maxLabelLength-1)
        Error(ERR_REJECTED,"SetVarLabel","Label length exeeds limits");

    #endif

    if (varLabel==NULL && newLabel!=NULL && strcmp(newLabel,"")!=0)
    {
        varLabel = new TString[lMax];

        for (TVar j=0;j<lMax;j++) varLabel[j] = NULL;

        LogEntry(LOG_MEM,"...Variable labels allocated");
    }

    if (tp==OWNED_BY_RECEIVER) varLabel[i] = newLabel;
    else
    {
        if (newLabel==NULL || strcmp(newLabel,"")==0 ||
            strcmp(newLabel,mipInstance::VarLabel(i,OWNED_BY_RECEIVER))==0)
        {
            if (varLabel!=NULL)
            {
                delete[] varLabel[i];
                varLabel[i] = NULL;
            }
        }
        else
        {
            varLabel[i] = new char[strlen(newLabel)+1];
            strcpy(varLabel[i],newLabel);
        }
    }

    if (varIndex!=NULL)
    {
        if (newLabel!=NULL && strcmp(newLabel,"")!=0) 
            varIndex -> ChangeKey(newLabel,i,NoIndex,OWNED_BY_SENDER);
        else varIndex -> ChangeKey(mipInstance::VarLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }
}


static char thisRestrLabel[maxLabelLength] = "";

char* goblinLPSolver::RestrLabel(TRestr i,TOwnership tp)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("RestrLabel",i);

    #endif

    if (restrLabel==NULL || restrLabel[i]==NULL)
    {
        sprintf(thisRestrLabel,"%ld",kMax);
        int len = strlen(thisRestrLabel);

        sprintf(thisRestrLabel,"r%*.*ld",len,len,i+1);
    }
    else sprintf(thisRestrLabel,"%s",restrLabel[i]);

    if (tp==OWNED_BY_RECEIVER) return thisRestrLabel;

    char* retLabel = new char[strlen(thisRestrLabel)+1];
    strcpy(retLabel,thisRestrLabel);
    return retLabel;
}


void goblinLPSolver::SetRestrLabel(TRestr i,char* newLabel,TOwnership tp)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetRestrLabel",i);
    if (newLabel!=NULL && strlen(newLabel)>maxLabelLength-1)
        Error(ERR_REJECTED,"SetRestrLabel","Label length exeeds limits");

    #endif

    if (restrLabel==NULL && newLabel!=NULL && strcmp(newLabel,"")!=0)
    {
        restrLabel = new TString[kMax];
        for (TRestr j=0;j<kMax;j++) restrLabel[j] = NULL;
        LogEntry(LOG_MEM,"...Restriction labels allocated");
    }

    if (tp==OWNED_BY_RECEIVER) restrLabel[i] = newLabel;
    else
    {
        if (newLabel==NULL || strcmp(newLabel,"")==0 ||
            strcmp(newLabel,mipInstance::RestrLabel(i,OWNED_BY_RECEIVER))==0)
        {
            if (restrLabel!=NULL)
            {
                delete[] restrLabel[i];
                restrLabel[i] =NULL;
            }
        }
        else
        {
            restrLabel[i] = new char[strlen(newLabel)+1];
            strcpy(restrLabel[i],newLabel);
        }
    }

    if (restrIndex!=NULL)
    {
        if (newLabel!=NULL && strcmp(newLabel,"")!=0) 
            restrIndex -> ChangeKey(newLabel,i,NoIndex,OWNED_BY_SENDER);
        else restrIndex -> ChangeKey(mipInstance::RestrLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }
}


TVar goblinLPSolver::VarIndex(char* thisLabel) const throw()
{
    if (varIndex==NULL)
    {
        varIndex = new goblinDictionary<TVar>(lMax,NoVar,CT);

        for (TVar i=0;i<L();i++)
            varIndex -> ChangeKey(VarLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }

    return varIndex->Key(thisLabel);
}


TRestr goblinLPSolver::RestrIndex(char* thisLabel) const throw()
{
    if (restrIndex==NULL)
    {
        restrIndex = new goblinDictionary<TRestr>(kMax,NoRestr,CT);

        for (TRestr i=0;i<L();i++)
            restrIndex -> ChangeKey(RestrLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }

    return restrIndex->Key(thisLabel);
}


nativeLPFactory::nativeLPFactory() throw() : managedObject(goblinDefaultContext)
{
}


mipInstance* nativeLPFactory::NewInstance(TRestr k,TVar l,TIndex nz,TObjectSense dir,
    goblinController& thisContext) const throw(ERRange)
{
    return new goblinLPSolver(k,l,nz,dir,thisContext);
}


mipInstance* nativeLPFactory::ReadInstance(const char* impFileName,goblinController& thisContext)
    const throw(ERRange)
{
    return new goblinLPSolver(impFileName,thisContext);
}


nativeLPFactory::~nativeLPFactory() throw()
{
}


unsigned long nativeLPFactory::Size() const throw()
{
    return sizeof(nativeLPFactory);
}


char* nativeLPFactory::Authors() const throw()
{
    return "Bernhard Schmidt, Christian Fremuth Paeger";
}


int nativeLPFactory::MajorRelease() const throw()
{
    return 1;
}


int nativeLPFactory::MinorRelease() const throw()
{
    return 0;
}


char* nativeLPFactory::PatchLevel() const throw()
{
    return "";
}


char* nativeLPFactory::BuildDate() const throw()
{
    return "2003/05/01";
}


char* nativeLPFactory::License() const throw()
{
    return "LGPL";
}


mipFactory::TLPOrientation nativeLPFactory::Orientation() const throw()
{
    return COLUMN_ORIENTED;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpRepresentation.cpp
/// \brief  #goblinLPSolver partial class implementation

#include "lpSolver.h"
#include "fileImport.h"


goblinLPSolver::goblinLPSolver(TRestr kk,TVar ll,TIndex size,
    TObjectSense cDir,goblinController& thisContext) throw() :
    mipInstance(thisContext)
{
    if (kk>0) kMax = kk; else kMax = 1;
    if (ll>0) lMax = ll; else lMax = 1;
    if (size==0) size++;
    kAct = 0;
    lAct = 0;

    cCost    = 0;
    cUBound  = InfFloat;
    cLBound  = -InfFloat;
    cURange  = InfFloat;
    cLRange  = -InfFloat;
    cVarType = VAR_FLOAT;

    coeff = new goblinHashTable<TIndex,TFloat>(kMax*lMax,size,0,CT);

    cost       = NULL;
    uBound     = NULL;
    lBound     = NULL;
    uRange     = NULL;
    lRange     = NULL;
    varType    = NULL;
    restrLabel = NULL;
    varLabel   = NULL;

    varIndex   = NULL;
    restrIndex = NULL;

    restrType = new TRestrType[kMax+lMax];
    index     = new TVar[lMax];
    revIndex  = new TVar[kMax+lMax];
    baseInv   = NULL;
    keptInv   = NULL;
    x         = NULL;
    y         = NULL;

    baseInitial = true;
    baseValid  = false;
    dataValid  = false;

    for (TIndex i=0;i<kMax+lMax;i++)
    {
        restrType[i] = NON_BASIC;
        revIndex[i]  = NoVar;
        if (i<lMax) index[i] = NoRestr;
    }

    dir = cDir;

    LogEntry(LOG_MEM,"...Native LP allocated");
}


goblinLPSolver::goblinLPSolver(const char* impFileName,
    goblinController& thisContext) throw(ERFile,ERParse) :
    mipInstance(thisContext)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading native LP...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading native LP...");

    goblinImport F(impFileName,CT);

    F.Scan("mixed_integer");

    F.Scan("rows");
    TIndex* nrows = F.GetTIndexTuple(1);
    kMax = nrows[0];
    delete[] nrows;

    F.Scan("columns");
    TIndex* ncols = F.GetTIndexTuple(1);
    lMax = ncols[0];
    delete[] ncols;

    F.Scan("size");
    TIndex* nzero = F.GetTIndexTuple(1);
    TIndex size = nzero[0];
    delete[] nzero;

    if (kMax==0) kMax = 1;
    if (lMax==0) lMax = 1;
    if (size==0) size++;

    kAct = lAct = 0;

    cCost    = 0;
    cUBound  = InfFloat;
    cLBound  = -InfFloat;
    cURange  = InfFloat;
    cLRange  = -InfFloat;
    cVarType = VAR_FLOAT;

    coeff = new goblinHashTable<TIndex,TFloat>(kMax*lMax,size,0,CT);

    cost       = NULL;
    uBound     = NULL;
    lBound     = NULL;
    uRange     = NULL;
    lRange     = NULL;
    varType    = NULL;
    restrLabel = NULL;
    varLabel   = NULL;

    varIndex   = NULL;
    restrIndex = NULL;

    restrType = new TRestrType[kMax+lMax];
    index     = new TVar[lMax];
    revIndex  = new TVar[kMax+lMax];
    baseInv   = NULL;
    keptInv   = NULL;
    x         = NULL;
    y         = NULL;

    baseInitial = true;
    baseValid  = false;
    dataValid  = false;

    for (TIndex i=0;i<kMax+lMax;i++)
    {
        restrType[i] = NON_BASIC;
        revIndex[i]  = NoVar;
        if (i<lMax) index[i] = NoRestr;
    }

    F.Scan("pivot");
    TIndex* pPivot = F.GetTIndexTuple(3);

    if (pPivot[0]==NoIndex)
    {
        pivotRow = NoRestr;
        pivotColumn = NoVar;
        pivotDir = LOWER;
    }
    else
    {
        pivotRow = pPivot[0];
        pivotColumn = pPivot[1];
        pivotDir = TLowerUpper(pPivot[2]);
    }

    delete[] pPivot;

    ReadVarValues(&F,lMax);

    // Variable values are released by the following operations
    TFloat* swapVarValue = varValue;
    TVar swapNumVars = numVars;
    varValue = NULL;

    F.Scan("rowvis");
    char* pDummy = F.GetCharTuple(kAct);
    delete[] pDummy;

    F.Scan("colvis");
    pDummy = F.GetCharTuple(lAct);
    delete[] pDummy;

    F.Scan("configure");
    F.ReadConfiguration();

    F.Scan(); // mixed_integer

    ReadMPSFile(F.Stream());
    ReadBASFile(F.Stream());

    int l = strlen(impFileName)-4;
    char* tmpLabel = new char[l+1];
    memcpy(tmpLabel,impFileName,l);
    tmpLabel[l] = 0;
    SetLabel(tmpLabel);
    delete[] tmpLabel;
    CT.SetMaster(Handle());

    varValue = swapVarValue;
    numVars = swapNumVars;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


goblinLPSolver::~goblinLPSolver() throw()
{
    delete[] cost;
    delete[] uBound;
    delete[] lBound;
    delete[] uRange;
    delete[] lRange;
    delete[] varType;
    delete[] restrLabel;
    delete[] varLabel;

    if (varIndex) delete varIndex;
    if (restrIndex) delete restrIndex;

    delete[] restrType;
    delete[] index;
    delete[] revIndex;
    delete baseInv;
    delete keptInv;
    delete[] x;
    delete[] y;

    delete coeff;

    LogEntry(LOG_MEM,"...Native LP disallocated");
}


unsigned long goblinLPSolver::Size() const throw()
{
    return
          sizeof(goblinLPSolver)
        + managedObject::Allocated()
        + goblinLPSolver::Allocated();
}


unsigned long goblinLPSolver::Allocated() const throw()
{
    unsigned long tmpSize = (2*kMax+3*lMax)*(sizeof(TRestrType)+sizeof(TVar));
      // restrType, index, revIndex

    if (cost!=NULL)         tmpSize += lMax*sizeof(TFloat);
    if (uBound!=NULL)       tmpSize += kMax*sizeof(TFloat);
    if (lBound!=NULL)       tmpSize += kMax*sizeof(TFloat);
    if (uRange!=NULL)       tmpSize += lMax*sizeof(TFloat);
    if (lRange!=NULL)       tmpSize += lMax*sizeof(TFloat);
    if (varType!=NULL)      tmpSize += lMax*sizeof(TVarType);

    if (restrLabel!=NULL)
    {
        tmpSize += kMax*sizeof(char);
        for (TRestr i=0;i<kAct;i++)
            if (restrLabel[i]!=NULL)
                tmpSize += sizeof(char)*(strlen(restrLabel[i])+1);
    }

    if (varLabel!=NULL)
    {
        tmpSize += lMax*sizeof(char);
        for (TVar i=0;i<lAct;i++)
            if (varLabel[i]!=NULL)
                tmpSize += sizeof(char)*(strlen(varLabel[i])+1);
    }

    if (baseValid)          tmpSize += (kAct+2*lAct)*sizeof(TFloat);

    return tmpSize;
}


TRestr goblinLPSolver::K() const throw()
{
    return kAct;
}


TVar goblinLPSolver::L() const throw()
{
    return lAct;
}


TIndex goblinLPSolver::NZ() const throw()
{
    return coeff->NZ();
}


TRestr goblinLPSolver::AddRestr(TFloat l,TFloat u)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (l>u) Error(ERR_REJECTED,"AddRestr","Incompatible bounds");

    #endif

    if (kAct==kMax) Resize(2*kMax,2*lAct,coeff->NMax());

    kAct++;

    try
    {
        if (UBound(kAct-1)!=InfFloat)
        {
            SetLBound(kAct-1,-InfFloat);
            SetUBound(kAct-1,u);
            SetLBound(kAct-1,l);
        }
        else
        {
            SetLBound(kAct-1,l);
            SetUBound(kAct-1,u);
        }

        restrType[kAct-1] = NON_BASIC;
    }
    catch (ERRange)
    {
        kAct--;
        throw ERRejected();
    }

    revIndex[kAct-1] = NoVar;

    for (TVar j=0;j<lAct;j++) SetCoeff(kAct-1,j,0);

    if (y!=NULL)
    {
        // Extend dual solution

        TFloat *y2 = new TFloat[kAct+lAct];

        if (baseValid)
        {
            for (TRestr j=0;j<kAct-1;j++) y2[j] = y[j];

            y2[kAct-1] = 0;

            for (TVar j=0;j<lAct;j++) y2[j+kAct] = y[j+kAct-1];
        }

        delete[] y;
        y = y2;
    }

    return kAct-1;
}


TRestr goblinLPSolver::AddVar(TFloat l,TFloat u,TFloat c,TVarType vt)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (l>u) Error(ERR_REJECTED,"AddVar","Incompatible bounds");

    #endif

    ReleaseVarValues();

    if (lAct==lMax) Resize(2*kAct,2*lMax,coeff->NMax());

    lAct++;

    try
    {
        SetCost(lAct-1,c);
        SetLRange(lAct-1,-InfFloat);
        SetURange(lAct-1,u);
        SetLRange(lAct-1,l);
        SetVarType(lAct-1,vt);
        if (u==InfFloat)
            SetIndex(kAct+lAct-1,lAct-1,LOWER);
        else SetIndex(kAct+lAct-1,lAct-1,UPPER);
    }
    catch (ERRange)
    {
        lAct--;
        throw ERRejected();
    }

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,lAct-1,0);

    if (baseInv)
    {
        if (baseValid)
        {
            // Reuse old basis information

            denseMatrix<TIndex,TFloat> *newInv
                = new denseMatrix<TIndex,TFloat>(lAct,lAct);

            for (TIndex i=0;i<lAct-1;i++)
            {
                for (TIndex j=0;j<lAct-1;j++)
                    newInv -> SetCoeff(i,j,baseInv->Coeff(i,j));

                newInv -> SetCoeff(lAct-1,i,0);
                newInv -> SetCoeff(i,lAct-1,0);
            }

            newInv -> SetCoeff(lAct-1,lAct-1,1);

            delete keptInv;
            keptInv = new denseMatrix<TIndex,TFloat>(lAct,lAct);

            x = (TFloat *)GoblinRealloc(index,lAct*sizeof(TFloat));

            if (RestrType(kAct+lAct-1)==BASIC_UB)
                x[lAct-1] = -UBound(kAct+lAct-1);
            else x[lAct-1] = LBound(kAct+lAct-1);

            y = (TFloat *)GoblinRealloc(index,(kAct+lAct)*sizeof(TFloat));
            y[kAct+lAct-1] = Cost(lAct-1);
        }
        else
        {
            delete baseInv;
            baseInv   = NULL;
            delete keptInv;
            keptInv   = NULL;
            delete[] x;
            x         = NULL;
            delete[] y;
            y         = NULL;
        }
    }

    return lAct-1;
}


void goblinLPSolver::DeleteRestr(TRestr i)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("DeleteRestr",i);
    if (restrType[i]!=NON_BASIC)
        Error(ERR_RANGE,"DeleteRestr","Cannot delete a restriction in basis");

    #endif

    for (TVar j=0;j<lAct;j++) SetCoeff(i,j,0);

    restrType[i] = RESTR_CANCELED;

    delete[] restrLabel[i];
    restrLabel[i] = NULL;
}


void goblinLPSolver::DeleteVar(TVar j)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("DeleteVar",j);
    if (RestrType(j+kMax)!=NON_BASIC &&
        RestrType(j+kMax)!=RESTR_CANCELED &&
        RevIndex(j+kMax)!=j)
    {
        Error(ERR_REJECTED,"DeleteVar","Cannot delete a restriction in basis");
    }

    #endif

    ReleaseVarValues();

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,lAct-1,0);

    restrType[index[j]] = NON_BASIC;
    revIndex[index[j]] = NoVar;

    varType[j] = VAR_CANCELED;
    restrType[kMax+j] = RESTR_CANCELED;

    delete[] varLabel[j];
    varLabel[j] = NULL;
}


void goblinLPSolver::Resize(TRestr kk,TVar ll,TIndex nz)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (kk<kAct || ll<lAct || nz<coeff->NZ())
        Error(ERR_RANGE,"Resize","Parameters insufficient to maintain LP");

    #endif

    if (kk==0) kk++;
    if (ll==0) ll++;
    if (nz==0) nz++;

    // Reallocate coefficient matrix

    goblinHashTable<TIndex,TFloat> *newCoeff
        = new goblinHashTable<TIndex,TFloat>(kk*ll,nz,0,CT);

    for (TIndex i=0;i<kAct;i++)
    {
        for (TIndex j=0;j<lAct;j++)
        {
            newCoeff -> ChangeKey(ll*i+j,coeff->Key(lMax*i+j));
        }
    }

    delete coeff;
    coeff = newCoeff;


    // Update basis information

    index = (TVar *)GoblinRealloc(index,ll*sizeof(TVar));
    restrType = (TRestrType *)GoblinRealloc(restrType,(kk+ll)*sizeof(TRestrType));
    revIndex = (TVar *)GoblinRealloc(revIndex,(kk+ll)*sizeof(TVar));

    if (kk>kMax)
    {
        for (TIndex i=kk+ll-1;i>=kMax;i--)
        {
            if (i<kk || i>=kk+lAct)
            {
                restrType[i] = NON_BASIC;
                revIndex[i]  = NoVar;
            }
            else
            {    
                restrType[i] = restrType[i+kMax-kk];
                revIndex[i]  = revIndex[i+kMax-kk];

                if (revIndex[i]!=NoIndex) index[revIndex[i]] = i;
            }
        }
    }
    else
    {
        for (TIndex i=kMax;i<kk+ll;i++)
        {
            if (i<kk || i>=kk+lAct)
            {
                restrType[i] = NON_BASIC;
                revIndex[i]  = NoVar;
            }
            else
            {
                restrType[i] = restrType[i+kMax-kk];
                revIndex[i]  = revIndex[i+kMax-kk];

                if (revIndex[i]!=NoIndex) index[revIndex[i]] = i;
            }
        }
    }

    for (TIndex i=lMax;i<ll;i++) index[i]  = NoRestr;

    // Update varable/restriction labels

    if (lBound!=NULL)
    {
        lBound = (TFloat *)GoblinRealloc(lBound,kk*sizeof(TFloat));
        for (TIndex i=kMax;i<kk;i++) lBound[i] = cLBound;
    }

    if (uBound!=NULL)
    {
        uBound = (TFloat *)GoblinRealloc(uBound,kk*sizeof(TFloat));
        for (TIndex i=kMax;i<kk;i++) uBound[i] = cUBound;
    }

    if (lRange!=NULL)
    {
        lRange = (TFloat *)GoblinRealloc(lRange,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) lRange[i] = cLRange;
    }

    if (uRange!=NULL)
    {
        uRange = (TFloat *)GoblinRealloc(uRange,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) uRange[i] = cURange;
    }

    if (cost!=NULL)
    {
        cost = (TFloat *)GoblinRealloc(cost,ll*sizeof(TFloat));
        for (TIndex i=lMax;i<ll;i++) cost[i] = cCost;
    }

    if (varType!=NULL)
    {
        varType = (TVarType *)GoblinRealloc(varType,ll*sizeof(TVarType));
        for (TIndex i=lMax;i<ll;i++) varType[i] = cVarType;
    }

    if (varLabel!=NULL)
    {
        varLabel = (char**)GoblinRealloc(varLabel,ll*sizeof(char*));
        for (TIndex i=lMax;i<ll;i++) varLabel[i] = NULL;
    }

    if (restrLabel!=NULL)
    {
        restrLabel = (char**)GoblinRealloc(restrLabel,kk*sizeof(char*));
        for (TIndex i=kMax;i<kk;i++) restrLabel[i] = NULL;
    }

    kMax = kk;
    lMax = ll;
}


TFloat goblinLPSolver::Coeff(TRestr i,TVar j)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("Coeff",i);
    if (j>=lAct)  NoSuchVar("Coeff",j);

    #endif

    return coeff->Key(lMax*i+j);
}


void goblinLPSolver::SetCoeff(TRestr i,TVar j,TFloat aa)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("SetCoeff",i);
    if (j>=lAct)  NoSuchVar("SetCoeff",j);

    if (aa>=InfFloat || aa<=-InfFloat)
        Error(ERR_RANGE,"SetCoeff","Finite matrix coefficients required");

    #endif

    coeff -> ChangeKey(lMax*i+j,aa);

    baseValid = false;
    dataValid = false;
}


TVar goblinLPSolver::GetRow(TRestr i,TVar* index,double* val)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("GetRow",i);

    #endif

    TVar nz = 0;

    for (TVar j=0;j<lAct;j++)
    {
        TFloat thisCoeff = Coeff(i,j);

        if (thisCoeff==0) continue;

        index[nz] = TIndex(j);
        val[nz] = thisCoeff;
        nz++;
    }

    return nz;
}


void goblinLPSolver::SetRow(TRestr i,TVar len,TVar* index,double* val)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetRow",i);

    #endif

    for (TVar j=0;j<lAct;j++) SetCoeff(i,j,0);

    for (TVar j=0;j<len;j++)
    {
        #if defined(_FAILSAVE_)

        if (index[j]>=lAct) NoSuchVar("SetRow",index[j]);

        #endif

        SetCoeff(i,index[j],val[j]);
    }
}


TRestr goblinLPSolver::GetColumn(TVar j,TRestr* index,double* val)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("GetColumn",j);

    #endif

    TVar nz = 0;

    for (TRestr i=0;i<lAct;i++)
    {
        TFloat thisCoeff = Coeff(i,j);

        if (thisCoeff==0) continue;

        index[nz] = TIndex(i);
        val[nz] = thisCoeff;
        nz++;
    }

    return nz;
}


void goblinLPSolver::SetColumn(TVar j,TRestr len,TRestr* index,double* val)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (j>=lAct) NoSuchVar("SetColumn",j);

    #endif

    for (TRestr i=0;i<kAct;i++) SetCoeff(i,j,0);

    for (TRestr i=0;i<len;i++)
    {
        #if defined(_FAILSAVE_)

        if (index[i]>=kAct) NoSuchVar("SetColumn",index[i]);

        #endif

        SetCoeff(index[i],j,val[i]);
    }
}


TFloat goblinLPSolver::Cost(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("Cost",i);

    #endif

    if (CCost()) return cCost;
    else return cost[i];
}


void goblinLPSolver::SetCost(TVar i,TFloat cc)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetCost",i);

    if (cc>=InfFloat || cc<=-InfFloat)
        Error(ERR_RANGE,"SetCost","Finite cost coefficients required");

    #endif

    if ((cost==NULL) && (cc != cCost))
    {
        cost = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) cost[j] = cCost;

        LogEntry(LOG_MEM,"...Upper variable bounds allocated");
    }

    if (cost!=NULL)
    {
        if (cc>=cCost) cost[i] = cCost = cc;
        else
        {
            if (cost[i]==cCost)
            {
                cCost = -InfFloat;
                for (TVar j=0;j<lAct;j++)
                    if (cost[j]>cCost) cCost = cost[j];
            }

            cost[i] = cc;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::URange(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("URange",i);

    #endif

    if (CURange()) return cURange;
    else return uRange[i];
}


void goblinLPSolver::SetURange(TVar i,TFloat uu)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetURange",i);

    if (uu<LRange(i) || uu<=-InfFloat)
        Error(ERR_RANGE,"SetURange","Incompatible bounds");

    #endif

    if ((uRange==NULL) && (uu != cURange))
    {
        uRange = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) uRange[j] = cURange;

        LogEntry(LOG_MEM,"...Upper variable bounds allocated");
    }

    if (uRange!=NULL)
    {
        if (uu>=cURange) uRange[i] = cURange = uu;
        else
        {
            if (uRange[i]==cURange)
            {
                cURange = -InfFloat;

                for (TVar j=0;j<lAct && cURange<InfFloat;j++)
                    if (uRange[j]>cURange) cURange = uRange[j];
            }

            uRange[i] = uu;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::LRange(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("LRange",i);

    #endif

    if (CLRange()) return cLRange;
    else return lRange[i];
}


void goblinLPSolver::SetLRange(TVar i,TFloat ll)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetLRange",i);

    if (ll>URange(i) || ll>=InfFloat)
        Error(ERR_RANGE,"SetLRange","Incompatible bounds");

    #endif

    if ((lRange==NULL) && (ll != cLRange))
    {
        lRange = new TFloat[lMax];

        for (TVar j=0;j<lMax;j++) lRange[j] = cLRange;

        LogEntry(LOG_MEM,"...Lower variable bounds allocated");
    }

    if (lRange!=NULL)
    {
        if (ll>=cLRange) lRange[i] = cLRange = ll;
        else
        {
            if (lRange[i]==cLRange)
            {
                cLRange = InfFloat;

                for (TVar j=0;j<lAct && cLRange>-InfFloat;j++)
                    if (lRange[j]<cLRange) cLRange = lRange[j];
            }

            lRange[i] = ll;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::UBound(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("UBound",i);

    #endif

    if (i>=kAct) return URange(i-kAct);

    if (CUBound()) return cUBound;
    else return uBound[i];
}


void goblinLPSolver::SetUBound(TRestr i,TFloat uu)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetUBound",i);

    if (uu<LBound(i) || uu<=-InfFloat)
    {
        InternalError("SetUBound","Incompatible bounds");
    }

    #endif

    if ((uBound==NULL) && (uu != cUBound))
    {
        uBound = new TFloat[kMax];

        for (TRestr j=0;j<kMax;j++) uBound[j] = cUBound;

        LogEntry(LOG_MEM,"...Upper bounds allocated");
    }

    if (uBound!=NULL)
    {
        if (uu>=cUBound) uBound[i] = cUBound = uu;
        else
        {
            if (uBound[i]==cUBound)
            {
                cUBound = -InfFloat;
                for (TRestr j=0;j<kAct && cUBound<InfFloat;j++)
                    if (uBound[j]>cUBound) cUBound = uBound[j];
            }

            uBound[i] = uu;
        }
    }

    dataValid = false;
}


TFloat goblinLPSolver::LBound(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("LBound",i);

    #endif

    if (i>=kAct) return LRange(i-kAct);

    if (CLBound()) return cLBound;
    else return lBound[i];
}


void goblinLPSolver::SetLBound(TRestr i,TFloat ll)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetLBound",i);

    if (ll>UBound(i) || ll>=InfFloat)
        Error(ERR_RANGE,"SetLBound","Incompatible bounds");

    #endif

    if ((lBound==NULL) && (ll != cLBound))
    {
        lBound = new TFloat[kMax];
        for (TRestr j=0;j<kMax;j++) lBound[j] = cLBound;
        LogEntry(LOG_MEM,"...Lower bounds allocated");
    }

    if (lBound!=NULL)
    {
        if (ll>=cLBound) lBound[i] = cLBound = ll;
        else
        {
            if (lBound[i]==cLBound)
            {
                cLBound = InfFloat;

                for (TRestr j=0;j<kAct && cLBound>-InfFloat;j++)
                    if (lBound[j]<cLBound) cLBound = lBound[j];
            }

            lBound[i] = ll;
        }
    }

    dataValid = false;
}


void goblinLPSolver::SetObjectSense(TObjectSense dd) throw() 
{
    if (dd==NO_OBJECTIVE)
    {
        if (cost)
        {
            delete[] cost;
            cost = NULL;
        }

        cCost = 0;
        dataValid = false;
    }

    dir = dd;
}


mipInstance::TVarType goblinLPSolver::VarType(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("VarType",i);

    #endif

    if (CVarType()) return cVarType;
    else return varType[i];
}


void goblinLPSolver::SetVarType(TVar i,TVarType vt)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetVarType",i);

    #endif

    if ((varType==NULL) && (vt != cVarType))
    {
        if (lAct==1) cVarType = vt;
        else
        {
            varType = new TVarType[lMax];
            for (TVar j=0;j<lAct;j++) varType[j] = cVarType;
            LogEntry(LOG_MEM,"...Variable types allocated");
        }
    }

    if (varType!=NULL) varType[i] = vt;
}


static const char maxLabelLength = 20;
static char thisVarLabel[maxLabelLength] = "";

char* goblinLPSolver::VarLabel(TVar i,TOwnership tp)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("VarLabel",i);

    #endif

    if (varLabel==NULL || varLabel[i]==NULL)
    {
        sprintf(thisVarLabel,"%ld",lMax);
        int len = strlen(thisVarLabel);

        sprintf(thisVarLabel,"x%*.*ld",len,len,i+1);
    }
    else sprintf(thisVarLabel,"%s",varLabel[i]);

    if (tp==OWNED_BY_RECEIVER) return thisVarLabel;

    char* retLabel = new char[strlen(thisVarLabel)+1];
    strcpy(retLabel,thisVarLabel);
    return retLabel;
}


void goblinLPSolver::SetVarLabel(TVar i,char* newLabel,TOwnership tp)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct) NoSuchVar("SetVarLabel",i);
    if (newLabel!=NULL && strlen(newLabel)>maxLabelLength-1)
        Error(ERR_REJECTED,"SetVarLabel","Label length exeeds limits");

    #endif

    if (varLabel==NULL && newLabel!=NULL && strcmp(newLabel,"")!=0)
    {
        varLabel = new TString[lMax];

        for (TVar j=0;j<lMax;j++) varLabel[j] = NULL;

        LogEntry(LOG_MEM,"...Variable labels allocated");
    }

    if (tp==OWNED_BY_RECEIVER) varLabel[i] = newLabel;
    else
    {
        if (newLabel==NULL || strcmp(newLabel,"")==0 ||
            strcmp(newLabel,mipInstance::VarLabel(i,OWNED_BY_RECEIVER))==0)
        {
            if (varLabel!=NULL)
            {
                delete[] varLabel[i];
                varLabel[i] = NULL;
            }
        }
        else
        {
            varLabel[i] = new char[strlen(newLabel)+1];
            strcpy(varLabel[i],newLabel);
        }
    }

    if (varIndex!=NULL)
    {
        if (newLabel!=NULL && strcmp(newLabel,"")!=0) 
            varIndex -> ChangeKey(newLabel,i,NoIndex,OWNED_BY_SENDER);
        else varIndex -> ChangeKey(mipInstance::VarLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }
}


static char thisRestrLabel[maxLabelLength] = "";

char* goblinLPSolver::RestrLabel(TRestr i,TOwnership tp)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct)  NoSuchRestr("RestrLabel",i);

    #endif

    if (restrLabel==NULL || restrLabel[i]==NULL)
    {
        sprintf(thisRestrLabel,"%ld",kMax);
        int len = strlen(thisRestrLabel);

        sprintf(thisRestrLabel,"r%*.*ld",len,len,i+1);
    }
    else sprintf(thisRestrLabel,"%s",restrLabel[i]);

    if (tp==OWNED_BY_RECEIVER) return thisRestrLabel;

    char* retLabel = new char[strlen(thisRestrLabel)+1];
    strcpy(retLabel,thisRestrLabel);
    return retLabel;
}


void goblinLPSolver::SetRestrLabel(TRestr i,char* newLabel,TOwnership tp)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct) NoSuchRestr("SetRestrLabel",i);
    if (newLabel!=NULL && strlen(newLabel)>maxLabelLength-1)
        Error(ERR_REJECTED,"SetRestrLabel","Label length exeeds limits");

    #endif

    if (restrLabel==NULL && newLabel!=NULL && strcmp(newLabel,"")!=0)
    {
        restrLabel = new TString[kMax];
        for (TRestr j=0;j<kMax;j++) restrLabel[j] = NULL;
        LogEntry(LOG_MEM,"...Restriction labels allocated");
    }

    if (tp==OWNED_BY_RECEIVER) restrLabel[i] = newLabel;
    else
    {
        if (newLabel==NULL || strcmp(newLabel,"")==0 ||
            strcmp(newLabel,mipInstance::RestrLabel(i,OWNED_BY_RECEIVER))==0)
        {
            if (restrLabel!=NULL)
            {
                delete[] restrLabel[i];
                restrLabel[i] =NULL;
            }
        }
        else
        {
            restrLabel[i] = new char[strlen(newLabel)+1];
            strcpy(restrLabel[i],newLabel);
        }
    }

    if (restrIndex!=NULL)
    {
        if (newLabel!=NULL && strcmp(newLabel,"")!=0) 
            restrIndex -> ChangeKey(newLabel,i,NoIndex,OWNED_BY_SENDER);
        else restrIndex -> ChangeKey(mipInstance::RestrLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }
}


TVar goblinLPSolver::VarIndex(char* thisLabel) const throw()
{
    if (varIndex==NULL)
    {
        varIndex = new goblinDictionary<TVar>(lMax,NoVar,CT);

        for (TVar i=0;i<L();i++)
            varIndex -> ChangeKey(VarLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }

    return varIndex->Key(thisLabel);
}


TRestr goblinLPSolver::RestrIndex(char* thisLabel) const throw()
{
    if (restrIndex==NULL)
    {
        restrIndex = new goblinDictionary<TRestr>(kMax,NoRestr,CT);

        for (TRestr i=0;i<L();i++)
            restrIndex -> ChangeKey(RestrLabel(i),i,NoIndex,OWNED_BY_SENDER);
    }

    return restrIndex->Key(thisLabel);
}


nativeLPFactory::nativeLPFactory() throw() : managedObject(goblinDefaultContext)
{
}


mipInstance* nativeLPFactory::NewInstance(TRestr k,TVar l,TIndex nz,TObjectSense dir,
    goblinController& thisContext) const throw(ERRange)
{
    return new goblinLPSolver(k,l,nz,dir,thisContext);
}


mipInstance* nativeLPFactory::ReadInstance(const char* impFileName,goblinController& thisContext)
    const throw(ERRange)
{
    return new goblinLPSolver(impFileName,thisContext);
}


nativeLPFactory::~nativeLPFactory() throw()
{
}


unsigned long nativeLPFactory::Size() const throw()
{
    return sizeof(nativeLPFactory);
}


char* nativeLPFactory::Authors() const throw()
{
    return "Bernhard Schmidt, Christian Fremuth Paeger";
}


int nativeLPFactory::MajorRelease() const throw()
{
    return 1;
}


int nativeLPFactory::MinorRelease() const throw()
{
    return 0;
}


char* nativeLPFactory::PatchLevel() const throw()
{
    return "";
}


char* nativeLPFactory::BuildDate() const throw()
{
    return "2003/05/01";
}


char* nativeLPFactory::License() const throw()
{
    return "LGPL";
}


mipFactory::TLPOrientation nativeLPFactory::Orientation() const throw()
{
    return COLUMN_ORIENTED;
}
