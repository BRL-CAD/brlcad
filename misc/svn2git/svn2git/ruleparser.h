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

#ifndef RULEPARSER_H
#define RULEPARSER_H

#include <QList>
#include <QMap>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QStringBuilder>

class Rules
{
public:
    struct Rule
    {
        QString filename;
        int lineNumber;
        Rule() : lineNumber(0) {}
    };
    struct Repository : Rule
    {
        struct Branch
        {
            QString name;
        };

        QString name;
        QList<Branch> branches;
        QString description;

        QString forwardTo;
        QString prefix;

        Repository() { }
        const QString info() const {
            const QString info = Rule::filename % ":" % QByteArray::number(Rule::lineNumber);
            return info;
        }

    };

    struct Match : Rule
    {
        struct Substitution {
            QRegExp pattern;
            QString replacement;

            bool isValid() { return !pattern.isEmpty(); }
            QString& apply(QString &string) { return string.replace(pattern, replacement); }
        };

        QRegExp rx;
        QString repository;
        QList<Substitution> repo_substs;
        QString branch;
        QList<Substitution> branch_substs;
        QString prefix;
        int minRevision;
        int maxRevision;
        bool annotate;

        enum Action {
            Ignore,
            Export,
            Recurse
        } action;

        Match() : minRevision(-1), maxRevision(-1), annotate(false), action(Ignore) { }
        const QString info() const {
            const QString info = Rule::filename % ":" % QByteArray::number(Rule::lineNumber) % " " % rx.pattern();
            return info;
        }
    };

    Rules(const QString &filename);
    ~Rules();

    const QList<Repository> repositories() const;
    const QList<Match> matchRules() const;
    Match::Substitution parseSubstitution(const QString &string);
    void load();

private:
    void load(const QString &filename);
    QString filename;
    QList<Repository> m_repositories;
    QList<Match> m_matchRules;
    QMap<QString,QString> m_variables;
};

class RulesList
{
public:
  RulesList( const QString &filenames);
  ~RulesList();

  const QList<Rules::Repository> allRepositories() const;
  const QList<QList<Rules::Match> > allMatchRules() const;
  const QList<Rules*> rules() const;
  void load();

private:
  QString m_filenames;
  QList<Rules*> m_rules;
  QList<Rules::Repository> m_allrepositories;
  QList<QList<Rules::Match> > m_allMatchRules;
};

class Stats
{
public:
    static Stats *instance();
    void printStats() const;
    void ruleMatched(const Rules::Match &rule, const int rev = -1);
    void addRule( const Rules::Match &rule);
    static void init();
    ~Stats();

private:
    Stats();
    class Private;
    Private * const d;
    static Stats *self;
    bool use;
};

#ifndef QT_NO_DEBUG_STREAM
class QDebug;
QDebug operator<<(QDebug, const Rules::Match &);
#endif

#endif
