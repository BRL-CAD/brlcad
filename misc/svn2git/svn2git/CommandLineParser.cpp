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

#include "CommandLineParser.h"

#include <QDebug>
#include <QTextStream>
#include <QStringList>
#include <QList>
#include <QHash>

CommandLineParser *CommandLineParser::self = 0;

class CommandLineParser::Private
{
public:
    Private(int argc, char **argv);

    // functions
    void addDefinitions(const CommandLineOption * options);
    void setArgumentDefinition(const char *definition);
    void parse();

    // variables;
    const int argumentCount;
    char ** const argumentStrings;
    bool dirty;
    int requiredArguments;
    QString argumentDefinition;

    struct OptionDefinition {
        OptionDefinition() : optionalParameters(0), requiredParameters(0) { }
        QString name;
        QString comment;
        QChar shortName;
        int optionalParameters;
        int requiredParameters;
    };

    // result of what the user typed
    struct ParsedOption {
        QString option;
        QList<QString> parameters;
    };

    QList<OptionDefinition> definitions;
    QHash<QString, ParsedOption> options;
    QList<QString> arguments;
    QList<QString> undefinedOptions;
    QList<QString> errors;
};


CommandLineParser::Private::Private(int argc, char **argv)
    : argumentCount(argc), argumentStrings(argv), dirty(true),
    requiredArguments(0)
{
}

void CommandLineParser::Private::addDefinitions(const CommandLineOption * options)
{
    for (int i=0; options[i].specification != 0; i++) {
        OptionDefinition definition;
        QString option = QString::fromLatin1(options[i].specification);
        // options with optional params are written as "--option required[, optional]
        if (option.indexOf(QLatin1Char(',')) >= 0 && ( option.indexOf(QLatin1Char('[')) < 0 || option.indexOf(QLatin1Char(']')) < 0) ) {
            QStringList optionParts = option.split(QLatin1Char(','), QString::SkipEmptyParts);
            if (optionParts.count() != 2) {
                qWarning() << "WARN: option definition '" << option << "' is faulty; only one ',' allowed";
                continue;
            }
            foreach (QString s, optionParts) {
                s = s.trimmed();
                if (s.startsWith(QLatin1String("--")) && s.length() > 2)
                    definition.name = s.mid(2);
                else if (s.startsWith(QLatin1String("-")) && s.length() > 1)
                    definition.shortName = s.at(1);
                else {
                    qWarning() << "WARN: option definition '" << option << "' is faulty; the option should start with a -";
                    break;
                }
            }
        }
        else if (option.startsWith(QLatin1String("--")) && option.length() > 2)
            definition.name = option.mid(2);
        else
            qWarning() << "WARN: option definition '" << option << "' has unrecognized format. See the api docs for CommandLineParser for a howto";

        if(definition.name.isEmpty())
            continue;
        if (option.indexOf(QLatin1Char(' ')) > 0) {
            QStringList optionParts = definition.name.split(QLatin1Char(' '), QString::SkipEmptyParts);
            definition.name = optionParts[0];
            bool first = true;
            foreach (QString s, optionParts) {
                if (first) {
                    first = false;
                    continue;
                }
                s = s.trimmed();
                if (s[0].unicode() == '[' && s.endsWith(QLatin1Char(']')))
                    definition.optionalParameters++;
                else
                    definition.requiredParameters++;
            }
        }

        definition.comment = QString::fromLatin1(options[i].description);
        definitions << definition;
    }
/*
    foreach (OptionDefinition def, definitions) {
        qDebug() << "definition:" << (def.shortName != 0 ? def.shortName : QChar(32)) << "|" << def.name << "|" << def.comment
            << "|" << def.requiredParameters << "+" << def.optionalParameters;
    }
*/

    dirty = true;
}

void CommandLineParser::Private::setArgumentDefinition(const char *defs)
{
    requiredArguments = 0;
    argumentDefinition = QString::fromLatin1(defs);
    QStringList optionParts = argumentDefinition.split(QLatin1Char(' '), QString::SkipEmptyParts);
    bool inArg = false;
    foreach (QString s, optionParts) {
        s = s.trimmed();
        if (s[0].unicode() == '<') {
            inArg = true;
            requiredArguments++;
        }
        else if (s[0].unicode() == '[')
            inArg = true;
        if (s.endsWith(QLatin1Char('>')))
            inArg = false;
        else if (!inArg)
            requiredArguments++;
    }
}

void CommandLineParser::Private::parse()
{
    if (dirty == false)
        return;
    errors.clear();
    options.clear();
    arguments.clear();
    undefinedOptions.clear();
    dirty = false;

    class OptionProcessor {
      public:
        OptionProcessor(Private *d) : clp(d) { }

        void next(Private::ParsedOption &option) {
            if (! option.option.isEmpty()) {
                // find the definition to match.
                OptionDefinition def;
                foreach (Private::OptionDefinition definition, clp->definitions) {
                    if (definition.name == option.option) {
                        def = definition;
                        break;
                    }
                }
                if (! def.name.isEmpty() && def.requiredParameters >= option.parameters.count() &&
                        def.requiredParameters + def.optionalParameters <= option.parameters.count())
                    clp->options.insert(option.option, option);
                else if (!clp->undefinedOptions.contains(option.option))
                    clp->undefinedOptions << option.option;
                else
                    clp->errors.append(QLatin1String("Not enough arguments passed for option `")
                            + option.option +QLatin1Char('\''));
            }
            option.option.clear();
            option.parameters.clear();
        }

      private:
        CommandLineParser::Private *clp;
    };
    OptionProcessor processor(this);

    bool optionsAllowed = true;
    ParsedOption option;
    OptionDefinition currentDefinition;
    for (int i = 1; i < argumentCount; i++) {
        QString arg = QString::fromLocal8Bit(argumentStrings[i]);
        if (optionsAllowed) {
            if (arg == QLatin1String("--")) {
                optionsAllowed = false;
                continue;
            }
            if (arg.startsWith(QLatin1String("--"))) {
                processor.next(option);
                int end = arg.indexOf(QLatin1Char('='));
                option.option = arg.mid(2, end - 2);
                if (end > 0)
                    option.parameters << arg.mid(end+1);
                continue;
            }
            if (arg[0].unicode() == '-' && arg.length() > 1) {
                for (int x = 1; x < arg.length(); x++) {
                    bool resolved = false;
                    foreach (OptionDefinition definition, definitions) {
                        if (definition.shortName == arg[x]) {
                            resolved = true;
                            processor.next(option);
                            currentDefinition = definition;
                            option.option = definition.name;

                            if (definition.requiredParameters == 1 && arg.length() >= x+2) {
                                option.parameters << arg.mid(x+1, arg.length());
                                x = arg.length();
                            }
                            break;
                        }
                    }
                    if (!resolved) { // nothing found; copy char so it ends up in unrecognized
                        option.option = arg[x];
                        processor.next(option);
                    }
                }
                continue;
            }
        }
        if (! option.option.isEmpty()) {
            if (currentDefinition.name != option.option) {
                // find the definition to match.
                foreach (OptionDefinition definition, definitions) {
                    if (definition.name == option.option) {
                        currentDefinition = definition;
                        break;
                    }
                }
            }
            if (currentDefinition.requiredParameters + currentDefinition.optionalParameters <= option.parameters.count())
                processor.next(option);
        }
        if (option.option.isEmpty())
            arguments << arg;
        else
            option.parameters << arg;
    }
    processor.next(option);

    if (requiredArguments > arguments.count())
        errors.append(QLatin1String("Not enough arguments, usage: ") + QString::fromLocal8Bit(argumentStrings[0])
                + QLatin1Char(' ') + argumentDefinition);

/*
    foreach (QString key, options.keys()) {
        ParsedOption p = options[key];
        qDebug() << "-> " << p.option;
        foreach (QString v, p.parameters)
            qDebug() << "   +" << v;
    }
    qDebug() << "---";
    foreach (QString arg, arguments) {
        qDebug() << arg;
    }
*/
}

// -----------------------------------


// static
void CommandLineParser::init(int argc, char **argv)
{
    if (self)
        delete self;
    self = new CommandLineParser(argc, argv);
}

// static
void CommandLineParser::addOptionDefinitions(const CommandLineOption * optionList)
{
    if (!self) {
        qWarning() << "WARN: CommandLineParser:: Use init before addOptionDefinitions!";
        return;
    }
    self->d->addDefinitions(optionList);
}

// static
CommandLineParser *CommandLineParser::instance()
{
    return self;
}

// static
void CommandLineParser::setArgumentDefinition(const char *definition)
{
    if (!self) {
        qWarning() << "WARN: CommandLineParser:: Use init before addOptionDefinitions!";
        return;
    }
    self->d->setArgumentDefinition(definition);
}


CommandLineParser::CommandLineParser(int argc, char **argv)
    : d(new Private(argc, argv))
{
}

CommandLineParser::~CommandLineParser()
{
    delete d;
}

void CommandLineParser::usage(const QString &name, const QString &argumentDescription)
{
    QTextStream cout(stdout, QIODevice::WriteOnly);
    cout << "Usage: " << d->argumentStrings[0];
    if (! name.isEmpty())
        cout << " " << name;
    if (d->definitions.count())
        cout << " [OPTION]";
    if (! argumentDescription.isEmpty())
        cout << " " << argumentDescription;
    cout << endl << endl;

    if (d->definitions.count() > 0)
        cout << "Options:" << endl;
    int commandLength = 0;
    foreach (Private::OptionDefinition definition, d->definitions)
        commandLength = qMax(definition.name.length(), commandLength);

    foreach (Private::OptionDefinition definition, d->definitions) {
        cout << "    ";
        if (definition.shortName == 0)
            cout << "   --";
        else
            cout << "-" << definition.shortName << " --";
        cout << definition.name;
        for (int i = definition.name.length(); i <= commandLength; i++)
            cout << ' ';
         cout << definition.comment <<endl;
    }
}

QStringList CommandLineParser::options() const
{
    d->parse();
    return d->options.keys();
}

bool CommandLineParser::contains(const QString & key) const
{
    d->parse();
    return d->options.contains(key);
}

QStringList CommandLineParser::arguments() const
{
    d->parse();
    return d->arguments;
}

QStringList CommandLineParser::undefinedOptions() const
{
    d->parse();
    return d->undefinedOptions;
}

QString CommandLineParser::optionArgument(const QString &optionName, const QString &defaultValue) const
{
    QStringList answer = optionArguments(optionName);
    if (answer.isEmpty())
        return defaultValue;
    return answer.first();
}

QStringList CommandLineParser::optionArguments(const QString &optionName) const
{
    if (! contains(optionName))
        return QStringList();
    Private::ParsedOption po = d->options[optionName];
    return po.parameters;
}

QStringList CommandLineParser::parseErrors() const
{
    d->parse();
    return d->errors;
}
