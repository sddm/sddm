/*
 * INI Configuration parser classes
 * Copyright (C) 2014 Martin Bříza <mbriza@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include "ConfigReader.h"
#include <QtCore/QFile>

#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QMap>
#include <QtCore/QBuffer>


namespace SDDM {
    // has to be specialised because QTextStream reads only words into a QString
    template <> void ConfigEntry<QString>::setValue(const QString &str) {
        m_value = str.trimmed();
    }


    ConfigSection::ConfigSection(ConfigBase *parent, const QString &name) : m_parent(parent),
        m_name(name) {
        m_parent->m_sections.insert(name, this);
    }

    ConfigEntryBase *ConfigSection::entry(const QString &name) {
        if (m_entries.contains(name))
            return m_entries[name];
        return nullptr;
    }

    const ConfigEntryBase *ConfigSection::entry(const QString &name) const {
        if (m_entries.contains(name))
            return m_entries[name];
        return nullptr;
    }

    const QMap<QString, ConfigEntryBase*> &ConfigSection::entries() const {
        return m_entries;
    }


    const QString &ConfigSection::name() const {
        return m_name;
    }

    void ConfigSection::save(ConfigEntryBase *entry) {
        m_parent->save(this, entry);
    }

    QString ConfigSection::toConfigFull() const {
        QString final = QString("[%1]\n").arg(m_name);
        for (const ConfigEntryBase *entry : m_entries)
            final.append(entry->toConfigFull());
        return final;
    }

    QString ConfigSection::toConfigShort() const {
        return QString("[%1]").arg(name());
    }



    ConfigBase::ConfigBase(const QString &configPath) : m_path(configPath) {
        // so far it's safe to assume we want to load everything on Initialization
        load();
    }

    const QString &ConfigBase::path() const {
        return m_path;
    }

    bool ConfigBase::hasUnused() const {
        return m_unusedSections || m_unusedVariables;
    }

    void ConfigBase::load() {
        // first check if there's at least anything to read, otherwise stick to default values
        if (!QFile::exists(m_path))
            return;

        QString currentSection = IMPLICIT_SECTION;

        QFile in(m_path);
        in.open(QIODevice::ReadOnly);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            // get rid of comments first
            line = line.left(line.indexOf('#')).trimmed();

            // value assignment
            int separatorPosition = line.indexOf('=');
            if (separatorPosition >= 0) {
                QString name = line.left(separatorPosition).trimmed();
                QString value = line.mid(separatorPosition + 1).trimmed();

                if (m_sections.contains(currentSection) && m_sections[currentSection]->entry(name))
                    m_sections[currentSection]->entry(name)->setValue(value);
                else
                    // if we don't have such member in the config, nag about it
                    m_unusedVariables = true;
            }
            // section start
            else if (line.startsWith('[') && line.endsWith(']'))
                currentSection = line.mid(1, line.length() - 2);
        }
    }

    void ConfigBase::save(const ConfigSection *section, const ConfigEntryBase *entry) {
        // to know if we should overwrite the config or not
        bool changed = false;
        // stores the order of the loaded sections
        // every one could be there only once - if it occurs more times in the config, the occurences are merged
        QList<const ConfigSection*> sectionOrder;
        // the actual bytearray data for every section
        QMap<const ConfigSection*, QByteArray> sectionData;
        // map of nondefault entries which should be saved if they are not found in the current config file
        QMultiMap<const ConfigSection*, const ConfigEntryBase*> remainingEntries;


        /*
         * Initialization of the map of nondefault values to be saved
         */
        if (section) {
            if (entry && !entry->isDefault())
                remainingEntries.insert(section, entry);
            else
                for (const ConfigEntryBase *b : section->entries().values())
                    if (!b->isDefault())
                        remainingEntries.insert(section, b);
        }
        else {
            for (const ConfigSection *s : m_sections)
                for (const ConfigEntryBase *b : s->entries().values())
                    if (!b->isDefault())
                        remainingEntries.insert(s, b);
        }

        // initialize the current section - General, usually
        const ConfigSection *currentSection = m_sections[IMPLICIT_SECTION];

        // stuff to store the pre-section stuff (comments) to the start of the right section, not the end of the previous one
        QByteArray junk;
        // stores the junk to the temporary storage
        auto collectJunk = [&junk](const QString &data) {
            junk.append(data);
        };

        // a short function to assign the current junk and current line to the right section, eventually create a new one
        auto writeSectionData = [currentSection, &junk, &sectionOrder, &sectionData](const QString &data) {
            if (currentSection && !sectionOrder.contains(currentSection)) {
                sectionOrder.append(currentSection);
                sectionData[currentSection] = QByteArray();
            }
            sectionData[currentSection].append(junk);
            sectionData[currentSection].append(data);
            junk.clear();
        };

        // loading and checking phase
        QFile file(m_path);
        file.open(QIODevice::ReadOnly); // first just for reading
        while (!file.atEnd()) {
            QString line = file.readLine();
            // get rid of comments first
            QString trimmedLine = line.left(line.indexOf('#')).trimmed();
            QString comment;
            if (line.indexOf('#') >= 0)
                comment = line.mid(line.indexOf('#')).trimmed();

            // value assignment
            int separatorPosition = trimmedLine.indexOf('=');
            if (separatorPosition >= 0) {
                QString name = trimmedLine.left(separatorPosition).trimmed();
                QString value = trimmedLine.mid(separatorPosition + 1).trimmed();

                if (currentSection && currentSection->entry(name)) {
                    // this monstrous condition checks the parameters if only one entry/section should be saved
                    if ((entry && section && section->name() == currentSection->name() && entry->name() == name) ||
                        (!entry && section && section->name() == currentSection->name()) ||
                        value != currentSection->entry(name)->value()) {
                        changed = true;
                        writeSectionData(QString("%1=%2 %3\n").arg(name).arg(currentSection->entry(name)->value()).arg(comment));
                    }
                    else
                        writeSectionData(line);
                    remainingEntries.remove(currentSection, currentSection->entry(name));
                }
                else {
                    if (currentSection)
                        m_unusedVariables = true;
                    writeSectionData(QString("%1 %2\n").arg(trimmedLine).arg(UNUSED_VARIABLE_COMMENT));
                }
            }

            // section start
            else if (trimmedLine.startsWith('[') && trimmedLine.endsWith(']')) {
                QString name = trimmedLine.mid(1, trimmedLine.length() - 2);
                if (m_sections.contains(name)) {
                    currentSection = m_sections[name];
                    if (!sectionOrder.contains(currentSection))
                        writeSectionData(line);
                }
                else {
                    m_unusedSections = true;
                    currentSection = nullptr;
                    writeSectionData(line);
                }
            }

            // other stuff, like comments and whatnot
            else {
                if (line != UNUSED_SECTION_COMMENT)
                    collectJunk(line);
            }
        }
        file.close();

        for (auto it = remainingEntries.begin(); it != remainingEntries.end(); it++) {
            changed = true;
            currentSection = it.key();
            if (!sectionOrder.contains(currentSection))
                writeSectionData(currentSection->toConfigShort());
            writeSectionData("\n");
            writeSectionData(it.value()->toConfigFull());
        }

        // rewrite the whole thing only if there are changes
        if (changed) {
            file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            for (const ConfigSection *s : sectionOrder)
                file.write(sectionData[s]);

            if (sectionData.contains(nullptr)) {
                file.write("\n");
                file.write(UNUSED_SECTION_COMMENT);
                file.write(sectionData[nullptr].trimmed());
                file.write("\n");
            }
        }
    }


    QTextStream &operator>>(QTextStream &str, QStringList &list)  {
        QStringList tempList = str.readLine().split(",");
        foreach(const QString &s, tempList)
            if (!s.trimmed().isEmpty())
                list.append(s.trimmed());
        return str;
    }

    QTextStream &operator<<(QTextStream &str, const QStringList &list) {
        str << list.join(",");
        return str;
    }

    QTextStream &operator>>(QTextStream &str, bool &val) {
        if (0 == str.readLine().trimmed().compare("true", Qt::CaseInsensitive))
            val = true;
        else
            val = false;
        return str;
    }

    QTextStream &operator<<(QTextStream &str, const bool &val) {
        if (val)
            str << "true";
        else
            str << "false";
        return str;
    }
}
