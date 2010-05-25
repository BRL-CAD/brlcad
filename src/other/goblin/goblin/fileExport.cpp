
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileExport.cpp
/// \brief  #goblinExport class implementation

#include "fileExport.h"


goblinExport::goblinExport(const char* expFileName,goblinController &thisContext)
    throw(ERFile) : expFile(expFileName, ios::out), CT(thisContext)
{
    if (!expFile)
    {
        sprintf(CT.logBuffer,"Could not open export file %s, io_state %d",
            expFileName,expFile.rdstate());
        CT.Error(ERR_FILE,NoHandle,"goblinExport",CT.logBuffer);
    }

    expFile.flags(expFile.flags() | ios::right);
    expFile.setf(ios::scientific,ios::floatfield);
    expFile.precision(CT.externalPrecision-1);

    currentLevel = 0;
    currentType = 0;
}


void goblinExport::StartTuple(const char* header, char type) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentType != 0)
        CT.Error(ERR_REJECTED,NoHandle,"StartTuple","Illegal operation");

    #endif

    if (currentLevel>0) expFile << endl;

    currentLevel ++;
    currentPos = type;
    currentType = type;
    expFile.width(currentLevel);
    expFile << "(" << header;
}


void goblinExport::StartTuple(unsigned long k, char type) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentType != 0)
        CT.Error(ERR_REJECTED,NoHandle,"StartTuple","Illegal operation");

    #endif

    currentLevel ++;
    currentPos = type;   
    currentType = type;

    expFile << endl;
    expFile.width(currentLevel);
    expFile << "(" << k;
}


void goblinExport::EndTuple() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentLevel == 0)
        CT.Error(ERR_REJECTED,NoHandle,"EndTuple","Exceeding minimum depth");

    #endif

    if (currentType == 0)
    {
        expFile << endl;
        expFile.width(currentLevel);
    }

    expFile << ")";

    currentType = 0;
    currentLevel --;
}


template <> void goblinExport::MakeItem(const char* item,int length) throw()
{
    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(currentLevel+1);
        expFile << "";
    }
    else
    {
        currentPos ++;
        expFile << " ";
    }

    expFile.width(length);
    expFile << "\"" << item << "\"";
}


static char itemBuffer[25];

template <> void goblinExport::MakeItem(double item,int length) throw()
{
    if (fabs(item)==InfFloat)
    {
        MakeNoItem(length);
        return;
    }

    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(currentLevel+1);
        expFile << "";
    }
    else
    {
        currentPos ++;
        expFile << " ";
    }

    sprintf(itemBuffer,"%*.*f",length,CT.externalPrecision,double(item));
    expFile.width(length);
    expFile << itemBuffer;
}


template <> void goblinExport::MakeItem(float item,int length) throw()
{
    MakeItem(static_cast<double>(item),length);
}


template <typename T> void goblinExport::MakeItem(T item,int length) throw()
{
    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(length+currentLevel+1);
    }
    else
    {
        currentPos ++;
        expFile << " ";
        expFile.width(length);
    }

    expFile << item;
}


template void goblinExport::MakeItem(int item,int length) throw();
template void goblinExport::MakeItem(unsigned item,int length) throw();
template void goblinExport::MakeItem(long item,int length) throw();
template void goblinExport::MakeItem(unsigned long item,int length) throw();
template void goblinExport::MakeItem(char item,int length) throw();
template void goblinExport::MakeItem(unsigned char item,int length) throw();
template void goblinExport::MakeItem(short item,int length) throw();
template void goblinExport::MakeItem(unsigned short item,int length) throw();
template void goblinExport::MakeItem(TOptLayoutTokens item,int length) throw();


void goblinExport::MakeNoItem(int length) throw()
{
    if ((currentType != 1)&&(currentType==currentPos))
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(length+currentLevel+1);
    }
    else
    {
        currentPos ++;
        expFile << " ";
        expFile.width(length);
    }

    expFile << "*";
}


template <typename T>
void goblinExport::WriteAttribute(const T* array,const char* attributeLabel,size_t size,T undefined) throw()
{
    if (size==1)
    {
        StartTuple(attributeLabel,1);

        if  (array[0]==undefined)
        {
            MakeNoItem(0);
        }
        else
        {
            MakeItem(array[0],0);
        }
    }
    else
    {
        StartTuple(attributeLabel,10);

        int length = 1;

        for (size_t i=0;i<size;i++)
        {
            char thisLength = CT.template ExternalLength<T>(array[i]);

            if (array[i]!=undefined && thisLength>length) length = thisLength;
        }

        for (size_t i=0;i<size;i++)
        {
            if (array[i]!=undefined)
            {
                MakeItem(array[i],length);
            }
            else
            {
                MakeNoItem(length);
            }
        }
    }

    EndTuple();
}

template void goblinExport::WriteAttribute(
    const double* array,const char* attributeLabel,size_t size,double undefined) throw();
template void goblinExport::WriteAttribute(
    const float* array,const char* attributeLabel,size_t size,float undefined) throw();
template void goblinExport::WriteAttribute(
    const unsigned long* array,const char* attributeLabel,size_t size,unsigned long undefined) throw();
template void goblinExport::WriteAttribute(
    const unsigned short* array,const char* attributeLabel,size_t size,unsigned short undefined) throw();
template void goblinExport::WriteAttribute(
    const int* array,const char* attributeLabel,size_t size,int undefined) throw();
template void goblinExport::WriteAttribute(
    const bool* array,const char* attributeLabel,size_t size,bool undefined) throw();
template void goblinExport::WriteAttribute(
    const char* array,const char* attributeLabel,size_t size,char undefined) throw();
template void goblinExport::WriteAttribute(
    const TString* array,const char* attributeLabel,size_t size,char* undefined) throw();


void goblinExport::WriteConfiguration(const goblinController &CT1,TConfig tp) throw()
{
    goblinController& CT2 = goblinDefaultContext;

    StartTuple("configure",2);

    if (CT1.sourceNodeInFile!=CT2.sourceNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -sourceNode         " << CT1.sourceNodeInFile;

    if (CT1.targetNodeInFile!=CT2.targetNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -targetNode         " << CT1.targetNodeInFile;

    if (CT1.rootNodeInFile!=CT2.rootNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -rootNode           " << CT1.rootNodeInFile;


    if (CT1.displayMode!=CT2.displayMode || tp==CONF_FULL)
        expFile << endl << "   -displayMode        " << CT1.displayMode;

    if (CT1.displayZoom!=CT2.displayZoom || tp==CONF_DIFF)
        expFile << endl << "   -displayZoom        " << CT1.displayZoom;

    if (CT1.pixelWidth!=CT2.pixelWidth || tp==CONF_DIFF)
        expFile << endl << "   -pixelWidth         " << CT1.pixelWidth;

    if (CT1.pixelHeight!=CT2.pixelHeight || tp==CONF_DIFF)
        expFile << endl << "   -pixelHeight        " << CT1.pixelHeight;

    if (CT1.legenda!=CT2.legenda || tp==CONF_DIFF)
        expFile << endl << "   -legenda            " << CT1.legenda;

    if (strcmp(CT1.wallpaper,CT2.wallpaper)!=0 || tp==CONF_FULL)
    {
        expFile << endl << "   -wallpaper          "
            << "\"" << CT1.wallpaper << "\"";
    }


    if (CT1.traceLevel!=CT2.traceLevel || tp==CONF_FULL)
        expFile << endl << "   -traceLevel         " << CT1.traceLevel;

    if (CT1.traceData!=CT2.traceData || tp==CONF_FULL)
        expFile << endl << "   -traceData          " << CT1.traceData;

    if (CT1.traceStep!=CT2.traceStep || tp==CONF_FULL)
        expFile << endl << "   -traceStep          " << CT1.traceStep;

    if (CT1.threshold!=CT2.threshold || tp==CONF_FULL)
        expFile << endl << "   -threshold          " << CT1.threshold;


    if (CT1.logMeth!=CT2.logMeth || tp==CONF_FULL)
        expFile << endl << "   -logMeth            " << CT1.logMeth;

    if (CT1.logMem!=CT2.logMem || tp==CONF_FULL)
        expFile << endl << "   -logMem             " << CT1.logMem;

    if (CT1.logMan!=CT2.logMan || tp==CONF_FULL)
        expFile << endl << "   -logMan             " << CT1.logMan;

    if (CT1.logIO!=CT2.logIO || tp==CONF_FULL)
        expFile << endl << "   -logIO              " << CT1.logIO;

    if (CT1.logRes!=CT2.logRes || tp==CONF_FULL)
        expFile << endl << "   -logRes             " << CT1.logRes;

    if (CT1.logWarn!=CT2.logWarn || tp==CONF_FULL)
        expFile << endl << "   -logWarn            " << CT1.logWarn;

    if (CT1.logTimers!=CT2.logTimers || tp==CONF_FULL)
        expFile << endl << "   -logTimers          " << CT1.logTimers;

    if (CT1.logGaps!=CT2.logGaps || tp==CONF_FULL)
        expFile << endl << "   -logGaps            " << CT1.logGaps;


    if (CT1.methFailSave!=CT2.methFailSave || tp==CONF_FULL)
        expFile << endl << "   -methFailSave       " << CT1.methFailSave;

    if (CT1.methDSU!=CT2.methDSU || tp==CONF_FULL)
        expFile << endl << "   -methDSU            " << CT1.methDSU;

    if (CT1.methPQ!=CT2.methPQ || tp==CONF_FULL)
        expFile << endl << "   -methPQ             " << CT1.methPQ;

    if (CT1.methModLength!=CT2.methModLength || tp==CONF_FULL)
        expFile << endl << "   -methModLength      " << CT1.methModLength;


    if (CT1.methSPX!=CT2.methSPX || tp==CONF_FULL)
        expFile << endl << "   -methSPX            " << CT1.methSPX;

    if (CT1.methMST!=CT2.methMST || tp==CONF_FULL)
        expFile << endl << "   -methMST            " << CT1.methMST;

    if (CT1.methMXF!=CT2.methMXF || tp==CONF_FULL)
        expFile << endl << "   -methMXF            " << CT1.methMXF;

    if (CT1.methMCFST!=CT2.methMCFST || tp==CONF_FULL)
        expFile << endl << "   -methMCFST          " << CT1.methMCFST;

    if (CT1.methMCF!=CT2.methMCF || tp==CONF_FULL)
        expFile << endl << "   -methMCF            " << CT1.methMCF;

    if (CT1.methNWPricing!=CT2.methNWPricing || tp==CONF_FULL)
        expFile << endl << "   -methNWPricing      " << CT1.methNWPricing;

    if (CT1.methMCC!=CT2.methMCC || tp==CONF_FULL)
        expFile << endl << "   -methMCC            " << CT1.methMCC;

    if (CT1.methMaxBalFlow!=CT2.methMaxBalFlow || tp==CONF_FULL)
        expFile << endl << "   -methMaxBalFlow     " << CT1.methMaxBalFlow;

    if (CT1.methBNS!=CT2.methBNS || tp==CONF_FULL)
        expFile << endl << "   -methBNS            " << CT1.methBNS;

    if (CT1.methMinCBalFlow!=CT2.methMinCBalFlow || tp==CONF_FULL)
        expFile << endl << "   -methMinCBalFlow    " << CT1.methMinCBalFlow;

    if (CT1.methPrimalDual!=CT2.methPrimalDual || tp==CONF_FULL)
        expFile << endl << "   -methPrimalDual     " << CT1.methPrimalDual;

    if (CT1.methCandidates!=CT2.methCandidates || tp==CONF_FULL)
        expFile << endl << "   -methCandidates     " << CT1.methCandidates;

    if (CT1.methColour!=CT2.methColour || tp==CONF_FULL)
        expFile << endl << "   -methColour         " << CT1.methColour;

    if (CT1.methHeurTSP!=CT2.methHeurTSP || tp==CONF_FULL)
        expFile << endl << "   -methHeurTSP        " << CT1.methHeurTSP;

    if (CT1.methRelaxTSP1!=CT2.methRelaxTSP1 || tp==CONF_FULL)
        expFile << endl << "   -methRelaxTSP1      " << CT1.methRelaxTSP1;

    if (CT1.methRelaxTSP2!=CT2.methRelaxTSP2 || tp==CONF_FULL)
        expFile << endl << "   -methRelaxTSP2      " << CT1.methRelaxTSP2;

    if (CT1.methMaxCut!=CT2.methMaxCut || tp==CONF_FULL)
        expFile << endl << "   -methMaxCut         " << CT1.methMaxCut;


    if (CT1.methLP!=CT2.methLP || tp==CONF_FULL)
        expFile << endl << "   -methLP             " << CT1.methLP;

    if (CT1.methLPPricing!=CT2.methLPPricing || tp==CONF_FULL)
        expFile << endl << "   -methLPPricing      " << CT1.methLPPricing;

    if (CT1.methLPQTest!=CT2.methLPQTest || tp==CONF_FULL)
        expFile << endl << "   -methLPQTest        " << CT1.methLPQTest;

    if (CT1.methLPStart!=CT2.methLPStart || tp==CONF_FULL)
        expFile << endl << "   -methLPStart        " << CT1.methLPStart;


    if (CT1.methSolve!=CT2.methSolve || tp==CONF_FULL)
        expFile << endl << "   -methSolve          " << CT1.methSolve;

    if (CT1.methLocal!=CT2.methLocal || tp==CONF_FULL)
        expFile << endl << "   -methLocal          " << CT1.methLocal;

    if (CT1.maxBBIterations!=CT2.maxBBIterations || tp==CONF_FULL)
        expFile << endl << "   -maxBBIterations    " << CT1.maxBBIterations;

    if (CT1.maxBBNodes!=CT2.maxBBNodes || tp==CONF_FULL)
        expFile << endl << "   -maxBBNodes         " << CT1.maxBBNodes;


    if (CT1.methFDP!=CT2.methFDP || tp==CONF_FULL)
        expFile << endl << "   -methFDP            " << CT1.methFDP;

    if (CT1.methPlanarity!=CT2.methPlanarity || tp==CONF_FULL)
        expFile << endl << "   -methPlanarity      " << CT1.methPlanarity;

    if (CT1.methOrthogonal!=CT2.methOrthogonal || tp==CONF_FULL)
        expFile << endl << "   -methOrthogonal     " << CT1.methOrthogonal;

    if (CT1.methOrthoRefine!=CT2.methOrthoRefine || tp==CONF_FULL)
        expFile << endl << "   -methOrthoRefine    " << CT1.methOrthoRefine;


    if (CT1.randMin!=CT2.randMin || tp==CONF_FULL)
        expFile << endl << "   -randMin            " << CT1.randMin;

    if (CT1.randMax!=CT2.randMax || tp==CONF_FULL)
        expFile << endl << "   -randMax            " << CT1.randMax;

    if (CT1.randUCap!=CT2.randUCap || tp==CONF_FULL)
        expFile << endl << "   -randUCap           " << CT1.randUCap;

    if (CT1.randLCap!=CT2.randLCap || tp==CONF_FULL)
        expFile << endl << "   -randLCap           " << CT1.randLCap;

    if (CT1.randLength!=CT2.randLength || tp==CONF_FULL)
        expFile << endl << "   -randLength         " << CT1.randLength;

    if (CT1.randParallels!=CT2.randParallels || tp==CONF_FULL)
        expFile << endl << "   -randParallels      " << CT1.randParallels;

    if (CT1.randGeometry!=CT2.randGeometry || tp==CONF_FULL)
        expFile << endl << "   -randGeometry       " << CT1.randGeometry;

    EndTuple();
}


goblinExport::~goblinExport() throw()
{
    expFile << endl;
    expFile.close();

    if (currentLevel > 0)
        CT.Error(ERR_INTERNAL,NoHandle,"goblinExport","Some lists are open");
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileExport.cpp
/// \brief  #goblinExport class implementation

#include "fileExport.h"


goblinExport::goblinExport(const char* expFileName,goblinController &thisContext)
    throw(ERFile) : expFile(expFileName, ios::out), CT(thisContext)
{
    if (!expFile)
    {
        sprintf(CT.logBuffer,"Could not open export file %s, io_state %d",
            expFileName,expFile.rdstate());
        CT.Error(ERR_FILE,NoHandle,"goblinExport",CT.logBuffer);
    }

    expFile.flags(expFile.flags() | ios::right);
    expFile.setf(ios::scientific,ios::floatfield);
    expFile.precision(CT.externalPrecision-1);

    currentLevel = 0;
    currentType = 0;
}


void goblinExport::StartTuple(const char* header, char type) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentType != 0)
        CT.Error(ERR_REJECTED,NoHandle,"StartTuple","Illegal operation");

    #endif

    if (currentLevel>0) expFile << endl;

    currentLevel ++;
    currentPos = type;
    currentType = type;
    expFile.width(currentLevel);
    expFile << "(" << header;
}


void goblinExport::StartTuple(unsigned long k, char type) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentType != 0)
        CT.Error(ERR_REJECTED,NoHandle,"StartTuple","Illegal operation");

    #endif

    currentLevel ++;
    currentPos = type;   
    currentType = type;

    expFile << endl;
    expFile.width(currentLevel);
    expFile << "(" << k;
}


void goblinExport::EndTuple() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (currentLevel == 0)
        CT.Error(ERR_REJECTED,NoHandle,"EndTuple","Exceeding minimum depth");

    #endif

    if (currentType == 0)
    {
        expFile << endl;
        expFile.width(currentLevel);
    }

    expFile << ")";

    currentType = 0;
    currentLevel --;
}


template <> void goblinExport::MakeItem(const char* item,int length) throw()
{
    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(currentLevel+1);
        expFile << "";
    }
    else
    {
        currentPos ++;
        expFile << " ";
    }

    expFile.width(length);
    expFile << "\"" << item << "\"";
}


static char itemBuffer[25];

template <> void goblinExport::MakeItem(double item,int length) throw()
{
    if (fabs(item)==InfFloat)
    {
        MakeNoItem(length);
        return;
    }

    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(currentLevel+1);
        expFile << "";
    }
    else
    {
        currentPos ++;
        expFile << " ";
    }

    sprintf(itemBuffer,"%*.*f",length,CT.externalPrecision,double(item));
    expFile.width(length);
    expFile << itemBuffer;
}


template <> void goblinExport::MakeItem(float item,int length) throw()
{
    MakeItem(static_cast<double>(item),length);
}


template <typename T> void goblinExport::MakeItem(T item,int length) throw()
{
    if (currentType!=1 && currentType==currentPos)
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(length+currentLevel+1);
    }
    else
    {
        currentPos ++;
        expFile << " ";
        expFile.width(length);
    }

    expFile << item;
}


template void goblinExport::MakeItem(int item,int length) throw();
template void goblinExport::MakeItem(unsigned item,int length) throw();
template void goblinExport::MakeItem(long item,int length) throw();
template void goblinExport::MakeItem(unsigned long item,int length) throw();
template void goblinExport::MakeItem(char item,int length) throw();
template void goblinExport::MakeItem(unsigned char item,int length) throw();
template void goblinExport::MakeItem(short item,int length) throw();
template void goblinExport::MakeItem(unsigned short item,int length) throw();
template void goblinExport::MakeItem(TOptLayoutTokens item,int length) throw();


void goblinExport::MakeNoItem(int length) throw()
{
    if ((currentType != 1)&&(currentType==currentPos))
    {
        currentPos = 1;
        expFile << endl;
        expFile.width(length+currentLevel+1);
    }
    else
    {
        currentPos ++;
        expFile << " ";
        expFile.width(length);
    }

    expFile << "*";
}


template <typename T>
void goblinExport::WriteAttribute(const T* array,const char* attributeLabel,size_t size,T undefined) throw()
{
    if (size==1)
    {
        StartTuple(attributeLabel,1);

        if  (array[0]==undefined)
        {
            MakeNoItem(0);
        }
        else
        {
            MakeItem(array[0],0);
        }
    }
    else
    {
        StartTuple(attributeLabel,10);

        int length = 1;

        for (size_t i=0;i<size;i++)
        {
            char thisLength = CT.template ExternalLength<T>(array[i]);

            if (array[i]!=undefined && thisLength>length) length = thisLength;
        }

        for (size_t i=0;i<size;i++)
        {
            if (array[i]!=undefined)
            {
                MakeItem(array[i],length);
            }
            else
            {
                MakeNoItem(length);
            }
        }
    }

    EndTuple();
}

template void goblinExport::WriteAttribute(
    const double* array,const char* attributeLabel,size_t size,double undefined) throw();
template void goblinExport::WriteAttribute(
    const float* array,const char* attributeLabel,size_t size,float undefined) throw();
template void goblinExport::WriteAttribute(
    const unsigned long* array,const char* attributeLabel,size_t size,unsigned long undefined) throw();
template void goblinExport::WriteAttribute(
    const unsigned short* array,const char* attributeLabel,size_t size,unsigned short undefined) throw();
template void goblinExport::WriteAttribute(
    const int* array,const char* attributeLabel,size_t size,int undefined) throw();
template void goblinExport::WriteAttribute(
    const bool* array,const char* attributeLabel,size_t size,bool undefined) throw();
template void goblinExport::WriteAttribute(
    const char* array,const char* attributeLabel,size_t size,char undefined) throw();
template void goblinExport::WriteAttribute(
    const TString* array,const char* attributeLabel,size_t size,char* undefined) throw();


void goblinExport::WriteConfiguration(const goblinController &CT1,TConfig tp) throw()
{
    goblinController& CT2 = goblinDefaultContext;

    StartTuple("configure",2);

    if (CT1.sourceNodeInFile!=CT2.sourceNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -sourceNode         " << CT1.sourceNodeInFile;

    if (CT1.targetNodeInFile!=CT2.targetNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -targetNode         " << CT1.targetNodeInFile;

    if (CT1.rootNodeInFile!=CT2.rootNodeInFile || tp==CONF_DIFF)
        expFile << endl << "   -rootNode           " << CT1.rootNodeInFile;


    if (CT1.displayMode!=CT2.displayMode || tp==CONF_FULL)
        expFile << endl << "   -displayMode        " << CT1.displayMode;

    if (CT1.displayZoom!=CT2.displayZoom || tp==CONF_DIFF)
        expFile << endl << "   -displayZoom        " << CT1.displayZoom;

    if (CT1.pixelWidth!=CT2.pixelWidth || tp==CONF_DIFF)
        expFile << endl << "   -pixelWidth         " << CT1.pixelWidth;

    if (CT1.pixelHeight!=CT2.pixelHeight || tp==CONF_DIFF)
        expFile << endl << "   -pixelHeight        " << CT1.pixelHeight;

    if (CT1.legenda!=CT2.legenda || tp==CONF_DIFF)
        expFile << endl << "   -legenda            " << CT1.legenda;

    if (strcmp(CT1.wallpaper,CT2.wallpaper)!=0 || tp==CONF_FULL)
    {
        expFile << endl << "   -wallpaper          "
            << "\"" << CT1.wallpaper << "\"";
    }


    if (CT1.traceLevel!=CT2.traceLevel || tp==CONF_FULL)
        expFile << endl << "   -traceLevel         " << CT1.traceLevel;

    if (CT1.traceData!=CT2.traceData || tp==CONF_FULL)
        expFile << endl << "   -traceData          " << CT1.traceData;

    if (CT1.traceStep!=CT2.traceStep || tp==CONF_FULL)
        expFile << endl << "   -traceStep          " << CT1.traceStep;

    if (CT1.threshold!=CT2.threshold || tp==CONF_FULL)
        expFile << endl << "   -threshold          " << CT1.threshold;


    if (CT1.logMeth!=CT2.logMeth || tp==CONF_FULL)
        expFile << endl << "   -logMeth            " << CT1.logMeth;

    if (CT1.logMem!=CT2.logMem || tp==CONF_FULL)
        expFile << endl << "   -logMem             " << CT1.logMem;

    if (CT1.logMan!=CT2.logMan || tp==CONF_FULL)
        expFile << endl << "   -logMan             " << CT1.logMan;

    if (CT1.logIO!=CT2.logIO || tp==CONF_FULL)
        expFile << endl << "   -logIO              " << CT1.logIO;

    if (CT1.logRes!=CT2.logRes || tp==CONF_FULL)
        expFile << endl << "   -logRes             " << CT1.logRes;

    if (CT1.logWarn!=CT2.logWarn || tp==CONF_FULL)
        expFile << endl << "   -logWarn            " << CT1.logWarn;

    if (CT1.logTimers!=CT2.logTimers || tp==CONF_FULL)
        expFile << endl << "   -logTimers          " << CT1.logTimers;

    if (CT1.logGaps!=CT2.logGaps || tp==CONF_FULL)
        expFile << endl << "   -logGaps            " << CT1.logGaps;


    if (CT1.methFailSave!=CT2.methFailSave || tp==CONF_FULL)
        expFile << endl << "   -methFailSave       " << CT1.methFailSave;

    if (CT1.methDSU!=CT2.methDSU || tp==CONF_FULL)
        expFile << endl << "   -methDSU            " << CT1.methDSU;

    if (CT1.methPQ!=CT2.methPQ || tp==CONF_FULL)
        expFile << endl << "   -methPQ             " << CT1.methPQ;

    if (CT1.methModLength!=CT2.methModLength || tp==CONF_FULL)
        expFile << endl << "   -methModLength      " << CT1.methModLength;


    if (CT1.methSPX!=CT2.methSPX || tp==CONF_FULL)
        expFile << endl << "   -methSPX            " << CT1.methSPX;

    if (CT1.methMST!=CT2.methMST || tp==CONF_FULL)
        expFile << endl << "   -methMST            " << CT1.methMST;

    if (CT1.methMXF!=CT2.methMXF || tp==CONF_FULL)
        expFile << endl << "   -methMXF            " << CT1.methMXF;

    if (CT1.methMCFST!=CT2.methMCFST || tp==CONF_FULL)
        expFile << endl << "   -methMCFST          " << CT1.methMCFST;

    if (CT1.methMCF!=CT2.methMCF || tp==CONF_FULL)
        expFile << endl << "   -methMCF            " << CT1.methMCF;

    if (CT1.methNWPricing!=CT2.methNWPricing || tp==CONF_FULL)
        expFile << endl << "   -methNWPricing      " << CT1.methNWPricing;

    if (CT1.methMCC!=CT2.methMCC || tp==CONF_FULL)
        expFile << endl << "   -methMCC            " << CT1.methMCC;

    if (CT1.methMaxBalFlow!=CT2.methMaxBalFlow || tp==CONF_FULL)
        expFile << endl << "   -methMaxBalFlow     " << CT1.methMaxBalFlow;

    if (CT1.methBNS!=CT2.methBNS || tp==CONF_FULL)
        expFile << endl << "   -methBNS            " << CT1.methBNS;

    if (CT1.methMinCBalFlow!=CT2.methMinCBalFlow || tp==CONF_FULL)
        expFile << endl << "   -methMinCBalFlow    " << CT1.methMinCBalFlow;

    if (CT1.methPrimalDual!=CT2.methPrimalDual || tp==CONF_FULL)
        expFile << endl << "   -methPrimalDual     " << CT1.methPrimalDual;

    if (CT1.methCandidates!=CT2.methCandidates || tp==CONF_FULL)
        expFile << endl << "   -methCandidates     " << CT1.methCandidates;

    if (CT1.methColour!=CT2.methColour || tp==CONF_FULL)
        expFile << endl << "   -methColour         " << CT1.methColour;

    if (CT1.methHeurTSP!=CT2.methHeurTSP || tp==CONF_FULL)
        expFile << endl << "   -methHeurTSP        " << CT1.methHeurTSP;

    if (CT1.methRelaxTSP1!=CT2.methRelaxTSP1 || tp==CONF_FULL)
        expFile << endl << "   -methRelaxTSP1      " << CT1.methRelaxTSP1;

    if (CT1.methRelaxTSP2!=CT2.methRelaxTSP2 || tp==CONF_FULL)
        expFile << endl << "   -methRelaxTSP2      " << CT1.methRelaxTSP2;

    if (CT1.methMaxCut!=CT2.methMaxCut || tp==CONF_FULL)
        expFile << endl << "   -methMaxCut         " << CT1.methMaxCut;


    if (CT1.methLP!=CT2.methLP || tp==CONF_FULL)
        expFile << endl << "   -methLP             " << CT1.methLP;

    if (CT1.methLPPricing!=CT2.methLPPricing || tp==CONF_FULL)
        expFile << endl << "   -methLPPricing      " << CT1.methLPPricing;

    if (CT1.methLPQTest!=CT2.methLPQTest || tp==CONF_FULL)
        expFile << endl << "   -methLPQTest        " << CT1.methLPQTest;

    if (CT1.methLPStart!=CT2.methLPStart || tp==CONF_FULL)
        expFile << endl << "   -methLPStart        " << CT1.methLPStart;


    if (CT1.methSolve!=CT2.methSolve || tp==CONF_FULL)
        expFile << endl << "   -methSolve          " << CT1.methSolve;

    if (CT1.methLocal!=CT2.methLocal || tp==CONF_FULL)
        expFile << endl << "   -methLocal          " << CT1.methLocal;

    if (CT1.maxBBIterations!=CT2.maxBBIterations || tp==CONF_FULL)
        expFile << endl << "   -maxBBIterations    " << CT1.maxBBIterations;

    if (CT1.maxBBNodes!=CT2.maxBBNodes || tp==CONF_FULL)
        expFile << endl << "   -maxBBNodes         " << CT1.maxBBNodes;


    if (CT1.methFDP!=CT2.methFDP || tp==CONF_FULL)
        expFile << endl << "   -methFDP            " << CT1.methFDP;

    if (CT1.methPlanarity!=CT2.methPlanarity || tp==CONF_FULL)
        expFile << endl << "   -methPlanarity      " << CT1.methPlanarity;

    if (CT1.methOrthogonal!=CT2.methOrthogonal || tp==CONF_FULL)
        expFile << endl << "   -methOrthogonal     " << CT1.methOrthogonal;

    if (CT1.methOrthoRefine!=CT2.methOrthoRefine || tp==CONF_FULL)
        expFile << endl << "   -methOrthoRefine    " << CT1.methOrthoRefine;


    if (CT1.randMin!=CT2.randMin || tp==CONF_FULL)
        expFile << endl << "   -randMin            " << CT1.randMin;

    if (CT1.randMax!=CT2.randMax || tp==CONF_FULL)
        expFile << endl << "   -randMax            " << CT1.randMax;

    if (CT1.randUCap!=CT2.randUCap || tp==CONF_FULL)
        expFile << endl << "   -randUCap           " << CT1.randUCap;

    if (CT1.randLCap!=CT2.randLCap || tp==CONF_FULL)
        expFile << endl << "   -randLCap           " << CT1.randLCap;

    if (CT1.randLength!=CT2.randLength || tp==CONF_FULL)
        expFile << endl << "   -randLength         " << CT1.randLength;

    if (CT1.randParallels!=CT2.randParallels || tp==CONF_FULL)
        expFile << endl << "   -randParallels      " << CT1.randParallels;

    if (CT1.randGeometry!=CT2.randGeometry || tp==CONF_FULL)
        expFile << endl << "   -randGeometry       " << CT1.randGeometry;

    EndTuple();
}


goblinExport::~goblinExport() throw()
{
    expFile << endl;
    expFile.close();

    if (currentLevel > 0)
        CT.Error(ERR_INTERNAL,NoHandle,"goblinExport","Some lists are open");
}
