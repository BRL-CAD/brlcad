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

#ifndef GECODE_GIST_MAINWINDOW_HH
#define GECODE_GIST_MAINWINDOW_HH

#include <gecode/gist.hh>
#include <gecode/gist/qtgist.hh>

namespace Gecode { namespace Gist {

  /// Display information about %Gist
  class AboutGist : public QDialog {
  public:
    /// Constructor
    AboutGist(QWidget* parent = 0);
  };

  /**
   * \brief Main window for stand-alone %Gist
   *
   * \ingroup TaskGist
   */
  class GECODE_GIST_EXPORT GistMainWindow : public QMainWindow {
    Q_OBJECT
  private:
    /// Options
    Options opt;
    /// Whether search is currently running
    bool isSearching;
    /// Measure search time
    Support::Timer searchTimer;
    /// Status bar label for maximum depth indicator
    QLabel* depthLabel;
    /// Status bar label for weakly monotonic propagator indicator
    QLabel* wmpLabel;
    /// Status bar label for number of solutions
    QLabel* solvedLabel;
    /// Status bar label for number of failures
    QLabel* failedLabel;
    /// Status bar label for number of choices
    QLabel* choicesLabel;
    /// Status bar label for number of open nodes
    QLabel* openLabel;
    /// Menu for solution inspectors
    QMenu* solutionInspectorsMenu;
    /// Menu for double click inspectors
    QMenu* doubleClickInspectorsMenu;
    /// Menu for move inspectors
    QMenu* moveInspectorsMenu;
    /// Menu for comparators
    QMenu* comparatorsMenu;
    /// Menu for bookmarks
    QMenu* bookmarksMenu;
    /// Menu for direct node inspection
    QMenu* inspectNodeMenu;
    /// Menu for direct node inspection before fixpoint
    QMenu* inspectNodeBeforeFPMenu;
    /// Action for activating the preferences menu
    QAction* prefAction;
  protected:
    /// The contained %Gist object
    Gist* c;
    /// A menu bar
    QMenuBar* menuBar;
    /// About dialog
    AboutGist aboutGist;

  protected Q_SLOTS:
    /// The status has changed (e.g., new solutions have been found)
    void statusChanged(const Statistics& stats, bool finished);
    /// Open the about dialog
    void about(void);
    /// Open the preferences dialog
    void preferences(bool setup=false);
    /// Populate the inspector menus from the actions found in Gist
    void populateInspectorSelection(void);
    /// Populate the inspector menus from the actions found in Gist
    void populateInspectors(void);
    /// Populate the bookmarks menus from the actions found in Gist
    void populateBookmarks(void);
  public:
    /// Constructor
    GistMainWindow(Space* root, bool bab, const Options& opt);
  protected:
    /// Close Gist
    void closeEvent(QCloseEvent* event);
  };

}}

#endif


// STATISTICS: gist-any
