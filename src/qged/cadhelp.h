#ifndef QCADHELP_H
#define QCADHELP_H 

#include <QObject>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include "cadapp.h"

class CADManViewer : public QDialog
{
  Q_OBJECT

public:
  CADManViewer(QWidget* Parent, QString *manpage = 0);
  ~CADManViewer();

  QTextBrowser *browser;
  QDialogButtonBox *buttons;
};

int cad_man_view(QString *args, CADApp *app);

#endif //QCADHELP_H

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

