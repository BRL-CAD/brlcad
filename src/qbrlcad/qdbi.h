#ifndef QDBI_H
#define QDBI_H

#include <QObject>
#include <QString>
#include <QMap>

#include "bu/file.h"
#include "raytrace.h"
#include "ged.h"

class QDBI : public QObject
{
    Q_OBJECT

    public:
	QDBI();
	QDBI(QString filename);
	QDBI(const QDBI &other);
	~QDBI();

	int open(QString filename);
	int close();

	QString current_filename;
	struct db_i *dbip;
	struct ged  *gedp;
	struct rt_wdb *ged_wdbp;

    signals:
	void db_change();  // TODO - need this to carry some information about what has changed, if possible...
	void view_change();
};

#endif //QDBI_H

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

