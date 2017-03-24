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

#include <QTextStream>
#include <QList>
#include <QFile>
#include <QDebug>

#include "ruleparser.h"
#include "CommandLineParser.h"

RulesList::RulesList(const QString &filenames)
  : m_filenames(filenames)
{
}

RulesList::~RulesList() {}

void RulesList::load()
{
    foreach(const QString filename, m_filenames.split(',') ) {
        qDebug() << "Loading rules from:" << filename;
        Rules *rules = new Rules(filename);
        m_rules.append(rules);
        rules->load();
        m_allrepositories.append(rules->repositories());
        QList<Rules::Match> matchRules = rules->matchRules();
        m_allMatchRules.append( QList<Rules::Match>(matchRules));
    }
}

const QList<Rules::Repository> RulesList::allRepositories() const
{
  return m_allrepositories;
}

const QList<QList<Rules::Match> > RulesList::allMatchRules() const
{
  return m_allMatchRules;
}

const QList<Rules*> RulesList::rules() const
{
  return m_rules;
}

Rules::Rules(const QString &fn)
    : filename(fn)
{
}

Rules::~Rules()
{
}

const QList<Rules::Repository> Rules::repositories() const
{
    return m_repositories;
}

const QList<Rules::Match> Rules::matchRules() const
{
    return m_matchRules;
}

Rules::Match::Substitution Rules::parseSubstitution(const QString &string)
{
    if (string.at(0) != 's' || string.length() < 5)
        return Match::Substitution();

    const QChar sep = string.at(1);

    if (string.at(string.length() - 1) != sep)
        return Match::Substitution();

    int i = 2, end = 0;
    Match::Substitution subst;

    // Separator might have been escaped with a backslash
    while (i > end) {
        int backslashCount = 0;
        if ((end = string.indexOf(sep, i)) > -1) {
            for (i = end - 1; i >= 2; i--) {
                if (string.at(i) == '\\')
                    backslashCount++;
                else
                    break;
            }
        } else {
            return Match::Substitution(); // error
        }

        if (backslashCount % 2 != 0) {
            // Separator was escaped. Search for another one
            i = end + 1;
        }
    }

    // Found the end of the pattern
    subst.pattern = QRegExp(string.mid(2, end - 2));
    if (!subst.pattern.isValid())
        return Match::Substitution(); // error
    subst.replacement = string.mid(end + 1, string.length() - 1 - end - 1);

    return subst;
}

void Rules::load()
{
    load(filename);
}

void Rules::load(const QString &filename)
{
    qDebug() << "Loading rules from" << filename;
    // initialize the regexps we will use
    QRegExp repoLine("create repository\\s+(\\S+)", Qt::CaseInsensitive);

    QString varRegex("[A-Za-z0-9_]+");

    QRegExp matchLine("match\\s+(.*)", Qt::CaseInsensitive);
    QRegExp matchActionLine("action\\s+(\\w+)", Qt::CaseInsensitive);
    QRegExp matchRepoLine("repository\\s+(\\S+)", Qt::CaseInsensitive);
    QRegExp matchDescLine("description\\s+(.+)$", Qt::CaseInsensitive);
    QRegExp matchRepoSubstLine("substitute repository\\s+(.+)$", Qt::CaseInsensitive);
    QRegExp matchBranchLine("branch\\s+(\\S+)", Qt::CaseInsensitive);
    QRegExp matchBranchSubstLine("substitute branch\\s+(.+)$", Qt::CaseInsensitive);
    QRegExp matchRevLine("(min|max) revision (\\d+)", Qt::CaseInsensitive);
    QRegExp matchAnnotateLine("annotated\\s+(\\S+)", Qt::CaseInsensitive);
    QRegExp matchPrefixLine("prefix\\s+(.*)$", Qt::CaseInsensitive);
    QRegExp declareLine("declare\\s+("+varRegex+")\\s*=\\s*(\\S+)", Qt::CaseInsensitive);
    QRegExp variableLine("\\$\\{("+varRegex+")(\\|[^}$]*)?\\}", Qt::CaseInsensitive);
    QRegExp includeLine("include\\s+(.*)", Qt::CaseInsensitive);

    enum { ReadingNone, ReadingRepository, ReadingMatch } state = ReadingNone;
    Repository repo;
    Match match;
    int lineNumber = 0;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Could not read the rules file: %s", qPrintable(filename));

    QTextStream s(&file);
    QStringList lines = s.readAll().split('\n', QString::KeepEmptyParts);

    QStringList::iterator it;
    for(it = lines.begin(); it != lines.end(); ++it) {
        ++lineNumber;
        QString origLine = *it;
        QString line = origLine;

        int hash = line.indexOf('#');
        if (hash != -1)
            line.truncate(hash);
        line = line.trimmed();
        if (line.isEmpty())
            continue;

        bool isIncludeRule = includeLine.exactMatch(line);
        if (isIncludeRule) {
            int index = filename.lastIndexOf("/");
            QString includeFile = filename.left( index + 1) + includeLine.cap(1);
            load(includeFile);
        } else {
            while( variableLine.indexIn(line) != -1 ) {
                QString replacement;
                if (m_variables.contains(variableLine.cap(1))) {
                    replacement = m_variables[variableLine.cap(1)];
                } else {
                    if (variableLine.cap(2).startsWith('|')) {
                        replacement = variableLine.cap(2).mid(1);
                    } else {
                        qFatal("Undeclared variable: %s", qPrintable(variableLine.cap(1)));
                    }
                }
                line = line.replace(variableLine.cap(0), replacement);
            }
            if (state == ReadingRepository) {
                if (matchBranchLine.exactMatch(line)) {
                    Repository::Branch branch;
                    branch.name = matchBranchLine.cap(1);

                    repo.branches += branch;
                    continue;
                } else if (matchDescLine.exactMatch(line)) {
                    repo.description = matchDescLine.cap(1);
                    continue;
                } else if (matchRepoLine.exactMatch(line)) {
                    repo.forwardTo = matchRepoLine.cap(1);
                    continue;
                } else if (matchPrefixLine.exactMatch(line)) {
                    repo.prefix = matchPrefixLine.cap(1);
                    continue;
                } else if (line == "end repository") {
                    if (!repo.forwardTo.isEmpty()
                        && !repo.description.isEmpty()) {

                        qFatal("Specifing repository and description on repository is invalid on line %d", lineNumber);
                    }

                    if (!repo.forwardTo.isEmpty()
                        && !repo.branches.isEmpty()) {

                        qFatal("Specifing repository and branches on repository is invalid on line %d", lineNumber);
                    }

                    m_repositories += repo;
                    {
                        // clear out 'repo'
                        Repository temp;
                        std::swap(repo, temp);
                    }
                    state = ReadingNone;
                    continue;
                }
            } else if (state == ReadingMatch) {
                if (matchRepoLine.exactMatch(line)) {
                    match.repository = matchRepoLine.cap(1);
                    continue;
                } else if (matchBranchLine.exactMatch(line)) {
                    match.branch = matchBranchLine.cap(1);
                    continue;
                } else if (matchRepoSubstLine.exactMatch(line)) {
                    Match::Substitution subst = parseSubstitution(matchRepoSubstLine.cap(1));
                    if (!subst.isValid()) {
                        qFatal("Malformed substitution in rules file: line %d: %s",
                            lineNumber, qPrintable(origLine));
                    }
                    match.repo_substs += subst;
                    continue;
                } else if (matchBranchSubstLine.exactMatch(line)) {
                    Match::Substitution subst = parseSubstitution(matchBranchSubstLine.cap(1));
                    if (!subst.isValid()) {
                        qFatal("Malformed substitution in rules file: line %d: %s",
                            lineNumber, qPrintable(origLine));
                    }
                    match.branch_substs += subst;
                    continue;
                } else if (matchRevLine.exactMatch(line)) {
                    if (matchRevLine.cap(1) == "min")
                        match.minRevision = matchRevLine.cap(2).toInt();
                    else            // must be max
                        match.maxRevision = matchRevLine.cap(2).toInt();
                    continue;
                } else if (matchPrefixLine.exactMatch(line)) {
                    match.prefix = matchPrefixLine.cap(1);
                    if( match.prefix.startsWith('/'))
                        match.prefix = match.prefix.mid(1);
                    continue;
                } else if (matchActionLine.exactMatch(line)) {
                    QString action = matchActionLine.cap(1);
                    if (action == "export")
                        match.action = Match::Export;
                    else if (action == "ignore")
                        match.action = Match::Ignore;
                    else if (action == "recurse")
                        match.action = Match::Recurse;
                    else
                        qFatal("Invalid action \"%s\" on line %d", qPrintable(action), lineNumber);
                    continue;
                } else if (matchAnnotateLine.exactMatch(line)) {
                    match.annotate = matchAnnotateLine.cap(1) == "true";
                    continue;
                } else if (line == "end match") {
                    if (!match.repository.isEmpty())
                        match.action = Match::Export;
                    m_matchRules += match;
                    Stats::instance()->addRule(match);
                    state = ReadingNone;
                    continue;
                }
            }

            bool isRepositoryRule = repoLine.exactMatch(line);
            bool isMatchRule = matchLine.exactMatch(line);
            bool isVariableRule = declareLine.exactMatch(line);

            if (isRepositoryRule) {
                // repository rule
                state = ReadingRepository;
                repo = Repository(); // clear
                repo.name = repoLine.cap(1);
                repo.lineNumber = lineNumber;
                repo.filename = filename;
            } else if (isMatchRule) {
                // match rule
                state = ReadingMatch;
                match = Match();
                match.rx = QRegExp(matchLine.cap(1), Qt::CaseSensitive, QRegExp::RegExp2);
                if( !match.rx.isValid() )
                    qFatal("Malformed regular expression '%s' in file:'%s':%d, Error: %s",
                           qPrintable(matchLine.cap(1)), qPrintable(filename), lineNumber,
                           qPrintable(match.rx.errorString()));
                match.lineNumber = lineNumber;
                match.filename = filename;
            } else if (isVariableRule) {
                QString variable = declareLine.cap(1);
                QString value = declareLine.cap(2);
                m_variables.insert(variable, value);
            } else {
                qFatal("Malformed line in rules file: line %d: %s",
                       lineNumber, qPrintable(origLine));
            }
        }
    }
}

Stats *Stats::self = 0;

class Stats::Private
{
public:
    Private();

    void printStats() const;
    void ruleMatched(const Rules::Match &rule, const int rev);
    void addRule(const Rules::Match &rule);
private:
    QMap<Rules::Match,int> m_usedRules;
};

Stats::Stats() : d(new Private())
{
    use = CommandLineParser::instance()->contains("stats");
}

Stats::~Stats()
{
    delete d;
}

void Stats::init()
{
    if(self)
        delete self;
    self = new Stats();
}

Stats* Stats::instance()
{
    return self;
}

void Stats::printStats() const
{
    if(use)
        d->printStats();
}

void Stats::ruleMatched(const Rules::Match &rule, const int rev)
{
    if(use)
        d->ruleMatched(rule, rev);
}

void Stats::addRule( const Rules::Match &rule)
{
    if(use)
        d->addRule(rule);
}

Stats::Private::Private()
{
}

void Stats::Private::printStats() const
{
    printf("\nRule stats\n");
    foreach(const Rules::Match rule, m_usedRules.keys()) {
        printf("%s was matched %i times\n", qPrintable(rule.info()), m_usedRules[rule]);
    }
}

void Stats::Private::ruleMatched(const Rules::Match &rule, const int rev)
{
    Q_UNUSED(rev);
    if(!m_usedRules.contains(rule)) {
        m_usedRules.insert(rule, 1);
        qWarning() << "WARN: New match rule" << rule.info() << ", should have been added when created.";
    } else {
        m_usedRules[rule]++;
    }
}

void Stats::Private::addRule( const Rules::Match &rule)
{
    if(m_usedRules.contains(rule))
        qWarning() << "WARN: Rule" << rule.info() << "was added multiple times.";
    m_usedRules.insert(rule, 0);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug s, const Rules::Match &rule)
{
    s.nospace() << rule.info();
    return s.space();
}

#endif
