#ifndef CONV_STEP_STEP_G_OPENSHELL_H
#define CONV_STEP_STEP_G_OPENSHELL_H

#include "ConnectedFaceSet.h"

class OpenShell : public ConnectedFaceSet
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:

public:
    OpenShell();
    virtual ~OpenShell();
    OpenShell(STEPWrapper *sw, int step_id);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);
    virtual void ReverseFaceSet();

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_OPENSHELL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
