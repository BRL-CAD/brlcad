
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   libImport.cpp
/// \brief  A collection of file import filters for graph objects

#include <stdio.h>
#include "mixedGraph.h"
#include "sparseBigraph.h"
#include "sparseDigraph.h"
#include "sparseGraph.h"
#include "balancedDigraph.h"
#include "denseBigraph.h"
#include "denseDigraph.h"
#include "denseGraph.h"
#include "ilpWrapper.h"

managedObject* goblinController::ImportByFormatName(const char* filename,const char* formatName)
    throw(ERParse)
{
    struct TImportFormatTable {
        char*        formatName;
        TFileFormat  formatToken;
    };

    const unsigned numInportFormats = 8;
    const TImportFormatTable listOfImportFormats[numInportFormats] =
    {
        { "goblin",             FMT_GOBLIN              },
        { "dimacsMin",          FMT_DIMACS_MIN          },
        { "dimacsEdge",         FMT_DIMACS_EDGE         },
        { "dimacsGeom",         FMT_DIMACS_GEOM         },
        { "squareUCap",         FMT_SQUARE_UCAP         },
        { "squareLength",       FMT_SQUARE_LENGTH       },
        { "triangularUCap",     FMT_TRIANGULAR_UCAP     },
        { "triangularLength",   FMT_TRIANGULAR_LENGTH   }
    };

    for (unsigned i=0;i<numInportFormats;++i)
    {
        if (strcmp(formatName,listOfImportFormats[i].formatName)==0)
        {
            return ImportFromFile(filename,listOfImportFormats[i].formatToken);
        }
    }

    return NULL;
}


managedObject* goblinController::ImportFromFile(const char* filename, TFileFormat format)
    throw(ERParse)
{
    switch (format)
    {
        case FMT_GOBLIN:
        {
            return Import_Native(filename);
        }
        case FMT_DIMACS_MIN:
        {
            return Import_DimacsMin(filename);
        }
        case FMT_DIMACS_EDGE:
        {
            return Import_DimacsEdge(filename);
        }
        case FMT_DIMACS_GEOM:
        {
            return Import_DimacsGeom(filename);
        }
        case FMT_SQUARE_UCAP:
        case FMT_SQUARE_LENGTH:
        {
            return Import_SquareMatrix(filename,format);
        }
        case FMT_TRIANGULAR_UCAP:
        case FMT_TRIANGULAR_LENGTH:
        {
            return Import_TriangularMatrix(filename,format);
        }
        default:
        {
        }
    }

    return NULL;
}


managedObject* goblinController::Import_Native(const char* filename)
    throw(ERParse)
{
    try
    {
        goblinImport F(filename,*this);
        char* type = F.Scan();
        F.DontComplain();

        if (strcmp(type,"dense_bigraph")==0)
        {
            return new denseBiGraph(filename,*this);
        }
        else if (strcmp(type,"bigraph")==0)
        {
            return new sparseBiGraph(filename,*this);
        }
        else if (strcmp(type,"dense_graph")==0)
        {
            return new denseGraph(filename,*this);
        }
        else if (strcmp(type,"graph")==0)
        {
            return new sparseGraph(filename,*this);
        }
        else if (strcmp(type,"dense_digraph")==0)
        {
            return new denseDiGraph(filename,*this);
        }
        else if (strcmp(type,"digraph")==0)
        {
            return new sparseDiGraph(filename,*this);
        }
        else if (strcmp(type,"mixed")==0)
        {
            return new mixedGraph(filename,*this);
        }
        else if (strcmp(type,"balanced_fnw")==0)
        {
            return new balancedFNW(filename,*this);
        }
        else if (strcmp(type,"mixed_integer")==0 && pMipFactory)
        {
            mipFactory* theMipFactory =
                reinterpret_cast<mipFactory*>(const_cast<char*>(pMipFactory));
            return theMipFactory->ReadInstance(filename,*this);
        }
    }
    catch (...) {}

    return NULL;
}

sparseDiGraph* goblinController::Import_DimacsMin(const char* filename)
    throw(ERParse)
{
    sparseDiGraph* G = NULL;
    sparseRepresentation* GR = NULL;
    const unsigned LINE_BUFFER_LENGTH = 128;
    char lineBuffer[LINE_BUFFER_LENGTH];
    int nFound = 0;
    unsigned long n = 0;
    unsigned long m = 0;

    FILE* inputFileDescriptor = fopen(filename,"r");

    while (fgets(lineBuffer,LINE_BUFFER_LENGTH,inputFileDescriptor))
    {
        if (n==0)
        {
            nFound = sscanf(lineBuffer,"p min %lu %lu",&n,&m);

            if (nFound>0)
            {
                if (n==0)
                {
                    fclose(inputFileDescriptor);
                    Error(ERR_PARSE,NoHandle,"Import_DimacsMin",
                        "Insufficient problem dimensions");
                }

                randGeometry = 0;
                G = new sparseDiGraph(TNode(n),*this);
                GR = static_cast<sparseRepresentation*>(G->Representation());
                GR -> SetCapacity(TNode(n),TArc(m));
                GR -> SetCDemand(0);
                GR -> SetCUCap(1);
                GR -> SetCLCap(0);
                GR -> SetCLength(1);
            }

            continue;
        }

        unsigned long u = 0;
        unsigned long v = 0;
        double lcap = 0.0;
        double ucap = 1.0;
        double length = 1.0;
        nFound = sscanf(lineBuffer,"a %lu %lu %lf %lf %lf",&u,&v,&lcap,&ucap,&length);

        if (nFound>0)
        {
            if (nFound<2)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsMin",
                    "Missing target node index");
            }

            if (u>n || u<1 || v>n || v<1)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsMin",
                    "Node index exeeds problem dimension");
            }

            G -> InsertArc(TNode(u-1),TNode(v-1),TCap(ucap),TFloat(length),TCap(lcap));

            continue;
        }

        double demand = 0.0;
        nFound = sscanf(lineBuffer,"n %lu %lf",&v,&demand);

        if (nFound>0)
        {
            if (nFound<2)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsMin",
                    "Missing demand value");
            }

            if (v>n || v<1)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsMin",
                    "Node index exeeds problem dimension");
            }

            GR -> SetDemand(TNode(v-1),TCap(-demand));
        }
    }

    fclose(inputFileDescriptor);

    if (n==0)
    {
        delete G;
        Error(ERR_PARSE,NoHandle,"Import_DimacsMin","Missing problem line");
    }

    if (m!=G->M())
    {
        Error(MSG_WARN,NoHandle,"Import_DimacsMin",
            "Actual number of arcs does not match the problem dimensions");
    }

    return G;
}


sparseGraph* goblinController::Import_DimacsEdge(const char* filename)
    throw(ERParse)
{
    sparseGraph* G = NULL;
    sparseRepresentation* GR = NULL;
    const unsigned LINE_BUFFER_LENGTH = 128;
    char lineBuffer[LINE_BUFFER_LENGTH];
    int nFound = 0;
    unsigned long n = 0;
    unsigned long m = 0;

    FILE* inputFileDescriptor = fopen(filename,"r");

    while (fgets(lineBuffer,LINE_BUFFER_LENGTH,inputFileDescriptor))
    {
        if (n==0)
        {
            nFound = sscanf(lineBuffer,"p edge %lu %lu",&n,&m);

            if (nFound>0)
            {
                if (n==0)
                {
                    fclose(inputFileDescriptor);
                    Error(ERR_PARSE,NoHandle,"Import_DimacsEdge",
                        "Insufficient problem dimensions");
                }

                randGeometry = 0;
                G = new sparseGraph(TNode(n),*this);
                GR = static_cast<sparseRepresentation*>(G->Representation());
                GR -> SetCapacity(TNode(n),TArc(m));
                GR -> SetCDemand(1);
                GR -> SetCUCap(1);
                GR -> SetCLCap(0);
                GR -> SetCLength(1);
            }

            continue;
        }

        unsigned long u = 0;
        unsigned long v = 0;
        double lcap = 0.0;
        double ucap = 1.0;
        double length = 1.0;
        nFound = sscanf(lineBuffer,"e %lu %lu %lf %lf %lf",&u,&v,&length,&ucap,&lcap);

        if (nFound>0)
        {
            if (nFound<2)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsEdge",
                    "Missing target node index");
            }

            if (u>n || u<1 || v>n || v<1)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsEdge",
                    "Node index exeeds problem dimension");
            }

            G -> InsertArc(TNode(u-1),TNode(v-1),TCap(ucap),TFloat(length),TCap(lcap));

            continue;
        }

        double demand = 1.0;
        nFound = sscanf(lineBuffer,"n %lu %lf",&v,&demand);

        if (nFound>0)
        {
            if (nFound<2)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsEdge",
                    "Missing demand value");
            }

            if (v>n || v<1)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsEdge",
                    "Node index exeeds problem dimension");
            }

            GR -> SetDemand(TNode(v-1),TCap(-demand));
        }
    }

    fclose(inputFileDescriptor);

    if (n==0)
    {
        delete G;
        Error(ERR_PARSE,NoHandle,"Import_DimacsEdge","Missing problem line");
    }

    if (m!=G->M())
    {
        Error(MSG_WARN,NoHandle,"Import_DimacsEdge",
            "Actual number of arcs does not match the problem dimensions");
    }

    return G;
}


denseGraph* goblinController::Import_DimacsGeom(const char* filename)
    throw(ERParse)
{
    denseGraph* G = NULL;
    denseRepresentation* GR = NULL;
    const unsigned LINE_BUFFER_LENGTH = 128;
    char lineBuffer[LINE_BUFFER_LENGTH];
    int nFound = 0;
    int dim = 2;
    unsigned long n = 0;
    unsigned long nLines = 0;

    FILE* inputFileDescriptor = fopen(filename,"r");

    while (fgets(lineBuffer,LINE_BUFFER_LENGTH,inputFileDescriptor))
    {
        if (n==0)
        {
            nFound = sscanf(lineBuffer,"p geom %lu %d",&n,&dim);

            if (nFound>0)
            {
                if (n==0)
                {
                    fclose(inputFileDescriptor);
                    Error(ERR_PARSE,NoHandle,"Import_DimacsGeom",
                        "Insufficient problem dimensions");
                }

                if (dim<1 || dim>3)
                {
                    fclose(inputFileDescriptor);
                    Error(ERR_PARSE,NoHandle,"Import_DimacsGeom",
                        "Insupported geometric dimension");
                }

                randGeometry = 0;
                G = new denseGraph(TNode(n),TOption(0),*this);
                GR = static_cast<denseRepresentation*>(G->Representation());
                GR -> SetCDemand(1);
                GR -> SetCUCap(1);
                GR -> SetCLCap(0);
                GR -> SetCLength(1);
            }

            continue;
        }

        double pos[3];
        nFound = sscanf(lineBuffer,"v %lf %lf %lf",&pos[0],&pos[1],&pos[2]);

        if (nFound>0)
        {
            if (nFound!=dim)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsGeom",
                    "Missing demand value");
            }

            ++nLines;

            if (nLines>n)
            {
                fclose(inputFileDescriptor);
                delete G;
                Error(ERR_PARSE,NoHandle,"Import_DimacsGeom",
                    "Too many node definition lines");
            }

            for (TDim i=0;i<dim;++i)
                GR -> SetC(TNode(nLines-1),i,TFloat(pos[i]));
        }
    }

    fclose(inputFileDescriptor);

    if (n==0)
    {
        delete G;
        Error(ERR_PARSE,NoHandle,"Import_DimacsGeom","Missing problem line");
    }

    GR -> SetMetricType(abstractMixedGraph::METRIC_EUCLIDIAN);

    return G;
}


denseDiGraph* goblinController::Import_SquareMatrix(const char* filename,TFileFormat format)
    throw(ERParse)
{
    FILE* inputFileDescriptor = fopen(filename,"r");
    TArc countValues = 0;
    double doubleValue = 0.0;
    char tokenBuffer[256];

    // First pass: Determine the number of nodes
    do
    {
        if (   fscanf(inputFileDescriptor,"%lg", &doubleValue)==1
            || fscanf(inputFileDescriptor,"%s%lg", tokenBuffer, &doubleValue)==2)
        {
            ++countValues;
            continue;
        }
    }
    while (!feof(inputFileDescriptor));

    double nFile = floor(sqrt(countValues)+0.5);

    if (fabs(nFile*nFile-countValues)>0.5)
    {
        Error(ERR_PARSE,NoHandle,"Import_SquareMatrix","Not a square matrix");
    }

    // Second pass: Generate a graph object with the specified capacity bounds
    randGeometry = randLength = randUCap = 0;
    denseDiGraph* G = new denseDiGraph(TNode(nFile),TOption(0),*this);
    denseRepresentation* GR = static_cast<denseRepresentation*>(G->Representation());
    GR -> SetCDemand(1);
    GR -> SetCUCap(1);
    GR -> SetCLCap(0);
    GR -> SetCLength(1);

    rewind(inputFileDescriptor);

    for (TNode i=0;i<G->N();++i)
    {
        for (TNode j=0;j<G->N();++j)
        {
            while (   fscanf(inputFileDescriptor,"%lg", &doubleValue)==0
                   && fscanf(inputFileDescriptor,"%s%lg", tokenBuffer, &doubleValue)<2 ) {}

            if (format==FMT_SQUARE_UCAP)
            {
                GR -> SetUCap(G->Adjacency(i,j),TCap(doubleValue));
            }
            else // if (format==FMT_SQUARE_LENGTH)
            {
                GR -> SetLength(G->Adjacency(i,j),TFloat(doubleValue));
            }
        }
    }

    fclose(inputFileDescriptor);

    return G;
}


denseGraph* goblinController::Import_TriangularMatrix(const char* filename,TFileFormat format)
    throw(ERParse)
{
    FILE* inputFileDescriptor = fopen(filename,"r");
    TArc countValues = 0;
    double doubleValue = 0.0;
    char tokenBuffer[256];

    // First pass: Determine the number of nodes
    do
    {
        if (   fscanf(inputFileDescriptor,"%lg", &doubleValue)==1
            || fscanf(inputFileDescriptor,"%s%lg", tokenBuffer, &doubleValue)==2)
        {
            ++countValues;
            continue;
        }
    }
    while (!feof(inputFileDescriptor));

    // Apply the formula: nFile*(nFile+1)/2 = countValues
    // nFile = -0.5 + sqrt(2*countValues+0.25)
    double nFile = floor(sqrt(2*countValues+0.25));

    if (fabs(nFile*(nFile+1)/2.0-countValues)>0.5)
    {
        Error(ERR_PARSE,NoHandle,"Import_TriangularMatrix","Not a triangular matrix");
    }

    // Second pass: Generate a graph object with the specified capacity bounds
    randGeometry = randLength = randUCap = 0;
    denseGraph* G = new denseGraph(TNode(nFile),TOption(0),*this);
    denseRepresentation* GR = static_cast<denseRepresentation*>(G->Representation());
    GR -> SetCDemand(1);
    GR -> SetCUCap(1);
    GR -> SetCLCap(0);
    GR -> SetCLength(1);

    rewind(inputFileDescriptor);

    for (TNode i=0;i<G->N();++i)
    {
        for (TNode j=0;j<=i;++j)
        {
            while (   fscanf(inputFileDescriptor,"%lg", &doubleValue)==0
                   && fscanf(inputFileDescriptor,"%s%lg", tokenBuffer, &doubleValue)<2 ) {}

            if (format==FMT_TRIANGULAR_UCAP)
            {
                GR -> SetUCap(G->Adjacency(i,j),TCap(doubleValue));
            }
            else // if (format==FMT_TRIANGULAR_LENGTH)
            {
                GR -> SetLength(G->Adjacency(i,j),TFloat(doubleValue));
            }
        }
    }

    fclose(inputFileDescriptor);

    return G;
}
