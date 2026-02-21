#include "bb_search.h"
#include "bb_signal.h"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <csignal>

static std::string toLower(const std::string& s) {
    std::string r = s;

    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return std::tolower(c); });

    return r;
}

static bool isSkipRoot(const std::filesystem::path& p) {
    static const std::vector<std::string> skip = {
        "/proc", "/sys", "/dev", "/run"
    };

    std::string s = p.string();

    for (auto& k : skip) {
        if (s == k) return true;
        if (s.size() > k.size() && s.rfind(k + "/", 0) == 0) return true; // startswith
    }

    return false;
}

void searchRecursive(const std::string& root, const std::string& pattern) {
    namespace fs = std::filesystem;

    fs::path start(root);

    std::error_code ec;

    if (start.is_absolute() && isSkipRoot(start)) return;

    fs::recursive_directory_iterator it(
        start,
        fs::directory_options::skip_permission_denied,
        ec
    );

    fs::recursive_directory_iterator end;

    if (ec) return;

    for (; it != end; it.increment(ec)) {
        if (g_interrupted) {
            g_interrupted = 0;
            std::cout << "\n^C\n";
            return;
        }

        if (ec) {
            ec.clear();
            continue;
        }

        const fs::path p = it->path();

        if (p.is_absolute() && isSkipRoot(p)) {
            it.disable_recursion_pending();
            continue;
        }

        if (it->is_symlink(ec)) {
            ec.clear();
            it.disable_recursion_pending();
            continue;
        }

        const std::string s = p.string();

        if (s.find(pattern) != std::string::npos) {
            std::cout << s << "\n";
        }
    }
}
