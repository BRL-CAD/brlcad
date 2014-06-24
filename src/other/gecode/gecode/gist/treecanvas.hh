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

#ifndef GECODE_GIST_TREECANVAS_HH
#define GECODE_GIST_TREECANVAS_HH

#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif

#include <gecode/kernel.hh>
#include <gecode/gist.hh>

#include <gecode/gist/visualnode.hh>

namespace Gecode {  namespace Gist {

  /// \brief Parameters for the tree layout
  namespace LayoutConfig {
    /// Minimum scale factor
    const int minScale = 10;
    /// Maximum scale factor
    const int maxScale = 400;
    /// Default scale factor
    const int defScale = 100;
    /// Maximum scale factor for automatic zoom
    const int maxAutoZoomScale = defScale;
  }

  class TreeCanvas;

  /// \brief A thread that concurrently explores the tree
  class SearcherThread : public QThread {
    Q_OBJECT
  private:
    VisualNode* node;
    int depth;
    bool a;
    TreeCanvas* t;
    void updateCanvas(void);
  public:
    void search(VisualNode* n, bool all, TreeCanvas* ti);

  Q_SIGNALS:
    void update(int w, int h, int scale0);
    void statusChanged(bool);
    void scaleChanged(int);
    void solution(const Space*);
    void searchFinished(void);
    void moveToNode(VisualNode* n,bool);
  protected:
    void run(void);
  };

  /// \brief A canvas that displays the search tree
  class GECODE_GIST_EXPORT TreeCanvas : public QWidget {
    Q_OBJECT

    friend class SearcherThread;
    friend class Gist;

  public:
    /// Constructor
    TreeCanvas(Space* rootSpace, bool bab, QWidget* parent, 
               const Options& opt);
    /// Destructor
    ~TreeCanvas(void);

    /// Add inspector \a i
    void addDoubleClickInspector(Inspector* i);
    /// Set active inspector
    void activateDoubleClickInspector(int i, bool active);
    /// Add inspector \a i
    void addSolutionInspector(Inspector* i);
    /// Set active inspector
    void activateSolutionInspector(int i, bool active);
    /// Add inspector \a i
    void addMoveInspector(Inspector* i);
    /// Set active inspector
    void activateMoveInspector(int i, bool active);
    /// Add comparator \a c
    void addComparator(Comparator* c);
    /// Set active comparator
    void activateComparator(int i, bool active);

  public Q_SLOTS:
    /// Set scale factor to \a scale0
    void scaleTree(int scale0, int zoomx=-1, int zoomy=-1);

    /// Explore complete subtree of selected node
    void searchAll(void);
    /// Find next solution below selected node
    void searchOne(void);
    /// Toggle hidden state of selected node
    void toggleHidden(void);
    /// Hide failed subtrees of selected node
    void hideFailed(void);
    /// Unhide all nodes below selected node
    void unhideAll(void);
    /// Do not stop at selected stop node
    void toggleStop(void);
    /// Do not stop at any stop node
    void unstopAll(void);
    /// Export pdf of the current subtree
    void exportPDF(void);
    /// Export pdf of the whole tree
    void exportWholeTreePDF(void);
    /// Print the tree
    void print(void);
    /// Zoom the canvas so that the whole tree fits
    void zoomToFit(void);
    /// Center the view on the currently selected node
    void centerCurrentNode(void);
    /**
     * \brief Call the double click inspector for the currently selected node
     *
     * If \a fix is true, then the node is inspected after fixpoint
     * computation, otherwise its status after branching but before
     * fixpoint computation is inspected.
     */
    void inspectCurrentNode(bool fix=true, int inspectorNo=-1);
    /// Calls inspectCurrentNode(false)
    void inspectBeforeFP(void);
    /// Label all branches in subtree under current node
    void labelBranches(void);
    /// Label all branches on path to root node
    void labelPath(void);

    /// Stop current search
    void stopSearch(void);

    /// Reset
    void reset(void);

    /// Move selection to the parent of the selected node
    void navUp(void);
    /// Move selection to the first child of the selected node
    void navDown(void);
    /// Move selection to the left sibling of the selected node
    void navLeft(void);
    /// Move selection to the right sibling of the selected node
    void navRight(void);
    /// Move selection to the root node
    void navRoot(void);
    /// Move selection to next solution (in DFS order)
    void navNextSol(bool back = false);
    /// Move selection to previous solution (in DFS order)
    void navPrevSol(void);

    /// Bookmark current node
    void bookmarkNode(void);
    /// Set the current node to be the head of the path
    void setPath(void);
    /// Call the double click inspector for all nodes on the path from root to head of the path
    void inspectPath(void);
    /// Wait for click on node to compare with current node
    void startCompareNodes(void);
    /// Wait for click on node to compare with current node before fixpoint
    void startCompareNodesBeforeFP(void);
    
    /// Re-emit status change information for current node
    void emitStatusChanged(void);

    /// Set recomputation distances
    void setRecompDistances(int c_d, int a_d);
    /// Set preference whether to automatically hide failed subtrees
    void setAutoHideFailed(bool b);
    /// Set preference whether to automatically zoom to fit
    void setAutoZoom(bool b);
    /// Return preference whether to automatically hide failed subtrees
    bool getAutoHideFailed(void);
    /// Return preference whether to automatically zoom to fit
    bool getAutoZoom(void);
    /// Set preference whether to show copies in the tree
    void setShowCopies(bool b);
    /// Return preference whether to show copies in the tree
    bool getShowCopies(void);
    /// Set refresh rate
    void setRefresh(int i);
    /// Set refresh pause in msec
    void setRefreshPause(int i);
    /// Return preference whether to use smooth scrolling and zooming
    bool getSmoothScrollAndZoom(void);
    /// Set preference whether to use smooth scrolling and zooming
    void setSmoothScrollAndZoom(bool b);
    /// Return preference whether to move cursor during search
    bool getMoveDuringSearch(void);
    /// Set preference whether to move cursor during search
    void setMoveDuringSearch(bool b);
    /// Resize to the outer widget size if auto zoom is enabled
    void resizeToOuter(void);

    /// Stop search and wait for it to finish
    bool finish(void);

  Q_SIGNALS:
    /// The scale factor has changed
    void scaleChanged(int);
    /// The auto-zoom state was changed
    void autoZoomChanged(bool);
    /// Context menu triggered
    void contextMenu(QContextMenuEvent*);
    /// Status bar update
    void statusChanged(VisualNode*,const Statistics&, bool);
    /// Signals that a solution has been found
    void solution(const Space*);
    /// Signals that %Gist is finished
    void searchFinished(void);
    /// Signals that a bookmark has been added
    void addedBookmark(const QString& id);
    /// Signals that a bookmark has been removed
    void removedBookmark(int idx);
  protected:
    /// Mutex for synchronizing acccess to the tree
    QMutex mutex;
    /// Mutex for synchronizing layout and drawing
    QMutex layoutMutex;
    /// Search engine thread
    SearcherThread searcher;
    /// Flag signalling the search to stop
    bool stopSearchFlag;
    /// Flag signalling that Gist is ready to be closed
    bool finishedFlag;
    /// Allocator for nodes
    Node::NodeAllocator* na;
    /// The root node of the tree
    VisualNode* root;
    /// The currently best solution (for branch-and-bound)
    BestNode* curBest;
    /// The currently selected node
    VisualNode* currentNode;
    /// The head of the currently selected path
    VisualNode* pathHead;
    /// The registered click inspectors, and whether they are active
    QVector<QPair<Inspector*,bool> > doubleClickInspectors;
    /// The registered solution inspectors, and whether they are active
    QVector<QPair<Inspector*,bool> > solutionInspectors;
    /// The registered move inspectors, and whether they are active
    QVector<QPair<Inspector*,bool> > moveInspectors;
    /// The registered comparators, and whether they are active
    QVector<QPair<Comparator*,bool> > comparators;

    /// The bookmarks map
    QVector<VisualNode*> bookmarks;

    /// Whether node comparison action is running
    bool compareNodes;
    /// Whether node comparison action computes fixpoint
    bool compareNodesBeforeFP;

    /// The scale bar
    QSlider* scaleBar;

    /// Statistics about the search tree
    Statistics stats;

    /// Current scale factor
    double scale;
    /// Offset on the x axis so that the tree is centered
    int xtrans;

    /// Whether to hide failed subtrees automatically
    bool autoHideFailed;
    /// Whether to zoom automatically
    bool autoZoom;
    /// Whether to show copies in the tree
    bool showCopies;
    /// Refresh rate
    int refresh;
    /// Time (in msec) to pause after each refresh
    int refreshPause;
    /// Whether to use smooth scrolling and zooming
    bool smoothScrollAndZoom;
    /// Whether to move cursor during search
    bool moveDuringSearch;
    
    /// The recomputation distance
    int c_d;
    /// The adaptive recomputation distance
    int a_d;

    /// Return the node corresponding to the \a event position
    VisualNode* eventNode(QEvent *event);
    /// General event handler, used for displaying tool tips
    bool event(QEvent *event);
    /// Paint the tree
    void paintEvent(QPaintEvent* event);
    /// Handle mouse press event
    void mousePressEvent(QMouseEvent* event);
    /// Handle mouse double click event
    void mouseDoubleClickEvent(QMouseEvent* event);
    /// Handle context menu event
    void contextMenuEvent(QContextMenuEvent* event);
    /// Handle resize event
    void resizeEvent(QResizeEvent* event);
    /// Handle mouse wheel events
    void wheelEvent(QWheelEvent* event);

    /// Timer for smooth zooming
    QTimeLine zoomTimeLine;
    /// Timer for smooth scrolling
    QTimeLine scrollTimeLine;
    /// Target x coordinate after smooth scrolling
    int targetX;
    /// Source x coordinate after smooth scrolling
    int sourceX;
    /// Target y coordinate after smooth scrolling
    int targetY;
    /// Target y coordinate after smooth scrolling
    int sourceY;

    /// Target width after layout
    int targetW;
    /// Target height after layout
    int targetH;
    /// Target scale after layout
    int targetScale;
    /// Timer id for delaying the update
    int layoutDoneTimerId;

    /// Timer invoked for smooth zooming and scrolling
    virtual void timerEvent(QTimerEvent* e);

  public Q_SLOTS:
    /// Update display
    void update(void);
    /// React to scroll events
    void scroll(void);
    /// Layout done
    void layoutDone(int w, int h, int scale0);
    /// Set the selected node to \a n
    void setCurrentNode(VisualNode* n, bool finished=true, bool update=true);
  private Q_SLOTS:
    /// Search has finished
    void statusChanged(bool);
    /// Export PDF of the subtree of \a n
    void exportNodePDF(VisualNode* n);
    /// A new solution \a s has been found
    void inspectSolution(const Space* s);
    /// Scroll to \a i percent of the target
    void scroll(int i);
  };

}}

#endif

// STATISTICS: gist-any
