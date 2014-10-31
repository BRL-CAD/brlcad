#ifndef CADAPP_H
#define CADAPP_H

#include <QApplication>
#include <QMap>
#include <QSet>

#include "qdbi.h"
#include "ged.h"

class CADApp : public QApplication
{
    Q_OBJECT

    public:
	CADApp(int &argc, char *argv[]) :QApplication(argc, argv) {};
	~CADApp() {};

	int register_command(QString cmdname, ged_func_ptr func, int db_changer = 0, int view_changer = 0);
	int exec_command(QDBI *dbi, QString *command, QString *result);

    public:
	QMap<QString, ged_func_ptr> cmd_map;
	QSet<QString> edit_cmds;  // Commands that potentially change the database contents */
	QSet<QString> view_cmds;  // Commands that potentially change the view, but not the database contents */

};

#endif // CADAPP_H

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

