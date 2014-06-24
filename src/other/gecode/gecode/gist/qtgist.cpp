/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/gist/qtgist.hh>

#include <gecode/gist/zoomToFitIcon.hpp>
#include <gecode/gist/nodevisitor.hh>
#include <gecode/gist/nodecursor.hh>

namespace Gecode { namespace Gist {

  Gist::Gist(Space* root, bool bab, QWidget* parent,
             const Options& opt) : QWidget(parent) {
    QGridLayout* layout = new QGridLayout(this);

    QAbstractScrollArea* scrollArea = new QAbstractScrollArea(this);

    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setAutoFillBackground(true);
    QPalette myPalette(scrollArea->palette());
    myPalette.setColor(QPalette::Window, Qt::white);
    scrollArea->setPalette(myPalette);
    canvas = new TreeCanvas(root, bab, scrollArea->viewport(),opt);
    canvas->setPalette(myPalette);
    canvas->setObjectName("canvas");

    connect(scrollArea->horizontalScrollBar(), SIGNAL(valueChanged(int)),
            canvas, SLOT(scroll(void)));
    connect(scrollArea->verticalScrollBar(), SIGNAL(valueChanged(int)),
            canvas, SLOT(scroll(void)));

    QVBoxLayout* sa_layout = new QVBoxLayout();
    sa_layout->setContentsMargins(0,0,0,0);
    sa_layout->addWidget(canvas);
    scrollArea->viewport()->setLayout(sa_layout);

    connect(canvas, SIGNAL(solution(const Space*)),
            this, SIGNAL(solution(const Space*)));

    connect(canvas, SIGNAL(searchFinished(void)), this, SIGNAL(searchFinished(void)));

    QPixmap myPic;
    myPic.loadFromData(zoomToFitIcon, sizeof(zoomToFitIcon));

    QToolButton* autoZoomButton = new QToolButton();
    autoZoomButton->setCheckable(true);
    autoZoomButton->setIcon(myPic);

    nodeStatInspector = new NodeStatInspector(this);

    inspect = new QAction("Inspect", this);
    inspect->setShortcut(QKeySequence("Return"));
    connect(inspect, SIGNAL(triggered()), canvas,
                       SLOT(inspectCurrentNode()));

    inspectBeforeFP = new QAction("Inspect before fixpoint", this);
    inspectBeforeFP->setShortcut(QKeySequence("Ctrl+Return"));
    connect(inspectBeforeFP, SIGNAL(triggered()), canvas,
            SLOT(inspectBeforeFP(void)));

    stop = new QAction("Stop search", this);
    stop->setShortcut(QKeySequence("Esc"));
    connect(stop, SIGNAL(triggered()), canvas,
                    SLOT(stopSearch()));

    reset = new QAction("Reset", this);
    reset->setShortcut(QKeySequence("Ctrl+R"));
    connect(reset, SIGNAL(triggered()), canvas,
            SLOT(reset()));

    navUp = new QAction("Up", this);
    navUp->setShortcut(QKeySequence("Up"));
    connect(navUp, SIGNAL(triggered()), canvas,
                   SLOT(navUp()));

    navDown = new QAction("Down", this);
    navDown->setShortcut(QKeySequence("Down"));
    connect(navDown, SIGNAL(triggered()), canvas,
                     SLOT(navDown()));

    navLeft = new QAction("Left", this);
    navLeft->setShortcut(QKeySequence("Left"));
    connect(navLeft, SIGNAL(triggered()), canvas,
                     SLOT(navLeft()));

    navRight = new QAction("Right", this);
    navRight->setShortcut(QKeySequence("Right"));
    connect(navRight, SIGNAL(triggered()), canvas,
                      SLOT(navRight()));

    navRoot = new QAction("Root", this);
    navRoot->setShortcut(QKeySequence("R"));
    connect(navRoot, SIGNAL(triggered()), canvas,
                      SLOT(navRoot()));

    navNextSol = new QAction("To next solution", this);
    navNextSol->setShortcut(QKeySequence("Shift+Right"));
    connect(navNextSol, SIGNAL(triggered()), canvas,
                      SLOT(navNextSol()));

    navPrevSol = new QAction("To previous solution", this);
    navPrevSol->setShortcut(QKeySequence("Shift+Left"));
    connect(navPrevSol, SIGNAL(triggered()), canvas,
                      SLOT(navPrevSol()));

    searchNext = new QAction("Next solution", this);
    searchNext->setShortcut(QKeySequence("N"));
    connect(searchNext, SIGNAL(triggered()), canvas, SLOT(searchOne()));

    searchAll = new QAction("All solutions", this);
    searchAll->setShortcut(QKeySequence("A"));
    connect(searchAll, SIGNAL(triggered()), canvas, SLOT(searchAll()));

    toggleHidden = new QAction("Hide/unhide", this);
    toggleHidden->setShortcut(QKeySequence("H"));
    connect(toggleHidden, SIGNAL(triggered()), canvas, SLOT(toggleHidden()));

    hideFailed = new QAction("Hide failed subtrees", this);
    hideFailed->setShortcut(QKeySequence("F"));
    connect(hideFailed, SIGNAL(triggered()), canvas, SLOT(hideFailed()));

    unhideAll = new QAction("Unhide all", this);
    unhideAll->setShortcut(QKeySequence("U"));
    connect(unhideAll, SIGNAL(triggered()), canvas, SLOT(unhideAll()));

    labelBranches = new QAction("Label/clear branches", this);
    labelBranches->setShortcut(QKeySequence("L"));
    connect(labelBranches, SIGNAL(triggered()),
            canvas, SLOT(labelBranches()));

    labelPath = new QAction("Label/clear path", this);
    labelPath->setShortcut(QKeySequence("Shift+L"));
    connect(labelPath, SIGNAL(triggered()),
            canvas, SLOT(labelPath()));

    toggleStop = new QAction("Stop/unstop", this);
    toggleStop->setShortcut(QKeySequence("X"));
    connect(toggleStop, SIGNAL(triggered()), canvas, SLOT(toggleStop()));

    unstopAll = new QAction("Do not stop in subtree", this);
    unstopAll->setShortcut(QKeySequence("Shift+X"));
    connect(unstopAll, SIGNAL(triggered()), canvas, SLOT(unstopAll()));

    zoomToFit = new QAction("Zoom to fit", this);
    zoomToFit->setShortcut(QKeySequence("Z"));
    connect(zoomToFit, SIGNAL(triggered()), canvas, SLOT(zoomToFit()));

    center = new QAction("Center current node", this);
    center->setShortcut(QKeySequence("C"));
    connect(center, SIGNAL(triggered()), canvas, SLOT(centerCurrentNode()));

    exportPDF = new QAction("Export subtree PDF...", this);
    exportPDF->setShortcut(QKeySequence("P"));
    connect(exportPDF, SIGNAL(triggered()), canvas,
            SLOT(exportPDF()));

    exportWholeTreePDF = new QAction("Export PDF...", this);
    exportWholeTreePDF->setShortcut(QKeySequence("Ctrl+Shift+P"));
    connect(exportWholeTreePDF, SIGNAL(triggered()), canvas,
            SLOT(exportWholeTreePDF()));

    print = new QAction("Print...", this);
    print->setShortcut(QKeySequence("Ctrl+P"));
    connect(print, SIGNAL(triggered()), canvas,
            SLOT(print()));

    bookmarkNode = new QAction("Add/remove bookmark", this);
    bookmarkNode->setShortcut(QKeySequence("Shift+B"));
    connect(bookmarkNode, SIGNAL(triggered()), canvas, SLOT(bookmarkNode()));

    compareNode = new QAction("Compare", this);
    compareNode->setShortcut(QKeySequence("V"));
    connect(compareNode, SIGNAL(triggered()),
            canvas, SLOT(startCompareNodes()));

    compareNodeBeforeFP = new QAction("Compare before fixpoint", this);
    compareNodeBeforeFP->setShortcut(QKeySequence("Ctrl+V"));
    connect(compareNodeBeforeFP, SIGNAL(triggered()),
            canvas, SLOT(startCompareNodesBeforeFP()));

    connect(canvas, SIGNAL(addedBookmark(const QString&)),
            this, SLOT(addBookmark(const QString&)));
    connect(canvas, SIGNAL(removedBookmark(int)),
            this, SLOT(removeBookmark(int)));

    nullBookmark = new QAction("<none>",this);
    nullBookmark->setCheckable(true);
    nullBookmark->setChecked(false);
    nullBookmark->setEnabled(false);
    bookmarksGroup = new QActionGroup(this);
    bookmarksGroup->setExclusive(false);
    bookmarksGroup->addAction(nullBookmark);
    connect(bookmarksGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(selectBookmark(QAction*)));

    bookmarksMenu = new QMenu("Bookmarks");
    connect(bookmarksMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateBookmarksMenu()));


    setPath = new QAction("Set path", this);
    setPath->setShortcut(QKeySequence("Shift+P"));
    connect(setPath, SIGNAL(triggered()), canvas, SLOT(setPath()));

    inspectPath = new QAction("Inspect path", this);
    inspectPath->setShortcut(QKeySequence("Shift+I"));
    connect(inspectPath, SIGNAL(triggered()), canvas, SLOT(inspectPath()));

    showNodeStats = new QAction("Node statistics", this);
    showNodeStats->setShortcut(QKeySequence("S"));
    connect(showNodeStats, SIGNAL(triggered()),
            this, SLOT(showStats()));

    addAction(inspect);
    addAction(inspectBeforeFP);
    addAction(compareNode);
    addAction(compareNodeBeforeFP);
    addAction(stop);
    addAction(reset);
    addAction(navUp);
    addAction(navDown);
    addAction(navLeft);
    addAction(navRight);
    addAction(navRoot);
    addAction(navNextSol);
    addAction(navPrevSol);

    addAction(searchNext);
    addAction(searchAll);
    addAction(toggleHidden);
    addAction(hideFailed);
    addAction(unhideAll);
    addAction(labelBranches);
    addAction(labelPath);
    addAction(toggleStop);
    addAction(unstopAll);
    addAction(zoomToFit);
    addAction(center);
    addAction(exportPDF);
    addAction(exportWholeTreePDF);
    addAction(print);

    addAction(setPath);
    addAction(inspectPath);
    addAction(showNodeStats);

    nullSolutionInspector = new QAction("<none>",this);
    nullSolutionInspector->setCheckable(true);
    nullSolutionInspector->setChecked(false);
    nullSolutionInspector->setEnabled(false);
    solutionInspectorGroup = new QActionGroup(this);
    solutionInspectorGroup->setExclusive(false);
    solutionInspectorGroup->addAction(nullSolutionInspector);
    connect(solutionInspectorGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(selectSolutionInspector(QAction*)));

    nullDoubleClickInspector = new QAction("<none>",this);
    nullDoubleClickInspector->setCheckable(true);
    nullDoubleClickInspector->setChecked(false);
    nullDoubleClickInspector->setEnabled(false);
    doubleClickInspectorGroup = new QActionGroup(this);
    doubleClickInspectorGroup->setExclusive(false);
    doubleClickInspectorGroup->addAction(nullDoubleClickInspector);
    connect(doubleClickInspectorGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(selectDoubleClickInspector(QAction*)));

    nullMoveInspector = new QAction("<none>",this);
    nullMoveInspector->setCheckable(true);
    nullMoveInspector->setChecked(false);
    nullMoveInspector->setEnabled(false);
    moveInspectorGroup = new QActionGroup(this);
    moveInspectorGroup->setExclusive(false);
    moveInspectorGroup->addAction(nullMoveInspector);
    connect(moveInspectorGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(selectMoveInspector(QAction*)));

    nullComparator = new QAction("<none>",this);
    nullComparator->setCheckable(true);
    nullComparator->setChecked(false);
    nullComparator->setEnabled(false);
    comparatorGroup = new QActionGroup(this);
    comparatorGroup->setExclusive(false);
    comparatorGroup->addAction(nullComparator);
    connect(comparatorGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(selectComparator(QAction*)));

    solutionInspectorMenu = new QMenu("Solution inspectors");
    solutionInspectorMenu->addActions(solutionInspectorGroup->actions());
    doubleClickInspectorMenu = new QMenu("Double click inspectors");
    doubleClickInspectorMenu->addActions(
      doubleClickInspectorGroup->actions());
    moveInspectorMenu = new QMenu("Move inspectors");
    moveInspectorMenu->addActions(moveInspectorGroup->actions());
    comparatorMenu = new QMenu("Comparators");
    comparatorMenu->addActions(comparatorGroup->actions());

    inspectGroup = new QActionGroup(this);
    connect(inspectGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(inspectWithAction(QAction*)));
    inspectBeforeFPGroup = new QActionGroup(this);
    connect(inspectBeforeFPGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(inspectBeforeFPWithAction(QAction*)));

    inspectNodeMenu = new QMenu("Inspect");
    inspectNodeMenu->addAction(inspect);
    connect(inspectNodeMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectors()));

    inspectNodeBeforeFPMenu = new QMenu("Inspect before fixpoint");
    inspectNodeBeforeFPMenu->addAction(inspectBeforeFP);
    connect(inspectNodeBeforeFPMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectors()));
    populateInspectors();

    contextMenu = new QMenu(this);
    contextMenu->addMenu(inspectNodeMenu);
    contextMenu->addMenu(inspectNodeBeforeFPMenu);
    contextMenu->addAction(compareNode);
    contextMenu->addAction(compareNodeBeforeFP);
    contextMenu->addAction(showNodeStats);
    contextMenu->addAction(center);

    contextMenu->addSeparator();

    contextMenu->addAction(searchNext);
    contextMenu->addAction(searchAll);

    contextMenu->addSeparator();

    contextMenu->addAction(toggleHidden);
    contextMenu->addAction(hideFailed);
    contextMenu->addAction(unhideAll);
    contextMenu->addAction(labelBranches);
    contextMenu->addAction(labelPath);

    contextMenu->addAction(toggleStop);
    contextMenu->addAction(unstopAll);

    contextMenu->addSeparator();

    contextMenu->addMenu(bookmarksMenu);
    contextMenu->addAction(setPath);
    contextMenu->addAction(inspectPath);

    contextMenu->addSeparator();

    contextMenu->addMenu(doubleClickInspectorMenu);
    contextMenu->addMenu(solutionInspectorMenu);
    contextMenu->addMenu(moveInspectorMenu);

    connect(autoZoomButton, SIGNAL(toggled(bool)), canvas,
            SLOT(setAutoZoom(bool)));

    connect(canvas, SIGNAL(autoZoomChanged(bool)),
            autoZoomButton, SLOT(setChecked(bool)));

    {
      unsigned int i = 0;
      while (opt.inspect.solution(i)) {
        addSolutionInspector(opt.inspect.solution(i++));
      }
      i = 0;
      while (opt.inspect.click(i)) {
        addDoubleClickInspector(opt.inspect.click(i++));
      }
      i = 0;
      while (opt.inspect.move(i)) {
        addMoveInspector(opt.inspect.move(i++));
      }
      i = 0;
      while (opt.inspect.compare(i)) {
        addComparator(opt.inspect.compare(i++));
      }
    }


    layout->addWidget(scrollArea, 0,0,-1,1);
    layout->addWidget(canvas->scaleBar, 1,1, Qt::AlignHCenter);
    layout->addWidget(autoZoomButton, 0,1, Qt::AlignHCenter);

    setLayout(layout);

    canvas->show();

    resize(500, 400);

    // enables on_<sender>_<signal>() mechanism
    QMetaObject::connectSlotsByName(this);
  }

  void
  Gist::resizeEvent(QResizeEvent*) {
    canvas->resizeToOuter();
  }

  void
  Gist::addInspector(Inspector* i0, QAction*& nas, QAction*& nad,
                     QAction*&nam) {
    if (doubleClickInspectorGroup->
      actions().indexOf(nullDoubleClickInspector) != -1) {
      doubleClickInspectorGroup->removeAction(nullDoubleClickInspector);
      solutionInspectorGroup->removeAction(nullSolutionInspector);
      moveInspectorGroup->removeAction(nullMoveInspector);
    }
    canvas->addSolutionInspector(i0);
    canvas->addDoubleClickInspector(i0);
    canvas->addMoveInspector(i0);

    nas = new QAction(i0->name().c_str(), this);
    nas->setCheckable(true);
    solutionInspectorGroup->addAction(nas);
    solutionInspectorMenu->clear();
    solutionInspectorMenu->addActions(solutionInspectorGroup->actions());

    nad = new QAction(i0->name().c_str(), this);
    nad->setCheckable(true);
    doubleClickInspectorGroup->addAction(nad);
    doubleClickInspectorMenu->clear();
    doubleClickInspectorMenu->addActions(
      doubleClickInspectorGroup->actions());

    nam = new QAction(i0->name().c_str(), this);
    nam->setCheckable(true);
    moveInspectorGroup->addAction(nam);
    moveInspectorMenu->clear();
    moveInspectorMenu->addActions(
      moveInspectorGroup->actions());
    
    QAction* ia = new QAction(i0->name().c_str(), this);
    inspectGroup->addAction(ia);
    QAction* ibfpa = new QAction(i0->name().c_str(), this);
    inspectBeforeFPGroup->addAction(ibfpa);

    if (inspectGroup->actions().size() < 10) {
      ia->setShortcut(QKeySequence(QString("Ctrl+")+
        QString("").setNum(inspectGroup->actions().size())));
      ibfpa->setShortcut(QKeySequence(QString("Ctrl+Alt+")+
        QString("").setNum(inspectBeforeFPGroup->actions().size())));
    }
  }

  void
  Gist::addSolutionInspector(Inspector* ins) {
    QAction* nas;
    QAction* nad;
    QAction* nam;
    if (doubleClickInspectorGroup->
      actions().indexOf(nullDoubleClickInspector) == -1) {
      QList<QAction*> is = solutionInspectorGroup->actions();
      for (int i=0; i<is.size(); i++) {
        canvas->activateSolutionInspector(i,false);
        is[i]->setChecked(false);
      }
    }
    addInspector(ins, nas,nad,nam);
    nas->setChecked(true);
    selectSolutionInspector(nas);
  }

  void
  Gist::addDoubleClickInspector(Inspector* ins) {
    QAction* nas;
    QAction* nad;
    QAction* nam;
    if (doubleClickInspectorGroup->
      actions().indexOf(nullDoubleClickInspector) == -1) {
      QList<QAction*> is = doubleClickInspectorGroup->actions();
      for (int i=0; i<is.size(); i++) {
        canvas->activateDoubleClickInspector(i,false);
        is[i]->setChecked(false);
      }
    }
    addInspector(ins, nas,nad,nam);
    nad->setChecked(true);
    selectDoubleClickInspector(nad);
  }

  void
  Gist::addMoveInspector(Inspector* ins) {
    QAction* nas;
    QAction* nad;
    QAction* nam;
    if (doubleClickInspectorGroup->
      actions().indexOf(nullDoubleClickInspector) == -1) {
      QList<QAction*> is = moveInspectorGroup->actions();
      for (int i=0; i<is.size(); i++) {
        canvas->activateMoveInspector(i,false);
        is[i]->setChecked(false);
      }
    }
    addInspector(ins, nas,nad,nam);
    nam->setChecked(true);
    selectMoveInspector(nam);
  }

  void
  Gist::addComparator(Comparator* c) {
    if (comparatorGroup->actions().indexOf(nullComparator) == -1) {
      QList<QAction*> is = comparatorGroup->actions();
      for (int i=0; i<is.size(); i++) {
        canvas->activateComparator(i,false);
        is[i]->setChecked(false);
      }
    } else {
      comparatorGroup->removeAction(nullComparator);
    }
    canvas->addComparator(c);

    QAction* ncs = new QAction(c->name().c_str(), this);
    ncs->setCheckable(true);
    comparatorGroup->addAction(ncs);
    comparatorMenu->clear();
    comparatorMenu->addActions(comparatorGroup->actions());
    ncs->setChecked(true);
    selectComparator(ncs);
  }

  Gist::~Gist(void) { delete canvas; }

  void
  Gist::on_canvas_contextMenu(QContextMenuEvent* event) {
    contextMenu->popup(event->globalPos());
  }

  void
  Gist::on_canvas_statusChanged(VisualNode* n, const Statistics& stats,
                                bool finished) {
    nodeStatInspector->node(*canvas->na,n,stats,finished);
    if (!finished) {
      inspect->setEnabled(false);
      inspectGroup->setEnabled(false);
      inspectBeforeFP->setEnabled(false);
      inspectBeforeFPGroup->setEnabled(false);
      compareNode->setEnabled(false);
      compareNodeBeforeFP->setEnabled(false);
      showNodeStats->setEnabled(false);
      stop->setEnabled(true);
      reset->setEnabled(false);
      navUp->setEnabled(false);
      navDown->setEnabled(false);
      navLeft->setEnabled(false);
      navRight->setEnabled(false);
      navRoot->setEnabled(false);
      navNextSol->setEnabled(false);
      navPrevSol->setEnabled(false);

      searchNext->setEnabled(false);
      searchAll->setEnabled(false);
      toggleHidden->setEnabled(false);
      hideFailed->setEnabled(false);
      unhideAll->setEnabled(false);
      labelBranches->setEnabled(false);
      labelPath->setEnabled(false);
      
      toggleStop->setEnabled(false);
      unstopAll->setEnabled(false);
      
      center->setEnabled(false);
      exportPDF->setEnabled(false);
      exportWholeTreePDF->setEnabled(false);
      print->setEnabled(false);

      setPath->setEnabled(false);
      inspectPath->setEnabled(false);
      bookmarkNode->setEnabled(false);
      bookmarksGroup->setEnabled(false);
    } else {
      stop->setEnabled(false);
      reset->setEnabled(true);

      if ( (n->isOpen() || n->hasOpenChildren()) && (!n->isHidden()) ) {
        searchNext->setEnabled(true);
        searchAll->setEnabled(true);
      } else {
        searchNext->setEnabled(false);
        searchAll->setEnabled(false);
      }
      if (n->getNumberOfChildren() > 0) {
        navDown->setEnabled(true);
        toggleHidden->setEnabled(true);
        hideFailed->setEnabled(true);
        unhideAll->setEnabled(true);
        unstopAll->setEnabled(true);
      } else {
        navDown->setEnabled(false);
        toggleHidden->setEnabled(false);
        hideFailed->setEnabled(false);
        unhideAll->setEnabled(false);
        unstopAll->setEnabled(false);
      }
      
      toggleStop->setEnabled(n->getStatus() == STOP ||
        n->getStatus() == UNSTOP);

      showNodeStats->setEnabled(true);
      inspect->setEnabled(true);
      labelPath->setEnabled(true);
      if (n->getStatus() == UNDETERMINED) {
        inspectGroup->setEnabled(false);
        inspectBeforeFP->setEnabled(false);
        inspectBeforeFPGroup->setEnabled(false);
        compareNode->setEnabled(false);
        compareNodeBeforeFP->setEnabled(false);
        labelBranches->setEnabled(false);
      } else {
        inspectGroup->setEnabled(true);
        inspectBeforeFP->setEnabled(true);
        inspectBeforeFPGroup->setEnabled(true);
        compareNode->setEnabled(true);
        compareNodeBeforeFP->setEnabled(true);
        labelBranches->setEnabled(!n->isHidden());
      }

      navRoot->setEnabled(true);
      VisualNode* p = n->getParent(*canvas->na);
      if (p == NULL) {
        inspectBeforeFP->setEnabled(false);
        inspectBeforeFPGroup->setEnabled(false);
        navUp->setEnabled(false);
        navRight->setEnabled(false);
        navLeft->setEnabled(false);
      } else {
        navUp->setEnabled(true);
        unsigned int alt = n->getAlternative(*canvas->na);
        navRight->setEnabled(alt + 1 < p->getNumberOfChildren());
        navLeft->setEnabled(alt > 0);
      }

      VisualNode* root = n;
      while (!root->isRoot())
        root = root->getParent(*canvas->na);
      NextSolCursor nsc(n, false, *canvas->na);
      PreorderNodeVisitor<NextSolCursor> nsv(nsc);
      nsv.run();
      navNextSol->setEnabled(nsv.getCursor().node() != root);

      NextSolCursor psc(n, true, *canvas->na);
      PreorderNodeVisitor<NextSolCursor> psv(psc);
      psv.run();
      navPrevSol->setEnabled(psv.getCursor().node() != root);

      center->setEnabled(true);
      exportPDF->setEnabled(true);
      exportWholeTreePDF->setEnabled(true);
      print->setEnabled(true);

      setPath->setEnabled(true);
      inspectPath->setEnabled(true);

      bookmarkNode->setEnabled(true);
      bookmarksGroup->setEnabled(true);
    }
    emit statusChanged(stats,finished);
  }

  void
  Gist::inspectWithAction(QAction* a) {
    canvas->inspectCurrentNode(true,inspectGroup->actions().indexOf(a));
  }

  void
  Gist::inspectBeforeFPWithAction(QAction* a) {
    canvas->inspectCurrentNode(false,
      inspectBeforeFPGroup->actions().indexOf(a));
  }

  bool
  Gist::finish(void) {
    return canvas->finish();
  }

  void
  Gist::selectDoubleClickInspector(QAction* a) {
    canvas->activateDoubleClickInspector(
      doubleClickInspectorGroup->actions().indexOf(a),
      a->isChecked());
  }
  void
  Gist::selectSolutionInspector(QAction* a) {
    canvas->activateSolutionInspector(
      solutionInspectorGroup->actions().indexOf(a),
      a->isChecked());
  }
  void
  Gist::selectMoveInspector(QAction* a) {
    canvas->activateMoveInspector(
      moveInspectorGroup->actions().indexOf(a),
      a->isChecked());
  }
  void
  Gist::selectComparator(QAction* a) {
    canvas->activateComparator(comparatorGroup->actions().indexOf(a),
      a->isChecked());
  }
  void
  Gist::selectBookmark(QAction* a) {
    int idx = bookmarksGroup->actions().indexOf(a);
    canvas->setCurrentNode(canvas->bookmarks[idx]);
    canvas->centerCurrentNode();
  }

  void
  Gist::addBookmark(const QString& id) {
    if (bookmarksGroup->actions().indexOf(nullBookmark) != -1) {
      bookmarksGroup->removeAction(nullBookmark);
    }

    QAction* nb = new QAction(id, this);
    nb->setCheckable(true);
    bookmarksGroup->addAction(nb);
  }

  void
  Gist::removeBookmark(int idx) {
    QAction* a = bookmarksGroup->actions()[idx];
    bookmarksGroup->removeAction(a);
    if (bookmarksGroup->actions().size() == 0) {
      bookmarksGroup->addAction(nullBookmark);
    }
  }
  
  void
  Gist::populateBookmarksMenu(void) {
    bookmarksMenu->clear();
    bookmarksMenu->addAction(bookmarkNode);
    bookmarksMenu->addSeparator();
    bookmarksMenu->addActions(bookmarksGroup->actions());
  }

  void
  Gist::populateInspectors(void) {
    inspectNodeMenu->clear();
    inspectNodeMenu->addAction(inspect);
    inspectNodeMenu->addSeparator();
    inspectNodeMenu->addActions(inspectGroup->actions());
    inspectNodeBeforeFPMenu->clear();
    inspectNodeBeforeFPMenu->addAction(inspectBeforeFP);
    inspectNodeBeforeFPMenu->addSeparator();
    inspectNodeBeforeFPMenu->addActions(inspectBeforeFPGroup->actions());
  }
  
  void
  Gist::setAutoHideFailed(bool b) { canvas->setAutoHideFailed(b); }
  void
  Gist::setAutoZoom(bool b) { canvas->setAutoZoom(b); }
  bool
  Gist::getAutoHideFailed(void) { return canvas->getAutoHideFailed(); }
  bool
  Gist::getAutoZoom(void) { return canvas->getAutoZoom(); }
  void
  Gist::setRefresh(int i) { canvas->setRefresh(i); }
  void
  Gist::setRefreshPause(int i) { canvas->setRefreshPause(i); }
  bool
  Gist::getSmoothScrollAndZoom(void) {
    return canvas->getSmoothScrollAndZoom();
  }
  void
  Gist::setSmoothScrollAndZoom(bool b) {
    canvas->setSmoothScrollAndZoom(b);
  }
  bool
  Gist::getMoveDuringSearch(void) {
    return canvas->getMoveDuringSearch();
  }
  void
  Gist::setMoveDuringSearch(bool b) {
    canvas->setMoveDuringSearch(b);
  }
  void
  Gist::setRecompDistances(int c_d, int a_d) {
    canvas->setRecompDistances(c_d, a_d);
  }

  int
  Gist::getCd(void) {
    return canvas->c_d;
  }
  int
  Gist::getAd(void) {
    return canvas->a_d;
  }

  void
  Gist::setShowCopies(bool b) {
    canvas->setShowCopies(b);
  }
  bool
  Gist::getShowCopies(void) {
    return canvas->getShowCopies();
  }

  void
  Gist::showStats(void) {
    nodeStatInspector->showStats();
    canvas->emitStatusChanged();
  }

}}

// STATISTICS: gist-any
