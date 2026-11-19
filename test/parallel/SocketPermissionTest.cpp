// SPDX-License-Identifier: GPL-2.0-or-later
#include "Auth.h"
#include <QCoreApplication>
#include <QDir>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <grp.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
using namespace SDDM;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    umask(0);
    Auth auth;
    QDir temp(QDir::tempPath());
    const QStringList names = temp.entryList({QStringLiteral("sddm-auth-*")}, QDir::System);
    if (names.size() != 1) return 1;
    struct stat st = {};
    const auto path = temp.filePath(names.first()).toLocal8Bit();
    if (lstat(path.constData(), &st) != 0 || !S_ISSOCK(st.st_mode) || (st.st_mode & 0077) != 0) return 2;
    if (geteuid() == 0) {
        const pid_t child = fork();
        if (child < 0) return 3;
        if (child == 0) {
            if (setgroups(0, nullptr) || setgid(65534) || setuid(65534)) _exit(4);
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            struct sockaddr_un addr = {};
            addr.sun_family = AF_UNIX;
            if (path.size() >= int(sizeof(addr.sun_path))) _exit(5);
            std::memcpy(addr.sun_path, path.constData(), path.size() + 1);
            const int result = connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
            _exit(result < 0 && errno == EACCES ? 0 : 6);
        }
        int status = 0;
        if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return 7;
        std::puts("unrelated mapped UID was denied");
    } else {
        std::puts("socket mode verified; distinct UID requires mapped-root test");
    }
    return 0;
}
