#ifndef BB_ENV_H
#define BB_ENV_H

#include <string>

namespace Color {
    extern const std::string RESET;
    extern const std::string RED;
    extern const std::string GREEN;
    extern const std::string YELLOW;
    extern const std::string BLUE;
    extern const std::string CYAN;
    extern const std::string GRAY;
}

std::string getVersion();
std::string getProgrammers();
std::string getUser();
std::string getHost();
std::string getCwd();
std::string prettyPath(const std::string& path, const std::string& user);

#endif
