#include "cadapp.h"
#include "qdbi.h"
#include "bu/malloc.h"

int
CADApp::register_command(QString cmdname, ged_func_ptr func, int db_changer, int view_changer)
{
    if (cmd_map.find(cmdname) != cmd_map.end()) return -1;
    cmd_map.insert(cmdname, func);
    if (db_changer) {
	edit_cmds.insert(cmdname);
	view_cmds.insert(cmdname);
    }
    if (view_changer && !db_changer) {
	view_cmds.insert(cmdname);
    }
    return 0;
}

int
CADApp::exec_command(QDBI *dbi, QString *command, QString *result)
{
    int ret = 0;
    if (dbi->gedp != GED_NULL && dbi && command && command->length() > 0) {
	char *lcmd = bu_strdup(command->toLocal8Bit());
	char **largv = (char **)bu_calloc(command->length()/2+1, sizeof(char *), "cmd_eval argv");
	int largc = bu_argv_from_string(largv, command->length()/2, lcmd);

	QMap<QString, ged_func_ptr>::iterator cmd_itr = cmd_map.find(QString(largv[0]));
	if (cmd_itr != cmd_map.end()) {
	    bu_vls_trunc(dbi->gedp->ged_result_str, 0);
	    ret = (*(cmd_itr.value()))(dbi->gedp, largc, (const char **)largv);
	    if (result && bu_vls_strlen(dbi->gedp->ged_result_str) > 0) {
		*result = QString(QLatin1String(bu_vls_addr(dbi->gedp->ged_result_str)));
	    }
	} else {
	    *result = QString("command not found");
	    ret = -1;
	}
	bu_free(lcmd, "free tmp cmd str");
	bu_free(largv, "free tmp argv");
    }
    return ret;
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

