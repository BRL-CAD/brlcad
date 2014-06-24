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

#include <gecode/gist/mainwindow.hh>
#include <gecode/gist/preferences.hh>
#include <gecode/gist/nodewidget.hh>
#include <gecode/gist/drawingcursor.hh>

#include <gecode/gist/gecodelogo.hh>

namespace Gecode { namespace Gist {

  AboutGist::AboutGist(QWidget* parent) : QDialog(parent) {

    Logos logos;
    QPixmap myPic;
    myPic.loadFromData(logos.logo, logos.logoSize);

    QPixmap myPic2;
    myPic2.loadFromData(logos.gistLogo, logos.gistLogoSize);
    setWindowIcon(myPic2);


    setMinimumSize(300, 240);
    setMaximumSize(300, 240);
    QVBoxLayout* layout = new QVBoxLayout();
    QLabel* logo = new QLabel();
    logo->setPixmap(myPic);
    layout->addWidget(logo, 0, Qt::AlignCenter);
    QLabel* aboutLabel =
      new QLabel(tr("<h2>Gist</h2>"
                     "<p><b>The Gecode Interactive Search Tool</b</p> "
                    "<p>You can find more information about Gecode and Gist "
                    "at</p>"
                    "<p><a href='http://www.gecode.org'>www.gecode.org</a>"
                    "</p"));
    aboutLabel->setOpenExternalLinks(true);
    aboutLabel->setWordWrap(true);
    aboutLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(aboutLabel);
    setLayout(layout);
    setWindowTitle(tr("About Gist"));
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
  }

  GistMainWindow::GistMainWindow(Space* root, bool bab,
                                 const Options& opt0)
  : opt(opt0), aboutGist(this) {
    c = new Gist(root,bab,this,opt);
    setCentralWidget(c);
    setWindowTitle(tr("Gist"));

    Logos logos;
    QPixmap myPic;
    myPic.loadFromData(logos.gistLogo, logos.gistLogoSize);
    setWindowIcon(myPic);

    resize(500,500);
    setMinimumSize(400, 200);

    menuBar = new QMenuBar(0);

    QMenu* fileMenu = menuBar->addMenu(tr("&File"));
    fileMenu->addAction(c->print);
#if QT_VERSION >= 0x040400
    fileMenu->addAction(c->exportWholeTreePDF);
#endif
    QAction* quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAction, SIGNAL(triggered()),
            this, SLOT(close()));
    prefAction = fileMenu->addAction(tr("Preferences"));
    connect(prefAction, SIGNAL(triggered()), this, SLOT(preferences()));

    QMenu* nodeMenu = menuBar->addMenu(tr("&Node"));
    
    inspectNodeMenu = new QMenu("Inspect");
    inspectNodeMenu->addAction(c->inspect);
    connect(inspectNodeMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectors()));

    inspectNodeBeforeFPMenu = new QMenu("Inspect before fixpoint");
    inspectNodeBeforeFPMenu->addAction(c->inspectBeforeFP);
    connect(inspectNodeBeforeFPMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectors()));
    populateInspectors();
    
    nodeMenu->addMenu(inspectNodeMenu);
    nodeMenu->addMenu(inspectNodeBeforeFPMenu);
    nodeMenu->addAction(c->compareNode);
    nodeMenu->addAction(c->compareNodeBeforeFP);
    nodeMenu->addAction(c->setPath);
    nodeMenu->addAction(c->inspectPath);
    nodeMenu->addAction(c->showNodeStats);
    bookmarksMenu = new QMenu("Bookmarks");
    bookmarksMenu->addAction(c->bookmarkNode);
    connect(bookmarksMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateBookmarks()));
    nodeMenu->addMenu(bookmarksMenu);
    nodeMenu->addSeparator();
    nodeMenu->addAction(c->navUp);
    nodeMenu->addAction(c->navDown);
    nodeMenu->addAction(c->navLeft);
    nodeMenu->addAction(c->navRight);
    nodeMenu->addAction(c->navRoot);
    nodeMenu->addAction(c->navNextSol);
    nodeMenu->addAction(c->navPrevSol);
    nodeMenu->addSeparator();
    nodeMenu->addAction(c->toggleHidden);
    nodeMenu->addAction(c->hideFailed);
    nodeMenu->addAction(c->unhideAll);
    nodeMenu->addAction(c->labelBranches);
    nodeMenu->addAction(c->labelPath);
    nodeMenu->addAction(c->toggleStop);
    nodeMenu->addAction(c->unstopAll);
    nodeMenu->addSeparator();
    nodeMenu->addAction(c->zoomToFit);
    nodeMenu->addAction(c->center);
#if QT_VERSION >= 0x040400
    nodeMenu->addAction(c->exportPDF);
#endif

    QMenu* searchMenu = menuBar->addMenu(tr("&Search"));
    searchMenu->addAction(c->searchNext);
    searchMenu->addAction(c->searchAll);
    searchMenu->addSeparator();
    searchMenu->addAction(c->stop);
    searchMenu->addSeparator();
    searchMenu->addAction(c->reset);

    QMenu* toolsMenu = menuBar->addMenu(tr("&Tools"));
    doubleClickInspectorsMenu = new QMenu("Double click Inspectors");
    connect(doubleClickInspectorsMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectorSelection()));
    toolsMenu->addMenu(doubleClickInspectorsMenu);
    solutionInspectorsMenu = new QMenu("Solution inspectors");
    connect(solutionInspectorsMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectorSelection()));
    toolsMenu->addMenu(solutionInspectorsMenu);
    moveInspectorsMenu = new QMenu("Move inspectors");
    connect(moveInspectorsMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectorSelection()));
    toolsMenu->addMenu(moveInspectorsMenu);
    comparatorsMenu = new QMenu("Comparators");
    connect(comparatorsMenu, SIGNAL(aboutToShow()),
            this, SLOT(populateInspectorSelection()));
    toolsMenu->addMenu(comparatorsMenu);

    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("About"));
    connect(aboutAction, SIGNAL(triggered()),
            this, SLOT(about()));

    // Don't add the menu bar on Mac OS X
#ifndef Q_WS_MAC
    setMenuBar(menuBar);
#endif

    // Set up status bar
    QWidget* stw = new QWidget();
    QHBoxLayout* hbl = new QHBoxLayout();
    hbl->setContentsMargins(0,0,0,0);
    wmpLabel = new QLabel("");
    hbl->addWidget(wmpLabel);
    hbl->addWidget(new QLabel("Depth:"));
    depthLabel = new QLabel("0");
    hbl->addWidget(depthLabel);
    hbl->addWidget(new NodeWidget(SOLVED));
    solvedLabel = new QLabel("0");
    hbl->addWidget(solvedLabel);
    hbl->addWidget(new NodeWidget(FAILED));
    failedLabel = new QLabel("0");
    hbl->addWidget(failedLabel);
    hbl->addWidget(new NodeWidget(BRANCH));
    choicesLabel = new QLabel("0");
    hbl->addWidget(choicesLabel);
    hbl->addWidget(new NodeWidget(UNDETERMINED));
    openLabel = new QLabel("     0");
    hbl->addWidget(openLabel);
    stw->setLayout(hbl);
    statusBar()->addPermanentWidget(stw);

    isSearching = false;
    statusBar()->showMessage("Ready");

    connect(c,SIGNAL(statusChanged(const Statistics&,bool)),
            this,SLOT(statusChanged(const Statistics&,bool)));

    connect(c,SIGNAL(searchFinished(void)),this,SLOT(close(void)));

    preferences(true);
    show();
    c->reset->trigger();
  }

  void
  GistMainWindow::closeEvent(QCloseEvent* event) {
    if (c->finish())
      event->accept();
    else
      event->ignore();
  }

  void
  GistMainWindow::statusChanged(const Statistics& stats, bool finished) {
    if (stats.maxDepth==0) {
      isSearching = false;
      statusBar()->showMessage("Ready");
      prefAction->setEnabled(true);
    } else if (isSearching && finished) {
      isSearching = false;
      double ms = searchTimer.stop();
      double s = std::floor(ms / 1000.0);
      ms -= s*1000.0;
      double m = std::floor(s / 60.0);
      s -= m*60.0;
      double h = std::floor(m / 60.0);
      m -= h*60.0;

      // QString t;
      // if (static_cast<int>(h) != 0)
      //   t += QString().setNum(static_cast<int>(h))+"h ";
      // if (static_cast<int>(m) != 0)
      //   t += QString().setNum(static_cast<int>(m))+"m ";
      // if (static_cast<int>(s) != 0)
      //   t += QString().setNum(static_cast<int>(s));
      // else
      //   t += "0";
      // t += "."+QString().setNum(static_cast<int>(ms))+"s";
      // statusBar()->showMessage(QString("Ready (search time ")+t+")");
      statusBar()->showMessage("Ready");
      prefAction->setEnabled(true);
    } else if (!isSearching && !finished) {
      prefAction->setEnabled(false);
      statusBar()->showMessage("Searching");
      isSearching = true;
      searchTimer.start();
    }
    depthLabel->setNum(stats.maxDepth);
    solvedLabel->setNum(stats.solutions);
    failedLabel->setNum(stats.failures);
    choicesLabel->setNum(stats.choices);
    openLabel->setNum(stats.undetermined);
    if (stats.wmp)
      wmpLabel->setText("WMP");
    else
      wmpLabel->setText("");
  }

  void
  GistMainWindow::about(void) {
    aboutGist.show();
  }

  void
  GistMainWindow::preferences(bool setup) {
    PreferencesDialog pd(opt, this);
    if (setup) {
      c->setAutoZoom(pd.zoom);
    }
    if (setup || pd.exec() == QDialog::Accepted) {
      c->setAutoHideFailed(pd.hideFailed);
      c->setRefresh(pd.refresh);
      c->setRefreshPause(pd.refreshPause);
      c->setSmoothScrollAndZoom(pd.smoothScrollAndZoom);
      c->setMoveDuringSearch(pd.moveDuringSearch);
      c->setRecompDistances(pd.c_d,pd.a_d);
      opt.c_d = pd.c_d;
      opt.a_d = pd.a_d;
      c->setShowCopies(pd.copies);
    }
  }

  void
  GistMainWindow::populateInspectorSelection(void) {
    doubleClickInspectorsMenu->clear();
    doubleClickInspectorsMenu->addActions(
      c->doubleClickInspectorGroup->actions());
    solutionInspectorsMenu->clear();
    solutionInspectorsMenu->addActions(c->solutionInspectorGroup->actions());
    moveInspectorsMenu->clear();
    moveInspectorsMenu->addActions(c->moveInspectorGroup->actions());
    comparatorsMenu->clear();
    comparatorsMenu->addActions(c->comparatorGroup->actions());
  }

  void
  GistMainWindow::populateBookmarks(void) {
    bookmarksMenu->clear();
    bookmarksMenu->addAction(c->bookmarkNode);
    bookmarksMenu->addSeparator();
    bookmarksMenu->addActions(c->bookmarksGroup->actions());
  }

  void
  GistMainWindow::populateInspectors(void) {
    inspectNodeMenu->clear();
    inspectNodeMenu->addAction(c->inspect);
    inspectNodeMenu->addSeparator();
    inspectNodeMenu->addActions(c->inspectGroup->actions());
    inspectNodeBeforeFPMenu->clear();
    inspectNodeBeforeFPMenu->addAction(c->inspectBeforeFP);
    inspectNodeBeforeFPMenu->addSeparator();
    inspectNodeBeforeFPMenu->addActions(c->inspectBeforeFPGroup->actions());
  }

}}

// STATISTICS: gist-any
