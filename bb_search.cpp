#include "bb_search.h"
#include "bb_signal.h"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <csignal>
#include <fstream>

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

static bool fileContainsPattern(const std::filesystem::path& p, const std::string& pattern) {
    std::ifstream in(p, std::ios::in | std::ios::binary);

    if (!in) return false;

    const size_t sniffN = 4096;

    std::string sniff;

    sniff.resize(sniffN);
    in.read(sniff.data(), (std::streamsize)sniffN);

    std::streamsize got = in.gcount();

    for (std::streamsize i = 0; i < got; i++) {
        if (sniff[(size_t)i] == '\0') return false;
    }

    if (got > 0) {
        std::string_view sv(sniff.data(), (size_t)got);
        if (sv.find(pattern) != std::string_view::npos) return true;
    }

    in.clear();
    in.seekg(0);

    std::string line;

    while (std::getline(in, line)) {
        if (line.find(pattern) != std::string::npos) return true;
    }

    return false;
}

void searchRecursive(const std::string& root, const std::string& pattern, bool inFiles) {
    namespace fs = std::filesystem;

    fs::path start(root);
    std::error_code ec;

    if (start.is_absolute() && isSkipRoot(start)) return;

    fs::recursive_directory_iterator it(start, fs::directory_options::skip_permission_denied, ec);
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

        if (!inFiles) {
            const std::string s = p.string();

            if (s.find(pattern) != std::string::npos) {
                std::cout << s << "\n";
            }

            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        // auto sz = it->file_size(ec);
        // ec.clear();
        // if (!ec && sz > 50 * 1024 * 1024) continue; // >50MB skip

        if (fileContainsPattern(p, pattern)) {
            std::cout << p.string() << "\n";
        }
    }
}
