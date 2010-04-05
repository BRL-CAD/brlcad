
//provisorische Implementierung zum Schreiben von abstractSubgraph in Datei

#include "abstractSubgraph.h"


sparseGraph *abstractSubgraph::CreateGraphObject() const throw()
{
    sparseGraph *G = new sparseGraph(this->SourceGraph()->N(),CT);
    graphRepresentation* GR = G->Representation();

    for (TArc a=0; a<SourceGraph()->M()*2; a++)
    {
        if (this->HasArc(a))
            G->InsertArc(StartNode(a),EndNode(a));
    }

    for (TDim i=0;i<SourceGraph()->Dim();i++)
    {
        for (TNode v=0;v<SourceGraph()->N(); v++)
        {
            GR -> SetC(v,i,SourceGraph()->C(v,i));
        }
    }

    return G;
}


void abstractSubgraph::Write(const char* fileName) const throw(ERFile)
{
    abstractMixedGraph* G = CreateGraphObject();
    G -> Write(fileName);
    delete G;
}


TArc abstractSubgraph::Reverse(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)
    //if (a>=2*m) NoSuchArc("Reverse",a);
    #endif

    return a^1;
}


unsigned long abstractSubgraph::Allocated() const throw()
{
    return 0;
}
