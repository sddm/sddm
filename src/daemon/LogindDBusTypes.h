#ifndef LOGIND_DBUSTYPES
#define LOGIND_DBUSTYPES

#include <QDBusObjectPath>
#include <QDBusArgument>

struct Logind
{
    static bool isAvailable();
    static QString serviceName();
    static QString managerPath();
    static QString managerIfaceName();
    static QString sessionIfaceName();
    static QString seatIfaceName();
    static QString userIfaceName();
};

struct ScheduledShutdownInfo
{
    QString type;
    quint64 microseconds;
};

typedef QList<ScheduledShutdownInfo> ScheduledShutdownInfoList;

inline QDBusArgument &operator<<(QDBusArgument &argument, const ScheduledShutdownInfo& scheduledShutdownInfo)
{
    argument.beginStructure();
    argument << scheduledShutdownInfo.type;
    argument << scheduledShutdownInfo.microseconds;
    argument.endStructure();

    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, ScheduledShutdownInfo &scheduledShutdownInfo)
{
    argument.beginStructure();
    argument >> scheduledShutdownInfo.type;
    argument >> scheduledShutdownInfo.microseconds;
    argument.endStructure();

    return argument;
}

Q_DECLARE_METATYPE(ScheduledShutdownInfo);
Q_DECLARE_METATYPE(QList<ScheduledShutdownInfo>);

/*  <arg type="a(sv)" name="properties" direction="in"/>
  <annotion name="org.qtproject.QtDBus.QtTypeName.In0" value="CreateSessionPropertiesInfoList"/>
*/

struct CreateSessionPropertiesInfo
{
    QString key;
    QVariantMap value;
};

typedef QList<CreateSessionPropertiesInfo> CreateSessionPropertiesInfoList;

inline QDBusArgument &operator<<(QDBusArgument &argument, const CreateSessionPropertiesInfo& createSessionPropertiesInfo)
{
    argument.beginStructure();
    argument << createSessionPropertiesInfo.key;
    argument << createSessionPropertiesInfo.value;
    argument.endStructure();

    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, CreateSessionPropertiesInfo &createSessionPropertiesInfo)
{
    argument.beginStructure();
    argument >> createSessionPropertiesInfo.key;
    argument >> createSessionPropertiesInfo.value;
    argument.endStructure();

    return argument;
}

Q_DECLARE_METATYPE(CreateSessionPropertiesInfo);
Q_DECLARE_METATYPE(QList<CreateSessionPropertiesInfo>);

struct SessionInfo
{
    QString sessionId;
    uint userId;
    QString userName;
    QString seatId;
    QDBusObjectPath sessionPath;
};

typedef QList<SessionInfo> SessionInfoList;

inline QDBusArgument &operator<<(QDBusArgument &argument, const SessionInfo& sessionInfo)
{
    argument.beginStructure();
    argument << sessionInfo.sessionId;
    argument << sessionInfo.userId;
    argument << sessionInfo.userName;
    argument << sessionInfo.seatId;
    argument << sessionInfo.sessionPath;
    argument.endStructure();

    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SessionInfo &sessionInfo)
{
    argument.beginStructure();
    argument >> sessionInfo.sessionId;
    argument >> sessionInfo.userId;
    argument >> sessionInfo.userName;
    argument >> sessionInfo.seatId;
    argument >> sessionInfo.sessionPath;
    argument.endStructure();

    return argument;
}

struct UserInfo
{
    uint userId;
    QString name;
    QDBusObjectPath path;
};

typedef QList<UserInfo> UserInfoList;

inline QDBusArgument &operator<<(QDBusArgument &argument, const UserInfo& userInfo)
{
    argument.beginStructure();
    argument << userInfo.userId;
    argument << userInfo.name;
    argument << userInfo.path;
    argument.endStructure();

    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, UserInfo& userInfo)
{
    argument.beginStructure();
    argument >> userInfo.userId;
    argument >> userInfo.name;
    argument >> userInfo.path;
    argument.endStructure();

    return argument;
}

struct NamedSeatPath
{
    QString name;
    QDBusObjectPath path;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const NamedSeatPath& namedSeat)
{
    argument.beginStructure();
    argument << namedSeat.name;
    argument << namedSeat.path;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, NamedSeatPath& namedSeat)
{
    argument.beginStructure();
    argument >> namedSeat.name;
    argument >> namedSeat.path;
    argument.endStructure();
    return argument;
}

typedef QList<NamedSeatPath> NamedSeatPathList;

typedef NamedSeatPath NamedSessionPath;
typedef NamedSeatPathList NamedSessionPathList;

class NamedUserPath
{
public:
    uint userId;
    QDBusObjectPath path;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const NamedUserPath& namedUser)
{
    argument.beginStructure();
    argument << namedUser.userId;
    argument << namedUser.path;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, NamedUserPath& namedUser)
{
    argument.beginStructure();
    argument >> namedUser.userId;
    argument >> namedUser.path;
    argument.endStructure();
    return argument;
}

class Inhibitor
{
public:
    QString what;
    QString who;
    QString why;
    QString mode;
    int userId;
    uint processId;
};

typedef QList<Inhibitor> InhibitorList;

inline QDBusArgument &operator<<(QDBusArgument &argument, const Inhibitor& inhibitor)
{
    argument.beginStructure();
    argument << inhibitor.what;
    argument << inhibitor.who;
    argument << inhibitor.why;
    argument << inhibitor.mode;
    argument << inhibitor.userId;
    argument << inhibitor.processId;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Inhibitor& inhibitor)
{
    argument.beginStructure();
    argument >> inhibitor.what;
    argument >> inhibitor.who;
    argument >> inhibitor.why;
    argument >> inhibitor.mode;
    argument >> inhibitor.userId;
    argument >> inhibitor.processId;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(SessionInfo);
Q_DECLARE_METATYPE(QList<SessionInfo>);

Q_DECLARE_METATYPE(UserInfo);
Q_DECLARE_METATYPE(QList<UserInfo>);

Q_DECLARE_METATYPE(NamedSeatPath);
Q_DECLARE_METATYPE(QList<NamedSeatPath>);

Q_DECLARE_METATYPE(NamedUserPath);

Q_DECLARE_METATYPE(Inhibitor);
Q_DECLARE_METATYPE(QList<Inhibitor>);

#endif
