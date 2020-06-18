/*
 * This file is part of the vng project
 * Copyright (C) 2008 Thomas Zander <tzander@trolltech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QString>

struct CommandLineOption {
    /**
     * The specification of an option includes the name of the option the user must pass and optional arguments it has.
     * Example specifications are;
     * <ol>
     * <li>"-a, --all" </li>
     * <li>"--version" </li>
     * <li>"--type name" </li>
     * <li>"--list item[,item]" </li>
     * <li>"-f, --format name [suffix] [foo]" </li> </ol>
     * Number 1 allows the user to either type -a or --all (or /A on Windows) to activate this option.
     * Number 2 allows the user to type --version to activate this option.
     * Number 3 requires the user to type a single argument after the option.
     * Number 4 allows the user to use an option that takes a required argument and one or more optional ones
     * Number 5 Allows the user to either use -f or --format, which is followed by one required argument
     *          and optionally 2 more arguments.
     */
    const char *specification;
    /**
     * A textual description of the option that will be printed when the user asks for help.
     */
    const char *description;
};

#define CommandLineLastOption { 0, 0 }

/**
 * The CommandLineParser singleton
 */
class CommandLineParser
{
public:
    static void init(int argc, char *argv[]);
    static void addOptionDefinitions(const CommandLineOption *definitions);
    static void setArgumentDefinition(const char *definition);
    static CommandLineParser *instance();

    ~CommandLineParser();

    void usage(const QString &name, const QString &argumentDescription = QString());

    /// return the options that the user passed
    QStringList options() const;
    /**
     * returns true if the option was found.
     * Consider the following definition "--expert level"  The user can type as an argument
     * "--expert 10".  Calling contains("expert") will return true.
     * @see optionArgument()
     */
    bool contains(const QString & key) const;

    /// returns the list of items that are not options, note that the first one is the name of the command called
    QStringList arguments() const;

    /// return the list of options that the user passed but we don't have a definition for.
    QStringList undefinedOptions() const;

    /**
     * Return the argument passed to an option.
     * Consider the following definition "--expert level"  The user can type as an argument
     * "--expert 10".  Calling optionArgument("expert") will return a string "10"
     * @see contains()
     */
    QString optionArgument(const QString &optionName, const QString &defaultValue = QString()) const;
    QStringList optionArguments(const QString &optionName) const;

    QStringList parseErrors() const;

private:
    CommandLineParser(int argc, char **argv);
    class Private;
    Private * const d;
    static CommandLineParser *self;
};

#endif

