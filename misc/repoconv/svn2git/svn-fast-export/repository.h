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
    protected:
        Transaction() {}
    public:
        virtual ~Transaction() {}
        virtual void commit() = 0;

        virtual void setAuthor(const QByteArray &author) = 0;
        virtual void setDateTime(uint dt) = 0;
        virtual void setLog(const QByteArray &log) = 0;

        virtual void noteCopyFromBranch (const QString &prevbranch, int revFrom) = 0;

        virtual void deleteFile(const QString &path) = 0;
        virtual QIODevice *addFile(const QString &path, int mode, qint64 length) = 0;

        virtual void commitNote(const QByteArray &noteText, bool append,
                                const QByteArray &commit = QByteArray()) = 0;
    };
    virtual int setupIncremental(int &cutoff) = 0;
    virtual void restoreLog() = 0;
    virtual ~Repository() {}

    virtual void reloadBranches() = 0;
    virtual int createBranch(const QString &branch, int revnum,
                             const QString &branchFrom, int revFrom) = 0;
    virtual int deleteBranch(const QString &branch, int revnum) = 0;
    virtual Repository::Transaction *newTransaction(const QString &branch, const QString &svnprefix, int revnum) = 0;

    virtual void createAnnotatedTag(const QString &name, const QString &svnprefix, int revnum,
                                    const QByteArray &author, uint dt,
                                    const QByteArray &log) = 0;
    virtual void finalizeTags() = 0;
    virtual void commit() = 0;

    static QByteArray formatMetadataMessage(const QByteArray &svnprefix, int revnum,
                                            const QByteArray &tag = QByteArray());

    virtual bool branchExists(const QString& branch) const = 0;
    virtual const QByteArray branchNote(const QString& branch) const = 0;
    virtual void setBranchNote(const QString& branch, const QByteArray& noteText) = 0;

    virtual bool hasPrefix() const = 0;

    virtual QString getName() const = 0;
    virtual Repository *getEffectiveRepository() = 0;
};

Repository *createRepository(const Rules::Repository &rule, const QHash<QString, Repository *> &repositories);

#endif
