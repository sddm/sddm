// elena

#ifndef EMBEDDEDKEYBOARDBACKEND_H
#define EMBEDDEDKEYBOARDBACKEND_H

#include "KeyboardBackend.h"

#include <QMap>

namespace SDDM {
    class EmbeddedKeyboardBackend : public KeyboardBackend {
        public:
            EmbeddedKeyboardBackend(KeyboardModelPrivate *kmp);
            virtual ~EmbeddedKeyboardBackend();

            void init() override;
            void disconnect() override;
            void sendChanges() override;
            void dispatchEvents() override;

            void connectEventsDispatcher(KeyboardModel *model) override;

        private:
            typedef QMap<QString, QString> KeymapList;
            KeymapList keymaps;
            QString currKeymap;
    };
}

#endif // EMBEDDEDKEYBOARDBACKEND_H