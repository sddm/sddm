#include "KeyboardModel.h"

#define explicit explicit_is_keyword_in_cpp
#include <xcb/xkb.h>
#undef explicit
#include <QDebug>

namespace SDDM {
    class KeyboardModelPrivate {
    public:
        bool enabled { true };

        bool numlock { false };
        bool capslock { false };

        int layout_id;
        QList<QObject*> layouts;

        xcb_connection_t *conn { nullptr };
    };

    class Layout : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString name READ shortName CONSTANT)
        Q_PROPERTY(QString longName READ longName CONSTANT)
    public:
        Layout(QString shortName, QString longName) : m_short(shortName), m_long(longName) {
        }

        ~Layout() = default;

        QString shortName() const {
            return m_short;
        }

        QString longName() const {
            return m_long;
        }

    private:
        QString m_short, m_long;
    };


    KeyboardModel::KeyboardModel() : d(new KeyboardModelPrivate) {
        // Connect and initialize xkb extention
        xcb_xkb_use_extension_cookie_t cookie;
        xcb_generic_error_t *error = nullptr;

        d->conn = xcb_connect(nullptr, nullptr);
        if (d->conn == nullptr) {
            qCritical() << "xcb_connect failed, keyboard extention disabled";
            d->enabled = false;
            return;
        }

        // Initialize xkb extension
        cookie = xcb_xkb_use_extension(d->conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        xcb_xkb_use_extension_reply(d->conn, cookie, &error);

        if (error != nullptr) {
            qCritical() << "xcb_xkb_use_extension failed, keyboard extention disabled, error code"
                        << error->error_code;
            d->enabled = false;
            return;
        }

        // Load leds state
        d->capslock = true;

        // Load layouts
        d->layouts << new Layout("en", "English (US)")
                   << new Layout("ru", "Russian");
    }

    KeyboardModel::~KeyboardModel() {
        for (QObject *layout: d->layouts) {
            delete layout;
        }

        xcb_disconnect(d->conn);
        delete d;
    }

    bool KeyboardModel::numLockState() const {
        return d->numlock;
    }

    void KeyboardModel::setNumLockState(bool state) {
        // TODO
        d->numlock = state;
        emit numLockStateChanged();
    }

    bool KeyboardModel::capsLockState() const {
        return d->capslock;
    }

    void KeyboardModel::setCapsLockState(bool state) {
        // TODO
        d->capslock = state;
        emit capsLockStateChanged();
    }

    QList<QObject*> KeyboardModel::layouts() const {
        return d->layouts;
    }

    int KeyboardModel::currentLayout() const {
        return d->layout_id;
    }

    void KeyboardModel::setCurrentLayout(int id) {
        // TODO
        qDebug() << "set keyboard layouto to: " << id;
        d->layout_id = id;
        emit currentLayoutChanged();
    }

    bool KeyboardModel::enabled() const {
        return d->enabled;
    }

}


#include "KeyboardModel.moc"
