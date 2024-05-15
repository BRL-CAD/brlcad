/*                     B R L M A N  . C P P
 * BRL-CAD
 *
 * Copyright (c) 2005-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brlman.cpp
 *
 *  Man page viewer for BRL-CAD man pages.
 *
 */

#include "common.h"

#include <string.h>
#include <ctype.h> /* for isdigit */
#include "bresource.h"

#include "bnetwork.h"
#include "bio.h"

#if !defined(USE_QT) && defined(HAVE_TK)
#  include "tcl.h"
#  include "tk.h"
#endif

#ifdef USE_QT
#include <QApplication>
#include <QObject>
#include <QFile>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QSplitter>
#include <QDialogButtonBox>
#include "qtcad/QgAccordion.h"
#endif

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"
#if !defined(USE_QT) && defined(HAVE_TK)
#  include "tclcad/setup.h"
#endif

/* Confine the constraining build conditions here - ultimately, we care
 * about graphical and non-graphical, whatever the reasons. */
#if defined(HAVE_TK) || defined(USE_QT)
#  define MAN_GUI 1
#endif
#ifndef HAVE_WINDOWS_H
#  define MAN_CMDLINE 1
#endif

/* Supported man sections */

static char *
find_man_file(const char *man_name, const char *lang, char section, int gui)
{
    const char *ddir;
    char *ret = NULL;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mfile = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	ddir = bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	ddir = bu_dir(NULL, 0, BU_DIR_MAN, NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    }
    bu_vls_sprintf(&mfile, "%s/%s/man%c/%s.%s", bu_vls_addr(&data_dir), lang, section, man_name, bu_vls_addr(&file_ext));
    if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	ret = bu_strdup(bu_vls_addr(&mfile));
    } else {
	bu_vls_sprintf(&mfile, "%s/man%c/%s.%s", bu_vls_addr(&data_dir), section, man_name, bu_vls_addr(&file_ext));
	if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	    ret = bu_strdup(bu_vls_addr(&mfile));
	}
    }
    bu_vls_free(&data_dir);
    bu_vls_free(&file_ext);
    bu_vls_free(&mfile);
    return ret;
}


#if !defined(USE_QT) && defined(HAVE_TK)
// Tk based GUI
int
tk_man_gui(const char *man_name, char man_section, const char *man_file)
{
    const char *result;
    const char *fullname;
    const char *tcl_man_file;
    const char *brlman_tcl = NULL;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    Tcl_DString temp;
    Tcl_Interp *interp = Tcl_CreateInterp();
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

#ifdef HAVE_WINDOWS_H
    Tk_InitConsoleChannels(interp);
#endif

    int status = tclcad_init(interp, 1, &tlog);

    if (status == TCL_ERROR) {
	bu_log("brlman tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	bu_vls_free(&tlog);
	bu_exit(1, "tcl init error");
    }
    bu_vls_free(&tlog);

    /* Have gui flag and specified man page but no file found - graphical failure notice */
    if (man_name && !man_file) {
	if (man_section != '\0') {
	    bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s in section %c\" -type ok", man_name, man_section);
	} else {
	    bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s\" -type ok", man_name);
	}
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	bu_exit(EXIT_FAILURE, NULL);
    }

    /* Pass the key variables into the interp */
    if (man_section != '\0') {
	bu_vls_sprintf(&tcl_cmd, "set ::section_number %c", man_section);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (man_file) {
	Tcl_Obj *pathP, *norm;

	bu_vls_sprintf(&tcl_cmd, "set ::man_name %s", man_name);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

	pathP = Tcl_NewStringObj(man_file, -1);
	norm = Tcl_FSGetTranslatedPath(interp, pathP);
	tcl_man_file = Tcl_GetString(norm);
	bu_vls_sprintf(&tcl_cmd, "set ::man_file %s", tcl_man_file);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    } else {
	bu_vls_sprintf(&tcl_cmd, "set ::data_dir %s/html", bu_dir(NULL, 0, BU_DIR_DOC, NULL));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    brlman_tcl = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "brlman", "brlman.tcl", NULL);
    Tcl_DStringInit(&temp);
    fullname = Tcl_TranslateFileName(interp, brlman_tcl, &temp);
    status = Tcl_EvalFile(interp, fullname);
    Tcl_DStringFree(&temp);

    result = Tcl_GetStringResult(interp);
    if (strlen(result) > 0 && status == TCL_ERROR) {
	bu_log("%s\n", result);
    }
    Tcl_DeleteInterp(interp);

    return BRLCAD_OK;
}
#endif

#ifdef USE_QT

static size_t
list_man_files(QListWidget *l, const char *lang, char section, int gui)
{
    const char *ddir;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mpattern = BU_VLS_INIT_ZERO;
    struct bu_vls mdir = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	ddir = bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	ddir = bu_dir(NULL, 0, BU_DIR_MAN, NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    }

    bu_vls_sprintf(&mdir, "%s/%s/man%c", bu_vls_addr(&data_dir), lang, section);
    if (!bu_file_directory(bu_vls_addr(&mdir))) {
	bu_vls_sprintf(&mdir, "%s/man%c", bu_vls_addr(&data_dir), section);
	if (!bu_file_directory(bu_vls_addr(&mdir))) {
	    return 0;
	}
    }

    bu_vls_sprintf(&mpattern, "*.%s", bu_vls_cstr(&file_ext));

    char **mfiles = NULL;
    size_t mcnt = bu_file_list(bu_vls_cstr(&mdir), bu_vls_cstr(&mpattern), &mfiles);

    bu_vls_free(&data_dir);
    bu_vls_free(&mpattern);
    bu_vls_free(&file_ext);
    bu_vls_free(&mdir);

    if (mcnt) {
	for (size_t i = 0; i < mcnt; i++) {
	    struct bu_vls mname = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&mname, mfiles[i], BU_PATH_BASENAME_EXTLESS)) {
		new QListWidgetItem(bu_vls_cstr(&mname), l);
	    }
	    bu_vls_free(&mname);
	}
    }

    return mcnt;
}

class ManViewer : public QDialog
{
    public:
	ManViewer(QWidget *p = NULL, const char *man_name = NULL, char man_section = '0', const char *lang = NULL);
	~ManViewer();

	void do_select(const char *man_name, char man_section);

	QListWidget *l1;
	QListWidget *l3;
	QListWidget *l5;
	QListWidget *ln;

    public slots:
        void do_man1(QListWidgetItem *i, QListWidgetItem *p);
        void do_man3(QListWidgetItem *i, QListWidgetItem *p);
        void do_man5(QListWidgetItem *i, QListWidgetItem *p);
        void do_mann(QListWidgetItem *i, QListWidgetItem *p);

    private:
	struct bu_vls mlang = BU_VLS_INIT_ZERO;
	QgAccordion *lists;
	QTextBrowser *browser;
	QDialogButtonBox *buttons;
};

void
ManViewer::do_man1(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '1');
}

void
ManViewer::do_man3(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '3');
}

void
ManViewer::do_man5(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '5');
}

void
ManViewer::do_mann(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, 'n');
}

void
ManViewer::do_select(const char *man_name, char man_section)
{
    const char *man_file = find_man_file(man_name, bu_vls_cstr(&mlang), man_section, 1);
    if (!man_file)
	return;

    struct bu_vls title = BU_VLS_INIT_ZERO;
    if (man_name) {
	bu_vls_sprintf(&title, "%s (%c)", man_name, man_section);
    } else {
	bu_vls_sprintf(&title, "BRL-CAD Manual Pages");
    }
    this->setWindowTitle(QString(bu_vls_cstr(&title)));
    bu_vls_free(&title);

    QString filename(man_file);
    QFile manfile(filename);
    if (manfile.open(QFile::ReadOnly | QFile::Text)) {
	browser->setHtml(manfile.readAll());
    }
}

ManViewer::ManViewer(QWidget *pparent, const char *man_name, char man_section, const char *lang) : QDialog(pparent)
{
    bu_vls_sprintf(&mlang, "%s", lang);

    QVBoxLayout *l = new QVBoxLayout;
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &ManViewer::accept);

    QSplitter *s = new QSplitter(Qt::Horizontal, this);
    lists = new QgAccordion();
    s->addWidget(lists);

    l1 = new QListWidget(this);
    list_man_files(l1, bu_vls_cstr(&mlang), '1', 1);
    l1->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a1 = new QgAccordionObject(lists, l1, "Programs (man1)");
    lists->addObject(a1);
    QObject::connect(l1, &QListWidget::currentItemChanged, this, &ManViewer::do_man1);

    l3 = new QListWidget(this);
    list_man_files(l3, bu_vls_cstr(&mlang), '3', 1);
    l3->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a3 = new QgAccordionObject(lists, l3, "Libraries (man3)");
    lists->addObject(a3);
    QObject::connect(l3, &QListWidget::currentItemChanged, this, &ManViewer::do_man3);

    l5 = new QListWidget(this);
    list_man_files(l5, bu_vls_cstr(&mlang), '5', 1);
    l5->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a5 = new QgAccordionObject(lists, l5, "Conventions (man5)");
    lists->addObject(a5);
    QObject::connect(l5, &QListWidget::currentItemChanged, this, &ManViewer::do_man5);

    ln = new QListWidget(this);
    list_man_files(ln, bu_vls_cstr(&mlang), 'n', 1);
    ln->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *an = new QgAccordionObject(lists, ln, "GED (mann)");
    lists->addObject(an);
    QObject::connect(ln, &QListWidget::currentItemChanged, this, &ManViewer::do_mann);

    switch (man_section) {
	case '1':
	    emit a1->select(a1);
	    break;
	case '3':
	    emit a3->select(a3);
	    break;
	case '5':
	    emit a5->select(a5);
	    break;
	case 'n':
	    emit an->select(an);
	    break;
	default:
	    emit a1->select(a1);
	    break;
    }

    browser = new QTextBrowser();

    if (!man_name) {
	do_select("Introduction", 'n');
    } else {
	do_select(man_name, man_section);
    }

    s->addWidget(browser);

    s->setStretchFactor(0, 0);
    s->setStretchFactor(1, 100000);

    l->addWidget(s);

    l->addWidget(buttons);
    setLayout(l);


    struct bu_vls title = BU_VLS_INIT_ZERO;
    if (man_name) {
	bu_vls_sprintf(&title, "%s (%c)", man_name, man_section);
    } else {
	bu_vls_sprintf(&title, "BRL-CAD Manual Pages");
    }
    this->setWindowTitle(QString(bu_vls_cstr(&title)));
    bu_vls_free(&title);

    // TODO - find something smarter to do for size
    setMinimumWidth(800);
    setMinimumHeight(600);

    setWindowModality(Qt::NonModal);
}

ManViewer::~ManViewer()
{
    delete browser;
}


int
qt_man_gui(const char *man_name, char man_section, const char *UNUSED(man_file))
{
    int ac = 0;
    char **av = NULL;
    QApplication brlman(ac, av);
    ManViewer *v = new ManViewer(NULL, man_name, man_section);
    v->show();
    brlman.exec();
    return BRLCAD_OK;
}
#endif

#ifndef HAVE_WINDOWS_H
#  define APIENTRY
#  define BRLMAN_MAIN main
#else
#  define BRLMAN_MAIN WinMain
#endif

/* main() */
int APIENTRY
BRLMAN_MAIN(
#ifdef HAVE_WINDOWS_H
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow
#else
    int argc,
    const char **argv
#endif
    )
{

#if !defined(MAN_CMDLINE) && !defined(MAN_GUI)
    bu_exit(EXIT_FAILURE, "Error: man page display is not supported.");
#endif

    int i = 0;
    int status = BRLCAD_ERROR;
    int uac = 0;
#ifndef MAN_CMDLINE
    int enable_gui = 1;
#else
    int enable_gui = 0;
#endif
    int disable_gui = 0;
    int print_help = 0;
    const char *man_cmd = NULL;
    const char *man_name = NULL;
    const char *man_file = NULL;
    char man_section = '\0';
    struct bu_vls lang = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[6];

#ifdef HAVE_WINDOWS_H
    const char **argv;
    int argc;

    /* Get our args from the c-runtime. Ignore lpszCmdLine. */
    argc = __argc;
    argv = (const char **)__argv;
#endif

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);

    /* Handle options in C */
    BU_OPT(d[0], "h", "help",        "",                NULL, &print_help,  "Print help and exit");
    BU_OPT(d[1], "g", "gui",         "",                NULL, &enable_gui,  "Enable GUI");
    BU_OPT(d[2], "",  "no-gui",      "",                NULL, &disable_gui, "Disable GUI");
    BU_OPT(d[3], "L", "language",  "lg",        &bu_opt_lang, &lang,        "Set language");
    BU_OPT(d[4], "S", "section",    "#", &bu_opt_man_section, &man_section, "Set section");
    BU_OPT_NULL(d[5]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, argv, d);
    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    /* If we want help, print help */
    if (print_help) {
	char *option_help = bu_opt_describe(d, NULL);
	bu_log("Usage: brlman [options] [man_page]\n");
	if (option_help) {
	    bu_log("Options:\n%s\n", option_help);
	}
	bu_free(option_help, "help str");
	bu_exit(EXIT_SUCCESS, NULL);
    }

    /* If we only have one non-option arg, assume it's the man name */
    if (uac == 1) {
	man_name = argv[0];
    } else {
	/* If we've got more, check for a section number. */
	for (i = 0; i < uac; i++) {
	    if (strlen(argv[i]) == 1 && (isdigit(argv[i][0]) || argv[i][0] == 'n')) {

		/* Record the section */
		man_section = argv[i][0];

		/* If we have a section identifier and it's not already the last
		 * element in argv, adjust the array */
		if (i < uac - 1) {
		    const char *tmp = argv[uac-1];
		    argv[uac-1] = argv[i];
		    argv[i] = tmp;
		}

		/* Found a number and processed - one less argv entry. */
		uac--;

		/* First number wins. */
		break;
	    }
	}

	/* For now, only support specifying one man page at a time */
	if (uac > 1) {
	    bu_exit(EXIT_FAILURE, "Error - need a single man page name");
	}

	/* If we have one, use it.  Zero is allowed if we're in graphical
	 * mode, so uac == at this point isn't necessarily a failure */
	if (uac == 1) {
	    man_name = argv[0];
	}
    }

    if (enable_gui && disable_gui) {
#if defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform supports both, enabling GUI.\n");
	disable_gui = 0;
#endif
#if !defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports command line, disabling GUI.\n");
#endif
#if defined(MAN_GUI) && !defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports GUI viewer, enabling GUI.\n");
#endif
    }

    /* If we're not graphical and we don't have a name at this point, we're done. */
    if (!enable_gui && !man_name) {
	bu_exit(EXIT_FAILURE, "Error: Please specify man page");
    }

    /* If we're not graphical, make sure we have a man command to use */
    if (!enable_gui) {
#ifndef MAN_CMDLINE
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#else
	man_cmd = bu_which("man");
	if (!man_cmd) {
	    if (disable_gui) {
		bu_exit(EXIT_FAILURE, "Error: Graphical man display is disabled, but no man command is available.");
	    } else {
		/* Don't have any specific instructions - fall back on gui */
		bu_log("Warning: Graphical mode was not specified, but no man command is available - enabling GUI.");
		enable_gui = 1;
	    }
	}
#endif
    }

    /* If we don't have a language, check environment */
    if (!bu_vls_strlen(&lang)) {
	char *env_lang = getenv("LANG");
	if (env_lang && strlen(env_lang) >= 2) {
	    /* Grab the first two characters */
	    bu_vls_strncat(&lang, env_lang, 2);
	}
    }

    /* If we *still* don't have a language, use en */
    if (!bu_vls_strlen(&lang)) {
	bu_vls_sprintf(&lang, "en");
    }

    /* Now we have a lang and name - find the file */

    if (man_section != '\0') {
	man_file = find_man_file(man_name, bu_vls_addr(&lang), man_section, enable_gui);
    } else {
	const char sections[] = BRLCAD_MAN_SECTIONS;
	i = 0;
	while(sections[i] != '\0') {
	    man_file = find_man_file(man_name, bu_vls_addr(&lang), sections[i], enable_gui);
	    if (man_file) {
		man_section = sections[i];
		break;
	    }
	    i++;
	}
    }

    if (!enable_gui) {
#ifdef MAN_CMDLINE
	if (!man_file) {
	    if (man_section != '\0') {
		bu_exit(EXIT_FAILURE, "No man page found for %s in section %c\n", man_name, man_section);
	    } else {
		bu_exit(EXIT_FAILURE, "No man page found for %s\n", man_name);
	    }
	}
	/* Note - for reasons that are unclear, passing in man_cmd as the first
	 * argument to execlp will *not* work... */
	status = execlp("man", man_cmd, man_file, (char *)NULL);
	return status;
#else
	/* Shouldn't get here - should be caught by above tests */
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#endif
    }

#ifdef MAN_GUI
#  if !defined(USE_QT) && defined(HAVE_TK)
    return tk_man_gui(man_name, man_section, man_file);
#  else
    return qt_man_gui(man_name, man_section, man_file);
#  endif
#else
    /* Shouldn't get here - should be caught by above tests */
    bu_exit(EXIT_FAILURE, "Error: graphical man display is not supported.");
#endif
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

