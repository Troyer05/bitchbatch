#ifndef BB_UTIL_H
#define BB_UTIL_H

#include <string>
#include <vector>

std::vector<std::string> splitSimple(const std::string& line);
bool startsWith(const std::string& s, const std::string& p);
std::string commonPrefix(const std::vector<std::string>& v);

#endif
