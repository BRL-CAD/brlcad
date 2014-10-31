#include "qdbi.h"
#include <QFileInfo>

QDBI::QDBI()
{
    gedp = GED_NULL;
    ged_wdbp = RT_WDB_NULL;
    dbip = DBI_NULL;
}

QDBI::~QDBI()
{
    if (dbip) (void)close();
}

QDBI::QDBI(QString filename)
{
    (void)open(filename);
    current_filename = filename;
}

int
QDBI::open(QString filename)
{
    /* First, make sure the file is actually there */
    QFileInfo *fileinfo = new QFileInfo(filename);
    if (!fileinfo->exists()) {
	delete fileinfo;
	return 1;
    }

    // TODO - somewhere in here we'll need to handle
    // starting the conversion process if we don't have
    // a .g file.  For now, punt unless it's a .g file.
    if (fileinfo->suffix() != "g") {
	std::cout << "unsupported file type!\n";
	delete fileinfo;
	return 1;
    }

    // If we've already got an open file, close it.
    if (dbip != DBI_NULL) (void)close();

    // Call BRL-CAD's database open function
    if ((dbip = db_open(filename.toLocal8Bit(), DB_OPEN_READONLY)) == DBI_NULL) {
	delete fileinfo;
	return 2;
    }

    // Do the rest of the standard initialization steps
    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	delete fileinfo;
	return 3;
    }
    db_update_nref(dbip, &rt_uniresource);

    ged_wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    BU_GET(gedp, struct ged);
    GED_INIT(gedp, ged_wdbp);

    // Inform the world the database has changed
    emit db_change();

    delete fileinfo;
    return 0;

}

int
QDBI::close()
{
    if (gedp && gedp != GED_NULL) {
	ged_close(gedp);
	BU_PUT(gedp, struct ged);
    }
    gedp = GED_NULL;
    ged_wdbp = RT_WDB_NULL;
    dbip = DBI_NULL;

    return 0;
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

