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

#ifndef GECODE_GIST_QTGIST_HH
#define GECODE_GIST_QTGIST_HH

#include <gecode/gist/treecanvas.hh>
#include <gecode/gist/nodestats.hh>

/*
 * Configure linking
 *
 */

#if !defined(GIST_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_GIST
#define GECODE_GIST_EXPORT __declspec( dllexport )
#else
#define GECODE_GIST_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_GIST_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_GIST_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_GIST
#define GECODE_LIBRARY_NAME "Gist"
#include <gecode/support/auto-link.hpp>
#endif

namespace Gecode {  namespace Gist {

  /**
   * \brief %Gecode Interactive %Search Tool
   *
   * This class provides an interactive search tree viewer and explorer as
   * a Qt widget. You can embedd or inherit from this widget to use %Gist
   * in your own project.
   *
   * \ingroup TaskGist
   */
  class GECODE_GIST_EXPORT Gist : public QWidget {
    Q_OBJECT
  private:
    /// The canvas implementation
    TreeCanvas* canvas;
    /// The time slider
    QSlider* timeBar;
    /// Context menu
    QMenu* contextMenu;
    /// Action used when no solution inspector is registered
    QAction* nullSolutionInspector;
    /// Menu of solution inspectors
    QMenu* solutionInspectorMenu;
    /// Action used when no double click inspector is registered
    QAction* nullDoubleClickInspector;
    /// Menu of double click inspectors
    QMenu* doubleClickInspectorMenu;
    /// Action used when no double click inspector is registered
    QAction* nullMoveInspector;
    /// Menu of double click inspectors
    QMenu* moveInspectorMenu;
    /// Action used when no comparator is registered
    QAction* nullComparator;
    /// Menu of comparators
    QMenu* comparatorMenu;
    /// Action used when no bookmark exists
    QAction* nullBookmark;
    /// Bookmark menu
    QMenu* bookmarksMenu;
    /// Menu for direct node inspection
    QMenu* inspectNodeMenu;
    /// Menu for direct node inspection before fixpoint
    QMenu* inspectNodeBeforeFPMenu;
    /// Information about individual nodes
    NodeStatInspector* nodeStatInspector;
  public:
    /// Inspect current node
    QAction* inspect;
    /// Inspect current node before fixpoint
    QAction* inspectBeforeFP;
    /// Stop search
    QAction* stop;
    /// Reset %Gist
    QAction* reset;
    /// Navigate to parent node
    QAction* navUp;
    /// Navigate to leftmost child node
    QAction* navDown;
    /// Navigate to left sibling
    QAction* navLeft;
    /// Navigate to right sibling
    QAction* navRight;
    /// Navigate to root node
    QAction* navRoot;
    /// Navigate to next solution (to the left)
    QAction* navNextSol;
    /// Navigate to previous solution (to the right)
    QAction* navPrevSol;
    /// Search next solution in current subtree
    QAction* searchNext;
    /// Search all solutions in current subtree
    QAction* searchAll;
    /// Toggle whether current node is hidden
    QAction* toggleHidden;
    /// Hide failed subtrees under current node
    QAction* hideFailed;
    /// Unhide all hidden subtrees under current node
    QAction* unhideAll;
    /// Label branches under current node
    QAction* labelBranches;
    /// Label branches on path to root
    QAction* labelPath;
    /// Zoom tree to fit window
    QAction* zoomToFit;
    /// Center on current node
    QAction* center;
    /// Export PDF of current subtree
    QAction* exportPDF;
    /// Export PDF of whole tree
    QAction* exportWholeTreePDF;
    /// Print tree
    QAction* print;

    /// Bookmark current node
    QAction* bookmarkNode;
    /// Compare current node to other node
    QAction* compareNode;
    /// Compare current node to other node before fixpoint
    QAction* compareNodeBeforeFP;
    /// Set path from current node to the root
    QAction* setPath;
    /// Inspect all nodes on selected path
    QAction* inspectPath;
    /// Open node statistics inspector
    QAction* showNodeStats;
    /// Bookmark current node
    QAction* toggleStop;
    /// Bookmark current node
    QAction* unstopAll;

    /// Group of all actions for solution inspectors
    QActionGroup* solutionInspectorGroup;
    /// Group of all actions for double click inspectors
    QActionGroup* doubleClickInspectorGroup;
    /// Group of all actions for move inspectors
    QActionGroup* moveInspectorGroup;
    /// Group of all actions for comparators
    QActionGroup* comparatorGroup;
    /// Group of all actions for bookmarks
    QActionGroup* bookmarksGroup;
    /// Group of all actions for direct inspector selection
    QActionGroup* inspectGroup;
    /// Group of all actions for direct inspector selection
    QActionGroup* inspectBeforeFPGroup;
  public:
    /// Constructor
    Gist(Space* root, bool bab, QWidget* parent, const Options& opt);
    /// Destructor
    ~Gist(void);

    /// Add double click inspector \a i0
    void addDoubleClickInspector(Inspector* i0);
    /// Add solution inspector \a i0
    void addSolutionInspector(Inspector* i0);
    /// Add move inspector \a i0
    void addMoveInspector(Inspector* i0);
    /// Add comparator \a c0
    void addComparator(Comparator* c0);

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

    /// Set recomputation parameters \a c_d and \a a_d
    void setRecompDistances(int c_d, int a_d);
    /// Return recomputation distance
    int getCd(void);
    /// Return adaptive recomputation distance
    int getAd(void);

    /// Stop search and wait until finished
    bool finish(void);

    /// Handle resize event
    void resizeEvent(QResizeEvent*);

  Q_SIGNALS:
    /// Signals that the tree has changed
    void statusChanged(const Statistics&, bool);

    /// Signals that a solution has been found
    void solution(const Space*);

    /// Signals that %Gist is ready to be closed
    void searchFinished(void);

  private Q_SLOTS:
    /// Displays the context menu for a node
    void on_canvas_contextMenu(QContextMenuEvent*);
    /// Reacts on status changes
    void on_canvas_statusChanged(VisualNode*, const Statistics&, bool);
    /// Reacts on double click inspector selection
    void selectDoubleClickInspector(QAction*);
    /// Reacts on solution inspector selection
    void selectSolutionInspector(QAction*);
    /// Reacts on move inspector selection
    void selectMoveInspector(QAction*);
    /// Reacts on comparator selection
    void selectComparator(QAction*);
    /// Reacts on bookmark selection
    void selectBookmark(QAction*);
    /// Reacts on adding a bookmark
    void addBookmark(const QString& id);
    /// Reacts on removing a bookmark
    void removeBookmark(int idx);
    /// Populate the inspector menus from the actions found in Gist
    void populateInspectors(void);
    /// Populate the bookmarks menu
    void populateBookmarksMenu(void);
    /// Shows node status information
    void showStats(void);
    /// Inspect current node with inspector described by \a a
    void inspectWithAction(QAction* a);
    /// Inspect current node with inspector described by \a a
    void inspectBeforeFPWithAction(QAction* a);
  protected:
    /// Add inspector \a i0
    void addInspector(Inspector* i, QAction*& nas, QAction*& nad,
                      QAction*& nam);
  };

}}

#endif

// STATISTICS: gist-any
