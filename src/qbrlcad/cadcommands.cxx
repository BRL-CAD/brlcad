#include "cadcommands.h"
#include "cadhelp.h"

void cad_register_commands(CADApp *app)
{
    // Geometry Editing (libged) commands
    app->register_command(QString("ls"), ged_ls);
    app->register_command(QString("tops"), ged_tops);
    app->register_command(QString("search"), ged_search);

    // GUI commands
    app->register_gui_command(QString("man"), cad_man_view);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

