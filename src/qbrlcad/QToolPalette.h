#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QIcon>
#include <QScrollArea>
#include <QSplitter>
#include "QFlowLayout.h"

class QToolPaletteElement;
class QToolControlContainer;

class QToolPaletteButton: public QPushButton
{
    Q_OBJECT

    public:
	QToolPaletteButton(QWidget *bparent = 0, QIcon *iicon = 0, QToolPaletteElement *eparent = 0);
	~QToolPaletteButton(){};

	void setButtonElement(QIcon *iicon, QToolPaletteElement *n_element);

    public slots:
	void select_element();

    signals:
	void element_selected(QToolPaletteElement *);

    private:
	QToolPaletteElement *element;
};


class QToolPaletteElement: public QWidget
{
    Q_OBJECT

    public:
	QToolPaletteElement(QWidget *eparent = 0, QIcon *iicon = 0, QWidget *control = 0);
	~QToolPaletteElement();

	void setButton(QToolPaletteButton *n_button);
	void setControls(QWidget *n_controls);

    public:
	QToolPaletteButton *button;
	QWidget *controls;
};

class QToolControlContainer: public QWidget
{
    Q_OBJECT

    public:
	QToolControlContainer(QWidget *eparent = 0, QWidget *control = 0);
	~QToolControlContainer() {};

    public:
	QVBoxLayout *control_layout;
};


class QToolPalette: public QWidget
{
    Q_OBJECT

    public:
	QToolPalette(QWidget *pparent = 0, QToolControlContainer *ccontainer = 0);
	~QToolPalette();
	void addElement(QToolPaletteElement *element);
	void deleteElement(QToolPaletteElement *element);
	void setIconWidth(int iwidth);
	void setIconHeight(int iheight);
	void setAlwaysSelected(int iheight);  // If 0 can disable all tools, if 1 some tool is always selected

	// TODO - do we need to change containers or is assign-at-creation sufficient?
	//void setControlContainer(QToolControlContainer *container);
	void resizeEvent(QResizeEvent *pevent);

   public slots:
	void displayElement(QToolPaletteElement *);
	void button_layout_resize();

   private:
      int always_selected;
      int icon_width;
      int icon_height;
      QSplitter *splitter;
      QWidget *button_container;
      QFlowLayout *button_layout;
      QToolControlContainer *control_container;
      QToolPaletteElement *selected;
      QSet<QToolPaletteElement *> elements;
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

