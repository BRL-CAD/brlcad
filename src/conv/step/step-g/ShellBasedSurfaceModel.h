#ifndef CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H
#define CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H

#include "GeometricRepresentationItem.h"

// forward declaration of class
class OpenShell;
class STEPWrapper;
class ON_Brep;

typedef std::list<OpenShell *> LIST_OF_OPEN_SHELLS;

class ShellBasedSurfaceModel: public GeometricRepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_OPEN_SHELLS sbsm_boundary;

public:
    ShellBasedSurfaceModel();
    ShellBasedSurfaceModel(STEPWrapper *sw, int step_id);
    virtual ~ShellBasedSurfaceModel();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    ON_Brep *GetONBrep();
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
