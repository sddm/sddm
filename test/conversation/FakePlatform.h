#pragma once
#include <QString>
#include <QUuid>
namespace SDDM {
class FixturePower {
public:
 int capabilities()const{return 0;}void powerOff(){}void reboot(){}void suspend(){}void hibernate(){}void hybridSleep(){}
};
class FixtureDaemon {public:FixturePower power;FixturePower*powerManager(){return &power;}QString hostName()const{return QStringLiteral("fixture-host");}};
extern FixtureDaemon *daemonApp;
inline QString generateName(int){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
}
