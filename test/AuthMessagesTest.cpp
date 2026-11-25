#include <QtTest/QtTest>

#include "AuthMessages.h"

using namespace SDDM;

class AuthMessagesTest : public QObject {
    Q_OBJECT
private slots:

    void rejectsUnknownMethod() {
        QByteArray bytes;
        QDataStream output(&bytes, QIODevice::WriteOnly);
        output << qint32(2);

        QDataStream input(bytes);
        AuthMethod method = AuthMethod::Password;
        input >> method;
        QCOMPARE(input.status(), QDataStream::ReadCorruptData);
    }
};

QTEST_GUILESS_MAIN(AuthMessagesTest)
#include "AuthMessagesTest.moc"
