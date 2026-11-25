// Availability-only private D-Bus fixture; never authenticates or accesses a reader.
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFile>
#include <QTimer>

class Device final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.reactivated.Fprint.Device")
public:
    bool hold = false;
    QList<QDBusMessage> pending;
    QStringList users;
public slots:
    QStringList ListEnrolledFingers(const QString &user) {
        if (hold) {
            setDelayedReply(true);
            pending.append(message());
            users.append(user);
            return {};
        }
        return user == QLatin1String("b") ? QStringList() : QStringList{QStringLiteral("test-finger")};
    }
    void ReleaseReplies() {
        hold = false;
        for (int i = 0; i < pending.size(); ++i) {
            const auto fingers = users.at(i) == QLatin1String("b")
                    ? QStringList() : QStringList{QStringLiteral("test-finger")};
            QDBusConnection::systemBus().send(pending.at(i).createReply(QVariant::fromValue(fingers)));
        }
        pending.clear(); users.clear();
    }
    int PendingReplies() const { return pending.size(); }
};

class Manager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.reactivated.Fprint.Manager")
public slots:
    QList<QDBusObjectPath> GetDevices() const {
        return {QDBusObjectPath(QStringLiteral("/net/reactivated/Fprint/Device/0"))};
    }
    void Quit() { QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit); }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.contains(QStringLiteral("--fail-start"))) return 1;
    qDBusRegisterMetaType<QList<QDBusObjectPath>>();
    Device device;
    device.hold = args.contains(QStringLiteral("--hold-enrollment"));
    Manager manager;
    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) return 2;
    if (!bus.registerObject(QStringLiteral("/net/reactivated/Fprint/Manager"), &manager,
                            QDBusConnection::ExportAllSlots)
        || !bus.registerObject(QStringLiteral("/net/reactivated/Fprint/Device/0"), &device,
                               QDBusConnection::ExportAllSlots)) return 3;
    bus.connect(QString(), QStringLiteral("/org/freedesktop/DBus/Local"),
                QStringLiteral("org.freedesktop.DBus.Local"), QStringLiteral("Disconnected"),
                &app, SLOT(quit()));
    auto acquire = [&] {
        const auto reply = bus.interface()->registerService(QStringLiteral("net.reactivated.Fprint"),
                args.contains(QStringLiteral("--replace")) ? QDBusConnectionInterface::ReplaceExistingService
                                                           : QDBusConnectionInterface::DontQueueService,
                QDBusConnectionInterface::AllowReplacement);
        if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered)
            app.exit(4);
    };
    const int gateIndex = args.indexOf(QStringLiteral("--startup-gate"));
    QTimer gate;
    if (gateIndex >= 0 && gateIndex + 1 < args.size()) {
        const auto path = args.at(gateIndex + 1);
        QFile waiting(path + QStringLiteral(".waiting"));
        if (!waiting.open(QIODevice::WriteOnly)) return 5;
        waiting.close();
        QObject::connect(&gate, &QTimer::timeout, &app, [&, path] {
            if (QFile::exists(path)) { gate.stop(); acquire(); }
        });
        gate.start(10);
    } else QTimer::singleShot(0, &app, acquire);
    return app.exec();
}
#include "FingerprintAvailabilityService.moc"
