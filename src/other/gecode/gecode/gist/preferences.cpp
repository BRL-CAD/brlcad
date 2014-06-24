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

#include <gecode/gist/preferences.hh>

namespace Gecode { namespace Gist {

  PreferencesDialog::PreferencesDialog(const Options& opt, QWidget *parent)
  : QDialog(parent) {
    QSettings settings("gecode.org", "Gist");
    hideFailed = settings.value("search/hideFailed", true).toBool();
    zoom = settings.value("search/zoom", false).toBool();
    copies = settings.value("search/copies", false).toBool();
    refresh = settings.value("search/refresh", 500).toInt();
    refreshPause = settings.value("search/refreshPause", 0).toInt();
    smoothScrollAndZoom =
      settings.value("smoothScrollAndZoom", true).toBool();
    moveDuringSearch = false;

    c_d = opt.c_d;
    a_d = opt.a_d;

    hideCheck =
      new QCheckBox(tr("Hide failed subtrees automatically"));
    hideCheck->setChecked(hideFailed);
    zoomCheck =
      new QCheckBox(tr("Automatic zoom enabled on start-up"));
    zoomCheck->setChecked(zoom);
    smoothCheck =
      new QCheckBox(tr("Smooth scrolling and zooming"));
    smoothCheck->setChecked(smoothScrollAndZoom);

    QPushButton* defButton = new QPushButton(tr("Defaults"));
    QPushButton* cancelButton = new QPushButton(tr("Cancel"));
    QPushButton* okButton = new QPushButton(tr("Ok"));
    okButton->setDefault(true);
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(defButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);

    connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
    connect(defButton, SIGNAL(clicked()), this, SLOT(defaults()));
    connect(okButton, SIGNAL(clicked()), this, SLOT(writeBack()));

    QLabel* refreshLabel = new QLabel(tr("Display refresh rate:"));
    refreshBox  = new QSpinBox();
    refreshBox->setRange(0, 1000000);
    refreshBox->setValue(refresh);
    refreshBox->setSingleStep(100);
    QHBoxLayout* refreshLayout = new QHBoxLayout();
    refreshLayout->addWidget(refreshLabel);
    refreshLayout->addWidget(refreshBox);

    slowBox =
      new QCheckBox(tr("Slow down search"));
    slowBox->setChecked(refreshPause > 0);

    refreshBox->setEnabled(refreshPause == 0);

    connect(slowBox, SIGNAL(stateChanged(int)), this,
                     SLOT(toggleSlow(int)));

    moveDuringSearchBox =
      new QCheckBox(tr("Move cursor during search"));
    moveDuringSearchBox->setChecked(moveDuringSearch);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(hideCheck);
    layout->addWidget(zoomCheck);
    layout->addWidget(smoothCheck);
    layout->addLayout(refreshLayout);
    layout->addWidget(slowBox);
    layout->addWidget(moveDuringSearchBox);

    QTabWidget* tabs = new QTabWidget;
    QWidget* page1 = new QWidget;
    page1->setLayout(layout);
    tabs->addTab(page1, "Drawing");

    QLabel* cdlabel = new QLabel(tr("Commit distance:"));
    cdBox  = new QSpinBox();
    cdBox->setRange(0, 10000);
    cdBox->setValue(c_d);
    cdBox->setSingleStep(1);
    QHBoxLayout* cdLayout = new QHBoxLayout();
    cdLayout->addWidget(cdlabel);
    cdLayout->addWidget(cdBox);
    QLabel* adlabel = new QLabel(tr("Adaptive distance:"));
    adBox  = new QSpinBox();
    adBox->setRange(0, 10000);
    adBox->setValue(a_d);
    adBox->setSingleStep(1);
    QHBoxLayout* adLayout = new QHBoxLayout();
    adLayout->addWidget(adlabel);
    adLayout->addWidget(adBox);
    copiesCheck =
      new QCheckBox(tr("Show clones in the tree"));
    copiesCheck->setChecked(copies);
    layout = new QVBoxLayout();
    layout->addLayout(cdLayout);
    layout->addLayout(adLayout);
    layout->addWidget(copiesCheck);
    QWidget* page2 = new QWidget;
    page2->setLayout(layout);
    tabs->addTab(page2, "Search");

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addWidget(tabs);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);

    setWindowTitle(tr("Preferences"));
  }

  void
  PreferencesDialog::writeBack(void) {
    hideFailed = hideCheck->isChecked();
    zoom = zoomCheck->isChecked();
    refresh = refreshBox->value();
    refreshPause = slowBox->isChecked() ? 200 : 0;
    moveDuringSearch = moveDuringSearchBox->isChecked();
    smoothScrollAndZoom = smoothCheck->isChecked();
    copies = copiesCheck->isChecked();
    c_d = cdBox->value();
    a_d = adBox->value();
    QSettings settings("gecode.org", "Gist");
    settings.setValue("search/hideFailed", hideFailed);
    settings.setValue("search/zoom", zoom);
    settings.setValue("search/copies", copies);
    settings.setValue("search/refresh", refresh);
    settings.setValue("search/refreshPause", refreshPause);
    settings.setValue("smoothScrollAndZoom", smoothScrollAndZoom);

    accept();
  }

  void
  PreferencesDialog::defaults(void) {
    hideFailed = true;
    zoom = false;
    refresh = 500;
    refreshPause = 0;
    smoothScrollAndZoom = true;
    moveDuringSearch = false;
    copies = false;
    c_d = 8;
    a_d = 2;
    hideCheck->setChecked(hideFailed);
    zoomCheck->setChecked(zoom);
    refreshBox->setValue(refresh);
    slowBox->setChecked(refreshPause > 0);
    smoothCheck->setChecked(smoothScrollAndZoom);
    copiesCheck->setChecked(copies);
    moveDuringSearchBox->setChecked(moveDuringSearch);
  }

  void
  PreferencesDialog::toggleSlow(int state) {
    refreshBox->setEnabled(state != Qt::Checked);
  }

}}

// STATISTICS: gist-any
