#include "bb_env.h"

#include <unistd.h>
#include <limits.h>
#include <cstdlib>

namespace Color {
    const std::string RESET = "\033[0m";
    const std::string RED   = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW= "\033[33m";
    const std::string BLUE  = "\033[34m";
    const std::string CYAN  = "\033[36m";
}

std::string getUser() {
    const char* u = getenv("USER");
    if (u && *u) return u;
    return "user";
}

std::string getHost() {
    char buf[256];

    if (gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf)-1] = '\0';
        return buf;
    }

    return "machine";
}

std::string getCwd() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != nullptr) return buf;
    return "?";
}

std::string prettyPath(const std::string& path, const std::string& user) {
    std::string home = "/home/" + user;

    if (path == home) return "~";
    if (path.rfind(home + "/", 0) == 0) return "~" + path.substr(home.size());

    return path;
}
