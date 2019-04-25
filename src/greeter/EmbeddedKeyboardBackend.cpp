// elena

#include "EmbeddedKeyboardBackend.h"

#include <QDir>
#include <QtPlatformHeaders/QEglFSFunctions>
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
#include <QtPlatformHeaders/QLinuxFbFunctions>
#endif

#include "Configuration.h"
#include "KeyboardModel_p.h"
#include "KeyboardLayout.h"

namespace SDDM {
    EmbeddedKeyboardBackend::EmbeddedKeyboardBackend(KeyboardModelPrivate *kmp) : KeyboardBackend(kmp), currKeymap(QString()) {
    }

    EmbeddedKeyboardBackend::~EmbeddedKeyboardBackend() {
    }

    void EmbeddedKeyboardBackend::init() {
        QDir dir(mainConfig.Embedded.KeymapDir.get());

        QStringList filters;
        filters<<QLatin1String("*.qmap");
        dir.setNameFilters(filters);

        QFileInfoList keymapFiles=dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for(int i=0; i<keymapFiles.size(); ++i)
            keymaps.insert(keymapFiles[i].baseName(), keymapFiles[i].absoluteFilePath());
    }

    void EmbeddedKeyboardBackend::disconnect() {
    }

    void EmbeddedKeyboardBackend::sendChanges() {
        QString longName=static_cast<KeyboardLayout*>(d->layouts[d->layout_id])->longName();
        if(currKeymap==longName)
            return;

        KeymapList::const_iterator it=keymaps.find(longName);
        if(it!=keymaps.end()) {
            if(mainConfig.Embedded.Platform.get()==QLatin1String("eglfs")) {
                QEglFSFunctions::loadKeymap(it.value());
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
                if(longName!=QLatin1String("en_GB"))
                    QEglFSFunctions::switchLang();
#endif
            }
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
            else if(mainConfig.Embedded.Platform.get()==QLatin1String("linuxfb")) {
                QLinuxFbFunctions::loadKeymap(it.value());
                if(longName!=QLatin1String("en_GB"))
                    QLinuxFbFunctions::switchLang();
            }
#endif
        }

        currKeymap=longName;
    }

    void EmbeddedKeyboardBackend::dispatchEvents() {
    }

    void EmbeddedKeyboardBackend::connectEventsDispatcher(KeyboardModel *model) {
    }
}
