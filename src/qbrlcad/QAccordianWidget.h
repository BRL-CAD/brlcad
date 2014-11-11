
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QScrollArea>

class QAccordianWidget;

class QAccordianObject : public QWidget
{
    Q_OBJECT

    public:
	QAccordianObject(QWidget *pparent = 0, QWidget *object = 0, QString header_title = QString(""));
	~QAccordianObject();
	void setWidget(QWidget *object);
	QWidget *getWidget();
	void setVisibility(int val);
	QPushButton *toggle;
	QVBoxLayout *objlayout;
	int visible;

    signals:
	void made_visible(QAccordianObject *);

    public slots:
	void toggleVisibility();

    private:
	QString title;
	QWidget *child_object;
};

class QAccordianWidget : public QWidget
{
    Q_OBJECT

    public:
	QAccordianWidget(QWidget *pparent = 0);
	~QAccordianWidget();
	void addObject(QAccordianObject *object);
	void insertObject(int idx, QAccordianObject *object);
	void deleteObject(QAccordianObject *object);

	void setUniqueVisibility(int val);
	int count();

    public slots:
	void syncVisibility();
	void setOpenObject(QAccordianObject *);

    public:
	int unique_visibility;
	QAccordianObject *open_object;

    private:
	QScrollArea *scrollarea;
	QVBoxLayout *alayout;
	QSet<QAccordianObject *> objects;
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

