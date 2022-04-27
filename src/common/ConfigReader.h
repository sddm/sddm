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

#ifndef CONFIGREADER_H
#define CONFIGREADER_H

#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QDir>

#define IMPLICIT_SECTION "General"
#define UNUSED_VARIABLE_COMMENT "# Unused variable"
#define UNUSED_SECTION_COMMENT "### These sections and their variables were not used: ###\n"

///// convenience macros
// efficient qstring initializer
#define _S(x) QStringLiteral(x)

// config wrapper
#define Config(name, file, dir, sysDir, ...) \
    class name : public SDDM::ConfigBase, public SDDM::ConfigSection { \
    public: \
        name() : SDDM::ConfigBase(file, dir, sysDir), SDDM::ConfigSection(this, QStringLiteral(IMPLICIT_SECTION)) { \
            load(); \
        } \
        void save() { SDDM::ConfigBase::save(nullptr, nullptr); } \
        void save(SDDM::ConfigEntryBase *) const = delete; \
        QString toConfigFull() const { \
            return SDDM::ConfigBase::toConfigFull(); \
        } \
        __VA_ARGS__ \
    }
// entry wrapper
#define Entry(name, type, default, description, ...) \
    SDDM::ConfigEntry<type> name { this, QStringLiteral(#name), default, description, __VA_ARGS__ }
// section wrapper
#define Section(name, ...) \
    class name : public SDDM::ConfigSection { \
    public: \
        name (SDDM::ConfigBase *_parent, const QString &_name) : SDDM::ConfigSection(_parent, _name) { } \
        __VA_ARGS__ \
    } name { this, QStringLiteral(#name) };

QTextStream &operator>>(QTextStream &str, QStringList &list);
QTextStream &operator<<(QTextStream &str, const QStringList &list);
QTextStream &operator>>(QTextStream &str, bool &val);
QTextStream &operator<<(QTextStream &str, const bool &val);

namespace SDDM {
    template<class> class ConfigEntry;
    class ConfigSection;
    class ConfigBase;

    class ConfigEntryBase {
    public:
        virtual const QString &name() const = 0;
        virtual QString value() const = 0;
        virtual void setValue(const QString &str) = 0;
        virtual QString toConfigShort() const = 0;
        virtual QString toConfigFull() const = 0;
        virtual bool matchesDefault() const = 0;
        virtual bool isDefault() const = 0;
        virtual bool setDefault() = 0;
    };

    class ConfigSection {
    public:
        ConfigSection(ConfigBase *parent, const QString &name);
        ConfigEntryBase *entry(const QString &name);
        const ConfigEntryBase *entry(const QString &name) const;
        void save(ConfigEntryBase *entry);
        void clear();
        const QString &name() const;
        QString toConfigShort() const;
        QString toConfigFull() const;
        const QMap<QString, ConfigEntryBase*> &entries() const;
    private:
        template<class T> friend class ConfigEntryPrivate;
        QMap<QString, ConfigEntryBase*> m_entries {};

        ConfigBase *m_parent { nullptr };
        QString m_name { };
        template<class T> friend class ConfigEntry;
    };

    template <class T>
    class ConfigEntry : public ConfigEntryBase {
    public:
        ConfigEntry(ConfigSection *parent, const QString &name, const T &value, const QString &description) : ConfigEntryBase(),
            m_name(name),
            m_description(description),
            m_default(value),
            m_value(value),
            m_isDefault(true),
            m_parent(parent) {
            m_parent->m_entries[name] = this;
        }

        T get() const {
            return m_value;
        }

        void set(const T val) {
            m_value = val;
            m_isDefault = false;
        }

        bool matchesDefault() const {
            return m_value == m_default;
        }

        bool isDefault() const {
            return m_isDefault;
        }

        bool setDefault() {
            m_isDefault = true;
            if (m_value == m_default)
                return false;
            m_value = m_default;
            return true;
        }

        void save() {
            m_parent->save(this);
        }

        const QString &name() const {
            return m_name;
        }

        QString value() const {
            QString str;
            QTextStream out(&str);
            out << m_value;
            return str;
        }

        // specialised for QString
        void setValue(const QString &str) {
            m_isDefault = false;
            QTextStream in(qPrintable(str));
            in >> m_value;
        }

        QString toConfigShort() const {
            return QStringLiteral("%1=%2").arg(m_name).arg(value());
        }

        QString toConfigFull() const {
            QString str;
            for (const QString &line : m_description.split(QLatin1Char('\n')))
                str.append(QStringLiteral("# %1\n").arg(line));
            str.append(QStringLiteral("%1=%2\n\n").arg(m_name).arg(value()));
            return str;
        }
    private:
        const QString m_name;
        const QString m_description;
        T m_default;
        T m_value;
        bool m_isDefault;
        ConfigSection *m_parent;
    };

    // Base has to be separate from the Config itself - order of initialization
    class ConfigBase {
    public:
        ConfigBase(const QString &configPath, const QString &configDir=QString(), const QString &sysConfigDir=QString());

        void load();
        void save(const ConfigSection *section = nullptr, const ConfigEntryBase *entry = nullptr);
        void wipe();
        bool hasUnused() const;
        QString toConfigFull() const;
    protected:
        bool m_unusedVariables { false };
        bool m_unusedSections { false };

        QString m_path {};
        QString m_configDir;
        QString m_sysConfigDir;
        QMap<QString, ConfigSection*> m_sections;
        friend class ConfigSection;
    private:
        QDateTime dirLatestModifiedTime(const QString &directory);
        void loadInternal(const QString &filepath);
        QDateTime m_fileModificationTime;
    };
}


#endif // CONFIGREADER_H
