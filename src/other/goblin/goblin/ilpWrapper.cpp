
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   ilpWrapper.cpp
/// \brief  #mipInstance partial class implementastion

#include "ilpWrapper.h"
#include "moduleGuard.h"


mipInstance::mipInstance(goblinController& thisContext) throw() :
    managedObject(thisContext)
{
    pivotColumn = NoVar;
    pivotRow = NoRestr;
    pivotDir = LOWER;

    varValue = NULL;

    bufferLength = 20;
    labelBuffer = new char[20];

    LogEntry(LOG_MEM,"...Linear program allocated");
}


mipInstance::~mipInstance() throw()
{
    ReleaseVarValues();

    delete[] labelBuffer;

    LogEntry(LOG_MEM,"...Linear program disallocated");
}


unsigned long mipInstance::Allocated() const throw()
{
    unsigned long tmpSize = bufferLength;

    if (varValue) tmpSize += numVars*sizeof(TFloat);

    return tmpSize;
}


bool mipInstance::IsMixedILP() const
{
   return true;
}


char* mipInstance::Display() const throw(ERFile,ERRejected)
{
    #if defined(_TRACING_)

    CT.IncreaseCounter();

    char* gobName = new char[strlen(CT.Label())+15];
    sprintf(gobName,"%s.trace%d.gob",CT.Label(),CT.fileCounter);
    Write(gobName);

    if (CT.traceEventHandler) CT.traceEventHandler(gobName);

    delete[] gobName;

    return const_cast<char*>(CT.Label());

    #else

    return NULL;

    #endif
}


char* mipInstance::UnifiedLabel(TRestr i,TOwnership tp) const throw(ERRange)
{
    if (i<K()) return RestrLabel(i,tp);
    else return VarLabel(i-K(),tp);
}


//****************************************************************************//
//                               File Export                                  //
//****************************************************************************//


void mipInstance::WriteLPNaive(const char* fileName,TDisplayOpt opt)
    const throw(ERFile,ERRejected)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    ofstream expFile(fileName, ios::out);
    expFile.setf(ios::fixed);
    expFile.precision(CT.externalPrecision);

    bool first = true;
    TFloat epsilon = 0.001;
    TVar count = 0;

    // Determine display space for labels and values

    int maxColLen = 1;
    long maxIntAbs = 0;
    bool isIntObjective = true;
    char tmpPrint[20];

    for (TVar j=0;j<L();j++)
    {
        int wd = strlen(VarLabel(j));

        if (wd>maxColLen) maxColLen = wd;

        TFloat thisIntRound = floor(Cost(j)+0.5);

        if (fabs(Cost(j)-thisIntRound)<epsilon)
        {
            if (fabs(thisIntRound)>maxIntAbs)
                maxIntAbs = long(fabs(thisIntRound));
        }
        else isIntObjective = false;
    }

    sprintf(tmpPrint,"%ld",maxIntAbs);
    int maxCostIntLen = strlen(tmpPrint);

    int maxRowLen = 1;
    maxIntAbs = 0;
    bool isIntBounds = true;

    for (TRestr i=0;i<K();i++)
    {
        int wd = strlen(RestrLabel(i));
        if (wd>maxRowLen) maxRowLen = wd;

        TFloat thisIntRound = floor(LBound(i)+0.5);

        if (fabs(LBound(i)-thisIntRound)<epsilon)
        {
            if (fabs(thisIntRound)>maxIntAbs)
                maxIntAbs = long(fabs(thisIntRound));
        }
        else isIntBounds = false;
    }

    int maxUniLen = maxColLen;

    if (maxRowLen>maxColLen) maxUniLen = maxRowLen;

    int maxFloatLen = CT.externalFloatLength-2;
    int maxBoundLen = maxFloatLen;

    if (isIntBounds)
    {
        sprintf(tmpPrint,"%ld",maxIntAbs);
        maxBoundLen = strlen(tmpPrint);
    }

    maxIntAbs = 0;
    bool isIntMatrix = true;

    TVar* index = new TVar[L()];
    TFloat* val = new TFloat[L()];

    for (TRestr i=0;i<K();i++)
    {
        TIndex nz = GetRow(i,index,val);

        for (TVar j=0;j<nz;j++)
        {
            TFloat thisIntRound = floor(val[j]+0.5);

            if (fabs(val[j]-thisIntRound)<epsilon)
            {
                if (fabs(thisIntRound)>maxIntAbs)
                    maxIntAbs = long(fabs(thisIntRound));
            }
            else isIntMatrix = false;
        }
    }

    sprintf(tmpPrint,"%ld",maxIntAbs);
    int maxCoeffIntLen = strlen(tmpPrint);
    int maxIntLen = maxCoeffIntLen;

    if (maxCostIntLen>maxCoeffIntLen) maxIntLen = maxCostIntLen;


    // Check for LP solver orientation

    mipFactory *theMipFactory = (mipFactory*)CT.pMipFactory;
    bool columnOriented =
        (theMipFactory->Orientation() == mipFactory::COLUMN_ORIENTED);


    // If problem is not too big, align columns

    bool alignDisplay = (L()*(maxFloatLen+maxColLen+4)<160);

    if (isIntMatrix && isIntObjective)
        alignDisplay = (L()*(maxIntLen+maxColLen+4)<160);

    if (!opt) alignDisplay = false;



    if (!opt || (opt & DISPLAY_OBJECTIVE) && ObjectSense()!=NO_OBJECTIVE)
    {
        if (ObjectSense()==MAXIMIZE)
            expFile << "Maximize" << endl;
        else expFile << "Minimize" << endl;

        int maxValueLen = maxFloatLen;

        if (alignDisplay && isIntObjective && isIntMatrix)
        {
            maxValueLen = maxIntLen;
        }
        else if (!alignDisplay && isIntObjective)
        {
            maxValueLen = maxCostIntLen;
        }

        for (TVar j=0;j<L();j++)
        {
            TFloat c = Cost(j);

            if (fabs(c)<=epsilon && !alignDisplay) continue;

            if (first)
            {
                if (opt)
                {
                    expFile << endl << setw(maxRowLen+maxBoundLen+8) << " ";
                }
                else
                {
                    expFile << endl << " ";
                }
            }

            if (fabs(c)>epsilon)
            {
                char sign = '+';

                if (c<0)
                {
                    sign = '-';
                    c *= -1;
                }

                if (first)
                {
                    if (sign=='+')
                        expFile << "  ";
                    else expFile << "- ";
                }
                else expFile << sign << " ";

                if (c==1)
                {
                    expFile << setw(maxColLen+maxValueLen+1)
                        << VarLabel(j) << " ";
                }
                else if (isIntObjective)
                {
                    expFile << setw(maxValueLen) << long(c) << " "
                        << setw(maxColLen) << VarLabel(j) << " ";
                }
                else if (opt)
                {
                    expFile << setw(maxValueLen) << c << " "
                        << setw(maxColLen) << VarLabel(j) << " ";
                }
                else
                {
                    expFile << c << " " << VarLabel(j) << " ";
                }
            }
            else
            {
                if (alignDisplay && opt)
                    expFile << setw(maxColLen+maxValueLen+4) << " ";
            }

            first = false;
        }

        expFile << endl;

        if (!first) expFile << endl;
    }


    if (!opt)
    {
        // Unformatted output of the restrictions

        first = true;

        for (TRestr i=0;i<K();i++)
        {
            if (first)
            {
                if (ObjectSense()!=NO_OBJECTIVE)
                    expFile << "Subject To" << endl << endl;
                else expFile << "Polyhedron" << endl << endl;
            }

            first = false;
            expFile << "  " << RestrLabel(i) << ": ";

            if (LBound(i)<=-InfFloat) expFile << "-inf";
            else expFile << LBound(i);

            expFile << " <= ";

            bool first2 = true;

            TIndex nz = GetRow(i,index,val);

            for (TVar k=0;k<nz;k++)
            {
                TVar j = index[k];
                TFloat c = val[k];

                if (fabs(c)<=epsilon) continue;

                char sign = '+';

                if (c<0)
                {
                    sign = '-';
                    c *= -1;
                }

                if (c==1)
                {
                    if (first2)
                    {
                        if (sign=='+') expFile << VarLabel(j) << " ";
                        else expFile << "-" << VarLabel(j) << " ";
                    }
                    else expFile << sign << " " << VarLabel(j) << " ";
                }
                else
                {
                    if (first2)
                    {
                        if (sign=='+')
                          expFile << c << " " << VarLabel(j) << " ";
                        else
                          expFile << "-" << c << " " << VarLabel(j) << " ";
                    }
                    else expFile << sign << " " << c << " " << VarLabel(j) << " ";
                }

                first2 = false;
            }

            expFile << "<= ";

            if (UBound(i)>=InfFloat) expFile << "inf";
            else expFile << UBound(i);

            expFile << endl;
        }

        if (!first) expFile << endl;
    }

    if (opt & DISPLAY_RESTRICTIONS)
    {
        // Output of the restrictions, aligned with the objective function

        int maxValueLen = maxFloatLen;

        if (alignDisplay && isIntObjective && isIntMatrix)
        {
            maxValueLen = maxIntLen;
        }
        else if (!alignDisplay && isIntMatrix)
        {
            maxValueLen = maxCoeffIntLen;
        }

        first = true;

        for (TRestr i=0;i<K();i++)
        {
            if (first)
            {
                if (ObjectSense()!=NO_OBJECTIVE)
                    expFile << "Subject To" << endl << endl;
                else expFile << "Polyhedron" << endl << endl;
            }

            first = false;
            expFile << "  " << setw(maxRowLen) << RestrLabel(i) << ": ";

            if (LBound(i)<=-InfFloat)
            {
                if (isIntBounds)
                {
                    expFile << setw(maxBoundLen) << long(UBound(i)) << " >= ";
                }
                else
                {
                    expFile << setw(maxBoundLen) << UBound(i) << " >= ";
                }
            }
            else
            {
                if (UBound(i)<=LBound(i))
                {
                    if (isIntBounds)
                    {
                        expFile << setw(maxBoundLen) << long(LBound(i)) << "  = ";
                    }
                    else
                    {
                        expFile << setw(maxBoundLen) << LBound(i) << "  = ";
                    }
                }
                else
                {
                    if (isIntBounds)
                    {
                        expFile << setw(maxBoundLen) << long(LBound(i)) << " <= ";
                    }
                    else
                    {
                        expFile << setw(maxBoundLen) << LBound(i) << " <= ";
                    }
                }
            }

            bool first2 = true;

            for (TVar j=0;j<L();j++)
            {
                TFloat c = Coeff(i,j);

                if (fabs(c)>epsilon)
                {
                    char sign = '+';

                    if (c<0)
                    {
                        sign = '-';
                        c *= -1;
                    }

                    if (first2)
                    {
                        if (sign=='+')
                            expFile << "  ";
                        else expFile << "- ";
                    }
                    else expFile << sign << " ";

                    if (c==1)
                    {
                        expFile << setw(maxColLen+maxValueLen+1)
                            << VarLabel(j) << " ";
                    }
                    else if (isIntMatrix)
                    {
                        expFile << setw(maxValueLen) << long(c) << " "
                            << setw(maxColLen) << VarLabel(j) << " ";
                    }
                    else
                    {
                        expFile << setw(maxValueLen) << c << " "
                            << setw(maxColLen) << VarLabel(j) << " ";
                    }

                    first2 = false;
                }
                else if (alignDisplay)
                {
                    expFile << setw(maxColLen+maxValueLen+4) << " ";
                }
            }

            if (UBound(i)<InfFloat && UBound(i)>LBound(i) && LBound(i)>-InfFloat)
            {
                if (isIntBounds)
                {
                    expFile << "<= " << long(UBound(i));
                }
                else
                {
                    expFile << "<= " << UBound(i);
                }
            }

            expFile << endl;
        }

        if (!first) expFile << endl;
    }

    if (!opt)
    {
        expFile << "Bounds" << endl << endl;

        for (TVar j=0;j<L();j++)
        {
            expFile << "  ";

            if (LRange(j)<=-InfFloat) expFile << "-inf";
            else expFile << LRange(j);

            expFile << " <= " << VarLabel(j) << " <= ";

            if (URange(j)>=InfFloat) expFile << "inf" << endl;
            else expFile << URange(j) << endl;
        }

        expFile << endl;
    }

    if (opt & DISPLAY_BOUNDS)
    {
        expFile << "Bounds" << endl << endl;

        bool initial = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)==0 && URange(j)>=InfFloat)
            {
                if (!count)
                {
                    if (initial) expFile << "  0 <= ";
                    else expFile << endl << "       ";
                }
                else expFile << ", ";

                expFile << setw(maxColLen) << VarLabel(j);
                count = (count+1)%10;
                initial = false;
            }
        }

        if (!initial) expFile << endl << endl;

        initial = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)<=-InfFloat && URange(j)==0)
            {
                if (!count)
                {
                    if (initial) expFile << "  0 >= ";
                    else expFile << endl << "       ";
                }
                else expFile << ", ";

                expFile << setw(maxColLen) << VarLabel(j);
                count = (count+1)%10;
                initial = false;
            }
        }

        if (!initial) expFile << endl << endl;

        initial = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)==0 && URange(j)==0)
            {
                if (!count)
                {
                    if (initial) expFile << "  0  = ";
                    else expFile << endl << "       ";
                }
                else expFile << ", ";

                expFile << setw(maxColLen) << VarLabel(j);
                count = (count+1)%10;
                initial = false;
            }
        }

        if (!initial) expFile << endl << endl;

        initial = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)==0 && URange(j)==1)
            {
                if (count%10==0)
                {
                    if (initial) expFile << "  0 <= ";
                    else expFile << endl << "       ";
                }
                else expFile << ", ";

                expFile << setw(maxColLen) << VarLabel(j);
                count++;
                initial = false;
            }
        }

        if (!initial)
        {
            if (count%10!=0 && count>10)
                expFile << setw((maxColLen+2)*(10-count)+5);

            expFile << " <= 1" << endl << endl;
        }

        initial = true;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)<=-InfFloat && URange(j)>=InfFloat) continue;
            if (LRange(j)==0 && URange(j)>=InfFloat) continue;
            if (LRange(j)<=-InfFloat && URange(j)==0) continue;
            if (LRange(j)==0 && URange(j)==1) continue;
            if (LRange(j)==0 && URange(j)==0) continue;

            initial = false;

            expFile << "  ";

            if (LRange(j)==URange(j))
            {
                expFile << VarLabel(j) << " = " << URange(j) << endl;
                continue;
            }

            if (LRange(j)<=-InfFloat)
            {
                expFile << VarLabel(j) << " <= " << URange(j) << endl;
                continue;
            }

            if (URange(j)>=InfFloat)
            {
                expFile << VarLabel(j) << " >= " << LRange(j) << endl;
                continue;
            }
            expFile << LRange(j) << " <= " << VarLabel(j) << " <= " << URange(j) << endl;
        }

        if (!initial) expFile << endl;
    }

    if (!opt || (opt & DISPLAY_INTEGERS))
    {
        first = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (VarType(j)!=VAR_FLOAT)
            {
                if (first)
                    expFile << "Integers" << endl << endl << "  " << VarLabel(j);
                else
                {
                    if (count) expFile << ", " << VarLabel(j);
                    else expFile << "," << endl << "  " << VarLabel(j);
                };

                first = false;
                count = (count+1)%10;
            }
        }

        if (!first) expFile << endl << endl;
    }

    if (opt & DISPLAY_FIXED)
    {
        first = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            if (LRange(j)==URange(j))
            {
                if (first)
                    expFile << "Fixed" << endl << endl << "  " << VarLabel(j);
                else
                {
                    if (count) expFile << ", " << VarLabel(j);
                    else expFile << "," << endl << "  " << VarLabel(j);
                }

                first = false;
                count = (count+1)%10;
            }
        }

        if (!first) expFile << endl << endl;
    }

    if (opt & DISPLAY_PRIMAL)
    {
        first = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            TFloat c = X(j);

            if (fabs(c)<epsilon) continue;

            if (first)
                expFile << "Primal" << endl << endl << "  ( "
                    << setw(maxColLen) << VarLabel(j) << " "
                    << setw(maxFloatLen) << c;
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxColLen) << VarLabel(j) << " "
                     << setw(maxFloatLen) << c;
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxColLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;
    }

    if (opt & DISPLAY_DUAL)
    {
        first = true;
        count = 0;
        for (TRestr j=0;j<K()+L();j++)
        {
            if (RestrType(j)!=BASIC_LB) continue;

            TFloat c = Y(j,LOWER);

            if (fabs(c)<epsilon) continue;

            if (first)
            {
                expFile << "Dual (lower)" << endl << endl << "  ( "
                    << setw(maxUniLen) << UnifiedLabel(j)
                    << " " << setw(maxFloatLen) << c;
            }
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxUniLen) << UnifiedLabel(j)
                    << " " << setw(maxFloatLen) << c;
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxUniLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;

        first = true;
        count = 0;

        for (TRestr j=0;j<K()+L();j++)
        {
            if (RestrType(j)!=BASIC_UB) continue;

            TFloat c = Y(j,UPPER);

            if (fabs(c)<epsilon) continue;

            if (first)
            {
                expFile << "Dual (upper)" << endl << endl << "  ( "
                    << setw(maxUniLen) << UnifiedLabel(j)
                    << " " << setw(maxFloatLen) << c;
            }
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxUniLen) << UnifiedLabel(j)
                    << " " << setw(maxFloatLen) << c;
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxUniLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;
    }

    if (opt & DISPLAY_VALUES)
    {
        first = true;
        count = 0;

        for (TVar j=0;j<L();j++)
        {
            TFloat c = VarValue(j);

            if (fabs(c)==InfFloat) continue;

            if (first)
                expFile << "Values" << endl << endl << "  ( "
                    << setw(maxColLen) << VarLabel(j) << " "
                    << setw(maxFloatLen) << c;
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxColLen) << VarLabel(j) << " "
                     << setw(maxFloatLen) << c;
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxColLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;
    }

    if (opt & DISPLAY_SLACKS)
    {
        first = true;
        count = 0;

        for (TRestr j=0;j<K()+L();j++)
        {
            if (LBound(j)<=-InfFloat) continue;

            if (RestrType(j)==RESTR_CANCELED) continue;

            if (fabs(Slack(j,LOWER))<epsilon) continue;

            if (first)
            {
                expFile << "Slacks (lower)" << endl << endl << "  ( "
                    << setw(maxUniLen) << UnifiedLabel(j) << " "
                    << setw(maxFloatLen) << Slack(j,LOWER);
            }
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxUniLen) << UnifiedLabel(j) << " "
                    << setw(maxFloatLen) << Slack(j,LOWER);
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxUniLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;

        first = true;
        count = 0;

        for (TRestr j=0;j<K()+L();j++)
        {
            if (UBound(j)>=InfFloat) continue;

            if (RestrType(j)==RESTR_CANCELED) continue;

            if (fabs(Slack(j,UPPER))<epsilon) continue;

            if (first)
            {
                expFile << "Slacks (upper)" << endl << endl << "  ( "
                    << setw(maxUniLen) << UnifiedLabel(j) << " "
                    << setw(maxFloatLen) << Slack(j,UPPER);
            }
            else
            {
                if (count) expFile << ", ";
                else expFile << "," << endl << "    ";

                expFile << setw(maxUniLen) << UnifiedLabel(j) << " "
                    << setw(maxFloatLen) << Slack(j,UPPER);
            }

            count = (count+1)%5;
            first = false;
        }

        if (!first)
            expFile << setw((maxFloatLen+maxUniLen+3)*((5-count)%5)+3)
                << ")" << endl << endl;
    }

    if (opt & DISPLAY_BASIS)
    {
        first = true;
        count = 0;

        if (columnOriented)
        {
            for (TVar j=0;j<L();j++)
            {
                char* tpStr = "lo";

                if (RestrType(Index(j))==BASIC_UB) tpStr = "up";

                if (first)
                {
                    expFile << "Column Index" << endl << endl << "  ( "
                        << setw(maxColLen) << VarLabel(j) << " "
                        << setw(maxUniLen) << UnifiedLabel(Index(j))
                        << " " << tpStr;
                }
                else
                {
                    if (count) expFile << ", ";
                    else expFile << "," << endl << "    ";

                    expFile << setw(maxColLen) << VarLabel(j) << " "
                        << setw(maxUniLen) << UnifiedLabel(Index(j))
                        << " " << tpStr;
                }

                count = (count+1)%5;
                first = false;
            }

            if (!first)
                expFile << setw((maxColLen+maxUniLen+6)*((5-count)%5)+3)
                    << ")" << endl << endl;
        }
        else
        {
            for (TRestr j=0;j<K();j++)
            {
                if (first)
                {
                    expFile << "Row Index" << endl << endl << "  ( "
                        << setw(maxRowLen) << RestrLabel(j) << " "
                        << setw(maxUniLen) << UnifiedLabel(RowIndex(j));
                }
                else
                {
                    if (count) expFile << ", ";
                    else expFile << "," << endl << "    ";

                    expFile << setw(maxRowLen) << RestrLabel(j) << " "
                        << setw(maxUniLen) << UnifiedLabel(RowIndex(j));
                }

                count = (count+1)%5;
                first = false;
            }

            if (!first)
                expFile << setw((maxRowLen+maxUniLen+3)*((5-count)%5)+3)
                    << ")" << endl << endl;
        }
    }

    if (opt & DISPLAY_TABLEAU)
    {
        expFile << "Tableau" << endl << endl;

        if (columnOriented)
        {
            int wd1 = maxColLen;
            int wd2 = CT.externalFloatLength;

            if (maxUniLen>wd2) wd2 = maxUniLen;

            expFile << "  " << setw(wd1) << " " << " |";

            for (TRestr i=0;i<K()+L();i++)
            {
                if (RevIndex(i)!=NoVar) continue;

                expFile << " " << setw(wd2) << UnifiedLabel(i);
            }

            expFile << endl << "  " << setfill('-') << setw(wd1+2) << "|"
                << setw((wd2+1)*K()+1) << " " << setfill(' ') << endl;

            for (TVar j=0;j<L();j++)
            {
                TRestr j0 = Index(j);

                expFile << "  " << setw(wd1) << UnifiedLabel(j0) << " |";

                for (TRestr i=0;i<K()+L();i++)
                {
                    if (RevIndex(i)!=NoVar) continue;

                    TFloat c = Tableau(j0,i);

                    if (fabs(c)>epsilon) 
                        expFile << " " << setw(wd2) << c;
                    else expFile << " " << setw(wd2) << " ";
                }

                expFile << endl;
            }
        }
        else
        {
            int wd1 = maxRowLen;
            int wd2 = CT.externalFloatLength;

            if (maxUniLen>wd2) wd2 = maxUniLen;

            expFile << "  " << setw(wd1) << " " << " |";

            for (TVar i=0;i<L();i++)
            {
                expFile << " " << setw(wd2) << UnifiedLabel(Index(i));
            }

            expFile << endl << "  " << setfill('-') << setw(wd1+2) << "|"
                << setw((wd2+1)*L()+1) << " " << setfill(' ') << endl;

            for (TRestr j=0;j<K();j++)
            {
                TRestr j0 = RowIndex(j);

                expFile << "  " << setw(wd1) << UnifiedLabel(j0) << " |";

                for (TVar i=0;i<L();i++)
                {
                    TVar i0 = Index(i);

                    TFloat c = Tableau(j0,i0);

                    if (fabs(c)>epsilon) 
                        expFile << " " << setw(wd2) << c;
                    else expFile << " " << setw(wd2) << " ";
                }

                expFile << endl;
            }
        }

        expFile << endl;
    }

    if (opt & DISPLAY_INVERSE)
    {
        expFile << "Inverse" << endl << endl;

        if (columnOriented)
        {
            int wd1 = maxColLen;
            int wd2 = CT.externalFloatLength;

            if (maxUniLen>wd2) wd2 = maxUniLen;

            expFile << "  " << setw(wd1) << " " << " |";

            for (TRestr i=0;i<L();i++)
                expFile << " " << setw(wd2) << UnifiedLabel(Index(i));

            expFile << endl << "  " << setfill('-') << setw(wd1+2) << "|"
                << setw((wd2+1)*L()+1) << " " << setfill(' ') << endl;

            for (TVar j=0;j<L();j++)
            {
                expFile << "  " << setw(wd1) << VarLabel(j) << " |";

                for (TVar i=0;i<L();i++)
                {
                    TFloat c = BaseInverse(Index(j),i);

                    if (fabs(c)>epsilon) 
                        expFile << " " << setw(wd2) << c;
                    else expFile << " " << setw(wd2) << " ";
                }

                expFile << endl;
            }
        }
        else
        {
            int wd1 = maxRowLen;
            int wd2 = CT.externalFloatLength;

            if (maxUniLen>wd2) wd2 = maxUniLen;

            expFile << "  " << setw(wd1) << " " << " |";

            for (TRestr i=0;i<K();i++)
                expFile << " " << setw(wd2) << RestrLabel(i);

            expFile << endl << "  " << setfill('-') << setw(wd1+2) << "|"
                << setw((wd2+1)*K()+1) << " " << setfill(' ') << endl;

            for (TRestr j=0;j<K()+L();j++)
            {
                if (RevIndex(j)!=NoVar) continue;

                expFile << "  " << setw(wd1) << UnifiedLabel(j) << " |";

                for (TRestr i=0;i<K();i++)
                {
                    TFloat c = BaseInverse(j,i);

                    if (fabs(c)>epsilon) 
                        expFile << " " << setw(wd2) << c;
                    else expFile << " " << setw(wd2) << " ";
                }

                expFile << endl;
            }
        }

        expFile << endl;
    }

    expFile << "End" << endl;

    delete[] index;
    delete[] val;

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif

    return;
}


void mipInstance::WriteMPSFile(const char* fileName,TLPFormat f)
    const throw(ERFile,ERRejected)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    ofstream expFile(fileName, ios::out);
    WriteMPSFile(expFile,f);

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void mipInstance::WriteMPSFile(ofstream& expFile,TLPFormat f)
    const throw(ERFile,ERRejected)
{
    char lineBuffer[64] = "";
    int l1 = 11;
    int l2 = CT.externalPrecision;

    expFile << "NAME          " << Label() << endl;

    if (f==MPS_CPLEX)
    {
        expFile << "OBJSENSE" << endl;

        switch (ObjectSense())
        {
            case MAXIMIZE :
            {
                expFile << "  MAX" << endl;
                break;
            }
            case NO_OBJECTIVE :
            case MINIMIZE :
            default :
            {
                expFile << "  MIN" << endl;
                break;
            }
        }

        expFile << "OBJNAME" << endl;
        expFile << "  OBJ" << endl;
    }

    expFile << "ROWS" << endl;
    expFile << " N  OBJ" << endl;

    for (TRestr i=0;i<K();i++)
    {
        if (LBound(i)==-InfFloat) expFile << " L  ";
        else if (UBound(i)==InfFloat) expFile << " G  ";
            else
            {
                if (LBound(i)==UBound(i)) expFile << " E  ";
                else expFile << " L  ";
            }

        expFile << RestrLabel(i) << endl;
    }

    expFile << "COLUMNS" << endl;
    TVar countInts = 0;

    for (TVar j=0;j<L();j++)
    {
        if (VarType(j)==VAR_INT) countInts++;
        else
        {
            TFloat thisCost = 0;
            if (ObjectSense()!=NO_OBJECTIVE) thisCost = Cost(j);

            sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                VarLabel(j),"OBJ",l1,l2,thisCost);
            expFile << lineBuffer;

            for (TRestr i=0;i<K();i++)
            {
                if (Coeff(i,j)!=0)
                { 
                    sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                        VarLabel(j),RestrLabel(i),l1,l2,Coeff(i,j));
                    expFile << lineBuffer;
                }
            }
        }
    }

    if (countInts>0 && f==MPS_CPLEX)
    {
        expFile << "    MARK0000  'MARKER'            'INTORG'" << endl;

        for (TVar j=0;j<L();j++)
        {
            if (VarType(j)==VAR_INT)
            {
                TFloat thisCost = 0;
                if (ObjectSense()!=NO_OBJECTIVE) thisCost = Cost(j);

                sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                    VarLabel(j),"OBJ",l1,l2,thisCost);
                expFile << lineBuffer;

                for (TRestr i=0;i<K();i++)
                {
                    if (Coeff(i,j)!=0) 
                    { 
                        sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                            VarLabel(j),RestrLabel(i),l1,l2,Coeff(i,j));
                        expFile << lineBuffer;
                    }
                }
            }
        }

        expFile << "    MARK0001  'MARKER'            'INTEND'" << endl;
    }

    expFile << "RHS" << endl;

    for (TRestr i=0;i<K();i++)
    {
        if (UBound(i)==InfFloat)
        {
            sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                "RHS",RestrLabel(i),l1,l2,LBound(i));
        }
        else
        {
            sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                "RHS",RestrLabel(i),l1,l2,UBound(i));
        }

        expFile << lineBuffer;
    }

    bool first = true;

    for (TRestr i=0;i<K();i++)
    {
        if (LBound(i)!=-InfFloat && UBound(i)!=InfFloat && LBound(i)<UBound(i))
        {
            if (first) expFile << "RANGES" << endl;

            sprintf(lineBuffer,"    %-8s  %-8s  %*.*f\n",
                "RHS",RestrLabel(i),l1,l2,UBound(i)-LBound(i));
            expFile << lineBuffer;
            first = false;
        }
    }

    first = true;

    for (TVar j=0;j<L();j++)
    {
        if (LRange(j)!=-InfFloat && URange(j)!=InfFloat && LRange(j)==URange(j))
        {
            if (first) expFile << "BOUNDS" << endl;

            sprintf(lineBuffer," FX %-8s  %-8s  %*.*f\n",
                "BOUND",VarLabel(j),l1,l2,URange(j));
            expFile << lineBuffer;
            first = false;
        }
        else
        {
            if ((LRange(j)!=0 && URange(j)>0) || URange(j)==0 ||
                (LRange(j)!=-InfFloat && URange(j)<0))
            {
                if (first) expFile << "BOUNDS" << endl;

                if (LRange(j)==-InfFloat)
                {
                    sprintf(lineBuffer," MI %-8s  %-8s  %*.*f\n",
                        "BOUND",VarLabel(j),l1,l2,LRange(j));
                }
                else
                {
                    sprintf(lineBuffer," LO %-8s  %-8s  %*.*f\n",
                        "BOUND",VarLabel(j),l1,l2,LRange(j));
                }

                expFile << lineBuffer;
                first = false;
            }

            if (URange(j)<InfFloat)
            {
                if (first) expFile << "BOUNDS" << endl;

                if (URange(j)==InfFloat)
                {
                    sprintf(lineBuffer," PL %-8s  %-8s  %*.*f\n",
                        "BOUND",VarLabel(j),l1,l2,URange(j));
                }
                else
                {
                    sprintf(lineBuffer," UP %-8s  %-8s  %*.*f\n",
                        "BOUND",VarLabel(j),l1,l2,URange(j));
                }
                expFile << lineBuffer;
                first = false;
            }
        }
    }

    expFile << "ENDATA" << endl;

    return;
}


void mipInstance::WriteBASFile(const char* fileName,TLPFormat f)
    const throw(ERFile,ERRejected)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    ofstream expFile(fileName, ios::out);
    WriteBASFile(expFile,f);

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void mipInstance::WriteBASFile(ofstream& expFile,TLPFormat f)
    const throw(ERFile,ERRejected)
{
    char lineBuffer[64] = "";

    expFile << "NAME          " << Label() << endl;

    for (TVar j=0;j<L();j++)
    {
        if (Index(j)<K())
        {
            if (RestrType(Index(j))==BASIC_UB)
            {
                sprintf(lineBuffer," XU %-8s  %-8s",
                    VarLabel(j),RestrLabel(Index(j)));
            }
            else
            {
                sprintf(lineBuffer," XL %-8s  %-8s",
                    VarLabel(j),RestrLabel(Index(j)));
            }
        }
        else
        {
            if (RestrType(Index(j))==BASIC_UB)
            {
                sprintf(lineBuffer," UL %-8s",VarLabel(j));

                if (f==BAS_GOBLIN) sprintf(lineBuffer,"%s  %-8s",lineBuffer,
                    VarLabel(Index(j)-K()));
            }
            else
            {
                sprintf(lineBuffer," LL %-8s",VarLabel(j));

                if (f==BAS_GOBLIN) sprintf(lineBuffer,"%s  %-8s",lineBuffer,
                    VarLabel(Index(j)-K()));
            }
        }

        expFile << lineBuffer << endl;
    }

    expFile << "ENDATA" << endl;

    return;
}


void mipInstance::Write(const char* fileName,TOption opt)
    const throw(ERFile,ERRejected)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    goblinExport F(fileName,CT);

    F.StartTuple("mixed_integer",0);

    F.StartTuple("rows",1);
    F.MakeItem(K(),0);
    F.EndTuple();

    F.StartTuple("columns",1);
    F.MakeItem(L(),0);
    F.EndTuple();

    F.StartTuple("size",1);
    F.MakeItem(100,0);
    F.EndTuple();

    F.StartTuple("pivot",1);

    if (pivotRow!=NoRestr && pivotColumn!=NoVar)
    {
        F.MakeItem(pivotRow,0);
        F.MakeItem(pivotColumn,0);
        F.MakeItem(static_cast<long>(pivotDir),0);
    }
    else F.MakeNoItem(0);

    F.EndTuple();

    WriteVarValues(&F);

    F.StartTuple("rowvis",1);
    F.MakeItem(1,0);
    F.EndTuple();

    F.StartTuple("colvis",1);
    F.MakeItem(1,0);
    F.EndTuple();

    F.WriteConfiguration(CT);

    F.EndTuple(); // mixed_integer

    F.Stream() << endl << endl;
    WriteMPSFile(F.Stream(),MPS_CPLEX);

    F.Stream() << endl;
    WriteBASFile(F.Stream(),BAS_GOBLIN);

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void mipInstance::Write(const char* fileName,TLPFormat f,TOption opt)
    const throw(ERFile,ERRejected)
{
    switch (f)
    {
        case GOB_FORMAT:
        {
            Write(fileName,opt);
            return;
        }
        case LP_FORMAT:  WriteLPNaive(fileName,TDisplayOpt(opt)); return;
        case MPS_CPLEX:
        case MPS_FORMAT:
        {
            WriteMPSFile(fileName,f);
            return;
        }
        case BAS_CPLEX:
        case BAS_GOBLIN:
        {
            WriteBASFile(fileName);
            return;
        }
    }
}


void mipInstance::ExportToAscii(const char* fileName,TOption format) const throw(ERFile)
{
    WriteLPNaive(fileName,TDisplayOpt(format));
}


//****************************************************************************//
//                               File Import                                  //
//****************************************************************************//


TRestr mipInstance::ReadRowLabel(char* thisLabel) throw()
{
    TRestr i = RestrIndex(thisLabel);

    if (i==NoRestr)
    {
        i = AddRestr(0,0);
        SetRestrLabel(i,thisLabel,OWNED_BY_SENDER);
    }

    return i;
}


TVar mipInstance::ReadColLabel(char* thisLabel, bool markInt) throw()
{
    TVar j = VarIndex(thisLabel);

    if (j==NoVar)
    {
        if (markInt) j = AddVar(0,InfFloat,0,VAR_INT);
        else j = AddVar(0,InfFloat,0,VAR_FLOAT);

        SetVarLabel(j,thisLabel,OWNED_BY_SENDER);
    }

    return j;
}


void mipInstance::ReadMPSFile(const char* fileName)
    throw(ERParse,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (K()>0 && L()>0)
        Error(ERR_REJECTED,"ReadMPSFile","Problem must be initial");

    #endif

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    ifstream impFile(fileName, ios::in);
    ReadMPSFile(impFile);

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void mipInstance::ReadMPSFile(ifstream& impFile) throw(ERParse,ERRejected)
{
    char lineBuffer[200] = "";
    char fieldBuffer1[200] = "";
    char fieldBuffer2[200] = "";
    char fieldBuffer3[200] = "";
    char fieldBuffer4[200] = "";
    char fieldBuffer5[200] = "";

    char* objectiveLabel = NULL;
    char* rhsLabel = NULL;
    char* rangeLabel = NULL;
    char* boundLabel = NULL;

    long int lineNr = 0;
    bool markInt = 0;
    char state = 0;
    bool objectiveFound = false;

    while (impFile.getline(lineBuffer,200,'\n') && state<11)
    {
        lineNr++;

        if (strlen(lineBuffer)==0) continue;

        switch (state)
        {
            case 0:
            {
                if (strncmp(lineBuffer,"NAME",4)==0)
                {
                    state = 1;
                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Missing \"NAME\" descriptor");
            }
            case 1:
            {
                if (strncmp(lineBuffer,"OBJSENSE",8)==0)
                {
                    state = 2;
                    break;
                }

                if (strncmp(lineBuffer,"OBJNAME",7)==0)
                {
                    state = 4;
                    break;
                }

                if (strncmp(lineBuffer,"ROWS",4)==0)
                {
                    state = 6;
                    break;
                }

                Error(ERR_PARSE,"ReadMPSFile","\"ROWS\" descriptor expected");
            }
            case 2:
            {
                if      (strncmp(lineBuffer,"  MAX",5)==0)
                {
                    SetObjectSense(MAXIMIZE);
                    state = 3;
                    break;
                }
                else if (strncmp(lineBuffer,"  MIN",5)==0)
                {
                    SetObjectSense(MINIMIZE);
                    state = 3;
                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Invalid \"OBJSENSE\" specifier");
            }
            case 3:
            {
                if (strncmp(lineBuffer,"OBJNAME",7)==0)
                {
                    state = 4;
                    break;
                }

                if (strncmp(lineBuffer,"ROWS",4)==0)
                {
                    state = 6;
                    break;
                }

                Error(ERR_PARSE,"ReadMPSFile","\"ROWS\" descriptor expected");
            }
            case 4:
            {
                if (strncmp(lineBuffer,"  ",2)==0 &&
                    sscanf(lineBuffer,"%s",fieldBuffer1)==1)
                {
                    // Extract objective name

                    objectiveLabel = new char[strlen(fieldBuffer1)+1];
                    strcpy(objectiveLabel,fieldBuffer1);

                    state = 5;
                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Missing \"OBJNAME\" specifier");
            }
            case 5:
            {
                if (strncmp(lineBuffer,"ROWS",4)==0)
                {
                    state = 6;
                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Missing \"ROWS\" descriptor");
            }
            case 6:
            {
                if (strncmp(lineBuffer,"COLUMNS",7)==0)
                {
                    state = 7;
                    break;
                }

                if (sscanf(lineBuffer,"%s %s",fieldBuffer1,fieldBuffer2)==2)
                {
                    if (strcmp(fieldBuffer1,"N")!=0)
                    {
                        TIndex i = ReadRowLabel(fieldBuffer2);
                        if (strcmp(fieldBuffer1,"L")==0)
                            SetLBound(i,-InfFloat);
                        if (strcmp(fieldBuffer1,"G")==0)
                            SetUBound(i,InfFloat);
                    }
                    else if (objectiveLabel==NULL)
                    {
                        objectiveLabel = new char[strlen(fieldBuffer2)+1];
                        strcpy(objectiveLabel,fieldBuffer2);
                    }

                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Parse error in \"ROWS\" section");
            }
            case 7:
            {
                if (strncmp(lineBuffer,"RHS",3)==0)
                {
                    state = 8;
                    break;
                }

                // Read matrix and objective function

                int width = sscanf(lineBuffer,"%s %s %s %s %s",
                    fieldBuffer1,fieldBuffer2,fieldBuffer3,fieldBuffer4,fieldBuffer5);

                if (width==3 || width==5)
                {
                    if (strcmp(fieldBuffer2,"'MARKER'")==0)
                    {
                        if (strcmp(fieldBuffer3,"'INTORG'")==0)
                            markInt = true;
                        if (strcmp(fieldBuffer3,"'INTEND'")==0)
                            markInt = false;
                        break;
                    }

                    TIndex j = ReadColLabel(fieldBuffer1,markInt);

                    if (strcmp(fieldBuffer2,objectiveLabel)!=0)
                    {
                        TIndex i = ReadRowLabel(fieldBuffer2);
                        SetCoeff(i,j,atof(fieldBuffer3));
                    }
                    else
                    {
                        SetCost(j,atof(fieldBuffer3));

                        if (atof(fieldBuffer3)!=0) objectiveFound = true;
                    }

                    if (width==5)
                    {
                        if (strcmp(fieldBuffer4,objectiveLabel)!=0)
                        {
                            TIndex i = ReadRowLabel(fieldBuffer4);
                            SetCoeff(i,j,atof(fieldBuffer5));
                        }
                        else
                        {
                            SetCost(j,atof(fieldBuffer5));

                            if (atof(fieldBuffer5)!=0) objectiveFound = true;
                        }
                    }

                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Parse error in \"COLUMNS\" section");
            }
            case 8:
            {
                if (strncmp(lineBuffer,"RANGES",6)==0)
                {
                    state = 9;
                    break;
                }

                if (strncmp(lineBuffer,"BOUNDS",6)==0)
                {
                    state = 10;
                    break;
                }

                if (strncmp(lineBuffer,"ENDDATA",7)==0 ||
                    strncmp(lineBuffer,"ENDATA",6)==0)
                {
                    state = 11;
                    break;
                }

                // Read right hand sides

                int width = sscanf(lineBuffer,"%s %s %s %s %s",
                    fieldBuffer1,fieldBuffer2,fieldBuffer3,fieldBuffer4,fieldBuffer5);

                if (width==3 || width==5)
                {
                    if (rhsLabel==NULL)
                    {
                        rhsLabel = new char[strlen(fieldBuffer1)+1];
                        strcpy(rhsLabel,fieldBuffer1);
                    }

                    if (strcmp(fieldBuffer1,rhsLabel)==0)
                    {
                        TIndex i = ReadRowLabel(fieldBuffer2);
                        TFloat val = atof(fieldBuffer3);
                        if (val<0)
                        {
                            if (LBound(i)==0) SetLBound(i,val);
                            if (UBound(i)==0) SetUBound(i,val);
                        }
                        else
                        {
                            if (UBound(i)==0) SetUBound(i,val);
                            if (LBound(i)==0) SetLBound(i,val);
                        }

                        if (width==5)
                        {
                            i = ReadRowLabel(fieldBuffer4);
                            val = atof(fieldBuffer5);
                            if (val<0)
                            {
                                if (LBound(i)==0) SetLBound(i,val);
                                if (UBound(i)==0) SetUBound(i,val);
                            }
                            else
                            {
                                if (UBound(i)==0) SetUBound(i,val);
                                if (LBound(i)==0) SetLBound(i,val);
                            }
                        }
                    }

                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Parse error in \"RHS\" section");
            }
            case 9:
            {
                if (strncmp(lineBuffer,"BOUNDS",6)==0)
                {
                    state = 10;
                    break;
                }

                if (strncmp(lineBuffer,"ENDDATA",7)==0 ||
                    strncmp(lineBuffer,"ENDATA",6)==0)
                {
                    state = 11;
                    break;
                }

                // Read differences between upper and lower right hand sides

                int width = sscanf(lineBuffer,"%s %s %s %s %s",
                    fieldBuffer1,fieldBuffer2,fieldBuffer3,fieldBuffer4,fieldBuffer5);

                if (width==3 || width==5)
                {
                    if (rangeLabel==NULL)
                    {
                        rangeLabel = new char[strlen(fieldBuffer1)+1];
                        strcpy(rangeLabel,fieldBuffer1);
                    }

                    if (strcmp(fieldBuffer1,rangeLabel)==0)
                    {
                        TIndex i = ReadRowLabel(fieldBuffer2);
                        TFloat val = atof(fieldBuffer3);

                        if (UBound(i)==LBound(i))
                        {
                            if (val<0) SetLBound(i,LBound(i)-val);
                            else SetUBound(i,UBound(i)+val);
                        }
                        else
                        {
                            if (UBound(i)==InfFloat)
                                SetUBound(i,LBound(i)+fabs(val));
                            if (LBound(i)==-InfFloat)
                                SetLBound(i,UBound(i)-fabs(val));
                        }

                        if (width==5)
                        {
                            i = ReadRowLabel(fieldBuffer4);
                            val = atof(fieldBuffer5);

                            if (val<0)
                            {
                                if (LBound(i)==0) SetLBound(i,val);
                                if (UBound(i)==0) SetUBound(i,val);
                            }
                            else
                            {
                                if (UBound(i)==0) SetUBound(i,val);
                                if (LBound(i)==0) SetLBound(i,val);
                            }
                        }
                    }

                    break;
                }
                else Error(ERR_PARSE,"ReadMPSFile","Parse error in \"RANGES\" section");
            }
            case 10:
            {
                if (strncmp(lineBuffer,"ENDDATA",7)==0 ||
                    strncmp(lineBuffer,"ENDATA",6)==0)
                {
                    state = 11;
                    break;
                }

                // Read variable bounds

                int nArg = sscanf(lineBuffer,"%s %s %s %s",
                    fieldBuffer1,fieldBuffer2,fieldBuffer3,fieldBuffer4);

                if (nArg>=3)
                {
                    if (boundLabel==NULL)
                    {
                        boundLabel = new char[strlen(fieldBuffer2)+1];
                        strcpy(boundLabel,fieldBuffer2);
                    }

                    TIndex j = ReadColLabel(fieldBuffer3,false);
                    if (strcmp(fieldBuffer2,boundLabel)==0)
                    {
                        if (strcmp(fieldBuffer1,"BV")==0)
                        {
                            if (1<LRange(j))
                            {
                                SetLRange(j,0);
                                SetURange(j,1);
                            }
                            else
                            {
                                SetURange(j,1);
                                SetLRange(j,0);
                            }

                            SetVarType(j,VAR_INT);
                            break;
                        }

                        if (nArg==3) Error(ERR_PARSE,"ReadMPSFile",
                            "Parse error in \"BOUNDS\" section");

                        TFloat val = atof(fieldBuffer4);

                        if (strcmp(fieldBuffer1,"FR")==0)
                        {
                            SetLRange(j,-InfFloat);
                            SetURange(j,InfFloat);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"MI")==0)
                        {
                            SetLRange(j,-InfFloat);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"PL")==0)
                        {
                            SetURange(j,InfFloat);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"FX")==0)
                        {
                            SetLRange(j,-InfFloat);
                            SetURange(j,InfFloat);
                            SetLRange(j,val);
                            SetURange(j,val);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"LO")==0)
                        {
                            if (val>URange(j)) SetURange(j,val);

                            SetLRange(j,val);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"LI")==0)
                        {
                            if (val>URange(j)) SetURange(j,val);

                            SetLRange(j,val);
                            SetVarType(j,VAR_INT);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"UP")==0)
                        {
                            if (val<LRange(j)) SetLRange(j,val);

                            SetURange(j,val);
                            break;
                        }

                        if (strcmp(fieldBuffer1,"UI")==0)
                        {
                            if (val<LRange(j)) SetLRange(j,val);

                            SetURange(j,val);
                            SetVarType(j,VAR_INT);
                            break;
                        }
                    }
                }

                Error(ERR_PARSE,"ReadMPSFile","Parse error in \"BOUNDS\" section");
            }
        }
    }

    delete[] objectiveLabel;
    delete[] rhsLabel;
    delete[] rangeLabel;
    delete[] boundLabel;

    if (!objectiveFound) SetObjectSense(NO_OBJECTIVE);
}


void mipInstance::ReadBASFile(const char* fileName)
    throw(ERParse,ERRejected)
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    ifstream impFile(fileName, ios::in);
    ReadBASFile(impFile);

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


void mipInstance::ReadBASFile(ifstream& impFile) throw(ERParse,ERRejected)
{
    char lineBuffer[64] = "";
    char fieldBuffer1[64] = "";
    char fieldBuffer2[64] = "";
    char fieldBuffer3[64] = "";

    long int lineNr = 0;
    char state = 0;
    while (impFile.getline(lineBuffer,64,'\n') && state<2)
    {
        lineNr++;

        if (strlen(lineBuffer)==0) continue;

        switch (state)
        {
            case 0:
            {
                if (strncmp(lineBuffer,"NAME",4)==0)
                {
                    state = 1;
                    break;
                }
                else Error(ERR_PARSE,"ReadBASFile","Missing \"NAME\" descriptor");
            }

            case 1:
            {
                if (strncmp(lineBuffer,"ENDATA",7)==0 ||
                    strncmp(lineBuffer,"ENDDATA",7)==0)
                {
                    state = 2;
                    break;
                }

                char nArg = sscanf(lineBuffer,"%s %s %s",
                    fieldBuffer1,fieldBuffer2,fieldBuffer3);

                if (nArg>=2)
                {
                    TIndex j = ReadColLabel(fieldBuffer2,false);

                    if (strcmp(fieldBuffer1,"UL")==0 ||
                        strcmp(fieldBuffer1,"LL")==0)
                    {
                        TIndex i = K()+j;
                        i = K()+ReadColLabel(fieldBuffer3,false);

                        if (strcmp(fieldBuffer1,"UL")==0)
                            SetIndex(i,j,UPPER);
                        else SetIndex(i,j,LOWER);

                        break;
                    }

                    if (nArg==2) Error(ERR_PARSE,"ReadBASFile",
                        "Parse error in BAS file");

                    TIndex i = ReadRowLabel(fieldBuffer3);
                    if (strcmp(fieldBuffer1,"XU")==0 ||
                        strcmp(fieldBuffer1,"XL")==0)
                    {
                        if (strcmp(fieldBuffer1,"XU")==0)
                            SetIndex(i,j,UPPER);
                        else SetIndex(i,j,LOWER);

                        break;
                    }
                }

                Error(ERR_PARSE,"ReadBASFile","Parse error in BAS file");
            }
        }
    }
}


void mipInstance::NoSuchVar(char* methodName,TVar j) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such variable: %ld",(unsigned long)j);

    CT.Error(ERR_RANGE,Handle(),methodName,CT.logBuffer);
}


void mipInstance::NoSuchRestr(char* methodName,TRestr i) const throw(ERRange)
{
    sprintf(CT.logBuffer,"No such restriction: %ld",(unsigned long)i);

    CT.Error(ERR_RANGE,Handle(),methodName,CT.logBuffer);
}


char* mipInstance::VarLabel(TVar i,TOwnership tp) const throw(ERRange)
{
    sprintf(labelBuffer,"%ld",L());
    int len = strlen(labelBuffer);
    sprintf(labelBuffer,"x%*.*ld",len,len,i+1);

    if (tp==OWNED_BY_RECEIVER) return labelBuffer;

    char* retLabel = new char[strlen(labelBuffer)+1];
    strcpy(retLabel,labelBuffer);
    return retLabel;
}


char* mipInstance::RestrLabel(TRestr i,TOwnership tp) const throw(ERRange)
{
    sprintf(labelBuffer,"%ld",K());
    int len = strlen(labelBuffer);
    sprintf(labelBuffer,"r%*.*ld",len,len,i+1);

    if (tp==OWNED_BY_RECEIVER) return labelBuffer;

    char* retLabel = new char[strlen(labelBuffer)+1];
    strcpy(retLabel,labelBuffer);
    return retLabel;
}


TVar mipInstance::VarIndex(char* thisLabel) const throw()
{
    TVar i = 0;

    for (;i<L() && strcmp(thisLabel,VarLabel(i,OWNED_BY_RECEIVER));i++) {};

    if (i==L()) return NoVar;

    return i;
}


TRestr mipInstance::RestrIndex(char* thisLabel) const throw()
{
    TRestr i = 0;

    for (;i<K() && strcmp(thisLabel,RestrLabel(i,OWNED_BY_RECEIVER));i++) {};

    if (i==K()) return NoRestr;

    return i;
}


TFloat mipInstance::ObjVal()
    const throw()
{
    TFloat s = 0;

    for (TVar i=0;i<L();i++)
    {
        if (varValue)
            s += Cost(i)*varValue[i];
        else s += Cost(i)*X(i);
    }

    return s;
}


//****************************************************************************//
//                           Integer Variable Values                          //
//****************************************************************************//


TFloat mipInstance::VarValue(TVar j)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (varValue && numVars!=L())
        InternalError("VarValue","Number of variables has changed");

    if (j>=L())  NoSuchVar("VarValue",j);

    #endif

    if (!varValue) return InfFloat;

    return varValue[j];
}


void mipInstance::SetVarValue(TVar j,TFloat val)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (varValue && numVars!=L())
        InternalError("SetVarValue","Number of variables has changed");

    if (j>=L())  NoSuchVar("SetVarValue",j);

    #endif

    if (!varValue)
    {
        if (fabs(val)<InfFloat)
        {
            InitVarValues();
            varValue[j] = val;
        }
    }
    else varValue[j] = val;
}


void mipInstance::InitVarValues(TFloat defVal)
    throw()
{
    if (!varValue)
    {
        numVars = L();
        varValue =  new TFloat[numVars];
        LogEntry(LOG_MEM,"...Variable values allocated");
    }
    else
    {
        #if defined(_LOGGING_)

        Error(MSG_WARN,"InitVarValues","Variable values are already present");

        #endif
    }

    for (TNode i=0;i<numVars;i++) varValue[i] = defVal;
}


void mipInstance::ReleaseVarValues()
    throw()
{
    if (varValue)
    {
        delete[] varValue;
        varValue = NULL;
        LogEntry(LOG_MEM,"...Variable values disallocated");
    }
}


void mipInstance::WriteVarValues(goblinExport* F)
    const throw()
{
    if (varValue==NULL)
    {
        F -> StartTuple("values",1);
        F -> MakeNoItem(0);
    }
    else
    {
        #if defined(_FAILSAVE_)

        if (numVars!=L())
        {
            InternalError("WriteVarValues","Number of variables has changed");
        }

        #endif

        F -> StartTuple("values",10);

        int Length = 1;

        for (TVar j=0;j<L();++j)
        {
            int thisLength = CT.ExternalLength<TFloat>(varValue[j]);

            if (thisLength>Length) Length = thisLength;
        }

        for (TVar j=0;j<L();++j)
        {
            if (varValue[j]!=InfFloat)
                F->MakeItem(varValue[j],Length);
            else F->MakeNoItem(10);
        }
    }

    F -> EndTuple();
}


void mipInstance::ReadVarValues(goblinImport* F,TVar l)
    throw(ERParse)
{
    ReleaseVarValues();

    F -> Scan("values");
    varValue = F->GetTFloatTuple(l);

    if (F->Constant())
    {
        delete[] varValue;
        varValue = NULL;
    }
    else
    {
        numVars = l;
        LogEntry(LOG_MEM,"...Variable values allocated");
    }
}


TFloat mipInstance::Slack(TRestr i,TLowerUpper lu) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=K()+L()) NoSuchRestr("Slack",i);

    #endif

    if ((RestrType(i)==BASIC_UB && lu==UPPER ) ||
        (RestrType(i)==BASIC_LB && lu==LOWER))
       return 0;

    if (lu==LOWER && LBound(i)== -InfFloat)
        return InfFloat;

    if (lu==UPPER && UBound(i)== InfFloat)
        return InfFloat;

    if (i>=K())
    {
        if (lu==LOWER) return X(i-K())-LBound(i);
        else return UBound(i)-X(i-K());
    }

    TFloat slack = 0;

    if (lu==LOWER)
    {
        slack = -LBound(i);
        for (TVar j=0;j<L();j++)
            slack += X(j)*Coeff(i,j);
    }

    if (lu==UPPER)
    {
        slack = UBound(i);
        for (TVar j=0;j<L();j++)
            slack -= X(j)*Coeff(i,j);
    }

    return slack;
}


bool mipInstance::PrimalFeasible(TFloat epsilon)
    throw()
{
    for (TRestr i=0;i<K()+L();i++)
    {
        if (Slack(i,LOWER)< -epsilon || Slack(i,UPPER)< -epsilon)
        {
            sprintf(CT.logBuffer,"...Primal infeasibility at restriction %ld",i);
            LogEntry(LOG_RES2,CT.logBuffer);
            return false;
        }
    }

    LogEntry(LOG_RES2,"...Basis is primal feasible");

    return true;
}


bool mipInstance::DualFeasible(TFloat epsilon)
    throw()
{
    for (TVar i=0;i<L();i++)
    {
        TRestr j = Index(i);

        if (LBound(j)>=UBound(j)) return true;

        if (Y(j,LOWER)< -epsilon || Y(j,UPPER)> epsilon || 
             (Y(j,LOWER)>epsilon && LBound(j)== -InfFloat) )
        {
            sprintf(CT.logBuffer,"...Dual infeasibility at variable %ld",i);
            LogEntry(LOG_RES2,CT.logBuffer);
            return false;
        }
    }

    LogEntry(LOG_RES2,"...Basis is dual feasible");

    return true;
}


void mipInstance::SetVarVisibility(TVar,bool) throw(ERRange)
{
}


void mipInstance::SetRestrVisibility(TRestr,bool) throw(ERRange)
{
}


bool mipInstance::VarVisibility(TVar) const throw(ERRange)
{
    return true;
}


bool mipInstance::RestrVisibility(TRestr) const throw(ERRange)
{
    return true;
}


void mipInstance::ResetBasis() throw()
{
    InitBasis();
}


//****************************************************************************//
//                                   Solver                                   //
//****************************************************************************//


TFloat mipInstance::SolveLP()
    throw(ERRejected) 
{
    for (TVar j=0;j<L();j++)
        if (RestrType(Index(j))!=BASIC_UB && RestrType(Index(j))!=BASIC_LB)
        {
            InternalError("SolveLP","Indices are corrupted");
        }

    moduleGuard M(ModLpSolve,*this,"Starting LP solver...");

    if (ObjectSense()==MAXIMIZE)
        for (TVar i=0;i<L();i++) SetCost(i,-Cost(i));

    TFloat ret = InfFloat;
    TSimplexMethod usedMethod = TSimplexMethod(CT.methLP);

    if (usedMethod==SIMPLEX_AUTO)
    {
        if (!Initial() && DualFeasible())  usedMethod = SIMPLEX_DUAL;
        else usedMethod = SIMPLEX_PRIMAL;
    }

    if (usedMethod==SIMPLEX_PRIMAL)
    {
        LogEntry(LOG_METH,"(Primal simplex method)");
        LogEntry(LOG_METH,"Phase I pivoting...");

        bool phaseI = StartPrimal();

        if (CT.SolverRunning())
        {
            if (phaseI==false)
            {
                sprintf(CT.logBuffer,"...Problem is infeasible");
                ret = -InfFloat;
            }
            else
            {
                LogEntry(LOG_METH,"Phase II pivoting...");

                ret = SolvePrimal();

                if (ret==InfFloat)
                {
                    if (ObjectSense()==MAXIMIZE) ret *= -1;

                    sprintf(CT.logBuffer,"...Problem is unbounded");
                }
                else
                {
                    if (ObjectSense()==MAXIMIZE) ret *= -1;

                    sprintf(CT.logBuffer,"...Optimal Objective Value: %g",ret);
                }
            }
        }
    }
    else
    {
        LogEntry(LOG_METH,"(Dual simplex method)");

        LogEntry(LOG_METH,"Phase I pivoting...");
        bool phaseI = StartDual();

        if (CT.SolverRunning())
        {
            if (phaseI==false)
            {
                sprintf(CT.logBuffer,"...Problem is unbounded");
                ret = -InfFloat;
            }
            else
            {
                LogEntry(LOG_METH,"Phase II pivoting...");

                ret = SolveDual();

                if (ret==InfFloat)
                {
                    if (ObjectSense()==MAXIMIZE) ret *= -1;

                    sprintf(CT.logBuffer,"...Problem is infeasible");
                }
                else
                {
                    if (ObjectSense()==MAXIMIZE) ret *= -1;

                    sprintf(CT.logBuffer,"...Optimal Objective Value: %g",ret);
                }
            }
        }
    }

    if (ObjectSense()==MAXIMIZE)
    {
        for (TVar i=0;i<L();i++) SetCost(i,-Cost(i));
    }

    M.Shutdown(LOG_RES,CT.logBuffer);

    return ret;
}


void mipInstance::AddCuttingPlane() throw(ERRejected)
{
    Error(ERR_REJECTED,"AddCuttingPlane","Not implemented yet");

    throw ERRejected();
}


mipFactory::mipFactory() throw()
{
}


mipFactory::~mipFactory() throw()
{
}
