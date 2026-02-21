#include "bb_search.h"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <string>
#include <cctype>

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return std::tolower(c); });

    return r;
}

void searchRecursive(const std::string& root, const std::string& pattern) {
    namespace fs = std::filesystem;

    std::string p = toLower(pattern);

    for (auto& entry : fs::recursive_directory_iterator(root,
            fs::directory_options::skip_permission_denied)) {

        if (g_sigint) {
            g_sigint = 0;
            return;
        }

        std::string name = entry.path().filename().string();

        if (toLower(name).find(p) != std::string::npos) {
            std::cout << entry.path().string() << "\n";
        }
    }
}
