// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Messages.h"
#include "Session.h"
#include <QDataStream>
namespace SDDM {
struct LoginRequest {
    GreeterMessages kind = GreeterMessages::Connect;
    QString user;
    QString password;
    Session session;
};
inline bool readLoginRequest(QDataStream &stream, LoginRequest &result) {
    stream.startTransaction();
    quint32 kind = 0;
    LoginRequest request;
    stream >> kind;
    request.kind = GreeterMessages(kind);
    if (request.kind == GreeterMessages::Login)
        stream >> request.user >> request.password >> request.session;
    else if (request.kind == GreeterMessages::StartFingerprint)
        stream >> request.user >> request.session;
    if (!stream.commitTransaction()) return false;
    result = request;
    return true;
}
struct LoginReply {
    DaemonMessages kind = DaemonMessages::HostName;
    QString text;
    quint32 capabilities = 0;
    bool active = false;
};
inline bool readLoginReply(QDataStream &stream, LoginReply &result) {
    stream.startTransaction();
    quint32 kind = 0;
    LoginReply reply;
    stream >> kind;
    reply.kind = DaemonMessages(kind);
    if (reply.kind == DaemonMessages::HostName || reply.kind == DaemonMessages::InformationMessage)
        stream >> reply.text;
    else if (reply.kind == DaemonMessages::Capabilities)
        stream >> reply.capabilities;
    else if (reply.kind == DaemonMessages::ParallelFingerprint || reply.kind == DaemonMessages::FingerprintActive)
        stream >> reply.active;
    if (!stream.commitTransaction()) return false;
    result = reply;
    return true;
}
}
