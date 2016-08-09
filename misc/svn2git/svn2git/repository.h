/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <QHash>
#include <QProcess>
#include <QVector>
#include <QFile>

#include "ruleparser.h"
#include "CommandLineParser.h"

class LoggingQProcess : public QProcess
{
    QFile log;
    bool logging;
public:
    LoggingQProcess(const QString filename) : QProcess(), log() {
        if(CommandLineParser::instance()->contains("debug-rules")) {
            logging = true;
            QString name = filename;
            name.replace('/', '_');
            name.prepend("gitlog-");
            log.setFileName(name);
            log.open(QIODevice::WriteOnly);
        } else {
            logging = false;
        }
    };
    ~LoggingQProcess() {
        if(logging) {
            log.close();
        }
    };

    qint64 write(const char *data) {
        Q_ASSERT(state() == QProcess::Running);
        if(logging) {
            log.write(data);
        }
        return QProcess::write(data);
    }
    qint64 write(const char *data, qint64 length) {
        Q_ASSERT(state() == QProcess::Running);
        if(logging) {
            log.write(data);
        }
        return QProcess::write(data, length);
    }
    qint64 write(const QByteArray &data) {
        Q_ASSERT(state() == QProcess::Running);
        if(logging) {
            log.write(data);
        }
        return QProcess::write(data);
    }
    qint64 writeNoLog(const char *data) {
        Q_ASSERT(state() == QProcess::Running);
        return QProcess::write(data);
    }
    qint64 writeNoLog(const char *data, qint64 length) {
        Q_ASSERT(state() == QProcess::Running);
        return QProcess::write(data, length);
    }
    qint64 writeNoLog(const QByteArray &data) {
        Q_ASSERT(state() == QProcess::Running);
        return QProcess::write(data);
    }
    bool putChar( char c) {
        Q_ASSERT(state() == QProcess::Running);
        if(logging) {
            log.putChar(c);
        }
        return QProcess::putChar(c);
    }
};


class Repository
{
public:
    class Transaction
    {
        Q_DISABLE_COPY(Transaction)
        friend class Repository;

        Repository *repository;
        QByteArray branch;
        QByteArray svnprefix;
        QByteArray author;
        QByteArray log;
        uint datetime;
        int revnum;

        QVector<int> merges;

        QStringList deletedFiles;
        QByteArray modifiedFiles;

        inline Transaction() {}
    public:
        ~Transaction();
        void commit();

        void setAuthor(const QByteArray &author);
        void setDateTime(uint dt);
        void setLog(const QByteArray &log);

        void noteCopyFromBranch (const QString &prevbranch, int revFrom);

        void deleteFile(const QString &path);
        QIODevice *addFile(const QString &path, int mode, qint64 length);

        void commitNote(const QByteArray &noteText, bool append,
                        const QByteArray &commit = QByteArray());
    };
    Repository(const Rules::Repository &rule);
    int setupIncremental(int &cutoff);
    void restoreLog();
    ~Repository();

    void reloadBranches();
    int createBranch(const QString &branch, int revnum,
                     const QString &branchFrom, int revFrom);
    int deleteBranch(const QString &branch, int revnum);
    Repository::Transaction *newTransaction(const QString &branch, const QString &svnprefix, int revnum);

    void createAnnotatedTag(const QString &name, const QString &svnprefix, int revnum,
                            const QByteArray &author, uint dt,
                            const QByteArray &log);
    void finalizeTags();
    void commit();

    static QByteArray formatMetadataMessage(const QByteArray &svnprefix, int revnum,
                                            const QByteArray &tag = QByteArray());

    bool branchExists(const QString& branch) const;
    const QByteArray branchNote(const QString& branch) const;
    void setBranchNote(const QString& branch, const QByteArray& noteText);

private:
    struct Branch
    {
        int created;
        QVector<int> commits;
        QVector<int> marks;
        QByteArray note;
    };
    struct AnnotatedTag
    {
        QString supportingRef;
        QByteArray svnprefix;
        QByteArray author;
        QByteArray log;
        uint dt;
        int revnum;
    };

    QHash<QString, Branch> branches;
    QHash<QString, AnnotatedTag> annotatedTags;
    QString name;
    QString prefix;
    LoggingQProcess fastImport;
    int commitCount;
    int outstandingTransactions;
    QByteArray deletedBranches;
    QByteArray resetBranches;

    /* starts at 0, and counts up.  */
    int last_commit_mark;

    /* starts at maxMark and counts down. Reset after each SVN revision */
    int next_file_mark;

    bool processHasStarted;

    void startFastImport();
    void closeFastImport();

    // called when a transaction is deleted
    void forgetTransaction(Transaction *t);

    int resetBranch(const QString &branch, int revnum, int mark, const QByteArray &resetTo, const QByteArray &comment);
    int markFrom(const QString &branchFrom, int branchRevNum, QByteArray &desc);

    friend class ProcessCache;
    Q_DISABLE_COPY(Repository)
};
#endif
