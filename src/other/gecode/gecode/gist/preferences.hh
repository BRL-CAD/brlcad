/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
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

#ifndef GECODE_GIST_PREFERENCES_HH
#define GECODE_GIST_PREFERENCES_HH

#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif
#include <gecode/gist.hh>

namespace Gecode { namespace Gist {

  /**
   * \brief Preferences dialog for %Gist
   */
  class PreferencesDialog : public QDialog {
    Q_OBJECT

  protected:
    QCheckBox* hideCheck;
    QCheckBox* zoomCheck;
    QCheckBox* smoothCheck;
    QCheckBox* copiesCheck;
    QSpinBox*  refreshBox;
    QCheckBox* slowBox;
    QCheckBox* moveDuringSearchBox;
    QSpinBox*  cdBox;
    QSpinBox*  adBox;
  protected Q_SLOTS:
    /// Write settings
    void writeBack(void);
    /// Reset to defaults
    void defaults(void);
    /// Toggle slow down setting
    void toggleSlow(int state);
  public:
    /// Constructor
    PreferencesDialog(const Options& opt, QWidget* parent = 0);

    /// Whether to automatically hide failed subtrees during search
    bool hideFailed;
    /// Whether to automatically zoom during search
    bool zoom;
    /// Whether to show where copies are in the tree
    bool copies;
    /// How often to refresh the display during search
    int refresh;
    /// Milliseconds to wait after each refresh (to slow down search)
    int refreshPause;
    /// Whether to use smooth scrolling and zooming
    bool smoothScrollAndZoom;
    /// Whether to move cursor during search
    bool moveDuringSearch;

    /// The copying distance
    int c_d;
    /// The adaptive recomputation distance
    int a_d;

  };

}}

#endif

// STATISTICS: gist-any
