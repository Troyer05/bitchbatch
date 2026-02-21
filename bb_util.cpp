#include "bb_util.h"
#include <sstream>

std::vector<std::string> splitSimple(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> out;
    std::string tok;

    while (iss >> tok) out.push_back(tok);

    return out;
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::string commonPrefix(const std::vector<std::string>& v) {
    if (v.empty()) return "";

    std::string p = v[0];

    for (size_t i = 1; i < v.size(); i++) {
        size_t j = 0;

        while (j < p.size() && j < v[i].size() && p[j] == v[i][j]) j++;

        p.resize(j);

        if (p.empty()) break;
    }

    return p;
}
