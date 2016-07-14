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

#ifndef SVN_H
#define SVN_H

#include <QHash>
#include <QList>
#include "ruleparser.h"

class Repository;

class SvnPrivate;
class Svn
{
public:
    static void initialize();

    Svn(const QString &pathToRepository);
    ~Svn();

    void setMatchRules(const QList<QList<Rules::Match> > &matchRules);
    void setRepositories(const QHash<QString, Repository *> &repositories);
    void setIdentityMap(const QHash<QByteArray, QByteArray> &identityMap);
    void setIdentityDomain(const QString &identityDomain);

    int youngestRevision();
    bool exportRevision(int revnum);

private:
    SvnPrivate * const d;
};

#endif
